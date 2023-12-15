//
//  UploadsView.swift
//  HelloCloudApple
//
//  Created by Deling Ren on 12/15/23.
//

import SwiftUI

struct UploadsView: View {
  @EnvironmentObject var model: Main

  var body: some View {
    Button(action: { model.notifyReceiver() }) {
      Text("Upload")}
  }
}

#Preview {
    UploadsView()
}
