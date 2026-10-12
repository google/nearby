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

- (BOOL)sendDispatchData:(dispatch_data_t)data
             isComplete:(BOOL)isComplete
                  error:(NSError **)error;

@end

@implementation GNCNWFrameworkSocket {
}

@synthesize connection = _connection;

- (id<GNCNWConnection>)connection {
  @synchronized(self) {
    return _connection;
  }
}

- (instancetype)initWithConnection:(nonnull id<GNCNWConnection>)connection {
  self = [super init];
  if (self) {
    _connection = connection;
  }
  return self;
}

- (NSData *)readMaxLength:(NSUInteger)length error:(NSError **)error {
  if (!self.connection) {
    if (error) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorUnknown
                               userInfo:nil];
    }
    return nil;
  }

  NSCondition *condition = [[NSCondition alloc] init];
  __block BOOL completed = NO;

  __block NSData *blockResult = nil;
  __block NSError *blockError = nil;

  @synchronized(self) {
    if (!_connection) {
      if (error) {
        *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                     code:GNCNWFrameworkErrorNotConnected
                                 userInfo:nil];
      }
      return nil;
    }
    [_connection
        receiveMessageWithMinLength:(uint32_t)length
                          maxLength:(uint32_t)length
                  completionHandler:^(dispatch_data_t _Nullable content,
                                      nw_content_context_t _Nullable context, bool isComplete,
                                      nw_error_t _Nullable receiveError) {
                    [condition lock];
                    if (completed) {
                      [condition unlock];
                      return;
                    }
                    if (receiveError) {
                      blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(receiveError);
                    }
#if __LP64__
                    // This cast is only safe in a 64-bit runtime.
                    blockResult = [(NSData *)content copy];
#else
                    blockResult = nil;
#endif
                    completed = YES;
                    [condition signal];
                    [condition unlock];
                  }];
  }

  [condition lock];
  while (!completed) {
    [condition wait];
  }
  NSData *result = blockResult;
  NSError *resultError = blockError;
  [condition unlock];

  if (error != nil) {
    *error = resultError;
  }
  return result;
}

- (std::optional<std::string>)readStringWithMaxLength:(NSUInteger)length error:(NSError **)error {
  if (!self.connection) {
    if (error) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorNotConnected
                               userInfo:nil];
    }
    return std::nullopt;
  }

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block std::string resultString;
  __block NSError *blockError = nil;
  __block BOOL contentReceived = NO;

  @synchronized(self) {
    if (!_connection) {
      if (error) {
        *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                     code:GNCNWFrameworkErrorNotConnected
                                 userInfo:nil];
      }
      return std::nullopt;
    }
    [_connection
        receiveMessageWithMinLength:(uint32_t)length
                          maxLength:(uint32_t)length
                  completionHandler:^(dispatch_data_t _Nullable content,
                                      nw_content_context_t _Nullable context, bool isComplete,
                                      nw_error_t _Nullable receiveError) {
                    if (receiveError) {
                      blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(receiveError);
                    }
                    if (content) {
                      contentReceived = YES;
                      // OPTIMIZATION: Copy directly from dispatch_data_t into std::string
                      resultString.reserve(dispatch_data_get_size(content));
                      dispatch_data_apply(content, ^bool(dispatch_data_t region, size_t offset,
                                                         const void *buffer, size_t size) {
                        resultString.append((const char *)buffer, size);
                        return true;
                      });
                    }
                    dispatch_semaphore_signal(semaphore);
                  }];
  }

  // Block the current thread until the network callback completes.
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

  if (error != nil) {
    *error = blockError;
  }
  return (!contentReceived) ? std::nullopt : std::make_optional(std::move(resultString));
}

- (BOOL)write:(NSData *)data error:(NSError **)error {
  if (!self.connection) {
    if (error) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorUnknown
                               userInfo:nil];
    }
    return NO;
  }

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

  return [self sendDispatchData:dispatchData isComplete:NO error:error];
}

- (BOOL)writeBytes:(const void *)bytes length:(NSUInteger)length error:(NSError **)error {
  if (!self.connection) {
    if (error) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorNotConnected
                               userInfo:nil];
    }
    return NO;
  }

  // OPTIMIZATION: Use DISPATCH_DATA_DESTRUCTOR_DEFAULT to perform a
  // single copy into a GCD-managed buffer. No NSData required.
  // TODO: edwinwu - Investigate to see if it is worth to make it zero-copy by replacing
  // DISPATCH_DATA_DESTRUCTOR_DEFAULT with a custom empty destructor:
  //     dispatch_data_t dispatchData = dispatch_data_create(bytes, length, nil, ^{
  //         // Zero-copy: ownership remains with the caller.
  //     });
  dispatch_data_t dispatchData =
      dispatch_data_create(bytes, length, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);

  return [self sendDispatchData:dispatchData isComplete:NO error:error];
}

- (BOOL)sendDispatchData:(dispatch_data_t)data
             isComplete:(BOOL)isComplete
                  error:(NSError **)error {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  NSObject *completionMonitor = [[NSObject alloc] init];
  __block BOOL completed = NO;
  __block NSError *blockError = nil;
  @synchronized(self) {
    if (!_connection) {
      if (error) {
        *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                    code:GNCNWFrameworkErrorNotConnected
                                userInfo:nil];
      }
      return NO;
    }
    [_connection sendData:data
                 context:NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT
              isComplete:isComplete
       completionHandler:^(nw_error_t _Nullable sendError) {
         @synchronized(completionMonitor) {
           if (completed) {
             return;
           }
         }
         // Conversion must not prevent the waiting caller from timing out.
         NSError *sendNSError = sendError ?
             (__bridge_transfer NSError *)nw_error_copy_cf_error(sendError) : nil;
         @synchronized(completionMonitor) {
           if (completed) {
             return;
           }
           blockError = sendNSError;
           completed = YES;
         }
         dispatch_semaphore_signal(semaphore);
       }];
  }
  intptr_t waitResult = dispatch_semaphore_wait(
      semaphore,
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kConnectionWriteTimeout * NSEC_PER_SEC)));
  NSError *resultError;
  @synchronized(completionMonitor) {
    if (waitResult != 0) {
      completed = YES;  // Freeze the uncertain timeout; never retry this write.
      resultError = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                       code:GNCNWFrameworkErrorTimedOut
                                   userInfo:nil];
    } else {
      resultError = blockError;
    }
  }
  if (error) {
    *error = resultError;
  }
  return waitResult == 0 && resultError == nil;
}

- (void)close {
  id<GNCNWConnection> connection;
  @synchronized(self) {
    connection = _connection;
    _connection = nil;
  }
  // Never hold the lifecycle monitor across cancellation callbacks.
  [connection cancel];
}

@end
