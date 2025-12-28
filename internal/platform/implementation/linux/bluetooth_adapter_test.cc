// filepath: /workspace/internal/platform/implementation/linux/bluetooth_adapter_test.cc
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

#include "internal/platform/implementation/linux/bluetooth_adapter.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include <sdbus-c++/sdbus-c++.h>

namespace nearby {
namespace linux {
namespace {

constexpr absl::string_view kName = "Test Radio Name";

// Tests are disabled because they may interact with the system DBus and
// modify adapter state; they are intended as compile-time and manual-run
// validations to mirror the Windows test suite.

TEST(BluetoothAdapter, DISABLED_SetStatusReturnsTrue) {
  auto connection = sdbus::createSystemBusConnection();
  connection -> enterEventLoopAsync();
  sdbus::ObjectPath object_path{"/org/bluez/hci0"};
  BluetoothAdapter adapter(*connection, object_path);

  // Our implementation returns bool; assert it returns true on attempted
  // enable. This test is disabled by default to avoid changing system state.
  EXPECT_TRUE(adapter.SetStatus(api::BluetoothAdapter::Status::kEnabled));
}

TEST(BluetoothAdapter, DISABLED_SetAndGetName) {
  auto connection = sdbus::createSystemBusConnection();
  connection -> enterEventLoopAsync();
  sdbus::ObjectPath object_path{"/org/bluez/hci0"};
  BluetoothAdapter adapter(*connection, object_path);

  const std::string new_name = "nearby-linux-test-name";
  bool ok = adapter.SetName(new_name);
  EXPECT_TRUE(ok);
  if (ok) {
    // If SetName succeeded, GetName should reflect the set value.
    EXPECT_EQ(adapter.GetName(), new_name);
  }
}

TEST(BluetoothAdapter, DISABLED_GetMacAddressNotEmpty) {
  auto connection = sdbus::createSystemBusConnection();
  connection -> enterEventLoopAsync();
  sdbus::ObjectPath object_path{"/org/bluez/hci0"};
  BluetoothAdapter adapter(*connection, object_path);

  EXPECT_FALSE(adapter.GetMacAddress().empty());
}

TEST(BluetoothAdapter, DISABLED_SetScanModeWhenEnabled) {
  auto connection = sdbus::createSystemBusConnection();
  connection -> enterEventLoopAsync();
  sdbus::ObjectPath object_path{"/org/bluez/hci0"};
  BluetoothAdapter adapter(*connection, object_path);

  if (!adapter.IsEnabled()) {
    GTEST_SKIP() << "Adapter not enabled on this machine; skipping scan-mode test.";
  }

  // Try to set discoverable connectable; may be disallowed by system policy.
  bool set_ok = adapter.SetScanMode(api::BluetoothAdapter::ScanMode::kConnectableDiscoverable);
  if (!set_ok) {
    GTEST_SKIP() << "SetScanMode returned false; skipping further scan-mode checks.";
  }

  auto scan_mode = adapter.GetScanMode();
  EXPECT_EQ(scan_mode, api::BluetoothAdapter::ScanMode::kConnectableDiscoverable);
}

}  // namespace
}  // namespace linux
}  // namespace nearby
