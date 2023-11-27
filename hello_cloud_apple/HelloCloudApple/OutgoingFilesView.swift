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
import PhotosUI
import Atomics

struct OutgoingFilesView: View {
  let model: Endpoint

  @State private var busy: Bool = false
  @State private var photosPicked: [PhotosPickerItem] = []

  func updateFileList() -> Void {
    print("Starting loading photos")
    busy = true
    model.outgoingFiles.removeAll()

    // TODO: is there something in swift similar to Task.waitAll in .NET?
    let counter = ManagedAtomic<Int>(photosPicked.count)

    for photo in photosPicked {
      print (photo.itemIdentifier ?? "no item identifier")
      Task { [counter] in
        // TODO: handle failure
        let data = try? await photo.loadTransferable(type: Data.self)
        // TODO: is this running on the main/UI thread? Or does Swift make sure updating the UI
        // happens on the UI thread?
        model.outgoingFiles.append(OutgoingFile(localPath: "Photo1", fileSize: UInt64(data!.count)))
        counter.wrappingDecrement(ordering: .relaxed)
        if counter.load(ordering: .relaxed) == 0 {
          busy = false
        }
      }
    }
  }

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

  var body: some View {
    // TODO: make the buttons float at the button; add a vertical scrollbar
    Form {
      Section {
        HStack{
          PhotosPicker(selection: $photosPicked, matching: .images) {
            Label("Pick", systemImage: "doc.fill.badge.plus").frame(maxWidth: .infinity)
          }
          .disabled(busy)
          .onChange(of: photosPicked, updateFileList)

          Button(action: upload) {
            Label("Upload", systemImage: "arrow.up.circle.fill").frame(maxWidth: .infinity)
          }.disabled(model.outgoingFiles.isEmpty || model.outgoingFiles.allSatisfy({$0.state == .loaded}))

          Button(action: send) {
            Label("Send", systemImage: "arrow.up.backward.circle.fill").frame(maxWidth: .infinity)
          }.disabled(model.outgoingFiles.isEmpty || !model.outgoingFiles.allSatisfy({$0.state == .uploaded}))
        }.buttonStyle(.bordered).fixedSize()

        if busy {
          ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
        } else {
          List {
            ForEach(model.outgoingFiles) {file in
              HStack {
                Label(file.localPath, systemImage: "doc")
                Spacer()
                switch file.state {
                case .uploaded: Image(systemName: "circle.fill").foregroundColor(.green)
                case .loading, .uploading: ProgressView()
                case .picked: Image(systemName: "circle.dotted").foregroundColor(.gray)
                case .loaded: Image(systemName: "circle.fill").foregroundColor(.gray)
                }

//                if (file.state == .uploaded) {
//                  Image(systemName: "circle.fill").foregroundColor(.green)
//                } else if (file.state == .uploading) {
//                  ProgressView()
//                } else {
//                  Image(systemName: "circle.fill").foregroundColor(.gray)
//                }
              }
            }
          }
        }
      }
    }
    .navigationTitle("Outgoing files")
  }
}

#Preview {
  OutgoingFilesView(
    model: Endpoint(
      id: "R2D2",
      name: "Debug droid",
      isIncoming: false, state: .discovered,
      outgoingFiles: [
        OutgoingFile(localPath: "IMG_0001.jpg", fileSize: 4000000, state: .loading),
        OutgoingFile(localPath: "IMG_0002.jpg", fileSize: 5000000, state: .uploading),
        OutgoingFile(localPath: "IMG_0003.jpg", fileSize: 5000000, state: .uploaded, remotePath: "1234567890ABCDEF"),
        OutgoingFile(localPath: "IMG_0004.jpg", fileSize: 4000000, state: .loaded),
        OutgoingFile(localPath: "IMG_0005.jpg", fileSize: 4000000, state: .picked)
      ]
    )
  )
}
