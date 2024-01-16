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
#include <ostream>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "internal/platform/mutex_lock.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_sharing_decoder.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {
namespace {

using FrameType = ::nearby::sharing::service::proto::V1Frame_FrameType;
using V1Frame = ::nearby::sharing::service::proto::V1Frame;
using Frame = ::nearby::sharing::service::proto::Frame;

std::ostream& operator<<(std::ostream& out, const FrameType& obj) {
  out << static_cast<std::underlying_type<FrameType>::type>(obj);
  return out;
}

}  // namespace

IncomingFramesReader::IncomingFramesReader(Context* context,
                                           NearbySharingDecoder* decoder,
                                           NearbyConnection* connection)
    : connection_(connection), decoder_(decoder) {
  NL_DCHECK(decoder);
  NL_DCHECK(connection);
  timeout_timer_ = context->CreateTimer();
}

IncomingFramesReader::~IncomingFramesReader() {
  MutexLock lock(&mutex_);
  NL_LOG(INFO) << "~IncomingFramesReader is called";
  Done(std::nullopt);
}

void IncomingFramesReader::ReadFrame(
    std::function<void(std::optional<V1Frame>)> callback) {
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

  ReadFrameInfo read_fame_info{std::nullopt, std::move(callback), std::nullopt};
  read_frame_info_queue_.push(std::move(read_fame_info));
  ReadNextFrame();
}

void IncomingFramesReader::ReadFrame(
    FrameType frame_type, std::function<void(std::optional<V1Frame>)> callback,
    absl::Duration timeout) {
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

  if (timeout_timer_->IsRunning()) {
    timeout_timer_->Stop();
  }

  timeout_timer_->Start(
      timeout / absl::Milliseconds(1), 0, [&, reader = GetWeakPtr()]() {
        auto frame_reader = reader.lock();
        if (frame_reader == nullptr) {
          NL_LOG(WARNING) << "IncomingFramesReader is released before.";
          return;
        }
        OnTimeout();
      });

  ReadNextFrame();
}

void IncomingFramesReader::ReadNextFrame() {
  connection_->Read(
      [&, reader = GetWeakPtr()](std::optional<std::vector<uint8_t>> bytes) {
        auto frame_reader = reader.lock();
        if (frame_reader == nullptr) {
          NL_LOG(WARNING) << "IncomingFramesReader is released before.";
          return;
        }

        OnDataReadFromConnection(std::move(bytes));
      });
}

void IncomingFramesReader::OnTimeout() {
  MutexLock lock(&mutex_);
  NL_LOG(WARNING) << __func__ << ": Timed out reading from NearbyConnection.";
  Done(std::nullopt);
}

void IncomingFramesReader::OnDataReadFromConnection(
    std::optional<std::vector<uint8_t>> bytes) {
  MutexLock lock(&mutex_);
  if (read_frame_info_queue_.empty()) {
    return;
  }

  if (!bytes.has_value()) {
    NL_LOG(WARNING) << __func__ << ": Failed to read frame";
    Done(std::nullopt);
    return;
  }

  std::unique_ptr<Frame> frame =
      decoder_->DecodeFrame(absl::MakeSpan(bytes->data(), bytes->size()));
  if (frame == nullptr) {
    NL_LOG(WARNING)
        << __func__
        << ": Cannot decode frame. Not currently bound to nearby process";
    Done(std::nullopt);
    return;
  }

  OnFrameDecoded(std::move(*frame));
}

void IncomingFramesReader::OnFrameDecoded(std::optional<Frame> frame) {
  if (!frame.has_value()) {
    ReadNextFrame();
    return;
  }

  if (frame->version() != Frame::V1) {
    NL_VLOG(1) << __func__ << ": Frame read does not have V1Frame";
    ReadNextFrame();
    return;
  }

  auto v1_frame = frame->v1();
  FrameType v1_frame_type = v1_frame.type();

  const ReadFrameInfo& frame_info = read_frame_info_queue_.front();
  if (frame_info.frame_type.has_value() &&
      *frame_info.frame_type != v1_frame_type) {
    NL_LOG(WARNING) << __func__ << ": Failed to read frame of type "
                    << *frame_info.frame_type << ", but got frame of type "
                    << v1_frame_type << ". Cached for later.";
    cached_frames_.insert({v1_frame_type, std::move(v1_frame)});
    ReadNextFrame();
    return;
  }

  Done(std::move(v1_frame));
}

void IncomingFramesReader::Done(std::optional<V1Frame> frame) {
  if (read_frame_info_queue_.empty()) {
    return;
  }

  if (timeout_timer_ != nullptr) {
    timeout_timer_->Stop();
  }

  bool is_empty_frame = !frame.has_value();
  ReadFrameInfo read_frame_info = std::move(read_frame_info_queue_.front());
  read_frame_info_queue_.pop();
  read_frame_info.callback(std::move(frame));

  if (is_empty_frame) {
    // should complete all pending readers.
    while (!read_frame_info_queue_.empty()) {
      read_frame_info = std::move(read_frame_info_queue_.front());
      read_frame_info_queue_.pop();
      read_frame_info.callback(std::nullopt);
    }
    return;
  }

  if (!read_frame_info_queue_.empty()) {
    ReadFrameInfo read_frame_info = std::move(read_frame_info_queue_.front());
    read_frame_info_queue_.pop();

    if (read_frame_info.timeout.has_value()) {
      ReadFrame(*read_frame_info.frame_type,
                std::move(read_frame_info.callback), *read_frame_info.timeout);
    } else {
      ReadFrame(std::move(read_frame_info.callback));
    }
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
