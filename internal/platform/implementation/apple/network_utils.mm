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

#import "internal/platform/implementation/apple/network_utils.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/service_address.h"

#include "internal/platform/cancellation_flag.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"

namespace nearby {
namespace apple {
namespace network_utils {

bool StartAdvertising(GNCNWFramework* medium, const nearby::NsdServiceInfo& nsd_service_info) {
  int port = nsd_service_info.GetPort();
  NSString* serviceName = @(nsd_service_info.GetServiceName().c_str());
  NSString* serviceType = @(nsd_service_info.GetServiceType().c_str());
  NSMutableDictionary<NSString*, NSString*>* txtRecords = [[NSMutableDictionary alloc] init];
  for (const auto& record : nsd_service_info.GetTxtRecords()) {
    [txtRecords setObject:@(record.second.c_str()) forKey:@(record.first.c_str())];
  }

  [medium startAdvertisingPort:port
                   serviceName:serviceName
                   serviceType:serviceType
                    txtRecords:txtRecords];

  return true;
}

bool StopAdvertising(GNCNWFramework* medium, const NsdServiceInfo& nsd_service_info) {
  NSInteger port = nsd_service_info.GetPort();
  [medium stopAdvertisingPort:port];
  return true;
}

bool StartDiscovery(GNCNWFramework* medium, const std::string& service_type,
                    NetworkDiscoveredServiceCallback callback, bool include_peer_to_peer) {
  if (medium.isDiscoveringAnyService) {
    GNCLoggerError(@"Error already discovering for service");
    return false;
  }
  __block NSString* serviceType = @(service_type.c_str());
  __block NetworkDiscoveredServiceCallback client_callback = std::move(callback);

  NSError* error = nil;
  BOOL result = [medium startDiscoveryForServiceType:serviceType
      serviceFoundHandler:^(NSString* name, NSDictionary<NSString*, NSString*>* txtRecords) {
        NsdServiceInfo nsd_service_info;
        nsd_service_info.SetServiceType([serviceType UTF8String]);
        nsd_service_info.SetServiceName([name UTF8String]);
        [txtRecords
            enumerateKeysAndObjectsUsingBlock:[nsd_service_info = &nsd_service_info](
                                                  NSString* key, NSString* val, BOOL* stop) {
              nsd_service_info->SetTxtRecord([key UTF8String], [val UTF8String]);
            }];
        client_callback.network_service_discovered_cb(nsd_service_info);
      }
      serviceLostHandler:^(NSString* name, NSDictionary<NSString*, NSString*>* txtRecords) {
        NsdServiceInfo nsd_service_info;
        nsd_service_info.SetServiceType([serviceType UTF8String]);
        nsd_service_info.SetServiceName([name UTF8String]);
        [txtRecords
            enumerateKeysAndObjectsUsingBlock:[nsd_service_info = &nsd_service_info](
                                                  NSString* key, NSString* val, BOOL* stop) {
              nsd_service_info->SetTxtRecord([key UTF8String], [val UTF8String]);
            }];
        client_callback.network_service_lost_cb(nsd_service_info);
      }
      includePeerToPeer:include_peer_to_peer
      error:&error];
  if (error != nil) {
    GNCLoggerError(@"Error starting discovery for service type<%@>: %@", serviceType, error);
  }
  return result;
}

bool StopDiscovery(GNCNWFramework* medium, const std::string& service_type) {
  NSString* serviceType = @(service_type.c_str());
  [medium stopDiscoveryForServiceType:serviceType];
  return true;
}

GNCNWFrameworkSocket* ConnectToService(GNCNWFramework* medium,
                                       const NsdServiceInfo& remote_service_info,
                                       CancellationFlag* cancellation_flag) {
  NSError* error = nil;
  NSString* serviceName = @(remote_service_info.GetServiceName().c_str());
  NSString* serviceType = @(remote_service_info.GetServiceType().c_str());
  GNCNWFrameworkSocket* socket = [medium connectToServiceName:serviceName
                                                  serviceType:serviceType
                                                        error:&error];
  if (socket) {
    return socket;
  }
  if (error != nil) {
    GNCLoggerError(@"Error connecting to service name<%@> type<%@>: %@", serviceName, serviceType,
                   error);
  }
  return nil;
}

GNCNWFrameworkSocket* ConnectToService(GNCNWFramework* medium,
                                       const NsdServiceInfo& remote_service_info,
                                       const api::PskInfo& psk_info,
                                       CancellationFlag* cancellation_flag) {
  NSError* error = nil;
  NSString* serviceName = @(remote_service_info.GetServiceName().c_str());
  NSString* serviceType = @(remote_service_info.GetServiceType().c_str());
  NSData* pskIdentity = [NSData dataWithBytes:psk_info.identity.data()
                                       length:psk_info.identity.size()];
  NSData* pskPassword = [NSData dataWithBytes:psk_info.password.data()
                                       length:psk_info.password.size()];
  GNCNWFrameworkSocket* socket = [medium connectToServiceName:serviceName
                                                  serviceType:serviceType
                                                  PSKIdentity:pskIdentity
                                              PSKSharedSecret:pskPassword
                                                        error:&error];
  if (socket) {
    return socket;
  }
  if (error != nil) {
    GNCLoggerError(@"Error connecting to service name<%@> type<%@>: %@", serviceName, serviceType,
                   error);
  }
  return nil;
}

GNCNWFrameworkSocket* ConnectToService(GNCNWFramework* medium,
                                       const ServiceAddress& service_address,
                                       bool include_peer_to_peer,
                                       CancellationFlag* cancellation_flag) {
  NSError* error = nil;
  if (service_address.address.size() != 4) {
    GNCLoggerError(@"Error IP address must be 4 bytes, but is %lu bytes",
                   service_address.address.size());
    return nil;
  }
  NSData* hostData = [NSData dataWithBytes:service_address.address.data()
                                    length:service_address.address.size()];
  GNCIPv4Address* host = [GNCIPv4Address addressFromData:hostData];
  GNCNWFrameworkSocket* socket = [medium connectToHost:host
                                                  port:service_address.port
                                     includePeerToPeer:include_peer_to_peer
                                          cancelSource:nil
                                                 queue:nil
                                                 error:&error];
  if (socket != nil) {
    return socket;
  }
  if (error != nil) {
    GNCLoggerError(@"Error connecting to %@:%d: %@", host, service_address.port, error);
  }
  return nil;
}

GNCNWFrameworkServerSocket* ListenForService(GNCNWFramework* medium, int port,
                                             bool include_peer_to_peer) {
  if (medium.isListeningForAnyService) {
    GNCLoggerError(@"Error already listening for service");
    return nil;
  }
  NSError* error = nil;
  GNCNWFrameworkServerSocket* serverSocket = [medium listenForServiceOnPort:port
                                                          includePeerToPeer:include_peer_to_peer
                                                                      error:&error];
  if (serverSocket != nil) {
    return serverSocket;
  }
  if (error != nil) {
    GNCLoggerError(@"Error listening for service: %@", error);
  }
  return nil;
}

GNCNWFrameworkServerSocket* ListenForService(GNCNWFramework* medium, const api::PskInfo& psk_info,
                                             int port, bool include_peer_to_peer) {
  if (medium.isListeningForAnyService) {
    GNCLoggerError(@"Error already listening for service");
    return nil;
  }
  NSError* error = nil;
  NSData* pskIdentity = [NSData dataWithBytes:psk_info.identity.data()
                                       length:psk_info.identity.size()];
  NSData* pskPassword = [NSData dataWithBytes:psk_info.password.data()
                                       length:psk_info.password.size()];
  GNCNWFrameworkServerSocket* serverSocket =
      [medium listenForServiceWithPSKIdentity:pskIdentity
                              PSKSharedSecret:pskPassword
                                         port:port
                            includePeerToPeer:include_peer_to_peer
                                        error:&error];
  if (serverSocket != nil) {
    return serverSocket;
  }
  if (error != nil) {
    GNCLoggerError(@"Error listening for service: %@", error);
  }
  return nil;
}

}  // namespace network_utils
}  // namespace apple
}  // namespace nearby
