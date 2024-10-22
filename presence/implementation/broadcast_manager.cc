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

#include "presence/implementation/broadcast_manager.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/implementation/advertisement_factory.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/mediums/advertisement_data.h"

namespace nearby {
namespace presence {
namespace {

using AdvertisingCallback =
    ::nearby::api::ble_v2::BleMedium::AdvertisingCallback;
using AdvertisingSession = ::nearby::api::ble_v2::BleMedium::AdvertisingSession;
using LocalCredential = internal::LocalCredential;

uint16_t SaltToInt(absl::string_view salt) {
  if (salt.length() < 2) return 0;
  uint16_t b0 = salt[0];
  uint16_t b1 = salt[1];
  return b0 << 8 | b1;
}
std::string SaltFromInt(uint16_t x) {
  std::string salt;
  salt.resize(2);
  salt[0] = x >> 8 & 0xFF;
  salt[1] = x & 0xFF;
  return salt;
}

// Selects a salt that has not been used yet. The salt is added to
// `credential.consumed_salts`.
// We may fail to find an unused salt. In this unlikely event, an already
// consumed salt is returned.
std::string SelectSalt(LocalCredential& credential,
                       absl::string_view preferred_salt) {
  // NP certificate guidelines say that we should try to get an unused salt 128
  // times.
  constexpr int kMaxSaltSelectRetries = 128;

  uint16_t s = SaltToInt(preferred_salt);
  for (int i = 0; i < kMaxSaltSelectRetries; i++) {
    if (!credential.consumed_salts().contains(s)) {
      break;
    }
    s = nearby::RandData<uint16_t>();
  }
  credential.mutable_consumed_salts()->insert({s, true});
  return SaltFromInt(s);
}

}  // namespace

absl::StatusOr<BroadcastSessionId> BroadcastManager::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  absl::StatusOr<BaseBroadcastRequest> request =
      BaseBroadcastRequest::Create(broadcast_request);
  if (!request.ok()) {
    NEARBY_LOGS(WARNING) << "Invalid broadcast request, reason: "
                         << request.status();
    callback.start_broadcast_cb(request.status());
    return request.status();
  }
  BroadcastSessionId id = GenerateBroadcastSessionId();
  RunOnServiceControllerThread(
      "start-broadcast",
      [this, id, power_mode = broadcast_request.power_mode, request = *request,
       broadcast_callback = std::move(
           callback)]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) mutable {
        sessions_.insert({id, BroadcastSessionState(
                                  std::move(broadcast_callback), power_mode)});
        FetchCredentials(id, std::move(request));
      });
  return id;
}

void BroadcastManager::FetchCredentials(
    BroadcastSessionId id, BaseBroadcastRequest broadcast_request) {
  absl::StatusOr<CredentialSelector> credential_selector =
      AdvertisementFactory::GetCredentialSelector(broadcast_request);
  if (!credential_selector.ok()) {
    // Public advertisement, we don't need credential to advertise.
    Advertise(id, broadcast_request, /*credentials=*/{});
    return;
  }
  credential_manager_->GetLocalCredentials(
      *credential_selector,
      GetLocalCredentialsResultCallback{
          .credentials_fetched_cb =
              [this, id, broadcast_request = std::move(broadcast_request),
               selector = *credential_selector](
                  absl::StatusOr<
                      std::vector<::nearby::internal::LocalCredential>>
                      credentials) {
                if (!credentials.ok()) {
                  NEARBY_LOGS(WARNING)
                      << "Failed to fetch credentials, status: "
                      << credentials.status();
                  NotifyStartCallbackStatus(id, credentials.status());
                  return;
                }
                RunOnServiceControllerThread(
                    "advertise-non-public",
                    [this, id, broadcast_request = std::move(broadcast_request),
                     credentials = std::move(*credentials),
                     selector = std::move(selector)]()
                        ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) mutable {
                          absl::optional<LocalCredential> credential =
                              Advertise(id, broadcast_request, credentials);
                          if (credential) {
                            credential_manager_->UpdateLocalCredential(
                                selector, std::move(*credential),
                                {[](absl::Status status) {
                                  if (!status.ok()) {
                                    NEARBY_LOGS(WARNING)
                                        << "Failed to update private "
                                           "credential, status: "
                                        << status;
                                  }
                                }});
                          }
                        });
              }});
}

