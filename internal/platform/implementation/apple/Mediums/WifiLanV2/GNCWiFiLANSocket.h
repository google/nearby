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

#import <Foundation/Foundation.h>
#import <Network/Network.h>

@interface GNCWiFiLANSocket : NSObject

/**
 * @remark init is not an available initializer.
 */
- (nonnull instancetype)init NS_UNAVAILABLE;

/**
 * Creates a socket that allows reading/writing for a given connection.
 *
 * @param connection A bidirectional data connection between a local and remote endpoint. This class
 *                   will take ownership of connection and manage its lifetime. The connection
 *                   should not be shared or reused.
 */
- (nonnull instancetype)initWithConnection:(nonnull nw_connection_t)connection
    NS_DESIGNATED_INITIALIZER;

/**
 * Reads the requested amount of bytes from the connection.
 *
 * Blocks execution until the bytes have been read or an error occurs.
 *
 * @param length The number of bytes to read.
 * @param[out] error Error that will be populated on failure. A read may return non-nil data along
 *                   with an error. This normally happens if the data read is shorter than the
 *                   requested length.
 */
- (nullable NSData *)readMaxLength:(NSUInteger)length error:(NSError **_Nullable)error;

/**
 * Writes the given data to the connection.
 *
 * Blocks execution until the bytes have been successfully written or an error occurs.
 *
 * @param data The data to write.
 * @param[out] error Error that will be populated on failure.
 */
- (BOOL)write:(NSData *)data error:(NSError **_Nullable)error;

/**
 * Gracefully closes the connection to remote endpoint.
 *
 * If a read or write is in progress, they will be unblocked with an error.
 */
- (void)close;

@end
