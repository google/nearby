// Copyright 2023 Google LLC
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

import NearbyCoreAdapter
import XCTest

// Used by GitHub Actions to test building/linking the app.
//
// This is mainly useful for catching copybara issues or for when source files are added to the
// project, but the `Package.swift` is not updated.
class BuildTests: XCTestCase {
  func testBuild() {
    // At least one test case is needed.
  }
}
