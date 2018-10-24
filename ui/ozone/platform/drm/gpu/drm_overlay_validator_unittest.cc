// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_validator.h"

#include <drm_fourcc.h>

#include <memory>
#include <utility>

#include "base/files/platform_file.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_scanout_buffer.h"
#include "ui/ozone/platform/drm/gpu/mock_scanout_buffer_generator.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace {

// Mode of size 6x4.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;
constexpr uint32_t kCrtcIdBase = 1;
constexpr uint32_t kConnectorIdBase = 100;
constexpr uint32_t kPlaneIdBase = 200;
constexpr uint32_t kInFormatsBlobPropIdBase = 400;

constexpr uint32_t kTypePropId = 300;
constexpr uint32_t kInFormatsPropId = 301;

}  // namespace

class DrmOverlayValidatorTest : public testing::Test {
 public:
  DrmOverlayValidatorTest() {}

  void SetUp() override;
  void TearDown() override;

  void OnSwapBuffers(gfx::SwapResult result) {
    on_swap_buffers_count_++;
    last_swap_buffers_result_ = result;
  }

  scoped_refptr<ui::ScanoutBuffer> ReturnNullBuffer(const gfx::Size& size,
                                                    uint32_t format) {
    return nullptr;
  }

  void AddPlane(const ui::OverlayCheck_Params& params);

 protected:
  struct PlaneState {
    std::vector<uint32_t> formats;
  };

  struct CrtcState {
    std::vector<PlaneState> planes;
  };

  void InitializeDrmState(const std::vector<CrtcState>& crtc_states);

  std::unique_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<ui::MockDrmDevice> drm_;
  std::unique_ptr<ui::MockScanoutBufferGenerator> buffer_generator_;
  std::unique_ptr<ui::ScreenManager> screen_manager_;
  std::unique_ptr<ui::DrmDeviceManager> drm_device_manager_;
  ui::DrmWindow* window_;
  std::unique_ptr<ui::DrmOverlayValidator> overlay_validator_;
  std::vector<ui::OverlayCheck_Params> overlay_params_;
  ui::OverlayPlaneList plane_list_;

  int on_swap_buffers_count_;
  gfx::SwapResult last_swap_buffers_result_;
  gfx::Rect overlay_rect_;
  gfx::Rect primary_rect_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DrmOverlayValidatorTest);
};

void DrmOverlayValidatorTest::SetUp() {
  on_swap_buffers_count_ = 0;
  last_swap_buffers_result_ = gfx::SwapResult::SWAP_FAILED;

  message_loop_.reset(new base::MessageLoopForUI);
  drm_ = new ui::MockDrmDevice(false);

  CrtcState crtc_state = {.planes = {
                              {.formats = {DRM_FORMAT_XRGB8888}},
                          }};
  InitializeDrmState({crtc_state});

  buffer_generator_.reset(new ui::MockScanoutBufferGenerator());
  screen_manager_.reset(new ui::ScreenManager(buffer_generator_.get()));
  screen_manager_->AddDisplayController(drm_, kCrtcIdBase, kConnectorIdBase);
  screen_manager_->ConfigureDisplayController(
      drm_, kCrtcIdBase, kConnectorIdBase, gfx::Point(), kDefaultMode);

  drm_device_manager_.reset(new ui::DrmDeviceManager(nullptr));

  std::unique_ptr<ui::DrmWindow> window(new ui::DrmWindow(
      kDefaultWidgetHandle, drm_device_manager_.get(), screen_manager_.get()));
  window->Initialize(buffer_generator_.get());
  window->SetBounds(
      gfx::Rect(gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay)));
  screen_manager_->AddWindow(kDefaultWidgetHandle, std::move(window));
  window_ = screen_manager_->GetWindow(kDefaultWidgetHandle);
  overlay_validator_.reset(
      new ui::DrmOverlayValidator(window_, buffer_generator_.get()));

  overlay_rect_ =
      gfx::Rect(0, 0, kDefaultMode.hdisplay / 2, kDefaultMode.vdisplay / 2);

  primary_rect_ = gfx::Rect(0, 0, kDefaultMode.hdisplay, kDefaultMode.vdisplay);

  ui::OverlayCheck_Params primary_candidate;
  primary_candidate.buffer_size = primary_rect_.size();
  primary_candidate.display_rect = primary_rect_;
  primary_candidate.format = gfx::BufferFormat::BGRX_8888;
  overlay_params_.push_back(primary_candidate);
  AddPlane(primary_candidate);

  ui::OverlayCheck_Params overlay_candidate;
  overlay_candidate.buffer_size = overlay_rect_.size();
  overlay_candidate.display_rect = overlay_rect_;
  overlay_candidate.plane_z_order = 1;
  overlay_candidate.format = gfx::BufferFormat::BGRX_8888;
  overlay_params_.push_back(overlay_candidate);
  AddPlane(overlay_candidate);
}

