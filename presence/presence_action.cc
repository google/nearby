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

#include "presence/presence_action.h"

#include "internal/platform/logging.h"

namespace nearby {
namespace presence {

PresenceAction::PresenceAction(int action_identifier)
    : action_identifier_(action_identifier) {
  CHECK(kMinActionIdentifierValue <= action_identifier_ &&
        action_identifier_ <= kMaxActionIdentifierValue);
}

int PresenceAction::GetActionIdentifier() const { return action_identifier_; }

}  // namespace presence
}  // namespace nearby
