#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include "utilities.h"

// Based on StackOverflow thread: https://stackoverflow.com/questions/4643579/objective-c-get-notifications-about-a-users-idle-state
CFTimeInterval getUserIdleTimeInterval() {
    uint64_t idle = 0;
    kern_return_t error;
    
    mach_port_t port;
    io_iterator_t iter;
    IOMasterPort(MACH_PORT_NULL, &port);
    IOServiceGetMatchingServices(port, IOServiceMatching(kIOHIDSystemClass), &iter);
    
    if (!iter) {
        return 0;
    }
    
    io_registry_entry_t entry = IOIteratorNext(iter);
    
    if (!entry) {
        IOObjectRelease(iter);
        return 0;
    }
    
    CFMutableDictionaryRef properties = NULL;
    error = IORegistryEntryCreateCFProperties(entry, &properties, kCFAllocatorDefault, 0);
    
    if (error != KERN_SUCCESS || !properties) {
        if (properties) {
            CFRelease(properties);
        }
        
        IOObjectRelease(entry);
        IOObjectRelease(iter);
        return 0;
    }
    
    CFTypeRef value = kCFNull;
    
    if (!CFDictionaryGetValueIfPresent(properties, CFSTR(kIOHIDIdleTimeKey), &value)) {
        CFRelease(properties);
        IOObjectRelease(entry);
        IOObjectRelease(iter);
        return 0;
    }
    
    if (CFGetTypeID(value) == CFDataGetTypeID()) {
        CFDataGetBytes((CFDataRef)value, CFRangeMake(0, sizeof(idle)), (UInt8 *)&idle);
    } else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &idle);
    }

    CFRelease(properties);
    IOObjectRelease(entry);
    IOObjectRelease(iter);

    return idle / 1000000000.0;
}
