// latency-test: measures the delay added by the Proxy Audio Device driver.
//
//   swiftc -O -swift-version 5 latency-test.swift -o latency-test
//   latency-test <output device name contains> <loopback device name contains> [clicks] [client buffer frames] [--muted]
//
// It plays single-sample clicks of exactly 0.9 at known device times and prints, for each one,
// the host time and sample time the output device said it would present it at ("played:" lines).
//
// The driver recognises that sample value and logs when each click was written to it and when
// the target device presents it:
//
//   /usr/bin/log show --last 1m --predicate 'eventMessage CONTAINS "ProxyAudio: click"'
//
// "click presented: ... host H" minus the "played: host" value for the same sample label is the
// delay the proxy adds, in host ticks. With --muted the proxy device is muted for the test; the
// driver sees the audio before its volume control, so the measurement still works, silently.
//
// If the output device's sound ends up on a loopback device (e.g. the proxy targeting BlackHole),
// the tool also looks for the click in the loopback recording. Note that timestamps on a loopback
// device's input side are offset by the client buffer size, so prefer the driver's trace.
import CoreAudio
import Foundation

let system = AudioObjectID(kAudioObjectSystemObject)
func addr(_ s: AudioObjectPropertySelector, _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal, _ el: AudioObjectPropertyElement = kAudioObjectPropertyElementMain) -> AudioObjectPropertyAddress {
    AudioObjectPropertyAddress(mSelector: s, mScope: scope, mElement: el)
}
func string(_ id: AudioObjectID, _ s: AudioObjectPropertySelector) -> String? {
    var a = addr(s); var v: Unmanaged<CFString>?; var size = UInt32(MemoryLayout<Unmanaged<CFString>?>.size)
    guard AudioObjectGetPropertyData(id, &a, 0, nil, &size, &v) == noErr, let v else { return nil }
    return v.takeRetainedValue() as String
}
func u32(_ id: AudioObjectID, _ s: AudioObjectPropertySelector, _ scope: AudioObjectPropertyScope) -> UInt32 {
    var a = addr(s, scope); var v: UInt32 = 0; var size: UInt32 = 4
    AudioObjectGetPropertyData(id, &a, 0, nil, &size, &v); return v
}
func device(named part: String) -> AudioDeviceID? {
    var a = addr(kAudioHardwarePropertyDevices); var size: UInt32 = 0
    AudioObjectGetPropertyDataSize(system, &a, 0, nil, &size)
    var ids = [AudioDeviceID](repeating: 0, count: Int(size) / 4)
    AudioObjectGetPropertyData(system, &a, 0, nil, &size, &ids)
    return ids.first { (string($0, kAudioObjectPropertyName) ?? "").localizedCaseInsensitiveContains(part) }
}
func setVolume(_ id: AudioDeviceID, _ v: Float32) -> [Float32] {
    var old: [Float32] = []
    for el in [UInt32(0), 1, 2] {
        var a = addr(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, el)
        guard AudioObjectHasProperty(id, &a) else { old.append(-1); continue }
        var cur: Float32 = 0; var size: UInt32 = 4
        AudioObjectGetPropertyData(id, &a, 0, nil, &size, &cur); old.append(cur)
        var nv = v; AudioObjectSetPropertyData(id, &a, 0, nil, 4, &nv)
    }
    return old
}
func restoreVolume(_ id: AudioDeviceID, _ old: [Float32]) {
    for (i, el) in [UInt32(0), 1, 2].enumerated() where old[i] >= 0 {
        var a = addr(kAudioDevicePropertyVolumeScalar, kAudioObjectPropertyScopeOutput, el); var v = old[i]
        AudioObjectSetPropertyData(id, &a, 0, nil, 4, &v)
    }
}

let args = CommandLine.arguments
guard args.count >= 3, let out = device(named: args[1]), let loop = device(named: args[2]) else {
    print("usage: latency-test <output name> <loopback name> [clicks] [client buffer frames]"); exit(1)
}
let clicks = args.count > 3 ? Int(args[3]) ?? 20 : 20
Thread.sleep(forTimeInterval: 0)
if args.count > 4, var frames = UInt32(args[4]) {
    var a = addr(kAudioDevicePropertyBufferFrameSize); AudioObjectSetPropertyData(out, &a, 0, nil, 4, &frames)
}
var tb = mach_timebase_info_data_t(); mach_timebase_info(&tb)
let ticksPerSecond = 1e9 * Double(tb.denom) / Double(tb.numer)
var rate: Float64 = 48000
do { var a = addr(kAudioDevicePropertyNominalSampleRate); var size = UInt32(8); AudioObjectGetPropertyData(loop, &a, 0, nil, &size, &rate) }
let ticksPerFrame = ticksPerSecond / rate

nonisolated(unsafe) var clickRequested = false
nonisolated(unsafe) var playHost: UInt64 = 0
nonisolated(unsafe) var recHost: UInt64 = 0
nonisolated(unsafe) var armed = false
nonisolated(unsafe) var inputPeak: Float = 0
nonisolated(unsafe) var inCycles = 0
nonisolated(unsafe) var playSample: Float64 = 0
nonisolated(unsafe) var recSample: Float64 = 0
nonisolated(unsafe) var dbg: [Float64] = [0, 0, 0, 0, 0, 0]

