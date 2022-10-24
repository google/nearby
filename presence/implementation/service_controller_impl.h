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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_IMPL_H_

#include <memory>

#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/implementation/credential_manager_impl.h"
#include "presence/implementation/mediums/mediums.h"
#include "presence/implementation/service_controller.h"
#include "presence/scan_request.h"
/*
 * This class implements {@code ServiceController} functions. Owns mediums and
 * other managers instances.
 */
namespace nearby {
namespace presence {

class ServiceControllerImpl : public ServiceController {
 public:
  ServiceControllerImpl() = default;
  std::unique_ptr<ScanSession> StartScan(ScanRequest scan_request,
                                         ScanCallback callback) override;
  std::unique_ptr<BroadcastSession> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) override;

 private:
  Mediums mediums_;  // NOLINT: further impl will use it.
  CredentialManagerImpl
      credential_manager_;  // NOLINT: further impl will use it.
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_IMPL_H_
