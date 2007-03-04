/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Assertion Workers, Ring-0 Drivers, OS/2.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>

#include <VBox/log.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The last assert message. (in DATA16) */
extern char g_szRTAssertMsg[2048];
/** The length of the last assert message. (in DATA16) */
extern size_t g_cchRTAssertMsg;

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(size_t) rtR0Os2AssertOutputCB(void *pvArg, const char *pachChars, size_t cbChars);


/**
 * The 1st part of an assert message.
 *
 * @param   pszExpr     Expression. Can be NULL.
 * @param   uLine       Location line number.
 * @param   pszFile     Location file name.
 * @param   pszFunction Location function name.
 * @remark  This API exists in HC Ring-3 and GC.
 */
RTDECL(void)    AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
#ifdef IN_GUEST_R0
    RTLogBackdoorPrintf("\n!!Assertion Failed!!\n"
                        "Expression: %s\n"
                        "Location  : %s(%d) %s\n",
                        pszExpr, pszFile, uLine, pszFunction);
#endif

#if defined(DEBUG_bird)
    RTLogComPrintf("\n!!Assertion Failed!!\n"
                   "Expression: %s\n"
                   "Location  : %s(%d) %s\n",
                   pszExpr, pszFile, uLine, pszFunction);
#endif

    g_cchRTAssertMsg = RTStrPrintf(g_szRTAssertMsg, sizeof(g_szRTAssertMsg),
                                   "\r\n!!Assertion Failed!!\r\n"
                                   "Expression: %s\r\n"
                                   "Location  : %s(%d) %s\r\n",
                                   pszExpr, pszFile, uLine, pszFunction);
}


/**
 * The 2nd (optional) part of an assert message.
 *
 * @param   pszFormat   Printf like format string.
 * @param   ...         Arguments to that string.
 * @remark  This API exists in HC Ring-3 and GC.
 */
RTDECL(void) AssertMsg2(const char *pszFormat, ...)
{
    va_list va;

#ifdef IN_GUEST_R0
    va_start(va, pszFormat);
    RTLogBackdoorPrintfV(pszFormat, va);
    va_end(va);
#endif

#if defined(DEBUG_bird)
    va_start(va, pszFormat);
    RTLogComPrintfV(pszFormat, va);
    va_end(va);
#endif

    va_start(va, pszFormat);
    size_t cch = g_cchRTAssertMsg;
    char *pch = &g_szRTAssertMsg[cch];
    cch += RTStrFormatV(rtR0Os2AssertOutputCB, &pch, NULL, NULL, pszFormat, va);
    g_cchRTAssertMsg = cch;
    va_end(va);
}


/**
 * Output callback.
 *
 * @returns number of bytes written.
 * @param   pvArg       Pointer to a char pointer with the current output position.
 * @param   pachChars   Pointer to an array of utf-8 characters.
 * @param   cbChars     Number of bytes in the character array pointed to by pachChars.
 */
static DECLCALLBACK(size_t) rtR0Os2AssertOutputCB(void *pvArg, const char *pachChars, size_t cbChars)
{
    char **ppch = (char **)pvArg;
    char *pch = *ppch;

    while (cbChars-- > 0)
    {
        const char ch = *pachChars++;
        if (ch == '\r')
            continue;
        if (ch == '\n')
        {
            if (pch + 1 >= &g_szRTAssertMsg[sizeof(g_szRTAssertMsg)])
                break;
            *pch++ = '\r';
        }
        if (pch + 1 >= &g_szRTAssertMsg[sizeof(g_szRTAssertMsg)])
            break;
        *pch++ = ch;
    }
    *pch = '\0';

    size_t cbWritten = pch - *ppch;
    *ppch = pch;
    return cbWritten;
}

