

#include <OSLibPrivate.h>
#include <OSThread.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>

#define DEFAULT_STACK_SIZE (10240 * 1024)  // Default stack size

/* Function Name        : OSCreateThread
 * IN            : OSHANDLE()(OSHANDLE),
 *                  OSHANDLE
 * OUT            : void
 * RETURN        : OSHANDLE
 * DESCRIPTION        : Returns the reference to the newly created thread.
 */

OSHANDLE OSCreateThread(OSHANDLE (*startRoutine)(OSHANDLE), OSHANDLE args)
{
    pthread_attr_t threadAttr;

    pthread_t *threadObject = (pthread_t *)malloc(sizeof(pthread_t));

    if (threadObject == NULL)  // Unable to allocate memory
    {
        return NULL;
    }

    // Initializing thread attribute
    pthread_attr_init(&threadAttr);
    pthread_attr_setstacksize(&threadAttr, DEFAULT_STACK_SIZE);

    if (pthread_create(threadObject, &threadAttr, startRoutine, args))
    {
        free(threadObject);
        threadObject = NULL;
    }

    pthread_attr_destroy(&threadAttr);

    return threadObject;
}

/* Planning to implement later */

void OSWaitForThreadTerminate(OSHANDLE threadObject)
{
    OSHANDLE value;

    if (NULL != threadObject)
    {
        pthread_join(*(pthread_t *)threadObject, &value);
    }
}
