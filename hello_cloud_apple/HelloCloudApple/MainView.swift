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
import NearbyConnections

struct MainView: View {
  @EnvironmentObject private var model: Main

  var body: some View {
    NavigationSplitView {
      Form {
        HStack{
          Label("Local Endpoint ID:", systemImage: "network")
          Text(model.localEndpointId)
        }
        TextField("Endpoint Name", text: $model.localEndpointName)
        Toggle("Advertising", isOn: $model.isAdvertisingEnabled)
        Toggle("Discovery", isOn: $model.isDiscoveryEnabled)
        Section(header: Text("Endpoints")) {
          ForEach(model.endpoints) { endpoint in
            NavigationLink(value: endpoint) {
              Text(endpoint.id)
              Text("(" + endpoint.name + ")")
            }
          }
        }
      }
      .navigationDestination(for: String.self) { endpointId in
        // TODO: guard here?
        let endpoint = model.endpoints.first(where: { $0.id == endpointId})
        EndpointView(model: endpoint!)
      }
      .navigationTitle("Hello Cloud")
    } detail: {
      Text("No Endpoints Selected")
        .font(.headline)
        .foregroundColor(.secondary)
    }
  }
}

struct MainView_Previews: PreviewProvider {
  static let model = Main()

  static var previews: some View {
    MainView ().environmentObject(model)
  }
}
