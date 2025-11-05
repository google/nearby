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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowseResult.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

NS_ASSUME_NONNULL_BEGIN

// Fake interface for testing
@interface GNCFakeNWInterface : NSObject <OS_nw_interface>
@property(nonatomic) nw_interface_type_t type;
@end

@interface GNCFakeNWBrowseResult : NSObject <GNCNWBrowseResult>
@property(nonatomic, copy) NSArray<GNCFakeNWInterface *> *interfaces;
@property(nonatomic, nullable) NSDictionary<NSString *, NSString *> *txtRecord;

@property(nonatomic) nw_browse_result_change_t getChangesFromResult;
@property(nonatomic, nullable) nw_endpoint_t endpointFromResultResult;
@property(nonatomic, nullable) NSString *getBonjourServiceNameFromEndpointResult;

@end

NS_ASSUME_NONNULL_END
