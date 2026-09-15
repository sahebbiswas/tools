
#ifndef OS_OS_UTIL_HEADER_H
#define OS_OS_UTIL_HEADER_H

#include "OSCommon.h"

/**
 @fn    void OSToUpper(char* pIN, int maxLen);

 @brief OS to upper.

 @param [in,out]    pIN If non-null, the in.string
 @param maxLen          The maximum length.
 */

void OSToUpper(char* pIN, int maxLen);

/**
 @fn    void OSToLower(char* pIN, int maxLen);

 @brief OS to lower.

 @param [in,out]    pIN If non-null, the in.String
 @param maxLen          The maximum length.
 */

void OSToLower(char* pIN, int maxLen);

/**
 * @brief Trim white spaces from right
 *
 * @param text
 * @return char*
 */
char* OSRTrim(char* text);

#endif
