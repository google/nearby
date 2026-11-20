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
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEError.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralDelegateMultiplexer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPClient+Testing.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCBLEL2CAPFakeInputOutputStream.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeCBL2CAPChannel.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"

static const NSTimeInterval kTestTimeout = 1.0;

@interface GNCPendingL2CAPFakePeripheral : GNCFakePeripheral
@property(nonatomic) BOOL shouldPendOpenL2CAPChannel;
@property(nonatomic, copy, nullable) dispatch_block_t onOpenL2CAPChannel;
@property(nonatomic, readonly) NSMutableArray<NSNumber *> *openedPSMs;
@end

@implementation GNCPendingL2CAPFakePeripheral

- (instancetype)init {
  self = [super init];
  if (self) {
    _openedPSMs = [NSMutableArray array];
  }
  return self;
}

- (void)openL2CAPChannel:(CBL2CAPPSM)PSM {
  @synchronized(self.openedPSMs) {
    [self.openedPSMs addObject:@(PSM)];
  }
  if (self.onOpenL2CAPChannel) {
    self.onOpenL2CAPChannel();
  }
  if (self.shouldPendOpenL2CAPChannel) {
    return;
  }
  [super openL2CAPChannel:PSM];
}

@end

@interface GNCBLEL2CAPClientTest : XCTestCase
@property(nonatomic) GNCFakePeripheral *fakePeripheral;
@property(nonatomic) GNCBLEL2CAPClient *l2capClient;
@property(nonatomic) XCTestExpectation *requestDisconnectionHandlerExpectation;
@end

@implementation GNCBLEL2CAPClientTest

#pragma mark - Tests

- (void)setUp {
  [super setUp];
  self.fakePeripheral = [[GNCFakePeripheral alloc] init];
  self.l2capClient =
      [[GNCBLEL2CAPClient alloc] initWithQueue:nil
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral) {
                     XCTAssertNotNil(peripheral);
                     [self.requestDisconnectionHandlerExpectation fulfill];
                   }];
}

- (void)tearDown {
  self.fakePeripheral.peripheralDelegate = nil;
  self.fakePeripheral = nil;
  self.l2capClient = nil;
  [super tearDown];
}

- (void)flushMainQueue {
  XCTestExpectation *flushExpectation = [self expectationWithDescription:@"Main queue flushed"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [flushExpectation fulfill];
  });
  [self waitForExpectations:@[ flushExpectation ] timeout:kTestTimeout];
}

- (void)testSuccessfulOpenL2CAPChannel {
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"L2CAP channel opened successfully"];
  uint16_t psm = 123;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error, @"Error should be nil");
                            XCTAssertNil(stream, @"Stream should be nil due to testing limitation");
                            [expectation fulfill];
                          }];

  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
}

- (void)testOpenL2CAPChannelWithError {
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"L2CAP channel open failed with error"];
  uint16_t psm = 123;
  NSError *expectedError = [NSError errorWithDomain:@"TestErrorDomain" code:1 userInfo:nil];
  self.fakePeripheral.openL2CAPChannelError = expectedError;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNotNil(error, @"Error should not be nil");
                            XCTAssertEqualObjects(error, expectedError);
                            [expectation fulfill];
                          }];

  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
}

- (void)testDisconnect {
  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Request disconnection handler should be called"];
  uint16_t psm = 123;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error){
                          }];

  [self.l2capClient disconnect];

  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:kTestTimeout];
}

- (void)testOpenL2CAPChannelSetsPeripheralDelegate {
  uint16_t psm = 123;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error){
                          }];
  [self flushMainQueue];

  XCTAssertEqualObjects(self.fakePeripheral.peripheralDelegate, self.l2capClient);
}

- (void)testInitWithRequestDisconnectionHandlerAndDisconnect {
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"Disconnection handler called"];
  GNCBLEL2CAPClient *client = [[GNCBLEL2CAPClient alloc]
      initWithRequestDisconnectionHandler:^(id<GNCPeripheral> _Nullable peripheral) {
        XCTAssertNil(peripheral);
        [expectation fulfill];
      }];
  [client disconnect];
  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
}

- (void)testCloseL2CAPChannel {
  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Request disconnection handler should be called"];
  uint16_t psm = 123;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error){
                          }];

  [self.l2capClient closeL2CAPChannel];

  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:kTestTimeout];
}

- (void)testDidOpenL2CAPChannelWithStreams {
  XCTestExpectation *expectation =
      [self expectationWithDescription:@"L2CAP channel opened successfully"];
  uint16_t psm = 123;
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  self.fakePeripheral.channelInputStream = fakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = fakeStreams.outputStream;

  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            [expectation fulfill];
                          }];

  [self waitForExpectations:@[ expectation ] timeout:kTestTimeout];
  self.fakePeripheral.peripheralDelegate = nil;
  self.fakePeripheral = nil;
  self.l2capClient = nil;
  [fakeStreams tearDown];
}

