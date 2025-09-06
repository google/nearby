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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

@protocol GNCNWConnection;

NS_ASSUME_NONNULL_BEGIN

@interface GNCNWFrameworkServerSocket (Internal)

// Properties for testing
@property(nonatomic, readonly) NSMutableArray<nw_connection_t> *pendingConnections;
@property(nonatomic, readonly) NSMutableArray<nw_connection_t> *readyConnections;

// Properties for testing
@property(nonatomic) nw_listener_state_t listenerState;
@property(nonatomic, nullable) NSError *listenerError;
@property(nonatomic, nullable) id<GNCNWListener> testingListener;

/**
 * Starts listening for inbound connections.
 *
 * Blocks execution until listening has started or failed.
 *
 * @param includePeerToPeer Indicates if the server should be configured for peer-to-peer
 * connections.
 * @param[out] error Error that will be populated on failure.
 * @return Returns YES when listening has successfully started.
 */
- (BOOL)startListeningWithError:(NSError **_Nullable)error
              includePeerToPeer:(BOOL)includePeerToPeer;

/**
 * Starts listening for inbound connections with a PSK identity and shared secret.
 *
 * Blocks execution until listening has started or failed.
 *
 * @param PSKIdentity The PSK identity to use for the connection.
 * @param PSKSharedSecret The PSK shared secret to use for the connection.
 * @param includePeerToPeer Indicates if the server should be configured for peer-to-peer
 * connections.
 * @param[out] error Error that will be populated on failure.
 * @return Returns YES when listening has successfully started.
 */
- (BOOL)startListeningWithPSKIdentity:(NSData *)PSKIdentify
                      PSKSharedSecret:(NSData *)PSKSharedSecret
                    includePeerToPeer:(BOOL)includePeerToPeer
                                error:(NSError **_Nullable)error;

/**
 * Creates a Bonjour service that advertises the listener on the local network.
 *
 * @param serviceName The Bonjour name of the service.
 * @param serviceType The Bonjour type of the service.
 * @param txtRecords The TXT record to advertise with the service.
 */
- (void)startAdvertisingServiceName:(NSString *)serviceName
                        serviceType:(NSString *)serviceType
                         txtRecords:(NSDictionary<NSString *, NSString *> *)txtRecords;

/**
 * Removes the Bonjour service advertisement.
 */
- (void)stopAdvertising;

/**
 * Adds a fake connection to the server socket for testing.
 *
 * @param connection The fake connection to add.
 */
- (void)addFakeConnection:(id<GNCNWConnection>)connection;

/**
 * Handles a state change for a given connection.
 *
 * @param connection The connection that changed state.
 * @param state The new state.
 * @param error An error object if the state is `failed`, otherwise `nil`.
 */
- (void)handleConnectionStateChange:(nw_connection_t)connection
                              state:(nw_connection_state_t)state
                              error:(nullable nw_error_t)error;

@end

NS_ASSUME_NONNULL_END
