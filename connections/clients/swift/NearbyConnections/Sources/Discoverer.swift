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

/// Searches for nearby endpoints and provides the ability to request connections to them.
public class Discoverer {

  /// The delegate object that handles discovery-related events.
  public weak var delegate: DiscovererDelegate?

  /// The connection manager for this instance.
  public let connectionManager: ConnectionManager

  lazy var connection: InternalConnection? = {
    let connection = InternalConnection()
    connection.delegate = self
    return connection
  }()

  lazy var discovery: InternalDiscovery? = {
    let discovery = InternalDiscovery()
    discovery.delegate = self
    return discovery
  }()

  /// Initializes the discoverer object.
  ///
  /// - Parameter connectionManager: The connection manager for this instance.
  public init(connectionManager: ConnectionManager) {
    self.connectionManager = connectionManager
  }

  /// Starts searching for nearby remote endpoints.
  ///
  /// After this method is called (until you call `stopDiscovery()`), the framework calls your
  /// delegate’s `discoverer(_:didFind:with:)` and `discoverer(_:didLose:)` methods as new endpoints
  /// are found and lost.
  public func startDiscovery() {
    let options = GNCDiscoveryOptions(strategy: connectionManager.strategy.objc)
    // TODO(b/257824412): Handle errors from completion handler.
    GNCCoreAdapter.shared.startDiscovery(
      asService: connectionManager.serviceID, options: options, delegate: discovery)
  }

  /// Stops searching for nearby remote endpoints.
  public func stopDiscovery() {
    // TODO(b/257824412): Handle errors from completion handler.
    GNCCoreAdapter.shared.stopDiscovery()
  }

  /// Requests a connection to a discovered remote endpoint.
  ///
  /// - Parameters:
  ///   - endpointID: The ID of the endpoint to request a connection to.
  ///   - context: An arbitrary piece of data that is passed to the nearby endpoint. This can be
  ///     used to provide further information to the user about the nature of the invitation.
  public func requestConnection(to endpointID: EndpointID, using context: Data) {
    let options = GNCConnectionOptions()
    // TODO(b/257824412): Handle errors from completion handler.
    GNCCoreAdapter.shared.requestConnection(
      toEndpoint: endpointID, endpointInfo: context,
      options: options, delegate: connection)
  }

  deinit {
    stopDiscovery()
  }
}

extension Discoverer: InternalConnectionDelegate {

  func connected(
    toEndpoint endpointID: String, withEndpointInfo info: Data, authenticationToken: String
  ) {
    connectionManager.queue.async { [weak self] in
      guard let connectionManager = self?.connectionManager else { return }
      connectionManager.delegate?.connectionManager(
        connectionManager, didChangeTo: .connecting, for: endpointID)
      connectionManager.delegate?.connectionManager(
        connectionManager, didReceive: authenticationToken, from: endpointID
      ) { accept in
        guard accept else {
          // TODO(b/257824412): Handle errors from completion handler.
          GNCCoreAdapter.shared.rejectConnectionRequest(fromEndpoint: endpointID)
          return
        }
        // TODO(b/257824412): Handle errors from completion handler.
        GNCCoreAdapter.shared.acceptConnectionRequest(
          fromEndpoint: endpointID, delegate: connectionManager.payload)
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

extension Discoverer: InternalDiscoveryDelegate {

  func foundEndpoint(_ endpointID: String, withEndpointInfo info: Data) {
    connectionManager.queue.async { [weak self] in
      guard let self = self else { return }
      self.delegate?.discoverer(self, didFind: endpointID, with: info)
    }
  }

  func lostEndpoint(_ endpointID: String) {
    connectionManager.queue.async { [weak self] in
      guard let self = self else { return }
      self.delegate?.discoverer(self, didLose: endpointID)
    }
  }
}

/// The `DiscovererDelegate` protocol defines methods that a `Discoverer` object’s delegate can
/// implement to handle discovery-related events.
///
/// Delegate methods are executed on the `.main` queue by default, but a specific `DispatchQueue` on
/// which to call the delegate methods can be passed to `ConnectionManager`.
public protocol DiscovererDelegate: AnyObject {

  /// Called when a nearby endpoint is found.
  ///
  /// The endpoint ID provided to this delegate method can be used to request a connection with the
  /// nearby endpoint.
  ///
  /// - Parameters:
  ///   - discoverer: The discoverer object that found the nearby endpoint.
  ///   - endpointID: The unique ID of the endpoint that was found.
  ///   - context: An arbitrary piece of data received from the nearby endpoint. This can be used to
  ///     provide further information to the user about the nature of the invitation.
  func discoverer(_ discoverer: Discoverer, didFind endpointID: EndpointID, with context: Data)

  /// Called when a nearby endpoint is lost.
  ///
  /// This callback informs your app that connection requests can no longer be sent to an endpoint,
  /// and that your app should remove that endpoint from its user interface.
  ///
  /// Important: Because there is a delay between when a discovered endpoint disappears and when
  /// Nearby Connections detects that it has left, the fact that your app has not yet received a
  /// disappearance callback does not guarantee that it can communicate with the endpoint
  /// successfully.
  ///
  /// - Parameters:
  ///   - discoverer: The discoverer object that lost the nearby endpoint.
  ///   - endpointID: The unique ID of the nearby endpoint that was lost.
  func discoverer(_ discoverer: Discoverer, didLose endpointID: EndpointID)
}