- (void)testDidOpenL2CAPChannelCalledTwice {
  XCTestExpectation *firstOpenExpectation =
      [self expectationWithDescription:@"First L2CAP channel opened"];
  XCTestExpectation *secondOpenExpectation =
      [self expectationWithDescription:@"Second L2CAP channel opened"];
  XCTestExpectation *unexpectedDisconnectionExpectation =
      [self expectationWithDescription:@"Active peripheral must not disconnect during stream swap"];
  unexpectedDisconnectionExpectation.inverted = YES;
  self.requestDisconnectionHandlerExpectation = unexpectedDisconnectionExpectation;

  uint16_t psm = 123;
  GNCBLEL2CAPFakeInputOutputStream *firstFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  self.fakePeripheral.channelInputStream = firstFakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = firstFakeStreams.outputStream;

  __block GNCBLEL2CAPStream *firstStream = nil;
  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            firstStream = stream;
                            [firstOpenExpectation fulfill];
                          }];

  [self waitForExpectations:@[ firstOpenExpectation ] timeout:kTestTimeout];

  // Reopen the channel with a new stream pair while the first stream is still open.
  GNCBLEL2CAPFakeInputOutputStream *secondFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  self.fakePeripheral.channelInputStream = secondFakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = secondFakeStreams.outputStream;
  __block GNCBLEL2CAPStream *secondStream = nil;
  [self.l2capClient openL2CAPChannelWithPSM:psm
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            secondStream = stream;
                            [secondOpenExpectation fulfill];
                          }];

  [self waitForExpectations:@[ secondOpenExpectation ] timeout:kTestTimeout];
  // Closing the already-replaced first stream must not disconnect the active peripheral either.
  [firstStream close];
  [self waitForExpectations:@[ unexpectedDisconnectionExpectation ] timeout:0.1];

  // Closing the active second stream must still disconnect self.fakePeripheral.
  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Request disconnection handler called on active close"];
  [secondStream close];
  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:kTestTimeout];

  firstStream = nil;
  secondStream = nil;
  self.fakePeripheral.peripheralDelegate = nil;
  self.fakePeripheral = nil;
  self.l2capClient = nil;
  [firstFakeStreams tearDown];
  [secondFakeStreams tearDown];
}

- (void)testDidOpenL2CAPChannelAfterDisconnectIsIgnored {
  GNCPendingL2CAPFakePeripheral *pendingPeripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  pendingPeripheral.shouldPendOpenL2CAPChannel = YES;
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  pendingPeripheral.peripheralDelegate = multiplexer;
  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Request disconnection handler should be called"];

  __block NSUInteger completionCount = 0;
  [self.l2capClient openL2CAPChannelWithPSM:123
                                 peripheral:pendingPeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            completionCount++;
                          }];

  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  CBL2CAPChannel *channel = [[CBL2CAPChannel alloc] init];
  [channel setValue:fakeStreams.inputStream forKey:@"inputStream"];
  [channel setValue:fakeStreams.outputStream forKey:@"outputStream"];

  // Enqueue disconnect followed immediately by a queued didOpenL2CAPChannel: on the client queue.
  [self.l2capClient disconnect];
  [self.l2capClient peripheral:(CBPeripheral *)pendingPeripheral
           didOpenL2CAPChannel:channel
                         error:nil];

  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:kTestTimeout];

  // Also broadcast through the multiplexer after disconnect has completed.
  [multiplexer peripheral:(CBPeripheral *)pendingPeripheral
      didOpenL2CAPChannel:channel
                    error:nil];

  XCTestExpectation *flushExpectation = [self expectationWithDescription:@"Queue flushed"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [flushExpectation fulfill];
  });
  [self waitForExpectations:@[ flushExpectation ] timeout:kTestTimeout];

  XCTAssertEqual(completionCount, 0u);
  pendingPeripheral.peripheralDelegate = nil;
  self.l2capClient = nil;
  [fakeStreams tearDown];
}

