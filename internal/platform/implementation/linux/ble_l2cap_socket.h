// Copyright 2024 Google LLC
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

#ifndef PLATFORM_IMPL_LINUX_BLE_L2CAP_SOCKET_H_
#define PLATFORM_IMPL_LINUX_BLE_L2CAP_SOCKET_H_

#include <atomic>
#include <memory>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace linux {

class BleL2capInputStream final : public InputStream {
 public:
  explicit BleL2capInputStream(int fd);
  ~BleL2capInputStream() override;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  std::atomic<int> fd_{-1};
  std::string pending_;  // holds unread bytes from full SDUs
  };

class BleL2capOutputStream final : public OutputStream {
 public:
  explicit BleL2capOutputStream(int fd);
  ~BleL2capOutputStream() override;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override { return {Exception::kSuccess}; }
  Exception Close() override;

 private:
  mutable absl::Mutex fd_mutex_;
  std::atomic<int> fd_{-1};
};

class BleL2capSocket final : public api::ble_v2::BleL2capSocket {
 public:
  BleL2capSocket(int fd, api::ble_v2::BlePeripheral::UniqueId peripheral_id);
  ~BleL2capSocket() override;

  InputStream& GetInputStream() override { return *input_stream_; }
  OutputStream& GetOutputStream() override { return *output_stream_; }
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  api::ble_v2::BlePeripheral::UniqueId GetRemotePeripheralId() override {
    return peripheral_id_;
  }

  bool IsClosed() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  void DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  std::unique_ptr<BleL2capInputStream> input_stream_;
  std::unique_ptr<BleL2capOutputStream> output_stream_;
  api::ble_v2::BlePeripheral::UniqueId peripheral_id_;
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_BLE_L2CAP_SOCKET_H_
