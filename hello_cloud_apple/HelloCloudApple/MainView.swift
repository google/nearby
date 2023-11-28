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
  @EnvironmentObject var model: Main

  func toggleIsAdvertising() -> Void {
    model.isAdvertising.toggle();
  }

  var body: some View {
    NavigationStack {
      Form {
        Section{
          Grid (horizontalSpacing: 20, verticalSpacing: 10) {
            GridRow {
              Label("ID:", systemImage: "1.square.fill").gridColumnAlignment(.leading)
              Text(model.localEndpointId).font(.custom("Menlo", fixedSize: 15)).gridColumnAlignment(.leading)
            }
            GridRow {
              Label("Name:", systemImage: "a.square.fill").gridColumnAlignment(.leading)
              TextField("Local Endpoint Name", text: $model.localEndpointName)
            }
          }
        } header: {
          Text("Local endpoint")
        }

        Section {
          Button(action: {model.isAdvertising.toggle()}) {
            model.isAdvertising
            ? Label("Stop advertising", systemImage: "stop.circle")
            : Label("Start advertising", systemImage: "play.circle")
          }.foregroundColor(model.isAdvertising ? .red : .green)
          Button(action: {model.isDiscovering.toggle()}) {
            model.isDiscovering
            ? Label("Stop discovering", systemImage: "stop.circle")
            : Label("Start discovering", systemImage: "play.circle")
          }.foregroundColor(model.isDiscovering ? .red : .green)
        }

        List {
          Section {
            ForEach(model.endpoints) { endpoint in
              HStack{
                NavigationLink {EndpointView(model: endpoint)} label: {
                  Text(endpoint.id).font(.custom("Menlo", fixedSize: 15))
                  Text("(" + endpoint.name + ")")
                }
              }
            }
          } header: {
            Text("Remote endpoints")
          }
        }
      }.navigationTitle("Hello Cloud")
    }
  }
}

struct MainView_Previews: PreviewProvider {
  static let model = Main.createDebugModel()

  static var previews: some View {
    MainView().environment(model)
  }
}
