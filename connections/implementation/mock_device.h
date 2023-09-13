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

#ifndef CORE_INTERNAL_MOCK_DEVICE
#define CORE_INTERNAL_MOCK_DEVICE

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "internal/interop/device.h"

namespace nearby {

class MockNearbyDevice : public NearbyDevice {
 public:
  MOCK_METHOD(NearbyDevice::Type, GetType, (), (const override));
  MOCK_METHOD(std::string, GetEndpointId, (), (const override));
  MOCK_METHOD(std::vector<ConnectionInfoVariant>, GetConnectionInfos, (),
              (const override));
  MOCK_METHOD(std::string, ToProtoBytes, (), (const override));
};

}  // namespace nearby

#endif  // CORE_INTERNAL_MOCK_DEVICE
