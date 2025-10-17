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
@class GNCNWFrameworkServerSocket;
@class GNCNWFrameworkSocket;

/** A handler that delivers updates about discovered services. */
typedef void (^ServiceUpdateHandler)(NSString *_Nonnull serviceName,
                                     NSDictionary<NSString *, NSString *> *_Nonnull txtRecords);

/**
 * The @c GNCNWFramework object is used as a delegate by the platform abstraction layer for
 * Network.Framework medium related functionality on Apple platforms.
 */
@interface GNCNWFramework : NSObject

/** Whether to include peer-to-peer services. */
@property(atomic, readonly) BOOL includePeerToPeer;

/**
 * Listens for incoming connections on a given port.
 *
 * This function will always be called before a call to
 * startAdvertisingServiceName:serviceType:error:, but may also be called on its own without a call
 * to start advertising the service. This function will also be called during upgrade attempts on
 * the advertising side.
 *
 * This function should block execution until ready for incoming connections.
 *
 * @param port The port on which the listener can accept connections. Should be a number between 1
 *             and 65536 to open a server socket on that exact port. Zero can be used to listen on
 *             a random port.
 * @param includePeerToPeer Whether to include peer-to-peer services.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a server socket or nil if an error has occured.
 */
- (nullable GNCNWFrameworkServerSocket *)listenForServiceOnPort:(NSInteger)port
                                              includePeerToPeer:(BOOL)includePeerToPeer
                                                          error:(NSError **_Nullable)error;

/**
 * Listens for incoming connections with PSK based TLS on a given port.
 *
 * This function will always be called before a call to
 * startAdvertisingServiceName:serviceType:error:, but may also be called on its own without a call
 * to start advertising the service. This function will also be called during upgrade attempts on
 * the advertising side.
 *
 * This function should block execution until ready for incoming connections.
 *
 * @param port The port on which the listener can accept connections. Should be a number between 1
 *             and 65536 to open a server socket on that exact port. Zero can be used to listen on
 *             a random port.
 * @param PSKIdentity The PSK identity of the service.
 * @param PSKSharedSecret The PSK shared secret of the service.
 * @param includePeerToPeer Whether to include peer-to-peer services.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a server socket or nil if an error has occured.
 */
- (nullable GNCNWFrameworkServerSocket *)listenForServiceWithPSKIdentity:(NSData *)PSKIdentity
                                                         PSKSharedSecret:(NSData *)PSKSharedSecret
                                                                    port:(NSInteger)port
                                                       includePeerToPeer:(BOOL)includePeerToPeer
                                                                   error:(NSError **_Nullable)error;

/**
 * Creates a Bonjour service that advertises the listener on the local network.
 *
 * @param port The port of the listener to advertise.
 * @param serviceName The Bonjour name of the service.
 * @param serviceType The Bonjour type of the service.
 * @param txtRecords The TXT record to advertise with the service.
 */
- (void)startAdvertisingPort:(NSInteger)port
                 serviceName:(NSString *)serviceName
                 serviceType:(NSString *)serviceType
                  txtRecords:(NSDictionary<NSString *, NSString *> *)txtRecords;

/**
 * Removes the Bonjour service advertisement.
 *
 * @param port The port of the listener to stop advertising.
 */
- (void)stopAdvertisingPort:(NSInteger)port;

/**
 * Starts browsing for a Bonjour service.
 *
 * @param serviceType The Bonjour type of the service.
 * @param serviceFoundHandler A handler called when a new service is found.
 * @param serviceLostHandler A handler called when a previously discovered service is lost.
 * @param includePeerToPeer Whether to include peer-to-peer services.
 * @param[out] error Error that will be populated on failure.
 * @return Returns YES when discovery has successfully started.
 */
- (BOOL)startDiscoveryForServiceType:(NSString *)serviceType
                 serviceFoundHandler:(ServiceUpdateHandler)serviceFoundHandler
                  serviceLostHandler:(ServiceUpdateHandler)serviceLostHandler
                   includePeerToPeer:(BOOL)includePeerToPeer
                               error:(NSError **_Nullable)error;

/**
 * Stops browsing for a specific Bonjour service.
 *
 * @param serviceType The Bonjour type of the service.
 */
- (void)stopDiscoveryForServiceType:(NSString *)serviceType;

/**
 * Connects to a Bonjour service.
 *
 * @param serviceName The Bonjour name of the service.
 * @param serviceType The Bonjour type of the service.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a connected socket or nil if an error has occured.
 */
- (nullable GNCNWFrameworkSocket *)connectToServiceName:(NSString *)serviceName
                                            serviceType:(NSString *)serviceType
                                                  error:(NSError **_Nullable)error;

/**
 * Connects to a Bonjour service with PSK based TLS.
 *
 * @param serviceName The Bonjour name of the service.
 * @param serviceType The Bonjour type of the service.
 * @param PSKIdentity The PSK identity of the service.
 * @param PSKSharedSecret The PSK shared secret of the service.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a connected socket or nil if an error has occured.
 */
- (nullable GNCNWFrameworkSocket *)connectToServiceName:(NSString *)serviceName
                                            serviceType:(NSString *)serviceType
                                            PSKIdentity:(NSData *)PSKIdentity
                                        PSKSharedSecret:(NSData *)PSKSharedSecret
                                                  error:(NSError **_Nullable)error;

/**
 * Connects to an IP address and port.
 *
 * @param host The IPv4 address to connect to.
 * @param port The port to connect to.
 * @param includePeerToPeer Whether to include peer-to-peer services.
 * @param cancelSource An optional dispatch source to cancel the connection attempt.
 * @param queue An optional dispatch queue to use for connection events. If nil, the main queue is
 * used.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a connected socket or nil if an error has occured.
 */
- (nullable GNCNWFrameworkSocket *)connectToHost:(GNCIPv4Address *)host
                                            port:(NSInteger)port
                               includePeerToPeer:(BOOL)includePeerToPeer
                                    cancelSource:(nullable dispatch_source_t)cancelSource
                                           queue:(nullable dispatch_queue_t)queue
                                           error:(NSError **_Nullable)error;

/**
 * Returns YES if the framework is listening for any service.
 */
- (BOOL)isListeningForAnyService;

/**
 * Returns YES if the framework is discovering any service.
 */
- (BOOL)isDiscoveringAnyService;
@end

NS_ASSUME_NONNULL_END
