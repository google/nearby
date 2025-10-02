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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <XCTest/XCTest.h>
#import <objc/runtime.h>

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakePeripheral.h"

/** Fake delegate of @c GNCPeripheralDelegate */
@interface GNCFakePeripheralDelegate : NSObject <GNCPeripheralDelegate>
@end

@implementation GNCFakePeripheralDelegate
@end

@interface GNCPeripheralTest : XCTestCase
@end

@implementation GNCPeripheralTest

- (void)testSetPeripheralDelegate_SetsDelegate {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  id<GNCPeripheralDelegate> delegate = [[GNCFakePeripheralDelegate alloc] init];

  // Get IMP of the category method.
  Method setMethod =
      class_getInstanceMethod([CBPeripheral class], @selector(setPeripheralDelegate:));
  void (*setImp)(id, SEL, id) = (void (*)(id, SEL, id))method_getImplementation(setMethod);

  // Call the implementation directly on the fake.
  setImp(fakePeripheral, @selector(setPeripheralDelegate:), delegate);

  // Verify that the underlying delegate setter was called.
  XCTAssertEqual(fakePeripheral.delegate, delegate);
}

- (void)testPeripheralDelegate_GetsDelegate {
  GNCFakePeripheral *fakePeripheral = [[GNCFakePeripheral alloc] init];
  id<GNCPeripheralDelegate> delegate = [[GNCFakePeripheralDelegate alloc] init];

  // Set the underlying delegate property.
  fakePeripheral.delegate = delegate;

  // Get IMP of the category method.
  Method getMethod = class_getInstanceMethod([CBPeripheral class], @selector(peripheralDelegate));
  id (*getImp)(id, SEL) = (id (*)(id, SEL))method_getImplementation(getMethod);

  // Call the implementation directly and check the result.
  id result = getImp(fakePeripheral, @selector(peripheralDelegate));

  XCTAssertEqual(result, delegate);
}

@end
