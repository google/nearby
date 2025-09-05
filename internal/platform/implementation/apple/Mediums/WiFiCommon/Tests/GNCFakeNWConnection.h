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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnection.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * A fake implementation of GNCNWConnection for testing purposes.
 */
@interface GNCFakeNWConnection : NSObject <GNCNWConnection>

/** The data to be received by the fake connection. */
@property(nonatomic, nullable) dispatch_data_t dataToReceive;

/** Whether cancel has been called on the fake connection. */
@property(nonatomic) BOOL cancelCalled;

/** Property to simulate send failure. */
@property(nonatomic) BOOL simulateSendFailure;

/** Property to simulate receive failure. */
@property(nonatomic) BOOL simulateReceiveFailure;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNWConnection:(nw_connection_t)connection NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END