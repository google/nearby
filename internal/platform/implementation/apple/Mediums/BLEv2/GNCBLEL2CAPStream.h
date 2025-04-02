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

NS_ASSUME_NONNULL_BEGIN

/// Block invoked when the stream is closed.
typedef void (^GNCBLEL2CAPStreamClosedBlock)(void);

/// Block invoked when |data| is received from the remote device on the L2CAP connection.
typedef void (^GNCBLEL2CAPControllerReceivedDataBlock)(NSData *data);

/**
 * Abstraction to take in two streams returned from L2CAP controller and provide a simplified
 * interface to send and receive data from the streams.
 * This class is thread-safe.
 */
@interface GNCBLEL2CAPStream : NSObject

/// Returns an instance of @c GNCBLEL2CAPStream which sends and receives data on |inputStream|
/// and |outputStream|.
/// Invokes |closedBlock| if stream closed signal is received when reading data.
/// The stream should be torn down when this block is called.
/// Invokes |receivedDataBlock| with data received |inputStream|.
/// The blocks are invoked on an arbitrary queue with DISPATCH_QUEUE_PRIORITY_HIGH.
- (instancetype)initWithClosedBlock:(GNCBLEL2CAPStreamClosedBlock)closedBlock
                  receivedDataBlock:(GNCBLEL2CAPControllerReceivedDataBlock)receivedDataBlock
                        inputStream:(NSInputStream *)inputStream
                       outputStream:(NSOutputStream *)outputStream NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// Queues up |data| to be sent to the device. Asserts if |data| is empty.
///
/// |completionBlock| is invoked on an arbitrary queue after sending finishes. If the parameter is
/// YES, data is completely written to the local L2CAP socket. Note that this does not necessarily
/// mean the device received it. NO indicates writing failed, which typically happens when the
/// stream is torn down.
- (void)sendData:(NSData *)data completionBlock:(void (^)(BOOL))completionBlock;

/// Closes the stream.
- (void)close;

/// Tears down the stream.
- (void)tearDown;

@end

NS_ASSUME_NONNULL_END
