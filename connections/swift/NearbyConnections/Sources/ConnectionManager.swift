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

import NearbyCoreAdapter

/// Manages communication between the local endpoint and all remote endpoints connected to it.
public class ConnectionManager {

  /// The delegate object that handles connection-related events.
  public weak var delegate: ConnectionManagerDelegate?

  /// The service identifier to advertise as or search for.
  public let serviceID: String

  /// A value indicating the connection strategy and restrictions for advertisement or discovery.
  public let strategy: Strategy

  /// `DispatchQueue` on which all delegate methods are called.
  public let queue: DispatchQueue

  private var transfers: [PayloadID: [EndpointID: Progress]] = [:]

  lazy var payload: InternalPayload? = {
    let payload = InternalPayload()
    payload.delegate = self
    return payload
  }()

  /// Initializes the connection manager object.
  ///
  /// - Parameters:
  ///   - serviceID: The indentifier of your service advertised to other endpoints or to be searched
  ///     for while discovering other endpoints. This can be an arbitrary string, so long as it
  ///     uniquely identifies your service. A good default is to use your app’s package name.
  ///   - strategy: Connection strategy to be used during advertisement or discovery.
  ///   - queue: `DispatchQueue` on which all delegate methods are called. `.main` by default.
  public init(serviceID: String, strategy: Strategy, queue: DispatchQueue = .main) {
    self.serviceID = serviceID
    self.strategy = strategy
    self.queue = queue
  }

  /// Sends a message as `Data` to nearby endpoints.
  ///
  /// This method is asynchronous (nonblocking).
  ///
  /// On the recipient device, the connection manager instance calls its delegate’s
  /// `connectionManager(_:didReceive:withID:from:)` method with the message after it has been fully
  /// received.
  ///
  /// On both the local and recipient device, the connection manager instance calls its delegate’s
  /// `connectionManager(_:didReceiveTransferUpdate:from:forPayload:)` method when delivery
  /// succeeds, has a progress update or when an in-flight error occurs.
  ///
  /// - Parameters:
  ///   - data: An instance containing the message to send.
  ///   - endpointIDs: An array of endpoint IDs representing the endpoints that should receive the
  ///     message.
  ///   - id: A unique ID for the payload. This ID is provided to the remote endpoint. A random ID
  ///     will be generated if none is set.
  ///   - completionHandler: A closure that gets called when the framework attempts to send the
  ///     message. Upon success, the handler is called with an error value of nil. Upon failure,
  ///     the handle is called with an error object that indicates what went wrong. This does not
  ///     indicate that the send has successfully completed yet. Errors might still occur during
  ///     transmission (and at different times for different endpoints).
  ///     `connectionManager(_:didReceiveTransferUpdate:from:forPayload:)`is called when delivery
  ///     succeeds or when an in-flight error occurs.
  /// - Returns: A cancellation token used to cancel the transfer.
  public func send(
    _ data: Data, to endpointIDs: [EndpointID], id: PayloadID? = nil,
    completionHandler: ((Error?) -> Void)? = nil
  )
    -> CancellationToken
  {
    let payload: GNCBytesPayload
    if let id = id {
      payload = GNCBytesPayload(data: data, identifier: id)
    } else {
      payload = GNCBytesPayload(data: data)
    }

    return send(payload, to: endpointIDs, completionHandler: completionHandler)
  }

