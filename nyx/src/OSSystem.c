/**
 * @file    inc\OSLibPrivate.h
 *
 * @Author  Saheb Biswas (SB)
 * @brief   Defines linux specific implementations
 *           for OS function interfaces.
 */
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "OSLibPrivate.h"
#include "OSSystem.h"

#define validate_time_range(ptm)                                     \
    (ptm.tm_year < 70 || ptm.tm_year > 1100 || (ptm.tm_mon > 11) ||  \
     (ptm.tm_mon < 0) || (ptm.tm_mday > 31) || (ptm.tm_mday < 0) ||  \
     (ptm.tm_hour > 23) || (ptm.tm_hour < 0) || (ptm.tm_min > 59) || \
     (ptm.tm_min < 0) || (ptm.tm_sec > 60) || (ptm.tm_sec < 0))

#define SUPPORTED_TIMESTM_START_YEAR 1970
#define SUPPORTED_TIMESTM_END_YEAR 3000

/*
 * Function Name        : OSGetSystemTime
 * IN            : void
 * OUT            : char *
 * RETURN        : OSLibStatus
 * DESCRIPTION        : Returns the local system time.
 */

OSLibStatus OSGetSystemTime(SysTime *localTime)
{
    struct tm *lTime;
    time_t t;
    t = time(NULL);

    lTime = localtime(&t);

    localTime->wMiliSec = 0; /* Linux does not return this value*/
    localTime->wSec = lTime->tm_sec;
    localTime->wMin = lTime->tm_min;
    localTime->wHour = lTime->tm_hour;
    localTime->wDoW = lTime->tm_wday;
    localTime->wDay = lTime->tm_mday;
    localTime->wMon = lTime->tm_mon;
    localTime->wYear = lTime->tm_year;

    return OS_SUCCESS;
}

/*
 * Function Name        : OSGetHostName
 * IN            : int
 * OUT            : char *
 * RETURN        : OSLibStatus
 * DESCRIPTION        : Returns the host name.
 */

OSLibStatus OSGetHostName(char *hostName, int nameLen)
{
    gethostname(hostName, nameLen);
    return OS_SUCCESS;
}

/*
 * Function Name	: OSSleep
 * IN			: int
 * OUT			: void
 * RETURN		: OSLibStatus
 * DESCRIPTION	: sleeps for a specified time.
 */
OSLibStatus OSSleep(int timeInSec)
{
    sleep(timeInSec);
    return OS_SUCCESS;
}

OSLibStatus GetSystemOutput(char *cmd, char *buff, int buffer_len)
{
    /* simply invoke an app, pipe output*/

    FILE *pipe = NULL;
    char buf[512] = {0};
    int index = 0;
    int bLen = 0;
    memset(buff, 0, buffer_len);

    // redirect stderr
    sprintf(buf, "%s 2>/dev/null", cmd);

    if (NULL == (pipe = popen(buf, "r"))) return OS_FAILURE;
    memset(buf, 0, 512);
    if (!feof(pipe))
    {
        IGNORE_RETURN(fgets(buf, 512, pipe));
    }

    /* Close pipe */
    pclose(pipe);
    if (!strlen(buf)) return OS_FAILURE;
    while (buf[index] != 0)
    {
        if ((buf[index] >= ' ') && (((unsigned char)buf[index]) < 128) &&
            (buf[index] != '\"'))
            buff[bLen++] = buf[index];

        if (bLen >= (buffer_len - 1)) break;
        index++;
    }
    return OS_SUCCESS;
}

/**
 * This function attempts to extract the OS name and version of
 * a Linux based operating system from the release info file
 *
 */

OSLibStatus OSHelperCommonOsNames(char *posInfo,
                                  int len,
                                  char *osVer,
                                  int verLen)
{
    OSLibStatus retVal = OS_FAILURE;

    /* Check if the new standard os-release file is available*/
    if (!system("[ -f /etc/os-release ]"))
    {
        if (OS_SUCCESS == GetSystemOutput("awk \'$1 ~ /PRETTY_NAME/ {print "
                                          "$2}\' FS=\"=\" /etc/os-release",
                                          posInfo,
                                          len))
        {
            GetSystemOutput(
                "awk \'$1 ~ /VERSION_ID/ {print $2}\' FS=\"=\" /etc/os-release",
                osVer,
                verLen);
            return OS_SUCCESS;
        }
    }

    /* try common system specfic files now*/
    if (!system("[ -f /etc/redhat-release ]"))
    {
        if (OS_SUCCESS ==
            GetSystemOutput("cat /etc/redhat-release", posInfo, len))
        {
            GetSystemOutput(
                "cat /etc/redhat-release | sed s/.*release\\ // | sed s/\\ "
                ".*//",
                osVer,
                verLen);
            return OS_SUCCESS;
        }
    }
    else if (!system("[ -f /etc/SuSE-release ]"))
    {
        if (OS_SUCCESS ==
            GetSystemOutput("cat /etc/SuSE-release", posInfo, len))
        {
            GetSystemOutput(
                "cat /etc/SuSE-release | sed s/.*release\\ // | sed s/.*=\\ //",
                osVer,
                verLen);
            return OS_SUCCESS;
        }
    }
    else if (!system("[ -f /etc/mandrake-release ]"))
    {
        if (OS_SUCCESS ==
            GetSystemOutput("cat /etc/mandrake-release", posInfo, len))
        {
            GetSystemOutput(
                "cat /etc/mandrake-release | sed s/.*release\\ // | sed s/\\ "
                ".*//",
                osVer,
                verLen);
            return OS_SUCCESS;
        }
    }
    else if (!system("[ -f /etc/debian-release ]"))
    {
        if (OS_SUCCESS ==
            GetSystemOutput("cat /etc/debian-release", posInfo, len))
        {
            GetSystemOutput(
                "cat /etc/debian-release | sed s/.*release\\ // | sed s/\\ "
                ".*//",
                osVer,
                verLen);
            return OS_SUCCESS;
        }
    }

    return retVal;
}

