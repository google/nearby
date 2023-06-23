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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_DART_WINDOWS_FAST_PAIR_SERVICE_ADAPTER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_DART_WINDOWS_FAST_PAIR_SERVICE_ADAPTER_H_

#define DLL_EXPORT extern "C" __declspec(dllexport)

#include <string>

#include "fastpair/keyed_service/fast_pair_mediator.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"

namespace nearby {
namespace fastpair {
namespace windows {

// Initiates a default Mediator instance. Return the instance handle to client.
DLL_EXPORT void *__stdcall InitMediator();

// Starts scanning service
DLL_EXPORT void __stdcall StartScan(Mediator *pMediator);

// Adds a notification controller observer to the service.
DLL_EXPORT void __stdcall AddNotificationControllerObserver(
    Mediator *pMediator, FastPairNotificationController::Observer *observer);

// Removes a notification controller observer to the service.
DLL_EXPORT void __stdcall RemoveNotificationControllerObserver(
    Mediator *pMediator, FastPairNotificationController::Observer *observer);

// Triggers discovery click action
DLL_EXPORT void DiscoveryClicked(Mediator *pMediator, DiscoveryAction action);

// Sends screen locked event.
DLL_EXPORT void __stdcall SetIsScreenLocked(bool is_locked);

}  // namespace windows
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_DART_WINDOWS_FAST_PAIR_SERVICE_ADAPTER_H_
