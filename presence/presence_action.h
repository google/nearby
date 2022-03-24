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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_ACTION_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_ACTION_H_

namespace nearby {
namespace presence {
class PresenceAction {
 public:
  PresenceAction(int action_identifier = 1);
  int GetActionIdentifier() const;

 private:
  static constexpr int kMinActionIdentifierValue = 1;
  static constexpr int kMaxActionIdentifierValue = 255;
  const int action_identifier_;
};

inline bool operator==(const PresenceAction& a1, const PresenceAction& a2) {
  return a1.GetActionIdentifier() == a2.GetActionIdentifier();
}

inline bool operator!=(const PresenceAction& a1, const PresenceAction& a2) {
  return !(a1 == a2);
}
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_ACTION_H_
