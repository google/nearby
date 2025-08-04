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

#import "internal/platform/implementation/apple/Mediums/BLE/NSData+GNCBase85.h"

#import <Foundation/Foundation.h>

#include <optional>
#include <string>

#include "internal/encoding/base85.h"

#import "internal/platform/implementation/apple/Log/GNCLogger.h"

@implementation NSData (GNCBase85)

- (NSString *)base85EncodedString {
  const char *data = static_cast<const char *>(self.bytes);
  NSUInteger length = self.length;
  std::optional<std::string> base85String =
      nearby::encoding::Base85Encode(std::string(data, length));
  if (!base85String.has_value()) {
    return @"";
  }

  return @(base85String->c_str());
}

- (nullable instancetype)initWithBase85EncodedString:(NSString *)base85String {
  std::optional<std::string> decodedData =
      nearby::encoding::Base85Decode(std::string(base85String.UTF8String));
  if (!decodedData.has_value()) {
    return nil;
  }
  return [NSData dataWithBytes:decodedData->data() length:decodedData->size()];
}

@end
