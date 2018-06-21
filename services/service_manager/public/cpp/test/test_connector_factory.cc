// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/test/test_connector_factory.h"

#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/mojom/connector.mojom.h"

namespace service_manager {

namespace {

class TestServiceControl : public mojom::ServiceControl {
 public:
  TestServiceControl(ServiceContext* context,
                     mojom::ServiceControlAssociatedRequest control_request)
      : context_(context), binding_(this, std::move(control_request)) {}

  // mojom::ServiceControl implementation
  void RequestQuit() override { context_->QuitNow(); }

 private:
  ServiceContext* const context_;
  mojo::AssociatedBinding<mojom::ServiceControl> binding_;
};

class TestConnectorImplBase : public mojom::Connector {
 public:
  TestConnectorImplBase(std::string test_user_id,
                        bool release_service_on_quit_request)
      : test_user_id_(std::move(test_user_id)),
        release_service_on_quit_request_(release_service_on_quit_request) {}
  ~TestConnectorImplBase() override = default;

  void AddService(const std::string& service_name,
                  std::unique_ptr<Service> service,
                  mojom::ServicePtr* service_ptr) {
    auto service_context = std::make_unique<ServiceContext>(
        std::move(service), mojo::MakeRequest(service_ptr));
    auto* service_context_ptr = service_context.get();
    // Use of |Unretained(this)| is safe because |this| owns
    // |service_context|.
    service_context->SetQuitClosure(
        base::BindRepeating(&TestConnectorImplBase::OnServiceRequestingQuit,
                            base::Unretained(this), service_context_ptr));
    service_contexts_.emplace(service_context_ptr, std::move(service_context));
    (*service_ptr)
        ->OnStart(Identity("TestConnectorFactory", test_user_id_),
                  base::BindOnce(&TestConnectorImplBase::OnStartCallback,
                                 base::Unretained(this), service_context_ptr));
  }

 private:
  virtual mojom::ServicePtr* GetServicePtr(const std::string& service_name) = 0;

  void OnStartCallback(ServiceContext* context,
                       mojom::ConnectorRequest request,
                       mojom::ServiceControlAssociatedRequest control_request) {
    if (!release_service_on_quit_request_)
      return;
    service_controls_.emplace(context,
                              std::make_unique<TestServiceControl>(
                                  context, std::move(control_request)));
  }

  void OnServiceRequestingQuit(ServiceContext* context) {
    DCHECK(base::ContainsKey(service_controls_, context));
    DCHECK(base::ContainsKey(service_contexts_, context));
    service_controls_.erase(context);
    service_contexts_.erase(context);
  }

  // mojom::Connector implementation:
  void BindInterface(const Identity& target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     BindInterfaceCallback callback) override {
    mojom::ServicePtr* service_ptr = GetServicePtr(target.name());
    // If you hit the DCHECK below, you need to add a call to AddService() in
    // your test for the reported service.
    DCHECK(service_ptr) << "Binding interface for unregistered service "
                        << target.name();
    (*service_ptr)
        ->OnBindInterface(
            BindSourceInfo(Identity("TestConnectorFactory", test_user_id_),
                           CapabilitySet()),
            interface_name, std::move(interface_pipe), base::DoNothing());
    std::move(callback).Run(mojom::ConnectResult::SUCCEEDED, Identity());
  }

  void StartService(const Identity& target,
                    StartServiceCallback callback) override {
    NOTREACHED();
  }

  void QueryService(const Identity& target,
                    QueryServiceCallback callback) override {
    NOTREACHED();
  }

  void StartServiceWithProcess(
      const Identity& identity,
      mojo::ScopedMessagePipeHandle service,
      mojom::PIDReceiverRequest pid_receiver_request,
      StartServiceWithProcessCallback callback) override {
    NOTREACHED();
  }

