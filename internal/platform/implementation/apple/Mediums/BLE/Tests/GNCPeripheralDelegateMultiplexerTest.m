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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralDelegateMultiplexer.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"

@interface FakePeripheralDelegateListener : NSObject <GNCPeripheralDelegate>
@property(nonatomic) BOOL didDiscoverServicesCalled;
@property(nonatomic) BOOL didDiscoverCharacteristicsCalled;
@property(nonatomic) BOOL didUpdateValueCalled;
@property(nonatomic) BOOL didOpenL2CAPChannelCalled;
@property(nonatomic) BOOL cbDidDiscoverServicesCalled;
@property(nonatomic) BOOL cbDidOpenL2CAPChannelCalled;
@end

@implementation FakePeripheralDelegateListener

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral didDiscoverServices:(nullable NSError *)error {
  _didDiscoverServicesCalled = YES;
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error {
  _didDiscoverCharacteristicsCalled = YES;
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error {
  _didUpdateValueCalled = YES;
}

- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didOpenL2CAPChannel:(CBL2CAPChannel *)channel
                  error:(nullable NSError *)error {
  _didOpenL2CAPChannelCalled = YES;
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(nullable NSError *)error {
  _cbDidDiscoverServicesCalled = YES;
}

- (void)peripheral:(CBPeripheral *)peripheral
    didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                  error:(nullable NSError *)error {
  _cbDidOpenL2CAPChannelCalled = YES;
}

@end

@interface GNCPeripheralDelegateMultiplexerTest : XCTestCase
@end

@implementation GNCPeripheralDelegateMultiplexerTest

- (void)testForwardsCallbacksToAllListenersAndStopsAfterRemoval {
  GNCPeripheralDelegateMultiplexer *multiplexer = [[GNCPeripheralDelegateMultiplexer alloc] init];
  FakePeripheralDelegateListener *listener1 = [[FakePeripheralDelegateListener alloc] init];
  FakePeripheralDelegateListener *listener2 = [[FakePeripheralDelegateListener alloc] init];
  GNCFakePeripheral *peripheral = [[GNCFakePeripheral alloc] init];

  [multiplexer addListener:listener1];
  [multiplexer addListener:listener2];

  [multiplexer gnc_peripheral:peripheral didDiscoverServices:nil];
  [multiplexer gnc_peripheral:peripheral
      didDiscoverCharacteristicsForService:(CBService *)[[NSObject alloc] init]
                                     error:nil];
  [multiplexer gnc_peripheral:peripheral
      didUpdateValueForCharacteristic:(CBCharacteristic *)[[NSObject alloc] init]
                                error:nil];
  [multiplexer gnc_peripheral:peripheral
          didOpenL2CAPChannel:[[CBL2CAPChannel alloc] init]
                        error:nil];
  [multiplexer peripheral:(CBPeripheral *)peripheral didDiscoverServices:nil];
  [multiplexer peripheral:(CBPeripheral *)peripheral didOpenL2CAPChannel:nil error:nil];

  XCTAssertTrue(listener1.didDiscoverServicesCalled);
  XCTAssertTrue(listener1.didDiscoverCharacteristicsCalled);
  XCTAssertTrue(listener1.didUpdateValueCalled);
  XCTAssertTrue(listener1.didOpenL2CAPChannelCalled);
  XCTAssertTrue(listener1.cbDidDiscoverServicesCalled);
  XCTAssertTrue(listener1.cbDidOpenL2CAPChannelCalled);
  XCTAssertTrue(listener2.didDiscoverServicesCalled);
  XCTAssertTrue(listener2.didDiscoverCharacteristicsCalled);
  XCTAssertTrue(listener2.didUpdateValueCalled);
  XCTAssertTrue(listener2.didOpenL2CAPChannelCalled);
  XCTAssertTrue(listener2.cbDidDiscoverServicesCalled);
  XCTAssertTrue(listener2.cbDidOpenL2CAPChannelCalled);

  [multiplexer removeListener:listener2];
  listener2.didDiscoverServicesCalled = NO;
  [multiplexer gnc_peripheral:peripheral didDiscoverServices:nil];
  XCTAssertFalse(listener2.didDiscoverServicesCalled);
}

@end
