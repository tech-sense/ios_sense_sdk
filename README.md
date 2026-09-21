## Sense — iOS SDK

Sense is a device intelligence, identification and runtime application self-protection (RASP) SDK. It collects a comprehensive set of attributes unique to a device, forming a stable device identity (`sense_id`), and continuously monitors the runtime environment for jailbreak, instrumentation, network interception, screen capture and social-engineering threats.

The SDK exposes three independent surfaces:

| Surface | Interface / API | Purpose |
|---|---|---|
| Device identity | `SenseDelegate` | One-shot call returning the device identity payload |
| Runtime protection (RASP) | `SenseRASPDelegate` | Continuous threat callbacks, each fired with an `isOn` state |
| Screen shield | `Sense.screenShield(for:)` | Blanks the app's window against screenshots, recording and mirroring |

---

### Requirements

```
* iOS 13.0 or above          (scene-based lifecycle; iOS 12 supported in app-delegate mode)
* Swift 5.0 and above
* Xcode 14 or above
```

> Location, local-network and app-query entries in `Info.plist` are optional. Without them the corresponding signals are simply not collected — the SDK degrades gracefully rather than failing.

---

### Step 1 — Installation

**CocoaPods**

```ruby
pod 'SenseSDK', '~> 1.0.9'
```

```bash
pod install        # first time
pod update         # to pick up a newer patch release
```

---

### Step 2 — Import the SDK

```swift
import Sense
```

If you do not want the SDK's symbols re-exported from your own module's public interface (recommended for frameworks and for library targets):

```swift
internal import Sense      // Swift 5.9+
```

> **Module name vs pod name.** The CocoaPods pod is `SenseSDK`; the Swift module it vends is `Sense`. If `import Sense` does not resolve, try `import SenseSDK` and confirm the module name against the installed framework — this differs between SDK releases.

---

### Step 3 — Initialize the SDK

Initialize as early as possible in the app lifecycle — the earlier it runs, the sooner runtime protection is armed. In a scene-based app this is `scene(_:willConnectTo:options:)`; in an app-delegate app, `application(_:didFinishLaunchingWithOptions:)`.

```swift
func initiateSense() {
    let senseConfig = SenseConfig()
    senseConfig.apiKey           = "Your Unique Public API Key"
    senseConfig.senseInfo        = false        // true or false
    senseConfig.allowGeoLocation = true         // true or false
    senseConfig.tag              = "index"      // free-form call-site label
    senseConfig.sessionId        = ""           // optional, used for Workflow
    senseConfig.metaInfo = [
        "phone": "9987654532",
        "name":  "kumar"
    ]

    // Certificate pinning — see Step 4
    senseConfig.sensePinningConfig = SensePinningConfig(
        pinSets: [
            SensePinSet(
                host: "www.dempapp.com",
                pins: [
                    "sha256............"
                ]
            )
        ],
        enabled: true
    )

    Sense.initSDK(senseConfig: senseConfig, withDelegate: self)
}
```

#### `SenseConfig` properties

| Property | Type | Required | Description |
|---|---|:--:|---|
| `apiKey` | `String` | ✅ | Client **public** API key from the Sense Client panel. Never ship a secret key in the app binary. |
| `senseInfo` | `Bool` | | `true` returns the expanded payload — Request ID, First seen, Last seen, IP address — alongside the Sense ID. `false` returns only Sense ID and Request ID. |
| `allowGeoLocation` | `Bool` | | `true` permits geo-location enrichment, resolved server-side and returned in the Sense details. Requires a location usage description in `Info.plist`. |
| `tag` | `String` | | Free-form label for the call site (`"index"`, `"login"`, `"checkout"`, …). Useful for segmenting events in the dashboard. |
| `sessionId` | `String` | | Optional. Correlates this evaluation with a Workflow session. |
| `metaInfo` | `[String: String]` | | Business context attached to the device record. **Maximum 15 pairs; each key and value capped at 256 characters.** |
| `sensePinningConfig` | `SensePinningConfig` | | Per-host certificate pinning for your own API traffic. See Step 4. |

> Keep `metaInfo` to identifiers you already control (an internal user id, a masked phone). It is transmitted with the device record, so treat it as you would any other outbound PII.

---

### Step 4 — SSL pinning (`SensePinningConfig`)

