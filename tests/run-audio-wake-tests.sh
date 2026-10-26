#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."

binary="$(mktemp -t proxy-audio-wake-tests)"
trap 'rm -f "$binary"' EXIT

xcrun clang++ -std=c++11 -fblocks -DDEBUG=0 -Wno-deprecated-declarations \
    -IproxyAudioDevice -IproxyAudioDevice/PublicUtility -Ishared \
    tests/AudioWakeTests.cpp \
    proxyAudioDevice/ProxyAudioDevice.cpp proxyAudioDevice/AudioRingBuffer.cpp \
    proxyAudioDevice/utilities.cpp shared/AudioDevice.cpp \
    proxyAudioDevice/PublicUtility/CAMutex.cpp \
    proxyAudioDevice/PublicUtility/CADebugMacros.cpp \
    proxyAudioDevice/PublicUtility/CADebugPrintf.cpp \
    proxyAudioDevice/PublicUtility/CAHostTimeBase.cpp \
    -framework CoreAudio -framework CoreFoundation -framework Carbon \
    -framework AppKit -framework IOKit -o "$binary"
"$binary"
