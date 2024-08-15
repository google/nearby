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

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/platform/mutex_lock.h"
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

std::unique_ptr<Frame> DecodeFrame(absl::Span<const uint8_t> data) {
  auto frame = std::make_unique<Frame>();

  if (frame->ParseFromArray(data.data(), data.size())) {
    return frame;
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
  {
    MutexLock lock(&mutex_);
    if (!read_frame_info_queue_.empty()) {
      ReadFrameInfo read_fame_info{std::nullopt, std::move(callback),
                                   std::nullopt};
      read_frame_info_queue_.push(std::move(read_fame_info));
      return;
    }

    // Check in the cache for frame.
    std::optional<V1Frame> cached_frame = GetCachedFrame(std::nullopt);
    if (cached_frame.has_value()) {
      callback(std::move(cached_frame));
      return;
    }

    ReadFrameInfo read_fame_info{std::nullopt, std::move(callback),
                                 std::nullopt};
    read_frame_info_queue_.push(std::move(read_fame_info));
  }
  ReadNextFrame();
}

void IncomingFramesReader::ReadFrame(
    FrameType frame_type, std::function<void(std::optional<V1Frame>)> callback,
    absl::Duration timeout) {
  {
    MutexLock lock(&mutex_);
    if (!read_frame_info_queue_.empty()) {
      ReadFrameInfo read_fame_info{frame_type, std::move(callback), timeout};
      read_frame_info_queue_.push(std::move(read_fame_info));
      return;
    }

    // Check in the cache for frame.
    std::optional<V1Frame> cached_frame = GetCachedFrame(frame_type);
    if (cached_frame.has_value()) {
      callback(std::move(cached_frame));
      return;
    }

    ReadFrameInfo read_fame_info{frame_type, std::move(callback), timeout};
    read_frame_info_queue_.push(std::move(read_fame_info));

    timeout_timer_ = std::make_unique<ThreadTimer>(
        service_thread_, "frame_reader_timeout", timeout,
        [reader = GetWeakPtr()]() {
          auto frame_reader = reader.lock();
          if (frame_reader == nullptr) {
            NL_LOG(WARNING) << "IncomingFramesReader is released before.";
            return;
          }
          frame_reader->OnTimeout();
        });
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
        frame_reader->OnDataReadFromConnection(std::move(bytes));
      });
}

void IncomingFramesReader::OnTimeout() {
  NL_LOG(WARNING) << __func__ << ": Timed out reading from NearbyConnection.";
  CloseAllPendingReads();
}

void IncomingFramesReader::OnDataReadFromConnection(
    std::optional<std::vector<uint8_t>> bytes) {
  if (!bytes.has_value()) {
    NL_LOG(WARNING) << __func__ << ": Failed to read frame";
    CloseAllPendingReads();
    return;
  }

  std::unique_ptr<Frame> frame =
      DecodeFrame(absl::MakeSpan(bytes->data(), bytes->size()));
  if (frame == nullptr) {
    NL_LOG(WARNING)
        << __func__
        << ": Cannot decode frame. Not currently bound to nearby process";
    CloseAllPendingReads();
    return;
  }

  const V1Frame* v1_frame = OnFrameDecoded(*frame);
  if (v1_frame != nullptr) {
    Done(*v1_frame);
  }
}

const V1Frame* IncomingFramesReader::OnFrameDecoded(const Frame& frame) {
  if (frame.version() != Frame::V1) {
    NL_VLOG(1) << __func__ << ": Frame read does not have V1Frame";
    ReadNextFrame();
    return nullptr;
  }
  auto v1_frame = frame.v1();
  FrameType v1_frame_type = v1_frame.type();
  bool cached_frame = false;
  {
    MutexLock lock(&mutex_);
    if (read_frame_info_queue_.empty()) {
      return nullptr;
    }
    const ReadFrameInfo& frame_info = read_frame_info_queue_.front();
    if (frame_info.frame_type.has_value() &&
        *frame_info.frame_type != v1_frame_type) {
      NL_LOG(WARNING) << __func__ << ": Failed to read frame of type "
                      << *frame_info.frame_type << ", but got frame of type "
                      << v1_frame_type << ". Cached for later.";
      cached_frames_.insert({v1_frame_type, v1_frame});
      cached_frame = true;
    }
  }
  if (cached_frame) {
    ReadNextFrame();
    return nullptr;
  }
  return &frame.v1();
}

void IncomingFramesReader::CloseAllPendingReads() {
  std::queue<ReadFrameInfo> queue;
  {
    MutexLock lock(&mutex_);
    queue.swap(read_frame_info_queue_);
  }
  while (!queue.empty()) {
    ReadFrameInfo read_frame_info = std::move(queue.front());
    queue.pop();
    read_frame_info.callback(std::nullopt);
  }
}

void IncomingFramesReader::Done(const V1Frame& frame) {
  ReadFrameInfo read_frame_info;
  {
    MutexLock lock(&mutex_);
    timeout_timer_.reset();
    read_frame_info = std::move(read_frame_info_queue_.front());
    read_frame_info_queue_.pop();
  }
  read_frame_info.callback(std::move(frame));

  {
    MutexLock lock(&mutex_);
    if (read_frame_info_queue_.empty()) {
      return;
    }
    read_frame_info = std::move(read_frame_info_queue_.front());
    read_frame_info_queue_.pop();
  }

  if (read_frame_info.timeout.has_value()) {
    ReadFrame(*read_frame_info.frame_type,
              std::move(read_frame_info.callback), *read_frame_info.timeout);
  } else {
    ReadFrame(std::move(read_frame_info.callback));
  }
}

std::optional<V1Frame> IncomingFramesReader::GetCachedFrame(
    std::optional<nearby::sharing::service::proto::V1Frame_FrameType>
        frame_type) {
  NL_VLOG(1) << __func__ << ": Fetching cached frame";
  if (frame_type.has_value())
    NL_VLOG(1) << __func__ << ": Requested frame type - " << *frame_type;

  auto iter = frame_type.has_value() ? cached_frames_.find(*frame_type)
                                     : cached_frames_.begin();

  if (iter == cached_frames_.end()) return std::nullopt;

  NL_VLOG(1) << __func__ << ": Successfully read cached frame";
  std::optional<V1Frame> frame = std::move(iter->second);
  cached_frames_.erase(iter);
  return frame;
}

}  // namespace sharing
}  // namespace nearby
