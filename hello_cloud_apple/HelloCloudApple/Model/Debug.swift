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

extension OutgoingFile {
  static func createDebugModel(
    mimeType: String, fileSize: Int64, state: State = .picked) -> OutgoingFile {
      let result = OutgoingFile(mimeType: mimeType)
      result.fileSize = fileSize
      result.state = state
      return result
    }
}

extension IncomingFile {
  static func createDebugModel(
    mimeType: String, fileSize: Int64, state: State = .received) -> IncomingFile {
      let result = IncomingFile(mimeType: mimeType)
      result.fileSize = fileSize
      result.state = state
      return result
    }
}


extension Packet<OutgoingFile> {
  static func createOutgoingDebugModel() -> Packet<OutgoingFile>{
    let result = Packet<OutgoingFile>()
    result.notificationToken = "dUcjcnLNZ0hxuqWScq2UDh:APA91bGG8GTykBZgAkGA_xkBVnefjUb-PvR4mDNjwjv1Sv7EYGZc89zyfoy6Syz63cQ3OkQUH3D5Drf0674CZOumgBsgX8sR4JGQANWeFNjC_RScHWDyA8ZhYdzHdp7t6uQjqEhF_TEL"
    result.receiver = "Luke Skywalker"

    let file1 = OutgoingFile.createDebugModel(mimeType: "image/jpeg", fileSize: 1024*1024*4)
    let file2 = OutgoingFile.createDebugModel(mimeType: "image/png", fileSize: 1024*1024*8)

    result.files.append(file1)
    result.files.append(file2)
    return result
  }
}

extension Packet<IncomingFile> {
  static func createIncomingDebugModel() -> Packet<IncomingFile>{
    let result = Packet<IncomingFile>()
    result.packetId = "7C55BEA7-5DD8-4591-AAA5-17C02448BE36"
    result.sender = "Princess Leia"
    result.state = .received
    
    let file1 = IncomingFile.createDebugModel(mimeType: "image/jpeg", fileSize: 1024*1024*4)
    file1.fileId = "084849E6-5519-4460-A66F-68E1DCB20093"
    let file2 = IncomingFile.createDebugModel(mimeType: "image/png", fileSize: 1024*1024*8)
    file2.fileId = "092EB77A-69C6-4ED3-98CA-1C00D5EB8E6A"

    result.files.append(file1)
    result.files.append(file2)
    return result
  }
}

extension Main {
  static func createDebugModel() -> Main {
    let result = Main();
    result.localEndpointName = Config.defaultEndpointName

    result.endpoints.append(Endpoint(
      id: "R2D2",
      name: "Artoo",
      isIncoming: false,
      state: .discovered
    ))
//    result.outgoingPackets.append(Packet<OutgoingFile>.createOutgoingDebugModel())
//    result.incomingPackets.append(Packet<IncomingFile>.createIncomingDebugModel())
    return result;
  }
}