- (void)testMultiplexedClientUnregistersAfterOpenAndIgnoresSubsequentBroadcast {
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  self.fakePeripheral.peripheralDelegate = multiplexer;

  __block BOOL client1Disconnected = NO;
  GNCBLEL2CAPClient *client1 =
      [[GNCBLEL2CAPClient alloc] initWithQueue:nil
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral) {
                     client1Disconnected = YES;
                   }];
  GNCBLEL2CAPClient *client2 =
      [[GNCBLEL2CAPClient alloc] initWithQueue:nil
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral){
                   }];

  GNCBLEL2CAPFakeInputOutputStream *firstFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  self.fakePeripheral.channelInputStream = firstFakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = firstFakeStreams.outputStream;

  XCTestExpectation *client1OpenExpectation =
      [self expectationWithDescription:@"Client 1 opened L2CAP channel"];
  __block NSUInteger client1CompletionCount = 0;
  __block GNCBLEL2CAPStream *client1Stream = nil;
  [client1 openL2CAPChannelWithPSM:123
                        peripheral:self.fakePeripheral
                 completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                   client1CompletionCount++;
                   XCTAssertNil(error);
                   XCTAssertNotNil(stream);
                   client1Stream = stream;
                   [client1OpenExpectation fulfill];
                 }];

  [self waitForExpectations:@[ client1OpenExpectation ] timeout:kTestTimeout];
  XCTAssertEqual(client1CompletionCount, 1u);
  XCTAssertNotNil(client1Stream);

  // Open a second L2CAP channel on the same multiplexed peripheral while client1 is still alive.
  GNCBLEL2CAPFakeInputOutputStream *secondFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  self.fakePeripheral.channelInputStream = secondFakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = secondFakeStreams.outputStream;

  XCTestExpectation *client2OpenExpectation =
      [self expectationWithDescription:@"Client 2 opened L2CAP channel"];
  __block NSUInteger client2CompletionCount = 0;
  __block GNCBLEL2CAPStream *client2Stream = nil;
  [client2 openL2CAPChannelWithPSM:456
                        peripheral:self.fakePeripheral
                 completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                   client2CompletionCount++;
                   XCTAssertNil(error);
                   XCTAssertNotNil(stream);
                   client2Stream = stream;
                   [client2OpenExpectation fulfill];
                 }];

  [self waitForExpectations:@[ client2OpenExpectation ] timeout:kTestTimeout];

  // Also simulate a callback that was already queued on client1 before unregistration ran.
  GNCBLEL2CAPFakeInputOutputStream *thirdFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  CBL2CAPChannel *duplicateChannel = [[CBL2CAPChannel alloc] init];
  [duplicateChannel setValue:thirdFakeStreams.inputStream forKey:@"inputStream"];
  [duplicateChannel setValue:thirdFakeStreams.outputStream forKey:@"outputStream"];
  [client1 peripheral:(CBPeripheral *)self.fakePeripheral
      didOpenL2CAPChannel:duplicateChannel
                    error:nil];

  XCTestExpectation *flushExpectation = [self expectationWithDescription:@"Queue flushed"];
  dispatch_async(dispatch_get_main_queue(), ^{
    [flushExpectation fulfill];
  });
  [self waitForExpectations:@[ flushExpectation ] timeout:kTestTimeout];

  XCTAssertEqual(client1CompletionCount, 1u);
  XCTAssertFalse(client1Disconnected);
  XCTAssertEqual(client2CompletionCount, 1u);
  XCTAssertNotNil(client2Stream);

  [client1Stream tearDown];
  [client2Stream tearDown];
  client1Stream = nil;
  client2Stream = nil;
  client1 = nil;
  client2 = nil;
  self.fakePeripheral.peripheralDelegate = nil;
  self.fakePeripheral = nil;
  [firstFakeStreams tearDown];
  [secondFakeStreams tearDown];
  [thirdFakeStreams tearDown];
}

- (void)testMultiplexedClientUnregistersAfterOpenError {
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  self.fakePeripheral.peripheralDelegate = multiplexer;
  NSError *expectedError = [NSError errorWithDomain:@"TestErrorDomain" code:1 userInfo:nil];
  self.fakePeripheral.openL2CAPChannelError = expectedError;

  GNCBLEL2CAPClient *client1 =
      [[GNCBLEL2CAPClient alloc] initWithQueue:nil
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral){
                   }];
  GNCBLEL2CAPClient *client2 =
      [[GNCBLEL2CAPClient alloc] initWithQueue:nil
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral){
                   }];

  XCTestExpectation *client1ErrorExpectation =
      [self expectationWithDescription:@"Client 1 failed to open L2CAP channel"];
  __block NSUInteger client1CompletionCount = 0;
  [client1 openL2CAPChannelWithPSM:123
                        peripheral:self.fakePeripheral
                 completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                   client1CompletionCount++;
                   XCTAssertNil(stream);
                   XCTAssertEqualObjects(error, expectedError);
                   [client1ErrorExpectation fulfill];
                 }];

  [self waitForExpectations:@[ client1ErrorExpectation ] timeout:kTestTimeout];
  XCTAssertEqual(client1CompletionCount, 1u);

  self.fakePeripheral.openL2CAPChannelError = nil;
  XCTestExpectation *client2OpenExpectation =
      [self expectationWithDescription:@"Client 2 opened L2CAP channel"];
  [client2 openL2CAPChannelWithPSM:456
                        peripheral:self.fakePeripheral
                 completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                   XCTAssertNil(error);
                   [client2OpenExpectation fulfill];
                 }];

  [self waitForExpectations:@[ client2OpenExpectation ] timeout:kTestTimeout];
  XCTAssertEqual(client1CompletionCount, 1u);

  client1 = nil;
  client2 = nil;
}

- (void)testIgnoresDidOpenL2CAPChannelForDifferentPeripheral {
  GNCPendingL2CAPFakePeripheral *pendingPeripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  pendingPeripheral.shouldPendOpenL2CAPChannel = YES;
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  pendingPeripheral.peripheralDelegate = multiplexer;

  XCTestExpectation *openExpectation =
      [self expectationWithDescription:@"Target peripheral L2CAP channel opened"];
  __block NSUInteger completionCount = 0;
  __block GNCBLEL2CAPStream *openedStream = nil;
  [self.l2capClient openL2CAPChannelWithPSM:123
                                 peripheral:pendingPeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            completionCount++;
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            openedStream = stream;
                            [openExpectation fulfill];
                          }];
  [self flushMainQueue];

  // Deliver a callback for a different peripheral first; the client must stay registered.
  GNCFakePeripheral *otherPeripheral = [[GNCFakePeripheral alloc] init];
  NSError *spuriousError = [NSError errorWithDomain:@"TestErrorDomain" code:2 userInfo:nil];
  [multiplexer peripheral:(CBPeripheral *)otherPeripheral
      didOpenL2CAPChannel:nil
                    error:spuriousError];

  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  CBL2CAPChannel *channel = [[CBL2CAPChannel alloc] init];
  [channel setValue:fakeStreams.inputStream forKey:@"inputStream"];
  [channel setValue:fakeStreams.outputStream forKey:@"outputStream"];
  [multiplexer peripheral:(CBPeripheral *)pendingPeripheral
      didOpenL2CAPChannel:channel
                    error:nil];

  [self waitForExpectations:@[ openExpectation ] timeout:kTestTimeout];
  XCTAssertEqual(completionCount, 1u);

  [openedStream tearDown];
  openedStream = nil;
  pendingPeripheral.peripheralDelegate = nil;
  self.l2capClient = nil;
  [fakeStreams tearDown];
}

