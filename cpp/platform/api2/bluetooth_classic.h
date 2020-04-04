#ifndef PLATFORM_API2_BLUETOOTH_CLASSIC_H_
#define PLATFORM_API2_BLUETOOTH_CLASSIC_H_

#include <memory>
#include <string>

#include "platform/api2/input_stream.h"
#include "platform/api2/output_stream.h"
#include "platform/byte_array.h"
#include "platform/exception.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice {
 public:
  virtual ~BluetoothDevice() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  virtual std::string GetName() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket {
 public:
  virtual ~BluetoothSocket() {}

  // Returns the InputStream of the BluetoothSocket.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the BluetoothSocket.
  virtual OutputStream& GetOutputStream() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  virtual BluetoothDevice& GetRemoteDevice() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket {
 public:
  virtual ~BluetoothServerSocket() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // returns Exception::kIo on error.
  virtual ExceptionOr<std::unique_ptr<BluetoothSocket>> Accept() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium {
 public:
  virtual ~BluetoothClassicMedium() {}

  class DiscoveryCallback {
   public:
    virtual ~DiscoveryCallback() {}

    // BluetoothDevice* is not owned by callbacks.
    // Pointer is guaranteed to remain valid for the duration of a call.
    virtual void OnDeviceDiscovered(BluetoothDevice* device) = 0;
    virtual void OnDeviceNameChanged(BluetoothDevice* device) = 0;
    virtual void OnDeviceLost(BluetoothDevice* device) = 0;
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  //
  // Returns true once the process of discovery has been initiated.
  //
  // Does not take ownership of the passed-in discovery_callback -- destroying
  // that is up to the caller.
  virtual bool StartDiscovery(const DiscoveryCallback& discovery_callback) = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // startDiscovery().
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
  // On success, returns a new BluetoothSocket, wrapped in a ExceptionOr object.
  // On error, returns Exception object.
  virtual ExceptionOr<std::unique_ptr<BluetoothSocket>> ConnectToService(
      BluetoothDevice* remote_device, absl::string_view service_uuid) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns Exception::kIo on error.
  virtual ExceptionOr<std::unique_ptr<BluetoothServerSocket>> ListenForService(
      absl::string_view service_name, absl::string_view service_uuid) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_BLUETOOTH_CLASSIC_H_
