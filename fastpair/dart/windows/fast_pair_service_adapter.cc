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

#include "fastpair/dart/windows/fast_pair_service_adapter.h"

#include <memory>
#include <string>

#include "fastpair/keyed_service/fast_pair_mediator.h"
#include "fastpair/keyed_service/fast_pair_mediator_factory.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace windows {

static Mediator *pMediator_ = nullptr;
void *InitMediator() {
#if defined(NEARBY_LOG_SEVERITY)
  // Direct override of logging level.
  NEARBY_LOG_SET_SEVERITY(NEARBY_LOG_SEVERITY);
#endif  // LOG_SEVERITY_VERBOSE;

  Mediator *pMediator = MediatorFactory::GetInstance()->CreateMediator();
  pMediator_ = pMediator;
  return pMediator_;
}

void CloseMediator(Mediator *pMediator) {
  NEARBY_LOGS(INFO) << "[[ Closing Fast Pair Mediator. ]]";
  if (pMediator_ != nullptr) delete pMediator_;
  NEARBY_LOGS(INFO) << "[[ Successfully closed Fast Pair Mediator. ]]";
}

void __stdcall StartScan(Mediator *pMediator) {
  NEARBY_LOGS(INFO) << "StartScan is called";
  if (pMediator_ == nullptr) {
    NEARBY_LOGS(VERBOSE) << "The pMediator is a null pointer.";
    return;
  }
  pMediator_->StartScanning();
}

void __stdcall AddNotificationControllerObserver(
    Mediator *pMediator, FastPairNotificationController::Observer *observer) {
  NEARBY_LOGS(INFO) << "AddNotificationControllerObserver is called";
  if (pMediator_ == nullptr) {
    NEARBY_LOGS(VERBOSE) << "The pMediator is a null pointer.";
    return;
  }
  static_cast<Mediator *>(pMediator)->GetNotificationController()->AddObserver(
      observer);
}

void __stdcall RemoveNotificationControllerObserver(
    Mediator *pMediator, FastPairNotificationController::Observer *observer) {
  if (pMediator_ == nullptr) {
    NEARBY_LOGS(VERBOSE) << "The pMediator is a null pointer.";
    return;
  }
  static_cast<Mediator *>(pMediator)
      ->GetNotificationController()
      ->RemoveObserver(observer);
}

void __stdcall DiscoveryClicked(Mediator *pMediator, DiscoveryAction action) {
  static_cast<Mediator *>(pMediator)
      ->GetNotificationController()
      ->OnDiscoveryClicked(action);
}

void __stdcall SetIsScreenLocked(bool is_locked) {
  if (pMediator_ == nullptr) {
    NEARBY_LOGS(INFO) << "The pMediator is a null pointer.";
    return;
  }
  NEARBY_LOGS(INFO) << "SetIsScreenLocked :" << is_locked;
  pMediator_->SetIsScreenLocked(is_locked);
}
}  // namespace windows
}  // namespace fastpair
}  // namespace nearby
