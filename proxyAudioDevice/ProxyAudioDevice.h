#ifndef ProxyAudioDevice_h
#define ProxyAudioDevice_h

#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreAudio/CoreAudio.h>
#include <vector>
#include <atomic>

#include "AudioDevice.h"
#include "CAMutex.h"

class AudioRingBuffer;

enum {
    kObjectID_PlugIn = kAudioObjectPlugInObject,
    kObjectID_Box = 2,
    kObjectID_Device = 3,
    kObjectID_Stream_Output = 4,
    kObjectID_Volume_Output_L = 5,
    kObjectID_Volume_Output_R = 6,
    kObjectID_Mute_Output_Master = 7,
    kObjectID_DataSource_Output_Master = 8
};

#define kPlugIn_BundleID "net.briankendall.ProxyAudioDevice"
#define kBox_UID "ProxyAudioBox_UID"
#define kDevice_UID "ProxyAudioDevice_UID"
#define kDevice_ModelUID "ProxyAudioDevice_ModelUID"
#define kOutputDeviceDefaultBufferFrameSize 512
#define kOutputDeviceMinBufferFrameSize 4
#define kOutputDeviceDefaultActiveCondition ActiveCondition::userActive

class ProxyAudioDevice {
  public:
    enum class ConfigType { none, outputDevice, outputDeviceBufferFrameSize, deviceName, deviceActiveCondition };
    enum class ActiveCondition { proxiedDeviceActive = 0, userActive = 1, always = 2 };

    ProxyAudioDevice() : inputIOIsActive(false) {};
    AudioDevice findTargetOutputAudioDevice();
    static int outputDeviceAliveListenerStatic(AudioObjectID inObjectID,
                                               UInt32 inNumberAddresses,
                                               const AudioObjectPropertyAddress *inAddresses,
                                               void *inClientData);
    int outputDeviceAliveListener(AudioObjectID inObjectID,
                                  UInt32 inNumberAddresses,
                                  const AudioObjectPropertyAddress *inAddresses);
    static int outputDeviceSampleRateListenerStatic(AudioObjectID inObjectID,
                                                    UInt32 inNumberAddresses,
                                                    const AudioObjectPropertyAddress *inAddresses,
                                                    void *inClientData);
    int outputDeviceSampleRateListener(AudioObjectID inObjectID,
                                       UInt32 inNumberAddresses,
                                       const AudioObjectPropertyAddress *inAddresses);
    void updateOutputDeviceStartedState();
    void matchOutputDeviceSampleRateNoLock();
    void matchOutputDeviceSampleRate();
    static int devicesListenerProcStatic(AudioObjectID inObjectID,
                                         UInt32 inNumberAddresses,
                                         const AudioObjectPropertyAddress *inAddresses,
                                         void *inClientData);
    int devicesListenerProc(AudioObjectID inObjectID,
                            UInt32 inNumberAddresses,
                            const AudioObjectPropertyAddress *inAddresses);
    void setupAudioDevicesListener();
    void setupTargetOutputDevice();
    void initializeOutputDevice();
    void deinitializeOutputDeviceNoLock();
    void deinitializeOutputDevice();
    void resetInputData();
    static OSStatus outputDeviceIOProcStatic(AudioDeviceID inDevice,
                                             const AudioTimeStamp *inNow,
                                             const AudioBufferList *inInputData,
                                             const AudioTimeStamp *inInputTime,
                                             AudioBufferList *outOutputData,
                                             const AudioTimeStamp *inOutputTime,
                                             void *inClientData);
    OSStatus outputDeviceIOProc(AudioDeviceID inDevice,
                                const AudioTimeStamp *inNow,
                                const AudioBufferList *inInputData,
                                const AudioTimeStamp *inInputTime,
                                AudioBufferList *outOutputData,
                                const AudioTimeStamp *inOutputTime);
    void calculateVolumeFactors(Float32 volumeL,
                                Float32 volumeR,
                                bool mute,
                                Float32 &volumeFactorL,
                                Float32 &volumeFactorR);
    bool isConfigurationString(CFStringRef val);
    void parseConfigurationString(CFStringRef configString, ConfigType &action, CFStringRef &value);
    void setConfigurationValue(ConfigType action, CFStringRef value);
    CFStringRef copyConfigurationValue(ConfigType action);
    CFStringRef copyDeviceNameFromStorage();
    void setDeviceName(CFStringRef newName);
    CFStringRef copyDefaultProxyOutputDeviceUID();
    CFStringRef copyOutputDeviceUIDFromStorage();
    void setOutputDevice(CFStringRef deviceUID);
    UInt32 retrieveOutputDeviceBufferFrameSizeFromStorage();
    void setOutputDeviceBufferFrameSize(UInt32 size);
    ActiveCondition retrieveOutputDeviceActiveConditionFromStorage();
    void setOutputDeviceActiveCondition(ActiveCondition newActiveCondition);

