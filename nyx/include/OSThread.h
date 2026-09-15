

#ifndef OS_OSTHREAD_H
#define OS_OSTHREAD_H

#include "OSCommon.h"

#define OS_INFINITE_WAIT 0xFFFFFFFF
#define OS_WAIT_OBJECT_0 ((unsigned long)0x00000000L + 0)

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*OS_THREAD_ROUTINE)(void *);

/* Creates a new thread */
OSHANDLE OSCreateThread(OS_THREAD_ROUTINE,  // Start routine address
                        void *args);        // Arguments to be passed

/* Sets the priority value for specified thread */
void OSSetThreadPriority();

/* Makes a thread to wait until a specific thread terminates */
void OSWaitForThreadTerminate(OSHANDLE threadId);

#ifdef __cplusplus
}
#endif

#endif
