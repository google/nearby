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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCMConnection.h"

#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

/** A fake implementation of @c GNCMConnection for testing. */
@interface GNCMFakeConnection : NSObject <GNCMConnection>

@property(nonatomic, readwrite) BOOL isConnected;
@property(nonatomic, readwrite) NSInteger maxDataLength;
@property(nonatomic, readwrite) GNCBLEGATTCharacteristic *characteristic;
@property(nonatomic, readwrite) id<GNCPeripheral> peripheral;
@property(nonatomic, readwrite) GNCMConnectionHandlers *connectionHandlers;

- (void)simulateDataReceived:(NSData *)data;
- (void)simulateDisconnect;

@end
