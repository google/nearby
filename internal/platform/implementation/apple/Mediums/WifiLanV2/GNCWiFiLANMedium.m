// Copyright 2023 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/WifiLanV2/GNCWiFiLANMedium.h"

#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/WifiLanV2/GNCWiFiLANServerSocket+Internal.h"
#import "internal/platform/implementation/apple/Mediums/WifiLanV2/GNCWiFiLANServerSocket.h"
#import "GoogleToolboxForMac/GTMLogger.h"

@implementation GNCWiFiLANMedium {
  // Usage of .count should be avoided. Zombie keys won't show up in -keyEnumerator, but WILL be
  // included in .count until the next time that the internal hashtable is resized.
  NSMapTable<NSNumber *, GNCWiFiLANServerSocket *> *_serverSockets;
}

- (instancetype)init {
  if (self = [super init]) {
    _serverSockets = [NSMapTable strongToWeakObjectsMapTable];
  }
  return self;
}

- (GNCWiFiLANServerSocket *)listenForServiceOnPort:(NSInteger)port error:(NSError **)error {
  GNCWiFiLANServerSocket *serverSocket = [[GNCWiFiLANServerSocket alloc] initWithPort:port];
  BOOL success = [serverSocket startListeningWithError:error];
  if (success) {
    [_serverSockets setObject:serverSocket forKey:@(serverSocket.port)];
    return serverSocket;
  }
  return nil;
}

- (void)startAdvertisingPort:(NSInteger)port
                 serviceName:(NSString *)serviceName
                 serviceType:(NSString *)serviceType
                  txtRecords:(NSDictionary<NSString *, NSString *> *)txtRecords {
  GNCWiFiLANServerSocket *serverSocket = [_serverSockets objectForKey:@(port)];
  [serverSocket startAdvertisingServiceName:serviceName
                                serviceType:serviceType
                                 txtRecords:txtRecords];
}

- (void)stopAdvertisingPort:(NSInteger)port {
  GNCWiFiLANServerSocket *serverSocket = [_serverSockets objectForKey:@(port)];
  [serverSocket stopAdvertising];
}

@end
