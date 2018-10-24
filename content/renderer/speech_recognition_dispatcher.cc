// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/speech_recognition_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_frame_impl.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_speech_grammar.h"
#include "third_party/blink/public/web/web_speech_recognition_params.h"
#include "third_party/blink/public/web/web_speech_recognition_result.h"

using blink::WebVector;
using blink::WebString;
using blink::WebSpeechGrammar;
using blink::WebSpeechRecognitionHandle;
using blink::WebSpeechRecognitionResult;
using blink::WebSpeechRecognitionParams;
using blink::WebSpeechRecognizerClient;

namespace content {

SpeechRecognitionDispatcher::SpeechRecognitionDispatcher(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

SpeechRecognitionDispatcher::~SpeechRecognitionDispatcher() = default;

void SpeechRecognitionDispatcher::OnDestruct() {
  delete this;
}

void SpeechRecognitionDispatcher::WasHidden() {
#if defined(OS_ANDROID)
  for (const auto& it : session_map_) {
    it.second->Abort();
  }
#endif
}

void SpeechRecognitionDispatcher::Start(
    const WebSpeechRecognitionHandle& handle,
    const WebSpeechRecognitionParams& params,
    const WebSpeechRecognizerClient& recognizer_client) {
  DCHECK(recognizer_client_.IsNull() ||
         recognizer_client_ == recognizer_client);
  recognizer_client_ = recognizer_client;

  blink::mojom::StartSpeechRecognitionRequestParamsPtr msg_params =
      blink::mojom::StartSpeechRecognitionRequestParams::New();
  for (const WebSpeechGrammar& grammar : params.Grammars()) {
    msg_params->grammars.push_back(blink::mojom::SpeechRecognitionGrammar::New(
        grammar.Src(), grammar.Weight()));
  }
  msg_params->language = params.Language().Utf8();
  msg_params->max_hypotheses = static_cast<uint32_t>(params.MaxAlternatives());
  msg_params->continuous = params.Continuous();
  msg_params->interim_results = params.InterimResults();
  msg_params->origin = params.Origin();

  blink::mojom::SpeechRecognitionSessionClientPtrInfo client_ptr_info;
  blink::mojom::SpeechRecognitionSessionClientRequest client_request =
      mojo::MakeRequest(&client_ptr_info);
  bindings_.AddBinding(std::make_unique<SpeechRecognitionSessionClientImpl>(
                           this, handle, recognizer_client_),
                       std::move(client_request));

  blink::mojom::SpeechRecognitionSessionPtr session_client;
  blink::mojom::SpeechRecognitionSessionRequest request =
      mojo::MakeRequest(&session_client);

  AddHandle(handle, std::move(session_client));

  msg_params->client = std::move(client_ptr_info);
  msg_params->session_request = std::move(request);

  GetSpeechRecognitionHost().Start(std::move(msg_params));
}

void SpeechRecognitionDispatcher::Stop(
    const WebSpeechRecognitionHandle& handle,
    const WebSpeechRecognizerClient& recognizer_client) {
  // Ignore a |stop| issued without a matching |start|.
  if (recognizer_client_ != recognizer_client || !HandleExists(handle))
    return;
  GetSession(handle)->StopCapture();
}

void SpeechRecognitionDispatcher::Abort(
    const WebSpeechRecognitionHandle& handle,
    const WebSpeechRecognizerClient& recognizer_client) {
  // Ignore an |abort| issued without a matching |start|.
  if (recognizer_client_ != recognizer_client || !HandleExists(handle))
    return;
  GetSession(handle)->Abort();
}

static WebSpeechRecognizerClient::ErrorCode WebKitErrorCode(
    blink::mojom::SpeechRecognitionErrorCode e) {
  switch (e) {
    case blink::mojom::SpeechRecognitionErrorCode::kNone:
      NOTREACHED();
      return WebSpeechRecognizerClient::kOtherError;
    case blink::mojom::SpeechRecognitionErrorCode::kNoSpeech:
      return WebSpeechRecognizerClient::kNoSpeechError;
    case blink::mojom::SpeechRecognitionErrorCode::kAborted:
      return WebSpeechRecognizerClient::kAbortedError;
    case blink::mojom::SpeechRecognitionErrorCode::kAudioCapture:
      return WebSpeechRecognizerClient::kAudioCaptureError;
    case blink::mojom::SpeechRecognitionErrorCode::kNetwork:
      return WebSpeechRecognizerClient::kNetworkError;
    case blink::mojom::SpeechRecognitionErrorCode::kNotAllowed:
      return WebSpeechRecognizerClient::kNotAllowedError;
    case blink::mojom::SpeechRecognitionErrorCode::kServiceNotAllowed:
      return WebSpeechRecognizerClient::kServiceNotAllowedError;
    case blink::mojom::SpeechRecognitionErrorCode::kBadGrammar:
      return WebSpeechRecognizerClient::kBadGrammarError;
    case blink::mojom::SpeechRecognitionErrorCode::kLanguageNotSupported:
      return WebSpeechRecognizerClient::kLanguageNotSupportedError;
    case blink::mojom::SpeechRecognitionErrorCode::kNoMatch:
      NOTREACHED();
      return WebSpeechRecognizerClient::kOtherError;
  }
  NOTREACHED();
  return WebSpeechRecognizerClient::kOtherError;
}

void SpeechRecognitionDispatcher::AddHandle(
    const blink::WebSpeechRecognitionHandle& handle,
    blink::mojom::SpeechRecognitionSessionPtr session) {
  DCHECK(!HandleExists(handle));
  session_map_[handle] = std::move(session);
}

bool SpeechRecognitionDispatcher::HandleExists(
    const WebSpeechRecognitionHandle& handle) {
  return session_map_.find(handle) != session_map_.end();
}

void SpeechRecognitionDispatcher::RemoveHandle(
    const blink::WebSpeechRecognitionHandle& handle) {
  session_map_.erase(handle);
}

blink::mojom::SpeechRecognitionSession* SpeechRecognitionDispatcher::GetSession(
    const blink::WebSpeechRecognitionHandle& handle) {
  DCHECK(HandleExists(handle));
  return session_map_[handle].get();
}

blink::mojom::SpeechRecognizer&
SpeechRecognitionDispatcher::GetSpeechRecognitionHost() {
  if (!speech_recognition_host_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&speech_recognition_host_));
  }
  return *speech_recognition_host_;
}

