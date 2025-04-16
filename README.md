# Sense - iOS SDK 

Sense is a device intelligence and identification tool. This tool collects a comprehensive set of attributes unique to a device or browser, forming an identity that will help businesses.

## Requirements
    * iOS 12.0 or above
    * Swift version 5.0 and above

### Step 1 - Installation
```
pod 'SenseSDK', '~> 0.0.2'
pod update
```
### Step 2 - Import SDK
```
import SenseSDK
```
To initialize the SDK add the below line of code with the public key you retrieved from Sense client panel. If you don't have a public key create new one and call that below function.

```
    func initiateSense(){
        let senseConfig = SenseConfig()
        senseConfig.apiKey = "Your Unique Public API Key"
        senseConfig.senseInfo = false // true or false
        senseConfig.allowGeoLocation = false // true or false
        senseConfig.tag = "" // String whatever you need like Home
        senseConfig.sessionId = "" //Optional used for Workflow
        Sense.initSDK(senseConfig: senseConfig, withDelegate: self)
    }

```
#### Sense Config Information

If you pass senseInfo as true in above config means, it will return addtional Information like Request ID, First seen, Last seen, IP Address with SenseID. Otherwise it will return only SenseID and Request .

If you pass allowGeoLocation as true in above config means, it will return Device Location Information in Sense Details via our Server API.


### Step 3 - Implement  Delegate Method
Use the below code to obtain the Device result.
```
extension ViewController: SenseDelegate{
    func onFailure(message: String) {
        // Failure Callback.
    }
    func onSuccess(data: [String : Any]) {
        // Success Callback
    }
}
```
### Step 4 - Get Device Info
By call the following delegate to receive the device info shown below.
```
 Sense.getSenseDetails(withDelegate: self)
```
### Step 5 - Location Permission (optional)
You have to add this permission in Info.plist to get Device Location Information.

```
<key>NSLocationWhenInUseUsageDescription</key>
<string>This app needs access to your location to provide location-based services.</string>
<key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
<string>Location access is required for enhanced app functionality.</string>
<key>NSLocationWhenInUseUsageDescription</key>
<string>Require to get user location</string>
```

### Step 6 - If you need to know Installed Applications in our SDK, kindly update your Info.plist like below. You can add those apps that you need to check whether it's installed or not.

```
<key>LSApplicationQueriesSchemes</key>
<array>
    <string>whatsapp</string>
    <string>tez</string>
    <string>phonepe</string>
    <string>cred</string>
    <string>amazon</string>
</array>

```

## To get Behaviour Data for Keystrokes, Mouse Movements, Scroll Metrics, Touch Metrics, you can add the below methods to initialize it.

### For Keystrokes, pass your textfields as arguments like below

```
@IBOutlet weak var txtUsername: UITextField
@IBOutlet weak var txtPassword: UITextField

Sense.initKeyStrokeBehaviour(for: [txtUsername, txtPassword]);
```

### For Scroll Metrics

```
@IBOutlet weak var scrollView: UIScrollView

Sense.initScrollBehaviour(for: [scrollView]);
```

### For Touch Metrics

```
Sense.initTouchBehaviour(for: self.view)
```

### To get Behaviour Data, Call below method to get it

```
Sense.getBehaviourData(withDelegate: self)

extension ViewController: SenseBehaviourDelegate{
    func onFailureBehaviour(message: String) {
        print("Failure data")
    }

    func onSuccessBehaviour(data: String) {
        print("Success data")
    }
}
```
