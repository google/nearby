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

#include "core/medium_selector.h"
#include "core/strategy.h"
#include "platform/base/core_config.h"

namespace location {
namespace nearby {
class ByteArray;
namespace connections {
using BooleanMediumSelector = MediumSelector<bool>;

struct DLL_API ConnectionOptions {
 public:
  connections::Strategy strategy;
  connections::BooleanMediumSelector allowed;

  ByteArray *remote_bluetooth_mac_address;
  int keep_alive_interval_millis;
  int keep_alive_timeout_millis;

  // This call follows the standard Microsoft calling pattern of calling first
  // to get the size of the array. Caller then allocates memory for the array,
  // and makes this call again to copy the array into the provided location.
  void GetMediums(location::nearby::proto::connections::Medium *mediums,
                  uint32_t *mediumsSize);

  std::vector<Medium> GetMediums() const;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  //  WINDOWS_REQUEST_CONNECTION_OPTIONS_H_
