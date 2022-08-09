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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_

#include <memory>

#include "presence/implementation/service_controller.h"

namespace nearby {
namespace presence {

/*
 * PresenceService hosts presence functions by routing invokes to the unique
 * {@code ServiceController}. PresenceService should be initialized once and
 * only once in the process that hosting presence functions.
 */
class PresenceService {
 public:
  PresenceService() = default;
  ~PresenceService() = default;

 private:
  std::unique_ptr<ServiceController> service_controller_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_SERVICE_H_
