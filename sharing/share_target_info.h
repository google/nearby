// Copyright 2022-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_INFO_H_
#define THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_INFO_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/time/time.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/incoming_frames_reader.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/paired_key_verification_runner.h"
#include "sharing/payload_tracker.h"
#include "sharing/transfer_update_callback.h"

namespace nearby {
namespace sharing {

// Additional information about the connection to a remote device.
class ShareTargetInfo {
 public:
  ShareTargetInfo();
  ShareTargetInfo(ShareTargetInfo&&);
  ShareTargetInfo& operator=(ShareTargetInfo&&);
  virtual ~ShareTargetInfo();

  const std::optional<std::string>& endpoint_id() const { return endpoint_id_; }

  void set_endpoint_id(std::string endpoint_id) {
    endpoint_id_ = std::move(endpoint_id);
  }

  const std::optional<NearbyShareDecryptedPublicCertificate>& certificate()
      const {
    return certificate_;
  }

  void set_certificate(NearbyShareDecryptedPublicCertificate certificate) {
    certificate_ = std::move(certificate);
  }

  NearbyConnection* connection() const { return connection_; }

  void set_connection(NearbyConnection* connection) {
    connection_ = connection;
  }

  TransferUpdateCallback* transfer_update_callback() const {
    return transfer_update_callback_.get();
  }

  void set_transfer_update_callback(
      std::unique_ptr<TransferUpdateCallback> transfer_update_callback) {
    transfer_update_callback_ = std::move(transfer_update_callback);
  }

  const std::optional<std::string>& token() const { return token_; }

  void set_token(std::string token) { token_ = std::move(token); }

  IncomingFramesReader* frames_reader() const { return frames_reader_.get(); }

  void set_frames_reader(std::shared_ptr<IncomingFramesReader> frames_reader) {
    frames_reader_ = std::move(frames_reader);
  }

  PairedKeyVerificationRunner* key_verification_runner() {
    return key_verification_runner_.get();
  }

  void set_key_verification_runner(
      std::shared_ptr<PairedKeyVerificationRunner> key_verification_runner) {
    key_verification_runner_ = std::move(key_verification_runner);
  }

  std::weak_ptr<NearbyConnectionsManager::PayloadStatusListener>
  payload_tracker() {
    return payload_tracker_->GetWeakPtr();
  }

  void set_payload_tracker(std::shared_ptr<PayloadTracker> payload_tracker) {
    payload_tracker_ = std::move(payload_tracker);
  }

  int64_t session_id() { return session_id_; }

  void set_session_id(int64_t session_id) { session_id_ = session_id; }

  std::optional<absl::Time> connection_start_time() {
    return connection_start_time_;
  }

  void set_connection_start_time(
      std::optional<absl::Time> connection_start_time) {
    connection_start_time_ = connection_start_time;
  }

  ::location::nearby::proto::sharing::OSType os_type() { return os_type_; }

  void set_os_type(::location::nearby::proto::sharing::OSType os_type) {
    os_type_ = os_type;
  }

 private:
  std::optional<std::string> endpoint_id_;
  std::optional<NearbyShareDecryptedPublicCertificate> certificate_;
  NearbyConnection* connection_ = nullptr;
  std::unique_ptr<TransferUpdateCallback> transfer_update_callback_;
  std::optional<std::string> token_;
  std::shared_ptr<IncomingFramesReader> frames_reader_;
  std::shared_ptr<PairedKeyVerificationRunner> key_verification_runner_;
  std::shared_ptr<PayloadTracker> payload_tracker_;
  int64_t session_id_;
  std::optional<absl::Time> connection_start_time_;
  ::location::nearby::proto::sharing::OSType os_type_ =
      ::location::nearby::proto::sharing::OSType::UNKNOWN_OS_TYPE;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SHARE_TARGET_INFO_H_
