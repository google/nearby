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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SCAN_MANAGER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SCAN_MANAGER_H_

#include "presence/implementation/credential_manager.h"
#include "presence/implementation/mediums/mediums.h"

namespace nearby {
namespace presence {

/*
 * The instance of ScanManager is owned by {@code ServiceControllerImpl}.
 * Helping service controller to manage scan requests and callbacks.
 */
class ScanManager {
 public:
  ScanManager(Mediums& mediums, CredentialManager& credential_manager) {
    mediums_ = &mediums, credential_manager_ = &credential_manager;
  }
  ~ScanManager() = default;

 private:
  Mediums* mediums_;
  CredentialManager* credential_manager_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SCAN_MANAGER_H_
