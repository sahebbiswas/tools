
#include <malloc.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

#include "OSLibPrivate.h"
#include "OSMutex.h"

/*
 * Function Name        : OSCreateMutex
 * IN            : void
 * OUT            : void
 * RETURN        : OSHANDLE
 * DESCRIPTION        : Creates and initializes a mutex object and returns the
 * reference.
 */

OSHANDLE OSCreateMutex()
{
    pthread_mutexattr_t mutexAttr;  // Mutex attributes

    OS_DEBUG_MESSAGE("Create Mutex");
    // Initialize mutex attribute to default values
    pthread_mutexattr_init(&mutexAttr);

    // Initializing mutex attribute
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_t *mutexObject =
        (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

    if (pthread_mutex_init(mutexObject, &mutexAttr) == 0)
    {
        // Destroy mutex attribute
        pthread_mutexattr_destroy(&mutexAttr);
        return mutexObject;
    }
    else
    {
        pthread_mutexattr_destroy(&mutexAttr);
        return NULL;
    }
}

/*
 * Function Name    : OSWaitForMutex
 * IN            : OSHANDLE
 * OUT            : void
 * RETURN        : OSLibStatus
 * DESCRIPTION        : Put a lock on the mutex object, if it is already locked
 * then, waits for the mutex to be available.
 */

OSLibStatus OSWaitForMutex(OSHANDLE mutexObject)
{
    if (mutexObject != NULL)
    {
        if (pthread_mutex_lock((pthread_mutex_t *)mutexObject) == 0)
        {
            return OS_SUCCESS;
        }
        else
        {
            return OS_FAILURE;
        }
    }
    else
    {
        OS_DEBUG_MESSAGE("Mutex object is NULL");
        return OS_NULL_POINTER;
    }
}

/*
 * Function Name        : OSReleaseMutex
 * IN            : OSHANDLE
 * OUT            : void
 * RETURN        : OSLibStatus
 * DESCRIPTION        : Releases the mutex object.
 */

OSLibStatus OSReleaseMutex(OSHANDLE mutexObject)
{
    if (mutexObject != NULL)
    {
        if (pthread_mutex_unlock((pthread_mutex_t *)mutexObject) == 0)
        {
            return OS_SUCCESS;
        }
        else
        {
            return OS_FAILURE;
        }
    }
    else
    {
        OS_DEBUG_MESSAGE("Mutex object is NULL");
        return OS_NULL_POINTER;
    }
}

/*
 * Function Name        : OSDeleteMutex
 * IN            : OSHANDLE
 * OUT            : void
 * RETURN        : OSLibStatus
 * DESCRIPTION        : Deletes the mutex object.
 */

OSLibStatus OSDeleteMutex(OSHANDLE *mutexObject)
{
    if (*mutexObject != NULL)
    {
        if (pthread_mutex_destroy((pthread_mutex_t *)*mutexObject) == 0)
        {
            if (*mutexObject != NULL) free((pthread_mutex_t *)*mutexObject);

            *mutexObject = NULL;
            return OS_SUCCESS;
        }
        else
        {
            OS_DEBUG_MESSAGE("Failed to delete the mutex object");
            return OS_FAILURE;
        }
    }
    else
    {
        OS_DEBUG_MESSAGE(
            "Failed to delete the mutex object. Mutex object is NULL");
        return OS_NULL_POINTER;
    }
}
