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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_ADVERTISEMENT_DATA_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_ADVERTISEMENT_DATA_H_

#include <string>

namespace nearby {
namespace presence {

// Nearby Presence advertisement data over the air.
struct AdvertisementData {
  // If true, the advertisement needs to be broadcasted over BLE 5.0.
  bool is_extended_advertisement;
  // The advertised data.
  std::string content;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MEDIUMS_ADVERTISEMENT_DATA_H_
