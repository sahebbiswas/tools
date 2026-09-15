#include <OSLibPrivate.h>
#include <OSLog.h>
#include <OSSystem.h>
#include <stdio.h>

#define CHECK(API_CALL)                                        \
    {                                                          \
        OSLibStatus ec = OS_SUCCESS;                           \
        if (OS_SUCCESS != (ec = (API_CALL)))                   \
        {                                                      \
            printf("\n%40s : FAILED (%.8X)\n", #API_CALL, ec); \
            return ec;                                         \
        }                                                      \
        else                                                   \
        {                                                      \
            printf("\n%40s : PASS", #API_CALL);                \
        }                                                      \
    }
#define CHECK_SOFT(API_CALL)                                   \
    {                                                          \
        OSLibStatus ec = OS_SUCCESS;                           \
        if (OS_SUCCESS != (ec = (API_CALL)))                   \
        {                                                      \
            printf("\n%40s : FAILED (%.8X)\n", #API_CALL, ec); \
        }                                                      \
        else                                                   \
        {                                                      \
            printf("\n%40s : PASS", #API_CALL);                \
        }                                                      \
    }

#define CHECK_FAIL(API_CALL)                                   \
    {                                                          \
        OSLibStatus ec = OS_SUCCESS;                           \
        if (OS_SUCCESS != (ec = (API_CALL)))                   \
        {                                                      \
            printf("\n%40s : FAILED (%.8X)\n", #API_CALL, ec); \
        }                                                      \
        else                                                   \
        {                                                      \
            printf("\n%40s : PASS", #API_CALL);                \
            return ec;                                         \
        }                                                      \
    }

int main(int argc, char* argv[])
{
    SysTime local = {};
    char buffer[512] = {0};
    char buffer2[512] = {0};

    UNUSED(argc);
    UNUSED(argv);

    printf("Hello!");
    // CHECK(OSLogInit2("nyx_test"))
    CHECK(OSLogInitEx("Nyxtest", NULL, NULL))
    CHECK_SOFT(OSCreateDefaultLogConfig(NULL, NULL));
    CHECK(OSLogMsg(OSLogInfo, "Hello log File"));

    CHECK(OSGetSystemTime(&local));

    printf("\nCurrent date is : %.2d/%.2d/%d",
           local.wMon + 1,
           local.wDay,
           local.wYear + 1900);
    printf("\nCurrent time is : %.2d:%.2d:%.2d",
           local.wHour,
           local.wMin,
           local.wSec);

    CHECK(OSGetHostName(buffer, 512));
    printf("\n Host : %s", buffer);

    CHECK(OSGetOSInfo(buffer, 512, buffer2, 512));
    printf("\n OS : %s [ %s ]", buffer, buffer2);

    CHECK(OSAuthenticateUser("saheb", "z"));
    CHECK_FAIL(OSAuthenticateUser("saheb", "abcd"));

    CHECK(OSSleep(1));
    printf("\n");
    CHECK(OSLogUninit());
    printf("\n");

    return 0;
}
