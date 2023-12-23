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
import CoreImage.CIFilterBuiltins

struct QrCodeView: View {
  @EnvironmentObject var model: Main

  let context = CIContext()
  let filter = CIFilter.qrCodeGenerator()

  var body: some View {
    Image(uiImage: generateQRCode())
      .interpolation(.none)
      .resizable()
      .scaledToFit()
  }

  func generateQRCode() -> UIImage {
    filter.message = model.qrCodeData!

    if let outputImage = filter.outputImage {
      if let cgimg = context.createCGImage(outputImage, from: outputImage.extent) {
        return UIImage(cgImage: cgimg)
      }
    }

    return UIImage(systemName: "xmark.circle") ?? UIImage()
  }
}
