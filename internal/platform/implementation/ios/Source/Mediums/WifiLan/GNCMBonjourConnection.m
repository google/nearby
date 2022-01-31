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

#import "internal/platform/implementation/ios/Source/Mediums/WifiLan/GNCMBonjourConnection.h"

#import "internal/platform/implementation/ios/Source/Mediums/GNCMConnection.h"
#import "GoogleToolboxForMac/GTMLogger.h"

enum { kMaxPacketSize = 32 * 1024 };

@interface GNCMBonjourConnection () <NSStreamDelegate>
@property(nonatomic) NSInputStream *inputStream;
@property(nonatomic) NSOutputStream *outputStream;
@property(nonatomic) dispatch_queue_t callbackQueue;

@property(nonatomic, copy, nullable) NSData *dataBeingWritten;
@property(nonatomic) NSInteger numberOfBytesLeftToWrite;
@property(nonatomic, copy, nullable) GNCMProgressHandler progressHandler;
@property(nonatomic, copy, nullable) GNCMPayloadResultHandler completion;

@property(nonatomic) BOOL inputStreamOpen;
@property(nonatomic) BOOL outputStreamOpen;
@end

@implementation GNCMBonjourConnection

- (instancetype)initWithInputStream:(NSInputStream *)inputStream
                       outputStream:(NSOutputStream *)outputStream
                              queue:(nullable dispatch_queue_t)queue {
  self = [super init];
  if (self) {
    _inputStreamOpen = NO;
    _outputStreamOpen = NO;

    _inputStream = inputStream;
    _inputStream.delegate = self;
    [_inputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [_inputStream open];

    _outputStream = outputStream;
    _outputStream.delegate = self;
    [_outputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [_outputStream open];

    _callbackQueue = queue;
  }
  return self;
}

- (void)dealloc {
  [self closeStreams];
}

- (void)sendData:(NSData *)payload
    progressHandler:(GNCMProgressHandler)progressHandler
         completion:(GNCMPayloadResultHandler)completion {
  if (_dataBeingWritten) {
    GTMLoggerInfo(@"Attempting to send payload while one is already in flight");
    [self dispatchCallback:^{
      completion(GNCMPayloadResultFailure);
    }];
    return;
  }

  self.dataBeingWritten = payload;
  self.numberOfBytesLeftToWrite = payload.length;
  self.progressHandler = progressHandler;
  self.completion = completion;
  [self writeChunk];
}

#pragma mark NSStreamDelegate

- (void)stream:(NSStream *)stream handleEvent:(NSStreamEvent)event {
  switch (event) {
    case NSStreamEventHasBytesAvailable: {
      // Data has arrived on the input stream.
      NSAssert(stream == _inputStream, @"Error: Expected input stream");

      if (_connectionHandlers.payloadHandler) {
        uint8_t bytesRead[kMaxPacketSize];
        NSInteger numberOfBytesRead;
        @synchronized(self.inputStream) {
          numberOfBytesRead = [self.inputStream read:bytesRead maxLength:kMaxPacketSize];
        }
        NSData *data = nil;
        if (numberOfBytesRead > 0) {
          GTMLoggerInfo(@"Read %lu bytes", (u_long)numberOfBytesRead);
          data = [NSData dataWithBytes:bytesRead length:numberOfBytesRead];
        }
        [self dispatchCallback:^{
          self.connectionHandlers.payloadHandler(data ?: [NSData data]);
        }];
      }
      break;
    }

    case NSStreamEventHasSpaceAvailable:
      // There is space available on the output stream.
      NSAssert(stream == _outputStream, @"Error: Expected output stream");

      // Schedule this in a future runloop cycle because -writeChunk, which can cause this event
      // to be received synchronously, is not reentrant.
      [self performSelector:@selector(writeChunk) withObject:nil afterDelay:0.0];
      break;

    case NSStreamEventErrorOccurred:
      GTMLoggerInfo(@"Stream error: %@", [stream streamError]);
      // Fall through.
    case NSStreamEventEndEncountered: {
      GTMLoggerInfo(@"Stream closing");
      [self closeStreams];
      if (_connectionHandlers.disconnectedHandler) {
        [self dispatchCallback:^{
          self.connectionHandlers.disconnectedHandler();
        }];
      }
      break;
    }

    case NSStreamEventOpenCompleted:
      if (stream == _inputStream) {
        _inputStreamOpen = YES;
      }
      if (stream == _outputStream) {
        _outputStreamOpen = YES;
      }
      // Schedule this in a future runloop cycle because -writeChunk, which can cause this event
      // to be received synchronously, is not reentrant.
      [self performSelector:@selector(writeChunk) withObject:nil afterDelay:0.0];
      break;

    case NSStreamEventNone:
    default:
      break;
  }
}

#pragma mark Private

// Calls a block on the callback queue.
- (void)dispatchCallback:(dispatch_block_t)block {
  dispatch_async(_callbackQueue ?: dispatch_get_main_queue(), block);
}

// Writes a chunk of the outgoing data to the output stream, calling the progress and completion
// handlers as needed.
- (void)writeChunk {
  void (^reportProgress)(size_t) = ^(size_t count) {
    // Captures the progress handler because the property is nilled out below.
    GNCMProgressHandler progressHandler = _progressHandler;
    if (progressHandler != nil) {
      [self dispatchCallback:^{
        progressHandler(count);
      }];
    }
  };

  void (^completed)(GNCMPayloadResult) = ^(GNCMPayloadResult result) {
    reportProgress(_dataBeingWritten.length);
    _progressHandler = nil;
    _dataBeingWritten = nil;

    // Captures the completion because the property is nilled out below.
    GNCMPayloadResultHandler completion = _completion;
    if (completion != nil) {
      [self dispatchCallback:^{
        completion(result);
      }];
    }
    _completion = nil;
  };

  @synchronized(_outputStream) {
    if (_inputStreamOpen && _outputStreamOpen && _numberOfBytesLeftToWrite) {
      NSUInteger dataLength = (UInt32)_dataBeingWritten.length;
      if (_numberOfBytesLeftToWrite == dataLength) {
        GTMLoggerInfo(@"Starting a write operation of length %lu", (u_long)dataLength);
      }
      NSInteger numberOfPayloadBytesWritten =
          [_outputStream write:&_dataBeingWritten.bytes[dataLength - _numberOfBytesLeftToWrite]
                     maxLength:_numberOfBytesLeftToWrite];

      GTMLoggerInfo(@"Wrote %lu bytes", (u_long)numberOfPayloadBytesWritten);
      if (numberOfPayloadBytesWritten >= 0) {
        _numberOfBytesLeftToWrite -= numberOfPayloadBytesWritten;
        reportProgress(_dataBeingWritten.length - _numberOfBytesLeftToWrite);
        if (_numberOfBytesLeftToWrite < 0) {
          GTMLoggerInfo(@"Unexpected number of bytes written");
          _numberOfBytesLeftToWrite = 0;
        }
        if (_numberOfBytesLeftToWrite == 0) completed(GNCMPayloadResultSuccess);
      } else {
        GTMLoggerInfo(@"Error writing to output stream");
        completed(GNCMPayloadResultFailure);
      }
    }
  }
}

- (void)closeStreams {
  @synchronized(_inputStream) {
    [_inputStream close];
    _inputStream = nil;
  }

  @synchronized(_outputStream) {
    [_outputStream close];
    _outputStream = nil;
  }
}

@end
