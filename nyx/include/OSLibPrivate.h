#pragma once

#include "OSCommon.h"
#include "OSLog.h"

#define NYX_BUILD_DATE __DATE__
#define NYX_BUILD_TIME __TIME__

#define MAX_FILE_NAME_LENGTH MAX_STR_LENGTH
#define MAX_MODULE_NAME_LENGTH 256
#define MAX_MODULE_NAME_PRACTICAL_LENGTH 10
#define ASCII_HYPHEN_CHAR_VALUE 0x2D
#define MAX_LOG_TYPE 5

#define NULL_CHAR '\0'
#define MSG_TRUNC_END "..."

/* Debug Message */
#ifdef OS_DEBUG_ENABLE
#define OS_DEBUG_MESSAGE(x) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, x)
#else
#define OS_DEBUG_MESSAGE(x)
#endif

#define UNUSED(x) (void)(x)
#define IGNORE_RETURN(fn) (void)!(fn)

/* Array Element Count*/
#define _is_array(a) \
    ZERO_IF_BUILT_IN(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))

#define OS_COUNT_OF(ARRAY) \
    (sizeof(ARRAY) / sizeof((ARRAY)[0]) + _is_array(ARRAY))

/* File policy */
typedef enum { OSLPolNewFile = 0, OSLPolOverWrite } LogFilePolicy;

#define TK_LLEVEL "LEVEL"
#define TK_LPOLICY "OVERWRITE"
#define TK_LSIZE "SIZE"
#define TK_LPATH "LOGPATH"
#define TK_REP "REP"

/* Structure for storing Log information */
typedef struct Log_Info
{
    LogType minLogType;
    int noFilterRep;
    unsigned int size;
    char confFilePath[MAX_FILE_NAME_LENGTH];
    char logFilePath[MAX_FILE_NAME_LENGTH];
    char basePath[MAX_FILE_NAME_LENGTH];

    char modName[MAX_MODULE_NAME_LENGTH];
    char module[MAX_MODULE_NAME_LENGTH];
    OSHANDLE monHandle;
    int monitorActive;
    int monUpdFreq;
    LogFilePolicy policy;
} LogInfo;

typedef struct _Log_Entry
{
    char ts[MAX_TIME_BUFF];
    char msg[MAX_STR_LENGTH * 2];
    char mn[MAX_STR_NAME_LEN * 2];
    char ltStr[MAX_MODULE_NAME_PRACTICAL_LENGTH];
    int repCount;
} LogEntry;
