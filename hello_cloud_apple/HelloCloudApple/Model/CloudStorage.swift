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

  func upload(_ data: Data, as remotePath: String) {

    let fileRef = storageRef.child(remotePath)
    let uploadTask = fileRef.putData(data, metadata: nil) { (metadata, error) in
      guard let metadata = metadata else {
        print("Failed to upload file. " + (error?.localizedDescription ?? ""))
        return
      }
      print("Succeed uploading file " + remotePath)
      let size = metadata.size
      fileRef.downloadURL { (url, error) in
        guard let downloadURL = url else {
          print("Failed to retrieve the url.")
          return
        }
        print("Url of the upload file: " + (url?.absoluteString ?? ""))
      }
    }
  }
}
