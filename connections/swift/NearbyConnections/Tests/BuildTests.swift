import NearbyConnections
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
