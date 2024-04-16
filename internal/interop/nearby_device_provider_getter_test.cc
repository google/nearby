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

#include "internal/interop/nearby_device_provider_getter.h"

#include <cstdint>

#include "gtest/gtest.h"
#include "internal/interop/fake_device_provider.h"

namespace {

const uint8_t kExpectedDeviceProviderId = 1;
const uint8_t kExpectedDeviceProviderId2 = 2;

}  // namespace

namespace nearby {

class NearbyDeviceProviderGetterTest : public ::testing::Test {};

TEST_F(NearbyDeviceProviderGetterTest, AddDeviceProviderToMap) {
  FakeDeviceProvider fake_device_provider = FakeDeviceProvider();
  auto map_id_1 =
      NearbyDeviceProviderGetter::SaveDeviceProvider(&fake_device_provider);
  EXPECT_EQ(kExpectedDeviceProviderId,
            (NearbyDeviceProviderGetter::GetDeviceProvider(map_id_1))
                ->GetDeviceProviderId());
}

TEST_F(NearbyDeviceProviderGetterTest, AddAndRemoveDeviceProvider) {
  FakeDeviceProvider fake_device_provider = FakeDeviceProvider();
  auto map_id_1 =
      NearbyDeviceProviderGetter::SaveDeviceProvider(&fake_device_provider);
  NearbyDeviceProviderGetter::RemoveDeviceProvider(map_id_1);
  EXPECT_EQ(nullptr, (NearbyDeviceProviderGetter::GetDeviceProvider(map_id_1)));
}

TEST_F(NearbyDeviceProviderGetterTest, AddMultipleDeviceProvidersToMap) {
  FakeDeviceProvider fake_device_provider_1 = FakeDeviceProvider();
  FakeDeviceProvider fake_device_provider_2 = FakeDeviceProvider();
  auto map_id_1 =
      NearbyDeviceProviderGetter::SaveDeviceProvider(&fake_device_provider_1);
  auto map_id_2 =
      NearbyDeviceProviderGetter::SaveDeviceProvider(&fake_device_provider_2);

  EXPECT_EQ(kExpectedDeviceProviderId,
            (NearbyDeviceProviderGetter::GetDeviceProvider(map_id_1))
                ->GetDeviceProviderId());
  EXPECT_EQ(kExpectedDeviceProviderId2,
            (NearbyDeviceProviderGetter::GetDeviceProvider(map_id_2))
                ->GetDeviceProviderId());
}

}  // namespace nearby
