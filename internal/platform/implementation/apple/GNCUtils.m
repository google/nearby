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

#import "internal/platform/implementation/apple/GNCUtils.h"

#import <CommonCrypto/CommonDigest.h>

NS_ASSUME_NONNULL_BEGIN

const int GNCSocketVersion = 2;

NSData *GNCSha256Data(NSData *data) {
  unsigned char output[CC_SHA256_DIGEST_LENGTH];
  CC_LONG length = (CC_LONG)data.length;
  if (!CC_SHA256(data.bytes, length, output)) return nil;
  return [NSData dataWithBytes:output length:CC_SHA256_DIGEST_LENGTH];
}

NSData *GNCSha256String(NSString *string) {
  return GNCSha256Data([string dataUsingEncoding:NSUTF8StringEncoding]);
}

NSData *GNCMd5Data(NSData *data) {
  unsigned char md5Buffer[CC_MD5_DIGEST_LENGTH];
  CC_MD5(data.bytes, (CC_LONG)data.length, md5Buffer);
  return [NSData dataWithBytes:md5Buffer length:CC_MD5_DIGEST_LENGTH];
}

NSData *GNCMd5String(NSString *string) {
  return GNCMd5Data([string dataUsingEncoding:NSUTF8StringEncoding]);
}

NS_ASSUME_NONNULL_END
