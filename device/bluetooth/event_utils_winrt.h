// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_EVENT_UTILS_WINRT_H_
#define DEVICE_BLUETOOTH_EVENT_UTILS_WINRT_H_

#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"

namespace device {

namespace internal {

// Utility function to pretty print enum values.
constexpr const char* ToCString(AsyncStatus async_status) {
  switch (async_status) {
    case AsyncStatus::Started:
      return "AsyncStatus::Started";
    case AsyncStatus::Completed:
      return "AsyncStatus::Completed";
    case AsyncStatus::Canceled:
      return "AsyncStatus::Canceled";
    case AsyncStatus::Error:
      return "AsyncStatus::Error";
  }

  NOTREACHED();
  return "";
}

template <typename Interface, typename... Args>
using IMemberFunction = HRESULT (__stdcall Interface::*)(Args...);

template <typename T>
using AsyncAbiT = typename ABI::Windows::Foundation::Internal::GetAbiType<
    typename ABI::Windows::Foundation::IAsyncOperation<T>::TResult_complex>::
    type;

// Compile time switch to decide what container to use for the async results for
// |T|. Depends on whether the underlying Abi type is a pointer to IUnknown or
// not. It queries the internals of Windows::Foundation to obtain this
// information.
template <typename T>
using AsyncResultsT = std::conditional_t<
    std::is_convertible<AsyncAbiT<T>, IUnknown*>::value,
    Microsoft::WRL::ComPtr<std::remove_pointer_t<AsyncAbiT<T>>>,
    AsyncAbiT<T>>;

// Obtains the results of the provided async operation.
template <typename T>
AsyncResultsT<T> GetAsyncResults(
    ABI::Windows::Foundation::IAsyncOperation<T>* async_op) {
  AsyncResultsT<T> results;
  HRESULT hr = async_op->GetResults(&results);
  if (FAILED(hr)) {
    VLOG(2) << "GetAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }

  return results;
}

}  // namespace internal

// This method registers a completion handler for |async_op| and will post the
// results to |callback|. The |callback| will be run on the same thread that
// invoked this method. Callers need to ensure that this method is invoked in
// the correct COM apartment, i.e. the one that created |async_op|. While a WRL
// Callback can be constructed from callable types such as a lambda or
// std::function objects, it cannot be directly constructed from a
// base::OnceCallback. Thus the callback is moved into a capturing lambda, which
// then posts the callback once it is run. Posting the results to the TaskRunner
// is required, since the completion callback might be invoked on an arbitrary
// thread. Lastly, the lambda takes ownership of |async_op|, as this needs to be
// kept alive until GetAsyncResults can be invoked.
template <typename T>
HRESULT PostAsyncResults(
    Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<T>>
        async_op,
    base::OnceCallback<void(internal::AsyncResultsT<T>)> callback) {
  auto completion_cb = base::BindOnce(
      [](Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IAsyncOperation<T>>
             async_op,
         base::OnceCallback<void(internal::AsyncResultsT<T>)> callback) {
        std::move(callback).Run(internal::GetAsyncResults(async_op.Get()));
      },
      async_op, std::move(callback));

  return async_op->put_Completed(
      Microsoft::WRL::Callback<
          ABI::Windows::Foundation::IAsyncOperationCompletedHandler<T>>([
        task_runner(base::ThreadTaskRunnerHandle::Get()),
        completion_cb(std::move(completion_cb))
      ](auto&&, AsyncStatus async_status) mutable {
        if (async_status != AsyncStatus::Completed) {
          VLOG(2) << "Got unexpected AsyncStatus: "
                  << internal::ToCString(async_status);
        }

        // Note: We are ignoring the passed in pointer to async_op, as the
        // completion callback has access to the initially provided |async_op|.
        // Since the code within the lambda could be executed on any thread, it
        // is vital that the completion callback gets posted to the original
        // |task_runner|, as this is guaranteed to be in the correct COM
        // apartment.
        task_runner->PostTask(FROM_HERE, std::move(completion_cb));
        return S_OK;
      })
          .Get());
}

// Convenience template function to construct a TypedEventHandler from a
// base::RepeatingCallback of a matching signature. In case of success, the
// EventRegistrationToken is returned to the caller. A return value of
// base::nullopt indicates a failure.
template <typename Interface,
          typename Sender,
          typename Args,
          typename SenderAbi,
          typename ArgsAbi>
base::Optional<EventRegistrationToken> AddTypedEventHandler(
    Interface* i,
    internal::IMemberFunction<
        Interface,
        ABI::Windows::Foundation::ITypedEventHandler<Sender, Args>*,
        EventRegistrationToken*> function,
    base::RepeatingCallback<void(SenderAbi, ArgsAbi)> callback) {
  EventRegistrationToken token;
  HRESULT hr = ((*i).*function)(
      Microsoft::WRL::Callback<
          ABI::Windows::Foundation::ITypedEventHandler<Sender, Args>>(
          [callback](SenderAbi sender, ArgsAbi args) {
            callback.Run(std::move(sender), std::move(args));
            return S_OK;
          })
          .Get(),
      &token);

  if (FAILED(hr)) {
    VLOG(2) << "Adding EventHandler failed: "
            << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  return token;
}

}  // namespace device

#endif  // DEVICE_BLUETOOTH_EVENT_UTILS_WINRT_H_