void DrmOverlayValidatorTest::InitializeDrmState(
    const std::vector<CrtcState>& crtc_states) {
  std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties(
      crtc_states.size());
  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties;
  std::map<uint32_t, std::string> property_names = {
      {kTypePropId, "type"}, {kInFormatsPropId, "IN_FORMATS"},
  };

  uint32_t plane_id = kPlaneIdBase;
  uint32_t property_id = kInFormatsBlobPropIdBase;

  for (size_t crtc_idx = 0; crtc_idx < crtc_states.size(); ++crtc_idx) {
    crtc_properties[crtc_idx].id = kCrtcIdBase + crtc_idx;

    std::vector<ui::MockDrmDevice::PlaneProperties> crtc_plane_properties(
        crtc_states[crtc_idx].planes.size());
    for (size_t plane_idx = 0; plane_idx < crtc_states[crtc_idx].planes.size();
         ++plane_idx) {
      crtc_plane_properties[plane_idx].id = plane_id++;
      crtc_plane_properties[plane_idx].crtc_mask = 1 << crtc_idx;
      crtc_plane_properties[plane_idx].properties = {
          {.id = kTypePropId,
           .value = plane_idx == 0 ? DRM_PLANE_TYPE_PRIMARY
                                   : DRM_PLANE_TYPE_OVERLAY},
          {.id = kInFormatsPropId, .value = property_id++},
      };

      drm_->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
          crtc_plane_properties[plane_idx].properties[1].value,
          crtc_states[crtc_idx].planes[plane_idx].formats,
          std::vector<drm_format_modifier>()));
    }

    plane_properties.insert(plane_properties.end(),
                            crtc_plane_properties.begin(),
                            crtc_plane_properties.end());
  }

  drm_->InitializeState(crtc_properties, plane_properties, property_names,
                        /* use_atomic= */ false);
}

void DrmOverlayValidatorTest::AddPlane(const ui::OverlayCheck_Params& params) {
  scoped_refptr<ui::DrmDevice> drm =
      window_->GetController()->GetAllocationDrmDevice();
  scoped_refptr<ui::ScanoutBuffer> scanout_buffer = buffer_generator_->Create(
      drm, ui::GetFourCCFormatFromBufferFormat(params.format), {},
      params.buffer_size);
  ui::OverlayPlane plane(std::move(scanout_buffer), params.plane_z_order,
                         params.transform, params.display_rect,
                         params.crop_rect, true, nullptr);
  plane_list_.push_back(plane);
}

void DrmOverlayValidatorTest::TearDown() {
  std::unique_ptr<ui::DrmWindow> window =
      screen_manager_->RemoveWindow(kDefaultWidgetHandle);
  window->Shutdown();
  message_loop_.reset();
}

TEST_F(DrmOverlayValidatorTest, WindowWithNoController) {
  // We should never promote layers to overlay when controller is not
  // present.
  ui::HardwareDisplayController* controller = window_->GetController();
  window_->SetController(nullptr);
  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  EXPECT_EQ(returns.front().status, ui::OVERLAY_STATUS_NOT);
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_NOT);
  window_->SetController(controller);
}

TEST_F(DrmOverlayValidatorTest, DontPromoteMoreLayersThanAvailablePlanes) {
  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  EXPECT_EQ(returns.front().status, ui::OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_NOT);
}

TEST_F(DrmOverlayValidatorTest, DontCollapseOverlayToPrimaryInFullScreen) {
  // Overlay Validator should not collapse planes during validation.
  overlay_params_.back().buffer_size = primary_rect_.size();
  overlay_params_.back().display_rect = primary_rect_;
  plane_list_.back().display_bounds = primary_rect_;

  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  // Second candidate should be marked as Invalid as we have only one plane
  // per CRTC.
  EXPECT_EQ(returns.front().status, ui::OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_NOT);
}

TEST_F(DrmOverlayValidatorTest, OverlayFormat_XRGB) {
  // This test checks for optimal format in case of non full screen video case.
  // This should be XRGB when overlay doesn't support YUV.
  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = overlay_rect_;
  plane_list_.back().display_bounds = overlay_rect_;

  CrtcState state = {
      .planes =
          {
              {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_UYVY}},
              {.formats = {DRM_FORMAT_XRGB8888}},
          },
  };
  InitializeDrmState(std::vector<CrtcState>(1, state));

  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  for (const auto& param : returns)
    EXPECT_EQ(param.status, ui::OVERLAY_STATUS_ABLE);
}

TEST_F(DrmOverlayValidatorTest, OverlayFormat_YUV) {
  // This test checks for optimal format in case of non full screen video case.
  // Prefer YUV as optimal format when Overlay supports it and scaling is
  // needed.
  gfx::RectF crop_rect = gfx::RectF(0, 0, 0.5, 0.5);
  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = overlay_rect_;
  overlay_params_.back().crop_rect = crop_rect;
  overlay_params_.back().format = gfx::BufferFormat::UYVY_422;
  plane_list_.pop_back();
  AddPlane(overlay_params_.back());

  CrtcState state = {
      .planes =
          {
              {.formats = {DRM_FORMAT_XRGB8888}},
              {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_UYVY}},
          },
  };
  InitializeDrmState(std::vector<CrtcState>(1, state));

  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  for (const auto& param : returns)
    EXPECT_EQ(param.status, ui::OVERLAY_STATUS_ABLE);
}

