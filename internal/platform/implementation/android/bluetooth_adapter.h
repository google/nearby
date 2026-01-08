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

#ifndef PLATFORM_IMPL_ANDROID_BLUETOOTH_ADAPTER_H_
#define PLATFORM_IMPL_ANDROID_BLUETOOTH_ADAPTER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"

namespace nearby {
namespace android {

class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  ~BluetoothAdapter() override = default;

  bool SetStatus(Status status) override;

  bool IsEnabled() const override;

  ScanMode GetScanMode() const override;

  bool SetScanMode(ScanMode scan_mode) override;

  std::string GetName() const override;

  bool SetName(absl::string_view name) override;

  bool SetName(absl::string_view name, bool persist) override;

  std::string GetMacAddress() const override;
};

}  // namespace android
}  // namespace nearby

#endif  // PLATFORM_IMPL_ANDROID_BLUETOOTH_ADAPTER_H_
