// Copyright 2023 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANMedium.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANServerSocket+Internal.h"
#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANSocket.h"
#import "GoogleToolboxForMac/GTMLogger.h"

// An arbitrary timeout that should be pretty lenient.
NSTimeInterval const GNCConnectionTimeoutInSeconds = 2;

// This doesn't start flaking until 0.000005 seconds, so 0.5 should be plenty of time.
NSTimeInterval const GNCStartDiscoveryTimeoutInSeconds = 0.5;

BOOL GNCHasLoopbackInterface(nw_browse_result_t browseResult) {
  __block BOOL isLoopback = NO;
  nw_browse_result_enumerate_interfaces(browseResult, ^bool(nw_interface_t interface) {
    nw_interface_type_t type = nw_interface_get_type(interface);
    if (type == nw_interface_type_loopback) {
      isLoopback = YES;
      return NO;
    }
    return YES;
  });
  return isLoopback;
}

NSDictionary<NSString *, NSString *> *GNCTXTRecordForBrowseResult(nw_browse_result_t browseResult) {
  nw_txt_record_t txt_record = nw_browse_result_copy_txt_record_object(browseResult);
  NSMutableDictionary<NSString *, NSString *> *txtRecords =
      [[NSMutableDictionary<NSString *, NSString *> alloc] init];
  nw_txt_record_apply(txt_record, ^bool(const char *key, const nw_txt_record_find_key_t found,
                                        const uint8_t *value, const size_t value_len) {
    if (found == nw_txt_record_find_key_non_empty_value) {
      NSString *valueString = [[NSString alloc] initWithBytes:value
                                                       length:value_len
                                                     encoding:NSUTF8StringEncoding];
      [txtRecords setValue:valueString forKey:@(key)];
    }
    return YES;
  });
  return txtRecords;
}

@implementation GNCWiFiLANMedium {
  // Holds a weak reference to a server socket that is retrievable by port. This allows us to stop
  // advertisements for a given port without taking a strong reference. This keeps the ownership
  // of the server socket's lifetime with the caller of listenForServiceOnPort:error:.
  //
  // Usage of .count should be avoided. Zombie keys won't show up in -keyEnumerator, but WILL be
  // included in .count until the next time that the internal hashtable is resized.
  NSMapTable<NSNumber *, GNCWiFiLANServerSocket *> *_serverSockets;

  // Maps a Bonjour service type to its browser. This allows us to stop the discovery given the
  // service type. We maintain ownership of the browser's lifetime, so we can maintain a strong
  // reference.
  NSMutableDictionary<NSString *, nw_browser_t> *_serviceBrowsers;
}

