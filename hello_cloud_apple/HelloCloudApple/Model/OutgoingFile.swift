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

@Observable class OutgoingFile: File, CustomStringConvertible, Hashable, Encodable, Decodable {
  enum State: Int {
    case loaded, uploading, uploaded
  }

  let id: UUID
  let mimeType: String

  @ObservationIgnored var fileSize: Int64 = 0
  @ObservationIgnored var remotePath: String? = nil
  @ObservationIgnored var photoItem: PhotosPickerItem? = nil
  var state: State = .loaded

  var description: String {
    String(format: "\(mimeType), %.1f KB", (Double(fileSize)/1024.0))
  }

  init(id: UUID, mimeType: String) {
    self.id = id
    self.mimeType = mimeType
  }

  /** Upload to the cloud and return bytes uploaded if successful or nil otherwise */
  func upload() async -> Int64? {
    guard let photoItem else {
      print("File not loaded. Skipping uploading.")
      return nil;
    }
    
    if state != .loaded {
      print("File not loaded. Skipping uploading.")
      return nil
    }

    remotePath = UUID().uuidString
    if mimeType == "image/jpeg" {
      remotePath! += ".jpeg"
    } else if mimeType == "image/png" {
      remotePath! += ".png"
    }

    state = .uploading
    let size = await CloudStorage.shared.upload(from: self, to: remotePath!)
    state = size != nil ? .uploaded : .loaded
    return size
  }

  static func == (lhs: OutgoingFile, rhs: OutgoingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher) { hasher.combine(self.id) }

  enum CodingKeys: String, CodingKey {
    case id, mimeType, fileSize, remotePath
  }
}