- (void)testCloseStreamThenReopenAcrossQueuesDoesNotCancelActiveOpen {
  dispatch_queue_t callerQueue =
      dispatch_queue_create("com.google.nearby.test.medium", DISPATCH_QUEUE_SERIAL);
  GNCFakePeripheral *firstPeripheral = [[GNCFakePeripheral alloc] init];
  GNCPeripheralDelegateMultiplexer *firstMultiplexer =
      [[GNCPeripheralDelegateMultiplexer alloc] init];
  firstPeripheral.peripheralDelegate = firstMultiplexer;
  GNCBLEL2CAPFakeInputOutputStream *firstFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  firstPeripheral.channelInputStream = firstFakeStreams.inputStream;
  firstPeripheral.channelOutputStream = firstFakeStreams.outputStream;

  GNCPendingL2CAPFakePeripheral *secondPeripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  secondPeripheral.shouldPendOpenL2CAPChannel = YES;
  GNCPeripheralDelegateMultiplexer *secondMultiplexer =
      [[GNCPeripheralDelegateMultiplexer alloc] init];
  secondPeripheral.peripheralDelegate = secondMultiplexer;

  NSMutableArray<NSUUID *> *disconnectedIdentifiers = [NSMutableArray array];
  NSLock *disconnectedLock = [[NSLock alloc] init];
  XCTestExpectation *firstDisconnectedExpectation =
      [self expectationWithDescription:@"First peripheral disconnected"];

  GNCBLEL2CAPClient *client = [[GNCBLEL2CAPClient alloc]
      initWithRequestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral) {
        [disconnectedLock lock];
        if (peripheral.identifier) {
          [disconnectedIdentifiers addObject:peripheral.identifier];
        }
        [disconnectedLock unlock];
        if ([peripheral.identifier isEqual:firstPeripheral.identifier]) {
          [firstDisconnectedExpectation fulfill];
        }
      }];

  XCTestExpectation *firstOpenExpectation =
      [self expectationWithDescription:@"First peripheral L2CAP channel opened"];
  __block GNCBLEL2CAPStream *firstStream = nil;
  dispatch_async(callerQueue, ^{
    [client openL2CAPChannelWithPSM:123
                         peripheral:firstPeripheral
                  completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                    XCTAssertNil(error);
                    XCTAssertNotNil(stream);
                    firstStream = stream;
                    [firstOpenExpectation fulfill];
                  }];
  });
  [self waitForExpectations:@[ firstOpenExpectation ] timeout:kTestTimeout];

  // Close the first stream (scheduling async disconnect on client's private queue) and immediately
  // reuse the client from callerQueue to open a channel on secondPeripheral.
  XCTestExpectation *secondOpenRegisteredExpectation =
      [self expectationWithDescription:@"Second open registered on client queue"];
  secondPeripheral.onOpenL2CAPChannel = ^{
    [secondOpenRegisteredExpectation fulfill];
  };
  XCTestExpectation *secondOpenCompletedExpectation =
      [self expectationWithDescription:@"Second peripheral L2CAP channel opened"];
  __block GNCBLEL2CAPStream *secondStream = nil;
  [firstStream close];
  dispatch_async(callerQueue, ^{
    [client openL2CAPChannelWithPSM:456
                         peripheral:secondPeripheral
                  completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                    XCTAssertNil(error);
                    XCTAssertNotNil(stream);
                    secondStream = stream;
                    [secondOpenCompletedExpectation fulfill];
                  }];
  });

  [self waitForExpectations:@[ firstDisconnectedExpectation, secondOpenRegisteredExpectation ]
                    timeout:kTestTimeout];

  GNCBLEL2CAPFakeInputOutputStream *secondFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  CBL2CAPChannel *secondChannel = [[CBL2CAPChannel alloc] init];
  [secondChannel setValue:secondFakeStreams.inputStream forKey:@"inputStream"];
  [secondChannel setValue:secondFakeStreams.outputStream forKey:@"outputStream"];
  [secondMultiplexer peripheral:(CBPeripheral *)secondPeripheral
            didOpenL2CAPChannel:secondChannel
                          error:nil];

  [self waitForExpectations:@[ secondOpenCompletedExpectation ] timeout:kTestTimeout];

  [disconnectedLock lock];
  NSArray<NSUUID *> *recordedDisconnects = [disconnectedIdentifiers copy];
  [disconnectedLock unlock];
  XCTAssertEqualObjects(recordedDisconnects, @[ firstPeripheral.identifier ]);

  [secondStream tearDown];
  firstStream = nil;
  secondStream = nil;
  firstPeripheral.peripheralDelegate = nil;
  secondPeripheral.peripheralDelegate = nil;
  client = nil;
  [firstFakeStreams tearDown];
  [secondFakeStreams tearDown];
}

