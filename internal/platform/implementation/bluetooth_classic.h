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

#ifndef PLATFORM_API_BLUETOOTH_CLASSIC_H_
#define PLATFORM_API_BLUETOOTH_CLASSIC_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace api {

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice {
 public:
  virtual ~BluetoothDevice() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  virtual std::string GetName() const = 0;

  // Returns BT MAC address assigned to this device.
  virtual std::string GetMacAddress() const = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket {
 public:
  virtual ~BluetoothSocket() = default;

  // NOTE:
  // It is an undefined behavior if GetInputStream() or GetOutputStream() is
  // called for a not-connected BluetoothSocket, i.e. any object that is not
  // returned by BluetoothClassicMedium::ConnectToService() for client side or
  // BluetoothServerSocket::Accept() for server side of connection.

  // Returns the InputStream of this connected BluetoothSocket.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of this connected BluetoothSocket.
  virtual OutputStream& GetOutputStream() = 0;

  // Closes both input and output streams, marks Socket as closed.
  // After this call object should be treated as not connected.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  // Returns valid BluetoothDevice pointer if there is a connection, and
  // nullptr otherwise.
  virtual BluetoothDevice* GetRemoteDevice() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket {
 public:
  virtual ~BluetoothServerSocket() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  virtual std::unique_ptr<BluetoothSocket> Accept() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// https://developer.android.com/reference/com/google/android/things/bluetooth/PairingParams
//
// Encapsulates the data for a particular pairing attempt.
// The caller can use it to determine the pairing approach and choose a suitable
// way to obtain user consent to conclude the pairing process.
struct PairingParams {
  // Pairing type for this pairing attempt.
  // The Pairing type is based on the User Interface capabilities of both
  // the pairing devices, and determines the process of pairing.
  // `kConstent`: the user is expected to consent to the pairing process.
  // `kDisplayPasskey`: the user is notified of a pairing passkey.
  // `kDisplayPin`: same as kDisplayPasskey, but different pairing key format.
  // `kConfirmPasskey`: the user must confirm pairing after verifying a passkey.
  // `kRequestPin`: the user is supposed to enter a pin to confirm pairing.
  enum class PairingType {
    kUnknown = 0,
    kConsent = 1,
    kDisplayPasskey = 2,
    kDisplayPin = 3,
    kConfirmPasskey = 4,
    kRequestPin = 5,
    kLast,
  };
  PairingType pairing_type;

  // Pairing pin to notify the user for the pairing process.
  // If not relevant to the current pairing process, it's empty.
  std::string passkey;
};

// https://developer.android.com/reference/com/google/android/things/bluetooth/BluetoothPairingCallback
//
// This callback is invoked during the Bluetooth pairing process and
// contains all the relevant pairing information required for pairing.
struct BluetoothPairingCallback {
  // https://developer.android.com/reference/com/google/android/things/bluetooth/BluetoothPairingCallback.PairingError
  enum class PairingError {
    kUnknown = 0,
    kAuthCanceled = 1,     /* failed because we canceled the pairing process. */
    kAuthFailed = 2,       /* failed with pins did not match, or no response. */
    kAuthRejected = 3,     /* failed with the remote device rejected pairing. */
    kAuthTimeout = 4,      /* failed with authentication timeout. */
    kFailed = 5,           /* failed with no explicit reason. */
    kRepeatedAttempts = 6, /* failed with many repeated attempts. */
    kLast,
  };

  // Invoked when successfully paired with a device.
  absl::AnyInvocable<void()> on_paired_cb = DefaultCallback<>();

  // Invoked when pairing with a device is canceled or fails.
  absl::AnyInvocable<void(BluetoothPairingCallback::PairingError error)>
      on_pairing_error_cb =
          DefaultCallback<BluetoothPairingCallback::PairingError>();

  // Invoked when the pairing process has been initiated with a remote
  // Bluetooth device.
  absl::AnyInvocable<void(PairingParams pairingParams)>
      on_pairing_initiated_cb = DefaultCallback<PairingParams>();
};

// This class is responsible for handling Bluetooth pairing with a remote
// BluetoothDevice.
// DCHECK_CALLED_ON_VALID_SEQUENCE
class BluetoothPairing {
 public:
  virtual ~BluetoothPairing() = default;

  // https://developer.android.com/reference/com/google/android/things/bluetooth/BluetoothConnectionManager#initiatepairing
  // https://developer.android.com/reference/com/google/android/things/bluetooth/BluetoothConnectionManager#registerpairingcallback
  //
  // Initiate Bluetooth pairing process with a remote device.
  // Register a BluetoothPairingCallback to listen for Bluetooth pairing events
  // Such as incoming pairing request, devices paired etc.
  virtual bool InitiatePairing(BluetoothPairingCallback pairing_cb) = 0;

  // https://developer.android.com/reference/com/google/android/things/bluetooth/BluetoothConnectionManager#finishpairing
  //
  // Invoke this function to finish the pairing process with the remote device.
  // Should be called only after receiving a callback from onPairingInitiated.
  // Pin is needed for PairingType::kRequestPin
  virtual bool FinishPairing(std::optional<absl::string_view> pin_code) = 0;

  // https://developer.android.com/reference/com/google/android/things/bluetooth/BluetoothConnectionManager#cancelpairing
  //
  // Cancel an ongoing pairing process with a remote device.
  virtual bool CancelPairing() = 0;

  // https://developer.android.com/reference/com/google/android/things/bluetooth/BluetoothConnectionManager#unpair
  //
  // Destroys the existing pairing/bond with the remote device.
  virtual bool Unpair() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getBondState()
  //
  // Get the pairing state of the remote device.
  virtual bool IsPaired() = 0;
};

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium {
 public:
  virtual ~BluetoothClassicMedium() = default;

  struct DiscoveryCallback {
    // BluetoothDevice is a proxy object created as a result of BT discovery.
    // Its lifetime spans between calls to device_discovered_cb and
    // device_lost_cb.
    // It is safe to use BluetoothDevice in device_discovered_cb() callback
    // and at any time afterwards, until device_lost_cb() is called.
    // It is not safe to use BluetoothDevice after returning from
    // device_lost_cb() callback.
    absl::AnyInvocable<void(BluetoothDevice& device)> device_discovered_cb =
        DefaultCallback<BluetoothDevice&>();
    absl::AnyInvocable<void(BluetoothDevice& device)> device_name_changed_cb =
        DefaultCallback<BluetoothDevice&>();
    absl::AnyInvocable<void(BluetoothDevice& device)> device_lost_cb =
        DefaultCallback<BluetoothDevice&>();
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a new `device` is added to the adapter.
    virtual void DeviceAdded(BluetoothDevice& device) {}

    // Called when `device` is removed from the adapter.
    virtual void DeviceRemoved(BluetoothDevice& device) {}

    // Called when the address of `device` changed due to pairing.
    virtual void DeviceAddressChanged(BluetoothDevice& device,
                                      absl::string_view old_address) {}

    // Called when the paired property of `device` changed.
    virtual void DevicePairedChanged(BluetoothDevice& device,
                                     bool new_paired_status) {}

    // Called when `device` has connected or disconnected.
    virtual void DeviceConnectedStateChanged(BluetoothDevice& device,
                                             bool connected) {}
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  //
  // Returns true once the process of discovery has been initiated.
  virtual bool StartDiscovery(DiscoveryCallback discovery_callback) = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // StartDiscovery().
  virtual bool StopDiscovery() = 0;

  // A combination of
  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#createInsecureRfcommSocketToServiceRecord
  // followed by
  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#connect().
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  // On success, returns a new BluetoothSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<BluetoothSocket> ConnectToService(
      BluetoothDevice& remote_device, const std::string& service_uuid,
      CancellationFlag* cancellation_flag) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns nullptr error.
  virtual std::unique_ptr<BluetoothServerSocket> ListenForService(
      const std::string& service_name, const std::string& service_uuid) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#createBond()
  //
  // Start the bonding (pairing) process with the remote device.
  // Return a Bluetooth pairing instance to handle the pairing process with the
  // remote device.
  virtual std::unique_ptr<BluetoothPairing> CreatePairing(
      BluetoothDevice& remote_device) = 0;

  virtual BluetoothDevice* GetRemoteDevice(const std::string& mac_address) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_BLUETOOTH_CLASSIC_H_
