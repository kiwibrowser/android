// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/test/test_helper.h"
#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "device/vr/openvr/test/test_hook.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "third_party/openvr/src/src/ivrclientcore.h"

#include <D3D11_1.h>
#include <DXGI1_4.h>
#include <wrl.h>
#include <memory>

namespace vr {

void TestHelper::TestFailure() {
  NOTREACHED();
}

void TestHelper::OnPresentedFrame(ID3D11Texture2D* texture) {
  // Early-out if there is nobody listening.
  bool is_hooked = false;
  lock_.Acquire();
  if (test_hook_) {
    is_hooked = true;
  }
  lock_.Release();
  if (!is_hooked)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> device;
  texture->GetDevice(&device);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(&context);

  // We copy the submitted texture to a new texture, so we can map it, and
  // read back pixel data.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_copy;
  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);
  desc.Width = 1;
  desc.Height = 1;
  desc.MiscFlags = 0;
  desc.BindFlags = 0;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture_copy);
  if (FAILED(hr)) {
    TestFailure();
    return;
  }

  D3D11_BOX box = {0, 0, 0, 1, 1, 1};  // a 1-pixel box
  context->CopySubresourceRegion(texture_copy.Get(), 0, 0, 0, 0, texture, 0,
                                 &box);

  D3D11_MAPPED_SUBRESOURCE map_data = {};
  hr = context->Map(texture_copy.Get(), 0, D3D11_MAP_READ, 0, &map_data);
  if (FAILED(hr)) {
    TestFailure();
    return;
  }

  // We have a 1-pixel image.  Give it to the test hook.
  device::Color* color = reinterpret_cast<device::Color*>(map_data.pData);

  lock_.Acquire();
  if (test_hook_)
    test_hook_->OnFrameSubmitted(device::SubmittedFrameData{color[0]});
  lock_.Release();

  context->Unmap(texture_copy.Get(), 0);
}

void TestHelper::SetTestHook(device::OpenVRTestHook* hook) {
  lock_.Acquire();
  test_hook_ = hook;
  lock_.Release();
}

}  // namespace vr