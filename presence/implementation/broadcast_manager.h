// Copyright 2020 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_BROADCAST_MANAGER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_BROADCAST_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/proto/credential.pb.h"
#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/credential_manager.h"
#include "presence/implementation/mediums/mediums.h"

namespace nearby {
namespace presence {

// The instance of BroadcastManager is owned by {@code ServiceControllerImpl}.
// Helping service controller to manage broadcast requests and callbacks.

class BroadcastManager {
 public:
  using SingleThreadExecutor = ::nearby::SingleThreadExecutor;
  using AdvertisingSession =
      ::nearby::api::ble_v2::BleMedium::AdvertisingSession;
  using Runnable = ::nearby::Runnable;
  using LocalCredential = internal::LocalCredential;
  BroadcastManager(Mediums& mediums, CredentialManager& credential_manager,
                   SingleThreadExecutor& executor) {
    mediums_ = &mediums, credential_manager_ = &credential_manager,
    executor_ = &executor;
  }
  ~BroadcastManager() = default;
  absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback);
  void StopBroadcast(BroadcastSessionId);

 private:
  Mediums* mediums_;
  CredentialManager* credential_manager_;
  SingleThreadExecutor* executor_;
  class BroadcastSessionState {
   public:
    explicit BroadcastSessionState(BroadcastCallback broadcast_callback,
                                   PowerMode power_mode)
        : broadcast_callback_(std::move(broadcast_callback)),
          power_mode_(power_mode) {}

    void SetAdvertisingSession(std::unique_ptr<AdvertisingSession> session);
    void CallStartedCallback(absl::Status status);
    void StopAdvertising();

    PowerMode GetPowerMode() { return power_mode_; }

   private:
    BroadcastCallback broadcast_callback_;
    PowerMode power_mode_;
    std::unique_ptr<AdvertisingSession> advertising_session_;
  };
  BroadcastSessionId GenerateBroadcastSessionId();
  void NotifyStartCallbackStatus(BroadcastSessionId id, absl::Status status);
  void RunOnServiceControllerThread(absl::string_view name, Runnable runnable) {
    executor_->Execute(std::string(name), std::move(runnable));
  }
  void FetchCredentials(BroadcastSessionId id,
                        BaseBroadcastRequest broadcast_request)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  absl::optional<LocalCredential> SelectCredential(
      BaseBroadcastRequest& broadcast_request,
      std::vector<LocalCredential> credentials);

  // Returns the private credential, if any, selected to generate the
  // advertisement. A salt used in the advertisement is added to the returned
  // private credential. The caller must save it in the storage.
  absl::optional<LocalCredential> Advertise(
      BroadcastSessionId id, BaseBroadcastRequest broadcast_request,
      std::vector<LocalCredential> credentials)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  absl::flat_hash_map<BroadcastSessionId, BroadcastSessionState> sessions_
      ABSL_GUARDED_BY(*executor_);
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_BROADCAST_MANAGER_H_
