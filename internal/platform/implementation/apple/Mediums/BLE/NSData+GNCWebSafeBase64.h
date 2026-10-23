// Copyright 2023 Google LLC
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

@interface NSData (GNCWebSafeBase64)

/** Creates a Base64 encoded string from the data using websafe characters and no padding. */
- (NSString *)webSafeBase64EncodedString;

/**
 * Initializes a data object with the given Base64 encoded string.
 *
 * @param base64String A Base64 encoded string.
 * @return A data object built by Base64 decoding the provided string. Returns @c nil if the data
 *         object could not be decoded.
 */
- (nullable instancetype)initWithWebSafeBase64EncodedString:(NSString *)base64String;

@end

NS_ASSUME_NONNULL_END
