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

// #include "logs/proto/location/nearby/nearby_client_log.proto.h"
#include "google/protobuf/descriptor.h"
#include "gtest/gtest.h"
#include "internal/platform/logging.h"
#include "internal/proto/analytics/connections_log.pb.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace analytics {
namespace proto {

namespace {

using G3ConnectionsLog = ::location::nearby::analytics::proto::ConnectionsLog;
using P3ConnectionsLog = ::location::nearby::analytics::proto::ConnectionsLog;

using ::proto2::Descriptor;
using ::proto2::FieldDescriptor;

// Forward declaration.
bool Compare(const Descriptor* desc1, const Descriptor* desc2);

// Compares the two field descriptors and return false if name, number, label,
// or type is different.
bool Compare(const FieldDescriptor* field1, const FieldDescriptor* field2) {
  if (field1->name() != field2->name()) {
    NEARBY_LOGS(WARNING) << "Field name diff: " << field1->name() << " <=> "
                         << field2->name();
    return false;
  }
  if (field1->number() != field2->number()) {
    NEARBY_LOGS(WARNING) << "Field " << field1->name()
                         << " number diff: " << field1->number() << " <=> "
                         << field2->number();
    return false;
  }
  if (field1->label() != field2->label()) {
    NEARBY_LOGS(WARNING) << "Field " << field1->name()
                         << " label diff: " << field1->label() << " <=> "
                         << field2->label();
    return false;
  }
  bool bRet = false;
  if (field1->type() != field2->type()) {
    NEARBY_LOGS(WARNING) << "Field " << field1->name()
                         << " type diff: " << field1->type() << " <=> "
                         << field2->type();
    return bRet;
  } else if (field1->type() == FieldDescriptor::TYPE_MESSAGE) {
    const Descriptor* msg1 = field1->message_type();
    const Descriptor* msg2 = field2->message_type();

    bRet = Compare(msg1, msg2);
  } else {
    bRet = true;
  }

  return bRet;
}

// Compares the two descriptors and return false immediately if different.
bool Compare(const Descriptor* desc1, const Descriptor* desc2) {
  NEARBY_LOGS(INFO) << "Descriptor1 full name: " << desc1->full_name()
                    << " <=> " << desc2->full_name();
  for (int i = 0; i < desc1->field_count(); ++i) {
    const FieldDescriptor* field1 = desc1->field(i);
    const FieldDescriptor* field2 = desc2->FindFieldByName(field1->name());

    bool bRet = false;
    if (field2) {
      bRet = Compare(field1, field2);
    } else {
      NEARBY_LOGS(ERROR) << "Descriptor1 full name: " << desc1->full_name()
                         << "=> Extra field1 name=" << field1->name()
                         << ", number=" << field1->number()
                         << ", label=" << field1->label()
                         << ", type=" << field1->type();
    }
    if (!bRet) {
      return false;
    }
  }
  for (int i = 0; i < desc2->field_count(); ++i) {
    const FieldDescriptor* field2 = desc2->field(i);
    const FieldDescriptor* field1 = desc1->FindFieldByName(field2->name());
    if (!field1) {
      NEARBY_LOGS(ERROR) << "Descriptor2 full name: " << desc2->full_name()
                         << "=> Extra field2 name=" << field2->name()
                         << ", number=" << field2->number()
                         << ", label=" << field2->label()
                         << ", type=" << field2->type();
      return false;
    }
  }

  return true;
}

TEST(ConnectionsLogTest, TwoMessagesAreIdentical) {
  const proto2::Descriptor* descriptor1 = G3ConnectionsLog::descriptor();
  const proto2::Descriptor* descriptor2 = P3ConnectionsLog::descriptor();

  EXPECT_TRUE(Compare(descriptor1, descriptor2));
}

}  // namespace

}  // namespace proto
}  // namespace analytics
}  // namespace nearby
