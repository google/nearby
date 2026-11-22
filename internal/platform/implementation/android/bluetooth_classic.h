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

#ifndef PLATFORM_IMPL_ANDROID_BLUETOOTH_CLASSIC_H_
#define PLATFORM_IMPL_ANDROID_BLUETOOTH_CLASSIC_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace android {

class BluetoothDevice : public api::BluetoothDevice {
 public:
  ~BluetoothDevice() override = default;

  std::string GetName() const override;

  std::string GetMacAddress() const override;
};

class BluetoothSocket : public api::BluetoothSocket {
 public:
  ~BluetoothSocket() override = default;

  InputStream& GetInputStream() override;

  OutputStream& GetOutputStream() override;

  Exception Close() override;

  api::BluetoothDevice* GetRemoteDevice() override;
};

class BluetoothServerSocket : public api::BluetoothServerSocket {
 public:
  ~BluetoothServerSocket() override = default;

  std::unique_ptr<api::BluetoothSocket> Accept() override;

  Exception Close() override;
};

class BluetoothPairing : public api::BluetoothPairing {
 public:
  ~BluetoothPairing() override = default;

  bool InitiatePairing(api::BluetoothPairingCallback pairing_cb) override;

  bool FinishPairing(std::optional<absl::string_view> pin_code) override;

  bool CancelPairing() override;

  bool Unpair() override;

  bool IsPaired() override;
};

class BluetoothClassicMedium : public api::BluetoothClassicMedium {
 public:
  ~BluetoothClassicMedium() override = default;

  bool StartDiscovery(DiscoveryCallback discovery_callback) override;

  bool StopDiscovery() override;

  std::unique_ptr<api::BluetoothSocket> ConnectToService(
     api::BluetoothDevice& remote_device, const std::string& service_uuid,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::BluetoothServerSocket> ListenForService(
      const std::string& service_name,
      const std::string& service_uuid) override;

  std::unique_ptr<api::BluetoothPairing> CreatePairing(
      api::BluetoothDevice& remote_device) override;

  api::BluetoothDevice* GetRemoteDevice(
      const std::string& mac_address) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
};

}  // namespace android
}  // namespace nearby

#endif  // PLATFORM_IMPL_ANDROID_BLUETOOTH_CLASSIC_H_
