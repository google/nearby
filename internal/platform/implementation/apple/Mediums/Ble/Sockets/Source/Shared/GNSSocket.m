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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket+Private.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSWeavePacket.h"
#import "GoogleToolboxForMac/GTMLogger.h"

typedef void (^GNSIncomingChunkReceivedBlock)(NSData *incomingData);

@interface GNSSocket () {
  NSMutableData *_incomingBuffer;
  id _peer;
}

// Handler to generate a chunk for sending data. When the bluetooth stack is ready to send
// more data the handler is called recursively to send the next chunk.
@property(nonatomic, copy) GNSSendChunkBlock sendChunkCallback;

// Handler to receive chunk from the central.
@property(nonatomic, copy) GNSIncomingChunkReceivedBlock incomingChunkReceivedCallback;

@property(nonatomic, readwrite, assign, getter=isConnected) BOOL connected;

@property(nonatomic, readwrite) NSUUID *socketIdentifier;

- (instancetype)initWithOwner:(id<GNSSocketOwner>)owner
                         peer:(id)peer
                        queue:(dispatch_queue_t)queue NS_DESIGNATED_INITIALIZER;

@end

@implementation GNSSocket

- (void)dealloc {
  [_owner socketWillBeDeallocated:self];
}

- (BOOL)isSendOperationInProgress {
  return self.sendChunkCallback != nil;
}

- (void)sendData:(NSData *)data
    progressHandler:(GNSProgressHandler)progressHandler
         completion:(GNSErrorHandler)completion {
  void (^callCompletion)(NSError *) = ^(NSError *error) {
    if (completion) dispatch_async(_queue, ^{ completion(error); });
  };

  if (self.sendChunkCallback) {
    GTMLoggerInfo(@"Send operation already in progress");
    callCompletion(GNSErrorWithCode(GNSErrorOperationInProgress));
    return;
  }
  data = [data copy];
  NSUInteger totalDataSize = data.length;
  GTMLoggerInfo(@"Sending data with size %lu", (unsigned long)totalDataSize);

  // Capture self for the duration of the send operation, to ensure it is completely sent.
  // If the connection is lost, the block will be deleted and the retain cycle broken.
  __typeof__(self) selfRef = self;  // this avoids the retain cycle compiler warning
  self.sendChunkCallback = ^(NSUInteger offset) {
    __typeof__(self) self = selfRef;
    if (!self.isConnected) {
      self.sendChunkCallback = nil;
      callCompletion(GNSErrorWithCode(GNSErrorNoConnection));
      return;
    }
    if (progressHandler) {
      float progressPercentage = 1.0;
      if (totalDataSize > 0) {
        progressPercentage = (float)offset / totalDataSize;
      }
      progressHandler(progressPercentage);
    }
    NSUInteger newOffset = offset;
    GNSWeaveDataPacket *dataPacket =
        [GNSWeaveDataPacket dataPacketWithPacketCounter:self.sendPacketCounter
                                             packetSize:self.packetSize
                                                   data:data
                                                 offset:&newOffset];
    [self incrementSendPacketCounter];

    GTMLoggerInfo(@"Sending chunk with size %ld", (long)(newOffset - offset));
    [self.owner sendData:[dataPacket serialize] socket:self completion:^(NSError *_Nullable error) {
      if (error) {
        GTMLoggerInfo(@"Error sending chunk");
        self.sendChunkCallback = nil;
        callCompletion(error);
      } else {
        if (newOffset < totalDataSize) {
          // Dispatch async to avoid stack overflow on large payloads.
          dispatch_async(self.queue, ^{ self.sendChunkCallback(newOffset); });
        } else {
          GTMLoggerInfo(@"Finished sending payload");
          self.sendChunkCallback = nil;
          callCompletion(nil);
        }
      }
    }];
  };
  self.sendChunkCallback(0);
}

- (void)disconnect {
  if (!_connected) {
    GTMLoggerInfo(@"Socket already disconnected, socket: %@, delegate %@, owner %@", self,
                  _delegate, _owner);
    return;
  }
  GTMLoggerInfo(@"Disconnect");
  [_owner disconnectSocket:self];
}

