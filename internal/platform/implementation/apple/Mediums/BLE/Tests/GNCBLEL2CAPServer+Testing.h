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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPServer.h"

#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManager.h"

@protocol GNCPeripheralManager;

NS_ASSUME_NONNULL_BEGIN

@interface GNCBLEL2CAPServer (Testing) <GNCPeripheralManagerDelegate>

/**
 * Creates a L2CAP server with a provided peripheral manager.
 *
 * This is only exposed for testing and can be used to inject a fake peripheral manager.
 *
 * @param peripheralManager The peripheral manager instance.
 */
- (instancetype)initWithPeripheralManager:(id<GNCPeripheralManager>)peripheralManager;

/**
 * Closes the L2CAP channel.
 *
 * This is only exposed for testing.
 */
- (void)closeL2CAPChannel;

@end

NS_ASSUME_NONNULL_END