- (void)testMultiplexedGNCPeripheralCallbackFromBackgroundQueueDispatchesToClientQueue {
  static const char kClientQueueKey = 0;
  dispatch_queue_t clientQueue =
      dispatch_queue_create("com.google.nearby.test.l2capClient", DISPATCH_QUEUE_SERIAL);
  dispatch_queue_set_specific(clientQueue, &kClientQueueKey, (void *)&kClientQueueKey, NULL);
  dispatch_queue_t callbackQueue =
      dispatch_queue_create("com.google.nearby.test.multiplexerCallback", DISPATCH_QUEUE_SERIAL);

  GNCPendingL2CAPFakePeripheral *pendingPeripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  pendingPeripheral.shouldPendOpenL2CAPChannel = YES;
  XCTestExpectation *registeredExpectation =
      [self expectationWithDescription:@"Client registered on multiplexer"];
  pendingPeripheral.onOpenL2CAPChannel = ^{
    [registeredExpectation fulfill];
  };
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  pendingPeripheral.peripheralDelegate = multiplexer;

  GNCBLEL2CAPClient *client =
      [[GNCBLEL2CAPClient alloc] initWithQueue:clientQueue
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull peripheral){
                   }];

  XCTestExpectation *openExpectation =
      [self expectationWithDescription:@"L2CAP opened via gnc_peripheral callback"];
  __block NSUInteger completionCount = 0;
  __block GNCBLEL2CAPStream *openedStream = nil;
  [client openL2CAPChannelWithPSM:123
                       peripheral:pendingPeripheral
                completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                  completionCount++;
                  XCTAssertEqual(dispatch_get_specific(&kClientQueueKey), &kClientQueueKey);
                  XCTAssertNil(error);
                  XCTAssertNotNil(stream);
                  openedStream = stream;
                  [openExpectation fulfill];
                }];

  [self waitForExpectations:@[ registeredExpectation ] timeout:kTestTimeout];

  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  CBL2CAPChannel *channel = [[CBL2CAPChannel alloc] init];
  [channel setValue:fakeStreams.inputStream forKey:@"inputStream"];
  [channel setValue:fakeStreams.outputStream forKey:@"outputStream"];
  dispatch_async(callbackQueue, ^{
    [multiplexer gnc_peripheral:pendingPeripheral didOpenL2CAPChannel:channel error:nil];
  });

  [self waitForExpectations:@[ openExpectation ] timeout:kTestTimeout];

  // Ensure client unregistered itself from the multiplexer after completion.
  dispatch_sync(callbackQueue, ^{
    [multiplexer gnc_peripheral:pendingPeripheral didOpenL2CAPChannel:channel error:nil];
  });
  dispatch_sync(clientQueue, ^{
                });
  XCTAssertEqual(completionCount, 1u);

  [openedStream tearDown];
  openedStream = nil;
  pendingPeripheral.peripheralDelegate = nil;
  client = nil;
  [fakeStreams tearDown];
}

- (void)testReopenSamePeripheralWhilePreviousStreamClosesDoesNotLoseOpen {
  dispatch_queue_t clientQueue =
      dispatch_queue_create("com.google.nearby.test.l2capClientReopen", DISPATCH_QUEUE_SERIAL);
  GNCPendingL2CAPFakePeripheral *peripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  peripheral.peripheralDelegate = multiplexer;
  GNCBLEL2CAPFakeInputOutputStream *firstFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  peripheral.channelInputStream = firstFakeStreams.inputStream;
  peripheral.channelOutputStream = firstFakeStreams.outputStream;

  __block NSUInteger disconnectCount = 0;
  GNCBLEL2CAPClient *client =
      [[GNCBLEL2CAPClient alloc] initWithQueue:clientQueue
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull disconnectedPeripheral) {
                     disconnectCount++;
                   }];

  XCTestExpectation *firstOpenExpectation =
      [self expectationWithDescription:@"First L2CAP channel opened"];
  __block GNCBLEL2CAPStream *firstStream = nil;
  [client openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                  XCTAssertNil(error);
                  XCTAssertNotNil(stream);
                  firstStream = stream;
                  [firstOpenExpectation fulfill];
                }];
  [self waitForExpectations:@[ firstOpenExpectation ] timeout:kTestTimeout];

  // Start a second L2CAP open on the same peripheral before the first stream closes, then close the
  // first stream while the second open is pending.
  peripheral.shouldPendOpenL2CAPChannel = YES;
  XCTestExpectation *secondOpenExpectation =
      [self expectationWithDescription:@"Second L2CAP channel opened"];
  __block GNCBLEL2CAPStream *secondStream = nil;
  [client openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                  XCTAssertNil(error);
                  XCTAssertNotNil(stream);
                  secondStream = stream;
                  [secondOpenExpectation fulfill];
                }];
  [firstStream close];
  dispatch_sync(clientQueue, ^{
                });

  GNCBLEL2CAPFakeInputOutputStream *secondFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:1];
  CBL2CAPChannel *secondChannel = [[CBL2CAPChannel alloc] init];
  [secondChannel setValue:secondFakeStreams.inputStream forKey:@"inputStream"];
  [secondChannel setValue:secondFakeStreams.outputStream forKey:@"outputStream"];
  [multiplexer gnc_peripheral:peripheral didOpenL2CAPChannel:secondChannel error:nil];

  [self waitForExpectations:@[ secondOpenExpectation ] timeout:kTestTimeout];
  XCTAssertEqual(disconnectCount, 0u);

  [secondStream tearDown];
  firstStream = nil;
  secondStream = nil;
  peripheral.peripheralDelegate = nil;
  client = nil;
  [firstFakeStreams tearDown];
  [secondFakeStreams tearDown];
}

