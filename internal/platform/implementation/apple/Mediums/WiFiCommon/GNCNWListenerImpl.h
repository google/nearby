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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWListener.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Concrete implementation of GNCNWListener, wrapping the real Network.framework C functions.
 */
@interface GNCNWListenerImpl : NSObject <GNCNWListener>

- (instancetype)init NS_UNAVAILABLE;

/**
 * Initializes a listener with the given parameters.
 *
 * @param parameters The parameters to use for the listener.
 * @return The new listener object, or `nil` on error.
 */
- (nullable instancetype)initWithParameters:(nw_parameters_t)parameters NS_DESIGNATED_INITIALIZER;

/**
 * Initializes a listener with the given port and parameters.
 *
 * @param port The port to listen on.
 * @param parameters The parameters to use for the listener.
 * @return The new listener object, or `nil` on error.
 */
- (nullable instancetype)initWithPort:(const char *)port
                           parameters:(nw_parameters_t)parameters NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
