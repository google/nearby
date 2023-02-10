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

import Foundation
import NearbyCoreAdapter

/// Publishes an advertisement and notifies its delegate about connection requests from nearby
/// endpoints.
public class Advertiser {

  /// The delegate object that handles advertising-related events.
  public weak var delegate: AdvertiserDelegate?

  /// The connection manager for this instance.
  public let connectionManager: ConnectionManager

  lazy var connection: InternalConnection? = {
    let connection = InternalConnection()
    connection.delegate = self
    return connection
  }()

  /// Initializes the discoverer object.
  ///
  /// - Parameter connectionManager: The connection manager for this instance.
  public init(connectionManager: ConnectionManager) {
    self.connectionManager = connectionManager
  }

  /// Starts advertising the local endpoint.
  ///
  /// After this method is called (until you call `stopAdvertising()`), the framework calls your
  /// delegate’s `advertiser(_:didReceiveConnectionRequestFrom:with:connectionRequestHandler:)`
  /// method when remote endpoints request a connection.
  ///
  /// - Parameters:
  ///   - context: An arbitrary piece of data that is advertised to the nearby endpoint.
  ///   This can be used to provide further information to the user about the nature of the
  ///   advertisement.
  ///   - completionHandler: Called with `nil` if advertising started, or an error if advertising
  ///   failed to start.
  ///
  public func startAdvertising(using context: Data, completionHandler: ((Error?) -> Void)? = nil) {
    let options = GNCAdvertisingOptions(strategy: connectionManager.strategy.objc)
    GNCCoreAdapter.shared.startAdvertising(
      asService: connectionManager.serviceID, endpointInfo: context,
      options: options, delegate: connection, withCompletionHandler: completionHandler)
  }

  /// Stops advertising the local endpoint.
  ///
  /// - Parameter completionHandler: Called with `nil` if advertising stopped, or an error if
  /// advertising failed to stop.
  public func stopAdvertising(completionHandler: ((Error?) -> Void)? = nil) {
    GNCCoreAdapter.shared.stopAdvertising(completionHandler: completionHandler)
  }

  deinit {
    stopAdvertising()
  }
}

extension Advertiser: InternalConnectionDelegate {

  func connected(
    toEndpoint endpointID: String, withEndpointInfo info: Data, authenticationToken: String
  ) {
    connectionManager.queue.async { [weak self] in
      guard let self = self else { return }
      self.delegate?.advertiser(self, didReceiveConnectionRequestFrom: endpointID, with: info) {
        (accept) in
        guard accept else {
          GNCCoreAdapter.shared.rejectConnectionRequest(fromEndpoint: endpointID) {
            (error: Error?) in
            if let error = error {
              NSLog(
                """
                  Encountered an error while attempting to reject a connection request for \
                  endpoint %@: %@
                """, endpointID, "\(error)")
            }
          }
          return
        }
        self.connectionManager.delegate?.connectionManager(
          self.connectionManager, didChangeTo: .connecting, for: endpointID)
        self.connectionManager.delegate?.connectionManager(
          self.connectionManager, didReceive: authenticationToken, from: endpointID
        ) { accept in
          guard accept else {
            GNCCoreAdapter.shared.rejectConnectionRequest(fromEndpoint: endpointID) {
              (error: Error?) in
              if let error = error {
                NSLog(
                  """
                    Encountered an error while attempting to reject a connection request for \
                    endpoint %@: %@
                  """, endpointID, "\(error)")
              }
            }
            return
          }
          GNCCoreAdapter.shared.acceptConnectionRequest(
            fromEndpoint: endpointID, delegate: self.connectionManager.payload
          ) { (error: Error?) in
            if let error = error {
              NSLog(
                """
                  Encountered an error while attempting to accept a connection request for \
                  endpoint %@: %@
                """, endpointID, "\(error)")
            }
          }

        }
      }
    }
  }

  func acceptedConnection(toEndpoint endpointID: String) {
    connectionManager.queue.async { [weak self] in
      guard let connectionManager = self?.connectionManager else { return }
      connectionManager.delegate?.connectionManager(
        connectionManager, didChangeTo: .connected, for: endpointID)
    }
  }

  func rejectedConnection(toEndpoint endpointID: String, with status: GNCStatus) {
    connectionManager.queue.async { [weak self] in
      guard let connectionManager = self?.connectionManager else { return }
      connectionManager.delegate?.connectionManager(
        connectionManager, didChangeTo: .rejected, for: endpointID)
    }
  }

  func disconnected(fromEndpoint endpointID: String) {
    connectionManager.queue.async { [weak self] in
      guard let connectionManager = self?.connectionManager else { return }
      connectionManager.delegate?.connectionManager(
        connectionManager, didChangeTo: .disconnected, for: endpointID)
    }
  }
}

/// The `AdvertiserDelegate` protocol defines methods that an `Advertiser` object’s delegate can
/// implement to handle advertising-related events.
///
/// Delegate methods are executed on the `.main` queue by default, but a specific `DispatchQueue` on
/// which to call the delegate methods can be passed to `ConnectionManager`.
public protocol AdvertiserDelegate: AnyObject {

  /// Called when a connection request is received from a nearby endpoint.
  ///
  /// Important: Accepting a connection request will not yet establish a connection that allows
  /// data transference. The connection must first be verified by both parties via the
  /// `ConnectionManagerDelegate`, before data can flow freely.
  ///
  /// - Parameters:
  ///   - advertiser: The advertiser object that received a connection request.
  ///   - endpointID: The endpoint ID of the nearby enpoint that requested the connection.
  ///   - context: An arbitrary piece of data received from the nearby endpoint. This can be used to
  ///     provide further information to the user about the nature of the invitation.
  ///   - connectionRequestHandler: A block that your code must call to indicate whether the
  ///     advertiser should accept or decline the connection request.
  func advertiser(
    _ advertiser: Advertiser, didReceiveConnectionRequestFrom endpointID: EndpointID,
    with context: Data, connectionRequestHandler: @escaping (Bool) -> Void)
}
