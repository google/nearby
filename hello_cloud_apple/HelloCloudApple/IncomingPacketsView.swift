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

struct IncomingPacketsView: View {
  @EnvironmentObject var model: Main
  @State var imageUrl: URL? = nil

  func refresh () async {
    for packet in model.incomingPackets {
      if packet.state == .uploaded {
        continue
      }
      guard let newPacket = await CloudDatabase.shared.pull(packetId: packet.packetId) else {
        continue
      }
      packet.update(from: newPacket)
    }
  }
  
  var body: some View {
    VStack {
      Label("Incoming", systemImage: "tray").font(.title).padding([.top], 10)

      ZStack {
        Form {
          Section{
            List {
              ForEach($model.incomingPackets) { $packet in
                DisclosureGroup (isExpanded: $packet.expanded) {
                  ForEach(packet.files) { file in
                    HStack {
                      ZStack {
                        // .received: grey dotted circle
                        // .uploaded: grey filled circle
                        // .downloaded: green filled circle
                        // .downloading: spinner
                        Image(systemName: "circle.dotted").foregroundColor(.gray)
                          .opacity(file.state == .received ? 1 : 0)
                        Image(systemName: "circle.fill").foregroundColor(.gray)
                          .opacity(file.state == .uploaded ? 1 : 0)
                        Image(systemName: "circle.fill").foregroundColor(.green)
                          .opacity(file.state == .downloaded ? 1 : 0)
                        ProgressView()
                          .opacity(file.state == .downloading ? 1 :0)
                      }.padding(2)

                      if file.state == .downloaded {
                        Button(action: {
                          self.imageUrl = file.localUrl
                        }) {
                          Label(String(describing: file.description), systemImage: "photo")
                        }.buttonStyle(.borderless)
                      } else {
                        Label(String(describing: file.description), systemImage: "photo")
                      }
                    }
                  }
                } label: {
                  // .received: grey dotted circle
                  // .uploaded: download button
                  // .downloading: spinner
                  // .downloaded: green filled circle
                  ZStack{
                    Button(action: {
                      Task { await packet.download() }
                    }) {
                      Image(systemName: "icloud.and.arrow.down.fill")
                    }
                    .buttonStyle(.borderless)
                    .opacity(packet.state == .uploaded ? 1 :0)

                    Image(systemName: "circle.dotted").foregroundColor(.gray)
                      .opacity(packet.state == .received ? 1 :0)

                    ProgressView()
                      .opacity(packet.state == .downloading ? 1 : 0)

                    Image(systemName: "circle.fill").foregroundColor(.green)
                      .opacity(packet.state == .downloaded ? 1 :0)
                  }.padding(2)

                  Label(String(describing: packet), systemImage: "photo.on.rectangle.angled")
                }
              }
            }
          }
        }
        if imageUrl != nil {
          ImageView(url: $imageUrl)
        }
      }
      .refreshable {
        await refresh()
      }
    }
  }
}

#Preview {
  IncomingPacketsView().environment(Main.createDebugModel())
}
