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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPClient.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralDelegateMultiplexer.h"

static char *const kGNCBLEL2CAPClientQueueLabel = "com.google.nearby.GNCBLEL2CAPClient";

@interface GNCBLEL2CAPClient () <GNCPeripheralDelegate>
@end

@implementation GNCBLEL2CAPClient {
  dispatch_queue_t _queue;
  id<GNCPeripheral> _peripheral;
  GNCOpenL2CAPStreamCompletionHandler _completionHandler;
  GNCRequestDisconnectionHandler _requestDisconnectionHandler;
  CBL2CAPPSM _expectedPSM;
  BOOL _cancelledBeforeOpen;

  CBL2CAPChannel *_l2CAPChannel;
  GNCBLEL2CAPStream *_l2CAPStream;
  NSObject *_l2CAPStreamToken;
  id<GNCPeripheral> _l2CAPStreamPeripheral;
}

- (instancetype)initWithRequestDisconnectionHandler:
    (GNCRequestDisconnectionHandler)requestDisconnectionHandler {
  return
      [self initWithQueue:dispatch_queue_create(kGNCBLEL2CAPClientQueueLabel, DISPATCH_QUEUE_SERIAL)
          requestDisconnectionHandler:requestDisconnectionHandler];
};

// This is private and should only be used for tests.
- (instancetype)initWithQueue:(nullable dispatch_queue_t)queue
    requestDisconnectionHandler:(GNCRequestDisconnectionHandler)requestDisconnectionHandler {
  self = [super init];
  if (self) {
    _queue = queue ?: dispatch_get_main_queue();
    _requestDisconnectionHandler = requestDisconnectionHandler;
  }
  return self;
};

- (void)openL2CAPChannelWithPSM:(uint16_t)PSM
                     peripheral:(id<GNCPeripheral>)peripheral
              completionHandler:(GNCOpenL2CAPStreamCompletionHandler)completionHandler {
  GNCOpenL2CAPStreamCompletionHandler copiedCompletionHandler = [completionHandler copy];
  dispatch_async(_queue, ^{
    if (_cancelledBeforeOpen) {
      _cancelledBeforeOpen = NO;
      GNCLoggerInfo(
          @"[NEARBY] Ignoring openL2CAPChannelWithPSM = %d because client is disconnected", PSM);
      if (copiedCompletionHandler) {
        copiedCompletionHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                                         code:GNCBLEErrorUnknown
                                                     userInfo:nil]);
      }
      return;
    }
    GNCLoggerInfo(@"[NEARBY] openL2CAPChannelWithPSM = %d", PSM);
    if (_completionHandler) {
      GNCOpenL2CAPStreamCompletionHandler previousCompletionHandler = _completionHandler;
      _completionHandler = nil;
      previousCompletionHandler(nil, [NSError errorWithDomain:GNCBLEErrorDomain
                                                         code:GNCBLEErrorUnknown
                                                     userInfo:nil]);
    }
    if (_peripheral &&
        (_peripheral != peripheral || ![_peripheral.identifier isEqual:peripheral.identifier])) {
      if ([_peripheral.peripheralDelegate isKindOfClass:[GNCPeripheralDelegateMultiplexer class]]) {
        [(GNCPeripheralDelegateMultiplexer *)_peripheral.peripheralDelegate removeListener:self];
      } else if (_peripheral.peripheralDelegate == self) {
        _peripheral.peripheralDelegate = nil;
      }
    }
    _peripheral = peripheral;
    _expectedPSM = (CBL2CAPPSM)PSM;
    _completionHandler = copiedCompletionHandler;
    if ([_peripheral.peripheralDelegate isKindOfClass:[GNCPeripheralDelegateMultiplexer class]]) {
      [(GNCPeripheralDelegateMultiplexer *)_peripheral.peripheralDelegate addListener:self];
    } else {
      _peripheral.peripheralDelegate = self;
    }
    [_peripheral openL2CAPChannel:(CBL2CAPPSM)PSM];
  });
}

- (void)disconnect {
  dispatch_async(_queue, ^{
    if (!_peripheral && !_l2CAPStream && !_l2CAPChannel) {
      _cancelledBeforeOpen = YES;
    }
    [self disconnectOnQueue];
  });
}

