// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/certificate_transparency/sth_distributor.h"
#include "components/certificate_transparency/sth_observer.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/ct_log_response_parser.h"
#include "net/cert/signed_tree_head.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/log/net_log.h"
#include "net/log/net_log_util.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/mojo_net_log.h"
#include "services/network/network_context.h"
#include "services/network/network_usage_accumulator.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/url_request_context_builder_mojo.h"

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/cpu.h"
#endif

namespace network {

namespace {

// Field trial for network quality estimator. Seeds RTT and downstream
// throughput observations with values that correspond to the connection type
// determined by the operating system.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

std::unique_ptr<net::NetworkChangeNotifier>
CreateNetworkChangeNotifierIfNeeded() {
  // There is a global singleton net::NetworkChangeNotifier if NetworkService
  // is running inside of the browser process.
  if (!net::NetworkChangeNotifier::HasNetworkChangeNotifier()) {
#if defined(OS_ANDROID)
    // On Android, NetworkChangeNotifier objects are always set up in process
    // before NetworkService is run.
    return nullptr;
#elif defined(OS_CHROMEOS) || defined(OS_IOS) || defined(OS_FUCHSIA)
    // ChromeOS has its own implementation of NetworkChangeNotifier that lives
    // outside of //net. iOS doesn't embed //content. Fuchsia doesn't have an
    // implementation yet.
    // TODO(xunjieli): Figure out what to do for these 3 platforms.
    NOTIMPLEMENTED();
    return nullptr;
#endif
    return base::WrapUnique(net::NetworkChangeNotifier::Create());
  }
  return nullptr;
}

std::unique_ptr<net::HostResolver> CreateHostResolver(net::NetLog* net_log) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::unique_ptr<net::HostResolver> host_resolver(
      net::HostResolver::CreateDefaultResolver(net_log));
  if (!command_line.HasSwitch(switches::kHostResolverRules))
    return host_resolver;

  std::unique_ptr<net::MappedHostResolver> remapped_host_resolver(
      new net::MappedHostResolver(std::move(host_resolver)));
  remapped_host_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  return std::move(remapped_host_resolver);
}

}  // namespace

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry,
    mojom::NetworkServiceRequest request,
    net::NetLog* net_log)
    : registry_(std::move(registry)), binding_(this) {
  // |registry_| is nullptr when an in-process NetworkService is
  // created directly. The latter is done in concert with using
  // CreateNetworkContextWithBuilder to ease the transition to using the
  // network service.
  if (registry_) {
    DCHECK(!request.is_pending());
    registry_->AddInterface<mojom::NetworkService>(
        base::BindRepeating(&NetworkService::Bind, base::Unretained(this)));
  } else if (request.is_pending()) {
    Bind(std::move(request));
  }

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
  // Make sure OpenSSL is initialized before using it to histogram data.
  crypto::EnsureOpenSSLInit();

  // Measure CPUs with broken NEON units. See https://crbug.com/341598.
  UMA_HISTOGRAM_BOOLEAN("Net.HasBrokenNEON", CRYPTO_has_broken_NEON());
  // Measure Android kernels with missing AT_HWCAP2 auxv fields. See
  // https://crbug.com/boringssl/46.
  UMA_HISTOGRAM_BOOLEAN("Net.NeedsHWCAP2Workaround",
                        CRYPTO_needs_hwcap2_workaround());
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Record this once per session, though the switch is appled on a
  // per-NetworkContext basis.
  UMA_HISTOGRAM_BOOLEAN(
      "Net.Certificate.IgnoreCertificateErrorsSPKIListPresent",
      command_line->HasSwitch(
          network::switches::kIgnoreCertificateErrorsSPKIList));

  network_change_manager_ = std::make_unique<NetworkChangeManager>(
      CreateNetworkChangeNotifierIfNeeded());

  if (net_log) {
    net_log_ = net_log;
  } else {
    owned_net_log_ = std::make_unique<MojoNetLog>();
    // Note: The command line switches are only checked when not using the
    // embedder's NetLog, as it may already be writing to the destination log
    // file.
    owned_net_log_->ProcessCommandLine(*command_line);
    net_log_ = owned_net_log_.get();
  }

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new net::LoggingNetworkChangeObserver(net_log_));

  std::map<std::string, std::string> network_quality_estimator_params;
  base::GetFieldTrialParams(kNetworkQualityEstimatorFieldTrialName,
                            &network_quality_estimator_params);

  if (command_line->HasSwitch(switches::kForceEffectiveConnectionType)) {
    const std::string force_ect_value = command_line->GetSwitchValueASCII(
        switches::kForceEffectiveConnectionType);

    if (!force_ect_value.empty()) {
      // If the effective connection type is forced using command line switch,
      // it overrides the one set by field trial.
      network_quality_estimator_params[net::kForceEffectiveConnectionType] =
          force_ect_value;
    }
  }

  // Pass ownership.
  network_quality_estimator_ = std::make_unique<net::NetworkQualityEstimator>(
      std::make_unique<net::NetworkQualityEstimatorParams>(
          network_quality_estimator_params),
      net_log_);

