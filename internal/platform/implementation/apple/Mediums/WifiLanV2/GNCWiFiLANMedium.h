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

@end
