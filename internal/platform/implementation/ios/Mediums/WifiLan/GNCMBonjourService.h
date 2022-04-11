// Copyright 2020 Google LLC
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

#import "internal/platform/implementation/ios/Mediums/GNCMConnection.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * GNCMBonjourService publishes mDNS type and domain with the specified service ID via Apple
 * Bonjour service. The mDNS type is a string formatted as "_[serviceIdHash]._tcp." in which
 * [serviceIdHash] is generated as a SHA-256 hash from the service ID and taken the 6 first bytes
 * of string in upper case.
 * When the service connects, the specified |endpointConnectedHandler| is called, which establishes
 * a connection to the browser.
 *
 * Don't hold the strong reference of caller self in endpointConnectedHandler to ensure there is no
 * retain cycle between them.
 *
 * @param serviceName A service name that embeds the |WifiLanServiceInfo| information.
 * @param serviceType An mDNS type that uniquely identifies the published service to search for.
 * @param port The requesting socket port number.
 * @param txtRecordData The TXTRecord data.
 * @param endpointConnectedHandler The handler that is called when a browser connects.
 */
@interface GNCMBonjourService : NSObject

- (instancetype)initWithServiceName:(NSString *)serviceName
                        serviceType:(NSString *)serviceType
                               port:(NSInteger)port
                      TXTRecordData:(NSDictionary<NSString *, NSData *> *)TXTRecordData
           endpointConnectedHandler:(GNCMConnectionHandler)handler;

@end

NS_ASSUME_NONNULL_END
