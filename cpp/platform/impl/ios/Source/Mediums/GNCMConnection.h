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

/** Result of a medium payload transfer. */
typedef NS_ENUM(NSInteger, GNCMPayloadResult) {
  GNCMPayloadResultSuccess,   // Payload delivery was successful.
  GNCMPayloadResultFailure,   // An error occurred during payload delivery.
  GNCMPayloadResultCanceled,  // Payload delivery was canceled.
};

/** Handler for a @c GNCMPayloadResult value. */
typedef void (^GNCMPayloadResultHandler)(GNCMPayloadResult);

/**
 * A progress handler is periodically called during payload delivery. It is passed a value
 * ranging from 0 (when the operation has just started) to the total size (when the operation is
 * finished).
 */
typedef void (^GNCMProgressHandler)(size_t count);

/** This handler is called when data is received from a remote endpoint. */
typedef void (^GNCMPayloadHandler)(NSData *data);

/**
 * This represents a connection with a remote endpoint at the medium level. Use it to send
 * payloads to the remote endpoint, and release it to disconnect.
 */
@protocol GNCMConnection <NSObject>

/**
 * Sends data to the remote endpoint. Wait for the completion to be called before sending another
 * payload.
 *
 * @param payload         The data to send.
 * @param progressHandler Called repeatedly for progress feedback while the data is being sent.
 * @param completion      Callback called when the data has been fully sent,
 *                        or it has failed to be sent (not connected or disconnected).
 */
- (void)sendData:(NSData *)payload
    progressHandler:(GNCMProgressHandler)progressHandler
         completion:(GNCMPayloadResultHandler)completion;

@end

/** This class contains optional handlers for a connection. */
@interface GNCMConnectionHandlers : NSObject

/** This handler is called when data is sent from the remote endpoint. */
@property(nonatomic) GNCMPayloadHandler payloadHandler;

/** This handler is called when the connection is ended. */
@property(nonatomic) dispatch_block_t disconnectedHandler;

/** This method creates a GNCMConnectionHandlers object from payload and disconnect handlers. */
+ (instancetype)payloadHandler:(GNCMPayloadHandler)payloadHandler
           disconnectedHandler:(dispatch_block_t)disconnectedHandler;

@end

/**
 * This handler takes a GNCMConnection object and returns a GNCMConnectionHandlers object. It is
 * called when a connection is successfully made with a remote endpoint. If |connection| is nil,
 * the connection couldn't be established; in this case, return nil.
 */
typedef GNCMConnectionHandlers *_Nullable (^GNCMConnectionHandler)(
    id<GNCMConnection> __nullable connection);

/**
 * This handler is called by a discovering endpoint to request a connection with an an advertising
 * endpoint.
 */
typedef void (^GNCMConnectionRequester)(GNCMConnectionHandler connectionHandler);

/** This handler is called when a previously discovered advertising endpoint is lost. */
typedef void (^GNCMEndpointLostHandler)(void);

/**
 * This handler is called on a discoverer when a nearby advertising endpoint is
 * discovered. Calls |requestConnection| to request a connection with the advertiser.
 */
typedef GNCMEndpointLostHandler _Nonnull (^GNCMEndpointFoundHandler)(
    NSString *endpointId, NSString *serviceType, NSString *serviceName,
    NSDictionary<NSString *, NSData *> *_Nullable TXTRecordData,
    GNCMConnectionRequester requestConnection);

NS_ASSUME_NONNULL_END
