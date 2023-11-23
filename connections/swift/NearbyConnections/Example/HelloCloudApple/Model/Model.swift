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

class Model: ObservableObject {
    @Published var endpointName = Config.defaultEndpointName {
        didSet {
            invalidateAdvertising()
        }
    }
    @Published var strategy = Config.defaultStategy {
        didSet {
            invalidateAdvertising()
            invalidateDiscovery()
        }
    }
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

    @Published private(set) var requests: [ConnectionRequest] = []
    @Published private(set) var connections: [ConnectedEndpoint] = []
    @Published private(set) var endpoints: [DiscoveredEndpoint] = []

    var connectionManager: ConnectionManager!
    var advertiser: Advertiser?
    var discoverer: Discoverer?

    init() {
        invalidateAdvertising()
        invalidateDiscovery()
    }

    private var isAdvertising = Config.defaultAdvertisingState
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
        connectionManager = ConnectionManager(serviceID: Config.serviceId, strategy: strategy)
        connectionManager.delegate = self

        advertiser = Advertiser(connectionManager: connectionManager)
        advertiser?.delegate = self
        advertiser?.startAdvertising(using: endpointName.data(using: .utf8)!)
    }

    private var isDiscovering = Config.defaultDiscoveryState
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
        connectionManager = ConnectionManager(serviceID: Config.serviceId, strategy: strategy)
        connectionManager.delegate = self

        discoverer = Discoverer(connectionManager: connectionManager)
        discoverer?.delegate = self
        discoverer?.startDiscovery()
    }

    func requestConnection(to endpointID: EndpointID) {
        discoverer?.requestConnection(to: endpointID, using: endpointName.data(using: .utf8)!)
    }

    func disconnect(from endpointID: EndpointID) {
        connectionManager.disconnect(from: endpointID)
    }

    func sendBytes(to endpointIDs: [EndpointID]) {
        let payloadID = PayloadID.unique()
        let token = connectionManager.send(Config.bytePayload.data(using: .utf8)!, to: endpointIDs, id: payloadID)
        let payload = Payload(
            id: payloadID,
            type: .bytes,
            status: .inProgress(Progress()),
            isIncoming: false,
            cancellationToken: token
        )
        for endpointID in endpointIDs {
            guard let index = connections.firstIndex(where: { $0.endpointID == endpointID }) else {
                return
            }
            connections[index].payloads.insert(payload, at: 0)
        }
    }
}

extension Model: DiscovererDelegate {
    func discoverer(_ discoverer: Discoverer, didFind endpointID: EndpointID, with context: Data) {
        guard let endpointName = String(data: context, encoding: .utf8) else {
            return
        }
        let endpoint = DiscoveredEndpoint(
            id: UUID(),
            endpointID: endpointID,
            endpointName: endpointName
        )
        endpoints.insert(endpoint, at: 0)
    }

    func discoverer(_ discoverer: Discoverer, didLose endpointID: EndpointID) {
        guard let index = endpoints.firstIndex(where: { $0.endpointID == endpointID }) else {
            return
        }
        endpoints.remove(at: index)
    }
}

extension Model: AdvertiserDelegate {
    func advertiser(_ advertiser: Advertiser, didReceiveConnectionRequestFrom endpointID: EndpointID, with context: Data, connectionRequestHandler: @escaping (Bool) -> Void) {
        guard let endpointName = String(data: context, encoding: .utf8) else {
            return
        }
        let endpoint = DiscoveredEndpoint(
            id: UUID(),
            endpointID: endpointID,
            endpointName: endpointName
        )
        endpoints.insert(endpoint, at: 0)
        connectionRequestHandler(true)
    }
}

extension Model: ConnectionManagerDelegate {
    func connectionManager(_ connectionManager: ConnectionManager, didReceive verificationCode: String, from endpointID: EndpointID, verificationHandler: @escaping (Bool) -> Void) {
        guard let index = endpoints.firstIndex(where: { $0.endpointID == endpointID }) else {
            return
        }
        let endpoint = endpoints.remove(at: index)
        let request = ConnectionRequest(
            id: endpoint.id,
            endpointID: endpointID,
            endpointName: endpoint.endpointName,
            pin: verificationCode,
            shouldAccept: { accept in
                verificationHandler(accept)
            }
        )
        requests.insert(request, at: 0)
    }

    func connectionManager(_ connectionManager: ConnectionManager, didReceive data: Data, withID payloadID: PayloadID, from endpointID: EndpointID) {
        let payload = Payload(
            id: payloadID,
            type: .bytes,
            status: .success,
            isIncoming: true,
            cancellationToken: nil
        )
        guard let index = connections.firstIndex(where: { $0.endpointID == endpointID }) else {
            return
        }
        connections[index].payloads.insert(payload, at: 0)
    }

    func connectionManager(_ connectionManager: ConnectionManager, didReceive stream: InputStream, withID payloadID: PayloadID, from endpointID: EndpointID, cancellationToken token: CancellationToken) {
        let payload = Payload(
            id: payloadID,
            type: .stream,
            status: .success,
            isIncoming: true,
            cancellationToken: token
        )
        guard let index = connections.firstIndex(where: { $0.endpointID == endpointID }) else {
            return
        }
        connections[index].payloads.insert(payload, at: 0)
    }

    func connectionManager(_ connectionManager: ConnectionManager, didStartReceivingResourceWithID payloadID: PayloadID, from endpointID: EndpointID, at localURL: URL, withName name: String, cancellationToken token: CancellationToken) {
        let payload = Payload(
            id: payloadID,
            type: .file,
            status: .inProgress(Progress()),
            isIncoming: true,
            cancellationToken: token
        )
        guard let index = connections.firstIndex(where: { $0.endpointID == endpointID }) else {
            return
        }
        connections[index].payloads.insert(payload, at: 0)
    }

    func connectionManager(_ connectionManager: ConnectionManager, didReceiveTransferUpdate update: TransferUpdate, from endpointID: EndpointID, forPayload payloadID: PayloadID) {
        guard let connectionIndex = connections.firstIndex(where: { $0.endpointID == endpointID }),
              let payloadIndex = connections[connectionIndex].payloads.firstIndex(where: { $0.id == payloadID }) else {
            return
        }
        switch update {
        case .success:
            connections[connectionIndex].payloads[payloadIndex].status = .success
        case .canceled:
            connections[connectionIndex].payloads[payloadIndex].status = .canceled
        case .failure:
            connections[connectionIndex].payloads[payloadIndex].status = .failure
        case let .progress(progress):
            connections[connectionIndex].payloads[payloadIndex].status = .inProgress(progress)
        }
    }

    func connectionManager(_ connectionManager: ConnectionManager, didChangeTo state: ConnectionState, for endpointID: EndpointID) {
        switch (state) {
        case .connecting:
            break
        case .connected:
            guard let index = requests.firstIndex(where: { $0.endpointID == endpointID }) else {
                return
            }
            let request = requests.remove(at: index)
            let connection = ConnectedEndpoint(
                id: request.id,
                endpointID: endpointID,
                endpointName: request.endpointName
            )
            connections.insert(connection, at: 0)
        case .disconnected:
            guard let index = connections.firstIndex(where: { $0.endpointID == endpointID }) else {
                return
            }
            connections.remove(at: index)
        case .rejected:
            guard let index = requests.firstIndex(where: { $0.endpointID == endpointID }) else {
                return
            }
            requests.remove(at: index)
        }
    }
}