Pinning binds each host to a set of public-key hashes, so a proxy presenting a user-installed or otherwise rogue CA certificate is rejected even though iOS itself trusts it. This closes the gap that ATS alone leaves open: an attacker who can install a configuration profile on the device can intercept TLS traffic without pinning in place.

```swift
senseConfig.sensePinningConfig = SensePinningConfig(
    pinSets: [
        SensePinSet(
            host: "api.yourdomain.com",
            pins: [
                "sha256/<leaf-spki-hash>",
                "sha256/<intermediate-spki-hash>",
                "sha256/<backup-spki-hash>"
            ]
        )
    ],
    enabled: true
)
```

#### `SensePinningConfig`

| Field | Type | Description |
|---|---|---|
| `pinSets` | `[SensePinSet]` | One entry per host. Hosts not listed are not pinned. |
| `enabled` | `Bool` | Master switch. Set `false` to disable pinning without removing the configuration — useful for staging builds behind a proxy. |

#### `SensePinSet`

| Field | Type | Description |
|---|---|---|
| `host` | `String` | Exact hostname to pin. One `SensePinSet` per host; no wildcards. |
| `pins` | `[String]` | SPKI SHA-256 hashes, each prefixed with `sha256/`. |

#### Computing a pin

```bash
openssl s_client -servername api.yourdomain.com -connect api.yourdomain.com:443 </dev/null \
  | openssl x509 -pubkey -noout \
  | openssl pkey -pubin -outform der \
  | openssl dgst -sha256 -binary \
  | openssl base64
```

The hash is of the **subject public key info**, not the certificate, so it survives a renewal that reuses the same key pair. Note that the base64 output may itself start with `/` — `"sha256//1kYlcCuXHofjS1h…"` in the sample above has two slashes for exactly that reason, and both are correct.

To capture the whole chain at once and pin the intermediate as well:

```bash
openssl s_client -servername api.yourdomain.com -connect api.yourdomain.com:443 -showcerts </dev/null
```

> **Always include at least one backup pin** — typically the intermediate CA plus a spare key you hold offline. Pinning only the current leaf certificate means every renewal bricks the app for every installed user until they update, and there is no server-side fix once the binary is shipped.

#### Interaction with the RASP callbacks

| Situation | Callback |
|---|---|
| A pinned host presents a certificate whose key is not in its pin set | `onSSLPinningBypassDetected(isOn: true)` |
| The pinning layer itself is hooked or patched at runtime | `onSSLPinningBypassDetected(isOn: true)` |
| Plaintext HTTP traffic is observed | `onNonSSLTLSConnectionDetected(isOn: true)` |

Rotate pins by shipping the new hash **before** the certificate changes: release a build carrying both the current and the incoming pin, wait for adoption, then rotate the certificate.

---

### Step 5 — Implement `SenseDelegate`

```swift
extension ViewController: SenseDelegate {
    func onSuccess(data: String) {
        // Success callback — parse the identity payload
    }

    func onFailure(message: String) {
        // Network error, invalid key, or blocked environment
    }
}
```

> **Signature check.** Earlier SDK releases declared `onSuccess(data: [String: Any])`; current builds declare `onSuccess(data: String)` carrying a JSON string. Let Xcode's fix-it confirm which one your installed version expects — if the compiler reports the protocol is unsatisfied, you are on the other signature.

Parsing the JSON-string form:

```swift
func onSuccess(data: String) {
    guard
        let payload = data.data(using: .utf8),
        let json = try? JSONSerialization.jsonObject(with: payload) as? [String: Any]
    else { return }

    let senseId   = json["sense_id"]   as? String
    let requestId = json["request_id"] as? String
    // Forward senseId / requestId to your backend
}
```

| Field | Description |
|---|---|
| `sense_id` | Stable device identifier. |
| `request_id` | Identifier for this specific evaluation. Use it server-side to pull the full risk report. |

With `senseInfo = true` the payload additionally carries first-seen, last-seen and IP address.

> **Trust the server, not the client.** Treat `sense_id` as a lookup key: fetch the authoritative device and threat verdict from the Sense API on your backend using `request_id` rather than acting on client-side values alone. A jailbroken device can lie to your app; it cannot lie to your server.

---

### Step 6 — Fetch device details

```swift
Sense.getSenseDetails(withDelegate: self)
```

