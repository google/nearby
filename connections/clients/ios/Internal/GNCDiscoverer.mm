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

#import "connections/clients/ios/Public/NearbyConnections/GNCDiscoverer.h"

#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#import "connections/clients/ios/Internal/GNCCore.h"
#import "connections/clients/ios/Internal/GNCCoreConnection.h"
#import "connections/clients/ios/Internal/GNCPayloadListener.h"
#import "connections/clients/ios/Internal/GNCUtils.h"
#import "connections/clients/ios/Public/NearbyConnections/GNCConnection.h"
#include "connections/connection_options.h"
#include "connections/core.h"
#include "connections/discovery_options.h"
#include "connections/listeners.h"
#include "connections/status.h"
#include "internal/platform/byte_array.h"
#import "internal/platform/implementation/apple/utils.h"
#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

using ::nearby::CppStringFromObjCString;
using ::nearby::connections::DiscoveryListener;
using ::nearby::connections::DiscoveryOptions;
using ::nearby::connections::GNCStrategyToStrategy;
using ResultListener = ::nearby::connections::ResultCallback;
using ::nearby::connections::Status;

/** This is a GNCDiscovererConnectionInfo that provides storage for its properties. */
@interface GNCDiscovererConnectionInfo : NSObject <GNCDiscovererConnectionInfo>
@property(nonatomic, copy) NSString *authToken;

/** Creates a GNCDiscovererConnectionInfo object. */
+ (instancetype)infoWithAuthToken:(NSString *)authToken;
@end

@implementation GNCDiscovererConnectionInfo

+ (instancetype)infoWithAuthToken:(NSString *)authToken {
  GNCDiscovererConnectionInfo *info = [[GNCDiscovererConnectionInfo alloc] init];
  info.authToken = authToken;
  return info;
}

@end

/** This is a GNCDiscoveredEndpointInfo that provides storage for its properties. */
@interface GNCDiscoveredEndpointInfo : NSObject <GNCDiscoveredEndpointInfo>
@property(nonatomic, copy) NSString *endpointName;
@property(nonatomic, copy) NSData *endpointInfo;
@end

@implementation GNCDiscoveredEndpointInfo

@synthesize requestConnection = _requestConnection;

+ (instancetype)infoWithName:(NSString *)endpointName
                endpointInfo:(NSData *)endpointInfo
           requestConnection:(GNCConnectionRequester)requestConnection {
  GNCDiscoveredEndpointInfo *info = [[GNCDiscoveredEndpointInfo alloc] init];
  info.endpointName = endpointName;
  info.endpointInfo = endpointInfo;
  info->_requestConnection = requestConnection;
  return info;
}

@end

/** Information retained by the discoverer about each discovered endpoint. */
@interface GNCDiscovererEndpointInfo : NSObject

/** Handles lostHandler once |onEndpointLost| has been callback. */
@property(nonatomic) GNCEndpointLostHandler lostHandler;

/** The connInitHandler is stored after requestConnection. */
@property(nonatomic, nullable) GNCDiscovererConnectionInitializationHandler connInitHandler;

/** The connFailureHandler is stored after requestConnection. */
@property(nonatomic, nullable) GNCConnectionFailureHandler connFailureHandler;

/** Client responses Accept or Reject. */
@property(nonatomic) GNCConnectionResponse clientResponse;

/** Whether the client response has been received. */
@property(nonatomic) BOOL clientResponseReceived;

/**
 * The connectionhandler returned by connInitHandler. Stored here if the connection is accepted.
 */
@property(nonatomic, nullable) GNCConnectionHandler connectionHandler;

/** @c GNCCoreConnection is created and stored if connection is accepted. */
@property(nonatomic, weak) GNCCoreConnection *connection;

/** @c GNCConnectionHandlers object is returned by connectionHandler and stored here. */
@property(nonatomic) GNCConnectionHandlers *connectionHandlers;
@end

@implementation GNCDiscovererEndpointInfo
@end

