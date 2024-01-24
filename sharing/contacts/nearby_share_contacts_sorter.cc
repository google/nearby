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

#include <algorithm>
#include <locale>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "sharing/proto/rpc_resources.pb.h"

namespace nearby {
namespace sharing {
namespace {

struct ContactSortingFields {
  // Primary sorting key: person name if not empty; otherwise, email.
  std::optional<std::string> person_name_or_email;
  // Secondary sorting key. Note: It is okay if email is also used as the
  // primary sorting key.
  std::optional<std::string> email;
  // Tertiary sorting key.
  std::optional<std::string> phone_number;
  // Last resort sorting key. The contact ID should be unique for each
  // contact record, guaranteeing uniquely defined ordering.
  std::string id;
};

ContactSortingFields GetContactSortingFields(
    const nearby::sharing::proto::ContactRecord& contact) {
  ContactSortingFields fields;
  fields.id = contact.id();
  for (const proto::Contact_Identifier& identifier : contact.identifiers()) {
    switch (identifier.identifier_case()) {
      case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
          kAccountName:
        if (!fields.email) {
          fields.email = identifier.account_name();
        }
        break;
      case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
          kPhoneNumber:
        if (!fields.phone_number) {
          fields.phone_number = identifier.phone_number();
        }
        break;
      case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
          kObfuscatedGaia:
        break;
      case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
          IDENTIFIER_NOT_SET:
        break;
    }
  }
  fields.person_name_or_email =
      contact.person_name().empty()
          ? fields.email
          : std::make_optional<std::string>(contact.person_name());

  return fields;
}

class ContactRecordComparator {
 public:
  explicit ContactRecordComparator(std::locale locale) : locale_(locale) {}

  bool operator()(const nearby::sharing::proto::ContactRecord& c1,
                  const nearby::sharing::proto::ContactRecord& c2) const {
    ContactSortingFields f1 = GetContactSortingFields(c1);
    ContactSortingFields f2 = GetContactSortingFields(c2);

    switch (CollatorCompare(f1.person_name_or_email, f2.person_name_or_email)) {
      case 0:
        // Do nothing. Compare with the next field.
        break;
      case -1:
        return true;
      case 1:
        return false;
    }

    switch (CollatorCompare(f1.email, f2.email)) {
      case 0:
        // Do nothing. Compare with the next field.
        break;
      case -1:
        return true;
      case 1:
        return false;
    }

    if (f1.phone_number != f2.phone_number) {
      if (!f1.phone_number) return false;
      if (!f2.phone_number) return true;
      return *f1.phone_number < *f2.phone_number;
    }

    return f1.id < f2.id;
  }

 private:
  int CollatorCompare(const std::optional<std::string>& a,
                      const std::optional<std::string>& b) const {
    // Sort populated strings before absl::nullopt.
    if (!a && !b) return 0;
    if (!b) return -1;
    if (!a) return 1;

    // Sort using a locale-based collator if available.
    if (std::has_facet<std::ctype<char>>(locale_)) {
      auto& facet = std::use_facet<std::collate<char>>(locale_);
      std::string s1 = *a;
      std::string s2 = *b;

      return facet.compare(&s1[0], &s1[0] + s1.size(), &s2[0],
                           &s2[0] + s2.size());
    }

    // Fall back on standard string comparison, though we hope and expect
    // that locale-based sorting will succeed.
    if (*a == *b) {
      return 0;
    }
    return *a < *b ? -1 : 1;
  }

  std::locale locale_;
};

}  // namespace

void SortNearbyShareContactRecords(
    std::vector<nearby::sharing::proto::ContactRecord>* contacts,
    absl::string_view locale_string) {
  // initialized to default program environment locale.
  std::locale loc = std::locale("");

  if (!locale_string.empty()) {
    loc = std::locale(locale_string.data());
  }

  ContactRecordComparator comparator(loc);
  std::sort(contacts->begin(), contacts->end(), comparator);
}

}  // namespace sharing
}  // namespace nearby
