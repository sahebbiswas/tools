

#ifndef OS_OSSYSTEM_H
#define OS_OSSYSTEM_H

#include <time.h>
#include "OSCommon.h"

/* Structure for storing System time information */
typedef struct _SysTime
{
    int wMiliSec;  // Mili Second
    int wSec;      // Second
    int wMin;      // Minute
    int wHour;     // Hour
    int wDay;      // Day
    int wDoW;      // Day of Week
    int wMon;      // Month
    int wYear;     // Year
} SysTime;

#define OS_MAX_IP_PER_HOST 8
#define MAX_IP_ADDRESS_LEN 18 /* xxx.xxx.xxx.xxx\0 + 2*/
#define MAX_HOST_NAME_LEN 256

typedef struct _OSHostIpInfo
{
    unsigned char IpCount;
    char hostname[MAX_HOST_NAME_LEN];
    char IpAddr[OS_MAX_IP_PER_HOST][MAX_IP_ADDRESS_LEN];

} OSHostIpInfo;

typedef enum _OSHostArch {
    OS_ARCH_UNKNOWN,
    OS_ARCH_X64,
    OS_ARCH_ARM,
    OS_ARCH_IA64,
    OS_ARCH_X86
} OSHostArch;

#ifdef __cplusplus
extern "C" {
#endif

/* Retrives the local system time */
OSLibStatus OSGetSystemTime(
    SysTime *localTime);  // A pointer to the SysTime structure.

/* Retrives the host name */
OSLibStatus OSGetHostName(
    char *hostName,  // A pointer to a buffer that receives the host name.
    int nameLen);  // The length, in bytes, of the buffer pointed to by the name
                   // parameter.
                   // Maximum length of the buffer should be 256.

/* Retrives the current process Id */
long OSGetProcessId();

/* Reports the OS architecture*/
OSLibStatus OSGetHostArchitecture(char *pArch, int len);

/* Sleeps for specified time */
OSLibStatus OSSleep(int timeInSec);

/* Sleeps for specified milliseconds*/
OSLibStatus OSMilliSleep(int timeInMilliSecs);

/* Load's a dynamic link library at runtime */
void *OSLoadLibrary(const char *library, OSLibStatus *retVal);
void *OSLoadLibrary2(const char *library, OSLibStatus *retVal);

/* Retrieves the address of exported functions of dynamic link library */
void *OSGetFuncAddress(void *dllInstance,
                       const char *funcName,
                       OSLibStatus *retStatus);

/* Unload's a dynamic link library at runtime */
OSLibStatus OSUnloadLibrary(void **dllInstance);

/* Retrieves IP Address(s) of the host */
OSLibStatus OSGetHostIPInfo(OSHostIpInfo *hostIp);

/* Retrieves the Operating System information*/
OSLibStatus OSGetOSInfo(char *osName, int namelen, char *osVer, int verLen);

/* Convert the time stamp into human readable format */
OSLibStatus OSGetFormatedTime(long long in_epoc_msec,
                              char *b_time,
                              int bufflen);

/* Convert the time stamp into struct tm */
OSLibStatus OSGetTimeInTM(long long in_epoc_msec, struct tm *ptm);

/* Get time zone offset from GMT */
OSLibStatus OSGetTimeZone(char *b_time);

void *isApiPresent(char *lib, char *api, OSLibStatus *ret);

/* Compare strings case ignored*/
int OSStrCmpI(const char *str1, int strLen1, const char *str2, int strLen2);

/**
 * @brief Get bin base path
 *
 * @param filepath
 * @param size
 * @return
 */
OSLibStatus OSGetBinPath(char *filepath, int size);

/**
 * @brief OSAuthenticateUser()
 *
 * @author Saheb Biswas
 *
 * @param username
 * @param password
 * @return
 */
OSLibStatus OSAuthenticateUser(const char *username, const char *password);

/**
 *  @brief OSGetModuleFileName() -
 *
 *  @author Saheb Biswas
 *
 *  @param [out] fName Parameter_Description
 *  @param [in] size  Parameter_Description
 *  @return Return_Description
 *
 *  @details Details
 */
OSLibStatus OSGetModuleFileName(char *fName, int size);

#ifdef __cplusplus
}
#endif

#endif