- (void)testConcurrentClientsOnSameMultiplexedPeripheralFilterByPSM {
  dispatch_queue_t client1Queue =
      dispatch_queue_create("com.google.nearby.test.l2capClient1", DISPATCH_QUEUE_SERIAL);
  dispatch_queue_t client2Queue =
      dispatch_queue_create("com.google.nearby.test.l2capClient2", DISPATCH_QUEUE_SERIAL);
  GNCPendingL2CAPFakePeripheral *peripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  peripheral.shouldPendOpenL2CAPChannel = YES;
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  peripheral.peripheralDelegate = multiplexer;

  XCTestExpectation *bothRegisteredExpectation =
      [self expectationWithDescription:@"Both clients called openL2CAPChannel"];
  bothRegisteredExpectation.expectedFulfillmentCount = 2;
  peripheral.onOpenL2CAPChannel = ^{
    [bothRegisteredExpectation fulfill];
  };

  GNCBLEL2CAPClient *client1 =
      [[GNCBLEL2CAPClient alloc] initWithQueue:client1Queue
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull __unused p){
                   }];
  GNCBLEL2CAPClient *client2 =
      [[GNCBLEL2CAPClient alloc] initWithQueue:client2Queue
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nonnull __unused p){
                   }];

  XCTestExpectation *client1OpenExpectation =
      [self expectationWithDescription:@"Client 1 opened PSM 123"];
  __block NSUInteger client1CompletionCount = 0;
  __block GNCBLEL2CAPStream *client1Stream = nil;
  [client1 openL2CAPChannelWithPSM:123
                        peripheral:peripheral
                 completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                   client1CompletionCount++;
                   XCTAssertNil(error);
                   XCTAssertNotNil(stream);
                   client1Stream = stream;
                   [client1OpenExpectation fulfill];
                 }];

  XCTestExpectation *client2OpenExpectation =
      [self expectationWithDescription:@"Client 2 opened PSM 456"];
  __block NSUInteger client2CompletionCount = 0;
  __block GNCBLEL2CAPStream *client2Stream = nil;
  [client2 openL2CAPChannelWithPSM:456
                        peripheral:peripheral
                 completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                   client2CompletionCount++;
                   XCTAssertNil(error);
                   XCTAssertNotNil(stream);
                   client2Stream = stream;
                   [client2OpenExpectation fulfill];
                 }];

  [self waitForExpectations:@[ bothRegisteredExpectation ] timeout:kTestTimeout];

  // Broadcast PSM 123 first while both clients have overlapping open attempts on the multiplexer.
  GNCBLEL2CAPFakeInputOutputStream *firstFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:64];
  GNCFakeCBL2CAPChannel *firstChannel = [[GNCFakeCBL2CAPChannel alloc] init];
  firstChannel.inputStream = firstFakeStreams.inputStream;
  firstChannel.outputStream = firstFakeStreams.outputStream;
  firstChannel.PSM = 123;
  [multiplexer peripheral:(CBPeripheral *)peripheral
      didOpenL2CAPChannel:(CBL2CAPChannel *)firstChannel
                    error:nil];

  [self waitForExpectations:@[ client1OpenExpectation ] timeout:kTestTimeout];
  dispatch_sync(client2Queue, ^{
                });
  XCTAssertEqual(client1CompletionCount, 1u);
  XCTAssertEqual(client2CompletionCount, 0u);

  // Now broadcast PSM 456; client2 must still be listening and wrap its own channel streams.
  GNCBLEL2CAPFakeInputOutputStream *secondFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:64];
  GNCFakeCBL2CAPChannel *secondChannel = [[GNCFakeCBL2CAPChannel alloc] init];
  secondChannel.inputStream = secondFakeStreams.inputStream;
  secondChannel.outputStream = secondFakeStreams.outputStream;
  secondChannel.PSM = 456;
  [multiplexer peripheral:(CBPeripheral *)peripheral
      didOpenL2CAPChannel:(CBL2CAPChannel *)secondChannel
                    error:nil];

  [self waitForExpectations:@[ client2OpenExpectation ] timeout:kTestTimeout];
  dispatch_sync(client1Queue, ^{
                });
  XCTAssertEqual(client1CompletionCount, 1u);
  XCTAssertEqual(client2CompletionCount, 1u);

  // Verify neither stream was corrupted by duplicate stream wrapping.
  NSData *payload = [@"ping" dataUsingEncoding:NSUTF8StringEncoding];
  XCTestExpectation *client1SendExpectation =
      [self expectationWithDescription:@"Client 1 stream writes data"];
  XCTestExpectation *client2SendExpectation =
      [self expectationWithDescription:@"Client 2 stream writes data"];
  [client1Stream sendData:payload
          completionBlock:^(BOOL result) {
            XCTAssertTrue(result);
            [client1SendExpectation fulfill];
          }];
  [client2Stream sendData:payload
          completionBlock:^(BOOL result) {
            XCTAssertTrue(result);
            [client2SendExpectation fulfill];
          }];
  [self waitForExpectations:@[ client1SendExpectation, client2SendExpectation ]
                    timeout:kTestTimeout];

  [client1Stream tearDown];
  [client2Stream tearDown];
  client1Stream = nil;
  client2Stream = nil;
  peripheral.peripheralDelegate = nil;
  client1 = nil;
  client2 = nil;
  [firstFakeStreams tearDown];
  [secondFakeStreams tearDown];
}

