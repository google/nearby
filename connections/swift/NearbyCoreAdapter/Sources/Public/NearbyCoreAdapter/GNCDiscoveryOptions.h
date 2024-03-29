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

typedef NS_CLOSED_ENUM(NSInteger, GNCStrategy);

NS_ASSUME_NONNULL_BEGIN

/**
 * A @c GNCDiscoveryOptions object represents the configuration for remote
 * endpoint discovery.
 */
@interface GNCDiscoveryOptions : NSObject

/** The @c GNCStrategy advertising strategy. */
@property(nonatomic, readonly) GNCStrategy strategy;

/** Defines which mediums are supported for advertising.*/
@property(nonatomic) GNCSupportedMediums* mediums;

/** Whether topology constraints should be enforced. */
@property(nonatomic) BOOL enforceTopologyConstraints;

/** Whether low power should be used. */
@property(nonatomic) BOOL lowPower;

/** @remark init is not an available initializer. */
- (instancetype)init NS_UNAVAILABLE;

/**
 * Creates a discovery options object.
 *
 * @param strategy The connection strategy.
 *
 * @return The initialized options object, or nil if an error occurs.
 */
- (instancetype)initWithStrategy:(GNCStrategy)strategy NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
