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

#include "third_party/nearby_connections/cpp/presence/public/client.h"

#include "platform/base/logging.h"
#include "third_party/nearby_connections/cpp/presence/public/advertisement_generator.h"

namespace nearby {
namespace presence {

class PresenceClient {
 public:
  PresenceClient();
  ~PresenceClient();
  void PresenceClient::StartAdvertising() {
    NEARBY_LOGS(INFO) << "uudi: "
                      << advertisement_generator_.GeneratePresenceServiceUuid();
    NEARBY_LOGS(INFO)
        << "Advertisement bytes: "
        << advertisement_generator_.GeneratePresenceAdvertisementBytes();
  }
  void PresenceClient::StopAdvertising() {
    NEARBY_LOGS(INFO) << "Stop advertising.";
  }

 private:
  AdvertisementGenerator advertisement_generator_;
};

}  // namespace presence
}  // namespace nearby
