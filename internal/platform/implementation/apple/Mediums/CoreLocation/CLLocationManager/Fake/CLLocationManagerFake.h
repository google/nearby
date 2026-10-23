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

#import <CoreLocation/CLLocation.h>
#import <CoreLocation/CLLocationManagerDelegate.h>
#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/** Fake object of CLLocationManager. */
__attribute__((objc_subclassing_restricted))  // NO_LINT
@interface CLLocationManagerFake : CLLocationManager

/// Returns the current authorization status.
- (CLAuthorizationStatus)authorizationStatus;

/// Request permission to access location data when the app is in use.
- (void)requestWhenInUseAuthorization;

/// Request permission to always access location data.
- (void)requestAlwaysAuthorization;

/// Sets the authorization status for location permission.
- (void)setAuthorizationStatus:(CLAuthorizationStatus)authorizationStatus;

@end

NS_ASSUME_NONNULL_END
