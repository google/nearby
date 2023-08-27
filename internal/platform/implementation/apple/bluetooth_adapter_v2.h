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

// Note: File language is detected using heuristics. Many Objective-C++ headers are incorrectly
// classified as C++ resulting in invalid linter errors. The use of "NSArray" and other Foundation
// classes like "NSData", "NSDictionary" and "NSUUID" are highly weighted for Objective-C and
// Objective-C++ scores. Oddly, "#import <Foundation/Foundation.h>" does not contribute any points.
// This comment alone should be enough to trick the IDE in to believing this is actually some sort
// of Objective-C file. See: cs/google3/devtools/search/lang/recognize_language_classifiers_data

#import <Foundation/Foundation.h>

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/bluetooth_adapter.h"

namespace nearby {
namespace apple {

// Concrete implementation representing the local device Bluetooth adapter.
class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  BluetoothAdapter() = default;
  ~BluetoothAdapter() override = default;

  // Synchronously sets the status of the BluetoothAdapter to @c status, and
  // returns true if the operation was a success.
  bool SetStatus(api::BluetoothAdapter::Status status) override;

  // Returns true if the BluetoothAdapter's current status is @c kEnabled.
  bool IsEnabled() const override;

  // Get the current Bluetooth scan mode of the local Bluetooth adapter.
  //
  // The Bluetooth scan mode determines if the local adapter is connectable
  // and/or discoverable from remote Bluetooth devices.
  //
  // Possible values are: @c kNone, @c kConnectable,
  // @c kConnectableDiscoverable.
  //
  // Returns @c kUnknown on error.
  api::BluetoothAdapter::ScanMode GetScanMode() const override;

  // Synchronously sets the scan mode of the adapter, and returns true if the
  // operation was a success.
  bool SetScanMode(api::BluetoothAdapter::ScanMode scan_mode) override;

  // Get the friendly Bluetooth name of the local Bluetooth adapter.
  //
  // Returns an empty string on error
  std::string GetName() const override;

  // Set and persist the friendly Bluetooth name of the local Bluetooth adapter.
  //
  // This name is visible to remote Bluetooth devices.
  //
  // Valid Bluetooth names are a maximum of 248 bytes using UTF-8 encoding,
  // although many remote devices can only display the first 40 characters, and
  // some may be limited to just 20.
  bool SetName(absl::string_view name) override;

  // Set the friendly Bluetooth name of the local Bluetooth adapter.
  //
  // This name is visible to remote Bluetooth devices.
  //
  // Valid Bluetooth names are a maximum of 248 bytes using UTF-8 encoding,
  // although many remote devices can only display the first 40 characters, and
  // some may be limited to just 20.
  //
  // If persist is false, we will restore the original radio names when complete.
  bool SetName(absl::string_view name, bool persist) override;

  // Returns BT MAC address assigned to this adapter.
  std::string GetMacAddress() const override;
};

}  // namespace apple
}  // namespace nearby
