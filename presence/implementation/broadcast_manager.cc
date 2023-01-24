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

#include <memory>
#include <utility>
#include <vector>

#include "internal/crypto/random.h"
#include "presence/implementation/advertisement_factory.h"

namespace nearby {
namespace presence {
namespace {

using AdvertisingCallback =
    ::nearby::api::ble_v2::BleMedium::AdvertisingCallback;
using AdvertisingSession = ::nearby::api::ble_v2::BleMedium::AdvertisingSession;

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
  credential_manager_->GetPrivateCredentials(
      *credential_selector,
      GetPrivateCredentialsResultCallback{
          .credentials_fetched_cb =
              [this, id, broadcast_request = std::move(broadcast_request)](
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
                     credentials = std::move(*credentials)]()
                        ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_) {
                          Advertise(id, broadcast_request, credentials);
                        });
              }});
}

void BroadcastManager::Advertise(BroadcastSessionId id,
                                 BaseBroadcastRequest broadcast_request,
                                 std::vector<LocalCredential> credentials) {
  auto it = sessions_.find(id);
  if (it == sessions_.end()) {
    NEARBY_LOGS(INFO) << "Broadcast session terminated, id: " << id;
    return;
  }
  absl::StatusOr<AdvertisementData> advertisement =
      AdvertisementFactory().CreateAdvertisement(broadcast_request,
                                                 credentials);
  if (!advertisement.ok()) {
    NEARBY_LOGS(WARNING) << "Can't create advertisement, reason: "
                         << advertisement.status();
    NotifyStartCallbackStatus(id, advertisement.status());
    return;
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
    return;
  }
  it->second.SetAdvertisingSession(std::move(session));
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
          NEARBY_LOGS(VERBOSE)
              << absl::StrFormat("BroadcastSession(0x%x) not found", id);
          return;
        }
        it->second.StopAdvertising();
        sessions_.erase(it);
      });
}

BroadcastSessionId BroadcastManager::GenerateBroadcastSessionId() {
  return ::crypto::RandData<BroadcastSessionId>();
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
