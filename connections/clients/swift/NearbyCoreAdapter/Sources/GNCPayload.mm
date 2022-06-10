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

#import "connections/clients/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCPayload.h"

#import <Foundation/Foundation.h>

#include <string>

#include "connections/payload.h"

#import "connections/clients/swift/NearbyCoreAdapter/Sources/CPPInputStreamBinding.h"

using ::location::nearby::connections::Payload;

@implementation GNCPayload

- (instancetype)initWithIdentifier:(int64_t)identifier {
  self = [super init];
  if (self) {
    _identifier = identifier;
  }
  return self;
}

@end

@implementation GNCBytesPayload

- (instancetype)initWithData:(NSData *)data identifier:(int64_t)identifier {
  self = [super initWithIdentifier:identifier];
  if (self) {
    _data = [data copy];
  }
  return self;
}

- (instancetype)initWithData:(NSData *)data {
  return [self initWithData:data identifier:Payload::GenerateId()];
}

@end

@implementation GNCStreamPayload

- (instancetype)initWithStream:(NSInputStream *)stream identifier:(int64_t)identifier {
  self = [super initWithIdentifier:identifier];
  if (self) {
    _stream = stream;
    [CPPInputStreamBinding bindToStream:stream];
  }
  return self;
}

- (instancetype)initWithStream:(NSInputStream *)stream {
  return [self initWithStream:stream identifier:Payload::GenerateId()];
}

@end

@implementation GNCFilePayload

- (instancetype)initWithFileURL:(NSURL *)fileURL identifier:(int64_t)identifier {
  self = [super initWithIdentifier:identifier];
  if (self) {
    _fileURL = [fileURL copy];
  }
  return self;
}

- (instancetype)initWithFileURL:(NSURL *)fileURL {
  return [self initWithFileURL:fileURL identifier:Payload::GenerateId()];
}

@end
