
#include <OSLibPrivate.h>
#include <OSLog.h>
#include <OSMutex.h>
#include <OSSystem.h>
#include <OSThread.h>
#include <OSUtil.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

/* Declaring a reference of LogInfo type and initializing it to NULL*/
static LogInfo* g_LogMsg = NULL;
static void* g_LogMutex = NULL;

/* Flag to indicate Log initialization */
static OSLIB_BOOL LOGInit = OS_FALSE;
static OSLIB_BOOL LOGInit2 = OS_FALSE;

// Trace file max size 10MB
#define MAX_TRACE_FILE_SIZE ((g_LogMsg->size) * NYX_SIZE_1M)

unsigned long logFileSize()
{
    unsigned long size = 0;
    FILE* fp = fopen(g_LogMsg->logFilePath, "r");

    if (NULL == fp) return size;
    fseek(fp, 0, SEEK_END);

    size = ftell(fp);

    fclose(fp);
    return size;
}

OSLibStatus archiveLogFile()
{
    char archiveName[MAX_STR_LENGTH] = {0};
    int extLen = strlen(LOG_FILE_EXT);
    int nameLen = strlen(g_LogMsg->logFilePath);
    char tstamp[MAX_STR_NAME_LEN] = {0};
    FILE* tfp = NULL;
    if (OSLPolNewFile == g_LogMsg->policy)
    {
        snprintf(archiveName, MAX_STR_LENGTH, "%s", g_LogMsg->logFilePath);
        nameLen -= extLen;

        OSGetLocalTime(tstamp, 4);
        sprintf(&archiveName[nameLen], "%s%s", tstamp, LOG_FILE_EXT);

        rename(g_LogMsg->logFilePath, archiveName);
    }

    tfp = fopen(g_LogMsg->logFilePath, "w");
    if (NULL != tfp) fclose(tfp);

    return OS_SUCCESS;
}

#define LINE_END(X)                                      \
    if (((X) == CONF_COMMENT_1) || ((X) == CONF_NULL) || \
        ((X) == CONF_COMMENT_2))                         \
    {                                                    \
        return retVal;                                   \
    }

OSLibStatus getTokenValue(char* line, char* token, char* value)
{
    int count = 0;
    int wIndex = 0;
    OSLibStatus retVal = OS_FAILURE;

    memset(token, 0, MAX_TOKEN_STR_LEN);
    memset(value, 0, MAX_TOKEN_STR_LEN);

    while (line[count])
    {
        LINE_END(line[count]);

        if (line[count] <= CONF_SPACE)
        {
            count++;
            continue;
        }

        break;
    }

    //  Read token
    while (line[count])
    {
        LINE_END(line[count]);

        if (line[count] > CONF_SPACE)
        {
            if (line[count] == CONF_SEPARATOR) break;
            token[wIndex++] = line[count++];
            if (wIndex == 127) break;
            continue;
        }
        else if (wIndex)
            break;
        count++;
    }

    // ignore separator
    while (line[count])
    {
        LINE_END(line[count]);

        if (line[count] <= CONF_SPACE)
        {
            count++;
            continue;
        }
        if (line[count] == CONF_SEPARATOR) count++;
        break;
    }

    // Strip leading spaces
    while (line[count] == CONF_SPACE) count++;

    wIndex = 0;
    // read value
    while (line[count])
    {
        if (line[count] == CONF_NULL) return retVal;

        if (line[count] >= CONF_SPACE)
        {
            // Spaces allowed in value
            value[wIndex++] = line[count++];
            if (wIndex == 127) break;
            retVal = OS_SUCCESS;
            continue;
        }
        else if (wIndex)
            break;
        count++;
    }

    return retVal;
}

void OSToUpper(char* pIN, int maxLen)
{
    int count = 0;
    while (pIN[count] && count < maxLen)
    {
        if (pIN[count] >= 'a' && pIN[count] <= 'z')
        {
            pIN[count] -= 32;
        }
        count++;
    }
}

/*
 * <Meta> Potential Util Method
 */
