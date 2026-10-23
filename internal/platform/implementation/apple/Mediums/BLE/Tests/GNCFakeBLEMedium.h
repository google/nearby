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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEMedium.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeBLEGATTServer.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCFakeBLEMedium : GNCBLEMedium

// Properties to control fake behavior.
@property(nonatomic, nullable) NSError *startAdvertisingError;
@property(nonatomic, nullable) NSError *stopAdvertisingError;
@property(nonatomic, nullable) NSError *startScanningError;
@property(nonatomic, nullable) NSError *stopScanningError;
@property(nonatomic, nullable) NSError *resumeScanningError;
@property(nonatomic, nullable) NSError *startGATTServerError;
@property(nonatomic, nullable) NSError *connectToGATTServerError;
@property(nonatomic, nullable) NSError *openServerSocketError;
@property(nonatomic, nullable) NSError *openL2CAPServerSocketError;
@property(nonatomic, nullable) NSError *openL2CAPChannelError;

@property(nonatomic, nullable) GNCBLEGATTServer *fakeGATTServer;
@property(nonatomic, nullable) GNCBLEGATTClient *fakeGATTClient;
@property(nonatomic, nullable) GNCBLEL2CAPStream *fakeL2CAPStream;
@property(nonatomic) uint16_t fakePSM;

@property(nonatomic, nullable) id<GNCPeripheral> lastConnectedPeripheral;
@property(nonatomic, nullable) GNCGATTDisconnectionHandler lastDisconnectionHandler;

@property(nonatomic, nullable) GNCAdvertisementFoundHandler advertisementFoundHandler;

/** When NO, the completion for opening an L2CAP channel will not be invoked. Default is YES. */
@property(nonatomic) BOOL openL2CAPChannelShouldComplete;

@end

NS_ASSUME_NONNULL_END
