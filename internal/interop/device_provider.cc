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

#include "internal/interop/device_provider.h"

#include "internal/interop/nearby_device_provider_getter.h"

namespace nearby {

NearbyDeviceProvider::NearbyDeviceProvider()
    : device_provider_id_(
          NearbyDeviceProviderGetter::SaveDeviceProvider(this)) {}

NearbyDeviceProvider::~NearbyDeviceProvider() {
  NearbyDeviceProviderGetter::RemoveDeviceProvider(device_provider_id_);
}

}  // namespace nearby
