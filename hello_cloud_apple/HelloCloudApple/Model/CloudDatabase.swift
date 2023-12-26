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
import FirebaseCore
import FirebaseDatabase

// https://stackoverflow.com/questions/45209743/how-can-i-use-swift-s-codable-to-encode-into-a-dictionary
class DictionaryEncoder {
    private let encoder = JSONEncoder()

    var dateEncodingStrategy: JSONEncoder.DateEncodingStrategy {
        set { encoder.dateEncodingStrategy = newValue }
        get { return encoder.dateEncodingStrategy }
    }

    var dataEncodingStrategy: JSONEncoder.DataEncodingStrategy {
        set { encoder.dataEncodingStrategy = newValue }
        get { return encoder.dataEncodingStrategy }
    }

    var nonConformingFloatEncodingStrategy: JSONEncoder.NonConformingFloatEncodingStrategy {
        set { encoder.nonConformingFloatEncodingStrategy = newValue }
        get { return encoder.nonConformingFloatEncodingStrategy }
    }

    var keyEncodingStrategy: JSONEncoder.KeyEncodingStrategy {
        set { encoder.keyEncodingStrategy = newValue }
        get { return encoder.keyEncodingStrategy }
    }

    func encode<T>(_ value: T) throws -> [String: Any] where T : Encodable {
        let data = try encoder.encode(value)
        return try JSONSerialization.jsonObject(with: data, options: .allowFragments) as! [String: Any]
    }
}

class DictionaryDecoder {
    private let decoder = JSONDecoder()

    var dateDecodingStrategy: JSONDecoder.DateDecodingStrategy {
        set { decoder.dateDecodingStrategy = newValue }
        get { return decoder.dateDecodingStrategy }
    }

    var dataDecodingStrategy: JSONDecoder.DataDecodingStrategy {
        set { decoder.dataDecodingStrategy = newValue }
        get { return decoder.dataDecodingStrategy }
    }

    var nonConformingFloatDecodingStrategy: JSONDecoder.NonConformingFloatDecodingStrategy {
        set { decoder.nonConformingFloatDecodingStrategy = newValue }
        get { return decoder.nonConformingFloatDecodingStrategy }
    }

    var keyDecodingStrategy: JSONDecoder.KeyDecodingStrategy {
        set { decoder.keyDecodingStrategy = newValue }
        get { return decoder.keyDecodingStrategy }
    }

    func decode<T>(_ type: T.Type, from dictionary: [String: Any]) throws -> T where T : Decodable {
        let data = try JSONSerialization.data(withJSONObject: dictionary, options: [])
        return try decoder.decode(type, from: data)
    }
}

class CloudDatabase {
  static let shared = CloudDatabase()

  let database: Database
  let databaseRef: DatabaseReference

  init() {
    database = Database.database()
    if Config.localCloud {
      database.useEmulator(withHost: Config.localCloudHost, port: 9000)
    }
    databaseRef = database.reference()
  }

  /** 
   Push the packet to the database, overwriting existing one if it exists.
   */
  func push(packet: Packet<OutgoingFile>) async -> DatabaseReference? {
    guard let packetData = try? DictionaryEncoder().encode(packet) else {
      print("E: Failed to encode packet into a dictionary")
      return nil
    }
    return try? await databaseRef.child("packets/\(packet.id)").updateChildValues(packetData)
  }

  static func readPacket(from snapshot: DataSnapshot) -> Packet<IncomingFile>? {
    guard let value = snapshot.value as? [String: Any] else {
      print("E: failed to read from the snapshot.")
      return nil
    }
    guard let packet = try? DictionaryDecoder().decode(Packet<IncomingFile>.self, from: value) else {
      print("E: failed to decode the snapshot data.")
      return nil
    }
    return packet;
  }

  func observePacket(id: UUID, _ notification: @escaping (Packet<IncomingFile>) -> Void) {
    databaseRef.child("packets/\(id)").observe(.value) { snapshot in
      guard let packet = CloudDatabase.readPacket(from: snapshot) else {
        return
      }
      notification(packet)
    }
  }

  func pull(packetId: UUID) async -> Packet<IncomingFile>? {
    guard let snapshot = try? await databaseRef.child("packets/\(packetId)").getData() else {
      print("E: Failed to read snapshot from database")
      return nil
    }
    guard let packet = CloudDatabase.readPacket(from: snapshot) else {
      return nil
    }
    return packet
  }
}
