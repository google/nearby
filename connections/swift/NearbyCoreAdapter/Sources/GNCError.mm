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

#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCError.h"

#import <Foundation/Foundation.h>

#include "connections/status.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCError+Internal.h"

extern NSErrorDomain const GNCErrorDomain = @"com.google.nearby.connections.error";

namespace nearby {
namespace connections {

NSError *NSErrorFromCppStatus(Status status) {
  switch (status.value) {
    case Status::kSuccess:
      return nil;
    case Status::kError:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorUnknown userInfo:nil];
    case Status::kOutOfOrderApiCall:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorOutOfOrderApiCall userInfo:nil];
    case Status::kAlreadyHaveActiveStrategy:
      return [NSError errorWithDomain:GNCErrorDomain
                                 code:GNCErrorAlreadyHaveActiveStrategy
                             userInfo:nil];
    case Status::kAlreadyAdvertising:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorAlreadyAdvertising userInfo:nil];
    case Status::kAlreadyDiscovering:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorAlreadyDiscovering userInfo:nil];
    case Status::kAlreadyListening:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorAlreadyListening userInfo:nil];
    case Status::kEndpointIoError:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorEndpointIoError userInfo:nil];
    case Status::kEndpointUnknown:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorEndpointUnknown userInfo:nil];
    case Status::kConnectionRejected:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorConnectionRejected userInfo:nil];
    case Status::kAlreadyConnectedToEndpoint:
      return [NSError errorWithDomain:GNCErrorDomain
                                 code:GNCErrorAlreadyConnectedToEndpoint
                             userInfo:nil];
    case Status::kNotConnectedToEndpoint:
      return [NSError errorWithDomain:GNCErrorDomain
                                 code:GNCErrorNotConnectedToEndpoint
                             userInfo:nil];
    case Status::kBluetoothError:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorBluetoothError userInfo:nil];
    case Status::kBleError:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorBleError userInfo:nil];
    case Status::kWifiLanError:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorWifiLanError userInfo:nil];
    case Status::kPayloadUnknown:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorPayloadUnknown userInfo:nil];
    case Status::kReset:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorReset userInfo:nil];
    case Status::kTimeout:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorTimeout userInfo:nil];
    case Status::kUnknown:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorUnknown userInfo:nil];
    case Status::kNextValue:
      return [NSError errorWithDomain:GNCErrorDomain code:GNCErrorUnknown userInfo:nil];
  }
}

}  // namespace connections
}  // namespace nearby
