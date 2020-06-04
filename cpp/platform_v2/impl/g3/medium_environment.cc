// Copyright 2020 Google LLC
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

#include "platform_v2/impl/g3/medium_environment.h"

namespace location {
namespace nearby {
namespace g3 {

MediumEnvironment& MediumEnvironment::Instance() {
  static std::aligned_storage_t<sizeof(MediumEnvironment),
                                alignof(MediumEnvironment)>
      storage;
  static MediumEnvironment* env = new (&storage) MediumEnvironment();
  return *env;
}

void MediumEnvironment::Reset() {
  absl::MutexLock lock(&mutex_);
  bluetooth_adapters_.clear();
}

void MediumEnvironment::OnBluetoothAdapterChangedState(
    BluetoothAdapter& adapter) {
  absl::MutexLock lock(&mutex_);
  // We don't care if there is an adapter already since all we store is a
  // pointer.
  bluetooth_adapters_.emplace(&adapter);
  // TODO(apolyudov): Add event propagation code when Medium registration is
  // implemented.
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
