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

#include "fastpair/server_access/fast_pair_http_notifier.h"

#include "nlohmann/json.hpp"
#include "fastpair/proto/proto_to_json.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

void FastPairHttpNotifier::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairHttpNotifier::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Notifies all observers of GetObservedDeviceRequest/Response
void FastPairHttpNotifier::NotifyOfRequest(
    const proto::GetObservedDeviceRequest& request) {
  NEARBY_LOGS(VERBOSE) << ": GetObservedDeviceRequest="
                       << FastPairProtoToJson(request).dump();
  for (auto& observer : observers_.GetObservers())
    observer->OnGetObservedDeviceRequest(request);
}

void FastPairHttpNotifier::NotifyOfResponse(
    const proto::GetObservedDeviceResponse& response) {
  NEARBY_LOGS(VERBOSE) << ": GetObservedDeviceResponse="
                       << FastPairProtoToJson(response).dump();
  for (auto& observer : observers_.GetObservers())
    observer->OnGetObservedDeviceResponse(response);
}

// Notifies all observers of UserReadDevicesRequest/Response
void FastPairHttpNotifier::NotifyOfRequest(
    const proto::UserReadDevicesRequest& request) {
  NEARBY_LOGS(VERBOSE) << ": UserReadDevicesRequest="
                       << FastPairProtoToJson(request).dump();
  for (auto& observer : observers_.GetObservers())
    observer->OnUserReadDevicesRequest(request);
}

void FastPairHttpNotifier::NotifyOfResponse(
    const proto::UserReadDevicesResponse& response) {
  NEARBY_LOGS(VERBOSE) << ": UserReadDevicesResponse="
                       << FastPairProtoToJson(response).dump();
  for (auto& observer : observers_.GetObservers())
    observer->OnUserReadDevicesResponse(response);
}

// Notifies all observers of UserWriteDeviceRequest/Response
void FastPairHttpNotifier::NotifyOfRequest(
    const proto::UserWriteDeviceRequest& request) {
  NEARBY_LOGS(VERBOSE) << ": UserWriteDeviceRequest="
                       << FastPairProtoToJson(request).dump();
  for (auto& observer : observers_.GetObservers())
    observer->OnUserWriteDeviceRequest(request);
}

void FastPairHttpNotifier::NotifyOfResponse(
    const proto::UserWriteDeviceResponse& response) {
  for (auto& observer : observers_.GetObservers())
    observer->OnUserWriteDeviceResponse(response);
}

// Notifies all observers of UserDeleteDeviceRequest/Response
void FastPairHttpNotifier::NotifyOfRequest(
    const proto::UserDeleteDeviceRequest& request) {
  NEARBY_LOGS(VERBOSE) << ": UserDeleteDeviceRequest="
                       << FastPairProtoToJson(request).dump();
  for (auto& observer : observers_.GetObservers())
    observer->OnUserDeleteDeviceRequest(request);
}
void FastPairHttpNotifier::NotifyOfResponse(
    const proto::UserDeleteDeviceResponse& response) {
  NEARBY_LOGS(VERBOSE) << ": UserDeleteDeviceResponse="
                       << FastPairProtoToJson(response).dump();
  for (auto& observer : observers_.GetObservers())
    observer->OnUserDeleteDeviceResponse(response);
}

}  // namespace fastpair
}  // namespace nearby
