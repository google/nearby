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

#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCCoreAdapter.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "connections/core.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCAdvertisingOptions+CppConversions.h"
#import "connections/swift/NearbyCoreAdapter/Sources/GNCConnectionOptions+CppConversions.h"
#import "connections/swift/NearbyCoreAdapter/Sources/GNCDiscoveryOptions+CppConversions.h"
#import "connections/swift/NearbyCoreAdapter/Sources/GNCError+Internal.h"
#import "connections/swift/NearbyCoreAdapter/Sources/GNCPayload+CppConversions.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCAdvertisingOptions.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCConnectionDelegate.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCConnectionOptions.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCDiscoveryDelegate.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCDiscoveryOptions.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCError.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCPayload.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCPayloadDelegate.h"

using ::nearby::ByteArray;
using ::nearby::connections::AdvertisingOptions;
using ::nearby::connections::ConnectionListener;
using ::nearby::connections::ConnectionOptions;
using ::nearby::connections::ConnectionRequestInfo;
using ::nearby::connections::ConnectionResponseInfo;
using ::nearby::connections::Core;
using ::nearby::connections::DiscoveryListener;
using ::nearby::connections::DiscoveryOptions;
using ::nearby::connections::Payload;
using ::nearby::connections::PayloadListener;
using ::nearby::connections::PayloadProgressInfo;
using ResultListener = ::nearby::connections::ResultCallback;
using ::nearby::connections::ServiceControllerRouter;
using ::nearby::connections::Status;

GNCStatus GNCStatusFromCppStatus(Status status) {
  switch (status.value) {
    case Status::kSuccess:
      return GNCStatusSuccess;
    case Status::kError:
      return GNCStatusError;
    case Status::kOutOfOrderApiCall:
      return GNCStatusOutOfOrderApiCall;
    case Status::kAlreadyHaveActiveStrategy:
      return GNCStatusAlreadyHaveActiveStrategy;
    case Status::kAlreadyAdvertising:
      return GNCStatusAlreadyAdvertising;
    case Status::kAlreadyDiscovering:
      return GNCStatusAlreadyDiscovering;
    case Status::kEndpointIoError:
      return GNCStatusEndpointIoError;
    case Status::kEndpointUnknown:
      return GNCStatusEndpointUnknown;
    case Status::kConnectionRejected:
      return GNCStatusConnectionRejected;
    case Status::kAlreadyConnectedToEndpoint:
      return GNCStatusAlreadyConnectedToEndpoint;
    case Status::kNotConnectedToEndpoint:
      return GNCStatusNotConnectedToEndpoint;
    case Status::kBluetoothError:
      return GNCStatusBluetoothError;
    case Status::kBleError:
      return GNCStatusBleError;
    case Status::kWifiLanError:
      return GNCStatusWifiLanError;
    case Status::kPayloadUnknown:
      return GNCStatusPayloadUnknown;
  }
}

@interface GNCCoreAdapter () {
  std::unique_ptr<Core> _core;
  std::unique_ptr<ServiceControllerRouter> _serviceControllerRouter;
}

@end

@implementation GNCCoreAdapter

+ (GNCCoreAdapter *)shared {
  static dispatch_once_t onceToken;
  static GNCCoreAdapter *shared = nil;
  dispatch_once(&onceToken, ^{
    shared = [[GNCCoreAdapter alloc] init];
  });
  return shared;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _serviceControllerRouter = std::make_unique<ServiceControllerRouter>();
    _core = std::make_unique<Core>(_serviceControllerRouter.get());
  }
  return self;
}

- (void)dealloc {
  // Make sure Core is deleted before ServiceControllerRouter.
  _core.reset();
  _serviceControllerRouter.reset();
}

