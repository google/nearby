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

#include "internal/platform/implementation/g3/wifi_direct.h"

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/implementation/g3/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace g3 {
bool WifiDirectMedium::StartWifiDirect(
    HotspotCredentials* wifi_direct_credentials) {
  return true;
}

bool WifiDirectMedium::StopWifiDirect() {
  absl::MutexLock lock(&mutex_);
  return true;
}

bool WifiDirectMedium::ConnectWifiDirect(
    HotspotCredentials* wifi_direct_credentials) {
  return true;
}

bool WifiDirectMedium::DisconnectWifiDirect() { return true; }

std::unique_ptr<api::WifiHotspotSocket> WifiDirectMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag* cancellation_flag) {
  return nullptr;
}

std::unique_ptr<api::WifiHotspotServerSocket>
WifiDirectMedium::ListenForService(int port) {
  return nullptr;
}
}  // namespace g3
}  // namespace nearby
}  // namespace location