OSLibStatus OSLogToFile(LogType lt, char* tMsg)
{
    FILE* fpLogFile = NULL;
    char localTime[MAX_TIME_BUFF] = {0};
    unsigned long long fileSize = 0;
    int space = 0;
    char spaceStr[MAX_STR_NAME_LEN] = "";
    char tidStr[MAX_STR_NAME_LEN] = "";
    int indx = 0;
    char msg[MAX_STR_LENGTH * 2] = {0};
    int msgLen = 0;

    // ignore dummy log msg
    if (OSLogIgnore == lt)
    {
        return OS_SUCCESS;
    }

    sprintf(msg, "%s", tMsg);
    msgLen = strlen(msg);
    msgLen--;

    if (OS_SUCCESS != OSWaitForMutex(g_LogMutex))
    {
        OS_DEBUG_MESSAGE("Unable to lock log mutex");
        return OS_FAILURE;
    }

    /* Check if init is done and also if logging is enabled */
    if ((LOGInit != OS_TRUE) ||
        (OS_FALSE == OSFileExists(g_LogMsg->logFilePath, &fileSize)))
    {
        OS_DEBUG_MESSAGE("LogInit is not done.");
        OSReleaseMutex(g_LogMutex);
        return OS_FAILURE;
    }

    // Archive or rollover log
    if (logFileSize() >= MAX_TRACE_FILE_SIZE)
    {
        archiveLogFile();
    }

    /* Get the local system time */
    OSGetLocalTime(localTime, 3);
    fpLogFile = fopen(g_LogMsg->logFilePath, "r+");
    if (fpLogFile == NULL)
    {
        OSReleaseMutex(g_LogMutex);
        OS_DEBUG_MESSAGE("Unable to open the log file in append mode");
        return OS_FAILURE;
    }
    fseek(fpLogFile, 0, SEEK_END);

    /* Write the msg into log file */
    fprintf(fpLogFile, "%s", localTime);

    sprintf(tidStr, "%X", (int)pthread_self());
    space = MAX_STR_NAME_LEN - strlen(g_LogMsg->modName) - strlen(tidStr);
    while (space-- > 0) spaceStr[indx++] = ASCII_HYPHEN_CHAR_VALUE;
    spaceStr[indx] = '\0';
    fprintf(fpLogFile, "[%s%s%s] ", g_LogMsg->module, spaceStr, tidStr);

    switch (lt)
    {
        case OSLogFatal:
            fprintf(fpLogFile, "|Fatal> ");
            break;
        case OSLogError:
            fprintf(fpLogFile, "|Error> ");
            break;
        case OSLogWarning:
            fprintf(fpLogFile, "|Warn > ");
            break;
        case OSLogDebug:
            fprintf(fpLogFile, "|Debug> ");
            break;
        case OSLogTrace:
            fprintf(fpLogFile, "|Trace> ");
            break;
        case OSLogInfo:
        default:
            fprintf(fpLogFile, "|info > ");
            break;
    }
    // Remove Trailing New lines
    while (msg[msgLen] == '\n' || msg[msgLen] == '\r') msgLen--;
    msg[msgLen + 1] = 0;

    if (msgLen >= MAX_STR_LENGTH)
    {
        msg[MAX_STR_LENGTH] = 0;
        strcat(msg, MSG_TRUNC_END);
    }

    fprintf(fpLogFile, "%s\n", msg);  // write the string to file
    fclose(fpLogFile);
    OSReleaseMutex(g_LogMutex);
    return OS_SUCCESS;
}

