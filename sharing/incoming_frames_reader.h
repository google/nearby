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
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/task_runner.h"
#include "sharing/nearby_connection.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/thread_timer.h"

namespace nearby {
namespace sharing {

// Helper class to read incoming frames from Nearby devices.
class IncomingFramesReader
    : public std::enable_shared_from_this<IncomingFramesReader> {
 public:
  IncomingFramesReader(TaskRunner& service_thread,
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
          callback) ABSL_LOCKS_EXCLUDED(mutex_);

  // Reads a frame of type |frame_type| from |connection|. |callback| is called
  // with the frame read from connection or nullopt if connection socket is
  // closed or |timeout| units of time have passed.
  //
  // Note: Callers are expected wait for |callback| to be run before scheduling
  // subsequent calls to ReadFrame(..).
  virtual void ReadFrame(
      nearby::sharing::service::proto::V1Frame::FrameType frame_type,
      std::function<
          void(std::optional<nearby::sharing::service::proto::V1Frame>)>
          callback,
      absl::Duration timeout) ABSL_LOCKS_EXCLUDED(mutex_);

  std::weak_ptr<IncomingFramesReader> GetWeakPtr() {
    return this->weak_from_this();
  }

 private:
  struct ReadFrameInfo {
    std::optional<nearby::sharing::service::proto::V1Frame::FrameType>
        frame_type = std::nullopt;
    std::function<void(std::optional<nearby::sharing::service::proto::V1Frame>)>
        callback = nullptr;
    absl::Duration timeout = absl::ZeroDuration();
  };

  void ProcessReadRequest(
      std::optional<nearby::sharing::service::proto::V1Frame::FrameType>
          frame_type,
      std::function<
          void(std::optional<nearby::sharing::service::proto::V1Frame>)>
          callback,
      absl::Duration timeout) ABSL_LOCKS_EXCLUDED(mutex_);
  void CloseAllPendingReads() ABSL_LOCKS_EXCLUDED(mutex_);
  void ReadNextFrame() ABSL_LOCKS_EXCLUDED(mutex_);
  void OnDataReadFromConnection(const std::vector<uint8_t>& bytes)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnTimeout();
  void Done(std::unique_ptr<nearby::sharing::service::proto::V1Frame> frame)
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<nearby::sharing::service::proto::V1Frame> PopCachedFrame(
      std::optional<nearby::sharing::service::proto::V1Frame::FrameType>
          frame_type) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  TaskRunner& service_thread_;
  NearbyConnection* const connection_;

  absl::Mutex mutex_;
  std::queue<ReadFrameInfo> read_frame_info_queue_ ABSL_GUARDED_BY(mutex_);

  // Caches frames read from NearbyConnection which are not used immediately.
  std::list<std::unique_ptr<nearby::sharing::service::proto::V1Frame>>
      cached_frames_ ABSL_GUARDED_BY(mutex_);

  std::unique_ptr<ThreadTimer> timeout_timer_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INCOMING_FRAMES_READER_H_
