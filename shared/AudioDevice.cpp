#include "AudioDevice.h"

#include "ProxyAudioDevice.h"
#include "debugHelpers.h"
#include "CFTypeHelpers.h"

AudioDevice::AudioDevice(AudioObjectID inId, bool inIsOutput) {
    id = inId;
    isOutput = inIsOutput;
    safetyOffset = 0;
    bufferFrameSize = 0;
    procId = nullptr;
    isStarted = false;

    initialize();
}

void AudioDevice::initialize() {
    if (id == kAudioObjectUnknown) {
        return;
    }

    if (updateStreamInfo() != noErr) {
        invalidate();
    }
}

bool AudioDevice::isValid() {
    return id != kAudioObjectUnknown;
}

void AudioDevice::invalidate() {
    id = kAudioObjectUnknown;
}

OSStatus AudioDevice::updateStreamInfo() {
    if (id == kAudioObjectUnknown) {
        return noErr;
    }

    OSStatus err = getIntegerPropertyData(safetyOffset,
                                          kAudioDevicePropertySafetyOffset,
                                          isOutput ? kAudioObjectPropertyScopeOutput : kAudioObjectPropertyScopeInput,
                                          kAudioObjectPropertyElementMaster);

    if (err != noErr) {
        syslog(LOG_WARNING, "ProxyAudio: error: failed to get safety offset of device %u", id);
        return err;
    }

    err = getIntegerPropertyData(bufferFrameSize,
                                 kAudioDevicePropertyBufferFrameSize,
                                 isOutput ? kAudioObjectPropertyScopeOutput : kAudioObjectPropertyScopeInput,
                                 kAudioObjectPropertyElementMaster);

    if (err != noErr) {
        syslog(LOG_WARNING, "ProxyAudio: error: failed to get buffer frame size of device %u", id);
        return err;
    }

    err = getDoublePropertyData(sampleRate,
                                kAudioDevicePropertyNominalSampleRate,
                                kAudioObjectPropertyScopeGlobal,
                                kAudioObjectPropertyElementMaster);

    if (err != noErr) {
        syslog(LOG_WARNING, "ProxyAudio: error: failed to get sample rate of device %u", id);
        return err;
    }

    return noErr;
}

void AudioDevice::addPropertyListener(AudioObjectPropertySelector selector,
                                      AudioObjectPropertyScope scope,
                                      AudioObjectPropertyElement element,
                                      AudioObjectPropertyListenerProc proc,
                                      void *clientData) {
    AudioObjectPropertyAddress listenerPropertyAddress = {selector, scope, element};
    OSStatus err = AudioObjectAddPropertyListener(id, &listenerPropertyAddress, proc, clientData);

    if (err != noErr) {
        syslog(LOG_WARNING,
               "ProxyAudio: failed to add listener for '%c%c%c%c' '%c%c%c%c' %u",
               (listenerPropertyAddress.mSelector >> 24) % 0xFF,
               (listenerPropertyAddress.mSelector >> 16) % 0xFF,
               (listenerPropertyAddress.mSelector >> 8) % 0xFF,
               (listenerPropertyAddress.mSelector) % 0xFF,
               (listenerPropertyAddress.mScope >> 24) % 0xFF,
               (listenerPropertyAddress.mScope >> 16) % 0xFF,
               (listenerPropertyAddress.mScope >> 8) % 0xFF,
               (listenerPropertyAddress.mScope) % 0xFF,
               listenerPropertyAddress.mElement);
    }
}

OSStatus AudioDevice::getIntegerPropertyData(UInt32 &outValue,
                                             AudioObjectPropertySelector selector,
                                             AudioObjectPropertyScope scope,
                                             AudioObjectPropertyElement element) {
    AudioObjectPropertyAddress propertyAddress = {selector, scope, element};
    UInt32 size = sizeof(UInt32);
    UInt32 value = 0;
    OSStatus err = AudioObjectGetPropertyData(id, &propertyAddress, 0, NULL, &size, &value);

    if (err != noErr) {
        syslog(LOG_WARNING,
               "ProxyAudio error: failed to get int property: '%c%c%c%c' "
               "'%c%c%c%c' %u",
               (propertyAddress.mSelector >> 24) % 0xFF,
               (propertyAddress.mSelector >> 16) % 0xFF,
               (propertyAddress.mSelector >> 8) % 0xFF,
               (propertyAddress.mSelector) % 0xFF,
               (propertyAddress.mScope >> 24) % 0xFF,
               (propertyAddress.mScope >> 16) % 0xFF,
               (propertyAddress.mScope >> 8) % 0xFF,
               (propertyAddress.mScope) % 0xFF,
               propertyAddress.mElement);
        return err;
    }

    outValue = value;

    return noErr;
}

