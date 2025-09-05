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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFrameworkSocket.h"

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
    return data;
  }
  return [NSData data];
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

- (void)close {
  self.isClosed = YES;
}

@end
