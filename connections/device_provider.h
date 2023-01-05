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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_DEVICE_PROVIDER_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_DEVICE_PROVIDER_H_

#include <string>

#include "internal/device.h"

namespace nearby {
namespace connections {

using ::nearby::NearbyDevice;

class NearbyDeviceProvider {
  virtual ~NearbyDeviceProvider() = default;
  virtual NearbyDevice* GetLocalDevice() = 0;
  virtual std::string GetServiceId() = 0;
};

}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_DEVICE_PROVIDER_H_
