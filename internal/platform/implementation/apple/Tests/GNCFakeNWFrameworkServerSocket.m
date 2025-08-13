// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/Tests/GNCFakeNWFrameworkServerSocket.h"

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Tests/GNCFakeNWFrameworkSocket.h"

@implementation GNCFakeNWFrameworkServerSocket {
  GNCIPv4Address *_ipAddress;
}

- (instancetype)initWithPort:(NSInteger)port {
  self = [super initWithPort:port];
  if (self) {
    _ipAddress = [[GNCIPv4Address alloc] initWithByte1:192 byte2:168 byte3:1 byte4:1];
  }
  return self;
}

- (GNCIPv4Address *)ipAddress {
  return _ipAddress;
}

- (nullable GNCNWFrameworkSocket *)acceptWithError:(NSError **)error {
  if (self.acceptError) {
    if (error) {
      *error = self.acceptError;
    }
    return nil;
  }
  if (self.socketToReturnOnAccept) {
    return self.socketToReturnOnAccept;
  }
  nw_connection_t connection = (nw_connection_t)@"mock connection";
  return [[GNCFakeNWFrameworkSocket alloc] initWithConnection:connection];
}

- (void)close {
  self.isClosed = YES;
}

@end
