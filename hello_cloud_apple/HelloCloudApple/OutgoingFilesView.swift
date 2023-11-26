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

import SwiftUI

struct OutgoingFilesView: View {
  let model: Endpoint

  func upload () -> Void {
    for file in model.outgoingFiles {
      // TODO: async this
      file.upload()

      model.transfers.append(Transfer(direction: .upload, localPath: file.localPath, remotePath: file.remotePath!, result: .success))
    }
  }

  func send () -> Void {
    // TODO: encode and send payload
    for file in model.outgoingFiles {
      model.transfers.append(Transfer(direction: .send, localPath: file.localPath, remotePath: file.remotePath!, result: .success))
    }
  }

  let pick: () -> Void = {}

  var body: some View {
    // TODO: make the buttons float at the button; add a vertical scrollbar
    Form {
      Section(header: Text("Outgoing Files")) {
        HStack{
          Button(action: pick) {
            Label("Pick", systemImage: "doc.fill.badge.plus").frame(maxWidth: .infinity)
          }
          Spacer()
          Button(action: upload) {
            Label("Upload", systemImage: "arrow.up.circle.fill").frame(maxWidth: .infinity)
          }.disabled(model.outgoingFiles.isEmpty || model.outgoingFiles.allSatisfy({$0.isUploaded || $0.isUploading}))
          Spacer()
          Button(action: send) {
            Label("Send", systemImage: "arrow.up.backward.circle.fill").frame(maxWidth: .infinity)
          }.disabled(model.outgoingFiles.isEmpty || !model.outgoingFiles.allSatisfy({$0.isUploaded}))
        }

        ForEach(model.outgoingFiles) {file in
          HStack {
            Label(file.localPath, systemImage: "doc")
            Spacer()
            if (file.isUploaded) {
              Image(systemName: "circle.fill").foregroundColor(.green)
            } else if (file.isUploading) {
              ProgressView()
            } else {
              Image(systemName: "circle.fill").foregroundColor(.gray)
            }
          }
        }
      }
      .headerProminence(.increased)
    }
    .navigationTitle(model.name)
  }
}

struct OutgoingFilesView_Previews: PreviewProvider {
  static var previews: some View {
    OutgoingFilesView(
      model: Endpoint(
        id: "R2D2",
        name: "Debug droid",
        isIncoming: false, state: .discovered,
        outgoingFiles: [
          OutgoingFile(localPath: "IMG_0001.jpg", fileSize: 4000000, isUploading: false),
          OutgoingFile(localPath: "IMG_0002.jpg", fileSize: 5000000, isUploading: true),
          OutgoingFile(localPath: "IMG_0003.jpg", fileSize: 5000000, remotePath: "1234567890ABCDEF", isUploading: false)
        ]
      )
    )
  }
}

