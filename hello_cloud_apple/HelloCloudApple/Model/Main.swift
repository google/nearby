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

class Main: ObservableObject {
  let localEndpointId: String
  @Published var localEndpointName: String = ""
  @Published var isAdvertisingEnabled = Config.defaultAdvertisingState {
    didSet {
      invalidateAdvertising()
    }
  }
  @Published var isDiscoveryEnabled = Config.defaultDiscoveryState {
    didSet {
      invalidateDiscovery()
    }
  }
  @Published private(set) var endpoints: [Endpoint] = []

  private var connectionManager: ConnectionManager!
  private var advertiser: Advertiser?
  private var discoverer: Discoverer?
  private var isAdvertising = Config.defaultAdvertisingState
  private var isDiscovering = Config.defaultDiscoveryState

  init() {
    localEndpointId = "ABCD"
    invalidateAdvertising()
    invalidateDiscovery()
    endpoints.append(Endpoint(
      id: "R2D2",
      name: "Debug droid",
      // medium: Endpoint.Medium.bluetooth,
      state: Endpoint.State.discovered,
      isIncoming: false
    ))
  }

  private func invalidateAdvertising() {
    defer {
      isAdvertising = isAdvertisingEnabled
    }
    if isAdvertising {
      advertiser?.stopAdvertising()
    }
    if !isAdvertisingEnabled {
      return
    }
    connectionManager = ConnectionManager(serviceID: Config.serviceId, strategy: Config.defaultStategy)
    connectionManager.delegate = self

    // TODO: String.data does not include the null character at the end. Add one
    advertiser = Advertiser(connectionManager: connectionManager)
    advertiser?.delegate = self
    advertiser?.startAdvertising(using: localEndpointName.data(using: .utf8)!)
  }


  private func invalidateDiscovery() {
    defer {
      isDiscovering = isDiscoveryEnabled
    }
    if isDiscovering {
      discoverer?.stopDiscovery()
    }
    if !isDiscoveryEnabled {
      return
    }
    connectionManager = ConnectionManager(serviceID: Config.serviceId, strategy: Config.defaultStategy)
    connectionManager.delegate = self

    discoverer = Discoverer(connectionManager: connectionManager)
    discoverer?.delegate = self
    discoverer?.startDiscovery()
  }

  func requestConnection(to endpointID: EndpointID) {
    discoverer?.requestConnection(to: endpointID, using: localEndpointName.data(using: .utf8)!)
  }

  func disconnect(from endpointID: EndpointID) {
    connectionManager.disconnect(from: endpointID)
  }

  func sendFiles(to endpointId: EndpointID) {

  }

  //    func sendBytes(to endpointIDs: [EndpointID]) {
  //        let payloadID = PayloadID.unique()
  //        let token = connectionManager.send(Config.bytePayload.data(using: .utf8)!, to: endpointIDs, id: payloadID)
  //        let payload = Payload(
  //            id: payloadID,
  //            type: .bytes,
  //            status: .inProgress(Progress()),
  //            isIncoming: false,
  //            cancellationToken: token
  //        )
  //        for endpointID in endpointIDs {
  //            guard let index = connections.firstIndex(where: { $0.endpointID == endpointID }) else {
  //                return
  //            }
  //            connections[index].payloads.insert(payload, at: 0)
  //        }
  //    }
}

extension Main: DiscovererDelegate {
  func discoverer(_ discoverer: Discoverer, didFind endpointID: EndpointID, /*medium: Endpoint.Medium,*/ with context: Data) {
    guard let endpointName = String(data: context, encoding: .utf8) else {
      return
    }
    let endpoint = Endpoint(
      id: endpointID,
      name: endpointName,
      // medium: "",
      state: Endpoint.State.discovered,
      isIncoming: false
    )
    endpoints.append(endpoint)
  }

  func discoverer(_ discoverer: Discoverer, didLose endpointID: EndpointID) {
    endpoints.removeAll(where: {$0.id == endpointID})
  }
}

extension Main: AdvertiserDelegate {
  func advertiser(_ advertiser: Advertiser, didReceiveConnectionRequestFrom endpointID: EndpointID, with context: Data, connectionRequestHandler: @escaping (Bool) -> Void) {
    guard let endpointName = String(data: context, encoding: .utf8) else {
      return
    }
    let endpoint = Endpoint(
      id: endpointID,
      name: endpointName,
      state: Endpoint.State.pending,
      isIncoming: true
    )
    endpoints.append(endpoint)
    connectionRequestHandler(true)
  }
}

