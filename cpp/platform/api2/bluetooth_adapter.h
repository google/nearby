#ifndef PLATFORM_API2_BLUETOOTH_ADAPTER_H_
#define PLATFORM_API2_BLUETOOTH_ADAPTER_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter {
 public:
  virtual ~BluetoothAdapter() {}

  // Eligible statuses of the BluetoothAdapter.
  enum class Status {
    kDisabled,
    kEnabled,
  };

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  virtual bool SetStatus(Status status) = 0;
  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::kEnabled.
  virtual bool IsEnabled() = 0;

  // Scan modes of a BluetoothAdapter, as described at
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode().
  enum class ScanMode {
    kUnknown,
    kConnectableDiscoverable,
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::kUnknown on error.
  virtual ScanMode GetScanMode() = 0;
  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  virtual bool SetScanMode(ScanMode scan_mode) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  // Returns an empty string on error
  virtual std::string GetName() const = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  virtual bool SetName(absl::string_view name) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_BLUETOOTH_ADAPTER_H_
