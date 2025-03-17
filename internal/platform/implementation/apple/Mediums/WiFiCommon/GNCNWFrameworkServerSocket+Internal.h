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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"

@interface GNCNWFrameworkServerSocket (Internal)

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
- (BOOL)startListeningWithError:(NSError **_Nullable)error includePeerToPeer:(BOOL)peerToPeer;

/**
 * Creates a Bonjour service that advertises the listener on the local network.
 *
 * @param serviceName The Bonjour name of the service.
 * @param serviceType The Bonjour type of the service.
 * @param txtRecords The TXT record to advertise with the service.
 * @param includePeerToPeer Indicates if the service should be configured for peer-to-peer
 * connections.
 */
- (void)startAdvertisingServiceName:(nonnull NSString *)serviceName
                        serviceType:(nonnull NSString *)serviceType
                         txtRecords:(nonnull NSDictionary<NSString *, NSString *> *)txtRecords
                             includePeerToPeer:(BOOL)includePeerToPeer;
/**
 * Removes the Bonjour service advertisement.
 */
- (void)stopAdvertising;

@end
