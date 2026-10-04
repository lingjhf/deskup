import Cocoa
import FlutterMacOS
import Sparkle

public class DeskupPlugin: NSObject, FlutterPlugin {
  private var updaterController: SPUStandardUpdaterController?
  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "deskup", binaryMessenger: registrar.messenger)
    registrar.addMethodCallDelegate(DeskupPlugin(), channel: channel)
  }
  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "ready":
      self.startUpdatesIfConfigured()
      result(nil)
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

  private func startUpdatesIfConfigured() {
    guard updaterController == nil,
      let feed = Bundle.main.object(forInfoDictionaryKey: "SUFeedURL") as? String,
      let url = URL(string: feed),
      let key = Bundle.main.object(forInfoDictionaryKey: "SUPublicEDKey") as? String,
      Data(base64Encoded: key)?.count == 32 else { return }
    let localTest = Bundle.main.object(forInfoDictionaryKey: "DeskupAllowLocalHTTP") as? Bool == true
      && url.scheme == "http" && url.host == "localhost"
    guard url.scheme == "https" || localTest else { return }
    let controller = SPUStandardUpdaterController(
      startingUpdater: false, updaterDelegate: nil, userDriverDelegate: nil)
    do {
      try controller.updater.start()
      updaterController = controller
    } catch {
      NSLog("Could not start deskup updater: %@", error.localizedDescription)
    }
  }
}
