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

- (void)enumerateInterfaces:(nw_browse_result_t)browseResult
                 usingBlock:(nw_browse_result_enumerate_interface_t)block;

- (nw_interface_type_t)interfaceGetType:(nw_interface_t)interface;

- (nullable nw_txt_record_t)copyTXTRecordObject:(nw_browse_result_t)browseResult;

- (void)applyTXTRecord:(nw_txt_record_t)txtRecord block:(nw_txt_record_applier_t)block;

- (nw_browse_result_change_t)getChangesFrom:(nw_browse_result_t)oldResult
                                         to:(nw_browse_result_t)newResult;

- (nullable nw_endpoint_t)copyEndpointFromResult:(nw_browse_result_t)result;

- (nullable NSString *)getBonjourServiceNameFromEndpoint:(nw_endpoint_t)endpoint;

@end

NS_ASSUME_NONNULL_END
