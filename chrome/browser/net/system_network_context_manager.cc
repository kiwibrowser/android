// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/system_network_context_manager.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_handle.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/default_network_context_params.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "net/net_buildflags.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// Called on IOThread to disable QUIC for HttpNetworkSessions not using the
// network service. Note that re-enabling QUIC dynamically is not supported for
// simpliciy and requires a browser restart.
void DisableQuicOnIOThread(IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    content::GetNetworkServiceImpl()->DisableQuic();
  io_thread->DisableQuic();
}

void GetStubResolverConfig(
    bool* stub_resolver_enabled,
    base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
        dns_over_https_servers) {
  DCHECK(!dns_over_https_servers->has_value());

  PrefService* local_state = g_browser_process->local_state();

  const auto& doh_server_list =
      local_state->GetList(prefs::kDnsOverHttpsServers)->GetList();
  const auto& doh_server_method_list =
      local_state->GetList(prefs::kDnsOverHttpsServerMethods)->GetList();
  DCHECK_EQ(doh_server_list.size(), doh_server_method_list.size());

  for (size_t i = 0;
       i < doh_server_list.size() && i < doh_server_method_list.size(); ++i) {
    if (!doh_server_list[i].is_string() ||
        !doh_server_method_list[i].is_string()) {
      continue;
    }

    if (!dns_over_https_servers) {
      *dns_over_https_servers = base::make_optional<
          std::vector<network::mojom::DnsOverHttpsServerPtr>>();
    }
    network::mojom::DnsOverHttpsServerPtr dns_over_https_server =
        network::mojom::DnsOverHttpsServer::New();
    dns_over_https_server->url = GURL(doh_server_list[i].GetString());
    dns_over_https_server->use_posts =
        (doh_server_method_list[i].GetString() == "POST");
    (*dns_over_https_servers)->emplace_back(std::move(dns_over_https_server));
  }

  *stub_resolver_enabled =
      !!dns_over_https_servers ||
      local_state->GetBoolean(prefs::kBuiltInDnsClientEnabled);
}

void OnStubResolverConfigChanged(const std::string& pref_name) {
  bool stub_resolver_enabled;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers;
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  content::GetNetworkService()->ConfigureStubHostResolver(
      stub_resolver_enabled, std::move(dns_over_https_servers));
}

// Constructs HttpAuthStaticParams based on global state.
network::mojom::HttpAuthStaticParamsPtr CreateHttpAuthStaticParams() {
  PrefService* local_state = g_browser_process->local_state();
  network::mojom::HttpAuthStaticParamsPtr auth_static_params =
      network::mojom::HttpAuthStaticParams::New();

  // TODO(https://crbug/549273): Allow this to change after startup.
  auth_static_params->supported_schemes =
      base::SplitString(local_state->GetString(prefs::kAuthSchemes), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  auth_static_params->gssapi_library_name =
      local_state->GetString(prefs::kGSSAPILibraryName);
#endif

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  auth_static_params->allow_gssapi_library_load =
      connector->IsActiveDirectoryManaged();
#endif

  return auth_static_params;
}

// Constructs HttpAuthDynamicParams based on current global state.
network::mojom::HttpAuthDynamicParamsPtr CreateHttpAuthDynamicParams() {
  PrefService* local_state = g_browser_process->local_state();
  network::mojom::HttpAuthDynamicParamsPtr auth_dynamic_params =
      network::mojom::HttpAuthDynamicParams::New();

  auth_dynamic_params->server_whitelist =
      local_state->GetString(prefs::kAuthServerWhitelist);
  auth_dynamic_params->delegate_whitelist =
      local_state->GetString(prefs::kAuthNegotiateDelegateWhitelist);
  auth_dynamic_params->negotiate_disable_cname_lookup =
      local_state->GetBoolean(prefs::kDisableAuthNegotiateCnameLookup);
  auth_dynamic_params->enable_negotiate_port =
      local_state->GetBoolean(prefs::kEnableAuthNegotiatePort);

#if defined(OS_POSIX)
  auth_dynamic_params->ntlm_v2_enabled =
      local_state->GetBoolean(prefs::kNtlmV2Enabled);
#endif  // defined(OS_POSIX)

#if defined(OS_ANDROID)
  auth_dynamic_params->android_negotiate_account_type =
      local_state->GetString(prefs::kAuthAndroidNegotiateAccountType);
#endif  // defined(OS_ANDROID)

  return auth_dynamic_params;
}

void OnAuthPrefsChanged(const std::string& pref_name) {
  content::GetNetworkService()->ConfigureHttpAuthPrefs(
      CreateHttpAuthDynamicParams());
}

// Check the AsyncDns field trial and return true if it should be enabled. On
// Android this includes checking the Android version in the field trial.
bool ShouldEnableAsyncDns() {
  bool feature_can_be_enabled = true;
#if defined(OS_ANDROID)
  int min_sdk =
      base::GetFieldTrialParamByFeatureAsInt(features::kAsyncDns, "min_sdk", 0);
  if (base::android::BuildInfo::GetInstance()->sdk_int() < min_sdk)
    feature_can_be_enabled = false;
#endif
  return feature_can_be_enabled &&
         base::FeatureList::IsEnabled(features::kAsyncDns);
}

}  // namespace

