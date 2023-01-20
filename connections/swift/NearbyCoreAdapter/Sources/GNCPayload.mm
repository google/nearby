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

#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCPayload.h"

#import <Foundation/Foundation.h>

#include <string>

#include "connections/payload.h"

#import "connections/swift/NearbyCoreAdapter/Sources/CPPInputStreamBinding.h"

using ::nearby::connections::Payload;

@implementation GNCPayload

+ (int64_t)generateID {
  return Payload::GenerateId();
}

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

@synthesize totalSize = _totalSize;

- (NSNumber *)totalSize {
  if (!_totalSize) {
    NSNumber *fileSize;
    BOOL result = [_fileURL getResourceValue:&fileSize forKey:NSURLFileSizeKey error:nil];
    if (!result) {
      fileSize = [NSNumber numberWithInt:-1];
    }
    _totalSize = fileSize;
  }
  return _totalSize;
}

- (instancetype)initWithFileURL:(NSURL *)fileURL
                   parentFolder:(NSString *)parentFolder
                       fileName:(NSString *)fileName
                     identifier:(int64_t)identifier {
  self = [super initWithIdentifier:identifier];
  if (self) {
    _fileURL = [fileURL copy];
    _parentFolder = [parentFolder copy];
    _fileName = [fileName copy];
  }
  return self;
}

- (instancetype)initWithFileURL:(NSURL *)fileURL
                   parentFolder:(NSString *)parentFolder
                       fileName:(NSString *)fileName {
  return [self initWithFileURL:fileURL
                  parentFolder:parentFolder
                      fileName:fileName
                    identifier:Payload::GenerateId()];
}

@end
