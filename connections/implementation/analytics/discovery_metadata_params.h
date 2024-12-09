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

#ifndef ANALYTICS_DISCOVERY_METADATA_PARAMS_H_
#define ANALYTICS_DISCOVERY_METADATA_PARAMS_H_

#include <vector>

#include "internal/proto/analytics/connections_log.pb.h"

namespace nearby {

// A struct to construct DiscoveryMetadata for the analytics recorder.
struct DiscoveryMetadataParams {
  bool is_extended_advertisement_supported = false;
  int connected_ap_frequency = 0;
  bool is_nfc_available = false;
  std::vector<location::nearby::analytics::proto::ConnectionsLog::
                  OperationResultWithMedium>
      operation_result_with_mediums = {};
};

}  // namespace nearby

#endif  // ANALYTICS_DISCOVERY_METADATA_PARAMS_H_
