// Copyright 2022 Google LLC
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

#import "internal/platform/implementation/apple/bluetooth_adapter_v2.h"

#import <Foundation/Foundation.h>

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"

namespace nearby {
namespace apple {

bool BluetoothAdapter::SetStatus(api::BluetoothAdapter::Status status) {
  // We can't force a radio state in macOS/iOS.
  // NOTE: This is supposed to return the success/failure of the change (which should always be
  // "fail" for our case). However, usage in the codebase expects this to return with "success" if
  // the radio is on, so we just return whether the radio is currently enabled.
  return IsEnabled();
}

// TODO(b/290385712): Implement.
bool BluetoothAdapter::IsEnabled() const {
  return true;
}

// TODO(b/290385712): Implement.
api::BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  return api::BluetoothAdapter::ScanMode::kUnknown;
}

// TODO(b/290385712): Implement.
bool BluetoothAdapter::SetScanMode(api::BluetoothAdapter::ScanMode scan_mode) {
  return false;
}

// TODO(b/290385712): Implement.
std::string BluetoothAdapter::GetName() const {
  return "";
}

// TODO(b/290385712): Implement.
bool BluetoothAdapter::SetName(absl::string_view name) {
  return false;
}

// TODO(b/290385712): Implement.
bool BluetoothAdapter::SetName(absl::string_view name, bool persist) {
  return false;
}

// TODO(b/290385712): Implement.
std::string BluetoothAdapter::GetMacAddress() const {
  return "";
}

}  // namespace apple
}  // namespace nearby
