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

#ifndef PLATFORM_IMPL_LINUX_BLUETOOTH_PAIRING_H_
#define PLATFORM_IMPL_LINUX_BLUETOOTH_PAIRING_H_

#include <memory>
#include <string>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <systemd/sd-bus.h>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_classic_device.h"

namespace nearby {
namespace linux {
class BluetoothPairing final : public api::BluetoothPairing {
 public:
  BluetoothPairing(BluetoothAdapter &adapter,
                   std::shared_ptr<BluetoothDevice> remote_device);

  bool InitiatePairing(api::BluetoothPairingCallback pairing_cb) override;
  bool FinishPairing(std::optional<absl::string_view> pin_code) override;
  bool CancelPairing() override;
  bool Unpair() override;
  bool IsPaired() override;

 private:
  void pairing_reply_handler(const sdbus::Error *e);

  sdbus::PendingAsyncCall pair_async_call_;

  std::shared_ptr<BluetoothDevice> device_;
  sdbus::ObjectPath device_object_path_;
  linux::BluetoothAdapter adapter_;

  api::BluetoothPairingCallback pairing_cb_;
};
}  // namespace linux
}  // namespace nearby

#endif