OSStatus AudioDevice::getDoublePropertyData(Float64 &outValue,
                                            AudioObjectPropertySelector selector,
                                            AudioObjectPropertyScope scope,
                                            AudioObjectPropertyElement element) {
    AudioObjectPropertyAddress propertyAddress = {selector, scope, element};
    UInt32 size = sizeof(Float64);
    Float64 value = 0;
    OSStatus err = AudioObjectGetPropertyData(id, &propertyAddress, 0, NULL, &size, &value);

    if (err != noErr) {
        syslog(LOG_WARNING,
               "ProxyAudio error: failed to get Float64 property: '%c%c%c%c' "
               "'%c%c%c%c' %u",
               (propertyAddress.mSelector >> 24) % 0xFF,
               (propertyAddress.mSelector >> 16) % 0xFF,
               (propertyAddress.mSelector >> 8) % 0xFF,
               (propertyAddress.mSelector) % 0xFF,
               (propertyAddress.mScope >> 24) % 0xFF,
               (propertyAddress.mScope >> 16) % 0xFF,
               (propertyAddress.mScope >> 8) % 0xFF,
               (propertyAddress.mScope) % 0xFF,
               propertyAddress.mElement);
        return err;
    }

    outValue = value;

    return noErr;
}

void AudioDevice::setBufferFrameSize(UInt32 newBufferFrameSize) {
    AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyBufferFrameSize,
                                                  isOutput ? kAudioObjectPropertyScopeOutput
                                                           : kAudioObjectPropertyScopeInput,
                                                  kAudioObjectPropertyElementMaster};

    OSStatus err =
        AudioObjectSetPropertyData(id, &propertyAddress, 0, NULL, sizeof(bufferFrameSize), &newBufferFrameSize);

    if (err == noErr) {
        bufferFrameSize = newBufferFrameSize;
    } else {
        syslog(LOG_WARNING, "ProxyAudio: error: failed to set buffer frame size of device %u", id);
    }
}

void AudioDevice::setupIOProc(AudioDeviceIOProc inProc, void *clientData) {
    if (!isValid()) {
        syslog(LOG_WARNING,
               "ProxyAudio: error: tried to set up an IO proc on a null audio "
               "device!");
        return;
    }

    if (procId != nullptr) {
        return;
    }

    OSStatus err = AudioDeviceCreateIOProcID(id, inProc, clientData, &procId);

    if (err != noErr) {
        syslog(LOG_WARNING, "ProxyAudio: error: failed to setup proc id on device %u", id);
        procId = nullptr;
    }
}

void AudioDevice::destroyIOProc() {
    if (procId != nullptr) {
        AudioDeviceDestroyIOProcID(id, procId);
        procId = nullptr;
    }
}

void AudioDevice::start() {
    if (isStarted) {
        return;
    }

    if (procId == nullptr) {
        syslog(LOG_WARNING, "ProxyAudio: error: tried to start device %u with no IO proc setup", id);
        return;
    }

    OSStatus err = AudioDeviceStart(id, procId);

    if (err != noErr) {
        syslog(LOG_WARNING, "ProxyAudio: error: failed to start device %u", id);
    } else {
        isStarted = true;
    }
}

void AudioDevice::stop() {
    if (!isStarted) {
        return;
    }

    if (procId == nullptr) {
        syslog(LOG_WARNING, "ProxyAudio: error: tried to start device %u with no IO proc setup", id);
        return;
    }

    OSStatus err = AudioDeviceStop(id, procId);
    isStarted = false;

    if (err != noErr) {
        syslog(LOG_WARNING, "ProxyAudio: error: failed to stop device %u", id);
    }
}

