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

#ifndef THIRD_PARTY_NEARBY_SHARING_INCOMING_FRAMES_READER_H_
#define THIRD_PARTY_NEARBY_SHARING_INCOMING_FRAMES_READER_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "absl/time/time.h"
#include "internal/platform/mutex.h"
#include "internal/platform/timer.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_sharing_decoder.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {

// Helper class to read incoming frames from Nearby devices.
class IncomingFramesReader
    : public std::enable_shared_from_this<IncomingFramesReader> {
 public:
  IncomingFramesReader(Context* context, NearbySharingDecoder* decoder,
                       NearbyConnection* connection);
  virtual ~IncomingFramesReader();
  IncomingFramesReader(const IncomingFramesReader&) = delete;
  IncomingFramesReader& operator=(IncomingFramesReader&) = delete;

  // Reads an incoming frame from |connection|. |callback| is called
  // with the frame read from connection or nullopt if connection socket is
  // closed.
  //
  // Note: Callers are expected wait for |callback| to be run before scheduling
  // subsequent calls to ReadFrame(..).
  virtual void ReadFrame(
      std::function<
          void(std::optional<nearby::sharing::service::proto::V1Frame>)>
          callback);

  // Reads a frame of type |frame_type| from |connection|. |callback| is called
  // with the frame read from connection or nullopt if connection socket is
  // closed or |timeout| units of time have passed.
  //
  // Note: Callers are expected wait for |callback| to be run before scheduling
  // subsequent calls to ReadFrame(..).
  virtual void ReadFrame(
      nearby::sharing::service::proto::V1Frame_FrameType frame_type,
      std::function<
          void(std::optional<nearby::sharing::service::proto::V1Frame>)>
          callback,
      absl::Duration timeout);

  std::weak_ptr<IncomingFramesReader> GetWeakPtr() {
    return this->weak_from_this();
  }

 private:
  struct ReadFrameInfo {
    std::optional<nearby::sharing::service::proto::V1Frame_FrameType>
        frame_type = std::nullopt;
    std::function<void(std::optional<nearby::sharing::service::proto::V1Frame>)>
        callback = nullptr;
    std::optional<absl::Duration> timeout = std::nullopt;
  };

  void ReadNextFrame();
  void OnDataReadFromConnection(std::optional<std::vector<uint8_t>> bytes);
  void OnFrameDecoded(
      std::optional<nearby::sharing::service::proto::Frame> frame);
  void OnTimeout();
  void Done(std::optional<nearby::sharing::service::proto::V1Frame> frame);
  std::optional<nearby::sharing::service::proto::V1Frame> GetCachedFrame(
      std::optional<nearby::sharing::service::proto::V1Frame_FrameType>
          frame_type);

  NearbyConnection* connection_;
  NearbySharingDecoder* decoder_ = nullptr;

  RecursiveMutex mutex_;
  std::queue<ReadFrameInfo> read_frame_info_queue_;
  std::function<void()> timeout_callback_;

  // Caches frames read from NearbyConnection which are not used immediately.
  std::map<nearby::sharing::service::proto::V1Frame_FrameType,
           std::optional<nearby::sharing::service::proto::V1Frame>>
      cached_frames_;

  std::unique_ptr<Timer> timeout_timer_ = nullptr;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INCOMING_FRAMES_READER_H_
