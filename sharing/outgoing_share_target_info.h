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

#ifndef THIRD_PARTY_NEARBY_SHARING_OUTGOING_SHARE_TARGET_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_OUTGOING_SHARE_TARGET_INFO_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sharing/nearby_connections_types.h"
#include "sharing/share_target_info.h"

namespace nearby {
namespace sharing {

// A description of the outgoing connection to a remote device.
class OutgoingShareTargetInfo : public ShareTargetInfo {
 public:
  OutgoingShareTargetInfo();
  OutgoingShareTargetInfo(OutgoingShareTargetInfo&&);
  OutgoingShareTargetInfo& operator=(OutgoingShareTargetInfo&&);
  ~OutgoingShareTargetInfo() override;

  const std::optional<std::string>& obfuscated_gaia_id() const {
    return obfuscated_gaia_id_;
  }

  void set_obfuscated_gaia_id(std::string obfuscated_gaia_id) {
    obfuscated_gaia_id_ = std::move(obfuscated_gaia_id);
  }

  const std::vector<Payload>& text_payloads() const { return text_payloads_; }

  void set_text_payloads(std::vector<Payload> payloads) {
    text_payloads_ = std::move(payloads);
  }

  const std::vector<Payload>& wifi_credentials_payloads() const {
    return wifi_credentials_payloads_;
  }

  void set_wifi_credentials_payloads(std::vector<Payload> payloads) {
    wifi_credentials_payloads_ = std::move(payloads);
  }

  const std::vector<Payload>& file_payloads() const { return file_payloads_; }

  void set_file_payloads(std::vector<Payload> payloads) {
    file_payloads_ = std::move(payloads);
  }

  Status connection_layer_status() const { return connection_layer_status_; }

  void set_connection_layer_status(Status status) {
    connection_layer_status_ = status;
  }

  std::vector<Payload> ExtractTextPayloads();
  std::vector<Payload> ExtractFilePayloads();
  std::vector<Payload> ExtractWifiCredentialsPayloads();
  std::optional<Payload> ExtractNextPayload();

 private:
  std::optional<std::string> obfuscated_gaia_id_;
  std::vector<Payload> text_payloads_;
  std::vector<Payload> file_payloads_;
  std::vector<Payload> wifi_credentials_payloads_;
  Status connection_layer_status_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_OUTGOING_SHARE_TARGET_INFO_H_
