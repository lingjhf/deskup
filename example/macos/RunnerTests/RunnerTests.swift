import Cocoa
import FlutterMacOS
import XCTest
@testable import deskup

class RunnerTests: XCTestCase {
  func testStatusWithoutConfiguredFeed() {
    let plugin = DeskupPlugin()
    let call = FlutterMethodCall(methodName: "status", arguments: nil)
    let response = expectation(description: "status reply")
    plugin.handle(call) { result in
      let status = result as? [String: Any]
      XCTAssertEqual(status?["configured"] as? Bool, false)
      XCTAssertEqual(status?["supportsDownload"] as? Bool, false)
      XCTAssertEqual(status?["supportsInstall"] as? Bool, false)
      response.fulfill()
    }
    waitForExpectations(timeout: 1)
  }
}