- (void)startAdvertisingAsService:(NSString *)serviceID
                     endpointInfo:(NSData *)endpointInfo
                          options:(GNCAdvertisingOptions *)advertisingOptions
                         delegate:(id<GNCConnectionDelegate>)delegate
            withCompletionHandler:(void (^)(NSError *error))completionHandler {
  std::string service_id = [serviceID cStringUsingEncoding:[NSString defaultCStringEncoding]];

  AdvertisingOptions advertising_options = [advertisingOptions toCpp];

  ConnectionListener listener;
  listener.initiated_cb = ^(const std::string &endpoint_id, const ConnectionResponseInfo &info) {
    NSString *endpointID = @(endpoint_id.c_str());
    NSData *endpointInfo = [NSData dataWithBytes:info.remote_endpoint_info.data()
                                          length:info.remote_endpoint_info.size()];
    NSString *authenticationToken = @(info.authentication_token.c_str());
    [delegate connectedToEndpoint:endpointID
                 withEndpointInfo:endpointInfo
              authenticationToken:authenticationToken];
  };
  listener.accepted_cb = ^(const std::string &endpoint_id) {
    NSString *endpointID = @(endpoint_id.c_str());
    [delegate acceptedConnectionToEndpoint:endpointID];
  };
  listener.rejected_cb = ^(const std::string &endpoint_id, Status status) {
    NSString *endpointID = @(endpoint_id.c_str());
    [delegate rejectedConnectionToEndpoint:endpointID withStatus:GNCStatusFromCppStatus(status)];
  };
  listener.disconnected_cb = ^(const std::string &endpoint_id) {
    NSString *endpointID = @(endpoint_id.c_str());
    [delegate disconnectedFromEndpoint:endpointID];
  };

  ConnectionRequestInfo connection_request_info;
  connection_request_info.endpoint_info =
      ByteArray((const char *)endpointInfo.bytes, endpointInfo.length);
  connection_request_info.listener = std::move(listener);

  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->StartAdvertising(service_id, advertising_options, connection_request_info, result);
}

- (void)stopAdvertisingWithCompletionHandler:(void (^)(NSError *error))completionHandler {
  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->StopAdvertising(result);
}

- (void)startDiscoveryAsService:(NSString *)serviceID
                        options:(GNCDiscoveryOptions *)discoveryOptions
                       delegate:(id<GNCDiscoveryDelegate>)delegate
          withCompletionHandler:(void (^)(NSError *error))completionHandler {
  std::string service_id = [serviceID cStringUsingEncoding:[NSString defaultCStringEncoding]];

  DiscoveryOptions discovery_options = [discoveryOptions toCpp];

  DiscoveryListener listener;
  listener.endpoint_found_cb = ^(const std::string &endpoint_id, const ByteArray &endpoint_info,
                                 const std::string &service_id) {
    NSString *endpointID = @(endpoint_id.c_str());
    NSData *info = [NSData dataWithBytes:endpoint_info.data() length:endpoint_info.size()];
    [delegate foundEndpoint:endpointID withEndpointInfo:info];
  };
  listener.endpoint_lost_cb = ^(const std::string &endpoint_id) {
    NSString *endpointID = @(endpoint_id.c_str());
    [delegate lostEndpoint:endpointID];
  };

  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->StartDiscovery(service_id, discovery_options, std::move(listener), result);
}

- (void)stopDiscoveryWithCompletionHandler:(void (^)(NSError *error))completionHandler {
  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->StopDiscovery(result);
}

- (void)requestConnectionToEndpoint:(NSString *)endpointID
                       endpointInfo:(NSData *)endpointInfo
                            options:(GNCConnectionOptions *)connectionOptions
                           delegate:(id<GNCConnectionDelegate>)delegate
              withCompletionHandler:(void (^)(NSError *error))completionHandler {
  std::string endpoint_id = [endpointID cStringUsingEncoding:[NSString defaultCStringEncoding]];

  ConnectionListener listener;
  listener.initiated_cb = ^(const std::string &endpoint_id, const ConnectionResponseInfo &info) {
    NSString *endpointID = @(endpoint_id.c_str());
    NSData *endpointInfo = [NSData dataWithBytes:info.remote_endpoint_info.data()
                                          length:info.remote_endpoint_info.size()];
    NSString *authenticationToken = @(info.authentication_token.c_str());
    [delegate connectedToEndpoint:endpointID
                 withEndpointInfo:endpointInfo
              authenticationToken:authenticationToken];
  };
  listener.accepted_cb = ^(const std::string &endpoint_id) {
    NSString *endpointID = @(endpoint_id.c_str());
    [delegate acceptedConnectionToEndpoint:endpointID];
  };
  listener.rejected_cb = ^(const std::string &endpoint_id, Status status) {
    NSString *endpointID = @(endpoint_id.c_str());
    [delegate rejectedConnectionToEndpoint:endpointID withStatus:GNCStatusFromCppStatus(status)];
  };
  listener.disconnected_cb = ^(const std::string &endpoint_id) {
    NSString *endpointID = @(endpoint_id.c_str());
    [delegate disconnectedFromEndpoint:endpointID];
  };

  ConnectionRequestInfo connection_request_info;
  connection_request_info.endpoint_info =
      ByteArray((const char *)endpointInfo.bytes, endpointInfo.length);
  connection_request_info.listener = std::move(listener);

  ConnectionOptions connection_options = [connectionOptions toCpp];

  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->RequestConnection(endpoint_id, connection_request_info, connection_options, result);
}