The call is asynchronous and resolves through `SenseDelegate`. Call it after `initSDK` has been given a chance to complete its setup — in the sample below it is deferred to `sceneDidBecomeActive`.

---

### Step 7 — Register the RASP delegate

Runtime threat callbacks are delivered to whatever object is assigned to `Sense.senseRASPdegate`.

```swift
Sense.senseRASPdegate = self
```

> The property name is spelled `senseRASPdegate` in the SDK (not `…Delegate`). This is the actual symbol — autocomplete will confirm it.

Assign it early, before the first sensitive screen is reachable. It is a strong reference target, so assign an object that outlives the session — the `SceneDelegate`, the `AppDelegate`, or a dedicated long-lived security coordinator — rather than a view controller that will be popped.

---

### Step 8 — Implement `SenseRASPDelegate`

**Every callback carries an `isOn: Bool`.** `true` means the condition is currently active; `false` means it has cleared. Callbacks are stateful — the same signal fires again when it flips — so drive UI and blocking decisions from the flag rather than assuming a callback means "threat".

```swift
extension SceneDelegate: SenseRASPDelegate {
    func onJailbreakDetected(isOn: Bool) {
        guard isOn else { return }
        // Block the sensitive flow, warn the user, report to your backend
    }
}
```

Callbacks may arrive off the main thread. Hop to the main queue before touching UIKit:

```swift
private func threat(_ label: String, _ isOn: Bool) {
    DispatchQueue.main.async {
        isOn ? ToastManager.show(label) : ToastManager.clear(label)
    }
}
```

#### Callback catalogue

**Device integrity & tampering**

| Callback | Fires when |
|---|---|
| `onJailbreakDetected` | Device is jailbroken (Cydia/Sileo, suspicious paths, write outside the sandbox) |
| `onFridaDetected` | Frida or another dynamic instrumentation framework is attached |
| `onSimulatorDetected` | App is running in the iOS Simulator |
| `onSandboxEnvironment` | App is running inside an analysis sandbox or altered container |
| `onAutomatedInstall` | The app was installed by automated tooling rather than a normal user install |
| `onDevicePolicyEnforcementDetected` | An MDM / device-policy controller is enforcing policy |
| `onSystemTimeManipulationDetected` | The system clock has been manipulated |
| `onTimeZoneMismatch` | Reported timezone conflicts with other device signals |
| `onMockLocationDetected` | Location is being spoofed |
| `onFaceIdChanged` | The enrolled Face ID / Touch ID biometric set changed since the last observation |
| `onBiometricStrengthValidationFailed` | Enrolled biometrics do not meet the required strength class |

**Application & memory integrity**

| Callback | Fires when |
|---|---|
| `onChecksumValidationFailed` | Binary checksum validation failed — the app was modified or re-signed |
| `onMemoryProtectionFailed` | The memory protection guard was triggered |
| `onMemoryEditingDetected` | Live memory editing was observed |
| `onMemoryTamperingDetected` | Process memory was modified |
| `onApplicationDataLeakageDetected` | App data is leaking to an untrusted destination |

**Network & transport**

| Callback | Fires when |
|---|---|
| `onVpnDetected` | A VPN tunnel is active |
| `onSystemVPNDetected` | A system-level VPN profile is configured |
| `onProxyDetected` | An HTTP/S proxy is configured |
| `onSystemProxyDetected` | A system-wide proxy is configured |
| `onNonSSLTLSConnectionDetected` | Plaintext (non-TLS) traffic was observed |
| `onSSLPinningBypassDetected` | A certificate-pinning bypass attempt was detected |
| `onIPBlocklisted` | The device's IP matches a blocklist entry |
| `onNetworkSandboxingDetected` | Traffic is being routed through a sandboxed or analysis network |
| `onSecureSessionSignallingFailed` | Secure-session signalling could not be established or was broken |
| `onWifiDetected` | The device is on Wi-Fi |

**Screen capture, overlay & remote access**

| Callback | Fires when |
|---|---|
| `onScreenRecordingDetected` | The screen is being recorded (`UIScreen.isCaptured`) |
| `onScreenMirroringDetected` | The display is mirrored to an external screen or AirPlay sink |
| `onScreenSharingAppsDetected` | Screen-sharing apps are installed or active |
| `onMaliciousPiPDetected` | A malicious picture-in-picture window is present |
| `onRemoteControlDetected` | A remote-access / remote-control tool is driving the device |