TEST_F(DrmOverlayValidatorTest, RejectYUVBuffersIfNotSupported) {
  // Check case where buffer storage format is already UYVY but planes dont
  // support it.
  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = overlay_rect_;
  overlay_params_.back().format = gfx::BufferFormat::UYVY_422;
  plane_list_.pop_back();
  AddPlane(overlay_params_.back());

  CrtcState state = {
      .planes =
          {
              {.formats = {DRM_FORMAT_XRGB8888}},
              {.formats = {DRM_FORMAT_XRGB8888}},
          },
  };
  InitializeDrmState(std::vector<CrtcState>(1, state));

  std::vector<ui::OverlayCheck_Params> validated_params = overlay_params_;
  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(validated_params,
                                       ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_NOT);
}

TEST_F(DrmOverlayValidatorTest,
       RejectYUVBuffersIfNotSupported_MirroredControllers) {
  std::vector<CrtcState> crtc_states = {
      {
          .planes =
              {
                  {.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_UYVY}},
              },
      },
      {
          .planes =
              {
                  {.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_UYVY}},
              },
      },
  };
  InitializeDrmState(crtc_states);

  ui::HardwareDisplayController* controller = window_->GetController();
  controller->AddCrtc(
      std::unique_ptr<ui::CrtcController>(new ui::CrtcController(
          drm_.get(), kCrtcIdBase + 1, kConnectorIdBase + 1)));
  ui::OverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                              new ui::MockScanoutBuffer(primary_rect_.size())),
                          nullptr);
  EXPECT_TRUE(controller->Modeset(plane1, kDefaultMode));

  gfx::RectF crop_rect = gfx::RectF(0, 0, 0.5, 0.5);
  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = overlay_rect_;
  overlay_params_.back().crop_rect = crop_rect;
  plane_list_.back().display_bounds = overlay_rect_;
  plane_list_.back().crop_rect = crop_rect;

  std::vector<ui::OverlayCheck_Params> validated_params = overlay_params_;
  validated_params.back().format = gfx::BufferFormat::UYVY_422;
  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(validated_params,
                                       ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_ABLE);

  // This configuration should not be promoted to Overlay when either of the
  // controllers dont support UYVY format.

  // Check case where we dont have support for packed formats in Mirrored CRTC.
  crtc_states[1].planes[1].formats = {DRM_FORMAT_XRGB8888};
  InitializeDrmState(crtc_states);

  returns = overlay_validator_->TestPageFlip(validated_params,
                                             ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_NOT);

  // Check case where we dont have support for packed formats in primary
  // display.
  crtc_states[0].planes[1].formats = {DRM_FORMAT_XRGB8888};
  crtc_states[1].planes[1].formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_UYVY};
  InitializeDrmState(crtc_states);

  returns = overlay_validator_->TestPageFlip(validated_params,
                                             ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_NOT);
  controller->RemoveCrtc(drm_, kCrtcIdBase + 1);
}

TEST_F(DrmOverlayValidatorTest, OptimalFormatXRGB_MirroredControllers) {
  std::vector<CrtcState> crtc_states = {
      {
          .planes =
              {
                  {.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_UYVY}},
              },
      },
      {
          .planes =
              {
                  {.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_UYVY}},
              },
      },
  };
  InitializeDrmState(crtc_states);

  ui::HardwareDisplayController* controller = window_->GetController();
  controller->AddCrtc(
      std::unique_ptr<ui::CrtcController>(new ui::CrtcController(
          drm_.get(), kCrtcIdBase + 1, kConnectorIdBase + 1)));
  ui::OverlayPlane plane1(scoped_refptr<ui::ScanoutBuffer>(
                              new ui::MockScanoutBuffer(primary_rect_.size())),
                          nullptr);
  EXPECT_TRUE(controller->Modeset(plane1, kDefaultMode));

  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = overlay_rect_;
  plane_list_.back().display_bounds = overlay_rect_;

  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_ABLE);

  // Check case where we dont have support for packed formats in Mirrored CRTC.
  crtc_states[1].planes[1].formats = {DRM_FORMAT_XRGB8888};
  InitializeDrmState(crtc_states);

  returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_ABLE);

  // Check case where we dont have support for packed formats in primary
  // display.
  crtc_states[0].planes[1].formats = {DRM_FORMAT_XRGB8888};
  crtc_states[1].planes[1].formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_UYVY};
  InitializeDrmState(crtc_states);

  returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back().status, ui::OVERLAY_STATUS_ABLE);

  controller->RemoveCrtc(drm_, kCrtcIdBase + 1);
}

TEST_F(DrmOverlayValidatorTest, RejectBufferAllocationFail) {
  // Buffer allocation for scanout might fail.
  // In that case we should reject the overlay candidate.
  buffer_generator_->set_allocation_failure(true);

  std::vector<ui::OverlayCheckReturn_Params> returns =
      overlay_validator_->TestPageFlip(overlay_params_, ui::OverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.front().status, ui::OVERLAY_STATUS_NOT);
}
