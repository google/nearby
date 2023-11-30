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

struct OutgoingFilesView: View {
  let model: Endpoint

  @EnvironmentObject var mainModel: Main
  @Environment(\.dismiss) var dismiss

  @State private var photosPicked: [PhotosPickerItem] = []
  
  func updateFileList() -> Void {
    model.outgoingFiles.removeAll()
    
    for photo in photosPicked {
      let type = photo.supportedContentTypes.first(where: {$0.preferredMIMEType == "image/jpeg"})
      let ext = type?.preferredFilenameExtension
      if ext == nil {
        print("Picked photo does not support jpeg format, using raw format")
      }
      let path = UUID().uuidString + (ext == nil ? "" : "." + ext!)
      let file = OutgoingFile(localPath: path, fileSize: 0, state: .loading)
      model.outgoingFiles.append(file)
      
      Task { [file, ext] in
        guard let data = try? await photo.loadTransferable(type: Data.self) else {
          return
        }

        if ext != nil {
          guard let uiImage = UIImage(data: data) else {
            return
          }
          guard let jpegData = uiImage.jpegData(compressionQuality: 1) else {
            return
          }
          file.data  = jpegData
          file.fileSize = UInt64(jpegData.count)
        } else {
          file.data = data
          file.fileSize = UInt64(data.count)
        }
        file.state = .loaded
      }
    }
  }
  
  func upload() -> Void {
    for file in model.outgoingFiles {
      file.upload()
      model.transfers.append(Transfer(direction: .upload, localPath: file.localPath, remotePath: file.remotePath!, result: .success))
    }
  }
  
  func send() -> Void {
    guard let payload = OutgoingFile.encodeOutgoingFiles(model.outgoingFiles) else {
      print("Failed to encode outgoing files. This should not happen.")
      return
    }
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
          } // Can pick files only when no files are loading or uploading
          .disabled(!model.outgoingFiles.allSatisfy({$0.state != .loading && $0.state != .uploading}))
          .onChange(of: photosPicked, updateFileList)
          
          Button(action: upload) {
            Label("Upload", systemImage: "arrow.up.circle.fill").frame(maxWidth: .infinity)
          } // Can upload only when all files are loaded in memory
          .disabled(model.outgoingFiles.isEmpty || !model.outgoingFiles.allSatisfy({$0.state == .loaded}))
          
          Button(action: send) {
            Label("Send", systemImage: "arrow.up.backward.circle.fill").frame(maxWidth: .infinity)
          } // Can send only when all files are uploaded to the cloud
          .disabled(model.outgoingFiles.isEmpty || !model.outgoingFiles.allSatisfy({$0.state == .uploaded}))
        }.buttonStyle(.bordered).fixedSize()
        
        List {
          ForEach(model.outgoingFiles) {file in
            HStack {
              // TODO: replace placholder with thumbnail
              Image(systemName: "photo")
              Text(file.localPath)
              Spacer()
              switch file.state {
              case .picked:
                Image(systemName: "circle.dotted").foregroundColor(.gray)
              case .loading:
                ProgressView()
              case .loaded:
                Image(systemName: "circle.fill").foregroundColor(.gray)
              case .uploading:
                ProgressView()
              case .uploaded:
                Image(systemName: "circle.fill").foregroundColor(.green)
              }
            }
          }
        }
      }
    }
    .navigationTitle("Outgoing files")
    .onChange(of: mainModel.endpoints) { _, endpoints in
      if !endpoints.contains(model) { dismiss() }
    }
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
  ).environment(Main.createDebugModel())
}
