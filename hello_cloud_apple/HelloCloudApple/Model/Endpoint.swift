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
  var state: State
  var outgoingPackets: [Packet<OutgoingFile>] = []
  var incomingPackets: [Packet<IncomingFile>] = []
  var transfers: [Transfer] = []
  var notificationToken: String?

  static func == (lhs: Endpoint, rhs: Endpoint) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher){ hasher.combine(id) }

  init(id: String, name: String, isIncoming: Bool = false, state: State = State.discovered) {
    self.id = id
    self.name = name
    self.isIncoming = isIncoming
    self.state = state
  }

  func onNotificationTakenReceived(token: String) -> Void {
    notificationToken = token
  }

  func onPacketReceived(packet: Packet<IncomingFile>) -> Void {
    packet.sender = self.name
    incomingPackets.append(packet)
    Main.shared.incomingPackets.append(packet)
  }
}
