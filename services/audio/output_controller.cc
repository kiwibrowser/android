// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_controller.h"

#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_timestamp_helper.h"

using base::TimeDelta;

namespace audio {

namespace {

// Time in seconds between two successive measurements of audio power levels.
constexpr int kPowerMonitorLogIntervalSeconds = 15;

// Used to log the result of rendering startup.
// Elements in this enum should not be deleted or rearranged; the only
// permitted operation is to add new elements before
// STREAM_CREATION_RESULT_MAX and update STREAM_CREATION_RESULT_MAX.
enum StreamCreationResult {
  STREAM_CREATION_OK = 0,
  STREAM_CREATION_CREATE_FAILED = 1,
  STREAM_CREATION_OPEN_FAILED = 2,
  STREAM_CREATION_RESULT_MAX = STREAM_CREATION_OPEN_FAILED,
};

void LogStreamCreationResult(bool for_device_change,
                             StreamCreationResult result) {
  if (for_device_change) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.AudioOutputController.ProxyStreamCreationResultForDeviceChange",
        result, STREAM_CREATION_RESULT_MAX + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.AudioOutputController.ProxyStreamCreationResult", result,
        STREAM_CREATION_RESULT_MAX + 1);
  }
}

}  // namespace

OutputController::ErrorStatisticsTracker::ErrorStatisticsTracker()
    : start_time_(base::TimeTicks::Now()), on_more_io_data_called_(0) {
  // WedgeCheck() will look to see if |on_more_io_data_called_| is true after
  // the timeout expires and log this as a UMA stat. If the stream is
  // paused/closed before the timer fires, nothing is logged.
  wedge_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(5), this,
                     &ErrorStatisticsTracker::WedgeCheck);
}

OutputController::ErrorStatisticsTracker::~ErrorStatisticsTracker() {
  UMA_HISTOGRAM_LONG_TIMES("Media.OutputStreamDuration",
                           base::TimeTicks::Now() - start_time_);
  UMA_HISTOGRAM_BOOLEAN("Media.AudioOutputController.CallbackError",
                        error_during_callback_);
}

void OutputController::ErrorStatisticsTracker::RegisterError() {
  error_during_callback_ = true;
}

void OutputController::ErrorStatisticsTracker::OnMoreDataCalled() {
  // Indicate that we haven't wedged (at least not indefinitely, WedgeCheck()
  // may have already fired if OnMoreData() took an abnormal amount of time).
  // Since this thread is the only writer of |on_more_io_data_called_| once the
  // thread starts, it's safe to compare and then increment.
  if (on_more_io_data_called_.IsZero())
    on_more_io_data_called_.Increment();
}

void OutputController::ErrorStatisticsTracker::WedgeCheck() {
  UMA_HISTOGRAM_BOOLEAN("Media.AudioOutputControllerPlaybackStartupSuccess",
                        on_more_io_data_called_.IsOne());
}

OutputController::OutputController(media::AudioManager* audio_manager,
                                   EventHandler* handler,
                                   const media::AudioParameters& params,
                                   const std::string& output_device_id,
                                   const base::UnguessableToken& group_id,
                                   SyncReader* sync_reader)
    : audio_manager_(audio_manager),
      params_(params),
      handler_(handler),
      task_runner_(audio_manager->GetTaskRunner()),
      output_device_id_(output_device_id),
      group_id_(group_id),
      stream_(NULL),
      disable_local_output_(false),
      should_duplicate_(0),
      volume_(1.0),
      state_(kEmpty),
      sync_reader_(sync_reader),
      power_monitor_(
          params.sample_rate(),
          TimeDelta::FromMilliseconds(kPowerMeasurementTimeConstantMillis)),
      weak_factory_for_stream_(this) {
  DCHECK(audio_manager);
  DCHECK(handler_);
  DCHECK(sync_reader_);
  DCHECK(task_runner_.get());
}

OutputController::~OutputController() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(kClosed, state_);
  DCHECK_EQ(nullptr, stream_);
  DCHECK(snoopers_.empty());
  DCHECK(should_duplicate_.IsZero());
}

