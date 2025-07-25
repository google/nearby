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

#include "internal/platform/wifi_direct.h"

#include "absl/strings/string_view.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/logging.h"

namespace nearby {

WifiDirectSocket WifiDirectMedium::ConnectToService(
    absl::string_view ip_address, int port,
    CancellationFlag* cancellation_flag) {
  LOG(INFO) << "WifiDirectMedium::ConnectToService: ip address=" << ip_address
            << ", port=" << port;
  return WifiDirectSocket(
      impl_->ConnectToService(ip_address, port, cancellation_flag));
}

}  // namespace nearby
