// Copyright 2021 Google LLC
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

#include "sharing/contacts/nearby_share_contacts_sorter.h"

#include <stddef.h>

#include <algorithm>
#include <locale>
#include <random>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::proto::ContactRecord;
using ::protobuf_matchers::EqualsProto;
using ::testing::Pointwise;

const std::vector<ContactRecord>& contacts() {
  static const std::vector<ContactRecord>* contacts =
      new std::vector<ContactRecord>([] {
        ContactRecord contact0;
        contact0.set_person_name("Claire");
        contact0.set_is_reachable(true);

        ContactRecord contact1;
        contact1.set_person_name("Alice");
        contact1.add_identifiers()->set_account_name("y@gmail.com");
        contact1.set_is_reachable(true);

        ContactRecord contact2;
        contact2.set_person_name("Alice");
        contact2.add_identifiers()->set_account_name("x@gmail.com");
        contact2.set_is_reachable(true);

        ContactRecord contact3;
        contact3.add_identifiers()->set_account_name("bob@gmail.com");
        contact3.set_is_reachable(true);

        ContactRecord contact4;
        contact4.add_identifiers()->set_phone_number("222-222-2222");
        contact4.set_is_reachable(true);

        ContactRecord contact5;
        contact5.add_identifiers()->set_phone_number("111-111-1111");
        contact5.set_is_reachable(true);

        ContactRecord contact6;
        contact6.set_person_name("David");
        contact6.add_identifiers()->set_account_name("z@gmail.com");
        contact6.add_identifiers()->set_phone_number("222-222-2222");
        contact6.set_is_reachable(true);

        ContactRecord contact7;
        contact7.set_person_name("David");
        contact7.add_identifiers()->set_account_name("z@gmail.com");
        contact7.add_identifiers()->set_phone_number("111-111-1111");
        contact7.set_id("2");
        contact7.set_is_reachable(true);

        ContactRecord contact8;
        contact8.set_person_name("David");
        contact8.add_identifiers()->set_account_name("z@gmail.com");
        contact8.add_identifiers()->set_phone_number("111-111-1111");
        contact8.set_id("1");
        contact8.set_is_reachable(true);

        ContactRecord contact9;
        contact9.set_person_name("中村光");
        contact9.set_is_reachable(true);
        auto a = contact9.person_name();

        ContactRecord contact10;
        contact10.set_person_name("王皓");
        contact10.set_is_reachable(true);

        ContactRecord contact11;
        contact11.set_person_name("中村俊輔");
        contact11.set_is_reachable(true);

        ContactRecord contact12;
        contact12.set_person_name("丁立人");
        contact12.set_is_reachable(true);

        ContactRecord contact13;
        contact13.set_person_name("Á");
        contact13.set_is_reachable(true);

        ContactRecord contact14;
        contact14.set_person_name("Ñ");
        contact14.set_is_reachable(true);

        ContactRecord contact15;
        contact15.set_person_name("å");
        contact15.set_id("5");
        contact15.set_is_reachable(true);

        ContactRecord contact16;
        contact16.set_person_name("Å");
        contact16.set_id("3");
        contact16.set_is_reachable(true);

        ContactRecord contact17;
        contact17.set_person_name("åz");
        contact17.set_id("4");
        contact17.set_is_reachable(true);

        ContactRecord contact18;
        contact18.set_person_name("Opus");
        contact18.set_is_reachable(true);

        return std::vector<ContactRecord>{
            contact0,  contact1,  contact2,  contact3,  contact4,
            contact5,  contact6,  contact7,  contact8,  contact9,
            contact10, contact11, contact12, contact13, contact14,
            contact15, contact16, contact17, contact18};
      }());

  return *contacts;
}

void VerifySort(const std::vector<ContactRecord>& expected_contacts,
                const std::vector<ContactRecord>& unsorted_contacts,
                absl::string_view locale_string) {
  // Try a few different permutations of |unsorted_contacts|, which should all
  // be sorted to |expected_contacts|.
  std::default_random_engine rng;
  for (size_t i = 0; i < 10u; ++i) {
    std::vector<ContactRecord> sorted_contacts = contacts();
    std::shuffle(sorted_contacts.begin(), sorted_contacts.end(), rng);
    SortNearbyShareContactRecords(&sorted_contacts, locale_string);
    ASSERT_EQ(expected_contacts.size(), sorted_contacts.size());
    EXPECT_THAT(sorted_contacts, Pointwise(EqualsProto(), sorted_contacts));
  }
}

