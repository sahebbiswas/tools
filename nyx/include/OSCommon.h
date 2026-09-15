
#pragma once
#include "OSVer.h"

/* OS Macros*/
#define OS_FALSE 0
#define OS_TRUE 1
#define OS_UNDEFINED -1

#define MAX_STR_LENGTH 512
#define MAX_STR_NAME_LEN 20
#define MAX_TIME_BUFF 32
#define MAX_TOKEN_STR_LEN 128
#define NYX_SIZE_1K 1024
#define NYX_SIZE_1M ((NYX_SIZE_1K) * (NYX_SIZE_1K))

#define CONF_SEPARATOR '='
#define CONF_COMMENT_1 '#'
#define CONF_COMMENT_2 ';'
#define CONF_SPACE ' '
#define CONF_NULL '\0'

/* OS variables*/
typedef int OSLIB_BOOL;
typedef void* OSHANDLE;
typedef void* RWLOCKOBJECT;
typedef long long OSU64;
typedef int OSU32;

/* Error codes for OS Neutral Lib */
typedef enum _OS_LIB_ERROR_CODE {
    OS_SUCCESS,
    OS_FAILURE = 0xA0002000,  // 0xA0002000
    OS_INSUFFICIENT_MEM,      // 0xA0002001
    OS_NULL_POINTER,          // 0xA0002002
    OS_MODE_NOT_SUPPORTED,    // 0xA0002003
    OS_OVER_LIMIT,            // 0xA0002004
    OS_PIPE_BROKEN,           // 0xA0002005
    OS_PIPE_CLOSED,           // 0xA0002006
    OS_INVALID_PARAM,         // 0xA0002007
    OS_WAIT_TIMEOUT,          // 0xA0002008
    OS_DLL_LOAD_FAIL,         // 0xA0002009 : Library could not be loaded
    OS_PROC_ADDR_NOT_FOUND,   // 0xA000200A : Function address not found in
                              // loaded library,
    OS_CONF_INVALID_VALUE,    // 0xA000200B : Config file value is not valid
    OS_BUFF_INSUFFICIENT,  // 0xA000200C : Method input buffer is not sufficient
    OS_LOGGING_NOT_INITED,  // 0xA000200D : Logging not initialized, method can
                            // not be invoked
    OS_NOT_ALLOWED          // 0xA000200E

} OSLibStatus;