base::LazyInstance<SystemNetworkContextManager>::Leaky
    g_system_network_context_manager = LAZY_INSTANCE_INITIALIZER;

// SharedURLLoaderFactory backed by a SystemNetworkContextManager and its
// network context. Transparently handles crashes.
class SystemNetworkContextManager::URLLoaderFactoryForSystem
    : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForSystem(SystemNetworkContextManager* manager)
      : manager_(manager) {}

  // mojom::URLLoaderFactory implementation:

  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!manager_)
      return;
    manager_->GetURLLoaderFactory()->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    if (!manager_)
      return;
    manager_->GetURLLoaderFactory()->Clone(std::move(request));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    NOTREACHED() << "This isn't supported. SharedURLLoaderFactory can only be"
                    " used on the UI thread.";
    return nullptr;
  }

  void Shutdown() { manager_ = nullptr; }

 private:
  friend class base::RefCounted<URLLoaderFactoryForSystem>;
  ~URLLoaderFactoryForSystem() override {}

  SystemNetworkContextManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryForSystem);
};

network::mojom::NetworkContext* SystemNetworkContextManager::GetContext() {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // SetUp should already have been called.
    DCHECK(io_thread_network_context_);
    return io_thread_network_context_.get();
  }

  if (!network_service_network_context_ ||
      network_service_network_context_.encountered_error()) {
    // This should call into OnNetworkServiceCreated(), which will re-create
    // the network service, if needed. There's a chance that it won't be
    // invoked, if the NetworkContext has encountered an error but the
    // NetworkService has not yet noticed its pipe was closed. In that case,
    // trying to create a new NetworkContext would fail, anyways, and hopefully
    // a new NetworkContext will be created on the next GetContext() call.
    content::GetNetworkService();
    DCHECK(network_service_network_context_);
  }
  return network_service_network_context_.get();
}

network::mojom::URLLoaderFactory*
SystemNetworkContextManager::GetURLLoaderFactory() {
  // Create the URLLoaderFactory as needed.
  if (url_loader_factory_ && !url_loader_factory_.encountered_error()) {
    return url_loader_factory_.get();
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  GetContext()->CreateURLLoaderFactory(mojo::MakeRequest(&url_loader_factory_),
                                       std::move(params));
  return url_loader_factory_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
SystemNetworkContextManager::GetSharedURLLoaderFactory() {
  return shared_url_loader_factory_;
}

void SystemNetworkContextManager::SetUp(
    network::mojom::NetworkContextRequest* network_context_request,
    network::mojom::NetworkContextParamsPtr* network_context_params,
    bool* stub_resolver_enabled,
    base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
        dns_over_https_servers,
    network::mojom::HttpAuthStaticParamsPtr* http_auth_static_params,
    network::mojom::HttpAuthDynamicParamsPtr* http_auth_dynamic_params,
    bool* is_quic_allowed) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    *network_context_request = mojo::MakeRequest(&io_thread_network_context_);
    *network_context_params = CreateNetworkContextParams();
  } else {
    // Just use defaults if the network service is enabled, since
    // CreateNetworkContextParams() can only be called once.
    *network_context_params = CreateDefaultNetworkContextParams();
  }
  *is_quic_allowed = is_quic_allowed_;
  *http_auth_static_params = CreateHttpAuthStaticParams();
  *http_auth_dynamic_params = CreateHttpAuthDynamicParams();
  GetStubResolverConfig(stub_resolver_enabled, dns_over_https_servers);
}

