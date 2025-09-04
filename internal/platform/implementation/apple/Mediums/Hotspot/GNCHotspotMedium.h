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

#import <Foundation/Foundation.h>  // NOLINT
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

@class GNCIPv4Address;
@class GNCHotspotSocket;
@class GNCNWFrameworkSocket;

/**
 * The main Hotspot medium used inside of Nearby. This class is responsible for managing the
 * Hotspot functionality of Nearby.
 *
 * @note The public APIs of this class are thread safe.
 */
@interface GNCHotspotMedium : NSObject

/**
 * Initializes the Hotspot medium with the specified queue.
 *
 * @param queue The queue to use for all internal operations.
 * @return An initialized Hotspot medium.
 */
- (instancetype)initWithQueue:(dispatch_queue_t)queue;

/**
 * Connects to a Wifi Hotspot with the specified SSID and password.
 *
 * @param ssid The SSID of the Hotspot to connect to.
 * @param password The password of the Hotspot to connect to.
 * @return YES if the connection was successful, NO otherwise.
 */
- (BOOL)connectToWifiNetworkWithSSID:(NSString *)ssid password:(NSString *)password;

/**
 * Disconnects from a Wifi Hotspot with the specified SSID.
 *
 * @param ssid The SSID of the Hotspot to disconnect from.
 */
- (void)disconnectToWifiNetworkWithSSID:(NSString *)ssid;

/**
 * Gets the current Wifi SSID.
 *
 * @return The current Wifi SSID, or nil if not connected to a Wifi network.
 */
- (NSString *)getCurrentWifiSSID;

/**
 * Connects to an IP address and port.
 *
 * @param host The IPv4 address to connect to.
 * @param port The port to connect to.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a connected socket or nil if an error has occurred.
 */
- (nullable GNCNWFrameworkSocket *)connectToHost:(GNCIPv4Address *)host
                                            port:(NSInteger)port
                                    cancelSource:(nullable dispatch_source_t)cancelSource
                                           error:(NSError *_Nullable *_Nullable)error;

@end

NS_ASSUME_NONNULL_END
