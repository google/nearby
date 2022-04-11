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

#import "internal/platform/implementation/ios/Mediums/WifiLan/GNCMBonjourService.h"

#import "internal/platform/implementation/ios/Mediums/GNCMConnection.h"
#import "internal/platform/implementation/ios/Mediums/WifiLan/GNCMBonjourConnection.h"
#import "internal/platform/implementation/ios/Mediums/WifiLan/GNCMBonjourUtils.h"
#import "GoogleToolboxForMac/GTMLogger.h"

@interface GNCMBonjourService () <NSNetServiceDelegate>

@property(nonatomic, copy) NSString *serviceName;
@property(nonatomic, copy) NSString *serviceType;
@property(nonatomic, copy) GNCMConnectionHandler endpointConnectedHandler;

@property(nonatomic) NSNetService *netService;

@end

@implementation GNCMBonjourService

- (instancetype)initWithServiceName:(NSString *)serviceName
                        serviceType:(NSString *)serviceType
                               port:(NSInteger)port
                      TXTRecordData:(NSDictionary<NSString *, NSData *> *)TXTRecordData
           endpointConnectedHandler:(GNCMConnectionHandler)handler {
  self = [super init];
  if (self) {
    _serviceName = [serviceName copy];
    _serviceType = [serviceType copy];
    _endpointConnectedHandler = handler;

    _netService = [[NSNetService alloc] initWithDomain:GNCMBonjourDomain
                                                  type:_serviceType
                                                  name:_serviceName
                                                  port:port];
    [_netService setTXTRecordData:[NSNetService dataFromTXTRecordDictionary:TXTRecordData]];

    _netService.delegate = self;
    [_netService scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
    [_netService publishWithOptions:NSNetServiceListenForConnections];
  }
  return self;
}

- (void)dealloc {
  [_netService stop];
}

#pragma mark NSNetServiceDelegate

- (void)netServiceDidPublish:(NSNetService *)service {
  GTMLoggerDebug(@"Did publish service: %@", service);
}

- (void)netService:(NSNetService *)service
     didNotPublish:(NSDictionary<NSString *, NSNumber *> *)errorDict {
  GTMLoggerDebug(@"Error publishing: service: %@, errorDic: %@", service, errorDict);
}

- (void)netServiceDidStop:(NSNetService *)service {
  GTMLoggerDebug(@"Stopped publishing service: %@", service);
}

- (void)netService:(NSNetService *)service
    didAcceptConnectionWithInputStream:(NSInputStream *)inputStream
                          outputStream:(NSOutputStream *)outputStream {
  GTMLoggerDebug(@"Accepted connection, service: %@", service);
  GNCMBonjourConnection *connection =
      [[GNCMBonjourConnection alloc] initWithInputStream:inputStream
                                            outputStream:outputStream
                                                   queue:nil];
  connection.connectionHandlers = _endpointConnectedHandler(connection);
}

@end
