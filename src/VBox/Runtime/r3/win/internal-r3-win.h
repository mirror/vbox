
#ifndef ___internal_r3_win_h
#define ___internal_r3_win_h

#include "internal/iprt.h"
#include <iprt/types.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Windows OS type as determined by rtSystemWinOSType().
 *
 * @note ASSUMPTIONS are made regarding ordering. Win 9x should come first, then
 *       NT. The Win9x and NT versions should internally be ordered in ascending
 *       version/code-base order.
 */
typedef enum RTWINOSTYPE
{
    kRTWinOSType_UNKNOWN    = 0,
    kRTWinOSType_9XFIRST    = 1,
    kRTWinOSType_95         = kRTWinOSType_9XFIRST,
    kRTWinOSType_95SP1,
    kRTWinOSType_95OSR2,
    kRTWinOSType_98,
    kRTWinOSType_98SP1,
    kRTWinOSType_98SE,
    kRTWinOSType_ME,
    kRTWinOSType_9XLAST     = 99,
    kRTWinOSType_NTFIRST    = 100,
    kRTWinOSType_NT31       = kRTWinOSType_NTFIRST,
    kRTWinOSType_NT351,
    kRTWinOSType_NT4,
    kRTWinOSType_2K,
    kRTWinOSType_XP,
    kRTWinOSType_2003,
    kRTWinOSType_VISTA,
    kRTWinOSType_2008,
    kRTWinOSType_7,
    kRTWinOSType_8,
    kRTWinOSType_81,
    kRTWinOSType_10,
    kRTWinOSType_NT_UNKNOWN = 199,
    kRTWinOSType_NT_LAST    = kRTWinOSType_UNKNOWN
} RTWINOSTYPE;

/**
 * Windows loader protection level.
 */
typedef enum RTR3WINLDRPROT
{
    RTR3WINLDRPROT_INVALID = 0,
    RTR3WINLDRPROT_NONE,
    RTR3WINLDRPROT_NO_CWD,
    RTR3WINLDRPROT_SAFE
} RTR3WINLDRPROT;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
extern DECLHIDDEN(RTR3WINLDRPROT)   g_enmWinLdrProt;
extern DECLHIDDEN(RTWINOSTYPE)      g_enmWinVer;
#ifdef _WINDEF_
extern DECLHIDDEN(HMODULE)          g_hModKernel32;
extern DECLHIDDEN(HMODULE)          g_hModNtDll;
extern DECLHIDDEN(OSVERSIONINFOEXW) g_WinOsInfoEx;
#endif


#endif

