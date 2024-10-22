// Copyright 2022 Google LLC
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

#include "sharing/internal/base/mime.h"

#include "gtest/gtest.h"

namespace nearby::utils {
namespace {

TEST(MimeTest, GetWellKnownMimeTypeFromExtension) {
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("txt"), "text/plain");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("pdf"), "application/pdf");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("doc"),
            "application/msword");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("docx"),
            "application/vnd.openxmlformats-officedocument.wordprocessingml."
            "document");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("xls"),
            "application/vnd.ms-excel");
}

TEST(MimeTest, GetWellKnownMimeTypeFromExtensionCaseInsensitive) {
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("TXT"), "text/plain");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("PDF"), "application/pdf");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("DOC"), "application/msword");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("DOCX"),
            "application/vnd.openxmlformats-officedocument.wordprocessingml."
            "document");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("XLS"),
            "application/vnd.ms-excel");
  EXPECT_EQ(GetWellKnownMimeTypeFromExtension("PNG"), "image/png");
}

}  // namespace
}  // namespace nearby::utils
