#pragma once

#include "OSCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Creates a mutes object */
OSHANDLE OSCreateMutex();

/* Put a lock on the mutex object */
OSLibStatus OSWaitForMutex(OSHANDLE mutexObject);

/* Releases the mutex object */
OSLibStatus OSReleaseMutex(OSHANDLE mutexObject);

/* Deletes the mutexobject */
OSLibStatus OSDeleteMutex(OSHANDLE *mutexObject);

#ifdef __cplusplus
}
#endif
