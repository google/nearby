#ifndef PLATFORM_V2_PUBLIC_BLUETOOTH_ADAPTER_H_
#define PLATFORM_V2_PUBLIC_BLUETOOTH_ADAPTER_H_

#include <string>

#include "platform_v2/api/bluetooth_adapter.h"
#include "platform_v2/api/platform.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

// https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html
class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  using Status = api::BluetoothAdapter::Status;
  using ScanMode = api::BluetoothAdapter::ScanMode;

  BluetoothAdapter()
      : impl_(api::ImplementationPlatform::CreateBluetoothAdapter()) {}
  ~BluetoothAdapter() override = default;
  BluetoothAdapter(BluetoothAdapter&&) = default;
  BluetoothAdapter& operator=(BluetoothAdapter&&) = default;

  // Synchronously sets the status of the BluetoothAdapter to 'status', and
  // returns true if the operation was a success.
  bool SetStatus(Status status) override { return impl_->SetStatus(status); }
  Status GetStatus() const {
    return IsEnabled() ? Status::kEnabled : Status::kDisabled;
  }

  // Returns true if the BluetoothAdapter's current status is
  // Status::Value::kEnabled.
  bool IsEnabled() const override { return impl_->IsEnabled(); }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getScanMode()
  //
  // Returns ScanMode::kUnknown on error.
  ScanMode GetScanMode() const override { return impl_->GetScanMode(); }

  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  bool SetScanMode(ScanMode scan_mode) override {
    return impl_->SetScanMode(scan_mode);
  }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#getName()
  // Returns an empty string on error
  std::string GetName() const override { return impl_->GetName(); }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#setName(java.lang.String)
  bool SetName(absl::string_view name) override { return impl_->SetName(name); }

  bool IsValid() const { return impl_ != nullptr; }

 private:
  std::unique_ptr<api::BluetoothAdapter> impl_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_BLUETOOTH_ADAPTER_H_
