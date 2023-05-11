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

import SwiftUI

struct EndpointDetail: View {
    var endpointID: String

    @EnvironmentObject private var model: Model

    var body: some View {
        let endpoint = model.endpoints.first { $0.endpointID == endpointID }
        let connectionRequest = model.requests.first { $0.endpointID == endpointID }
        let connection = model.connections.first { $0.endpointID == endpointID }
        let name = connectionRequest?.endpointName ?? connection?.endpointName ?? endpoint?.endpointName

        Form {
            if let endpoint {
                DiscoveredEndpointView(endpoint: endpoint, onRequestConnection: {
                    model.requestConnection(to: endpointID)
                }).id(endpoint.id)
            }

            if let connectionRequest {
                ConnectionRequestView(connectionRequest: connectionRequest, onAcceptConnection: {
                    connectionRequest.shouldAccept(true)
                }, onRejectConnection: {
                    connectionRequest.shouldAccept(false)
                }).id(connectionRequest.id)
            }

            if let connection {
                ConnectedView(connection: connection, onSendBytes: {
                    model.sendBytes(to: [endpointID])
                }, onDisconnect: {
                    model.disconnect(from: endpointID)
                }).id(connection.id)
            }
        }
        .navigationTitle(name ?? "")
    }
}
