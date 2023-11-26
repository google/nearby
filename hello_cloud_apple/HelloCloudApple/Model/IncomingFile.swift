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

@Observable class IncomingFile: Identifiable, Hashable {
  let id: UUID = UUID()
  
  let localPath: String
  let remotePath: String
  let fileSize: Int64

  var isDownloading: Bool
  var isDownloaded: Bool

  init(localPath: String, remotePath: String, fileSize: Int64, isDownloading: Bool, isDownloaded: Bool) {
    self.localPath = localPath
    self.remotePath = remotePath
    self.fileSize = fileSize
    self.isDownloading = isDownloading
    self.isDownloaded = isDownloaded
  }

  func download () -> Void {
    // TODO: async this and actually download
    isDownloading = false;
    isDownloaded = true;
  }
  
  static func == (lhs: IncomingFile, rhs: IncomingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher){ hasher.combine(self.id) }
}