  /// Opens a byte stream to nearby endpoints.
  ///
  /// This method is asynchronous (nonblocking).
  ///
  /// On the recipient device, the connection manager instance calls its delegate’s
  /// `connectionManager(_:didReceive:withID:from:cancellationToken:)` method with the stream.
  ///
  /// On both the local and recipient device, the connection manager instance calls its delegate’s
  /// `connectionManager(_:didReceiveTransferUpdate:from:forPayload:)` method when delivery
  /// succeeds, has a progress update or when an in-flight error occurs.
  ///
  /// - Parameters:
  ///   - stream:
  ///   - endpointIDs: An array of endpoint IDs representing the endpoints that should receive this
  ///     stream.
  ///   - id: A unique ID for the payload. This ID is provided to the remote endpoint. A random ID
  ///     will be generated if none is set.
  ///   - completionHandler: A closure that gets called when the framework attempts to send the
  ///     stream. Upon success, the handler is called with an error value of nil. Upon failure,
  ///     the handle is called with an error object that indicates what went wrong. This does not
  ///     indicate that the send has successfully completed yet. Errors might still occur during
  ///     transmission (and at different times for different endpoints).
  ///     `connectionManager(_:didReceiveTransferUpdate:from:forPayload:)`is called when delivery
  ///     succeeds or when an in-flight error occurs.
  /// - Returns: A cancellation token used to cancel the transfer.
  public func startStream(
    _ stream: InputStream, to endpointIDs: [EndpointID], id: PayloadID? = nil,
    completionHandler: ((Error?) -> Void)? = nil
  ) -> CancellationToken {
    let payload: GNCStreamPayload
    if let id = id {
      payload = GNCStreamPayload(stream: stream, identifier: id)
    } else {
      payload = GNCStreamPayload(stream: stream)
    }

    return send(payload, to: endpointIDs, completionHandler: completionHandler)
  }

  /// Sends the contents of a URL to nearby endpoints.
  ///
  /// This method is asynchronous (nonblocking).
  ///
  /// On the recipient device, the connection manager calls its delegate’s
  /// `connectionManager(_:didStartReceivingResourceWithID:from:at:withName:cancellationToken:)`
  /// method as soon as it begins receiving the resource.
  ///
  /// On both the local and recipient device, the connection manager instance calls its delegate’s
  /// `connectionManager(_:didReceiveTransferUpdate:from:forPayload:)` method when delivery
  /// succeeds, has a progress update or when an in-flight error occurs.
  ///
  /// - Parameters:
  ///   - resourceURL: A file URL.
  ///   - resourceName: A name for the resource.
  ///   - endpointIDs: An array of endpoint IDs representing the endpoints that should receive this
  ///     resource.
  ///   - id: A unique ID for the payload. This ID is provided to the remote endpoint. A random ID
  ///     will be generated if none is set.
  ///   - completionHandler: A closure that gets called when the framework attempts to send the
  ///     resource. Upon success, the handler is called with an error value of nil. Upon failure,
  ///     the handle is called with an error object that indicates what went wrong. This does not
  ///     indicate that the send has successfully completed yet. Errors might still occur during
  ///     transmission (and at different times for different endpoints).
  ///     `connectionManager(_:didReceiveTransferUpdate:from:forPayload:)`is called when delivery
  ///     succeeds or when an in-flight error occurs.
  /// - Returns: A cancellation token used to cancel the transfer.
  public func sendResource(
    at resourceURL: URL, withName resourceName: String, to endpointIDs: [EndpointID],
    id: PayloadID? = nil, completionHandler: ((Error?) -> Void)? = nil
  ) -> CancellationToken {
    let remotePath = URL(fileURLWithPath: resourceName)
    let parentFolder = remotePath.deletingLastPathComponent().relativePath
    let fileName = remotePath.lastPathComponent

    let payload: GNCFilePayload
    if let id = id {
      payload = GNCFilePayload(
        fileURL: resourceURL, parentFolder: parentFolder, fileName: fileName, identifier: id)
    } else {
      payload = GNCFilePayload(fileURL: resourceURL, parentFolder: parentFolder, fileName: fileName)
    }

    return send(payload, to: endpointIDs, completionHandler: completionHandler)
  }

  private func send(
    _ payload: GNCPayload, to endpointIDs: [EndpointID],
    completionHandler: ((Error?) -> Void)? = nil
  ) -> CancellationToken {
    for endpointID in endpointIDs {
      if transfers[payload.identifier] == nil {
        transfers[payload.identifier] = [:]
      }
      transfers[payload.identifier]?[endpointID] = Progress()
    }

    GNCCoreAdapter.shared.send(
      payload,
      toEndpoints: endpointIDs,
      withCompletionHandler: completionHandler
    )

    return CancellationToken(payload.identifier)
  }