bool OutputController::Create(bool is_for_device_change) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.CreateTime");
  TRACE_EVENT0("audio", "OutputController::Create");
  handler_->OnLog(is_for_device_change
                      ? "OutputController::Create (for device change)"
                      : "OutputController::Create");

  // Close() can be called before Create() is executed.
  if (state_ == kClosed)
    return false;

  StopCloseAndClearStream();  // Calls RemoveOutputDeviceChangeListener().
  DCHECK_EQ(kEmpty, state_);

  if (disable_local_output_) {
    // Create a fake AudioOutputStream that will continue pumping the audio
    // data, but does not play it out anywhere. Pumping the audio data is
    // necessary because video playback is synchronized to the audio stream and
    // will freeze otherwise.
    media::AudioParameters mute_params = params_;
    mute_params.set_format(media::AudioParameters::AUDIO_FAKE);
    stream_ = audio_manager_->MakeAudioOutputStream(
        mute_params, std::string(),
        /*log_callback, not used*/ base::DoNothing());
  } else {
    stream_ =
        audio_manager_->MakeAudioOutputStreamProxy(params_, output_device_id_);
  }

  if (!stream_) {
    state_ = kError;
    LogStreamCreationResult(is_for_device_change,
                            STREAM_CREATION_CREATE_FAILED);
    handler_->OnControllerError();
    return false;
  }

  weak_this_for_stream_ = weak_factory_for_stream_.GetWeakPtr();
  if (!stream_->Open()) {
    StopCloseAndClearStream();
    LogStreamCreationResult(is_for_device_change, STREAM_CREATION_OPEN_FAILED);
    state_ = kError;
    handler_->OnControllerError();
    return false;
  }

  LogStreamCreationResult(is_for_device_change, STREAM_CREATION_OK);

    audio_manager_->AddOutputDeviceChangeListener(this);

  // We have successfully opened the stream. Set the initial volume.
  stream_->SetVolume(volume_);

  // Finally set the state to kCreated.
  state_ = kCreated;

  return true;
}

void OutputController::Play() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.PlayTime");
  TRACE_EVENT0("audio", "OutputController::Play");
  handler_->OnLog("OutputController::Play");

  // We can start from created or paused state.
  if (state_ != kCreated && state_ != kPaused)
    return;

  // Ask for first packet.
  sync_reader_->RequestMoreData(base::TimeDelta(), base::TimeTicks(), 0);

  state_ = kPlaying;

  if (will_monitor_audio_levels()) {
    last_audio_level_log_time_ = base::TimeTicks::Now();
  }

  stats_tracker_.emplace();

  stream_->Start(this);

  handler_->OnControllerPlaying();
}

void OutputController::StopStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kPlaying) {
    stream_->Stop();
    stats_tracker_.reset();

    if (will_monitor_audio_levels()) {
      LogAudioPowerLevel("StopStream");
    }

    // A stopped stream is silent, and power_montior_.Scan() is no longer being
    // called; so we must reset the power monitor.
    power_monitor_.Reset();

    state_ = kPaused;
  }
}

void OutputController::Pause() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.PauseTime");
  TRACE_EVENT0("audio", "OutputController::Pause");
  handler_->OnLog("OutputController::Pause");

  StopStream();

  if (state_ != kPaused)
    return;

  // Let the renderer know we've stopped.  Necessary to let PPAPI clients know
  // audio has been shutdown.  TODO(dalecurtis): This stinks.  PPAPI should have
  // a better way to know when it should exit PPB_Audio_Shared::Run().
  sync_reader_->RequestMoreData(base::TimeDelta::Max(), base::TimeTicks(), 0);

  handler_->OnControllerPaused();
}

void OutputController::Close() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.CloseTime");
  TRACE_EVENT0("audio", "OutputController::Close");
  handler_->OnLog("OutputController::Close");

  if (state_ != kClosed) {
    StopCloseAndClearStream();
    sync_reader_->Close();

    state_ = kClosed;
  }
}

void OutputController::SetVolume(double volume) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Saves the volume to a member first. We may not be able to set the volume
  // right away but when the stream is created we'll set the volume.
  volume_ = volume;

  switch (state_) {
    case kCreated:
    case kPlaying:
    case kPaused:
      stream_->SetVolume(volume_);
      break;
    default:
      break;
  }
}

void OutputController::ReportError() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("audio", "OutputController::ReportError");
  DLOG(ERROR) << "OutputController::ReportError";
  if (state_ != kClosed) {
    if (stats_tracker_)
      stats_tracker_->RegisterError();
    handler_->OnControllerError();
  }
}

int OutputController::OnMoreData(base::TimeDelta delay,
                                 base::TimeTicks delay_timestamp,
                                 int prior_frames_skipped,
                                 media::AudioBus* dest) {
  TRACE_EVENT_BEGIN1("audio", "OutputController::OnMoreData", "frames skipped",
                     prior_frames_skipped);

  stats_tracker_->OnMoreDataCalled();

  sync_reader_->Read(dest);

  const int frames =
      dest->is_bitstream_format() ? dest->GetBitstreamFrames() : dest->frames();
  delay +=
      media::AudioTimestampHelper::FramesToTime(frames, params_.sample_rate());

  sync_reader_->RequestMoreData(delay, delay_timestamp, prior_frames_skipped);

  if (!should_duplicate_.IsZero()) {
    const base::TimeTicks reference_time = delay_timestamp + delay;
    std::unique_ptr<media::AudioBus> copy(media::AudioBus::Create(params_));
    dest->CopyTo(copy.get());
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OutputController::BroadcastDataToSnoopers,
                       weak_this_for_stream_, std::move(copy), reference_time));
  }

  if (will_monitor_audio_levels()) {
    // Note: this code path should never be hit when using bitstream streams.
    // Scan doesn't expect compressed audio, so it may go out of bounds trying
    // to read |frames| frames of PCM data.
    CHECK(!params_.IsBitstreamFormat());
    power_monitor_.Scan(*dest, frames);

    const auto now = base::TimeTicks::Now();
    if ((now - last_audio_level_log_time_).InSeconds() >
        kPowerMonitorLogIntervalSeconds) {
      LogAudioPowerLevel("OnMoreData");
      last_audio_level_log_time_ = now;
    }
  }

  TRACE_EVENT_END2("audio", "OutputController::OnMoreData", "timestamp (ms)",
                   (delay_timestamp - base::TimeTicks()).InMillisecondsF(),
                   "delay (ms)", delay.InMillisecondsF());
  return frames;
}

