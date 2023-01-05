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

#import <XCTest/XCTest.h>

#include <string>

#include "internal/platform/implementation/crypto.h"
#include "internal/platform/byte_array.h"

@interface GNCCryptoTest : XCTestCase
@end

@implementation GNCCryptoTest

// Tests that the hash functions return the expected values.
- (void)testHashValues {
  std::string string("com.google.location.nearby");

  // SHA-256 test.
  {
    const uint8_t sha256ExpectedHash[] = {
        0x09, 0x8e, 0x3c, 0x54, 0x2d, 0xee, 0x9e, 0x35,
        0x1d, 0xe7, 0x5b, 0xcf, 0xda, 0xb5, 0x62, 0xd3,
        0xde, 0xba, 0x14, 0x49, 0xbd, 0xf5, 0x95, 0x04,
        0x1f, 0x1d, 0x99, 0x84, 0x87, 0xc3, 0xcb, 0x8a, };
    nearby::ByteArray sha256Hash = nearby::Crypto::Sha256(string);
    XCTAssert(sha256Hash.size() == sizeof(sha256ExpectedHash) &&
              memcmp(sha256Hash.data(), sha256ExpectedHash, sizeof(sha256ExpectedHash)) == 0);
  }

  // MD5 test.
  {
    const uint8_t md5ExpectedHash[] = {
        0x97, 0x78, 0x1d, 0x3d, 0xee, 0xd7, 0xdc, 0x5a,
        0x6e, 0xee, 0x50, 0x08, 0xce, 0xd1, 0xb2, 0xe8 };
    nearby::ByteArray md5Hash = nearby::Crypto::Md5(string);
    XCTAssert(md5Hash.size() == sizeof(md5ExpectedHash) &&
              memcmp(md5Hash.data(), md5ExpectedHash, sizeof(md5ExpectedHash)) == 0);
  }
}

@end
