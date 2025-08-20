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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Peripheral/GNSFakePeripheralManager.h"

#import <CoreBluetooth/CoreBluetooth.h>

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@implementation GNSFakePeripheralManager

@synthesize mockCBPeripheralManager = _mockCBPeripheralManager;
@synthesize advertisementData = _advertisementData;
@synthesize advertisementInProgressData = _advertisementInProgressData;

- (instancetype)initWithAdvertisedName:(NSString *)advertisedName
                     restoreIdentifier:(nullable NSString *)restoreIdentifier
                                 queue:(nullable dispatch_queue_t)queue {
  return [self initWithAdvertisedName:advertisedName
                    restoreIdentifier:restoreIdentifier
                                queue:queue
                    peripheralManager:OCMClassMock([CBPeripheralManager class])];
}

- (instancetype)initWithAdvertisedName:(NSString *)advertisedName
                     restoreIdentifier:(nullable NSString *)restoreIdentifier
                                 queue:(nullable dispatch_queue_t)queue
                     peripheralManager:(CBPeripheralManager *)peripheralManager {
  self = [super initWithAdvertisedName:advertisedName
                     restoreIdentifier:restoreIdentifier
                                 queue:queue
                     peripheralManager:peripheralManager];
  if (self) {
    // Initialize with a mock CBPeripheralManager to avoid nil issues.
    _mockCBPeripheralManager = peripheralManager;
    _updateOutgoingCharOnSocketCalled = NO;
    _updateOutgoingCharacteristicCalled = NO;
  }
  return self;
}

- (BOOL)updateOutgoingCharacteristic:(NSData *)data onSocket:(GNSSocket *)socket {
  self.updateOutgoingCharacteristicCalled = YES;
  self.lastUpdatedData = data;
  self.lastUpdatedSocket = socket;
  // Simulate success.
  return YES;
}

- (void)updateOutgoingCharOnSocket:(GNSSocket *)socket withHandler:(GNSUpdateValueHandler)handler {
  self.updateOutgoingCharOnSocketCalled = YES;
  self.lastUpdatedSocket = socket;
  self.serviceManagerUpdateHandler = handler;
  if (self.updateOutgoingCharOnSocketBlock) {
    self.updateOutgoingCharOnSocketBlock(socket, handler);
  }
}

- (void)simulateReadyToUpdateSubscribers {
  if (self.serviceManagerUpdateHandler) {
    self.serviceManagerUpdateHandler();
  }
}

- (void)socketDidDisconnect:(GNSSocket *)socket {
  if (self.socketDidDisconnectBlock) {
    self.socketDidDisconnectBlock(socket);
  }
}

- (CBPeripheralManager *)cbPeripheralManagerWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                                                   queue:(dispatch_queue_t)queue
                                                 options:(NSDictionary<NSString *, id> *)options {
  // Override the method from the base class to return the mock peripheral manager.
  return self.mockCBPeripheralManager;
}

@end