extension Main: ConnectionManagerDelegate {
//  func connectionManager(_ connectionManager: NearbyConnections.ConnectionManager, didChangeTo state: NearbyConnections.ConnectionState, for endpointID: NearbyConnections.EndpointID) {
//  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceive verificationCode: String, from endpointID: EndpointID, verificationHandler: @escaping (Bool) -> Void) {
    //        guard let index = endpoints.firstIndex(where: { $0.endpointID == endpointID }) else {
    //            return
    //        }
    //        let endpoint = endpoints.remove(at: index)
    //        let request = ConnectionRequest(
    //            id: endpoint.id,
    //            endpointID: endpointID,
    //            endpointName: endpoint.endpointName,
    //            pin: verificationCode,
    //            shouldAccept: { accept in
    //                verificationHandler(accept)
    //            }
    //        )
    //        requests.insert(request, at: 0)
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceive data: Data, withID payloadID: PayloadID, from endpointID: EndpointID) {
    //        let payload = Payload(
    //            id: payloadID,
    //            type: .bytes,
    //            status: .success,
    //            isIncoming: true,
    //            cancellationToken: nil
    //        )
    //        guard let index = endpoints.firstIndex(where: { $0.endpointID == endpointID }) else {
    //            return
    //        }
    //        endpoints[index].payloads.insert(payload, at: 0)
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceive stream: InputStream, withID payloadID: PayloadID, from endpointID: EndpointID, cancellationToken token: CancellationToken) {
    //        let payload = Payload(
    //            id: payloadID,
    //            type: .stream,
    //            status: .success,
    //            isIncoming: true,
    //            cancellationToken: token
    //        )
    //        guard let index = endpoints.firstIndex(where: { $0.endpointID == endpointID }) else {
    //            return
    //        }
    //        endpoints[index].payloads.insert(payload, at: 0)
  }

  func connectionManager(_ connectionManager: ConnectionManager, didStartReceivingResourceWithID payloadID: PayloadID, from endpointID: EndpointID, at localURL: URL, withName name: String, cancellationToken token: CancellationToken) {
    //        let payload = Payload(
    //            id: payloadID,
    //            type: .file,
    //            status: .inProgress(Progress()),
    //            isIncoming: true,
    //            cancellationToken: token
    //        )
    //        guard let index = endpoints.firstIndex(where: { $0.endpointID == endpointID }) else {
    //            return
    //        }
    //        endpoints[index].payloads.insert(payload, at: 0)
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceiveTransferUpdate update: TransferUpdate, from endpointID: EndpointID, forPayload payloadID: PayloadID) {
    //        guard let connectionIndex = connections.firstIndex(where: { $0.endpointID == endpointID }),
    //              let payloadIndex = connections[connectionIndex].payloads.firstIndex(where: { $0.id == payloadID }) else {
    //            return
    //        }
    //        switch update {
    //        case .success:
    //            endpoints[connectionIndex].payloads[payloadIndex].status = .success
    //        case .canceled:
    //            endpoints[connectionIndex].payloads[payloadIndex].status = .canceled
    //        case .failure:
    //            endpoints[connectionIndex].payloads[payloadIndex].status = .failure
    //        case let .progress(progress):
    //            endpoints[connectionIndex].payloads[payloadIndex].status = .inProgress(progress)
  }

  func connectionManager(_ connectionManager: ConnectionManager, didChangeTo state: ConnectionState, for endpointID: EndpointID) {
    //        switch (state) {
    //        case .connecting:
    //            break
    //        case .connected:
    //            guard let index = requests.firstIndex(where: { $0.endpointID == endpointID }) else {
    //                return
    //            }
    //            let request = requests.remove(at: index)
    //            let connection = ConnectedEndpoint(
    //                id: request.id,
    //                endpointID: endpointID,
    //                endpointName: request.endpointName
    //            )
    //            endpoints.insert(connection, at: 0)
    //        case .disconnected:
    //            guard let index = connections.firstIndex(where: { $0.endpointID == endpointID }) else {
    //                return
    //            }
    //            endpoints.remove(at: index)
    //        case .rejected:
    //            guard let index = requests.firstIndex(where: { $0.endpointID == endpointID }) else {
    //                return
    //            }
    //            endpoints.remove(at: index)
    //        }
  }
}