TEST(NearbyShareContactsSorter, US) {
  // Expected ordering:
  //  Á        |               |              |
  //  å        |               |              | ID: 5
  //  Å        |               |              | ID: 3
  //  Alice    | x@gmail.com   |              |
  //  Alice    | y@gmail.com   |              |
  //  åz       |               |              | ID: 4
  //           | bob@gmail.com |              |
  //  Claire   |               |              |
  //  David    | z@gmail.com   | 111-111-1111 | ID: 1
  //  David    | z@gmail.com   | 111-111-1111 | ID: 2
  //  David    | z@gmail.com   | 222-222-2222 |
  //  Ñ        |               |              |
  //  Opus     |               |              |
  //  丁立人   |               |              |
  //  中村俊輔 |               |              |
  //  中村光   |               |              |
  //  王皓     |               |              |
  //           |               | 111-111-1111 |
  //           |               | 222-222-2222 |
  std::vector<ContactRecord> expected_contacts{
      contacts()[13], contacts()[15], contacts()[16], contacts()[2],
      contacts()[1],  contacts()[17], contacts()[3],  contacts()[0],
      contacts()[8],  contacts()[7],  contacts()[6],  contacts()[14],
      contacts()[18], contacts()[12], contacts()[11], contacts()[9],
      contacts()[10], contacts()[5],  contacts()[4]};
  ASSERT_NO_FATAL_FAILURE(
      VerifySort(expected_contacts, contacts(), "en_US.UTF-8"));
}

TEST(NearbyShareContactsSorter, DISABLED_Sweden) {
  // Expected ordering:
  //  Á        |               |              |
  //  Alice    | x@gmail.com   |              |
  //  Alice    | y@gmail.com   |              |
  //           | bob@gmail.com |              |
  //  Claire   |               |              |
  //  David    | z@gmail.com   | 111-111-1111 | ID: 1
  //  David    | z@gmail.com   | 111-111-1111 | ID: 2
  //  David    | z@gmail.com   | 222-222-2222 |
  //  Ñ        |               |              |
  //  Opus     |               |              |
  //  å        |               |              | ID: 5
  //  Å        |               |              | ID: 3
  //  åz       |               |              | ID: 4
  //  丁立人   |               |              |
  //  中村俊輔 |               |              |
  //  中村光   |               |              |
  //  王皓     |               |              |
  //           |               | 111-111-1111 |
  //           |               | 222-222-2222 |
  std::vector<ContactRecord> expected_contacts{
      contacts()[13], contacts()[2],  contacts()[1],  contacts()[3],
      contacts()[0],  contacts()[8],  contacts()[7],  contacts()[6],
      contacts()[14], contacts()[18], contacts()[15], contacts()[16],
      contacts()[17], contacts()[12], contacts()[11], contacts()[9],
      contacts()[10], contacts()[5],  contacts()[4]};
  ASSERT_NO_FATAL_FAILURE(
      VerifySort(expected_contacts, contacts(), "sv-SE.UTF-8"));
}

TEST(NearbyShareContactsSorter, DISABLED_China) {
  // Expected ordering:
  //  Á        |               |              |
  //  å        |               |              | ID: 5
  //  Å        |               |              | ID: 3
  //  Alice    | x@gmail.com   |              |
  //  Alice    | y@gmail.com   |              |
  //  åz       |               |              | ID: 4
  //           | bob@gmail.com |              |
  //  Claire   |               |              |
  //  David    | z@gmail.com   | 111-111-1111 | ID: 1
  //  David    | z@gmail.com   | 111-111-1111 | ID: 2
  //  David    | z@gmail.com   | 222-222-2222 |
  //  Ñ        |               |              |
  //  Opus     |               |              |
  //  丁立人   |               |              |
  //  王皓     |               |              |
  //  中村光   |               |              |
  //  中村俊輔 |               |              |
  //           |               | 111-111-1111 |
  //           |               | 222-222-2222 |
  std::vector<ContactRecord> expected_contacts{
      contacts()[13], contacts()[15], contacts()[16], contacts()[2],
      contacts()[1],  contacts()[17], contacts()[3],  contacts()[0],
      contacts()[8],  contacts()[7],  contacts()[6],  contacts()[14],
      contacts()[18], contacts()[12], contacts()[10], contacts()[9],
      contacts()[11], contacts()[5],  contacts()[4]};
  ASSERT_NO_FATAL_FAILURE(
      VerifySort(expected_contacts, contacts(), "zh_CN.UTF-8"));
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