  /// Disconnect from a remote endpoint.
  ///
  /// - Parameters:
  ///   - endpointID: The ID of the remote endpoint.
  ///   - completionHandler: Called with `nil` when the endpoint has been disconnected,
  ///     or an error if disconnecting failed
  public func disconnect(
    from endpointID: EndpointID,
    completionHandler: ((Error?) -> Void)? = nil
  ) {
    GNCCoreAdapter.shared.disconnect(
      fromEndpoint: endpointID,
      withCompletionHandler: completionHandler
    )
  }
}

extension ConnectionManager: InternalPayloadDelegate {

  func receivedPayload(_ payload: GNCPayload, fromEndpoint endpointID: String) {
    queue.async { [weak self] in
      guard let self = self else { return }
      if self.transfers[payload.identifier] == nil {
        self.transfers[payload.identifier] = [:]
      }
      self.transfers[payload.identifier]?[endpointID] = Progress()

      let payloadID = payload.identifier
      let cancellationToken = CancellationToken(payload.identifier)

      if let bytesPayload = payload as? GNCBytesPayload {
        self.delegate?.connectionManager(
          self, didReceive: bytesPayload.data, withID: payloadID, from: endpointID)
      }
      if let streamPayload = payload as? GNCStreamPayload {
        self.delegate?.connectionManager(
          self, didReceive: streamPayload.stream, withID: payloadID, from: endpointID,
          cancellationToken: cancellationToken)
      }
      if let filePayload = payload as? GNCFilePayload {
        let name = (filePayload.parentFolder as NSString).appendingPathComponent(
          filePayload.fileName)
        self.delegate?.connectionManager(
          self, didStartReceivingResourceWithID: payloadID, from: endpointID,
          at: filePayload.fileURL, withName: name, cancellationToken: cancellationToken)
      }
    }
  }

  func receivedProgressUpdate(
    forPayload payloadID: Int64, with status: GNCPayloadStatus, fromEndpoint endpointID: String,
    bytesTransfered: Int64, totalBytes: Int64
  ) {
    queue.async { [weak self] in
      guard let self = self, let progress = self.transfers[payloadID]?[endpointID] else { return }

      progress.totalUnitCount = totalBytes
      progress.completedUnitCount = bytesTransfered
      switch status {
      case .success:
        self.transfers[payloadID]?.removeValue(forKey: endpointID)
        self.delegate?.connectionManager(
          self, didReceiveTransferUpdate: .success, from: endpointID, forPayload: payloadID
        )
      case .failure:
        self.transfers[payloadID]?.removeValue(forKey: endpointID)

        self.delegate?.connectionManager(
          self,
          didReceiveTransferUpdate: .failure,
          from: endpointID,
          forPayload: payloadID
        )
        break
      case .canceled:
        self.transfers[payloadID]?.removeValue(forKey: endpointID)
        self.delegate?.connectionManager(
          self, didReceiveTransferUpdate: .canceled, from: endpointID, forPayload: payloadID)
      case .inProgress:
        self.delegate?.connectionManager(
          self, didReceiveTransferUpdate: .progress(progress), from: endpointID,
          forPayload: payloadID)
      }
      if self.transfers[payloadID]?.isEmpty ?? false {
        self.transfers.removeValue(forKey: payloadID)
      }
    }
  }
}

/// The `ConnectionManagerDelegate` protocol defines methods that a `ConnectionManager`  object’s
/// delegate can implement to handle connection-related events.
///
/// Delegate methods are executed on the `.main` queue by default, but a specific `DispatchQueue` on
/// which to call the delegate methods can be passed to `ConnectionManager`.
public protocol ConnectionManagerDelegate: AnyObject {

