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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"

@implementation GNCFakeNWFrameworkSocket

- (instancetype)initWithConnection:(id<GNCNWConnection>)connection {
  self = [super initWithConnection:connection];
  if (self) {
    _writtenData = [NSMutableData data];
  }
  return self;
}

- (nullable NSData *)readMaxLength:(NSUInteger)length error:(NSError **)error {
  if (self.readError) {
    if (error) {
      *error = self.readError;
    }
    return nil;
  }
  if (self.dataToRead) {
    NSData *data = self.dataToRead;
    self.dataToRead = nil;
    if (data.length > length) {
      self.dataToRead = [data subdataWithRange:NSMakeRange(length, data.length - length)];
      return [data subdataWithRange:NSMakeRange(0, length)];
    }
    return data;
  }
  return [NSData data];
}

- (std::optional<std::string>)readStringWithMaxLength:(NSUInteger)length error:(NSError **)error {
  if (self.readError) {
    if (error) *error = self.readError;
    return std::nullopt;
  }
  if (self.dataToRead) {
    NSData *data = self.dataToRead;
    self.dataToRead = nil;
    NSUInteger actualLength = MIN(length, data.length);
    if (data.length > actualLength) {
      self.dataToRead =
          [data subdataWithRange:NSMakeRange(actualLength, data.length - actualLength)];
    }
    NSData *returnData = [data subdataWithRange:NSMakeRange(0, actualLength)];
    return std::string((const char *)returnData.bytes, returnData.length);
  }
  return std::string();
}

- (BOOL)write:(NSData *)data error:(NSError **)error {
  if (self.writeError) {
    if (error) {
      *error = self.writeError;
    }
    return NO;
  }
  [self.writtenData appendData:data];
  return YES;
}

- (BOOL)writeBytes:(const void *)bytes length:(NSUInteger)length error:(NSError **)error {
  if (self.writeError) {
    if (error) {
      *error = self.writeError;
    }
    return NO;
  }
  [self.writtenData appendBytes:bytes length:length];
  return YES;
}

- (void)close {
  self.isClosed = YES;
}

@end
