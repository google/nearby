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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#include <optional>
#include <string>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"

static const NSTimeInterval kConnectionWriteTimeout = 5.0;  // 5 seconds timeout

@interface GNCNWFrameworkSocket ()

@property(nonatomic, readonly) id<GNCNWConnection> connection;

@end

@implementation GNCNWFrameworkSocket {
}

- (instancetype)initWithConnection:(nonnull id<GNCNWConnection>)connection {
  self = [super init];
  if (self) {
    _connection = connection;
  }
  return self;
}

- (NSData *)readMaxLength:(NSUInteger)length error:(NSError **)error {
  dispatch_data_t content = [self receiveDataWithLength:length error:error];
#if __LP64__
  // This cast is only safe in a 64-bit runtime.
  return [(NSData *)content copy];
#else
  return nil;
#endif
}

- (std::optional<std::string>)readStringWithMaxLength:(NSUInteger)length error:(NSError **)error {
  dispatch_data_t content = [self receiveDataWithLength:length error:error];
  if (content == nil) {
    return std::nullopt;
  }

  __block std::string resultString;
  resultString.reserve(dispatch_data_get_size(content));
  dispatch_data_apply(
      content, ^bool(dispatch_data_t region, size_t offset, const void *buffer, size_t size) {
        resultString.append(static_cast<const char *>(buffer), size);
        return true;
      });
  return std::move(resultString);
}

- (BOOL)write:(NSData *)data error:(NSError **)error {
  __block NSData *blockData = [data copy];
  dispatch_data_t dispatchData =
      dispatch_data_create(blockData.bytes, blockData.length, dispatch_get_main_queue(), ^{
        // One would assume that since `write` blocks execution until the send is complete, that
        // `data` would stay alive when needed and released at the end of the method. However, that
        // doesn't seem to be the case. We must keep a reference to `blockData` in this callback to
        // keep it alive until `dispatchData` is done being used.
        // See: b/280525159
        blockData = nil;
      });
  return [self sendData:dispatchData error:error];
}

- (void)close {
  [_connection cancel];
  _connection = nil;
}

// MARK: - Private Methods

- (nullable dispatch_data_t)receiveDataWithLength:(NSUInteger)length error:(NSError **)error {
  id<GNCNWConnection> connection = self.connection;
  if (!connection) {
    if (error) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorNotConnected
                               userInfo:nil];
    }
    return nil;
  }

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block dispatch_data_t resultData = nil;
  __block NSError *blockError = nil;

  // Min and max length are set to the same value to indicate that we expect to receive a single
  // message of the given length. Completion is only called once and only when the received message
  // is at least min length. The returned data will be at most max length.
  [connection receiveMessageWithMinLength:static_cast<uint32_t>(length)
                                maxLength:static_cast<uint32_t>(length)
                        completionHandler:^(dispatch_data_t _Nullable content,
                                            nw_content_context_t _Nullable context, bool isComplete,
                                            nw_error_t _Nullable receiveError) {
                          if (receiveError) {
                            blockError =
                                (__bridge_transfer NSError *)nw_error_copy_cf_error(receiveError);
                          }
                          resultData = content;
                          dispatch_semaphore_signal(semaphore);
                        }];

  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  if (error) {
    *error = blockError;
  }
  return resultData;
}

- (BOOL)sendData:(dispatch_data_t)data error:(NSError **)error {
  id<GNCNWConnection> connection = self.connection;
  if (!connection) {
    if (error) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorNotConnected
                               userInfo:nil];
    }
    return NO;
  }

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

  __block NSError *blockError = nil;

  // TODO(bourdakos): Investigate if the context should be NW_CONNECTION_DEFAULT_STREAM_CONTEXT.
  [connection sendData:data
                context:NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT
             isComplete:NO
      completionHandler:^(nw_error_t _Nullable sendError) {
        if (sendError) {
          blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(sendError);
        }
        dispatch_semaphore_signal(semaphore);
      }];

  // Wait until signaled or the 5-second timeout passes
  intptr_t waitResult = dispatch_semaphore_wait(
      semaphore, dispatch_time(DISPATCH_TIME_NOW,
                               static_cast<int64_t>(kConnectionWriteTimeout * NSEC_PER_SEC)));
  if (waitResult != 0) {
    if (error != nil) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorTimedOut
                               userInfo:nil];
    }
    return NO;
  }

  if (error != nil) {
    *error = blockError;
  }
  return blockError == nil;
}

@end
