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

#import <Foundation/Foundation.h> // NOLINT
#import <Network/Network.h>

@class GNCIPv4Address;
@class GNCHotspotSocket;

/**
 * The main Hotspot medium used inside of Nearby. This class is responsible for managing the
 * Hotspot functionality of Nearby.
 *
 * @note The public APIs of this class are thread safe.
 */
@interface GNCHotspotMedium : NSObject

/**
 * Connects to a Wifi Hotspot with the specified SSID and password.
 *
 * @param ssid The SSID of the Hotspot to connect to.
 * @param password The password of the Hotspot to connect to.
 * @return YES if the connection was successful, NO otherwise.
 */
- (BOOL)connectToWifiNetworkWithSSID:(nonnull NSString *)ssid
                      password:(nonnull NSString *)password;

/**
 * Disconnects from a Wifi Hotspot with the specified SSID.
 *
 * @param ssid The SSID of the Hotspot to disconnect from.
 */
- (void) disconnectToWifiNetworkWithSSID:(nonnull NSString *)ssid;

/**
 * Connects to an IP address and port.
 *
 * @param host The IPv4 address to connect to.
 * @param port The port to connect to.
 * @param[out] error Error that will be populated on failure.
 * @return Returns a connected socket or nil if an error has occurred.
 */
- (nullable GNCHotspotSocket *)connectToHost:(nonnull GNCIPv4Address *)host
                                            port:(NSInteger)port
                                           error:(NSError **_Nullable)error;

@end
