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

struct PayloadView: View {
    var payload: Payload

    var body: some View {
        HStack(alignment: .top) {
            Image(systemName: payload.isIncoming
                  ? "square.and.arrow.down"
                  : "square.and.arrow.up")
            VStack(alignment: .leading) {
                switch payload.type {
                case .bytes:
                    Text("Bytes")
                case .file:
                    Text("File")
                case .stream:
                    Text("Stream")
                }
                HStack {
                    switch payload.status {
                    case let .inProgress(progress):
                        Image(systemName: "xmark.circle.fill").onTapGesture {
                            payload.cancellationToken?.cancel()
                        }.foregroundColor(.secondary)
                        ProgressView(value: Float(progress.completedUnitCount), total: Float(progress.totalUnitCount))
                    case .canceled:
                        Text("Canceled")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    case .failure:
                        Text("Failed")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    case .success:
                        Text(payload.isIncoming ?  "Received" : "Sent")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
            }
        }
    }
}

struct PayloadView_Previews: PreviewProvider {
    static var previews: some View {
        List {
            PayloadView(
                payload: Payload(
                    id: 0,
                    type: .bytes,
                    status: .success,
                    isIncoming: false,
                    cancellationToken: nil
                )
            )
            PayloadView(
                payload: Payload(
                    id: 0,
                    type: .bytes,
                    status: .success,
                    isIncoming: true,
                    cancellationToken: nil
                )
            )
            PayloadView(
                payload: Payload(
                    id: 0,
                    type: .bytes,
                    status: .canceled,
                    isIncoming: true,
                    cancellationToken: nil
                )
            )
            PayloadView(
                payload: Payload(
                    id: 0,
                    type: .bytes,
                    status: .failure,
                    isIncoming: true,
                    cancellationToken: nil
                )
            )
            PayloadView(
                payload: Payload(
                    id: 0,
                    type: .file,
                    status: .inProgress(Progress(totalUnitCount: 100)),
                    isIncoming: true,
                    cancellationToken: nil
                )
            )
            PayloadView(
                payload: Payload(
                    id: 0,
                    type: .stream,
                    status: .inProgress(Progress()),
                    isIncoming: true,
                    cancellationToken: nil
                )
            )
        }
    }
}
