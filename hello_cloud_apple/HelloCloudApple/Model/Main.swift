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
import UIKit
import SwiftUI
import PhotosUI

@Observable class Main: ObservableObject {
  static var shared: Main! = nil

  private(set) var localEndpointId: String = ""
  var localEndpointName: String = ""
  
  var qrCodeData: Data? = nil

  var showingInbox = false
  var showingOutbox = false
  var loadingPhotos = false
  var showingQrCode = false {
    didSet {
      // Once the QR code page is dismissed, clear the photo selection for the next pick.
      photosPicked = []
    }
  }
  var showingQrScanner = false

  var isAdvertising = false {
    didSet {
      if !isAdvertising {
        advertiser?.stopAdvertising()
        localEndpointId = ""
      } else {
        guard let name = localEndpointName.data(using: .utf8) else {
          print("E: Device name is invalid. This shouldn't happen.")
          return
        }
        advertiser.startAdvertising(using: name)
        localEndpointId = advertiser.getLocalEndpointId()
        print("I: Starting advertising local endpoint id: " + localEndpointId)
      }
    }
  }

  var isDiscovering = false {
    didSet {
      if !isDiscovering {
        discoverer.stopDiscovery()
        endpoints.removeAll(where: {$0.state == .discovered})
        return
      } else {
        discoverer.startDiscovery()
        localEndpointId = advertiser.getLocalEndpointId()
      }
    }
  }

  var endpoints: [Endpoint] = []
  var outgoingPackets: [Packet<OutgoingFile>] = []
  var incomingPackets: [Packet<IncomingFile>] = []

  var photosPicked: [PhotosPickerItem] = [] {
    didSet {
      if photosPicked.count > 0 {
        Task {await self.loadAndGenerateQr()}
      }
    }
  }

  private var connectionManager: ConnectionManager!
  private var advertiser: Advertiser!
  private var discoverer: Discoverer!
  
  init() {
    connectionManager = ConnectionManager(
      serviceID: Config.serviceId, strategy: Strategy.cluster, delegate: self)
    advertiser = Advertiser(connectionManager: connectionManager, delegate: self)
    discoverer = Discoverer(connectionManager: connectionManager, delegate: self)

    localEndpointName = Config.defaultEndpointName
    Main.shared = self
  }

  func flashPacket(packet: Packet<IncomingFile>?, after delay: Double = 0.0) {
    DispatchQueue.main.asyncAfter(deadline: .now() + delay) {
      packet?.highlighted = true
    }
    DispatchQueue.main.asyncAfter(deadline: .now() + delay + 1.5) {
      packet?.highlighted = false
    }
  }


  public func showInboxAndHilight(packet id: UUID) {
    let packet = incomingPackets.first(where: {$0.id == id})
    DispatchQueue.main.async {
      self.showingInbox = true
    }
    flashPacket(packet: packet, after: 0.5)
  }

  func observePacket(_ packet: Packet<IncomingFile>, fromQr: Bool) {
    CloudDatabase.shared.observePacket(id: packet.id) { [weak packet, weak self, fromQr] newPacket in
      guard let packet, let self else {
        return
      }
      packet.update(from: newPacket)
      flashPacket(packet: packet)

      if fromQr {
        // If it's from QR code, we won't get a push notification. So we pop up a local
        // notification.
        self.showLocalNotification(for: packet)
      }

      Task { await packet.download() }
    }
  }

  func showLocalNotification(for packet: Packet<IncomingFile>) {
    let content = UNMutableNotificationContent()
    content.title = "You've got files!"
    content.subtitle = "Your files from \(packet.sender!) will start downloading"
    content.sound = UNNotificationSound.default
    content.userInfo["packetId"] = packet.id.uuidString

    let request = UNNotificationRequest(identifier: UUID().uuidString, content: content, trigger: nil)
    UNUserNotificationCenter.current().add(request)
  }

