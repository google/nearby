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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MEDIUM_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MEDIUM_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/message_stream/message.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/future.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

class Medium {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnConnectionResult(absl::Status result) = 0;

    virtual void OnDisconnected(absl::Status reason) = 0;

    virtual void OnReceived(Message message) = 0;
  };
  Medium(const FastPairDevice& device,
         std::optional<BluetoothClassicMedium*> bt_classic, Observer& observer)
      : device_(device), bt_classic_medium_(bt_classic), observer_(observer) {}
  Medium(Medium&& other) = default;

  ~Medium() {
    NEARBY_LOGS(INFO) << "Destructing FP Medium";
    {
      MutexLock lock(&mutex_);
      cancellation_flag_.Cancel();
      NEARBY_LOGS(INFO) << "Destructing FP Medium 1";
      if (socket_.IsValid()) {
        socket_.Close();
      }
    }
    NEARBY_LOGS(INFO) << "Destructing FP Medium 2";
    executor_.Shutdown();
    NEARBY_LOGS(INFO) << "Destructed FP Medium";
  }

  // Opens RFCOMM connection with the remote party.
  // Returns an error if a connection attempt could not be made.
  // Otherwise, `OnConnectionResult()` will be called with the connection
  // result. When the connection is successful, Medium starts processing
  // messages coming in from the remote party, and calls `OnReceived()` for each
  // complete message.
  absl::Status OpenRfcomm();

  // Opens L2CAP connection with the remote party.
  // Returns an error if a connection attempt could not be made.
  // Otherwise, `OnConnectionResult()` will be called with the connection
  // result. When the connection is successful, Medium starts processing
  // messages coming in from the remote party, and calls `OnReceived()` for each
  // complete message.
  absl::Status OpenL2cap(absl::string_view ble_address);

  absl::Status Disconnect();

  // Returns OK if the message was queued for delivery. It does not mean the
  // message was delivered to the remote party.
  absl::Status Send(Message message, bool compute_and_append_mac = false);

 private:
  void RunLoop(BluetoothSocket socket);
  ByteArray Serialize(Message message, bool compute_and_append_mac);
  void SetSocket(BluetoothSocket socket);
  void CloseSocket();
  BluetoothSocket GetSocket();
  const FastPairDevice& device_;
  std::optional<BluetoothClassicMedium*> bt_classic_medium_;
  Observer& observer_;
  BluetoothSocket socket_ ABSL_GUARDED_BY(mutex_);
  Mutex mutex_;
  CancellationFlag cancellation_flag_;
  SingleThreadExecutor executor_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MEDIUM_H_
