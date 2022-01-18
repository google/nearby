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

NS_ASSUME_NONNULL_BEGIN

/** This class encapsulates a Bytes payload. */
@interface GNCBytesPayload : NSObject

/** The unique identifier of the payload. */
@property(nonatomic, readonly) int64_t identifier;

/** The content of the payload. */
@property(nonatomic, readonly) NSData *bytes;

/**
 * Creates a Bytes payload object.
 * Note: To maximize performance, @c bytes is strongly referenced, not copied.
 */
+ (instancetype)payloadWithBytes:(NSData *)bytes;

@end

/** This class encapsulates a Stream payload. */
@interface GNCStreamPayload : NSObject

/** The unique identifier of the payload. */
@property(nonatomic, readonly) int64_t identifier;

/** The payload data is read from this input stream. */
@property(nonatomic, readonly) NSInputStream *stream;

+ (instancetype)payloadWithStream:(NSInputStream *)stream;

@end

/** This class encapsulates a File payload. */
@interface GNCFilePayload : NSObject

/** The unique identifier of the payload. */
@property(nonatomic, readonly) int64_t identifier;

/** A URL that identifies the file. */
@property(nonatomic, readonly, copy) NSURL *fileURL;

+ (instancetype)payloadWithFileURL:(NSURL *)fileURL;

+ (instancetype)payloadWithFileURL:(NSURL *)fileURL identifier:(int64_t)identifier;

@end

NS_ASSUME_NONNULL_END
