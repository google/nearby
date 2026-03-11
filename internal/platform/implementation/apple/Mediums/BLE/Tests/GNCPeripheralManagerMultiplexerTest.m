// Copyright 2026 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManagerMultiplexer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheralManager.h"

@interface GNCPeripheralManagerMultiplexerTest : XCTestCase
@end

@interface FakePeripheralManagerDelegate : NSObject <GNCPeripheralManagerDelegate>
@property(nonatomic) XCTestExpectation *expectation;
@property(nonatomic) BOOL didUpdateStateCalled;
@property(nonatomic) CBManagerState state;

@property(nonatomic) BOOL didStartAdvertisingCalled;
@property(nonatomic) NSError *startAdvertisingError;

@property(nonatomic) BOOL didAddServiceCalled;
@property(nonatomic) CBService *addedService;
@property(nonatomic) NSError *addServiceError;

@property(nonatomic) BOOL didReceiveReadRequestCalled;
@property(nonatomic) CBATTRequest *readRequest;

@property(nonatomic) BOOL didPublishL2CAPChannelCalled;
@property(nonatomic) CBL2CAPPSM publishedPSM;
@property(nonatomic) NSError *publishL2CAPChannelError;

@property(nonatomic) BOOL didUnpublishL2CAPChannelCalled;
@property(nonatomic) CBL2CAPPSM unpublishedPSM;
@property(nonatomic) NSError *unpublishL2CAPChannelError;

@property(nonatomic) BOOL didOpenL2CAPChannelCalled;
@property(nonatomic) CBL2CAPChannel *openedChannel;
@property(nonatomic) NSError *openL2CAPChannelError;

@end

@implementation FakePeripheralManagerDelegate