- (void)testOpenL2CAPChannelAbortsIfAlreadyDisconnected {
  dispatch_queue_t clientQueue =
      dispatch_queue_create("com.google.nearby.test.l2capDisconnected", DISPATCH_QUEUE_SERIAL);
  GNCPendingL2CAPFakePeripheral *peripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  peripheral.shouldPendOpenL2CAPChannel = YES;
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  peripheral.peripheralDelegate = multiplexer;

  XCTestExpectation *disconnectExpectation =
      [self expectationWithDescription:@"Disconnect handler called"];
  GNCBLEL2CAPClient *client =
      [[GNCBLEL2CAPClient alloc] initWithQueue:clientQueue
                   requestDisconnectionHandler:^(id<GNCPeripheral> _Nullable disconnected) {
                     if (!disconnected) {
                       [disconnectExpectation fulfill];
                     }
                   }];

  XCTestExpectation *abortedOpenExpectation =
      [self expectationWithDescription:@"Pre-disconnected open fails with error"];
  __block NSUInteger completionCount = 0;
  // Enqueue -disconnect before openL2CAPChannelWithPSM:'s async block runs on clientQueue.
  [client disconnect];
  [client openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                  completionCount++;
                  XCTAssertNil(stream);
                  XCTAssertNotNil(error);
                  XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                  XCTAssertEqual(error.code, GNCBLEErrorUnknown);
                  [abortedOpenExpectation fulfill];
                }];

  [self waitForExpectations:@[ disconnectExpectation, abortedOpenExpectation ]
                    timeout:kTestTimeout];
  dispatch_sync(clientQueue, ^{
                });
  XCTAssertEqual(peripheral.openedPSMs.count, 0u);
  XCTAssertEqual(completionCount, 1u);

  // Verify the client never registered on the multiplexer or wrapped subsequent channel streams.
  GNCBLEL2CAPFakeInputOutputStream *fakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:16];
  GNCFakeCBL2CAPChannel *channel = [[GNCFakeCBL2CAPChannel alloc] init];
  channel.inputStream = fakeStreams.inputStream;
  channel.outputStream = fakeStreams.outputStream;
  channel.PSM = 123;
  [multiplexer peripheral:(CBPeripheral *)peripheral
      didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                    error:nil];
  dispatch_sync(clientQueue, ^{
                });

  XCTAssertEqual(completionCount, 1u);
  XCTAssertNil(fakeStreams.inputStream.delegate);
  XCTAssertNil(fakeStreams.outputStream.delegate);

  // Verify the client remains reusable on subsequent openL2CAPChannelWithPSM: calls.
  XCTestExpectation *reopenExpectation =
      [self expectationWithDescription:@"Subsequent open succeeds after one-shot cancellation"];
  __block GNCBLEL2CAPStream *reopenedStream = nil;
  [client openL2CAPChannelWithPSM:123
                       peripheral:peripheral
                completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                  XCTAssertNil(error);
                  XCTAssertNotNil(stream);
                  reopenedStream = stream;
                  [reopenExpectation fulfill];
                }];
  dispatch_sync(clientQueue, ^{
                });
  XCTAssertEqualObjects(peripheral.openedPSMs, @[ @123 ]);
  [multiplexer peripheral:(CBPeripheral *)peripheral
      didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                    error:nil];
  [self waitForExpectations:@[ reopenExpectation ] timeout:kTestTimeout];

  [reopenedStream tearDown];
  reopenedStream = nil;
  peripheral.peripheralDelegate = nil;
  client = nil;
  [fakeStreams tearDown];
}

- (void)testClientCanReopenAfterDisconnectAndCloseL2CAPChannel {
  GNCBLEL2CAPFakeInputOutputStream *firstFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:16];
  self.fakePeripheral.channelInputStream = firstFakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = firstFakeStreams.outputStream;

  XCTestExpectation *firstOpenExpectation =
      [self expectationWithDescription:@"First channel opened"];
  __block GNCBLEL2CAPStream *firstStream = nil;
  [self.l2capClient openL2CAPChannelWithPSM:123
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            firstStream = stream;
                            [firstOpenExpectation fulfill];
                          }];
  [self waitForExpectations:@[ firstOpenExpectation ] timeout:kTestTimeout];

  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Disconnected after first open"];
  [self.l2capClient disconnect];
  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:kTestTimeout];
  [firstStream tearDown];
  firstStream = nil;

  // Reuse the same client after -disconnect.
  GNCBLEL2CAPFakeInputOutputStream *secondFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:16];
  self.fakePeripheral.channelInputStream = secondFakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = secondFakeStreams.outputStream;

  XCTestExpectation *secondOpenExpectation =
      [self expectationWithDescription:@"Second channel opened after disconnect"];
  __block GNCBLEL2CAPStream *secondStream = nil;
  [self.l2capClient openL2CAPChannelWithPSM:456
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            secondStream = stream;
                            [secondOpenExpectation fulfill];
                          }];
  [self waitForExpectations:@[ secondOpenExpectation ] timeout:kTestTimeout];

  self.requestDisconnectionHandlerExpectation =
      [self expectationWithDescription:@"Closed after second open"];
  [self.l2capClient closeL2CAPChannel];
  [self waitForExpectations:@[ self.requestDisconnectionHandlerExpectation ] timeout:kTestTimeout];
  secondStream = nil;

  // Reuse the same client after -closeL2CAPChannel.
  GNCBLEL2CAPFakeInputOutputStream *thirdFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:16];
  self.fakePeripheral.channelInputStream = thirdFakeStreams.inputStream;
  self.fakePeripheral.channelOutputStream = thirdFakeStreams.outputStream;

  XCTestExpectation *thirdOpenExpectation =
      [self expectationWithDescription:@"Third channel opened after closeL2CAPChannel"];
  __block GNCBLEL2CAPStream *thirdStream = nil;
  [self.l2capClient openL2CAPChannelWithPSM:789
                                 peripheral:self.fakePeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            thirdStream = stream;
                            [thirdOpenExpectation fulfill];
                          }];
  [self waitForExpectations:@[ thirdOpenExpectation ] timeout:kTestTimeout];

  [thirdStream tearDown];
  thirdStream = nil;
  self.fakePeripheral.peripheralDelegate = nil;
  self.fakePeripheral = nil;
  self.l2capClient = nil;
  [firstFakeStreams tearDown];
  [secondFakeStreams tearDown];
  [thirdFakeStreams tearDown];
}

