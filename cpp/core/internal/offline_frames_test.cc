// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core/internal/offline_frames.h"

#include <memory>

#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/byte_array.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location::nearby::connections {

namespace {
using Medium = proto::connections::Medium;

std::unique_ptr<OfflineFrame> MakeFrame(V1Frame* sub_frame) {
  auto frame = std::make_unique<OfflineFrame>();
  frame->set_version(OfflineFrame::V1);
  frame->set_allocated_v1(sub_frame);
  return frame;
}

void SetSubframe(V1Frame* frame, ConnectionRequestFrame* sub_frame) {
  frame->set_type(V1Frame::CONNECTION_REQUEST);
  frame->set_allocated_connection_request(sub_frame);
}

constexpr ConnectionRequestFrame::Medium ToConnectionRequestMedium(
    proto::connections::Medium medium) {
  switch (medium) {
    case proto::connections::MDNS:
      return ConnectionRequestFrame::MDNS;
    case proto::connections::BLUETOOTH:
      return ConnectionRequestFrame::BLUETOOTH;
    case proto::connections::WIFI_HOTSPOT:
      return ConnectionRequestFrame::WIFI_HOTSPOT;
    case proto::connections::BLE:
      return ConnectionRequestFrame::BLE;
    case proto::connections::WIFI_LAN:
      return ConnectionRequestFrame::WIFI_LAN;
    default:
      return ConnectionRequestFrame::UNKNOWN_MEDIUM;
  }
}

}  // namespace

TEST(OfflineFramesTest, CanParseMessageFromBytes) {
  const string endpoint_id{"ABC"};
  const string endpoint_name{"XYZ"};
  const int nonce{1234};
  const std::vector<proto::connections::Medium> mediums{Medium::BLE,
                                                        Medium::BLUETOOTH};

  auto* v1_frame = new V1Frame{};
  auto* sub_frame = new ConnectionRequestFrame{};
  sub_frame->set_endpoint_id(endpoint_id);
  sub_frame->set_endpoint_name(endpoint_name);
  sub_frame->set_nonce(nonce);

  for (auto& medium : mediums) {
    sub_frame->add_mediums(ToConnectionRequestMedium(medium));
  }

  SetSubframe(v1_frame, sub_frame);
  auto frame = MakeFrame(v1_frame);

  auto bytes = MakeConstPtr(new ByteArray(frame->SerializeAsString()));

  auto ret_value = OfflineFrames::fromBytes(bytes);
  ASSERT_TRUE(ret_value.ok());
  const auto& rx_message = ret_value.result();
  ASSERT_TRUE(rx_message->has_version());
  ASSERT_EQ(rx_message->version(), OfflineFrame::V1);
  ASSERT_TRUE(rx_message->has_v1());
  const auto& rx_frame = rx_message->v1();
  ASSERT_EQ(rx_frame.type(), V1Frame::CONNECTION_REQUEST);
  ASSERT_TRUE(rx_frame.has_connection_request());
  const auto& req = rx_frame.connection_request();
  ASSERT_TRUE(req.has_endpoint_id());
  ASSERT_TRUE(req.has_endpoint_name());
  ASSERT_TRUE(req.has_nonce());
  ASSERT_EQ(req.endpoint_id(), endpoint_id);
  ASSERT_EQ(req.endpoint_name(), endpoint_name);
  ASSERT_EQ(req.nonce(), nonce);
  ASSERT_EQ(req.mediums_size(), mediums.size());
}

}  // namespace location::nearby::connections