std::vector<AudioObjectID> AudioDevice::allAudioDevices() {
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

    UInt32 devicesSize = 0;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &devicesSize);
    UInt32 deviceCount = devicesSize / sizeof(AudioObjectID);

    if (deviceCount == 0) {
        return std::vector<AudioObjectID>();
    }

    std::vector<AudioObjectID> devices(deviceCount);
    OSStatus error =
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &devicesSize, devices.data());

    if (error != noErr) {
        return std::vector<AudioObjectID>();
    }

    return devices;
}

std::vector<AudioObjectID> AudioDevice::devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice() {
    std::vector<AudioObjectID> devices = allAudioDevices();
    std::vector<AudioObjectID> result;

    for (AudioObjectID device : devices) {
        UInt32 values[2];
        AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyPreferredChannelsForStereo,
                                                      kAudioObjectPropertyScopeOutput,
                                                      kAudioObjectPropertyElementMaster};
        UInt32 size = sizeof(values);
        OSStatus error = AudioObjectGetPropertyData(device, &propertyAddress, 0, NULL, &size, values);

        if (error != noErr || values[0] == values[1]) {
            continue;
        }

        CFStringSmartRef uid = AudioDevice::copyDeviceUID(device);

        if (!uid || CFStringCompare(uid, CFSTR(kDevice_UID), 0) == kCFCompareEqualTo) {
            continue;
        }

        result.push_back(device);
    }

    return result;
}

AudioObjectID AudioDevice::defaultOutputDevice() {
    AudioObjectID result;
    UInt32 size = sizeof(result);
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
    OSStatus error = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size, &result);

    return (error == noErr) ? result : kAudioObjectUnknown;
}

CFStringRef AudioDevice::copyDeviceUID(AudioObjectID device) {
    if (device == kAudioObjectUnknown) {
        return nullptr;
    }

    AudioObjectPropertyAddress uidAddr = {
        kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
    CFStringRef uid = NULL;
    UInt32 size = sizeof(uid);
    OSStatus error = AudioObjectGetPropertyData(device, &uidAddr, 0, NULL, &size, &uid);

    return (error == noErr) ? uid : nullptr;
}

CFStringRef AudioDevice::copyObjectName(AudioObjectID device) {
    if (device == kAudioObjectUnknown) {
        return nullptr;
    }

    AudioObjectPropertyAddress nameAddr = {
        kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
    CFStringRef name = NULL;
    UInt32 size = sizeof(name);
    OSStatus error = AudioObjectGetPropertyData(device, &nameAddr, 0, NULL, &size, &name);

    return (error == noErr) ? name : nullptr;
}

void AudioDevice::setObjectName(AudioObjectID object, CFStringRef newName) {
    AudioObjectPropertyAddress setNameAddr = {
        kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
    AudioObjectSetPropertyData(object, &setNameAddr, 0, NULL, sizeof(newName), &newName);
}

AudioDeviceID AudioDevice::audioDeviceIDForUID(CFStringRef uid, AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress propertyAddress = {
        selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

    AudioObjectID result;
    UInt32 resultSize = sizeof(result);
    UInt32 uidSize = sizeof(uid);
    OSStatus error =
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, uidSize, &uid, &resultSize, &result);

    if (error != noErr) {
        return kAudioObjectUnknown;
    }

    return result;
}

AudioDeviceID AudioDevice::audioDeviceIDForDeviceUID(CFStringRef uid) {
    return audioDeviceIDForUID(uid, kAudioHardwarePropertyTranslateUIDToDevice);
}

AudioDeviceID AudioDevice::audioDeviceIDForBoxUID(CFStringRef uid) {
    return audioDeviceIDForUID(uid, kAudioHardwarePropertyTranslateUIDToBox);
}

bool AudioDevice::setIdentifyValue(AudioDeviceID device, SInt32 value) {
    AudioObjectPropertyAddress setIdentifyAddr = {
        kAudioObjectPropertyIdentify, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

    return AudioObjectSetPropertyData(device, &setIdentifyAddr, 0, NULL, sizeof(value), &value) == noErr;
}
