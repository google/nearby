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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowseResult.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowseResultImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowser.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWBrowserImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnection.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnectionImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket+Internal.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWParameters.h"

NS_ASSUME_NONNULL_BEGIN

// An arbitrary timeout that should be pretty lenient.
NSTimeInterval const GNCConnectionTimeoutInSeconds = 2;

// This doesn't start flaking until 0.000005 seconds, so 0.5 should be plenty of time.
NSTimeInterval const GNCStartDiscoveryTimeoutInSeconds = 0.5;

BOOL GNCHasLoopbackInterface(nw_browse_result_t browseResult) {
  __block BOOL isLoopback = NO;
  id<GNCNWBrowseResult> wrapper = [GNCNWBrowseResultImpl sharedInstance];
  [wrapper enumerateInterfaces:browseResult
                    usingBlock:^bool(nw_interface_t interface) {
                      nw_interface_type_t type = [wrapper interfaceGetType:interface];
                      if (type == nw_interface_type_loopback) {
                        isLoopback = YES;
                        return NO;
                      }
                      return YES;
                    }];
  return isLoopback;
}

NSDictionary<NSString *, NSString *> *GNCTXTRecordForBrowseResult(nw_browse_result_t browseResult) {
  id<GNCNWBrowseResult> wrapper = [GNCNWBrowseResultImpl sharedInstance];
  nw_txt_record_t txt_record = [wrapper copyTXTRecordObject:browseResult];
  NSMutableDictionary<NSString *, NSString *> *txtRecords =
      [[NSMutableDictionary<NSString *, NSString *> alloc] init];
  [wrapper applyTXTRecord:txt_record
                    block:^bool(const char *key, const nw_txt_record_find_key_t found,
                                const uint8_t *value, const size_t value_len) {
                      if (found == nw_txt_record_find_key_non_empty_value) {
                        NSString *valueString =
                            [[NSString alloc] initWithBytes:value
                                                     length:value_len
                                                   encoding:NSUTF8StringEncoding];
                        [txtRecords setValue:valueString forKey:@(key)];
                      }
                      return YES;
                    }];
  return txtRecords;
}

@interface GNCNWFramework ()
- (nullable GNCNWFrameworkSocket *)internalConnectToEndpoint:(nw_endpoint_t)endpoint
                                                 PSKIdentity:(nullable NSData *)PSKIdentity
                                             PSKSharedSecret:(nullable NSData *)PSKSharedSecret
                                           includePeerToPeer:(BOOL)includePeerToPeer
                                                cancelSource:
                                                    (nullable dispatch_source_t)cancelSource
                                                       queue:(nullable dispatch_queue_t)queue
                                                       error:(NSError **)error;
@end

@implementation GNCNWFramework {
  // Holds a weak reference to a server socket that is retrievable by port. This allows us to stop
  // advertisements for a given port without taking a strong reference. This keeps the ownership
  // of the server socket's lifetime with the caller of listenForServiceOnPort:error:.
  //
  // Usage of .count should be avoided. Zombie keys won't show up in -keyEnumerator, but WILL be
  // included in .count until the next time that the internal hashtable is resized.
  NSMapTable<NSNumber *, GNCNWFrameworkServerSocket *> *_serverSockets;

  // Maps a Bonjour service type to its browser. This allows us to stop the discovery given the
  // service type. We maintain ownership of the browser's lifetime, so we can maintain a strong
  // reference.
  NSMutableDictionary<NSString *, nw_browser_t> *_serviceBrowsers;

  // The dispatch queue used for all callbacks.
  dispatch_queue_t _dispatchQueue;
}

- (instancetype)init {
  if (self = [super init]) {
    _includePeerToPeer = NO;
    _serverSockets = [NSMapTable strongToWeakObjectsMapTable];
    _serviceBrowsers = [[NSMutableDictionary alloc] init];
    _dispatchQueue = dispatch_queue_create("GNCNWFramework", DISPATCH_QUEUE_SERIAL);
  }
  return self;
}

- (BOOL)isListeningForAnyService {
  return _serverSockets.count > 0;
}

- (BOOL)isDiscoveringAnyService {
  return _serviceBrowsers.count > 0;
}

- (nullable GNCNWFrameworkServerSocket *)listenForServiceOnPort:(NSInteger)port
                                              includePeerToPeer:(BOOL)includePeerToPeer
                                                          error:(NSError **)error {
  GNCLoggerInfo(@"[GNCNWFramework] Listen on port: %ld with includePeerToPeer: %@.", (long)port,
                (includePeerToPeer ? @"true" : @"false"));
  GNCNWFrameworkServerSocket *serverSocket = [[GNCNWFrameworkServerSocket alloc] initWithPort:port];
  _includePeerToPeer = includePeerToPeer;
  BOOL success = [serverSocket startListeningWithError:error includePeerToPeer:_includePeerToPeer];
  if (success) {
    [_serverSockets setObject:serverSocket forKey:@(serverSocket.port)];
    return serverSocket;
  }
  return nil;
}

