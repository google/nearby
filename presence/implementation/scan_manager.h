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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SCAN_MANAGER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SCAN_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/runnable.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_types.h"
#include "presence/implementation/advertisement_filter.h"
#include "presence/implementation/credential_manager.h"
#include "presence/implementation/mediums/mediums.h"
#include "presence/scan_request.h"

#ifdef USE_RUST_DECODER
#include "presence/implementation/advertisement_decoder_rust_impl.h"
#else
#include "presence/implementation/advertisement_decoder_impl.h"
#endif


namespace nearby {
namespace presence {

// The instance of ScanManager is owned by `ServiceControllerImpl`.
// Helping service controller to manage scan requests and callbacks.
class ScanManager {
 public:
  using SingleThreadExecutor = ::nearby::SingleThreadExecutor;
  using Mutex = ::nearby::Mutex;
  using MutexLock = ::nearby::MutexLock;
  using ScanningSession = ::nearby::api::ble_v2::BleMedium::ScanningSession;
  using Runnable = ::nearby::Runnable;
  using BleAdvertisementData = ::nearby::api::ble_v2::BleAdvertisementData;
  using SharedCredential = ::nearby::internal::SharedCredential;
  using IdentityType = ::nearby::internal::IdentityType;

  ScanManager(Mediums& mediums, CredentialManager& credential_manager,
              SingleThreadExecutor& executor) {
    mediums_ = &mediums, credential_manager_ = &credential_manager;
    executor_ = &executor;
  }
  ~ScanManager() = default;

  ScanSessionId StartScan(ScanRequest scan_request, ScanCallback cb);
  void StopScan(ScanSessionId session_id);
  // Below functions are test only.
  // Reference: go/totw/135#augmenting-the-public-api-for-tests
  int ScanningCallbacksLengthForTest();

 private:
  struct ScanSessionState {
    ScanRequest request;
    ScanCallback callback;
    absl::flat_hash_map<IdentityType, std::vector<SharedCredential>>
        credentials;
    AdvertisementDecoderImpl decoder;
    AdvertisementFilter advertisement_filter;
    std::unique_ptr<ScanningSession> scanning_session;
  };
  void NotifyFoundBle(ScanSessionId id, BleAdvertisementData data,
                      absl::string_view remote_address)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void NotifyLostBle(ScanSessionId id, absl::string_view remote_address)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void FetchCredentials(ScanSessionId id, const ScanRequest& scan_request)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void UpdateCredentials(ScanSessionId id, IdentityType identity_type,
                         std::vector<SharedCredential> credentials)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void RunOnServiceControllerThread(absl::string_view name, Runnable runnable) {
    executor_->Execute(std::string(name), std::move(runnable));
  }
  Mediums* mediums_;
  CredentialManager* credential_manager_;
  absl::flat_hash_map<ScanSessionId, ScanSessionState> scan_sessions_
      ABSL_GUARDED_BY(*executor_);
  absl::flat_hash_map<std::string, std::string>
      device_address_to_endpoint_id_map_
      ABSL_GUARDED_BY(*executor_);
  SingleThreadExecutor* executor_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SCAN_MANAGER_H_
