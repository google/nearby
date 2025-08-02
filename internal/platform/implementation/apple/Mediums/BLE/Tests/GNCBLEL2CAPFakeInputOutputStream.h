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

#import <Foundation/Foundation.h>

/**
 * Test helper to create fake @c NSInputStream and @c NSOutputStream objects to simulate BLE
 * communication with a watch via L2CAP channels during tests.
 */
@interface GNCBLEL2CAPFakeInputOutputStream : NSObject

@property(readonly) NSInputStream *inputStream;

@property(readonly) NSOutputStream *outputStream;

/// Returns an instance of streams initialized with the given |bufferSize|.
- (instancetype)initWithBufferSize:(NSUInteger)bufferSize NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// Should be called in [XCTestCase tearDown] to tear down the streams.
- (void)tearDown;

/// Returns the data "sent" to the watch with |maxBytes|.
- (NSData *)dataSentToWatchWithMaxBytes:(NSUInteger)maxBytes;

/// Simulates the device writing |data| to |inputStream|.
- (void)writeFromDevice:(NSData *)data;

@end
