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

NS_ASSUME_NONNULL_BEGIN

/** A fake implementation of @c GNCNWFramework to inject for testing. */
@interface GNCFakeNWFramework : GNCNWFramework

@property(nonatomic, nullable) NSNumber* startedAdvertisingPort;
@property(nonatomic, nullable) NSString* startedAdvertisingServiceName;
@property(nonatomic, nullable) NSString* startedAdvertisingServiceType;
@property(nonatomic, nullable) NSNumber* stoppedAdvertisingPort;
@property(nonatomic, nullable) NSString* startedDiscoveryServiceType;
@property(nonatomic) BOOL startedDiscoveryIncludePeerToPeer;
@property(nonatomic, nullable) NSString* stoppedDiscoveryServiceType;
@property(nonatomic, nullable) NSString* connectedToServiceName;
@property(nonatomic, nullable) NSString* connectedToServiceType;
@property(nonatomic, nullable) GNCIPv4Address* connectedToHost;
@property(nonatomic) NSInteger connectedToPort;
@property(nonatomic) BOOL connectedToIncludePeerToPeer;
@property(nonatomic) NSInteger listenedForServiceOnPort;
@property(nonatomic) BOOL listenedForServiceIncludePeerToPeer;
@property(nonatomic, readonly) NSMutableArray* sockets;
@property(nonatomic, readonly) NSMutableArray* serverSockets;

@end

NS_ASSUME_NONNULL_END
