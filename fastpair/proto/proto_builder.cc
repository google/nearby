// Copyright 2023 Google LLC
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

#include "fastpair/proto/proto_builder.h"

#include <optional>
#include <string>

#include "absl/time/time.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/proto/data.proto.h"
#include "fastpair/proto/enum.proto.h"
#include "fastpair/proto/fast_pair_string.proto.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

void BuildFastPairInfo(::nearby::fastpair::proto::FastPairInfo* fast_pair_info,
                       const FastPairDevice& fast_pair_device) {
  NEARBY_LOGS(VERBOSE) << __func__;
  const AccountKey& account_key = fast_pair_device.GetAccountKey();
  auto& metadata = fast_pair_device.GetMetadata();
  DCHECK(metadata);
  DCHECK(account_key.Ok());

  // Sets FastPairDevice of FastPairInfo
  auto* device = fast_pair_info->mutable_device();
  device->set_account_key(account_key.GetAsBytes());
  // Create a SHA256 hash of the |mac_address| with the |account_key| as salt.
  // The hash is used to identify devices via non discoverable advertisements.
  if (fast_pair_device.GetPublicAddress().has_value()) {
    device->set_sha256_account_key_public_address(
        FastPairRepository::GenerateSha256OfAccountKeyAndMacAddress(
            account_key, fast_pair_device.GetPublicAddress().value()));
  }

  auto& details = metadata->GetDetails();
  auto& strings = metadata->GetResponse().strings();

  // Sets proto::StoredDiscoveryItem to FastPairDevice
  proto::StoredDiscoveryItem discovery_item;
  discovery_item.set_id(fast_pair_device.GetModelId());
  discovery_item.set_trigger_id(fast_pair_device.GetModelId());

  std::optional<std::string> display_name = fast_pair_device.GetDisplayName();
  if (display_name.has_value()) {
    discovery_item.set_title(display_name.value());
  } else {
    discovery_item.set_title(details.name());
  }

  discovery_item.set_description(strings.initial_notification_description());
  discovery_item.set_type(proto::NearbyType::NEARBY_TYPE_DEVICE);
  discovery_item.set_action_url_type(
      proto::ResolvedUrlType::RESOLVED_URL_TYPE_APP);
  discovery_item.set_action_url(details.intent_uri());
  discovery_item.set_last_observation_timestamp_millis(
      absl::ToInt64Milliseconds(absl::Now() - absl::UnixEpoch()));
  discovery_item.set_first_observation_timestamp_millis(
      absl::ToInt64Milliseconds(absl::Now() - absl::UnixEpoch()));
  discovery_item.set_state(proto::StoredDiscoveryItem::STATE_ENABLED);
  discovery_item.set_icon_fife_url(
      metadata->GetResponse().device().image_url());
  discovery_item.set_icon_png(metadata->GetResponse().image());
  discovery_item.add_stored_relevances()->mutable_relevance()->set_evaluation(
      proto::Evaluation::EVALUATION_GREAT);
  discovery_item.set_last_user_experience(
      proto::StoredDiscoveryItem::EXPERIENCE_GOOD);
  if (details.has_anti_spoofing_key_pair()) {
    discovery_item.set_authentication_public_key_secp256r1(
        details.anti_spoofing_key_pair().public_key());
  }
  if (details.has_true_wireless_images()) {
    auto* images = discovery_item.mutable_fast_pair_information()
                       ->mutable_true_wireless_images();
    images->set_left_bud_url(details.true_wireless_images().left_bud_url());
    images->set_right_bud_url(details.true_wireless_images().right_bud_url());
    images->set_case_url(details.true_wireless_images().case_url());
  }

  auto* fast_pair_strings = discovery_item.mutable_fast_pair_strings();
  fast_pair_strings->set_tap_to_pair_with_account(
      strings.initial_notification_description());
  fast_pair_strings->set_tap_to_pair_without_account(
      strings.initial_notification_description_no_account());
  fast_pair_strings->set_initial_pairing_description(
      strings.initial_pairing_description());
  fast_pair_strings->set_pairing_finished_companion_app_installed(
      strings.connect_success_companion_app_installed());
  fast_pair_strings->set_pairing_finished_companion_app_not_installed(
      strings.connect_success_companion_app_not_installed());
  fast_pair_strings->set_subsequent_pairing_description(
      strings.subsequent_pairing_description());
  fast_pair_strings->set_retroactive_pairing_description(
      strings.retroactive_pairing_description());
  fast_pair_strings->set_wait_app_launch_description(
      strings.wait_launch_companion_app_description());
  fast_pair_strings->set_pairing_fail_description(
      strings.fail_connect_go_to_settings_description());
  fast_pair_strings->set_confirm_pin_title(strings.confirm_pin_title());
  fast_pair_strings->set_confirm_pin_description(
      strings.confirm_pin_description());
  fast_pair_strings->set_sync_contacts_title(strings.sync_contacts_title());
  fast_pair_strings->set_sync_contacts_description(
      strings.sync_contacts_description());
  fast_pair_strings->set_sync_sms_title(strings.sync_sms_title());
  fast_pair_strings->set_sync_sms_description(strings.sync_sms_description());

  device->set_discovery_item_bytes(discovery_item.SerializeAsString());
}

void BuildFastPairInfo(::nearby::fastpair::proto::FastPairInfo* fast_pair_info,
                       proto::OptInStatus opt_in_status) {
  fast_pair_info->set_opt_in_status(opt_in_status);
}
}  // namespace fastpair
}  // namespace nearby
