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

#ifndef CORE_V2_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_
#define CORE_V2_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_

#include <cstdint>
#include <string>

#include "core_v2/internal/mediums/bluetooth_radio.h"
#include "core_v2/listeners.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/bluetooth_adapter.h"
#include "platform_v2/public/bluetooth_classic.h"
#include "platform_v2/public/multi_thread_executor.h"
#include "platform_v2/public/mutex.h"
#include "absl/container/flat_hash_map.h"

namespace location {
namespace nearby {
namespace connections {

class BluetoothClassic {
 public:
  using DiscoveredDeviceCallback = BluetoothClassicMedium::DiscoveryCallback;
  using ScanMode = BluetoothAdapter::ScanMode;

  // Callback that is invoked when a new connection is accepted.
  struct AcceptedConnectionCallback {
    std::function<void(BluetoothSocket socket)> accepted_cb =
        DefaultCallback<BluetoothSocket>();
  };

  explicit BluetoothClassic(BluetoothRadio& bluetooth_radio);
  ~BluetoothClassic();

  // Returns true, if BT communications are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Sets custom device name, and then enables BT discoverable mode.
  // Returns true, if name and scan mode are successfully set, and false
  // otherwise.
  // Called by server.
  bool TurnOnDiscoverability(const std::string& device_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables BT discoverability, and restores scan mode and device name to
  // what they were before the call to TurnOnDiscoverability().
  // Returns false if no successful call TurnOnDiscoverability() was previously
  // made, otherwise returns true.
  // Called by server.
  bool TurnOffDiscoverability() ABSL_LOCKS_EXCLUDED(mutex_);

  // Enables BT discovery mode. Will report any discoverable devices in range
  // through a callback.
  // Returns true, if discovery mode was enabled, false otherwise.
  // Called by client.
  bool StartDiscovery(DiscoveredDeviceCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Disables BT discovery mode.
  // Returns true, if discovery mode was previously enabled, false otherwise.
  // Called by client.
  bool StopDiscovery() ABSL_LOCKS_EXCLUDED(mutex_);

  // Starts a worker thread, creates a BT server socket, associates it with a
  // service name; in a worker thread repeatedly calls ServerSocket::Accept().
  // Any connected sockets returned from Accept() are passed to a callback.
  // Returns true, if server socket was successfully created, false otherwise.
  // Called by server.
  bool StartAcceptingConnections(const std::string& service_name,
                                 AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true, if object is currently running a Accept() loop.
  bool IsAcceptingConnections(const std::string& service_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Closes server socket corresponding to a service name. This automatically
  // terminates Accept() loop, if it were running.
  // Called by server.
  bool StopAcceptingConnections(const std::string& service_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if this object owns a valid platform implementation.
  bool IsMediumValid() const ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return medium_.IsValid();
  }

  // Returns true if this object has a valid BluetoothAdapter reference.
  bool IsAdapterValid() const ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return adapter_.IsValid();
  }

  // Establishes connection to BT service that was might be started on another
  // device with StartAcceptingConnections() using the same service_name.
  // Blocks until connection is established, or server-side is terminated.
  // Returns socket instance. On success, BluetoothSocket.IsValid() return true.
  // Called by client.
  BluetoothSocket Connect(BluetoothDevice& bluetooth_device,
                          const std::string& service_name)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::string GetMacAddress() const ABSL_LOCKS_EXCLUDED(mutex_);

  BluetoothDevice GetRemoteDevice(const std::string& mac_address)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  struct ScanInfo {
    bool valid = false;
  };

  static constexpr int kMaxConcurrentAcceptLoops = 5;

  // Constructs UUID object from arbitrary string, using MD5 hash, and then
  // converts UUID to a readable UUID string and returns it.
  static std::string GenerateUuidFromString(const std::string& data);

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Same as IsAcceptingConnections(), but must be called with mutex_ held.
  bool IsAcceptingConnectionsLocked(const std::string& service_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true, if discoverability is enabled with TurnOnDiscoverability().
  bool IsDiscoverable() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Assignes a different name to BT adapter.
  // Returns true if successful. Stores original device name.
  bool ModifyDeviceName(const std::string& device_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Changes current scan mode. This is an implementation of
  // Turn<On/Off>Discoveradility() method. Stores original scan mode.
  bool ModifyScanMode(ScanMode scan_mode) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Restores original device name (the one before the very first call to
  // ModifyDeviceName()). Returns true if successful.
  bool RestoreScanMode() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Restores original device scan mode (the one before the very first call to
  // ModifyScanMode()). Returns true if successful.
  bool RestoreDeviceName() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true if device is currently in discovery mode.
  bool IsDiscovering() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  BluetoothRadio& radio_ ABSL_GUARDED_BY(mutex_);
  BluetoothAdapter& adapter_ ABSL_GUARDED_BY(mutex_){
      radio_.GetBluetoothAdapter()};
  BluetoothClassicMedium medium_ ABSL_GUARDED_BY(mutex_){adapter_};

  // A bundle of state required to do a Bluetooth Classic scan. When non-null,
  // we are currently performing a Bluetooth scan.
  ScanInfo scan_info_ ABSL_GUARDED_BY(mutex_);

  // The original scan mode (that controls visibility to scanners) of the device
  // before we modified it. Restored when we stop advertising.
  ScanMode original_scan_mode_ ABSL_GUARDED_BY(mutex_) = ScanMode::kUnknown;

  // The original Bluetooth device name, before we modified it. If non-empty, we
  // are currently Bluetooth discoverable. Restored when we stop advertising.
  std::string original_device_name_ ABSL_GUARDED_BY(mutex_);

  // A thread pool dedicated to running all the accept loops from
  // StartAcceptingConnections().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};

  // A map of service Name -> ServerSocket. If map is non-empty, we
  // are currently listening for incoming connections.
  // BluetoothServerSocket instances are used from accept_loops_runner_,
  // and thus require pointer stability.
  absl::flat_hash_map<std::string, BluetoothServerSocket> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_
