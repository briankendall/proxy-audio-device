#include "ProxyAudioDevice.h"

#include <algorithm>
#include <string>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>

#include "AudioDevice.h"
#include "AudioRingBuffer.h"
#include "CFTypeHelpers.h"
#include "debugHelpers.h"

#pragma mark Utility Functions

template<typename T>
bool contains(const std::vector<T> &v, const T &val) {
    return std::find(v.begin(), v.end(), val) != v.end();
}

std::string CFStringToStdString(CFStringRef s);

std::string CFStringToStdString(CFStringRef s) {
    if (!s) {
        return std::string("<null>");
    }
    
    char *buffer;
    size_t length = CFStringGetLength(s) + 1;
    buffer = new char[length];
    CFStringGetCString(s, buffer, length, kCFStringEncodingUTF8);
    std::string result(buffer);
    delete buffer;
    
    return result;
}

#pragma mark The Interface

static AudioServerPlugInDriverInterface gAudioServerPlugInDriverInterface = {
    NULL,
    ProxyAudioDevice::ProxyAudio_QueryInterface,
    ProxyAudioDevice::ProxyAudio_AddRef,
    ProxyAudioDevice::ProxyAudio_Release,
    ProxyAudioDevice::ProxyAudio_Initialize,
    ProxyAudioDevice::ProxyAudio_CreateDevice,
    ProxyAudioDevice::ProxyAudio_DestroyDevice,
    ProxyAudioDevice::ProxyAudio_AddDeviceClient,
    ProxyAudioDevice::ProxyAudio_RemoveDeviceClient,
    ProxyAudioDevice::ProxyAudio_PerformDeviceConfigurationChange,
    ProxyAudioDevice::ProxyAudio_AbortDeviceConfigurationChange,
    ProxyAudioDevice::ProxyAudio_HasProperty,
    ProxyAudioDevice::ProxyAudio_IsPropertySettable,
    ProxyAudioDevice::ProxyAudio_GetPropertyDataSize,
    ProxyAudioDevice::ProxyAudio_GetPropertyData,
    ProxyAudioDevice::ProxyAudio_SetPropertyData,
    ProxyAudioDevice::ProxyAudio_StartIO,
    ProxyAudioDevice::ProxyAudio_StopIO,
    ProxyAudioDevice::ProxyAudio_GetZeroTimeStamp,
    ProxyAudioDevice::ProxyAudio_WillDoIOOperation,
    ProxyAudioDevice::ProxyAudio_BeginIOOperation,
    ProxyAudioDevice::ProxyAudio_DoIOOperation,
    ProxyAudioDevice::ProxyAudio_EndIOOperation};
static AudioServerPlugInDriverInterface *gAudioServerPlugInDriverInterfacePtr = &gAudioServerPlugInDriverInterface;
static AudioServerPlugInDriverRef gAudioServerPlugInDriverRef = &gAudioServerPlugInDriverInterfacePtr;

#pragma mark Factory

void *ProxyAudio_Create(CFAllocatorRef inAllocator, CFUUIDRef inRequestedTypeUUID) {
    //    This is the CFPlugIn factory function. Its job is to create the implementation for the given
    //    type provided that the type is supported. Because this driver is simple and all its
    //    initialization is handled via static iniitalization when the bundle is loaded, all that
    //    needs to be done is to return the AudioServerPlugInDriverRef that points to the driver's
    //    interface. A more complicated driver would create any base line objects it needs to satisfy
    //    the IUnknown methods that are used to discover that actual interface to talk to the driver.
    //    The majority of the driver's initilization should be handled in the Initialize() method of
    //    the driver's AudioServerPlugInDriverInterface.

#pragma unused(inAllocator)
    void *theAnswer = NULL;
    if (CFEqual(inRequestedTypeUUID, kAudioServerPlugInTypeUUID)) {
        theAnswer = gAudioServerPlugInDriverRef;
    }
    return theAnswer;
}

ProxyAudioDevice *ProxyAudioDevice::deviceForDriver(void *inDriver) {
#pragma unused(inDriver)
    static ProxyAudioDevice *mainDevice = nullptr;

    if (!mainDevice) {
        mainDevice = new ProxyAudioDevice;
    }

    return mainDevice;
}

HRESULT ProxyAudioDevice::ProxyAudio_QueryInterface(void *inDriver, REFIID inUUID, LPVOID *outInterface) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return E_NOINTERFACE;
    }

    return device->QueryInterface(inDriver, inUUID, outInterface);
}

ULONG ProxyAudioDevice::ProxyAudio_AddRef(void *inDriver) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return 0;
    }

    return device->AddRef(inDriver);
}

ULONG ProxyAudioDevice::ProxyAudio_Release(void *inDriver) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return 0;
    }

    return device->Release(inDriver);
}

OSStatus ProxyAudioDevice::ProxyAudio_Initialize(AudioServerPlugInDriverRef inDriver, AudioServerPlugInHostRef inHost) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->Initialize(inDriver, inHost);
}

OSStatus ProxyAudioDevice::ProxyAudio_CreateDevice(AudioServerPlugInDriverRef inDriver,
                                                   CFDictionaryRef inDescription,
                                                   const AudioServerPlugInClientInfo *inClientInfo,
                                                   AudioObjectID *outDeviceObjectID) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->CreateDevice(inDriver, inDescription, inClientInfo, outDeviceObjectID);
}

OSStatus ProxyAudioDevice::ProxyAudio_DestroyDevice(AudioServerPlugInDriverRef inDriver,
                                                    AudioObjectID inDeviceObjectID) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->DestroyDevice(inDriver, inDeviceObjectID);
}

OSStatus ProxyAudioDevice::ProxyAudio_AddDeviceClient(AudioServerPlugInDriverRef inDriver,
                                                      AudioObjectID inDeviceObjectID,
                                                      const AudioServerPlugInClientInfo *inClientInfo) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->AddDeviceClient(inDriver, inDeviceObjectID, inClientInfo);
}

OSStatus ProxyAudioDevice::ProxyAudio_RemoveDeviceClient(AudioServerPlugInDriverRef inDriver,
                                                         AudioObjectID inDeviceObjectID,
                                                         const AudioServerPlugInClientInfo *inClientInfo) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->RemoveDeviceClient(inDriver, inDeviceObjectID, inClientInfo);
}

OSStatus ProxyAudioDevice::ProxyAudio_PerformDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver,
                                                                       AudioObjectID inDeviceObjectID,
                                                                       UInt64 inChangeAction,
                                                                       void *inChangeInfo) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->PerformDeviceConfigurationChange(inDriver, inDeviceObjectID, inChangeAction, inChangeInfo);
}

OSStatus ProxyAudioDevice::ProxyAudio_AbortDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver,
                                                                     AudioObjectID inDeviceObjectID,
                                                                     UInt64 inChangeAction,
                                                                     void *inChangeInfo) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->AbortDeviceConfigurationChange(inDriver, inDeviceObjectID, inChangeAction, inChangeInfo);
}

Boolean ProxyAudioDevice::ProxyAudio_HasProperty(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inObjectID,
                                                 pid_t inClientProcessID,
                                                 const AudioObjectPropertyAddress *inAddress) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return false;
    }

    return device->HasProperty(inDriver, inObjectID, inClientProcessID, inAddress);
}

OSStatus ProxyAudioDevice::ProxyAudio_IsPropertySettable(AudioServerPlugInDriverRef inDriver,
                                                         AudioObjectID inObjectID,
                                                         pid_t inClientProcessID,
                                                         const AudioObjectPropertyAddress *inAddress,
                                                         Boolean *outIsSettable) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->IsPropertySettable(inDriver, inObjectID, inClientProcessID, inAddress, outIsSettable);
}

OSStatus ProxyAudioDevice::ProxyAudio_GetPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                                          AudioObjectID inObjectID,
                                                          pid_t inClientProcessID,
                                                          const AudioObjectPropertyAddress *inAddress,
                                                          UInt32 inQualifierDataSize,
                                                          const void *inQualifierData,
                                                          UInt32 *outDataSize) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->GetPropertyDataSize(
        inDriver, inObjectID, inClientProcessID, inAddress, inQualifierDataSize, inQualifierData, outDataSize);
}

OSStatus ProxyAudioDevice::ProxyAudio_GetPropertyData(AudioServerPlugInDriverRef inDriver,
                                                      AudioObjectID inObjectID,
                                                      pid_t inClientProcessID,
                                                      const AudioObjectPropertyAddress *inAddress,
                                                      UInt32 inQualifierDataSize,
                                                      const void *inQualifierData,
                                                      UInt32 inDataSize,
                                                      UInt32 *outDataSize,
                                                      void *outData) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->GetPropertyData(inDriver,
                                   inObjectID,
                                   inClientProcessID,
                                   inAddress,
                                   inQualifierDataSize,
                                   inQualifierData,
                                   inDataSize,
                                   outDataSize,
                                   outData);
}

OSStatus ProxyAudioDevice::ProxyAudio_SetPropertyData(AudioServerPlugInDriverRef inDriver,
                                                      AudioObjectID inObjectID,
                                                      pid_t inClientProcessID,
                                                      const AudioObjectPropertyAddress *inAddress,
                                                      UInt32 inQualifierDataSize,
                                                      const void *inQualifierData,
                                                      UInt32 inDataSize,
                                                      const void *inData) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->SetPropertyData(
        inDriver, inObjectID, inClientProcessID, inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
}

OSStatus ProxyAudioDevice::ProxyAudio_StartIO(AudioServerPlugInDriverRef inDriver,
                                              AudioObjectID inDeviceObjectID,
                                              UInt32 inClientID) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->StartIO(inDriver, inDeviceObjectID, inClientID);
}

OSStatus ProxyAudioDevice::ProxyAudio_StopIO(AudioServerPlugInDriverRef inDriver,
                                             AudioObjectID inDeviceObjectID,
                                             UInt32 inClientID) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->StopIO(inDriver, inDeviceObjectID, inClientID);
}

OSStatus ProxyAudioDevice::ProxyAudio_GetZeroTimeStamp(AudioServerPlugInDriverRef inDriver,
                                                       AudioObjectID inDeviceObjectID,
                                                       UInt32 inClientID,
                                                       Float64 *outSampleTime,
                                                       UInt64 *outHostTime,
                                                       UInt64 *outSeed) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->GetZeroTimeStamp(inDriver, inDeviceObjectID, inClientID, outSampleTime, outHostTime, outSeed);
}

OSStatus ProxyAudioDevice::ProxyAudio_WillDoIOOperation(AudioServerPlugInDriverRef inDriver,
                                                        AudioObjectID inDeviceObjectID,
                                                        UInt32 inClientID,
                                                        UInt32 inOperationID,
                                                        Boolean *outWillDo,
                                                        Boolean *outWillDoInPlace) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->WillDoIOOperation(
        inDriver, inDeviceObjectID, inClientID, inOperationID, outWillDo, outWillDoInPlace);
}

OSStatus ProxyAudioDevice::ProxyAudio_BeginIOOperation(AudioServerPlugInDriverRef inDriver,
                                                       AudioObjectID inDeviceObjectID,
                                                       UInt32 inClientID,
                                                       UInt32 inOperationID,
                                                       UInt32 inIOBufferFrameSize,
                                                       const AudioServerPlugInIOCycleInfo *inIOCycleInfo) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->BeginIOOperation(
        inDriver, inDeviceObjectID, inClientID, inOperationID, inIOBufferFrameSize, inIOCycleInfo);
}

OSStatus ProxyAudioDevice::ProxyAudio_DoIOOperation(AudioServerPlugInDriverRef inDriver,
                                                    AudioObjectID inDeviceObjectID,
                                                    AudioObjectID inStreamObjectID,
                                                    UInt32 inClientID,
                                                    UInt32 inOperationID,
                                                    UInt32 inIOBufferFrameSize,
                                                    const AudioServerPlugInIOCycleInfo *inIOCycleInfo,
                                                    void *ioMainBuffer,
                                                    void *ioSecondaryBuffer) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->DoIOOperation(inDriver,
                                 inDeviceObjectID,
                                 inStreamObjectID,
                                 inClientID,
                                 inOperationID,
                                 inIOBufferFrameSize,
                                 inIOCycleInfo,
                                 ioMainBuffer,
                                 ioSecondaryBuffer);
}

OSStatus ProxyAudioDevice::ProxyAudio_EndIOOperation(AudioServerPlugInDriverRef inDriver,
                                                     AudioObjectID inDeviceObjectID,
                                                     UInt32 inClientID,
                                                     UInt32 inOperationID,
                                                     UInt32 inIOBufferFrameSize,
                                                     const AudioServerPlugInIOCycleInfo *inIOCycleInfo) {
    ProxyAudioDevice *device = ProxyAudioDevice::deviceForDriver(inDriver);

    if (!device) {
        return kAudioHardwareBadObjectError;
    }

    return device->EndIOOperation(
        inDriver, inDeviceObjectID, inClientID, inOperationID, inIOBufferFrameSize, inIOCycleInfo);
}

#pragma mark Inheritence

HRESULT ProxyAudioDevice::QueryInterface(void *inDriver, REFIID inUUID, LPVOID *outInterface) {
    //    This function is called by the HAL to get the interface to talk to the plug-in through.
    //    AudioServerPlugIns are required to support the IUnknown interface and the
    //    AudioServerPlugInDriverInterface. As it happens, all interfaces must also provide the
    //    IUnknown interface, so we can always just return the single interface we made with
    //    gAudioServerPlugInDriverInterfacePtr regardless of which one is asked for.

    //    declare the local variables
    HRESULT theAnswer = 0;
    CFUUIDRef theRequestedUUID = NULL;

    //    validate the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_QueryInterface: bad driver reference");
    FailWithAction(outInterface == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "ProxyAudio_QueryInterface: no place to store the returned interface");

    //    make a CFUUIDRef from inUUID
    theRequestedUUID = CFUUIDCreateFromUUIDBytes(NULL, inUUID);
    FailWithAction(theRequestedUUID == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "ProxyAudio_QueryInterface: failed to create the CFUUIDRef");

    //    AudioServerPlugIns only support two interfaces, IUnknown (which has to be supported by all
    //    CFPlugIns and AudioServerPlugInDriverInterface (which is the actual interface the HAL will
    //    use).
    if (CFEqual(theRequestedUUID, IUnknownUUID) || CFEqual(theRequestedUUID, kAudioServerPlugInDriverInterfaceUUID)) {
        CAMutex::Locker locker(stateMutex);
        ++gPlugIn_RefCount;
        *outInterface = gAudioServerPlugInDriverRef;
    } else {
        theAnswer = E_NOINTERFACE;
    }

    //    make sure to release the UUID we created
    CFRelease(theRequestedUUID);

Done:
    return theAnswer;
}

ULONG ProxyAudioDevice::AddRef(void *inDriver) {
    //    This call returns the resulting reference count after the increment.

    //    declare the local variables
    ULONG theAnswer = 0;

    //    check the arguments
    FailIf(inDriver != gAudioServerPlugInDriverRef, Done, "ProxyAudio_AddRef: bad driver reference");

    //    decrement the refcount
    {
        CAMutex::Locker locker(stateMutex);
        if (gPlugIn_RefCount < UINT32_MAX) {
            ++gPlugIn_RefCount;
        }
        theAnswer = gPlugIn_RefCount;
    }
Done:
    return theAnswer;
}

ULONG ProxyAudioDevice::Release(void *inDriver) {
    //    This call returns the resulting reference count after the decrement.

    //    declare the local variables
    ULONG theAnswer = 0;

    //    check the arguments
    FailIf(inDriver != gAudioServerPlugInDriverRef, Done, "ProxyAudio_Release: bad driver reference");

    //    increment the refcount
    {
        CAMutex::Locker locker(stateMutex);
        if (gPlugIn_RefCount > 0) {
            --gPlugIn_RefCount;
            //    Note that we don't do anything special if the refcount goes to zero as the HAL
            //    will never fully release a plug-in it opens. We keep managing the refcount so that
            //    the API semantics are correct though.
        }
        theAnswer = gPlugIn_RefCount;
    }

Done:
    return theAnswer;
}

#pragma mark Basic Operations

OSStatus ProxyAudioDevice::Initialize(AudioServerPlugInDriverRef inDriver, AudioServerPlugInHostRef inHost) {
    //    The job of this method is, as the name implies, to get the driver initialized. One specific
    //    thing that needs to be done is to store the AudioServerPlugInHostRef so that it can be used
    //    later. Note that when this call returns, the HAL will scan the various lists the driver
    //    maintains (such as the device list) to get the inital set of objects the driver is
    //    publishing. So, there is no need to notifiy the HAL about any objects created as part of the
    //    execution of this method.

    //    declare the local variables
    OSStatus theAnswer = 0;
    DebugMsg("ProxyAudio: ProxyAudio_Initialize");

    //    check the arguments
    if (inDriver != gAudioServerPlugInDriverRef) {
        DebugMsg("ProxyAudio: ProxyAudio_Initialize: bad driver reference");
        { theAnswer = kAudioHardwareBadObjectError; }
        return theAnswer;
    }

    //    store the AudioServerPlugInHostRef
    gPlugIn_Host = inHost;

    //    initialize the box acquired property from the settings
    CFPropertyListRef theSettingsData = NULL;
    gPlugIn_Host->CopyFromStorage(gPlugIn_Host, CFSTR("box acquired"), &theSettingsData);
    if (theSettingsData != NULL) {
        if (CFGetTypeID(theSettingsData) == CFBooleanGetTypeID()) {
            gBox_Acquired = CFBooleanGetValue((CFBooleanRef)theSettingsData);
        } else if (CFGetTypeID(theSettingsData) == CFNumberGetTypeID()) {
            SInt32 theValue = 0;
            CFNumberGetValue((CFNumberRef)theSettingsData, kCFNumberSInt32Type, &theValue);
            gBox_Acquired = theValue ? 1 : 0;
        }
        CFRelease(theSettingsData);
    }

    //    initialize the box name from the settings
    gPlugIn_Host->CopyFromStorage(gPlugIn_Host, CFSTR("box acquired"), &theSettingsData);
    if (theSettingsData != NULL) {
        if (CFGetTypeID(theSettingsData) == CFStringGetTypeID()) {
            boxName = CFStringCreateCopy(NULL, (CFStringRef)theSettingsData);
        }
        CFRelease(theSettingsData);
    }

    //    set the box name directly as a last resort
    if (boxName == NULL) {
        boxName = CFStringCreateCopy(NULL, CFSTR("Proxy Audio Box"));
    }

    dispatch_queue_attr_t priorityAttribute = dispatch_queue_attr_make_with_qos_class(
        DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, -1
    );
    audioOutputQueue = dispatch_queue_create("net.briankendall.ProxyAudioDevice.audioOutputQueue", priorityAttribute);
    
    deviceName = copyDeviceNameFromStorage();
    outputDeviceUID = copyOutputDeviceUIDFromStorage();
    outputDeviceBufferFrameSize = retrieveOutputDeviceBufferFrameSizeFromStorage();

    //    calculate the host ticks per frame
    struct mach_timebase_info theTimeBaseInfo;
    mach_timebase_info(&theTimeBaseInfo);
    Float64 theHostClockFrequency = theTimeBaseInfo.denom / theTimeBaseInfo.numer;
    theHostClockFrequency *= 1000000000.0;
    gDevice_HostTicksPerFrame = theHostClockFrequency / gDevice_SampleRate;

    inputBuffer = new AudioRingBuffer(gDevice_BytesPerFrameInChannel * gDevice_ChannelsPerFrame, 88200);
    workBuffer = new Byte[gDevice_BytesPerFrameInChannel * gDevice_ChannelsPerFrame * kDevice_RingBufferSize * 2];

    initializeOutputDevice();

    return theAnswer;
}