- (void)testOverlappingOpenL2CAPChannelFailsSupersededCompletionHandler {
  GNCPendingL2CAPFakePeripheral *firstPeripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  firstPeripheral.shouldPendOpenL2CAPChannel = YES;
  GNCPeripheralDelegateMultiplexer *firstMultiplexer =
      [[GNCPeripheralDelegateMultiplexer alloc] init];
  firstPeripheral.peripheralDelegate = firstMultiplexer;

  GNCPendingL2CAPFakePeripheral *secondPeripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  secondPeripheral.shouldPendOpenL2CAPChannel = YES;
  GNCPeripheralDelegateMultiplexer *secondMultiplexer =
      [[GNCPeripheralDelegateMultiplexer alloc] init];
  secondPeripheral.peripheralDelegate = secondMultiplexer;

  XCTestExpectation *supersededExpectation =
      [self expectationWithDescription:@"First pending open failed when superseded"];
  [self.l2capClient openL2CAPChannelWithPSM:123
                                 peripheral:firstPeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(stream);
                            XCTAssertNotNil(error);
                            XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                            XCTAssertEqual(error.code, GNCBLEErrorUnknown);
                            [supersededExpectation fulfill];
                          }];

  XCTestExpectation *secondOpenExpectation =
      [self expectationWithDescription:@"Second open succeeded"];
  __block GNCBLEL2CAPStream *secondStream = nil;
  [self.l2capClient openL2CAPChannelWithPSM:456
                                 peripheral:secondPeripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(error);
                            XCTAssertNotNil(stream);
                            secondStream = stream;
                            [secondOpenExpectation fulfill];
                          }];

  [self waitForExpectations:@[ supersededExpectation ] timeout:kTestTimeout];
  [self flushMainQueue];

  GNCBLEL2CAPFakeInputOutputStream *secondFakeStreams =
      [[GNCBLEL2CAPFakeInputOutputStream alloc] initWithBufferSize:16];
  GNCFakeCBL2CAPChannel *secondChannel = [[GNCFakeCBL2CAPChannel alloc] init];
  secondChannel.inputStream = secondFakeStreams.inputStream;
  secondChannel.outputStream = secondFakeStreams.outputStream;
  secondChannel.PSM = 456;
  [secondMultiplexer peripheral:(CBPeripheral *)secondPeripheral
            didOpenL2CAPChannel:(CBL2CAPChannel *)secondChannel
                          error:nil];

  [self waitForExpectations:@[ secondOpenExpectation ] timeout:kTestTimeout];

  [secondStream tearDown];
  secondStream = nil;
  firstPeripheral.peripheralDelegate = nil;
  secondPeripheral.peripheralDelegate = nil;
  self.l2capClient = nil;
  [secondFakeStreams tearDown];
}

- (void)testDidOpenL2CAPChannelWithNilChannelSynthesizesFallbackError {
  GNCPendingL2CAPFakePeripheral *peripheral = [[GNCPendingL2CAPFakePeripheral alloc] init];
  peripheral.shouldPendOpenL2CAPChannel = YES;
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  peripheral.peripheralDelegate = multiplexer;

  XCTestExpectation *openFailedExpectation =
      [self expectationWithDescription:@"Nil channel completes with fallback error"];
  [self.l2capClient openL2CAPChannelWithPSM:123
                                 peripheral:peripheral
                          completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                            XCTAssertNil(stream);
                            XCTAssertNotNil(error);
                            XCTAssertEqualObjects(error.domain, GNCBLEErrorDomain);
                            XCTAssertEqual(error.code, GNCBLEErrorUnknown);
                            [openFailedExpectation fulfill];
                          }];
  [self flushMainQueue];

  [multiplexer peripheral:(CBPeripheral *)peripheral didOpenL2CAPChannel:nil error:nil];

  [self waitForExpectations:@[ openFailedExpectation ] timeout:kTestTimeout];
  peripheral.peripheralDelegate = nil;
}

@end
