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

#ifdef __cplusplus

#import <Foundation/Foundation.h>

#import "connections/clients/ios/Internal/GNCCore.h"
#import "connections/clients/ios/Public/NearbyConnections/GNCConnection.h"

NS_ASSUME_NONNULL_BEGIN

/** This holds the progress and completion for a pending payload. */
@interface GNCPayloadInfo : NSObject
@property(nonatomic, nullable) NSProgress *progress;
@property(nonatomic, nullable) GNCPayloadResultHandler completion;

+ (instancetype)infoWithProgress:(nullable NSProgress *)progress
                      completion:(GNCPayloadResultHandler)completion;
- (void)callCompletion:(GNCPayloadResult)result;
@end

/** GNCConnection that interfaces with the Core library. */
@interface GNCCoreConnection : NSObject <GNCConnection>
@property(nonatomic) GNCCore *core;
@property(nonatomic, copy) GNCEndpointId endpointId;
@property(nonatomic) dispatch_block_t deallocHandler;
@property(nonatomic) NSMutableDictionary<NSNumber *, GNCPayloadInfo *> *payloads;

+ (instancetype)connectionWithEndpointId:(GNCEndpointId)endpointId
                                    core:(GNCCore *)core
                          deallocHandler:(dispatch_block_t)deallocHandler;
@end

NS_ASSUME_NONNULL_END

#endif
