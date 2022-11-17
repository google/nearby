// Copyright 2022 Google LLC
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

@class GNCPayload;

/**
 * Indicates the status of a payload transfer.
 */
typedef NS_CLOSED_ENUM(NSInteger, GNCPayloadStatus) {
  GNCPayloadStatusSuccess,
  GNCPayloadStatusFailure,
  GNCPayloadStatusInProgress,
  GNCPayloadStatusCanceled,
};

/**
 * The @c GNCPayloadDelegate protocol defines methods that a delegate can implement to handle
 * payload-related events.
 */
@protocol GNCPayloadDelegate

/**
 * Called when a payload is received from a remote endpoint. Depending on the type of the payload,
 * all of the data may or may not have been received at the time of this call.
 *
 * @param payload The Payload object received.
 * @param endpointID The identifier for the remote endpoint that sent the payload.
 */
- (void)receivedPayload:(nonnull GNCPayload *)payload fromEndpoint:(nonnull NSString *)endpointID;

/**
 * Called with progress information about an active payload transfer, either incoming or outgoing.
 *
 * @param payloadID The identifier the sent or received payload.
 * @param status The status of the payload transfer.
 * @param endpointID The identifier for the remote endpoint that is sending or receiving this
 *                   payload.
 * @param bytesTransfered The number of bytes transfered so far.
 * @param totalBytes The total bytes of the transfer.
 */
- (void)receivedProgressUpdateForPayload:(int64_t)payloadID
                              withStatus:(GNCPayloadStatus)status
                            fromEndpoint:(nonnull NSString *)endpointID
                         bytesTransfered:(int64_t)bytesTransfered
                              totalBytes:(int64_t)totalBytes;

@end
