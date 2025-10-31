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

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeBLEMedium.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEMedium.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"

NS_ASSUME_NONNULL_BEGIN

@implementation GNCFakeBLEMedium

- (instancetype)init {
  self = [super init];
  if (self) {
    _openL2CAPChannelShouldComplete = YES;
  }
  return self;
}

- (void)startAdvertisingData:(NSDictionary<CBUUID *, NSData *> *)advertisementData
           completionHandler:(nullable GNCStartAdvertisingCompletionHandler)completionHandler {
  if (completionHandler) {
    completionHandler(self.startAdvertisingError);
  }
}

- (void)stopAdvertisingWithCompletionHandler:
    (nullable GNCStopAdvertisingCompletionHandler)completionHandler {
  if (completionHandler) {
    completionHandler(self.stopAdvertisingError);
  }
}

- (void)startScanningForService:(CBUUID *)serviceUUID
      advertisementFoundHandler:(GNCAdvertisementFoundHandler)advertisementFoundHandler
              completionHandler:(nullable GNCStartScanningCompletionHandler)completionHandler {
  self.advertisementFoundHandler = advertisementFoundHandler;
  if (completionHandler) {
    completionHandler(self.startScanningError);
  }
}

- (void)startScanningForMultipleServices:(NSArray<CBUUID *> *)serviceUUIDs
               advertisementFoundHandler:(GNCAdvertisementFoundHandler)advertisementFoundHandler
                       completionHandler:
                           (nullable GNCStartScanningCompletionHandler)completionHandler {
  self.advertisementFoundHandler = advertisementFoundHandler;
  if (completionHandler) {
    completionHandler(self.startScanningError);
  }
}

- (void)stopScanningWithCompletionHandler:
    (nullable GNCStopScanningCompletionHandler)completionHandler {
  if (completionHandler) {
    completionHandler(self.stopScanningError);
  }
}

- (void)resumeMediumScanning:(nullable GNCStartScanningCompletionHandler)completionHandler {
  if (completionHandler) {
    completionHandler(self.resumeScanningError);
  }
}

- (void)startGATTServerWithCompletionHandler:
    (nullable GNCGATTServerCompletionHandler)completionHandler {
  if (completionHandler) {
    completionHandler(self.fakeGATTServer, self.startGATTServerError);
  }
}

- (void)connectToGATTServerForPeripheral:(id<GNCPeripheral>)peripheral
                    disconnectionHandler:(nullable GNCGATTDisconnectionHandler)disconnectionHandler
                       completionHandler:
                           (nullable GNCGATTConnectionCompletionHandler)completionHandler {
  self.lastConnectedPeripheral = peripheral;
  self.lastDisconnectionHandler = disconnectionHandler;
  if (completionHandler) {
    if (!self.connectToGATTServerError) {
      if (!self.fakeGATTClient) {
        // Create a default fake client if one isn't provided.
        self.fakeGATTClient = [[GNCBLEGATTClient alloc] initWithPeripheral:peripheral
                                               requestDisconnectionHandler:^(id<GNCPeripheral> p){
                                                   // Do nothing in fake.
                                               }];
      }
      completionHandler(self.fakeGATTClient, nil);
    } else {
      completionHandler(nil, self.connectToGATTServerError);
    }
  }
}

- (void)openL2CAPServerWithPSMPublishedCompletionHandler:
            (GNCOpenL2CAPServerPSMPublishedCompletionHandler)psmPublishedCompletionHandler
                          channelOpenedCompletionHandler:
                              (GNCOpenL2CAPServerChannelOpendCompletionHandler)
                                  channelOpenedCompletionHandler
                                       peripheralManager:
                                           (nullable id<GNCPeripheralManager>)peripheralManager {
  if (psmPublishedCompletionHandler) {
    psmPublishedCompletionHandler(self.fakePSM, self.openL2CAPServerSocketError);
  }
  // In the fake, we don't have a real channel opened event.
}

- (void)openL2CAPChannelWithPSM:(CBL2CAPPSM)PSM
                     peripheral:(id<GNCPeripheral>)peripheral
              completionHandler:(nullable GNCOpenL2CAPStreamCompletionHandler)completionHandler {
  self.lastConnectedPeripheral = peripheral;
  if (self.openL2CAPChannelShouldComplete && completionHandler) {
    completionHandler(self.fakeL2CAPStream, self.openL2CAPChannelError);
  }
}

- (BOOL)supportsExtendedAdvertisements {
  return NO;
}

@end

NS_ASSUME_NONNULL_END
