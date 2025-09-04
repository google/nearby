// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkSocket.h"

@implementation GNCFakeNWFramework

- (instancetype)init {
  self = [super init];
  if (self) {
    _sockets = [NSMutableArray array];
    _serverSockets = [NSMutableArray array];
  }
  return self;
}

- (void)startAdvertisingPort:(NSInteger)port
                 serviceName:(NSString*)serviceName
                 serviceType:(NSString*)serviceType
                  txtRecords:(NSDictionary<NSString*, NSString*>*)txtRecords {
  self.startedAdvertisingPort = @(port);
  self.startedAdvertisingServiceName = serviceName;
  self.startedAdvertisingServiceType = serviceType;
}

- (void)stopAdvertisingPort:(NSInteger)port {
  self.stoppedAdvertisingPort = @(port);
}

- (BOOL)startDiscoveryForServiceType:(NSString*)serviceType
                 serviceFoundHandler:(nonnull ServiceUpdateHandler)serviceFoundHandler
                  serviceLostHandler:(nonnull ServiceUpdateHandler)serviceLostHandler
                   includePeerToPeer:(BOOL)includePeerToPeer
                               error:(NSError**)error {
  self.startedDiscoveryServiceType = serviceType;
  self.startedDiscoveryIncludePeerToPeer = includePeerToPeer;
  return YES;
}

- (void)stopDiscoveryForServiceType:(NSString*)serviceType {
  self.stoppedDiscoveryServiceType = serviceType;
}

- (GNCNWFrameworkSocket*)connectToServiceName:(NSString*)serviceName
                                  serviceType:(NSString*)serviceType
                                        error:(NSError**)error {
  self.connectedToServiceName = serviceName;
  self.connectedToServiceType = serviceType;
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* socket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  [self.sockets addObject:socket];
  return socket;
}

- (GNCNWFrameworkSocket*)connectToHost:(GNCIPv4Address*)host
                                  port:(NSInteger)port
                     includePeerToPeer:(BOOL)includePeerToPeer
                                 error:(NSError**)error {
  self.connectedToHost = host;
  self.connectedToPort = port;
  self.connectedToIncludePeerToPeer = includePeerToPeer;
  nw_connection_t connection = (nw_connection_t) @"mock connection";
  GNCFakeNWFrameworkSocket* socket =
      [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
  [self.sockets addObject:socket];
  return socket;
}

- (GNCNWFrameworkServerSocket*)listenForServiceOnPort:(NSInteger)port
                                    includePeerToPeer:(BOOL)includePeerToPeer
                                                error:(NSError**)error {
  self.listenedForServiceOnPort = port;
  self.listenedForServiceIncludePeerToPeer = includePeerToPeer;
  GNCFakeNWFrameworkServerSocket* serverSocket =
      [[GNCFakeNWFrameworkServerSocket alloc] initWithPort:port];
  [self.serverSockets addObject:serverSocket];
  return serverSocket;
}

@end