OSLibStatus OSGetOSInfo(char *posInfo, int len, char *osVer, int verLen)
{
    struct utsname osInfo;
    char tempStr[MAX_STR_LENGTH] = {0};

    /* Check if LSB works*/
    if (OS_SUCCESS == (GetSystemOutput("lsb_release -sd", posInfo, len)))
    {
        GetSystemOutput("lsb_release -sr", osVer, verLen);
        return OS_SUCCESS;
    }

    /* Attempt to identify common OS flavors*/
    if (OS_SUCCESS == OSHelperCommonOsNames(posInfo, len, osVer, verLen))
        return OS_SUCCESS;

    /* LSB Failed , revert to default*/
    int ret = uname(&osInfo);

    if (ret == 0)
    {
        OS_DEBUG_MESSAGE("uname: Successfully retrieved os information.");
        sprintf(tempStr, "%s", osInfo.sysname);
        strncpy(posInfo, tempStr, len);
        sprintf(tempStr, "%s", osInfo.release);
        strncpy(osVer, tempStr, verLen);
    }
    else
    {
        OS_DEBUG_MESSAGE("uname: Cannot retrieved os information.");
        return OS_FAILURE;
    }

    return OS_SUCCESS;
}

OSLibStatus OSGetHostArchitecture(char *pArch, int len)
{
    struct utsname osInfo;
    int ret = 0;
    ;

    if (len < 10) return OS_BUFF_INSUFFICIENT;

    if (0 == (ret = uname(&osInfo)))
    {
        OS_DEBUG_MESSAGE("uname: Successfully retrieved os information.");
        sprintf(pArch, "%s", osInfo.machine);
    }
    else
    {
        OS_DEBUG_MESSAGE("uname: Cannot retrieved os information.");
        return OS_FAILURE;
    }

    return OS_SUCCESS;
}

OSLibStatus OSGetBinPath(char *filepath, int size)
{
    int i = 0;
    if (NULL == filepath) return OS_NULL_POINTER;

    if (-1 == readlink("/proc/self/exe", filepath, size - 1))
    {
        OS_DEBUG_MESSAGE("Cannot get File Path ");  //(%d)\n", GetLastError());
        return OS_FAILURE;
    }
    filepath[size] = 0;
    for (i = strlen(filepath); i >= 0; i--)
    {
        if (filepath[i] == '/')
        {
            filepath[i] = 0;
            break;
        }
    }
    return OS_SUCCESS;
}

OSLibStatus OSAuthenticateUser(const char *username, const char *password)
{
    char *encrptPwd = NULL;
    char *pszEncryptedPwd = NULL;
    struct spwd *spPwdEntry = getspnam(username);
    if (spPwdEntry == NULL)
    {
        OS_DEBUG_MESSAGE("getspname returns NULL");
        return OS_NOT_ALLOWED;
    }

    encrptPwd = spPwdEntry->sp_pwdp;

    pszEncryptedPwd = crypt(password, encrptPwd);

    if (strcmp(pszEncryptedPwd, encrptPwd) == 0)
    {
        return OS_SUCCESS;
    }
    return OS_FAILURE;
}

OSLibStatus OSGetModuleFileName(char *fName, int size)
{
    int i = 0;
    int j = 0;
    Dl_info dl_info;

    UNUSED(size);

    if (NULL == fName) return OS_NULL_POINTER;

    dladdr((void *)OSGetModuleFileName, &dl_info);

    // Get filename start
    for (i = (int)strlen(dl_info.dli_fname); i >= 0; i--)
    {
        if (dl_info.dli_fname[i] == '/')
        {
            break;
        }
    }
    while (dl_info.dli_fname[i] == '/') i++;
    // copy filename
    for (j = i; j <= (int)strlen(dl_info.dli_fname); j++)
        fName[j - i] = dl_info.dli_fname[j];
    fName[j - i] = 0;
    // strip extension
    for (i = (int)strlen(fName); i >= 0; i--)
    {
        if (fName[i] == '.')
        {
            fName[i] = 0;
            break;
        }
    }

    return OS_SUCCESS;
}