absl::optional<LocalCredential> BroadcastManager::SelectCredential(  // NOLINT
    BaseBroadcastRequest& broadcast_request,
    std::vector<LocalCredential> credentials) {
  if (credentials.empty()) {
    return absl::optional<LocalCredential>();  // NOLINT
  }
  auto credential =
      std::min_element(credentials.begin(), credentials.end(),
                       [](const LocalCredential& a, const LocalCredential& b) {
                         return a.start_time_millis() < b.start_time_millis();
                       });
  if (credential == credentials.end()) {
    NEARBY_LOGS(WARNING) << "No active credentials";
    return absl::optional<LocalCredential>();  // NOLINT
  }
  std::string salt = SelectSalt(*credential, broadcast_request.salt);
  if (salt != broadcast_request.salt) {
    NEARBY_VLOG(1) << "Changed salt";
    broadcast_request.salt = salt;
  }
  return *credential;
}

absl::optional<LocalCredential> BroadcastManager::Advertise(  // NOLINT
    BroadcastSessionId id, BaseBroadcastRequest broadcast_request,
    std::vector<LocalCredential> credentials) {
  auto it = sessions_.find(id);
  if (it == sessions_.end()) {
    NEARBY_LOGS(INFO) << "Broadcast session terminated, id: " << id;
    return absl::optional<LocalCredential>();  // NOLINT
  }
  absl::optional<LocalCredential> credential =  // NOLINT
      SelectCredential(broadcast_request, std::move(credentials));
  absl::StatusOr<AdvertisementData> advertisement =
      AdvertisementFactory().CreateAdvertisement(broadcast_request, credential);
  if (!advertisement.ok()) {
    NEARBY_LOGS(WARNING) << "Can't create advertisement, reason: "
                         << advertisement.status();
    NotifyStartCallbackStatus(id, advertisement.status());
    return absl::optional<LocalCredential>();  // NOLINT
  }
  std::unique_ptr<AdvertisingSession> session =
      mediums_->GetBle().StartAdvertising(
          *advertisement, it->second.GetPowerMode(),
          AdvertisingCallback{
              .start_advertising_result = [this, id](absl::Status status) {
                NotifyStartCallbackStatus(id, status);
              }});
  if (!session) {
    NotifyStartCallbackStatus(id,
                              absl::InternalError("Can't start advertising"));
    return absl::optional<LocalCredential>();  // NOLINT
  }
  it->second.SetAdvertisingSession(std::move(session));
  return credential;
}

void BroadcastManager::NotifyStartCallbackStatus(BroadcastSessionId id,
                                                 absl::Status status) {
  RunOnServiceControllerThread("started-broadcast-cb",
                               [this, id, status]()
                                   ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) {
                                     auto it = sessions_.find(id);
                                     if (it == sessions_.end()) {
                                       return;
                                     }
                                     it->second.CallStartedCallback(status);
                                     if (!status.ok()) {
                                       // Delete failed session.
                                       sessions_.erase(it);
                                     }
                                   });
}

void BroadcastManager::StopBroadcast(BroadcastSessionId id) {
  RunOnServiceControllerThread(
      "stop-broadcast", [this, id]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) {
        auto it = sessions_.find(id);
        if (it == sessions_.end()) {
          NEARBY_VLOG(1) << absl::StrFormat("BroadcastSession(0x%x) not found",
                                            id);
          return;
        }
        it->second.StopAdvertising();
        sessions_.erase(it);
      });
}

BroadcastSessionId BroadcastManager::GenerateBroadcastSessionId() {
  return nearby::RandData<BroadcastSessionId>();
}

void BroadcastManager::BroadcastSessionState::SetAdvertisingSession(
    std::unique_ptr<AdvertisingSession> session) {
  advertising_session_ = std::move(session);
}

void BroadcastManager::BroadcastSessionState::CallStartedCallback(
    absl::Status status) {
  BroadcastCallback callback = std::move(broadcast_callback_);
  if (callback.start_broadcast_cb) {
    callback.start_broadcast_cb(status);
  }
}

void BroadcastManager::BroadcastSessionState::StopAdvertising() {
  std::unique_ptr<AdvertisingSession> advertising_session =
      std::move(advertising_session_);
  if (advertising_session) {
    absl::Status status = advertising_session->stop_advertising();
    if (!status.ok()) {
      NEARBY_LOGS(WARNING) << "StopAdvertising error: " << status;
    }
  }
}

}  // namespace presence
}  // namespace nearby