- (void)acceptConnectionRequestFromEndpoint:(NSString *)endpointID
                                   delegate:(id<GNCPayloadDelegate>)delegate
                      withCompletionHandler:(void (^)(NSError *error))completionHandler {
  std::string endpoint_id = [endpointID cStringUsingEncoding:[NSString defaultCStringEncoding]];

  PayloadListener listener;
  listener.payload_cb = ^(const std::string &endpoint_id, Payload payload) {
    NSString *endpointID = @(endpoint_id.c_str());
    GNCPayload *gncPayload = [GNCPayload fromCpp:std::move(payload)];
    [delegate receivedPayload:gncPayload fromEndpoint:endpointID];
  };
  listener.payload_progress_cb =
      ^(const std::string &endpoint_id, const PayloadProgressInfo &info) {
        NSString *endpointID = @(endpoint_id.c_str());
        GNCPayloadStatus status;
        switch (info.status) {
          case PayloadProgressInfo::Status::kSuccess:
            status = GNCPayloadStatusSuccess;
            break;
          case PayloadProgressInfo::Status::kFailure:
            status = GNCPayloadStatusFailure;
            break;
          case PayloadProgressInfo::Status::kInProgress:
            status = GNCPayloadStatusInProgress;
            break;
          case PayloadProgressInfo::Status::kCanceled:
            status = GNCPayloadStatusCanceled;
            break;
        }
        [delegate receivedProgressUpdateForPayload:info.payload_id
                                        withStatus:status
                                      fromEndpoint:endpointID
                                   bytesTransfered:info.bytes_transferred
                                        totalBytes:info.total_bytes];
      };

  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->AcceptConnection(endpoint_id, std::move(listener), result);
}

- (void)rejectConnectionRequestFromEndpoint:(NSString *)endpointID
                      withCompletionHandler:(void (^)(NSError *error))completionHandler {
  std::string endpoint_id = [endpointID cStringUsingEncoding:[NSString defaultCStringEncoding]];

  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->RejectConnection(endpoint_id, result);
}

- (void)sendPayload:(GNCPayload *)payload
              toEndpoints:(NSArray<NSString *> *)endpointIDs
    withCompletionHandler:(void (^)(NSError *error))completionHandler {
  std::vector<std::string> endpoint_ids;
  endpoint_ids.reserve([endpointIDs count]);
  for (NSString *endpointID in endpointIDs) {
    std::string endpoint_id = [endpointID cStringUsingEncoding:[NSString defaultCStringEncoding]];
    endpoint_ids.push_back(endpoint_id);
  }

  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->SendPayload(endpoint_ids, [payload toCpp], result);
}

- (void)cancelPayload:(int64_t)payloadID
    withCompletionHandler:(void (^)(NSError *error))completionHandler {
  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->CancelPayload(payloadID, result);
}

- (void)disconnectFromEndpoint:(NSString *)endpointID
         withCompletionHandler:(void (^)(NSError *error))completionHandler {
  std::string endpoint_id = [endpointID cStringUsingEncoding:[NSString defaultCStringEncoding]];

  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->DisconnectFromEndpoint(endpoint_id, result);
}

- (void)stopAllEndpointsWithCompletionHandler:(void (^)(NSError *error))completionHandler {
  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };
  _core->StopAllEndpoints(result);
}

- (void)initiateBandwidthUpgrade:(NSString *)endpointID
           withCompletionHandler:(void (^)(NSError *error))completionHandler {
  std::string endpoint_id = [endpointID cStringUsingEncoding:[NSString defaultCStringEncoding]];

  ResultListener result;
  result.result_cb = ^(Status status) {
    NSError *err = NSErrorFromCppStatus(status);
    if (completionHandler) {
      completionHandler(err);
    }
  };

  _core->InitiateBandwidthUpgrade(endpoint_id, result);
}

- (NSString *)localEndpointID {
  std::string endpoint_id = _core->GetLocalEndpointId();
  return @(endpoint_id.c_str());
}

@end
