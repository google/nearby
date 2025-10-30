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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"

#import "internal/platform/implementation/apple/Log/GNCLogger.h"

NS_ASSUME_NONNULL_BEGIN

enum { READ_BUFFER_SIZE = 409600 };

/** A pending packet that will be written to the L2CAP socket. */
@interface GNCBLEL2CAPStreamWriteOperation : NSObject

/// Initializes a write with given data and completion block.
- (instancetype)initWithData:(NSData *)data
             completionBlock:(void (^)(BOOL))completionBlock NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// The remaining data that should be written to the L2CAP socket.
@property(nonatomic, readonly) NSData *remainingData;

/// Invoked when this packet is completely written.
@property(nonatomic, readonly) void (^completionBlock)(BOOL);

/// Removes given number of bytes from the beginning of the remaining data.
- (void)consumeBytes:(NSUInteger)consumedByteCount;

@end

@interface GNCBLEL2CAPStream () <NSStreamDelegate>

/// Input stream from the device. Operations to this stream are synchronized by |_streamQueue|
/// dispatch queue.
@property(nonatomic, nullable) NSInputStream *inputStream;

/// Output stream to the device. Operations to this stream are synchronized by synchronized access
/// on |_writeBufferArray|.
@property(nonatomic, nullable) NSOutputStream *outputStream;

@end

@implementation GNCBLEL2CAPStream {
  GNCBLEL2CAPStreamClosedBlock _closedBlock;

  /// Serial queue used when invoking delegate didReceiveData.
  dispatch_queue_t _receivedDataQueue;

  /// Queue used exclusively from events on |inputStream| and |outputStream|.
  dispatch_queue_t _streamQueue;

  /// Pending data to be written to the remote device, synchronized access on itself.
  /// @synchronized used since 3x speedup in benchmark over dispatch_async.
  NSMutableArray<GNCBLEL2CAPStreamWriteOperation *> *_writeBufferArray;

  /// NSOutputStream has notified that space is available to write data.
  /// synchronized access on |_writeBufferArray|.
  BOOL _writeBufferReadyForData;

  /// Verbose logging for some statements which are only useful when debugging but produce far too
  /// much log-spam to enable on Dev.
  BOOL _verboseLoggingEnabled;

  /// Whether the stream is closed.
  BOOL _closed;
}

#pragma mark Public

- (instancetype)initWithClosedBlock:(GNCBLEL2CAPStreamClosedBlock)closedBlock
                        inputStream:(NSInputStream *)inputStream
                       outputStream:(NSOutputStream *)outputStream {
  self = [super init];
  if (self) {
    _streamQueue = dispatch_queue_create("com.google.nearby.GNCBLEL2CAPStream",
                                         dispatch_queue_attr_make_with_qos_class(
                                             DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, -1));
    _receivedDataQueue =
        dispatch_queue_create("com.google.nearby.GNCBLEL2CAPStream.receivedData",
                              dispatch_queue_attr_make_with_qos_class(
                                  DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, -1));

    _closedBlock = closedBlock;

    _writeBufferArray = [NSMutableArray array];

    [self configureStreamsWithInputStream:inputStream outputStream:outputStream];
  }
  return self;
}

- (void)close {
  if (_closed) {
    return;
  }
  if (_closedBlock) {
    _closedBlock();
  }
  _closed = YES;
}

- (void)tearDown {
  dispatch_async(_streamQueue, ^{
    GNCLoggerDebug(@"[NEARBY] Closing inputStream %@ by tearDown", self.inputStream);
    [self.inputStream close];
    self.inputStream.delegate = nil;
    self.inputStream = nil;

    @synchronized(self->_writeBufferArray) {
      GNCLoggerDebug(@"[NEARBY] Closing outputStream %@ by tearDown", self.outputStream);
      [self.outputStream close];
      self.outputStream.delegate = nil;
      self.outputStream = nil;

      for (GNCBLEL2CAPStreamWriteOperation *pendingWrite in [self->_writeBufferArray copy]) {
        pendingWrite.completionBlock(NO);
      }

      [self->_writeBufferArray removeAllObjects];
      self->_writeBufferReadyForData = NO;
    }
  });
}

