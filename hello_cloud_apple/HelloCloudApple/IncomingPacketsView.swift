//
//  Downloads.swift
//  HelloCloudApple
//
//  Created by Deling Ren on 12/15/23.
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
      await CloudDatabase.shared.pull(packet: packet)
    }
  }

  var body: some View {
    ZStack {
      Form {
        Section{
          List {
            ForEach(Array(model.incomingPackets.enumerated()), id: \.1.id) { i, packet in
              DisclosureGroup (isExpanded: $model.incomingPackets[i].expanded) {
                ForEach(packet.files) { file in
                  HStack {
                    if file.state == .downloaded {
                      Button(action: {
                        self.imageUrl = file.localUrl
                      }) {
                        Label(String(describing: file.description), systemImage: "photo")
                      }.buttonStyle(.borderless)
                    } else {
                      Label(String(describing: file.description), systemImage: "photo")
                    }

                    Spacer()
                    ZStack {
                      // .received: grey dotted circle
                      // .downloaded: green filled circle
                      // .downloading: spinner
                      Image(systemName: "circle.dotted").foregroundColor(.gray)
                        .opacity(file.state == .received ? 1 : 0)
                      Image(systemName: "circle.fill").foregroundColor(.green)
                        .opacity(file.state == .downloaded ? 1 : 0)
                      ProgressView()
                        .opacity(file.state == .downloading ? 1 :0)
                    }
                  }
                }
              } label: {
                Text(String(describing: packet))
                Spacer()
                // .received: grey dotted circle
                // .uploaded: download button
                // .downloading: spinner
                // .downloaded: green filled circle
                ZStack{
                  Button(action: { packet.download() }) {
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
                }
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
    .navigationTitle("Incoming packets")
  }
}

#Preview {
  IncomingPacketsView().environment(Main.createDebugModel())
}