- (instancetype)init {
  if (self = [super init]) {
    _serverSockets = [NSMapTable strongToWeakObjectsMapTable];
    _serviceBrowsers = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (GNCWiFiLANServerSocket *)listenForServiceOnPort:(NSInteger)port error:(NSError **)error {
  GNCWiFiLANServerSocket *serverSocket = [[GNCWiFiLANServerSocket alloc] initWithPort:port];
  BOOL success = [serverSocket startListeningWithError:error];
  if (success) {
    [_serverSockets setObject:serverSocket forKey:@(serverSocket.port)];
    return serverSocket;
  }
  return nil;
}

- (void)startAdvertisingPort:(NSInteger)port
                 serviceName:(NSString *)serviceName
                 serviceType:(NSString *)serviceType
                  txtRecords:(NSDictionary<NSString *, NSString *> *)txtRecords {
  GNCWiFiLANServerSocket *serverSocket = [_serverSockets objectForKey:@(port)];
  [serverSocket startAdvertisingServiceName:serviceName
                                serviceType:serviceType
                                 txtRecords:txtRecords];
}

- (void)stopAdvertisingPort:(NSInteger)port {
  GNCWiFiLANServerSocket *serverSocket = [_serverSockets objectForKey:@(port)];
  [serverSocket stopAdvertising];
}

- (BOOL)startDiscoveryForServiceType:(NSString *)serviceType
                 serviceFoundHandler:(ServiceUpdateHandler)serviceFoundHandler
                  serviceLostHandler:(ServiceUpdateHandler)serviceLostHandler
                               error:(NSError **)error {
  if ([_serviceBrowsers objectForKey:serviceType] != nil) {
    if (error != nil) {
      *error = [NSError errorWithDomain:GNCWiFiLANErrorDomain
                                   code:GNCWiFiLANErrorDuplicateDiscovererForServiceType
                               userInfo:nil];
    }
    return NO;
  }

  // Create a parameters object configured to support TCP. TLS MUST be disabled for Nearby to
  // function properly. This also is set to include peer-to-peer, but unsure if it's required.
  nw_parameters_t parameters =
      nw_parameters_create_secure_tcp(/*tls*/ NW_PARAMETERS_DISABLE_PROTOCOL,
                                      /*tcp*/ NW_PARAMETERS_DEFAULT_CONFIGURATION);
  nw_parameters_set_include_peer_to_peer(parameters, true);
  nw_browse_descriptor_t descriptor =
      nw_browse_descriptor_create_bonjour_service([serviceType UTF8String], /*domain=*/nil);
  nw_browse_descriptor_set_include_txt_record(descriptor, YES);
  nw_browser_t browser = nw_browser_create(descriptor, parameters);

  nw_browser_set_queue(browser, dispatch_get_main_queue());

  nw_browser_set_browse_results_changed_handler(browser, ^(nw_browse_result_t old_result,
                                                           nw_browse_result_t new_result,
                                                           bool batch_complete) {
    nw_browse_result_change_t change = nw_browse_result_get_changes(old_result, new_result);
    switch (change) {
      case nw_browse_result_change_identical:
      case nw_browse_result_change_interface_added:
      case nw_browse_result_change_interface_removed:
        break;
      case nw_browse_result_change_result_added: {
        // If the service has a loopback interface, we probably discovered our own
        // advertisement, so we ignore it.
        BOOL hasLoopback = GNCHasLoopbackInterface(new_result);
        if (hasLoopback) {
          GTMLoggerInfo(@"Ignoring a service discovered with loopback interface.");
          break;
        }
        nw_endpoint_t endpoint = nw_browse_result_copy_endpoint(new_result);
        NSString *name = @(nw_endpoint_get_bonjour_service_name(endpoint));
        NSDictionary<NSString *, NSString *> *txtRecords = GNCTXTRecordForBrowseResult(new_result);
        serviceFoundHandler(name, txtRecords);
        break;
      }
      case nw_browse_result_change_txt_record_changed: {
        // If the service has a loopback interface, we probably lost our own advertisement, so
        // we ignore it.
        BOOL oldHasLoopback = GNCHasLoopbackInterface(old_result);
        BOOL newHasLoopback = GNCHasLoopbackInterface(new_result);
        if (oldHasLoopback || newHasLoopback) {
          GTMLoggerInfo(@"Ignoring a service change with loopback interface.");
          break;
        }
        nw_endpoint_t old_endpoint = nw_browse_result_copy_endpoint(old_result);
        NSString *oldName = @(nw_endpoint_get_bonjour_service_name(old_endpoint));
        NSDictionary<NSString *, NSString *> *oldTXTRecords =
            GNCTXTRecordForBrowseResult(old_result);
        serviceLostHandler(oldName, oldTXTRecords);

        nw_endpoint_t new_endpoint = nw_browse_result_copy_endpoint(new_result);
        NSString *newName = @(nw_endpoint_get_bonjour_service_name(new_endpoint));
        NSDictionary<NSString *, NSString *> *newTXTRecords =
            GNCTXTRecordForBrowseResult(new_result);
        serviceFoundHandler(newName, newTXTRecords);
        break;
      }
      case nw_browse_result_change_result_removed: {
        // If the service has a loopback interface, we probably lost our own advertisement, so
        // we ignore it.
        BOOL hasLoopback = GNCHasLoopbackInterface(old_result);
        if (hasLoopback) {
          GTMLoggerInfo(@"Ignoring a service lost with loopback interface.");
          break;
        }
        nw_endpoint_t endpoint = nw_browse_result_copy_endpoint(old_result);
        NSString *name = @(nw_endpoint_get_bonjour_service_name(endpoint));
        NSDictionary<NSString *, NSString *> *txtRecords = GNCTXTRecordForBrowseResult(old_result);
        serviceLostHandler(name, txtRecords);
        break;
      }
    }
  });

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];

  __block nw_browser_state_t blockResult = nw_browser_state_invalid;
  __block NSError *blockError = nil;

  nw_browser_set_state_changed_handler(browser, ^(nw_browser_state_t state, nw_error_t error) {
    [condition lock];
    blockResult = state;
    if (error != nil) {
      blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(error);
    }
    [condition signal];
    [condition unlock];
  });

  [_serviceBrowsers setObject:browser forKey:serviceType];
  nw_browser_start(browser);

  BOOL didSignal = [condition
      waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:GNCStartDiscoveryTimeoutInSeconds]];
  [condition unlock];

  // We timed out waiting for the connection to transition into a state.
  if (!didSignal) {
    [self stopDiscoveryForServiceType:serviceType];
    if (error != nil) {
      *error = [NSError errorWithDomain:GNCWiFiLANErrorDomain
                                   code:GNCWiFiLANErrorTimedOut
                               userInfo:nil];
    }
    return NO;
  }

  if (error != nil) {
    *error = blockError;
  }

  switch (blockResult) {
    case nw_browser_state_invalid:
    case nw_browser_state_waiting:
    case nw_browser_state_failed:
    case nw_browser_state_cancelled:
      return NO;
    case nw_browser_state_ready:
      return YES;
  }
}

