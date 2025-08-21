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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Central/GNSFakeCentralManager.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Tests/Central/GNSFakePeripheral.h"
#import "third_party/objective_c/ocmock/v3/Source/OCMock/OCMock.h"

@interface GNSFakeCentralManager ()
@property(nonatomic) NSMutableDictionary<NSUUID *, GNSCentralPeerManager *> *peerManagers;
@end

@implementation GNSFakeCentralManager

- (instancetype)initWithSocketServiceUUID:(CBUUID *)socketServiceUUID {
  self = [super initWithSocketServiceUUID:socketServiceUUID];
  if (self) {
    _peerManagers = [NSMutableDictionary dictionary];
  }
  return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
  // Forward to own delegate if set.
  [self.delegate centralManagerDidUpdateBleState:self];
}

// Override to return the test state.
- (CBManagerState)state {
  return self.testCbManagerState;
}

// This is the property on CBCentralManager.
- (CBManagerState)cbManagerState {
  return self.testCbManagerState;
}

- (void)connectPeripheralForPeer:(GNSCentralPeerManager *)peer
                         options:(nullable NSDictionary<NSString *, id> *)options {
  self.connectingPeer = peer;
  // Simulate async connection.
  dispatch_async(dispatch_get_main_queue(), ^{
    if (self.failConnection) {
      NSError *error = [NSError errorWithDomain:CBErrorDomain
                                           code:CBErrorPeripheralDisconnected
                                       userInfo:nil];
      [peer bleDisconnectedWithError:error];
      self.connectingPeer = nil;
      return;
    }
    if (self.connectingPeer == peer) {
      CBPeripheral *peripheral = peer.cbPeripheral;
      if ([peripheral isKindOfClass:[GNSFakePeripheral class]]) {
        GNSFakePeripheral *fakePeripheral = (GNSFakePeripheral *)peripheral;
        fakePeripheral.state = CBPeripheralStateConnected;
        fakePeripheral.delegate = (id<CBPeripheralDelegate>)peer;
      } else if ([peripheral isKindOfClass:[OCMockObject class]]) {
        OCMStub([peripheral state]).andReturn(CBPeripheralStateConnected);
      }
      self.connectedPeripheral = peripheral;
      [peer bleConnected];
      [(id)peripheral discoverServices:nil];
    }
  });
}

- (void)cancelPeripheralConnectionForPeer:(GNSCentralPeerManager *)peer {
  if (self.connectingPeer == peer) {
    self.connectingPeer = nil;
  }
  if (self.connectedPeripheral == peer.cbPeripheral) {
    self.connectedPeripheral = nil;
    CBPeripheral *peripheral = peer.cbPeripheral;
    if ([peripheral isKindOfClass:[GNSFakePeripheral class]]) {
      ((GNSFakePeripheral *)peripheral).state = CBPeripheralStateDisconnected;
    } else if ([peripheral isKindOfClass:[OCMockObject class]]) {
      OCMStub([peripheral state]).andReturn(CBPeripheralStateDisconnected);
    }
    [peer bleDisconnectedWithError:nil];
  }
}

- (void)centralPeerManagerDidDisconnect:(GNSCentralPeerManager *)peer {
  if (self.connectedPeripheral == peer.cbPeripheral) {
    self.connectedPeripheral = nil;
  }
}

- (GNSCentralPeerManager *)retrieveCentralPeerWithIdentifier:(NSUUID *)identifier {
  GNSCentralPeerManager *peerManager = self.peerManagers[identifier];
  if (!peerManager) {
    NSArray<CBPeripheral *> *peripherals =
        [self retrievePeripheralsWithIdentifiers:@[ identifier ]];
    if (peripherals.count > 0) {
      peerManager = [[GNSCentralPeerManager alloc] initWithPeripheral:peripherals[0]
                                                       centralManager:self];
      self.peerManagers[identifier] = peerManager;
    } else {
      return nil;  // Match the real behavior when no peripheral is found.
    }
  }
  return peerManager;
}

- (NSArray<CBPeripheral *> *)retrievePeripheralsWithIdentifiers:(NSArray<NSUUID *> *)identifiers {
  if (self.peripheralsToRetrieve) {
    NSMutableArray<CBPeripheral *> *foundPeripherals = [NSMutableArray array];
    for (CBPeripheral *p in self.peripheralsToRetrieve) {
      if ([identifiers containsObject:p.identifier]) {
        [foundPeripherals addObject:p];
      }
    }
    return foundPeripherals;
  }
  // Default behavior: return a new fake peripheral for each identifier.
  NSMutableArray<CBPeripheral *> *peripherals = [NSMutableArray array];
  for (NSUUID *identifier in identifiers) {
    GNSFakePeripheral *peripheral =
        [[GNSFakePeripheral alloc] initWithIdentifier:identifier
                                          serviceUUID:self.socketServiceUUID];
    [peripherals addObject:(id)peripheral];
  }
  return peripherals;
}

#pragma mark - CBCentralManagerDelegate Overrides

- (void)centralManager:(CBCentralManager *)central
    didDiscoverPeripheral:(CBPeripheral *)peripheral
        advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                     RSSI:(NSNumber *)RSSI {
  // The filtering logic is in GNSCentralManager, so the fake doesn't need to redo it.
  // We just need to create the peer and notify the delegate.
  GNSCentralPeerManager *peerManager =
      [self retrieveCentralPeerWithIdentifier:peripheral.identifier];
  if (peerManager && self.delegate) {
    [self.delegate centralManager:self
                  didDiscoverPeer:peerManager
                advertisementData:advertisementData];
  }
}

@end
