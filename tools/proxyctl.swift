// proxyctl: configure the Proxy Audio Device driver from the command line, using the same
// channel as its Settings app (the driver's "box" object).
//   proxyctl get                      show all settings
//   proxyctl set <key> <value>        keys: outputDevice (a device UID), outputDeviceBufferFrameSize,
//                                     deviceName, outputDeviceActiveCondition (0 = while playing,
//                                     1 = while the user is active, 2 = always),
//                                     outputDeviceHideWhenUnavailable (0/1)
//   proxyctl devices                  list output devices and their UIDs
//   proxyctl make-default             make the proxy device the Mac's default output
import CoreAudio
import Foundation

let system = AudioObjectID(kAudioObjectSystemObject)
func addr(_ s: AudioObjectPropertySelector, _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal) -> AudioObjectPropertyAddress {
    AudioObjectPropertyAddress(mSelector: s, mScope: scope, mElement: kAudioObjectPropertyElementMain)
}
func string(_ id: AudioObjectID, _ s: AudioObjectPropertySelector) -> String? {
    var a = addr(s); var v: Unmanaged<CFString>?; var size = UInt32(MemoryLayout<Unmanaged<CFString>?>.size)
    guard AudioObjectGetPropertyData(id, &a, 0, nil, &size, &v) == noErr, let v else { return nil }
    return v.takeRetainedValue() as String
}
func box() -> AudioObjectID {
    var a = addr(kAudioHardwarePropertyTranslateUIDToBox)
    var uid: CFString = "ProxyAudioBox_UID" as CFString
    var id = AudioObjectID(0); var size = UInt32(MemoryLayout<AudioObjectID>.size)
    let status = withUnsafeMutablePointer(to: &uid) {
        AudioObjectGetPropertyData(system, &a, UInt32(MemoryLayout<CFString>.size), $0, &size, &id)
    }
    return status == noErr ? id : 0
}
func identify(_ box: AudioObjectID, _ value: Int32) {
    var a = addr(kAudioObjectPropertyIdentify); var v = value
    AudioObjectSetPropertyData(box, &a, 0, nil, UInt32(MemoryLayout<Int32>.size), &v)
}
func setName(_ box: AudioObjectID, _ value: String) -> OSStatus {
    var a = addr(kAudioObjectPropertyName); var v: CFString = value as CFString
    return withUnsafeMutablePointer(to: &v) { AudioObjectSetPropertyData(box, &a, 0, nil, UInt32(MemoryLayout<CFString>.size), $0) }
}
let keys = ["outputDevice": 1, "outputDeviceBufferFrameSize": 2, "deviceName": 3, "outputDeviceActiveCondition": 4, "outputDeviceHideWhenUnavailable": 5]
let args = CommandLine.arguments
if args.count >= 2, args[1] == "devices" {
    var a = addr(kAudioHardwarePropertyDevices); var size: UInt32 = 0
    AudioObjectGetPropertyDataSize(system, &a, 0, nil, &size)
    var ids = [AudioDeviceID](repeating: 0, count: Int(size) / 4)
    AudioObjectGetPropertyData(system, &a, 0, nil, &size, &ids)
    for id in ids {
        var sa = addr(kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput); var ssize: UInt32 = 0
        AudioObjectGetPropertyDataSize(id, &sa, 0, nil, &ssize)
        if ssize > 0 { print("\(string(id, kAudioObjectPropertyName) ?? "?")\t\(string(id, kAudioDevicePropertyDeviceUID) ?? "?")") }
    }
    exit(0)
}
if args.count >= 2, args[1] == "make-default" {
    // Make the proxy device the Mac's default output.
    var ta = addr(kAudioHardwarePropertyTranslateUIDToDevice)
    var uid: CFString = "ProxyAudioDevice_UID" as CFString
    var dev = AudioDeviceID(0); var size = UInt32(4)
    _ = withUnsafeMutablePointer(to: &uid) { AudioObjectGetPropertyData(system, &ta, UInt32(MemoryLayout<CFString>.size), $0, &size, &dev) }
    guard dev != 0 else { print("proxy device not found"); exit(1) }
    var da = addr(kAudioHardwarePropertyDefaultOutputDevice)
    let status = AudioObjectSetPropertyData(system, &da, 0, nil, 4, &dev)
    print(status == noErr ? "default output is now \(string(dev, kAudioObjectPropertyName) ?? "the proxy device")" : "failed (\(status))")
    exit(status == noErr ? 0 : 1)
}
let b = box()
guard b != 0 else { print("Proxy Audio Device driver not found"); exit(1) }
identify(b, Int32(getpid()))
if args.count == 4, args[1] == "set", keys[args[2]] != nil {
    let status = setName(b, "\(args[2])=\(args[3])")
    print(status == noErr ? "set \(args[2]) = \(args[3])" : "failed (\(status))")
} else {
    for (key, type) in keys.sorted(by: { $0.value < $1.value }) {
        identify(b, Int32(-type))
        print("\(key) = \(string(b, kAudioObjectPropertyName) ?? "?")")
    }
}
