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

struct OutgoingPacketsView: View {
  @EnvironmentObject var model: Main
  @State var expanded: [Bool] = []

  var body: some View {
    VStack {
      Label("Outgoing", systemImage: "paperplane").font(.title).padding([.top], 10)

      Form {
        Section{
          List {
            ForEach($model.outgoingPackets) { $packet in
              DisclosureGroup (isExpanded: $packet.expanded) {
                ForEach(Array(packet.files)) { file in
                  HStack {
                    ZStack {
                      // .picked: grey dotted circle
                      // .loaded: grey filled circle
                      // .uploaded: green filled circle
                      // .uploading, .loading: spinner
                      Image(systemName: "circle.dotted").foregroundColor(.gray)
                        .opacity(file.state == .picked ? 1 : 0)
                      Image(systemName: "circle.fill").foregroundColor(.gray)
                        .opacity(file.state == .loaded ? 1 : 0)
                      Image(systemName: "circle.fill").foregroundColor(.green)
                        .opacity(file.state == .uploaded ? 1 : 0)
                      ProgressView()
                        .opacity(file.state == .loading || file.state == .uploading ? 1 : 0)
                    }.padding(2)
                    Label(String(describing: file.description), systemImage: "photo")
                  }
                }
              } label: {
                ZStack {
                  Button(action: {
                    Task { await packet.upload() }
                  }) {
                    Image(systemName: "icloud.and.arrow.up.fill")
                  }
                  .buttonStyle(.borderless)
                  .opacity(packet.state == .loaded ? 1 : 0)

                  ProgressView()
                    .opacity(packet.state == .uploading ? 1 : 0)

                  Image(systemName: "circle.fill").foregroundColor(.green)
                    .opacity(packet.state == .uploaded ? 1 :0)
                }.padding(2)

                Label(String(describing: packet), systemImage: "photo.on.rectangle.angled")
              }
            }
          }
        }
      }
    }
  }
}

#Preview {
  OutgoingPacketsView().environment(Main.createDebugModel())
}
