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
    notificationToken: String?) async -> Packet<OutgoingFile>? {
    let packet = Packet<OutgoingFile>(id: UUID())
    packet.notificationToken = notificationToken
    packet.state = .loading
    packet.receiver = receiver
    packet.sender = Main.shared.localEndpointName

    guard let directoryUrl = try? FileManager.default.url(
      for: .documentDirectory,
      in: .userDomainMask,
      appropriateFor: nil,
      create: true) else {
      print("Failed to obtain directory for saving photos.")
      return nil
    }

    await withTaskGroup(of: OutgoingFile?.self) { group in
      for photo in photosPicked {
        group.addTask {
          return await Utils.loadPhoto(photo: photo, directoryUrl: directoryUrl)
        }
      }

      for await (file) in group {
        guard let file else {
          continue;
        }
        packet.files.append(file)
      }
    }

    // Very rudimentary error handling. Succeeds only if all files are saved. No partial success.
    if (packet.files.count == photosPicked.count)
    {
      packet.state = .loaded
      return packet
    } else {
      packet.state = .picked
      return nil
    }
  }

  static func loadPhoto(photo: PhotosPickerItem, directoryUrl: URL) async -> OutgoingFile? {
    let type =
    photo.supportedContentTypes.first(
      where: {$0.preferredMIMEType == "image/jpeg"}) ??
    photo.supportedContentTypes.first(
      where: {$0.preferredMIMEType == "image/png"})

    let file = OutgoingFile(id: UUID(), mimeType: type?.preferredMIMEType ?? "application/octet-stream")

    guard let data = try? await photo.loadTransferable(type: Data.self) else {
      return nil
    }
    var typedData: Data
    if file.mimeType == "image/jpeg" {
      guard let uiImage = UIImage(data: data) else {
        return nil
      }
      guard let jpegData = uiImage.jpegData(compressionQuality: 1) else {
        return nil
      }
      typedData  = jpegData
      file.fileSize = Int64(jpegData.count)
    }
    else if file.mimeType == "image/png" {
      guard let uiImage = UIImage(data: data) else {
        return nil
      }
      guard let pngData = uiImage.pngData() else {
        return nil
      }
      typedData  = pngData
      file.fileSize = Int64(pngData.count)
    }
    else {
      typedData = data
      file.fileSize = Int64(data.count)
    }

    let url = directoryUrl.appendingPathComponent(UUID().uuidString)
    do {
      try typedData.write(to:url)
    } catch {
      print("Failed to save photo to file")
      return nil;
    }
    file.localUrl = url
    file.state = .loaded
    return file
  }
}
