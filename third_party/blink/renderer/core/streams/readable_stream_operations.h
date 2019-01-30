// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_OPERATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_OPERATIONS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8.h"

namespace blink {

class UnderlyingSourceBase;
class ExceptionState;
class ScriptState;

// This class has various methods for ReadableStream[Reader] implemented with
// V8 Extras.
// All methods should be called in an appropriate V8 context. All ScriptValue
// arguments must not be empty.
//
// Boolean methods return an optional bool, where an empty value indicates that
// Javascript failed to return a value (ie. an exception occurred). Exceptions
// are not caught, so that they can be handled by user Javascript. This implicit
// exception passing is error-prone and bad.
//
// TODO(ricea): Add ExceptionState arguments and make exception passing
// explicit. https://crbug.com/853189.
class CORE_EXPORT ReadableStreamOperations {
  STATIC_ONLY(ReadableStreamOperations);

 public:
  // createReadableStreamWithExternalController
  // If the caller supplies an invalid strategy (e.g. one that returns
  // negative sizes, or doesn't have appropriate properties), or an exception
  // occurs for another reason, this will return an empty value.
  static ScriptValue CreateReadableStream(ScriptState*,
                                          UnderlyingSourceBase*,
                                          ScriptValue strategy);

  // createBuiltInCountQueuingStrategy
  // If the constructor throws, this will return an empty value.
  static ScriptValue CreateCountQueuingStrategy(ScriptState*,
                                                size_t high_water_mark);

  // AcquireReadableStreamDefaultReader
  // This function assumes |IsReadableStream(stream)|.
  static ScriptValue GetReader(ScriptState*, ScriptValue stream);

  // IsReadableStream. Exceptions are not caught.
  static base::Optional<bool> IsReadableStream(ScriptState*, ScriptValue);

  // IsReadableStream, exception-catching version. Exceptions will be passed to
  // |exception_state|.
  static base::Optional<bool> IsReadableStream(ScriptState*,
                                               ScriptValue,
                                               ExceptionState& exception_state);

  // IsReadableStreamDisturbed.
  // This function assumes |IsReadableStream(stream)|.
  static base::Optional<bool> IsDisturbed(ScriptState*, ScriptValue stream);

  // IsReadableStreamLocked.
  // This function assumes |IsReadableStream(stream)|.
  static base::Optional<bool> IsLocked(ScriptState*, ScriptValue stream);

  // IsReadableStreamReadable.
  // This function assumes |IsReadableStream(stream)|.
  static base::Optional<bool> IsReadable(ScriptState*, ScriptValue stream);

  // IsReadableStreamClosed.
  // This function assumes |IsReadableStream(stream)|.
  static base::Optional<bool> IsClosed(ScriptState*, ScriptValue stream);

  // IsReadableStreamErrored.
  // This function assumes |IsReadableStream(stream)|.
  static base::Optional<bool> IsErrored(ScriptState*, ScriptValue stream);

  // IsReadableStreamDefaultReader.
  static base::Optional<bool> IsReadableStreamDefaultReader(ScriptState*,
                                                            ScriptValue);

  // ReadableStreamDefaultReaderRead
  // This function assumes |IsReadableStreamDefaultReader(reader)|.
  // If an exception occurs, returns a rejected promise.
  static ScriptPromise DefaultReaderRead(ScriptState*, ScriptValue reader);

  // ReadableStreamTee
  // This function assumes |IsReadableStream(stream)| and |!IsLocked(stream)|
  // Returns without setting new_stream1 or new_stream2 if an error occurs.
  // Exceptions are caught and rethrown on |exception_state|.
  static void Tee(ScriptState*,
                  ScriptValue stream,
                  ScriptValue* new_stream1,
                  ScriptValue* new_stream2,
                  ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_OPERATIONS_H_
