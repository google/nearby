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

#import "internal/platform/implementation/ios/Source/Mediums/WifiLan/GNCMBonjourBrowser.h"

#import "internal/platform/implementation/ios/Source/Mediums/GNCMConnection.h"
#import "internal/platform/implementation/ios/Source/Mediums/WifiLan/GNCMBonjourConnection.h"
#import "internal/platform/implementation/ios/Source/Mediums/WifiLan/GNCMBonjourUtils.h"
#import "GoogleToolboxForMac/GTMLogger.h"

typedef NSString *GNCEndpointId;

@interface GNCMNetServiceInfo : NSObject
@property(nonatomic) NSNetService *service;
@property(nonatomic, copy) GNCMEndpointLostHandler endpointLostHandler;
@property(nonatomic, copy, nullable) GNCMConnectionHandler connectionHandler;
@end

@implementation GNCMNetServiceInfo

+ (instancetype)infoWithService:(NSNetService *)service {
  GNCMNetServiceInfo *info = [[GNCMNetServiceInfo alloc] init];
  info.service = service;
  return info;
}

- (BOOL)isEqual:(GNCMNetServiceInfo *)object {
  return [_service isEqual:object.service];
}

- (NSUInteger)hash {
  return [_service hash];
}

@end

@interface GNCMBonjourBrowser () <NSNetServiceBrowserDelegate, NSNetServiceDelegate>

@property(nonatomic, copy) GNCMEndpointFoundHandler endpointFoundHandler;

@property(nonatomic) NSNetServiceBrowser *netBrowser;
@property(nonatomic) NSMutableDictionary<GNCEndpointId, GNCMNetServiceInfo *> *endpoints;

@end

@implementation GNCMBonjourBrowser

- (instancetype)initWithServiceType:(NSString *)serviceType
               endpointFoundHandler:(GNCMEndpointFoundHandler)handler {
  self = [super init];
  if (self) {
    _endpointFoundHandler = handler;

    _netBrowser = [[NSNetServiceBrowser alloc] init];
    _netBrowser.delegate = self;
    [_netBrowser scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
    [_netBrowser searchForServicesOfType:serviceType inDomain:GNCMBonjourDomain];
    _endpoints = [NSMutableDictionary dictionary];
  }
  return self;
}

#pragma mark NSNetServiceBrowserDelegate

- (void)netServiceBrowserWillSearch:(NSNetServiceBrowser *)browser {
  GTMLoggerDebug(@"Browsing");
}

- (void)netServiceBrowserDidStopSearch:(NSNetServiceBrowser *)browser {
  GTMLoggerDebug(@"Stop browsing");
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
             didNotSearch:(NSDictionary<NSString *, NSNumber *> *)errorDict {
  GTMLoggerDebug(@"Not browsing with errorDict: %@", errorDict);
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
           didFindService:(NSNetService *)service
               moreComing:(BOOL)moreComing {
  GTMLoggerDebug(@"Found service: %@", service);

  GNCMNetServiceInfo *info = [GNCMNetServiceInfo infoWithService:service];

  // Just to be safe, check if the service is already known and deal with it accordingly.
  NSArray<GNCEndpointId> *endpointIds = [_endpoints allKeysForObject:info];
  if (endpointIds.count > 0) return;

  // Add the newly discovered service to the list of services.
  GNCEndpointId endpointId = [[NSUUID UUID] UUIDString];
  _endpoints[endpointId] = info;

  service.delegate = self;
  [service resolveWithTimeout:0];
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
         didRemoveService:(NSNetService *)service
               moreComing:(BOOL)moreComing {
  GTMLoggerDebug(@"Lost service: %@", service);
  NSArray<GNCEndpointId> *endpointIds =
      [_endpoints allKeysForObject:[GNCMNetServiceInfo infoWithService:service]];
  NSAssert(endpointIds.count <= 1, @"Unexpected duplicate service");
  if (endpointIds.count > 0) {
    GNCEndpointId endpointId = endpointIds[0];
    GNCMNetServiceInfo *info = _endpoints[endpointId];
    [_endpoints removeObjectForKey:endpointId];
    // Tail call to preserve reentrancy.
    if (info.endpointLostHandler) {
      info.endpointLostHandler();
    }
  }
}

#pragma mark NSNetServiceDelegate

- (void)netServiceDidResolveAddress:(NSNetService *)service {
  GTMLoggerDebug(@"Resolved service: %@ addresses: %@", service, service.addresses);

  GNCMNetServiceInfo *info = [GNCMNetServiceInfo infoWithService:service];

  NSArray<GNCEndpointId> *endpointIds = [_endpoints allKeysForObject:info];
  if (endpointIds.count > 0) {
    // Get TXTRecord data.
    NSData *data = [service TXTRecordData];
    NSDictionary<NSString *, NSData *> *TXTRecordData =
        [NSNetService dictionaryFromTXTRecordData:data];

    // The endpointLostHandler is returned from the endpointFoundHandler. The main benefit of this
    // is that it allows rejection from the remote endpoint to be received by the local endpoint
    // before the local endpoint has accepted or rejected.
    info.endpointLostHandler = _endpointFoundHandler(
        endpointIds[0], service.type, service.name, TXTRecordData,
        ^(GNCMConnectionHandler connectionHandler) {
          // A connection is requested, so resolve the service to get the I/O streams.
          info.connectionHandler = connectionHandler;
          if (info.connectionHandler) {
            dispatch_sync(dispatch_get_main_queue(), ^{
              NSInputStream *inputStream;
              NSOutputStream *outputStream;
              [info.service scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
              [info.service getInputStream:&inputStream outputStream:&outputStream];
              GNCMBonjourConnection *connection =
                  [[GNCMBonjourConnection alloc] initWithInputStream:inputStream
                                                        outputStream:outputStream
                                                               queue:nil];
              connection.connectionHandlers = info.connectionHandler(connection);
            });
          }
        });
    _endpoints[endpointIds[0]] = info;
  }
}

- (void)netService:(NSNetService *)service
     didNotResolve:(NSDictionary<NSString *, NSNumber *> *)errorDict {
  GTMLoggerDebug(@"Did not resolve service: %@", service);
  NSArray<GNCEndpointId> *endpointIds =
      [_endpoints allKeysForObject:[GNCMNetServiceInfo infoWithService:service]];
  if (endpointIds.count > 0) {
    _endpoints[endpointIds[0]].connectionHandler = nil;
  }
}

@end
