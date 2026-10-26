#include "ProxyAudioDevice.h"
#include "CAHostTimeBase.h"

#include <cassert>
#include <cstdio>

static void testOutputWatchdog() {
    ProxyAudioDevice device;
    // Fake device ID is used only by the pure watchdog predicate; no Core Audio calls are made.
    device.outputDevice.id = 123;
    device.outputDeviceReady = true;
    device.outputDevice.isStarted = true;
    device.inputIOIsActive = true;
    UInt64 now = CAHostTimeBase::ConvertFromNanos(100 * NSEC_PER_SEC);
    const UInt64 step = CAHostTimeBase::ConvertFromNanos(NSEC_PER_SEC / 2);
    auto poll = [&](bool input, bool output) {
        now += step;
        if (input) ++device.inputCallbackCount;
        if (output) ++device.outputCallbackCount;
        return device.shouldReconnectStalledOutput(now);
    };

    for (int i = 0; i < 10; ++i) assert(!poll(true, true));
    // Brief callback gaps and input callbacks less frequent than the timer are tolerated.
    assert(!poll(true, false));
    assert(!poll(false, false));
    assert(!poll(true, true));
    assert(!poll(true, false));
    assert(!poll(true, false));
    assert(!poll(true, false));
    assert(!poll(true, false));
    assert(poll(true, false)); // Exactly two seconds after the first stalled observation.
    for (int i = 0; i < 19; ++i) assert(!poll(true, false));
    assert(poll(true, false)); // Reconnects are rate limited to ten seconds.

    // Recovery callbacks immediately clear the stalled observation.
    assert(!poll(true, true));
    assert(device.stalledOutputSince == 0);
    now += CAHostTimeBase::ConvertFromNanos(20 * NSEC_PER_SEC);
    assert(!poll(false, false));
    assert(device.stalledOutputSince == 0);

    // A single input callback followed by a stall must not cause a reconnect.
    assert(!poll(true, false));
    for (int i = 0; i < 10; ++i) assert(!poll(false, false));
    assert(device.stalledOutputSince == 0);

    // Sustained sparse input (one callback per second) still allows recovery.
    assert(!poll(true, false));
    assert(!poll(false, false));
    assert(!poll(true, false));
    assert(!poll(false, false));
    assert(poll(true, false));
    assert(!poll(true, true));

    // Stopped, unavailable, and unready devices are not forcibly restarted.
    for (int state = 0; state < 3; ++state) {
        device.inputIOIsActive = state != 0;
        device.outputDeviceReady = state != 1;
        device.outputDevice.id = state == 2 ? kAudioObjectUnknown : 123;
        for (int i = 0; i < 30; ++i) assert(!poll(true, false));
        assert(device.stalledOutputSince == 0);
    }

    // A failed AudioDeviceStart does not permanently disable recovery.
    device.inputIOIsActive = true;
    device.outputDeviceReady = true;
    device.outputDevice.id = 123;
    device.outputDevice.isStarted = false;
    assert(!poll(true, false));
    for (int i = 0; i < 3; ++i) assert(!poll(true, false));
    assert(poll(true, false));
}

static void testOutputHeartbeat() {
    ProxyAudioDevice device;
    AudioBufferList output = {};
    AudioTimeStamp timestamp = {};
    device.outputDevice.sampleRate = 48000;
    for (unsigned i = 0; i < 10; ++i) {
        assert(device.outputDeviceIOProc(0, nullptr, nullptr, nullptr, &output, &timestamp) == noErr);
        assert(device.outputCallbackCount.load() == i + 1);
    }
}

int main() {
    testOutputWatchdog();
    testOutputHeartbeat();
    std::puts("Audio wake recovery regression tests passed");
}
