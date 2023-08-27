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

#import "internal/platform/implementation/apple/Mediums/BLEv2/NSData+GNCWebSafeBase64.h"

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@implementation NSData (GNCWebSafeBase64)

- (NSString *)webSafeBase64EncodedString {
  NSString *encoded = [self base64EncodedStringWithOptions:0];

  // Convert the standard base64 characters to URL safe variants.
  encoded = [encoded stringByReplacingOccurrencesOfString:@"+" withString:@"-"];
  encoded = [encoded stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
  encoded = [encoded stringByReplacingOccurrencesOfString:@"=" withString:@""];

  return encoded;
}

- (nullable instancetype)initWithWebSafeBase64EncodedString:(NSString *)base64String {
  // Convert the URL safe base64 characters to the standard variants.
  base64String = [base64String stringByReplacingOccurrencesOfString:@"-" withString:@"+"];
  base64String = [base64String stringByReplacingOccurrencesOfString:@"_" withString:@"/"];

  // @c initWithBase64EncodedString:options: requires a padded base64 string. Append enough "="
  // characters to make the string a multiple of 4.
  NSUInteger paddedLength = base64String.length + ((4 - (base64String.length % 4)) % 4);
  base64String = [base64String stringByPaddingToLength:paddedLength
                                            withString:@"="
                                       startingAtIndex:0];
  return [self initWithBase64EncodedString:base64String options:0];
}

@end

NS_ASSUME_NONNULL_END