/** GNCDiscoverer members. */
@interface GNCDiscoverer ()
@property(nonatomic) GNCCore *core;
@property(nonatomic) GNCEndpointFoundHandler endpointFoundHandler;
@property(nonatomic, assign) Status status;
@property(nonatomic) NSMapTable<GNCEndpointId, GNCDiscovererEndpointInfo *> *endpoints;
@end

/** C++ classes passed to the core library by GNCDiscoverer. */

namespace nearby {
namespace connections {

/** This class contains the discoverer callbacks related to a connection. */
class GNCDiscovererConnectionListener {
 public:
  GNCDiscovererConnectionListener(GNCCore *core,
                                  NSMapTable<GNCEndpointId, GNCDiscovererEndpointInfo *> *endpoints)
      : core_(core), endpoints_(endpoints) {}

  void OnInitiated(const std::string &endpoint_id, const ConnectionResponseInfo &info) {
    NSString *endpointId = ObjCStringFromCppString(endpoint_id);
    GNCDiscovererEndpointInfo *endpointInfo = [endpoints_ objectForKey:endpointId];
    if (!endpointInfo) {
      return;
    }

    // Call the connection initiation handler. Synchronous because it returns the connection
    // handler.
    NSString *authToken = ObjCStringFromCppString(info.authentication_token);
    GNCCore *core = core_;  // don't capture |this|
    dispatch_sync(dispatch_get_main_queue(), ^{
      endpointInfo.connectionHandler = endpointInfo.connInitHandler(
          [GNCDiscovererConnectionInfo infoWithAuthToken:authToken],
          ^(GNCConnectionResponse response) {
            endpointInfo.clientResponse = response;
            endpointInfo.clientResponseReceived = YES;
            if (response == GNCConnectionResponseAccept) {
              // The connect was accepted by the client.
              if (payload_listener_ == nullptr) {
                payload_listener_ = std::make_unique<GNCPayloadListener>(
                    core,
                    ^{
                      return endpointInfo.connectionHandlers;
                    },
                    ^{
                      return endpointInfo.connection.payloads;
                    });
              }
              core->_core->AcceptConnection(
                  CppStringFromObjCString(endpointId),
                  PayloadListener{
                      .payload_cb =
                          absl::bind_front(&GNCPayloadListener::OnPayload, payload_listener_.get()),
                      .payload_progress_cb = absl::bind_front(
                          &GNCPayloadListener::OnPayloadProgress, payload_listener_.get()),
                  },
                  ResultListener{});
            } else {
              // The connect was rejected by the client.
              core->_core->RejectConnection(CppStringFromObjCString(endpointId), ResultListener{});
            }
          });
    });
  }

  void OnAccepted(const std::string &endpoint_id) {
    NSString *endpointId = ObjCStringFromCppString(endpoint_id);
    GNCDiscovererEndpointInfo *endpointInfo = [endpoints_ objectForKey:endpointId];
    if (!endpointInfo) {
      return;
    }

    // The connection has been accepted by both endpoints, so create the GNCConnection object
    // and pass it to |successHandler| for the client to use.
    // Note: Use a local strong reference to the connection object; don't just assign to
    // |endpointInfo.connection|. Without a strong reference, the connection object can be
    // deallocated before |successHandler| is called in the Release build.
    id<GNCConnection> connection = [GNCCoreConnection
        connectionWithEndpointId:endpointId
                            core:core_
                  deallocHandler:^{
                      // Don't remove the remote endpoint (like GNCAdvertiser does) because that's
                      // done when the endpoint is lost.
                  }];
    endpointInfo.connection = connection;

    // Callback is synchronous because it returns the connection handlers.
    dispatch_sync(dispatch_get_main_queue(), ^{
      endpointInfo.connectionHandlers = endpointInfo.connectionHandler(endpointInfo.connection);
    });
    endpointInfo.clientResponseReceived = NO;  // support reconnection after disconnection
  }

