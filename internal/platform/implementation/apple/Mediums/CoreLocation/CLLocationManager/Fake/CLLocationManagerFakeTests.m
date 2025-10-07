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

#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/CoreLocation/CLLocationManager/Fake/CLLocationManagerDelegateFake.h"
#import "internal/platform/implementation/apple/Mediums/CoreLocation/CLLocationManager/Fake/CLLocationManagerFake.h"

@interface CLLocationManagerFakeTests : XCTestCase
@end

@implementation CLLocationManagerFakeTests {
  CLLocationManagerFake *_fakeLocationManager;
}

- (void)setUp {
  [super setUp];
  _fakeLocationManager = [[CLLocationManagerFake alloc] init];
}

- (void)testRequestWhenInUseAuthorizationGranted {
  // GIVEN
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedWhenInUse];

  // WHEN
  [_fakeLocationManager requestWhenInUseAuthorization];

  // THEN
  if (@available(iOS 14.0, *)) {
    XCTAssertEqual([_fakeLocationManager authorizationStatus], kCLAuthorizationStatusAuthorizedWhenInUse);
  }
}

- (void)testRequestWhenInUseAuthorizationDenied {
  // GIVEN
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusDenied];

  // WHEN
  [_fakeLocationManager requestWhenInUseAuthorization];

  // THEN
  if (@available(iOS 14.0, *)) {
    XCTAssertEqual([_fakeLocationManager authorizationStatus], kCLAuthorizationStatusDenied);
  }
}

- (void)testRequestAlwaysAuthorizationGranted {
  // GIVEN
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusAuthorizedAlways];

  // WHEN
  [_fakeLocationManager requestAlwaysAuthorization];

  // THEN
  if (@available(iOS 14.0, *)) {
    XCTAssertEqual([_fakeLocationManager authorizationStatus], kCLAuthorizationStatusAuthorizedAlways);
  }
}

- (void)testRequestAlwaysAuthorizationDenied {
  // GIVEN
  [_fakeLocationManager setAuthorizationStatus:kCLAuthorizationStatusDenied];

  // WHEN
  [_fakeLocationManager requestAlwaysAuthorization];

  // THEN
  if (@available(iOS 14.0, *)) {
    XCTAssertEqual([_fakeLocationManager authorizationStatus], kCLAuthorizationStatusDenied);
  }
}

- (void)testCallsDelegateAfterRequestingAuthorization {
  // GIVEN
  CLAuthorizationStatus expectedStatus = kCLAuthorizationStatusAuthorizedAlways;
  [_fakeLocationManager setAuthorizationStatus:expectedStatus];

  XCTestExpectation *delegateExpectation =
      [self expectationWithDescription:@"Delegate should have been called."];
  CLLocationManagerDelegateFake *fakeDelegate = [[CLLocationManagerDelegateFake alloc] init];
  fakeDelegate.authorizationUpdateBlock = ^(CLAuthorizationStatus newStatus) {
    XCTAssertEqual(newStatus, expectedStatus);
    [delegateExpectation fulfill];
  };
  [_fakeLocationManager setDelegate:fakeDelegate];

  // WHEN
  [_fakeLocationManager requestAlwaysAuthorization];

  // THEN
  [self waitForExpectations:@[ delegateExpectation ]];
}

@end
