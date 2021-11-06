// Copyright 2020 Google LLC
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

@class GNCBytesPayload, GNCStreamPayload, GNCFilePayload;

/** Response to a connection request. */
typedef NS_ENUM(NSInteger, GNCConnectionResponse) {
  GNCConnectionResponseReject,  // reject the connection request
  GNCConnectionResponseAccept,  // accept the connection request
};

/** Reason for a failed connection request. */
typedef NS_ENUM(NSInteger, GNCConnectionFailure) {
  GNCConnectionFailureRejected,  // an endpoint rejected the connection request
  GNCConnectionFailureUnknown,   // there was an error while attempting to make the connection
};

/** Handler for a @c GNCConnectionFailure value. */
typedef void (^GNCConnectionFailureHandler)(GNCConnectionFailure);

/** Reasons that a connection can be severed by either endpoint. */
typedef NS_ENUM(NSInteger, GNCDisconnectedReason) {
  GNCDisconnectedReasonUnknown,  // the endpoint can no longer be reached
};

/** Handler for a @c GNCDisconnectedReason value. */
typedef void (^GNCDisconnectedHandler)(GNCDisconnectedReason);

/** Result of a payload transfer. */
typedef NS_ENUM(NSInteger, GNCPayloadResult) {
  GNCPayloadResultSuccess,   // Payload delivery was successful.
  GNCPayloadResultFailure,   // An error occurred during payload delivery.
  GNCPayloadResultCanceled,  // Payload delivery was canceled.
};

/** Handler for a @c GNCPayloadResult value. */
typedef void (^GNCPayloadResultHandler)(GNCPayloadResult);

/** Connection topology. See https://developers.google.com/nearby/connections/strategies. */
typedef NS_ENUM(NSInteger, GNCStrategy) {
  GNCStrategyCluster,       // M-to-N
  GNCStrategyStar,          // 1-to-N
  GNCStrategyPointToPoint,  // 1-to-1
};

/** Every endpoint has a unique identifier. */
typedef NSString *GNCEndpointId;

/** This handler receives a Bytes payload.  It is called when the payload data is fully received. */
typedef void (^GNCBytesPayloadHandler)(GNCBytesPayload *payload);

/**
 * This handler receives a Stream payload, signifying the start of receipt of a stream. The payload
 * data should be read from the supplied input stream. The progress object can be used to monitor
 * progress or cancel the operation. This handler must return a completion handler, which is
 * called when the operation is finished.
 */
typedef GNCPayloadResultHandler _Nonnull (^GNCStreamPayloadHandler)(GNCStreamPayload *payload,
                                                                    NSProgress *progress);

/**
 * This handler receives a File payload, signifying the start of receipt of a file. The
 * progress object can be used to monitor progress or cancel the operation. This handler must
 * return a completion handler, which is called when the operation finishes successfully or if
 * there is an error. The file will be stored in a temporary location. If an error occurs or the
 * operation is canceled, the file will contain all data that was received. It is the client's
 * responsibility to delete the file when it is no longer needed.
 */
typedef GNCPayloadResultHandler _Nonnull (^GNCFilePayloadHandler)(GNCFilePayload *payload,
                                                                  NSProgress *progress);

/** This class contains optional handlers for a connection. */
@interface GNCConnectionHandlers : NSObject

/**
 * This handler receives Bytes payloads. It is optional; apps that don't send and receive Bytes
 * payloads need not supply this handler.
 */
@property(nonatomic, nullable) GNCBytesPayloadHandler bytesPayloadHandler;

/**
 * This handler receives a stream that delivers a payload in chunks. It is optional; apps that
 * don't send and receive Stream payloads need not supply this handler.
 */
@property(nonatomic, nullable) GNCStreamPayloadHandler streamPayloadHandler;

/**
 * This handler receives a File payload. It is optional; apps that don't send and receive File
 * payloads need not supply this handler.
 * Note: File payloads are not yet supported.
 */
@property(nonatomic, nullable) GNCFilePayloadHandler filePayloadHandler;

/**
 * This handler is called when the connection is ended, whether due to the endpoint disconnecting
 * or moving out of range.  It is optional.
 */
@property(nonatomic, nullable) GNCDisconnectedHandler disconnectedHandler;

/**
 * This factory method lets you specify a subset of the connection handlers in a single expression.
 *
 * @param builderBlock Set up the handlers in this block.
 */
+ (instancetype)handlersWithBuilder:(void (^)(GNCConnectionHandlers *))builderBlock;

@end

/**
 * This represents a connection with an endpoint. Use it to send payloads to the endpoint, and
 * release it to disconnect.
 */
@protocol GNCConnection <NSObject>

/**
 * Send a Bytes payload. A progress object is returned, which can be used to monitor
 * progress or cancel the operation. |completion| will be called when the operation completes
 * (in all cases, even if failed or was canceled).
 */
- (NSProgress *)sendBytesPayload:(GNCBytesPayload *)payload
                      completion:(GNCPayloadResultHandler)completion;

/**
 * Send a Stream payload. A progress object is returned, which can be used to monitor
 * progress or cancel the operation. The stream data is read from the supplied NSInputStream.
 * |completion| will be called when the operation completes.
 */
- (NSProgress *)sendStreamPayload:(GNCStreamPayload *)payload
                       completion:(GNCPayloadResultHandler)completion;

/**
 * Send a File payload. A progress object is returned, which can be used to monitor progress or
 * cancel the operation. |completion| will be called when the operation completes.
 * Note: File payloads are not yet supported.
 */
- (NSProgress *)sendFilePayload:(GNCFilePayload *)payload
                     completion:(GNCPayloadResultHandler)completion;
@end

/**
 * This handler takes a @c GNCConnection object and returns a @c GNCConnectionHandlers
 * object containing the desired payload and connection-ended handlers.
 */
typedef GNCConnectionHandlers *_Nonnull (^GNCConnectionHandler)(id<GNCConnection> connection);

/**
 * This handler takes a response to a connection request. Pass @c GNCConnectionResponseAccept to
 * accept the request and @c GNCConnectionResponseReject to reject it.
 */
typedef void (^GNCConnectionResponseHandler)(GNCConnectionResponse response);

NS_ASSUME_NONNULL_END
