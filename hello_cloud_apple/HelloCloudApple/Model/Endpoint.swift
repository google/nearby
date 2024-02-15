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
      if photosPicked.count > 0 {
        DispatchQueue.main.async {
          self.showingConfirmation = true
        }
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
    Main.shared.flashPacket(packet: packet)
    Main.shared.observePacket(packet, fromQr: false)
  }

  /** Load files, create packet in memory, send the packet to the remote endpoint, and upload */
  func loadSendAndUpload() async -> Error? {
    // Load photos and save them to local files
    loadingPhotos = true
    defer {loadingPhotos = false}
    
    guard let packet = await Utils.loadPhotos(
      photos: photosPicked, receiver: name, notificationToken: notificationToken) else {
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

    // Clear the photo selection for the next pick
    photosPicked = []

    // Start uploading the packet
    Task { await packet.upload() }
    return nil
  }
}
