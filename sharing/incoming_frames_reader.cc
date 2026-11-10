// Copyright 2022-2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sharing/incoming_frames_reader.h"

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/task_runner.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/thread_timer.h"

namespace nearby::sharing {
namespace {

using FrameType = ::nearby::sharing::service::proto::V1Frame_FrameType;
using V1Frame = ::nearby::sharing::service::proto::V1Frame;
using Frame = ::nearby::sharing::service::proto::Frame;

std::unique_ptr<V1Frame> DecodeV1Frame(const std::vector<uint8_t>& data) {
  auto frame = std::make_unique<Frame>();

  if (frame->ParseFromArray(data.data(), data.size()) &&
      frame->version() == Frame::V1) {
    return absl::WrapUnique(frame->release_v1());
  } else {
    return nullptr;
  }
}

}  // namespace

IncomingFramesReader::IncomingFramesReader(TaskRunner& service_thread,
                                           NearbyConnection* connection)
    : service_thread_(service_thread), connection_(connection) {
  DCHECK(connection);
}

IncomingFramesReader::~IncomingFramesReader() {
  VLOG(1) << "~IncomingFramesReader is called";
  std::optional<ReadFrameInfo> next_read_frame_info;
  {
    absl::MutexLock lock(mutex_);
    next_read_frame_info.swap(next_read_frame_info_);
    timeout_timer_.reset();
  }
  if (next_read_frame_info.has_value()) {
    service_thread_.PostTask(
        [callback = std::move(next_read_frame_info->callback)]() mutable {
          std::move(callback)(/*is_timeout=*/false, std::nullopt);
        });
  }
}

void IncomingFramesReader::ReadFrame(
    absl::AnyInvocable<void(bool is_timeout, std::optional<V1Frame>)> callback,
    absl::Duration timeout) {
  AddNewRequest(std::nullopt, std::move(callback), timeout);
}

void IncomingFramesReader::ReadFrame(
    FrameType frame_type,
    absl::AnyInvocable<void(bool is_timeout, std::optional<V1Frame>)> callback,
    absl::Duration timeout) {
  AddNewRequest(frame_type, std::move(callback), timeout);
}

void IncomingFramesReader::AddNewRequest(
    std::optional<FrameType> frame_type,
    absl::AnyInvocable<void(bool is_timeout, std::optional<V1Frame>)> callback,
    absl::Duration timeout) {
  {
    absl::MutexLock lock(mutex_);
    if (next_read_frame_info_.has_value()) {
      LOG(DFATAL) << __func__ << ": Overwriting previous read frame request.";
      next_read_frame_info_.reset();
      timeout_timer_.reset();
    }
    // Check in the cache for frame.
    std::unique_ptr<V1Frame> cached_frame = PopCachedFrame(frame_type);
    if (cached_frame == nullptr) {
      next_read_frame_info_ = ReadFrameInfo{.frame_type = frame_type,
                                            .callback = std::move(callback)};
      if (timeout != absl::ZeroDuration()) {
        timeout_timer_ = std::make_unique<ThreadTimer>(
            service_thread_, "frame_reader_timeout", timeout,
            [reader = GetWeakPtr()]() {
              std::shared_ptr<IncomingFramesReader> frame_reader =
                  reader.lock();
              if (frame_reader == nullptr) {
                LOG(WARNING)
                    << "IncomingFramesReader has already been released "
                       "before read timeout.";
                return;
              }
              frame_reader->OnTimeout();
            });
      }
    } else {
      // Use the cached frame and make the callback on the service thread.
      MakeFrameCallbackOnServiceThread(std::move(callback),
                                       std::move(cached_frame));
      return;
    }
  }
  ReadNextFrame();
}

void IncomingFramesReader::MakeFrameCallbackOnServiceThread(
    absl::AnyInvocable<void(bool, std::optional<V1Frame>)> callback,
    std::unique_ptr<V1Frame> frame) {
  service_thread_.PostTask(
      [callback = std::move(callback), frame = std::move(frame)]() mutable {
        std::move(callback)(/*is_timeout=*/false, std::move(*frame));
      });
}

void IncomingFramesReader::ReadNextFrame() {
  connection_->Read(
      [reader = GetWeakPtr()](std::optional<std::vector<uint8_t>> bytes) {
        auto frame_reader = reader.lock();
        if (frame_reader == nullptr) {
          LOG(WARNING) << "IncomingFramesReader is released before.";
          return;
        }
        if (!bytes.has_value()) {
          LOG(WARNING) << __func__ << ": Failed to read frame";
          // Schedule ClosePendingRead on service thread.
          frame_reader->service_thread_.PostTask(
              [reader = frame_reader->GetWeakPtr()]() {
                auto frame_reader = reader.lock();
                if (frame_reader == nullptr) {
                  return;
                }
                frame_reader->ClosePendingRead(/*is_timeout=*/false);
              });
          return;
        }
        frame_reader->OnDataReadFromConnection(*bytes);
      });
}

void IncomingFramesReader::OnTimeout() {
  LOG(WARNING) << __func__ << ": Timed out reading from NearbyConnection.";
  ClosePendingRead(/*is_timeout=*/true);
}

void IncomingFramesReader::OnDataReadFromConnection(
    const std::vector<uint8_t>& bytes) {
  std::unique_ptr<V1Frame> frame = DecodeV1Frame(bytes);
  if (frame == nullptr) {
    LOG(WARNING)
        << __func__
        << ": Cannot decode frame. Not currently bound to nearby process";
    ReadNextFrame();
    return;
  }
  FrameType frame_type = frame->type();
  bool cached_frame = false;
  ReadFrameInfo next_read_frame_info;
  {
    absl::MutexLock lock(mutex_);
    if (!next_read_frame_info_.has_value()) {
      // Drop the frame if no one is waiting.
      return;
    }
    if (next_read_frame_info_->frame_type.has_value() &&
        *next_read_frame_info_->frame_type != frame_type) {
      LOG(WARNING) << __func__ << ": Failed to read frame of type "
                   << *next_read_frame_info_->frame_type
                   << ", but got frame of type " << frame_type
                   << ". Cached for later.";
      cached_frames_.push_back(std::move(frame));
      cached_frame = true;
    } else {
      // Deliver the frame to the caller.
      next_read_frame_info = std::move(*next_read_frame_info_);
      next_read_frame_info_.reset();
      timeout_timer_.reset();
    }
  }
  if (cached_frame) {
    ReadNextFrame();
    return;
  }
  MakeFrameCallbackOnServiceThread(std::move(next_read_frame_info.callback),
                                   std::move(frame));
}

void IncomingFramesReader::ClosePendingRead(bool is_timeout) {
  std::optional<ReadFrameInfo> next_read_frame_info;
  {
    absl::MutexLock lock(mutex_);
    next_read_frame_info.swap(next_read_frame_info_);
    timeout_timer_.reset();
  }
  if (next_read_frame_info.has_value()) {
    next_read_frame_info->callback(is_timeout, std::nullopt);
  }
}

std::unique_ptr<V1Frame> IncomingFramesReader::PopCachedFrame(
    std::optional<V1Frame::FrameType> frame_type) {
  VLOG(1) << __func__ << ": Fetching cached frame";
  if (cached_frames_.empty()) {
    return nullptr;
  }
  if (!frame_type.has_value()) {
    std::unique_ptr<V1Frame> frame = std::move(cached_frames_.front());
    cached_frames_.pop_front();
    return frame;
  }
  VLOG(1) << __func__ << ": Requested frame type - " << *frame_type;

  auto iter =
      std::find_if(cached_frames_.begin(), cached_frames_.end(),
                   [frame_type = *frame_type](std::unique_ptr<V1Frame>& frame) {
                     return frame->type() == frame_type;
                   });

  if (iter == cached_frames_.end()) return nullptr;
  VLOG(1) << __func__ << ": Successfully read cached frame";
  std::unique_ptr<V1Frame> frame = std::move(*iter);
  cached_frames_.erase(iter);
  return frame;
}

}  // namespace nearby::sharing
