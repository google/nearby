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

@class GNCWiFiLANServerSocket;
@class GNCWiFiLANSocket;

/** A handler that delivers updates about discovered services. */
typedef void (^ServiceUpdateHandler)(NSString *_Nonnull serviceName,
                                     NSDictionary<NSString *, NSString *> *_Nonnull txtRecords);

/**
 * The @c GNCWiFiLANMedium object is used as a delegate by the platform abstraction layer for
 * Wi-Fi LAN related functionality on Apple platforms.
 */
@interface GNCWiFiLANMedium : NSObject

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
 * @param[out] error Error that will be populated on failure.
 * @return Returns a server socket or nil if an error has occured.
 */
- (nullable GNCWiFiLANServerSocket *)listenForServiceOnPort:(NSInteger)port
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
                 serviceName:(nonnull NSString *)serviceName
                 serviceType:(nonnull NSString *)serviceType
                  txtRecords:(nonnull NSDictionary<NSString *, NSString *> *)txtRecords;

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
 * @param[out] error Error that will be populated on failure.
 * @return Returns YES when discovery has successfully started.
 */
- (BOOL)startDiscoveryForServiceType:(nonnull NSString *)serviceType
                 serviceFoundHandler:(ServiceUpdateHandler)serviceFoundHandler
                  serviceLostHandler:(ServiceUpdateHandler)serviceLostHandler
                               error:(NSError **_Nullable)error;

/**
 * Stops browsing for a specific Bonjour service.
 *
 * @param serviceType The Bonjour type of the service.
 */
- (void)stopDiscoveryForServiceType:(nonnull NSString *)serviceType;

/**
 * Connects to a Bonjour service.
 *
 * @param serviceName The Bonjour name of the service.
 * @param serviceType The Bonjour type of the service.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a connected socket or nil if an error has occured.
 */
- (nullable GNCWiFiLANSocket *)connectToServiceName:(nonnull NSString *)serviceName
                                        serviceType:(nonnull NSString *)serviceType
                                              error:(NSError **_Nullable)error;

/**
 * Connects to an IP address and port.
 *
 * @param host The 4 byte binary representation IPv4 address to connect to.
 * @param port The port to connect to.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a connected socket or nil if an error has occured.
 */
- (nullable GNCWiFiLANSocket *)connectToHost:(nonnull NSString *)host
                                        port:(NSInteger)port
                                       error:(NSError **_Nullable)error;

@end