OSLibStatus OSCreateDefaultLogConfig(const char* config_file,
                                     const char* log_file_path)
{
    char conf_file_name[512] = {0};
    if (NULL == config_file)
    {
        if (OS_FALSE == LOGInit)
        {
            OS_DEBUG_MESSAGE("Log Not initialized");
            return OS_INVALID_PARAM;
        }
        else
        {
            sprintf(conf_file_name, "%s", g_LogMsg->confFilePath);
        }
    }
    else
    {
        sprintf(conf_file_name, "%s", config_file);
    }
    FILE* fp = fopen(conf_file_name, "w");
    if (fp == NULL)
    {
        OS_DEBUG_MESSAGE("Failed to create config file");
        return OS_FAILURE;
    }
    // Add Config entries
    /*
    Config Format:
    "LEVEL" : TRACE | FATAL | INFO | DEBUG | WARN | ERROR
    "OVERWRITE" : TRUE | FALSE
    "SIZE" : in Mb
    "LOGPATH" : Path to log file
    */
    fprintf(fp, "# Log Level; TRACE | FATAL | INFO | DEBUG | WARN | ERROR \n");
    fprintf(fp, "LEVEL = TRACE \n");
    fprintf(fp,
            "\n# Log Overwrite, if FALSE, old log is  renamed and archived\n");
    fprintf(fp, "OVERWRITE = TRUE \n");
    fprintf(fp, "\n# Max log file size in Mega bytes\n");
    fprintf(fp, "SIZE = 10 \n");
    fprintf(fp, "\n# Log file path\n");
    fprintf(
        fp, "LOGPATH = %s\n", (log_file_path == NULL) ? ("") : (log_file_path));
    fclose(fp);
    return OS_SUCCESS;
}

void processLoggerConfig()
{
    char line[MAX_FILE_NAME_LENGTH] = {0};
    char token[MAX_TOKEN_STR_LEN] = {0};
    char value[MAX_TOKEN_STR_LEN] = {0};
    FILE* fpConf = NULL;

    OS_DEBUG_MESSAGE(g_LogMsg->confFilePath);

    if (OS_TRUE != LOGInit)
    {
        // set Defaults
        g_LogMsg->minLogType = OSLogFatal;
#ifdef NYX_DEV
        g_LogMsg->minLogType = OSLogTrace;
#endif
        g_LogMsg->size = MAX_LOG_SIZE_DEFAULT;  // 10 Megs
    }

    fpConf = fopen(g_LogMsg->confFilePath, "r");
    if (NULL == fpConf)
    {
        OS_DEBUG_MESSAGE("Config File open failed");
        return;
    }

    while (!feof(fpConf))
    {
        IGNORE_RETURN(fgets(line, 500, fpConf));

        if (OS_SUCCESS != getTokenValue(line, token, value)) continue;

        OSToUpper(token, MAX_TOKEN_STR_LEN);
        OS_DEBUG_MESSAGE(token);
        OS_DEBUG_MESSAGE(value);
        if (!strcmp(token, TK_LLEVEL))
        {
            OSRTrim(value);
            OSToUpper(value, MAX_TOKEN_STR_LEN);

            OS_DEBUG_MESSAGE("Set Level");
            if (!strcmp(value, "TRACE"))
            {
                g_LogMsg->minLogType = OSLogTrace;
                OS_DEBUG_MESSAGE(" :: To Trace");
            }
            if (!strcmp(value, "FATAL")) g_LogMsg->minLogType = OSLogFatal;
            if (!strcmp(value, "INFO")) g_LogMsg->minLogType = OSLogInfo;
            if (!strcmp(value, "DEBUG")) g_LogMsg->minLogType = OSLogDebug;
            if (!strcmp(value, "WARN")) g_LogMsg->minLogType = OSLogWarning;
            if (!strcmp(value, "ERROR")) g_LogMsg->minLogType = OSLogError;
        }
        if (!strcmp(token, TK_LSIZE))
        {
            g_LogMsg->size = atoi(value);
            if (g_LogMsg->size > MAX_LOG_SIZE_SANE ||
                g_LogMsg->size < MAX_LOG_SIZE_DEFAULT)
            {
                g_LogMsg->size = MAX_LOG_SIZE_DEFAULT;
            }
        }
        if (!strcmp(token, TK_LPATH) && (OS_FALSE == LOGInit) &&
            (0 != strlen(value)))
        {
            // Set log path only when  app initializes
            sprintf(g_LogMsg->logFilePath, "%s", value);
        }
        if (!strcmp(token, TK_LPOLICY))
        {
            g_LogMsg->policy = OSLPolNewFile;

            OSToUpper(value, MAX_TOKEN_STR_LEN);
            if (!strcmp(value, "TRUE"))
            {
                g_LogMsg->policy = OSLPolOverWrite;
            }
        }
    }
    fclose(fpConf);
}