**Input & accessibility abuse**

| Callback | Fires when |
|---|---|
| `onThirdPartyKeyboardDetected` | A non-system custom keyboard extension is active |
| `onUntrustedScreenReaderDetected` | An untrusted accessibility / screen-reader service is running |
| `onKeystrokeInjectionDetected` | Synthetic key events are being injected |
| `onAutoClickerDetected` | An auto-clicker or automation service is active |

**Telephony & call**

| Callback | Fires when |
|---|---|
| `onSimNotFound` | No SIM is present |
| `onActivePhoneCallDetected` | A cellular call is in progress |
| `onVoIPVideoCallDetected` | A VoIP or video call is in progress |

> The signals that matter most for scam and remote-assistance fraud are `onActivePhoneCallDetected`, `onVoIPVideoCallDetected`, `onScreenSharingAppsDetected`, `onScreenRecordingDetected` and `onRemoteControlDetected`. A user on a call while sharing their screen during a payment flow is the classic pattern — consider blocking the screen outright rather than only logging.

---

### Step 9 — Screen shield

`Sense.screenShield(for:)` protects a window's contents against screenshots, screen recording and mirroring. The window must already be attached, so call it once the scene is active.

```swift
func sceneDidBecomeActive(_ scene: UIScene) {
    Sense.senseRASPdegate = self

    DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
        if let window = self.window {
            Sense.screenShield(for: window)
        }
    }
}
```

An equivalent shared-instance form is available in some releases:

```swift
SenseScreenShield.shared.activate(for: window)
```

> The 2-second delay in the sample exists because the window hierarchy is not guaranteed to be laid out the instant the scene becomes active. A fixed delay is a workable starting point but is timing-dependent — prefer hooking the shield to a point where you know the window is attached and key, and note that applying it on every `sceneDidBecomeActive` will re-apply it on each foreground transition.

---

### Step 10 — `Info.plist` configuration

#### Location (optional)

Required only if `allowGeoLocation = true`.

```xml
<key>NSLocationWhenInUseUsageDescription</key>
<string>This app needs access to your location to provide location-based services.</string>
<key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
<string>Location access is required for enhanced app functionality.</string>
```

> Declare each key **once**. A duplicated `NSLocationWhenInUseUsageDescription` is a plist error; App Store review also rejects vague strings, so describe the actual user-facing benefit.

#### Remote-access and meeting app detection (optional)

iOS only lets an app test for another app's URL scheme if that scheme is declared in `LSApplicationQueriesSchemes`. Undeclared schemes silently return "not installed", so `onScreenSharingAppsDetected` and `onRemoteControlDetected` will under-report without these entries.

```xml
<key>LSApplicationQueriesSchemes</key>
<array>
    <!-- Remote access -->
    <string>tvcontrol1</string>   <!-- TeamViewer -->
    <string>anydesk</string>      <!-- AnyDesk -->
    <string>rustdesk</string>     <!-- RustDesk -->

    <!-- Meeting / screen share -->
    <string>zoommtg</string>      <!-- Zoom -->
    <string>msteams</string>      <!-- Microsoft Teams -->
    <string>slack</string>        <!-- Slack -->
    <string>skype</string>        <!-- Skype -->
</array>
```

#### Installed-application checks (optional)

Add any additional schemes you want the SDK to probe to the **same** `LSApplicationQueriesSchemes` array — the key may appear only once in the plist.

```xml
<key>LSApplicationQueriesSchemes</key>
<array>
    <string>tvcontrol1</string>
    <string>anydesk</string>
    <string>rustdesk</string>
    <string>zoommtg</string>
    <string>msteams</string>
    <string>slack</string>
    <string>skype</string>
    <string>whatsapp</string>
    <string>tez</string>
    <string>phonepe</string>
    <string>cred</string>
    <string>amazon</string>
</array>
```

> iOS caps `LSApplicationQueriesSchemes` at **50 entries**. Declare only the schemes your risk model actually uses. App Store review asks why an app queries other apps, so be ready to justify the list in your review notes.

---

### Complete sample — `SceneDelegate`

