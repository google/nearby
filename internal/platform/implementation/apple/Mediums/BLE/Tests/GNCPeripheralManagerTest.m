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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheralManager.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <XCTest/XCTest.h>
#import <objc/runtime.h>

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheralManager.h"

/** Fake delegate of @c GNCPeripheralManagerDelegate */
@interface GNCFakePeripheralManagerDelegate : NSObject <GNCPeripheralManagerDelegate>
@end

@implementation GNCFakePeripheralManagerDelegate

// Add dummy implementations for protocol methods
- (void)gnc_peripheralManagerDidUpdateState:(id<GNCPeripheralManager>)peripheral {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
                didAddService:(CBService *)service
                        error:(nullable NSError *)error {
}
- (void)gnc_peripheralManagerDidStartAdvertising:(id<GNCPeripheralManager>)peripheral
                                           error:(nullable NSError *)error {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
        didReceiveReadRequest:(CBATTRequest *)request {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
      didReceiveWriteRequests:(NSArray<CBATTRequest *> *)requests {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
    isReadyToSendWriteWithoutResponse:(id)sender {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
    didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
       didPublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(nullable NSError *)error {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
     didUnpublishL2CAPChannel:(CBL2CAPPSM)PSM
                        error:(NSError *)error {
}
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
          didOpenL2CAPChannel:(nullable CBL2CAPChannel *)channel
                        error:(nullable NSError *)error {
}

// CBPeripheralManagerDelegate methods
- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral {
}

@end

@interface GNCPeripheralManagerTest : XCTestCase
@end

@implementation GNCPeripheralManagerTest

- (void)testSetPeripheralDelegate_SetsDelegate {
  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  id<GNCPeripheralManagerDelegate> delegate = [[GNCFakePeripheralManagerDelegate alloc] init];

  fakeManager.peripheralDelegate = delegate;

  XCTAssertEqual(fakeManager.peripheralDelegate, delegate);
}

- (void)testPeripheralDelegate_GetsDelegate {
  GNCFakePeripheralManager *fakeManager = [[GNCFakePeripheralManager alloc] init];
  id<GNCPeripheralManagerDelegate> delegate = [[GNCFakePeripheralManagerDelegate alloc] init];

  // Set the underlying delegate property in the fake.
  fakeManager.peripheralDelegate = delegate;

  // Get IMP of the category method from CBPeripheralManager.
  Method getMethod =
      class_getInstanceMethod([CBPeripheralManager class], @selector(peripheralDelegate));
  id (*getImp)(id, SEL) = (id (*)(id, SEL))method_getImplementation(getMethod);

  // Call the implementation directly on the fake.
  id result = getImp(fakeManager, @selector(peripheralDelegate));

  XCTAssertEqual(result, delegate);
}

@end
