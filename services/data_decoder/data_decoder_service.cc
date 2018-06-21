// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/data_decoder_service.h"

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/data_decoder/image_decoder_impl.h"
#include "services/data_decoder/json_parser_impl.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/data_decoder/xml_parser.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace data_decoder {

DataDecoderService::DataDecoderService() = default;

DataDecoderService::~DataDecoderService() = default;

// static
std::unique_ptr<service_manager::Service> DataDecoderService::Create() {
  return std::make_unique<DataDecoderService>();
}

void DataDecoderService::OnStart() {
  constexpr int kMaxServiceIdleTimeInSeconds = 5;
  keepalive_ = std::make_unique<service_manager::ServiceKeepalive>(
      context(), base::TimeDelta::FromSeconds(kMaxServiceIdleTimeInSeconds));
  registry_.AddInterface(base::BindRepeating(
      &DataDecoderService::BindImageDecoder, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &DataDecoderService::BindJsonParser, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(&DataDecoderService::BindXmlParser,
                                             base::Unretained(this)));
}

void DataDecoderService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void DataDecoderService::BindImageDecoder(mojom::ImageDecoderRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<ImageDecoderImpl>(keepalive_->CreateRef()),
      std::move(request));
}

void DataDecoderService::BindJsonParser(mojom::JsonParserRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<JsonParserImpl>(keepalive_->CreateRef()),
      std::move(request));
}

void DataDecoderService::BindXmlParser(mojom::XmlParserRequest request) {
  mojo::MakeStrongBinding(std::make_unique<XmlParser>(keepalive_->CreateRef()),
                          std::move(request));
}

}  // namespace data_decoder
