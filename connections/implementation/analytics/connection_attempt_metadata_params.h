// Copyright 2022-2023 Google LLC
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

#ifndef ANALYTICS_CONNECTION_ATTEMPT_METADATA_PARAMS_H_
#define ANALYTICS_CONNECTION_ATTEMPT_METADATA_PARAMS_H_

#include <string>

#include "proto/connections_enums.pb.h"

namespace nearby {

// A struct to construct ConnectionAttemptMetadata for the analytics recorder.
struct ConnectionAttemptMetadataParams {
  location::nearby::proto::connections::ConnectionTechnology technology =
      location::nearby::proto::connections::
          CONNECTION_TECHNOLOGY_UNKNOWN_TECHNOLOGY;
  location::nearby::proto::connections::ConnectionBand band =
      location::nearby::proto::connections::CONNECTION_BAND_UNKNOWN_BAND;
  int frequency = -1;  // -1 as Unknown.
  int try_count = 0;
  std::string network_operator = {};
  std::string country_code = {};
  bool is_tdls_used = false;
  bool wifi_hotspot_enabled = false;
  int max_wifi_tx_speed = 0;
  int max_wifi_rx_speed = 0;
  int channel_width = -1;  // -1 as Unknown.
};

}  // namespace nearby

#endif  // ANALYTICS_CONNECTION_ATTEMPT_METADATA_PARAMS_H_
