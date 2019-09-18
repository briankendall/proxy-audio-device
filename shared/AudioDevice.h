#ifndef __AudioDevice_h__
#define __AudioDevice_h__

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <vector>

class AudioDevice {
  public:
    AudioDevice(AudioObjectID inId = kAudioObjectUnknown, bool inIsOutput = true);
    bool isValid();
    void invalidate();
    OSStatus updateStreamInfo();
    void addPropertyListener(AudioObjectPropertySelector selector,
                             AudioObjectPropertyScope scope,
                             AudioObjectPropertyElement element,
                             AudioObjectPropertyListenerProc proc,
                             void *clientData);
    OSStatus getIntegerPropertyData(UInt32 &outValue,
                                    AudioObjectPropertySelector selector,
                                    AudioObjectPropertyScope scope,
                                    AudioObjectPropertyElement element);
    OSStatus getDoublePropertyData(Float64 &outValue,
                                   AudioObjectPropertySelector selector,
                                   AudioObjectPropertyScope scope,
                                   AudioObjectPropertyElement element);
    void setBufferFrameSize(UInt32 bufferFrameSize);
    void setupIOProc(AudioDeviceIOProc inProc, void *clientData);
    void destroyIOProc();
    void start();
    void stop();
    static std::vector<AudioObjectID> allAudioDevices();
    static std::vector<AudioObjectID> devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice();
    static AudioObjectID defaultOutputDevice();
    static CFStringRef copyDeviceUID(AudioObjectID device);
    static CFStringRef copyObjectName(AudioObjectID device);
    static void setObjectName(AudioObjectID device, CFStringRef newName);
    static AudioDeviceID audioDeviceIDForUID(CFStringRef uid, AudioObjectPropertySelector selector);
    static AudioDeviceID audioDeviceIDForDeviceUID(CFStringRef uid);
    static AudioDeviceID audioDeviceIDForBoxUID(CFStringRef uid);
    static bool setIdentifyValue(AudioDeviceID device, SInt32 value);

    AudioObjectID id;
    bool isOutput;
    UInt32 safetyOffset;
    UInt32 bufferFrameSize;
    Float64 sampleRate;
    AudioDeviceIOProcID procId;
    bool isStarted;

  protected:
    void initialize();
};

#endif
