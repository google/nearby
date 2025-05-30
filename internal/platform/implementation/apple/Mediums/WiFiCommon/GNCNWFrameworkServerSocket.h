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

NS_ASSUME_NONNULL_BEGIN

@class GNCIPv4Address;
@class GNCNWFrameworkSocket;

@interface GNCNWFrameworkServerSocket : NSObject

/**
 * @remark init is not an available initializer.
 */
- (instancetype)init NS_UNAVAILABLE;

/**
 * Creates a server socket for a given port.
 *
 * @param port The port of the server socket.
 */
- (instancetype)initWithPort:(NSInteger)port NS_DESIGNATED_INITIALIZER;

/**
 * The IPv4 address of the physical network interface.
 */
@property(nonatomic, readonly, copy) GNCIPv4Address *ipAddress;

/**
 * The port of the server socket.
 */
@property(nonatomic, readonly) NSInteger port;

/**
 * Returns an arbitrary, connected socket, ready to exchange data.
 *
 * Blocks execution until at least one connection is ready, a permanent error occurs or the server
 * socket was closed.
 *
 * Any error reported indicates a permanent failure and an automatic closure of the server socket.
 *
 * @param[out] error Error that will be populated on failure.
 * @return Returns a connected socket or nil if an error has occured.
 */
- (nullable GNCNWFrameworkSocket *)acceptWithError:(NSError **_Nullable)error;

/**
 * Stops listening for inbound connections.
 *
 * Any calls to acceptWithError: will be unblocked with an error.
 */
- (void)close;

@end

NS_ASSUME_NONNULL_END