void* MonitorConf(void* args)
{
    UNUSED(args);

    // Set Monitor Active
    g_LogMsg->monitorActive = OS_TRUE;
    if (0 == g_LogMsg->monUpdFreq) g_LogMsg->monUpdFreq = NYX_MON_UPDATE_DELAY;

    // Start monitor loop
    while (OS_TRUE == g_LogMsg->monitorActive)
    {
        processLoggerConfig();
        OSSleep(g_LogMsg->monUpdFreq);
    }

    return NULL;
}

OSLibStatus OSLogInitEx(const char* moduleName,
                        const char* config_file,
                        const char* log_file)
{
    OSLibStatus retVal = OS_SUCCESS;

    // Initialize Logging
    OS_DEBUG_MESSAGE("Init log");
    if ((OS_SUCCESS ==
         (retVal = OSLogInit(moduleName, config_file, log_file))) &&
        (OS_TRUE == LOGInit))
    {
        // Start conf file monitor thread
        OS_DEBUG_MESSAGE("Create Monitor");
        g_LogMsg->monHandle = OSCreateThread(MonitorConf, NULL);
    }
    return retVal;
}

#define PAD_STR(x)                                                            \
    {                                                                         \
        char buff[128] = {0};                                                 \
        snprintf(buff, 128, "%s%*s", x, (uint32_t)(varLen - strlen(x)), pad); \
        sprintf(x, "%s", buff);                                               \
        OSLogMsg(OSLogTrace, "%s", x);                                        \
    }

void PlaceSignature()
{
    char str[MAX_TOKEN_STR_LEN] = {0};
    char buildStr[MAX_TOKEN_STR_LEN] = {0};
    char verStr[MAX_TOKEN_STR_LEN] = {0};
    char tdate[MAX_TOKEN_STR_LEN] = {0};
    char logP[MAX_TOKEN_STR_LEN] = {0};
    char confP[MAX_TOKEN_STR_LEN] = {0};
    char date[MAX_TOKEN_STR_LEN] = {0};

    char pad[3] = "*";
    uint32_t varLen = 0;
    uint32_t i = 0;

    OSGetLocalTime(tdate, 2);
    varLen = snprintf(buildStr,
                      MAX_TOKEN_STR_LEN,
                      "%c%-30s:   %s %s",
                      '*',
                      "  [NYX]BUILD_TIME_STAMP",
                      NYX_BUILD_DATE,
                      NYX_BUILD_TIME);
    varLen = (varLen < (uint32_t)(snprintf(verStr,
                                           MAX_TOKEN_STR_LEN,
                                           "%c%-30s:   %d.%d",
                                           '*',
                                           "  [NYX]BUILD_NUMBER",
                                           NYX_BUILD_MAJOR,
                                           NYX_BUILD_MINOR)))
                 ? strlen(verStr)
                 : varLen;
    varLen = (varLen < (uint32_t)(snprintf(date,
                                           MAX_TOKEN_STR_LEN,
                                           "%c%-30s:   %s",
                                           '*',
                                           "  [NYX]LOG START DATE",
                                           tdate)))
                 ? strlen(date)
                 : varLen;
    varLen = (varLen < (uint32_t)(snprintf(logP,
                                           MAX_TOKEN_STR_LEN,
                                           "%c%-30s:   %s",
                                           '*',
                                           "  [NYX]config_file",
                                           g_LogMsg->confFilePath)))
                 ? strlen(logP)
                 : varLen;
    varLen = (varLen < (uint32_t)(snprintf(confP,
                                           MAX_TOKEN_STR_LEN,
                                           "%c%-30s:   %s",
                                           '*',
                                           "  [NYX]log_file",
                                           g_LogMsg->logFilePath)))
                 ? strlen(confP)
                 : varLen;

    varLen += 2;

    for (i = 0; i < varLen; i++)
    {
        strcat(str, pad);
    }
    OSLogMsg(OSLogTrace, "%s", str);
    PAD_STR(buildStr);
    PAD_STR(verStr);
    PAD_STR(date);
    PAD_STR(logP);
    PAD_STR(confP);
    OSLogMsg(OSLogTrace, "%c%*s", '*', varLen - 1, pad);
    OSLogMsg(OSLogTrace, "%s", str);
}

