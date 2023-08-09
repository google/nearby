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

/**
 * Indicates the status of a connection.
 */
typedef NS_CLOSED_ENUM(NSInteger, GNCStatus) {
  GNCStatusSuccess,
  GNCStatusError,
  GNCStatusOutOfOrderApiCall,
  GNCStatusAlreadyHaveActiveStrategy,
  GNCStatusAlreadyAdvertising,
  GNCStatusAlreadyDiscovering,
  GNCStatusAlreadyListening,
  GNCStatusEndpointIoError,
  GNCStatusEndpointUnknown,
  GNCStatusConnectionRejected,
  GNCStatusAlreadyConnectedToEndpoint,
  GNCStatusNotConnectedToEndpoint,
  GNCStatusBluetoothError,
  GNCStatusBleError,
  GNCStatusWifiLanError,
  GNCStatusPayloadUnknown,
  GNCStatusUnknown,
  GNCStatusReset,
  GNCStatusTimeout,
};

/**
 * The @c GNCConnectionDelegate protocol defines methods that a delegate can implement to handle
 * connection-related events.
 */
@protocol GNCConnectionDelegate

/**
 * A basic encrypted channel has been created between you and the endpoint. Both sides are now
 * asked if they wish to accept or reject the connection before any data can be sent over this
 * channel.
 *
 * This is your chance, before you accept the connection, to confirm that you connected to the
 * correct device. Both devices are given an identical token; it's up to you to decide how to
 * verify it before proceeding. Typically this involves showing the token on both devices and
 * having the users manually compare and confirm; however, this is only required if you desire a
 * secure connection between the devices.
 *
 * Whichever route you decide to take (including not authenticating the other device), call
 * GNCCoreAdapter::acceptConnectionRequestFromEndpoint:delegate: when you're ready to talk, or
 * GNCCoreAdapter::rejectConnectionRequestFromEndpoint:withCompletionHandler: to close the
 * connection.
 *
 * @param endpointID The identifier for the remote endpoint.
 * @param info Other relevant information about the connection.
 * @param authenticationToken The token to be verified.
 */
- (void)connectedToEndpoint:(nonnull NSString *)endpointID
           withEndpointInfo:(nonnull NSData *)info
        authenticationToken:(nonnull NSString *)authenticationToken;

/**
 * Called after both sides have accepted the connection. Both sides may now send Payloads to each
 * other.
 *
 * @param endpointID The identifier for the remote endpoint.
 */
- (void)acceptedConnectionToEndpoint:(nonnull NSString *)endpointID;

/**
 * Called when either side rejected the connection. Payloads can not be exchaged. Call
 * GNCCoreAdapter::disconnectFromEndpoint:withCompletionHandler: to terminate the connection.
 *
 * @param endpointID The identifier for the remote endpoint.
 * @param status The reason for rejection.
 */
- (void)rejectedConnectionToEndpoint:(nonnull NSString *)endpointID withStatus:(GNCStatus)status;

/**
 * Called when a remote endpoint is disconnected or has become unreachable. At this point service
 * (re-)discovery may start again.
 *
 * @param endpointID The identifier for the remote endpoint.
 */
- (void)disconnectedFromEndpoint:(nonnull NSString *)endpointID;

// TODO(b/239832442): Add OnBandwidthChanged(const std::string &endpoint_id, Medium medium).

@end