void OutputController::BroadcastDataToSnoopers(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks reference_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("audio", "OutputController::BroadcastDataToSnoopers",
               "reference_time (ms)",
               (reference_time - base::TimeTicks()).InMillisecondsF());
  if (state_ != kPlaying)
    return;

  for (Snooper* snooper : snoopers_)
    snooper->OnData(*audio_bus, reference_time, volume_);
}

void OutputController::LogAudioPowerLevel(const char* call_name) {
  std::pair<float, bool> power_and_clip =
      power_monitor_.ReadCurrentPowerAndClip();
  handler_->OnLog(
      base::StringPrintf("OutputController::%s: average audio level=%.2f dBFS",
                         call_name, power_and_clip.first));
}

void OutputController::OnError() {
  // Handle error on the audio controller thread.  We defer errors for one
  // second in case they are the result of a device change; delay chosen to
  // exceed duration of device changes which take a few hundred milliseconds.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OutputController::ReportError, weak_this_for_stream_),
      base::TimeDelta::FromSeconds(1));
}

void OutputController::StopCloseAndClearStream() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Allow calling unconditionally and bail if we don't have a stream_ to close.
  if (stream_) {
    // Ensure any pending tasks, specific to the stream_, are canceled.
    weak_factory_for_stream_.InvalidateWeakPtrs();

    // De-register from state change callbacks if stream_ was created via
    // AudioManager.
      audio_manager_->RemoveOutputDeviceChangeListener(this);

    StopStream();
    stream_->Close();
    stats_tracker_.reset();

    stream_ = NULL;
  }

  state_ = kEmpty;
}

const base::UnguessableToken& OutputController::GetGroupId() {
  return group_id_;
}

const media::AudioParameters& OutputController::GetAudioParameters() {
  return params_;
}

void OutputController::StartSnooping(Snooper* snooper) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(snooper);

  if (snoopers_.empty())
    should_duplicate_.Increment();
  DCHECK(!base::ContainsValue(snoopers_, snooper));
  snoopers_.push_back(snooper);
}

void OutputController::StopSnooping(Snooper* snooper) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  const auto it = std::find(snoopers_.begin(), snoopers_.end(), snooper);
  DCHECK(it != snoopers_.end());
  snoopers_.erase(it);
  if (snoopers_.empty())
    should_duplicate_.Decrement();
}

void OutputController::StartMuting() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (disable_local_output_)
    return;
  disable_local_output_ = true;

  // If there is an active |stream_| that plays out audio locally, invoke a
  // device change to switch to a fake AudioOutputStream for muting.
  if (state_ != kClosed && stream_)
    OnDeviceChange();
}

void OutputController::StopMuting() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!disable_local_output_)
    return;
  disable_local_output_ = false;

  // If there is an active |stream_| and it is the fake stream for muting,
  // invoke a device change to switch back to the normal AudioOutputStream.
  if (state_ != kClosed && stream_)
    OnDeviceChange();
}

void OutputController::OnDeviceChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AudioOutputController.DeviceChangeTime");
  TRACE_EVENT0("audio", "OutputController::OnDeviceChange");

  auto state_to_string = [](State state) {
    switch (state) {
      case kEmpty:
        return "empty";
      case kCreated:
        return "created";
      case kPlaying:
        return "playing";
      case kPaused:
        return "paused";
      case kClosed:
        return "closed";
      case kError:
        return "error";
    }
    return "unknown";
  };

  handler_->OnLog(
      base::StringPrintf("OutputController::OnDeviceChange while in state: %s",
                         state_to_string(state_)));

  // TODO(dalecurtis): Notify the renderer side that a device change has
  // occurred.  Currently querying the hardware information here will lead to
  // crashes on OSX.  See http://crbug.com/158170.

  // Recreate the stream (Create() will first shut down an existing stream).
  // Exit if we ran into an error.
  const State original_state = state_;
  Create(true);
  if (!stream_ || state_ == kError)
    return;

  // Get us back to the original state or an equivalent state.
  switch (original_state) {
    case kPlaying:
      Play();
      return;
    case kCreated:
    case kPaused:
      // From the outside these two states are equivalent.
      return;
    default:
      NOTREACHED() << "Invalid original state.";
  }
}


std::pair<float, bool> OutputController::ReadCurrentPowerAndClip() {
  DCHECK(will_monitor_audio_levels());
  return power_monitor_.ReadCurrentPowerAndClip();
}

}  // namespace audio