- (void)dealloc {
  NSStream *inputStream = _inputStream;
  NSStream *outputStream = _outputStream;
  dispatch_async(_streamQueue, ^{
    GNCLoggerDebug(@"[NEARBY] Closing streams inputStream %@ outputStream %@ by deallocation",
                   inputStream, outputStream);
    [inputStream close];
    inputStream.delegate = nil;

    [outputStream close];
    outputStream.delegate = nil;
  });
}

- (void)sendData:(NSData *)data completionBlock:(void (^)(BOOL))completionBlock {
  if (!data || data.length == 0) {
    GNCLoggerError(@"[NEARBY] Sending data cannot be nil or empty");
  }

  if (_closed) {
    GNCLoggerInfo(@"[NEARBY] Sending data after stream is closed.");
    completionBlock(YES);
    return;
  }

  GNCBLEL2CAPStreamWriteOperation *write =
      [[GNCBLEL2CAPStreamWriteOperation alloc] initWithData:data completionBlock:completionBlock];
  @synchronized(_writeBufferArray) {
    [_writeBufferArray addObject:write];

    if (_writeBufferReadyForData) {
      _writeBufferReadyForData = NO;
      [self sendWriteBufferData];
    }
  }
}

#pragma mark NSStreamDelegate

- (void)stream:(NSStream *)stream handleEvent:(NSStreamEvent)eventCode {
  if (_verboseLoggingEnabled) {
    GNCLoggerDebug(@"[NEARBY] Stream event %@ for stream %@",
                   [[self class] stringFromStreamEventCode:eventCode], stream);
  }
  if ([stream isEqual:self.inputStream]) {
    switch (eventCode) {
      case NSStreamEventHasBytesAvailable:
        [self receiveStreamData];
        break;
      case NSStreamEventErrorOccurred:
      case NSStreamEventEndEncountered:
        [self tearDown];
        break;
      case NSStreamEventOpenCompleted:
      case NSStreamEventHasSpaceAvailable:
      case NSStreamEventNone:
      default:
        GNCLoggerInfo(@"[NEARBY] Received event %@ for stream %@",
                      [[self class] stringFromStreamEventCode:eventCode], stream);
        break;
    }
    return;
  }

  if ([stream isEqual:self.outputStream]) {
    switch (eventCode) {
      case NSStreamEventHasSpaceAvailable: {
        @synchronized(_writeBufferArray) {
          if (!_writeBufferArray.count) {
            _writeBufferReadyForData = YES;
            return;
          }
          [self sendWriteBufferData];
        }
        break;
      }
      case NSStreamEventHasBytesAvailable:
      case NSStreamEventOpenCompleted:
      case NSStreamEventErrorOccurred:
      case NSStreamEventEndEncountered:
      case NSStreamEventNone:
      default:
        GNCLoggerInfo(@"[NEARBY] Received event %@ for stream %@",
                      [[self class] stringFromStreamEventCode:eventCode], stream);
        break;
    }
    return;
  }
}

#pragma mark Private

/// Returns a string representation of @c NSStreamEvent.
+ (NSString *)stringFromStreamEventCode:(NSStreamEvent)eventCode {
  switch (eventCode) {
    case NSStreamEventOpenCompleted:
      return @"NSStreamEventOpenCompleted";
    case NSStreamEventHasSpaceAvailable:
      return @"NSStreamEventHasSpaceAvailable";
    case NSStreamEventHasBytesAvailable:
      return @"NSStreamEventHasBytesAvailable";
    case NSStreamEventErrorOccurred:
      return @"NSStreamEventErrorOccurred";
    case NSStreamEventEndEncountered:
      return @"NSStreamEventEndEncountered";
    case NSStreamEventNone:
      return @"NSStreamEventNone";
    default:
      return [NSString stringWithFormat:@"Unknown NSStreamEvent %@", @(eventCode)];
  }
}

