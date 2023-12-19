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
import PhotosUI

struct EndpointView: View {
  let model: Endpoint

  @EnvironmentObject var mainModel: Main
  @Environment(\.dismiss) var dismiss
  @State private var showConfirmation: Bool = false
  @State private var photosPicked: [PhotosPickerItem] = []
  @State private var loadPhotos = false
  @State var imageUrl: URL? = nil

  func connect() -> Void {
    model.state = .connecting
    mainModel.requestConnection(to: model.id) {
      error in
      print("RequestConnection completed: " + (error?.localizedDescription ?? ""))
    }
  }

  func disconnect() -> Void {
    model.state = .disconnecting
    mainModel.disconnect(from: model.id) {
      error in
      print("Disconnect completed: " + (error?.localizedDescription ?? ""))
    }
  }

  func loadPhotosAsync(_ completionHandler: ((Packet<OutgoingFile>?) -> Void)?) async -> Void {
    loadPhotos = true

    let packet = Packet<OutgoingFile>()
    packet.packetId = UUID().uuidString.uppercased()
    packet.notificationToken = model.notificationToken
    packet.state = .loading
    packet.receiver = model.name

    guard let directoryUrl = try? FileManager.default.url(
      for: .documentDirectory,
      in: .userDomainMask,
      appropriateFor: nil,
      create: true) else {
      print("Failed to obtain directory for saving photos.")
      return
    }

    await withTaskGroup(of: OutgoingFile?.self) { group in
      for photo in photosPicked {
        group.addTask{
          return await loadPhotoAsync(photo: photo, directoryUrl: directoryUrl)
        }
      }

      for await (file) in group {
        guard let file else {
          continue;
        }
        packet.files.append(file)
      }
    }

    loadPhotos = false
    // Very rudimentary error handling. Succeeds only if all files are saved. No partial success.
    if (packet.files.count == photosPicked.count)
    {
      packet.state = .loaded
      mainModel.outgoingPackets.append(packet)
      completionHandler?(packet)
    } else {
      packet.state = .picked
      completionHandler?(nil)
    }
  }

  func loadPhotoAsync(photo: PhotosPickerItem, directoryUrl: URL) async -> OutgoingFile? {
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

  func sendPacket(_ packet: Packet<OutgoingFile>) -> Error? {
    let payload = try? JSONEncoder().encode(packet)
    guard let payload else {
      return NSError(domain: "Encoding", code: 1)
    }
    Main.shared.sendData(payload, to: model.id)
    return nil
  }

  @State var expanded: [Bool] = []

  var body: some View {
    NavigationStack {
      Form {
        Section {
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
                case .connected:
                  Image(systemName: "circle.fill").foregroundColor(.green)
                case .discovered:
                  Image(systemName: "circle.fill").foregroundColor(.gray)
                default:
                  Image(systemName: "circle.dotted").foregroundColor(.gray)
                }
                Text(String(describing: model.state)).gridColumnAlignment(.leading)
              }
            }
          }
        }

        HStack {
          Button(action: connect) {
            HStack {
              Label("Connect", systemImage: "phone.connection.fill")
                .foregroundColor(model.state == .discovered ? .green : .gray)
            }
          }
          .disabled(model.state != .discovered)
          .buttonStyle(.plain).fixedSize()
          Spacer()
          if model.state == .connecting {
            ProgressView()
          }
        }.frame(maxWidth: .infinity)

        HStack {
          Button(action: disconnect) {
            Label("Disconnect", systemImage: "phone.down.fill")
              .foregroundColor(model.state == .connected ? .red : .gray)
          }
          .disabled(model.state != .connected)
          .buttonStyle(.plain).fixedSize()

          Spacer()
          if model.state == .disconnecting {
            ProgressView()
          }
        }.frame(maxWidth: .infinity)

        PhotosPicker(selection: $photosPicked, matching: .images) {
          HStack {
            Label("Pick Photos", systemImage: "photo.badge.plus.fill")
            Spacer()
            if loadPhotos {
              ProgressView()
            }
          }
        }
        .frame(maxWidth: .infinity)
        .disabled(loadPhotos)
        .onChange(of: photosPicked, { DispatchQueue.main.async { showConfirmation = true }})

        NavigationLink{ TransfersView(model: model) } label: {
          Label("Transfer Log", systemImage: "list.bullet.clipboard.fill")
        }

        Section(header: Text("Incoming packets")) {
          List {
            ForEach(Array(model.incomingPackets.enumerated()), id: \.1.id) { i, packet in
              DisclosureGroup () {
                ForEach(packet.files) { file in
                  HStack {
                    Image(systemName: "photo")
                    Text(String(describing: file.description))
                    Spacer()
                    switch file.state {
                    case .received:
                      Image(systemName: "circle.dotted").foregroundColor(.gray)
                    case .downloading:
                      ProgressView()
                    case .downloaded:
                      Image(systemName: "circle.fill").foregroundColor(.green)
                    }
                  }
                }
              } label: {
                Button(action: { packet.download() }) {
                  Image(systemName: "icloud.and.arrow.down.fill")
                }.buttonStyle(.borderless)
                Text(String(describing: packet))
                Spacer()
                ProgressView().opacity(packet.state == .downloading ? 1 : 0)
              }
            }
          }
        }
      }
      .alert("Do you want to send the claim token to the remote endpoint? You will need to go to the uploads page and upload this packet. Once it's uploaded, the other device will get a notification and will be able to download.", isPresented: $showConfirmation) {
        Button("Yes") {
          Task {
            await loadPhotosAsync { result in
              guard let result else {
                return
              }
              let data = DataWrapper(packet: result)
              let json = try? JSONEncoder().encode(data)
              guard let json else {
                return
              }

              print("Encoded packet:")
              print(String(data: json, encoding: .utf8) ?? "Invalid")

//              let roundTrip = try? JSONDecoder().decode(DataWrapper<IncomingFile>.self, from: json)

              Main.shared.sendData(json, to: model.id)
              CloudDatabase.shared.createNewPacket(packet: result)
            }
          }
        }
        Button("No", role: .cancel) { }
      }
      .navigationTitle(model.name)
    }
    .onChange(of: mainModel.endpoints) { _, endpoints in
      if !endpoints.contains(model) { dismiss() }
    }
  }
}

#Preview {
  EndpointView(
    model: Main.shared.endpoints[0]
  ).environment(Main.shared)
}