// ------------ SpeechRecognitionSessionClientImpl
// ------------------------------------

SpeechRecognitionSessionClientImpl::SpeechRecognitionSessionClientImpl(
    SpeechRecognitionDispatcher* dispatcher,
    const blink::WebSpeechRecognitionHandle& handle,
    const blink::WebSpeechRecognizerClient& client)
    : parent_dispatcher_(dispatcher), handle_(handle), web_client_(client) {}

void SpeechRecognitionSessionClientImpl::Started() {
  web_client_.DidStart(handle_);
}

void SpeechRecognitionSessionClientImpl::AudioStarted() {
  web_client_.DidStartAudio(handle_);
}

void SpeechRecognitionSessionClientImpl::SoundStarted() {
  web_client_.DidStartSound(handle_);
}

void SpeechRecognitionSessionClientImpl::SoundEnded() {
  web_client_.DidEndSound(handle_);
}

void SpeechRecognitionSessionClientImpl::AudioEnded() {
  web_client_.DidEndAudio(handle_);
}

void SpeechRecognitionSessionClientImpl::ErrorOccurred(
    const blink::mojom::SpeechRecognitionErrorPtr error) {
  if (error->code == blink::mojom::SpeechRecognitionErrorCode::kNoMatch) {
    web_client_.DidReceiveNoMatch(handle_, WebSpeechRecognitionResult());
  } else {
    web_client_.DidReceiveError(handle_,
                                WebString(),  // TODO(primiano): message?
                                WebKitErrorCode(error->code));
  }
}

void SpeechRecognitionSessionClientImpl::Ended() {
  parent_dispatcher_->RemoveHandle(handle_);
  web_client_.DidEnd(handle_);
}

void SpeechRecognitionSessionClientImpl::ResultRetrieved(
    std::vector<blink::mojom::SpeechRecognitionResultPtr> results) {
  size_t provisional_count =
      std::count_if(results.begin(), results.end(),
                    [](const blink::mojom::SpeechRecognitionResultPtr& result) {
                      return result->is_provisional;
                    });

  WebVector<WebSpeechRecognitionResult> provisional(provisional_count);
  WebVector<WebSpeechRecognitionResult> final(results.size() -
                                              provisional_count);

  int provisional_index = 0, final_index = 0;
  for (const blink::mojom::SpeechRecognitionResultPtr& result : results) {
    WebSpeechRecognitionResult* webkit_result =
        result->is_provisional ? &provisional[provisional_index++]
                               : &final[final_index++];

    const size_t num_hypotheses = result->hypotheses.size();
    WebVector<WebString> transcripts(num_hypotheses);
    WebVector<float> confidences(num_hypotheses);
    for (size_t i = 0; i < num_hypotheses; ++i) {
      transcripts[i] = WebString::FromUTF16(result->hypotheses[i]->utterance);
      confidences[i] = static_cast<float>(result->hypotheses[i]->confidence);
    }
    webkit_result->Assign(transcripts, confidences, !result->is_provisional);
  }

  web_client_.DidReceiveResults(handle_, final, provisional);
}

}  // namespace content
