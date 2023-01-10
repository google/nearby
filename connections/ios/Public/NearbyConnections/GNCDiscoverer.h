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

/** This is info about an advertiser endpoint with which the discoverer has requested a connection.
 */
@protocol GNCDiscovererConnectionInfo <NSObject>
/** This token can be used to verify the identity of the advertiser. */
@property(nonatomic, readonly, copy) NSString *authToken;
@end

/**
 * This handler is called to establish authorization with the advertiser. In response,
 * @c responseHandler should be called to accept or reject the connection.
 *
 * @param connectionInfo Information about the advertiser.
 * @param responseHandler Handler for the connection response, which is either an acceptance or
 *                        rejection of the connection request.
 * @return Handler for the connection if it was successful.
 */
typedef GNCConnectionHandler _Nonnull (^GNCDiscovererConnectionInitializationHandler)(
    id<GNCDiscovererConnectionInfo> connectionInfo, GNCConnectionResponseHandler responseHandler);

/**
 * This handler should be called to request a connection with an advertiser.
 *
 * @param endpointInfo A data for endpoint info which contains readable name of this endpoint,
 *                     to be displayed on other endpoints.
 * @param authorizationHandler This handler is called to establish authorization.
 * @param failureHandler This handler is called if there was an error making the connection.
 */
typedef void (^GNCConnectionRequester)(
    NSData *endpointInfo,
    GNCDiscovererConnectionInitializationHandler connectionAuthorizationHandler,
    GNCConnectionFailureHandler failureHandler);

/** Information about an endpoint when it's discovered. */
@protocol GNCDiscoveredEndpointInfo <NSObject>
/** The human readable name of the remote endpoint. */
@property(nonatomic, readonly, copy) NSString *endpointName;
/** Information advertised by the remote endpoint. */
@property(nonatomic, readonly, copy) NSData *endpointInfo;
/** Call this block to request a connection with the advertiser. */
@property(nonatomic, readonly) GNCConnectionRequester requestConnection;
@end

/** This handler is called when a previously discovered advertiser endpoint is lost. */
typedef void (^GNCEndpointLostHandler)(void);

/**
 * This handler is called when an advertiser endpoint is discovered.
 *
 * @param endpointId The ID of the endpoint.
 * @param connectionInfo Information about the endpoint.
 * @return Block that is called when the endpoint is lost.
 */
typedef GNCEndpointLostHandler _Nonnull (^GNCEndpointFoundHandler)(
    GNCEndpointId endpointId, id<GNCDiscoveredEndpointInfo> endpointInfo);

/**
 * A discoverer searches for endpoints advertising the specified service, and allows connection
 * requests to be sent to them. Release the discoverer object to stop discovering.
 */
@interface GNCDiscoverer : NSObject

/**
 * Factory method that creates a discoverer.
 *
 * @param serviceId A string that uniquely identifies the advertised service to search for.
 * @param strategy The connection topology to use.
 * @param endpointFoundHandler This handler is called when an endpoint advertising the service is
 *                              discovered.
 */
+ (instancetype)discovererWithServiceId:(NSString *)serviceId
                               strategy:(GNCStrategy)strategy
                   endpointFoundHandler:(GNCEndpointFoundHandler)endpointFoundHandler;

@end

NS_ASSUME_NONNULL_END