- (nullable GNCNWFrameworkServerSocket *)listenForServiceWithPSKIdentity:(NSData *)PSKIdentity
                                                         PSKSharedSecret:(NSData *)PSKSharedSecret
                                                                    port:(NSInteger)port
                                                       includePeerToPeer:(BOOL)includePeerToPeer
                                                                   error:
                                                                       (NSError **_Nullable)error {
  GNCLoggerInfo(
      @"[GNCNWFramework] Listen on port: %ld with includePeerToPeer: %@, and PSKIdentity: %@.",
      (long)port, (includePeerToPeer ? @"true" : @"false"), PSKIdentity);
  GNCNWFrameworkServerSocket *serverSocket = [[GNCNWFrameworkServerSocket alloc] initWithPort:port];
  _includePeerToPeer = includePeerToPeer;
  BOOL success = [serverSocket startListeningWithPSKIdentity:PSKIdentity
                                             PSKSharedSecret:PSKSharedSecret
                                           includePeerToPeer:_includePeerToPeer
                                                       error:error];
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
  GNCNWFrameworkServerSocket *serverSocket = [_serverSockets objectForKey:@(port)];
  [serverSocket startAdvertisingServiceName:serviceName
                                serviceType:serviceType
                                 txtRecords:txtRecords];
}

- (void)stopAdvertisingPort:(NSInteger)port {
  GNCNWFrameworkServerSocket *serverSocket = [_serverSockets objectForKey:@(port)];
  [serverSocket stopAdvertising];
  [_serverSockets removeObjectForKey:@(port)];
}

