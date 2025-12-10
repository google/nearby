// Copyright 2025 Google LLC
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

#include "internal/platform/service_address.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"

namespace nearby {
namespace {

using ProtoServiceAddress = ::location::nearby::connections::ServiceAddress;

TEST(ServiceAddressTest, IPv4ServiceAddressToProto) {
  ServiceAddress service_address = {
      .address = {127, 0, 0, 1},
      .port = 8080,
  };
  ProtoServiceAddress proto;
  ServiceAddressToProto(service_address, proto);
  EXPECT_EQ(proto.ip_address(), std::string("\x7f\0\0\1", 4));
  EXPECT_EQ(proto.port(), 8080);
}

TEST(ServiceAddressTest, IPv4ServiceAddressFromProto) {
  ProtoServiceAddress proto;
  proto.set_ip_address(std::string("\x7f\0\0\1", 4));
  proto.set_port(8080);
  ServiceAddress service_address;
  EXPECT_TRUE(ServiceAddressFromProto(proto, service_address));
  EXPECT_EQ(service_address.address, std::vector<char>({127, 0, 0, 1}));
  EXPECT_EQ(service_address.port, 8080);
}

TEST(ServiceAddressTest, IPv6ServiceAddressToProto) {
  ServiceAddress service_address = {
      .address = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      .port = 8080,
  };
  ProtoServiceAddress proto;
  ServiceAddressToProto(service_address, proto);
  EXPECT_EQ(proto.ip_address(),
            std::string("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16));
  EXPECT_EQ(proto.port(), 8080);
}

TEST(ServiceAddressTest, IPv6ServiceAddressFromProto) {
  ProtoServiceAddress proto;
  proto.set_ip_address(std::string("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16));
  proto.set_port(8080);
  ServiceAddress service_address;
  EXPECT_TRUE(ServiceAddressFromProto(proto, service_address));
  EXPECT_EQ(
      service_address.address,
      std::vector<char>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));
  EXPECT_EQ(service_address.port, 8080);
}

TEST(ServiceAddressTest, ServiceAddressFromProtoInvalidAddress) {
  ProtoServiceAddress proto;
  proto.set_ip_address(std::string("\x7f\0\0", 3));
  proto.set_port(8080);
  ServiceAddress service_address;
  EXPECT_FALSE(ServiceAddressFromProto(proto, service_address));
}

TEST(ServiceAddressTest, ServiceAddressFromProtoInvalidPort) {
  ProtoServiceAddress proto;
  proto.set_ip_address(std::string("\x7f\0\0\1", 4));
  proto.set_port(0);
  ServiceAddress service_address;
  EXPECT_FALSE(ServiceAddressFromProto(proto, service_address));
}

TEST(ServiceAddressTest, ServiceAddressEquality) {
  ServiceAddress service_address1 = {
      .address = {127, 0, 0, 1},
      .port = 8080,
  };
  ServiceAddress service_address2 = {
      .address = {127, 0, 0, 1},
      .port = 8080,
  };
  ServiceAddress service_address3 = {
      .address = {127, 0, 0, 2},
      .port = 8080,
  };
  ServiceAddress service_address4 = {
      .address = {127, 0, 0, 1},
      .port = 8081,
  };
  ServiceAddress service_address5 = {
      .address = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      .port = 8080,
  };
  ServiceAddress service_address6 = {
      .address = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
      .port = 8080,
  };
  EXPECT_EQ(service_address1, service_address2);
  EXPECT_NE(service_address1, service_address3);
  EXPECT_NE(service_address1, service_address4);
  EXPECT_NE(service_address1, service_address5);
  EXPECT_EQ(service_address5, service_address6);
}

}  // namespace
}  // namespace nearby
