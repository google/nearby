// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_BANDWIDTH_INFO_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_BANDWIDTH_INFO_H_

#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace v3 {

// Represents the connection quality.
enum class Quality {
  // Unknown connection quality. Recommended to wait for one of kMedium or
  // kHigh.
  kUnknown = 0,
  // The connection quality is poor (5KBps) and is not suitable for
  // sending files. It's recommended you wait until the connection
  // quality improves.
  kLow = 1,
  // The connection quality is ok (60~200KBps) and is suitable for
  // sending small files. For large files, it's recommended you wait
  // until the connection quality improves.
  kMedium = 2,
  // The connection quality is good or great (1MBps~60MBps) and files
  // can readily be sent. The connection quality cannot improve further
  // but may still be impacted by environment or hardware limitations.
  kHigh = 3,
};

// Used to indicate the new connection quality when upgrading to a new medium
// e.g. BT -> WiFi direct.
struct BandwidthInfo {
  Quality quality;
  ::location::nearby::proto::connections::Medium medium;
};

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_BANDWIDTH_INFO_H_
