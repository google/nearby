// Copyright 2022 Google LLC
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

#include "sharing/share_target.h"

#include <cinttypes>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "internal/network/url.h"
#include "sharing/common/nearby_share_enums.h"

namespace nearby {
namespace sharing {
namespace {
using ::nearby::network::Url;

// Used to generate device ID.
static int64_t kLastGeneratedId = 0;
}  // namespace

ShareTarget::ShareTarget() { id = ++kLastGeneratedId; }

ShareTarget::ShareTarget(const ShareTarget&) = default;

ShareTarget::ShareTarget(ShareTarget&&) = default;

ShareTarget& ShareTarget::operator=(const ShareTarget&) = default;

ShareTarget& ShareTarget::operator=(ShareTarget&&) = default;

ShareTarget::~ShareTarget() = default;

/* static */
ShareTarget ShareTarget::CreateShareTargetForTest(
    std::string device_name, Url image_url, ShareTargetType type,
    bool is_incoming, absl::string_view endpoint_id,
    std::optional<std::string> full_name, bool is_known,
    std::optional<std::string> device_id, bool for_self_share) {
  ShareTarget share_target;
  share_target.device_name = std::move(device_name);
  share_target.image_url = std::move(image_url);
  share_target.type = type;
  share_target.is_incoming = is_incoming;
  share_target.full_name = std::move(full_name);
  share_target.is_known = is_known;
  share_target.device_id = std::move(device_id);
  share_target.for_self_share = for_self_share;
  share_target.endpoint_id = std::string(endpoint_id);
  return share_target;
}

std::string ShareTarget::ToString() const {
  std::vector<std::string> fmt;

  fmt.push_back(absl::StrFormat("id: %" PRId64, id));
  fmt.push_back(absl::StrFormat("type: %d", type));
  fmt.push_back(absl::StrFormat("device_name: %s", device_name));
  if (full_name) {
    fmt.push_back(absl::StrFormat("full_name: %s", *full_name));
  }
  if (image_url) {
    fmt.push_back(absl::StrFormat("image_url: %s", image_url->GetUrlPath()));
  }
  if (device_id) {
    fmt.push_back(absl::StrFormat("device_id: %s", *device_id));
  }
  fmt.push_back(absl::StrFormat("endpoint_id: %s", endpoint_id));
  fmt.push_back(absl::StrFormat("is_known: %d", is_known));
  fmt.push_back(absl::StrFormat("is_incoming: %d", is_incoming));
  fmt.push_back(absl::StrFormat("for_self_share: %d", for_self_share));
  fmt.push_back(absl::StrFormat("vendor_id: %d", vendor_id));
  fmt.push_back(absl::StrFormat("receive_disabled: %d", receive_disabled));

  return absl::StrCat("ShareTarget<", absl::StrJoin(fmt, ", "), ">");
}

bool ShareTarget::operator==(const ShareTarget& other) const {
  return id == other.id && device_name == other.device_name &&
         image_url == other.image_url && type == other.type &&
         is_incoming == other.is_incoming && full_name == other.full_name &&
         is_known == other.is_known && device_id == other.device_id &&
         for_self_share == other.for_self_share &&
         vendor_id == other.vendor_id &&
         receive_disabled == other.receive_disabled &&
         endpoint_id == other.endpoint_id;
}

}  // namespace sharing
}  // namespace nearby
