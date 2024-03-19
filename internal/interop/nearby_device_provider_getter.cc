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

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/logging.h"

namespace {

// static
int g_current_provider_id_ = 0;

// Map of all the valid device providers.
using DeviceProviderMap =
    absl::flat_hash_map<int, nearby::NearbyDeviceProvider*>;

// This is the pattern from
// //google3/third_party/absl/base/no_destructor.h for static variables
// with a non-trivial destructor.
DeviceProviderMap* provider_map() {
  static absl::NoDestructor<DeviceProviderMap> map;
  return map.get();
}

}  // namespace

namespace nearby {

// static
nearby::NearbyDeviceProvider* NearbyDeviceProviderGetter::GetDeviceProvider(
    int provider_id) {
  auto it = provider_map()->find(provider_id);
  if (it == provider_map()->end()) {
    NEARBY_LOGS(ERROR) << "Device provider with id: " << provider_id
                       << " does not exist.";
    return nullptr;
  }
  return it->second;
}

// static
int NearbyDeviceProviderGetter::SaveDeviceProvider(
    nearby::NearbyDeviceProvider* provider) {
  g_current_provider_id_++;
  provider_map()->insert_or_assign(g_current_provider_id_, provider);
  return g_current_provider_id_;
}

// static
void NearbyDeviceProviderGetter::RemoveDeviceProvider(int provider_id) {
  provider_map()->erase(provider_id);
}

NearbyDeviceProviderGetter::NearbyDeviceProviderGetter() = default;
NearbyDeviceProviderGetter::~NearbyDeviceProviderGetter() {
  provider_map()->clear();
  g_current_provider_id_ = 0;
}

}  // namespace nearby
