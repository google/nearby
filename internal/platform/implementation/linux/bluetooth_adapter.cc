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



#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"

namespace nearby {
namespace linux {
  bool BluetoothAdapter::SetStatus(Status status)
  {
    if (status == Status::kEnabled)
    {
      Powered(true);
    }
    else
    {
      Powered(false);
    }
    return true;
  }
  bool BluetoothAdapter::IsEnabled() const
  {
    return Powered();
  }
  api::BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const
  {
    if (!IsEnabled())
    {
      return ScanMode::kNone;
    }
    if (Discoverable())
    {
      return ScanMode::kConnectableDiscoverable;
    }
    return ScanMode::kConnectable;
  }
  bool BluetoothAdapter::SetScanMode(ScanMode scan_mode)
  {
    if (!IsEnabled()) return false;
    switch (scan_mode)
    {
    case ScanMode::kConnectable:
      Discoverable(false);
      return true;
    case ScanMode::kConnectableDiscoverable:
      Discoverable(true);
      return true;
    default:
      return false;
    }
  }
  std::string BluetoothAdapter::GetMacAddress() const
  {
    return Address();
  }
  std::string BluetoothAdapter::GetName() const
  {
    return Alias();
  }
  bool BluetoothAdapter::SetName(absl::string_view name)
  {
    try {
        Alias(std::string(name));
        return true;
    } catch (const sdbus::Error&) {return false;}
  }

  bool BluetoothAdapter::SetName(absl::string_view name,bool persist)
  {
    return BluetoothAdapter::SetName(name);
  }



}  // namespace linux
}  // namespace nearby

