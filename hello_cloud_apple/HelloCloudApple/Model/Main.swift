//
//  Copyright 2023 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

import Foundation
import NearbyConnections
#if os(iOS) || os(watchOS) || os(tvOS)
import UIKit
#endif

@Observable class Main: ObservableObject {
  static let shared = createDebugModel()

  private(set) var localEndpointId: String = ""
  var localEndpointName: String = ""
  
  var isAdvertising = false {
    didSet {
      if !isAdvertising {
        advertiser?.stopAdvertising()
        localEndpointId = ""
      } else {
        guard let name = localEndpointName.data(using: .utf8) else {
          print("Device name is invalid. This shouldn't happen!")
          return
        }
        advertiser.startAdvertising(using: name)
        localEndpointId = advertiser.getLocalEndpointId()
        print("Starting advertising local endpoint id: " + localEndpointId)
      }
    }
  }

  var isDiscovering = false {
    didSet {
      if !isDiscovering {
        discoverer.stopDiscovery()
        endpoints.removeAll(where: {$0.state == .discovered})
        return
      } else {
        discoverer.startDiscovery()
        localEndpointId = advertiser.getLocalEndpointId()
      }
    }
  }

  private(set) var endpoints: [Endpoint] = []

  private var connectionManager: ConnectionManager!
  private var advertiser: Advertiser!
  private var discoverer: Discoverer!
  
  init() {
    connectionManager = ConnectionManager(
      serviceID: Config.serviceId, strategy: Config.defaultStategy, delegate: self)
    advertiser = Advertiser(connectionManager: connectionManager, delegate: self)
    discoverer = Discoverer(connectionManager: connectionManager, delegate: self)
  }
  
  static func createDebugModel() -> Main {
    let model = Main();
    model.localEndpointName = Config.defaultEndpointName

    model.endpoints.append(Endpoint(
      id: "R2D2",
      name: "Nice droid",
      // medium: Endpoint.Medium.bluetooth,
      isIncoming: false, state: .discovered
    ))
    
    model.endpoints.append(Endpoint(
      id: "C3P0",
      name: "Fussy droid",
      // medium: Endpoint.Medium.bluetooth,
      isIncoming: false, state: .discovered,
      outgoingFiles: [
        OutgoingFile(mimeType: "image/jpeg", fileSize: 4000000, state: .loading),
        OutgoingFile(mimeType: "image/jpeg", fileSize: 5000000, state: .uploading),
        OutgoingFile(mimeType: "image/jpeg", fileSize: 5000000, state: .uploaded, remotePath: "1234567890ABCDEF"),
        OutgoingFile(mimeType: "image/jpeg", fileSize: 4000000, state: .loaded),
        OutgoingFile(mimeType: "image/jpeg", fileSize: 4000000, state: .picked)
      ],
      incomingFiles: [
        IncomingFile(mimeType:"image/png", fileName: "IMG_0001.png",
                     remotePath: "E66C4645-E8C3-4842-AE1D-C0CE47DBA1FC.png",
                     fileSize: 4000000, state: .downloading),
        IncomingFile(mimeType:"image/png", fileName: "IMG_0002.png",
                     remotePath: "E66C4645-E8C3-4842-AE1D-C0CE47DBA1FC.png",
                     fileSize: 5000000, state: .received),
        IncomingFile(mimeType:"image/png", fileName: "IMG_0003.png",
                     remotePath: "E66C4645-E8C3-4842-AE1D-C0CE47DBA1FC.png",
                     fileSize: 5000000, state: .downloaded)
      ],
      transfers: [
        Transfer(
          direction: Transfer.Direction.upload,
          remotePath: "1234567890ABCDEF",
          result: .success
        ),
        Transfer(
          direction: Transfer.Direction.download,
          remotePath: "1234567890ABCDEF",
          result: .failure
        ),
        Transfer(
          direction: Transfer.Direction.send,
          remotePath: "1234567890ABCDEF",
          result: .success
        ),
        Transfer(
          direction: Transfer.Direction.receive,
          remotePath: "1234567890ABCDEF",
          result: .cancaled
        )
      ]
    ))
    
    return model;
  }
  
  public func endpoint(id: String) -> Endpoint? {
    return endpoints.first(where: {$0.id == id})
  }
  
  func requestConnection(to endpointId: String, _ completionHandler: ((Error?) -> Void)?) {
    discoverer?.requestConnection(
      to: endpointId,
      using: localEndpointName.data(using: .utf8)!,
      completionHandler: completionHandler)
  }
  
  func disconnect(from endpointId: String, _ completionHandler: ((Error?) -> Void)?) {
    connectionManager.disconnect(from: endpointId, completionHandler: completionHandler)
  }
  
  func sendFiles(_ payload: Data, to endpointId: String) {
    print("Sending files to " + endpointId)
    let endpoint = endpoints.first(where: {$0.id == endpointId})
    endpoint?.state = .sending
    _ = connectionManager.send(
      payload,
      to: [endpointId],
      id: PayloadID.unique()) { [endpoint] result in
        print("Done sending files.")
        endpoint?.state = .connected
      }
  }
}

