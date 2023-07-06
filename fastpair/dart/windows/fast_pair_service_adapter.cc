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

#include "fastpair/fast_pair_service.h"
#include "fastpair/internal/fast_pair_seeker_impl.h"
#include "fastpair/keyed_service/fast_pair_mediator.h"
#include "fastpair/keyed_service/fast_pair_mediator_factory.h"
#include "fastpair/plugins/windows_admin_plugin.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace windows {

// Set to 1 to use Mediator. Set to 0 to use scalable seeker.
#define USE_MEDIATOR 0

namespace {
#if USE_MEDIATOR
Mediator *pMediator_ = nullptr;
#else
WindowsAdminPlugin::PluginState *plugin_state = nullptr;
#endif /* USE_MEDIATOR */

WindowsAdminPlugin::PluginState *InitFastPairService() {
  NEARBY_LOGS(INFO) << "[[ Init Fast Pair Service. ]]";
  plugin_state = new WindowsAdminPlugin::PluginState();
  plugin_state->fast_pair_service = std::make_unique<FastPairService>();
  plugin_state->fast_pair_service->RegisterPluginProvider(
      "admin", std::make_unique<WindowsAdminPlugin::Provider>(plugin_state));
  return plugin_state;
}

void CloseFastPairService(void *instance) {
  NEARBY_LOGS(INFO) << "[[ Closing Fast Pair Service. ]]";
  CHECK_EQ(plugin_state, instance);
  delete plugin_state;
  plugin_state = nullptr;
  NEARBY_LOGS(INFO) << "[[ Successfully closed Fast Pair Service. ]]";
}

void StartFastPairServiceScan(void *instance) {
  CHECK_EQ(plugin_state, instance);
  FastPairSeekerExt *seeker = static_cast<FastPairSeekerExt *>(
      plugin_state->fast_pair_service->GetSeeker());
  absl::Status status = seeker->StartFastPairScan();
  NEARBY_LOGS(INFO) << "Start FP scan result: " << status;
}
}  // namespace

void *InitMediator() {
#if defined(NEARBY_LOG_SEVERITY)
  // Direct override of logging level.
  NEARBY_LOG_SET_SEVERITY(NEARBY_LOG_SEVERITY);
#endif  // LOG_SEVERITY_VERBOSE;

#if USE_MEDIATOR
  pMediator_ = MediatorFactory::GetInstance()->CreateMediator();
  return pMediator_;
#else
  return InitFastPairService();
#endif /* USE_MEDIATOR */
}

void CloseMediator(void *instance) {
#if USE_MEDIATOR
  NEARBY_LOGS(INFO) << "[[ Closing Fast Pair Mediator. ]]";
  if (pMediator_ != nullptr) delete pMediator_;
  NEARBY_LOGS(INFO) << "[[ Successfully closed Fast Pair Mediator. ]]";
#else
  CloseFastPairService(instance);
#endif /* USE_MEDIATOR */
}

void __stdcall StartScan(void *instance) {
  NEARBY_LOGS(INFO) << "StartScan is called";
#if USE_MEDIATOR
  if (pMediator_ == nullptr) {
    NEARBY_LOGS(VERBOSE) << "The pMediator is a null pointer.";
    return;
  }
  Mediator *mediator = static_cast<Mediator *>(instance);
  mediator->StartScanning();
#else
  StartFastPairServiceScan(instance);
#endif /* USE_MEDIATOR */
}

void __stdcall AddNotificationControllerObserver(
    void *instance, FastPairNotificationController::Observer *observer) {
  NEARBY_LOGS(INFO) << "AddNotificationControllerObserver is called";
#if USE_MEDIATOR
  if (pMediator_ == nullptr) {
    NEARBY_LOGS(VERBOSE) << "The pMediator is a null pointer.";
    return;
  }
  Mediator *mediator = static_cast<Mediator *>(instance);
  mediator->GetNotificationController()->AddObserver(observer);
#else
  CHECK_EQ(plugin_state, instance);
  plugin_state->observers.AddObserver(observer);
#endif /* USE_MEDIATOR */
}

void __stdcall RemoveNotificationControllerObserver(
    void *instance, FastPairNotificationController::Observer *observer) {
#if USE_MEDIATOR
  if (pMediator_ == nullptr) {
    NEARBY_LOGS(VERBOSE) << "The pMediator is a null pointer.";
    return;
  }
  Mediator *mediator = static_cast<Mediator *>(instance);
  mediator->GetNotificationController()->RemoveObserver(observer);
#else
  CHECK_EQ(plugin_state, instance);
  plugin_state->observers.RemoveObserver(observer);
#endif /* USE_MEDIATOR */
}

void __stdcall DiscoveryClicked(void *instance, DiscoveryAction action) {
#if USE_MEDIATOR
  Mediator *mediator = static_cast<Mediator *>(instance);
  mediator->GetNotificationController()->OnDiscoveryClicked(action);
#else
  CHECK_EQ(plugin_state, instance);
  plugin_state->DiscoveryClicked(action);
#endif /* USE_MEDIATOR */
}

void __stdcall SetIsScreenLocked(bool is_locked) {
#if USE_MEDIATOR
  if (pMediator_ == nullptr) {
    NEARBY_LOGS(INFO) << "The pMediator is a null pointer.";
    return;
  }
  NEARBY_LOGS(INFO) << "SetIsScreenLocked :" << is_locked;
  pMediator_->SetIsScreenLocked(is_locked);
#else
  if (plugin_state == nullptr) {
    NEARBY_LOGS(INFO) << "Plugin not initialized";
    return;
  }
  plugin_state->SetIsScreenLocked(is_locked);
#endif /* USE_MEDIATOR */
}

}  // namespace windows
}  // namespace fastpair
}  // namespace nearby