- (void)stopDiscoveryForServiceType:(nonnull NSString *)serviceType {
  nw_browser_t browser = [_serviceBrowsers objectForKey:serviceType];
  [_serviceBrowsers removeObjectForKey:serviceType];
  nw_browser_cancel(browser);
}

- (GNCWiFiLANSocket *)connectToServiceName:(NSString *)serviceName
                               serviceType:(NSString *)serviceType
                                     error:(NSError **)error {
  nw_endpoint_t endpoint = nw_endpoint_create_bonjour_service([serviceName UTF8String],
                                                              [serviceType UTF8String], "local");
  return [self connectToEndpoint:endpoint error:error];
}

- (GNCWiFiLANSocket *)connectToHost:(GNCIPv4Address *)host
                               port:(NSInteger)port
                              error:(NSError **)error {
  nw_endpoint_t endpoint =
      nw_endpoint_create_host(host.dottedRepresentation.UTF8String, @(port).stringValue.UTF8String);
  return [self connectToEndpoint:endpoint error:error];
}

- (GNCWiFiLANSocket *)connectToEndpoint:(nw_endpoint_t)endpoint error:(NSError **)error {
  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];

  __block nw_connection_state_t blockResult = nw_connection_state_invalid;
  __block NSError *blockError = nil;

  nw_parameters_t parameters =
      nw_parameters_create_secure_tcp(/*tls*/ NW_PARAMETERS_DISABLE_PROTOCOL,
                                      /*tcp*/ NW_PARAMETERS_DEFAULT_CONFIGURATION);
  nw_parameters_set_include_peer_to_peer(parameters, true);
  nw_connection_t connection = nw_connection_create(endpoint, parameters);
  nw_connection_set_queue(connection, dispatch_get_main_queue());
  nw_connection_set_state_changed_handler(
      connection, ^(nw_connection_state_t state, nw_error_t error) {
        [condition lock];
        // Ignore the preparing state, because it is not a final state.
        if (state != nw_connection_state_preparing) {
          blockResult = state;
          if (error != nil) {
            blockError = (__bridge_transfer NSError *)nw_error_copy_cf_error(error);
          }
          [condition signal];
        }
        [condition unlock];
      });
  nw_connection_start(connection);

  BOOL didSignal =
      [condition waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:GNCConnectionTimeoutInSeconds]];
  [condition unlock];

  // We timed out waiting for the connection to transition into a state.
  if (!didSignal) {
    nw_connection_cancel(connection);
    if (error != nil) {
      *error = [NSError errorWithDomain:GNCWiFiLANErrorDomain
                                   code:GNCWiFiLANErrorTimedOut
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
      return nil;
    case nw_connection_state_ready:
      return [[GNCWiFiLANSocket alloc] initWithConnection:connection];
  }
}

@end