extension Main: DiscovererDelegate {
  func discoverer(_ discoverer: Discoverer, didFind endpointId: String, /*medium: Endpoint.Medium,*/ with context: Data) {
    print("OnEndpointFound: " + endpointId)
    guard let endpointName = String(data: context, encoding: .utf8) else {
      print("Failed to parse endpointInfo.")
      return
    }

    let endpoint = endpoints.first(where: {$0.id == endpointId})
    if endpoint == nil {
      let endpoint = Endpoint(
        id: endpointId,
        name: endpointName,
        // medium: "",
        isIncoming: false, state: .discovered
      )
      endpoints.append(endpoint)
    } else {
      endpoint?.isIncoming = false
    }
  }

  func discoverer(_ discoverer: Discoverer, didLose endpointId: String) {
    print("OnEndpointLost: " + endpointId)
    endpoints.removeAll(where: {$0.id == endpointId})
  }
}

extension Main: AdvertiserDelegate {
  func advertiser(_ advertiser: Advertiser, didReceiveConnectionRequestFrom endpointId: String, with endpoindInfo: Data, connectionRequestHandler: @escaping (Bool) -> Void) {
    print("OnConnectionInitiated from " + endpointId);

    guard let endpointName = String(data: endpoindInfo, encoding: .utf8) else {
      print("Failed to parse endpointInfo.")
      return
    }

    if endpoints.first(where: {$0.id == endpointId}) == nil {
      endpoints.append(Endpoint(
        id: endpointId,
        name: endpointName,
        isIncoming: true,
        state: .pending
      ))
    }

    connectionRequestHandler(true)
  }
}

extension Main: ConnectionManagerDelegate {
  func connectionManager(_ connectionManager: ConnectionManager, didReceive verificationCode: String, from endpointId: String, verificationHandler: @escaping (Bool) -> Void) {
    print("OnConnectionVerification token received: " + verificationCode + ". Accepting connection request.")
    verificationHandler(true)
    guard let endpoint = endpoints.first(where: {$0.id == endpointId}) else {
      print("End point not found. It has probably stopped advertising or canceled the connection request.")
      return
    }
    endpoint.state = .connected
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceive data: Data, withID payloadID: PayloadID, from endpointId: String) {
    guard let endpoint = endpoints.first(where: {$0.id == endpointId}) else {
      print("Endpoint not found. " + endpointId)
      return
    }
    if (endpoint.state != .sending) {
      endpoint.state = .receiving

      guard let files = IncomingFile.decodeIncomingFiles(fromJson: data) else {
        print("Failed to decode incoming files.")
        return
      }
      
      for file in files {
        endpoint.incomingFiles.append(file)
        let transfer = Transfer(direction: .receive, remotePath: file.remotePath, result: .success)
        endpoint.transfers.append(transfer)
      }
    }
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceive stream: InputStream, withID payloadID: PayloadID, from endpointId: String, cancellationToken token: CancellationToken) {
    print("OnPayloadReceived. Payload type: stream.")
  }

  func connectionManager(_ connectionManager: ConnectionManager, didStartReceivingResourceWithID payloadID: PayloadID, from endpointId: String, at localURL: URL, withName name: String, cancellationToken token: CancellationToken) {
    print("OnPayloadReceived. Payload type: file.")
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceiveTransferUpdate update: TransferUpdate, from endpointId: String, forPayload payloadID: PayloadID) {
    print("OnPayloadProgress: " + endpointId)

    guard let endpoint = endpoints.first(where: {$0.id == endpointId}) else {
      print("Endpoint not found. " + endpointId)
      return
    }

    switch update {
    case .progress(let progress):
      print(String(format: "Transfer progress: %d/%d transferred.",
                   progress.completedUnitCount,
                   progress.totalUnitCount))
      return
    case .success:
      print("Transferred succeeded.")
      endpoint.state = .connected
    case .canceled, .failure:
      print("Transfer failed or canceled.")
      endpoint.state = .connected
    }
  }

  func connectionManager(_ connectionManager: ConnectionManager, didChangeTo state: ConnectionState, for endpointId: String) {
    func removeOrChangeState (_ endpoint: Endpoint) -> Void {
      // If the endpoint wasn't discovered by us in the first place, remove it.
      // Otherwise, keep it and change its state to Discovered.
      if endpoint.isIncoming || !isDiscovering {
        endpoints.removeAll(where: {$0.id == endpointId})
      } else {
        endpoint.state = .discovered
      }
    }

    print("OnEndpointStateChange: " + endpointId)
    guard let endpoint = endpoints.first(where: {$0.id == endpointId}) else {
      print("Endpoint not found.")
      return
    }
    switch (state) {
    case .connecting:
      endpoint.state = .pending
      break
    case .connected:
      endpoint.state = .connected
    case .disconnected:
      removeOrChangeState(endpoint)
    case .rejected:
      removeOrChangeState(endpoint)
    }
  }
}