  public func loadAndGenerateQr() async -> Error? {
    loadingPhotos = true
    defer {loadingPhotos = false}

    // We have no way to get the receiver's name and notification token.
    guard let packet = await Utils.loadPhotos(
      photos: photosPicked, receiver: nil, notificationToken: nil) else {
      return NSError(domain: "Loading", code: 1)
    }

    // Encode the packet
    // TODO: make it more compact by using a more dense encoder than json
    // or at least remove unused fields
    qrCodeData = try? JSONEncoder().encode(packet)
    if qrCodeData == nil {
      print("E: Failed to encode packet into QR code")
      return nil
    }

    // Display the QR code
    showingQrCode = true
    // Add the packet to outbox
    Main.shared.outgoingPackets.append(packet)
    // Upload the packet
    Task { await packet.upload() }
    return nil
  }

  public func onQrCodeReceived(string: String) -> Packet<IncomingFile>? {
    guard let packet = try? JSONDecoder().decode(
      Packet<IncomingFile>.self, from: string.data(using: .utf8)!) else {
      print("E: failed to decode packet from QR code")
      return nil
    }
    if packet.sender == nil {
      return nil
    }
    packet.state = .received
    incomingPackets.append(packet)
    observePacket(packet, fromQr: true)
    return packet
  }

  public func endpoint(id: String) -> Endpoint? {
    return endpoints.first(where: {$0.id == id})
  }

  func requestConnection(to endpointId: String, _ completionHandler: ((Error?) -> Void)?) {
    discoverer?.requestConnection(
      to: endpointId,
      using: localEndpointName.data(using: .utf8)!,
      completionHandler: completionHandler)
  }
  
  func disconnect(from endpointId: String, _ completionHandler: ((Error?) -> Void)?) {
    connectionManager.disconnect(from: endpointId, completionHandler: completionHandler)
  }
  
  func sendData(_ payload: Data, to endpointId: String) async -> Error? {
    print("I: Sending data to \(endpointId).")
    let endpoint = endpoints.first(where: {$0.id == endpointId})
    endpoint?.state = .sending

    return await withCheckedContinuation { continuation in
      _ = connectionManager.send(
        payload,
        to: [endpointId],
        id: PayloadID.unique()) { [endpoint] error in
          if error == nil {
            print("I: Data sent to \(endpointId).")
          } else {
            print("E: Failed to send data to \(endpointId).")
          }
          endpoint?.state = .connected
          continuation.resume(returning: error)
        }
    }
  }

  func onPacketUploaded(id: UUID) {
    print("I: Packet \(id) is uploaded and ready for downloading.")
    guard let packet = incomingPackets.first(where: { $0.id == id }) else {
      print("W: Packet \(id) not in our inbox, skipping.")
      return
    }

    Task {
      print("I: Pulling file remote paths from the database.")
      guard let newPacket = await CloudDatabase.shared.pull(packetId: packet.id) else {
        return
      }
      packet.update(from: newPacket)
    }
  }
}

extension Main: DiscovererDelegate {
  func discoverer(_ discoverer: Discoverer, didFind endpointId: String, /*medium: Endpoint.Medium,*/ with context: Data) {
    print("I: Endpoint found: \(endpointId).")
    guard let endpointName = String(data: context, encoding: .utf8) else {
      print("E: Failed to parse endpointInfo.")
      return
    }

    let endpoint = endpoints.first(where: {$0.id == endpointId})
    if endpoint == nil {
      let endpoint = Endpoint(
        id: endpointId,
        name: endpointName,
        // medium: "",
        isIncoming: false, state: .discovered
      )
      endpoints.append(endpoint)
    } else {
      endpoint?.isIncoming = false
    }
  }

  func discoverer(_ discoverer: Discoverer, didLose endpointId: String) {
    print("I: Endpoint lost: \(endpointId)")
    endpoints.removeAll(where: {$0.id == endpointId})
  }
}

extension Main: AdvertiserDelegate {
  func advertiser(_ advertiser: Advertiser, didReceiveConnectionRequestFrom endpointId: String, with endpoindInfo: Data, connectionRequestHandler: @escaping (Bool) -> Void) {
    print("I: Connection initiated from \(endpointId).")

    guard let endpointName = String(data: endpoindInfo, encoding: .utf8) else {
      print("E: Failed to parse endpointInfo.")
      return
    }

    if endpoints.first(where: {$0.id == endpointId}) == nil {
      endpoints.append(Endpoint(
        id: endpointId,
        name: endpointName,
        isIncoming: true,
        state: .connecting
      ))
    }

    connectionRequestHandler(true)
  }
}

