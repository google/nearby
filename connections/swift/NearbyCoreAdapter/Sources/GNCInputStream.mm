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

#import "connections/swift/NearbyCoreAdapter/Sources/GNCInputStream.h"

#import <Foundation/Foundation.h>

#include <algorithm>

#include "connections/payload.h"

#import "connections/swift/NearbyCoreAdapter/Sources/GNCException+Internal.h"
#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCException.h"

using ::nearby::ByteArray;
using ::nearby::ExceptionOr;
using ::nearby::connections::NSErrorFromCppException;
using ::nearby::connections::Payload;

@implementation GNCInputStream {
  NSStreamStatus _streamStatus;
  NSError *_streamError;
  id<NSStreamDelegate> _delegate;
  Payload _payload;
}

- (instancetype)initWithPayload:(Payload)payload {
  // Init with empty data because init is not a designated initializer.
  self = [super initWithData:[[NSData alloc] init]];
  if (self) {
    _streamStatus = NSStreamStatusNotOpen;
    _delegate = self;
    _payload = std::move(payload);
  }

  return self;
}

- (BOOL)hasBytesAvailable {
  return _streamStatus == NSStreamStatusOpen;
}

- (NSInteger)read:(uint8_t *)buffer maxLength:(NSUInteger)maxLen {
  ExceptionOr<ByteArray> readResult = _payload.AsStream()->Read(maxLen);

  if (!readResult.ok()) {
    _streamError = NSErrorFromCppException(readResult.GetException());
    _streamStatus = NSStreamStatusError;
    return -1;
  }

  ByteArray byteArray = readResult.GetResult();
  if (byteArray.size() == 0) {
    _streamStatus = NSStreamStatusAtEnd;
    return 0;
  }

  memcpy(buffer, byteArray.data(), std::min(maxLen, byteArray.size()));
  return byteArray.size();
}

- (BOOL)getBuffer:(uint8_t **)buffer length:(NSUInteger *)length {
  return NO;
}

- (void)scheduleInRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode {
}

- (void)removeFromRunLoop:(NSRunLoop *)runLoop forMode:(NSString *)mode {
}

- (void)open {
  _streamStatus = NSStreamStatusOpen;
}

- (void)close {
  _streamStatus = NSStreamStatusClosed;
  _payload.AsStream()->Close();
}

- (NSStreamStatus)streamStatus {
  return _streamStatus;
}

- (NSError *)streamError {
  return _streamError;
}

- (id<NSStreamDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id<NSStreamDelegate>)delegate {
  // By default, a stream is its own delegate, and subclasses of NSInputStream and NSOutputStream
  // must maintain this contract. Since we override this method in our subclass, we must
  // restore the receiver as its own delegate when passed nil.
  if (delegate) {
    _delegate = delegate;
  } else {
    _delegate = self;
  }
}

- (id)propertyForKey:(NSStreamPropertyKey)key {
  return nil;
}

- (BOOL)setProperty:(id)property forKey:(NSStreamPropertyKey)key {
  return NO;
}

@end