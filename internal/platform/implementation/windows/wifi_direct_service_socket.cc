// Copyright 2021 Google LLC
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

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/wifi_direct_service.h"

namespace nearby {
namespace windows {

WifiDirectServiceSocket::WifiDirectServiceSocket()
    : client_socket_(std::make_unique<NearbyClientSocket>()),
      input_stream_(client_socket_.get()),
      output_stream_(client_socket_.get()) {}

WifiDirectServiceSocket::WifiDirectServiceSocket(
    absl_nonnull std::unique_ptr<NearbyClientSocket> socket)
    : client_socket_(std::move(socket)),
      input_stream_(client_socket_.get()),
      output_stream_(client_socket_.get()) {}

WifiDirectServiceSocket::~WifiDirectServiceSocket() { Close(); }

}  // namespace windows
}  // namespace nearby
