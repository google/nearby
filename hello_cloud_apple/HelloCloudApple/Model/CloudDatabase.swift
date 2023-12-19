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

protocol Storable {
  func toDictionary() -> [String : Any]
}

extension Packet<OutgoingFile>: Storable {
  func toDictionary() -> [String : Any] {
    return [
      "packetId": packetId,
      "notificationToken": notificationToken ?? "",
      "files": files.map({ $0.toDictionary() }),
    ]
  }
}

extension OutgoingFile: Storable {
  func toDictionary() -> [String : Any] {
    return [
      "fileId": fileId,
      "mimeType": mimeType,
      "fileSize": fileSize,
    ]
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

  func createNewPacket(packet: Packet<OutgoingFile>) {
    let packetData = packet.toDictionary()
    databaseRef.root.child("packets").child(packet.packetId).updateChildValues(packetData)
  }
}