  void OnRejected(const std::string &endpoint_id, Status status) {
    NSString *endpointId = ObjCStringFromCppString(endpoint_id);
    GNCDiscovererEndpointInfo *endpointInfo = [endpoints_ objectForKey:endpointId];
    if (!endpointInfo) {
      return;
    }

    // If either side rejected, call failureHandler with the connection status.
    dispatch_async(dispatch_get_main_queue(), ^{
      endpointInfo.connFailureHandler(GNCConnectionFailureRejected);
    });
    endpointInfo.clientResponseReceived = NO;  // support reconnection after disconnection
  }

  void OnDisconnected(const std::string &endpoint_id) {
    NSString *endpointId = ObjCStringFromCppString(endpoint_id);
    GNCDiscovererEndpointInfo *endpointInfo = [endpoints_ objectForKey:endpointId];
    if (!endpointInfo) {
      return;
    }

    if (endpointInfo.connection) {
      GNCDisconnectedHandler disconnectedHandler =
          endpointInfo.connectionHandlers.disconnectedHandler;
      dispatch_async(dispatch_get_main_queue(), ^{
        if (disconnectedHandler) disconnectedHandler(GNCDisconnectedReasonUnknown);
      });
    }
  }

  void OnBandwidthChanged(const std::string &endpoint_id, Medium medium) {
    // TODO(b/169292092): Implement.
  }

 private:
  GNCCore *core_;
  NSMapTable<GNCEndpointId, GNCDiscovererEndpointInfo *> *endpoints_;
  std::unique_ptr<GNCPayloadListener> payload_listener_;
};

class GNCDiscoveryListener {
 public:
  explicit GNCDiscoveryListener(GNCDiscoverer *discoverer) : discoverer_(discoverer) {}

  void OnEndpointFound(const std::string &endpoint_id, const ByteArray &endpoint_info,
                       const std::string &service_id) {
    GNCDiscoverer *discoverer = discoverer_;  // strongify
    if (!discoverer) {
      return;
    }
    NSString *endpointId = ObjCStringFromCppString(endpoint_id);

    if ([discoverer.endpoints objectForKey:endpointId] != nil) {
      GTMLoggerError(@"Endpoint already discovered: %@", endpointId);
    } else {
      // The GNCDiscoveredEndpointInfo object created here lives as long as the client has strong
      // reference to it. Here's the chain of strong references maintained here:
      // client -> GNCDiscoveredEndpointInfo -> RequestConnection block ->
      //   GNCDiscovererEndpointInfo (stored weakly in the |endpoints| map table)
      GNCDiscovererEndpointInfo *endpointInfo = [[GNCDiscovererEndpointInfo alloc] init];
      [discoverer.endpoints setObject:endpointInfo forKey:endpointId];

      NSString *name = ObjCStringFromCppString(std::string(endpoint_info));
      NSData *info = NSDataFromByteArray(endpoint_info);
      GNCCore *core = discoverer.core;  // don't capture |this| or |discoverer|
      NSMapTable<GNCEndpointId, GNCDiscovererEndpointInfo *> *endpoints = discoverer.endpoints;
      GNCDiscoveredEndpointInfo *discEndpointInfo = [GNCDiscoveredEndpointInfo
               infoWithName:name
               endpointInfo:info
          requestConnection:^(NSData *info,
                              GNCDiscovererConnectionInitializationHandler connInitHandler,
                              GNCConnectionFailureHandler connFailureHandler) {
            endpointInfo.connInitHandler = connInitHandler;
            endpointInfo.connFailureHandler = connFailureHandler;
            if (discoverer_connection_listener_ == nullptr) {
              discoverer_connection_listener_ =
                  std::make_unique<GNCDiscovererConnectionListener>(core, endpoints);
            }

            ConnectionListener listener = {
                .initiated_cb = absl::bind_front(&GNCDiscovererConnectionListener::OnInitiated,
                                                 discoverer_connection_listener_.get()),
                .accepted_cb = absl::bind_front(&GNCDiscovererConnectionListener::OnAccepted,
                                                discoverer_connection_listener_.get()),
                .rejected_cb = absl::bind_front(&GNCDiscovererConnectionListener::OnRejected,
                                                discoverer_connection_listener_.get()),
                .disconnected_cb =
                    absl::bind_front(&GNCDiscovererConnectionListener::OnDisconnected,
                                     discoverer_connection_listener_.get()),
            };

            core->_core->RequestConnection(
                CppStringFromObjCString(endpointId),
                ConnectionRequestInfo{.endpoint_info = ByteArrayFromNSData(info),
                                      .listener = std::move(listener)},
                ConnectionOptions{},
                ResultListener{.result_cb = [connFailureHandler](Status status) {
                  if (!status.Ok()) {
                    dispatch_sync(dispatch_get_main_queue(), ^{
                      connFailureHandler(GNCConnectionFailureUnknown);
                    });
                  }
                }});
          }];

      // Call the client endpoint-found handler.  Tail call for reentrancy.
      dispatch_sync(dispatch_get_main_queue(), ^{
        endpointInfo.lostHandler = discoverer.endpointFoundHandler(endpointId, discEndpointInfo);
      });
    }
  }

