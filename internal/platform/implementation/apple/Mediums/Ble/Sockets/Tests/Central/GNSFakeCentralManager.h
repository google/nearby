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

#import <CoreBluetooth/CoreBluetooth.h>

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralManager.h"

NS_ASSUME_NONNULL_BEGIN

@class GNSCentralPeerManager;
@class GNSFakePeripheral;

// A fake implementation of GNSCentralManager for testing.
@interface GNSFakeCentralManager : GNSCentralManager <CBCentralManagerDelegate>

@property(nonatomic) CBUUID *socketServiceUUID;
@property(nonatomic, assign) CBManagerState testCbManagerState;
@property(nonatomic, nullable) CBPeripheral *connectedPeripheral;
@property(nonatomic, nullable) GNSCentralPeerManager *connectingPeer;
@property(nonatomic, strong) GNSCentralPeerManager *centralPeerManager;
@property(nonatomic) BOOL failConnection;
@property(nonatomic, nullable) NSArray<CBPeripheral *> *peripheralsToRetrieve;

- (void)connectPeripheralForPeer:(GNSCentralPeerManager *)peer
                         options:(nullable NSDictionary<NSString *, id> *)options;
- (void)cancelPeripheralConnectionForPeer:(GNSCentralPeerManager *)peer;
- (void)centralPeerManagerDidDisconnect:(GNSCentralPeerManager *)peer;

@end

NS_ASSUME_NONNULL_END