extension Main: ConnectionManagerDelegate {
  func connectionManager(_ connectionManager: ConnectionManager, didReceive verificationCode: String, from endpointId: String, verificationHandler: @escaping (Bool) -> Void) {
    print("I: Connection verification token received: \(verificationCode). Accepting connection request.")
    if nil == endpoints.first(where: {$0.id == endpointId}) {
      print("E: Endpoint \(endpointId) not found. It has probably stopped advertising or canceled the connection request.")
      return
    }
    verificationHandler(true)
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceive data: Data, withID payloadID: PayloadID, from endpointId: String) {
    guard let endpoint = endpoints.first(where: {$0.id == endpointId}) else {
      print("E: Endpoint \(endpointId) not found.")
      return
    }
    if (endpoint.state != .sending) {
      endpoint.state = .receiving

      let string = String(data: data, encoding: .utf8)
      print(string!)

      guard let data = try? JSONDecoder().decode(DataWrapper<IncomingFile>.self, from: data) else {
        print ("E: Unable to decode incoming payload.")
        return
      }

      switch data.data {
      case .notificationToken(let notificationToken):
        endpoint.onNotificationTakenReceived(token: notificationToken)
      case .packet(let packet):
        endpoint.onPacketReceived(packet: packet)
      }
    }
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceive stream: InputStream, withID payloadID: PayloadID, from endpointId: String, cancellationToken token: CancellationToken) {
    print("OnPayloadReceived. Payload type: stream.")
  }

  func connectionManager(_ connectionManager: ConnectionManager, didStartReceivingResourceWithID payloadID: PayloadID, from endpointId: String, at localURL: URL, withName name: String, cancellationToken token: CancellationToken) {
    print("OnPayloadReceived. Payload type: file.")
  }

  func connectionManager(_ connectionManager: ConnectionManager, didReceiveTransferUpdate update: TransferUpdate, from endpointId: String, forPayload payloadID: PayloadID) {
    print("OnPayloadProgress: " + endpointId)

    guard let endpoint = endpoints.first(where: {$0.id == endpointId}) else {
      print("E: Endpoint \(endpointId) not found.")
      return
    }

    switch update {
    case .progress(let progress):
      print(String(format: "E: Transfer progress: %d/%d transferred.",
                   progress.completedUnitCount,
                   progress.totalUnitCount))
      return
    case .success:
      print("Transferred succeeded.")
      endpoint.state = .connected
    case .canceled, .failure:
      print("Transfer failed or canceled.")
      endpoint.state = .connected
    }
  }

  func connectionManager(_ connectionManager: ConnectionManager, didChangeTo state: ConnectionState, for endpointId: String) {
    func removeOrChangeState (_ endpoint: Endpoint) -> Void {
      // If the endpoint wasn't discovered by us in the first place, remove it.
      // Otherwise, keep it and change its state to Discovered.
      if endpoint.isIncoming || !isDiscovering {
        endpoints.removeAll(where: {$0.id == endpointId})
      } else {
        endpoint.state = .discovered
      }
    }

    print("I: Endpoint state change: " + endpointId)
    guard let endpoint = endpoints.first(where: {$0.id == endpointId}) else {
      print("I: Endpoint \(endpointId) not found.")
      return
    }
    switch (state) {
    case .connecting:
      endpoint.state = .connecting
      break
    case .connected:
      endpoint.state = .connected
      if let token = AppDelegate.shared.notificationToken {
        let data = DataWrapper<OutgoingFile>(notificationToken: token)
        if let json = try? JSONEncoder().encode(data) {
          Task { await sendData(json, to: endpointId) }
        }
      }
    case .disconnected:
      removeOrChangeState(endpoint)
    case .rejected:
      removeOrChangeState(endpoint)
    }
  }
}