/* Reference count for Log initializations */
// static volatile unsigned int initCount = 0;

OSLibStatus OSLogInit(const char* moduleName,
                      const char* config_file,
                      const char* log_file)
{
    char filePath[MAX_FILE_NAME_LENGTH] = {0};

    /* Check whether init is already done */
    if (LOGInit == OS_FALSE)
    {
        OS_DEBUG_MESSAGE("Attempt Init log");

        // Monitor not started by default!
        if (g_LogMsg) g_LogMsg->monHandle = NULL;

        if (NULL == (g_LogMutex = OSCreateMutex()))
        {
            return OS_FAILURE;
        }

        if (moduleName == NULL)
        {
            OS_DEBUG_MESSAGE("Module Name parameter of OSLogInit() is NULL");
            return OS_NULL_POINTER;
        }

        g_LogMsg = (LogInfo*)malloc(sizeof(LogInfo));
        if (NULL == g_LogMsg)
        {
            // Unable to allocate memory
            OS_DEBUG_MESSAGE("Unable to allocate memory.");
            return OS_INSUFFICIENT_MEM;
        }

        memset(g_LogMsg, 0, sizeof(LogInfo));
        strcpy(g_LogMsg->modName, moduleName);
        OS_DEBUG_MESSAGE(g_LogMsg->modName);

        // Set file base to CWD
        if (OS_SUCCESS != OSGetBinPath(filePath, MAX_FILE_NAME_LENGTH))
        {
            sprintf(filePath, ".");
        }
        OS_DEBUG_MESSAGE("Located Base");
        snprintf(g_LogMsg->basePath, MAX_FILE_NAME_LENGTH, "%s", filePath);
        OS_DEBUG_MESSAGE(g_LogMsg->basePath);

        if (NULL != config_file)
        {
            snprintf(g_LogMsg->confFilePath,
                     MAX_FILE_NAME_LENGTH,
                     "%s",
                     config_file);
        }
        else
        {
            strcat(filePath, RELATIVE_CONF_PATH);
            strcat(filePath, g_LogMsg->modName);
            strcat(filePath, CONF_FILE_EXT);
            snprintf(
                g_LogMsg->confFilePath, MAX_FILE_NAME_LENGTH, "%s", filePath);
        }

        if (NULL != log_file)
        {
            snprintf(
                g_LogMsg->logFilePath, MAX_FILE_NAME_LENGTH, "%s", log_file);
        }
        else
        {
            sprintf(g_LogMsg->logFilePath, "%s", g_LogMsg->basePath);
            strcat(g_LogMsg->logFilePath, RELATIVE_LOG_PATH);
            strcat(g_LogMsg->logFilePath, g_LogMsg->modName);
            strcat(g_LogMsg->logFilePath, LOG_FILE_EXT);
        }

        OS_DEBUG_MESSAGE(g_LogMsg->confFilePath);
        OS_DEBUG_MESSAGE(g_LogMsg->logFilePath);

        strcpy(g_LogMsg->module, g_LogMsg->modName);

        processLoggerConfig();

        LOGInit = OS_TRUE;
        if (OS_FALSE == LOGInit2)
        {
            PlaceSignature();
            LOGInit2 = OS_TRUE;
        }

    } /* End of !LOGInit */

    return OS_SUCCESS;
}

OSLibStatus OSLogUninit()
{
    OSTrace("*********************** nyx Exit *****************************");

    if (NULL == g_LogMsg) return OS_SUCCESS;

    // Stop monitor Thread
    OS_DEBUG_MESSAGE("Set monitor stop");
    g_LogMsg->monitorActive = OS_FALSE;
    if (NULL != g_LogMsg->monHandle)
        OSWaitForThreadTerminate(g_LogMsg->monHandle);

    if (OS_SUCCESS != OSWaitForMutex(g_LogMutex))
    {
        OS_DEBUG_MESSAGE("Unable to lock log mutex");
        return OS_FAILURE;
    }
    if (g_LogMsg != NULL) free((LogInfo*)g_LogMsg);
    g_LogMsg = NULL;
    LOGInit = OS_FALSE;

    OSReleaseMutex(g_LogMutex);
    OSDeleteMutex(&g_LogMutex);

    return OS_SUCCESS;
}

