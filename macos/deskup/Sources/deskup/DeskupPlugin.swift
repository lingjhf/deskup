import Cocoa
import FlutterMacOS
import Sparkle

public class DeskupPlugin: NSObject, FlutterPlugin, SPUUpdaterDelegate {
  private var updaterController: SPUStandardUpdaterController?
  private var initialized = false
  private var configuredFeed: String?

  public func feedURLString(for updater: SPUUpdater) -> String? { configuredFeed }
  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "deskup", binaryMessenger: registrar.messenger)
    registrar.addMethodCallDelegate(DeskupPlugin(), channel: channel)
  }
  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "ready":
      guard !initialized else {
        return result(FlutterError(code: "alreadyInitialized", message: "Native configuration is immutable after initialization", details: nil))
      }
      guard let root = call.arguments as? [String: Any], let settings = root["macos"] as? [String: Any] else {
        return result(FlutterError(code: "invalidConfiguration", message: "Expected macOS configuration", details: nil))
      }
      do {
        try self.startUpdatesIfConfigured(settings)
        initialized = true
        result(nil)
      } catch {
        result(FlutterError(code: "invalidConfiguration", message: error.localizedDescription, details: nil))
      }
    case "status":
      result([
        "supportsDownload": false,
        "supportsInstall": false,
        "configured": self.updaterController != nil,
        "canCheck": self.updaterController?.updater.canCheckForUpdates ?? false,
        "automaticChecks": self.updaterController?.updater.automaticallyChecksForUpdates ?? false,
        "version": Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "",
        "build": Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "",
      ])
    case "automaticChecks":
      guard let enabled = call.arguments as? Bool, let controller = self.updaterController else {
        return result(FlutterError(code: "unconfigured", message: "Updates are not configured", details: nil))
      }
      controller.updater.automaticallyChecksForUpdates = enabled
      result(nil)
    case "check":
      guard let controller = self.updaterController else {
        return result(FlutterError(code: "unconfigured", message: "Updates are not configured", details: nil))
      }
      if controller.updater.canCheckForUpdates { controller.checkForUpdates(nil) }
      result(nil)
    default:
      result(FlutterMethodNotImplemented)
    }
  }

  private func startUpdatesIfConfigured(_ settings: [String: Any]) throws {
    guard let allowLocal = settings["allowLocalHttp"] as? Bool else {
      throw configurationError("Expected allowLocalHttp")
    }
    let feed = settings["feedUrl"] as? String ?? Bundle.main.object(forInfoDictionaryKey: "SUFeedURL") as? String
    guard let feed = feed else { return }
    guard let url = URL(string: feed), url.host != nil, url.user == nil, url.password == nil,
      url.query == nil, url.fragment == nil else { throw configurationError("Invalid appcast URL") }
    let localTest = allowLocal && url.scheme == "http" && url.port != nil
      && (url.host == "localhost" || url.host == "127.0.0.1")
    guard url.scheme == "https" || localTest else { throw configurationError("Appcast requires HTTPS") }
    guard let key = Bundle.main.object(forInfoDictionaryKey: "SUPublicEDKey") as? String,
      Data(base64Encoded: key)?.count == 32 else { throw configurationError("Missing bundled Ed25519 public key") }
    let interval = settings["automaticCheckIntervalSeconds"] as? Int
    if let interval = interval, !(3600...2592000).contains(interval) {
      throw configurationError("Invalid automatic check interval")
    }
    configuredFeed = feed
    let controller = SPUStandardUpdaterController(
      startingUpdater: false, updaterDelegate: self, userDriverDelegate: nil)
    if let interval = interval { controller.updater.updateCheckInterval = TimeInterval(interval) }
    try controller.updater.start()
    updaterController = controller
  }

  private func configurationError(_ message: String) -> NSError {
    NSError(domain: "deskup.configuration", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
  }
}