#if defined(OS_CHROMEOS)
  // Get network id asynchronously to workaround https://crbug.com/821607 where
  // AddressTrackerLinux stucks with a recv() call and blocks IO thread.
  // TODO(https://crbug.com/821607): Remove after the bug is resolved.
  network_quality_estimator_->EnableGetNetworkIdAsynchronously();
#endif

  host_resolver_ = CreateHostResolver(net_log_);

  network_usage_accumulator_ = std::make_unique<NetworkUsageAccumulator>();
  sth_distributor_ =
      std::make_unique<certificate_transparency::STHDistributor>();
}

NetworkService::~NetworkService() {
  // Destroy owned network contexts.
  DestroyNetworkContexts();

  // All NetworkContexts (Owned and unowned) must have been deleted by this
  // point.
  DCHECK(network_contexts_.empty());
}

std::unique_ptr<NetworkService> NetworkService::Create(
    mojom::NetworkServiceRequest request,
    net::NetLog* net_log) {
  return std::make_unique<NetworkService>(nullptr, std::move(request), net_log);
}

std::unique_ptr<mojom::NetworkContext>
NetworkService::CreateNetworkContextWithBuilder(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params,
    std::unique_ptr<URLRequestContextBuilderMojo> builder,
    net::URLRequestContext** url_request_context) {
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(this, std::move(request),
                                       std::move(params), std::move(builder));
  *url_request_context = network_context->url_request_context();
  return network_context;
}

void NetworkService::SetHostResolver(
    std::unique_ptr<net::HostResolver> host_resolver) {
  DCHECK(network_contexts_.empty());
  host_resolver_ = std::move(host_resolver);
}

std::unique_ptr<NetworkService> NetworkService::CreateForTesting() {
  return base::WrapUnique(
      new NetworkService(std::make_unique<service_manager::BinderRegistry>()));
}

void NetworkService::RegisterNetworkContext(NetworkContext* network_context) {
  // If IsPrimaryNetworkContext() is true, there must be no other
  // NetworkContexts created yet.
  DCHECK(!network_context->IsPrimaryNetworkContext() ||
         network_contexts_.empty());

  DCHECK_EQ(0u, network_contexts_.count(network_context));
  network_contexts_.insert(network_context);
  if (quic_disabled_)
    network_context->DisableQuic();
}

void NetworkService::DeregisterNetworkContext(NetworkContext* network_context) {
  // If the NetworkContext is bthe primary network context, all other
  // NetworkContexts must already have been destroyed.
  DCHECK(!network_context->IsPrimaryNetworkContext() ||
         network_contexts_.size() == 1);

  DCHECK_EQ(1u, network_contexts_.count(network_context));
  network_contexts_.erase(network_context);
}

void NetworkService::CreateNetLogEntriesForActiveObjects(
    net::NetLog::ThreadSafeObserver* observer) {
  std::set<net::URLRequestContext*> contexts;
  for (NetworkContext* nc : network_contexts_)
    contexts.insert(nc->url_request_context());
  return net::CreateNetLogEntriesForActiveObjects(contexts, observer);
}

void NetworkService::SetClient(mojom::NetworkServiceClientPtr client) {
  client_ = std::move(client);
}

void NetworkService::CreateNetworkContext(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params) {
  // Only the first created NetworkContext can have |primary_next_context| set
  // to true.
  DCHECK(!params->primary_network_context || network_contexts_.empty());

  owned_network_contexts_.emplace(std::make_unique<NetworkContext>(
      this, std::move(request), std::move(params),
      base::BindOnce(&NetworkService::OnNetworkContextConnectionClosed,
                     base::Unretained(this))));
}

void NetworkService::ConfigureStubHostResolver(
    bool stub_resolver_enabled,
    base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
        dns_over_https_servers) {
  // If the stub resolver is not enabled, |dns_over_https_servers| has no
  // effect.
  DCHECK(stub_resolver_enabled || !dns_over_https_servers);
  DCHECK(!dns_over_https_servers || !dns_over_https_servers->empty());

  // Enable or disable the stub resolver, as needed. "DnsClient" is class that
  // implements the stub resolver.
  host_resolver_->SetDnsClientEnabled(stub_resolver_enabled);

  // Configure DNS over HTTPS.
  host_resolver_->ClearDnsOverHttpsServers();
  if (!dns_over_https_servers)
    return;

  for (auto* network_context : network_contexts_) {
    if (!network_context->IsPrimaryNetworkContext())
      continue;

    host_resolver_->SetRequestContext(network_context->url_request_context());
    for (const auto& doh_server : *dns_over_https_servers) {
      host_resolver_->AddDnsOverHttpsServer(doh_server->url.spec(),
                                            doh_server->use_posts);
    }
    return;
  }

  // Execution should generally not reach this line, but could run into races
  // with teardown, or restarting a crashed network process, that could
  // theoretically result in reaching it.
}

