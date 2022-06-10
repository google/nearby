// Copyright 2022 Google LLC
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

/**
 * The @c GNCDiscoveryDelegate protocol defines methods that a delegate can implement to handle
 * discovery-related events.
 */
@protocol GNCDiscoveryDelegate

/**
 * Called when a remote endpoint is discovered.
 *
 * @param endpointID The ID of the remote endpoint that was discovered.
 * @param info The info of the remote endpoint represented by bytes.
 */
- (void)foundEndpoint:(nonnull NSString *)endpointID withEndpointInfo:(nonnull NSData *)info;

/**
 * Called when a remote endpoint is no longer discoverable.
 *
 * @param endpointID The ID of the remote endpoint that was lost.
 */
- (void)lostEndpoint:(nonnull NSString *)endpointID;

@end
