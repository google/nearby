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

#include <filesystem>  // NOLINT
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/nearby_file_handler.h"
#include "sharing/share_target.h"
#include "sharing/share_target_info.h"
#include "sharing/transfer_metadata.h"

namespace nearby::sharing {

// A description of the outgoing connection to a remote device.
class OutgoingShareTargetInfo : public ShareTargetInfo {
 public:
  OutgoingShareTargetInfo(std::string endpoint_id,
                          const ShareTarget& share_target,
                          std::function<void(OutgoingShareTargetInfo&,
                                             const TransferMetadata&)>
                              transfer_update_callback);
  OutgoingShareTargetInfo(OutgoingShareTargetInfo&&);
  OutgoingShareTargetInfo& operator=(OutgoingShareTargetInfo&&);
  ~OutgoingShareTargetInfo() override;

  bool IsIncoming() const override { return false; }

  const std::optional<std::string>& obfuscated_gaia_id() const {
    return obfuscated_gaia_id_;
  }

  void set_obfuscated_gaia_id(std::string obfuscated_gaia_id) {
    obfuscated_gaia_id_ = std::move(obfuscated_gaia_id);
  }

  const std::vector<Payload>& text_payloads() const { return text_payloads_; }

  const std::vector<Payload>& wifi_credentials_payloads() const {
    return wifi_credentials_payloads_;
  }

  const std::vector<Payload>& file_payloads() const { return file_payloads_; }

  Status connection_layer_status() const { return connection_layer_status_; }

  void set_connection_layer_status(Status status) {
    connection_layer_status_ = status;
  }

  std::vector<std::filesystem::path> GetFilePaths() const;

  void CreateTextPayloads();
  void CreateWifiCredentialsPayloads();
  // Create file payloads and update the file size of all file attachments.
  // The list of file infos must be sorted in the same order as the file
  // attachments in the share target.
  // Returns true if all file payloads are created successfully.
  bool CreateFilePayloads(
      const std::vector<NearbyFileHandler::FileInfo>& files);

  std::vector<Payload> ExtractTextPayloads();
  std::vector<Payload> ExtractFilePayloads();
  std::vector<Payload> ExtractWifiCredentialsPayloads();
  std::optional<Payload> ExtractNextPayload();

 protected:
  void InvokeTransferUpdateCallback(const TransferMetadata& metadata) override;
  bool OnNewConnection(NearbyConnection* connection) override;


 private:
  std::optional<std::string> obfuscated_gaia_id_;
  // All payloads are in the same order as the attachments in the share target.
  std::vector<Payload> text_payloads_;
  std::vector<Payload> file_payloads_;
  std::vector<Payload> wifi_credentials_payloads_;
  Status connection_layer_status_;
  std::function<void(OutgoingShareTargetInfo&, const TransferMetadata&)>
      transfer_update_callback_;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_OUTGOING_SHARE_TARGET_INFO_H_
