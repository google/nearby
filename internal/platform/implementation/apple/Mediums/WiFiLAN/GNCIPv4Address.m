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

#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCIPv4Address.h"

@implementation GNCIPv4Address

- (instancetype)initWithByte1:(uint8_t)byte1
                        byte2:(uint8_t)byte2
                        byte3:(uint8_t)byte3
                        byte4:(uint8_t)byte4 {
  self = [super init];
  if (self) {
    _byte1 = byte1;
    _byte2 = byte2;
    _byte3 = byte3;
    _byte4 = byte4;
  }
  return self;
}

+ (instancetype)addressFromFourByteInt:(uint32_t)address {
  uint8_t byte1 = (address >> (8 * 0)) & 0xff;
  uint8_t byte2 = (address >> (8 * 1)) & 0xff;
  uint8_t byte3 = (address >> (8 * 2)) & 0xff;
  uint8_t byte4 = (address >> (8 * 3)) & 0xff;
  return [[GNCIPv4Address alloc] initWithByte1:byte1 byte2:byte2 byte3:byte3 byte4:byte4];
}

+ (instancetype)addressFromData:(NSData *)address {
  NSAssert(address.length == 4, @"Address must be 4 bytes");
  const uint8_t *bytes = address.bytes;
  return [[GNCIPv4Address alloc] initWithByte1:bytes[0]
                                         byte2:bytes[1]
                                         byte3:bytes[2]
                                         byte4:bytes[3]];
}

- (NSData *)binaryRepresentation {
  const uint8_t bytes[] = {_byte1, _byte2, _byte3, _byte4};
  return [NSData dataWithBytes:bytes length:4];
}

- (NSString *)dottedRepresentation {
  return [NSString stringWithFormat:@"%d.%d.%d.%d", _byte1, _byte2, _byte3, _byte4];
}

@end
