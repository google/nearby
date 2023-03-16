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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_PROVIDER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_PROVIDER_H_

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
#include "fastpair/message_stream/message.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

// Fake BT device with the Provider role. Tailored to testing message stream.
class FakeProvider {
 public:
  ~FakeProvider() { Shutdown(); }

  void Shutdown() { provider_thread_.Shutdown(); }

  void DiscoverProvider(BluetoothClassicMedium& seeker_medium) {
    CountDownLatch found_latch(1);
    seeker_medium.StartDiscovery(BluetoothClassicMedium::DiscoveryCallback{
        .device_discovered_cb =
            [&](BluetoothDevice& device) {
              NEARBY_LOG(INFO, "Device discovered: %s",
                         device.GetName().c_str());
              found_latch.CountDown();
            },
    });
    provider_adapter_.SetScanMode(
        BluetoothAdapter::ScanMode::kConnectableDiscoverable);
    ASSERT_TRUE(found_latch.Await().Ok());
  }

  void EnableProviderRfcomm() {
    std::string service_name{"service"};
    std::string uuid(kRfcommUuid);
    provider_server_socket_ =
        provider_medium_.ListenForService(service_name, uuid);
    provider_thread_.Execute(
        [this]() { provider_socket_ = provider_server_socket_.Accept(); });
  }

  Future<std::string> ReadProviderBytes(size_t num_bytes) {
    Future<std::string> result;
    provider_thread_.Execute([this, result, num_bytes]() mutable {
      if (!provider_socket_.IsValid()) {
        result.SetException({Exception::kIo});
        return;
      }
      ExceptionOr<ByteArray> bytes =
          provider_socket_.GetInputStream().Read(num_bytes);
      if (bytes.ok()) {
        result.Set(std::string(bytes.GetResult().AsStringView()));
      } else {
        result.SetException(bytes.GetException());
      }
    });
    return result;
  }

  void WriteProviderBytes(std::string bytes) {
    CountDownLatch latch(1);
    provider_thread_.Execute([&, data = ByteArray(bytes)]() {
      if (provider_socket_.IsValid()) {
        provider_socket_.GetOutputStream().Write(data);
      }
      latch.CountDown();
    });
    latch.Await();
  }

  void DisableProviderRfcomm() {
    if (provider_server_socket_.IsValid()) {
      provider_server_socket_.Close();
    }
    provider_thread_.Execute([this]() {
      if (provider_socket_.IsValid()) {
        provider_socket_.Close();
      }
    });
  }

  std::string GetMacAddress() const {
    return provider_adapter_.GetMacAddress();
  }

 private:
  BluetoothAdapter provider_adapter_;
  BluetoothClassicMedium provider_medium_{provider_adapter_};
  BluetoothServerSocket provider_server_socket_;
  BluetoothSocket provider_socket_;
  SingleThreadExecutor provider_thread_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_FAKE_PROVIDER_H_