- (void)gnc_peripheralManagerDidUpdateState:(id<GNCPeripheralManager>)peripheral {
  _didUpdateStateCalled = YES;
  _state = peripheral.state;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)gnc_peripheralManagerDidStartAdvertising:(id<GNCPeripheralManager>)peripheral
                                           error:(nullable NSError *)error {
  _didStartAdvertisingCalled = YES;
  _startAdvertisingError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
                didAddService:(CBService *)service
                        error:(nullable NSError *)error {
  _didAddServiceCalled = YES;
  _addedService = service;
  _addServiceError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
        didReceiveReadRequest:(CBATTRequest *)request {
  _didReceiveReadRequestCalled = YES;
  _readRequest = request;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
       didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(nullable NSError *)error {
  _didPublishL2CAPChannelCalled = YES;
  _publishedPSM = PSM;
  _publishL2CAPChannelError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
     didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(NSError *)error {
  _didUnpublishL2CAPChannelCalled = YES;
  _unpublishedPSM = PSM;
  _unpublishL2CAPChannelError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
          didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                        error:(nullable NSError *)error {
  _didOpenL2CAPChannelCalled = YES;
  _openedChannel = channel;
  _openL2CAPChannelError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)peripheralManagerDidUpdateState:(nonnull CBPeripheralManager *)peripheral {
  _didUpdateStateCalled = YES;
  _state = peripheral.state;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(nullable NSError *)error {
  _didStartAdvertisingCalled = YES;
  _startAdvertisingError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
            didAddService:(CBService *)service
                    error:(nullable NSError *)error {
  _didAddServiceCalled = YES;
  _addedService = service;
  _addServiceError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveReadRequest:(CBATTRequest *)request {
  _didReceiveReadRequestCalled = YES;
  _readRequest = request;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
   didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                    error:(nullable NSError *)error {
  _didPublishL2CAPChannelCalled = YES;
  _publishedPSM = PSM;
  _publishL2CAPChannelError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
 didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                    error:(nullable NSError *)error {
  _didUnpublishL2CAPChannelCalled = YES;
  _unpublishedPSM = PSM;
  _unpublishL2CAPChannelError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
      didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                    error:(nullable NSError *)error {
  _didOpenL2CAPChannelCalled = YES;
  _openedChannel = channel;
  _openL2CAPChannelError = error;
  if (_expectation) {
    [_expectation fulfill];
  }
}

@end

@implementation GNCPeripheralManagerMultiplexerTest

- (void)testMultiplexerForwardsCallbacks {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate1 = [[FakePeripheralManagerDelegate alloc] init];
  FakePeripheralManagerDelegate *delegate2 = [[FakePeripheralManagerDelegate alloc] init];

  delegate1.expectation = [self expectationWithDescription:@"Delegate 1 called"];
  delegate2.expectation = [self expectationWithDescription:@"Delegate 2 called"];

  [multiplexer addListener:delegate1];
  [multiplexer addListener:delegate2];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  fakeManager.state = CBManagerStatePoweredOn;

  [multiplexer gnc_peripheralManagerDidUpdateState:fakeManager];

  [self waitForExpectationsWithTimeout:1 handler:nil];

  XCTAssertTrue(delegate1.didUpdateStateCalled);
  XCTAssertTrue(delegate2.didUpdateStateCalled);
  XCTAssertEqual(delegate1.state, CBManagerStatePoweredOn);
  XCTAssertEqual(delegate2.state, CBManagerStatePoweredOn);
}

- (void)testMultiplexerRemovesListener {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate1 = [[FakePeripheralManagerDelegate alloc] init];

  delegate1.expectation = [self expectationWithDescription:@"Delegate 1 called"];

  [multiplexer addListener:delegate1];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  fakeManager.state = CBManagerStatePoweredOn;

  [multiplexer removeListener:delegate1];
  [multiplexer gnc_peripheralManagerDidUpdateState:fakeManager];

  // We expect delegate1 NOT to be called.
  // Since removals are async, we wait a bit to ensure it had a chance (or didn't).
  XCTWaiterResult result = [XCTWaiter waitForExpectations:@[ delegate1.expectation ] timeout:0.5];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
  XCTAssertFalse(delegate1.didUpdateStateCalled);
}

- (void)testMultiplexerForwardsDidStartAdvertising {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  NSError *error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];

  [multiplexer gnc_peripheralManagerDidStartAdvertising:fakeManager error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didStartAdvertisingCalled);
  XCTAssertEqualObjects(delegate.startAdvertisingError, error);
}

- (void)testMultiplexerForwardsDidAddService {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  CBMutableService *service =
      [[CBMutableService alloc] initWithType:[CBUUID UUIDWithString:@"180D"] primary:YES];
  NSError *error = [NSError errorWithDomain:@"test" code:2 userInfo:nil];

  [multiplexer gnc_peripheralManager:fakeManager didAddService:service error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didAddServiceCalled);
  XCTAssertEqualObjects(delegate.addedService, service);
  XCTAssertEqualObjects(delegate.addServiceError, error);
}

- (void)testMultiplexerForwardsDidReceiveReadRequest {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  id request = [NSNull null];  // Use NSNull or any object as placeholder since we can't create
                               // CBATTRequest

  [multiplexer gnc_peripheralManager:fakeManager didReceiveReadRequest:request];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didReceiveReadRequestCalled);
  XCTAssertEqual(delegate.readRequest, request);
}

- (void)testMultiplexerForwardsDidPublishL2CAPChannel {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  CBL2CAPPSM psm = 42;
  NSError *error = [NSError errorWithDomain:@"test" code:3 userInfo:nil];

  [multiplexer gnc_peripheralManager:fakeManager didPublishL2CAPChannel:psm error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didPublishL2CAPChannelCalled);
  XCTAssertEqual(delegate.publishedPSM, psm);
  XCTAssertEqualObjects(delegate.publishL2CAPChannelError, error);
}

- (void)testMultiplexerForwardsDidUnpublishL2CAPChannel {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  CBL2CAPPSM psm = 42;
  NSError *error = [NSError errorWithDomain:@"test" code:4 userInfo:nil];

  [multiplexer gnc_peripheralManager:fakeManager didUnpublishL2CAPChannel:psm error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didUnpublishL2CAPChannelCalled);
  XCTAssertEqual(delegate.unpublishedPSM, psm);
  XCTAssertEqualObjects(delegate.unpublishL2CAPChannelError, error);
}

- (void)testMultiplexerForwardsDidOpenL2CAPChannel {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  id channel = [NSNull null];  // Placeholder
  NSError *error = [NSError errorWithDomain:@"test" code:5 userInfo:nil];

  [multiplexer gnc_peripheralManager:fakeManager didOpenL2CAPChannel:channel error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didOpenL2CAPChannelCalled);
  XCTAssertEqual(delegate.openedChannel, channel);
  XCTAssertEqualObjects(delegate.openL2CAPChannelError, error);
}

- (void)testCBPeripheralManagerDelegateDidUpdateState {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  fakeManager.state = CBManagerStatePoweredOn;

  [multiplexer peripheralManagerDidUpdateState:(CBPeripheralManager *)fakeManager];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didUpdateStateCalled);
  XCTAssertEqual(delegate.state, CBManagerStatePoweredOn);
}

- (void)testCBPeripheralManagerDelegateDidStartAdvertising {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  NSError *error = [NSError errorWithDomain:@"test" code:10 userInfo:nil];

  [multiplexer peripheralManagerDidStartAdvertising:(CBPeripheralManager *)fakeManager error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didStartAdvertisingCalled);
  XCTAssertEqualObjects(delegate.startAdvertisingError, error);
}

- (void)testCBPeripheralManagerDelegateDidAddService {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  CBMutableService *service =
      [[CBMutableService alloc] initWithType:[CBUUID UUIDWithString:@"180F"] primary:YES];
  NSError *error = [NSError errorWithDomain:@"test" code:11 userInfo:nil];

  [multiplexer peripheralManager:(CBPeripheralManager *)fakeManager didAddService:service error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didAddServiceCalled);
  XCTAssertEqualObjects(delegate.addedService, service);
  XCTAssertEqualObjects(delegate.addServiceError, error);
}

- (void)testCBPeripheralManagerDelegateDidReceiveReadRequest {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  id request = [NSNull null];

  [multiplexer peripheralManager:(CBPeripheralManager *)fakeManager didReceiveReadRequest:request];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didReceiveReadRequestCalled);
  XCTAssertEqual(delegate.readRequest, request);
}

- (void)testCBPeripheralManagerDelegateDidPublishL2CAPChannel {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  CBL2CAPPSM psm = 100;
  NSError *error = [NSError errorWithDomain:@"test" code:13 userInfo:nil];

  [multiplexer peripheralManager:(CBPeripheralManager *)fakeManager
          didPublishL2CAPChannel:psm
                           error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didPublishL2CAPChannelCalled);
  XCTAssertEqual(delegate.publishedPSM, psm);
  XCTAssertEqualObjects(delegate.publishL2CAPChannelError, error);
}

- (void)testCBPeripheralManagerDelegateDidUnpublishL2CAPChannel {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  CBL2CAPPSM psm = 101;
  NSError *error = [NSError errorWithDomain:@"test" code:14 userInfo:nil];

  [multiplexer peripheralManager:(CBPeripheralManager *)fakeManager
       didUnpublishL2CAPChannel:psm
                          error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didUnpublishL2CAPChannelCalled);
  XCTAssertEqual(delegate.unpublishedPSM, psm);
  XCTAssertEqualObjects(delegate.unpublishL2CAPChannelError, error);
}

- (void)testCBPeripheralManagerDelegateDidOpenL2CAPChannel {
  GNCPeripheralManagerMultiplexer *multiplexer =
      [[GNCPeripheralManagerMultiplexer alloc] initWithCallbackQueue:dispatch_get_main_queue()];
  FakePeripheralManagerDelegate *delegate = [[FakePeripheralManagerDelegate alloc] init];
  delegate.expectation = [self expectationWithDescription:@"Delegate called"];
  [multiplexer addListener:delegate];

  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  id channel = [NSNull null];
  NSError *error = [NSError errorWithDomain:@"test" code:15 userInfo:nil];

  [multiplexer peripheralManager:(CBPeripheralManager *)fakeManager
            didOpenL2CAPChannel:channel
                          error:error];

  [self waitForExpectationsWithTimeout:1 handler:nil];
  XCTAssertTrue(delegate.didOpenL2CAPChannelCalled);
  XCTAssertEqual(delegate.openedChannel, channel);
  XCTAssertEqualObjects(delegate.openL2CAPChannelError, error);
}

@end
