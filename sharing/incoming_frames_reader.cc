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
#include <queue>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/task_runner.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/thread_timer.h"

namespace nearby {
namespace sharing {
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
    : service_thread_(service_thread),
      connection_(connection) {
  NL_DCHECK(connection);
}

IncomingFramesReader::~IncomingFramesReader() {
  NL_LOG(INFO) << "~IncomingFramesReader is called";
  CloseAllPendingReads();
}

void IncomingFramesReader::ReadFrame(
    std::function<void(std::optional<V1Frame>)> callback) {
  ProcessReadRequest(std::nullopt, std::move(callback), absl::ZeroDuration());
}

void IncomingFramesReader::ReadFrame(
    FrameType frame_type, std::function<void(std::optional<V1Frame>)> callback,
    absl::Duration timeout) {
  ProcessReadRequest(frame_type, std::move(callback), timeout);
}

void IncomingFramesReader::ProcessReadRequest(
    std::optional<FrameType> frame_type,
    std::function<void(std::optional<V1Frame>)> callback,
    absl::Duration timeout) {
  std::unique_ptr<V1Frame> cached_frame;
  {
    absl::MutexLock lock(&mutex_);
    if (!read_frame_info_queue_.empty()) {
      // There are already outstanding read requests, just queue this up.
      ReadFrameInfo read_fame_info{frame_type, std::move(callback), timeout};
      read_frame_info_queue_.push(std::move(read_fame_info));
      return;
    }

    // Check in the cache for frame.
    cached_frame = PopCachedFrame(frame_type);
  }
  if (cached_frame) {
    callback(*cached_frame);
    return;
  }
  {
    // No matching cached frame, queue this request, then read more frames.
    absl::MutexLock lock(&mutex_);
    ReadFrameInfo read_frame_info{frame_type, std::move(callback), timeout};
    read_frame_info_queue_.push(std::move(read_frame_info));

    if (timeout != absl::ZeroDuration()) {
    timeout_timer_ = std::make_unique<ThreadTimer>(
        service_thread_, "frame_reader_timeout", timeout,
        [reader = GetWeakPtr()]() {
          auto frame_reader = reader.lock();
          if (frame_reader == nullptr) {
            NL_LOG(WARNING) << "IncomingFramesReader has already been released "
                               "before read timeout.";
            return;
          }
          frame_reader->OnTimeout();
        });
    }
  }
  ReadNextFrame();
}

void IncomingFramesReader::ReadNextFrame() {
  connection_->Read(
      [reader = GetWeakPtr()](std::optional<std::vector<uint8_t>> bytes) {
        auto frame_reader = reader.lock();
        if (frame_reader == nullptr) {
          NL_LOG(WARNING) << "IncomingFramesReader is released before.";
          return;
        }
        if (!bytes.has_value()) {
          NL_LOG(WARNING) << __func__ << ": Failed to read frame";
          frame_reader->CloseAllPendingReads();
          return;
        }
        frame_reader->OnDataReadFromConnection(*bytes);
      });
}

void IncomingFramesReader::OnTimeout() {
  NL_LOG(WARNING) << __func__ << ": Timed out reading from NearbyConnection.";
  CloseAllPendingReads();
}

void IncomingFramesReader::OnDataReadFromConnection(
    const std::vector<uint8_t>& bytes) {
  std::unique_ptr<V1Frame> frame = DecodeV1Frame(bytes);
  if (frame == nullptr) {
    NL_LOG(WARNING)
        << __func__
        << ": Cannot decode frame. Not currently bound to nearby process";
    ReadNextFrame();
    return;
  }
  FrameType frame_type = frame->type();
  bool cached_frame = false;
  {
    absl::MutexLock lock(&mutex_);
    if (read_frame_info_queue_.empty()) {
      return;
    }
    const ReadFrameInfo& frame_info = read_frame_info_queue_.front();
    if (frame_info.frame_type.has_value() &&
        *frame_info.frame_type != frame_type) {
      NL_LOG(WARNING) << __func__ << ": Failed to read frame of type "
                      << *frame_info.frame_type << ", but got frame of type "
                      << frame_type << ". Cached for later.";
      cached_frames_.push_back(std::move(frame));
      cached_frame = true;
    }
  }
  if (cached_frame) {
    ReadNextFrame();
    return;
  }
  Done(std::move(frame));
}

void IncomingFramesReader::CloseAllPendingReads() {
  std::queue<ReadFrameInfo> queue;
  {
    absl::MutexLock lock(&mutex_);
    queue.swap(read_frame_info_queue_);
  }
  while (!queue.empty()) {
    ReadFrameInfo read_frame_info = std::move(queue.front());
    queue.pop();
    read_frame_info.callback(std::nullopt);
  }
}

void IncomingFramesReader::Done(std::unique_ptr<V1Frame> frame) {
  ReadFrameInfo read_frame_info;
  {
    absl::MutexLock lock(&mutex_);
    timeout_timer_.reset();
    read_frame_info = std::move(read_frame_info_queue_.front());
    read_frame_info_queue_.pop();
  }
  read_frame_info.callback(*frame);

  {
    absl::MutexLock lock(&mutex_);
    if (read_frame_info_queue_.empty()) {
      return;
    }
    read_frame_info = std::move(read_frame_info_queue_.front());
    read_frame_info_queue_.pop();
  }

  if (read_frame_info.timeout != absl::ZeroDuration()) {
    ReadFrame(*read_frame_info.frame_type,
              std::move(read_frame_info.callback), read_frame_info.timeout);
  } else {
    ReadFrame(std::move(read_frame_info.callback));
  }
}

std::unique_ptr<V1Frame> IncomingFramesReader::PopCachedFrame(
    std::optional<V1Frame::FrameType> frame_type) {
  NL_VLOG(1) << __func__ << ": Fetching cached frame";
  if (cached_frames_.empty()) {
    return nullptr;
  }
  if (!frame_type.has_value()) {
    std::unique_ptr<V1Frame> frame = std::move(cached_frames_.front());
    cached_frames_.pop_front();
    return frame;
  }
  NL_VLOG(1) << __func__ << ": Requested frame type - " << *frame_type;

  auto iter =
      std::find_if(cached_frames_.begin(), cached_frames_.end(),
                   [frame_type = *frame_type](std::unique_ptr<V1Frame>& frame) {
                     return frame->type() == frame_type;
                   });

  if (iter == cached_frames_.end()) return nullptr;
  NL_VLOG(1) << __func__ << ": Successfully read cached frame";
  std::unique_ptr<V1Frame> frame = std::move(*iter);
  cached_frames_.erase(iter);
  return frame;
}

}  // namespace sharing
}  // namespace nearby