var outProc: AudioDeviceIOProcID?
AudioDeviceCreateIOProcIDWithBlock(&outProc, out, nil) { now, _, _, output, outTime in
    dbg[0] = now.pointee.mSampleTime; dbg[1] = outTime.pointee.mSampleTime; dbg[2] = Double(Int64(bitPattern: outTime.pointee.mHostTime &- now.pointee.mHostTime)); dbg[5] = Double(Int64(bitPattern: mach_absolute_time() &- now.pointee.mHostTime))
    let bufs = UnsafeMutableAudioBufferListPointer(output)
    for b in bufs { if let d = b.mData { memset(d, 0, Int(b.mDataByteSize)) } }
    if clickRequested {
        for b in bufs {
            guard let d = b.mData else { continue }
            let p = d.assumingMemoryBound(to: Float32.self)
            for ch in 0..<Int(b.mNumberChannels) { p[ch] = 0.9 }
        }
        playHost = outTime.pointee.mHostTime
        playSample = outTime.pointee.mSampleTime
        clickRequested = false
        armed = true
    }
}
var inProc: AudioDeviceIOProcID?
AudioDeviceCreateIOProcIDWithBlock(&inProc, loop, nil) { now, input, inTime, _, _ in
    dbg[3] = inTime.pointee.mSampleTime - now.pointee.mSampleTime; dbg[4] = Double(Int64(bitPattern: inTime.pointee.mHostTime &- now.pointee.mHostTime))
    inCycles += 1
    let bufs = UnsafeMutableAudioBufferListPointer(UnsafeMutablePointer(mutating: input))
    guard let b = bufs.first, let d = b.mData else { return }
    let ch = Int(max(b.mNumberChannels, 1))
    let n = Int(b.mDataByteSize) / (4 * ch)
    let p = d.assumingMemoryBound(to: Float32.self)
    for i in 0..<n {
        let v = abs(p[i * ch])
        if v > inputPeak { inputPeak = v }
        if armed, recHost == 0, v > 0.0005 {
            recHost = inTime.pointee.mHostTime &+ UInt64(Double(i) * ticksPerFrame)
            recSample = inTime.pointee.mSampleTime + Double(i)
        }
    }
}
let outName = string(out, kAudioObjectPropertyName) ?? "?"
print("output: \(outName)  buffer \(u32(out, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal)) frames, reported latency \(u32(out, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeOutput)), safety offset \(u32(out, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeOutput));  loopback: \(string(loop, kAudioObjectPropertyName) ?? "?") at \(Int(rate)) Hz")
// --muted: mute the output device for the test instead of raising its volume. The driver's click
// trace sees the audio before its volume control, so the measurement still works, silently.
let muted = CommandLine.arguments.contains("--muted")
func setMute(_ id: AudioDeviceID, _ on: UInt32) -> UInt32 {
    var a = addr(kAudioDevicePropertyMute, kAudioObjectPropertyScopeOutput); var cur: UInt32 = 0; var size: UInt32 = 4
    AudioObjectGetPropertyData(id, &a, 0, nil, &size, &cur)
    var v = on; AudioObjectSetPropertyData(id, &a, 0, nil, 4, &v)
    return cur
}
var oldMute: UInt32 = 0
var oldOut: [Float32] = []
var oldLoop: [Float32] = []
if muted { oldMute = setMute(out, 1) } else {
    oldOut = setVolume(out, 1.0)
    oldLoop = out == loop ? [] : setVolume(loop, 1.0)
}
AudioDeviceStart(loop, inProc); AudioDeviceStart(out, outProc)
Thread.sleep(forTimeInterval: 2.0)
var results: [Double] = []
for n in 1...clicks {
    recHost = 0; armed = false; clickRequested = true
    Thread.sleep(forTimeInterval: 0.35 + Double(n % 7) * 0.013)   // vary the phase between clicks
    if recHost != 0 {
        let ms = Double(Int64(bitPattern: recHost &- playHost)) / ticksPerSecond * 1000
        results.append(ms)
        print("  played: host \(playHost) sample \(Int(playSample))")
        print(String(format: "click %2d: %+.3f ms   [samples rec-play %+.0f | out cycle: out-now %+.0f samples, %+.1f frames by host time, woke %.1f frames after now | in cycle: in-now %+.0f samples, %+.1f frames by host time]", n, ms, recSample - playSample, dbg[1] - dbg[0], dbg[2] / ticksPerFrame, dbg[5] / ticksPerFrame, dbg[3], dbg[4] / ticksPerFrame))
    } else {
        print("  played: host \(playHost) sample \(Int(playSample))")
        print("click \(n): not detected (input peak so far \(inputPeak), input cycles \(inCycles))")
    }
}
AudioDeviceStop(out, outProc); AudioDeviceStop(loop, inProc)
if muted { _ = setMute(out, oldMute) } else { restoreVolume(out, oldOut); if out != loop { restoreVolume(loop, oldLoop) } }
if !results.isEmpty {
    let sorted = results.sorted()
    print(String(format: "RESULT %@: median %+.3f ms, min %+.3f, max %+.3f, detected %d/%d", outName, sorted[sorted.count / 2], sorted.first!, sorted.last!, results.count, clicks))
}
