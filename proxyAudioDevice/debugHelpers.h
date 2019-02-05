#ifndef debugHelpers_h
#define debugHelpers_h

#include <sys/syslog.h>

#if DEBUG
#define DebugMsg(inFormat, ...) syslog(LOG_NOTICE, inFormat, ##__VA_ARGS__)

#define FailIf(inCondition, inHandler, inMessage)                                                                      \
    if (inCondition) {                                                                                                 \
        DebugMsg("ProxyAudio error: " inMessage);                                                                      \
        goto inHandler;                                                                                                \
    }

#define FailWithAction(inCondition, inAction, inHandler, inMessage)                                                    \
    if (inCondition) {                                                                                                 \
        DebugMsg("ProxyAudio error: " inMessage);                                                                      \
        { inAction; }                                                                                                  \
        goto inHandler;                                                                                                \
    }

#else

#define DebugMsg(inFormat, ...)

#define FailIf(inCondition, inHandler, inMessage)                                                                      \
    if (inCondition) {                                                                                                 \
        goto inHandler;                                                                                                \
    }

#define FailWithAction(inCondition, inAction, inHandler, inMessage)                                                    \
    if (inCondition) {                                                                                                 \
        {                                                                                                              \
            inAction;                                                                                                  \
        }                                                                                                              \
        goto inHandler;                                                                                                \
    }

#endif

#endif
