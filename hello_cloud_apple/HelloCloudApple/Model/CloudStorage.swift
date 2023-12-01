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
import FirebaseStorage

class CloudStorage {
  static let shared = CloudStorage()

  let storage: Storage;
  let storageRef: StorageReference;

  init() {
    storage = Storage.storage()
    storageRef = storage.reference()
  }

  func upload(_ data: Data, as remotePath: String, completion: ((_: Int, _: Error?) -> Void)? = nil) {
    let fileRef = storageRef.child(remotePath)
    let _ = fileRef.putData(data, metadata: nil) { metadata, error in
      if error == nil {
        print("Succeeded uploading file " + remotePath)
      } else {
        print("Failed uploading file " + remotePath +
              ". Error: " + (error?.localizedDescription ?? ""))
      }
      completion?((Int) (metadata?.size ?? 0), error)
    }
  }

  func download(_ remotePath: String, as localPath: String, completion: ((_: Int, _: Error?) -> Void)? = nil) {
    let fileRef = storageRef.child(remotePath)

    guard let downloadsDirectory = try? FileManager.default.url(
      for: .downloadsDirectory,
      in: .allDomainsMask,
      appropriateFor: nil,
      create: true) else {
      let error = NSError(domain: "Failed to obtain Downloads folder.", code: 1)
      completion?(0, error)
      return
    }

    let localUrl = downloadsDirectory.appendingPathComponent(localPath)
    let _ = fileRef.write(toFile: localUrl) { [remotePath, localPath]
      url, error in
      var size: Int
      if error == nil {
        print("Succeeded downloading file " + remotePath + " to " + localPath)
        size = (try? url?.resourceValues(forKeys:[.fileSizeKey]).fileSize) ?? 0
      } else {
        print("Failed downloading file " + remotePath + " to " + localPath +
              ". Error: " + (error?.localizedDescription ?? ""))
        size = 0
      }
      completion?(size, error)
    }
  }
}
