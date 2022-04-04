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

#import "GNCConnection.h"

NS_ASSUME_NONNULL_BEGIN

/** This contains info about a discoverer endpoint intitiating a connection with an advertiser. */
@protocol GNCAdvertiserConnectionInfo <NSObject>
/** Information advertised by the remote endpoint. */
@property(nonatomic, readonly, copy) NSData *endpointInfo;
/** This token can be used to verify the identity of the discoverer. */
@property(nonatomic, readonly, copy) NSString *authToken;
@end

/** This class contains success and failure handlers for the connection request. */
@interface GNCConnectionResultHandlers : NSObject

/**
 * This factory method creates a pair of handlers for a successful or failed connection.
 *
 * @param successHandler This handler is called if both endpoints accept the connection.
 *                       A @c GNCConnection object is passed, meaning that the connection has
 *                       been established and you may start sending and receiving payloads.
 * @param failureHandler This handler is called if either endpoint rejects the connection.
 */
+ (instancetype)successHandler:(GNCConnectionHandler)successHandler
                failureHandler:(GNCConnectionFailureHandler)failureHandler;

@end

/**
 * This handler is called when a discoverer requests a connection with an advertiser. In
 * response, the advertiser should accept or reject via @c responseHandler.
 *
 * @param endpointId The ID of the endpoint.
 * @param connectionInfo Information about the discoverer.
 * @param responseHandler Handler for the connection response, which is either an acceptance or
 *                        rejection of the connection request.
 * @return Handlers for the final connection result. This will be called as soon as the final
 *         connection result is known, when either side rejects or both sides accept.
 */
typedef GNCConnectionResultHandlers *_Nonnull (^GNCAdvertiserConnectionInitiationHandler)(
    GNCEndpointId endpointId, id<GNCAdvertiserConnectionInfo> connectionInfo,
    GNCConnectionResponseHandler responseHandler);

/**
 *  An advertiser broadcasts a service that can be seen by discoverers, which can then make
 *  requests to connect to it. Release the advertiser object to stop advertising.
 */
@interface GNCAdvertiser : NSObject

/**
 * Factory method that creates an advertiser.
 *
 * @param endpointInfo A data for endpoint info which contains readable name of this endpoint,
 *                     to be displayed on other endpoints.
 * @param serviceId A string that uniquely identifies the advertised service.
 * @param strategy The connection topology to use.
 * @param connectionInitiationHandler A handler that is called when a discoverer requests a
 *                                    connection with this endpoint.
 */
+ (instancetype)advertiserWithEndpointInfo:(NSData *)endpointInfo
                                 serviceId:(NSString *)serviceId
                                  strategy:(GNCStrategy)strategy
               connectionInitiationHandler:
                   (GNCAdvertiserConnectionInitiationHandler)connectionInitiationHandler;

@end

NS_ASSUME_NONNULL_END