#pragma mark - GNCPeripheralDelegate

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                  error:(NSError *)error {
  dispatch_async(_queue, ^{
    [self handleDidOpenL2CAPChannelOnQueue:channel forPeripheral:peripheral error:error];
  });
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral
    didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                  error:(NSError *)error {
  if ([self respondsToSelector:@selector(gnc_peripheral:didOpenL2CAPChannel:error:)]) {
    [self gnc_peripheral:peripheral didOpenL2CAPChannel:channel error:error];
  }
}

#pragma mark Private

- (void)handleDidOpenL2CAPChannelOnQueue:(CBL2CAPChannel *)channel
                           forPeripheral:(id<GNCPeripheral>)peripheral
                                   error:(NSError *)error {
  dispatch_assert_queue(_queue);
  if (!_peripheral || ![peripheral.identifier isEqual:_peripheral.identifier] ||
      !_completionHandler) {
    return;
  }
  if (channel.PSM != 0 && channel.PSM != _expectedPSM) {
    GNCLoggerDebug(@"[NEARBY] Ignoring didOpenL2CAPChannel for PSM %u (expected %u)", channel.PSM,
                   _expectedPSM);
    return;
  }
  if ([_peripheral.peripheralDelegate isKindOfClass:[GNCPeripheralDelegateMultiplexer class]]) {
    [(GNCPeripheralDelegateMultiplexer *)_peripheral.peripheralDelegate removeListener:self];
  }
  GNCOpenL2CAPStreamCompletionHandler completionHandler = _completionHandler;
  _completionHandler = nil;
  _expectedPSM = 0;
  GNCLoggerDebug(
      @"[NEARBY] didOpenL2CAPChannel, channel: %@, inputStream: %@, outputStream: %@, error: %@",
      channel, channel.inputStream, channel.outputStream, error);
  // TODO: b/399815436 - channel.inputStream is null when doing testing. Refactor tests in the
  // future.
  if (error || !channel || !channel.inputStream || !channel.outputStream) {
    NSError *completionError =
        error ?: (!channel ? [NSError errorWithDomain:GNCBLEErrorDomain
                                                 code:GNCBLEErrorUnknown
                                             userInfo:nil]
                           : nil);
    completionHandler(nil, completionError);
    return;
  }

  // Cleanup older references without scheduling an async disconnect on the active peripheral.
  if (_l2CAPStream || _l2CAPChannel) {
    // The watch may establish a new L2CAP socket connection while the old socket is still
    // connected if sysproxy stopped for a reason other than Bluetooth disconnection. Tearing down
    // the old stream here resets stream state between the two connection sessions without
    // disconnecting the active peripheral.
    id<GNCPeripheral> previousStreamPeripheral = _l2CAPStreamPeripheral;
    [self tearDownL2CAPStreamOnQueue];
    if (previousStreamPeripheral &&
        ![previousStreamPeripheral.identifier isEqual:_peripheral.identifier] &&
        _requestDisconnectionHandler) {
      _requestDisconnectionHandler(previousStreamPeripheral);
    }
  }

  _l2CAPChannel = channel;
  NSObject *streamToken = [[NSObject alloc] init];
  _l2CAPStreamToken = streamToken;
  _l2CAPStreamPeripheral = _peripheral;
  __weak __typeof__(self) weakSelf = self;
  _l2CAPStream = [[GNCBLEL2CAPStream alloc]
      initWithClosedBlock:^{
        __typeof__(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        dispatch_async(strongSelf->_queue, ^{
          // Ignore close notifications from streams that were already torn down or replaced.
          if (strongSelf->_l2CAPStreamToken != streamToken) {
            return;
          }
          id<GNCPeripheral> streamPeripheral = strongSelf->_l2CAPStreamPeripheral;
          [strongSelf tearDownL2CAPStreamOnQueue];
          if (!streamPeripheral) {
            return;
          }
          if (strongSelf->_peripheral &&
              [strongSelf->_peripheral.identifier isEqual:streamPeripheral.identifier] &&
              !strongSelf->_completionHandler) {
            [strongSelf disconnectOnQueue];
          } else if ((!strongSelf->_peripheral ||
                      ![strongSelf->_peripheral.identifier isEqual:streamPeripheral.identifier]) &&
                     strongSelf->_requestDisconnectionHandler) {
            strongSelf->_requestDisconnectionHandler(streamPeripheral);
          }
        });
      }
              inputStream:_l2CAPChannel.inputStream
             outputStream:_l2CAPChannel.outputStream];
  completionHandler(_l2CAPStream, nil);
}

- (void)tearDownL2CAPStreamOnQueue {
  dispatch_assert_queue(_queue);
  GNCBLEL2CAPStream *stream = _l2CAPStream;
  _l2CAPStream = nil;
  _l2CAPChannel = nil;
  _l2CAPStreamToken = nil;
  _l2CAPStreamPeripheral = nil;
  [stream tearDown];
}

- (void)disconnectOnQueue {
  dispatch_assert_queue(_queue);
  id<GNCPeripheral> localPeripheral = _peripheral;
  if ([localPeripheral.peripheralDelegate isKindOfClass:[GNCPeripheralDelegateMultiplexer class]]) {
    [(GNCPeripheralDelegateMultiplexer *)localPeripheral.peripheralDelegate removeListener:self];
  } else if (localPeripheral.peripheralDelegate == self) {
    localPeripheral.peripheralDelegate = nil;
  }
  _peripheral = nil;
  _expectedPSM = 0;
  _completionHandler = nil;
  if ([_l2CAPStreamPeripheral.identifier isEqual:localPeripheral.identifier]) {
    _l2CAPStreamPeripheral = nil;
  }
  if (_requestDisconnectionHandler) {
    _requestDisconnectionHandler(localPeripheral);
  }
}

- (void)closeL2CAPChannel {
  dispatch_async(_queue, ^{
    _cancelledBeforeOpen = NO;
    [self tearDownL2CAPStreamOnQueue];
    [self disconnectOnQueue];
  });
}

@end
