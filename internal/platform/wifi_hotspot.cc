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

#include "internal/platform/wifi_hotspot.h"

#include "internal/platform/cancellation_flag.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {

WifiHotspotSocket WifiHotspotMedium::ConnectToService(
    const ServiceAddress& service_address,
    CancellationFlag* cancellation_flag) {
  LOG(INFO) << "WifiHotspotMedium::ConnectToService: port="
            << service_address.port;
  return WifiHotspotSocket(
      impl_->ConnectToService(service_address, cancellation_flag));
}

}  // namespace nearby
