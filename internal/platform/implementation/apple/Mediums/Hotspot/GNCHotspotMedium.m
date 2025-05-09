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

#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotMedium.h"

#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>
#import <Network/Network.h>
#if TARGET_OS_IOS
#import <NetworkExtension/NetworkExtension.h>
#endif  // TARGET_OS_IOS
#import <SystemConfiguration/CaptiveNetwork.h>

#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "GoogleToolboxForMac/GTMLogger.h"

#if TARGET_OS_IOS
// The maximum number of retries for connecting to the Hotspot.
static const UInt8 kMaxRetryCount = 3;
// Timeout after 18s for connection attempt. From the stability test, connection take between 8-14s
// on iOS
static const UInt8 kConnectionTimeoutInSeconds = 18;
#endif  // TARGET_OS_IOS

// An arbitrary timeout that should be pretty lenient.
static const UInt8 kConnectionToHostTimeoutInSeconds = 10;

@implementation GNCHotspotMedium

- (BOOL)connectToWifiNetworkWithSSID:(nonnull NSString *)ssid
                            password:(nonnull NSString *)password {
#if TARGET_OS_IOS
  __block BOOL connected = NO;
  NEHotspotConfiguration *config = [[NEHotspotConfiguration alloc] initWithSSID:ssid
                                                                     passphrase:password
                                                                          isWEP:NO];
  config.joinOnce = YES;  // A temporary connection

  dispatch_time_t timeout =
      dispatch_time(DISPATCH_TIME_NOW, kConnectionTimeoutInSeconds * NSEC_PER_SEC);
  for (UInt8 i = 0; i < kMaxRetryCount; i++) {
    dispatch_semaphore_t semaphore_internal = dispatch_semaphore_create(0);
    [[NEHotspotConfigurationManager sharedManager]
        applyConfiguration:config
         completionHandler:^(NSError *_Nullable error) {
           if (error) {
             if ([error.domain isEqualToString:NEHotspotConfigurationErrorDomain] &&
                 error.code == NEHotspotConfigurationErrorAlreadyAssociated) {
               GTMLoggerInfo(@"Already connected to %@", ssid);
               connected = YES;
             } else {
               GTMLoggerError(@"Failed to connect: %@ (%@)", ssid, error.localizedDescription);
             }
           } else {
             GTMLoggerInfo(@"Successfully connected to %@", ssid);
             connected = YES;
           }
           dispatch_semaphore_signal(semaphore_internal);
         }];
    if (dispatch_semaphore_wait(semaphore_internal, timeout) != 0) {
      GTMLoggerError(@"Connecting to %@ timeout in %d seconds", ssid, kConnectionTimeoutInSeconds);
    }
    if (connected) {
      NSString * currentSSID = [self getCurrentWifiSSID];
      if ([currentSSID isEqualToString:ssid]) {
        GTMLoggerDebug(@"Connected to %@ successfully", ssid);
        break;
      } else {
        GTMLoggerError(@"Connected to wrong SSID: %@", currentSSID);
        connected = NO;
      }
    } else {
        [[NEHotspotConfigurationManager sharedManager] removeConfigurationForSSID:ssid];
    }
  }
  return connected;
#else
  GTMLoggerError(@"Not implemented for macOS");
  return NO;
#endif  // TARGET_OS_IOS
}

- (void)disconnectToWifiNetworkWithSSID:(nonnull NSString *)ssid {
#if TARGET_OS_IOS
  [[NEHotspotConfigurationManager sharedManager] removeConfigurationForSSID:ssid];
#else
  GTMLoggerError(@"Not implemented for macOS");
#endif  // TARGET_OS_IOS
}

- (GNCHotspotSocket *)connectToHost:(GNCIPv4Address *)host
                               port:(NSInteger)port
                              error:(NSError **)error {
  // Validate host address
  if (!host.dottedRepresentation.UTF8String) {
    if (error) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorUnknown
                               userInfo:nil];
    }
    GTMLoggerError(@"Invalid host address");
    return nil;
  }

  nw_endpoint_t endpoint =
      nw_endpoint_create_host(host.dottedRepresentation.UTF8String, @(port).stringValue.UTF8String);
  return [self connectToEndpoint:endpoint includePeerToPeer:(BOOL)YES error:error];
}

