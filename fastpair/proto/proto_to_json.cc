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

#include "fastpair/proto/proto_to_json.h"

#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace {
using json = ::nlohmann::json;
std::string Encode(absl::string_view str) {
  return absl::WebSafeBase64Escape(str);
}

std::string TruncateString(absl::string_view str) {
  if (str.length() <= 10) return std::string(str);
  return absl::StrCat(str.substr(0, 5), "...",
                      str.substr(str.length() - 5, str.length()));
}
}  // namespace

// GetObservedDeviceRequest/Response
json FastPairProtoToJson(
    const nearby::fastpair::proto::GetObservedDeviceRequest& request) {
  json dict = json::object();
  dict["device_id"] = request.device_id();
  dict["mode"] = request.mode();
  dict["locale"] = request.locale();
  dict["hex_device_id"] = request.hex_device_id();
  dict["max_icon_size_pixels"] = request.max_icon_size_pixels();
  return dict;
}

json FastPairProtoToJson(
    const nearby::fastpair::proto::GetObservedDeviceResponse& response) {
  json dict = json::object();
  dict["device"] = FastPairProtoToJson(response.device());
  dict["observed_device_strings "] = FastPairProtoToJson(response.strings());
  return dict;
}

// UserReadDevicesRequest/Response
json FastPairProtoToJson(
    const nearby::fastpair::proto::UserReadDevicesRequest& request) {
  json dict = json::object();
  dict["secondary_id"] = request.secondary_id();
  return dict;
}

json FastPairProtoToJson(
    const nearby::fastpair::proto::UserReadDevicesResponse& response) {
  json dict = json::object();
  json fast_pair_info_list = json::array();
  for (const auto& fast_pair_info : response.fast_pair_info()) {
    fast_pair_info_list.push_back(FastPairProtoToJson(fast_pair_info));
  }
  dict["fast_pair_info"] = std::move(fast_pair_info_list);
  return dict;
}

// UserWriteDeviceRequest
json FastPairProtoToJson(
    const nearby::fastpair::proto::UserWriteDeviceRequest& request) {
  json dict = json::object();
  dict["fast_pair_info"] = FastPairProtoToJson(request.fast_pair_info());
  return dict;
}

// UserDeleteDeviceRequest/Response
json FastPairProtoToJson(
    const nearby::fastpair::proto::UserDeleteDeviceRequest& request) {
  json dict = json::object();
  dict["hex_account_key"] = TruncateString(Encode(request.hex_account_key()));
  return dict;
}

json FastPairProtoToJson(
    const nearby::fastpair::proto::UserDeleteDeviceResponse& response) {
  json dict = json::object();
  dict["success"] = response.success();
  json error_messages = json::array();
  for (const auto& error_message : response.error_messages()) {
    error_messages.push_back(error_message);
  }
  dict["error_messages"] = std::move(error_messages);
  return dict;
}

json FastPairProtoToJson(
    const nearby::fastpair::proto::FastPairInfo& fast_pair_info) {
  json dict = json::object();
  if (fast_pair_info.has_device()) {
    dict["device"] = FastPairProtoToJson(fast_pair_info.device());
  } else if (fast_pair_info.has_opt_in_status()) {
    dict["opt_in_status"] = fast_pair_info.opt_in_status();
  }
  return dict;
}

json FastPairProtoToJson(
    const nearby::fastpair::proto::FastPairDevice& fast_pair_device) {
  json dict = json::object();
  dict["account_key"] = TruncateString(Encode(fast_pair_device.account_key()));
  return dict;
}

json FastPairProtoToJson(
    const nearby::fastpair::proto::ObservedDeviceStrings& strings) {
  json dict = json::object();
  dict["locale"] = strings.locale();
  return dict;
}

json FastPairProtoToJson(const nearby::fastpair::proto::Device& device) {
  json dict = json::object();
  dict["id"] = device.id();
  dict["project_number"] = device.project_number();
  dict["notification_type"] = device.notification_type();
  dict["image_url"] = device.image_url();
  dict["name"] = device.name();
  dict["intent_uri"] = device.intent_uri();
  dict["ble_tx_power"] = device.ble_tx_power();
  dict["trigger_distance"] = device.trigger_distance();
  dict["device_type"] = device.device_type();
  dict["display_name"] = device.display_name();
  return dict;
}
}  // namespace fastpair
}  // namespace nearby