void NetworkService::DisableQuic() {
  quic_disabled_ = true;

  for (auto* network_context : network_contexts_) {
    network_context->DisableQuic();
  }
}

void NetworkService::SetUpHttpAuth(
    mojom::HttpAuthStaticParamsPtr http_auth_static_params) {
  DCHECK(!http_auth_handler_factory_);

  http_auth_handler_factory_ = net::HttpAuthHandlerRegistryFactory::Create(
      host_resolver_.get(), &http_auth_preferences_,
      http_auth_static_params->supported_schemes
#if defined(OS_CHROMEOS)
      ,
      http_auth_static_params->allow_gssapi_library_load
#elif (defined(OS_POSIX) && !defined(OS_ANDROID)) || defined(OS_FUCHSIA)
      ,
      http_auth_static_params->gssapi_library_name
#endif
      );
}

void NetworkService::ConfigureHttpAuthPrefs(
    mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_params) {
  http_auth_preferences_.SetServerWhitelist(
      http_auth_dynamic_params->server_whitelist);
  http_auth_preferences_.SetDelegateWhitelist(
      http_auth_dynamic_params->delegate_whitelist);
  http_auth_preferences_.set_negotiate_disable_cname_lookup(
      http_auth_dynamic_params->negotiate_disable_cname_lookup);
  http_auth_preferences_.set_negotiate_enable_port(
      http_auth_dynamic_params->enable_negotiate_port);

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  http_auth_preferences_.set_ntlm_v2_enabled(
      http_auth_dynamic_params->ntlm_v2_enabled);
#endif

#if defined(OS_ANDROID)
  http_auth_preferences_.set_auth_android_negotiate_account_type(
      http_auth_dynamic_params->android_negotiate_account_type);
#endif
}

void NetworkService::SetRawHeadersAccess(uint32_t process_id, bool allow) {
  DCHECK(process_id);
  if (allow)
    processes_with_raw_headers_access_.insert(process_id);
  else
    processes_with_raw_headers_access_.erase(process_id);
}

bool NetworkService::HasRawHeadersAccess(uint32_t process_id) const {
  // Allow raw headers for browser-initiated requests.
  if (!process_id)
    return true;
  return processes_with_raw_headers_access_.find(process_id) !=
         processes_with_raw_headers_access_.end();
}

net::NetLog* NetworkService::net_log() const {
  return net_log_;
}

void NetworkService::GetNetworkChangeManager(
    mojom::NetworkChangeManagerRequest request) {
  network_change_manager_->AddRequest(std::move(request));
}

void NetworkService::GetTotalNetworkUsages(
    mojom::NetworkService::GetTotalNetworkUsagesCallback callback) {
  std::move(callback).Run(network_usage_accumulator_->GetTotalNetworkUsages());
}

void NetworkService::UpdateSignedTreeHead(const net::ct::SignedTreeHead& sth) {
  sth_distributor_->NewSTHObserved(sth);
}

net::HttpAuthHandlerFactory* NetworkService::GetHttpAuthHandlerFactory() {
  if (!http_auth_handler_factory_) {
    http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault(
        host_resolver_.get(), &http_auth_preferences_);
  }
  return http_auth_handler_factory_.get();
}

certificate_transparency::STHReporter* NetworkService::sth_reporter() {
  return sth_distributor_.get();
}

void NetworkService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(interface_name, std::move(interface_pipe));
}

void NetworkService::DestroyNetworkContexts() {
  // Delete NetworkContexts. If there's a primary NetworkContext, it must be
  // deleted after all other NetworkContexts, to avoid use-after-frees.
  for (auto it = owned_network_contexts_.begin();
       it != owned_network_contexts_.end();) {
    const auto last = it;
    ++it;
    if (!(*last)->IsPrimaryNetworkContext())
      owned_network_contexts_.erase(last);
  }

  // If DNS over HTTPS is enabled, the HostResolver is currently using the
  // primary NetworkContext to do DNS lookups, so need to tell the HostResolver
  // to stop using DNS over HTTPS before destroying the primary NetworkContext.
  // The ClearDnsOverHttpsServers() call will will fail any in-progress DNS
  // lookups, but only if DNS over HTTPS is currently enabled.
  host_resolver_->ClearDnsOverHttpsServers();
  host_resolver_->SetRequestContext(nullptr);

  DCHECK_LE(owned_network_contexts_.size(), 1u);
  owned_network_contexts_.clear();
}

void NetworkService::OnNetworkContextConnectionClosed(
    NetworkContext* network_context) {
  if (network_context->IsPrimaryNetworkContext()) {
    DestroyNetworkContexts();
    return;
  }

  auto it = owned_network_contexts_.find(network_context);
  DCHECK(it != owned_network_contexts_.end());
  owned_network_contexts_.erase(it);
}

void NetworkService::Bind(mojom::NetworkServiceRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

}  // namespace network
