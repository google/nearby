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
 * GNCMBonjourBrowser browses mDNS services publishing the specified mDNS type and domain. The mDNS
 * type is a string formatted as "_[serviceIdHash]._tcp." in which [serviceIdHash] is generated as
 * a SHA-256 hash from the service ID and taken the 6 first bytes of string in upper case.
 * Calls the specififed endpoint found handler when one is found. The endpoint found handler
 * supplies a requester block, which can be called to establish a socket to the service found.
 *
 * Don't hold the strong reference of caller self in endpointFoundHandler to ensure there is no
 * retain cycle between them.
 *
 * @param serviceType An mDNS type that uniquely identifies the published service to search for.
 * @param endpointFoundHandler The handler that is called when an endpoint publishing the service
 *                             ID is discovered.
 */
@interface GNCMBonjourBrowser : NSObject

- (instancetype)initWithServiceType:(NSString *)serviceType
               endpointFoundHandler:(GNCMEndpointFoundHandler)handler;

@end

NS_ASSUME_NONNULL_END
