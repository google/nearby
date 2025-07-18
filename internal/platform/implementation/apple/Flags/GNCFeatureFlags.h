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

#import <Foundation/Foundation.h>

/** A utility class for accessing and overriding Nearby feature flags. */
@interface GNCFeatureFlags : NSObject

/** Checks whether DCT is enabled in the Nearby Connections SDK. */
@property(nonatomic, class, readonly) BOOL dctEnabled;

/** Checks whether GATT client disconnection is enabled in the Nearby Connections SDK. */
@property(nonatomic, class, readonly) BOOL gattClientDisconnectionEnabled;

/** Checks whether BLE L2CAP is enabled in the Nearby Connections SDK. */
@property(nonatomic, class, readonly) BOOL bleL2capEnabled;

/** Checks whether BLE L2CAP refactor is enabled in the Nearby Connections SDK. */
@property(nonatomic, class, readonly) BOOL refactorBleL2capEnabled;

@end