- (GNCHotspotSocket *)connectToEndpoint:(nw_endpoint_t)endpoint
                      includePeerToPeer:(BOOL)includePeerToPeer
                                  error:(NSError **)error {
  GTMLoggerInfo(@"connectToEndpoint: %@ includePeerToPeer: %d", endpoint.debugDescription,
                includePeerToPeer);

  dispatch_semaphore_t semaphore_internal = dispatch_semaphore_create(0);
  __block nw_connection_state_t blockResult = nw_connection_state_invalid;
  __block NSError *blockError = nil;

  nw_parameters_t parameters =
      nw_parameters_create_secure_tcp(/*tls*/ NW_PARAMETERS_DISABLE_PROTOCOL,
                                      /*tcp*/ NW_PARAMETERS_DEFAULT_CONFIGURATION);

  nw_parameters_set_include_peer_to_peer(parameters, includePeerToPeer);
  nw_connection_t connection = nw_connection_create(endpoint, parameters);

  nw_connection_set_queue(connection, dispatch_get_main_queue());

  nw_connection_set_state_changed_handler(
      connection, ^(nw_connection_state_t state, nw_error_t error) {
        GTMLoggerDebug(@"connectToEndpoint state changed to: %d", state);

        // Ignore the preparing state and waiting state, because it is not a final state.
        if ((state != nw_connection_state_preparing) && (state != nw_connection_state_waiting)) {
          blockResult = state;
          if (error != nil) {
            GTMLoggerError(@"connectToEndpoint Error: %@", error.debugDescription);
            blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(error);
          }
          dispatch_semaphore_signal(semaphore_internal);
        }
      });

  nw_connection_start(connection);
  dispatch_time_t timeout =
      dispatch_time(DISPATCH_TIME_NOW, kConnectionToHostTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore_internal, timeout) != 0) {
    GTMLoggerError(@"Connecting to %@ timeout in %d seconds", endpoint.debugDescription,
                   kConnectionToHostTimeoutInSeconds);
    nw_connection_set_state_changed_handler(connection, nil);  // Prevent callback issues
    nw_connection_cancel(connection);
    if (error != nil) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorTimedOut
                               userInfo:nil];
    }
    return nil;
  }

  if (error != nil) {
    *error = blockError;
  }

  switch (blockResult) {
    case nw_connection_state_invalid:
    case nw_connection_state_waiting:
    case nw_connection_state_preparing:
    case nw_connection_state_failed:
    case nw_connection_state_cancelled:
      GTMLoggerError(@"connectToEndpoint failed with result: %d", blockResult);
      return nil;
    case nw_connection_state_ready:
      return [[GNCHotspotSocket alloc] initWithConnection:connection];
  }
}

- (NSString *)getCurrentWifiSSID {
#if TARGET_OS_IOS
  // Request permission
  CLLocationManager *locationManager = [[CLLocationManager alloc] init];
  [locationManager requestWhenInUseAuthorization];
  GTMLoggerDebug(@"Request Location permission");
  dispatch_semaphore_t semaphore_internal = dispatch_semaphore_create(0);
  __block NSString *networkSSID = nil;

  // Delay 1 seconds to ensure that we got the permission
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (@available(iOS 14.0, *)) {
          [NEHotspotNetwork
              fetchCurrentWithCompletionHandler:^(NEHotspotNetwork *_Nullable network) {
                if (network) {
                  networkSSID = network.SSID;
                  GTMLoggerDebug(@"iOS 14+ Current Wi-Fi SSID: %@", networkSSID);
                } else {
                  GTMLoggerError(@"Failed to get current Wifi SSID");
                }
                dispatch_semaphore_signal(semaphore_internal);
              }];
        } else {
          NSArray<NSString *> *interfaces = CFBridgingRelease(CNCopySupportedInterfaces());
          for (NSString *interface in interfaces) {
            id info =
                CFBridgingRelease(CNCopyCurrentNetworkInfo((__bridge CFStringRef)(interface)));
            networkSSID = [info valueForKey:@"SSID"];
            GTMLoggerDebug(@"Current Wi-Fi SSID: %@", networkSSID);
          }
          dispatch_semaphore_signal(semaphore_internal);
        }
      });
  dispatch_time_t timeout =
      dispatch_time(DISPATCH_TIME_NOW, kConnectionToHostTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore_internal, timeout) != 0) {
    GTMLoggerError(@"Getting current Wifi SSID timeout in %d seconds",
                   kConnectionToHostTimeoutInSeconds);
  }

  return networkSSID;
#else
  GTMLoggerError(@"Not implemented for macOS");
  return nil;
#endif  // TARGET_OS_IOS
}


@end
