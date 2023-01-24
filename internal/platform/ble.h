// Copyright 2020 Google LLC
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

#ifndef PLATFORM_PUBLIC_BLE_H_
#define PLATFORM_PUBLIC_BLE_H_

#include "absl/container/flat_hash_map.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"

namespace nearby {

class BleSocket final {
 public:
  BleSocket() = default;
  BleSocket(const BleSocket&) = default;
  BleSocket& operator=(const BleSocket&) = default;
  explicit BleSocket(api::BleSocket* socket) : impl_(socket) {}
  explicit BleSocket(std::unique_ptr<api::BleSocket> socket)
      : impl_(socket.release()) {}
  ~BleSocket() = default;

  // Returns the InputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  InputStream& GetInputStream() { return impl_->GetInputStream(); }

  // Returns the OutputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  OutputStream& GetOutputStream() { return impl_->GetOutputStream(); }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() { return impl_->Close(); }

  BlePeripheral GetRemotePeripheral() {
    return BlePeripheral(impl_->GetRemotePeripheral());
  }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by BleMedium::Connect
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while BleSocket object is
  // itself valid. Typically BleSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::BleSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::BleSocket> impl_;
};

// Container of operations that can be performed over the BLE medium.
class BleMedium final {
 public:
  using Platform = api::ImplementationPlatform;
  struct DiscoveredPeripheralCallback {
    absl::AnyInvocable<void(
        BlePeripheral& peripheral, const std::string& service_id,
        const ByteArray& advertisement_bytes, bool fast_advertisement)>
        peripheral_discovered_cb =
            DefaultCallback<BlePeripheral&, const std::string&,
                            const ByteArray&, bool>();
    absl::AnyInvocable<void(BlePeripheral& peripheral,
                            const std::string& service_id)>
        peripheral_lost_cb =
            DefaultCallback<BlePeripheral&, const std::string&>();
  };
  struct ScanningInfo {
    BlePeripheral peripheral;
  };

  struct AcceptedConnectionCallback {
    absl::AnyInvocable<void(BleSocket& socket, const std::string& service_id)>
        accepted_cb = DefaultCallback<BleSocket&, const std::string&>();
  };
  struct AcceptedConnectionInfo {
    BleSocket socket;
  };

  explicit BleMedium(BluetoothAdapter& adapter)
      : impl_(Platform::CreateBleMedium(adapter.GetImpl())),
        adapter_(adapter) {}
  ~BleMedium() = default;

  // Returns true once the BLE advertising has been initiated.
  bool StartAdvertising(const std::string& service_id,
                        const ByteArray& advertisement_bytes,
                        const std::string& fast_advertisement_service_uuid);
  bool StopAdvertising(const std::string& service_id);

  // Returns true once the BLE scan has been initiated.
  bool StartScanning(const std::string& service_id,
                     const std::string& fast_advertisement_service_uuid,
                     DiscoveredPeripheralCallback callback);

  // Returns true once BLE scanning for service_id is well and truly stopped;
  // after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to StartScanning() for service_id.
  bool StopScanning(const std::string& service_id);

  // Returns true once BLE socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback);
  bool StopAcceptingConnections(const std::string& service_id);

  // Returns a new BleSocket. On Success, BleSocket::IsValid()
  // returns true.
  BleSocket Connect(BlePeripheral& peripheral, const std::string& service_id,
                    CancellationFlag* cancellation_flag);

  bool IsValid() const { return impl_ != nullptr; }

  api::BleMedium& GetImpl() { return *impl_; }
  BluetoothAdapter& GetAdapter() { return adapter_; }

 private:
  Mutex mutex_;
  std::unique_ptr<api::BleMedium> impl_;
  BluetoothAdapter& adapter_;
  absl::flat_hash_map<api::BlePeripheral*, std::unique_ptr<ScanningInfo>>
      peripherals_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<api::BleSocket*, std::unique_ptr<AcceptedConnectionInfo>>
      sockets_ ABSL_GUARDED_BY(mutex_);
  DiscoveredPeripheralCallback discovered_peripheral_callback_
      ABSL_GUARDED_BY(mutex_);
  AcceptedConnectionCallback accepted_connection_callback_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_BLE_H_
