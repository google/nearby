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

#include "connections/payload.h"
#include "internal/platform/payload_id.h"

#include <stdlib.h>

using ::nearby::PayloadId;
using ::nearby::connections::Payload;

NS_ASSUME_NONNULL_BEGIN

PayloadId GenerateId() {
  return Payload::GenerateId();
}

@implementation GNCBytesPayload

- (instancetype)initWithBytes:(NSData *)bytes identifier:(int64_t)identifier {
  self = [super init];
  if (self) {
    _identifier = identifier;
    _bytes = bytes;
  }
  return self;
}

+ (instancetype)payloadWithBytes:(NSData *)bytes {
  return [[self alloc] initWithBytes:bytes identifier:GenerateId()];
}

+ (instancetype)payloadWithBytes:(NSData *)bytes identifier:(int64_t)identifier {
  return [[self alloc] initWithBytes:bytes identifier:identifier];
}

@end

@implementation GNCStreamPayload

- (instancetype)initWithStream:(NSInputStream *)stream identifier:(int64_t)identifier {
  self = [super init];
  if (self) {
    _identifier = identifier;
    _stream = stream;
  }
  return self;
}

+ (instancetype)payloadWithStream:(NSInputStream *)stream {
  return [[self alloc] initWithStream:stream identifier:GenerateId()];
}

+ (instancetype)payloadWithStream:(NSInputStream *)stream identifier:(int64_t)identifier {
  return [[self alloc] initWithStream:stream identifier:identifier];
}

@end

@implementation GNCFilePayload

- (instancetype)initWithFileURL:(NSURL *)fileURL identifier:(int64_t)identifier {
  self = [super init];
  if (self) {
    _identifier = identifier;
    _fileURL = [fileURL copy];
  }
  return self;
}

+ (instancetype)payloadWithFileURL:(NSURL *)fileURL {
  return [[self alloc] initWithFileURL:fileURL identifier:GenerateId()];
}

+ (instancetype)payloadWithFileURL:(NSURL *)fileURL identifier:(int64_t)identifier {
  return [[self alloc] initWithFileURL:fileURL identifier:identifier];
}

@end

NS_ASSUME_NONNULL_END
