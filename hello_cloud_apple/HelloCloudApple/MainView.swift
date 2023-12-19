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
import NearbyConnections
import SwiftUI
import PhotosUI

struct MainView: View {
  @EnvironmentObject var model: Main
  @State private var photosPicked: [PhotosPickerItem] = []
  @State private var loadingPhotos = false
  @State private var showConfirmation: Bool = false

  func connect(to endpoint: Endpoint) -> Void {
    endpoint.state = .connecting
    model.requestConnection(to: endpoint.id) { [weak endpoint] error in
      if error != nil {
        endpoint?.state = .discovered
        print("E: Failed to connect: " + (error?.localizedDescription ?? ""))
      }
    }
  }

  func disconnect(from endpoint: Endpoint) -> Void {
    endpoint.state = .disconnecting
    model.disconnect(from: endpoint.id) { [weak endpoint] error in
      endpoint?.state = .discovered
      if error != nil {
        print("I: Failed to disconnected: " + (error?.localizedDescription ?? ""))
      }
    }
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

        Section {
          NavigationLink {IncomingPacketsView()} label: {Text("Incoming packets")}
          NavigationLink {OutgoingPacketsView()} label: {Text("Outgoing packets")}
        }

        List {
          Section {
            ForEach(model.endpoints) { endpoint in
              HStack {
                HStack {
                  switch endpoint.state {
                  case .connected:
                    Image(systemName: "circle.fill").foregroundColor(.green)
                  case .discovered:
                    Image(systemName: "circle.fill").foregroundColor(.gray)
                  default:
                    Image(systemName: "circle.fill").foregroundColor(.gray)
                  }
                  VStack {
                    Text(endpoint.id).font(.custom("Menlo", fixedSize: 10))
                      .frame(maxWidth: .infinity, alignment: .leading)
                    Text(endpoint.name)
                      .frame(maxWidth: .infinity, alignment: .leading)
                  }
                }

                Spacer()

                HStack {
                  Button(action: { connect(to: endpoint) }) {
                    ZStack {
                      Image(systemName: "phone.connection.fill")
                        .foregroundColor(endpoint.state == .discovered ? .green : .gray)
                        .opacity(endpoint.state == .connecting ? 0 : 1)
                      ProgressView().opacity(endpoint.state == .connecting ? 1 : 0)
                    }
                  }
                  .disabled(endpoint.state != .discovered)
                  .buttonStyle(.bordered).fixedSize()
                  .frame(maxHeight: .infinity)
//                  .border(Color.blue)

                  Button(action: { disconnect(from: endpoint) }) {
                    ZStack {
                      Image(systemName: "phone.down.fill")
                        .foregroundColor(endpoint.state == .connected ? .red : .gray)
                        .opacity(endpoint.state == .disconnecting ? 0 : 1)
                      ProgressView().opacity(endpoint.state == .disconnecting ? 1 : 0)
                    }
                  }
                  .disabled(endpoint.state != .connected)
                  .buttonStyle(.bordered).fixedSize()
                  .frame(maxHeight: .infinity)
//                  .border(Color.blue)

                  PhotosPicker(selection: $photosPicked, matching: .images) {
                    ZStack {
                      Image(systemName: "photo.badge.plus.fill").opacity(loadingPhotos ? 0 : 1)
                      ProgressView().opacity(loadingPhotos ? 1 : 0)
                    }
                  }
                  .disabled(loadingPhotos)
                  .buttonStyle(.bordered).fixedSize()
                  .frame(maxHeight: .infinity)
//                  .border(Color.blue)
                  .onChange(of: photosPicked, { DispatchQueue.main.async { showConfirmation = true }})
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