SystemNetworkContextManager::SystemNetworkContextManager()
    : ssl_config_service_manager_(SSLConfigServiceManager::CreateDefaultManager(
          g_browser_process->local_state())) {
  const base::Value* value =
      g_browser_process->policy_service()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .GetValue(policy::key::kQuicAllowed);
  if (value)
    value->GetAsBoolean(&is_quic_allowed_);
  shared_url_loader_factory_ = new URLLoaderFactoryForSystem(this);

  pref_change_registrar_.Init(g_browser_process->local_state());

  PrefChangeRegistrar::NamedChangeCallback dns_pref_callback =
      base::BindRepeating(&OnStubResolverConfigChanged);
  pref_change_registrar_.Add(prefs::kBuiltInDnsClientEnabled,
                             dns_pref_callback);
  pref_change_registrar_.Add(prefs::kDnsOverHttpsServers, dns_pref_callback);
  pref_change_registrar_.Add(prefs::kDnsOverHttpsServerMethods,
                             dns_pref_callback);

  PrefChangeRegistrar::NamedChangeCallback auth_pref_callback =
      base::BindRepeating(&OnAuthPrefsChanged);
  pref_change_registrar_.Add(prefs::kAuthServerWhitelist, auth_pref_callback);
  pref_change_registrar_.Add(prefs::kAuthNegotiateDelegateWhitelist,
                             auth_pref_callback);
  pref_change_registrar_.Add(prefs::kDisableAuthNegotiateCnameLookup,
                             auth_pref_callback);
  pref_change_registrar_.Add(prefs::kEnableAuthNegotiatePort,
                             auth_pref_callback);

#if defined(OS_POSIX)
  pref_change_registrar_.Add(prefs::kNtlmV2Enabled, auth_pref_callback);
#endif  // defined(OS_POSIX)

#if defined(OS_ANDROID)
  pref_change_registrar_.Add(prefs::kAuthAndroidNegotiateAccountType,
                             auth_pref_callback);
#endif  // defined(OS_ANDROID)

  enable_referrers_.Init(
      prefs::kEnableReferrers, g_browser_process->local_state(),
      base::BindRepeating(&SystemNetworkContextManager::UpdateReferrersEnabled,
                          base::Unretained(this)));
}

SystemNetworkContextManager::~SystemNetworkContextManager() {
  shared_url_loader_factory_->Shutdown();
}

void SystemNetworkContextManager::RegisterPrefs(PrefRegistrySimple* registry) {
  // DnsClient prefs.
  registry->RegisterBooleanPref(prefs::kBuiltInDnsClientEnabled,
                                ShouldEnableAsyncDns());
  // Set default DNS over HTTPS server list and server methods, based on whether
  // or not the DNS over HTTPS feature is enabled.
  std::unique_ptr<base::ListValue> default_doh_servers =
      std::make_unique<base::ListValue>();
  std::unique_ptr<base::ListValue> default_doh_server_methods =
      std::make_unique<base::ListValue>();
  if (base::FeatureList::IsEnabled(features::kDnsOverHttps)) {
    base::Value server(variations::GetVariationParamValueByFeature(
        features::kDnsOverHttps, "server"));
    base::Value method(variations::GetVariationParamValueByFeature(
        features::kDnsOverHttps, "method"));
    if (!server.GetString().empty()) {
      default_doh_servers->GetList().push_back(std::move(server));
      default_doh_server_methods->GetList().push_back(std::move(method));
    }
  }
  registry->RegisterListPref(prefs::kDnsOverHttpsServers,
                             std::move(default_doh_servers));
  registry->RegisterListPref(prefs::kDnsOverHttpsServerMethods,
                             std::move(default_doh_server_methods));

  // Static auth params
  registry->RegisterStringPref(prefs::kAuthSchemes,
                               "basic,digest,ntlm,negotiate");
#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

  // Dynamic auth params.
  registry->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup, false);
  registry->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist,
                               std::string());
