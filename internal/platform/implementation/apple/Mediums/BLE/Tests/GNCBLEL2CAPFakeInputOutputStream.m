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

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"

@implementation GNCBLEL2CAPFakeInputOutputStream {
  /// Output stream for |_inputStream| to "simulate" data from watch during tests.
  NSOutputStream* _outputTestStream;

  /// Input stream for |_outputStream| to "simulate" data to watch during tests.
  NSInputStream* _inputTestStream;
}

#pragma mark Public

- (instancetype)initWithBufferSize:(NSUInteger)bufferSize {
  self = [super init];
  if (self) {
    [self setupStreamsWithBufferSize:bufferSize];
  }
  return self;
}

- (void)tearDown {
  [_inputTestStream close];
  [_outputTestStream close];

  _inputStream = nil;
  _outputStream = nil;
  _inputTestStream = nil;
  _outputTestStream = nil;
}

- (void)writeFromDevice:(NSData*)data {
  [_outputTestStream write:data.bytes maxLength:data.length];
}

- (NSData*)dataSentToWatchWithMaxBytes:(NSUInteger)maxBytes {
  NSMutableData* readData = [NSMutableData data];
  uint8_t buf[409600];
  NSUInteger totalBytesRead = 0u;

  while (totalBytesRead < maxBytes) {
    NSInteger bytesRead = [_inputTestStream read:buf maxLength:409600];
    [readData appendBytes:buf length:(NSUInteger)bytesRead];
    totalBytesRead += (NSUInteger)bytesRead;
  }
  return readData;
}

#pragma mark Private

/// Opens a connection to |stream| on the main run loop.
+ (void)openStream:(NSStream*)stream {
  [stream scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
  [stream open];
}

/// Sets up the stream instance variables with the given |bufferSize|.
- (void)setupStreamsWithBufferSize:(NSUInteger)bufferSize {
  {
    // Local variables created to avoid error in passing pointer of non-local object.
    NSInputStream* inputStream;
    NSOutputStream* outputStream;

    [NSStream getBoundStreamsWithBufferSize:bufferSize
                                inputStream:&inputStream
                               outputStream:&outputStream];

    _inputStream = inputStream;
    _outputTestStream = outputStream;
  }

  {
    // Local variables created to avoid error in passing pointer of non-local object.
    NSInputStream* inputStream;
    NSOutputStream* outputStream;
    [NSStream getBoundStreamsWithBufferSize:bufferSize
                                inputStream:&inputStream
                               outputStream:&outputStream];

    _inputTestStream = inputStream;
    _outputStream = outputStream;
  }

  [[self class] openStream:_inputTestStream];
  [[self class] openStream:_outputTestStream];
}

@end
