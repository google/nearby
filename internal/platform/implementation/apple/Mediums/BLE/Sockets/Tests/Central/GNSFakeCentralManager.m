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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Central/GNSFakeCentralManager.h"

@implementation GNSFakeCentralManager

@synthesize socketServiceUUID = _socketServiceUUID;
@synthesize cbManagerState = _cbManagerState;
@synthesize connectPeripheralCalled = _connectPeripheralCalled;
@synthesize cancelConnectionCalled = _cancelConnectionCalled;
@synthesize didDisconnectCalled = _didDisconnectCalled;

- (instancetype)init {
  return self;
}

- (void)connectPeripheralForPeer:(id)peer options:(nullable id)options {
  self.connectPeripheralCalled = YES;
}

- (void)cancelPeripheralConnectionForPeer:(id)peer {
  self.cancelConnectionCalled = YES;
}

- (void)centralPeerManagerDidDisconnect:(id)peer {
  self.didDisconnectCalled = YES;
}

@end
