#ifndef CFTYPEHELPERS_H
#define CFTYPEHELPERS_H

#include <ApplicationServices/ApplicationServices.h>

// A basic smart pointer class meant to be used with Core Foundation object references
// Automatically releases the CF object once it goes out of scope.
template<typename T>
class CFTypeSmartRef {
  public:
    T item;

    CFTypeSmartRef() {
        item = NULL;
    }

    CFTypeSmartRef(T inItem) {
        item = inItem;
    }

    ~CFTypeSmartRef() {
        if (item) {
            CFRelease(item);
        }
    }

    CFTypeSmartRef &operator=(T inItem) {
        item = inItem;
        return *this;
    }

    operator T &() {
        return item;
    }

    operator const T &() const {
        return item;
    }

    T *operator&() {
        return &item;
    }

    CFTypeRef ref() const {
        return (CFTypeRef)item;
    }
};

typedef CFTypeSmartRef<CFArrayRef> CFArraySmartRef;
typedef CFTypeSmartRef<CFStringRef> CFStringSmartRef;
typedef CFTypeSmartRef<CFNumberRef> CFNumberSmartRef;
typedef CFTypeSmartRef<CFDictionaryRef> CFDictionarySmartRef;
typedef CFTypeSmartRef<CFPropertyListRef> CFPropertyListSmartRef;

#endif // CFTYPEHELPERS_H