#if defined(OS_POSIX)
  registry->RegisterBooleanPref(
      prefs::kNtlmV2Enabled,
      base::FeatureList::IsEnabled(features::kNtlmV2Enabled));
#endif  // defined(OS_POSIX)
#if defined(OS_ANDROID)
  registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                               std::string());
#endif  // defined(OS_ANDROID)

  // Per-NetworkContext pref. The pref value from
  // g_browser_process->local_state() is used for the system NetworkContext, and
  // the per-profile pref values are used for the profile NetworkContexts.
  registry->RegisterBooleanPref(prefs::kEnableReferrers, true);
}

void SystemNetworkContextManager::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;
  // Disable QUIC globally, if needed.
  if (!is_quic_allowed_)
    network_service->DisableQuic();

  network_service->SetUpHttpAuth(CreateHttpAuthStaticParams());
  network_service->ConfigureHttpAuthPrefs(CreateHttpAuthDynamicParams());

  // The system NetworkContext must be created first, since it sets
  // |primary_network_context| to true.
  network_service->CreateNetworkContext(
      MakeRequest(&network_service_network_context_),
      CreateNetworkContextParams());

  // Configure the stub resolver. This must be done after the system
  // NetworkContext is created, but before anything has the chance to use it.
  bool stub_resolver_enabled;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers;
  GetStubResolverConfig(&stub_resolver_enabled, &dns_over_https_servers);
  content::GetNetworkService()->ConfigureStubHostResolver(
      stub_resolver_enabled, std::move(dns_over_https_servers));
}

void SystemNetworkContextManager::DisableQuic() {
  is_quic_allowed_ = false;

  // Disabling QUIC for a profile disables QUIC globally. As a side effect, new
  // Profiles will also have QUIC disabled (because both IOThread's
  // NetworkService and the network service, if enabled will disable QUIC).

  content::GetNetworkService()->DisableQuic();

  IOThread* io_thread = g_browser_process->io_thread();
  // Nothing more to do if IOThread has already been shut down.
  if (!io_thread)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DisableQuicOnIOThread, io_thread));
}

void SystemNetworkContextManager::AddSSLConfigToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  ssl_config_service_manager_->AddToNetworkContextParams(
      network_context_params);
}

void SystemNetworkContextManager::FlushSSLConfigManagerForTesting() {
  ssl_config_service_manager_->FlushForTesting();
}

void SystemNetworkContextManager::FlushProxyConfigMonitorForTesting() {
  proxy_config_monitor_.FlushForTesting();
}

void SystemNetworkContextManager::FlushNetworkInterfaceForTesting() {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    DCHECK(io_thread_network_context_);
    io_thread_network_context_.FlushForTesting();
  } else {
    DCHECK(network_service_network_context_);
    network_service_network_context_.FlushForTesting();
  }
  if (url_loader_factory_)
    url_loader_factory_.FlushForTesting();
}

network::mojom::NetworkContextParamsPtr
SystemNetworkContextManager::CreateNetworkContextParams() {
  // TODO(mmenke): Set up parameters here (in memory cookie store, etc).
  network::mojom::NetworkContextParamsPtr network_context_params =
      CreateDefaultNetworkContextParams();

  network_context_params->context_name = std::string("system");

  network_context_params->enable_referrers = enable_referrers_.GetValue();

  network_context_params->http_cache_enabled = false;

  // These are needed for PAC scripts that use file or data URLs (Or FTP URLs?).
  // TODO(crbug.com/839566): remove file support for all cases.
  network_context_params->enable_data_url_support = true;
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    network_context_params->enable_file_url_support = true;
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  network_context_params->enable_ftp_url_support = true;
#endif

  network_context_params->primary_network_context = true;

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params.get());

  return network_context_params;
}

void SystemNetworkContextManager::UpdateReferrersEnabled() {
  GetContext()->SetEnableReferrers(enable_referrers_.GetValue());
}
