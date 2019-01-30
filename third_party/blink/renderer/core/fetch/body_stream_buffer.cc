// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"

#include <memory>
#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_wrapper.h"
#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

class BodyStreamBuffer::LoaderClient final
    : public GarbageCollectedFinalized<LoaderClient>,
      public ContextLifecycleObserver,
      public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(LoaderClient);

 public:
  LoaderClient(ExecutionContext* execution_context,
               BodyStreamBuffer* buffer,
               FetchDataLoader::Client* client)
      : ContextLifecycleObserver(execution_context),
        buffer_(buffer),
        client_(client) {}

  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> blob_data_handle) override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedBlobHandle(std::move(blob_data_handle));
  }

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedArrayBuffer(array_buffer);
  }

  void DidFetchDataLoadedFormData(FormData* form_data) override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedFormData(form_data);
  }

  void DidFetchDataLoadedString(const String& string) override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedString(string);
  }

  void DidFetchDataLoadedDataPipe() override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedDataPipe();
  }

  void DidFetchDataLoadedCustomFormat() override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedCustomFormat();
  }

  void DidFetchDataLoadFailed() override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadFailed();
  }

  void Abort() override { NOTREACHED(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(buffer_);
    visitor->Trace(client_);
    ContextLifecycleObserver::Trace(visitor);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  void ContextDestroyed(ExecutionContext*) override { buffer_->StopLoading(); }

  Member<BodyStreamBuffer> buffer_;
  Member<FetchDataLoader::Client> client_;
  DISALLOW_COPY_AND_ASSIGN(LoaderClient);
};

BodyStreamBuffer::BodyStreamBuffer(ScriptState* script_state,
                                   BytesConsumer* consumer,
                                   AbortSignal* signal)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      consumer_(consumer),
      signal_(signal),
      made_from_readable_stream_(false) {
  v8::Local<v8::Value> body_value = ToV8(this, script_state);
  DCHECK(!body_value.IsEmpty());
  DCHECK(body_value->IsObject());
  v8::Local<v8::Object> body = body_value.As<v8::Object>();

  {
    // Leaving an exception pending will cause Blink to crash in the bindings
    // code later, so catch instead.
    v8::TryCatch try_catch(script_state->GetIsolate());
    ScriptValue strategy =
        ReadableStreamOperations::CreateCountQueuingStrategy(script_state, 0);
    if (!strategy.IsEmpty()) {
      ScriptValue readable_stream =
          ReadableStreamOperations::CreateReadableStream(script_state, this,
                                                         strategy);
      if (!readable_stream.IsEmpty()) {
        V8PrivateProperty::GetInternalBodyStream(script_state->GetIsolate())
            .Set(body, readable_stream.V8Value());
      } else {
        stream_broken_ = true;
      }
    } else {
      stream_broken_ = true;
    }
    DCHECK_EQ(stream_broken_, try_catch.HasCaught());
  }
  consumer_->SetClient(this);
  if (signal) {
    if (signal->aborted()) {
      Abort();
    } else {
      signal->AddAlgorithm(
          WTF::Bind(&BodyStreamBuffer::Abort, WrapWeakPersistent(this)));
    }
  }
  OnStateChange();
}

BodyStreamBuffer::BodyStreamBuffer(ScriptState* script_state,
                                   ScriptValue stream,
                                   ExceptionState& exception_state)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      signal_(nullptr),
      made_from_readable_stream_(true) {
  DCHECK(ReadableStreamOperations::IsReadableStream(script_state, stream,
                                                    exception_state)
             .value_or(true));

  if (exception_state.HadException())
    return;

  v8::Local<v8::Value> body_value = ToV8(this, script_state);
  DCHECK(!body_value.IsEmpty());
  DCHECK(body_value->IsObject());
  v8::Local<v8::Object> body = body_value.As<v8::Object>();

  V8PrivateProperty::GetInternalBodyStream(script_state->GetIsolate())
      .Set(body, stream.V8Value());
}

ScriptValue BodyStreamBuffer::Stream() {
  // Since this is the implementation of response.body, we return the stream
  // even if |stream_broken_| is true, so that expected Javascript attribute
  // behaviour is not changed. User code is still permitted to access the
  // stream even when it has thrown an exception.
  ScriptState::Scope scope(script_state_.get());
  v8::Local<v8::Value> body_value = ToV8(this, script_state_.get());
  DCHECK(!body_value.IsEmpty());
  DCHECK(body_value->IsObject());
  v8::Local<v8::Object> body = body_value.As<v8::Object>();
  return ScriptValue(
      script_state_.get(),
      V8PrivateProperty::GetInternalBodyStream(script_state_->GetIsolate())
          .GetOrUndefined(body));
}

scoped_refptr<BlobDataHandle> BodyStreamBuffer::DrainAsBlobDataHandle(
    BytesConsumer::BlobSizePolicy policy) {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());
  if (IsStreamClosed() || IsStreamErrored())
    return nullptr;

  if (made_from_readable_stream_)
    return nullptr;

  scoped_refptr<BlobDataHandle> blob_data_handle =
      consumer_->DrainAsBlobDataHandle(policy);
  if (blob_data_handle) {
    CloseAndLockAndDisturb();
    return blob_data_handle;
  }
  return nullptr;
}

