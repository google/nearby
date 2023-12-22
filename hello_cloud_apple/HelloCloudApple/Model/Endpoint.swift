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
import SwiftUI
import PhotosUI

@Observable class Endpoint: Identifiable, Hashable {
  enum State: Int, CustomStringConvertible {
    case discovered, connecting, connected, disconnecting, sending, receiving

      var description: String {
        return switch self {
        case .discovered: "Discovered"
        case .connecting: "Connecting"
        case .disconnecting: "Disconnecting"
        case .connected: "Connected"
        case .sending: "Sending"
        case .receiving: "Receiving"
        }
      }
  }

  let id: String
  let name: String
  @ObservationIgnored var isIncoming: Bool
  var loadingPhotos: Bool = false
  var showingConfirmation: Bool = false
  var photosPicked: [PhotosPickerItem] = [] {
    didSet {
      DispatchQueue.main.async {
        self.showingConfirmation = true
      }
    }
  }
  var state: State
  @ObservationIgnored var notificationToken: String?

  static func == (lhs: Endpoint, rhs: Endpoint) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher){ hasher.combine(id) }

  init(id: String, name: String, isIncoming: Bool = false, state: State = State.discovered) {
    self.id = id
    self.name = name
    self.isIncoming = isIncoming
    self.state = state
  }

  func connect() -> Void {
    state = .connecting
    Main.shared.requestConnection(to: id) { [weak self] error in
      if error != nil {
        self?.state = .discovered
        print("E: Failed to connect: " + (error?.localizedDescription ?? ""))
      }
    }
  }

  func disconnect() -> Void {
    state = .disconnecting
    Main.shared.disconnect(from: id) { [weak self] error in
      self?.state = .discovered
      if error != nil {
        print("I: Failed to disconnected: " + (error?.localizedDescription ?? ""))
      }
    }
  }

  func onNotificationTakenReceived(token: String) -> Void {
    print("I: notification token received: \(token)")
    notificationToken = token
  }

  func onPacketReceived(packet: Packet<IncomingFile>) -> Void {
    packet.sender = self.name
    packet.state = .received

    Main.shared.incomingPackets.append(packet)

    CloudDatabase.shared.observePacket(id: packet.packetId) { [weak packet] newPacket in
      guard let packet else {
        return
      }
      packet.update(from: newPacket)
    }
  }

  /**
   Load files, create packet in memory, push it to the database, and send the packet
   to the remote endpoint.
   */
  func loadAndSend() async -> Error? {
    // Load photos and save them to local files
    guard let packet = await loadPhotos() else {
      return NSError(domain: "Loading", code: 1)
    }

    // Encode the packet into json
    let data = DataWrapper(packet: packet)
    let json = try? JSONEncoder().encode(data)
    guard let json else {
      return NSError(domain: "Encoding", code: 1)
    }

    // Send the json data to the remote endpoint
    if let error = await Main.shared.sendData(json, to: id) {
      return error
    }

    if packet.notificationToken != nil {
      print("I: Sending packet token to endpoint with notification token \(packet.notificationToken!).")
    } else {
      print("W: Sending packet token without notification token.")
    }

    // Add the packet to outbox
    Main.shared.outgoingPackets.append(packet)
    return nil
  }

  func loadPhotos() async -> Packet<OutgoingFile>? {
    loadingPhotos = true

    let packet = Packet<OutgoingFile>()
    packet.packetId = UUID().uuidString.uppercased()
    packet.notificationToken = notificationToken
    packet.state = .loading
    packet.receiver = name
    packet.sender = Main.shared.localEndpointName

    guard let directoryUrl = try? FileManager.default.url(
      for: .documentDirectory,
      in: .userDomainMask,
      appropriateFor: nil,
      create: true) else {
      print("Failed to obtain directory for saving photos.")
      return nil
    }

    await withTaskGroup(of: OutgoingFile?.self) { [self] group in
      for photo in photosPicked {
        group.addTask {
          return await self.loadPhoto(photo: photo, directoryUrl: directoryUrl)
        }
      }

      for await (file) in group {
        guard let file else {
          continue;
        }
        packet.files.append(file)
      }
    }

    loadingPhotos = false
    // Very rudimentary error handling. Succeeds only if all files are saved. No partial success.
    if (packet.files.count == photosPicked.count)
    {
      packet.state = .loaded

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
}
