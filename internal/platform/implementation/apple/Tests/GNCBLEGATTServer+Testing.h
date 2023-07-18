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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"

#import <Foundation/Foundation.h>

@protocol GNCPeripheralManager;

NS_ASSUME_NONNULL_BEGIN

@interface GNCBLEGATTServer (Testing)

/**
 * Creates a GATT server with a provided peripheral manager.
 *
 * This is only exposed for testing and can be used to inject a fake peripheral manager.
 *
 * @param peripheralManager The peripheral manager instance.
 */
- (instancetype)initWithPeripheralManager:(id<GNCPeripheralManager>)peripheralManager;

@end

NS_ASSUME_NONNULL_END
