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

#ifndef CORE_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_
#define CORE_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_

#include <cstdint>
#include <map>

#include "core/internal/mediums/bluetooth_radio.h"
#include "core/internal/mediums/utils.h"
#include "platform/api/bluetooth_adapter.h"
#include "platform/api/bluetooth_classic.h"
#include "platform/api/lock.h"
#include "platform/api/multi_thread_executor.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
class BluetoothClassic {
 public:
  explicit BluetoothClassic(Ptr<BluetoothRadio<Platform>> bluetooth_radio);
  ~BluetoothClassic();

  bool isAvailable();

  bool turnOnDiscoverability(const string& device_name);
  void turnOffDiscoverability();

  // Callback that is invoked when a nearby Bluetooth device is discovered.
  class DiscoveredDeviceCallback {
   public:
    virtual ~DiscoveredDeviceCallback() {}

    virtual void onDeviceDiscovered(Ptr<BluetoothDevice> device) = 0;
    virtual void onDeviceNameChanged(Ptr<BluetoothDevice> device) = 0;
    virtual void onDeviceLost(Ptr<BluetoothDevice> device) = 0;
  };

  bool startDiscovery(Ptr<DiscoveredDeviceCallback> discovered_device_callback);
  void stopDiscovery();

  // Callback that is invoked when a new connection is accepted.
  class AcceptedConnectionCallback {
   public:
    virtual ~AcceptedConnectionCallback() {}

    virtual void onConnectionAccepted(Ptr<BluetoothSocket> socket) = 0;
  };

  bool startAcceptingConnections(
      const string& service_name,
      Ptr<AcceptedConnectionCallback> accepted_connection_callback);
  bool isAcceptingConnections(const string& service_name);
  void stopAcceptingConnections(const string& service_name);

  Ptr<BluetoothSocket> connect(Ptr<BluetoothDevice> bluetooth_device,
                               const string& service_name);

 private:
  class BluetoothDiscoveryCallback
      : public BluetoothClassicMedium::DiscoveryCallback {
   public:
    explicit BluetoothDiscoveryCallback(
        Ptr<DiscoveredDeviceCallback> discovered_device_callback)
        : discovered_device_callback_(discovered_device_callback) {}
    ~BluetoothDiscoveryCallback() override {
      // Nothing to do.
    }

    void onDeviceDiscovered(Ptr<BluetoothDevice> bluetooth_device) override {
      discovered_device_callback_->onDeviceDiscovered(bluetooth_device);
    }
    void onDeviceNameChanged(Ptr<BluetoothDevice> bluetooth_device) override {
      discovered_device_callback_->onDeviceNameChanged(bluetooth_device);
    }
    void onDeviceLost(Ptr<BluetoothDevice> bluetooth_device) override {
      discovered_device_callback_->onDeviceLost(bluetooth_device);
    }

   private:
    // This could well have been a ScopedPtr, with BluetoothDiscoveryCallback in
    // turn being owned by ScanInfo (and it would have been cleaner overall,
    // since the chain of wrapped callbacks starting from
    // BluetoothDiscoveryCallback would then destruct like a stack of dominoes
    // falling, triggered by the destruction of ScanInfo), but we instead give
    // ownership of this DiscoveredDeviceCallback *and*
    // BluetoothDiscoveryCallback to ScanInfo, to maintain compatibility with
    // the Java code.
    Ptr<DiscoveredDeviceCallback> discovered_device_callback_;
  };

  struct ScanInfo {
    ScanInfo(Ptr<DiscoveredDeviceCallback> discovered_device_callback,
             Ptr<BluetoothDiscoveryCallback> bluetooth_discovery_callback)
        : discovered_device_callback(discovered_device_callback),
          bluetooth_discovery_callback(bluetooth_discovery_callback) {}
    ~ScanInfo() {
      // Nothing to do (the ScopedPtr members take care of themselves).
    }

    // Stores the DiscoveredDeviceCallback passed in to startDiscovery() by
    // clients so that we can internally stop and start Bluetooth scans
    // transparently as needed (for example, when a call to connect() is
    // invoked).
    ScopedPtr<Ptr<DiscoveredDeviceCallback>> discovered_device_callback;
    // The ordering of bluetooth_discovery_callback_ coming after
    // discovered_device_callback_ is very deliberate --
    // bluetooth_discovery_callback_ contains a reference to
    // discovered_device_callback_, so it should be destroyed first.
    ScopedPtr<Ptr<BluetoothDiscoveryCallback>> bluetooth_discovery_callback;
  };

  static string generateUUIDFromString(const string& data);

  static const std::int32_t kMaxConcurrentAcceptLoops;

  bool isDiscoverable() const;
  bool modifyDeviceName(const string& device_name);
  bool modifyScanMode(BluetoothAdapter::ScanMode::Value scan_mode);
  void restoreScanMode();
  void restoreDeviceName();
  bool isDiscovering() const;

  // ------------ GENERAL ------------

  ScopedPtr<Ptr<Lock>> lock_;

  // ------------ CORE BLUETOOTH ------------

  Ptr<BluetoothRadio<Platform>> bluetooth_radio_;
  ScopedPtr<Ptr<BluetoothAdapter>> bluetooth_adapter_;
  // The underlying, per-platform implementation.
  ScopedPtr<Ptr<BluetoothClassicMedium>> bluetooth_classic_medium_;

  // ------------ DISCOVERY ------------

  // A bundle of state required to do a Bluetooth Classic scan. When non-null,
  // we are currently performing a Bluetooth scan.
  Ptr<ScanInfo> scan_info_;

  // ------------ ADVERTISING ------------

  // The original scan mode (that controls visibility to scanners) of the device
  // before we modified it. Restored when we stop advertising.
  BluetoothAdapter::ScanMode::Value original_scan_mode_;
  // The original Bluetooth device name, before we modified it. If non-null, we
  // are currently Bluetooth discoverable. Restored when we stop advertising.
  Ptr<string> original_device_name_;
  // A thread pool dedicated to running all the accept loops from
  // startAcceptingConnections().
  ScopedPtr<Ptr<typename Platform::MultiThreadExecutorType>>
      accept_loops_thread_pool_;
  // A map of service name -> ServerSocket. While this map is non-empty, we
  // are currently listening for incoming connections.
  typedef std::map<string, Ptr<BluetoothServerSocket>> BluetoothServerSocketMap;
  BluetoothServerSocketMap bluetooth_server_sockets_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/bluetooth_classic.cc"

#endif  // CORE_INTERNAL_MEDIUMS_BLUETOOTH_CLASSIC_H_