scoped_refptr<EncodedFormData> BodyStreamBuffer::DrainAsFormData() {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());
  if (IsStreamClosed() || IsStreamErrored())
    return nullptr;

  if (made_from_readable_stream_)
    return nullptr;

  scoped_refptr<EncodedFormData> form_data = consumer_->DrainAsFormData();
  if (form_data) {
    CloseAndLockAndDisturb();
    return form_data;
  }
  return nullptr;
}

void BodyStreamBuffer::StartLoading(FetchDataLoader* loader,
                                    FetchDataLoader::Client* client) {
  DCHECK(!loader_);
  DCHECK(script_state_->ContextIsValid());
  loader_ = loader;
  if (signal_) {
    if (signal_->aborted()) {
      client->Abort();
      return;
    }
    signal_->AddAlgorithm(
        WTF::Bind(&FetchDataLoader::Client::Abort, WrapWeakPersistent(client)));
  }
  loader->Start(ReleaseHandle(),
                new LoaderClient(ExecutionContext::From(script_state_.get()),
                                 this, client));
}

void BodyStreamBuffer::Tee(BodyStreamBuffer** branch1,
                           BodyStreamBuffer** branch2,
                           ExceptionState& exception_state) {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());
  *branch1 = nullptr;
  *branch2 = nullptr;

  if (made_from_readable_stream_) {
    if (stream_broken_) {
      // We don't really know what state the stream is in, so throw an exception
      // rather than making things worse.
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Unsafe to tee stream in unknown state");
      return;
    }
    ScriptValue stream1, stream2;
    ReadableStreamOperations::Tee(script_state_.get(), Stream(), &stream1,
                                  &stream2, exception_state);
    if (exception_state.HadException()) {
      stream_broken_ = true;
      return;
    }

    // Exceptions here imply that |stream1| and/or |stream2| are broken, not the
    // stream owned by this object, so we shouldn't set |stream_broken_|.
    auto* tmp1 =
        new BodyStreamBuffer(script_state_.get(), stream1, exception_state);
    if (exception_state.HadException())
      return;
    auto* tmp2 =
        new BodyStreamBuffer(script_state_.get(), stream2, exception_state);
    if (exception_state.HadException())
      return;
    *branch1 = tmp1;
    *branch2 = tmp2;
    return;
  }
  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  BytesConsumer::Tee(ExecutionContext::From(script_state_.get()),
                     ReleaseHandle(), &dest1, &dest2);
  *branch1 = new BodyStreamBuffer(script_state_.get(), dest1, signal_);
  *branch2 = new BodyStreamBuffer(script_state_.get(), dest2, signal_);
}

ScriptPromise BodyStreamBuffer::pull(ScriptState* script_state) {
  DCHECK_EQ(script_state, script_state_.get());
  if (!consumer_) {
    // This is a speculative workaround for a crash. See
    // https://crbug.com/773525.
    // TODO(yhirano): Remove this branch or have a better comment.
    return ScriptPromise::CastUndefined(script_state);
  }

  if (stream_needs_more_)
    return ScriptPromise::CastUndefined(script_state);
  stream_needs_more_ = true;
  if (!in_process_data_)
    ProcessData();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise BodyStreamBuffer::Cancel(ScriptState* script_state,
                                       ScriptValue reason) {
  DCHECK_EQ(script_state, script_state_.get());
  if (Controller())
    Controller()->Close();
  CancelConsumer();
  return ScriptPromise::CastUndefined(script_state);
}

void BodyStreamBuffer::OnStateChange() {
  if (!consumer_ || !GetExecutionContext() ||
      GetExecutionContext()->IsContextDestroyed())
    return;

  switch (consumer_->GetPublicState()) {
    case BytesConsumer::PublicState::kReadableOrWaiting:
      break;
    case BytesConsumer::PublicState::kClosed:
      Close();
      return;
    case BytesConsumer::PublicState::kErrored:
      GetError();
      return;
  }
  ProcessData();
}

bool BodyStreamBuffer::HasPendingActivity() const {
  if (loader_)
    return true;
  return UnderlyingSourceBase::HasPendingActivity();
}

void BodyStreamBuffer::ContextDestroyed(ExecutionContext* destroyed_context) {
  CancelConsumer();
  UnderlyingSourceBase::ContextDestroyed(destroyed_context);
}

bool BodyStreamBuffer::IsStreamReadable() {
  return BooleanStreamOperationOrFallback(ReadableStreamOperations::IsReadable,
                                          false);
}

bool BodyStreamBuffer::IsStreamClosed() {
  return BooleanStreamOperationOrFallback(ReadableStreamOperations::IsClosed,
                                          true);
}

bool BodyStreamBuffer::IsStreamErrored() {
  return BooleanStreamOperationOrFallback(ReadableStreamOperations::IsErrored,
                                          true);
}

bool BodyStreamBuffer::IsStreamLocked() {
  return BooleanStreamOperationOrFallback(ReadableStreamOperations::IsLocked,
                                          true);
}

bool BodyStreamBuffer::IsStreamDisturbed() {
  return BooleanStreamOperationOrFallback(ReadableStreamOperations::IsDisturbed,
                                          true);
}

void BodyStreamBuffer::CloseAndLockAndDisturb() {
  if (IsStreamReadable()) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    Close();
  }
  if (stream_broken_)
    return;

  ScriptState::Scope scope(script_state_.get());
  ScriptValue reader =
      ReadableStreamOperations::GetReader(script_state_.get(), Stream());
  if (reader.IsEmpty())
    return;
  ReadableStreamOperations::DefaultReaderRead(script_state_.get(), reader);
}