```swift
import UIKit
internal import Sense

@available(iOS 13.0, *)
class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?

    func scene(_ scene: UIScene,
               willConnectTo session: UISceneSession,
               options connectionOptions: UIScene.ConnectionOptions) {
        // Arm runtime protection as early as the scene allows.
        Sense.senseRASPdegate = self
        initiateSense()
    }

    func sceneDidBecomeActive(_ scene: UIScene) {
        Sense.senseRASPdegate = self

        // Window must be attached before the shield can cover it.
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            if let window = self.window {
                Sense.screenShield(for: window)
            }
        }
    }

    func sceneDidDisconnect(_ scene: UIScene)      {}
    func sceneWillResignActive(_ scene: UIScene)   {}
    func sceneWillEnterForeground(_ scene: UIScene){}
    func sceneDidEnterBackground(_ scene: UIScene) {}

    // MARK: - Sense initialization

    func initiateSense() {
        let senseConfig = SenseConfig()
        senseConfig.apiKey           = "Your Unique Public API Key"
        senseConfig.allowGeoLocation = true
        senseConfig.senseInfo        = false
        senseConfig.tag              = "index"
        senseConfig.metaInfo = [
            "phone": "9987654532",
            "name":  "kumar"
        ]
        senseConfig.sensePinningConfig = SensePinningConfig(
            pinSets: [
                SensePinSet(
                    host: "dummyjson.com",
                    pins: [
                        "sha256//1kYlcCuXHofjS1hAFCOx21ULpPjNxh78MyOZ+EXFOQ=",
                        "sha256/kIdp6NNEd8wsugYyyIYFsi1ylMCED3hZbSR8ZFsa/A4=",
                        "sha256/mEflZT5enoR1FuXLgYYGqnVEoZvmf9c2bVBpiOjYQ0c="
                    ]
                )
            ],
            enabled: true
        )

        Sense.initSDK(senseConfig: senseConfig, withDelegate: self)
        Sense.getSenseDetails(withDelegate: self)
    }
}

// MARK: - Device identity

@available(iOS 13.0, *)
extension SceneDelegate: SenseDelegate {

    func onSuccess(data: String) {
        guard
            let payload = data.data(using: .utf8),
            let json = try? JSONSerialization.jsonObject(with: payload) as? [String: Any]
        else { return }

        let senseId   = json["sense_id"]   as? String ?? ""
        let requestId = json["request_id"] as? String ?? ""
        print("Sense ID: \(senseId), Request ID: \(requestId)")
        // Forward to your backend for the authoritative verdict
    }

    func onFailure(message: String) {
        print("Sense failure: \(message)")
    }
}

// MARK: - Runtime protection

@available(iOS 13.0, *)
extension SceneDelegate: SenseRASPDelegate {

    // Device integrity
    func onJailbreakDetected(isOn: Bool)                   { threat("Jailbreak Detected", isOn) }
    func onFridaDetected(isOn: Bool)                       { threat("Frida Detected", isOn) }
    func onSimulatorDetected(isOn: Bool)                   { threat("Simulator Detected", isOn) }
    func onSandboxEnvironment(isOn: Bool)                  { threat("Sandbox Environment Detected", isOn) }
    func onAutomatedInstall(isOn: Bool)                    { threat("Automated Install Detected", isOn) }
    func onDevicePolicyEnforcementDetected(isOn: Bool)     { threat("Device Policy Enforcement Detected", isOn) }
    func onSystemTimeManipulationDetected(isOn: Bool)      { threat("System Time Manipulation Detected", isOn) }
    func onTimeZoneMismatch(isOn: Bool)                    { threat("Timezone Mismatch Detected", isOn) }
    func onMockLocationDetected(isOn: Bool)                { threat("Mock Location Detected", isOn) }
    func onFaceIdChanged(isOn: Bool)                       { threat("Face ID Changes Detected", isOn) }
    func onBiometricStrengthValidationFailed(isOn: Bool)   { threat("Biometric Strength Validation Failed", isOn) }

    // App & memory integrity
    func onChecksumValidationFailed(isOn: Bool)            { threat("Checksum Validation Failed", isOn) }
    func onMemoryProtectionFailed(isOn: Bool)              { threat("Memory Protection Failed", isOn) }
    func onMemoryEditingDetected(isOn: Bool)               { threat("Memory Editing Detected", isOn) }
    func onMemoryTamperingDetected(isOn: Bool)             { threat("Memory Tampering Detected", isOn) }
    func onApplicationDataLeakageDetected(isOn: Bool)      { threat("Application Data Leakage Detected", isOn) }

    // Network
    func onVpnDetected(isOn: Bool)                         { threat("VPN Detected", isOn) }
    func onSystemVPNDetected(isOn: Bool)                   { threat("System VPN Detected", isOn) }
    func onProxyDetected(isOn: Bool)                       { threat("Proxy Detected", isOn) }
    func onSystemProxyDetected(isOn: Bool)                 { threat("System Proxy Detected", isOn) }
    func onNonSSLTLSConnectionDetected(isOn: Bool)         { threat("Non-SSL/TLS Connection Detected", isOn) }
    func onSSLPinningBypassDetected(isOn: Bool)            { threat("SSL Pinning Bypass Detected", isOn) }
    func onIPBlocklisted(isOn: Bool)                       { threat("IP Blocklisted", isOn) }
    func onNetworkSandboxingDetected(isOn: Bool)           { threat("Network Sandboxing Detected", isOn) }
    func onSecureSessionSignallingFailed(isOn: Bool)       { threat("Secure Session Signalling Failed", isOn) }
    func onWifiDetected(isOn: Bool)                        { threat("WiFi Detected", isOn) }

    // Screen capture & remote access
    func onScreenRecordingDetected(isOn: Bool)             { threat("Screen Recording Detected", isOn) }
    func onScreenMirroringDetected(isOn: Bool)             { threat("Screen Mirroring Detected", isOn) }
    func onScreenSharingAppsDetected(isOn: Bool)           { threat("Screen Sharing Apps Detected", isOn) }
    func onMaliciousPiPDetected(isOn: Bool)                { threat("Malicious PiP Detected", isOn) }
    func onRemoteControlDetected(isOn: Bool)               { threat("Remote Control Detected", isOn) }

    // Input & accessibility
    func onThirdPartyKeyboardDetected(isOn: Bool)          { threat("Third-Party Keyboard Detected", isOn) }
    func onUntrustedScreenReaderDetected(isOn: Bool)       { threat("Untrusted Screen Reader Detected", isOn) }
    func onKeystrokeInjectionDetected(isOn: Bool)          { threat("Keystroke Injection Detected", isOn) }
    func onAutoClickerDetected(isOn: Bool)                 { threat("Auto-Clicker Detected", isOn) }

    // Telephony
    func onSimNotFound(isOn: Bool)                         { threat("Sim Not Detected", isOn) }
    func onActivePhoneCallDetected(isOn: Bool)             { threat("Active Phone Call Detected", isOn) }
    func onVoIPVideoCallDetected(isOn: Bool)               { threat("VoIP/Video Call Detected", isOn) }

    // MARK: - Shared handler

    private func threat(_ label: String, _ isOn: Bool) {
        DispatchQueue.main.async {
            guard isOn else { return }
            ToastManager.show(label)
            // Production: block the sensitive flow and report to your backend
        }
    }
}
```

