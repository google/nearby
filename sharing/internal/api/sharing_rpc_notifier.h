// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_RPC_NOTIFIER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_RPC_NOTIFIER_H_

#include "sharing/proto/certificate_rpc.pb.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/device_rpc.pb.h"

namespace nearby::sharing::api {

// Interface for passing RPC Responses/Requests to observers, by passing
// instances of this class to each RPC Client.
class SharingRpcNotifier {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when HTTP RPC is made for request and responses.
    virtual void OnUpdateDeviceRequest(
        const nearby::sharing::proto::UpdateDeviceRequest& request) = 0;
    virtual void OnUpdateDeviceResponse(
        const nearby::sharing::proto::UpdateDeviceResponse& response) = 0;
    virtual void OnListContactPeopleRequest(
        const nearby::sharing::proto::ListContactPeopleRequest& request) = 0;
    virtual void OnListContactPeopleResponse(
        const nearby::sharing::proto::ListContactPeopleResponse& response) = 0;
    virtual void OnListPublicCertificatesRequest(
        const nearby::sharing::proto::ListPublicCertificatesRequest&
            request) = 0;
    virtual void OnListPublicCertificatesResponse(
        const nearby::sharing::proto::ListPublicCertificatesResponse&
            response) = 0;
  };

  virtual ~SharingRpcNotifier() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  SharingRpcNotifier() = default;
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_SHARING_RPC_NOTIFIER_H_
