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

#import "connections/clients/ios/Public/NearbyConnections/GNCPayload.h"

NS_ASSUME_NONNULL_BEGIN

/** This category adds the ability to specify a payload ID. */
@interface GNCBytesPayload (Internal)
+ (instancetype)payloadWithBytes:(NSData *)bytes identifier:(int64_t)identifier;
@end

/** This category adds the ability to specify a payload ID. */
@interface GNCStreamPayload (Internal)
+ (instancetype)payloadWithStream:(NSInputStream *)stream identifier:(int64_t)identifier;
@end

/** This category adds the ability to specify a payload ID. */
@interface GNCFilePayload (Internal)
+ (instancetype)payloadWithFileURL:(NSURL *)fileURL identifier:(int64_t)identifier;
@end

NS_ASSUME_NONNULL_END
