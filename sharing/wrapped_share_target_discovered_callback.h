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

#ifndef THIRD_PARTY_NEARBY_SHARING_WRAPPED_SHARE_TARGET_DISCOVERED_CALLBACK_H_
#define THIRD_PARTY_NEARBY_SHARING_WRAPPED_SHARE_TARGET_DISCOVERED_CALLBACK_H_

#include <cstdint>

#include "sharing/advertisement.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"

namespace nearby {
namespace sharing {
// Wraps a |ShareTargetDiscoveredCallback| for vendor ID blocking.
// If the vendor ID is not default (0), it will automatically be blocked.
class WrappedShareTargetDiscoveredCallback
    : public ShareTargetDiscoveredCallback {
 public:
  explicit WrappedShareTargetDiscoveredCallback(
      ShareTargetDiscoveredCallback* callback,
      Advertisement::BlockedVendorId blocked_vendor_id,
      bool disable_wifi_hotspot)
      : callback_(callback),
        blocked_vendor_id_(blocked_vendor_id),
        disable_wifi_hotspot_(disable_wifi_hotspot) {}
  void OnShareTargetDiscovered(const ShareTarget& target) override;
  void OnShareTargetUpdated(const ShareTarget& target) override;
  void OnShareTargetLost(const ShareTarget& target) override;
  const Advertisement::BlockedVendorId& BlockedVendorId() const {
    return blocked_vendor_id_;
  }
  bool disable_wifi_hotspot() const { return disable_wifi_hotspot_; }

 private:
  bool ShouldBlockShareTarget(const ShareTarget& target) const;

  ShareTargetDiscoveredCallback* callback_;
  const Advertisement::BlockedVendorId blocked_vendor_id_;
  const bool disable_wifi_hotspot_;
};
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_WRAPPED_SHARE_TARGET_DISCOVERED_CALLBACK_H_
