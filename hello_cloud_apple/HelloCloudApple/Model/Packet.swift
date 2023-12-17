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

@Observable class Packet<T: File>: Encodable, Decodable {
  enum State: Int {
    case unknown, picked, loading, loaded, uploading, uploaded, received, downloading, downloaded
  }

  var notificationToken: String? = nil
  var files: [T] = []
  
  var state: State = .unknown
  init() {}
  
  required init(from decoder: Decoder) throws {
    let container = try decoder.container(keyedBy: CodingKeys.self)
    notificationToken = try container.decode(String?.self, forKey: .notificationToken)
    files = try container.decode([T].self, forKey: .files)
  }

  func encode(to encoder: Encoder) throws {
    var container = encoder.container(keyedBy: CodingKeys.self) 
    try container.encode(notificationToken, forKey: .notificationToken)
    try container.encode(files, forKey: .files)
  }

  enum CodingKeys: String, CodingKey {
    case notificationToken, files
  }

  static func createDebugModel() {
    let result = Packet<OutgoingFile>()
    result.notificationToken = "abcd"
    result.files.append(OutgoingFile.createDebugModel(mimeType: "image/jpeg", remotePath: "ABCD"))
  }
}