  void Clone(mojom::ConnectorRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  void FilterInterfaces(const std::string& spec,
                        const Identity& source,
                        mojom::InterfaceProviderRequest source_request,
                        mojom::InterfaceProviderPtr target) override {
    NOTREACHED();
  }

  const std::string test_user_id_;
  const bool release_service_on_quit_request_;
  std::map<ServiceContext*, std::unique_ptr<ServiceContext>> service_contexts_;
  std::map<ServiceContext*, std::unique_ptr<TestServiceControl>>
      service_controls_;
  mojo::BindingSet<mojom::Connector> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectorImplBase);
};

class UniqueServiceConnector : public TestConnectorImplBase {
 public:
  explicit UniqueServiceConnector(std::unique_ptr<Service> service,
                                  std::string test_user_id,
                                  bool release_service_on_quit_request)
      : TestConnectorImplBase(std::move(test_user_id),
                              release_service_on_quit_request) {
    AddService(/*service_name=*/std::string(), std::move(service),
               &service_ptr_);
  }

 private:
  mojom::ServicePtr* GetServicePtr(const std::string& service_name) override {
    return &service_ptr_;
  }

  mojom::ServicePtr service_ptr_;

  DISALLOW_COPY_AND_ASSIGN(UniqueServiceConnector);
};

class MultipleServiceConnector : public TestConnectorImplBase {
 public:
  explicit MultipleServiceConnector(
      TestConnectorFactory::NameToServiceMap services,
      std::string test_user_id,
      bool release_service_on_quit_request)
      : TestConnectorImplBase(std::move(test_user_id),
                              release_service_on_quit_request) {
    for (auto& name_and_service : services) {
      mojom::ServicePtr service_ptr;
      const std::string& service_name = name_and_service.first;
      AddService(service_name, std::move(name_and_service.second),
                 &service_ptr);
      service_ptrs_.insert(
          std::make_pair(service_name, std::move(service_ptr)));
    }
  }

 private:
  using NameToServicePtrMap = std::map<std::string, mojom::ServicePtr>;

  mojom::ServicePtr* GetServicePtr(const std::string& service_name) override {
    NameToServicePtrMap::iterator it = service_ptrs_.find(service_name);
    return it == service_ptrs_.end() ? nullptr : &(it->second);
  }

  NameToServicePtrMap service_ptrs_;

  DISALLOW_COPY_AND_ASSIGN(MultipleServiceConnector);
};

}  // namespace

TestConnectorFactory::TestConnectorFactory(
    std::unique_ptr<mojom::Connector> impl,
    std::string test_user_id)
    : impl_(std::move(impl)), test_user_id_(std::move(test_user_id)) {}

TestConnectorFactory::~TestConnectorFactory() = default;

// static
std::unique_ptr<TestConnectorFactory>
TestConnectorFactory::CreateForUniqueService(
    std::unique_ptr<Service> service,
    bool release_service_on_quit_request) {
  // Note that we are not using std::make_unique below so TestConnectorFactory's
  // constructor can be kept private.
  std::string guid = base::GenerateGUID();
  return std::unique_ptr<TestConnectorFactory>(new TestConnectorFactory(
      std::make_unique<UniqueServiceConnector>(std::move(service), guid,
                                               release_service_on_quit_request),
      guid));
}

// static
std::unique_ptr<TestConnectorFactory> TestConnectorFactory::CreateForServices(
    NameToServiceMap services,
    bool release_service_on_quit_request) {
  std::string guid = base::GenerateGUID();
  return std::unique_ptr<TestConnectorFactory>(new TestConnectorFactory(
      std::make_unique<MultipleServiceConnector>(
          std::move(services), guid, release_service_on_quit_request),
      guid));
}

std::unique_ptr<Connector> TestConnectorFactory::CreateConnector() {
  mojom::ConnectorPtr proxy;
  impl_->Clone(mojo::MakeRequest(&proxy));
  return std::make_unique<Connector>(std::move(proxy));
}

}  // namespace service_manager
