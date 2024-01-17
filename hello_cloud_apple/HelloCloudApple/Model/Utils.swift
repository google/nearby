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
import SwiftUI
import PhotosUI
import UIKit

class Utils {
  static func loadPhotos(
    photos photosPicked: [PhotosPickerItem],
    receiver: String?,
    notificationToken: String?) -> Packet<OutgoingFile>? {
      let packet = Packet<OutgoingFile>(id: UUID())
      packet.notificationToken = notificationToken
      packet.state = .loaded
      packet.receiver = receiver
      packet.sender = Main.shared.localEndpointName

      for photo in photosPicked {
        let type =
        photo.supportedContentTypes.first(
          where: {$0.preferredMIMEType == "image/jpeg"}) ??
        photo.supportedContentTypes.first(
          where: {$0.preferredMIMEType == "image/png"})

        let file = OutgoingFile(id: UUID(), mimeType: type?.preferredMIMEType ?? "application/octet-stream")
        file.photoItem = photo
        file.state = .loaded
        packet.files.append(file)
      }
      return packet
    }
}
