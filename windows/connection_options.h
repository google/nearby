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

#ifndef WINDOWS_REQUEST_CONNECTION_OPTIONS_H_
#define WINDOWS_REQUEST_CONNECTION_OPTIONS_H_

#include <stdint.h>

#include <memory>

#include "platform/base/core_config.h"

namespace location {
namespace nearby {
class ByteArray;
namespace proto {
namespace connections {
enum Medium : int;
}
}  // namespace proto
namespace connections {
struct ConnectionOptions;
class Strategy;
template <typename>
struct MediumSelector;

using BooleanMediumSelector = MediumSelector<bool>;

namespace windows {
struct DLL_API ConnectionOptions {
  ConnectionOptions();
  operator connections::ConnectionOptions() const;

 private:
  std::unique_ptr<connections::ConnectionOptions,
                  void (*)(connections::ConnectionOptions *)>
      impl_;

 public:
  connections::Strategy &strategy;
  connections::BooleanMediumSelector &allowed;

  bool &auto_upgrade_bandwidth;
  bool &enable_bluetooth_listening;
  bool &enable_webrtc_listening;
  bool &low_power;
  bool &enforce_topology_constraints;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  bool &is_out_of_band_connection;
  ByteArray &remote_bluetooth_mac_address;
  const char *fast_advertisement_service_uuid;
  int &keep_alive_interval_millis;
  int &keep_alive_timeout_millis;

  // Verify if  ConnectionOptions is in a not-initialized (Empty) state.
  bool Empty() const;

  // Bring  ConnectionOptions to a not-initialized (Empty) state.
  void Clear();

  // Returns a copy and normalizes allowed mediums:
  // (1) If is_out_of_band_connection is true, verifies that there is only one
  //     medium allowed, defaulting to only Bluetooth if unspecified.
  // (2) If no mediums are allowed, allow all mediums.
  connections::ConnectionOptions CompatibleOptions() const;

  // This call follows the standard Microsoft calling pattern of calling first
  // to get the size of the array. Caller then allocates memory for the array,
  // and makes this call again to copy the array into the provided location.
  void GetMediums(location::nearby::proto::connections::Medium *mediums,
                  uint32_t *mediumsSize);
};

}  // namespace windows
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  //  WINDOWS_REQUEST_CONNECTION_OPTIONS_H_
