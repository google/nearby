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
    storage.useEmulator(withHost: "192.168.1.214", port: 9199)
    storageRef = storage.reference()
  }

  func upload(from localUri: URL, to remotePath: String, 
              completion: ((_: Int, _: Error?) -> Void)? = nil) {
    let fileRef = storageRef.child(remotePath)
    let _ = fileRef.putFile(from: localUri) { metadata, error in
      if error == nil {
        print("I: Uploaded file " + remotePath)
      } else {
        print("E: Failed uploading file \(remotePath). Error: " 
              + (error?.localizedDescription ?? ""))
      }
      completion?((Int) (metadata?.size ?? 0), error)
    }
  }

  func download(_ remotePath: String, as fileName: String,
                completion: ((_: URL?, _: Error?) -> Void)? = nil) {
    let fileRef = storageRef.child(remotePath)

    guard let directoryUrl = try? FileManager.default.url(
      for: .documentDirectory,
      in: .userDomainMask,
      appropriateFor: nil,
      create: true) else {
      let error = NSError(domain: "E: Failed to obtain directory for downloading.", code: 1)
      completion?(nil, error)
      return
    }

    let fileUrl = directoryUrl.appendingPathComponent(fileName)
    let _ = fileRef.write(toFile: fileUrl) { [remotePath]
      url, error in
      if error == nil {
        print("I: Downloaded file \(remotePath).")
      } else {
        print("E: Failed to download file \(remotePath). Error: "
              + (error?.localizedDescription ?? ""))
      }
      completion?(url, error)
    }
  }
}
