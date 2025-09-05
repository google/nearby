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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/socket.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWConnectionImpl.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkError.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket+Internal.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWParameters.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCNWFrameworkServerSocket ()

@property(nonatomic, readonly) NSMutableArray<nw_connection_t> *pendingConnections;
@property(nonatomic, readonly) NSMutableArray<nw_connection_t> *readyConnections;

- (BOOL)internalStartListeningWithPSKIdentity:(nullable NSData *)PSKIdentify
                              PSKSharedSecret:(nullable NSData *)PSKSharedSecret
                            includePeerToPeer:(BOOL)includePeerToPeer
                                        error:(NSError **_Nullable)error;

@end

@implementation GNCNWFrameworkServerSocket {
  NSCondition *_condition;
  nw_listener_t _listener;
  nw_listener_state_t _listenerState;
  NSError *_listenerError;
  dispatch_queue_t _dispatchQueue;
}

- (instancetype)initWithPort:(NSInteger)port {
  self = [super init];
  if (self) {
    _port = port;
    _condition = [[NSCondition alloc] init];
    _listenerState = nw_listener_state_invalid;
    _dispatchQueue = dispatch_queue_create("GNCNWFrameworkServerSocket", DISPATCH_QUEUE_SERIAL);
  }
  return self;
}

@synthesize pendingConnections = _pendingConnections;

- (NSMutableArray<nw_connection_t> *)pendingConnections {
  if (!_pendingConnections) {
    _pendingConnections = [[NSMutableArray alloc] init];
  }
  return _pendingConnections;
}

@synthesize readyConnections = _readyConnections;

- (NSMutableArray<nw_connection_t> *)readyConnections {
  if (!_readyConnections) {
    _readyConnections = [[NSMutableArray alloc] init];
  }
  return _readyConnections;
}

@synthesize ipAddress = _ipAddress;

- (GNCIPv4Address *)ipAddress {
  if (!_ipAddress) {
    _ipAddress = [GNCNWFrameworkServerSocket lookupIpAddress];
  }
  return _ipAddress;
}

- (nullable GNCNWFrameworkSocket *)acceptWithError:(NSError **)error {
  // Wait until we have a ready connection and listener is in a ready/invalid state.
  [_condition lock];
  nw_connection_t connection = [self.readyConnections lastObject];
  while (connection == nil && (_listenerState == nw_listener_state_ready ||
                               _listenerState == nw_listener_state_invalid)) {
    [_condition wait];
    connection = [self.readyConnections lastObject];
  }
  if (connection != nil) {
    [self.readyConnections removeObject:connection];
  }
  if (error != nil) {
    *error = [_listenerError copy];
  }
  [_condition unlock];

  if (connection == nil) {
    return nil;
  }
  return [[GNCNWFrameworkSocket alloc]
      initWithConnection:[[GNCNWConnectionImpl alloc] initWithNWConnection:connection]];
}

- (void)close {
  if (_listener == nil) {
    return;
  }
  nw_listener_cancel(_listener);
  _listener = nil;
}

- (BOOL)startListeningWithError:(NSError **)error includePeerToPeer:(BOOL)includePeerToPeer {
  return [self internalStartListeningWithPSKIdentity:nil
                                     PSKSharedSecret:nil
                                   includePeerToPeer:includePeerToPeer
                                               error:error];
}

- (BOOL)startListeningWithPSKIdentity:(NSData *)PSKIdentify
                      PSKSharedSecret:(NSData *)PSKSharedSecret
                    includePeerToPeer:(BOOL)includePeerToPeer
                                error:(NSError **_Nullable)error {
  return [self internalStartListeningWithPSKIdentity:PSKIdentify
                                     PSKSharedSecret:PSKSharedSecret
                                   includePeerToPeer:includePeerToPeer
                                               error:error];
}

- (void)startAdvertisingServiceName:(NSString *)serviceName
                        serviceType:(NSString *)serviceType
                         txtRecords:(NSDictionary<NSString *, NSString *> *)txtRecords {
  GNCLoggerInfo(@"[GNCNWFrameworkServerSocket] Start advertising {serviceName:%@, "
                @"serviceType: %@}",
                serviceName, serviceType);

  nw_advertise_descriptor_t advertiseDescriptor = nw_advertise_descriptor_create_bonjour_service(
      [serviceName UTF8String], [serviceType UTF8String], /*domain=*/nil);
  nw_txt_record_t txtRecord = nw_txt_record_create_dictionary();
  for (NSString *key in txtRecords) {
    NSString *recordValue = [txtRecords objectForKey:key];
    NSData *encodedRecord = [recordValue dataUsingEncoding:NSUTF8StringEncoding];
    if (!nw_txt_record_set_key(txtRecord, [key UTF8String], encodedRecord.bytes,
                               encodedRecord.length)) {
      GNCLoggerError(@"[GNCNWFrameworkServerSocket] Failed to set text record key: %@, value: %@",
                     key, recordValue);
      continue;
    }
    GNCLoggerDebug(@"[GNCNWFrameworkServerSocket] Text record {key: "
                   @"%@, value: %@}",
                   key, recordValue);
  }
  nw_advertise_descriptor_set_txt_record_object(advertiseDescriptor, txtRecord);
  nw_listener_set_advertise_descriptor(_listener, advertiseDescriptor);
}

- (void)stopAdvertising {
  nw_listener_set_advertise_descriptor(_listener, nil);
}

/**
 * Attemps to find an IPv4 hostname for a Wi-Fi/Ethernet interface.
 *
 * Returns the IP address as a 4 byte string. If not available, this returns an empty string to
 * align with the Windows implementation.
 */