/// Sets up |inputStream| and |outputStream| on |_streamQueue|.
- (void)configureStreamsWithInputStream:(NSInputStream *)inputStream
                           outputStream:(NSOutputStream *)outputStream {
  self.inputStream = inputStream;
  self.outputStream = outputStream;

  if (inputStream.delegate) {
    GNCLoggerError(@"[NEARBY] Should not have a delegate.");
    return;
  }
  if (outputStream.delegate) {
    GNCLoggerError(@"[NEARBY] Should not have a delegate.");
    return;
  }

  inputStream.delegate = self;
  outputStream.delegate = self;

  GNCLoggerDebug(@"[NEARBY] streams inputStream %@ outputStream %@", inputStream, outputStream);

  if (!_streamQueue) {
    GNCLoggerError(@"[NEARBY] Stream queue must be initialized.");
    return;
  }

  CFReadStreamSetDispatchQueue((__bridge CFReadStreamRef)inputStream, _streamQueue);
  CFWriteStreamSetDispatchQueue((__bridge CFWriteStreamRef)outputStream, _streamQueue);
  // Need to open streams on |_streamQueue|.
  dispatch_async(_streamQueue, ^{
    [inputStream open];
    [outputStream open];
  });
}

/// Sends data in the write buffer to the remote device.
/// Must be called after synchronizing on |_writeBufferArray|.
- (void)sendWriteBufferData {
  if (!_writeBufferArray.count) {
    GNCLoggerError(@"[NEARBY] sendWriteBufferData should not be called with empty buffer");
    return;
  }

  NSUInteger totalBytesToWrite = _writeBufferArray.firstObject.remainingData.length;
  NSInteger result =
      [self.outputStream write:(const uint8_t *)_writeBufferArray.firstObject.remainingData.bytes
                     maxLength:totalBytesToWrite];
  if (result < 0) {
    GNCLoggerError(@"[NEARBY] Stream write error %@", self.outputStream.streamError);
    return;
  }

  NSUInteger totalBytesWritten = (NSUInteger)result;

  if (_verboseLoggingEnabled) {
    GNCLoggerInfo(@"[NEARBY] Wrote %@/%@ bytes to stream", @(totalBytesWritten),
                  @(totalBytesToWrite));
  }

  if (totalBytesWritten == totalBytesToWrite) {
    GNCBLEL2CAPStreamWriteOperation *finishedWrite = _writeBufferArray.firstObject;
    [_writeBufferArray removeObjectAtIndex:0];
    finishedWrite.completionBlock(YES);
  } else {
    [_writeBufferArray.firstObject consumeBytes:totalBytesWritten];
    if (_writeBufferArray.firstObject.remainingData.length == 0) {
      GNCLoggerError(@"[NEARBY] Remaining data cannot be empty.");
      return;
    }
  }
}

/// Receives data from device and invokes |_receivedDataBlock|.
- (void)receiveStreamData {
  dispatch_assert_queue_debug(_streamQueue);
  uint8_t readBuffer[READ_BUFFER_SIZE];
  NSInteger bytesRead = [self.inputStream read:readBuffer maxLength:READ_BUFFER_SIZE];

  if (bytesRead > 0) {
    NSMutableData *data = [NSMutableData data];
    [data appendBytes:readBuffer length:(NSUInteger)bytesRead];

    if (_verboseLoggingEnabled) {
      GNCLoggerDebug(@"[NEARBY] Stream data from device of length %@", @(data.length));
    }

    dispatch_async(_receivedDataQueue, ^{
      [_delegate stream:self didReceiveData:data];
    });
  } else if (bytesRead < 0) {
    GNCLoggerError(@"[NEARBY] Stream read error: %@", self.inputStream.streamError);
    [_delegate stream:self didDisconnectWithError:self.inputStream.streamError];
    if (_closedBlock) {
      _closedBlock();
    }
  } else if (bytesRead == 0) {
    GNCLoggerDebug(@"[NEARBY] End of stream reached. Disconnecting");
    // This indicates the L2CAP socket is closed. Notifying the owner so that it can tear down this
    // stream and update its own state.
    [_delegate stream:self didDisconnectWithError:nil];
    if (_closedBlock) {
      _closedBlock();
    }
  }
}

@end

@implementation GNCBLEL2CAPStreamWriteOperation

- (instancetype)initWithData:(NSData *)data completionBlock:(void (^)(BOOL))completionBlock {
  if (self = [super init]) {
    // Create copy of data to prevent client modification.
    _remainingData = [data copy];
    _completionBlock = completionBlock;
  }
  return self;
}

- (void)consumeBytes:(NSUInteger)consumedByteCount {
  _remainingData = [_remainingData
      subdataWithRange:NSMakeRange(consumedByteCount, _remainingData.length - consumedByteCount)];
}

@end

NS_ASSUME_NONNULL_END
