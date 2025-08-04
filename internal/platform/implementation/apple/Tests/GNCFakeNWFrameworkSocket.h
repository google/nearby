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

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/** A fake implementation of @c GNCNWFrameworkSocket to inject for testing. */
@interface GNCFakeNWFrameworkSocket : GNCNWFrameworkSocket

@property(nonatomic, nullable) NSData* dataToRead;
@property(nonatomic, readonly) NSMutableData* writtenData;
@property(nonatomic) BOOL isClosed;
@property(nonatomic, nullable) NSError* readError;
@property(nonatomic, nullable) NSError* writeError;

@end

NS_ASSUME_NONNULL_END
