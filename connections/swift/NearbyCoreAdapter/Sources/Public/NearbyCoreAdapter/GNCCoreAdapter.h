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

@class GNCAdvertisingOptions;
@class GNCConnectionOptions;
@class GNCDiscoveryOptions;
@class GNCPayload;

@protocol GNCConnectionDelegate;
@protocol GNCDiscoveryDelegate;
@protocol GNCPayloadDelegate;

/**
 * This class defines the API of the Nearby Connections Core library.
 */
@interface GNCCoreAdapter : NSObject

/**
 * A @c GNCCoreAdapter singleton.
 */
@property(nonnull, nonatomic, class, readonly) GNCCoreAdapter *shared;

/**
 * Starts advertising an endpoint for a local app.
 *
 * @param serviceID An identifier to advertise your app to other endpoints. This can be an arbitrary
 *                  string, so long as it uniquely identifies your service. A good default is to use
 *                  your app's package name.
 * @param endpointInfo Arbitrary bytes of additional data that can be read and used by remote
 *                     endpoints. A good default is a utf-8 encoded string to represent a local
 *                     endpoint name.
 * @param advertisingOptions The options for advertising.
 * @param delegate A delegate to handle notifications about connection events.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)startAdvertisingAsService:(nonnull NSString *)serviceID
                     endpointInfo:(nonnull NSData *)endpointInfo
                          options:(nonnull GNCAdvertisingOptions *)advertisingOptions
                         delegate:(nullable id<GNCConnectionDelegate>)delegate
            withCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Stops advertising a local endpoint. Should be called after calling
 * startAdvertisingAsService:endpointInfo:options:delegate:withCompletionHandler:, as soon as the
 * application no longer needs to advertise itself or goes inactive. Payloads can still be sent to
 * connected endpoints after advertising ends.
 *
 * @param completionHandler Access the status of the operation when available.
 */
- (void)stopAdvertisingWithCompletionHandler:
    (nullable void (^)(NSError *_Nonnull error))completionHandler;

/**
 * Starts discovery for remote endpoints with the specified service ID.
 *
 * @param serviceID The ID for the service to be discovered, as specified in the corresponding call
 *                  to start advertising.
 * @param discoveryOptions The options for discovery.
 * @param delegate A delegate to handle notifications about endpoint discovery events.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)startDiscoveryAsService:(nonnull NSString *)serviceID
                        options:(nonnull GNCDiscoveryOptions *)discoveryOptions
                       delegate:(nullable id<GNCDiscoveryDelegate>)delegate
          withCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Stops discovery for remote endpoints, after a previous call to
 * startDiscoveryAsService:options:delegate:withCompletionHandler:, when the client no longer needs
 * to discover endpoints or goes inactive. Payloads can still be sent to connected endpoints after
 * discovery ends.
 *
 * @param completionHandler Access the status of the operation when available.
 */
- (void)stopDiscoveryWithCompletionHandler:
    (nullable void (^)(NSError *_Nonnull error))completionHandler;

/**
 * Sends a request to connect to a remote endpoint.
 *
 * @param endpointID The identifier for the remote endpoint to which a connection request will be
 *                   sent. Should match the endpointID provided by the
 *                   GNCDiscoveryDelegate::foundEndpoint:withEndpointInfo: delegate method.
 * @param endpointInfo Arbitrary bytes of additional data that can be read and used by remote
 *                     endpoints. A good default is a utf-8 encoded string to represent a local
 *                     endpoint name.
 * @param connectionOptions The options for connecting.
 * @param delegate A delegate to handle notifications about connection events.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)requestConnectionToEndpoint:(nonnull NSString *)endpointID
                       endpointInfo:(nonnull NSData *)endpointInfo
                            options:(nonnull GNCConnectionOptions *)connectionOptions
                           delegate:(nullable id<GNCConnectionDelegate>)delegate
              withCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Accepts a connection to a remote endpoint. This method must be called before payloads can be
 * exchanged with the remote endpoint.
 * @param endpointID The identifier for the remote endpoint. Should match the endpointID provided by
 *                   the
 * GNCConnectionDelegate::connectedToEndpoint:withEndpointInfo:authenticationToken: delegate method.
 * @param delegate A delegate to handle notifications about incoming payload events.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)acceptConnectionRequestFromEndpoint:(nonnull NSString *)endpointID
                                   delegate:(nullable id<GNCPayloadDelegate>)delegate
                      withCompletionHandler:
                          (nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Rejects a connection to a remote endpoint.
 *
 * @param endpointID The identifier for the remote endpoint. Should match the endpointID provided by
 *                   the
 * GNCConnectionDelegate::connectedToEndpoint:withEndpointInfo:authenticationToken: delegate method.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)rejectConnectionRequestFromEndpoint:(nonnull NSString *)endpointID
                      withCompletionHandler:
                          (nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Sends a payload to a list of remote endpoints. Payloads can only be sent to remote endpoints once
 * a notice of connection acceptance has been delivered via
 * GNCConnectionDelegate::acceptedConnectionToEndpoint:.
 *
 * @param payload The Payload to be sent.
 * @param endpointIDs List of remote endpoint identifiers to which the payload should be sent.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)sendPayload:(nonnull GNCPayload *)payload
              toEndpoints:(nonnull NSArray<NSString *> *)endpointIDs
    withCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Cancels a payload currently in-flight to or from remote endpoint(s).
 *
 * @param payloadID The identifier for the Payload to be canceled.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)cancelPayload:(int64_t)payloadID
    withCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Disconnects from a remote endpoint. Payloads can no longer be sent to or received from the
 * endpoint after this method is called.
 *
 * @param endpointID The identifier for the remote endpoint to disconnect from.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)disconnectFromEndpoint:(nonnull NSString *)endpointID
         withCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Disconnects from, and removes all traces of, all connected and/or discovered endpoints. This call
 * is expected to be preceded by a call to stop advertising or discovery as needed. After calling
 * stopAllEndpointsWithCompletionHandler:, no further operations with remote endpoints will be
 * possible until a new call to start advertising or discovery.
 *
 * @param completionHandler Access the status of the operation when available.
 */
- (void)stopAllEndpointsWithCompletionHandler:
    (nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * Sends a request to initiate connection bandwidth upgrade.
 *
 * @param endpointID The identifier for the remote endpoint which will be switching to a higher
 *                   connection data rate and possibly different wireless protocol.
 * @param completionHandler Access the status of the operation when available.
 */
- (void)initiateBandwidthUpgrade:(nonnull NSString *)endpointID
           withCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;

/**
 * The local endpoint indentifier generated by Nearby Connections.
 */
@property(nonnull, nonatomic, readonly, copy) NSString *localEndpointID;

@end