- (NSString *)description {
  return [NSString stringWithFormat:@"<%@: %p, %@, central: %@>", [self class], self,
                                    _connected ? @"connected" : @"not connected",
                                    self.peerIdentifier.UUIDString];
}

- (NSUUID *)peerIdentifier {
  return (NSUUID *)[_peer identifier];
}

- (NSUUID *)serviceIdentifier {
  return [_owner socketServiceIdentifier:self];
}

#pragma mark - Private

- (instancetype)init {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (instancetype)initWithOwner:(id<GNSSocketOwner>)owner
                  centralPeer:(CBCentral *)centralPeer
                        queue:(dispatch_queue_t)queue {
  return [self initWithOwner:owner peer:centralPeer queue:queue];
}

- (instancetype)initWithOwner:(id<GNSSocketOwner>)owner
               peripheralPeer:(CBPeripheral *)peripheralPeer
                        queue:(dispatch_queue_t)queue {
  return [self initWithOwner:owner peer:peripheralPeer queue:queue];
}

- (instancetype)initWithOwner:(id<GNSSocketOwner>)owner
                         peer:(id)peer
                        queue:(dispatch_queue_t)queue {
  self = [super init];
  if (self) {
    NSAssert(owner, @"Socket should have an owner.");
    NSAssert(peer, @"Socket should have a peer.");
    _owner = owner;
    _peer = peer;
    _queue = queue;
    _socketIdentifier = [NSUUID UUID];
    _packetSize = kGNSMinSupportedPacketSize;
  }
  return self;
}

- (void)incrementReceivePacketCounter {
  _receivePacketCounter = (_receivePacketCounter + 1) % kGNSMaxPacketCounterValue;
  GTMLoggerDebug(@"New receive packet counter %d", _receivePacketCounter);
}

- (void)incrementSendPacketCounter {
  _sendPacketCounter = (_sendPacketCounter + 1) % kGNSMaxPacketCounterValue;
  GTMLoggerDebug(@"New send packet counter %d", _sendPacketCounter);
}

- (void)didConnect {
  _connected = YES;
  [_delegate socketDidConnect:self];
}

- (void)didDisconnectWithError:(NSError *)error {
  _connected = NO;
  _incomingChunkReceivedCallback = nil;
  _incomingBuffer = nil;
  [_delegate socket:self didDisconnectWithError:error];
}

- (void)didReceiveIncomingWeaveDataPacket:(GNSWeaveDataPacket *)dataPacket {
  if (!_connected) {
    GTMLoggerError(@"Cannot receive incoming data packet while not being connected");
    return;
  }
  if (dataPacket.isFirstPacket) {
    NSAssert(!_incomingBuffer, @"There should not be a receive operation in progress.");
    _incomingBuffer = [NSMutableData data];
  }
  GTMLoggerInfo(@"Received chunk with size %lu", (unsigned long)dataPacket.data.length);
  [_incomingBuffer appendData:dataPacket.data];
  if (dataPacket.isLastPacket) {
    NSData *incomingData = _incomingBuffer;
    _incomingBuffer = nil;
    GTMLoggerInfo(@"Finished receiving payload with size %lu", (unsigned long)incomingData.length);
    [self.delegate socket:self didReceiveData:incomingData];
  }
}

- (BOOL)waitingForIncomingData {
  return _incomingBuffer != nil;
}

- (CBPeripheral *)peerAsPeripheral {
  NSAssert([_peer isKindOfClass:[CBPeripheral class]], @"Wrong peer type %@", _peer);
  if ([_peer isKindOfClass:[CBPeripheral class]]) {
    return _peer;
  } else {
    return nil;
  }
}

- (CBCentral *)peerAsCentral {
  NSAssert([_peer isKindOfClass:[CBCentral class]], @"Wrong peer type %@", _peer);
  if ([_peer isKindOfClass:[CBCentral class]]) {
    return _peer;
  } else {
    return nil;
  }
}

@end