    static ProxyAudioDevice *deviceForDriver(void *inDriver);

    //    Entry points for the COM methods
    static HRESULT ProxyAudio_QueryInterface(void *inDriver, REFIID inUUID, LPVOID *outInterface);
    static ULONG ProxyAudio_AddRef(void *inDriver);
    static ULONG ProxyAudio_Release(void *inDriver);
    static OSStatus ProxyAudio_Initialize(AudioServerPlugInDriverRef inDriver, AudioServerPlugInHostRef inHost);
    static OSStatus ProxyAudio_CreateDevice(AudioServerPlugInDriverRef inDriver,
                                            CFDictionaryRef inDescription,
                                            const AudioServerPlugInClientInfo *inClientInfo,
                                            AudioObjectID *outDeviceObjectID);
    static OSStatus ProxyAudio_DestroyDevice(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID);
    static OSStatus ProxyAudio_AddDeviceClient(AudioServerPlugInDriverRef inDriver,
                                               AudioObjectID inDeviceObjectID,
                                               const AudioServerPlugInClientInfo *inClientInfo);
    static OSStatus ProxyAudio_RemoveDeviceClient(AudioServerPlugInDriverRef inDriver,
                                                  AudioObjectID inDeviceObjectID,
                                                  const AudioServerPlugInClientInfo *inClientInfo);
    static OSStatus ProxyAudio_PerformDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver,
                                                                AudioObjectID inDeviceObjectID,
                                                                UInt64 inChangeAction,
                                                                void *inChangeInfo);
    static OSStatus ProxyAudio_AbortDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver,
                                                              AudioObjectID inDeviceObjectID,
                                                              UInt64 inChangeAction,
                                                              void *inChangeInfo);
    static Boolean ProxyAudio_HasProperty(AudioServerPlugInDriverRef inDriver,
                                          AudioObjectID inObjectID,
                                          pid_t inClientProcessID,
                                          const AudioObjectPropertyAddress *inAddress);
    static OSStatus ProxyAudio_IsPropertySettable(AudioServerPlugInDriverRef inDriver,
                                                  AudioObjectID inObjectID,
                                                  pid_t inClientProcessID,
                                                  const AudioObjectPropertyAddress *inAddress,
                                                  Boolean *outIsSettable);
    static OSStatus ProxyAudio_GetPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                                   AudioObjectID inObjectID,
                                                   pid_t inClientProcessID,
                                                   const AudioObjectPropertyAddress *inAddress,
                                                   UInt32 inQualifierDataSize,
                                                   const void *inQualifierData,
                                                   UInt32 *outDataSize);
    static OSStatus ProxyAudio_GetPropertyData(AudioServerPlugInDriverRef inDriver,
                                               AudioObjectID inObjectID,
                                               pid_t inClientProcessID,
                                               const AudioObjectPropertyAddress *inAddress,
                                               UInt32 inQualifierDataSize,
                                               const void *inQualifierData,
                                               UInt32 inDataSize,
                                               UInt32 *outDataSize,
                                               void *outData);
    static OSStatus ProxyAudio_SetPropertyData(AudioServerPlugInDriverRef inDriver,
                                               AudioObjectID inObjectID,
                                               pid_t inClientProcessID,
                                               const AudioObjectPropertyAddress *inAddress,
                                               UInt32 inQualifierDataSize,
                                               const void *inQualifierData,
                                               UInt32 inDataSize,
                                               const void *inData);
    static OSStatus ProxyAudio_StartIO(AudioServerPlugInDriverRef inDriver,
                                       AudioObjectID inDeviceObjectID,
                                       UInt32 inClientID);
    static OSStatus ProxyAudio_StopIO(AudioServerPlugInDriverRef inDriver,
                                      AudioObjectID inDeviceObjectID,
                                      UInt32 inClientID);
    static OSStatus ProxyAudio_GetZeroTimeStamp(AudioServerPlugInDriverRef inDriver,
                                                AudioObjectID inDeviceObjectID,
                                                UInt32 inClientID,
                                                Float64 *outSampleTime,
                                                UInt64 *outHostTime,
                                                UInt64 *outSeed);
    static OSStatus ProxyAudio_WillDoIOOperation(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inDeviceObjectID,
                                                 UInt32 inClientID,
                                                 UInt32 inOperationID,
                                                 Boolean *outWillDo,
                                                 Boolean *outWillDoInPlace);
    static OSStatus ProxyAudio_BeginIOOperation(AudioServerPlugInDriverRef inDriver,
                                                AudioObjectID inDeviceObjectID,
                                                UInt32 inClientID,
                                                UInt32 inOperationID,
                                                UInt32 inIOBufferFrameSize,
                                                const AudioServerPlugInIOCycleInfo *inIOCycleInfo);
    static OSStatus ProxyAudio_DoIOOperation(AudioServerPlugInDriverRef inDriver,
                                             AudioObjectID inDeviceObjectID,
                                             AudioObjectID inStreamObjectID,
                                             UInt32 inClientID,
                                             UInt32 inOperationID,
                                             UInt32 inIOBufferFrameSize,
                                             const AudioServerPlugInIOCycleInfo *inIOCycleInfo,
                                             void *ioMainBuffer,
                                             void *ioSecondaryBuffer);
    static OSStatus ProxyAudio_EndIOOperation(AudioServerPlugInDriverRef inDriver,
                                              AudioObjectID inDeviceObjectID,
                                              UInt32 inClientID,
                                              UInt32 inOperationID,
                                              UInt32 inIOBufferFrameSize,
                                              const AudioServerPlugInIOCycleInfo *inIOCycleInfo);

    //    Implementation
    HRESULT QueryInterface(void *inDriver, REFIID inUUID, LPVOID *outInterface);
    ULONG AddRef(void *inDriver);
    ULONG Release(void *inDriver);
    OSStatus Initialize(AudioServerPlugInDriverRef inDriver, AudioServerPlugInHostRef inHost);
    OSStatus CreateDevice(AudioServerPlugInDriverRef inDriver,
                          CFDictionaryRef inDescription,
                          const AudioServerPlugInClientInfo *inClientInfo,
                          AudioObjectID *outDeviceObjectID);
    OSStatus DestroyDevice(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID);
    OSStatus AddDeviceClient(AudioServerPlugInDriverRef inDriver,
                             AudioObjectID inDeviceObjectID,
                             const AudioServerPlugInClientInfo *inClientInfo);
    OSStatus RemoveDeviceClient(AudioServerPlugInDriverRef inDriver,
                                AudioObjectID inDeviceObjectID,
                                const AudioServerPlugInClientInfo *inClientInfo);
    OSStatus PerformDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver,
                                              AudioObjectID inDeviceObjectID,
                                              UInt64 inChangeAction,
                                              void *inChangeInfo);
    OSStatus AbortDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver,
                                            AudioObjectID inDeviceObjectID,
                                            UInt64 inChangeAction,
                                            void *inChangeInfo);
    Boolean HasProperty(AudioServerPlugInDriverRef inDriver,
                        AudioObjectID inObjectID,
                        pid_t inClientProcessID,
                        const AudioObjectPropertyAddress *inAddress);
    OSStatus IsPropertySettable(AudioServerPlugInDriverRef inDriver,
                                AudioObjectID inObjectID,
                                pid_t inClientProcessID,
                                const AudioObjectPropertyAddress *inAddress,
                                Boolean *outIsSettable);
    OSStatus GetPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                 AudioObjectID inObjectID,
                                 pid_t inClientProcessID,
                                 const AudioObjectPropertyAddress *inAddress,
                                 UInt32 inQualifierDataSize,
                                 const void *inQualifierData,
                                 UInt32 *outDataSize);
    OSStatus GetPropertyData(AudioServerPlugInDriverRef inDriver,
                             AudioObjectID inObjectID,
                             pid_t inClientProcessID,
                             const AudioObjectPropertyAddress *inAddress,
                             UInt32 inQualifierDataSize,
                             const void *inQualifierData,
                             UInt32 inDataSize,
                             UInt32 *outDataSize,
                             void *outData);
    OSStatus SetPropertyData(AudioServerPlugInDriverRef inDriver,
                             AudioObjectID inObjectID,
                             pid_t inClientProcessID,
                             const AudioObjectPropertyAddress *inAddress,
                             UInt32 inQualifierDataSize,
                             const void *inQualifierData,
                             UInt32 inDataSize,
                             const void *inData);
    OSStatus StartIO(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID);
    OSStatus StopIO(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID);
    OSStatus GetZeroTimeStamp(AudioServerPlugInDriverRef inDriver,
                              AudioObjectID inDeviceObjectID,
                              UInt32 inClientID,
                              Float64 *outSampleTime,
                              UInt64 *outHostTime,
                              UInt64 *outSeed);
    OSStatus WillDoIOOperation(AudioServerPlugInDriverRef inDriver,
                               AudioObjectID inDeviceObjectID,
                               UInt32 inClientID,
                               UInt32 inOperationID,
                               Boolean *outWillDo,
                               Boolean *outWillDoInPlace);
    OSStatus BeginIOOperation(AudioServerPlugInDriverRef inDriver,
                              AudioObjectID inDeviceObjectID,
                              UInt32 inClientID,
                              UInt32 inOperationID,
                              UInt32 inIOBufferFrameSize,
                              const AudioServerPlugInIOCycleInfo *inIOCycleInfo);
    OSStatus DoIOOperation(AudioServerPlugInDriverRef inDriver,
                           AudioObjectID inDeviceObjectID,
                           AudioObjectID inStreamObjectID,
                           UInt32 inClientID,
                           UInt32 inOperationID,
                           UInt32 inIOBufferFrameSize,
                           const AudioServerPlugInIOCycleInfo *inIOCycleInfo,
                           void *ioMainBuffer,
                           void *ioSecondaryBuffer);
    OSStatus EndIOOperation(AudioServerPlugInDriverRef inDriver,
                            AudioObjectID inDeviceObjectID,
                            UInt32 inClientID,
                            UInt32 inOperationID,
                            UInt32 inIOBufferFrameSize,
                            const AudioServerPlugInIOCycleInfo *inIOCycleInfo);

    Boolean HasPlugInProperty(AudioServerPlugInDriverRef inDriver,
                              AudioObjectID inObjectID,
                              pid_t inClientProcessID,
                              const AudioObjectPropertyAddress *inAddress);
    OSStatus IsPlugInPropertySettable(AudioServerPlugInDriverRef inDriver,
                                      AudioObjectID inObjectID,
                                      pid_t inClientProcessID,
                                      const AudioObjectPropertyAddress *inAddress,
                                      Boolean *outIsSettable);
    OSStatus GetPlugInPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                       AudioObjectID inObjectID,
                                       pid_t inClientProcessID,
                                       const AudioObjectPropertyAddress *inAddress,
                                       UInt32 inQualifierDataSize,
                                       const void *inQualifierData,
                                       UInt32 *outDataSize);
    OSStatus GetPlugInPropertyData(AudioServerPlugInDriverRef inDriver,
                                   AudioObjectID inObjectID,
                                   pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress *inAddress,
                                   UInt32 inQualifierDataSize,
                                   const void *inQualifierData,
                                   UInt32 inDataSize,
                                   UInt32 *outDataSize,
                                   void *outData);
    OSStatus SetPlugInPropertyData(AudioServerPlugInDriverRef inDriver,
                                   AudioObjectID inObjectID,
                                   pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress *inAddress,
                                   UInt32 inQualifierDataSize,
                                   const void *inQualifierData,
                                   UInt32 inDataSize,
                                   const void *inData,
                                   UInt32 *outNumberPropertiesChanged,
                                   AudioObjectPropertyAddress outChangedAddresses[2]);
    Boolean HasBoxProperty(AudioServerPlugInDriverRef inDriver,
                           AudioObjectID inObjectID,
                           pid_t inClientProcessID,
                           const AudioObjectPropertyAddress *inAddress);
    OSStatus IsBoxPropertySettable(AudioServerPlugInDriverRef inDriver,
                                   AudioObjectID inObjectID,
                                   pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress *inAddress,
                                   Boolean *outIsSettable);
    OSStatus GetBoxPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                    AudioObjectID inObjectID,
                                    pid_t inClientProcessID,
                                    const AudioObjectPropertyAddress *inAddress,
                                    UInt32 inQualifierDataSize,
                                    const void *inQualifierData,
                                    UInt32 *outDataSize);
    OSStatus GetBoxPropertyData(AudioServerPlugInDriverRef inDriver,
                                AudioObjectID inObjectID,
                                pid_t inClientProcessID,
                                const AudioObjectPropertyAddress *inAddress,
                                UInt32 inQualifierDataSize,
                                const void *inQualifierData,
                                UInt32 inDataSize,
                                UInt32 *outDataSize,
                                void *outData);
    OSStatus SetBoxPropertyData(AudioServerPlugInDriverRef inDriver,
                                AudioObjectID inObjectID,
                                pid_t inClientProcessID,
                                const AudioObjectPropertyAddress *inAddress,
                                UInt32 inQualifierDataSize,
                                const void *inQualifierData,
                                UInt32 inDataSize,
                                const void *inData,
                                UInt32 *outNumberPropertiesChanged,
                                AudioObjectPropertyAddress outChangedAddresses[2]);

    Boolean HasDeviceProperty(AudioServerPlugInDriverRef inDriver,
                              AudioObjectID inObjectID,
                              pid_t inClientProcessID,
                              const AudioObjectPropertyAddress *inAddress);
    OSStatus IsDevicePropertySettable(AudioServerPlugInDriverRef inDriver,
                                      AudioObjectID inObjectID,
                                      pid_t inClientProcessID,
                                      const AudioObjectPropertyAddress *inAddress,
                                      Boolean *outIsSettable);
    OSStatus GetDevicePropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                       AudioObjectID inObjectID,
                                       pid_t inClientProcessID,
                                       const AudioObjectPropertyAddress *inAddress,
                                       UInt32 inQualifierDataSize,
                                       const void *inQualifierData,
                                       UInt32 *outDataSize);
    OSStatus GetDevicePropertyData(AudioServerPlugInDriverRef inDriver,
                                   AudioObjectID inObjectID,
                                   pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress *inAddress,
                                   UInt32 inQualifierDataSize,
                                   const void *inQualifierData,
                                   UInt32 inDataSize,
                                   UInt32 *outDataSize,
                                   void *outData);
    OSStatus SetDevicePropertyData(AudioServerPlugInDriverRef inDriver,
                                   AudioObjectID inObjectID,
                                   pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress *inAddress,
                                   UInt32 inQualifierDataSize,
                                   const void *inQualifierData,
                                   UInt32 inDataSize,
                                   const void *inData,
                                   UInt32 *outNumberPropertiesChanged,
                                   AudioObjectPropertyAddress outChangedAddresses[2]);
    Boolean HasStreamProperty(AudioServerPlugInDriverRef inDriver,
                              AudioObjectID inObjectID,
                              pid_t inClientProcessID,
                              const AudioObjectPropertyAddress *inAddress);
    OSStatus IsStreamPropertySettable(AudioServerPlugInDriverRef inDriver,
                                      AudioObjectID inObjectID,
                                      pid_t inClientProcessID,
                                      const AudioObjectPropertyAddress *inAddress,
                                      Boolean *outIsSettable);
    OSStatus GetStreamPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                       AudioObjectID inObjectID,
                                       pid_t inClientProcessID,
                                       const AudioObjectPropertyAddress *inAddress,
                                       UInt32 inQualifierDataSize,
                                       const void *inQualifierData,
                                       UInt32 *outDataSize);
    OSStatus GetStreamPropertyData(AudioServerPlugInDriverRef inDriver,
                                   AudioObjectID inObjectID,
                                   pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress *inAddress,
                                   UInt32 inQualifierDataSize,
                                   const void *inQualifierData,
                                   UInt32 inDataSize,
                                   UInt32 *outDataSize,
                                   void *outData);
    OSStatus SetStreamPropertyData(AudioServerPlugInDriverRef inDriver,
                                   AudioObjectID inObjectID,
                                   pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress *inAddress,
                                   UInt32 inQualifierDataSize,
                                   const void *inQualifierData,
                                   UInt32 inDataSize,
                                   const void *inData,
                                   UInt32 *outNumberPropertiesChanged,
                                   AudioObjectPropertyAddress outChangedAddresses[2]);
    Boolean HasControlProperty(AudioServerPlugInDriverRef inDriver,
                               AudioObjectID inObjectID,
                               pid_t inClientProcessID,
                               const AudioObjectPropertyAddress *inAddress);
    OSStatus IsControlPropertySettable(AudioServerPlugInDriverRef inDriver,
                                       AudioObjectID inObjectID,
                                       pid_t inClientProcessID,
                                       const AudioObjectPropertyAddress *inAddress,
                                       Boolean *outIsSettable);
    OSStatus GetControlPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                        AudioObjectID inObjectID,
                                        pid_t inClientProcessID,
                                        const AudioObjectPropertyAddress *inAddress,
                                        UInt32 inQualifierDataSize,
                                        const void *inQualifierData,
                                        UInt32 *outDataSize);
    OSStatus GetControlPropertyData(AudioServerPlugInDriverRef inDriver,
                                    AudioObjectID inObjectID,
                                    pid_t inClientProcessID,
                                    const AudioObjectPropertyAddress *inAddress,
                                    UInt32 inQualifierDataSize,
                                    const void *inQualifierData,
                                    UInt32 inDataSize,
                                    UInt32 *outDataSize,
                                    void *outData);
    OSStatus SetControlPropertyData(AudioServerPlugInDriverRef inDriver,
                                    AudioObjectID inObjectID,
                                    pid_t inClientProcessID,
                                    const AudioObjectPropertyAddress *inAddress,
                                    UInt32 inQualifierDataSize,
                                    const void *inQualifierData,
                                    UInt32 inDataSize,
                                    const void *inData,
                                    UInt32 *outNumberPropertiesChanged,
                                    AudioObjectPropertyAddress outChangedAddresses[2]);
    void monitorUserActivity();
    dispatch_queue_t AudioOutputDispatchQueue();
    void ExecuteInAudioOutputThread(void (^block)());
    
    CAMutex stateMutex = CAMutex("ProxyAudioStateMutex");
    CAMutex IOMutex = CAMutex("ProxyAudioIOMutex");
    CAMutex outputDeviceMutex = CAMutex("ProxyAudioOutputDeviceMutex");
    CAMutex getZeroTimestampMutex = CAMutex("ProxyAudioGetZeroTimestampMutex");
    dispatch_queue_t audioOutputQueue = NULL;
    dispatch_source_t inputMonitoringTimer = NULL;
    AudioRingBuffer *inputBuffer = NULL;
    Byte *workBuffer = NULL;
    AudioDevice outputDevice;
    bool outputDeviceReady = false;
    std::atomic_bool inputIOIsActive;
    Float64 lastInputFrameTime = -1;
    Float64 lastInputBufferFrameSize = -1;
    Float64 inputOutputSampleDelta = -1;
    Float64 inputFinalFrameTime = -1;
    int inputCycleCount = 0;
    ConfigType nextConfigurationToRead = ConfigType::none;
    pid_t configuratorPid = 0;
    CFStringRef deviceName = NULL;
    CFStringRef boxName = NULL;
    CFStringRef outputDeviceUID = NULL;
    UInt32 outputDeviceBufferFrameSize = kOutputDeviceDefaultBufferFrameSize;
    SInt64 smallestFramesToBufferEnd = -1;
    Float64 outputAccumulatedRateRatio = 0.0;
    UInt64 outputAccumulatedRateRatioSamples = 0;
    ActiveCondition outputDeviceActiveCondition = ActiveCondition::userActive;
    
    UInt32 gPlugIn_RefCount = 0;
    AudioServerPlugInHostRef gPlugIn_Host = NULL;
    Boolean gBox_Acquired = true;
    Float64 gDevice_SampleRate = 44100.0;
    std::vector<Float64> gDevice_SampleRates = {22050, 44100, 48000, 88200, 96000, 176400, 192000};
    UInt64 gDevice_IOIsRunning = 0;
    const UInt32 kDevice_RingBufferSize = 16384;
    Float64 gDevice_HostTicksPerFrame = 0.0;
    UInt64 gDevice_NumberTimeStamps = 0;
    Float64 gDevice_AnchorSampleTime = 0.0;
    Float64 gDevice_ElapsedTicks = 0.0;
    UInt64 gDevice_AnchorHostTime = 0;
    bool gStream_Output_IsActive = true;
    const Float32 kVolume_MinDB = -25.0;
    const Float32 kVolume_MaxDB = 0.0;
    Float32 gVolume_Output_L_Value = 0.0;
    Float32 gVolume_Output_R_Value = 0.0;
    bool gMute_Output_Mute = false;
    const UInt32 gDevice_BytesPerFrameInChannel = 4;
    const UInt32 gDevice_ChannelsPerFrame = 2;
    const UInt32 gDevice_SafetyOffset = 0;
};

extern "C" void *ProxyAudio_Create(CFAllocatorRef inAllocator, CFUUIDRef inRequestedTypeUUID);

#endif /* ProxyAudioDevice_h */
