// Copyright 2026 Google LLC
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

#include <array>
#include <string>

#include "internal/crypto_cros/hmac.h"
#include "internal/crypto_cros/secure_hash.h"
#include "internal/crypto_cros/sha2.h"
// Include the crypto API alongside Nearby's similarly named identifiers.
#include <openssl/sha.h>
#include <openssl/hmac.h>

@interface CryptoConsumerTests : XCTestCase
@end

@implementation CryptoConsumerTests

- (void)testSHA256KnownVector {
  const std::array<uint8_t, 32> expected = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
      0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
      0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
      0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  const std::string digest = nearby::crypto::SHA256HashString("abc");
  XCTAssertTrue(digest == std::string(reinterpret_cast<const char *>(expected.data()),
                                    expected.size()));

  auto hash = nearby::crypto::SecureHash::Create(nearby::crypto::SecureHash::SHA256);
  XCTAssertTrue(hash != nullptr);
  if (!hash) return;
  hash->Update("a", 1);
  hash->Update("bc", 2);
  std::array<uint8_t, 32> incremental;
  hash->Finish(incremental.data(), incremental.size());
  XCTAssertTrue(incremental == expected);
}

- (void)testHMACSHA256KnownVector {
  const std::array<uint8_t, 32> expected = {
      0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
      0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
      0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
      0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
  nearby::crypto::HMAC hmac(nearby::crypto::HMAC::SHA256);
  XCTAssertTrue(hmac.Init(std::string(20, '\x0b')));
  std::array<uint8_t, 32> digest{};
  XCTAssertTrue(hmac.Sign("Hi There", digest.data(), digest.size()));
  XCTAssertTrue(digest == expected);
}

@end
