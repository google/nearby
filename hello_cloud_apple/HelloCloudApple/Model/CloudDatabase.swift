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

class CloudDatabase {
  static let shared = CloudDatabase()

  let database: Database
  let databaseRef: DatabaseReference

  init() {
    database = Database.database(url:"http://127.0.0.1:9000?ns=hello-cloud-5b73c")
//    database = Database.database()
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
    return try? await databaseRef.child("packets/\(packet.packetId)").updateChildValues(packetData)
  }

  func observePacketState(packetId: String, _ notification: @escaping (DataSnapshot) -> Void) {
    databaseRef.root.child("packets").child(packetId).child("state")
      .observe(.value, with: notification)
  }

  /**
   Retrieve the latest state of the packet from the database; Update the files' remote path if the
   packet's state is .uploaded. Do not update other fields.
   */
  func pull(packet: Packet<IncomingFile>) async {
    guard let snapshot = try? await databaseRef.child("packets/\(packet.packetId)").getData() else {
      print("E: Failed to read packet state from Firebase")
      return
    }
    guard let state = snapshot.childSnapshot(forPath: "state").value as? String else {
      return
    }

    if state == "uploaded" {
      for file in packet.files {
        guard let remotePath = snapshot.childSnapshot(
          forPath: "files/\(file.fileId)/remotePath").value as? String else {
          print("E: Failed to read remote path")
          return
        }
        file.remotePath = remotePath
      }
      packet.state = .uploaded
    }
  }
}
