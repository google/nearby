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

  func loadRecordAndSend(for endpoint: Endpoint) async -> Error? {
    // Load photos and save them to local files
    guard let packet = await loadPhotos(for: endpoint) else {
      return NSError(domain: "Loading", code: 1)
    }

    // Record the packet in the database
    if let error = await withCheckedContinuation({ continuation in
      CloudDatabase.shared.recordNewPacket(packet: packet) { succeeded in
        continuation.resume(returning: succeeded)
      }
    }) {
      return error
    }

    // Encode the packet into json
    let data = DataWrapper(packet: packet)
    let json = try? JSONEncoder().encode(data)
    guard let json else {
      return NSError(domain: "Encoding", code: 1)
    }

    // print("Encoded packet: " + String(data: json, encoding: .utf8) ?? "Invalid")
    // let roundTrip = try? JSONDecoder().decode(DataWrapper<IncomingFile>.self, from: json)

    // Send the json data to the endpoint
    if let error = await model.sendData(json, to: endpoint.id) {
      return error
    }

    return nil
  }

  func loadPhotos(for endpoint: Endpoint) async -> Packet<OutgoingFile>? {
    endpoint.loadingPhotos = true

    let packet = Packet<OutgoingFile>()
    packet.packetId = UUID().uuidString.uppercased()
    packet.notificationToken = endpoint.notificationToken
    packet.state = .loading
    packet.receiver = endpoint.name

    guard let directoryUrl = try? FileManager.default.url(
      for: .documentDirectory,
      in: .userDomainMask,
      appropriateFor: nil,
      create: true) else {
      print("Failed to obtain directory for saving photos.")
      return nil
    }

    await withTaskGroup(of: OutgoingFile?.self) { group in
      for photo in photosPicked {
        group.addTask {
          return await loadPhoto(photo: photo, directoryUrl: directoryUrl)
        }
      }

      for await (file) in group {
        guard let file else {
          continue;
        }
        packet.files.append(file)
      }
    }

    endpoint.loadingPhotos = false
    // Very rudimentary error handling. Succeeds only if all files are saved. No partial success.
    if (packet.files.count == photosPicked.count)
    {
      packet.state = .loaded
      model.outgoingPackets.append(packet)
      return packet
    } else {
      packet.state = .picked
      return nil
    }
  }

  func loadPhoto(photo: PhotosPickerItem, directoryUrl: URL) async -> OutgoingFile? {
    let type =
    photo.supportedContentTypes.first(
      where: {$0.preferredMIMEType == "image/jpeg"}) ??
    photo.supportedContentTypes.first(
      where: {$0.preferredMIMEType == "image/png"})

    let file = OutgoingFile(mimeType: type?.preferredMIMEType ?? "application/octet-stream")

    guard let data = try? await photo.loadTransferable(type: Data.self) else {
      return nil
    }
    var typedData: Data
    if file.mimeType == "image/jpeg" {
      guard let uiImage = UIImage(data: data) else {
        return nil
      }
      guard let jpegData = uiImage.jpegData(compressionQuality: 1) else {
        return nil
      }
      typedData  = jpegData
      file.fileSize = Int64(jpegData.count)
    }
    else if file.mimeType == "image/png" {
      guard let uiImage = UIImage(data: data) else {
        return nil
      }
      guard let pngData = uiImage.pngData() else {
        return nil
      }
      typedData  = pngData
      file.fileSize = Int64(pngData.count)
    }
    else {
      typedData = data
      file.fileSize = Int64(data.count)
    }

    let url = directoryUrl.appendingPathComponent(UUID().uuidString)
    do {
      try typedData.write(to:url)
    } catch {
      print("Failed to save photo to file")
      return nil;
    }
    file.localUrl = url
    file.state = .loaded
    return file
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

                  PhotosPicker(selection: $photosPicked, matching: .images) {
                    ZStack {
                      Image(systemName: "photo.badge.plus.fill")
                        .opacity(endpoint.loadingPhotos ? 0 : 1)
                      ProgressView()
                        .opacity(endpoint.loadingPhotos ? 1 : 0)
                    }
                  }
                  .disabled(endpoint.loadingPhotos)
                  .buttonStyle(.bordered).fixedSize()
                  .frame(maxHeight: .infinity)
                  .alert("Do you want to send the claim token to the remote endpoint? You will need to go to the uploads page and upload this packet. Once it's uploaded, the other device will get a notification and will be able to download.", isPresented: $showConfirmation) {
                    Button("Yes") {
                      Task { await loadRecordAndSend(for: endpoint) }
                    }
                    Button("No", role: .cancel) { }
                  }
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
