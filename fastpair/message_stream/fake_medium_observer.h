// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_MEDIUM_OBSERVER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_MEDIUM_OBSERVER_H_

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/message_stream/medium.h"
#include "fastpair/message_stream/message.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

class FakeMediumObserver : public Medium::Observer {
 public:
  void OnConnectionResult(absl::Status result) override {
    NEARBY_LOGS(INFO) << "OnConnectionResult " << result;
    connection_result_.Set(result);
  }

  void OnDisconnected(absl::Status status) override {
    NEARBY_LOGS(INFO) << "OnDisconnected " << status;
    disconnected_reason_.Set(status);
  }

  void OnReceived(Message message) override {
    NEARBY_LOGS(INFO) << "OnReceived " << message;
    MutexLock lock(&messages_mutex_);
    messages_.push_back(std::move(message));
  }

  std::vector<Message> GetMessages() {
    MutexLock lock(&messages_mutex_);
    return messages_;
  }

  absl::Status WaitForMessages(int message_count, absl::Duration timeout) {
    constexpr absl::Duration kStep = absl::Milliseconds(100);
    while (timeout > absl::ZeroDuration()) {
      std::vector<Message> messages = GetMessages();
      if (messages.size() >= message_count) {
        return absl::OkStatus();
      }
      timeout -= kStep;
      absl::SleepFor(kStep);
    }
    return absl::DeadlineExceededError("timeout waiting for messages");
  }

  Future<absl::Status> connection_result_;
  Future<absl::Status> disconnected_reason_;

 private:
  std::vector<Message> messages_;
  Mutex messages_mutex_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_MEDIUM_OBSERVER_H_