---

### Appendix — sample threat presenter

A minimal de-duplicating, queued toast used by the sample above. This is demo scaffolding, not part of the SDK — in production, drive a blocking UI from the `isOn` state instead of a transient toast.

```swift
final class ToastManager {

    static let shared = ToastManager()
    private init() {}

    private var queue: [(message: String, duration: TimeInterval)] = []
    private var isShowing = false
    private var lastMessage: String?
    private weak var currentToast: UIView?
    private var isCleared = false

    // MARK: - Public

    static func show(_ message: String, duration: TimeInterval = 2.0) {
        shared.enqueue(message, duration: duration)
    }

    func clearAll() {
        DispatchQueue.main.async {
            self.isCleared = true
            self.queue.removeAll()
            self.currentToast?.layer.removeAllAnimations()
            self.currentToast?.removeFromSuperview()
            self.currentToast = nil
            self.isShowing = false
        }
    }

    // MARK: - Queue

    private func enqueue(_ message: String, duration: TimeInterval) {
        DispatchQueue.main.async {
            guard self.lastMessage != message else { return }   // de-dupe repeats
            self.lastMessage = message
            self.queue.append((message, duration))
            self.showNextIfNeeded()
        }
    }

    private func showNextIfNeeded() {
        guard !isShowing, !queue.isEmpty else { return }
        isShowing = true
        isCleared = false
        let item = queue.removeFirst()
        showToast(message: item.message, duration: item.duration)
    }

    // MARK: - UI

    private func showToast(message: String, duration: TimeInterval) {
        guard let window = Self.keyWindow() else {
            isShowing = false
            return
        }

        let label = PaddingLabel()
        label.text            = message
        label.textColor       = .white
        label.textAlignment   = .center
        label.numberOfLines   = 0
        label.font            = .systemFont(ofSize: 15, weight: .medium)
        label.backgroundColor = UIColor.black.withAlphaComponent(0.85)
        label.layer.cornerRadius = 14
        label.clipsToBounds   = true
        label.alpha           = 0

        let maxWidth = window.frame.width * 0.8
        let size = label.sizeThatFits(CGSize(width: maxWidth, height: .greatestFiniteMagnitude))
        label.frame = CGRect(
            x: (window.frame.width  - size.width  - 24) / 2,
            y: (window.frame.height - size.height - 24) / 2,
            width:  size.width  + 24,
            height: size.height + 24
        )

        window.addSubview(label)
        currentToast = label

        UIView.animate(withDuration: 0.25) {
            label.alpha = 1
        } completion: { _ in
            UIView.animate(withDuration: 0.25, delay: duration, options: .curveEaseOut) {
                label.alpha = 0
            } completion: { _ in
                if self.isCleared { self.isShowing = false; return }
                label.removeFromSuperview()
                self.currentToast = nil
                self.isShowing    = false
                self.lastMessage  = nil
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
                    self.showNextIfNeeded()
                }
            }
        }
    }

    // MARK: - Helper

    private static func keyWindow() -> UIWindow? {
        UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .flatMap { $0.windows }
            .first { $0.isKeyWindow }
    }
}
```

