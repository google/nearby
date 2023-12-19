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
    databaseRef = database.reference()
  }

  func recordNewPacket(packet: Packet<OutgoingFile>, _ completionHandler: @escaping ((Error?) -> Void)) {
    if let packetData = try? DictionaryEncoder().encode(packet) {
      databaseRef.root.child("packets").child(packet.packetId).updateChildValues(packetData) {
        error, _ in
        completionHandler(error)
      }
    } else {
      completionHandler(NSError(domain: "Database", code: 1))
    }
  }

  func markPacketAsUploaded(packetId: String, _ completionHandler: @escaping ((Error?) -> Void)) {
    databaseRef.root.child("packets").child(packetId).child("status").setValue("uploaded") {
      error, _ in
      completionHandler(error)
    }
  }
}
