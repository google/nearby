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

#import "internal/platform/implementation/apple/Mediums/CoreLocation/CLLocationManager/Fake/CLLocationManagerFake.h"

#import <CoreLocation/CLLocationManager.h>
#import <CoreLocation/CLLocationManagerDelegate.h>
#import <Foundation/Foundation.h>

@implementation CLLocationManagerFake {
  CLAuthorizationStatus _authorizationStatus;
  id<CLLocationManagerDelegate> _delegate;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _authorizationStatus = kCLAuthorizationStatusNotDetermined;
  }

  return self;
}

- (void)setDelegate:(id<CLLocationManagerDelegate>)newValue {
  _delegate = newValue;
}

- (id<CLLocationManagerDelegate>)delegate {
  return _delegate;
}

- (CLAuthorizationStatus)authorizationStatus {
  if (@available(iOS 14.0, *)) {
    return _authorizationStatus;
  }
  return 0;
}

- (void)requestWhenInUseAuthorization {
  if (@available(iOS 14.0, *)) {
    [self.delegate locationManagerDidChangeAuthorization:self];
  }
}

- (void)requestAlwaysAuthorization {
  if (@available(iOS 14.0, *)) {
    [self.delegate locationManagerDidChangeAuthorization:self];
  }
}

- (void)setAuthorizationStatus:(CLAuthorizationStatus)authorizationStatus {
  _authorizationStatus = authorizationStatus;
}

@end
