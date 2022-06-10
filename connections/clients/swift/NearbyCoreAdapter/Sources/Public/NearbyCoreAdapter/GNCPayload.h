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
 * A @c GNCPayload object represents the data being sent to or received from a remote device.
 */
@interface GNCPayload : NSObject

/**
 * The unique identifier for this payload.
 */
@property(nonatomic, readonly) int64_t identifier;

/**
 * @remark init is not an available initializer.
 */
- (nonnull instancetype)init NS_UNAVAILABLE;

@end

/**
 * A @c GNCBytesPayload object represents a payload that holds simple bytes.
 */
@interface GNCBytesPayload : GNCPayload

/**
 * The byte data for this payload.
 */
@property(nonnull, nonatomic, readonly, copy) NSData *data;

/**
 * Creates a bytes payload object with identifier.
 *
 * @param data An instance containing the message to send.
 * @param identifier A unique identifier for the payload.
 *
 * @return The initialized payload object, or nil if an error occurs.
 */
- (nonnull instancetype)initWithData:(nonnull NSData *)data
                          identifier:(int64_t)identifier NS_DESIGNATED_INITIALIZER;

/**
 * Creates a bytes payload object.
 *
 * @param data An instance containing the message to send.
 * @return The initialized payload object, or nil if an error occurs.
 */
- (nonnull instancetype)initWithData:(nonnull NSData *)data;

@end

/**
 * A @c GNCStreamPayload object represents a payload holds an open stream that can be read from by
 * remote endpoints.
 */
@interface GNCStreamPayload : GNCPayload

/**
 * The open stream for this payload.
 */
@property(nonnull, nonatomic, readonly) NSInputStream *stream;

/**
 * Creates a stream payload object with identifier.
 *
 * @param stream An input stream instance.
 * @param identifier A unique identifier for the payload.
 *
 * @return The initialized payload object, or nil if an error occurs.
 */
- (nonnull instancetype)initWithStream:(nonnull NSInputStream *)stream
                            identifier:(int64_t)identifier NS_DESIGNATED_INITIALIZER;

/**
 * Creates a stream payload object.
 *
 * @param stream An input stream instance.
 *
 * @return The initialized payload object, or nil if an error occurs.
 */
- (nonnull instancetype)initWithStream:(nonnull NSInputStream *)stream;

@end

/**
 * A @c GNCFilePayload object represents a payload that holds a file.
 */
@interface GNCFilePayload : GNCPayload

/**
 * The URL of the file associated with this payload.
 */
@property(nonnull, nonatomic, readonly, copy) NSURL *fileURL;

/**
 * Creates a file payload object with identifier.
 *
 * @param fileURL A file URL.
 * @param identifier A unique identifier for the payload.
 *
 * @return The initialized payload object, or nil if an error occurs.
 */
- (nonnull instancetype)initWithFileURL:(nonnull NSURL *)fileURL
                             identifier:(int64_t)identifier NS_DESIGNATED_INITIALIZER;

/**
 * Creates a file payload object.
 *
 * @param fileURL A file URL.
 *
 * @return The initialized payload object, or nil if an error occurs.
 */
- (nonnull instancetype)initWithFileURL:(nonnull NSURL *)fileURL;

@end
