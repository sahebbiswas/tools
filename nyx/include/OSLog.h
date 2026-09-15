#pragma once

#include "OSCommon.h"

/**
* Config File parameters definition
* -------------------------------------------------
* Logging is enabled if a file <appname>.txt exists in the current Working
directory
* <appname>.cfg file can be used to throttle logging based on minimum severity
allowed, and configure
* different aspects for the debug logs. All the parameters including the config
file itself is optional.
* (Log file severity defaults to FATAL if config file is not available)
* Log file configuration is decided at start of the nyx engine. If you want to
update the log parameters
* at runtime, use the config API : OSSetlogLevel()
* experimental : Logging can also be initialized with runtime monitoring of the
config file, use the
* OSLogInitEx() API to enable this facility.
*
* Content syntax:
*   LEVEL : <sev>    | where <sev> can be (without quotes)
*                        "TRACE"
*                        "INFO"
*                        "DEBUG"
*                        "WARN"
*                        "ERROR"
*                        "FATAL" (default)
*
*   LOGPATH : <path_to_log_file> ()
*
*   SIZE : <size in Megs> (default and minimum is 10 Megs)
*
*   OVERWRITE : <true_ to enable log rolling for defined file size>  (default is
New File generation)
*   REP		  : <true/false> enable or disable log repeat detection. Default
is true.

*/

#define CONF_FILE_EXT ".conf"
#define CONF_FILE_NAME "nyx"##CONF_FILE_EXT
#define LOG_FILE_EXT ".log"
#define LOG_PATH "/var/log"
#define RELATIVE_CONF_PATH "/conf/"
#define RELATIVE_LOG_PATH "/logs/"

#define NYX_MON_UPDATE_DELAY 2
#define MAX_LOG_SIZE_DEFAULT 10
#define MAX_LOG_SIZE_SANE 256

/* Log types */
typedef enum {
    OSLogIgnore = -4,
    OSLogTrace = -3,
    OSLogInfo,
    OSLogDebug,
    OSLogWarning,
    OSLogError,
    OSLogFatal
} LogType;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All Methods Return OS_SUCCESS on success and OSLibStatus error code ec on
 * failure. Refer to OSCommon.h for details
 */

/**
 * @brief Initialize logger
 *
 * @param moduleName
 * @param config_file
 * @param log_file
 * @return OSLibStatus
 */
OSLibStatus OSLogInit(const char* moduleName,
                      const char* config_file,
                      const char* log_file);

/**
 * @brief Intialize Logger helper. logging will only start if log_file exists,
 *          Generates monitor thread for active log level updates
 *
 * @param moduleName module name in logs
 * @param config_file config file path
 * @param log_file log file path
 * @return OSLibStatus
 */
OSLibStatus OSLogInitEx(const char* moduleName,
                        const char* config_file,
                        const char* log_file);

/**
 * @brief Uninitializes the log module
 *
 * @return OSLibStatus
 */
OSLibStatus OSLogUninit();

/**
 * @brief Single statememt trace msg, log level is trace
 *
 * @param traceMsg
 * @return OSLibStatus
 */
OSLibStatus OSTrace(char* traceMsg);

/**
 * @brief Argument based logger method, configurable log severity defined by
 * logType,
 *
 * @param logType
 * @param msg
 * @param ...
 * @return OSLibStatus
 */
OSLibStatus OSLogMsg(LogType logType, const char* msg, ...);

/**
 * @brief retrieve current log threshold
 *
 * @param pLt
 * @return OSLibStatus
 */

OSLibStatus OSGetlogLevel(LogType* pLt);

/**
 * @brief  Change log threshold
 *
 * @param lt
 * @return OSLibStatus
 */
OSLibStatus OSSetlogLevel(LogType lt);

/**
 * @brief Create Default log Config
 *
 * @param config_path Path to config file, set to NULL if logger is already
 * initialized
 * @param log_file_path Path to log file
 * @return OSLibStatus
 */
OSLibStatus OSCreateDefaultLogConfig(const char* config_path,
                                     const char* log_file_path);

/* Log utility Methods*/

/**
 * @brief Stores the local system time in getTime
 *
 * @param getTime
 * @param format
 *       1  - Get Milisecond Time Stamp
 *       2  - Get Date
 *       3  - Get Date - Time Stamp
 * @return OSLibStatus
 */
OSLibStatus OSGetLocalTime(char* getTime, int format);

/**
 * @fn  OSFileExists(const char *pFilenameIN, unsigned long long *pSizeOUT);
 *
 * @brief   Detect if given file name exists at specifed location

 *
 * @param [in]  pFilenameIN   If non-null, the Filename.
 * @param [out]  pSizeOUT   If non-null, the File size if return value is
 OS_TRUE.
 *
 * @return  OS_TRUE if File was detected, OS_FALSE otherwise.
 */
int OSFileExists(const char* pFilenameIN, unsigned long long* pSizeOUT);

/**
 * @fn  OSLibStatus OSGetUserDefinedTagValue(char* pTag, int* pValue);
 *
 * @brief   OS get user define tag-vlue from nyx Config File
 *
 *
 * @param [in]  pTag   If non-null, the user defined tag.
 * @param [out]  pValue   If non-null, the value corresponding to the given tag.
 *
 * @return  OSLibStatus.
 */
OSLibStatus OSGetUserDefinedTagValue(const char* pTag, int* pValue);

/**
 * @fn  OSLibStatus OSGetlibVersion(char* pMajorVersion, char* pMinorVersion);
 *
 * @brief   OS getlib version.
 *
 *
 * @param [in,out]  pMajorVersion   If non-null, the major version.
 * @param [in,out]  pMinorVersion   If non-null, the minor version.
 *
 * @return  .
 */

OSLibStatus OSGetlibVersion(char* pMajorVersion, char* pMinorVersion);

#ifdef __cplusplus
}
#endif