  /// Called to validate the verification code generated for the pair of endpoints.
  ///
  /// Your app should confirm whether you connected to the correct endpoint. Typically this involves
  /// showing the verification code on both devices and having the users manually compare and
  /// confirm; however, this is only required if you desire a secure connection between the devices.
  /// Upon confirmation, your app should call the provided `verificationHandler` block, passing
  /// either `true` (to trust the nearby endpoint) or `false` (to reject it).
  ///
  /// Important: The Nearby Connections framework makes no attempt to validate the verification
  /// code in any way. If your delegate does not implement this method, all verification codes
  /// are accepted automatically.
  ///
  /// - Parameters:
  ///   - connectionManager: The connection manager object through which the verification code was
  ///     received.
  ///   - verificationCode: An identical token given to both endpoints.
  ///   - endpointID: The endpoint ID of the nearby endpoint that needs verification.
  ///   - verificationHandler: Your app should call this handler with a value of `true` if the
  ///     nearby endpoint should be trusted, or `false` otherwise.
  func connectionManager(
    _ connectionManager: ConnectionManager, didReceive verificationCode: String,
    from endpointID: EndpointID, verificationHandler: @escaping (Bool) -> Void)

  /// Indicates that a `Data` object has been received from a nearby endpoint.
  ///
  /// - Parameters:
  ///   - connectionManager: The connection manager object through which the data was received.
  ///   - data: An object containing the received data.
  ///   - payloadID: The ID of the payload, as provided by the sender.
  ///   - endpointID: The endpoint ID of the sender.
  func connectionManager(
    _ connectionManager: ConnectionManager, didReceive data: Data, withID payloadID: PayloadID,
    from endpointID: EndpointID)

  /// Called when a nearby endpoint opens a byte stream connection to the local endpoint.
  ///
  /// - Parameters:
  ///   - connectionManager: The connection manager object through which the byte stream was
  ///     received.
  ///   - stream: An `InputStream` object that represents the local endpoint for the byte stream.
  ///   - payloadID: The ID of the payload, as provided by the sender.
  ///   - endpointID: The endpoint ID of the originator of the stream.
  ///   - token: Token used to cancel the stream.
  func connectionManager(
    _ connectionManager: ConnectionManager, didReceive stream: InputStream,
    withID payloadID: PayloadID,
    from endpointID: EndpointID, cancellationToken token: CancellationToken)

  /// Indicates that the local endpoint began receiving a resource from a nearby endpoint.
  ///
  /// - Parameters:
  ///   - connectionManager: The connection manager object that started receiving the resource.
  ///   - payloadID: The ID of the payload, as provided by the sender.
  ///   - endpointID: The endpoint ID of the sender.
  ///   - localURL: Provides the location of a file containing the data being received.
  ///   - name: The name of the resource, as provided by the sender.
  ///   - token: Token used to cancel the resource transfer.
  func connectionManager(
    _ connectionManager: ConnectionManager, didStartReceivingResourceWithID payloadID: PayloadID,
    from endpointID: EndpointID, at localURL: URL, withName name: String,
    cancellationToken token: CancellationToken)

  /// Called when a progress update is available for an incoming or outgoing transfer.
  ///
  /// - Parameters:
  ///   - connectionManager: The connection manager object that received the update.
  ///   - update: A success, failure, cancellation or progress update.
  ///   - endpointID: The remote endpoint ID.
  ///   - payloadID: The ID of the payload, as provided by the sender.
  func connectionManager(
    _ connectionManager: ConnectionManager, didReceiveTransferUpdate update: TransferUpdate,
    from endpointID: EndpointID, forPayload payloadID: PayloadID)

  /// Called when the status of the connection to a nearby endpoint changes.
  ///
  /// - Parameters:
  ///   - connectionManager: The connection manager object that manages the connection whose state
  ///     changed.
  ///   - state: The new state of the connection.
  ///   - endpointID: The ID of the nearby endpoint whose connection status has changed.
  func connectionManager(
    _ connectionManager: ConnectionManager, didChangeTo state: ConnectionState,
    for endpointID: EndpointID
  )
}

extension ConnectionManagerDelegate {
  public func connectionManager(
    _ connectionManager: ConnectionManager, didReceive verificationCode: String,
    from endpointID: EndpointID, verificationHandler: @escaping (Bool) -> Void
  ) {
    verificationHandler(true)
  }
}
