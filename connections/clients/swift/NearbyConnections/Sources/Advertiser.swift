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
  public func startAdvertising(using context: Data) {
    let options = GNCAdvertisingOptions(strategy: connectionManager.strategy.objc)
    // TODO(b/257824412): Handle errors from completion handler.
    GNCCoreAdapter.shared.startAdvertising(
      asService: connectionManager.serviceID, endpointInfo: context,
      options: options, delegate: connection)
  }

  /// Stops advertising the local endpoint.
  public func stopAdvertising() {
    // TODO(b/257824412): Handle errors from completion handler.
    GNCCoreAdapter.shared.stopAdvertising()
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
          // TODO(b/257824412): Handle errors from completion handler.
          GNCCoreAdapter.shared.rejectConnectionRequest(fromEndpoint: endpointID)
          return
        }
        self.connectionManager.delegate?.connectionManager(
          self.connectionManager, didChangeTo: .connecting, for: endpointID)
        self.connectionManager.delegate?.connectionManager(
          self.connectionManager, didReceive: authenticationToken, from: endpointID
        ) { accept in
          guard accept else {
            // TODO(b/257824412): Handle errors from completion handler.
            GNCCoreAdapter.shared.rejectConnectionRequest(fromEndpoint: endpointID)
            return
          }
          // TODO(b/257824412): Handle errors from completion handler.
          GNCCoreAdapter.shared.acceptConnectionRequest(
            fromEndpoint: endpointID, delegate: self.connectionManager.payload)

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
  ///   - info: An arbitrary piece of data received from the nearby endpoint. This can be used to
  ///     provide further information to the user about the nature of the invitation.
  ///   - connectionRequestHandler: A block that your code must call to indicate whether the
  ///     advertiser should accept or decline the connection request.
  func advertiser(
    _ advertiser: Advertiser, didReceiveConnectionRequestFrom endpointID: EndpointID,
    with context: Data, connectionRequestHandler: @escaping (Bool) -> Void)
}
