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
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * A protocol to wrap Network.framework C functions to allow for faking in tests.
 */
@protocol GNCNWBrowseResult <NSObject>

/**
 * Enumerates the interfaces associated with a browse result.
 *
 * @param browseResult The browse result to enumerate.
 * @param block The block to apply to each interface.
 */
- (void)enumerateInterfaces:(nw_browse_result_t)browseResult
                 usingBlock:(nw_browse_result_enumerate_interface_t)block;

/**
 * Returns the type of an interface, such as Wi-Fi or cellular.
 *
 * @param interface The interface to get the type of.
 * @return The type of the interface.
 */
- (nw_interface_type_t)interfaceGetType:(nw_interface_t)interface;

/**
 * Copies the TXT record associated with a browse result.
 *
 * @param browseResult The browse result to copy the TXT record from.
 * @return The TXT record object, or nil if one doesn't exist.
 */
- (nullable nw_txt_record_t)copyTXTRecordObject:(nw_browse_result_t)browseResult;

/**
 * Allows you to inspect the keys and values in a TXT record.
 *
 * @param txtRecord The TXT record to apply the block to.
 * @param block The block to apply to each key-value pair in the TXT record.
 */
- (void)applyTXTRecord:(nw_txt_record_t)txtRecord block:(nw_txt_record_applier_t)block;

/**
 * Compares two browse results to determine if they refer to the same endpoint and if any specific
 * attributes of the endpoint have changed.
 *
 * @param oldResult The older browse result.
 * @param newResult The newer browse result.
 * @return The changes between the two browse results.
 */
- (nw_browse_result_change_t)getChangesFrom:(nw_browse_result_t)oldResult
                                         to:(nw_browse_result_t)newResult;

/**
 * Gets the endpoint associated with a browse result.
 *
 * @param result The browse result to get the endpoint from.
 * @return The endpoint associated with the browse result, or nil if one doesn't exist.
 */
- (nullable nw_endpoint_t)copyEndpointFromResult:(nw_browse_result_t)result;

/**
 * Returns the service name if the endpoint is a Bonjour service endpoint.
 *
 * @param endpoint The endpoint to get the Bonjour service name from.
 * @return The Bonjour service name, or nil if the endpoint is not a Bonjour service endpoint.
 */
- (nullable NSString *)getBonjourServiceNameFromEndpoint:(nw_endpoint_t)endpoint;

@end

NS_ASSUME_NONNULL_END
