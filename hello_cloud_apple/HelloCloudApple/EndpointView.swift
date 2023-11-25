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

struct EndpointView: View {
  // @EnvironmentObject
  var model: Endpoint

  let connect: () -> Void = {}
  let disconnect: () -> Void = {}

  var body: some View {
    Form {
      Section(header: Text("Remote Endpoint")) {
        Grid(horizontalSpacing: 20, verticalSpacing: 20) {
          GridRow{
            Text("ID:").gridColumnAlignment(.leading)
            Text(model.id).gridColumnAlignment(.leading)
          }
          GridRow{
            Text("Name:").gridColumnAlignment(.leading)
            Text(model.name).gridColumnAlignment(.leading)
          }
          GridRow{
            Text("State:").gridColumnAlignment(.leading)
            HStack {
              switch model.state {
              case Endpoint.State.pending, Endpoint.State.sending, Endpoint.State.receiving:
                ProgressView()
              case Endpoint.State.connected:
                Image(systemName: "circle.fill").foregroundColor(.green)
              case Endpoint.State.discovered:
                Image(systemName: "circle.fill").foregroundColor(.gray)
              }
              Text(String(describing: model.state)).gridColumnAlignment(.leading)
            }
          }
        }
        Button(action: connect) {
          Label("Connect", systemImage: "phone.connection.fill").foregroundColor(.green)
        }
        Button(action: disconnect) {
          Label("Disconnect", systemImage: "phone.down.fill").foregroundColor(.red)
        }
        NavigationLink { }
          label: {
            Label("Outgoing Files", systemImage: "arrow.up.doc.fill")
          }
        NavigationLink{} label: {
          Label("Incoming Files", systemImage: "arrow.down.doc.fill")
        }
        NavigationLink{ TransfersView(model: model) } label: {
          Label("Transfer Log", systemImage: "list.bullet.clipboard.fill")
        }
      }.headerProminence(.increased)
    }
    .navigationTitle(model.name)
  }

//    var body: some View {
//        let endpoint = model.endpoints.first { $0.endpointID == endpointID }
//        let connectionRequest = model.requests.first { $0.endpointID == endpointID }
//        let connection = model.connections.first { $0.endpointID == endpointID }
//        let name = connectionRequest?.endpointName ?? connection?.endpointName ?? endpoint?.endpointName

//        Form {
//            if let endpoint {
//                DiscoveredEndpointView(endpoint: endpoint, onRequestConnection: {
//                    model.requestConnection(to: endpointID)
//                }).id(endpoint.id)
//            }
//
//            if let connectionRequest {
//                ConnectionRequestView(connectionRequest: connectionRequest, onAcceptConnection: {
//                    connectionRequest.shouldAccept(true)
//                }, onRejectConnection: {
//                    connectionRequest.shouldAccept(false)
//                }).id(connectionRequest.id)
//            }
//
//            if let connection {
//                ConnectedView(connection: connection, onSendBytes: {
//                    model.sendBytes(to: [endpointID])
//                }, onDisconnect: {
//                    model.disconnect(from: endpointID)
//                }).id(connection.id)
//            }
//        }
//        .navigationTitle(name ?? "")
//    }
}

struct EndpointView_Previews: PreviewProvider {
  static var previews: some View {
    EndpointView(
      model: Endpoint(
        id: "R2D2",
        name: "Debug droid",
        state: Endpoint.State.discovered,
        isIncoming: false
      )
    )
  }
}
