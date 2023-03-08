// Copyright 2022 Google LLC
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

@class GNCSupportedMediums;

NS_ASSUME_NONNULL_BEGIN

/**
 * A @c GNCConnectionOptions object represents the configuration for connecting
 * to remote endpoints.
 */
@interface GNCConnectionOptions : NSObject

/** The @c GNCStrategy advertising strategy. */
@property(nonatomic) GNCSupportedMediums* mediums;

/** Whether bandwidth should automatically upgrade. */
@property(nonatomic) BOOL autoUpgradeBandwidth;

/** Whether topology constraints should be enforced. */
@property(nonatomic) BOOL enforceTopologyConstraints;

/** Whether low power should be used. */
@property(nonatomic) BOOL lowPower;

/** The remote bluetooth MAC address for this connection. */
@property(nonatomic, nullable) NSData* remoteBluetoothMACAddress;

/** How often to send a keep alive message, in seconds. */
@property(nonatomic) NSTimeInterval keepAliveIntervalInSeconds;

/**
 * How long to wait without receiving a keep alive message before timing out,
 * in seconds.
 */
@property(nonatomic) NSTimeInterval keepAliveTimeoutInSeconds;

/** @remark create a connection options class. */
- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
