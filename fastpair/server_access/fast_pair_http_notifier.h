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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_HTTP_NOTIFIER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_HTTP_NOTIFIER_H_

#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "internal/base/observer_list.h"

namespace nearby {
namespace fastpair {

// Interface for passing HTTP Responses/Requests to observers, by passing
// instances of this class to each HTTP Client.
class FastPairHttpNotifier {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when HTTP RPC is made for GetObservedDeviceRequest/Response
    virtual void OnGetObservedDeviceRequest(
        const proto::GetObservedDeviceRequest& request) = 0;
    virtual void OnGetObservedDeviceResponse(
        const proto::GetObservedDeviceResponse& response) = 0;

    // Called when HTTP RPC is made for UserReadDevicesRequest/Response
    virtual void OnUserReadDevicesRequest(
        const proto::UserReadDevicesRequest& request) = 0;
    virtual void OnUserReadDevicesResponse(
        const proto::UserReadDevicesResponse& response) = 0;

    // Called when HTTP RPC is made for UserWriteDeviceRequest/Response
    virtual void OnUserWriteDeviceRequest(
        const proto::UserWriteDeviceRequest& request) = 0;
    virtual void OnUserWriteDeviceResponse(
        const proto::UserWriteDeviceResponse& response) = 0;

    // Called when HTTP RPC is made for UserDeleteDeviceRequest/Response
    virtual void OnUserDeleteDeviceRequest(
        const proto::UserDeleteDeviceRequest& request) = 0;
    virtual void OnUserDeleteDeviceResponse(
        const proto::UserDeleteDeviceResponse& response) = 0;
  };

  FastPairHttpNotifier() = default;
  FastPairHttpNotifier(const FastPairHttpNotifier&) = delete;
  FastPairHttpNotifier& operator=(const FastPairHttpNotifier&) = delete;
  ~FastPairHttpNotifier() = default;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies all observers of GetObservedDeviceRequest/Response
  void NotifyOfRequest(const proto::GetObservedDeviceRequest& request);
  void NotifyOfResponse(const proto::GetObservedDeviceResponse& response);

  // Notifies all observers of UserReadDevicesRequest/Response
  void NotifyOfRequest(const proto::UserReadDevicesRequest& request);
  void NotifyOfResponse(const proto::UserReadDevicesResponse& response);

  // Notifies all observers of UserWriteDeviceRequest/Response
  void NotifyOfRequest(const proto::UserWriteDeviceRequest& request);
  void NotifyOfResponse(const proto::UserWriteDeviceResponse& response);

  // Notifies all observers of UserDeleteDeviceRequest/Response
  void NotifyOfRequest(const proto::UserDeleteDeviceRequest& request);
  void NotifyOfResponse(const proto::UserDeleteDeviceResponse& response);

 private:
  ObserverList<Observer> observers_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_HTTP_NOTIFIER_H_
