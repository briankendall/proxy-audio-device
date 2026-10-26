#include "ProxyAudioDevice.h"
#include "AudioRingBuffer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

static void testLatencyGuard(Float64 sampleRate, UInt32 inputFrames, UInt32 outputFrames) {
    ProxyAudioDevice device;
    AudioRingBuffer buffer(8, 88200);
    device.inputBuffer = &buffer;
    device.lastInputFrameTime = 100000;
    device.lastInputBufferFrameSize = inputFrames;
    const UInt32 safetyOffset = 32;
    const Float64 targetFill = 2.0 * inputFrames + outputFrames + safetyOffset;
    const Float64 threshold = std::max(0.050 * sampleRate, 2.0 * inputFrames);
    const Float64 lag = threshold + 1;

    auto check = [&](Float64 excess, bool fresh = true) {
        if (fresh) {
            device.lastInputFrameTime += inputFrames;
        }
        buffer.mEndFrame = device.lastInputFrameTime + inputFrames;
        return device.hasSustainedExcessLatency(
            buffer.mEndFrame - targetFill - excess, sampleRate, outputFrames, safetyOffset);
    };

    // Normal callback jitter, including the threshold boundary, must not trigger recovery.
    for (int i = 0; i < 1000; ++i) {
        assert(!check(i % 2 ? inputFrames : threshold));
    }
    for (int i = 0; i <= 100; ++i) {
        assert(!check(threshold * i / 100.0));
    }

    assert(!check(lag));
    for (int i = 0; i < 1000; ++i) {
        assert(!check(lag, false));
    }
    assert(device.excessLatencyFrames == inputFrames);
    assert(!check(0));
    assert(device.excessLatencyFrames == 0);

    // A single callback after a long gap must not count the gap as sustained latency.
    device.lastInputFrameTime += sampleRate * 10;
    assert(!check(lag));
    assert(device.excessLatencyFrames == inputFrames);
    assert(!check(0));

    unsigned cycles = 0;
    bool recovered = false;
    do {
        recovered = check(lag);
        ++cycles;
        assert(recovered == (cycles * inputFrames >= 0.250 * sampleRate));
        if (!recovered) {
            const Float64 accumulated = device.excessLatencyFrames;
            for (int i = 0; i < 4; ++i) {
                assert(!check(lag, false));
            }
            assert(device.excessLatencyFrames == accumulated);
        }
    } while (!recovered);

    // Re-anchoring resets both the detector and the servo, without clearing audio.
    AudioTimeStamp timestamp = {};
    timestamp.mSampleTime = 50000;
    device.phaseErrorIntegral = 1234;
    const SInt64 endFrame = buffer.mEndFrame;
    Float64 startFrame = device.anchorInputOutputSampleDelta(&timestamp, outputFrames, safetyOffset);
    assert(buffer.mEndFrame == endFrame);
    assert(buffer.mEndFrame - startFrame == targetFill);
    assert(device.excessLatencyFrames == 0);
    assert(device.phaseErrorIntegral == 0);
    assert(!check(0));

    // A backward input discontinuity starts a new observation period.
    assert(!check(lag));
    device.lastInputFrameTime -= sampleRate;
    assert(!check(lag, false));
    assert(device.excessLatencyFrames == 0);

    device.inputFinalFrameTime = buffer.mEndFrame;
    for (int i = 0; i < 1000; ++i) {
        assert(!check(lag));
    }
    assert(device.excessLatencyFrames == 0);
    device.resetInputData();
    assert(device.lastLatencyCheckInputFrameTime == -1);
    assert(device.excessLatencyFrames == 0);
}

static void testOutputRecovery(bool draining) {
    const UInt32 frames = 256;
    const Float64 sampleRate = 48000;
    const SInt64 backlog = 6000;
    ProxyAudioDevice device;
    AudioRingBuffer buffer(8, 88200);
    std::vector<Float32> samples(88200 * 2, 0.5f);
    std::vector<Byte> work(frames * 8);
    std::vector<Float32> output(frames * 2);
    device.inputBuffer = &buffer;
    device.workBuffer = work.data();
    device.gDevice_SampleRate = sampleRate;
    device.outputDevice.sampleRate = sampleRate;
    device.outputDevice.bufferFrameSize = frames;
    device.gVolume_Output_L_Value = 1;
    device.gVolume_Output_R_Value = 1;

    const SInt64 firstEnd = 20000;
    assert(buffer.Store((const Byte *)samples.data(), firstEnd, 1));
    device.lastInputFrameTime = buffer.mEndFrame - frames;
    device.lastInputBufferFrameSize = frames;
    device.inputOutputSampleDeltaValid = true;
    device.inputOutputSampleDelta = buffer.mEndFrame - 3 * frames - backlog;
    device.lastLatencyCheckInputFrameTime = device.lastInputFrameTime;
    if (draining) {
        device.inputFinalFrameTime = 100000;
    }
    const Float64 originalDelta = device.inputOutputSampleDelta;
    AudioBufferList outputData = {};
    outputData.mNumberBuffers = 1;
    outputData.mBuffers[0] = {2, frames * 8, output.data()};
    AudioTimeStamp timestamp = {};

    for (unsigned i = 0; i < 60; ++i) {
        const SInt64 end = buffer.mEndFrame;
        assert(buffer.Store((const Byte *)samples.data(), frames, end));
        device.lastInputFrameTime = end;
        timestamp.mSampleTime = i * frames;
        std::fill(output.begin(), output.end(), 0);
        assert(device.outputDeviceIOProc(0, nullptr, nullptr, nullptr, &outputData, &timestamp) == noErr);
        assert(output.front() == 0.5f && output.back() == 0.5f);
    }

    if (draining) {
        assert(device.inputOutputSampleDelta == originalDelta);
    } else {
        assert(device.inputOutputSampleDelta == originalDelta + backlog + frames);
        assert(device.excessLatencyFrames == 0);
    }
}

int main() {
    for (Float64 rate : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        for (UInt32 input : {128u, 512u, 4096u}) {
            for (UInt32 output : {128u, 1024u}) {
                testLatencyGuard(rate, input, output);
            }
        }
    }
    testOutputRecovery(false);
    testOutputRecovery(true);
    std::puts("Audio latency regression tests passed");
}