/*
 * Function Name : OSTrace
 * IN            : void
 * OUT           : char *
 * RETURN        : OSLibStatus
 * DESCRIPTION   : Writes the TRACE message into the log file.
 */

OSLibStatus OSTrace(char* traceMsg)
{
    // Is logging inited?
    if (LOGInit == OS_FALSE || OSLogTrace != g_LogMsg->minLogType)
        return OS_SUCCESS;
    return OSLogToFile(OSLogTrace, traceMsg);
}

OSLibStatus OSGetLocalTime(char* getTime, int format)
{
    struct timeval tv;
    struct timezone tz;
    struct tm localTime;

    gettimeofday(&tv, &tz);
    localtime_r(&tv.tv_sec, &localTime);

    if (format == 1)  // Time format dd/mm/yyyy hh:mm:ss
    {
        sprintf(getTime,
                "%.2d:%.2d:%.2d.%.3d",
                // localTime.tm_mon+1,localTime.wDay,localTime.tm_year+1900,
                localTime.tm_hour,
                localTime.tm_min,
                localTime.tm_sec,
                (int)(tv.tv_usec / 1000));
    }
    else if (2 == format)  // Time format mm_dd_yyyy hh.mm.ss
    {
        sprintf(getTime,
                "%.2d/%.2d/%.2d",
                localTime.tm_mday,
                localTime.tm_mon + 1,
                localTime.tm_year + 1900);
    }
    else if (3 == format)  // Time format mm_dd_yyyy hh.mm.ss
    {
        sprintf(getTime,
                "%.2d/%.2d %.2d:%.2d:%.2d.%.3d",
                localTime.tm_mon + 1,
                localTime.tm_mday,
                localTime.tm_hour,
                localTime.tm_min,
                localTime.tm_sec,
                (int)(tv.tv_usec / 1000));
    }
    else if (4 == format)
    {
        sprintf(getTime,
                "%.2d%.2d%.2d%.2d%.2d",
                localTime.tm_mon + 1,
                localTime.tm_mday,
                localTime.tm_hour,
                localTime.tm_min,
                localTime.tm_sec);
    }
    else
    {
        sprintf(getTime,
                "%.2d/%.2d/%.2d %.2d:%.2d:%.2d",
                localTime.tm_mday,
                localTime.tm_mon + 1,
                localTime.tm_year + 1900,
                localTime.tm_hour,
                localTime.tm_min,
                localTime.tm_sec);
    }

    return OS_SUCCESS;
}

OSLibStatus OSLogMsg(LogType logType, const char* msg, ...)
{
    char debugMsg[MAX_STR_LENGTH * 2] = {0};
    int logSize = 0;
    va_list vArgs;

    if ((LOGInit == OS_FALSE) || logType < g_LogMsg->minLogType)
    {
        OS_DEBUG_MESSAGE("SKIP LOGGING");
        if (LOGInit == OS_FALSE)
        {
            OS_DEBUG_MESSAGE("LOG INIT NOT DONE");
        }
        return OS_SUCCESS;
    }

    va_start(vArgs, msg);

    /* Copy the message into a temporary string */
    logSize = vsnprintf(debugMsg, MAX_STR_LENGTH, msg, vArgs);
    va_end(vArgs);

    if ((MAX_STR_LENGTH <= logSize) || (logSize < 0))
    {
        debugMsg[MAX_STR_LENGTH - 1] = 0;
    }
    return OSLogToFile(logType, debugMsg);
}

int OSFileExists(const char* pFilenameIN, unsigned long long* pSizeOUT)
{
    int rtnVal = 0;

    if (pFilenameIN)
    {
        struct stat statInfo;
        if (stat(pFilenameIN, &statInfo) == 0)
        {
            rtnVal = 1;
            if (pSizeOUT)
            {
                *pSizeOUT = (unsigned long long)statInfo.st_size;
            }
        }
    }
    return rtnVal;
}

char* OSRTrim(char* text)
{
    if (NULL == text) return text;

    char* back = text + strlen(text);
    while (isspace(*(--back)))
        ;
    *(back + 1) = '\0';
    return text;
}