+ (GNCIPv4Address *)lookupIpAddress {
  GNCIPv4Address *result = [GNCIPv4Address addressFromFourByteInt:0];
  struct ifaddrs *ifaddr;
  if (getifaddrs(&ifaddr) == 0) {
    for (struct ifaddrs *cur = ifaddr; cur != NULL; cur = cur->ifa_next) {
      struct sockaddr *address = cur->ifa_addr;
      if (address == nil) {
        continue;
      }

      // Filter out any interfaces that aren't IPv4.
      if (address->sa_family != AF_INET) {
        continue;
      }

      // Filter out any interfaces that aren't en0 (Wi-Fi/Ethernet).
      NSString *interfaceName = @(cur->ifa_name);
      if (![interfaceName isEqualToString:@"en0"]) {
        continue;
      }

      uint32_t host = ((struct sockaddr_in *)address)->sin_addr.s_addr;
      result = [GNCIPv4Address addressFromFourByteInt:host];
      break;
    }
    freeifaddrs(ifaddr);
  }
  return result;
}

// MARK: - Private Methods (Implementation)

- (BOOL)internalStartListeningWithPSKIdentity:(nullable NSData *)PSKIdentify
                              PSKSharedSecret:(nullable NSData *)PSKSharedSecret
                            includePeerToPeer:(BOOL)includePeerToPeer
                                        error:(NSError **_Nullable)error {
  nw_parameters_t parameters =
      (PSKIdentify == nil || PSKSharedSecret == nil)
          ? GNCBuildNonTLSParameters(/*includePeerToPeer=*/includePeerToPeer)
          : GNCBuildTLSParameters(/*PSK=*/PSKSharedSecret, /*identity=*/PSKIdentify,
                                  /*includePeerToPeer=*/includePeerToPeer);
  if (!parameters) {
    GNCLoggerError(@"[GNCNWFrameworkServerSocket] Failed to create NW parameters.");
    return NO;
  }

  // If the server socket port is zero, a random port will be selected. The server socket port
  // will be updated to reflect the port it's listening on, once the listener transitions to the
  // ready state.
  if (self.port == 0) {
    _listener = nw_listener_create(parameters);
  } else {
    _listener = nw_listener_create_with_port([[@(self.port) stringValue] UTF8String], parameters);
  }

  // Register to listen for incoming connections.
  nw_listener_set_queue(_listener, _dispatchQueue);
  nw_listener_set_new_connection_handler(_listener, ^(nw_connection_t connection) {
    // Keep track of any incoming connections. We must keep a reference otherwise the connection
    // will be dropped.
    [self.pendingConnections addObject:connection];
    // Register for state changes so we can clean up any failed/canceled connections and inform
    // Nearby of any ready connections when asked.
    nw_connection_set_queue(connection, _dispatchQueue);
    nw_connection_set_state_changed_handler(connection,
                                            ^(nw_connection_state_t state, nw_error_t error) {
                                              [_condition lock];
                                              switch (state) {
                                                case nw_connection_state_cancelled:
                                                case nw_connection_state_invalid:
                                                case nw_connection_state_failed:
                                                  [self.pendingConnections removeObject:connection];
                                                  break;
                                                case nw_connection_state_waiting:
                                                  // A connection couldn't be opened, but could be
                                                  // retried when conditions are more favorable. We
                                                  // choose to just drop the connection, but we
                                                  // could add retry logic here in the future.
                                                  [self.pendingConnections removeObject:connection];
                                                  break;
                                                case nw_connection_state_preparing:
                                                  // ignore.
                                                  break;
                                                case nw_connection_state_ready:
                                                  // Move the connection from "pending" to "ready".
                                                  [self.readyConnections addObject:connection];
                                                  [self.pendingConnections removeObject:connection];
                                                  [_condition signal];
                                                  break;
                                              }
                                              [_condition unlock];
                                            });
    nw_connection_start(connection);
  });

  // Register for state changes for the listener so we can block until we have an initial status.
  [_condition lock];
  nw_listener_set_state_changed_handler(_listener, ^(nw_listener_state_t state, nw_error_t error) {
    [_condition lock];

    _listenerState = state;
    if (error != nil) {
      _listenerError = (__bridge NSError *)nw_error_copy_cf_error(error);
    }
    [_condition signal];
    [_condition unlock];
  });

  nw_listener_start(_listener);

  // This doesn't start flaking until 0.0005 seconds, so 0.5 should be plenty of time.
  BOOL didSignal = [_condition waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.5]];
  if (error != nil) {
    *error = [_listenerError copy];
  }
  [_condition unlock];

  // We timed out waiting for the listener to transition into a state.
  if (!didSignal) {
    [self close];
    _listenerError = [NSError errorWithDomain:GNCNWFrameworkErrorDomain
                                         code:GNCNWFrameworkErrorTimedOut
                                     userInfo:nil];
    return NO;
  }

  switch (_listenerState) {
    case nw_listener_state_invalid:
    case nw_listener_state_waiting:
    case nw_listener_state_failed:
    case nw_listener_state_cancelled:
      GNCLoggerError(@"[GNCNWFrameworkServerSocket] Listen state: %u", _listenerState);
      [self close];
      return NO;
    case nw_listener_state_ready:
      _port = nw_listener_get_port(_listener);
      GNCLoggerInfo(@"[GNCNWFrameworkServerSocket] Listen on port: %ld", (long)_port);
      return YES;
  }
}

@end

NS_ASSUME_NONNULL_END