- (BOOL)startDiscoveryForServiceType:(NSString *)serviceType
                 serviceFoundHandler:(ServiceUpdateHandler)serviceFoundHandler
                  serviceLostHandler:(ServiceUpdateHandler)serviceLostHandler
                   includePeerToPeer:(BOOL)includePeerToPeer
                               error:(NSError **)error {
  if ([_serviceBrowsers objectForKey:serviceType] != nil) {
    if (error != nil) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorDuplicateDiscovererForServiceType
                               userInfo:nil];
    }
    return NO;
  }

  // Create a parameters object configured to support TCP. TLS MUST be disabled for Nearby to
  // function properly.
  _includePeerToPeer = includePeerToPeer;
  nw_parameters_t parameters = GNCBuildNonTLSParameters(/*includePeerToPeer=*/_includePeerToPeer);
  if (!parameters) {
    GNCLoggerError(@"[GNCNWFramework] Failed to create NW parameters.");
    return NO;
  }
  nw_browse_descriptor_t descriptor =
      nw_browse_descriptor_create_bonjour_service([serviceType UTF8String], /*domain=*/nil);
  nw_browse_descriptor_set_include_txt_record(descriptor, YES);

  id<GNCNWBrowser> browserWrapper = [[GNCNWBrowserImpl alloc] init];
  nw_browser_t browser = [browserWrapper createWithDescriptor:descriptor parameters:parameters];

  [browserWrapper setQueue:browser queue:_dispatchQueue];

  [browserWrapper
      setBrowseResultsChangedHandler:browser
                             handler:^(nw_browse_result_t old_result, nw_browse_result_t new_result,
                                       bool batch_complete) {
                               id<GNCNWBrowseResult> browseResultWrapper =
                                   [GNCNWBrowseResultImpl sharedInstance];
                               nw_browse_result_change_t change =
                                   [browseResultWrapper getChangesFrom:old_result to:new_result];
                               switch (change) {
                                 case nw_browse_result_change_identical:
                                 case nw_browse_result_change_interface_added:
                                 case nw_browse_result_change_interface_removed:
                                   break;
                                 case nw_browse_result_change_result_added: {
                                   // If the service has a loopback interface, we probably
                                   // discovered our own advertisement, so we ignore it.
                                   BOOL hasLoopback = GNCHasLoopbackInterface(new_result);
                                   if (hasLoopback) {
                                     GNCLoggerInfo(
                                         @"Ignoring a service discovered with loopback interface.");
                                     break;
                                   }
                                   nw_endpoint_t endpoint =
                                       [browseResultWrapper copyEndpointFromResult:new_result];
                                   NSString *name = [browseResultWrapper
                                       getBonjourServiceNameFromEndpoint:endpoint];
                                   NSDictionary<NSString *, NSString *> *txtRecords =
                                       GNCTXTRecordForBrowseResult(new_result);
                                   serviceFoundHandler(name, txtRecords);
                                   break;
                                 }
                                 case nw_browse_result_change_txt_record_changed: {
                                   // If the service has a loopback interface, we probably lost our
                                   // own advertisement, so we ignore it.
                                   BOOL oldHasLoopback = GNCHasLoopbackInterface(old_result);
                                   BOOL newHasLoopback = GNCHasLoopbackInterface(new_result);
                                   if (oldHasLoopback || newHasLoopback) {
                                     GNCLoggerInfo(
                                         @"Ignoring a service change with loopback interface.");
                                     break;
                                   }
                                   nw_endpoint_t old_endpoint =
                                       [browseResultWrapper copyEndpointFromResult:old_result];
                                   NSString *oldName = [browseResultWrapper
                                       getBonjourServiceNameFromEndpoint:old_endpoint];
                                   NSDictionary<NSString *, NSString *> *oldTXTRecords =
                                       GNCTXTRecordForBrowseResult(old_result);
                                   serviceLostHandler(oldName, oldTXTRecords);

                                   nw_endpoint_t new_endpoint =
                                       [browseResultWrapper copyEndpointFromResult:new_result];
                                   NSString *newName = [browseResultWrapper
                                       getBonjourServiceNameFromEndpoint:new_endpoint];
                                   NSDictionary<NSString *, NSString *> *newTXTRecords =
                                       GNCTXTRecordForBrowseResult(new_result);
                                   serviceFoundHandler(newName, newTXTRecords);
                                   break;
                                 }
                                 case nw_browse_result_change_result_removed: {
                                   // If the service has a loopback interface, we probably lost our
                                   // own advertisement, so we ignore it.
                                   BOOL hasLoopback = GNCHasLoopbackInterface(old_result);
                                   if (hasLoopback) {
                                     GNCLoggerInfo(
                                         @"Ignoring a service lost with loopback interface.");
                                     break;
                                   }
                                   nw_endpoint_t endpoint =
                                       [browseResultWrapper copyEndpointFromResult:old_result];
                                   NSString *name = [browseResultWrapper
                                       getBonjourServiceNameFromEndpoint:endpoint];
                                   NSDictionary<NSString *, NSString *> *txtRecords =
                                       GNCTXTRecordForBrowseResult(old_result);
                                   serviceLostHandler(name, txtRecords);
                                   break;
                                 }
                               }
                             }];

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];

  __block nw_browser_state_t blockResult = nw_browser_state_invalid;
  __block NSError *blockError = nil;

  [browserWrapper setStateChangedHandler:browser
                                 handler:^(nw_browser_state_t state, nw_error_t error) {
                                   [condition lock];
                                   blockResult = state;
                                   if (error != nil) {
                                     blockError =
                                         (__bridge_transfer NSError *)nw_error_copy_cf_error(error);
                                   }
                                   [condition signal];
                                   [condition unlock];
                                 }];

  [_serviceBrowsers setObject:browser forKey:serviceType];
  [browserWrapper start:browser];

  BOOL didSignal = [condition
      waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:GNCStartDiscoveryTimeoutInSeconds]];
  [condition unlock];

  // We timed out waiting for the connection to transition into a state.
  if (!didSignal) {
    [self stopDiscoveryForServiceType:serviceType];
    if (error != nil) {
      *error = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                   code:GNCNWFrameworkErrorTimedOut
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
  if (browser) {
    [_serviceBrowsers removeObjectForKey:serviceType];
    id<GNCNWBrowser> browserWrapper = [[GNCNWBrowserImpl alloc] init];
    [browserWrapper cancel:browser];
  }
}

- (nullable GNCNWFrameworkSocket *)connectToServiceName:(NSString *)serviceName
                                            serviceType:(NSString *)serviceType
                                                  error:(NSError **)error {
  GNCLoggerInfo(@"[GNCNWFramework] Connect to service {serviceName:%@, serviceType:%@, "
                @"includePeerToPeer:%@}.",
                serviceName, serviceType, (_includePeerToPeer ? @"true" : @"false"));
  nw_endpoint_t endpoint = nw_endpoint_create_bonjour_service([serviceName UTF8String],
                                                              [serviceType UTF8String], "local");
  return [self internalConnectToEndpoint:endpoint
                             PSKIdentity:nil
                         PSKSharedSecret:nil
                       includePeerToPeer:_includePeerToPeer
                            cancelSource:nil
                                   queue:nil
                                   error:error];
}

- (nullable GNCNWFrameworkSocket *)connectToServiceName:(NSString *)serviceName
                                            serviceType:(NSString *)serviceType
                                            PSKIdentity:(NSData *)PSKIdentity
                                        PSKSharedSecret:(NSData *)PSKSharedSecret
                                                  error:(NSError **_Nullable)error {
  GNCLoggerInfo(@"[GNCNWFramework] Connect to service {serviceName:%@, serviceType:%@, "
                @"includePeerToPeer:%@}.",
                serviceName, serviceType, (_includePeerToPeer ? @"true" : @"false"));
  nw_endpoint_t endpoint = nw_endpoint_create_bonjour_service([serviceName UTF8String],
                                                              [serviceType UTF8String], "local");
  return [self internalConnectToEndpoint:endpoint
                             PSKIdentity:PSKIdentity
                         PSKSharedSecret:PSKSharedSecret
                       includePeerToPeer:_includePeerToPeer
                            cancelSource:nil
                                   queue:nil
                                   error:error];
}

- (nullable GNCNWFrameworkSocket *)connectToHost:(GNCIPv4Address *)host
                                            port:(NSInteger)port
                               includePeerToPeer:(BOOL)includePeerToPeer
                                    cancelSource:(nullable dispatch_source_t)cancelSource
                                           queue:(nullable dispatch_queue_t)queue
                                           error:(NSError **)error {
  GNCLoggerInfo(@"[GNCNWFramework] Connect to host {host:%s, port:%ld}.",
                host.dottedRepresentation.UTF8String, (long)port);
  nw_endpoint_t endpoint =
      nw_endpoint_create_host(host.dottedRepresentation.UTF8String, @(port).stringValue.UTF8String);
  return [self internalConnectToEndpoint:endpoint
                             PSKIdentity:nil
                         PSKSharedSecret:nil
                       includePeerToPeer:(BOOL)includePeerToPeer
                            cancelSource:cancelSource
                                   queue:queue
                                   error:error];
}

// MARK: - Private Methods (Implementation)

- (nullable GNCNWFrameworkSocket *)internalConnectToEndpoint:(nw_endpoint_t)endpoint
                                                 PSKIdentity:(nullable NSData *)PSKIdentity
                                             PSKSharedSecret:(nullable NSData *)PSKSharedSecret
                                           includePeerToPeer:(BOOL)includePeerToPeer
                                                cancelSource:
                                                    (nullable dispatch_source_t)cancelSource
                                                       queue:(nullable dispatch_queue_t)queue
                                                       error:(NSError **)error {
  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];

  __block nw_connection_state_t blockResult = nw_connection_state_invalid;
  __block NSError *blockError = nil;

  _includePeerToPeer = includePeerToPeer;
  nw_parameters_t parameters =
      (PSKIdentity == nil || PSKSharedSecret == nil)
          ? GNCBuildNonTLSParameters(/*includePeerToPeer=*/_includePeerToPeer)
          : GNCBuildTLSParameters(/*PSK=*/PSKSharedSecret, /*identity=*/PSKIdentity,
                                  /*includePeerToPeer=*/_includePeerToPeer);
  if (!parameters) {
    GNCLoggerError(@"[GNCNWFramework] Failed to create NW parameters.");
    return nil;
  }

  id<GNCNWConnection> connectionWrapper = [[GNCNWConnectionImpl alloc] init];
  [connectionWrapper createConnectionWithEndpoint:endpoint parameters:parameters];
  [connectionWrapper setQueue:queue ?: _dispatchQueue];
  [connectionWrapper
      setStateChangedHandler:^(nw_connection_state_t state, nw_error_t error) {
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
      }];
  if (cancelSource) {
    dispatch_source_set_event_handler(cancelSource, ^{
      GNCLoggerInfo(
          @"[GNCNWFramework] Connection to endpoint was cancelled before it could be established.");
      [connectionWrapper cancel];
      dispatch_source_cancel(cancelSource);
    });
  }
  [connectionWrapper start];

  BOOL didSignal =
      [condition waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:GNCConnectionTimeoutInSeconds]];
  [condition unlock];
  // We timed out waiting for the connection to transition into a state.
  if (!didSignal) {
    [connectionWrapper cancel];
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

  GNCLoggerInfo(@"[GNCNWFramework] Connect to endpoint result %@", @(blockResult));
  switch (blockResult) {
    case nw_connection_state_invalid:
    case nw_connection_state_waiting:
    case nw_connection_state_preparing:
    case nw_connection_state_failed:
    case nw_connection_state_cancelled:
      return nil;
    case nw_connection_state_ready: {
      return [[GNCNWFrameworkSocket alloc] initWithConnection:connectionWrapper];
    }
  }
}

@end

NS_ASSUME_NONNULL_END