bool BodyStreamBuffer::IsAborted() {
  if (!signal_)
    return false;
  return signal_->aborted();
}

void BodyStreamBuffer::Abort() {
  Controller()->GetError(DOMException::Create(DOMExceptionCode::kAbortError));
  CancelConsumer();
}

void BodyStreamBuffer::Close() {
  Controller()->Close();
  CancelConsumer();
}

void BodyStreamBuffer::GetError() {
  {
    ScriptState::Scope scope(script_state_.get());
    Controller()->GetError(V8ThrowException::CreateTypeError(
        script_state_->GetIsolate(), "network error"));
  }
  CancelConsumer();
}

void BodyStreamBuffer::CancelConsumer() {
  if (consumer_) {
    consumer_->Cancel();
    consumer_ = nullptr;
  }
}

void BodyStreamBuffer::ProcessData() {
  DCHECK(consumer_);
  DCHECK(!in_process_data_);

  base::AutoReset<bool> auto_reset(&in_process_data_, true);
  while (stream_needs_more_) {
    const char* buffer = nullptr;
    size_t available = 0;
    auto result = consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;
    DOMUint8Array* array = nullptr;
    if (result == BytesConsumer::Result::kOk) {
      array = DOMUint8Array::Create(
          reinterpret_cast<const unsigned char*>(buffer), available);
      result = consumer_->EndRead(available);
    }
    switch (result) {
      case BytesConsumer::Result::kOk:
      case BytesConsumer::Result::kDone:
        if (array) {
          // Clear m_streamNeedsMore in order to detect a pull call.
          stream_needs_more_ = false;
          Controller()->Enqueue(array);
        }
        if (result == BytesConsumer::Result::kDone) {
          Close();
          return;
        }
        // If m_streamNeedsMore is true, it means that pull is called and
        // the stream needs more data even if the desired size is not
        // positive.
        if (!stream_needs_more_)
          stream_needs_more_ = Controller()->DesiredSize() > 0;
        break;
      case BytesConsumer::Result::kShouldWait:
        NOTREACHED();
        return;
      case BytesConsumer::Result::kError:
        GetError();
        return;
    }
  }
}

void BodyStreamBuffer::EndLoading() {
  DCHECK(loader_);
  loader_ = nullptr;
}

void BodyStreamBuffer::StopLoading() {
  if (!loader_)
    return;
  loader_->Cancel();
  loader_ = nullptr;
}

bool BodyStreamBuffer::BooleanStreamOperationOrFallback(
    base::Optional<bool> (*predicate)(ScriptState*, ScriptValue),
    bool fallback_value) {
  if (stream_broken_)
    return fallback_value;
  ScriptState::Scope scope(script_state_.get());
  const base::Optional<bool> result = predicate(script_state_.get(), Stream());
  if (!result.has_value()) {
    stream_broken_ = true;
    return fallback_value;
  }
  return result.value();
}

BytesConsumer* BodyStreamBuffer::ReleaseHandle() {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());

  if (stream_broken_) {
    return BytesConsumer::CreateErrored(
        BytesConsumer::Error("ReleaseHandle called with broken stream"));
  }

  if (made_from_readable_stream_) {
    ScriptState::Scope scope(script_state_.get());
    // We need to have |reader| alive by some means (as written in
    // ReadableStreamDataConsumerHandle). Based on the following facts
    //  - This function is used only from tee and startLoading.
    //  - This branch cannot be taken when called from tee.
    //  - startLoading makes hasPendingActivity return true while loading.
    // , we don't need to keep the reader explicitly.
    ScriptValue reader =
        ReadableStreamOperations::GetReader(script_state_.get(), Stream());
    if (reader.IsEmpty()) {
      stream_broken_ = true;
      return BytesConsumer::CreateErrored(
          BytesConsumer::Error("Failed to GetReader in ReleaseHandle"));
    }
    return new ReadableStreamBytesConsumer(script_state_.get(), reader);
  }
  // We need to call these before calling closeAndLockAndDisturb.
  const bool is_closed = IsStreamClosed();
  const bool is_errored = IsStreamErrored();
  BytesConsumer* consumer = consumer_.Release();

  CloseAndLockAndDisturb();

  if (is_closed) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    return BytesConsumer::CreateClosed();
  }
  if (is_errored)
    return BytesConsumer::CreateErrored(BytesConsumer::Error("error"));

  DCHECK(consumer);
  consumer->ClearClient();
  return consumer;
}

}  // namespace blink
