// Copyright 2026 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_SHARE_SESSION_USAGE_H_
#define THIRD_PARTY_NEARBY_SHARING_SHARE_SESSION_USAGE_H_

#include <string>

namespace nearby::sharing {

enum class ShareSessionUsage {
  kUnknown,
  kSharing,  // Connection is used for quick share.
  kPairing,  // Connection is used for setting up a binding.
  kFileSync,  // Connection is used for file sync.
};

inline std::string ShareSessionUsageToString(
    ShareSessionUsage transfer_usage) {
  switch (transfer_usage) {
    case ShareSessionUsage::kSharing:
      return "Sharing";
    case ShareSessionUsage::kPairing:
      return "Pairing";
    case ShareSessionUsage::kFileSync:
      return "FileSync";
    case ShareSessionUsage::kUnknown:
      return "Unknown";
  }
}

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_SHARE_SESSION_USAGE_H_
