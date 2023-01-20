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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_OUT_OF_BAND_CONNECTION_METADATA_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_OUT_OF_BAND_CONNECTION_METADATA_H_

#include <string>

#include "connections/c/medium_selector_w.h"
#include "connections/c/strategy_w.h"
#include "internal/platform/byte_array.h"
#include "proto/connections_enums.pb.h"

namespace nearby::windows {

extern "C" {

// Metadata injected to facilitate out-of-band connections. The medium field is
// required, and the other fields are only specified for a specific medium.
// Currently, Bluetooth is the only supported medium for out-of-band
// connections.
struct DLL_API OutOfBandConnectionMetadataW {
  // Medium to use for the out-of-band connection.
  MediumW medium;

  // Endpoint ID to use for the injected connection; will be included in the
  // endpoint_found_cb callback. Must be exactly 4 bytes and should be randomly-
  // generated such that no two IDs are identical.
  const char* endpoint_id;

  // Endpoint info to use for the injected connection; will be included in the
  // endpoint_found_cb callback. Should uniquely identify the InjectEndpoint()
  // call so that the client which made the call can verify the endpoint
  // that was found is the one that was injected.
  //
  // Cannot be empty, and must be <131 bytes.
  const char* endpoint_info;
  size_t endpoint_info_size;

  // Used for Bluetooth connections.
  const char* remote_bluetooth_mac_address;
  size_t remote_bluetooth_mac_address_size;
};

}  // extern "C"
}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_OUT_OF_BAND_CONNECTION_METADATA_H_
