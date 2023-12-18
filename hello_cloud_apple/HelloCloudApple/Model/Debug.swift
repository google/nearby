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
    result.notificationToken = "abcd"
    result.receiver = "Hans Solo"
    result.files.append(OutgoingFile.createDebugModel(mimeType: "image/jpeg", fileSize: 1024*1024*4))
    result.files.append(OutgoingFile.createDebugModel(mimeType: "image/png", fileSize: 1024*1024*8))
    return result
  }
}

extension Packet<IncomingFile> {
  static func createIncomingDebugModel() -> Packet<IncomingFile>{
    let result = Packet<IncomingFile>()
    result.notificationToken = "abcd"
    result.sender = "Princess Leia"
    result.files.append(IncomingFile.createDebugModel(mimeType: "image/jpeg", fileSize: 1024*1024*4))
    result.files.append(IncomingFile.createDebugModel(mimeType: "image/png", fileSize: 1024*1024*8))
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
    result.outgoingPackets.append(Packet<OutgoingFile>.createOutgoingDebugModel())
    result.incomingPackets.append(Packet<IncomingFile>.createIncomingDebugModel())
    result.endpoints[0].incomingPackets.append(Packet<IncomingFile>.createIncomingDebugModel())
    return result;
  }
}