> **Two caveats on this sample.** `lastMessage` de-duplicates only against the *immediately preceding* message, so alternating signals still queue up. And because it ignores `isOn`, it shows a toast when a condition clears as well as when it fires — production code should branch on the flag.

---

### Integration checklist

- [ ] `Sense.initSDK` runs at scene/app launch, before any sensitive screen is reachable
- [ ] `Sense.senseRASPdegate` is assigned to a long-lived object (scene delegate, app delegate, security coordinator)
- [ ] The public API key is used — never a secret key
- [ ] `metaInfo` stays within 15 pairs / 256 characters per key and value
- [ ] Every pinned host has at least one backup pin, and `enabled = true` in release builds
- [ ] `LSApplicationQueriesSchemes` is declared **once** and lists every scheme you want probed (≤ 50)
- [ ] Location usage-description keys appear once each, with user-facing wording
- [ ] RASP callbacks hop to the main queue before touching UIKit
- [ ] `Sense.screenShield(for:)` is applied after the window is attached and key
- [ ] Threat verdicts are confirmed server-side using `request_id`
- [ ] SDK logging / `print` diagnostics removed from release builds

---

### Troubleshooting

| Symptom | Likely cause |
|---|---|
| `import Sense` does not resolve | Module name differs from the pod name — try `import SenseSDK`, and run `pod install` after editing the Podfile |
| Protocol conformance error on `onSuccess` | Your SDK version uses the other signature — `data: String` vs `data: [String: Any]` |
| No RASP callbacks ever fire | `Sense.senseRASPdegate` was never assigned, or was assigned to an object that has since been deallocated |
| `onScreenSharingAppsDetected` / `onRemoteControlDetected` never fire | The relevant URL schemes are missing from `LSApplicationQueriesSchemes` |
| Geo-location absent from the payload | `allowGeoLocation` is `false`, the usage-description key is missing, or the user denied the prompt |
| Screen shield has no effect | Called before the window was attached — defer it until the scene is active and the window is key |
| `getSenseDetails` returns `onFailure` | Invalid key, no network, or a proxy/pinning failure on the Sense endpoint |
| All network calls to a host fail after enabling pinning | A pin is wrong or the certificate rotated — re-derive with `openssl s_client` and add backup pins |
| `onSSLPinningBypassDetected` fires on a corporate network | A TLS-inspecting proxy is in the path; that is a true positive, not a bug |
| Pinning appears to do nothing | The host is absent from `pinSets`, or `enabled` is `false` — unlisted hosts are not pinned |
| Crash inside a RASP callback | UIKit touched off the main thread — wrap the body in `DispatchQueue.main.async` |

---

### Support

Documentation and contact: [www.gosense.ai](https://www.gosense.ai/)