  void OnEndpointLost(const std::string &endpoint_id) {
    GNCDiscoverer *discoverer = discoverer_;  // strongify
    if (!discoverer) {
      return;
    }

    NSString *endpointId = ObjCStringFromCppString(endpoint_id);
    GNCDiscovererEndpointInfo *info = [discoverer.endpoints objectForKey:endpointId];
    if (!info) {
      GTMLoggerError(@"Endpoint already lost: %@", endpointId);
    } else {
      dispatch_async(dispatch_get_main_queue(), ^{
        info.lostHandler();
      });
    }
    [discoverer.endpoints removeObjectForKey:endpointId];
  }

  void OnEndpointDistanceChanged_cb(const std::string &endpoint_id, DistanceInfo info) {
    // TODO(b/169292092): Implement.
  }

 private:
  __weak GNCDiscoverer *discoverer_;
  std::unique_ptr<GNCDiscovererConnectionListener> discoverer_connection_listener_;
};

}  // namespace connections
}  // namespace nearby

using ::nearby::connections::GNCDiscoveryListener;

@interface GNCDiscoverer () {
  std::unique_ptr<GNCDiscoveryListener> discoveryListener;
};

@end

@implementation GNCDiscoverer

+ (instancetype)discovererWithServiceId:(NSString *)serviceId
                               strategy:(GNCStrategy)strategy
                   endpointFoundHandler:(GNCEndpointFoundHandler)endpointFoundHandler {
  GNCDiscoverer *discoverer = [[GNCDiscoverer alloc] init];
  discoverer.endpointFoundHandler = endpointFoundHandler;
  discoverer.endpoints = [NSMapTable strongToWeakObjectsMapTable];
  discoverer.core = GNCGetCore();
  discoverer->discoveryListener = std::make_unique<GNCDiscoveryListener>(discoverer);

  DiscoveryListener listener = {
      .endpoint_found_cb = absl::bind_front(&GNCDiscoveryListener::OnEndpointFound,
                                            discoverer->discoveryListener.get()),
      .endpoint_lost_cb = absl::bind_front(&GNCDiscoveryListener::OnEndpointLost,
                                           discoverer->discoveryListener.get()),
  };

  DiscoveryOptions discovery_options;
  discovery_options.strategy = GNCStrategyToStrategy(strategy);

  discoverer.core->_core->StartDiscovery(CppStringFromObjCString(serviceId), discovery_options,
                                         std::move(listener), ResultListener{});

  return discoverer;
}

- (void)dealloc {
  GTMLoggerInfo(@"GNCDiscoverer deallocated");
  _core->_core->StopDiscovery(ResultListener{});
}

@end

NS_ASSUME_NONNULL_END
