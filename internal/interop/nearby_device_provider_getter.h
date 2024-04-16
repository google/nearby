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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_NEARBY_DEVICE_PROVIDER_GETTER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_NEARBY_DEVICE_PROVIDER_GETTER_H_

namespace nearby {

class NearbyDeviceProvider;

// `NearbyDeviceProvider` exposes the local `NearbyDevice` to be used for
// authentication in `NearbyConnections::Core::ConnectV3()`.
// `NearbyDeviceProviderGetter` is a singleton for holding a map of
// `NearbyDeviceProvider` objects. `NearbyConnections` cannot depend directly on
// `NearbyPresence` so this class acts as a base dependency for both
// `NearbyPresence` and `NearbyConnections`. `NearbyPresence` can store a
// `NearbyDeviceProvider` which `NearbyConnections` can then access.
class NearbyDeviceProviderGetter {
 public:
  // It is the responsibility of the invoker of these methods to ensure sure the
  // `DeviceProvider` is valid and to call `RemoveDeviceProvider()`. A
  // `DeviceProvider` being in the map does not guarantee that it is valid or
  // that it will stay valid.
  static NearbyDeviceProvider* GetDeviceProvider(int provider_id);
  static int SaveDeviceProvider(NearbyDeviceProvider* provider);
  static void RemoveDeviceProvider(int provider_id);

 private:
  NearbyDeviceProviderGetter();
  ~NearbyDeviceProviderGetter();
  NearbyDeviceProviderGetter(const NearbyDeviceProviderGetter&) = delete;
  NearbyDeviceProviderGetter& operator=(const NearbyDeviceProviderGetter&) =
      delete;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_NEARBY_DEVICE_PROVIDER_GETTER_H_
