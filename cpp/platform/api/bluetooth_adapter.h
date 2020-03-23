#ifndef PLATFORM_API_BLUETOOTH_ADAPTER_H_
#define PLATFORM_API_BLUETOOTH_ADAPTER_H_

#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter {
 public:
  virtual ~BluetoothAdapter() {}

  // Eligible statuses of the BluetoothAdapter.
  struct Status {
    enum Value {
      DISABLED,
      ENABLED,
    };
  };

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  virtual bool setStatus(Status::Value status) = 0;
  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::ENABLED.
  virtual bool isEnabled() = 0;

  // Scan modes of a BluetoothAdapter, as described at
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode().
  struct ScanMode {
    enum Value {
      UNKNOWN,
      CONNECTABLE_DISCOVERABLE,
    };
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::UNKNOWN on error.
  virtual ScanMode::Value getScanMode() = 0;
  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  virtual bool setScanMode(ScanMode::Value scan_mode) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  //
  // Returns a null Ptr<string> on error.
  virtual Ptr<std::string> getName() = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  virtual bool setName(const std::string& name) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_BLUETOOTH_ADAPTER_H_