OSStatus ProxyAudioDevice::CreateDevice(AudioServerPlugInDriverRef inDriver,
                                        CFDictionaryRef inDescription,
                                        const AudioServerPlugInClientInfo *inClientInfo,
                                        AudioObjectID *outDeviceObjectID) {
    //    This method is used to tell a driver that implements the Transport Manager semantics to
    //    create an AudioEndpointDevice from a set of AudioEndpoints. Since this driver is not a
    //    Transport Manager, we just check the arguments and return
    //    kAudioHardwareUnsupportedOperationError.

#pragma unused(inDescription, inClientInfo, outDeviceObjectID)

    //    declare the local variables
    OSStatus theAnswer = kAudioHardwareUnsupportedOperationError;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_CreateDevice: bad driver reference");

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::DestroyDevice(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID) {
    //    This method is used to tell a driver that implements the Transport Manager semantics to
    //    destroy an AudioEndpointDevice. Since this driver is not a Transport Manager, we just check
    //    the arguments and return kAudioHardwareUnsupportedOperationError.

#pragma unused(inDeviceObjectID)

    //    declare the local variables
    OSStatus theAnswer = kAudioHardwareUnsupportedOperationError;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_DestroyDevice: bad driver reference");

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::AddDeviceClient(AudioServerPlugInDriverRef inDriver,
                                           AudioObjectID inDeviceObjectID,
                                           const AudioServerPlugInClientInfo *inClientInfo) {
    //    This method is used to inform the driver about a new client that is using the given device.
    //    This allows the device to act differently depending on who the client is. This driver does
    //    not need to track the clients using the device, so we just check the arguments and return
    //    successfully.

#pragma unused(inClientInfo)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_AddDeviceClient: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_AddDeviceClient: bad device ID");

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::RemoveDeviceClient(AudioServerPlugInDriverRef inDriver,
                                              AudioObjectID inDeviceObjectID,
                                              const AudioServerPlugInClientInfo *inClientInfo) {
    //    This method is used to inform the driver about a client that is no longer using the given
    //    device. This driver does not track clients, so we just check the arguments and return
    //    successfully.

#pragma unused(inClientInfo)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_RemoveDeviceClient: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_RemoveDeviceClient: bad device ID");

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::PerformDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver,
                                                            AudioObjectID inDeviceObjectID,
                                                            UInt64 inChangeAction,
                                                            void *inChangeInfo) {
    //    This method is called to tell the device that it can perform the configuation change that it
    //    had requested via a call to the host method, RequestDeviceConfigurationChange(). The
    //    arguments, inChangeAction and inChangeInfo are the same as what was passed to
    //    RequestDeviceConfigurationChange().
    //
    //    The HAL guarantees that IO will be stopped while this method is in progress. The HAL will
    //    also handle figuring out exactly what changed for the non-control related properties. This
    //    means that the only notifications that would need to be sent here would be for either
    //    custom properties the HAL doesn't know about or for controls.
    //
    //    For the device implemented by this driver, only sample rate changes go through this process
    //    as it is the only state that can be changed for the device that isn't a control. For this
    //    change, the new sample rate is passed in the inChangeAction argument.

#pragma unused(inChangeInfo)

    //    declare the local variables
    OSStatus theAnswer = 0;
    struct mach_timebase_info theTimeBaseInfo;
    Float64 theHostClockFrequency = 0;

    DebugMsg("ProxyAudio: PerformDeviceConfigurationChange");
    
    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_PerformDeviceConfigurationChange: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_PerformDeviceConfigurationChange: bad device ID");
    FailWithAction(!contains(gDevice_SampleRates, (Float64)inChangeAction),
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_PerformDeviceConfigurationChange: bad sample rate");

    //    lock the state mutex
    {
        CAMutex::Locker locker(stateMutex);

        //    change the sample rate
        gDevice_SampleRate = inChangeAction;
        DebugMsg("ProxyAudio: Setting sample rate to: %llu", inChangeAction);

        //    recalculate the state that depends on the sample rate
        mach_timebase_info(&theTimeBaseInfo);
        theHostClockFrequency = theTimeBaseInfo.denom / theTimeBaseInfo.numer;
        theHostClockFrequency *= 1000000000.0;
        gDevice_HostTicksPerFrame = theHostClockFrequency / gDevice_SampleRate;
    }

    DebugMsg("ProxyAudio: finished PerformDeviceConfigurationChange, will match sample rate");
    matchOutputDeviceSampleRate();

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::AbortDeviceConfigurationChange(AudioServerPlugInDriverRef inDriver,
                                                          AudioObjectID inDeviceObjectID,
                                                          UInt64 inChangeAction,
                                                          void *inChangeInfo) {
    //    This method is called to tell the driver that a request for a config change has been denied.
    //    This provides the driver an opportunity to clean up any state associated with the request.
    //    For this driver, an aborted config change requires no action. So we just check the arguments
    //    and return

#pragma unused(inChangeAction, inChangeInfo)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_PerformDeviceConfigurationChange: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "ProxyAudio_PerformDeviceConfigurationChange: bad device ID");

    syslog(LOG_ERR,
           "ProxyAudio error: was not able to change the sample rate of Proxy Audio Device to %llu",
           inChangeAction);

Done:
    return theAnswer;
}

#pragma mark Property Operations

Boolean ProxyAudioDevice::HasProperty(AudioServerPlugInDriverRef inDriver,
                                      AudioObjectID inObjectID,
                                      pid_t inClientProcessID,
                                      const AudioObjectPropertyAddress *inAddress) {
    //    This method returns whether or not the given object has the given property.

    //    declare the local variables
    Boolean theAnswer = false;

    //    check the arguments
    FailIf(inDriver != gAudioServerPlugInDriverRef, Done, "ProxyAudio_HasProperty: bad driver reference");
    FailIf(inAddress == NULL, Done, "ProxyAudio_HasProperty: no address");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the ProxyAudio_GetPropertyData() method.
    switch (inObjectID) {
        case kObjectID_PlugIn:
            theAnswer = HasPlugInProperty(inDriver, inObjectID, inClientProcessID, inAddress);
            break;

        case kObjectID_Box:
            theAnswer = HasBoxProperty(inDriver, inObjectID, inClientProcessID, inAddress);
            break;

        case kObjectID_Device:
            theAnswer = HasDeviceProperty(inDriver, inObjectID, inClientProcessID, inAddress);
            break;

        case kObjectID_Stream_Output:
            theAnswer = HasStreamProperty(inDriver, inObjectID, inClientProcessID, inAddress);
            break;

        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
        case kObjectID_Mute_Output_Master:
        case kObjectID_DataSource_Output_Master:
            theAnswer = HasControlProperty(inDriver, inObjectID, inClientProcessID, inAddress);
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::IsPropertySettable(AudioServerPlugInDriverRef inDriver,
                                              AudioObjectID inObjectID,
                                              pid_t inClientProcessID,
                                              const AudioObjectPropertyAddress *inAddress,
                                              Boolean *outIsSettable) {
    //    This method returns whether or not the given property on the object can have its value
    //    changed.

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsPropertySettable: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "IsPropertySettable: no address");
    FailWithAction(outIsSettable == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsPropertySettable: no place to put the return value");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetPropertyData() method.
    switch (inObjectID) {
        case kObjectID_PlugIn:
            theAnswer = IsPlugInPropertySettable(inDriver, inObjectID, inClientProcessID, inAddress, outIsSettable);
            break;

        case kObjectID_Box:
            theAnswer = IsBoxPropertySettable(inDriver, inObjectID, inClientProcessID, inAddress, outIsSettable);
            break;

        case kObjectID_Device:
            theAnswer = IsDevicePropertySettable(inDriver, inObjectID, inClientProcessID, inAddress, outIsSettable);
            break;

        case kObjectID_Stream_Output:
            theAnswer = IsStreamPropertySettable(inDriver, inObjectID, inClientProcessID, inAddress, outIsSettable);
            break;

        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
        case kObjectID_Mute_Output_Master:
        case kObjectID_DataSource_Output_Master:
            theAnswer = IsControlPropertySettable(inDriver, inObjectID, inClientProcessID, inAddress, outIsSettable);
            break;

        default:
            theAnswer = kAudioHardwareBadObjectError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                               AudioObjectID inObjectID,
                                               pid_t inClientProcessID,
                                               const AudioObjectPropertyAddress *inAddress,
                                               UInt32 inQualifierDataSize,
                                               const void *inQualifierData,
                                               UInt32 *outDataSize) {
    //    This method returns the byte size of the property's data.

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetPropertyDataSize: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "GetPropertyDataSize: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetPropertyDataSize: no place to put the return value");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetPropertyData() method.
    switch (inObjectID) {
        case kObjectID_PlugIn:
            theAnswer = GetPlugInPropertyDataSize(
                inDriver, inObjectID, inClientProcessID, inAddress, inQualifierDataSize, inQualifierData, outDataSize);
            break;

        case kObjectID_Box:
            theAnswer = GetBoxPropertyDataSize(
                inDriver, inObjectID, inClientProcessID, inAddress, inQualifierDataSize, inQualifierData, outDataSize);
            break;

        case kObjectID_Device:
            theAnswer = GetDevicePropertyDataSize(
                inDriver, inObjectID, inClientProcessID, inAddress, inQualifierDataSize, inQualifierData, outDataSize);
            break;

        case kObjectID_Stream_Output:
            theAnswer = GetStreamPropertyDataSize(
                inDriver, inObjectID, inClientProcessID, inAddress, inQualifierDataSize, inQualifierData, outDataSize);
            break;

        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
        case kObjectID_Mute_Output_Master:
        case kObjectID_DataSource_Output_Master:
            theAnswer = GetControlPropertyDataSize(
                inDriver, inObjectID, inClientProcessID, inAddress, inQualifierDataSize, inQualifierData, outDataSize);
            break;

        default:
            theAnswer = kAudioHardwareBadObjectError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetPropertyData(AudioServerPlugInDriverRef inDriver,
                                           AudioObjectID inObjectID,
                                           pid_t inClientProcessID,
                                           const AudioObjectPropertyAddress *inAddress,
                                           UInt32 inQualifierDataSize,
                                           const void *inQualifierData,
                                           UInt32 inDataSize,
                                           UInt32 *outDataSize,
                                           void *outData) {
    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "GetPropertyData: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetPropertyData: no place to put the return value size");
    FailWithAction(outData == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetPropertyData: no place to put the return value");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required.
    //
    //    Also, since most of the data that will get returned is static, there are few instances where
    //    it is necessary to lock the state mutex.
    switch (inObjectID) {
        case kObjectID_PlugIn:
            theAnswer = GetPlugInPropertyData(inDriver,
                                              inObjectID,
                                              inClientProcessID,
                                              inAddress,
                                              inQualifierDataSize,
                                              inQualifierData,
                                              inDataSize,
                                              outDataSize,
                                              outData);
            break;

        case kObjectID_Box:
            theAnswer = GetBoxPropertyData(inDriver,
                                           inObjectID,
                                           inClientProcessID,
                                           inAddress,
                                           inQualifierDataSize,
                                           inQualifierData,
                                           inDataSize,
                                           outDataSize,
                                           outData);
            break;

        case kObjectID_Device:
            theAnswer = GetDevicePropertyData(inDriver,
                                              inObjectID,
                                              inClientProcessID,
                                              inAddress,
                                              inQualifierDataSize,
                                              inQualifierData,
                                              inDataSize,
                                              outDataSize,
                                              outData);
            break;

        case kObjectID_Stream_Output:
            theAnswer = GetStreamPropertyData(inDriver,
                                              inObjectID,
                                              inClientProcessID,
                                              inAddress,
                                              inQualifierDataSize,
                                              inQualifierData,
                                              inDataSize,
                                              outDataSize,
                                              outData);
            break;

        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
        case kObjectID_Mute_Output_Master:
        case kObjectID_DataSource_Output_Master:
            theAnswer = GetControlPropertyData(inDriver,
                                               inObjectID,
                                               inClientProcessID,
                                               inAddress,
                                               inQualifierDataSize,
                                               inQualifierData,
                                               inDataSize,
                                               outDataSize,
                                               outData);
            break;

        default:
            theAnswer = kAudioHardwareBadObjectError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::SetPropertyData(AudioServerPlugInDriverRef inDriver,
                                           AudioObjectID inObjectID,
                                           pid_t inClientProcessID,
                                           const AudioObjectPropertyAddress *inAddress,
                                           UInt32 inQualifierDataSize,
                                           const void *inQualifierData,
                                           UInt32 inDataSize,
                                           const void *inData) {
    //    declare the local variables
    OSStatus theAnswer = 0;
    UInt32 theNumberPropertiesChanged = 0;
    AudioObjectPropertyAddress theChangedAddresses[2];

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "SetPropertyData: no address");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetPropertyData() method.
    switch (inObjectID) {
        case kObjectID_PlugIn:
            theAnswer = SetPlugInPropertyData(inDriver,
                                              inObjectID,
                                              inClientProcessID,
                                              inAddress,
                                              inQualifierDataSize,
                                              inQualifierData,
                                              inDataSize,
                                              inData,
                                              &theNumberPropertiesChanged,
                                              theChangedAddresses);
            break;

        case kObjectID_Box:
            theAnswer = SetBoxPropertyData(inDriver,
                                           inObjectID,
                                           inClientProcessID,
                                           inAddress,
                                           inQualifierDataSize,
                                           inQualifierData,
                                           inDataSize,
                                           inData,
                                           &theNumberPropertiesChanged,
                                           theChangedAddresses);
            break;

        case kObjectID_Device:
            theAnswer = SetDevicePropertyData(inDriver,
                                              inObjectID,
                                              inClientProcessID,
                                              inAddress,
                                              inQualifierDataSize,
                                              inQualifierData,
                                              inDataSize,
                                              inData,
                                              &theNumberPropertiesChanged,
                                              theChangedAddresses);
            break;

        case kObjectID_Stream_Output:
            theAnswer = SetStreamPropertyData(inDriver,
                                              inObjectID,
                                              inClientProcessID,
                                              inAddress,
                                              inQualifierDataSize,
                                              inQualifierData,
                                              inDataSize,
                                              inData,
                                              &theNumberPropertiesChanged,
                                              theChangedAddresses);
            break;

        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
        case kObjectID_Mute_Output_Master:
        case kObjectID_DataSource_Output_Master:
            theAnswer = SetControlPropertyData(inDriver,
                                               inObjectID,
                                               inClientProcessID,
                                               inAddress,
                                               inQualifierDataSize,
                                               inQualifierData,
                                               inDataSize,
                                               inData,
                                               &theNumberPropertiesChanged,
                                               theChangedAddresses);
            break;

        default:
            theAnswer = kAudioHardwareBadObjectError;
            break;
    };

    //    send any notifications
    if (theNumberPropertiesChanged > 0) {
        gPlugIn_Host->PropertiesChanged(gPlugIn_Host, inObjectID, theNumberPropertiesChanged, theChangedAddresses);
    }

Done:
    return theAnswer;
}

#pragma mark PlugIn Property Operations

Boolean ProxyAudioDevice::HasPlugInProperty(AudioServerPlugInDriverRef inDriver,
                                            AudioObjectID inObjectID,
                                            pid_t inClientProcessID,
                                            const AudioObjectPropertyAddress *inAddress) {
    //    This method returns whether or not the plug-in object has the given property.

#pragma unused(inClientProcessID)

    //    declare the local variables
    Boolean theAnswer = false;

    //    check the arguments
    FailIf(inDriver != gAudioServerPlugInDriverRef, Done, "HasPlugInProperty: bad driver reference");
    FailIf(inAddress == NULL, Done, "HasPlugInProperty: no address");
    FailIf(inObjectID != kObjectID_PlugIn, Done, "HasPlugInProperty: not the plug-in object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetPlugInPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyManufacturer:
        case kAudioObjectPropertyOwnedObjects:
        case kAudioPlugInPropertyBoxList:
        case kAudioPlugInPropertyTranslateUIDToBox:
        case kAudioPlugInPropertyDeviceList:
        case kAudioPlugInPropertyTranslateUIDToDevice:
        case kAudioPlugInPropertyResourceBundle:
            theAnswer = true;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::IsPlugInPropertySettable(AudioServerPlugInDriverRef inDriver,
                                                    AudioObjectID inObjectID,
                                                    pid_t inClientProcessID,
                                                    const AudioObjectPropertyAddress *inAddress,
                                                    Boolean *outIsSettable) {
    //    This method returns whether or not the given property on the plug-in object can have its
    //    value changed.

#pragma unused(inClientProcessID)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsPlugInPropertySettable: bad driver reference");
    FailWithAction(inAddress == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsPlugInPropertySettable: no address");
    FailWithAction(outIsSettable == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsPlugInPropertySettable: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_PlugIn,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsPlugInPropertySettable: not the plug-in object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetPlugInPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyManufacturer:
        case kAudioObjectPropertyOwnedObjects:
        case kAudioPlugInPropertyBoxList:
        case kAudioPlugInPropertyTranslateUIDToBox:
        case kAudioPlugInPropertyDeviceList:
        case kAudioPlugInPropertyTranslateUIDToDevice:
        case kAudioPlugInPropertyResourceBundle:
            *outIsSettable = false;
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetPlugInPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                                     AudioObjectID inObjectID,
                                                     pid_t inClientProcessID,
                                                     const AudioObjectPropertyAddress *inAddress,
                                                     UInt32 inQualifierDataSize,
                                                     const void *inQualifierData,
                                                     UInt32 *outDataSize) {
    //    This method returns the byte size of the property's data.

#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetPlugInPropertyDataSize: bad driver reference");
    FailWithAction(inAddress == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetPlugInPropertyDataSize: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetPlugInPropertyDataSize: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_PlugIn,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetPlugInPropertyDataSize: not the plug-in object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetPlugInPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyManufacturer:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyOwnedObjects:
            if (gBox_Acquired) {
                *outDataSize = 2 * sizeof(AudioClassID);
            } else {
                *outDataSize = sizeof(AudioClassID);
            }
            break;

        case kAudioPlugInPropertyBoxList:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioPlugInPropertyTranslateUIDToBox:
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioPlugInPropertyDeviceList:
            if (gBox_Acquired) {
                *outDataSize = sizeof(AudioClassID);
            } else {
                *outDataSize = 0;
            }
            break;

        case kAudioPlugInPropertyTranslateUIDToDevice:
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioPlugInPropertyResourceBundle:
            *outDataSize = sizeof(CFStringRef);
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetPlugInPropertyData(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inObjectID,
                                                 pid_t inClientProcessID,
                                                 const AudioObjectPropertyAddress *inAddress,
                                                 UInt32 inQualifierDataSize,
                                                 const void *inQualifierData,
                                                 UInt32 inDataSize,
                                                 UInt32 *outDataSize,
                                                 void *outData) {
#pragma unused(inClientProcessID)

    //    declare the local variables
    OSStatus theAnswer = 0;
    UInt32 theNumberItemsToFetch;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetPlugInPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "GetPlugInPropertyData: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetPlugInPropertyData: no place to put the return value size");
    FailWithAction(outData == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetPlugInPropertyData: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_PlugIn,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetPlugInPropertyData: not the plug-in object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required.
    //
    //    Also, since most of the data that will get returned is static, there are few instances where
    //    it is necessary to lock the state mutex.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
            //    The base class for kAudioPlugInClassID is kAudioObjectClassID
            FailWithAction(inDataSize < sizeof(AudioClassID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetPlugInPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyBaseClass for the plug-in");
            *((AudioClassID *)outData) = kAudioObjectClassID;
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            //    The class is always kAudioPlugInClassID for regular drivers
            FailWithAction(inDataSize < sizeof(AudioClassID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetPlugInPropertyData: not enough space for the return value of kAudioObjectPropertyClass "
                           "for the plug-in");
            *((AudioClassID *)outData) = kAudioPlugInClassID;
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            //    The plug-in doesn't have an owning object
            FailWithAction(inDataSize < sizeof(AudioObjectID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetPlugInPropertyData: not enough space for the return value of kAudioObjectPropertyOwner "
                           "for the plug-in");
            *((AudioObjectID *)outData) = kAudioObjectUnknown;
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyManufacturer:
            //    This is the human readable name of the maker of the plug-in.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetPlugInPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyManufacturer for the plug-in");
            *((CFStringRef *)outData) = CFSTR("Apple Inc.");
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyOwnedObjects:
            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);

            //    Clamp that to the number of boxes this driver implements (which is just 1)
            if (theNumberItemsToFetch > (gBox_Acquired ? 2 : 1)) {
                theNumberItemsToFetch = (gBox_Acquired ? 2 : 1);
            }

            //    Write the devices' object IDs into the return value
            if (theNumberItemsToFetch > 1) {
                ((AudioObjectID *)outData)[0] = kObjectID_Box;
                ((AudioObjectID *)outData)[0] = kObjectID_Device;
            } else if (theNumberItemsToFetch > 0) {
                ((AudioObjectID *)outData)[0] = kObjectID_Box;
            }

            //    Return how many bytes we wrote to
            *outDataSize = theNumberItemsToFetch * sizeof(AudioClassID);
            break;

        case kAudioPlugInPropertyBoxList:
            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);

            //    Clamp that to the number of boxes this driver implements (which is just 1)
            if (theNumberItemsToFetch > 1) {
                theNumberItemsToFetch = 1;
            }

            //    Write the devices' object IDs into the return value
            if (theNumberItemsToFetch > 0) {
                ((AudioObjectID *)outData)[0] = kObjectID_Box;
            }

            //    Return how many bytes we wrote to
            *outDataSize = theNumberItemsToFetch * sizeof(AudioClassID);
            break;

        case kAudioPlugInPropertyTranslateUIDToBox:
            //    This property takes the CFString passed in the qualifier and converts that
            //    to the object ID of the box it corresponds to. For this driver, there is
            //    just the one box. Note that it is not an error if the string in the
            //    qualifier doesn't match any devices. In such case, kAudioObjectUnknown is
            //    the object ID to return.
            FailWithAction(inDataSize < sizeof(AudioObjectID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetPlugInPropertyData: not enough space for the return value of "
                           "kAudioPlugInPropertyTranslateUIDToBox");
            FailWithAction(
                inQualifierDataSize == sizeof(CFStringRef),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetPlugInPropertyData: the qualifier is the wrong size for kAudioPlugInPropertyTranslateUIDToBox");
            FailWithAction(inQualifierData == NULL,
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetPlugInPropertyData: no qualifier for kAudioPlugInPropertyTranslateUIDToBox");
            
            if (CFStringCompare(*((CFStringRef *)inQualifierData), CFSTR(kBox_UID), 0) == kCFCompareEqualTo) {
                *((AudioObjectID *)outData) = kObjectID_Box;
            } else {
                *((AudioObjectID *)outData) = kAudioObjectUnknown;
            }
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioPlugInPropertyDeviceList:
            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);

            //    Clamp that to the number of devices this driver implements (which is just 1 if the
            //    box has been acquired)
            if (theNumberItemsToFetch > (gBox_Acquired ? 1 : 0)) {
                theNumberItemsToFetch = (gBox_Acquired ? 1 : 0);
            }

            //    Write the devices' object IDs into the return value
            if (theNumberItemsToFetch > 0) {
                ((AudioObjectID *)outData)[0] = kObjectID_Device;
            }

            //    Return how many bytes we wrote to
            *outDataSize = theNumberItemsToFetch * sizeof(AudioClassID);
            break;

        case kAudioPlugInPropertyTranslateUIDToDevice:
            //    This property takes the CFString passed in the qualifier and converts that
            //    to the object ID of the device it corresponds to. For this driver, there is
            //    just the one device. Note that it is not an error if the string in the
            //    qualifier doesn't match any devices. In such case, kAudioObjectUnknown is
            //    the object ID to return.
            FailWithAction(inDataSize < sizeof(AudioObjectID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetPlugInPropertyData: not enough space for the return value of "
                           "kAudioPlugInPropertyTranslateUIDToDevice");
            FailWithAction(
                inQualifierDataSize == sizeof(CFStringRef),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetPlugInPropertyData: the qualifier is the wrong size for kAudioPlugInPropertyTranslateUIDToDevice");
            FailWithAction(inQualifierData == NULL,
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetPlugInPropertyData: no qualifier for kAudioPlugInPropertyTranslateUIDToDevice");
            
            if (CFStringCompare(*((CFStringRef *)inQualifierData), CFSTR(kDevice_UID), 0) == kCFCompareEqualTo) {
                *((AudioObjectID *)outData) = kObjectID_Device;
            } else {
                *((AudioObjectID *)outData) = kAudioObjectUnknown;
            }
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioPlugInPropertyResourceBundle:
            //    The resource bundle is a path relative to the path of the plug-in's bundle.
            //    To specify that the plug-in bundle itself should be used, we just return the
            //    empty string.
            FailWithAction(
                inDataSize < sizeof(AudioObjectID),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetPlugInPropertyData: not enough space for the return value of kAudioPlugInPropertyResourceBundle");
            *((CFStringRef *)outData) = CFSTR("");
            *outDataSize = sizeof(CFStringRef);
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::SetPlugInPropertyData(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inObjectID,
                                                 pid_t inClientProcessID,
                                                 const AudioObjectPropertyAddress *inAddress,
                                                 UInt32 inQualifierDataSize,
                                                 const void *inQualifierData,
                                                 UInt32 inDataSize,
                                                 const void *inData,
                                                 UInt32 *outNumberPropertiesChanged,
                                                 AudioObjectPropertyAddress outChangedAddresses[2]) {
#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData, inDataSize, inData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetPlugInPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "SetPlugInPropertyData: no address");
    FailWithAction(outNumberPropertiesChanged == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetPlugInPropertyData: no place to return the number of properties that changed");
    FailWithAction(outChangedAddresses == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetPlugInPropertyData: no place to return the properties that changed");
    FailWithAction(inObjectID != kObjectID_PlugIn,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetPlugInPropertyData: not the plug-in object");

    //    initialize the returned number of changed properties
    *outNumberPropertiesChanged = 0;

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetPlugInPropertyData() method.
    switch (inAddress->mSelector) {
        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

#pragma mark Box Property Operations

Boolean ProxyAudioDevice::HasBoxProperty(AudioServerPlugInDriverRef inDriver,
                                         AudioObjectID inObjectID,
                                         pid_t inClientProcessID,
                                         const AudioObjectPropertyAddress *inAddress) {
    //    This method returns whether or not the box object has the given property.

#pragma unused(inClientProcessID)

    //    declare the local variables
    Boolean theAnswer = false;

    //    check the arguments
    FailIf(inDriver != gAudioServerPlugInDriverRef, Done, "HasBoxProperty: bad driver reference");
    FailIf(inAddress == NULL, Done, "HasBoxProperty: no address");
    FailIf(inObjectID != kObjectID_Box, Done, "HasBoxProperty: not the box object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetBoxPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyName:
        case kAudioObjectPropertyModelName:
        case kAudioObjectPropertyManufacturer:
        case kAudioObjectPropertyOwnedObjects:
        case kAudioObjectPropertyIdentify:
        case kAudioObjectPropertySerialNumber:
        case kAudioObjectPropertyFirmwareVersion:
        case kAudioBoxPropertyBoxUID:
        case kAudioBoxPropertyTransportType:
        case kAudioBoxPropertyHasAudio:
        case kAudioBoxPropertyHasVideo:
        case kAudioBoxPropertyHasMIDI:
        case kAudioBoxPropertyIsProtected:
        case kAudioBoxPropertyAcquired:
        case kAudioBoxPropertyAcquisitionFailed:
        case kAudioBoxPropertyDeviceList:
            theAnswer = true;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::IsBoxPropertySettable(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inObjectID,
                                                 pid_t inClientProcessID,
                                                 const AudioObjectPropertyAddress *inAddress,
                                                 Boolean *outIsSettable) {
    //    This method returns whether or not the given property on the plug-in object can have its
    //    value changed.

#pragma unused(inClientProcessID)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsBoxPropertySettable: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "IsBoxPropertySettable: no address");
    FailWithAction(outIsSettable == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsBoxPropertySettable: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_Box,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsBoxPropertySettable: not the plug-in object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetBoxPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyModelName:
        case kAudioObjectPropertyManufacturer:
        case kAudioObjectPropertyOwnedObjects:
        case kAudioObjectPropertySerialNumber:
        case kAudioObjectPropertyFirmwareVersion:
        case kAudioBoxPropertyBoxUID:
        case kAudioBoxPropertyTransportType:
        case kAudioBoxPropertyHasAudio:
        case kAudioBoxPropertyHasVideo:
        case kAudioBoxPropertyHasMIDI:
        case kAudioBoxPropertyIsProtected:
        case kAudioBoxPropertyAcquisitionFailed:
        case kAudioBoxPropertyDeviceList:
            *outIsSettable = false;
            break;

        case kAudioObjectPropertyName:
        case kAudioObjectPropertyIdentify:
        case kAudioBoxPropertyAcquired:
            *outIsSettable = true;
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetBoxPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                                  AudioObjectID inObjectID,
                                                  pid_t inClientProcessID,
                                                  const AudioObjectPropertyAddress *inAddress,
                                                  UInt32 inQualifierDataSize,
                                                  const void *inQualifierData,
                                                  UInt32 *outDataSize) {
    //    This method returns the byte size of the property's data.

#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetBoxPropertyDataSize: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "GetBoxPropertyDataSize: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetBoxPropertyDataSize: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_Box,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetBoxPropertyDataSize: not the plug-in object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetBoxPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyName:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyModelName:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyManufacturer:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyOwnedObjects:
            *outDataSize = 0;
            break;

        case kAudioObjectPropertyIdentify:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioObjectPropertySerialNumber:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyFirmwareVersion:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioBoxPropertyBoxUID:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioBoxPropertyTransportType:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyHasAudio:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyHasVideo:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyHasMIDI:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyIsProtected:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyAcquired:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyAcquisitionFailed:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyDeviceList: {
            CAMutex::Locker locker(stateMutex);
            *outDataSize = gBox_Acquired ? sizeof(AudioObjectID) : 0;
        } break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetBoxPropertyData(AudioServerPlugInDriverRef inDriver,
                                              AudioObjectID inObjectID,
                                              pid_t inClientProcessID,
                                              const AudioObjectPropertyAddress *inAddress,
                                              UInt32 inQualifierDataSize,
                                              const void *inQualifierData,
                                              UInt32 inDataSize,
                                              UInt32 *outDataSize,
                                              void *outData) {
#pragma unused(inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetBoxPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "GetBoxPropertyData: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetBoxPropertyData: no place to put the return value size");
    FailWithAction(outData == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetBoxPropertyData: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_Box,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetBoxPropertyData: not the plug-in object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required.
    //
    //    Also, since most of the data that will get returned is static, there are few instances where
    //    it is necessary to lock the state mutex.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
            //    The base class for kAudioBoxClassID is kAudioObjectClassID
            FailWithAction(inDataSize < sizeof(AudioClassID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of kAudioObjectPropertyBaseClass "
                           "for the box");
            *((AudioClassID *)outData) = kAudioObjectClassID;
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            //    The class is always kAudioBoxClassID for regular drivers
            FailWithAction(
                inDataSize < sizeof(AudioClassID),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetBoxPropertyData: not enough space for the return value of kAudioObjectPropertyClass for the box");
            *((AudioClassID *)outData) = kAudioBoxClassID;
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            //    The owner is the plug-in object
            FailWithAction(
                inDataSize < sizeof(AudioObjectID),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetBoxPropertyData: not enough space for the return value of kAudioObjectPropertyOwner for the box");
            *((AudioObjectID *)outData) = kObjectID_PlugIn;
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyName:
            //    This is the human readable name of the maker of the box.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyManufacturer for the box");

            // See the comment in the switch case for 'kAudioObjectPropertyIdentify' in SetBoxPropertyData to get a
            // description of the crazy hackery that is going on here.

            if (inClientProcessID == configuratorPid && nextConfigurationToRead != ConfigType::none) {
                DebugMsg("ProxyAudio: returning config data type %d instead of box name", nextConfigurationToRead);
                *((CFStringRef *)outData) = copyConfigurationValue(nextConfigurationToRead);
                
            } else {
                CAMutex::Locker locker(stateMutex);
                *((CFStringRef *)outData) = CFStringCreateCopy(NULL, boxName);
            }

            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyModelName:
            //    This is the human readable name of the maker of the box.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyManufacturer for the box");
            *((CFStringRef *)outData) = CFSTR("Proxy Audio Model");
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyManufacturer:
            //    This is the human readable name of the maker of the box.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyManufacturer for the box");
            *((CFStringRef *)outData) = CFSTR("Apple Inc.");
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyOwnedObjects:
            //    This returns the objects directly owned by the object. Boxes don't own anything.
            *outDataSize = 0;
            break;

        case kAudioObjectPropertyIdentify:
            //    This is used to highling the device in the UI, but it's value has no meaning
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of kAudioObjectPropertyIdentify "
                           "for the box");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioObjectPropertySerialNumber:
            //    This is the human readable serial number of the box.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertySerialNumber for the box");
            *((CFStringRef *)outData) = CFSTR("00000001");
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyFirmwareVersion:
            //    This is the human readable firmware version of the box.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyFirmwareVersion for the box");
            *((CFStringRef *)outData) = CFSTR("1.0");
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioBoxPropertyBoxUID:
            //    Boxes have UIDs the same as devices
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyManufacturer for the box");
            *((CFStringRef *)outData) = CFSTR(kBox_UID);
            break;

        case kAudioBoxPropertyTransportType:
            //    This value represents how the device is attached to the system. This can be
            //    any 32 bit integer, but common values for this property are defined in
            //    <CoreAudio/AudioHardwareBase.h>
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyTransportType for the box");
            *((UInt32 *)outData) = kAudioDeviceTransportTypeVirtual;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyHasAudio:
            //    Indicates whether or not the box has audio capabilities
            FailWithAction(
                inDataSize < sizeof(UInt32),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetBoxPropertyData: not enough space for the return value of kAudioBoxPropertyHasAudio for the box");
            *((UInt32 *)outData) = 1;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyHasVideo:
            //    Indicates whether or not the box has video capabilities
            FailWithAction(
                inDataSize < sizeof(UInt32),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetBoxPropertyData: not enough space for the return value of kAudioBoxPropertyHasVideo for the box");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyHasMIDI:
            //    Indicates whether or not the box has MIDI capabilities
            FailWithAction(
                inDataSize < sizeof(UInt32),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetBoxPropertyData: not enough space for the return value of kAudioBoxPropertyHasMIDI for the box");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyIsProtected:
            //    Indicates whether or not the box has requires authentication to use
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of kAudioBoxPropertyIsProtected "
                           "for the box");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyAcquired:
            //    When set to a non-zero value, the device is acquired for use by the local machine
            FailWithAction(
                inDataSize < sizeof(UInt32),
                theAnswer = kAudioHardwareBadPropertySizeError,
                Done,
                "GetBoxPropertyData: not enough space for the return value of kAudioBoxPropertyAcquired for the box");
            {
                CAMutex::Locker locker(stateMutex);
                *((UInt32 *)outData) = gBox_Acquired ? 1 : 0;
            }

            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyAcquisitionFailed:
            //    This is used for notifications to say when an attempt to acquire a device has failed.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetBoxPropertyData: not enough space for the return value of "
                           "kAudioBoxPropertyAcquisitionFailed for the box");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioBoxPropertyDeviceList:
            //    This is used to indicate which devices came from this box
            {
                CAMutex::Locker locker(stateMutex);
                if (gBox_Acquired) {
                    FailWithAction(inDataSize < sizeof(AudioObjectID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetBoxPropertyData: not enough space for the return value of "
                                   "kAudioBoxPropertyDeviceList for the box");
                    *((AudioObjectID *)outData) = kObjectID_Device;
                    *outDataSize = sizeof(AudioObjectID);
                } else {
                    *outDataSize = 0;
                }
            }
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::SetBoxPropertyData(AudioServerPlugInDriverRef inDriver,
                                              AudioObjectID inObjectID,
                                              pid_t inClientProcessID,
                                              const AudioObjectPropertyAddress *inAddress,
                                              UInt32 inQualifierDataSize,
                                              const void *inQualifierData,
                                              UInt32 inDataSize,
                                              const void *inData,
                                              UInt32 *outNumberPropertiesChanged,
                                              AudioObjectPropertyAddress outChangedAddresses[2]) {
#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData, inDataSize, inData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetBoxPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "SetBoxPropertyData: no address");
    FailWithAction(outNumberPropertiesChanged == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetBoxPropertyData: no place to return the number of properties that changed");
    FailWithAction(outChangedAddresses == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetBoxPropertyData: no place to return the properties that changed");
    FailWithAction(inObjectID != kObjectID_Box,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetBoxPropertyData: not the box object");

    //    initialize the returned number of changed properties
    *outNumberPropertiesChanged = 0;

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetPlugInPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyName:
            //    Boxes should allow their name to be editable
            {
                FailWithAction(inDataSize != sizeof(CFStringRef),
                               theAnswer = kAudioHardwareBadPropertySizeError,
                               Done,
                               "SetBoxPropertyData: wrong size for the data for kAudioObjectPropertyName");
                CFStringRef *newValue = (CFStringRef *)inData;

                FailWithAction((newValue == NULL || *newValue == NULL || CFGetTypeID(*newValue) != CFStringGetTypeID()),
                               theAnswer = kAudioHardwareIllegalOperationError,
                               Done,
                               "SetBoxPropertyData: bad value for kAudioObjectPropertyName");

                // See the comment in the switch case for 'kAudioObjectPropertyIdentify' to get a description of the
                // crazy hackery that is going on here.
                
                if (inClientProcessID == configuratorPid) {
                    DebugMsg("ProxyAudio: setting box name from configurator process, performing configuration action "
                             "instead!");
                    CFStringSmartRef value = nullptr;
                    ConfigType action = ConfigType::none;
                    parseConfigurationString(*newValue, action, value);

                    if (action != ConfigType::none && value) {
                        setConfigurationValue(action, value);
                    }
                } else {
                    CAMutex::Locker locker(stateMutex);

                    if (boxName != NULL) {
                        CFRelease(boxName);
                    }

                    boxName = CFStringCreateCopy(NULL, *newValue);
                    *outNumberPropertiesChanged = 1;
                    outChangedAddresses[0].mSelector = kAudioObjectPropertyName;
                    outChangedAddresses[0].mScope = kAudioObjectPropertyScopeGlobal;
                    outChangedAddresses[0].mElement = kAudioObjectPropertyElementMaster;
                }

            }
            break;

        case kAudioObjectPropertyIdentify:
            FailWithAction(inDataSize != sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "SetBoxPropertyData: wrong size for the data for kAudioObjectPropertyIdentify");
            {
                SInt32 signedValue = *((SInt32 *)inData);

                // To handle reading and setting configurations for the driver, we're using a rather ridiculous hack.
                // However Apple makes it very, very difficult for a client process to configure the driver in any way.
                // From the research I did, it seems to require at minimum using a LaunchDaemon to relay messages from a
                // client process, which seemed like such a tedious and annoying thing to set up.

                // So here's the alternative method: we use a few of the properties of the box device in ways they
                // weren't intended to be used:

                // First, if a process sets the "identify" property of the box device to its pid, then that pid will
                // become the designated 'configurator' pid. The driver will then respond differently to read and write
                // actions taken by the process for certain key properties on the box device.

                // If the configurator process writes a negative value to the box device's identify property, then that
                // sets the current setting (with the absolute value of what was written to the identify property
                // corresponding to the ConfigType enum). The current setting is what will be returned if that same
                // process reads the box device's name property. In the case of integer settings (such as the output
                // device's buffer frame length) then it is converted to a string before being returned.

                // Lastly, the configurator process can write out new values to the driver's settings again using the
                // box's name property. When it sets it to a value in the form "settingName=value" then it will parse
                // that string and adjust the specified setting accordingly.

                // The identify and name properties of the box are used for this because they are a few of the only
                // settings that can be written to at all, and aren't of particular importance to the operation of the
                // driver.

                if (signedValue != 0 && signedValue != 1) {
                    if (signedValue < 0) {
                        nextConfigurationToRead = ConfigType(-signedValue);
                        DebugMsg("ProxyAudio: received signal, will return data on next call to box name: %d",
                                 nextConfigurationToRead);
                    } else {
                        configuratorPid = signedValue;
                        DebugMsg("ProxyAudio: received signal, configurator pid is: %d",
                                 configuratorPid);
                    }
                    
                }
            }

            theAnswer = noErr;
            break;

        case kAudioBoxPropertyAcquired:
            //    When the box is acquired, it means the contents, namely the device, are available to the system
            {
                FailWithAction(inDataSize != sizeof(UInt32),
                               theAnswer = kAudioHardwareBadPropertySizeError,
                               Done,
                               "SetBoxPropertyData: wrong size for the data for kAudioBoxPropertyAcquired");
                {
                    CAMutex::Locker locker(stateMutex);
                    if (gBox_Acquired != (*((UInt32 *)inData) != 0)) {
                        //    the new value is different from the old value, so save it
                        gBox_Acquired = *((UInt32 *)inData) != 0;
                        gPlugIn_Host->WriteToStorage(
                            gPlugIn_Host, CFSTR("box acquired"), gBox_Acquired ? kCFBooleanTrue : kCFBooleanFalse);

                        //    and it means that this property and the device list property have changed
                        *outNumberPropertiesChanged = 2;
                        outChangedAddresses[0].mSelector = kAudioBoxPropertyAcquired;
                        outChangedAddresses[0].mScope = kAudioObjectPropertyScopeGlobal;
                        outChangedAddresses[0].mElement = kAudioObjectPropertyElementMaster;
                        outChangedAddresses[1].mSelector = kAudioBoxPropertyDeviceList;
                        outChangedAddresses[1].mScope = kAudioObjectPropertyScopeGlobal;
                        outChangedAddresses[1].mElement = kAudioObjectPropertyElementMaster;

                        //    but it also means that the device list has changed for the plug-in too
                        ExecuteInAudioOutputThread(^() {
                            AudioObjectPropertyAddress theAddress = {kAudioPlugInPropertyDeviceList,
                                                                     kAudioObjectPropertyScopeGlobal,
                                                                     kAudioObjectPropertyElementMaster};
                            gPlugIn_Host->PropertiesChanged(gPlugIn_Host, kObjectID_PlugIn, 1, &theAddress);
                        });
                    }
                }
            }
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

#pragma mark Device Property Operations

Boolean ProxyAudioDevice::HasDeviceProperty(AudioServerPlugInDriverRef inDriver,
                                            AudioObjectID inObjectID,
                                            pid_t inClientProcessID,
                                            const AudioObjectPropertyAddress *inAddress) {
    //    This method returns whether or not the given object has the given property.

#pragma unused(inClientProcessID)

    //    declare the local variables
    Boolean theAnswer = false;

    //    check the arguments
    FailIf(inDriver != gAudioServerPlugInDriverRef, Done, "HasDeviceProperty: bad driver reference");
    FailIf(inAddress == NULL, Done, "HasDeviceProperty: no address");
    FailIf(inObjectID != kObjectID_Device, Done, "HasDeviceProperty: not the device object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetDevicePropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyName:
        case kAudioObjectPropertyManufacturer:
        case kAudioObjectPropertyOwnedObjects:
        case kAudioDevicePropertyDeviceUID:
        case kAudioDevicePropertyModelUID:
        case kAudioDevicePropertyTransportType:
        case kAudioDevicePropertyRelatedDevices:
        case kAudioDevicePropertyClockDomain:
        case kAudioDevicePropertyDeviceIsAlive:
        case kAudioDevicePropertyDeviceIsRunning:
        case kAudioObjectPropertyControlList:
        case kAudioDevicePropertyNominalSampleRate:
        case kAudioDevicePropertyAvailableNominalSampleRates:
        case kAudioDevicePropertyIsHidden:
        case kAudioDevicePropertyZeroTimeStampPeriod:
        case kAudioDevicePropertyIcon:
        case kAudioDevicePropertyStreams:
            theAnswer = true;
            break;

        case kAudioDevicePropertyDeviceCanBeDefaultDevice:
        case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
        case kAudioDevicePropertyLatency:
        case kAudioDevicePropertySafetyOffset:
        case kAudioDevicePropertyPreferredChannelsForStereo:
        case kAudioDevicePropertyPreferredChannelLayout:
            theAnswer = (inAddress->mScope == kAudioObjectPropertyScopeInput)
                        || (inAddress->mScope == kAudioObjectPropertyScopeOutput);
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::IsDevicePropertySettable(AudioServerPlugInDriverRef inDriver,
                                                    AudioObjectID inObjectID,
                                                    pid_t inClientProcessID,
                                                    const AudioObjectPropertyAddress *inAddress,
                                                    Boolean *outIsSettable) {
    //    This method returns whether or not the given property on the object can have its value
    //    changed.

#pragma unused(inClientProcessID)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsDevicePropertySettable: bad driver reference");
    FailWithAction(inAddress == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsDevicePropertySettable: no address");
    FailWithAction(outIsSettable == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsDevicePropertySettable: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsDevicePropertySettable: not the device object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetDevicePropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyName:
        case kAudioObjectPropertyManufacturer:
        case kAudioObjectPropertyOwnedObjects:
        case kAudioDevicePropertyDeviceUID:
        case kAudioDevicePropertyModelUID:
        case kAudioDevicePropertyTransportType:
        case kAudioDevicePropertyRelatedDevices:
        case kAudioDevicePropertyClockDomain:
        case kAudioDevicePropertyDeviceIsAlive:
        case kAudioDevicePropertyDeviceIsRunning:
        case kAudioDevicePropertyDeviceCanBeDefaultDevice:
        case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
        case kAudioDevicePropertyLatency:
        case kAudioDevicePropertyStreams:
        case kAudioObjectPropertyControlList:
        case kAudioDevicePropertySafetyOffset:
        case kAudioDevicePropertyAvailableNominalSampleRates:
        case kAudioDevicePropertyIsHidden:
        case kAudioDevicePropertyPreferredChannelsForStereo:
        case kAudioDevicePropertyPreferredChannelLayout:
        case kAudioDevicePropertyZeroTimeStampPeriod:
        case kAudioDevicePropertyIcon:
            *outIsSettable = false;
            break;

        case kAudioDevicePropertyNominalSampleRate:
            *outIsSettable = true;
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetDevicePropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                                     AudioObjectID inObjectID,
                                                     pid_t inClientProcessID,
                                                     const AudioObjectPropertyAddress *inAddress,
                                                     UInt32 inQualifierDataSize,
                                                     const void *inQualifierData,
                                                     UInt32 *outDataSize) {
    //    This method returns the byte size of the property's data.

#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetDevicePropertyDataSize: bad driver reference");
    FailWithAction(inAddress == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetDevicePropertyDataSize: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetDevicePropertyDataSize: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetDevicePropertyDataSize: not the device object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetDevicePropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyName:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyManufacturer:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyOwnedObjects:
            switch (inAddress->mScope) {
                case kAudioObjectPropertyScopeGlobal:
                    *outDataSize = 8 * sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyScopeInput:
                    *outDataSize = 4 * sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyScopeOutput:
                    *outDataSize = 4 * sizeof(AudioObjectID);
                    break;
            };
            break;

        case kAudioDevicePropertyDeviceUID:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioDevicePropertyModelUID:
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioDevicePropertyTransportType:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyRelatedDevices:
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioDevicePropertyClockDomain:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceIsAlive:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioDevicePropertyDeviceIsRunning:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceCanBeDefaultDevice:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyLatency:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyStreams:
            switch (inAddress->mScope) {
                case kAudioObjectPropertyScopeGlobal:
                    *outDataSize = 2 * sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyScopeInput:
                    *outDataSize = 1 * sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyScopeOutput:
                    *outDataSize = 1 * sizeof(AudioObjectID);
                    break;
            };
            break;

        case kAudioObjectPropertyControlList:
            *outDataSize = 6 * sizeof(AudioObjectID);
            break;

        case kAudioDevicePropertySafetyOffset:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyNominalSampleRate:
            *outDataSize = sizeof(Float64);
            break;

        case kAudioDevicePropertyAvailableNominalSampleRates:
            *outDataSize = (UInt32)gDevice_SampleRates.size() * sizeof(AudioValueRange);
            break;

        case kAudioDevicePropertyIsHidden:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyPreferredChannelsForStereo:
            *outDataSize = 2 * sizeof(UInt32);
            break;

        case kAudioDevicePropertyPreferredChannelLayout:
            *outDataSize = offsetof(AudioChannelLayout, mChannelDescriptions)
                           + (gDevice_ChannelsPerFrame * sizeof(AudioChannelDescription));
            break;

        case kAudioDevicePropertyZeroTimeStampPeriod:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyIcon:
            *outDataSize = sizeof(CFURLRef);
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetDevicePropertyData(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inObjectID,
                                                 pid_t inClientProcessID,
                                                 const AudioObjectPropertyAddress *inAddress,
                                                 UInt32 inQualifierDataSize,
                                                 const void *inQualifierData,
                                                 UInt32 inDataSize,
                                                 UInt32 *outDataSize,
                                                 void *outData) {
#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;
    UInt32 theNumberItemsToFetch;
    UInt32 theItemIndex;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetDevicePropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "GetDevicePropertyData: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetDevicePropertyData: no place to put the return value size");
    FailWithAction(outData == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetDevicePropertyData: no place to put the return value");
    FailWithAction(inObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetDevicePropertyData: not the device object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required.
    //
    //    Also, since most of the data that will get returned is static, there are few instances where
    //    it is necessary to lock the state mutex.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
            //    The base class for kAudioDeviceClassID is kAudioObjectClassID
            FailWithAction(inDataSize < sizeof(AudioClassID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyBaseClass for the device");
            *((AudioClassID *)outData) = kAudioObjectClassID;
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            //    The class is always kAudioDeviceClassID for devices created by drivers
            FailWithAction(inDataSize < sizeof(AudioClassID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of kAudioObjectPropertyClass "
                           "for the device");
            *((AudioClassID *)outData) = kAudioDeviceClassID;
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            //    The device's owner is the plug-in object
            FailWithAction(inDataSize < sizeof(AudioObjectID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of kAudioObjectPropertyOwner "
                           "for the device");
            *((AudioObjectID *)outData) = kObjectID_PlugIn;
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyName:
            //    This is the human readable name of the device.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyManufacturer for the device");
            *((CFStringRef *)outData) = CFStringCreateCopy(NULL, deviceName);
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyManufacturer:
            //    This is the human readable name of the maker of the plug-in.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyManufacturer for the device");
            *((CFStringRef *)outData) = CFSTR("ManufacturerName");
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioObjectPropertyOwnedObjects:
            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);

            //    The device owns its streams and controls. Note that what is returned here
            //    depends on the scope requested.
            switch (inAddress->mScope) {
                case kAudioObjectPropertyScopeGlobal:
                    //    global scope means return all objects
                    if (theNumberItemsToFetch > 4) {
                        theNumberItemsToFetch = 4;
                    }

                    //    fill out the list with as many objects as requested, which is everything
                    for (theItemIndex = 0; theItemIndex < theNumberItemsToFetch; ++theItemIndex) {
                        ((AudioObjectID *)outData)[theItemIndex] = kObjectID_Stream_Output + theItemIndex;
                    }
                    break;

                case kAudioObjectPropertyScopeInput:
                    theNumberItemsToFetch = 0;
                    break;

                case kAudioObjectPropertyScopeOutput:
                    //    output scope means just the objects on the output side
                    if (theNumberItemsToFetch > 4) {
                        theNumberItemsToFetch = 4;
                    }

                    //    fill out the list with the right objects
                    for (theItemIndex = 0; theItemIndex < theNumberItemsToFetch; ++theItemIndex) {
                        ((AudioObjectID *)outData)[theItemIndex] = kObjectID_Stream_Output + theItemIndex;
                    }
                    break;
            };

            //    report how much we wrote
            *outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
            break;

        case kAudioDevicePropertyDeviceUID:
            //    This is a CFString that is a persistent token that can identify the same
            //    audio device across boot sessions. Note that two instances of the same
            //    device must have different values for this property.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyDeviceUID for the device");
            *((CFStringRef *)outData) = CFSTR(kDevice_UID);
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioDevicePropertyModelUID:
            //    This is a CFString that is a persistent token that can identify audio
            //    devices that are the same kind of device. Note that two instances of the
            //    save device must have the same value for this property.
            FailWithAction(inDataSize < sizeof(CFStringRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyModelUID for the device");
            *((CFStringRef *)outData) = CFSTR(kDevice_ModelUID);
            *outDataSize = sizeof(CFStringRef);
            break;

        case kAudioDevicePropertyTransportType:
            //    This value represents how the device is attached to the system. This can be
            //    any 32 bit integer, but common values for this property are defined in
            //    <CoreAudio/AudioHardwareBase.h>
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyTransportType for the device");
            *((UInt32 *)outData) = kAudioDeviceTransportTypeVirtual;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyRelatedDevices:
            //    The related devices property identifys device objects that are very closely
            //    related. Generally, this is for relating devices that are packaged together
            //    in the hardware such as when the input side and the output side of a piece
            //    of hardware can be clocked separately and therefore need to be represented
            //    as separate AudioDevice objects. In such case, both devices would report
            //    that they are related to each other. Note that at minimum, a device is
            //    related to itself, so this list will always be at least one item long.

            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);

            //    we only have the one device...
            if (theNumberItemsToFetch > 1) {
                theNumberItemsToFetch = 1;
            }

            //    Write the devices' object IDs into the return value
            if (theNumberItemsToFetch > 0) {
                ((AudioObjectID *)outData)[0] = kObjectID_Device;
            }

            //    report how much we wrote
            *outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
            break;

        case kAudioDevicePropertyClockDomain:
            //    This property allows the device to declare what other devices it is
            //    synchronized with in hardware. The way it works is that if two devices have
            //    the same value for this property and the value is not zero, then the two
            //    devices are synchronized in hardware. Note that a device that either can't
            //    be synchronized with others or doesn't know should return 0 for this
            //    property.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyClockDomain for the device");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceIsAlive:
            //    This property returns whether or not the device is alive. Note that it is
            //    note uncommon for a device to be dead but still momentarily availble in the
            //    device list. In the case of this device, it will always be alive.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyDeviceIsAlive for the device");
            *((UInt32 *)outData) = 1;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceIsRunning:
            //    This property returns whether or not IO is running for the device. Note that
            //    we need to take both the state lock to check this value for thread safety.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyDeviceIsRunning for the device");
            {
                CAMutex::Locker locker(stateMutex);
                *((UInt32 *)outData) = ((gDevice_IOIsRunning > 0) > 0) ? 1 : 0;
            }
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceCanBeDefaultDevice:
            //    This property returns whether or not the device wants to be able to be the
            //    default device for content. This is the device that iTunes and QuickTime
            //    will use to play their content on and FaceTime will use as it's microhphone.
            //    Nearly all devices should allow for this.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyDeviceCanBeDefaultDevice for the device");
            *((UInt32 *)outData) = 1;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
            //    This property returns whether or not the device wants to be the system
            //    default device. This is the device that is used to play interface sounds and
            //    other incidental or UI-related sounds on. Most devices should allow this
            //    although devices with lots of latency may not want to.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyDeviceCanBeDefaultSystemDevice for the device");
            *((UInt32 *)outData) = 1;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyLatency:
            //    This property returns the presentation latency of the device. For this,
            //    device, the value is 0 due to the fact that it always vends silence.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyLatency for the device");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyStreams:
            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);

            //    Note that what is returned here depends on the scope requested.
            switch (inAddress->mScope) {
                case kAudioObjectPropertyScopeGlobal:
                    //    global scope means return all streams
                    if (theNumberItemsToFetch > 1) {
                        theNumberItemsToFetch = 1;
                    }

                    //    fill out the list with as many objects as requested
                    if (theNumberItemsToFetch > 0) {
                        ((AudioObjectID *)outData)[0] = kObjectID_Stream_Output;
                    }
                    // if (theNumberItemsToFetch > 1) {
                    //    ((AudioObjectID *)outData)[1] = kObjectID_Stream_Output;
                    //}
                    break;

                case kAudioObjectPropertyScopeInput:
                    theNumberItemsToFetch = 0;
                    break;

                case kAudioObjectPropertyScopeOutput:
                    //    output scope means just the objects on the output side
                    if (theNumberItemsToFetch > 1) {
                        theNumberItemsToFetch = 1;
                    }

                    //    fill out the list with as many objects as requested
                    if (theNumberItemsToFetch > 0) {
                        ((AudioObjectID *)outData)[0] = kObjectID_Stream_Output;
                    }
                    break;
            };

            //    report how much we wrote
            *outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyControlList:
            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch = inDataSize / sizeof(AudioObjectID);
            if (theNumberItemsToFetch > 4) {
                theNumberItemsToFetch = 4;
            }

            //    fill out the list with as many objects as requested, which is everything
            for (theItemIndex = 0; theItemIndex < theNumberItemsToFetch; ++theItemIndex) {
                ((AudioObjectID *)outData)[theItemIndex] = kObjectID_Volume_Output_L + theItemIndex;
            }

            //    report how much we wrote
            *outDataSize = theNumberItemsToFetch * sizeof(AudioObjectID);
            break;

        case kAudioDevicePropertySafetyOffset:
            //    This property returns the how close to now the HAL can read and write. For
            //    this, device, the value is 0 due to the fact that it always vends silence.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertySafetyOffset for the device");
            *((UInt32 *)outData) = gDevice_SafetyOffset;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyNominalSampleRate:
            //    This property returns the nominal sample rate of the device. Note that we
            //    only need to take the state lock to get this value.
            FailWithAction(inDataSize < sizeof(Float64),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyNominalSampleRate for the device");
            {
                CAMutex::Locker locker(stateMutex);
                *((Float64 *)outData) = gDevice_SampleRate;
            }
            *outDataSize = sizeof(Float64);
            break;

        case kAudioDevicePropertyAvailableNominalSampleRates:
            //    This returns all nominal sample rates the device supports as an array of
            //    AudioValueRangeStructs. Note that for discrete sampler rates, the range
            //    will have the minimum value equal to the maximum value.

            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch = (UInt32)std::min(inDataSize / sizeof(AudioValueRange), gDevice_SampleRates.size());

            for (unsigned int i = 0; i < theNumberItemsToFetch; ++i) {
                ((AudioValueRange *)outData)[i].mMinimum = gDevice_SampleRates[i];
                ((AudioValueRange *)outData)[i].mMaximum = gDevice_SampleRates[i];
            }

            //    report how much we wrote
            *outDataSize = theNumberItemsToFetch * sizeof(AudioValueRange);
            break;

        case kAudioDevicePropertyIsHidden:
            //    This returns whether or not the device is visible to clients.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyIsHidden for the device");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyPreferredChannelsForStereo:
            //    This property returns which two channesl to use as left/right for stereo
            //    data by default. Note that the channel numbers are 1-based.xz
            FailWithAction(inDataSize < (2 * sizeof(UInt32)),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyPreferredChannelsForStereo for the device");
            ((UInt32 *)outData)[0] = 1;
            ((UInt32 *)outData)[1] = 2;
            *outDataSize = 2 * sizeof(UInt32);
            break;

        case kAudioDevicePropertyPreferredChannelLayout:
            //    This property returns the default AudioChannelLayout to use for the device
            //    by default. For this device, we return a stereo ACL.
            {
                //    calcualte how big the
                UInt32 theACLSize =
                    offsetof(AudioChannelLayout, mChannelDescriptions) + (2 * sizeof(AudioChannelDescription));
                FailWithAction(inDataSize < theACLSize,
                               theAnswer = kAudioHardwareBadPropertySizeError,
                               Done,
                               "GetDevicePropertyData: not enough space for the return value of "
                               "kAudioDevicePropertyPreferredChannelLayout for the device");
                ((AudioChannelLayout *)outData)->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
                ((AudioChannelLayout *)outData)->mChannelBitmap = 0;
                ((AudioChannelLayout *)outData)->mNumberChannelDescriptions = gDevice_ChannelsPerFrame;
                for (theItemIndex = 0; theItemIndex < gDevice_ChannelsPerFrame; ++theItemIndex) {
                    ((AudioChannelLayout *)outData)->mChannelDescriptions[theItemIndex].mChannelLabel =
                        kAudioChannelLabel_Left + theItemIndex;
                    ((AudioChannelLayout *)outData)->mChannelDescriptions[theItemIndex].mChannelFlags = 0;
                    ((AudioChannelLayout *)outData)->mChannelDescriptions[theItemIndex].mCoordinates[0] = 0;
                    ((AudioChannelLayout *)outData)->mChannelDescriptions[theItemIndex].mCoordinates[1] = 0;
                    ((AudioChannelLayout *)outData)->mChannelDescriptions[theItemIndex].mCoordinates[2] = 0;
                }
                *outDataSize = theACLSize;
            }
            break;

        case kAudioDevicePropertyZeroTimeStampPeriod:
            //    This property returns how many frames the HAL should expect to see between
            //    successive sample times in the zero time stamps this device provides.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyZeroTimeStampPeriod for the device");
            *((UInt32 *)outData) = kDevice_RingBufferSize;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioDevicePropertyIcon: {
            //    This is a CFURL that points to the device's Icon in the plug-in's resource bundle.
            FailWithAction(inDataSize < sizeof(CFURLRef),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetDevicePropertyData: not enough space for the return value of "
                           "kAudioDevicePropertyDeviceUID for the device");
            CFBundleRef theBundle = CFBundleGetBundleWithIdentifier(CFSTR(kPlugIn_BundleID));
            FailWithAction(theBundle == NULL,
                           theAnswer = kAudioHardwareUnspecifiedError,
                           Done,
                           "GetDevicePropertyData: could not get the plug-in bundle for kAudioDevicePropertyIcon");
            CFURLRef theURL = CFBundleCopyResourceURL(theBundle, CFSTR("DeviceIcon.icns"), NULL, NULL);
            FailWithAction(theURL == NULL,
                           theAnswer = kAudioHardwareUnspecifiedError,
                           Done,
                           "GetDevicePropertyData: could not get the URL for kAudioDevicePropertyIcon");
            *((CFURLRef *)outData) = theURL;
            *outDataSize = sizeof(CFURLRef);
        } break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::SetDevicePropertyData(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inObjectID,
                                                 pid_t inClientProcessID,
                                                 const AudioObjectPropertyAddress *inAddress,
                                                 UInt32 inQualifierDataSize,
                                                 const void *inQualifierData,
                                                 UInt32 inDataSize,
                                                 const void *inData,
                                                 UInt32 *outNumberPropertiesChanged,
                                                 AudioObjectPropertyAddress outChangedAddresses[2]) {
#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;
    Float64 theOldSampleRate;
    UInt64 theNewSampleRate;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetDevicePropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "SetDevicePropertyData: no address");
    FailWithAction(outNumberPropertiesChanged == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetDevicePropertyData: no place to return the number of properties that changed");
    FailWithAction(outChangedAddresses == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetDevicePropertyData: no place to return the properties that changed");
    FailWithAction(inObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetDevicePropertyData: not the device object");

    //    initialize the returned number of changed properties
    *outNumberPropertiesChanged = 0;

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetDevicePropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioDevicePropertyNominalSampleRate:
            //    Changing the sample rate needs to be handled via the
            //    RequestConfigChange/PerformConfigChange machinery.

            //    check the arguments
            FailWithAction(inDataSize != sizeof(Float64),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "SetDevicePropertyData: wrong size for the data for kAudioDevicePropertyNominalSampleRate");
            FailWithAction((!contains(gDevice_SampleRates, *(Float64 *)inData)),
                           theAnswer = kAudioHardwareIllegalOperationError,
                           Done,
                           "SetDevicePropertyData: unsupported value for kAudioDevicePropertyNominalSampleRate");

            //    make sure that the new value is different than the old value
            {
                CAMutex::Locker locker(stateMutex);
                theOldSampleRate = gDevice_SampleRate;
            }

            if (*((const Float64 *)inData) != theOldSampleRate) {
                //    we dispatch this so that the change can happen asynchronously
                theOldSampleRate = *((const Float64 *)inData);
                theNewSampleRate = (UInt64)theOldSampleRate;
                ExecuteInAudioOutputThread(^{
                    gPlugIn_Host->RequestDeviceConfigurationChange(
                        gPlugIn_Host, kObjectID_Device, theNewSampleRate, NULL);
                });
            }
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

#pragma mark Stream Property Operations

Boolean ProxyAudioDevice::HasStreamProperty(AudioServerPlugInDriverRef inDriver,
                                            AudioObjectID inObjectID,
                                            pid_t inClientProcessID,
                                            const AudioObjectPropertyAddress *inAddress) {
    //    This method returns whether or not the given object has the given property.

#pragma unused(inClientProcessID)

    //    declare the local variables
    Boolean theAnswer = false;

    //    check the arguments
    FailIf(inDriver != gAudioServerPlugInDriverRef, Done, "HasStreamProperty: bad driver reference");
    FailIf(inAddress == NULL, Done, "HasStreamProperty: no address");
    FailIf((inObjectID != kObjectID_Stream_Output), Done, "HasStreamProperty: not a stream object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetStreamPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyOwnedObjects:
        case kAudioStreamPropertyIsActive:
        case kAudioStreamPropertyDirection:
        case kAudioStreamPropertyTerminalType:
        case kAudioStreamPropertyStartingChannel:
        case kAudioStreamPropertyLatency:
        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyPhysicalFormat:
        case kAudioStreamPropertyAvailableVirtualFormats:
        case kAudioStreamPropertyAvailablePhysicalFormats:
            theAnswer = true;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::IsStreamPropertySettable(AudioServerPlugInDriverRef inDriver,
                                                    AudioObjectID inObjectID,
                                                    pid_t inClientProcessID,
                                                    const AudioObjectPropertyAddress *inAddress,
                                                    Boolean *outIsSettable) {
    //    This method returns whether or not the given property on the object can have its value
    //    changed.

#pragma unused(inClientProcessID)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsStreamPropertySettable: bad driver reference");
    FailWithAction(inAddress == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsStreamPropertySettable: no address");
    FailWithAction(outIsSettable == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsStreamPropertySettable: no place to put the return value");
    FailWithAction((inObjectID != kObjectID_Stream_Output),
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsStreamPropertySettable: not a stream object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetStreamPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
        case kAudioObjectPropertyClass:
        case kAudioObjectPropertyOwner:
        case kAudioObjectPropertyOwnedObjects:
        case kAudioStreamPropertyDirection:
        case kAudioStreamPropertyTerminalType:
        case kAudioStreamPropertyStartingChannel:
        case kAudioStreamPropertyLatency:
        case kAudioStreamPropertyAvailableVirtualFormats:
        case kAudioStreamPropertyAvailablePhysicalFormats:
            *outIsSettable = false;
            break;

        case kAudioStreamPropertyIsActive:
        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyPhysicalFormat:
            *outIsSettable = true;
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetStreamPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                                     AudioObjectID inObjectID,
                                                     pid_t inClientProcessID,
                                                     const AudioObjectPropertyAddress *inAddress,
                                                     UInt32 inQualifierDataSize,
                                                     const void *inQualifierData,
                                                     UInt32 *outDataSize) {
    //    This method returns the byte size of the property's data.

#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetStreamPropertyDataSize: bad driver reference");
    FailWithAction(inAddress == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetStreamPropertyDataSize: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetStreamPropertyDataSize: no place to put the return value");
    FailWithAction((inObjectID != kObjectID_Stream_Output),
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetStreamPropertyDataSize: not a stream object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetStreamPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyOwnedObjects:
            *outDataSize = 0 * sizeof(AudioObjectID);
            break;

        case kAudioStreamPropertyIsActive:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyDirection:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyTerminalType:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyStartingChannel:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyLatency:
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyPhysicalFormat:
            *outDataSize = sizeof(AudioStreamBasicDescription);
            break;

        case kAudioStreamPropertyAvailableVirtualFormats:
        case kAudioStreamPropertyAvailablePhysicalFormats:
            *outDataSize = (UInt32)(gDevice_SampleRates.size() * sizeof(AudioStreamRangedDescription));
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetStreamPropertyData(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inObjectID,
                                                 pid_t inClientProcessID,
                                                 const AudioObjectPropertyAddress *inAddress,
                                                 UInt32 inQualifierDataSize,
                                                 const void *inQualifierData,
                                                 UInt32 inDataSize,
                                                 UInt32 *outDataSize,
                                                 void *outData) {
#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;
    UInt32 theNumberItemsToFetch;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetStreamPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "GetStreamPropertyData: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetStreamPropertyData: no place to put the return value size");
    FailWithAction(outData == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetStreamPropertyData: no place to put the return value");
    FailWithAction((inObjectID != kObjectID_Stream_Output),
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetStreamPropertyData: not a stream object");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required.
    //
    //    Also, since most of the data that will get returned is static, there are few instances where
    //    it is necessary to lock the state mutex.
    switch (inAddress->mSelector) {
        case kAudioObjectPropertyBaseClass:
            //    The base class for kAudioStreamClassID is kAudioObjectClassID
            FailWithAction(inDataSize < sizeof(AudioClassID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of "
                           "kAudioObjectPropertyBaseClass for the stream");
            *((AudioClassID *)outData) = kAudioObjectClassID;
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyClass:
            //    The class is always kAudioStreamClassID for streams created by drivers
            FailWithAction(inDataSize < sizeof(AudioClassID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of kAudioObjectPropertyClass "
                           "for the stream");
            *((AudioClassID *)outData) = kAudioStreamClassID;
            *outDataSize = sizeof(AudioClassID);
            break;

        case kAudioObjectPropertyOwner:
            //    The stream's owner is the device object
            FailWithAction(inDataSize < sizeof(AudioObjectID),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of kAudioObjectPropertyOwner "
                           "for the stream");
            *((AudioObjectID *)outData) = kObjectID_Device;
            *outDataSize = sizeof(AudioObjectID);
            break;

        case kAudioObjectPropertyOwnedObjects:
            //    Streams do not own any objects
            *outDataSize = 0 * sizeof(AudioObjectID);
            break;

        case kAudioStreamPropertyIsActive:
            //    This property tells the device whether or not the given stream is going to
            //    be used for IO. Note that we need to take the state lock to examine this
            //    value.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of "
                           "kAudioStreamPropertyIsActive for the stream");
            {
                CAMutex::Locker locker(stateMutex);
                *((UInt32 *)outData) = gStream_Output_IsActive;
            }
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyDirection:
            //    This returns whether the stream is an input stream or an output stream.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of "
                           "kAudioStreamPropertyDirection for the stream");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyTerminalType:
            //    This returns a value that indicates what is at the other end of the stream
            //    such as a speaker or headphones, or a microphone. Values for this property
            //    are defined in <CoreAudio/AudioHardwareBase.h>
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of "
                           "kAudioStreamPropertyTerminalType for the stream");
            *((UInt32 *)outData) = kAudioStreamTerminalTypeSpeaker;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyStartingChannel:
            //    This property returns the absolute channel number for the first channel in
            //    the stream. For exmaple, if a device has two output streams with two
            //    channels each, then the starting channel number for the first stream is 1
            //    and ths starting channel number fo the second stream is 3.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of "
                           "kAudioStreamPropertyStartingChannel for the stream");
            *((UInt32 *)outData) = 1;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyLatency:
            //    This property returns any additonal presentation latency the stream has.
            FailWithAction(inDataSize < sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of "
                           "kAudioStreamPropertyStartingChannel for the stream");
            *((UInt32 *)outData) = 0;
            *outDataSize = sizeof(UInt32);
            break;

        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyPhysicalFormat:
            //    This returns the current format of the stream in an
            //    AudioStreamBasicDescription. Note that we need to hold the state lock to get
            //    this value.
            //    Note that for devices that don't override the mix operation, the virtual
            //    format has to be the same as the physical format.
            FailWithAction(inDataSize < sizeof(AudioStreamBasicDescription),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "GetStreamPropertyData: not enough space for the return value of "
                           "kAudioStreamPropertyVirtualFormat for the stream");
            {
                CAMutex::Locker locker(stateMutex);
                ((AudioStreamBasicDescription *)outData)->mSampleRate = gDevice_SampleRate;
                ((AudioStreamBasicDescription *)outData)->mFormatID = kAudioFormatLinearPCM;
                ((AudioStreamBasicDescription *)outData)->mFormatFlags =
                    kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
                ((AudioStreamBasicDescription *)outData)->mBytesPerFrame =
                    gDevice_BytesPerFrameInChannel * gDevice_ChannelsPerFrame;
                ((AudioStreamBasicDescription *)outData)->mChannelsPerFrame = gDevice_ChannelsPerFrame;
                ((AudioStreamBasicDescription *)outData)->mBitsPerChannel = gDevice_BytesPerFrameInChannel * 8;
                ((AudioStreamBasicDescription *)outData)->mBytesPerPacket =
                    gDevice_BytesPerFrameInChannel * gDevice_ChannelsPerFrame;
                ((AudioStreamBasicDescription *)outData)->mFramesPerPacket = 1;
            }
            *outDataSize = sizeof(AudioStreamBasicDescription);
            break;

        case kAudioStreamPropertyAvailableVirtualFormats:
        case kAudioStreamPropertyAvailablePhysicalFormats:
            //    This returns an array of AudioStreamRangedDescriptions that describe what
            //    formats are supported.

            //    Calculate the number of items that have been requested. Note that this
            //    number is allowed to be smaller than the actual size of the list. In such
            //    case, only that number of items will be returned
            theNumberItemsToFetch =
                (UInt32)std::min(inDataSize / sizeof(AudioStreamRangedDescription), gDevice_SampleRates.size());

            for (unsigned int i = 0; i < theNumberItemsToFetch; ++i) {
                ((AudioStreamRangedDescription *)outData)[i].mFormat.mSampleRate = gDevice_SampleRates[i];
                ((AudioStreamRangedDescription *)outData)[i].mFormat.mFormatID = kAudioFormatLinearPCM;
                ((AudioStreamRangedDescription *)outData)[i].mFormat.mFormatFlags =
                    kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
                ((AudioStreamRangedDescription *)outData)[i].mFormat.mBytesPerPacket =
                    gDevice_BytesPerFrameInChannel * gDevice_ChannelsPerFrame;
                ((AudioStreamRangedDescription *)outData)[i].mFormat.mFramesPerPacket = 1;
                ((AudioStreamRangedDescription *)outData)[i].mFormat.mBytesPerFrame =
                    gDevice_BytesPerFrameInChannel * gDevice_ChannelsPerFrame;
                ((AudioStreamRangedDescription *)outData)[i].mFormat.mChannelsPerFrame = gDevice_ChannelsPerFrame;
                ((AudioStreamRangedDescription *)outData)[i].mFormat.mBitsPerChannel =
                    gDevice_BytesPerFrameInChannel * 8;
                ((AudioStreamRangedDescription *)outData)[i].mSampleRateRange.mMinimum = gDevice_SampleRates[i];
                ((AudioStreamRangedDescription *)outData)[i].mSampleRateRange.mMaximum = gDevice_SampleRates[i];
            }

            //    report how much we wrote
            *outDataSize = theNumberItemsToFetch * sizeof(AudioStreamRangedDescription);
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::SetStreamPropertyData(AudioServerPlugInDriverRef inDriver,
                                                 AudioObjectID inObjectID,
                                                 pid_t inClientProcessID,
                                                 const AudioObjectPropertyAddress *inAddress,
                                                 UInt32 inQualifierDataSize,
                                                 const void *inQualifierData,
                                                 UInt32 inDataSize,
                                                 const void *inData,
                                                 UInt32 *outNumberPropertiesChanged,
                                                 AudioObjectPropertyAddress outChangedAddresses[2]) {
#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;
    Float64 theOldSampleRate;
    UInt64 theNewSampleRate;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetStreamPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "SetStreamPropertyData: no address");
    FailWithAction(outNumberPropertiesChanged == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetStreamPropertyData: no place to return the number of properties that changed");
    FailWithAction(outChangedAddresses == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetStreamPropertyData: no place to return the properties that changed");
    FailWithAction((inObjectID != kObjectID_Stream_Output),
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetStreamPropertyData: not a stream object");

    //    initialize the returned number of changed properties
    *outNumberPropertiesChanged = 0;

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetStreamPropertyData() method.
    switch (inAddress->mSelector) {
        case kAudioStreamPropertyIsActive:
            //    Changing the active state of a stream doesn't affect IO or change the structure
            //    so we can just save the state and send the notification.
            FailWithAction(inDataSize != sizeof(UInt32),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "SetStreamPropertyData: wrong size for the data for kAudioDevicePropertyNominalSampleRate");
            {
                CAMutex::Locker locker(stateMutex);
                if (gStream_Output_IsActive != (*((const UInt32 *)inData) != 0)) {
                    gStream_Output_IsActive = *((const UInt32 *)inData) != 0;
                    *outNumberPropertiesChanged = 1;
                    outChangedAddresses[0].mSelector = kAudioStreamPropertyIsActive;
                    outChangedAddresses[0].mScope = kAudioObjectPropertyScopeGlobal;
                    outChangedAddresses[0].mElement = kAudioObjectPropertyElementMaster;
                }
            }
            break;

        case kAudioStreamPropertyVirtualFormat:
        case kAudioStreamPropertyPhysicalFormat:
            //    Changing the stream format needs to be handled via the
            //    RequestConfigChange/PerformConfigChange machinery. Note that because this
            //    device only supports 2 channel 32 bit float data, the only thing that can
            //    change is the sample rate.
            FailWithAction(inDataSize != sizeof(AudioStreamBasicDescription),
                           theAnswer = kAudioHardwareBadPropertySizeError,
                           Done,
                           "SetStreamPropertyData: wrong size for the data for kAudioStreamPropertyPhysicalFormat");
            FailWithAction(((const AudioStreamBasicDescription *)inData)->mFormatID != kAudioFormatLinearPCM,
                           theAnswer = kAudioDeviceUnsupportedFormatError,
                           Done,
                           "SetStreamPropertyData: unsupported format ID for kAudioStreamPropertyPhysicalFormat");
            FailWithAction(((const AudioStreamBasicDescription *)inData)->mFormatFlags
                               != (kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked),
                           theAnswer = kAudioDeviceUnsupportedFormatError,
                           Done,
                           "SetStreamPropertyData: unsupported format flags for kAudioStreamPropertyPhysicalFormat");
            FailWithAction(
                ((const AudioStreamBasicDescription *)inData)->mBytesPerPacket
                    != (gDevice_BytesPerFrameInChannel * gDevice_ChannelsPerFrame),
                theAnswer = kAudioDeviceUnsupportedFormatError,
                Done,
                "SetStreamPropertyData: unsupported bytes per packet for kAudioStreamPropertyPhysicalFormat");
            FailWithAction(
                ((const AudioStreamBasicDescription *)inData)->mFramesPerPacket != 1,
                theAnswer = kAudioDeviceUnsupportedFormatError,
                Done,
                "SetStreamPropertyData: unsupported frames per packet for kAudioStreamPropertyPhysicalFormat");
            FailWithAction(((const AudioStreamBasicDescription *)inData)->mBytesPerFrame
                               != (gDevice_BytesPerFrameInChannel * gDevice_ChannelsPerFrame),
                           theAnswer = kAudioDeviceUnsupportedFormatError,
                           Done,
                           "SetStreamPropertyData: unsupported bytes per frame for kAudioStreamPropertyPhysicalFormat");
            FailWithAction(
                ((const AudioStreamBasicDescription *)inData)->mChannelsPerFrame != gDevice_ChannelsPerFrame,
                theAnswer = kAudioDeviceUnsupportedFormatError,
                Done,
                "SetStreamPropertyData: unsupported channels per frame for kAudioStreamPropertyPhysicalFormat");
            FailWithAction(
                ((const AudioStreamBasicDescription *)inData)->mBitsPerChannel != 32,
                theAnswer = kAudioDeviceUnsupportedFormatError,
                Done,
                "SetStreamPropertyData: unsupported bits per channel for kAudioStreamPropertyPhysicalFormat");
            FailWithAction((!contains(gDevice_SampleRates, ((const AudioStreamBasicDescription *)inData)->mSampleRate)),
                           theAnswer = kAudioHardwareIllegalOperationError,
                           Done,
                           "SetStreamPropertyData: unsupported sample rate for kAudioStreamPropertyPhysicalFormat");

            //    If we made it this far, the requested format is something we support, so make sure the sample rate is
            //    actually different
            {
                CAMutex::Locker locker(stateMutex);
                theOldSampleRate = gDevice_SampleRate;
            }
            if (((const AudioStreamBasicDescription *)inData)->mSampleRate != theOldSampleRate) {
                //    we dispatch this so that the change can happen asynchronously
                theOldSampleRate = ((const AudioStreamBasicDescription *)inData)->mSampleRate;
                theNewSampleRate = (UInt64)theOldSampleRate;
                ExecuteInAudioOutputThread(^{
                    gPlugIn_Host->RequestDeviceConfigurationChange(
                        gPlugIn_Host, kObjectID_Device, theNewSampleRate, NULL);
                });
            }
            break;

        default:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;
    };

Done:
    return theAnswer;
}

#pragma mark Control Property Operations

Boolean ProxyAudioDevice::HasControlProperty(AudioServerPlugInDriverRef inDriver,
                                             AudioObjectID inObjectID,
                                             pid_t inClientProcessID,
                                             const AudioObjectPropertyAddress *inAddress) {
    //    This method returns whether or not the given object has the given property.

#pragma unused(inClientProcessID)

    //    declare the local variables
    Boolean theAnswer = false;
    return false;
    //    check the arguments
    FailIf(inDriver != gAudioServerPlugInDriverRef, Done, "HasControlProperty: bad driver reference");
    FailIf(inAddress == NULL, Done, "HasControlProperty: no address");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetControlPropertyData() method.
    switch (inObjectID) {
        case kObjectID_Volume_Output_R:
        case kObjectID_Volume_Output_L:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioObjectPropertyOwnedObjects:
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                case kAudioLevelControlPropertyScalarValue:
                case kAudioLevelControlPropertyDecibelValue:
                case kAudioLevelControlPropertyDecibelRange:
                case kAudioLevelControlPropertyConvertScalarToDecibels:
                case kAudioLevelControlPropertyConvertDecibelsToScalar:
                    theAnswer = true;
                    break;
            };
            break;

        case kObjectID_Mute_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioObjectPropertyOwnedObjects:
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                case kAudioBooleanControlPropertyValue:
                    theAnswer = true;
                    break;
            };
            break;

        case kObjectID_DataSource_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioObjectPropertyOwnedObjects:
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                    theAnswer = true;
                    break;
            };
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::IsControlPropertySettable(AudioServerPlugInDriverRef inDriver,
                                                     AudioObjectID inObjectID,
                                                     pid_t inClientProcessID,
                                                     const AudioObjectPropertyAddress *inAddress,
                                                     Boolean *outIsSettable) {
    //    This method returns whether or not the given property on the object can have its value
    //    changed.

#pragma unused(inClientProcessID)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "IsControlPropertySettable: bad driver reference");
    FailWithAction(inAddress == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsControlPropertySettable: no address");
    FailWithAction(outIsSettable == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "IsControlPropertySettable: no place to put the return value");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetControlPropertyData() method.
    switch (inObjectID) {
        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioObjectPropertyOwnedObjects:
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                case kAudioLevelControlPropertyDecibelRange:
                case kAudioLevelControlPropertyConvertScalarToDecibels:
                case kAudioLevelControlPropertyConvertDecibelsToScalar:
                    *outIsSettable = false;
                    break;

                case kAudioLevelControlPropertyScalarValue:
                case kAudioLevelControlPropertyDecibelValue:
                    *outIsSettable = true;
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        case kObjectID_Mute_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioObjectPropertyOwnedObjects:
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                    *outIsSettable = false;
                    break;

                case kAudioBooleanControlPropertyValue:
                    *outIsSettable = true;
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        case kObjectID_DataSource_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioObjectPropertyOwnedObjects:
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                    *outIsSettable = false;
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        default:
            theAnswer = kAudioHardwareBadObjectError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetControlPropertyDataSize(AudioServerPlugInDriverRef inDriver,
                                                      AudioObjectID inObjectID,
                                                      pid_t inClientProcessID,
                                                      const AudioObjectPropertyAddress *inAddress,
                                                      UInt32 inQualifierDataSize,
                                                      const void *inQualifierData,
                                                      UInt32 *outDataSize) {
    //    This method returns the byte size of the property's data.

#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetControlPropertyDataSize: bad driver reference");
    FailWithAction(inAddress == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetControlPropertyDataSize: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetControlPropertyDataSize: no place to put the return value");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetControlPropertyData() method.
    switch (inObjectID) {
        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyClass:
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyOwner:
                    *outDataSize = sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyOwnedObjects:
                    *outDataSize = 0 * sizeof(AudioObjectID);
                    break;

                case kAudioControlPropertyScope:
                    *outDataSize = sizeof(AudioObjectPropertyScope);
                    break;

                case kAudioControlPropertyElement:
                    *outDataSize = sizeof(AudioObjectPropertyElement);
                    break;

                case kAudioLevelControlPropertyScalarValue:
                    *outDataSize = sizeof(Float32);
                    break;

                case kAudioLevelControlPropertyDecibelValue:
                    *outDataSize = sizeof(Float32);
                    break;

                case kAudioLevelControlPropertyDecibelRange:
                    *outDataSize = sizeof(AudioValueRange);
                    break;

                case kAudioLevelControlPropertyConvertScalarToDecibels:
                    *outDataSize = sizeof(Float32);
                    break;

                case kAudioLevelControlPropertyConvertDecibelsToScalar:
                    *outDataSize = sizeof(Float32);
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        case kObjectID_Mute_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyClass:
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyOwner:
                    *outDataSize = sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyOwnedObjects:
                    *outDataSize = 0 * sizeof(AudioObjectID);
                    break;

                case kAudioControlPropertyScope:
                    *outDataSize = sizeof(AudioObjectPropertyScope);
                    break;

                case kAudioControlPropertyElement:
                    *outDataSize = sizeof(AudioObjectPropertyElement);
                    break;

                case kAudioBooleanControlPropertyValue:
                    *outDataSize = sizeof(UInt32);
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        case kObjectID_DataSource_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyClass:
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyOwner:
                    *outDataSize = sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyOwnedObjects:
                    *outDataSize = 0 * sizeof(AudioObjectID);
                    break;

                case kAudioControlPropertyScope:
                    *outDataSize = sizeof(AudioObjectPropertyScope);
                    break;

                case kAudioControlPropertyElement:
                    *outDataSize = sizeof(AudioObjectPropertyElement);
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        default:
            theAnswer = kAudioHardwareBadObjectError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetControlPropertyData(AudioServerPlugInDriverRef inDriver,
                                                  AudioObjectID inObjectID,
                                                  pid_t inClientProcessID,
                                                  const AudioObjectPropertyAddress *inAddress,
                                                  UInt32 inQualifierDataSize,
                                                  const void *inQualifierData,
                                                  UInt32 inDataSize,
                                                  UInt32 *outDataSize,
                                                  void *outData) {
#pragma unused(inClientProcessID)
#pragma unused(inQualifierDataSize)
#pragma unused(inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetControlPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "GetControlPropertyData: no address");
    FailWithAction(outDataSize == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetControlPropertyData: no place to put the return value size");
    FailWithAction(outData == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "GetControlPropertyData: no place to put the return value");

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required.
    //
    //    Also, since most of the data that will get returned is static, there are few instances where
    //    it is necessary to lock the state mutex.
    switch (inObjectID) {
        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    //    The base class for kAudioVolumeControlClassID is kAudioLevelControlClassID
                    FailWithAction(inDataSize < sizeof(AudioClassID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyBaseClass for the volume control");
                    *((AudioClassID *)outData) = kAudioLevelControlClassID;
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyClass:
                    //    Volume controls are of the class, kAudioVolumeControlClassID
                    FailWithAction(inDataSize < sizeof(AudioClassID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyClass for the volume control");
                    *((AudioClassID *)outData) = kAudioVolumeControlClassID;
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyOwner:
                    //    The control's owner is the device object
                    FailWithAction(inDataSize < sizeof(AudioObjectID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyOwner for the volume control");
                    *((AudioObjectID *)outData) = kObjectID_Device;
                    *outDataSize = sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyOwnedObjects:
                    //    Controls do not own any objects
                    *outDataSize = 0 * sizeof(AudioObjectID);
                    break;

                case kAudioControlPropertyScope:
                    //    This property returns the scope that the control is attached to.
                    FailWithAction(inDataSize < sizeof(AudioObjectPropertyScope),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioControlPropertyScope for the volume control");
                    *((AudioObjectPropertyScope *)outData) = kAudioObjectPropertyScopeOutput;
                    *outDataSize = sizeof(AudioObjectPropertyScope);
                    break;

                case kAudioControlPropertyElement:
                    //    This property returns the element that the control is attached to.
                    FailWithAction(inDataSize < sizeof(AudioObjectPropertyElement),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioControlPropertyElement for the volume control");
                    *((AudioObjectPropertyElement *)outData) = (inObjectID == kObjectID_Volume_Output_L) ? 1 : 2;
                    *outDataSize = sizeof(AudioObjectPropertyElement);
                    break;

                case kAudioLevelControlPropertyScalarValue:
                    //    This returns the value of the control in the normalized range of 0 to 1.
                    //    Note that we need to take the state lock to examine the value.
                    FailWithAction(inDataSize < sizeof(Float32),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioLevelControlPropertyScalarValue for the volume control");
                    {
                        CAMutex::Locker locker(stateMutex);
                        if (inObjectID == kObjectID_Volume_Output_L) {
                            *((Float32 *)outData) = gVolume_Output_L_Value;
                        } else {
                            *((Float32 *)outData) = gVolume_Output_R_Value;
                        }
                    }
                    *outDataSize = sizeof(Float32);
                    break;

                case kAudioLevelControlPropertyDecibelValue:
                    //    This returns the dB value of the control.
                    //    Note that we need to take the state lock to examine the value.
                    FailWithAction(inDataSize < sizeof(Float32),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioLevelControlPropertyDecibelValue for the volume control");
                    {
                        CAMutex::Locker locker(stateMutex);
                        if (inObjectID == kObjectID_Volume_Output_L) {
                            *((Float32 *)outData) = gVolume_Output_L_Value;
                        } else {
                            *((Float32 *)outData) = gVolume_Output_R_Value;
                        }
                    }

                    //    Note that we square the scalar value before converting to dB so as to
                    //    provide a better curve for the slider
                    *((Float32 *)outData) *= *((Float32 *)outData);
                    *((Float32 *)outData) = kVolume_MinDB + (*((Float32 *)outData) * (kVolume_MaxDB - kVolume_MinDB));

                    //    report how much we wrote
                    *outDataSize = sizeof(Float32);
                    break;

                case kAudioLevelControlPropertyDecibelRange:
                    //    This returns the dB range of the control.
                    FailWithAction(inDataSize < sizeof(AudioValueRange),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioLevelControlPropertyDecibelRange for the volume control");
                    ((AudioValueRange *)outData)->mMinimum = kVolume_MinDB;
                    ((AudioValueRange *)outData)->mMaximum = kVolume_MaxDB;
                    *outDataSize = sizeof(AudioValueRange);
                    break;

                case kAudioLevelControlPropertyConvertScalarToDecibels:
                    //    This takes the scalar value in outData and converts it to dB.
                    FailWithAction(inDataSize < sizeof(Float32),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioLevelControlPropertyDecibelValue for the volume control");

                    //    clamp the value to be between 0 and 1
                    if (*((Float32 *)outData) < 0.0) {
                        *((Float32 *)outData) = 0;
                    }
                    if (*((Float32 *)outData) > 1.0) {
                        *((Float32 *)outData) = 1.0;
                    }

                    //    Note that we square the scalar value before converting to dB so as to
                    //    provide a better curve for the slider
                    *((Float32 *)outData) *= *((Float32 *)outData);
                    *((Float32 *)outData) = kVolume_MinDB + (*((Float32 *)outData) * (kVolume_MaxDB - kVolume_MinDB));

                    //    report how much we wrote
                    *outDataSize = sizeof(Float32);
                    break;

                case kAudioLevelControlPropertyConvertDecibelsToScalar:
                    //    This takes the dB value in outData and converts it to scalar.
                    FailWithAction(inDataSize < sizeof(Float32),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioLevelControlPropertyDecibelValue for the volume control");

                    //    clamp the value to be between kVolume_MinDB and kVolume_MaxDB
                    if (*((Float32 *)outData) < kVolume_MinDB) {
                        *((Float32 *)outData) = kVolume_MinDB;
                    }
                    if (*((Float32 *)outData) > kVolume_MaxDB) {
                        *((Float32 *)outData) = kVolume_MaxDB;
                    }

                    //    Note that we square the scalar value before converting to dB so as to
                    //    provide a better curve for the slider. We undo that here.
                    *((Float32 *)outData) = *((Float32 *)outData) - kVolume_MinDB;
                    *((Float32 *)outData) /= kVolume_MaxDB - kVolume_MinDB;
                    *((Float32 *)outData) = sqrtf(*((Float32 *)outData));

                    //    report how much we wrote
                    *outDataSize = sizeof(Float32);
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        case kObjectID_Mute_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    //    The base class for kAudioMuteControlClassID is kAudioBooleanControlClassID
                    FailWithAction(inDataSize < sizeof(AudioClassID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyBaseClass for the mute control");
                    *((AudioClassID *)outData) = kAudioBooleanControlClassID;
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyClass:
                    //    Mute controls are of the class, kAudioMuteControlClassID
                    FailWithAction(inDataSize < sizeof(AudioClassID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyClass for the mute control");
                    *((AudioClassID *)outData) = kAudioMuteControlClassID;
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyOwner:
                    //    The control's owner is the device object
                    FailWithAction(inDataSize < sizeof(AudioObjectID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyOwner for the mute control");
                    *((AudioObjectID *)outData) = kObjectID_Device;
                    *outDataSize = sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyOwnedObjects:
                    //    Controls do not own any objects
                    *outDataSize = 0 * sizeof(AudioObjectID);
                    break;

                case kAudioControlPropertyScope:
                    //    This property returns the scope that the control is attached to.
                    FailWithAction(inDataSize < sizeof(AudioObjectPropertyScope),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioControlPropertyScope for the mute control");
                    *((AudioObjectPropertyScope *)outData) = kAudioObjectPropertyScopeOutput;
                    *outDataSize = sizeof(AudioObjectPropertyScope);
                    break;

                case kAudioControlPropertyElement:
                    //    This property returns the element that the control is attached to.
                    FailWithAction(inDataSize < sizeof(AudioObjectPropertyElement),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioControlPropertyElement for the mute control");
                    *((AudioObjectPropertyElement *)outData) = kAudioObjectPropertyElementMaster;
                    *outDataSize = sizeof(AudioObjectPropertyElement);
                    break;

                case kAudioBooleanControlPropertyValue:
                    //    This returns the value of the mute control where 0 means that mute is off
                    //    and audio can be heard and 1 means that mute is on and audio cannot be heard.
                    //    Note that we need to take the state lock to examine this value.
                    FailWithAction(inDataSize < sizeof(UInt32),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioBooleanControlPropertyValue for the mute control");
                    {
                        CAMutex::Locker locker(stateMutex);
                        *((UInt32 *)outData) = gMute_Output_Mute ? 1 : 0;
                    }
                    *outDataSize = sizeof(UInt32);
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        case kObjectID_DataSource_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    //    The base class for kAudioDataSourceControlClassID is kAudioSelectorControlClassID
                    FailWithAction(inDataSize < sizeof(AudioClassID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyBaseClass for the data source control");
                    *((AudioClassID *)outData) = kAudioSelectorControlClassID;
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyClass:
                    //    Data Source controls are of the class, kAudioDataSourceControlClassID
                    FailWithAction(inDataSize < sizeof(AudioClassID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyClass for the data source control");
                    *((AudioClassID *)outData) = kAudioDataSourceControlClassID;
                    *outDataSize = sizeof(AudioClassID);
                    break;

                case kAudioObjectPropertyOwner:
                    //    The control's owner is the device object
                    FailWithAction(inDataSize < sizeof(AudioObjectID),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioObjectPropertyOwner for the data source control");
                    *((AudioObjectID *)outData) = kObjectID_Device;
                    *outDataSize = sizeof(AudioObjectID);
                    break;

                case kAudioObjectPropertyOwnedObjects:
                    //    Controls do not own any objects
                    *outDataSize = 0 * sizeof(AudioObjectID);
                    break;

                case kAudioControlPropertyScope:
                    //    This property returns the scope that the control is attached to.
                    FailWithAction(inDataSize < sizeof(AudioObjectPropertyScope),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioControlPropertyScope for the data source control");
                    *((AudioObjectPropertyScope *)outData) = kAudioObjectPropertyScopeOutput;
                    *outDataSize = sizeof(AudioObjectPropertyScope);
                    break;

                case kAudioControlPropertyElement:
                    //    This property returns the element that the control is attached to.
                    FailWithAction(inDataSize < sizeof(AudioObjectPropertyElement),
                                   theAnswer = kAudioHardwareBadPropertySizeError,
                                   Done,
                                   "GetControlPropertyData: not enough space for the return value of "
                                   "kAudioControlPropertyElement for the data source control");
                    *((AudioObjectPropertyElement *)outData) = kAudioObjectPropertyElementMaster;
                    *outDataSize = sizeof(AudioObjectPropertyElement);
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        default:
            theAnswer = kAudioHardwareBadObjectError;
            break;
    };

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::SetControlPropertyData(AudioServerPlugInDriverRef inDriver,
                                                  AudioObjectID inObjectID,
                                                  pid_t inClientProcessID,
                                                  const AudioObjectPropertyAddress *inAddress,
                                                  UInt32 inQualifierDataSize,
                                                  const void *inQualifierData,
                                                  UInt32 inDataSize,
                                                  const void *inData,
                                                  UInt32 *outNumberPropertiesChanged,
                                                  AudioObjectPropertyAddress outChangedAddresses[2]) {
#pragma unused(inClientProcessID, inQualifierDataSize, inQualifierData)

    //    declare the local variables
    OSStatus theAnswer = 0;
    Float32 theNewVolume;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "SetControlPropertyData: bad driver reference");
    FailWithAction(
        inAddress == NULL, theAnswer = kAudioHardwareIllegalOperationError, Done, "SetControlPropertyData: no address");
    FailWithAction(outNumberPropertiesChanged == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetControlPropertyData: no place to return the number of properties that changed");
    FailWithAction(outChangedAddresses == NULL,
                   theAnswer = kAudioHardwareIllegalOperationError,
                   Done,
                   "SetControlPropertyData: no place to return the properties that changed");

    //    initialize the returned number of changed properties
    *outNumberPropertiesChanged = 0;

    //    Note that for each object, this driver implements all the required properties plus a few
    //    extras that are useful but not required. There is more detailed commentary about each
    //    property in the GetControlPropertyData() method.
    switch (inObjectID) {
        case kObjectID_Volume_Output_L:
        case kObjectID_Volume_Output_R:
            switch (inAddress->mSelector) {
                case kAudioLevelControlPropertyScalarValue:
                    //    For the scalar volume, we clamp the new value to [0, 1]. Note that if this
                    //    value changes, it implies that the dB value changed too.
                    FailWithAction(
                        inDataSize != sizeof(Float32),
                        theAnswer = kAudioHardwareBadPropertySizeError,
                        Done,
                        "SetControlPropertyData: wrong size for the data for kAudioLevelControlPropertyScalarValue");
                    theNewVolume = *((const Float32 *)inData);
                    if (theNewVolume < 0.0) {
                        theNewVolume = 0.0;
                    } else if (theNewVolume > 1.0) {
                        theNewVolume = 1.0;
                    }
                    {
                        CAMutex::Locker locker(stateMutex);
                        if (inObjectID == kObjectID_Volume_Output_L) {
                            if (gVolume_Output_L_Value != theNewVolume) {
                                gVolume_Output_L_Value = theNewVolume;
                                *outNumberPropertiesChanged = 2;
                                outChangedAddresses[0].mSelector = kAudioLevelControlPropertyScalarValue;
                                outChangedAddresses[0].mScope = kAudioObjectPropertyScopeGlobal;
                                outChangedAddresses[0].mElement = 1;
                                outChangedAddresses[1].mSelector = kAudioLevelControlPropertyDecibelValue;
                                outChangedAddresses[1].mScope = kAudioObjectPropertyScopeGlobal;
                                outChangedAddresses[1].mElement = 1;
                            }
                        } else {
                            if (gVolume_Output_R_Value != theNewVolume) {
                                gVolume_Output_R_Value = theNewVolume;
                                *outNumberPropertiesChanged = 2;
                                outChangedAddresses[0].mSelector = kAudioLevelControlPropertyScalarValue;
                                outChangedAddresses[0].mScope = kAudioObjectPropertyScopeGlobal;
                                outChangedAddresses[0].mElement = 2;
                                outChangedAddresses[1].mSelector = kAudioLevelControlPropertyDecibelValue;
                                outChangedAddresses[1].mScope = kAudioObjectPropertyScopeGlobal;
                                outChangedAddresses[1].mElement = 2;
                            }
                        }
                    }
                    break;

                case kAudioLevelControlPropertyDecibelValue:
                    //    For the dB value, we first convert it to a scalar value since that is how
                    //    the value is tracked. Note that if this value changes, it implies that the
                    //    scalar value changes as well.
                    FailWithAction(
                        inDataSize != sizeof(Float32),
                        theAnswer = kAudioHardwareBadPropertySizeError,
                        Done,
                        "SetControlPropertyData: wrong size for the data for kAudioLevelControlPropertyScalarValue");
                    theNewVolume = *((const Float32 *)inData);
                    if (theNewVolume < kVolume_MinDB) {
                        theNewVolume = kVolume_MinDB;
                    } else if (theNewVolume > kVolume_MaxDB) {
                        theNewVolume = kVolume_MaxDB;
                    }
                    //    Note that we square the scalar value before converting to dB so as to
                    //    provide a better curve for the slider. We undo that here.
                    theNewVolume = theNewVolume - kVolume_MinDB;
                    theNewVolume /= kVolume_MaxDB - kVolume_MinDB;
                    theNewVolume = sqrtf(theNewVolume);
                    {
                        CAMutex::Locker locker(stateMutex);
                        if (inObjectID == kObjectID_Volume_Output_L) {
                            if (gVolume_Output_L_Value != theNewVolume) {
                                gVolume_Output_L_Value = theNewVolume;
                                *outNumberPropertiesChanged = 2;
                                outChangedAddresses[0].mSelector = kAudioLevelControlPropertyScalarValue;
                                outChangedAddresses[0].mScope = kAudioObjectPropertyScopeGlobal;
                                outChangedAddresses[0].mElement = 1;
                                outChangedAddresses[1].mSelector = kAudioLevelControlPropertyDecibelValue;
                                outChangedAddresses[1].mScope = kAudioObjectPropertyScopeGlobal;
                                outChangedAddresses[1].mElement = 1;
                            }
                        } else {
                            if (gVolume_Output_R_Value != theNewVolume) {
                                gVolume_Output_R_Value = theNewVolume;
                                *outNumberPropertiesChanged = 2;
                                outChangedAddresses[0].mSelector = kAudioLevelControlPropertyScalarValue;
                                outChangedAddresses[0].mScope = kAudioObjectPropertyScopeGlobal;
                                outChangedAddresses[0].mElement = 2;
                                outChangedAddresses[1].mSelector = kAudioLevelControlPropertyDecibelValue;
                                outChangedAddresses[1].mScope = kAudioObjectPropertyScopeGlobal;
                                outChangedAddresses[1].mElement = 2;
                            }
                        }
                    }
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        case kObjectID_Mute_Output_Master:
            switch (inAddress->mSelector) {
                case kAudioBooleanControlPropertyValue:
                    FailWithAction(
                        inDataSize != sizeof(UInt32),
                        theAnswer = kAudioHardwareBadPropertySizeError,
                        Done,
                        "SetControlPropertyData: wrong size for the data for kAudioBooleanControlPropertyValue");
                    {
                        CAMutex::Locker locker(stateMutex);
                        if (gMute_Output_Mute != (*((const UInt32 *)inData) != 0)) {
                            gMute_Output_Mute = *((const UInt32 *)inData) != 0;
                            *outNumberPropertiesChanged = 1;
                            outChangedAddresses[0].mSelector = kAudioBooleanControlPropertyValue;
                            outChangedAddresses[0].mScope = kAudioObjectPropertyScopeGlobal;
                            outChangedAddresses[0].mElement = kAudioObjectPropertyElementMaster;
                        }
                    }
                    break;

                default:
                    theAnswer = kAudioHardwareUnknownPropertyError;
                    break;
            };
            break;

        case kObjectID_DataSource_Output_Master:
            theAnswer = kAudioHardwareUnknownPropertyError;
            break;

        default:
            theAnswer = kAudioHardwareBadObjectError;
            break;
    };

Done:
    return theAnswer;
}

#pragma mark Output Device Operations

AudioDevice ProxyAudioDevice::findTargetOutputAudioDevice() {
    DebugMsg("ProxyAudio: findTargetOutputAudioDevice");
    std::vector<AudioObjectID> devices = AudioDevice::allAudioDevices();
    CFStringSmartRef currentOutputDeviceUID;
    
    {
        CAMutex::Locker locker(&stateMutex);
        
        if (!outputDeviceUID) {
            DebugMsg("ProxyAudio: findTargetOutputAudioDevice finished, output device UID is null");
            return AudioDevice();
        }
        
        currentOutputDeviceUID = CFStringCreateCopy(NULL, outputDeviceUID);
    }
    
    DebugMsg("ProxyAudio: findTargetOutputAudioDevice target UID: %s", CFStringToStdString(currentOutputDeviceUID).c_str());
    
    for (AudioObjectID device : devices) {
        AudioObjectPropertyAddress propertyAddress = {
            kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMaster};

        CFStringSmartRef uid;
        UInt32 size = sizeof(CFStringRef);
        OSStatus err = AudioObjectGetPropertyData(device, &propertyAddress, 0, NULL, &size, &uid);

        if (err == noErr && uid) {
            if (CFStringCompare(uid, currentOutputDeviceUID, 0) == kCFCompareEqualTo) {
                DebugMsg("ProxyAudio: findTargetOutputAudioDevice finished, found output device");
                return AudioDevice(device);
            }
        }
    }

    DebugMsg("ProxyAudio: findTargetOutputAudioDevice finished, not not find output device");
    
    return AudioDevice();
}

int ProxyAudioDevice::outputDeviceAliveListenerStatic(AudioObjectID inObjectID,
                                                      UInt32 inNumberAddresses,
                                                      const AudioObjectPropertyAddress *inAddresses,
                                                      void *inClientData) {
    if (!inClientData) {
        return noErr;
    }

    return ((ProxyAudioDevice *)inClientData)->outputDeviceAliveListener(inObjectID, inNumberAddresses, inAddresses);
}

int ProxyAudioDevice::outputDeviceAliveListener(AudioObjectID inObjectID,
                                                UInt32 inNumberAddresses,
                                                const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inObjectID)
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)

    DebugMsg("ProxyAudio: outputDeviceAliveListener");
    {
        CAMutex::Locker locker(outputDeviceMutex);
        UInt32 alive = 0;
        OSStatus err = outputDevice.getIntegerPropertyData(
            alive, kAudioDevicePropertyDeviceIsAlive, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster);

        if (err == noErr && alive == 1) {
            return noErr;
        }
    }

    DebugMsg("ProxyAudio: outputDeviceAliveListener output device no longer alive");
    deinitializeOutputDevice();

    return noErr;
}

int ProxyAudioDevice::outputDeviceSampleRateListenerStatic(AudioObjectID inObjectID,
                                                           UInt32 inNumberAddresses,
                                                           const AudioObjectPropertyAddress *inAddresses,
                                                           void *inClientData) {
    if (!inClientData) {
        return noErr;
    }

    return ((ProxyAudioDevice *)inClientData)
        ->outputDeviceSampleRateListener(inObjectID, inNumberAddresses, inAddresses);
}

int ProxyAudioDevice::outputDeviceSampleRateListener(AudioObjectID inObjectID,
                                                     UInt32 inNumberAddresses,
                                                     const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inObjectID)
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)
    DebugMsg("ProxyAudio: outputDeviceSampleRateListener, will match sample rate");
    matchOutputDeviceSampleRate();

    return noErr;
}

int ProxyAudioDevice::devicesListenerProcStatic(AudioObjectID inObjectID,
                                                UInt32 inNumberAddresses,
                                                const AudioObjectPropertyAddress *inAddresses,
                                                void *inClientData) {
    if (!inClientData) {
        return noErr;
    }

    return ((ProxyAudioDevice *)inClientData)->devicesListenerProc(inObjectID, inNumberAddresses, inAddresses);
}

int ProxyAudioDevice::devicesListenerProc(AudioObjectID inObjectID,
                                          UInt32 inNumberAddresses,
                                          const AudioObjectPropertyAddress *inAddresses) {
#pragma unused(inObjectID)
#pragma unused(inNumberAddresses)
#pragma unused(inAddresses)
    DebugMsg("ProxyAudio: devicesListenerProc current devices changed");
    setupTargetOutputDevice();
    return noErr;
}

void ProxyAudioDevice::matchOutputDeviceSampleRateNoLock() {
    DebugMsg("ProxyAudio: matchOutputDeviceSampleRateNoLock");
    
    if (!outputDevice.isValid()) {
        DebugMsg("ProxyAudio: matchOutputDeviceSampleRateNoLock ... no valid output device");
        return;
    }

    Float64 currentInputSampleRate;
    OSStatus err = outputDevice.getDoublePropertyData(outputDevice.sampleRate,
                                                      kAudioDevicePropertyNominalSampleRate,
                                                      kAudioObjectPropertyScopeGlobal,
                                                      kAudioObjectPropertyElementMaster);

    if (err != noErr) {
        syslog(LOG_WARNING, "ProxyAudio error: couldn't get new sample rate of output device");
        return;
    }

    {
        CAMutex::Locker stateMutexLocker(stateMutex);
        currentInputSampleRate = gDevice_SampleRate;
    }
    
    if (currentInputSampleRate == outputDevice.sampleRate) {
        outputDevice.start();
        return;
    }

    DebugMsg("ProxyAudio: matchOutputDeviceSampleRateNoLock changing sample rate to match: %lf", outputDevice.sampleRate);
    // NB: it's important that we not modify OutputDevice until it is no longer playing since
    // we're not using a locking mechanism on its attributes between this function and its IO
    // function.
    
    outputDevice.stop();
    resetInputData();
    outputDevice.updateStreamInfo();

    if (!contains(gDevice_SampleRates, outputDevice.sampleRate)) {
        syslog(LOG_WARNING, "ProxyAudio: output device using unavailable sample rate, cannot play!");
        return;
    }

    DebugMsg("ProxyAudio: matchOutputDeviceSampleRateNoLock about to request device configuration change");
    
    ExecuteInAudioOutputThread(^{
        gPlugIn_Host->RequestDeviceConfigurationChange(gPlugIn_Host, kObjectID_Device, outputDevice.sampleRate, NULL);
    });
}

void ProxyAudioDevice::matchOutputDeviceSampleRate()
{
    DebugMsg("ProxyAudio: matchOutputDeviceSampleRate");
    CAMutex::Locker outputMutexLocker(outputDeviceMutex);
    matchOutputDeviceSampleRateNoLock();
}

void ProxyAudioDevice::setupTargetOutputDevice() {
    DebugMsg("ProxyAudio: setupTargetOutputDevice");
    AudioDevice newOutputDevice = findTargetOutputAudioDevice();

    DebugMsg("ProxyAudio: setupTargetOutputDevice newOutputDevice: %d", newOutputDevice.id);
    CAMutex::Locker locker(outputDeviceMutex);
    
    if (outputDevice.isValid() && outputDevice.id == newOutputDevice.id
        && outputDevice.bufferFrameSize == outputDeviceBufferFrameSize) {
        DebugMsg("ProxyAudio: setupTargetOutputDevice no change in device");
        return;
    }

    DebugMsg("ProxyAudio: setupTargetOutputDevice deinitializing old device");
    // NB: it's important that we not modify OutputDevice until it is no longer playing since
    // we're not using a locking mechanism on its attributes between this function and its IO
    // function.
    deinitializeOutputDeviceNoLock();

    if (newOutputDevice.isValid()) {
        DebugMsg("ProxyAudio: setupTargetOutputDevice setting up new device");
        resetInputData();
        outputDevice = newOutputDevice;
        outputDevice.setBufferFrameSize(outputDeviceBufferFrameSize);
        outputDevice.setupIOProc(outputDeviceIOProcStatic, this);
        outputDevice.addPropertyListener(kAudioDevicePropertyDeviceIsAlive,
                                         kAudioObjectPropertyScopeGlobal,
                                         kAudioObjectPropertyElementMaster,
                                         outputDeviceAliveListenerStatic,
                                         this);
        outputDevice.addPropertyListener(kAudioDevicePropertyNominalSampleRate,
                                         kAudioObjectPropertyScopeGlobal,
                                         kAudioObjectPropertyElementMaster,
                                         outputDeviceSampleRateListenerStatic,
                                         this);
        DebugMsg("ProxyAudio: setupTargetOutputDevice will match sample rate");
        matchOutputDeviceSampleRateNoLock();
    } else {
        syslog(LOG_WARNING, "ProxyAudio: setupTargetOutputDevice could not find output device");
    }
}

void ProxyAudioDevice::initializeOutputDevice() {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1000 * NSEC_PER_MSEC),
                   AudioOutputDispatchQueue(),
                   ^() {
                       // Any initialization that involves calling CoreAudio APIs must be done here
                       // in a separate thread from the rest of the driver. Otherwise we'll get
                       // deadlocks!
                       DebugMsg("ProxyAudio: initializeOutputDevice running in separate thread");
                       if (!outputDeviceUID) {
                           outputDeviceUID = copyDefaultProxyOutputDeviceUID();
                       }
                       
                       setupTargetOutputDevice();
                       setupAudioDevicesListener();
                   });
}

void ProxyAudioDevice::deinitializeOutputDeviceNoLock() {
    DebugMsg("ProxyAudio: deinitializeOutputDeviceNoLock");
    if (outputDevice.isValid()) {
        DebugMsg("ProxyAudio: deinitializeOutputDeviceNoLock stopping device");
        outputDevice.stop();
        DebugMsg("ProxyAudio: deinitializeOutputDeviceNoLock removing IO proc");
        outputDevice.destroyIOProc();
        DebugMsg("ProxyAudio: deinitializeOutputDeviceNoLock invalidating");
        outputDevice.invalidate();
    } else {
        DebugMsg("ProxyAudio: deinitializeOutputDeviceNoLock output device is already invalidated");
    }
}

void ProxyAudioDevice::deinitializeOutputDevice()
{
    DebugMsg("ProxyAudio: deinitializeOutputDevice");
    CAMutex::Locker locker(outputDeviceMutex);
    deinitializeOutputDeviceNoLock();
}

void ProxyAudioDevice::setupAudioDevicesListener() {
    DebugMsg("ProxyAudio: setupAudioDevicesListener");
    AudioObjectPropertyAddress listenerPropertyAddress = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
    
    OSStatus err = AudioObjectAddPropertyListener(
                                                  kAudioObjectSystemObject, &listenerPropertyAddress, &devicesListenerProcStatic, this);
    
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
    
    DebugMsg("ProxyAudio: setupAudioDevicesListener finished");
}

#pragma mark IO Operations

void ProxyAudioDevice::resetInputData() {
    DebugMsg("ProxyAudio: resetInputData");
    CAMutex::Locker locker(&IOMutex);
    
    if (inputBuffer) {
        inputBuffer->Clear();
    }

    lastInputFrameTime = -1;
    lastInputBufferFrameSize = -1;
    inputOutputSampleDelta = -1;
    inputFinalFrameTime = -1;
    DebugMsg("ProxyAudio: resetInputData finished");
}

OSStatus ProxyAudioDevice::StartIO(AudioServerPlugInDriverRef inDriver,
                                   AudioObjectID inDeviceObjectID,
                                   UInt32 inClientID) {
#pragma unused(inDriver)
#pragma unused(inDeviceObjectID)
    //    This call tells the device that IO is starting for the given client. When this routine
    //    returns, the device's clock is running and it is ready to have data read/written. It is
    //    important to note that multiple clients can have IO running on the device at the same time.
    //    So, work only needs to be done when the first client starts. All subsequent starts simply
    //    increment the counter.
    OSStatus theAnswer = 0;

#pragma unused(inClientID)
    
    DebugMsg("ProxyAudio: StartIO");
    resetInputData();

    CAMutex::Locker locker(stateMutex);

    //    figure out what we need to do
    if (gDevice_IOIsRunning == UINT64_MAX) {
        //    overflowing is an error
        theAnswer = kAudioHardwareIllegalOperationError;
    } else if (gDevice_IOIsRunning == 0) {
        //    We need to start the hardware, which in this case is just anchoring the time line.
        gDevice_IOIsRunning = 1;
        gDevice_NumberTimeStamps = 0;
        gDevice_AnchorSampleTime = 0;
        gDevice_AnchorHostTime = mach_absolute_time();
        gDevice_ElapsedTicks = 0;
        outputAccumulatedRateRatio = 0.0;
        outputAccumulatedRateRatioSamples = 0;
    } else {
        //    IO is already running, so just bump the counter
        ++gDevice_IOIsRunning;
    }
    
    DebugMsg("ProxyAudio: StartIO finished");
    
    return theAnswer;
}

OSStatus ProxyAudioDevice::StopIO(AudioServerPlugInDriverRef inDriver,
                                  AudioObjectID inDeviceObjectID,
                                  UInt32 inClientID) {
    //    This call tells the device that the client has stopped IO. The driver can stop the hardware
    //    once all clients have stopped.

#pragma unused(inClientID)
    DebugMsg("ProxyAudio: StopIO");
    inputFinalFrameTime = lastInputFrameTime + lastInputBufferFrameSize;

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "StopIO: bad driver reference");
    FailWithAction(
        inDeviceObjectID != kObjectID_Device, theAnswer = kAudioHardwareBadObjectError, Done, "StopIO: bad device ID");

    //    we need to hold the state lock
    {
        CAMutex::Locker locker(stateMutex);

        //    figure out what we need to do
        if (gDevice_IOIsRunning == 0) {
            //    underflowing is an error
            theAnswer = kAudioHardwareIllegalOperationError;
        } else if (gDevice_IOIsRunning == 1) {
            //    We need to stop the hardware, which in this case means that there's nothing to do.
            gDevice_IOIsRunning = 0;
        } else {
            //    IO is still running, so just bump the counter
            --gDevice_IOIsRunning;
        }
    }
    DebugMsg("ProxyAudio: StopIO finished");

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::GetZeroTimeStamp(AudioServerPlugInDriverRef inDriver,
                                            AudioObjectID inDeviceObjectID,
                                            UInt32 inClientID,
                                            Float64 *outSampleTime,
                                            UInt64 *outHostTime,
                                            UInt64 *outSeed) {
    //    This method returns the current zero time stamp for the device. The HAL models the timing of
    //    a device as a series of time stamps that relate the sample time to a host time. The zero
    //    time stamps are spaced such that the sample times are the value of
    //    kAudioDevicePropertyZeroTimeStampPeriod apart. This is often modeled using a ring buffer
    //    where the zero time stamp is updated when wrapping around the ring buffer.
    //
    //    For this device, the zero time stamps' sample time increments every kDevice_RingBufferSize
    //    frames and the host time increments by kDevice_RingBufferSize * gDevice_HostTicksPerFrame.

#pragma unused(inClientID)
    
    //    declare the local variables
    OSStatus theAnswer = 0;
    UInt64 theCurrentHostTime;
    Float64 theHostTicksPerRingBuffer;
    Float64 theHostTickOffset;
    UInt64 theNextHostTime;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetZeroTimeStamp: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "GetZeroTimeStamp: bad device ID");

    {
        CAMutex::Locker locker(&getZeroTimestampMutex);
        
        //    get the current host time
        theCurrentHostTime = mach_absolute_time();

        // In order to keep the input and output IO in sync, we keep a running
        // average of the output IO proc's mRateScalar field for its output time.
        // Then each time we calculate the next zero timestamp we slightly adjust
        // the tick count for our ring buffer by however much the output IO proc
        // deviated from its sample rate.
        
        Float64 rateRatio = 1.0;
        
        if (outputAccumulatedRateRatioSamples > 0) {
            rateRatio = outputAccumulatedRateRatio / outputAccumulatedRateRatioSamples;
        }
        
        //    calculate the next host time
        theHostTicksPerRingBuffer =
            gDevice_HostTicksPerFrame * ((Float64)kDevice_RingBufferSize) * rateRatio;
        theHostTickOffset = gDevice_ElapsedTicks + theHostTicksPerRingBuffer;
        theNextHostTime = gDevice_AnchorHostTime + ((UInt64)theHostTickOffset);

        //    go to the next time if the next host time is less than the current time
        if (theNextHostTime <= theCurrentHostTime) {
            ++gDevice_NumberTimeStamps;
            gDevice_ElapsedTicks += theHostTicksPerRingBuffer;
        }

        //    set the return values
        *outSampleTime = gDevice_NumberTimeStamps * kDevice_RingBufferSize;
        *outHostTime = gDevice_AnchorHostTime + gDevice_ElapsedTicks;
        *outSeed = 1;
        outputAccumulatedRateRatio = 0.0;
        outputAccumulatedRateRatioSamples = 0;
    }

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::WillDoIOOperation(AudioServerPlugInDriverRef inDriver,
                                             AudioObjectID inDeviceObjectID,
                                             UInt32 inClientID,
                                             UInt32 inOperationID,
                                             Boolean *outWillDo,
                                             Boolean *outWillDoInPlace) {
    //    This method returns whether or not the device will do a given IO operation. For this device,
    //    we only support reading input data and writing output data.

#pragma unused(inClientID)

    //    declare the local variables
    OSStatus theAnswer = 0;
    bool willDo = false;
    bool willDoInPlace = true;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "WillDoIOOperation: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "WillDoIOOperation: bad device ID");

    //    figure out if we support the operation
    switch (inOperationID) {
        case kAudioServerPlugInIOOperationReadInput:
            willDo = true;
            willDoInPlace = true;
            break;

        case kAudioServerPlugInIOOperationWriteMix:
            willDo = true;
            willDoInPlace = true;
            break;
    };

    //    fill out the return values
    if (outWillDo != NULL) {
        *outWillDo = willDo;
    }
    if (outWillDoInPlace != NULL) {
        *outWillDoInPlace = willDoInPlace;
    }

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::BeginIOOperation(AudioServerPlugInDriverRef inDriver,
                                            AudioObjectID inDeviceObjectID,
                                            UInt32 inClientID,
                                            UInt32 inOperationID,
                                            UInt32 inIOBufferFrameSize,
                                            const AudioServerPlugInIOCycleInfo *inIOCycleInfo) {
    //    This is called at the beginning of an IO operation. This device doesn't do anything, so just
    //    check the arguments and return.

#pragma unused(inClientID, inOperationID, inIOBufferFrameSize, inIOCycleInfo)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "BeginIOOperation: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "BeginIOOperation: bad device ID");

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::DoIOOperation(AudioServerPlugInDriverRef inDriver,
                                         AudioObjectID inDeviceObjectID,
                                         AudioObjectID inStreamObjectID,
                                         UInt32 inClientID,
                                         UInt32 inOperationID,
                                         UInt32 inIOBufferFrameSize,
                                         const AudioServerPlugInIOCycleInfo *inIOCycleInfo,
                                         void *ioMainBuffer,
                                         void *ioSecondaryBuffer) {
    //    This is called to actuall perform a given operation. For this device, all we need to do is
    //    clear the buffer for the ReadInput operation.

#pragma unused(inClientID, ioSecondaryBuffer)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "DoIOOperation: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "DoIOOperation: bad device ID");
    FailWithAction((inStreamObjectID != kObjectID_Stream_Output),
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "DoIOOperation: bad stream ID");

    //    clear the buffer if this iskAudioServerPlugInIOOperationReadInput
    if (inOperationID == kAudioServerPlugInIOOperationReadInput) {
        memset(ioMainBuffer, 0, inIOBufferFrameSize * 8);

    } else if (inOperationID == kAudioServerPlugInIOOperationWriteMix) {
        if (inputBuffer) {
            CAMutex::Locker locker(IOMutex);

            inputBuffer->Store((const Byte *)ioMainBuffer, inIOBufferFrameSize, inIOCycleInfo->mOutputTime.mSampleTime);
            
            lastInputFrameTime = inIOCycleInfo->mOutputTime.mSampleTime;
            lastInputBufferFrameSize = inIOBufferFrameSize;
            inputCycleCount += 1;
        }
    }

Done:
    return theAnswer;
}

OSStatus ProxyAudioDevice::outputDeviceIOProcStatic(AudioDeviceID inDevice,
                                                    const AudioTimeStamp *inNow,
                                                    const AudioBufferList *inInputData,
                                                    const AudioTimeStamp *inInputTime,
                                                    AudioBufferList *outOutputData,
                                                    const AudioTimeStamp *inOutputTime,
                                                    void *inClientData) {
    if (!inClientData) {
        return noErr;
    }

    return ((ProxyAudioDevice *)inClientData)
        ->outputDeviceIOProc(inDevice, inNow, inInputData, inInputTime, outOutputData, inOutputTime);
}

OSStatus ProxyAudioDevice::outputDeviceIOProc(AudioDeviceID inDevice,
                                              const AudioTimeStamp *inNow,
                                              const AudioBufferList *inInputData,
                                              const AudioTimeStamp *inInputTime,
                                              AudioBufferList *outOutputData,
                                              const AudioTimeStamp *inOutputTime) {
#pragma unused(inDevice)
#pragma unused(inNow)
#pragma unused(inInputData)
#pragma unused(inInputTime)
    CAMutex::Locker locker1(IOMutex);

    // In theory we don't need a locking mechanism here, because outputDevice will only be modified
    // while it is not playing.
    Float64 currentOutputDeviceSampleRate = outputDevice.sampleRate;
    UInt32 currentOutputDeviceBufferFrameSize = outputDevice.bufferFrameSize;
    UInt32 currentOutputDeviceSafetyOffset = outputDevice.safetyOffset;
    Float64 currentInputDeviceSampleRate;
    UInt32 currentInputDeviceChannelCount;
    Float32 currentVolumeR, currentVolumeL;
    bool currentMute;

    {
        CAMutex::Locker stateLocker(&stateMutex);
        currentInputDeviceSampleRate = gDevice_SampleRate;
        currentInputDeviceChannelCount = gDevice_ChannelsPerFrame;
        currentVolumeR = gVolume_Output_R_Value;
        currentVolumeL = gVolume_Output_L_Value;
        currentMute = gMute_Output_Mute;
    }
    
    {
        CAMutex::Locker locker(&getZeroTimestampMutex);
        
        // We don't need to keep taking samples of the device's ratio past
        // 10000 samples. If we get that far then the device is idling.
        if (outputAccumulatedRateRatioSamples < 10000) {
            outputAccumulatedRateRatio += inOutputTime->mRateScalar;
            outputAccumulatedRateRatioSamples += 1;
        }
    }
    
    inputCycleCount = 0;

    if (lastInputFrameTime < 0 || lastInputBufferFrameSize < 0) {
        return noErr;
    }

    if (currentOutputDeviceSampleRate != currentInputDeviceSampleRate) {
        DebugMsg("ProxyAudio: cannot play, mismatched sample rate");
        return noErr;
    }

    if (inputOutputSampleDelta == -1) {
        DebugMsg("ProxyAudio: outputDeviceIOProc recalculating inputOutputSampleDelta");
        Float64 targetFrameTime = (lastInputFrameTime - lastInputBufferFrameSize - currentOutputDeviceBufferFrameSize
                                   - currentOutputDeviceSafetyOffset);
        inputOutputSampleDelta = targetFrameTime - inOutputTime->mSampleTime;
        smallestFramesToBufferEnd = -1;
    }
        
    Float64 startFrame = inOutputTime->mSampleTime + inputOutputSampleDelta;

    if (inputFinalFrameTime != -1 && startFrame >= inputFinalFrameTime) {
        return noErr;
    }

    bool overrun = inputBuffer->Fetch(workBuffer, currentOutputDeviceBufferFrameSize, (SInt64)startFrame);

#if DEBUG
    // This is just some debugging info to tell when we might be gradually
    // approaching the end of the input buffer and headed for a buffer
    // overrun
    SInt64 framesToBufferEnd =
        inputBuffer->mEndFrame - (SInt64(startFrame) + SInt64(currentOutputDeviceBufferFrameSize));

    if (smallestFramesToBufferEnd == -1
        || (framesToBufferEnd < smallestFramesToBufferEnd && smallestFramesToBufferEnd >= 0)) {
        smallestFramesToBufferEnd = framesToBufferEnd;
        DebugMsg("ProxyAudio: frames to buffer end shrunk, is now: %lld", smallestFramesToBufferEnd);
    }
#endif

    if (overrun && inputFinalFrameTime == -1 && startFrame >= inputBuffer->mStartFrame) {
        // Since this warning could conceivably happen every cycle, explicitly make it
        // only appear once every five seconds at most
        static time_t lastBufferOverrunWarning = 0;
        time_t seconds;
        time(&seconds);
        
        if ((seconds - lastBufferOverrunWarning) > 5) {
            lastBufferOverrunWarning = seconds;
            syslog(LOG_WARNING, "ProxyAudio: output unexpected overrun");
            syslog(LOG_WARNING, "ProxyAudio: output frame: %lf", startFrame);
            syslog(LOG_WARNING,
                   "ProxyAudio: output buffer start: %llu    end: %llu",
                   inputBuffer->mStartFrame,
                   inputBuffer->mEndFrame);
        }
    }
    
    Float32 volumeFactorL = 1.0, volumeFactorR = 1.0;
    calculateVolumeFactors(currentVolumeL, currentVolumeR, currentMute, volumeFactorL, volumeFactorR);

    for (UInt32 bufferIndex = 0; bufferIndex < outOutputData->mNumberBuffers; bufferIndex++) {
        UInt32 outputChannelCount = outOutputData->mBuffers[bufferIndex].mNumberChannels;
        UInt32 numChannelsToProcess = std::min(outputChannelCount, currentInputDeviceChannelCount);

        for (UInt32 channelIndex = 0; channelIndex < numChannelsToProcess; channelIndex++) {
            Float32 *in = (Float32 *)workBuffer + channelIndex;
            Float32 *out = (Float32 *)outOutputData->mBuffers[bufferIndex].mData + channelIndex;
            long framesize = outputChannelCount * sizeof(Float32);

            for (UInt32 frame = 0; frame < outOutputData->mBuffers[bufferIndex].mDataByteSize; frame += framesize) {
                *out += (*in * ((channelIndex == 0) ? volumeFactorL : volumeFactorR));
                in += currentInputDeviceChannelCount;
                out += outputChannelCount;
            }
        }
    }

    return noErr;
}

void ProxyAudioDevice::calculateVolumeFactors(Float32 volumeL,
                                              Float32 volumeR,
                                              bool mute,
                                              Float32 &volumeFactorL,
                                              Float32 &volumeFactorR) {
    if (volumeL <= 0.0 || mute) {
        volumeFactorL = 0.0;
    } else if (volumeL >= 1.0) {
        volumeFactorL = 1.0;
    } else {
        volumeFactorL = pow(10, (volumeL * (kVolume_MaxDB - kVolume_MinDB) + kVolume_MinDB) / 10);
    }

    if (volumeR <= 0.0 || mute) {
        volumeFactorR = 0.0;
    } else if (volumeR >= 1.0) {
        volumeFactorR = 1.0;
    } else {
        volumeFactorR = pow(10, (volumeR * (kVolume_MaxDB - kVolume_MinDB) + kVolume_MinDB) / 10);
    }
}

OSStatus ProxyAudioDevice::EndIOOperation(AudioServerPlugInDriverRef inDriver,
                                          AudioObjectID inDeviceObjectID,
                                          UInt32 inClientID,
                                          UInt32 inOperationID,
                                          UInt32 inIOBufferFrameSize,
                                          const AudioServerPlugInIOCycleInfo *inIOCycleInfo) {
//    This is called at the end of an IO operation. This device doesn't do anything, so just check
//    the arguments and return.

#pragma unused(inClientID, inOperationID, inIOBufferFrameSize, inIOCycleInfo)

    //    declare the local variables
    OSStatus theAnswer = 0;

    //    check the arguments
    FailWithAction(inDriver != gAudioServerPlugInDriverRef,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "EndIOOperation: bad driver reference");
    FailWithAction(inDeviceObjectID != kObjectID_Device,
                   theAnswer = kAudioHardwareBadObjectError,
                   Done,
                   "EndIOOperation: bad device ID");

Done:
    return theAnswer;
}

void ProxyAudioDevice::parseConfigurationString(CFStringRef configString, ConfigType &action, CFStringRef &value) {
    CFRange splitter = CFStringFind(configString, CFSTR("="), 0);

    if (splitter.location == kCFNotFound) {
        return;
    }

    CFStringSmartRef actionString = CFStringCreateWithSubstring(NULL, configString, CFRangeMake(0, splitter.location));

    if (CFStringCompare(actionString, CFSTR("outputDevice"), 0) == kCFCompareEqualTo) {
        action = ConfigType::outputDevice;
    } else if (CFStringCompare(actionString, CFSTR("outputDeviceBufferFrameSize"), 0) == kCFCompareEqualTo) {
        action = ConfigType::outputDeviceBufferFrameSize;
    } else if (CFStringCompare(actionString, CFSTR("deviceName"), 0) == kCFCompareEqualTo) {
        action = ConfigType::deviceName;
    } else {
        return;
    }

    if (splitter.location == kCFNotFound) {
        return;
    }

    value =
        CFStringCreateWithSubstring(NULL,
                                    configString,
                                    CFRangeMake(splitter.location + splitter.length,
                                                CFStringGetLength(configString) - splitter.location - splitter.length));
}

#pragma mark Driver Configuration

void ProxyAudioDevice::setConfigurationValue(ConfigType type, CFStringRef value) {
    switch (type) {
        case ConfigType::outputDevice:
            setOutputDevice(value);
            break;

        case ConfigType::outputDeviceBufferFrameSize:
            setOutputDeviceBufferFrameSize(CFStringGetIntValue(value));
            break;

        case ConfigType::deviceName:
            setDeviceName(value);
            break;
        
        default:
            break;
    }
}

CFStringRef ProxyAudioDevice::copyConfigurationValue(ConfigType type) {
    CAMutex::Locker locker(stateMutex);
    
    switch (type) {
        case ConfigType::outputDevice:
            return CFStringCreateCopy(NULL, outputDeviceUID);
            
        case ConfigType::outputDeviceBufferFrameSize:
            return CFStringCreateWithFormat(NULL, NULL, CFSTR("%u"), outputDeviceBufferFrameSize);
            
        case ConfigType::deviceName:
            return CFStringCreateCopy(NULL, deviceName);
            
        default:
            return nullptr;
    }
}

CFStringRef ProxyAudioDevice::copyDeviceNameFromStorage()
{
    DebugMsg("ProxyAudio: copyDeviceNameFromStorage");
    
    if (!gPlugIn_Host) {
        DebugMsg("ProxyAudio: copyDeviceNameFromStorage no plugin host");
        return nullptr;
    }
    
    CFStringRef result = nullptr;
    CFPropertyListSmartRef data;
    
    gPlugIn_Host->CopyFromStorage(gPlugIn_Host, CFSTR("deviceName"), &data);
    
    if (data != NULL && CFGetTypeID(data) == CFStringGetTypeID()) {
        result = CFStringCreateCopy(NULL, CFStringRef(CFPropertyListRef(data)));
    }

    if (result == NULL) {
        CFBundleRef bundle = CFBundleGetBundleWithIdentifier(CFSTR(kPlugIn_BundleID));
        result = CFBundleCopyLocalizedString(
            bundle, CFSTR("DeviceName"), CFSTR("Proxy Audio Device"), CFSTR("Localizable"));
    }

    if (result == NULL) {
        result = CFStringCreateCopy(NULL, CFSTR("Proxy Audio Device"));
    }
    
    DebugMsg("ProxyAudio: copyDeviceNameFromStorage finished");
    
    return result;
}

void ProxyAudioDevice::setDeviceName(CFStringRef newName) {
    if (!newName || !gPlugIn_Host) {
        return;
    }
    
    {
        CAMutex::Locker locker(stateMutex);
        
        if (deviceName) {
            CFRelease(deviceName);
        }
        
        deviceName = CFStringCreateCopy(NULL, newName);
    }
    
    ExecuteInAudioOutputThread(^() {
        CAMutex::Locker locker(stateMutex);
        
        gPlugIn_Host->WriteToStorage(gPlugIn_Host, CFSTR("deviceName"), deviceName);
        
        AudioObjectPropertyAddress theAddress = {
            kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
        gPlugIn_Host->PropertiesChanged(gPlugIn_Host, kObjectID_Device, 1, &theAddress);
    });
}

CFStringRef ProxyAudioDevice::copyDefaultProxyOutputDeviceUID() {
    DebugMsg("ProxyAudio: copyDefaultProxyOutputDeviceUID");
    
    // First we check the default output device and make sure it's not the proxy audio device:
    AudioObjectID defaultDevice = AudioDevice::defaultOutputDevice();
    
    if (defaultDevice != kAudioObjectUnknown) {
        CFStringSmartRef uid = AudioDevice::copyDeviceUID(defaultDevice);
        
        if (uid && CFStringCompare(uid, CFSTR(kDevice_UID), 0) != kCFCompareEqualTo) {
            DebugMsg("ProxyAudio: copyDefaultProxyOutputDeviceUID returning default output device");
            return uid;
        }
    }
    
    // Failing that, we scan through all devices with stereo output capabilities and find the first
    // one that's not the proxy audio device:
    std::vector<AudioObjectID> outputDevices = AudioDevice::devicesWithOutputCapabilitiesThatAreNotProxyAudioDevice();
    
    if (outputDevices.size() > 0) {
        DebugMsg("ProxyAudio: copyDefaultProxyOutputDeviceUID returning first viable output device in list");
        return AudioDevice::copyDeviceUID(outputDevices[0]);
    }
    
    DebugMsg("ProxyAudio: copyDefaultProxyOutputDeviceUID could not find output device");
    
    return nullptr;
}

CFStringRef ProxyAudioDevice::copyOutputDeviceUIDFromStorage() {
    DebugMsg("ProxyAudio: copyOutputDeviceUIDFromStorage");
    
    if (!gPlugIn_Host) {
        DebugMsg("ProxyAudio: copyOutputDeviceUIDFromStorage no plugin host");
        return nullptr;
    }

    CFStringRef result = nullptr;
    CFPropertyListSmartRef data;

    gPlugIn_Host->CopyFromStorage(gPlugIn_Host, CFSTR("outputDeviceUID"), &data);

    if (data != NULL && CFGetTypeID(data) == CFStringGetTypeID()
        && CFStringCompare(CFStringRef(CFPropertyListRef(data)), CFSTR(kDevice_UID), 0) != kCFCompareEqualTo) {
        result = CFStringCreateCopy(NULL, CFStringRef(CFPropertyListRef(data)));
        DebugMsg("ProxyAudio: copyOutputDeviceUIDFromStorage finished with stored output device UID");
        return result;
    }

    DebugMsg("ProxyAudio: copyOutputDeviceUIDFromStorage no output device UID in storage");
    
    return nullptr;
}

void ProxyAudioDevice::setOutputDevice(CFStringRef deviceUID) {
    if (!gPlugIn_Host) {
        return;
    }
    
    {
        CAMutex::Locker locker(&stateMutex);
        
        if (outputDeviceUID) {
            CFRelease(outputDeviceUID);
        }
        
        outputDeviceUID = CFStringCreateCopy(NULL, deviceUID); 
    }
    
    ExecuteInAudioOutputThread(^{
        CAMutex::Locker locker(&stateMutex);
        gPlugIn_Host->WriteToStorage(gPlugIn_Host, CFSTR("outputDeviceUID"), outputDeviceUID);
    });
    
    ExecuteInAudioOutputThread(^{
        setupTargetOutputDevice();
    });
}

UInt32 ProxyAudioDevice::retrieveOutputDeviceBufferFrameSizeFromStorage() {
    DebugMsg("ProxyAudio: retrieveOutputDeviceBufferFrameSizeFromStorage");
    
    if (!gPlugIn_Host) {
        DebugMsg("ProxyAudio: retrieveOutputDeviceBufferFrameSizeFromStorage no plugin host");
        return kOutputDeviceDefaultBufferFrameSize;
    }

    CFPropertyListSmartRef data;
    gPlugIn_Host->CopyFromStorage(gPlugIn_Host, CFSTR("outputDeviceBufferFrameSize"), &data);

    if (data == NULL || CFGetTypeID(data) != CFNumberGetTypeID()) {
        DebugMsg("ProxyAudio: retrieveOutputDeviceBufferFrameSizeFromStorage finished returning default buffer frame size");
        return kOutputDeviceDefaultBufferFrameSize;
    }

    SInt32 value;
    CFNumberGetValue(CFNumberRef(CFPropertyListRef(data)), kCFNumberSInt32Type, &value);
    value = std::max(value, kOutputDeviceMinBufferFrameSize);
    
    DebugMsg("ProxyAudio: retrieveOutputDeviceBufferFrameSizeFromStorage finished returning stored buffer frame size");
    
    return UInt32(value);
}

void ProxyAudioDevice::setOutputDeviceBufferFrameSize(UInt32 newSize) {
    if (newSize <= 0 || newSize > INT32_MAX) {
        return;
    }
    
    {
        CAMutex::Locker locker(&stateMutex);
        outputDeviceBufferFrameSize = newSize;
        CFNumberSmartRef newSizeRef = CFNumberCreate(NULL, kCFNumberSInt32Type, &newSize);
        gPlugIn_Host->WriteToStorage(gPlugIn_Host, CFSTR("outputDeviceBufferFrameSize"), newSizeRef);
    }
    
    ExecuteInAudioOutputThread(^{
        setupTargetOutputDevice();
    });
}

dispatch_queue_t ProxyAudioDevice::AudioOutputDispatchQueue() {
    return audioOutputQueue;
}

void ProxyAudioDevice::ExecuteInAudioOutputThread(void (^block)()) {
    dispatch_async(AudioOutputDispatchQueue(), block);
}
