/* $Id$ */
/** @file
 * VirtualBox Support Library - Hardened main(), windows bits.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/nt/nt-and-windows.h>
#include <AccCtrl.h>
#include <AclApi.h>
#ifndef PROCESS_SET_LIMITED_INFORMATION
# define PROCESS_SET_LIMITED_INFORMATION        0x2000
#endif
#ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
# define LOAD_LIBRARY_SEARCH_APPLICATION_DIR    0x200
# define LOAD_LIBRARY_SEARCH_SYSTEM32           0x800
#endif

#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/thread.h>
#include <iprt/zero.h>

#include "SUPLibInternal.h"
#include "win/SUPHardenedVerify-win.h"
#include "../SUPDrvIOC.h"

#ifndef IMAGE_SCN_TYPE_NOLOAD
# define IMAGE_SCN_TYPE_NOLOAD 0x00000002
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The first argument of a respawed stub when respawned for the first time.
 * This just needs to be unique enough to avoid most confusion with real
 * executable names,  there are other checks in place to make sure we've respanwed. */
#define SUPR3_RESPAWN_1_ARG0  "60eaff78-4bdd-042d-2e72-669728efd737-suplib-2ndchild"

/** The first argument of a respawed stub when respawned for the second time.
 * This just needs to be unique enough to avoid most confusion with real
 * executable names,  there are other checks in place to make sure we've respanwed. */
#define SUPR3_RESPAWN_2_ARG0  "60eaff78-4bdd-042d-2e72-669728efd737-suplib-3rdchild"

/** Unconditional assertion. */
#define SUPR3HARDENED_ASSERT(a_Expr) \
    do { \
        if (!(a_Expr)) \
            supR3HardenedFatal("%s: %s\n", __FUNCTION__, #a_Expr); \
    } while (0)

/** Unconditional assertion of NT_SUCCESS. */
#define SUPR3HARDENED_ASSERT_NT_SUCCESS(a_Expr) \
    do { \
        NTSTATUS rcNtAssert = (a_Expr); \
        if (!NT_SUCCESS(rcNtAssert)) \
            supR3HardenedFatal("%s: %s -> %#x\n", __FUNCTION__, #a_Expr, rcNtAssert); \
    } while (0)

/** Unconditional assertion of a WIN32 API returning non-FALSE. */
#define SUPR3HARDENED_ASSERT_WIN32_SUCCESS(a_Expr) \
    do { \
        BOOL fRcAssert = (a_Expr); \
        if (fRcAssert == FALSE) \
            supR3HardenedFatal("%s: %s -> %#x\n", __FUNCTION__, #a_Expr, RtlGetLastWin32Error()); \
    } while (0)


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Security descriptor cleanup structure.
 */
typedef struct MYSECURITYCLEANUP
{
    union
    {
        SID                 Sid;
        uint8_t             abPadding[SECURITY_MAX_SID_SIZE];
    }                       Everyone, Owner, User, Login;
    union
    {
        ACL                 AclHdr;
        uint8_t             abPadding[1024];
    }                       Acl;
    PSECURITY_DESCRIPTOR    pSecDesc;
} MYSECURITYCLEANUP;
/** Pointer to security cleanup structure. */
typedef MYSECURITYCLEANUP *PMYSECURITYCLEANUP;


/**
 * Image verifier cache entry.
 */
typedef struct VERIFIERCACHEENTRY
{
    /** Pointer to the next entry with the same hash value. */
    struct VERIFIERCACHEENTRY * volatile pNext;
    /** Next entry in the WinVerifyTrust todo list. */
    struct VERIFIERCACHEENTRY * volatile pNextTodoWvt;

    /** The file handle. */
    HANDLE                  hFile;
    /** If fIndexNumber is set, this is an file system internal file identifier. */
    LARGE_INTEGER           IndexNumber;
    /** The path hash value. */
    uint32_t                uHash;
    /** The verification result. */
    int                     rc;
    /** Used for shutting up errors after a while. */
    uint32_t volatile       cErrorHits;
    /** The validation flags (for WinVerifyTrust retry). */
    uint32_t                fFlags;
    /** Whether IndexNumber is valid  */
    bool                    fIndexNumberValid;
    /** Whether verified by WinVerifyTrust. */
    bool volatile           fWinVerifyTrust;
    /** cwcPath * sizeof(RTUTF16). */
    uint16_t                cbPath;
    /** The full path of this entry (variable size).  */
    RTUTF16                 wszPath[1];
} VERIFIERCACHEENTRY;
/** Pointer to an image verifier path entry. */
typedef VERIFIERCACHEENTRY *PVERIFIERCACHEENTRY;


/**
 * Name of an import DLL that we need to check out.
 */
typedef struct VERIFIERCACHEIMPORT
{
    /** Pointer to the next DLL in the list. */
    struct VERIFIERCACHEIMPORT * volatile pNext;
    /** The length of pwszAltSearchDir if available. */
    uint32_t                cwcAltSearchDir;
    /** This points the directory containing the DLL needing it, this will be
     * NULL for a System32 DLL. */
    PWCHAR                  pwszAltSearchDir;
    /** The name of the import DLL (variable length). */
    char                    szName[1];
} VERIFIERCACHEIMPORT;
/** Pointer to a import DLL that needs checking out. */
typedef VERIFIERCACHEIMPORT *PVERIFIERCACHEIMPORT;


/**
 * VM process parameters.
 */
typedef struct SUPR3WINPROCPARAMS
{
    /** The event semaphore the child will be waiting on. */
    HANDLE          hEvtChild;
    /** The event semaphore the parent will be waiting on. */
    HANDLE          hEvtParent;

    /** The address of the NTDLL. */
    uintptr_t       uNtDllAddr;

    /** The last status. */
    int32_t         rc;
    /** Error message / path name string space. */
    char            szErrorMsg[4096];
} SUPR3WINPROCPARAMS;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Process parameters.  Specified by parent if VM process, see
 *  supR3HardenedVmProcessInit. */
static SUPR3WINPROCPARAMS   g_ProcParams = { NULL, NULL, 0, 0 };
/** Set if supR3HardenedVmProcessInit was invoked. */
bool                        g_fSupEarlyVmProcessInit = false;

/** @name Global variables initialized by suplibHardenedWindowsMain.
 * @{ */
/** Combined windows NT version number.  See SUP_MAKE_NT_VER_COMBINED. */
uint32_t                    g_uNtVerCombined = 0;
/** Count calls to the special main function for linking santity checks. */
static uint32_t volatile    g_cSuplibHardenedWindowsMainCalls;
/** The UTF-16 windows path to the executable. */
RTUTF16                     g_wszSupLibHardenedExePath[1024];
/** The NT path of the executable. */
SUPSYSROOTDIRBUF            g_SupLibHardenedExeNtPath;
/** The offset into g_SupLibHardenedExeNtPath of the executable name (WCHAR,
 * not byte). This also gives the length of the exectuable directory path,
 * including a trailing slash. */
uint32_t                    g_offSupLibHardenedExeNtName;
/** @} */

/** @name Hook related variables.
 * @{ */
/** The jump back address of the patched NtCreateSection. */
extern "C" PFNRT            g_pfnNtCreateSectionJmpBack = NULL;
/** Pointer to the bit of assembly code that will perform the original
 *  NtCreateSection operation. */
static NTSTATUS (NTAPI *    g_pfnNtCreateSectionReal)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
                                                      PLARGE_INTEGER, ULONG, ULONG, HANDLE);
#if 0
/** The jump back address of the patched LdrLoadDll. */
extern "C" PFNRT            g_pfnLdrLoadDllJmpBack = NULL;
#endif
/** Pointer to the bit of assembly code that will perform the original
 *  LdrLoadDll operation. */
static NTSTATUS (NTAPI *    g_pfnLdrLoadDllReal)(PWSTR, PULONG, PUNICODE_STRING, PHANDLE);
/** The hash table of verifier cache . */
static PVERIFIERCACHEENTRY  volatile g_apVerifierCache[128];
/** Queue of cached images which needs WinVerifyTrust to check them. */
static PVERIFIERCACHEENTRY  volatile g_pVerifierCacheTodoWvt = NULL;
/** Queue of cached images which needs their imports checked. */
static PVERIFIERCACHEIMPORT volatile g_pVerifierCacheTodoImports = NULL;

/** The windows path to dir \\SystemRoot\\System32 directory (technically
 *  this whatever \KnownDlls\KnownDllPath points to). */
SUPSYSROOTDIRBUF            g_System32WinPath;
/** @ */


/** Static error info structure used during init. */
static RTERRINFOSTATIC      g_ErrInfoStatic;

/** In the assembly file. */
extern "C" uint8_t          g_abSupHardReadWriteExecPage[PAGE_SIZE];

/** Whether we've patched our own LdrInitializeThunk or not.  We do this to
 * disable thread creation. */
static bool                 g_fSupInitThunkSelfPatched;
/** The backup of our own LdrInitializeThunk code, for enabling and disabling
 * thread creation in this process. */
static uint8_t              g_abLdrInitThunkSelfBackup[16];

/** Mask of adversaries that we've detected (SUPHARDNT_ADVERSARY_XXX). */
static uint32_t             g_fSupAdversaries = 0;
/** @name SUPHARDNT_ADVERSARY_XXX - Adversaries
 * @{ */
/** Symantec endpoint protection or similar including SysPlant.sys. */
#define SUPHARDNT_ADVERSARY_SYMANTEC_SYSPLANT       RT_BIT_32(0)
/** Symantec Norton 360. */
#define SUPHARDNT_ADVERSARY_SYMANTEC_N360           RT_BIT_32(1)
/** Avast! */
#define SUPHARDNT_ADVERSARY_AVAST                   RT_BIT_32(2)
/** TrendMicro OfficeScan and probably others. */
#define SUPHARDNT_ADVERSARY_TRENDMICRO              RT_BIT_32(3)
/** McAfee.  */
#define SUPHARDNT_ADVERSARY_MCAFEE                  RT_BIT_32(4)
/** Kaspersky or OEMs of it.  */
#define SUPHARDNT_ADVERSARY_KASPERSKY               RT_BIT_32(5)
/** Malwarebytes Anti-Malware (MBAM). */
#define SUPHARDNT_ADVERSARY_MBAM                    RT_BIT_32(6)
/** AVG Internet Security. */
#define SUPHARDNT_ADVERSARY_AVG                     RT_BIT_32(7)
/** Panda Security. */
#define SUPHARDNT_ADVERSARY_PANDA                   RT_BIT_32(8)
/** Microsoft Security Essentials. */
#define SUPHARDNT_ADVERSARY_MSE                     RT_BIT_32(9)
/** Comodo. */
#define SUPHARDNT_ADVERSARY_COMODO                  RT_BIT_32(10)
/** Check Point's Zone Alarm (may include Kaspersky).  */
#define SUPHARDNT_ADVERSARY_ZONE_ALARM              RT_BIT_32(11)
/** Unknown adversary detected while waiting on child. */
#define SUPHARDNT_ADVERSARY_UNKNOWN                 RT_BIT_32(31)
/** @} */


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static NTSTATUS supR3HardenedScreenImage(HANDLE hFile, bool fImage, PULONG pfAccess, PULONG pfProtect,
                                         bool *pfCallRealApi, const char *pszCaller, bool fAvoidWinVerifyTrust,
                                         bool *pfQuietFailure);

#ifdef RT_ARCH_AMD64
# define SYSCALL(a_Num) DECLASM(void) RT_CONCAT(supR3HardenedJmpBack_NtCreateSection_,a_Num)(void)
# include "NtCreateSection-template-amd64-syscall-type-1.h"
# undef SYSCALL
#endif
#ifdef RT_ARCH_X86
# define SYSCALL(a_Num) DECLASM(void) RT_CONCAT(supR3HardenedJmpBack_NtCreateSection_,a_Num)(void)
# include "NtCreateSection-template-x86-syscall-type-1.h"
# undef SYSCALL
#endif

DECLASM(void) supR3HardenedVmProcessInitThunk(void);



/**
 * Simple wide char search routine.
 *
 * @returns Pointer to the first location of @a wcNeedle in @a pwszHaystack.
 *          NULL if not found.
 * @param   pwszHaystack    Pointer to the string that should be searched.
 * @param   wcNeedle        The character to search for.
 */
static PRTUTF16 suplibHardenedWStrChr(PCRTUTF16 pwszHaystack, RTUTF16 wcNeedle)
{
    for (;;)
    {
        RTUTF16 wcCur = *pwszHaystack;
        if (wcCur == wcNeedle)
            return (PRTUTF16)pwszHaystack;
        if (wcCur == '\0')
            return NULL;
        pwszHaystack++;
    }
}


/**
 * Simple wide char string length routine.
 *
 * @returns The number of characters in the given string. (Excludes the
 *          terminator.)
 * @param   pwsz            The string.
 */
static size_t suplibHardenedWStrLen(PCRTUTF16 pwsz)
{
    PCRTUTF16 pwszCur = pwsz;
    while (*pwszCur != '\0')
        pwszCur++;
    return pwszCur - pwsz;
}


/**
 * Wrapper around LoadLibraryEx that deals with the UTF-8 to UTF-16 conversion
 * and supplies the right flags.
 *
 * @returns Module handle on success, NULL on failure.
 * @param   pszName             The full path to the DLL.
 * @param   fSystem32Only       Whether to only look for imports in the system32
 *                              directory.  If set to false, the application
 *                              directory is also searched.
 */
DECLHIDDEN(void *) supR3HardenedWinLoadLibrary(const char *pszName, bool fSystem32Only)
{
    WCHAR wszPath[RTPATH_MAX];
    PRTUTF16 pwszPath = wszPath;
    int rc = RTStrToUtf16Ex(pszName, RTSTR_MAX, &pwszPath, RT_ELEMENTS(wszPath), NULL);
    if (RT_SUCCESS(rc))
    {
        while (*pwszPath)
        {
            if (*pwszPath == '/')
                *pwszPath = '\\';
            pwszPath++;
        }

        DWORD fFlags = 0;
        if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0))
        {
           fFlags |= LOAD_LIBRARY_SEARCH_SYSTEM32;
           if (!fSystem32Only)
               fFlags |= LOAD_LIBRARY_SEARCH_APPLICATION_DIR;
        }

        void *pvRet = (void *)LoadLibraryExW(wszPath, NULL /*hFile*/, fFlags);

        /* Vista, W7, W2K8R might not work without KB2533623, so retry with no flags. */
        if (   !pvRet
            && fFlags
            && g_uNtVerCombined < SUP_MAKE_NT_VER_SIMPLE(6, 2)
            && RtlGetLastWin32Error() == ERROR_INVALID_PARAMETER)
            pvRet = (void *)LoadLibraryExW(wszPath, NULL /*hFile*/, 0);

        return pvRet;
    }
    supR3HardenedFatal("RTStrToUtf16Ex failed on '%s': %Rrc", pszName, rc);
    return NULL;
}


/**
 * Gets the internal index number of the file.
 *
 * @returns True if we got an index number, false if not.
 * @param   hFile           The file in question.
 * @param   pIndexNumber    where to return the index number.
 */
static bool supR3HardenedWinVerifyCacheGetIndexNumber(HANDLE hFile, PLARGE_INTEGER pIndexNumber)
{
    IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    NTSTATUS rcNt = NtQueryInformationFile(hFile, &Ios, pIndexNumber, sizeof(*pIndexNumber), FileInternalInformation);
    if (NT_SUCCESS(rcNt))
        rcNt = Ios.Status;
#ifdef DEBUG_bird
    if (!NT_SUCCESS(rcNt))
        __debugbreak();
#endif
    return NT_SUCCESS(rcNt) && pIndexNumber->QuadPart != 0;
}


/**
 * Calculates the hash value for the given UTF-16 path string.
 *
 * @returns Hash value.
 * @param   pUniStr             String to hash.
 */
static uint32_t supR3HardenedWinVerifyCacheHashPath(PCUNICODE_STRING pUniStr)
{
    uint32_t uHash   = 0;
    unsigned cwcLeft = pUniStr->Length / sizeof(WCHAR);
    PRTUTF16 pwc     = pUniStr->Buffer;

    while (cwcLeft-- > 0)
    {
        RTUTF16 wc = *pwc++;
        if (wc < 0x80)
            wc = wc != '/' ? RT_C_TO_LOWER(wc) : '\\';
        uHash = wc + (uHash << 6) + (uHash << 16) - uHash;
    }
    return uHash;
}


/**
 * Calculates the hash value for a directory + filename combo as if they were
 * one single string.
 *
 * @returns Hash value.
 * @param   pawcDir             The directory name.
 * @param   cwcDir              The length of the directory name. RTSTR_MAX if
 *                              not available.
 * @param   pszName             The import name (UTF-8).
 */
static uint32_t supR3HardenedWinVerifyCacheHashDirAndFile(PCRTUTF16 pawcDir, uint32_t cwcDir, const char *pszName)
{
    uint32_t uHash = 0;
    while (cwcDir-- > 0)
    {
        RTUTF16 wc = *pawcDir++;
        if (wc < 0x80)
            wc = wc != '/' ? RT_C_TO_LOWER(wc) : '\\';
        uHash = wc + (uHash << 6) + (uHash << 16) - uHash;
    }

    unsigned char ch = '\\';
    uHash = ch + (uHash << 6) + (uHash << 16) - uHash;

    while ((ch = *pszName++) != '\0')
    {
        ch = RT_C_TO_LOWER(ch);
        uHash = ch + (uHash << 6) + (uHash << 16) - uHash;
    }

    return uHash;
}


/**
 * Verify string cache compare function.
 *
 * @returns true if the strings match, false if not.
 * @param   pawcLeft            The left hand string.
 * @param   pawcRight           The right hand string.
 * @param   cwcToCompare        The number of chars to compare.
 */
static bool supR3HardenedWinVerifyCacheIsMatch(PCRTUTF16 pawcLeft, PCRTUTF16 pawcRight, uint32_t cwcToCompare)
{
    /* Try a quick memory compare first. */
    if (memcmp(pawcLeft, pawcRight, cwcToCompare * sizeof(RTUTF16)) == 0)
        return true;

    /* Slow char by char compare. */
    while (cwcToCompare-- > 0)
    {
        RTUTF16 wcLeft  = *pawcLeft++;
        RTUTF16 wcRight = *pawcRight++;
        if (wcLeft != wcRight)
        {
            wcLeft  = wcLeft  != '/' ? RT_C_TO_LOWER(wcLeft)  : '\\';
            wcRight = wcRight != '/' ? RT_C_TO_LOWER(wcRight) : '\\';
            if (wcLeft != wcRight)
                return false;
        }
    }

    return true;
}



/**
 * Inserts the given verifier result into the cache.
 *
 * @param   pUniStr             The full path of the image.
 * @param   hFile               The file handle - must either be entered into
 *                              the cache or closed.
 * @param   rc                  The verifier result.
 * @param   fWinVerifyTrust     Whether verified by WinVerifyTrust or not.
 * @param   fFlags              The image verification flags.
 */
static void supR3HardenedWinVerifyCacheInsert(PCUNICODE_STRING pUniStr, HANDLE hFile, int rc,
                                              bool fWinVerifyTrust, uint32_t fFlags)
{
    /*
     * Allocate and initalize a new entry.
     */
    PVERIFIERCACHEENTRY pEntry = (PVERIFIERCACHEENTRY)RTMemAllocZ(sizeof(VERIFIERCACHEENTRY) + pUniStr->Length);
    if (pEntry)
    {
        pEntry->pNext           = NULL;
        pEntry->pNextTodoWvt    = NULL;
        pEntry->hFile           = hFile;
        pEntry->uHash           = supR3HardenedWinVerifyCacheHashPath(pUniStr);
        pEntry->rc              = rc;
        pEntry->fFlags          = fFlags;
        pEntry->cErrorHits      = 0;
        pEntry->fWinVerifyTrust = fWinVerifyTrust;
        pEntry->cbPath          = pUniStr->Length;
        memcpy(pEntry->wszPath, pUniStr->Buffer, pUniStr->Length);
        pEntry->wszPath[pUniStr->Length / sizeof(WCHAR)] = '\0';
        pEntry->fIndexNumberValid = supR3HardenedWinVerifyCacheGetIndexNumber(hFile, &pEntry->IndexNumber);

        /*
         * Try insert it, careful with concurrent code as well as potential duplicates.
         */
        uint32_t iHashTab = pEntry->uHash % RT_ELEMENTS(g_apVerifierCache);
        VERIFIERCACHEENTRY * volatile *ppEntry = &g_apVerifierCache[iHashTab];
        for (;;)
        {
            if (ASMAtomicCmpXchgPtr(ppEntry, pEntry, NULL))
            {
                if (!fWinVerifyTrust)
                    do
                        pEntry->pNextTodoWvt = g_pVerifierCacheTodoWvt;
                    while (!ASMAtomicCmpXchgPtr(&g_pVerifierCacheTodoWvt, pEntry, pEntry->pNextTodoWvt));

                SUP_DPRINTF(("supR3HardenedWinVerifyCacheInsert: %ls\n", pUniStr->Buffer));
                return;
            }

            PVERIFIERCACHEENTRY pOther = *ppEntry;
            if (!pOther)
                continue;
            if (   pOther->uHash  == pEntry->uHash
                && pOther->cbPath == pEntry->cbPath
                && supR3HardenedWinVerifyCacheIsMatch(pOther->wszPath, pEntry->wszPath, pEntry->cbPath / sizeof(RTUTF16)))
                break;
            ppEntry = &pOther->pNext;
        }

        /* Duplicate entry (may happen due to races). */
        RTMemFree(pEntry);
    }
    NtClose(hFile);
}


/**
 * Looks up an entry in the verifier hash table.
 *
 * @return  Pointer to the entry on if found, NULL if not.
 * @param   pUniStr             The full path of the image.
 * @param   hFile               The file handle.
 */
static PVERIFIERCACHEENTRY supR3HardenedWinVerifyCacheLookup(PCUNICODE_STRING pUniStr, HANDLE hFile)
{
    PRTUTF16 const      pwszPath = pUniStr->Buffer;
    uint16_t const      cbPath   = pUniStr->Length;
    uint32_t            uHash    = supR3HardenedWinVerifyCacheHashPath(pUniStr);
    uint32_t            iHashTab = uHash % RT_ELEMENTS(g_apVerifierCache);
    PVERIFIERCACHEENTRY pCur     = g_apVerifierCache[iHashTab];
    while (pCur)
    {
        if (   pCur->uHash  == uHash
            && pCur->cbPath == cbPath
            && supR3HardenedWinVerifyCacheIsMatch(pCur->wszPath, pwszPath, cbPath / sizeof(RTUTF16)))
        {

            if (!pCur->fIndexNumberValid)
                return pCur;
            LARGE_INTEGER IndexNumber;
            bool fIndexNumberValid = supR3HardenedWinVerifyCacheGetIndexNumber(hFile, &IndexNumber);
            if (   fIndexNumberValid
                && IndexNumber.QuadPart == pCur->IndexNumber.QuadPart)
                return pCur;
#ifdef DEBUG_bird
            __debugbreak();
#endif
        }
        pCur = pCur->pNext;
    }
    return NULL;
}


/**
 * Looks up an import DLL in the verifier hash table.
 *
 * @return  Pointer to the entry on if found, NULL if not.
 * @param   pawcDir             The directory name.
 * @param   cwcDir              The length of the directory name.
 * @param   pszName             The import name (UTF-8).
 */
static PVERIFIERCACHEENTRY supR3HardenedWinVerifyCacheLookupImport(PCRTUTF16 pawcDir, uint32_t cwcDir, const char *pszName)
{
    uint32_t            uHash    = supR3HardenedWinVerifyCacheHashDirAndFile(pawcDir, cwcDir, pszName);
    uint32_t            iHashTab = uHash % RT_ELEMENTS(g_apVerifierCache);
    uint32_t const      cbPath   = (uint32_t)((cwcDir + 1 + strlen(pszName)) * sizeof(RTUTF16));
    PVERIFIERCACHEENTRY pCur     = g_apVerifierCache[iHashTab];
    while (pCur)
    {
        if (   pCur->uHash  == uHash
            && pCur->cbPath == cbPath)
        {
            if (supR3HardenedWinVerifyCacheIsMatch(pCur->wszPath, pawcDir, cwcDir))
            {
                if (pCur->wszPath[cwcDir] == '\\' || pCur->wszPath[cwcDir] == '/')
                {
                    if (RTUtf16ICmpAscii(&pCur->wszPath[cwcDir + 1], pszName))
                    {
                        return pCur;
                    }
                }
            }
        }

        pCur = pCur->pNext;
    }
    return NULL;
}


/**
 * Schedules the import DLLs for verification and entry into the cache.
 *
 * @param   hLdrMod             The loader module which imports should be
 *                              scheduled for verification.
 * @param   pwszName            The full NT path of the module.
 */
DECLHIDDEN(void) supR3HardenedWinVerifyCacheScheduleImports(RTLDRMOD hLdrMod, PCRTUTF16 pwszName)
{
    /*
     * Any imports?
     */
    uint32_t cImports;
    int rc = RTLdrQueryPropEx(hLdrMod, RTLDRPROP_IMPORT_COUNT, NULL /*pvBits*/, &cImports, sizeof(cImports), NULL);
    if (RT_SUCCESS(rc))
    {
        if (cImports)
        {
            /*
             * Figure out the DLL directory from pwszName.
             */
            PCRTUTF16 pawcDir = pwszName;
            uint32_t  cwcDir = 0;
            uint32_t  i = 0;
            RTUTF16   wc;
            while ((wc = pawcDir[i++]) != '\0')
                if ((wc == '\\' || wc == '/' || wc == ':') && cwcDir + 2 != i)
                    cwcDir = i - 1;
            if (   g_System32NtPath.UniStr.Length / sizeof(WCHAR) == cwcDir
                && supR3HardenedWinVerifyCacheIsMatch(pawcDir, g_System32NtPath.UniStr.Buffer, cwcDir))
                pawcDir = NULL;

            /*
             * Enumerate the imports.
             */
            for (i = 0; i < cImports; i++)
            {
                union
                {
                    char        szName[256];
                    uint32_t    iImport;
                } uBuf;
                uBuf.iImport = i;
                rc = RTLdrQueryPropEx(hLdrMod, RTLDRPROP_IMPORT_MODULE, NULL /*pvBits*/, &uBuf, sizeof(uBuf), NULL);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Skip kernel32, ntdll and API set stuff.
                     */
                    RTStrToLower(uBuf.szName);
                    if (   RTStrCmp(uBuf.szName, "kernel32.dll") == 0
                        || RTStrCmp(uBuf.szName, "kernelbase.dll") == 0
                        || RTStrCmp(uBuf.szName, "ntdll.dll") == 0
                        || RTStrNCmp(uBuf.szName, RT_STR_TUPLE("api-ms-win-")) == 0 )
                    {
                        continue;
                    }

                    /*
                     * Skip to the next one if it's already in the cache.
                     */
                    if (supR3HardenedWinVerifyCacheLookupImport(g_System32NtPath.UniStr.Buffer,
                                                                g_System32NtPath.UniStr.Length / sizeof(WCHAR),
                                                                uBuf.szName) != NULL)
                    {
                        SUP_DPRINTF(("supR3HardenedWinVerifyCacheScheduleImports: '%s' cached for system32\n", uBuf.szName));
                        continue;
                    }
                    if (supR3HardenedWinVerifyCacheLookupImport(g_SupLibHardenedExeNtPath.UniStr.Buffer,
                                                                g_offSupLibHardenedExeNtName,
                                                                uBuf.szName) != NULL)
                    {
                        SUP_DPRINTF(("supR3HardenedWinVerifyCacheScheduleImports: '%s' cached for appdir\n", uBuf.szName));
                        continue;
                    }
                    if (pawcDir && supR3HardenedWinVerifyCacheLookupImport(pawcDir, cwcDir, uBuf.szName) != NULL)
                    {
                        SUP_DPRINTF(("supR3HardenedWinVerifyCacheScheduleImports: '%s' cached for dll dir\n", uBuf.szName));
                        continue;
                    }

                    /* We could skip already scheduled modules, but that'll require serialization and extra work... */

                    /*
                     * Add it to the todo list.
                     */
                    SUP_DPRINTF(("supR3HardenedWinVerifyCacheScheduleImports: Import todo: #%u '%s'.\n", i, uBuf.szName));
                    uint32_t cbName        = (uint32_t)strlen(uBuf.szName) + 1;
                    uint32_t cbNameAligned = RT_ALIGN_32(cbName, sizeof(RTUTF16));
                    uint32_t cbNeeded      = RT_OFFSETOF(VERIFIERCACHEIMPORT, szName[cbNameAligned])
                                           + (pawcDir ? (cwcDir + 1) * sizeof(RTUTF16) : 0);
                    PVERIFIERCACHEIMPORT pImport = (PVERIFIERCACHEIMPORT)RTMemAllocZ(cbNeeded);
                    if (pImport)
                    {
                        /* Init it. */
                        memcpy(pImport->szName, uBuf.szName, cbName);
                        if (!pawcDir)
                        {
                            pImport->cwcAltSearchDir  = 0;
                            pImport->pwszAltSearchDir = NULL;
                        }
                        else
                        {
                            pImport->cwcAltSearchDir = cwcDir;
                            pImport->pwszAltSearchDir = (PRTUTF16)&pImport->szName[cbNameAligned];
                            memcpy(pImport->pwszAltSearchDir, pawcDir, cwcDir * sizeof(RTUTF16));
                            pImport->pwszAltSearchDir[cwcDir] = '\0';
                        }

                        /* Insert it. */
                        do
                            pImport->pNext = g_pVerifierCacheTodoImports;
                        while (!ASMAtomicCmpXchgPtr(&g_pVerifierCacheTodoImports, pImport, pImport->pNext));
                    }
                }
                else
                    SUP_DPRINTF(("RTLDRPROP_IMPORT_MODULE failed with rc=%Rrc i=%#x on '%ls'\n", rc, i, pwszName));
            }
        }
        else
            SUP_DPRINTF(("'%ls' has no imports\n", pwszName));
    }
    else
        SUP_DPRINTF(("RTLDRPROP_IMPORT_COUNT failed with rc=%Rrc on '%ls'\n", rc, pwszName));
}


/**
 * Processes the list of import todos.
 */
static void supR3HardenedWinVerifyCacheProcessImportTodos(void)
{
    /*
     * Work until we've got nothing more todo.
     */
    for (;;)
    {
        PVERIFIERCACHEIMPORT pTodo = ASMAtomicXchgPtrT(&g_pVerifierCacheTodoImports, NULL, PVERIFIERCACHEIMPORT);
        if (!pTodo)
            break;
        do
        {
            PVERIFIERCACHEIMPORT pCur = pTodo;
            pTodo = pTodo->pNext;

            /*
             * Not in the cached already?
             */
            if (   !supR3HardenedWinVerifyCacheLookupImport(g_System32NtPath.UniStr.Buffer,
                                                            g_System32NtPath.UniStr.Length / sizeof(WCHAR),
                                                            pCur->szName)
                && !supR3HardenedWinVerifyCacheLookupImport(g_SupLibHardenedExeNtPath.UniStr.Buffer,
                                                            g_offSupLibHardenedExeNtName,
                                                            pCur->szName)
                && (   pCur->cwcAltSearchDir == 0
                    || !supR3HardenedWinVerifyCacheLookupImport(pCur->pwszAltSearchDir, pCur->cwcAltSearchDir, pCur->szName)) )
            {
                /*
                 * Try locate the imported DLL and open it.
                 */
                SUP_DPRINTF(("supR3HardenedWinVerifyCacheProcessImportTodos: Processing '%s'...\n", pCur->szName));

                NTSTATUS    rcNt;
                NTSTATUS    rcNtRedir = 0x22222222;
                HANDLE      hFile = INVALID_HANDLE_VALUE;
                RTUTF16     wszPath[260 + 260]; /* Assumes we've limited the import name length to 256. */
                AssertCompile(sizeof(wszPath) > sizeof(g_System32NtPath));

                /*
                 * Check for DLL isolation / redirection / mapping.
                 */
                size_t      cwcName  = 260;
                PRTUTF16    pwszName = &wszPath[0];
                int rc = RTStrToUtf16Ex(pCur->szName, RTSTR_MAX, &pwszName, cwcName, &cwcName);
                if (RT_SUCCESS(rc))
                {
                    UNICODE_STRING UniStrName;
                    UniStrName.Buffer = wszPath;
                    UniStrName.Length = (USHORT)cwcName * sizeof(WCHAR);
                    UniStrName.MaximumLength = UniStrName.Length + sizeof(WCHAR);

                    UNICODE_STRING UniStrStatic;
                    UniStrStatic.Buffer = &wszPath[cwcName + 1];
                    UniStrStatic.Length = 0;
                    UniStrStatic.MaximumLength = (USHORT)(sizeof(wszPath) - cwcName * sizeof(WCHAR) - sizeof(WCHAR));

                    static UNICODE_STRING const s_DefaultSuffix = RTNT_CONSTANT_UNISTR(L".dll");
                    UNICODE_STRING  UniStrDynamic = { 0, 0, NULL };
                    PUNICODE_STRING pUniStrResult = NULL;

                    rcNtRedir = RtlDosApplyFileIsolationRedirection_Ustr(1 /*fFlags*/,
                                                                         &UniStrName,
                                                                         (PUNICODE_STRING)&s_DefaultSuffix,
                                                                         &UniStrStatic,
                                                                         &UniStrDynamic,
                                                                         &pUniStrResult,
                                                                         NULL /*pNewFlags*/,
                                                                         NULL /*pcbFilename*/,
                                                                         NULL /*pcbNeeded*/);
                    if (NT_SUCCESS(rcNtRedir))
                    {
                        IO_STATUS_BLOCK     Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
                        OBJECT_ATTRIBUTES   ObjAttr;
                        InitializeObjectAttributes(&ObjAttr, pUniStrResult,
                                                   OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);
                        rcNt = NtCreateFile(&hFile,
                                            FILE_READ_DATA | READ_CONTROL | SYNCHRONIZE,
                                            &ObjAttr,
                                            &Ios,
                                            NULL /* Allocation Size*/,
                                            FILE_ATTRIBUTE_NORMAL,
                                            FILE_SHARE_READ,
                                            FILE_OPEN,
                                            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                            NULL /*EaBuffer*/,
                                            0 /*EaLength*/);
                        if (NT_SUCCESS(rcNt))
                            rcNt = Ios.Status;
                        if (NT_SUCCESS(rcNt))
                        {
                            /* For accurate logging. */
                            size_t cwcCopy = RT_MIN(pUniStrResult->Length / sizeof(RTUTF16), RT_ELEMENTS(wszPath) - 1);
                            memcpy(wszPath, pUniStrResult->Buffer, cwcCopy * sizeof(RTUTF16));
                            wszPath[cwcCopy] = '\0';
                        }
                        else
                            hFile = INVALID_HANDLE_VALUE;
                        RtlFreeUnicodeString(&UniStrDynamic);
                    }
                }
                else
                    SUP_DPRINTF(("supR3HardenedWinVerifyCacheProcessImportTodos: RTStrToUtf16Ex #1 failed: %Rrc\n", rc));

                /*
                 * If not something that gets remapped, do the half normal searching we need.
                 */
                if (hFile == INVALID_HANDLE_VALUE)
                {
                    struct
                    {
                        PRTUTF16 pawcDir;
                        uint32_t cwcDir;
                    } Tmp, aDirs[] =
                    {
                        { g_System32NtPath.UniStr.Buffer,           g_System32NtPath.UniStr.Length / sizeof(WCHAR) },
                        { g_SupLibHardenedExeNtPath.UniStr.Buffer,  g_offSupLibHardenedExeNtName - 1 },
                        { pCur->pwszAltSearchDir,                   pCur->cwcAltSearchDir },
                    };

                    /* Search System32 first, unless it's a 'V*' or 'm*' name, the latter for msvcrt.  */
                    if (   pCur->szName[0] == 'v'
                        || pCur->szName[0] == 'V'
                        || pCur->szName[0] == 'm'
                        || pCur->szName[0] == 'M')
                    {
                        Tmp      = aDirs[0];
                        aDirs[0] = aDirs[1];
                        aDirs[1] = Tmp;
                    }

                    for (uint32_t i = 0; i < RT_ELEMENTS(aDirs); i++)
                    {
                        if (aDirs[i].pawcDir && aDirs[i].cwcDir && aDirs[i].cwcDir < RT_ELEMENTS(wszPath) / 3 * 2)
                        {
                            memcpy(wszPath, aDirs[i].pawcDir, aDirs[i].cwcDir * sizeof(RTUTF16));
                            uint32_t cwc = aDirs[i].cwcDir;
                            wszPath[cwc++] = '\\';
                            cwcName  = RT_ELEMENTS(wszPath) - cwc;
                            pwszName = &wszPath[cwc];
                            rc = RTStrToUtf16Ex(pCur->szName, RTSTR_MAX, &pwszName, cwcName, &cwcName);
                            if (RT_SUCCESS(rc))
                            {
                                IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;
                                UNICODE_STRING      NtName;
                                NtName.Buffer        = wszPath;
                                NtName.Length        = (USHORT)((cwc + cwcName) * sizeof(WCHAR));
                                NtName.MaximumLength = NtName.Length + sizeof(WCHAR);
                                OBJECT_ATTRIBUTES   ObjAttr;
                                InitializeObjectAttributes(&ObjAttr, &NtName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

                                rcNt = NtCreateFile(&hFile,
                                                    FILE_READ_DATA | READ_CONTROL | SYNCHRONIZE,
                                                    &ObjAttr,
                                                    &Ios,
                                                    NULL /* Allocation Size*/,
                                                    FILE_ATTRIBUTE_NORMAL,
                                                    FILE_SHARE_READ,
                                                    FILE_OPEN,
                                                    FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                                    NULL /*EaBuffer*/,
                                                    0 /*EaLength*/);
                                if (NT_SUCCESS(rcNt))
                                    rcNt = Ios.Status;
                                if (NT_SUCCESS(rcNt))
                                    break;
                                hFile = INVALID_HANDLE_VALUE;
                            }
                            else
                                SUP_DPRINTF(("supR3HardenedWinVerifyCacheProcessImportTodos: RTStrToUtf16Ex #2 failed: %Rrc\n", rc));
                        }
                    }
                }

                /*
                 * If we successfully opened it, verify it and cache the result.
                 */
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    SUP_DPRINTF(("supR3HardenedWinVerifyCacheProcessImportTodos: '%s' -> '%ls' [rcNtRedir=%#x]\n",
                                 pCur->szName, wszPath, rcNtRedir));

                    ULONG fAccess = 0;
                    ULONG fProtect = 0;
                    bool  fCallRealApi = false;
                    rcNt = supR3HardenedScreenImage(hFile, true /*fImage*/, &fAccess, &fProtect, &fCallRealApi,
                                                    "Imports", false /*fAvoidWinVerifyTrust*/, NULL /*pfQuietFailure*/);
                    NtClose(hFile);
                }
                else
                    SUP_DPRINTF(("supR3HardenedWinVerifyCacheProcessImportTodos: Failed to locate '%s'\n", pCur->szName));
            }
            else
                SUP_DPRINTF(("supR3HardenedWinVerifyCacheProcessImportTodos: '%s' is in the cache.\n", pCur->szName));

            RTMemFree(pCur);
        } while (pTodo);
    }
}


/**
 * Processes the list of WinVerifyTrust todos.
 */
static void supR3HardenedWinVerifyCacheProcessWvtTodos(void)
{
    PVERIFIERCACHEENTRY  pReschedule = NULL;
    PVERIFIERCACHEENTRY volatile *ppReschedLastNext = NULL;

    /*
     * Work until we've got nothing more todo.
     */
    for (;;)
    {
        if (!supHardenedWinIsWinVerifyTrustCallable())
            break;
        PVERIFIERCACHEENTRY pTodo = ASMAtomicXchgPtrT(&g_pVerifierCacheTodoWvt, NULL, PVERIFIERCACHEENTRY);
        if (!pTodo)
            break;
        do
        {
            PVERIFIERCACHEENTRY pCur = pTodo;
            pTodo = pTodo->pNextTodoWvt;
            pCur->pNextTodoWvt = NULL;

            if (   !pCur->fWinVerifyTrust
                && RT_SUCCESS(pCur->rc))
            {
                bool fWinVerifyTrust = false;
                int rc = supHardenedWinVerifyImageTrust(pCur->hFile, pCur->wszPath, pCur->fFlags, pCur->rc,
                                                        &fWinVerifyTrust, NULL /* pErrInfo*/);
                if (RT_FAILURE(rc) || fWinVerifyTrust)
                {
                    SUP_DPRINTF(("supR3HardenedWinVerifyCacheProcessWvtTodos: %d (was %d) fWinVerifyTrust=%d for '%ls'\n",
                                 rc, pCur->rc, fWinVerifyTrust, pCur->wszPath));
                    pCur->fWinVerifyTrust = true;
                    pCur->rc = rc;
                }
                else
                {
                    /* Retry it at a later time. */
                    SUP_DPRINTF(("supR3HardenedWinVerifyCacheProcessWvtTodos: %d (was %d) fWinVerifyTrust=%d for '%ls' [rescheduled]\n",
                                 rc, pCur->rc, fWinVerifyTrust, pCur->wszPath));
                    if (!pReschedule)
                        ppReschedLastNext = &pCur->pNextTodoWvt;
                    pCur->pNextTodoWvt = pReschedule;
                }
            }
            /* else: already processed. */
        } while (pTodo);
    }

    /*
     * Anything to reschedule.
     */
    if (pReschedule)
    {
        do
            *ppReschedLastNext = g_pVerifierCacheTodoWvt;
        while (!ASMAtomicCmpXchgPtr(&g_pVerifierCacheTodoWvt, pReschedule, *ppReschedLastNext));
    }
}


/**
 * Checks whether the path could be containing alternative 8.3 names generated
 * by NTFS, FAT, or other similar file systems.
 *
 * @returns Pointer to the first component that might be an 8.3 name, NULL if
 *          not 8.3 path.
 * @param   pwszPath        The path to check.
 */
static PRTUTF16 supR3HardenedWinIsPossible8dot3Path(PCRTUTF16 pwszPath)
{
    PCRTUTF16 pwszName = pwszPath;
    for (;;)
    {
        RTUTF16 wc = *pwszPath++;
        if (wc == '~')
        {
            /* Could check more here before jumping to conclusions... */
            if (pwszPath - pwszName <= 8+1+3)
                return (PRTUTF16)pwszName;
        }
        else if (wc == '\\' || wc == '/' || wc == ':')
            pwszName = pwszPath;
        else if (wc == 0)
            break;
    }
    return NULL;
}


/**
 * Fixes up a path possibly containing one or more alternative 8-dot-3 style
 * components.
 *
 * The path is fixed up in place.  Errors are ignored.
 *
 * @param   hFile       The handle to the file which path we're fixing up.
 * @param   pUniStr     The path to fix up. MaximumLength is the max buffer
 *                      length.
 */
static void supR3HardenedWinFix8dot3Path(HANDLE hFile, PUNICODE_STRING pUniStr)
{
    /*
     * We could use FileNormalizedNameInformation here and slap the volume device
     * path in front of the result, but it's only supported since windows 8.0
     * according to some docs... So we expand all supicious names.
     */
    PRTUTF16 pwszFix = pUniStr->Buffer;
    while (*pwszFix)
    {
        pwszFix = supR3HardenedWinIsPossible8dot3Path(pwszFix);
        if (pwszFix == NULL)
            break;

        RTUTF16 wc;
        PRTUTF16 pwszFixEnd = pwszFix;
        while ((wc = *pwszFixEnd) != '\0' && wc != '\\' && wc != '/')
            pwszFixEnd++;
        if (wc == '\0')
            break;

        RTUTF16 const wcSaved = *pwszFix;
        *pwszFix = '\0';                     /* paranoia. */

        UNICODE_STRING      NtDir;
        NtDir.Buffer = pUniStr->Buffer;
        NtDir.Length = NtDir.MaximumLength = (USHORT)((pwszFix - pUniStr->Buffer) * sizeof(WCHAR));

        HANDLE              hDir  = RTNT_INVALID_HANDLE_VALUE;
        IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;

        OBJECT_ATTRIBUTES   ObjAttr;
        InitializeObjectAttributes(&ObjAttr, &NtDir, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

        NTSTATUS rcNt = NtCreateFile(&hDir,
                                     FILE_READ_DATA | SYNCHRONIZE,
                                     &ObjAttr,
                                     &Ios,
                                     NULL /* Allocation Size*/,
                                     FILE_ATTRIBUTE_NORMAL,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     FILE_OPEN,
                                     FILE_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT,
                                     NULL /*EaBuffer*/,
                                     0 /*EaLength*/);
        *pwszFix = wcSaved;
        if (NT_SUCCESS(rcNt))
        {
            union
            {
                FILE_BOTH_DIR_INFORMATION Info;
                uint8_t abBuffer[sizeof(FILE_BOTH_DIR_INFORMATION) + 2048 * sizeof(WCHAR)];
            } uBuf;
            RT_ZERO(uBuf);

            IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
            UNICODE_STRING  NtFilterStr;
            NtFilterStr.Buffer = pwszFix;
            NtFilterStr.Length = (USHORT)((uintptr_t)pwszFixEnd - (uintptr_t)pwszFix);
            NtFilterStr.MaximumLength = NtFilterStr.Length;
            rcNt = NtQueryDirectoryFile(hDir,
                                        NULL /* Event */,
                                        NULL /* ApcRoutine */,
                                        NULL /* ApcContext */,
                                        &Ios,
                                        &uBuf,
                                        sizeof(uBuf) - sizeof(WCHAR),
                                        FileBothDirectoryInformation,
                                        FALSE /*ReturnSingleEntry*/,
                                        &NtFilterStr,
                                        FALSE /*RestartScan */);
            if (NT_SUCCESS(rcNt) && uBuf.Info.NextEntryOffset == 0) /* There shall only be one entry matching... */
            {
                uint32_t offName = uBuf.Info.FileNameLength / sizeof(WCHAR);
                while (offName > 0  && uBuf.Info.FileName[offName - 1] != '\\' && uBuf.Info.FileName[offName - 1] != '/')
                    offName--;
                uint32_t cwcNameNew = (uBuf.Info.FileNameLength / sizeof(WCHAR)) - offName;
                uint32_t cwcNameOld = pwszFixEnd - pwszFix;

                if (cwcNameOld == cwcNameNew)
                    memcpy(pwszFix, &uBuf.Info.FileName[offName], cwcNameNew * sizeof(WCHAR));
                else if (   pUniStr->Length + cwcNameNew * sizeof(WCHAR) - cwcNameOld * sizeof(WCHAR) + sizeof(WCHAR)
                         <= pUniStr->MaximumLength)
                {
                    size_t cwcLeft = pUniStr->Length - (pwszFixEnd - pUniStr->Buffer) * sizeof(WCHAR) + sizeof(WCHAR);
                    memmove(&pwszFix[cwcNameNew], pwszFixEnd, cwcLeft * sizeof(WCHAR));
                    pUniStr->Length -= (USHORT)(cwcNameOld * sizeof(WCHAR));
                    pUniStr->Length += (USHORT)(cwcNameNew * sizeof(WCHAR));
                    pwszFixEnd      -= cwcNameOld;
                    pwszFixEnd      -= cwcNameNew;
                    memcpy(pwszFix, &uBuf.Info.FileName[offName], cwcNameNew * sizeof(WCHAR));
                }
                /* else: ignore overflow. */
            }
            /* else: ignore failure. */

            NtClose(hDir);
        }

        /* Advance */
        pwszFix = pwszFixEnd;
    }
}


static NTSTATUS supR3HardenedScreenImage(HANDLE hFile, bool fImage, PULONG pfAccess, PULONG pfProtect,
                                         bool *pfCallRealApi, const char *pszCaller, bool fAvoidWinVerifyTrust,
                                         bool *pfQuietFailure)
{
    *pfCallRealApi = false;
    if (pfQuietFailure)
        *pfQuietFailure = false;

    /*
     * Query the name of the file, making sure to zero terminator the
     * string. (2nd half of buffer is used for error info, see below.)
     */
    union
    {
        UNICODE_STRING UniStr;
        uint8_t abBuffer[sizeof(UNICODE_STRING) + 2048 * sizeof(WCHAR)];
    } uBuf;
    RT_ZERO(uBuf);
    ULONG cbNameBuf;
    NTSTATUS rcNt = NtQueryObject(hFile, ObjectNameInformation, &uBuf, sizeof(uBuf) - sizeof(WCHAR) - 128, &cbNameBuf);
    if (!NT_SUCCESS(rcNt))
    {
        supR3HardenedError(VINF_SUCCESS, false,
                           "supR3HardenedScreenImage/%s: NtQueryObject -> %#x (fImage=%d fProtect=%#x fAccess=%#x)\n",
                           pszCaller, fImage, *pfProtect, *pfAccess);
        return rcNt;
    }

    if (supR3HardenedWinIsPossible8dot3Path(uBuf.UniStr.Buffer))
    {
        uBuf.UniStr.MaximumLength = sizeof(uBuf) - 128;
        supR3HardenedWinFix8dot3Path(hFile, &uBuf.UniStr);
    }

    /*
     * Check the cache.
     */
    PVERIFIERCACHEENTRY pCacheHit = supR3HardenedWinVerifyCacheLookup(&uBuf.UniStr, hFile);
    if (pCacheHit)
    {
        /* If we haven't done the WinVerifyTrust thing, do it if we can. */
        if (   !pCacheHit->fWinVerifyTrust
            && RT_SUCCESS(pCacheHit->rc)
            && supHardenedWinIsWinVerifyTrustCallable() )
        {
            if (!fAvoidWinVerifyTrust)
            {
                SUP_DPRINTF(("supR3HardenedScreenImage/%s: cache hit (%Rrc) on %ls [redoing WinVerifyTrust]\n",
                             pszCaller, pCacheHit->rc, pCacheHit->wszPath));

                bool fWinVerifyTrust = false;
                int rc = supHardenedWinVerifyImageTrust(pCacheHit->hFile, pCacheHit->wszPath, pCacheHit->fFlags, pCacheHit->rc,
                                                        &fWinVerifyTrust, NULL /* pErrInfo*/);
                if (RT_FAILURE(rc) || fWinVerifyTrust)
                {
                    SUP_DPRINTF(("supR3HardenedScreenImage/%s: %d (was %d) fWinVerifyTrust=%d for '%ls'\n",
                                 pszCaller, rc, pCacheHit->rc, fWinVerifyTrust, pCacheHit->wszPath));
                    pCacheHit->fWinVerifyTrust = true;
                    pCacheHit->rc = rc;
                }
                else
                    SUP_DPRINTF(("supR3HardenedScreenImage/%s: WinVerifyTrust not available, rescheduling %ls\n",
                                 pszCaller, pCacheHit->wszPath));
            }
            else
                SUP_DPRINTF(("supR3HardenedScreenImage/%s: cache hit (%Rrc) on %ls [avoiding WinVerifyTrust]\n",
                             pszCaller, pCacheHit->rc, pCacheHit->wszPath));
        }
        else if (pCacheHit->cErrorHits < 16)
            SUP_DPRINTF(("supR3HardenedScreenImage/%s: cache hit (%Rrc) on %ls%s\n",
                         pszCaller, pCacheHit->rc, pCacheHit->wszPath, pCacheHit->fWinVerifyTrust ? "" : " [lacks WinVerifyTrust]"));

        /* Return the cached value. */
        if (RT_SUCCESS(pCacheHit->rc))
        {
            *pfCallRealApi = true;
            return STATUS_SUCCESS;
        }

        uint32_t cErrorHits = ASMAtomicIncU32(&pCacheHit->cErrorHits);
        if (   cErrorHits < 8
            || RT_IS_POWER_OF_TWO(cErrorHits))
            supR3HardenedError(VINF_SUCCESS, false,
                               "supR3HardenedScreenImage/%s: cached rc=%Rrc fImage=%d fProtect=%#x fAccess=%#x cErrorHits=%u %ls\n",
                               pszCaller, pCacheHit->rc, fImage, *pfProtect, *pfAccess, cErrorHits, uBuf.UniStr.Buffer);
        else if (pfQuietFailure)
            *pfQuietFailure = true;

        return STATUS_TRUST_FAILURE;
    }

    /*
     * On XP the loader might hand us handles with just FILE_EXECUTE and
     * SYNCHRONIZE, the means reading will fail later on.  Also, we need
     * READ_CONTROL access to check the file ownership later on, and non
     * of the OS versions seems be giving us that.  So, in effect we
     * more or less always reopen the file here.
     */
    HANDLE hMyFile = NULL;
    rcNt = NtDuplicateObject(NtCurrentProcess(), hFile, NtCurrentProcess(),
                             &hMyFile,
                             FILE_READ_DATA | READ_CONTROL | SYNCHRONIZE,
                             0 /* Handle attributes*/, 0 /* Options */);
    if (!NT_SUCCESS(rcNt))
    {
        if (rcNt == STATUS_ACCESS_DENIED)
        {
            IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;
            OBJECT_ATTRIBUTES   ObjAttr;
            InitializeObjectAttributes(&ObjAttr, &uBuf.UniStr, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

            rcNt = NtCreateFile(&hMyFile,
                                FILE_READ_DATA | READ_CONTROL | SYNCHRONIZE,
                                &ObjAttr,
                                &Ios,
                                NULL /* Allocation Size*/,
                                FILE_ATTRIBUTE_NORMAL,
                                FILE_SHARE_READ,
                                FILE_OPEN,
                                FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                NULL /*EaBuffer*/,
                                0 /*EaLength*/);
            if (NT_SUCCESS(rcNt))
                rcNt = Ios.Status;
            if (!NT_SUCCESS(rcNt))
            {
                supR3HardenedError(VINF_SUCCESS, false,
                                   "supR3HardenedScreenImage/%s: Failed to duplicate and open the file: rcNt=%#x hFile=%p %ls\n",
                                   pszCaller, rcNt, hFile, uBuf.UniStr.Buffer);
                return rcNt;
            }

            /* Check that we've got the same file. */
            LARGE_INTEGER idMyFile, idInFile;
            bool fMyValid = supR3HardenedWinVerifyCacheGetIndexNumber(hMyFile, &idMyFile);
            bool fInValid = supR3HardenedWinVerifyCacheGetIndexNumber(hFile, &idInFile);
            if (   fMyValid
                && (   fMyValid != fInValid
                    || idMyFile.QuadPart != idInFile.QuadPart))
            {
                supR3HardenedError(VINF_SUCCESS, false,
                                   "supR3HardenedScreenImage/%s: Re-opened has different ID that input: %#llx vx %#llx (%ls)\n",
                                   pszCaller, rcNt, idMyFile.QuadPart, idInFile.QuadPart, uBuf.UniStr.Buffer);
                NtClose(hMyFile);
                return STATUS_TRUST_FAILURE;
            }
        }
        else
        {
            SUP_DPRINTF(("supR3HardenedScreenImage/%s: NtDuplicateObject -> %#x\n", pszCaller, rcNt));
#ifdef DEBUG

            supR3HardenedError(VINF_SUCCESS, false,
                               "supR3HardenedScreenImage/%s: NtDuplicateObject(,%#x,) failed: %#x\n", pszCaller, hFile, rcNt);
#endif
            hMyFile = hFile;
        }
    }

    /*
     * Special Kludge for Windows XP and W2K3 and their stupid attempts
     * at mapping a hidden XML file called c:\Windows\WindowsShell.Manifest
     * with executable access.  The image bit isn't set, fortunately.
     */
    if (   !fImage
        && uBuf.UniStr.Length > g_System32NtPath.UniStr.Length - sizeof(L"System32") + sizeof(WCHAR)
        && memcmp(uBuf.UniStr.Buffer, g_System32NtPath.UniStr.Buffer,
                  g_System32NtPath.UniStr.Length - sizeof(L"System32") + sizeof(WCHAR)) == 0)
    {
        PRTUTF16 pwszName = &uBuf.UniStr.Buffer[(g_System32NtPath.UniStr.Length - sizeof(L"System32") + sizeof(WCHAR)) / sizeof(WCHAR)];
        if (RTUtf16ICmpAscii(pwszName, "WindowsShell.Manifest") == 0)
        {
            /*
             * Drop all executable access to the mapping and let it continue.
             */
            SUP_DPRINTF(("supR3HardenedScreenImage/%s: Applying the drop-exec-kludge for '%ls'\n", pszCaller, uBuf.UniStr.Buffer));
            if (*pfAccess & SECTION_MAP_EXECUTE)
                *pfAccess = (*pfAccess & ~SECTION_MAP_EXECUTE) | SECTION_MAP_READ;
            if (*pfProtect & PAGE_EXECUTE)
                *pfProtect = (*pfProtect & ~PAGE_EXECUTE) | PAGE_READONLY;
            *pfProtect = (*pfProtect & ~UINT32_C(0xf0)) | ((*pfProtect & UINT32_C(0xe0)) >> 4);
            if (hMyFile != hFile)
                NtClose(hMyFile);
            *pfCallRealApi = true;
            return STATUS_SUCCESS;
        }
    }

#ifndef VBOX_PERMIT_EVEN_MORE
    /*
     * Check the path.  We don't allow DLLs to be loaded from just anywhere:
     *      1. System32      - normal code or cat signing, owner TrustedInstaller.
     *      2. WinSxS        - normal code or cat signing, owner TrustedInstaller.
     *      3. VirtualBox    - kernel code signing and integrity checks.
     *      4. AppPatchDir   - normal code or cat signing, owner TrustedInstaller.
     *      5. Program Files - normal code or cat signing, owner TrustedInstaller.
     *      6. Common Files  - normal code or cat signing, owner TrustedInstaller.
     *      7. x86 variations of 4 & 5 - ditto.
     */
    Assert(g_SupLibHardenedExeNtPath.UniStr.Buffer[g_offSupLibHardenedExeNtName - 1] == '\\');
    uint32_t fFlags = 0;
    if (supHardViUniStrPathStartsWithUniStr(&uBuf.UniStr, &g_System32NtPath.UniStr, true /*fCheckSlash*/))
        fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION | SUPHNTVI_F_TRUSTED_INSTALLER_OWNER;
    else if (supHardViUniStrPathStartsWithUniStr(&uBuf.UniStr, &g_WinSxSNtPath.UniStr, true /*fCheckSlash*/))
        fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION | SUPHNTVI_F_TRUSTED_INSTALLER_OWNER;
    else if (supHardViUtf16PathStartsWithEx(uBuf.UniStr.Buffer, uBuf.UniStr.Length / sizeof(WCHAR),
                                            g_SupLibHardenedExeNtPath.UniStr.Buffer,
                                            g_offSupLibHardenedExeNtName, false /*fCheckSlash*/))
        fFlags |= SUPHNTVI_F_REQUIRE_KERNEL_CODE_SIGNING | SUPHNTVI_F_REQUIRE_SIGNATURE_ENFORCEMENT;
# ifdef VBOX_PERMIT_MORE
    else if (supHardViIsAppPatchDir(uBuf.UniStr.Buffer, uBuf.UniStr.Length / sizeof(WCHAR)))
        fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION | SUPHNTVI_F_TRUSTED_INSTALLER_OWNER;
    else if (supHardViUniStrPathStartsWithUniStr(&uBuf.UniStr, &g_ProgramFilesNtPath.UniStr, true /*fCheckSlash*/))
        fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION | SUPHNTVI_F_TRUSTED_INSTALLER_OWNER;
    else if (supHardViUniStrPathStartsWithUniStr(&uBuf.UniStr, &g_CommonFilesNtPath.UniStr, true /*fCheckSlash*/))
        fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION | SUPHNTVI_F_TRUSTED_INSTALLER_OWNER;
#  ifdef RT_ARCH_AMD64
    else if (supHardViUniStrPathStartsWithUniStr(&uBuf.UniStr, &g_ProgramFilesX86NtPath.UniStr, true /*fCheckSlash*/))
        fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION | SUPHNTVI_F_TRUSTED_INSTALLER_OWNER;
    else if (supHardViUniStrPathStartsWithUniStr(&uBuf.UniStr, &g_CommonFilesX86NtPath.UniStr, true /*fCheckSlash*/))
        fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION | SUPHNTVI_F_TRUSTED_INSTALLER_OWNER;
#  endif
# endif
# ifdef VBOX_PERMIT_VISUAL_STUDIO_PROFILING
    /* Hack to allow profiling our code with Visual Studio. */
    else if (   uBuf.UniStr.Length > sizeof(L"\\SamplingRuntime.dll")
             && memcmp(uBuf.UniStr.Buffer + (uBuf.UniStr.Length - sizeof(L"\\SamplingRuntime.dll") + sizeof(WCHAR)) / sizeof(WCHAR),
                       L"\\SamplingRuntime.dll", sizeof(L"\\SamplingRuntime.dll") - sizeof(WCHAR)) == 0 )
    {
        if (hMyFile != hFile)
            NtClose(hMyFile);
        *pfCallRealApi = true;
        return STATUS_SUCCESS;
    }
# endif
    else
    {
        supR3HardenedError(VINF_SUCCESS, false,
                           "supR3HardenedScreenImage/%s: Not a trusted location: '%ls' (fImage=%d fProtect=%#x fAccess=%#x)\n",
                            pszCaller, uBuf.UniStr.Buffer, fImage, *pfAccess, *pfProtect);
        if (hMyFile != hFile)
            NtClose(hMyFile);
        return STATUS_TRUST_FAILURE;
    }

#else  /* VBOX_PERMIT_EVEN_MORE */
    /*
     * Require trusted installer + some kind of signature on everything, except
     * for the VBox bits where we require kernel code signing and special
     * integrity checks.
     */
    Assert(g_SupLibHardenedExeNtPath.UniStr.Buffer[g_offSupLibHardenedExeNtName - 1] == '\\');
    uint32_t fFlags = 0;
    if (supHardViUtf16PathStartsWithEx(uBuf.UniStr.Buffer, uBuf.UniStr.Length / sizeof(WCHAR),
                                       g_SupLibHardenedExeNtPath.UniStr.Buffer,
                                       g_offSupLibHardenedExeNtName, false /*fCheckSlash*/))
        fFlags |= SUPHNTVI_F_REQUIRE_KERNEL_CODE_SIGNING | SUPHNTVI_F_REQUIRE_SIGNATURE_ENFORCEMENT;
    else
        fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION | SUPHNTVI_F_TRUSTED_INSTALLER_OWNER;
#endif /* VBOX_PERMIT_EVEN_MORE */

    /*
     * Do the verification. For better error message we borrow what's
     * left of the path buffer for an RTERRINFO buffer.
     */
    RTERRINFO ErrInfo;
    RTErrInfoInit(&ErrInfo, (char *)&uBuf.abBuffer[cbNameBuf], sizeof(uBuf) - cbNameBuf);

    int  rc;
    bool fWinVerifyTrust = false;
    rc = supHardenedWinVerifyImageByHandle(hMyFile, uBuf.UniStr.Buffer, fFlags, fAvoidWinVerifyTrust, &fWinVerifyTrust, &ErrInfo);
    if (RT_FAILURE(rc))
    {
        supR3HardenedError(VINF_SUCCESS, false,
                           "supR3HardenedScreenImage/%s: rc=%Rrc fImage=%d fProtect=%#x fAccess=%#x %ls: %s\n",
                           pszCaller, rc, fImage, *pfAccess, *pfProtect, uBuf.UniStr.Buffer, ErrInfo.pszMsg);
        if (hMyFile != hFile)
            supR3HardenedWinVerifyCacheInsert(&uBuf.UniStr, hMyFile, rc, fWinVerifyTrust, fFlags);
        return STATUS_TRUST_FAILURE;
    }

    /*
     * Insert into the cache.
     */
    if (hMyFile != hFile)
        supR3HardenedWinVerifyCacheInsert(&uBuf.UniStr, hMyFile, rc, fWinVerifyTrust, fFlags);

    *pfCallRealApi = true;
    return STATUS_SUCCESS;
}


/**
 * Preloads a file into the verify cache if possible.
 *
 * This is used to avoid known cyclic LoadLibrary issues with WinVerifyTrust.
 *
 * @param   pwszName            The name of the DLL to verify.
 */
DECLHIDDEN(void) supR3HardenedWinVerifyCachePreload(PCRTUTF16 pwszName)
{
    HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;

    UNICODE_STRING      UniStr;
    UniStr.Buffer = (PWCHAR)pwszName;
    UniStr.Length = (USHORT)(RTUtf16Len(pwszName) * sizeof(WCHAR));
    UniStr.MaximumLength = UniStr.Length + sizeof(WCHAR);

    OBJECT_ATTRIBUTES   ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &UniStr, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

    NTSTATUS rcNt = NtCreateFile(&hFile,
                                 FILE_READ_DATA | READ_CONTROL | SYNCHRONIZE,
                                 &ObjAttr,
                                 &Ios,
                                 NULL /* Allocation Size*/,
                                 FILE_ATTRIBUTE_NORMAL,
                                 FILE_SHARE_READ,
                                 FILE_OPEN,
                                 FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                 NULL /*EaBuffer*/,
                                 0 /*EaLength*/);
    if (NT_SUCCESS(rcNt))
        rcNt = Ios.Status;
    if (!NT_SUCCESS(rcNt))
    {
        SUP_DPRINTF(("supR3HardenedWinVerifyCachePreload: Error %#x opening '%ls'.\n", rcNt, pwszName));
        return;
    }

    ULONG fAccess = 0;
    ULONG fProtect = 0;
    bool  fCallRealApi;
    //SUP_DPRINTF(("supR3HardenedWinVerifyCachePreload: scanning %ls\n", pwszName));
    supR3HardenedScreenImage(hFile, false, &fAccess, &fProtect, &fCallRealApi, "preload", false /*fAvoidWinVerifyTrust*/,
                             NULL /*pfQuietFailure*/);
    //SUP_DPRINTF(("supR3HardenedWinVerifyCachePreload: done %ls\n", pwszName));

    NtClose(hFile);
}



/**
 * Hook that monitors NtCreateSection calls.
 *
 * @returns NT status code.
 * @param   phSection           Where to return the section handle.
 * @param   fAccess             The desired access.
 * @param   pObjAttribs         The object attributes (optional).
 * @param   pcbSection          The section size (optional).
 * @param   fProtect            The max section protection.
 * @param   fAttribs            The section attributes.
 * @param   hFile               The file to create a section from (optional).
 */
static NTSTATUS NTAPI
supR3HardenedMonitor_NtCreateSection(PHANDLE phSection, ACCESS_MASK fAccess, POBJECT_ATTRIBUTES pObjAttribs,
                                     PLARGE_INTEGER pcbSection, ULONG fProtect, ULONG fAttribs, HANDLE hFile)
{
    if (   hFile != NULL
        && hFile != INVALID_HANDLE_VALUE)
    {
        bool const fImage    = RT_BOOL(fAttribs & (SEC_IMAGE | SEC_PROTECTED_IMAGE));
        bool const fExecMap  = RT_BOOL(fAccess & SECTION_MAP_EXECUTE);
        bool const fExecProt = RT_BOOL(fProtect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_WRITECOPY
                                                   | PAGE_EXECUTE_READWRITE));
        if (fImage || fExecMap || fExecProt)
        {
            DWORD dwSavedLastError = RtlGetLastWin32Error();

            bool fCallRealApi;
            //SUP_DPRINTF(("supR3HardenedMonitor_NtCreateSection: 1\n"));
            NTSTATUS rcNt = supR3HardenedScreenImage(hFile, fImage, &fAccess, &fProtect, &fCallRealApi,
                                                     "NtCreateSection", true /*fAvoidWinVerifyTrust*/, NULL /*pfQuietFailure*/);
            //SUP_DPRINTF(("supR3HardenedMonitor_NtCreateSection: 2 rcNt=%#x fCallRealApi=%#x\n", rcNt, fCallRealApi));

            RtlRestoreLastWin32Error(dwSavedLastError);

            if (!NT_SUCCESS(rcNt))
                return rcNt;
            Assert(fCallRealApi);
            if (!fCallRealApi)
                return STATUS_TRUST_FAILURE;

        }
    }

    /*
     * Call checked out OK, call the original.
     */
    return g_pfnNtCreateSectionReal(phSection, fAccess, pObjAttribs, pcbSection, fProtect, fAttribs, hFile);
}


/**
 * Helper for supR3HardenedMonitor_LdrLoadDll.
 *
 * @returns NT status code.
 * @param   pwszPath        The path destination buffer.
 * @param   cwcPath         The size of the path buffer.
 * @param   pUniStrResult   The result string.
 * @param   pOrgName        The orignal name (for errors).
 * @param   pcwc            Where to return the actual length.
 */
static NTSTATUS supR3HardenedCopyRedirectionResult(WCHAR *pwszPath, size_t cwcPath, PUNICODE_STRING pUniStrResult,
                                                   PUNICODE_STRING pOrgName, UINT *pcwc)
{
    UINT cwc;
    *pcwc = cwc = pUniStrResult->Length / sizeof(WCHAR);
    if (pUniStrResult->Buffer == pwszPath)
        pwszPath[cwc] = '\0';
    else
    {
        if (cwc > cwcPath - 1)
        {
            supR3HardenedError(VINF_SUCCESS, false,
                               "supR3HardenedMonitor_LdrLoadDll: Name too long: %.*ls -> %.*ls (RtlDosApplyFileIoslationRedirection_Ustr)\n",
                               pOrgName->Length / sizeof(WCHAR), pOrgName->Buffer,
                               pUniStrResult->Length / sizeof(WCHAR), pUniStrResult->Buffer);
            return STATUS_NAME_TOO_LONG;
        }
        memcpy(&pwszPath[0], pUniStrResult->Buffer, pUniStrResult->Length);
        pwszPath[cwc] = '\0';
    }
    return STATUS_SUCCESS;
}


/**
 * Hooks that intercepts LdrLoadDll calls.
 *
 * Two purposes:
 *      -# Enforce our own search path restrictions.
 *      -# Prevalidate DLLs about to be loaded so we don't upset the loader data
 *         by doing it from within the NtCreateSection hook (WinVerifyTrust
 *         seems to be doing harm there on W7/32).
 *
 * @returns
 * @param   pwszSearchPath      The search path to use.
 * @param   pfFlags             Flags on input. DLL characteristics or something
 *                              on return?
 * @param   pName               The name of the module.
 * @param   phMod               Where the handle of the loaded DLL is to be
 *                              returned to the caller.
 */
static NTSTATUS NTAPI
supR3HardenedMonitor_LdrLoadDll(PWSTR pwszSearchPath, PULONG pfFlags, PUNICODE_STRING pName, PHANDLE phMod)
{
    DWORD    dwSavedLastError = RtlGetLastWin32Error();
    NTSTATUS rcNt;

    /*
     * Process WinVerifyTrust todo before and after.
     */
    supR3HardenedWinVerifyCacheProcessWvtTodos();

    /*
     * Reject things we don't want to deal with.
     */
    if (!pName || pName->Length == 0)
    {
        supR3HardenedError(VINF_SUCCESS, false, "supR3HardenedMonitor_LdrLoadDll: name is NULL or have a zero length.\n");
        SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x (pName=%p)\n", STATUS_INVALID_PARAMETER, pName));
        RtlRestoreLastWin32Error(dwSavedLastError);
        return STATUS_INVALID_PARAMETER;
    }
    /*SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: pName=%.*ls *pfFlags=%#x pwszSearchPath=%p:%ls\n",
                 (unsigned)pName->Length / sizeof(WCHAR), pName->Buffer, pfFlags ? *pfFlags : UINT32_MAX, pwszSearchPath,
                 !((uintptr_t)pwszSearchPath & 1) && (uintptr_t)pwszSearchPath >= 0x2000U ? pwszSearchPath : L"<flags>"));*/

    /*
     * Reject long paths that's close to the 260 limit without looking.
     */
    if (pName->Length > 256 * sizeof(WCHAR))
    {
        supR3HardenedError(VINF_SUCCESS, false, "supR3HardenedMonitor_LdrLoadDll: too long name: %#x bytes\n", pName->Length);
        SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x\n", STATUS_NAME_TOO_LONG));
        RtlRestoreLastWin32Error(dwSavedLastError);
        return STATUS_NAME_TOO_LONG;
    }

    /*
     * Absolute path?
     */
    bool            fSkipValidation = false;
    WCHAR           wszPath[260];
    static UNICODE_STRING const s_DefaultSuffix = RTNT_CONSTANT_UNISTR(L".dll");
    UNICODE_STRING  UniStrStatic   = { 0, (USHORT)sizeof(wszPath) - sizeof(WCHAR), wszPath };
    UNICODE_STRING  UniStrDynamic  = { 0, 0, NULL };
    PUNICODE_STRING pUniStrResult  = NULL;
    UNICODE_STRING  ResolvedName;

    if (   (   pName->Length >= 4 * sizeof(WCHAR)
            && RT_C_IS_ALPHA(pName->Buffer[0])
            && pName->Buffer[1] == ':'
            && RTPATH_IS_SLASH(pName->Buffer[2]) )
        || (   pName->Length >= 1 * sizeof(WCHAR)
            && RTPATH_IS_SLASH(pName->Buffer[1]) )
       )
    {
        rcNt = RtlDosApplyFileIsolationRedirection_Ustr(1 /*fFlags*/,
                                                        pName,
                                                        (PUNICODE_STRING)&s_DefaultSuffix,
                                                        &UniStrStatic,
                                                        &UniStrDynamic,
                                                        &pUniStrResult,
                                                        NULL /*pNewFlags*/,
                                                        NULL /*pcbFilename*/,
                                                        NULL /*pcbNeeded*/);
        if (NT_SUCCESS(rcNt))
        {
            UINT cwc;
            rcNt = supR3HardenedCopyRedirectionResult(wszPath, RT_ELEMENTS(wszPath), pUniStrResult, pName, &cwc);
            RtlFreeUnicodeString(&UniStrDynamic);
            if (!NT_SUCCESS(rcNt))
            {
                SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x\n", rcNt));
                RtlRestoreLastWin32Error(dwSavedLastError);
                return rcNt;
            }

            ResolvedName.Buffer = wszPath;
            ResolvedName.Length = (USHORT)(cwc * sizeof(WCHAR));
            ResolvedName.MaximumLength = ResolvedName.Length + sizeof(WCHAR);

            SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: '%.*ls' -> '%.*ls' [redir]\n",
                         (unsigned)pName->Length / sizeof(WCHAR), pName->Buffer,
                         ResolvedName.Length / sizeof(WCHAR), ResolvedName.Buffer, rcNt));
            pName = &ResolvedName;
        }
        else
        {
            memcpy(wszPath, pName->Buffer, pName->Length);
            wszPath[pName->Length / sizeof(WCHAR)] = '\0';
        }
    }
    /*
     * Not an absolute path.  Check if it's one of those special API set DLLs
     * or something we're known to use but should be taken from WinSxS.
     */
    else if (supHardViUtf16PathStartsWithEx(pName->Buffer, pName->Length / sizeof(WCHAR),
                                            L"api-ms-win-", 11, false /*fCheckSlash*/))
    {
        memcpy(wszPath, pName->Buffer, pName->Length);
        wszPath[pName->Length / sizeof(WCHAR)] = '\0';
        fSkipValidation = true;
    }
    /*
     * Not an absolute path or special API set.  There are two alternatives
     * now, either there is no path at all or there is a relative path.  We
     * will resolve it to an absolute path in either case, failing the call
     * if we can't.
     */
    else
    {
        PCWCHAR  pawcName     = pName->Buffer;
        uint32_t cwcName      = pName->Length / sizeof(WCHAR);
        uint32_t offLastSlash = UINT32_MAX;
        uint32_t offLastDot   = UINT32_MAX;
        for (uint32_t i = 0; i < cwcName; i++)
            switch (pawcName[i])
            {
                case '\\':
                case '/':
                    offLastSlash = i;
                    offLastDot = UINT32_MAX;
                    break;
                case '.':
                    offLastDot = i;
                    break;
            }

        bool const fNeedDllSuffix = offLastDot == UINT32_MAX && offLastSlash == UINT32_MAX;

        if (offLastDot != UINT32_MAX && offLastDot == cwcName - 1)
            cwcName--;

        /*
         * Reject relative paths for now as they might be breakout attempts.
         */
        if (offLastSlash != UINT32_MAX)
        {
            supR3HardenedError(VINF_SUCCESS, false,
                               "supR3HardenedMonitor_LdrLoadDll: relative name not permitted: %.*ls\n",
                               cwcName, pawcName);
            SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x\n", STATUS_OBJECT_NAME_INVALID));
            RtlRestoreLastWin32Error(dwSavedLastError);
            return STATUS_OBJECT_NAME_INVALID;
        }

        /*
         * Perform dll redirection to WinSxS such.  We using an undocumented
         * API here, which as always is a bit risky...  ASSUMES that the API
         * returns a full DOS path.
         */
        UINT cwc;
        rcNt = RtlDosApplyFileIsolationRedirection_Ustr(1 /*fFlags*/,
                                                        pName,
                                                        (PUNICODE_STRING)&s_DefaultSuffix,
                                                        &UniStrStatic,
                                                        &UniStrDynamic,
                                                        &pUniStrResult,
                                                        NULL /*pNewFlags*/,
                                                        NULL /*pcbFilename*/,
                                                        NULL /*pcbNeeded*/);
        if (NT_SUCCESS(rcNt))
        {
            rcNt = supR3HardenedCopyRedirectionResult(wszPath, RT_ELEMENTS(wszPath), pUniStrResult, pName, &cwc);
            RtlFreeUnicodeString(&UniStrDynamic);
            if (!NT_SUCCESS(rcNt))
            {
                SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x\n", rcNt));
                RtlRestoreLastWin32Error(dwSavedLastError);
                return rcNt;
            }
        }
        else
        {
            /*
             * Search for the DLL.  Only System32 is allowed as the target of
             * a search on the API level, all VBox calls will have full paths.
             */
            AssertCompile(sizeof(g_System32WinPath.awcBuffer) <= sizeof(wszPath));
            cwc = g_System32WinPath.UniStr.Length / sizeof(RTUTF16); Assert(cwc > 2);
            if (cwc + 1 + cwcName + fNeedDllSuffix * 4 >= RT_ELEMENTS(wszPath))
            {
                supR3HardenedError(VINF_SUCCESS, false,
                                   "supR3HardenedMonitor_LdrLoadDll: Name too long (system32): %.*ls\n", cwcName, pawcName);
                SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x\n", STATUS_NAME_TOO_LONG));
                RtlRestoreLastWin32Error(dwSavedLastError);
                return STATUS_NAME_TOO_LONG;
            }
            memcpy(wszPath, g_System32WinPath.UniStr.Buffer, cwc * sizeof(RTUTF16));
            wszPath[cwc++] = '\\';
            memcpy(&wszPath[cwc], pawcName, cwcName * sizeof(WCHAR));
            cwc += cwcName;
            if (!fNeedDllSuffix)
                wszPath[cwc] = '\0';
            else
            {
                memcpy(&wszPath[cwc], L".dll", 5 * sizeof(WCHAR));
                cwc += 4;
            }
        }

        ResolvedName.Buffer = wszPath;
        ResolvedName.Length = (USHORT)(cwc * sizeof(WCHAR));
        ResolvedName.MaximumLength = ResolvedName.Length + sizeof(WCHAR);

        SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: '%.*ls' -> '%.*ls' [rcNt=%#x]\n",
                     (unsigned)pName->Length / sizeof(WCHAR), pName->Buffer,
                     ResolvedName.Length / sizeof(WCHAR), ResolvedName.Buffer, rcNt));
        pName = &ResolvedName;
    }

    if (!fSkipValidation)
    {
        /*
         * Try open the file.  If this fails, never mind, just pass it on to
         * the real API as we've replaced any searchable name with a full name
         * and the real API can come up with a fitting status code for it.
         */
        HANDLE          hRootDir;
        UNICODE_STRING  NtPathUniStr;
        int rc = RTNtPathFromWinUtf16Ex(&NtPathUniStr, &hRootDir, wszPath, RTSTR_MAX);
        if (RT_FAILURE(rc))
        {
            supR3HardenedError(rc, false,
                               "supR3HardenedMonitor_LdrLoadDll: RTNtPathFromWinUtf16Ex failed on '%ls': %Rrc\n", wszPath, rc);
            SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x\n", STATUS_OBJECT_NAME_INVALID));
            RtlRestoreLastWin32Error(dwSavedLastError);
            return STATUS_OBJECT_NAME_INVALID;
        }

        HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
        IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        OBJECT_ATTRIBUTES   ObjAttr;
        InitializeObjectAttributes(&ObjAttr, &NtPathUniStr, OBJ_CASE_INSENSITIVE, hRootDir, NULL /*pSecDesc*/);

        rcNt = NtCreateFile(&hFile,
                            FILE_READ_DATA | READ_CONTROL | SYNCHRONIZE,
                            &ObjAttr,
                            &Ios,
                            NULL /* Allocation Size*/,
                            FILE_ATTRIBUTE_NORMAL,
                            FILE_SHARE_READ,
                            FILE_OPEN,
                            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                            NULL /*EaBuffer*/,
                            0 /*EaLength*/);
        if (NT_SUCCESS(rcNt))
            rcNt = Ios.Status;
        if (NT_SUCCESS(rcNt))
        {
            ULONG fAccess = 0;
            ULONG fProtect = 0;
            bool  fCallRealApi = false;
            bool  fQuietFailure = false;
            rcNt = supR3HardenedScreenImage(hFile, true /*fImage*/, &fAccess, &fProtect, &fCallRealApi,
                                            "LdrLoadDll", false /*fAvoidWinVerifyTrust*/, &fQuietFailure);
            NtClose(hFile);
            if (!NT_SUCCESS(rcNt))
            {
                if (!fQuietFailure)
                {
                    supR3HardenedError(VINF_SUCCESS, false, "supR3HardenedMonitor_LdrLoadDll: rejecting '%ls': rcNt=%#x\n",
                                       wszPath, rcNt);
                    SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x '%ls'\n", rcNt, wszPath));
                }
                RtlRestoreLastWin32Error(dwSavedLastError);
                return rcNt;
            }

            supR3HardenedWinVerifyCacheProcessImportTodos();
        }
        else
        {
            DWORD dwErr = RtlGetLastWin32Error();
            SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: error opening '%ls': %u (NtPath=%.*ls)\n",
                         wszPath, dwErr, NtPathUniStr.Length / sizeof(RTUTF16), NtPathUniStr.Buffer));
        }
        RTNtPathFree(&NtPathUniStr, &hRootDir);
    }

    /*
     * Screened successfully enough.  Call the real thing.
     */
    SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: pName=%.*ls *pfFlags=%#x pwszSearchPath=%p:%ls [calling]\n",
                 (unsigned)pName->Length / sizeof(WCHAR), pName->Buffer, pfFlags ? *pfFlags : UINT32_MAX, pwszSearchPath,
                 !((uintptr_t)pwszSearchPath & 1) && (uintptr_t)pwszSearchPath >= 0x2000U ? pwszSearchPath : L"<flags>"));
    RtlRestoreLastWin32Error(dwSavedLastError);
    rcNt = g_pfnLdrLoadDllReal(pwszSearchPath, pfFlags, pName, phMod);

    /*
     * Log the result and process pending WinVerifyTrust work if we can.
     */
    dwSavedLastError = RtlGetLastWin32Error();

    if (NT_SUCCESS(rcNt) && phMod)
        SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x hMod=%p '%ls'\n", rcNt, *phMod, wszPath));
    else
        SUP_DPRINTF(("supR3HardenedMonitor_LdrLoadDll: returns rcNt=%#x '%ls'\n", rcNt, wszPath));
    supR3HardenedWinVerifyCacheProcessWvtTodos();

    RtlRestoreLastWin32Error(dwSavedLastError);

    return rcNt;
}


#ifdef RT_ARCH_AMD64
/**
 * Tries to allocate memory between @a uStart and @a uEnd.
 *
 * @returns Pointer to the memory on success.  NULL on failure.
 * @param   uStart              The start address.
 * @param   uEnd                The end address.  This is lower than @a uStart
 *                              if @a iDirection is negative, and higher if
 *                              positive.
 * @param   iDirection          The search direction.
 * @param   cbAlloc             The number of bytes to allocate.
 */
static void *supR3HardenedWinAllocHookMemory(uintptr_t uStart, uintptr_t uEnd, intptr_t iDirection, size_t cbAlloc)
{
    size_t const cbAllocGranularity    = _64K;
    size_t const uAllocGranularityMask = ~(cbAllocGranularity - 1);
    HANDLE const hProc                 = NtCurrentProcess();

    /*
     * Make uEnd the last valid return address.
     */
    if (iDirection > 0)
    {
        SUPR3HARDENED_ASSERT(uEnd > cbAlloc);
        uEnd -= cbAlloc;
        uEnd &= uAllocGranularityMask;
    }
    else
        uEnd = RT_ALIGN_Z(uEnd, cbAllocGranularity);

    /*
     * Search for free memory.
     */
    uintptr_t uCur = uStart & uAllocGranularityMask;
    for (;;)
    {
        /*
         * Examine the memory at this address, if it's free, try make the allocation here.
         */
        SIZE_T                      cbIgn;
        MEMORY_BASIC_INFORMATION    MemInfo;
        SUPR3HARDENED_ASSERT_NT_SUCCESS(NtQueryVirtualMemory(hProc,
                                                             (void *)uCur,
                                                             MemoryBasicInformation,
                                                             &MemInfo,
                                                             sizeof(MemInfo),
                                                             &cbIgn));
        if (   MemInfo.State      == MEM_FREE
            && MemInfo.RegionSize >= cbAlloc)
        {
            for (;;)
            {
                SUPR3HARDENED_ASSERT((uintptr_t)MemInfo.BaseAddress <= uCur);

                PVOID    pvMem = (PVOID)uCur;
                SIZE_T   cbMem = cbAlloc;
                NTSTATUS rcNt = NtAllocateVirtualMemory(hProc, &pvMem, 0 /*ZeroBits*/, &cbAlloc,
                                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (NT_SUCCESS(rcNt))
                {
                    if (  iDirection > 0
                        ?    (uintptr_t)pvMem >= uStart
                          && (uintptr_t)pvMem <= uEnd
                        :    (uintptr_t)pvMem >= uEnd
                          && (uintptr_t)pvMem <= uStart)
                        return pvMem;
                    NtFreeVirtualMemory(hProc, &pvMem, &cbMem, MEM_RELEASE);
                }

                /* Advance within the free area and try again? */
                uintptr_t uNext = iDirection > 0 ? uCur + cbAllocGranularity : uCur - cbAllocGranularity;
                uNext &= uAllocGranularityMask;
                if (  iDirection > 0
                    ?    uNext <= uCur
                      || uNext >  uEnd
                      || uNext - (uintptr_t)MemInfo.BaseAddress > MemInfo.RegionSize
                      || MemInfo.RegionSize - (uNext - (uintptr_t)MemInfo.BaseAddress) < cbAlloc
                    :    uNext >= uCur
                      || uNext <  uEnd
                      || uNext <  (uintptr_t)MemInfo.BaseAddress)
                    break;
                uCur = uNext;
            }
        }

        /*
         * Advance to the next memory region.
         */
        if (iDirection > 0)
        {
            uCur = (uintptr_t)MemInfo.BaseAddress + MemInfo.RegionSize;
            uCur = RT_ALIGN_Z(uCur, cbAllocGranularity);
            if (uCur >= uEnd)
                break;
        }
        else
        {
            uCur = (uintptr_t)(MemInfo.AllocationBase ? MemInfo.AllocationBase : MemInfo.BaseAddress);
            if (uCur > uEnd)
                uCur -= cbAlloc;
            uCur &= uAllocGranularityMask;
            if (uCur < uEnd)
                break;
        }
    }
    return NULL;
}
#endif


static void supR3HardenedWinHookFailed(const char *pszWhich, uint8_t const *pbPrologue)
{
    supR3HardenedFatalMsg("supR3HardenedWinInstallHooks", kSupInitOp_Misc, VERR_NO_MEMORY,
                          "Failed to install %s monitor: %x %x %x %x  %x %x %x %x  %x %x %x %x  %x %x %x %x\n "
#ifdef RT_ARCH_X86
                          "(It is also possible you are running 32-bit VirtualBox under 64-bit windows.)\n"
#endif
                          ,
                          pszWhich,
                          pbPrologue[0],  pbPrologue[1],  pbPrologue[2],  pbPrologue[3],
                          pbPrologue[4],  pbPrologue[5],  pbPrologue[6],  pbPrologue[7],
                          pbPrologue[8],  pbPrologue[9],  pbPrologue[10], pbPrologue[11],
                          pbPrologue[12], pbPrologue[13], pbPrologue[14], pbPrologue[15]);
}


/**
 * IPRT thread that waits for the parent process to terminate and reacts by
 * exiting the current process.
 *
 * @returns VINF_SUCCESS
 * @param   hSelf               The current thread.  Ignored.
 * @param   pvUser              The handle of the parent process.
 */
static DECLCALLBACK(int) supR3HardenedWinParentWatcherThread(RTTHREAD hSelf, void *pvUser)
{
    HANDLE hProcWait = (HANDLE)pvUser;
    NOREF(hSelf);

    /*
     * Wait for the parent to terminate.
     */
    NTSTATUS rcNt;
    for (;;)
    {
        rcNt = NtWaitForSingleObject(hProcWait, TRUE /*Alertable*/, NULL /*pTimeout*/);
        if (   rcNt == STATUS_WAIT_0
            || rcNt == STATUS_ABANDONED_WAIT_0)
            break;
        if (   rcNt != STATUS_TIMEOUT
            && rcNt != STATUS_USER_APC
            && rcNt != STATUS_ALERTED)
            supR3HardenedFatal("NtWaitForSingleObject returned %#x\n", rcNt);
    }

    /*
     * Proxy the termination code of the child, if it exited already.
     */
    PROCESS_BASIC_INFORMATION BasicInfo;
    NTSTATUS rcNt2 = NtQueryInformationProcess(hProcWait, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
    if (   !NT_SUCCESS(rcNt2)
        || BasicInfo.ExitStatus == STATUS_PENDING)
        BasicInfo.ExitStatus = RTEXITCODE_FAILURE;

    NtClose(hProcWait);
    SUP_DPRINTF(("supR3HardenedWinParentWatcherThread: Quitting: ExitCode=%#x rcNt=%#x\n", BasicInfo.ExitStatus, rcNt));
    suplibHardenedExit((RTEXITCODE)BasicInfo.ExitStatus);

    return VINF_SUCCESS; /* won't be reached. */
}


/**
 * Creates the parent watcher thread that will make sure this process exits when
 * the parent does.
 *
 * This is a necessary evil to make VBoxNetDhcp and VBoxNetNat termination from
 * Main work without too much new magic.  It also makes Ctrl-C or similar work
 * in on the hardened processes in the windows console.
 *
 * @param   hVBoxRT             The VBoxRT.dll handle.  We use RTThreadCreate to
 *                              spawn the thread to avoid duplicating thread
 *                              creation and thread naming code from IPRT.
 */
DECLHIDDEN(void) supR3HardenedWinCreateParentWatcherThread(HMODULE hVBoxRT)
{
    /*
     * Resolve runtime methods that we need.
     */
    PFNRTTHREADCREATE pfnRTThreadCreate = (PFNRTTHREADCREATE)GetProcAddress(hVBoxRT, "RTThreadCreate");
    SUPR3HARDENED_ASSERT(pfnRTThreadCreate != NULL);

    /*
     * Find the parent process ID.
     */
    PROCESS_BASIC_INFORMATION BasicInfo;
    NTSTATUS rcNt = NtQueryInformationProcess(NtCurrentProcess(), ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatal("supR3HardenedWinCreateParentWatcherThread: NtQueryInformationProcess failed: %#x\n", rcNt);

    /*
     * Open the parent process for waiting and exitcode query.
     */
    OBJECT_ATTRIBUTES ObjAttr;
    InitializeObjectAttributes(&ObjAttr, NULL, 0, NULL /*hRootDir*/, NULL /*pSecDesc*/);

    CLIENT_ID ClientId;
    ClientId.UniqueProcess = (HANDLE)BasicInfo.InheritedFromUniqueProcessId;
    ClientId.UniqueThread  = NULL;

    HANDLE hParent;
    rcNt = NtOpenProcess(&hParent, SYNCHRONIZE | PROCESS_QUERY_INFORMATION, &ObjAttr, &ClientId);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatalMsg("supR3HardenedWinCreateParentWatcherThread", kSupInitOp_Misc, VERR_GENERAL_FAILURE,
                              "NtOpenProcess(%p.0) failed: %#x\n", ClientId.UniqueProcess, rcNt);

    /*
     * Create the thread that should do the waiting.
     */
    int rc = pfnRTThreadCreate(NULL, supR3HardenedWinParentWatcherThread, hParent, _64K /* stack */,
                               RTTHREADTYPE_DEFAULT, 0 /*fFlags*/, "ParentWatcher");
    if (RT_FAILURE(rc))
        supR3HardenedFatal("supR3HardenedWinCreateParentWatcherThread: RTThreadCreate failed: %Rrc\n", rc);
}


/**
 * Simplify NtProtectVirtualMemory interface.
 *
 * Modifies protection for the current process.  Caller must know the current
 * protection as it's not returned.
 *
 * @returns NT status code.
 * @param   pvMem               The memory to change protection for.
 * @param   cbMem               The amount of memory to change.
 * @param   fNewProt            The new protection.
 */
static NTSTATUS supR3HardenedWinProtectMemory(PVOID pvMem, SIZE_T cbMem, ULONG fNewProt)
{
    ULONG fOldProt = 0;
    return NtProtectVirtualMemory(NtCurrentProcess(), &pvMem, &cbMem, fNewProt, &fOldProt);
}


/**
 * Install hooks for intercepting calls dealing with mapping shared libraries
 * into the process.
 *
 * This allows us to prevent undesirable shared libraries from being loaded.
 *
 * @remarks We assume we're alone in this process, so no seralizing trickery is
 *          necessary when installing the patch.
 *
 * @remarks We would normally just copy the prologue sequence somewhere and add
 *          a jump back at the end of it. But because we wish to avoid
 *          allocating executable memory, we need to have preprepared assembly
 *          "copies".  This makes the non-system call patching a little tedious
 *            and inflexible.
 */
DECLHIDDEN(void) supR3HardenedWinInstallHooks(void)
{
    NTSTATUS rcNt;

#ifndef VBOX_WITHOUT_DEBUGGER_CHECKS
    /*
     * Install a anti debugging hack before we continue.  This prevents most
     * notifications from ending up in the debugger. (Also applied to the
     * child process when respawning.)
     */
    rcNt = NtSetInformationThread(NtCurrentThread(), ThreadHideFromDebugger, NULL, 0);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatalMsg("supR3HardenedWinInstallHooks", kSupInitOp_Misc, VERR_GENERAL_FAILURE,
                              "NtSetInformationThread/ThreadHideFromDebugger failed: %#x\n", rcNt);
#endif

    /*
     * Disable hard error popups so we can quietly refuse images to be loaded.
     */
    ULONG fHardErr = 0;
    rcNt = NtQueryInformationProcess(NtCurrentProcess(), ProcessDefaultHardErrorMode, &fHardErr, sizeof(fHardErr), NULL);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatalMsg("supR3HardenedWinInstallHooks", kSupInitOp_Misc, VERR_GENERAL_FAILURE,
                              "NtQueryInformationProcess/ProcessDefaultHardErrorMode failed: %#x\n", rcNt);
    if (fHardErr & PROCESS_HARDERR_CRITICAL_ERROR)
    {
        fHardErr &= ~PROCESS_HARDERR_CRITICAL_ERROR;
        rcNt = NtSetInformationProcess(NtCurrentProcess(), ProcessDefaultHardErrorMode, &fHardErr, sizeof(fHardErr));
        if (!NT_SUCCESS(rcNt))
            supR3HardenedFatalMsg("supR3HardenedWinInstallHooks", kSupInitOp_Misc, VERR_GENERAL_FAILURE,
                                  "NtSetInformationProcess/ProcessDefaultHardErrorMode failed: %#x\n", rcNt);
    }

    /*
     * Locate the routines first so we can allocate memory that's near enough.
     */
    PFNRT pfnNtCreateSection = supR3HardenedWinGetRealDllSymbol("ntdll.dll", "NtCreateSection");
    SUPR3HARDENED_ASSERT(pfnNtCreateSection != NULL);
    //SUPR3HARDENED_ASSERT(pfnNtCreateSection == (FARPROC)NtCreateSection);

    PFNRT pfnLdrLoadDll = supR3HardenedWinGetRealDllSymbol("ntdll.dll", "LdrLoadDll");
    SUPR3HARDENED_ASSERT(pfnLdrLoadDll != NULL);
    //SUPR3HARDENED_ASSERT(pfnLdrLoadDll == (FARPROC)LdrLoadDll);


#ifdef RT_ARCH_AMD64
    /*
     * For 64-bit hosts we need some memory within a +/-2GB range of the
     * actual function to be able to patch it.
     */
    uintptr_t uStart = RT_MAX((uintptr_t)pfnNtCreateSection, (uintptr_t)pfnLdrLoadDll);
    size_t    cbMem  = _4K;
    void  *pvMem = supR3HardenedWinAllocHookMemory(uStart, uStart - _2G + PAGE_SIZE, -1, cbMem);
    if (!pvMem)
    {
        uintptr_t uStart = RT_MIN((uintptr_t)pfnNtCreateSection, (uintptr_t)pfnLdrLoadDll);
        pvMem = supR3HardenedWinAllocHookMemory(uStart, uStart + _2G - PAGE_SIZE, 1, cbMem);
        if (!pvMem)
            supR3HardenedFatalMsg("supR3HardenedWinInstallHooks", kSupInitOp_Misc, VERR_NO_MEMORY,
                                  "Failed to allocate memory within the +/-2GB range from NTDLL.\n");
    }
    uintptr_t *puJmpTab = (uintptr_t *)pvMem;
#endif

    /*
     * Hook #1 - NtCreateSection.
     * Purpose: Validate everything that can be mapped into the process before
     *          it's mapped and we still have a file handle to work with.
     */
    uint8_t * const pbNtCreateSection = (uint8_t *)(uintptr_t)pfnNtCreateSection;

#ifdef RT_ARCH_AMD64
    /*
     * Patch 64-bit hosts.
     */
    PFNRT       pfnCallReal = NULL;
    uint8_t     offJmpBack  = UINT8_MAX;

    /* Pattern #1: XP64/W2K3-64 thru Windows 8.1
       0:000> u ntdll!NtCreateSection
       ntdll!NtCreateSection:
       00000000`779f1750 4c8bd1          mov     r10,rcx
       00000000`779f1753 b847000000      mov     eax,47h
       00000000`779f1758 0f05            syscall
       00000000`779f175a c3              ret
       00000000`779f175b 0f1f440000      nop     dword ptr [rax+rax]
       The variant is the value loaded into eax: W2K3=??, Vista=47h?, W7=47h, W80=48h, W81=49h */
    if (   pbNtCreateSection[ 0] == 0x4c /* mov r10, rcx */
        && pbNtCreateSection[ 1] == 0x8b
        && pbNtCreateSection[ 2] == 0xd1
        && pbNtCreateSection[ 3] == 0xb8 /* mov eax, 000000xxh */
        && pbNtCreateSection[ 5] == 0x00
        && pbNtCreateSection[ 6] == 0x00
        && pbNtCreateSection[ 7] == 0x00
        && pbNtCreateSection[ 8] == 0x0f /* syscall */
        && pbNtCreateSection[ 9] == 0x05
        && pbNtCreateSection[10] == 0xc3 /* ret */

/* b8 22 35 ed 0 48 63 c0 ff e0 c3 f 1f 44 0 0 - necros2 - agnitum firewall? */
       )
    {
        offJmpBack = 8; /* the 3rd instruction (syscall). */
        switch (pbNtCreateSection[4])
        {
# define SYSCALL(a_Num) case a_Num: pfnCallReal = RT_CONCAT(supR3HardenedJmpBack_NtCreateSection_,a_Num); break;
# include "NtCreateSection-template-amd64-syscall-type-1.h"
# undef SYSCALL
        }
    }
    if (!pfnCallReal)
        supR3HardenedWinHookFailed("NtCreateSection", pbNtCreateSection);

    g_pfnNtCreateSectionJmpBack         = (PFNRT)(uintptr_t)(pbNtCreateSection + offJmpBack);
    *(PFNRT *)&g_pfnNtCreateSectionReal = pfnCallReal;
    *puJmpTab                           = (uintptr_t)supR3HardenedMonitor_NtCreateSection;

    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pbNtCreateSection, 16, PAGE_EXECUTE_READWRITE));

    pbNtCreateSection[0] = 0xff;
    pbNtCreateSection[1] = 0x25;
    *(uint32_t *)&pbNtCreateSection[2] = (uint32_t)((uintptr_t)puJmpTab - (uintptr_t)&pbNtCreateSection[2+4]);

    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pbNtCreateSection, 16, PAGE_EXECUTE_READ));
    puJmpTab++;

#else
    /*
     * Patch 32-bit hosts.
     */
    PFNRT       pfnCallReal = NULL;
    uint8_t     offJmpBack  = UINT8_MAX;

    /* Pattern #1: XP thru Windows 7
            kd> u ntdll!NtCreateSection
            ntdll!NtCreateSection:
            7c90d160 b832000000      mov     eax,32h
            7c90d165 ba0003fe7f      mov     edx,offset SharedUserData!SystemCallStub (7ffe0300)
            7c90d16a ff12            call    dword ptr [edx]
            7c90d16c c21c00          ret     1Ch
            7c90d16f 90              nop
       The variable bit is the value loaded into eax: XP=32h, W2K3=34h, Vista=4bh, W7=54h

       Pattern #2: Windows 8.1
            0:000:x86> u ntdll_6a0f0000!NtCreateSection
            ntdll_6a0f0000!NtCreateSection:
            6a15eabc b854010000      mov     eax,154h
            6a15eac1 e803000000      call    ntdll_6a0f0000!NtCreateSection+0xd (6a15eac9)
            6a15eac6 c21c00          ret     1Ch
            6a15eac9 8bd4            mov     edx,esp
            6a15eacb 0f34            sysenter
            6a15eacd c3              ret
       The variable bit is the value loaded into eax: W81=154h
       Note! One nice thing here is that we can share code pattern #1.  */

    if (   pbNtCreateSection[ 0] == 0xb8 /* mov eax, 000000xxh*/
        && pbNtCreateSection[ 2] <= 0x02
        && pbNtCreateSection[ 3] == 0x00
        && pbNtCreateSection[ 4] == 0x00
        && (   (   pbNtCreateSection[ 5] == 0xba /* mov edx, offset SharedUserData!SystemCallStub */
                && pbNtCreateSection[ 6] == 0x00
                && pbNtCreateSection[ 7] == 0x03
                && pbNtCreateSection[ 8] == 0xfe
                && pbNtCreateSection[ 9] == 0x7f
                && pbNtCreateSection[10] == 0xff /* call [edx] */
                && pbNtCreateSection[11] == 0x12
                && pbNtCreateSection[12] == 0xc2 /* ret 1ch */
                && pbNtCreateSection[13] == 0x1c
                && pbNtCreateSection[14] == 0x00)

            || (   pbNtCreateSection[ 5] == 0xe8 /* call [$+3] */
                && RT_ABS(*(int32_t *)&pbNtCreateSection[6]) < 0x10
                && pbNtCreateSection[10] == 0xc2 /* ret 1ch */
                && pbNtCreateSection[11] == 0x1c
                && pbNtCreateSection[12] == 0x00 )
          )
       )
    {
        offJmpBack = 5; /* the 2nd instruction. */
        switch (*(uint32_t const *)&pbNtCreateSection[1])
        {
# define SYSCALL(a_Num) case a_Num: pfnCallReal = RT_CONCAT(supR3HardenedJmpBack_NtCreateSection_,a_Num); break;
# include "NtCreateSection-template-x86-syscall-type-1.h"
# undef SYSCALL
        }
    }
    if (!pfnCallReal)
        supR3HardenedWinHookFailed("NtCreateSection", pbNtCreateSection);

    g_pfnNtCreateSectionJmpBack         = (PFNRT)(uintptr_t)(pbNtCreateSection + offJmpBack);
    *(PFNRT *)&g_pfnNtCreateSectionReal = pfnCallReal;

    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pbNtCreateSection, 16, PAGE_EXECUTE_READWRITE));

    pbNtCreateSection[0] = 0xe9;
    *(uint32_t *)&pbNtCreateSection[1] = (uintptr_t)supR3HardenedMonitor_NtCreateSection
                                       - (uintptr_t)&pbNtCreateSection[1+4];

    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pbNtCreateSection, 16, PAGE_EXECUTE_READ));

#endif

    /*
     * Hook #2 - LdrLoadDll
     * Purpose: (a) Enforce LdrLoadDll search path constraints, and (b) pre-validate
     *          DLLs so we can avoid calling WinVerifyTrust from the first hook,
     *          and thus avoiding messing up the loader data on some installations.
     *
     * This differs from the above function in that is no a system call and
     * we're at the mercy of the compiler.
     */
    uint8_t * const pbLdrLoadDll = (uint8_t *)(uintptr_t)pfnLdrLoadDll;
    uint32_t        offExecPage  = 0;
    memset(g_abSupHardReadWriteExecPage, 0xcc, PAGE_SIZE);

#ifdef RT_ARCH_AMD64
    /*
     * Patch 64-bit hosts.
     */
# if 0
    /* Pattern #1:
         Windows 8.1:
            0:000> u ntdll!LdrLoadDll
            ntdll!LdrLoadDll:
            00007ffa`814ccd44 488bc4          mov     rax,rsp
            00007ffa`814ccd47 48895808        mov     qword ptr [rax+8],rbx
            00007ffa`814ccd4b 48896810        mov     qword ptr [rax+10h],rbp
            00007ffa`814ccd4f 48897018        mov     qword ptr [rax+18h],rsi
            00007ffa`814ccd53 48897820        mov     qword ptr [rax+20h],rdi
            00007ffa`814ccd57 4156            push    r14
            00007ffa`814ccd59 4883ec70        sub     rsp,70h
            00007ffa`814ccd5d f6059cd2100009  test    byte ptr [ntdll!LdrpDebugFlags (00007ffa`815da000)],9
     */
    if (   pbLdrLoadDll[0] == 0x48 /* mov rax,rsp */
        && pbLdrLoadDll[1] == 0x8b
        && pbLdrLoadDll[2] == 0xc4
        && pbLdrLoadDll[3] == 0x48 /* mov qword ptr [rax+8],rbx */
        && pbLdrLoadDll[4] == 0x89
        && pbLdrLoadDll[5] == 0x58
        && pbLdrLoadDll[6] == 0x08)
    {
        offJmpBack = 7; /* the 3rd instruction. */
        pfnCallReal = supR3HardenedJmpBack_LdrLoadDll_Type1;
    }
    /*
       Pattern #2:
         Windows 8.0:
            0:000> u ntdll_w8_64!LdrLoadDll
            ntdll_w8_64!LdrLoadDll:
            00007ffa`52ffa7c0 48895c2408      mov     qword ptr [rsp+8],rbx
            00007ffa`52ffa7c5 4889742410      mov     qword ptr [rsp+10h],rsi
            00007ffa`52ffa7ca 48897c2418      mov     qword ptr [rsp+18h],rdi
            00007ffa`52ffa7cf 55              push    rbp
            00007ffa`52ffa7d0 4156            push    r14
            00007ffa`52ffa7d2 4157            push    r15
            00007ffa`52ffa7d4 488bec          mov     rbp,rsp
            00007ffa`52ffa7d7 4883ec60        sub     rsp,60h
            00007ffa`52ffa7db 8b05df321000    mov     eax,dword ptr [ntdll_w8_64!LdrpDebugFlags (00007ffa`530fdac0)]
            00007ffa`52ffa7e1 4d8bf1          mov     r14,r9

     */
    else if (   pbLdrLoadDll[0] == 0x48 /* mov qword ptr [rsp+8],rbx */
             && pbLdrLoadDll[1] == 0x89
             && pbLdrLoadDll[2] == 0x5c
             && pbLdrLoadDll[3] == 0x24
             && pbLdrLoadDll[4] == 0x08
             && pbLdrLoadDll[5] == 0x48 /* mov qword ptr [rsp+10h],rsi */
             && pbLdrLoadDll[6] == 0x89
             && pbLdrLoadDll[7] == 0x74
             && pbLdrLoadDll[8] == 0x24
             && pbLdrLoadDll[9] == 0x10)
    {
        offJmpBack = 10; /* the 3rd instruction. */
        pfnCallReal = supR3HardenedJmpBack_LdrLoadDll_Type2;
    }
    /*
       Pattern #3:
         Windows 7:
            ntdll_w7_64!LdrLoadDll:
            00000000`58be4a20 48895c2410      mov     qword ptr [rsp+10h],rbx
            00000000`58be4a25 48896c2418      mov     qword ptr [rsp+18h],rbp
            00000000`58be4a2a 56              push    rsi
            00000000`58be4a2b 57              push    rdi
            00000000`58be4a2c 4154            push    r12
            00000000`58be4a2e 4883ec50        sub     rsp,50h
            00000000`58be4a32 f605976e100009  test    byte ptr [ntdll_w7_64!ShowSnaps (00000000`58ceb8d0)],9
            00000000`58be4a39 498bf1          mov     rsi,r9

     */
    else if (   pbLdrLoadDll[0] == 0x48 /* mov qword ptr [rsp+10h],rbx */
             && pbLdrLoadDll[1] == 0x89
             && pbLdrLoadDll[2] == 0x5c
             && pbLdrLoadDll[3] == 0x24
             && pbLdrLoadDll[4] == 0x10)
    {
        offJmpBack = 5; /* the 2nd instruction. */
        pfnCallReal = supR3HardenedJmpBack_LdrLoadDll_Type3;
    }
    /*
       Pattern #4:
         Windows Vista:
            0:000> u ntdll_vista_64!LdrLoadDll
            ntdll_vista_64!LdrLoadDll:
            00000000`58c11f60 fff3            push    rbx
            00000000`58c11f62 56              push    rsi
            00000000`58c11f63 57              push    rdi
            00000000`58c11f64 4154            push    r12
            00000000`58c11f66 4155            push    r13
            00000000`58c11f68 4156            push    r14
            00000000`58c11f6a 4157            push    r15
            00000000`58c11f6c 4881ecb0020000  sub     rsp,2B0h
            00000000`58c11f73 488b05367b0e00  mov     rax,qword ptr [ntdll_vista_64!_security_cookie (00000000`58cf9ab0)]
            00000000`58c11f7a 4833c4          xor     rax,rsp
            00000000`58c11f7d 48898424a0020000 mov     qword ptr [rsp+2A0h],rax

     */
    else if (   pbLdrLoadDll[0] == 0xff /* push rbx */
             && pbLdrLoadDll[1] == 0xf3
             && pbLdrLoadDll[2] == 0x56 /* push rsi */
             && pbLdrLoadDll[3] == 0x57 /* push rdi */
             && pbLdrLoadDll[4] == 0x41 /* push r12 */
             && pbLdrLoadDll[5] == 0x54)
    {
        offJmpBack = 6; /* the 5th instruction. */
        pfnCallReal = supR3HardenedJmpBack_LdrLoadDll_Type4;
    }
    /*
       Pattern #5:
         Windows XP64:
            0:000> u ntdll!LdrLoadDll
            ntdll!LdrLoadDll:
            00000000`78efa580 4c8bdc          mov     r11,rsp
            00000000`78efa583 4881ece8020000  sub     rsp,2E8h
            00000000`78efa58a 49895bf8        mov     qword ptr [r11-8],rbx
            00000000`78efa58e 498973f0        mov     qword ptr [r11-10h],rsi
            00000000`78efa592 49897be8        mov     qword ptr [r11-18h],rdi
            00000000`78efa596 4d8963e0        mov     qword ptr [r11-20h],r12
            00000000`78efa59a 4d896bd8        mov     qword ptr [r11-28h],r13
            00000000`78efa59e 4d8973d0        mov     qword ptr [r11-30h],r14
            00000000`78efa5a2 4d897bc8        mov     qword ptr [r11-38h],r15
            00000000`78efa5a6 488b051bd10a00  mov     rax,qword ptr [ntdll!_security_cookie (00000000`78fa76c8)]
            00000000`78efa5ad 48898424a0020000 mov     qword ptr [rsp+2A0h],rax
            00000000`78efa5b5 4d8bf9          mov     r15,r9
            00000000`78efa5b8 4c8bf2          mov     r14,rdx
            00000000`78efa5bb 4c8be9          mov     r13,rcx
            00000000`78efa5be 4c89442458      mov     qword ptr [rsp+58h],r8
            00000000`78efa5c3 66c74424680000  mov     word ptr [rsp+68h],0

     */
    else if (   pbLdrLoadDll[0] == 0x4c /* mov r11,rsp */
             && pbLdrLoadDll[1] == 0x8b
             && pbLdrLoadDll[2] == 0xdc
             && pbLdrLoadDll[3] == 0x48 /* sub rsp,2e8h */
             && pbLdrLoadDll[4] == 0x81
             && pbLdrLoadDll[5] == 0xec
             && pbLdrLoadDll[6] == 0xe8
             && pbLdrLoadDll[7] == 0x02
             && pbLdrLoadDll[8] == 0x00
             && pbLdrLoadDll[9] == 0x00)
    {
        offJmpBack = 10; /* the 3rd instruction. */
        pfnCallReal = supR3HardenedJmpBack_LdrLoadDll_Type5;
    }
    else
        supR3HardenedWinHookFailed("LdrLoadDll", pbLdrLoadDll);
# else
    /* Just use the disassembler to skip 6 bytes or more. */
    DISSTATE Dis;
    uint32_t cbInstr;
    offJmpBack = 0;
    while (offJmpBack < 6)
    {
        cbInstr = 1;
        int rc = DISInstr(pbLdrLoadDll + offJmpBack, DISCPUMODE_64BIT, &Dis, &cbInstr);
        if (   RT_FAILURE(rc)
            || (Dis.pCurInstr->fOpType & (DISOPTYPE_CONTROLFLOW))
            || (Dis.ModRM.Bits.Mod == 0 && Dis.ModRM.Bits.Rm == 5 /* wrt RIP */) )
            supR3HardenedWinHookFailed("LdrLoadDll", pbLdrLoadDll);
        offJmpBack += cbInstr;
    }
# endif

    /* Assemble the code for resuming the call.*/
    *(PFNRT *)&g_pfnLdrLoadDllReal = (PFNRT)(uintptr_t)&g_abSupHardReadWriteExecPage[offExecPage];

    memcpy(&g_abSupHardReadWriteExecPage[offExecPage], pbLdrLoadDll, offJmpBack);
    offExecPage += offJmpBack;

    g_abSupHardReadWriteExecPage[offExecPage++] = 0xff; /* jmp qword [$+8 wrt RIP] */
    g_abSupHardReadWriteExecPage[offExecPage++] = 0x25;
    *(uint32_t *)&g_abSupHardReadWriteExecPage[offExecPage] = RT_ALIGN_32(offExecPage + 4, 8) - (offExecPage + 4);
    offExecPage = RT_ALIGN_32(offExecPage + 4, 8);
    *(uint64_t *)&g_abSupHardReadWriteExecPage[offExecPage] = (uintptr_t)&pbLdrLoadDll[offJmpBack];
    offExecPage = RT_ALIGN_32(offJmpBack + 8, 16);

    /* Patch the function. */
    *puJmpTab = (uintptr_t)supR3HardenedMonitor_LdrLoadDll;

    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pbLdrLoadDll, 16, PAGE_EXECUTE_READWRITE));

    Assert(offJmpBack >= 6);
    pbLdrLoadDll[0] = 0xff;
    pbLdrLoadDll[1] = 0x25;
    *(uint32_t *)&pbLdrLoadDll[2] = (uint32_t)((uintptr_t)puJmpTab - (uintptr_t)&pbLdrLoadDll[2+4]);

    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pbLdrLoadDll, 16, PAGE_EXECUTE_READ));
    puJmpTab++;

#else
    /*
     * Patch 32-bit hosts.
     */
# if 0
    /* Pattern #1:
          Windows 7:
            0:000> u ntdll!LdrLoadDll
            ntdll!LdrLoadDll:
            77aff585 8bff            mov     edi,edi
            77aff587 55              push    ebp
            77aff588 8bec            mov     ebp,esp
            77aff58a 51              push    ecx
            77aff58b 51              push    ecx
            77aff58c a1f8bdaf77      mov     eax,dword ptr [ntdll!LdrpLogLevelStateTable+0x24 (77afbdf8)]

          Windows 8 rtm:
            0:000:x86> u ntdll_67150000!LdrLoadDll
            ntdll_67150000!LdrLoadDll:
            67189f3f 8bff            mov     edi,edi
            67189f41 55              push    ebp
            67189f42 8bec            mov     ebp,esp
            67189f44 8b0d10eb2467    mov     ecx,dword ptr [ntdll_67150000!LdrpDebugFlags (6724eb10)]

          Windows 8.1:
            0:000:x86> u ntdll_w81_32!LdrLoadDll
            ntdll_w81_32!LdrLoadDll:
            6718aade 8bff            mov     edi,edi
            6718aae0 55              push    ebp
            6718aae1 8bec            mov     ebp,esp
            6718aae3 83ec14          sub     esp,14h
            6718aae6 f6050040246709  test    byte ptr [ntdll_w81_32!LdrpDebugFlags (67244000)],9

       Pattern #2:
          Windows XP:
            0:000:x86> u ntdll_xp!LdrLoadDll
            ntdll_xp!LdrLoadDll:
            77f569d2 6858020000      push    258h
            77f569d7 68d866f777      push    offset ntdll_xp!`string'+0x12c (77f766d8)
            77f569dc e83bb20200      call    ntdll_xp!_SEH_prolog (77f81c1c)
            77f569e1 33db            xor     ebx,ebx
            77f569e3 66895de0        mov     word ptr [ebp-20h],bx
            77f569e7 33c0            xor     eax,eax
            77f569e9 8d7de2          lea     edi,[ebp-1Eh]
            77f569ec ab              stos    dword ptr es:[edi]

          Windows Server 2003:
            0:000:x86> u ntdll_w2k3_32!LdrLoadDll
            ntdll_w2k3_32!LdrLoadDll:
            7c833f63 6840020000      push    240h
            7c833f68 68b040837c      push    offset ntdll_w2k3_32!`string'+0x12c (7c8340b0)
            7c833f6d e8a942ffff      call    ntdll_w2k3_32!_SEH_prolog (7c82821b)
            7c833f72 a13077887c      mov     eax,dword ptr [ntdll_w2k3_32!__security_cookie (7c887730)]
            7c833f77 8945e4          mov     dword ptr [ebp-1Ch],eax
            7c833f7a 8b4508          mov     eax,dword ptr [ebp+8]
            7c833f7d 8985b0fdffff    mov     dword ptr [ebp-250h],eax
            7c833f83 8b450c          mov     eax,dword ptr [ebp+0Ch]

          Windows Vista SP0 & SP1:
            0:000:x86> u ntdll_vista_sp0_32!LdrLoadDll
            ntdll_vista_sp0_32!LdrLoadDll:
            69b0eb00 6844020000      push    244h
            69b0eb05 6838e9b269      push    offset ntdll_vista_sp0_32! ?? ::FNODOBFM::`string'+0x39e (69b2e938)
            69b0eb0a e835420300      call    ntdll_vista_sp0_32!_SEH_prolog4_GS (69b42d44)
            69b0eb0f 8b4508          mov     eax,dword ptr [ebp+8]
            69b0eb12 8985acfdffff    mov     dword ptr [ebp-254h],eax
            69b0eb18 8b450c          mov     eax,dword ptr [ebp+0Ch]
            69b0eb1b 8985c0fdffff    mov     dword ptr [ebp-240h],eax
            69b0eb21 8b4510          mov     eax,dword ptr [ebp+10h]
         */

    if (   pbLdrLoadDll[0] == 0x8b /* mov edi, edi - for hot patching */
        && pbLdrLoadDll[1] == 0xff
        && pbLdrLoadDll[2] == 0x55 /* push ebp */
        && pbLdrLoadDll[3] == 0x8b /* mov  ebp,esp */
        && pbLdrLoadDll[4] == 0xec)
    {
        offJmpBack = 5; /* the 3rd instruction. */
        pfnCallReal = supR3HardenedJmpBack_LdrLoadDll_Type1;
    }
    else if (pbLdrLoadDll[0] == 0x68 /* push dword XXXXXXXX */)
    {
        offJmpBack = 5;
        pfnCallReal = supR3HardenedJmpBack_LdrLoadDll_Type2;
        g_supR3HardenedJmpBack_LdrLoadDll_Type2_PushDword = *(uint32_t const *)&pbLdrLoadDll[1];
    }
    else
        supR3HardenedWinHookFailed("LdrLoadDll", pbLdrLoadDll);

    g_pfnLdrLoadDllJmpBack = (PFNRT)(uintptr_t)(pbLdrLoadDll + offJmpBack);
    *(PFNRT *)&g_pfnLdrLoadDllReal = pfnCallReal;

# else
    /* Just use the disassembler to skip 6 bytes or more. */
    DISSTATE Dis;
    uint32_t cbInstr;
    offJmpBack = 0;
    while (offJmpBack < 5)
    {
        cbInstr = 1;
        int rc = DISInstr(pbLdrLoadDll + offJmpBack, DISCPUMODE_32BIT, &Dis, &cbInstr);
        if (   RT_FAILURE(rc)
            || (Dis.pCurInstr->fOpType & (DISOPTYPE_CONTROLFLOW)) )
            supR3HardenedWinHookFailed("LdrLoadDll", pbLdrLoadDll);
        offJmpBack += cbInstr;
    }

    /* Assemble the code for resuming the call.*/
    *(PFNRT *)&g_pfnLdrLoadDllReal = (PFNRT)(uintptr_t)&g_abSupHardReadWriteExecPage[offExecPage];

    memcpy(&g_abSupHardReadWriteExecPage[offExecPage], pbLdrLoadDll, offJmpBack);
    offExecPage += offJmpBack;

    g_abSupHardReadWriteExecPage[offExecPage++] = 0xe9;
    *(uint32_t *)&g_abSupHardReadWriteExecPage[offExecPage] = (uintptr_t)&pbLdrLoadDll[offJmpBack]
                                                            - (uintptr_t)&g_abSupHardReadWriteExecPage[offExecPage + 4];
    offExecPage = RT_ALIGN_32(offJmpBack + 4, 16);

# endif

    /* Patch LdrLoadDLl. */
    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pbLdrLoadDll, 16, PAGE_EXECUTE_READWRITE));
    Assert(offJmpBack >= 5);
    pbLdrLoadDll[0] = 0xe9;
    *(uint32_t *)&pbLdrLoadDll[1] = (uintptr_t)supR3HardenedMonitor_LdrLoadDll - (uintptr_t)&pbLdrLoadDll[1+4];

    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pbLdrLoadDll, 16, PAGE_EXECUTE_READ));
#endif

    /*
     * Seal the rwx page.
     */
    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(g_abSupHardReadWriteExecPage, PAGE_SIZE, PAGE_EXECUTE_READ));
}


/**
 * Verifies the process integrity.
 */
DECLHIDDEN(void) supR3HardenedWinVerifyProcess(void)
{
    RTErrInfoInitStatic(&g_ErrInfoStatic);
    int rc = supHardenedWinVerifyProcess(NtCurrentProcess(), NtCurrentThread(),
                                         SUPHARDNTVPKIND_VERIFY_ONLY, NULL /*pcFixes*/, &g_ErrInfoStatic.Core);
    if (RT_FAILURE(rc))
        supR3HardenedFatalMsg("supR3HardenedWinVerifyProcess", kSupInitOp_Integrity, rc,
                              "Failed to verify process integrity: %s", g_ErrInfoStatic.szMsg);
}


/**
 * Gets the SID of the user associated with the process.
 *
 * @returns @c true if we've got a login SID, @c false if not.
 * @param   pSidUser            Where to return the user SID.
 * @param   cbSidUser           The size of the user SID buffer.
 * @param   pSidLogin           Where to return the login SID.
 * @param   cbSidLogin          The size of the login SID buffer.
 */
static bool supR3HardenedGetUserAndLogSids(PSID pSidUser, ULONG cbSidUser, PSID pSidLogin, ULONG cbSidLogin)
{
    HANDLE hToken;
    SUPR3HARDENED_ASSERT_NT_SUCCESS(NtOpenProcessToken(NtCurrentProcess(), TOKEN_QUERY, &hToken));
    union
    {
        TOKEN_USER      UserInfo;
        TOKEN_GROUPS    Groups;
        uint8_t         abPadding[4096];
    } uBuf;
    ULONG cbRet = 0;
    SUPR3HARDENED_ASSERT_NT_SUCCESS(NtQueryInformationToken(hToken, TokenUser, &uBuf, sizeof(uBuf), &cbRet));
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlCopySid(cbSidUser, pSidUser, uBuf.UserInfo.User.Sid));

    bool fLoginSid = false;
    NTSTATUS rcNt = NtQueryInformationToken(hToken, TokenLogonSid, &uBuf, sizeof(uBuf), &cbRet);
    if (NT_SUCCESS(rcNt))
    {
        for (DWORD i = 0; i < uBuf.Groups.GroupCount; i++)
            if ((uBuf.Groups.Groups[i].Attributes & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID)
            {
                SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlCopySid(cbSidLogin, pSidLogin, uBuf.Groups.Groups[i].Sid));
                fLoginSid = true;
                break;
            }
    }

    SUPR3HARDENED_ASSERT_NT_SUCCESS(NtClose(hToken));

    return fLoginSid;
}


/**
 * Build security attributes for the process or the primary thread (@a fProcess)
 *
 * Process DACLs can be bypassed using the SeDebugPrivilege (generally available
 * to admins, i.e. normal windows users), or by taking ownership and/or
 * modifying the DACL.  However, it restricts
 *
 * @param   pSecAttrs           Where to return the security attributes.
 * @param   pCleanup            Cleanup record.
 * @param   fProcess            Set if it's for the process, clear if it's for
 *                              the primary thread.
 */
static void supR3HardenedInitSecAttrs(PSECURITY_ATTRIBUTES pSecAttrs, PMYSECURITYCLEANUP pCleanup, bool fProcess)
{
    /*
     * Safe return values.
     */
    suplibHardenedMemSet(pCleanup, 0, sizeof(*pCleanup));

    pSecAttrs->nLength              = sizeof(*pSecAttrs);
    pSecAttrs->bInheritHandle       = FALSE;
    pSecAttrs->lpSecurityDescriptor = NULL;

/** @todo This isn't at all complete, just sketches... */

    /*
     * Create an ACL detailing the access of the above groups.
     */
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlCreateAcl(&pCleanup->Acl.AclHdr, sizeof(pCleanup->Acl), ACL_REVISION));

    ULONG fDeny  = DELETE | WRITE_DAC | WRITE_OWNER;
    ULONG fAllow = SYNCHRONIZE | READ_CONTROL;
    ULONG fAllowLogin = SYNCHRONIZE | READ_CONTROL;
    if (fProcess)
    {
        fDeny       |= PROCESS_CREATE_THREAD | PROCESS_SET_SESSIONID | PROCESS_VM_OPERATION | PROCESS_VM_WRITE
                    |  PROCESS_CREATE_PROCESS | PROCESS_DUP_HANDLE | PROCESS_SET_QUOTA
                    |  PROCESS_SET_INFORMATION | PROCESS_SUSPEND_RESUME;
        fAllow      |= PROCESS_TERMINATE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION;
        fAllowLogin |= PROCESS_TERMINATE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION;
        if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0)) /* Introduced in Vista. */
        {
            fAllow      |= PROCESS_QUERY_LIMITED_INFORMATION;
            fAllowLogin |= PROCESS_QUERY_LIMITED_INFORMATION;
        }
        if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 3)) /* Introduced in Windows 8.1. */
            fAllow  |= PROCESS_SET_LIMITED_INFORMATION;
    }
    else
    {
        fDeny       |= THREAD_SUSPEND_RESUME | THREAD_SET_CONTEXT | THREAD_SET_INFORMATION | THREAD_SET_THREAD_TOKEN
                    |  THREAD_IMPERSONATE | THREAD_DIRECT_IMPERSONATION;
        fAllow      |= THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION;
        fAllowLogin |= THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION;
        if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0)) /* Introduced in Vista. */
        {
            fAllow      |= THREAD_QUERY_LIMITED_INFORMATION | THREAD_SET_LIMITED_INFORMATION;
            fAllowLogin |= THREAD_QUERY_LIMITED_INFORMATION;
        }

    }
    fDeny |= ~fAllow & (SPECIFIC_RIGHTS_ALL | STANDARD_RIGHTS_ALL);

    /* Deny everyone access to bad bits. */
#if 1
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlInitializeSid(&pCleanup->Everyone.Sid, &SIDAuthWorld, 1));
    *RtlSubAuthoritySid(&pCleanup->Everyone.Sid, 0) = SECURITY_WORLD_RID;
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlAddAccessDeniedAce(&pCleanup->Acl.AclHdr, ACL_REVISION,
                                                          fDeny, &pCleanup->Everyone.Sid));
#endif

#if 0
    /* Grant some access to the owner - doesn't work. */
    SID_IDENTIFIER_AUTHORITY SIDAuthCreator = SECURITY_CREATOR_SID_AUTHORITY;
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlInitializeSid(&pCleanup->Owner.Sid, &SIDAuthCreator, 1));
    *RtlSubAuthoritySid(&pCleanup->Owner.Sid, 0) = SECURITY_CREATOR_OWNER_RID;

    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlAddAccessDeniedAce(&pCleanup->Acl.AclHdr, ACL_REVISION,
                                                          fDeny, &pCleanup->Owner.Sid));
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlAddAccessAllowedAce(&pCleanup->Acl.AclHdr, ACL_REVISION,
                                                           fAllow, &pCleanup->Owner.Sid));
#endif

#if 1
    bool fHasLoginSid = supR3HardenedGetUserAndLogSids(&pCleanup->User.Sid, sizeof(pCleanup->User),
                                                       &pCleanup->Login.Sid, sizeof(pCleanup->Login));

# if 1
    /* Grant minimal access to the user. */
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlAddAccessDeniedAce(&pCleanup->Acl.AclHdr, ACL_REVISION,
                                                          fDeny, &pCleanup->User.Sid));
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlAddAccessAllowedAce(&pCleanup->Acl.AclHdr, ACL_REVISION,
                                                           fAllow, &pCleanup->User.Sid));
# endif

# if 1
    /* Grant very limited access to the login sid. */
    if (fHasLoginSid)
    {
        SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlAddAccessAllowedAce(&pCleanup->Acl.AclHdr, ACL_REVISION,
                                                               fAllowLogin, &pCleanup->Login.Sid));
    }
# endif

#endif

    /*
     * Create a security descriptor with the above ACL.
     */
    PSECURITY_DESCRIPTOR pSecDesc = (PSECURITY_DESCRIPTOR)RTMemAllocZ(SECURITY_DESCRIPTOR_MIN_LENGTH);
    pCleanup->pSecDesc = pSecDesc;

    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlCreateSecurityDescriptor(pSecDesc, SECURITY_DESCRIPTOR_REVISION));
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlSetDaclSecurityDescriptor(pSecDesc, TRUE /*fDaclPresent*/, &pCleanup->Acl.AclHdr,
                                                                 FALSE /*fDaclDefaulted*/));
    pSecAttrs->lpSecurityDescriptor = pSecDesc;
}


/**
 * Predicate function which tests whether @a ch is a argument separator
 * character.
 *
 * @returns True/false.
 * @param   ch                  The character to examine.
 */
DECLINLINE(bool) suplibCommandLineIsArgSeparator(int ch)
{
    return ch == ' '
        || ch == '\t'
        || ch == '\n'
        || ch == '\r';
}


/**
 * Construct the new command line.
 *
 * Since argc/argv are both derived from GetCommandLineW (see
 * suplibHardenedWindowsMain), we skip the argument by argument UTF-8 -> UTF-16
 * conversion and quoting by going to the original source.
 *
 * The executable name, though, is replaced in case it's not a fullly
 * qualified path.
 *
 * The re-spawn indicator is added immediately after the executable name
 * so that we don't get tripped up missing close quote chars in the last
 * argument.
 *
 * @returns Pointer to a command line string (heap).
 * @param   pUniStr         Unicode string structure to initialize to the
 *                          command line. Optional.
 * @param   iWhich          Which respawn we're to check for, 1 being the first
 *                          one, and 2 the second and final.
 */
static PRTUTF16 supR3HardenedWinConstructCmdLine(PUNICODE_STRING pString, int iWhich)
{
    SUPR3HARDENED_ASSERT(iWhich == 1 || iWhich == 2);

    /*
     * Get the command line and skip the executable name.
     */
    PUNICODE_STRING pCmdLineStr = &NtCurrentPeb()->ProcessParameters->CommandLine;
    PCRTUTF16 pawcArgs = pCmdLineStr->Buffer;
    uint32_t  cwcArgs  = pCmdLineStr->Length / sizeof(WCHAR);

    /* Skip leading space (shouldn't be any, but whatever). */
    while (cwcArgs > 0 && suplibCommandLineIsArgSeparator(*pawcArgs) )
        cwcArgs--, pawcArgs++;
    SUPR3HARDENED_ASSERT(cwcArgs > 0 && *pawcArgs != '\0');

    /* Walk to the end of it. */
    int fQuoted = false;
    do
    {
        if (*pawcArgs == '"')
        {
            fQuoted = !fQuoted;
            cwcArgs--; pawcArgs++;
        }
        else if (*pawcArgs != '\\' || (pawcArgs[1] != '\\' && pawcArgs[1] != '"'))
            cwcArgs--, pawcArgs++;
        else
        {
            unsigned cSlashes = 0;
            do
            {
                cSlashes++;
                cwcArgs--;
                pawcArgs++;
            }
            while (cwcArgs > 0 && *pawcArgs == '\\');
            if (cwcArgs > 0 && *pawcArgs == '"' && (cSlashes & 1))
                cwcArgs--, pawcArgs++; /* odd number of slashes == escaped quote */
        }
    } while (cwcArgs > 0 && (fQuoted || !suplibCommandLineIsArgSeparator(*pawcArgs)));

    /* Skip trailing spaces. */
    while (cwcArgs > 0 && suplibCommandLineIsArgSeparator(*pawcArgs))
        cwcArgs--, pawcArgs++;

    /*
     * Allocate a new buffer.
     */
    AssertCompile(sizeof(SUPR3_RESPAWN_1_ARG0) == sizeof(SUPR3_RESPAWN_2_ARG0));
    size_t cwcCmdLine = (sizeof(SUPR3_RESPAWN_1_ARG0) - 1) / sizeof(SUPR3_RESPAWN_1_ARG0[0]) /* Respawn exe name. */
                      + !!cwcArgs + cwcArgs; /* if arguments present, add space + arguments. */
    if (cwcCmdLine * sizeof(WCHAR) >= 0xfff0)
        supR3HardenedFatalMsg("supR3HardenedWinConstructCmdLine", kSupInitOp_Misc, VERR_OUT_OF_RANGE,
                              "Command line is too long (%u chars)!", cwcCmdLine);

    PRTUTF16 pwszCmdLine = (PRTUTF16)RTMemAlloc((cwcCmdLine + 1) * sizeof(RTUTF16));
    SUPR3HARDENED_ASSERT(pwszCmdLine != NULL);

    /*
     * Construct the new command line.
     */
    PRTUTF16 pwszDst = pwszCmdLine;
    for (const char *pszSrc = iWhich == 1 ? SUPR3_RESPAWN_1_ARG0 : SUPR3_RESPAWN_2_ARG0; *pszSrc; pszSrc++)
        *pwszDst++ = *pszSrc;

    if (cwcArgs)
    {
        *pwszDst++ = ' ';
        suplibHardenedMemCopy(pwszDst, pawcArgs, cwcArgs * sizeof(RTUTF16));
        pwszDst += cwcArgs;
    }

    *pwszDst = '\0';
    SUPR3HARDENED_ASSERT(pwszDst - pwszCmdLine == cwcCmdLine);

    if (pString)
    {
        pString->Buffer = pwszCmdLine;
        pString->Length = (USHORT)(cwcCmdLine * sizeof(WCHAR));
        pString->MaximumLength = pString->Length + sizeof(WCHAR);
    }
    return pwszCmdLine;
}


/**
 * Check if the zero terminated NT unicode string is the path to the given
 * system32 DLL.
 *
 * @returns true if it is, false if not.
 * @param   pUniStr             The zero terminated NT unicode string path.
 * @param   pszName             The name of the system32 DLL.
 */
static bool supR3HardNtIsNamedSystem32Dll(PUNICODE_STRING pUniStr, const char *pszName)
{
    if (pUniStr->Length > g_System32NtPath.UniStr.Length)
    {
        if (memcmp(pUniStr->Buffer, g_System32NtPath.UniStr.Buffer, g_System32NtPath.UniStr.Length) == 0)
        {
            if (pUniStr->Buffer[g_System32NtPath.UniStr.Length / sizeof(WCHAR)] == '\\')
            {
                if (RTUtf16ICmpAscii(&pUniStr->Buffer[g_System32NtPath.UniStr.Length / sizeof(WCHAR) + 1], pszName) == 0)
                    return true;
            }
        }
    }

    return false;
}


/**
 * Common code used for child and parent to make new threads exit immediately.
 *
 * This patches the LdrInitializeThunk code to call NtTerminateThread with
 * STATUS_SUCCESS instead of doing the NTDLL initialization.
 *
 * @returns VBox status code.
 * @param   hProcess            The process to do this to.
 * @param   pvLdrInitThunk      The address of the LdrInitializeThunk code to
 *                              override.
 * @param   pvNtTerminateThread The address of the NtTerminateThread function in
 *                              the NTDLL instance we're patching.  (Must be +/-
 *                              2GB from the thunk code.)
 * @param   pabBackup           Where to back up the original instruction bytes
 *                              at pvLdrInitThunk.
 * @param   cbBackup            The size of the backup area. Must be 16 bytes.
 * @param   pErrInfo            Where to return extended error information.
 *                              Optional.
 */
static int supR3HardNtDisableThreadCreationEx(HANDLE hProcess, void *pvLdrInitThunk, void *pvNtTerminateThread,
                                              uint8_t *pabBackup, size_t cbBackup, PRTERRINFO pErrInfo)
{
    SUP_DPRINTF(("supR3HardNtDisableThreadCreation: pvLdrInitThunk=%p pvNtTerminateThread=%p\n", pvLdrInitThunk, pvNtTerminateThread));
    SUPR3HARDENED_ASSERT(cbBackup == 16);
    SUPR3HARDENED_ASSERT(RT_ABS((intptr_t)pvLdrInitThunk - (intptr_t)pvNtTerminateThread) < 16*_1M);

    /*
     * Back up the thunk code.
     */
    SIZE_T  cbIgnored;
    NTSTATUS rcNt = NtReadVirtualMemory(hProcess, pvLdrInitThunk, pabBackup, cbBackup, &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE,
                             "supR3HardNtDisableThreadCreation: NtReadVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    /*
     * Cook up replacement code that calls NtTerminateThread.
     */
    uint8_t abReplacement[16];
    memcpy(abReplacement, pabBackup, sizeof(abReplacement));

#ifdef RT_ARCH_AMD64
    abReplacement[0] = 0x31;    /* xor ecx, ecx */
    abReplacement[1] = 0xc9;
    abReplacement[2] = 0x31;    /* xor edx, edx */
    abReplacement[3] = 0xd2;
    abReplacement[4] = 0xe8;    /* call near NtTerminateThread */
    *(int32_t *)&abReplacement[5] = (int32_t)((uintptr_t)pvNtTerminateThread - ((uintptr_t)pvLdrInitThunk + 9));
    abReplacement[9] = 0xcc;    /* int3 */
#elif defined(RT_ARCH_X86)
    abReplacement[0] = 0x6a;    /* push 0 */
    abReplacement[1] = 0x00;
    abReplacement[2] = 0x6a;    /* push 0 */
    abReplacement[3] = 0x00;
    abReplacement[4] = 0xe8;    /* call near NtTerminateThread */
    *(int32_t *)&abReplacement[5] = (int32_t)((uintptr_t)pvNtTerminateThread - ((uintptr_t)pvLdrInitThunk + 9));
    abReplacement[9] = 0xcc;    /* int3 */
#else
# error "Unsupported arch."
#endif

    /*
     * Install the replacment code.
     */
    PVOID  pvProt   = pvLdrInitThunk;
    SIZE_T cbProt   = cbBackup;
    ULONG  fOldProt = 0;
    rcNt = NtProtectVirtualMemory(hProcess, &pvProt, &cbProt, PAGE_EXECUTE_READWRITE, &fOldProt);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE,
                             "supR3HardNtDisableThreadCreationEx: NtProtectVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    rcNt = NtWriteVirtualMemory(hProcess, pvLdrInitThunk, abReplacement, sizeof(abReplacement), &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE,
                             "supR3HardNtDisableThreadCreationEx: NtWriteVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    pvProt   = pvLdrInitThunk;
    cbProt   = cbBackup;
    rcNt = NtProtectVirtualMemory(hProcess, &pvProt, &cbProt, fOldProt, &fOldProt);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE,
                             "supR3HardNtDisableThreadCreationEx: NtProtectVirtualMemory/LdrInitializeThunk/2 failed: %#x", rcNt);

    return VINF_SUCCESS;
}


/**
 * Undo the effects of supR3HardNtDisableThreadCreationEx.
 *
 * @returns VBox status code.
 * @param   hProcess            The process to do this to.
 * @param   pvLdrInitThunk      The address of the LdrInitializeThunk code to
 *                              override.
 * @param   pabBackup           Where to back up the original instruction bytes
 *                              at pvLdrInitThunk.
 * @param   cbBackup            The size of the backup area. Must be 16 bytes.
 * @param   pErrInfo            Where to return extended error information.
 *                              Optional.
 */
static int supR3HardNtEnableThreadCreationEx(HANDLE hProcess, void *pvLdrInitThunk, uint8_t const *pabBackup, size_t cbBackup,
                                             PRTERRINFO pErrInfo)
{
    SUP_DPRINTF(("supR3HardNtEnableThreadCreation:\n"));
    SUPR3HARDENED_ASSERT(cbBackup == 16);

    PVOID  pvProt   = pvLdrInitThunk;
    SIZE_T cbProt   = cbBackup;
    ULONG  fOldProt = 0;
    NTSTATUS rcNt = NtProtectVirtualMemory(hProcess, &pvProt, &cbProt, PAGE_EXECUTE_READWRITE, &fOldProt);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE,
                             "supR3HardNtDisableThreadCreationEx: NtProtectVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    SIZE_T cbIgnored;
    rcNt = NtWriteVirtualMemory(hProcess, pvLdrInitThunk, pabBackup, cbBackup, &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE,
                             "supR3HardNtEnableThreadCreation: NtWriteVirtualMemory/LdrInitializeThunk[restore] failed: %#x",
                             rcNt);

    pvProt   = pvLdrInitThunk;
    cbProt   = cbBackup;
    rcNt = NtProtectVirtualMemory(hProcess, &pvProt, &cbProt, fOldProt, &fOldProt);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE,
                             "supR3HardNtEnableThreadCreation: NtProtectVirtualMemory/LdrInitializeThunk[restore] failed: %#x",
                             rcNt);

    return VINF_SUCCESS;
}


/**
 * Disable thread creation for the current process.
 *
 * @remarks Doesn't really disables it, just makes the threads exit immediately
 *          without executing any real code.
 */
static void supR3HardenedWinDisableThreadCreation(void)
{
    /* Cannot use the imported NtTerminateThread as it's pointing to our own
       syscall assembly code. */
    static PFNRT s_pfnNtTerminateThread = NULL;
    if (s_pfnNtTerminateThread == NULL)
        s_pfnNtTerminateThread = supR3HardenedWinGetRealDllSymbol("ntdll.dll", "NtTerminateThread");
    SUPR3HARDENED_ASSERT(s_pfnNtTerminateThread);

    int rc = supR3HardNtDisableThreadCreationEx(NtCurrentProcess(),
                                                (void *)(uintptr_t)&LdrInitializeThunk,
                                                (void *)(uintptr_t)s_pfnNtTerminateThread,
                                                g_abLdrInitThunkSelfBackup, sizeof(g_abLdrInitThunkSelfBackup),
                                                NULL /* pErrInfo*/);
    g_fSupInitThunkSelfPatched = RT_SUCCESS(rc);
}


/**
 * Undoes the effects of supR3HardenedWinDisableThreadCreation.
 */
DECLHIDDEN(void) supR3HardenedWinEnableThreadCreation(void)
{
    if (g_fSupInitThunkSelfPatched)
    {
        int rc = supR3HardNtEnableThreadCreationEx(NtCurrentProcess(),
                                                   (void *)(uintptr_t)&LdrInitializeThunk,
                                                   g_abLdrInitThunkSelfBackup, sizeof(g_abLdrInitThunkSelfBackup),
                                                   RTErrInfoInitStatic(&g_ErrInfoStatic));
        if (RT_FAILURE(rc))
            supR3HardenedError(rc, true /*fFatal*/, "%s", g_ErrInfoStatic.szMsg);
        g_fSupInitThunkSelfPatched = false;
    }
}



/*
 * Child-Process Purification - release it from dubious influences. 
 *  
 * AV software and other things injecting themselves into the embryonic 
 * and budding process to intercept API calls and what not.  Unfortunately 
 * this is also the behavior of viruses, malware and other unfriendly 
 * software, so we won't stand for it.  AV software can scan our image 
 * as they are loaded via kernel hooks, that's sufficient.  No need for 
 * matching half of NTDLL or messing with the import table of the 
 * process executable. 
 */

typedef struct SUPR3HARDNTPUCH
{
    /** Process handle. */
    HANDLE                      hProcess;
    /** Primary thread handle. */
    HANDLE                      hThread;
    /** Error buffer. */
    PRTERRINFO                  pErrInfo;
    /** The address of NTDLL in the child. */
    uintptr_t                   uNtDllAddr;
    /** The address of NTDLL in this process. */
    uintptr_t                   uNtDllParentAddr;
    /** The basic process info. */
    PROCESS_BASIC_INFORMATION   BasicInfo;
    /** The probable size of the PEB. */
    size_t                      cbPeb;
    /** The pristine process environment block. */
    PEB                         Peb;
} SUPR3HARDNTPUCH;
typedef SUPR3HARDNTPUCH *PSUPR3HARDNTPUCH;


static int supR3HardNtPuChScrewUpPebForInitialImageEvents(PSUPR3HARDNTPUCH pThis)
{
    /*
     * Not sure if any of the cracker software uses the PEB at this point, but
     * just in case they do make some of the PEB fields a little less useful.
     */
    PEB Peb = pThis->Peb;

    /* Make ImageBaseAddress useless. */
    Peb.ImageBaseAddress = (PVOID)((uintptr_t)Peb.ImageBaseAddress ^ UINT32_C(0x5f139000));
#ifdef RT_ARCH_AMD64
    Peb.ImageBaseAddress = (PVOID)((uintptr_t)Peb.ImageBaseAddress | UINT64_C(0x0313000000000000));
#endif

    /*
     * Write the PEB.
     */
    SIZE_T cbActualMem = pThis->cbPeb;
    NTSTATUS rcNt = NtWriteVirtualMemory(pThis->hProcess, pThis->BasicInfo.PebBaseAddress, &Peb, pThis->cbPeb, &cbActualMem);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE, "NtWriteVirtualMemory/Peb failed: %#x", rcNt);
    return VINF_SUCCESS;
}


/**
 * Unmaps a DLL from the child process that was previously mapped by
 * supR3HardNtPuChMapDllIntoChild.
 *
 * @returns Pointer to the DLL mapping on success, NULL on failure.
 * @param   pThis               The child purification instance data.
 * @param   pvBase              The base address of the mapping.  Nothing done
 *                              if NULL.
 * @param   pszShort            The short name (for logging).
 */
static void supR3HardNtPuChUnmapDllFromChild(PSUPR3HARDNTPUCH pThis, PVOID pvBase, const char *pszShort)
{
    if (pvBase)
    {
        /*SUP_DPRINTF(("supR3HardNtPuChUnmapDllFromChild: Calling NtUnmapViewOfSection on %p / %s\n", pvBase, pszShort));*/
        NTSTATUS rcNt = NtUnmapViewOfSection(pThis->hProcess, pvBase);
        if (!NT_SUCCESS(!rcNt))
            SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: NtUnmapViewOfSection failed on %s: %#x (%p)\n",
                         pszShort, rcNt, pvBase));
    }
}


/**
 * Maps a DLL into the child process.
 *
 * @returns Pointer to the DLL mapping on success, NULL on failure.
 * @param   pThis               The child purification instance data.
 * @param   pNtName             The path to the DLL.
 * @param   pszShort            The short name (for logging).
 */
static PVOID supR3HardNtPuChMapDllIntoChild(PSUPR3HARDNTPUCH pThis, PUNICODE_STRING pNtName, const char *pszShort)
{
    HANDLE              hFile  = RTNT_INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK     Ios    = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    OBJECT_ATTRIBUTES   ObjAttr;
    InitializeObjectAttributes(&ObjAttr, pNtName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);
    NTSTATUS rcNt = NtCreateFile(&hFile,
                                 GENERIC_READ | GENERIC_EXECUTE,
                                 &ObjAttr,
                                 &Ios,
                                 NULL /* Allocation Size*/,
                                 FILE_ATTRIBUTE_NORMAL,
                                 FILE_SHARE_READ,
                                 FILE_OPEN,
                                 FILE_NON_DIRECTORY_FILE,
                                 NULL /*EaBuffer*/,
                                 0 /*EaLength*/);
    if (NT_SUCCESS(rcNt))
        rcNt = Ios.Status;
    PVOID pvRet = NULL;
    if (NT_SUCCESS(rcNt))
    {
        HANDLE hSection = RTNT_INVALID_HANDLE_VALUE;
        rcNt = NtCreateSection(&hSection,
                               SECTION_MAP_EXECUTE | SECTION_MAP_READ | SECTION_MAP_WRITE | SECTION_QUERY,
                               NULL /* pObjAttr*/, NULL /*pMaxSize*/,
                               PAGE_EXECUTE, SEC_IMAGE, hFile);
        if (NT_SUCCESS(rcNt))
        {
            SIZE_T cbView = 0;
            SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: mapping view of %s\n", pszShort)); /* For SEP. */
            rcNt = NtMapViewOfSection(hSection, pThis->hProcess, &pvRet, 0 /*ZeroBits*/, 0 /*CommitSize*/,
                                      NULL /*pOffSect*/, &cbView, ViewShare, 0 /*AllocationType*/, PAGE_READWRITE);
            if (NT_SUCCESS(rcNt))
                SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: %s mapped at %p LB %#x\n", pszShort, pvRet, cbView));
            else
            {
                SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: NtMapViewOfSection failed on %s: %#x\n", pszShort, rcNt));
                pvRet = NULL;
            }
            NtClose(hSection);
        }
        else
            SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: NtCreateSection failed on %s: %#x\n", pszShort, rcNt));
        NtClose(hFile);
    }
    else
        SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: Error opening %s: %#x\n", pszShort, rcNt));
    return pvRet;
}


/**
 * Trigger the initial image events without actually initializing the process.
 *
 * This is a trick to force sysplant.sys to call its hand by tripping the image
 * loaded event for the main executable and ntdll images.  This will happen when
 * the first thread in a process starts executing in PspUserThreadStartup.  We
 * create a second thread that quits immediately by means of temporarily
 * replacing ntdll!LdrInitializeThunk by a NtTerminateThread call.
 * (LdrInitializeThunk is called by way of an APC queued the thread is created,
 * thus NtSetContextThread is of no use.)
 *
 * @returns VBox status code.
 * @param   pThis               The child cleanup
 * @param   pErrInfo            For extended error information.
 */
static int supR3HardNtPuChTriggerInitialImageEvents(PSUPR3HARDNTPUCH pThis)
{
    /*
     * Use the on-disk image for the ntdll entrypoints here.
     */
    PSUPHNTLDRCACHEENTRY pLdrEntry;
    int rc = supHardNtLdrCacheOpen("ntdll.dll", &pLdrEntry);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "supHardNtLdrCacheOpen failed on NTDLL: %Rrc", rc);

    RTLDRADDR uLdrInitThunk;
    rc = RTLdrGetSymbolEx(pLdrEntry->hLdrMod, pLdrEntry->pbBits, pThis->uNtDllAddr, UINT32_MAX,
                          "LdrInitializeThunk", &uLdrInitThunk);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Error locating LdrInitializeThunk in NTDLL: %Rrc", rc);
    PVOID pvLdrInitThunk = (PVOID)(uintptr_t)uLdrInitThunk;

    RTLDRADDR uNtTerminateThread;
    rc = RTLdrGetSymbolEx(pLdrEntry->hLdrMod, pLdrEntry->pbBits, pThis->uNtDllAddr, UINT32_MAX,
                          "NtTerminateThread", &uNtTerminateThread);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pThis->pErrInfo, rc, "Error locating NtTerminateThread in NTDLL: %Rrc", rc);

    SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: uLdrInitThunk=%p uNtTerminateThread=%p\n",
                 (uintptr_t)uLdrInitThunk, (uintptr_t)uNtTerminateThread));

    /*
     * Patch the child's LdrInitializeThunk to exit the thread immediately.
     */
    uint8_t abBackup[16];
    rc = supR3HardNtDisableThreadCreationEx(pThis->hProcess, pvLdrInitThunk, (void *)(uintptr_t)uNtTerminateThread,
                                            abBackup, sizeof(abBackup), pThis->pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * To further muddle the waters, we map the executable image and ntdll.dll
     * a 2nd time into the process before we actually start executing the thread
     * and trigger the genuine image load events.
     *
     * Update #1 (after 4.3.15 build 7):
     *      Turns out Symantec Endpoint Protection deadlocks when we map the
     *      executable into the process like this.  The system only works
     *      halfways after that Powerbutton, impossible to shutdown without
     *      using the power or reset button. The order of the two mappings
     *      below doesn't matter. Haven't had time to look at stack yet.
     *      Observed on W7/64, SEP v12.1.4112.4156.
     *
     * Update #2 (after 4.3.16):
     *      Some avast! users complain about a deadlock mapping ntdll.dll
     *      as well.  Unfortunately not reproducible, so there may possibly be
     *      some other cause.  Sad as it's really a serious bug in whichever
     *      software it is that is causing it, and we'd like to report it to
     *      the responsible party.
     */
#if 0
    PVOID pvExe2 = supR3HardNtPuChMapDllIntoChild(pThis, &g_SupLibHardenedExeNtPath.UniStr, "executable[2nd]");
#else
    PVOID pvExe2 = NULL;
#endif
#if 0
    UNICODE_STRING NtName1 = RTNT_CONSTANT_UNISTR(L"\\SystemRoot\\System32\\ntdll.dll");
    PVOID pvNtDll2 = supR3HardNtPuChMapDllIntoChild(pThis, &NtName1, "ntdll.dll[2nd]");
#else
    PVOID pvNtDll2 = NULL;
#endif

    /*
     * Create the thread, waiting 10 seconds for it to complete.
     */
    CLIENT_ID Thread2Id;
    HANDLE hThread2;
    NTSTATUS rcNt = RtlCreateUserThread(pThis->hProcess,
                                        NULL /* SecurityAttribs */,
                                        FALSE /* CreateSuspended */,
                                        0 /* ZeroBits */,
                                        0 /* MaximumStackSize */,
                                        0 /* CommittedStackSize */,
                                        (PFNRT)2 /* StartAddress */,
                                        NULL /*Parameter*/ ,
                                        &hThread2,
                                        &Thread2Id);
    if (NT_SUCCESS(rcNt))
    {
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -10 * 10000000; /* 10 seconds */
        NtWaitForSingleObject(hThread2, FALSE /* Alertable */, &Timeout);
        NtTerminateThread(hThread2, DBG_TERMINATE_THREAD);
        NtClose(hThread2);
    }

    /*
     * Map kernel32.dll and kernelbase.dll (if applicable) into the process.
     * This triggers should image load events that may set of AV activities
     * that we'd rather see early than later.
     */
    UNICODE_STRING NtName2 = RTNT_CONSTANT_UNISTR(L"\\SystemRoot\\System32\\kernel32.dll");
    PVOID pvKernel32 = supR3HardNtPuChMapDllIntoChild(pThis, &NtName2, "kernel32.dll");

    UNICODE_STRING NtName3 = RTNT_CONSTANT_UNISTR(L"\\SystemRoot\\System32\\KernelBase.dll");
    PVOID pvKernelBase = g_uNtVerCombined >= SUP_NT_VER_VISTA
                       ? supR3HardNtPuChMapDllIntoChild(pThis, &NtName3, "KernelBase.dll")
                       : NULL;

    /*
     * Fudge factor for letting kernel threads get a chance to mess up our
     * process asynchronously.
     */
    DWORD    dwStart = GetTickCount();
    uint32_t cMsKludge = (g_fSupAdversaries & SUPHARDNT_ADVERSARY_SYMANTEC_SYSPLANT) ? 256 : g_fSupAdversaries ? 64 : 16;
    do
    {
        NtYieldExecution();
        LARGE_INTEGER Time;
        Time.QuadPart = -8000000 / 100; /* 8ms in 100ns units, relative time. */
        NtDelayExecution(FALSE, &Time);
    } while (GetTickCount() - dwStart < cMsKludge);
    SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: Startup delay kludge #1: %u ms\n", GetTickCount() - dwStart));

    /*
     * Unmap the image we mapped into the guest above.
     */
    supR3HardNtPuChUnmapDllFromChild(pThis, pvKernel32, "kernel32.dll");
    supR3HardNtPuChUnmapDllFromChild(pThis, pvKernelBase, "KernelBase.dll");
    supR3HardNtPuChUnmapDllFromChild(pThis, pvNtDll2, "ntdll.dll[2nd]");
    supR3HardNtPuChUnmapDllFromChild(pThis, pvExe2, "executable[2nd]");

    /*
     * Restore the original thunk code and protection.
     * We do this after waiting as anyone trying to kick of threads in the
     * process will get nothing done as long as our patch is in place.
     */
    rc = supR3HardNtEnableThreadCreationEx(pThis->hProcess, pvLdrInitThunk, abBackup, sizeof(abBackup), pThis->pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

#if 0
static int supR3HardenedWinScratchChildMemory(HANDLE hProcess, void *pv, size_t cb, const char *pszWhat, PRTERRINFO pErrInfo)
{
    SUP_DPRINTF(("supR3HardenedWinScratchChildMemory: %p %#x\n", pv, cb));

    PVOID  pvCopy = pv;
    SIZE_T cbCopy = cb;
    NTSTATUS rcNt = NtProtectVirtualMemory(hProcess, &pvCopy, &cbCopy, PAGE_NOACCESS, NULL);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE, "NtProtectVirtualMemory/%s (%p LB %#zx) failed: %#x",
                             pszWhat, pv, cb, rcNt);
    return VINF_SUCCESS;
}
#endif


static int supR3HardNtPuChSanitizePeb(PSUPR3HARDNTPUCH pThis)
{
    /*
     * Make a copy of the pre-execution PEB.
     */
    PEB Peb = pThis->Peb;

#if 0
    /*
     * There should not be any activation context, so if there is, we scratch the memory associated with it.
     */
    int rc = 0;
    if (RT_SUCCESS(rc) && Peb.pShimData && !((uintptr_t)Peb.pShimData & PAGE_OFFSET_MASK))
        rc = supR3HardenedWinScratchChildMemory(hProcess, Peb.pShimData, PAGE_SIZE, "pShimData", pErrInfo);
    if (RT_SUCCESS(rc) && Peb.ActivationContextData && !((uintptr_t)Peb.ActivationContextData & PAGE_OFFSET_MASK))
        rc = supR3HardenedWinScratchChildMemory(hProcess, Peb.ActivationContextData, PAGE_SIZE, "ActivationContextData", pErrInfo);
    if (RT_SUCCESS(rc) && Peb.ProcessAssemblyStorageMap && !((uintptr_t)Peb.ProcessAssemblyStorageMap & PAGE_OFFSET_MASK))
        rc = supR3HardenedWinScratchChildMemory(hProcess, Peb.ProcessAssemblyStorageMap, PAGE_SIZE, "ProcessAssemblyStorageMap", pErrInfo);
    if (RT_SUCCESS(rc) && Peb.SystemDefaultActivationContextData && !((uintptr_t)Peb.SystemDefaultActivationContextData & PAGE_OFFSET_MASK))
        rc = supR3HardenedWinScratchChildMemory(hProcess, Peb.ProcessAssemblyStorageMap, PAGE_SIZE, "SystemDefaultActivationContextData", pErrInfo);
    if (RT_SUCCESS(rc) && Peb.SystemAssemblyStorageMap && !((uintptr_t)Peb.SystemAssemblyStorageMap & PAGE_OFFSET_MASK))
        rc = supR3HardenedWinScratchChildMemory(hProcess, Peb.SystemAssemblyStorageMap, PAGE_SIZE, "SystemAssemblyStorageMap", pErrInfo);
    if (RT_FAILURE(rc))
        return rc;
#endif

    /*
     * Clear compatibility and activation related fields.
     */
    Peb.AppCompatFlags.QuadPart             = 0;
    Peb.AppCompatFlagsUser.QuadPart         = 0;
    Peb.pShimData                           = NULL;
    Peb.AppCompatInfo                       = NULL;
#if 0
    Peb.ActivationContextData               = NULL;
    Peb.ProcessAssemblyStorageMap           = NULL;
    Peb.SystemDefaultActivationContextData  = NULL;
    Peb.SystemAssemblyStorageMap            = NULL;
    /*Peb.Diff0.W6.IsProtectedProcess = 1;*/
#endif

    /*
     * Write back the PEB.
     */
    SIZE_T cbActualMem = pThis->cbPeb;
    NTSTATUS rcNt = NtWriteVirtualMemory(pThis->hProcess, pThis->BasicInfo.PebBaseAddress, &Peb, pThis->cbPeb, &cbActualMem);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE, "NtWriteVirtualMemory/Peb failed: %#x", rcNt);

    return VINF_SUCCESS;
}


static void supR3HardNtPuChFindNtdll(PSUPR3HARDNTPUCH pThis)
{
    /*
     * Find NTDLL in this process first and take that as a starting point.
     */
    pThis->uNtDllParentAddr = (uintptr_t)GetModuleHandleW(L"ntdll.dll");
    SUPR3HARDENED_ASSERT(pThis->uNtDllParentAddr != 0 && !(pThis->uNtDllParentAddr & PAGE_OFFSET_MASK));
    pThis->uNtDllAddr = pThis->uNtDllParentAddr;

    /*
     * Scan the virtual memory of the child.
     */
    uintptr_t   cbAdvance = 0;
    uintptr_t   uPtrWhere = 0;
    for (uint32_t i = 0; i < 1024; i++)
    {
        /* Query information. */
        SIZE_T                      cbActual = 0;
        MEMORY_BASIC_INFORMATION    MemInfo  = { 0, 0, 0, 0, 0, 0, 0 };
        NTSTATUS rcNt = NtQueryVirtualMemory(pThis->hProcess,
                                             (void const *)uPtrWhere,
                                             MemoryBasicInformation,
                                             &MemInfo,
                                             sizeof(MemInfo),
                                             &cbActual);
        if (!NT_SUCCESS(rcNt))
            break;

        if (   MemInfo.Type == SEC_IMAGE
            || MemInfo.Type == SEC_PROTECTED_IMAGE
            || MemInfo.Type == (SEC_IMAGE | SEC_PROTECTED_IMAGE))
        {
            if (MemInfo.BaseAddress == MemInfo.AllocationBase)
            {
                /* Get the image name. */
                union
                {
                    UNICODE_STRING UniStr;
                    uint8_t abPadding[4096];
                } uBuf;
                NTSTATUS rcNt = NtQueryVirtualMemory(pThis->hProcess,
                                                     MemInfo.BaseAddress,
                                                     MemorySectionName,
                                                     &uBuf,
                                                     sizeof(uBuf) - sizeof(WCHAR),
                                                     &cbActual);
                if (NT_SUCCESS(rcNt))
                {
                    uBuf.UniStr.Buffer[uBuf.UniStr.Length / sizeof(WCHAR)] = '\0';
                    if (supR3HardNtIsNamedSystem32Dll(&uBuf.UniStr, "ntdll.dll"))
                    {
                        pThis->uNtDllAddr = (uintptr_t)MemInfo.AllocationBase;
                        SUP_DPRINTF(("supR3HardNtPuChFindNtdll: uNtDllParentAddr=%p uNtDllChildAddr=%p\n",
                                     pThis->uNtDllParentAddr, pThis->uNtDllAddr));
                        return;
                    }
                }
            }
        }

        /*
         * Advance.
         */
        cbAdvance = MemInfo.RegionSize;
        if (uPtrWhere + cbAdvance <= uPtrWhere)
            break;
        uPtrWhere += MemInfo.RegionSize;
    }

#ifdef DEBUG
    supR3HardenedFatal("%s: ntdll.dll not found in child.", __FUNCTION__);
#endif
}



static int supR3HardenedWinPurifyChild(HANDLE hProcess, HANDLE hThread, uintptr_t *puChildNtDllAddr, uintptr_t *puChildExeAddr,
                                       PRTERRINFO pErrInfo)
{
    *puChildNtDllAddr = 0;
    *puChildExeAddr = 0;

    /*
     * Initialize the purifier instance data.
     */
    SUPR3HARDNTPUCH This;
    This.hProcess = hProcess;
    This.hThread  = hThread;
    This.pErrInfo = pErrInfo;

    ULONG cbActual = 0;
    NTSTATUS rcNt = NtQueryInformationProcess(hProcess, ProcessBasicInformation,
                                              &This.BasicInfo, sizeof(This.BasicInfo), &cbActual);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE,
                             "NtQueryInformationProcess/ProcessBasicInformation failed: %#x", rcNt);

    if (g_uNtVerCombined < SUP_NT_VER_W2K3)
        This.cbPeb = PEB_SIZE_W51;
    else if (g_uNtVerCombined < SUP_NT_VER_VISTA)
        This.cbPeb = PEB_SIZE_W52;
    else if (g_uNtVerCombined < SUP_NT_VER_W70)
        This.cbPeb = PEB_SIZE_W6;
    else if (g_uNtVerCombined < SUP_NT_VER_W80)
        This.cbPeb = PEB_SIZE_W7;
    else if (g_uNtVerCombined < SUP_NT_VER_W81)
        This.cbPeb = PEB_SIZE_W80;
    else
        This.cbPeb = PEB_SIZE_W81;

    SUP_DPRINTF(("supR3HardenedWinPurifyChild: PebBaseAddress=%p cbPeb=%#x\n", This.BasicInfo.PebBaseAddress, This.cbPeb));

    SIZE_T cbActualMem;
    RT_ZERO(This.Peb);
    rcNt = NtReadVirtualMemory(hProcess, This.BasicInfo.PebBaseAddress, &This.Peb, sizeof(This.Peb), &cbActualMem);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pErrInfo, VERR_GENERAL_FAILURE, "NtReadVirtualMemory/Peb failed: %#x", rcNt);

    supR3HardNtPuChFindNtdll(&This);

    *puChildNtDllAddr = This.uNtDllAddr;
    *puChildExeAddr   = (uintptr_t)This.Peb.ImageBaseAddress;

    /*
     * Do the work, the last bit we tag along with the process verfication code.
     */
    int rc = supR3HardNtPuChScrewUpPebForInitialImageEvents(&This);
    if (RT_SUCCESS(rc))
        rc = supR3HardNtPuChTriggerInitialImageEvents(&This);
    if (RT_SUCCESS(rc))
        rc = supR3HardNtPuChSanitizePeb(&This);
    if (RT_SUCCESS(rc))
        rc = supHardenedWinVerifyProcess(hProcess, hThread, SUPHARDNTVPKIND_CHILD_PURIFICATION, NULL /*pcFixes*/, pErrInfo);

    return rc;
}


/**
 * Terminates the child process.
 *
 * @param   hProcess            The process handle.
 * @param   pszWhere            Who's having child rasing troubles.
 * @param   rc                  The status code to report.
 * @param   pszFormat           The message format string.
 * @param   ...                 Message format arguments.
 */
static void supR3HardenedWinKillChild(HANDLE hProcess, const char *pszWhere, int rc, const char *pszFormat, ...)
{
    /*
     * Terminate the process ASAP and display error.
     */
    NtTerminateProcess(hProcess, RTEXITCODE_FAILURE);

    va_list va;
    va_start(va, pszFormat);
    supR3HardenedErrorV(rc, false /*fFatal*/, pszFormat, va);
    va_end(va);

    /*
     * Wait for the process to really go away.
     */
    PROCESS_BASIC_INFORMATION BasicInfo;
    NTSTATUS rcNtExit = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
    bool fExitOk = NT_SUCCESS(rcNtExit) && BasicInfo.ExitStatus != STATUS_PENDING;
    if (!fExitOk)
    {
        NTSTATUS rcNtWait;
        DWORD dwStartTick = GetTickCount();
        do
        {
            NtTerminateProcess(hProcess, DBG_TERMINATE_PROCESS);

            LARGE_INTEGER Timeout;
            Timeout.QuadPart = -20000000; /* 2 second */
            rcNtWait = NtWaitForSingleObject(hProcess, TRUE /*Alertable*/, &Timeout);

            rcNtExit = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
            fExitOk = NT_SUCCESS(rcNtExit) && BasicInfo.ExitStatus != STATUS_PENDING;
        } while (   !fExitOk
                 && (   rcNtWait == STATUS_TIMEOUT
                     || rcNtWait == STATUS_USER_APC
                     || rcNtWait == STATUS_ALERTED)
                 && GetTickCount() - dwStartTick < 60 * 1000);
        if (fExitOk)
            supR3HardenedError(rc, false /*fFatal*/,
                               "NtDuplicateObject failed and we failed to kill child: rc=%u (%#x) rcNtWait=%#x hProcess=%p\n",
                               rc, rc, rcNtWait, hProcess);
    }

    /*
     * Final error message.
     */
    va_start(va, pszFormat);
    supR3HardenedFatalMsgV(pszWhere, kSupInitOp_Misc, rc, pszFormat, va);
    va_end(va);
}


/**
 * Checks the child process for error when the parent event semaphore is
 * signaled.
 *
 * If there is an error pending, this function will not return.
 *
 * @param   hProcWait       The child process handle.
 * @param   uChildExeAddr   The address of the executable in the child process.
 * @param   phEvtParent     Pointer to the parent event semaphore handle.  We
 *                          may close the event semaphore and set it to NULL.
 * @param   phEvtChild      Pointer to the child event semaphore handle.  We may
 *                          close the event semaphore and set it to NULL.
 */
static void supR3HardenedWinCheckVmChild(HANDLE hProcWait, uintptr_t uChildExeAddr, HANDLE *phEvtParent, HANDLE *phEvtChild)
{
    /*
     * Read the process parameters from the child.
     */
    uintptr_t           uChildAddr = uChildExeAddr + ((uintptr_t)&g_ProcParams - (uintptr_t)NtCurrentPeb()->ImageBaseAddress);
    SIZE_T              cbIgnored;
    SUPR3WINPROCPARAMS  ChildProcParams;
    RT_ZERO(ChildProcParams);
    NTSTATUS rcNt = NtReadVirtualMemory(hProcWait, (PVOID)uChildAddr, &ChildProcParams, sizeof(ChildProcParams), &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcWait, "supR3HardenedWinCheckVmChild", rcNt,
                                  "NtReadVirtualMemory(,%p,) failed reading child process status: %#x\n", uChildAddr, rcNt);

    /*
     * Signal the child to get on with whatever it's doing.
     */
    rcNt = NtSetEvent(*phEvtChild, NULL);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcWait, "supR3HardenedWinCheckVmChild", rcNt, "NtSetEvent failed: %#x\n", rcNt);

    /*
     * Close the event semaphore handles.
     */
    rcNt = NtClose(*phEvtParent);
    if (NT_SUCCESS(rcNt))
        rcNt = NtClose(*phEvtChild);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcWait, "supR3HardenedWinCheckVmChild", rcNt, "NtClose failed on event sem: %#x\n", rcNt);
    *phEvtChild = NULL;
    *phEvtParent = NULL;

    /*
     * Process the information we read.
     */
    if (ChildProcParams.rc == VINF_SUCCESS)
        return;

    /* An error occurred, report it. */
    ChildProcParams.szErrorMsg[sizeof(ChildProcParams.szErrorMsg) - 1] = '\0';
    supR3HardenedFatalMsg("supR3HardenedWinCheckVmChild", kSupInitOp_Misc, ChildProcParams.rc, "%s", ChildProcParams.szErrorMsg);
}


static void supR3HardenedWinInitVmChild(HANDLE hProcess, HANDLE hThread, uintptr_t uChildNtDllAddr, uintptr_t uChildExeAddr,
                                        HANDLE hEvtChild, HANDLE hEvtParent)
{
    /*
     * Plant the process parameters.  This ASSUMES the handle inheritance is
     * performed when creating the child process.
     */
    SUPR3WINPROCPARAMS ChildProcParams;
    RT_ZERO(ChildProcParams);
    ChildProcParams.hEvtChild  = hEvtChild;
    ChildProcParams.hEvtParent = hEvtParent;
    ChildProcParams.uNtDllAddr = uChildNtDllAddr;
    ChildProcParams.rc         = VINF_SUCCESS;

    uintptr_t uChildAddr = uChildExeAddr + ((uintptr_t)&g_ProcParams - (uintptr_t)NtCurrentPeb()->ImageBaseAddress);
    SIZE_T    cbIgnored;
    NTSTATUS  rcNt = NtWriteVirtualMemory(hProcess, (PVOID)uChildAddr, &ChildProcParams, sizeof(ChildProcParams), &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinInitVmChild", rcNt,
                                  "NtWriteVirtualMemory(,%p,) failed writing child process parameters: %#x\n", uChildAddr, rcNt);

    /*
     * Locate the LdrInitializeThunk address in the child as well as pristine
     * code bits for it.
     */
    PSUPHNTLDRCACHEENTRY pLdrEntry;
    int rc = supHardNtLdrCacheOpen("ntdll.dll", &pLdrEntry);
    if (RT_FAILURE(rc))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinInitVmChild", rc,
                                  "supHardNtLdrCacheOpen failed on NTDLL: %Rrc\n", rc);

    uint8_t *pbChildNtDllBits;
    rc = supHardNtLdrCacheEntryGetBits(pLdrEntry, &pbChildNtDllBits, uChildNtDllAddr, NULL, NULL, NULL /*pErrInfo*/);
    if (RT_FAILURE(rc))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinInitVmChild", rc,
                                  "supHardNtLdrCacheEntryGetBits failed on NTDLL: %Rrc\n", rc);

    RTLDRADDR uLdrInitThunk;
    rc = RTLdrGetSymbolEx(pLdrEntry->hLdrMod, pbChildNtDllBits, uChildNtDllAddr, UINT32_MAX,
                          "LdrInitializeThunk", &uLdrInitThunk);
    if (RT_FAILURE(rc))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinInitVmChild", rc,
                                  "Error locating LdrInitializeThunk in NTDLL: %Rrc", rc);
    PVOID pvLdrInitThunk = (PVOID)(uintptr_t)uLdrInitThunk;
    SUP_DPRINTF(("supR3HardenedWinInitVmChild: uLdrInitThunk=%p\n", (uintptr_t)uLdrInitThunk));

    /*
     * Calculate the address of our code in the child process.
     */
    uintptr_t uEarlyVmProcInitEP = uChildExeAddr + (  (uintptr_t)&supR3HardenedVmProcessInitThunk
                                                    - (uintptr_t)NtCurrentPeb()->ImageBaseAddress);

    /*
     * Compose the LdrInitializeThunk replacement bytes.
     */
    uint8_t abNew[16];
    memcpy(abNew, pbChildNtDllBits + ((uintptr_t)uLdrInitThunk - uChildNtDllAddr), sizeof(abNew));
#ifdef RT_ARCH_AMD64
    abNew[0] = 0xff;
    abNew[1] = 0x25;
    *(uint32_t *)&abNew[2] = 0;
    *(uint64_t *)&abNew[6] = uEarlyVmProcInitEP;
#elif defined(RT_ARCH_X86)
    abNew[0] = 0xe9;
    *(uint32_t *)&abNew[1] = uEarlyVmProcInitEP - ((uint32_t)uLdrInitThunk + 5);
#else
# error "Unsupported arch."
#endif

    /*
     * Install the LdrInitializeThunk replacement code in the child process.
     */
    PVOID   pvProt = pvLdrInitThunk;
    SIZE_T  cbProt = sizeof(abNew);
    ULONG   fOldProt;
    rcNt = NtProtectVirtualMemory(hProcess, &pvProt, &cbProt, PAGE_EXECUTE_READWRITE, &fOldProt);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinInitVmChild", rcNt,
                                  "NtProtectVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    rcNt = NtWriteVirtualMemory(hProcess, pvLdrInitThunk, abNew, sizeof(abNew), &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinInitVmChild", rcNt,
                                  "NtWriteVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    pvProt = pvLdrInitThunk;
    cbProt = sizeof(abNew);
    rcNt = NtProtectVirtualMemory(hProcess, &pvProt, &cbProt, fOldProt, &fOldProt);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinInitVmChild", rcNt,
                                  "NtProtectVirtualMemory/LdrInitializeThunk[restore] failed: %#x", rcNt);

    /* Caller starts child execution. */
    SUP_DPRINTF(("supR3HardenedWinInitVmChild: Start child.\n"));
}


/**
 * Does the actually respawning.
 *
 * @returns Never, will call exit or raise fatal error.
 * @param   iWhich              Which respawn we're to check for, 1 being the
 *                              first one, and 2 the second and final.
 *
 * @todo    Split up this function.
 */
static int supR3HardenedWinDoReSpawn(int iWhich)
{
    NTSTATUS                        rcNt;
    PPEB                            pPeb              = NtCurrentPeb();
    PRTL_USER_PROCESS_PARAMETERS    pParentProcParams = pPeb->ProcessParameters;

    SUPR3HARDENED_ASSERT(g_cSuplibHardenedWindowsMainCalls == 1);

    /*
     * Set up VM child communication event semaphores.
     */
    HANDLE hEvtChild = NULL;
    HANDLE hEvtParent = NULL;
    if (iWhich >= 2)
    {
        OBJECT_ATTRIBUTES ObjAttrs;
        InitializeObjectAttributes(&ObjAttrs, NULL /*pName*/, OBJ_INHERIT, NULL /*hRootDir*/, NULL /*pSecDesc*/);
        SUPR3HARDENED_ASSERT_NT_SUCCESS(NtCreateEvent(&hEvtChild, EVENT_ALL_ACCESS, &ObjAttrs, NotificationEvent, FALSE));
        InitializeObjectAttributes(&ObjAttrs, NULL /*pName*/, OBJ_INHERIT, NULL /*hRootDir*/, NULL /*pSecDesc*/);
        SUPR3HARDENED_ASSERT_NT_SUCCESS(NtCreateEvent(&hEvtParent, EVENT_ALL_ACCESS, &ObjAttrs, NotificationEvent, FALSE));
    }

    /*
     * Set up security descriptors.
     */
    SECURITY_ATTRIBUTES ProcessSecAttrs;
    MYSECURITYCLEANUP   ProcessSecAttrsCleanup;
    supR3HardenedInitSecAttrs(&ProcessSecAttrs, &ProcessSecAttrsCleanup, true /*fProcess*/);

    SECURITY_ATTRIBUTES ThreadSecAttrs;
    MYSECURITYCLEANUP   ThreadSecAttrsCleanup;
    supR3HardenedInitSecAttrs(&ThreadSecAttrs, &ThreadSecAttrsCleanup, false /*fProcess*/);

#if 1
    /*
     * Configure the startup info and creation flags.
     */
    DWORD dwCreationFlags = CREATE_SUSPENDED;

    STARTUPINFOEXW SiEx;
    suplibHardenedMemSet(&SiEx, 0, sizeof(SiEx));
    if (1)
        SiEx.StartupInfo.cb = sizeof(SiEx.StartupInfo);
    else
    {
        SiEx.StartupInfo.cb = sizeof(SiEx);
        dwCreationFlags |= EXTENDED_STARTUPINFO_PRESENT;
        /** @todo experiment with protected process stuff later on. */
    }

    SiEx.StartupInfo.dwFlags |= pParentProcParams->WindowFlags & STARTF_USESHOWWINDOW;
    SiEx.StartupInfo.wShowWindow = (WORD)pParentProcParams->ShowWindowFlags;

    SiEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    SiEx.StartupInfo.hStdInput  = pParentProcParams->StandardInput;
    SiEx.StartupInfo.hStdOutput = pParentProcParams->StandardOutput;
    SiEx.StartupInfo.hStdError  = pParentProcParams->StandardError;

    /*
     * Construct the command line and launch the process.
     */
    PRTUTF16 pwszCmdLine = supR3HardenedWinConstructCmdLine(NULL, iWhich);

    supR3HardenedWinEnableThreadCreation();
    PROCESS_INFORMATION ProcessInfoW32;
    if (!CreateProcessW(g_wszSupLibHardenedExePath,
                        pwszCmdLine,
                        &ProcessSecAttrs,
                        &ThreadSecAttrs,
                        TRUE /*fInheritHandles*/,
                        dwCreationFlags,
                        NULL /*pwszzEnvironment*/,
                        NULL /*pwszCurDir*/,
                        &SiEx.StartupInfo,
                        &ProcessInfoW32))
        supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Misc, VERR_INVALID_NAME,
                              "Error relaunching VirtualBox VM process: %u\n"
                              "Command line: '%ls'",
                              RtlGetLastWin32Error(), pwszCmdLine);
    supR3HardenedWinDisableThreadCreation();

    SUP_DPRINTF(("supR3HardenedWinDoReSpawn(%d): New child %x.%x [kernel32].\n",
                 iWhich, ProcessInfoW32.dwProcessId, ProcessInfoW32.dwThreadId));
    HANDLE hProcess = ProcessInfoW32.hProcess;
    HANDLE hThread  = ProcessInfoW32.hThread;

#else

    /*
     * Construct the process parameters.
     */
    UNICODE_STRING W32ImageName;
    W32ImageName.Buffer = g_wszSupLibHardenedExePath; /* Yes the windows name for the process parameters. */
    W32ImageName.Length = (USHORT)RTUtf16Len(g_wszSupLibHardenedExePath) * sizeof(WCHAR);
    W32ImageName.MaximumLength = W32ImageName.Length + sizeof(WCHAR);

    UNICODE_STRING CmdLine;
    supR3HardenedWinConstructCmdLine(&CmdLine, iWhich);

    PRTL_USER_PROCESS_PARAMETERS pProcParams = NULL;
    SUPR3HARDENED_ASSERT_NT_SUCCESS(RtlCreateProcessParameters(&pProcParams,
                                                               &W32ImageName,
                                                               NULL /* DllPath - inherit from this process */,
                                                               NULL /* CurrentDirectory - inherit from this process */,
                                                               &CmdLine,
                                                               NULL /* Environment - inherit from this process */,
                                                               NULL /* WindowsTitle - none */,
                                                               NULL /* DesktopTitle - none. */,
                                                               NULL /* ShellInfo - none. */,
                                                               NULL /* RuntimeInfo - none (byte array for MSVCRT file info) */)
                                    );

    /** @todo this doesn't work. :-( */
    pProcParams->ConsoleHandle  = pParentProcParams->ConsoleHandle;
    pProcParams->ConsoleFlags   = pParentProcParams->ConsoleFlags;
    pProcParams->StandardInput  = pParentProcParams->StandardInput;
    pProcParams->StandardOutput = pParentProcParams->StandardOutput;
    pProcParams->StandardError  = pParentProcParams->StandardError;

    RTL_USER_PROCESS_INFORMATION ProcessInfoNt = { sizeof(ProcessInfoNt) };
    rcNt = RtlCreateUserProcess(&g_SupLibHardenedExeNtPath.UniStr,
                                OBJ_INHERIT | OBJ_CASE_INSENSITIVE /*Attributes*/,
                                pProcParams,
                                NULL, //&ProcessSecAttrs,
                                NULL, //&ThreadSecAttrs,
                                NtCurrentProcess() /* ParentProcess */,
                                FALSE /*fInheritHandles*/,
                                NULL /* DebugPort */,
                                NULL /* ExceptionPort */,
                                &ProcessInfoNt);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Misc, VERR_INVALID_NAME,
                              "Error relaunching VirtualBox VM process: %#x\n"
                              "Command line: '%ls'",
                              rcNt, CmdLine.Buffer);

    SUP_DPRINTF(("supR3HardenedWinDoReSpawn(%d): New child %x.%x [ntdll].\n",
                 iWhich, ProcessInfo.ClientId.UniqueProcess, ProcessInfo.ClientId.UniqueThread));
    RtlDestroyProcessParameters(pProcParams);

    HANDLE hProcess = ProcessInfoNt.ProcessHandle;
    HANDLE hThread  = ProcessInfoNt.ThreadHandle;
#endif

#ifndef VBOX_WITHOUT_DEBUGGER_CHECKS
    /*
     * Apply anti debugger notification trick to the thread.  (Also done in
     * supR3HardenedWinInstallHooks.)
     */
    rcNt = NtSetInformationThread(hThread, ThreadHideFromDebugger, NULL, 0);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinReSpawn", rcNt,
                                  "NtSetInformationThread/ThreadHideFromDebugger failed: %#x\n", rcNt);
#endif

    /*
     * Clean up the process.
     */
    uintptr_t uChildNtDllAddr;
    uintptr_t uChildExeAddr;
    int rc = supR3HardenedWinPurifyChild(hProcess, hThread, &uChildNtDllAddr, &uChildExeAddr,
                                         RTErrInfoInitStatic(&g_ErrInfoStatic));
    if (RT_FAILURE(rc))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinReSpawn", rc, "%s", g_ErrInfoStatic.szMsg);

    /*
     * Start the process execution.
     */
    if (iWhich >= 2)
        supR3HardenedWinInitVmChild(hProcess, hThread, uChildNtDllAddr, uChildExeAddr, hEvtChild, hEvtParent);

    ULONG cSuspendCount = 0;
    SUPR3HARDENED_ASSERT_NT_SUCCESS(NtResumeThread(hThread, &cSuspendCount));

    /*
     * Close the unrestricted access handles.  Since we need to wait on the
     * child process, we'll reopen the process with limited access before doing
     * away with the process handle returned by CreateProcess.
     */
    SUPR3HARDENED_ASSERT_NT_SUCCESS(NtClose(hThread));

    PROCESS_BASIC_INFORMATION BasicInfo;
    HANDLE hProcWait;
    ULONG fRights = SYNCHRONIZE | PROCESS_TERMINATE;
    if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0)) /* Introduced in Vista. */
        fRights |= PROCESS_QUERY_LIMITED_INFORMATION;
    else
        fRights |= PROCESS_QUERY_INFORMATION;
    if (iWhich == 2)
        fRights |= PROCESS_VM_READ;
    rcNt = NtDuplicateObject(NtCurrentProcess(), hProcess,
                             NtCurrentProcess(), &hProcWait,
                             fRights, 0 /*HandleAttributes*/, 0);
    if (rcNt == STATUS_ACCESS_DENIED)
        rcNt = NtDuplicateObject(NtCurrentProcess(), hProcess,
                                 NtCurrentProcess(), &hProcWait,
                                 SYNCHRONIZE, 0 /*HandleAttributes*/, 0);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedWinKillChild(hProcess, "supR3HardenedWinReSpawn", VERR_INVALID_NAME,
                                  "NtDuplicateObject failed on child process handle: %#x\n", rcNt);

    SUPR3HARDENED_ASSERT_NT_SUCCESS(NtClose(hProcess));
    hProcess = NULL;

    /*
     * Signal the VM child that we've closed the unrestricted handles.
     */
    if (iWhich >= 2)
    {
        rcNt = NtSetEvent(hEvtChild, NULL);
        if (!NT_SUCCESS(rcNt))
            supR3HardenedWinKillChild(hProcess, "supR3HardenedWinReSpawn", VERR_INVALID_NAME,
                                      "NtSetEvent failed on child process handle: %#x\n", rcNt);
    }

    /*
     * Ditch the loader cache so we don't sit on too much memory while waiting.
     */
    supR3HardenedWinFlushLoaderCache();
    supR3HardenedWinCompactHeaps();

    /*
     * Enable thread creation at this point so Ctrl-C and Ctrl-Break can be processed.
     */
    supR3HardenedWinEnableThreadCreation();

    /*
     * If this is the middle process, wait for both parent and child to quit.
     */
    HANDLE hParent = NULL;
    if (iWhich > 1)
    {
        rcNt = NtQueryInformationProcess(NtCurrentProcess(), ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
        if (NT_SUCCESS(rcNt))
        {
            OBJECT_ATTRIBUTES ObjAttr;
            InitializeObjectAttributes(&ObjAttr, NULL, 0, NULL /*hRootDir*/, NULL /*pSecDesc*/);

            CLIENT_ID ClientId;
            ClientId.UniqueProcess = (HANDLE)BasicInfo.InheritedFromUniqueProcessId;
            ClientId.UniqueThread  = NULL;

            rcNt = NtOpenProcess(&hParent, SYNCHRONIZE | PROCESS_QUERY_INFORMATION, &ObjAttr, &ClientId);
        }
#ifdef DEBUG
        SUPR3HARDENED_ASSERT_NT_SUCCESS(rcNt);
#endif
    }

    for (;;)
    {
        HANDLE ahHandles[3];
        ULONG  cHandles = 1;
        ahHandles[0] = hProcWait;
        if (hEvtParent != NULL)
            ahHandles[cHandles++] = hEvtParent;
        if (hParent != NULL)
            ahHandles[cHandles++] = hParent;

        rcNt = NtWaitForMultipleObjects(cHandles, &ahHandles[0], WaitAnyObject, TRUE /*Alertable*/, NULL /*pTimeout*/);
        if (rcNt == STATUS_WAIT_0 + 1 && hEvtParent != NULL)
            supR3HardenedWinCheckVmChild(hProcWait, uChildExeAddr, &hEvtParent, &hEvtChild);
        else if (   (ULONG)rcNt - (ULONG)STATUS_WAIT_0           < cHandles
                 || (ULONG)rcNt - (ULONG)STATUS_ABANDONED_WAIT_0 < cHandles)
            break;
        else if (   rcNt != STATUS_TIMEOUT
                 && rcNt != STATUS_USER_APC
                 && rcNt != STATUS_ALERTED)
            supR3HardenedFatal("NtWaitForMultipleObjects returned %#x\n", rcNt);
    }

    if (hParent != NULL)
        NtClose(hParent);

    /*
     * Proxy the termination code of the child, if it exited already.
     */
    NTSTATUS rcNt2 = NtQueryInformationProcess(hProcWait, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
    if (!NT_SUCCESS(rcNt2))
        BasicInfo.ExitStatus = RTEXITCODE_FAILURE;
    else if (BasicInfo.ExitStatus == STATUS_PENDING)
    {
        if (hEvtParent)
            NtTerminateProcess(hProcWait, RTEXITCODE_FAILURE);
        BasicInfo.ExitStatus = RTEXITCODE_FAILURE;
    }

    NtClose(hProcWait);
    SUP_DPRINTF(("supR3HardenedWinDoReSpawn(%d): Quitting: ExitCode=%#x rcNt=%#x\n", iWhich, BasicInfo.ExitStatus, rcNt));
    suplibHardenedExit((RTEXITCODE)BasicInfo.ExitStatus);
}


/**
 * Logs the content of the given object directory.
 *
 * @returns true if it exists, false if not.
 * @param   pszDir             The path of the directory to log (ASCII).
 */
static void supR3HardenedWinLogObjDir(const char *pszDir)
{
    /*
     * Open the driver object directory.
     */
    RTUTF16 wszDir[128];
    int rc = RTUtf16CopyAscii(wszDir, RT_ELEMENTS(wszDir), pszDir);
    if (RT_FAILURE(rc))
    {
        SUP_DPRINTF(("supR3HardenedWinLogObjDir: RTUtf16CopyAscii -> %Rrc on '%s'\n", rc, pszDir));
        return;
    }

    UNICODE_STRING NtDirName;
    NtDirName.Buffer = (WCHAR *)wszDir;
    NtDirName.Length = (USHORT)(RTUtf16Len(wszDir) * sizeof(WCHAR));
    NtDirName.MaximumLength = NtDirName.Length + sizeof(WCHAR);

    OBJECT_ATTRIBUTES ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &NtDirName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

    HANDLE hDir;
    NTSTATUS rcNt = NtOpenDirectoryObject(&hDir, DIRECTORY_QUERY | FILE_LIST_DIRECTORY, &ObjAttr);
    SUP_DPRINTF(("supR3HardenedWinLogObjDir: %ls => %#x\n", wszDir, rcNt));
    if (!NT_SUCCESS(rcNt))
        return;

    /*
     * Enumerate it, looking for the driver.
     */
    ULONG uObjDirCtx = 0;
    for (;;)
    {
        uint32_t    abBuffer[_64K + _1K];
        ULONG       cbActual;
        rcNt = NtQueryDirectoryObject(hDir,
                                      abBuffer,
                                      sizeof(abBuffer) - 4, /* minus four for string terminator space. */
                                      FALSE /*ReturnSingleEntry */,
                                      FALSE /*RestartScan*/,
                                      &uObjDirCtx,
                                      &cbActual);
        if (!NT_SUCCESS(rcNt) || cbActual < sizeof(OBJECT_DIRECTORY_INFORMATION))
        {
            SUP_DPRINTF(("supR3HardenedWinLogObjDir: NtQueryDirectoryObject => rcNt=%#x cbActual=%#x\n", rcNt, cbActual));
            break;
        }

        POBJECT_DIRECTORY_INFORMATION pObjDir = (POBJECT_DIRECTORY_INFORMATION)abBuffer;
        while (pObjDir->Name.Length != 0)
        {
            WCHAR wcSaved = pObjDir->Name.Buffer[pObjDir->Name.Length / sizeof(WCHAR)];
            SUP_DPRINTF(("  %.*ls  %.*ls\n",
                         pObjDir->TypeName.Length / sizeof(WCHAR), pObjDir->TypeName.Buffer,
                         pObjDir->Name.Length / sizeof(WCHAR), pObjDir->Name.Buffer));

            /* Next directory entry. */
            pObjDir++;
        }
    }

    /*
     * Clean up and return.
     */
    NtClose(hDir);
}


/**
 * Checks if the driver exists.
 *
 * This checks whether the driver is present in the /Driver object directory.
 * Drivers being initialized or terminated will have an object there
 * before/after their devices nodes are created/deleted.
 *
 * @returns true if it exists, false if not.
 * @param   pszDriver           The driver name.
 */
static bool supR3HardenedWinDriverExists(const char *pszDriver)
{
    /*
     * Open the driver object directory.
     */
    UNICODE_STRING NtDirName = RTNT_CONSTANT_UNISTR(L"\\Driver");

    OBJECT_ATTRIBUTES ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &NtDirName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

    HANDLE hDir;
    NTSTATUS rcNt = NtOpenDirectoryObject(&hDir, DIRECTORY_QUERY | FILE_LIST_DIRECTORY, &ObjAttr);
#ifdef VBOX_STRICT
    SUPR3HARDENED_ASSERT_NT_SUCCESS(rcNt);
#endif
    if (!NT_SUCCESS(rcNt))
        return true;

    /*
     * Enumerate it, looking for the driver.
     */
    bool  fFound = true;
    ULONG uObjDirCtx = 0;
    do
    {
        uint32_t    abBuffer[_64K + _1K];
        ULONG       cbActual;
        rcNt = NtQueryDirectoryObject(hDir,
                                      abBuffer,
                                      sizeof(abBuffer) - 4, /* minus four for string terminator space. */
                                      FALSE /*ReturnSingleEntry */,
                                      FALSE /*RestartScan*/,
                                      &uObjDirCtx,
                                      &cbActual);
        if (!NT_SUCCESS(rcNt) || cbActual < sizeof(OBJECT_DIRECTORY_INFORMATION))
            break;

        POBJECT_DIRECTORY_INFORMATION pObjDir = (POBJECT_DIRECTORY_INFORMATION)abBuffer;
        while (pObjDir->Name.Length != 0)
        {
            WCHAR wcSaved = pObjDir->Name.Buffer[pObjDir->Name.Length / sizeof(WCHAR)];
            pObjDir->Name.Buffer[pObjDir->Name.Length / sizeof(WCHAR)] = '\0';
            if (   pObjDir->Name.Length > 1
                && RTUtf16ICmpAscii(pObjDir->Name.Buffer, pszDriver) == 0)
            {
                fFound = true;
                break;
            }
            pObjDir->Name.Buffer[pObjDir->Name.Length / sizeof(WCHAR)] = wcSaved;

            /* Next directory entry. */
            pObjDir++;
        }
    } while (!fFound);

    /*
     * Clean up and return.
     */
    NtClose(hDir);

    return fFound;
}


/**
 * Open the stub device before the 2nd respawn.
 */
static void supR3HardenedWinOpenStubDevice(void)
{
    /*
     * Retry if we think driver might still be initializing (STATUS_NO_SUCH_DEVICE + \Drivers\VBoxDrv).
     */
    static const WCHAR  s_wszName[] = L"\\Device\\VBoxDrvStub";
    DWORD const         uStartTick = GetTickCount();
    NTSTATUS            rcNt;
    uint32_t            iTry;

    for (iTry = 0;; iTry++)
    {
        HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
        IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;

        UNICODE_STRING      NtName;
        NtName.Buffer        = (PWSTR)s_wszName;
        NtName.Length        = sizeof(s_wszName) - sizeof(WCHAR);
        NtName.MaximumLength = sizeof(s_wszName);

        OBJECT_ATTRIBUTES   ObjAttr;
        InitializeObjectAttributes(&ObjAttr, &NtName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

        rcNt = NtCreateFile(&hFile,
                            GENERIC_READ | GENERIC_WRITE,
                            &ObjAttr,
                            &Ios,
                            NULL /* Allocation Size*/,
                            FILE_ATTRIBUTE_NORMAL,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            FILE_OPEN,
                            FILE_NON_DIRECTORY_FILE,
                            NULL /*EaBuffer*/,
                            0 /*EaLength*/);
        if (NT_SUCCESS(rcNt))
            rcNt = Ios.Status;

        /* The STATUS_NO_SUCH_DEVICE might be returned if the device is not
           completely initialized.  Delay a little bit and try again. */
        if (rcNt != STATUS_NO_SUCH_DEVICE)
            break;
        if (iTry > 0 && GetTickCount() - uStartTick > 5000)  /* 5 sec, at least two tries */
            break;
        if (!supR3HardenedWinDriverExists("VBoxDrv"))
        {
            /** @todo Consider starting the VBoxdrv.sys service. Requires 2nd process
             *        though, rather complicated actually as CreateProcess causes all
             *        kind of things to happen to this process which would make it hard to
             *        pass the process verification tests... :-/ */
            break;
        }

        LARGE_INTEGER Time;
        if (iTry < 8)
            Time.QuadPart = -1000000 / 100; /* 1ms in 100ns units, relative time. */
        else
            Time.QuadPart = -32000000 / 100; /* 32ms in 100ns units, relative time. */
        NtDelayExecution(TRUE, &Time);
    }

    if (!NT_SUCCESS(rcNt))
    {
        /*
         * Report trouble (fatal).  For some errors codes we try gather some
         * extra information that goes into VBoxStartup.log so that we stand a
         * better chance resolving the issue.
         */
        int rc = VERR_OPEN_FAILED;
        if (SUP_NT_STATUS_IS_VBOX(rcNt)) /* See VBoxDrvNtErr2NtStatus. */
        {
            rc = SUP_NT_STATUS_TO_VBOX(rcNt);

            /*
             * \Windows\ApiPort open trouble.  So far only
             * STATUS_OBJECT_TYPE_MISMATCH has been observed.
             */
            if (rc == VERR_SUPDRV_APIPORT_OPEN_ERROR)
            {
                SUP_DPRINTF(("Error opening VBoxDrvStub: VERR_SUPDRV_APIPORT_OPEN_ERROR\n"));

                uint32_t uSessionId = NtCurrentPeb()->SessionId;
                SUP_DPRINTF(("  SessionID=%#x\n", uSessionId));
                char szDir[64];
                if (uSessionId == 0)
                    RTStrCopy(szDir, sizeof(szDir), "\\Windows");
                else
                {
                    RTStrPrintf(szDir, sizeof(szDir), "\\Sessions\\%u\\Windows", uSessionId);
                    supR3HardenedWinLogObjDir(szDir);
                }
                supR3HardenedWinLogObjDir("\\Windows");
                supR3HardenedWinLogObjDir("\\Sessions");
                supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Misc, rc,
                                      "NtCreateFile(%ls) failed: VERR_SUPDRV_APIPORT_OPEN_ERROR\n"
                                      "\n"
                                      "Error getting %s\\ApiPort in the driver from vboxdrv.\n"
                                      "\n"
                                      "Could be due to security software is redirecting access to it, so please include full "
                                      "details of such software in a bug report. VBoxStartup.log may contain details important "
                                      "to resolving the issue."
                                      , s_wszName, szDir);
            }

            /*
             * Generic VBox failure message.
             */
            supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Driver, rc,
                                  "NtCreateFile(%ls) failed: %Rrc (rcNt=%#x)\n", s_wszName, rc, rcNt);
        }
        else
        {
            const char *pszDefine;
            switch (rcNt)
            {
                case STATUS_NO_SUCH_DEVICE:         pszDefine = " STATUS_NO_SUCH_DEVICE"; break;
                case STATUS_OBJECT_NAME_NOT_FOUND:  pszDefine = " STATUS_OBJECT_NAME_NOT_FOUND"; break;
                case STATUS_ACCESS_DENIED:          pszDefine = " STATUS_ACCESS_DENIED"; break;
                case STATUS_TRUST_FAILURE:          pszDefine = " STATUS_TRUST_FAILURE"; break;
                default:                            pszDefine = ""; break;
            }

            /*
             * Problems opening the device is generally due to driver load/
             * unload issues.  Check whether the driver is loaded and make
             * suggestions accordingly.
             */
            if (   rcNt == STATUS_NO_SUCH_DEVICE
                || rcNt == STATUS_OBJECT_NAME_NOT_FOUND)
            {
                SUP_DPRINTF(("Error opening VBoxDrvStub: %s\n", pszDefine));
                if (supR3HardenedWinDriverExists("VBoxDrv"))
                    supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Driver, VERR_OPEN_FAILED,
                                          "NtCreateFile(%ls) failed: %#x%s (%u retries)\n"
                                          "\n"
                                          "Driver is probably stuck stopping/starting. Try 'sc.exe query vboxdrv' to get more "
                                          "information about its state. Rebooting may actually help.\n"
                                          , s_wszName, rcNt, pszDefine, iTry);
                else
                    supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Driver, VERR_OPEN_FAILED,
                                          "NtCreateFile(%ls) failed: %#x%s (%u retries)\n"
                                          "\n"
                                          "Driver is does not appear to be loaded. Try 'sc.exe start vboxdrv', reinstall "
                                          "VirtualBox or reboot.\n"
                                          , s_wszName, rcNt, pszDefine, iTry);
            }

            /* Generic NT failure message. */
            supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Driver, VERR_OPEN_FAILED,
                                  "NtCreateFile(%ls) failed: %#x%s (%u retries)\n", s_wszName, rcNt, pszDefine, iTry);
        }
    }
}


/**
 * Called by the main code if supR3HardenedWinIsReSpawnNeeded returns @c true.
 *
 * @returns Program exit code.
 */
DECLHIDDEN(int) supR3HardenedWinReSpawn(int iWhich)
{
    /*
     * Before the 2nd respawn we set up a child protection deal with the
     * support driver via /Devices/VBoxDrvStub.
     */
    if (iWhich == 2)
        supR3HardenedWinOpenStubDevice();

    /*
     * Respawn the process with kernel protection for the new process.
     */
    return supR3HardenedWinDoReSpawn(iWhich);
}


/**
 * Checks if re-spawning is required, replacing the respawn argument if not.
 *
 * @returns true if required, false if not. In the latter case, the first
 *          argument in the vector is replaced.
 * @param   iWhich              Which respawn we're to check for, 1 being the
 *                              first one, and 2 the second and final.
 * @param   cArgs               The number of arguments.
 * @param   papszArgs           Pointer to the argument vector.
 */
DECLHIDDEN(bool) supR3HardenedWinIsReSpawnNeeded(int iWhich, int cArgs, char **papszArgs)
{
    SUPR3HARDENED_ASSERT(g_cSuplibHardenedWindowsMainCalls == 1);
    SUPR3HARDENED_ASSERT(iWhich == 1 || iWhich == 2);

    if (cArgs < 1)
        return true;

    if (suplibHardenedStrCmp(papszArgs[0], SUPR3_RESPAWN_1_ARG0) == 0)
    {
        if (iWhich > 1)
            return true;
    }
    else if (suplibHardenedStrCmp(papszArgs[0], SUPR3_RESPAWN_2_ARG0) == 0)
    {
        if (iWhich < 2)
            return false;
    }
    else
        return true;

    /* Replace the argument. */
    papszArgs[0] = g_szSupLibHardenedExePath;
    return false;
}


/**
 * Initializes the windows verficiation bits.
 * @param   fFlags          The main flags.
 * @param   fAvastKludge    Whether to apply the avast kludge.
 */
DECLHIDDEN(void) supR3HardenedWinInit(uint32_t fFlags, bool fAvastKludge)
{
    /*
     * Init the verifier.
     */
    RTErrInfoInitStatic(&g_ErrInfoStatic);
    int rc = supHardenedWinInitImageVerifier(&g_ErrInfoStatic.Core);
    if (RT_FAILURE(rc))
        supR3HardenedFatalMsg("supR3HardenedWinInit", kSupInitOp_Misc, rc,
                              "supHardenedWinInitImageVerifier failed: %s", g_ErrInfoStatic.szMsg);

    /*
     * Get the windows system directory from the KnownDlls dir.
     */
    HANDLE              hSymlink = INVALID_HANDLE_VALUE;
    UNICODE_STRING      UniStr = RTNT_CONSTANT_UNISTR(L"\\KnownDlls\\KnownDllPath");
    OBJECT_ATTRIBUTES   ObjAttrs;
    InitializeObjectAttributes(&ObjAttrs, &UniStr, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);
    NTSTATUS rcNt = NtOpenSymbolicLinkObject(&hSymlink, SYMBOLIC_LINK_QUERY, &ObjAttrs);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatalMsg("supR3HardenedWinInit", kSupInitOp_Misc, rcNt, "Error opening '%ls': %#x", UniStr.Buffer, rcNt);

    g_System32WinPath.UniStr.Buffer = g_System32WinPath.awcBuffer;
    g_System32WinPath.UniStr.Length = 0;
    g_System32WinPath.UniStr.MaximumLength = sizeof(g_System32WinPath.awcBuffer) - sizeof(RTUTF16);
    rcNt = NtQuerySymbolicLinkObject(hSymlink, &g_System32WinPath.UniStr, NULL);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatalMsg("supR3HardenedWinInit", kSupInitOp_Misc, rcNt, "Error querying '%ls': %#x", UniStr.Buffer, rcNt);
    g_System32WinPath.UniStr.Buffer[g_System32WinPath.UniStr.Length / sizeof(RTUTF16)] = '\0';

    SUP_DPRINTF(("KnownDllPath: %ls\n", g_System32WinPath.UniStr.Buffer));
    NtClose(hSymlink);

    if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV))
    {
        if (fAvastKludge)
        {
            /*
             * Do a self purification to cure avast's weird NtOpenFile write-thru
             * change in GetBinaryTypeW change in kernel32.  Unfortunately, avast
             * uses a system thread to perform the process modifications, which
             * means it's hard to make sure it had the chance to make them...
             *
             * We have to resort to kludge doing yield and sleep fudging for a
             * number of milliseconds and schedulings before we can hope that avast
             * and similar products have done what they need to do.  If we do any
             * fixes, we wait for a while again and redo it until we're clean.
             *
             * This is unfortunately kind of fragile.
             */
            uint32_t cMsFudge = g_fSupAdversaries ? 512 : 128;
            uint32_t cFixes;
            for (uint32_t iLoop = 0; iLoop < 16; iLoop++)
            {
                uint32_t    cSleeps = 0;
                DWORD       dwStart = GetTickCount();
                do
                {
                    NtYieldExecution();
                    LARGE_INTEGER Time;
                    Time.QuadPart = -8000000 / 100; /* 8ms in 100ns units, relative time. */
                    NtDelayExecution(FALSE, &Time);
                    cSleeps++;
                } while (   GetTickCount() - dwStart <= cMsFudge
                         || cSleeps < 8);
                SUP_DPRINTF(("supR3HardenedWinInit: Startup delay kludge #2/%u: %u ms, %u sleeps\n",
                             iLoop, GetTickCount() - dwStart, cSleeps));

                cFixes = 0;
                rc = supHardenedWinVerifyProcess(NtCurrentProcess(), NtCurrentThread(), SUPHARDNTVPKIND_SELF_PURIFICATION,
                                                 &cFixes, NULL /*pErrInfo*/);
                if (RT_FAILURE(rc) || cFixes == 0)
                    break;

                if (!g_fSupAdversaries)
                    g_fSupAdversaries |= SUPHARDNT_ADVERSARY_UNKNOWN;
                cMsFudge = 512;

                /* Log the KiOpPrefetchPatchCount value if available, hoping it might sched some light on spider38's case. */
                ULONG cPatchCount = 0;
                rcNt = NtQuerySystemInformation(SystemInformation_KiOpPrefetchPatchCount,
                                                &cPatchCount, sizeof(cPatchCount), NULL);
                if (NT_SUCCESS(rcNt))
                    SUP_DPRINTF(("supR3HardenedWinInit: cFixes=%u g_fSupAdversaries=%#x cPatchCount=%#u\n",
                                 cFixes, g_fSupAdversaries, cPatchCount));
                else
                    SUP_DPRINTF(("supR3HardenedWinInit: cFixes=%u g_fSupAdversaries=%#x\n", cFixes, g_fSupAdversaries));
            }
        }

        /*
         * Install the hooks.
         */
        supR3HardenedWinInstallHooks();
    }

#ifndef VBOX_WITH_VISTA_NO_SP
    /*
     * Complain about Vista w/o service pack if we're launching a VM.
     */
    if (   !(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV)
        && g_uNtVerCombined >= SUP_NT_VER_VISTA
        && g_uNtVerCombined <  SUP_MAKE_NT_VER_COMBINED(6, 0, 6001, 0, 0))
        supR3HardenedFatalMsg("supR3HardenedWinInit", kSupInitOp_Misc, VERR_NOT_SUPPORTED,
                              "Window Vista without any service pack installed is not supported. Please install the latest service pack.");
#endif
}


/**
 * Converts the Windows command line string (UTF-16) to an array of UTF-8
 * arguments suitable for passing to main().
 *
 * @returns Pointer to the argument array.
 * @param   pawcCmdLine         The UTF-16 windows command line to parse.
 * @param   cwcCmdLine          The length of the command line.
 * @param   pcArgs              Where to return the number of arguments.
 */
static char **suplibCommandLineToArgvWStub(PCRTUTF16 pawcCmdLine, size_t cwcCmdLine, int *pcArgs)
{
    /*
     * Convert the command line string to UTF-8.
     */
    char *pszCmdLine = NULL;
    SUPR3HARDENED_ASSERT(RT_SUCCESS(RTUtf16ToUtf8Ex(pawcCmdLine, cwcCmdLine, &pszCmdLine, 0, NULL)));

    /*
     * Parse the command line, carving argument strings out of it.
     */
    int    cArgs          = 0;
    int    cArgsAllocated = 4;
    char **papszArgs      = (char **)RTMemAllocZ(sizeof(char *) * cArgsAllocated);
    char  *pszSrc         = pszCmdLine;
    for (;;)
    {
        /* skip leading blanks. */
        char ch = *pszSrc;
        while (suplibCommandLineIsArgSeparator(ch))
            ch = *++pszSrc;
        if (!ch)
            break;

        /* Add argument to the vector. */
        if (cArgs + 2 >= cArgsAllocated)
        {
            cArgsAllocated *= 2;
            papszArgs = (char **)RTMemRealloc(papszArgs, sizeof(char *) * cArgsAllocated);
        }
        papszArgs[cArgs++] = pszSrc;
        papszArgs[cArgs]   = NULL;

        /* Unquote and unescape the string. */
        char *pszDst = pszSrc++;
        bool fQuoted = false;
        do
        {
            if (ch == '"')
                fQuoted = !fQuoted;
            else if (ch != '\\' || (*pszSrc != '\\' && *pszSrc != '"'))
                *pszDst++ = ch;
            else
            {
                unsigned cSlashes = 0;
                while ((ch = *pszSrc++) == '\\')
                    cSlashes++;
                if (ch == '"')
                {
                    while (cSlashes >= 2)
                    {
                        cSlashes -= 2;
                        *pszDst++ = '\\';
                    }
                    if (cSlashes)
                        *pszDst++ = '"';
                    else
                        fQuoted = !fQuoted;
                }
                else
                {
                    pszSrc--;
                    while (cSlashes-- > 0)
                        *pszDst++ = '\\';
                }
            }

            ch = *pszSrc++;
        } while (ch != '\0' && (fQuoted || !suplibCommandLineIsArgSeparator(ch)));

        /* Terminate the argument. */
        *pszDst = '\0';
        if (!ch)
            break;
    }

    *pcArgs = cArgs;
    return papszArgs;
}


/**
 * Logs information about a file from a protection product or from Windows.
 *
 * The purpose here is to better see which version of the product is installed
 * and not needing to depend on the user supplying the correct information.
 *
 * @param   pwszFile            The NT path to the file.
 * @param   fAdversarial        Set if from a protection product, false if
 *                              system file.
 */
static void supR3HardenedLogFileInfo(PCRTUTF16 pwszFile, bool fAdversarial)
{
    /*
     * Open the file.
     */
    HANDLE              hFile  = RTNT_INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK     Ios    = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    UNICODE_STRING      UniStrName;
    UniStrName.Buffer = (WCHAR *)pwszFile;
    UniStrName.Length = (USHORT)(RTUtf16Len(pwszFile) * sizeof(WCHAR));
    UniStrName.MaximumLength = UniStrName.Length + sizeof(WCHAR);
    OBJECT_ATTRIBUTES   ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &UniStrName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);
    NTSTATUS rcNt = NtCreateFile(&hFile,
                                 GENERIC_READ,
                                 &ObjAttr,
                                 &Ios,
                                 NULL /* Allocation Size*/,
                                 FILE_ATTRIBUTE_NORMAL,
                                 FILE_SHARE_READ,
                                 FILE_OPEN,
                                 FILE_NON_DIRECTORY_FILE,
                                 NULL /*EaBuffer*/,
                                 0 /*EaLength*/);
    if (NT_SUCCESS(rcNt))
        rcNt = Ios.Status;
    if (NT_SUCCESS(rcNt))
    {
        SUP_DPRINTF(("%ls:\n", pwszFile));
        union
        {
            uint64_t                    u64AlignmentInsurance;
            FILE_BASIC_INFORMATION      BasicInfo;
            FILE_STANDARD_INFORMATION   StdInfo;
            uint8_t                     abBuf[32768];
            RTUTF16                     awcBuf[16384];
            IMAGE_DOS_HEADER            MzHdr;
        } u;
        RTTIMESPEC  TimeSpec;
        char        szTmp[64];

        /*
         * Print basic file information available via NtQueryInformationFile.
         */
        IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        rcNt = NtQueryInformationFile(hFile, &Ios, &u.BasicInfo, sizeof(u.BasicInfo), FileBasicInformation);
        if (NT_SUCCESS(rcNt) && NT_SUCCESS(Ios.Status))
        {
            SUP_DPRINTF(("    CreationTime:    %s\n", RTTimeSpecToString(RTTimeSpecSetNtTime(&TimeSpec, u.BasicInfo.CreationTime.QuadPart), szTmp, sizeof(szTmp))));
            /*SUP_DPRINTF(("    LastAccessTime:  %s\n", RTTimeSpecToString(RTTimeSpecSetNtTime(&TimeSpec, u.BasicInfo.LastAccessTime.QuadPart), szTmp, sizeof(szTmp))));*/
            SUP_DPRINTF(("    LastWriteTime:   %s\n", RTTimeSpecToString(RTTimeSpecSetNtTime(&TimeSpec, u.BasicInfo.LastWriteTime.QuadPart), szTmp, sizeof(szTmp))));
            SUP_DPRINTF(("    ChangeTime:      %s\n", RTTimeSpecToString(RTTimeSpecSetNtTime(&TimeSpec, u.BasicInfo.ChangeTime.QuadPart), szTmp, sizeof(szTmp))));
            SUP_DPRINTF(("    FileAttributes:  %#x\n", u.BasicInfo.FileAttributes));
        }
        else
            SUP_DPRINTF(("    FileBasicInformation -> %#x %#x\n", rcNt, Ios.Status));

        rcNt = NtQueryInformationFile(hFile, &Ios, &u.StdInfo, sizeof(u.StdInfo), FileStandardInformation);
        if (NT_SUCCESS(rcNt) && NT_SUCCESS(Ios.Status))
            SUP_DPRINTF(("    Size:            %#llx\n", u.StdInfo.EndOfFile.QuadPart));
        else
            SUP_DPRINTF(("    FileStandardInformation -> %#x %#x\n", rcNt, Ios.Status));

        /*
         * Read the image header and extract the timestamp and other useful info.
         */
        RT_ZERO(u);
        LARGE_INTEGER offRead;
        offRead.QuadPart = 0;
        rcNt = NtReadFile(hFile, NULL /*hEvent*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/, &Ios,
                          &u, (ULONG)sizeof(u), &offRead, NULL);
        if (NT_SUCCESS(rcNt) && NT_SUCCESS(Ios.Status))
        {
            uint32_t offNtHdrs = 0;
            if (u.MzHdr.e_magic == IMAGE_DOS_SIGNATURE)
                offNtHdrs = u.MzHdr.e_lfanew;
            if (offNtHdrs < sizeof(u) - sizeof(IMAGE_NT_HEADERS))
            {
                PIMAGE_NT_HEADERS64 pNtHdrs64 = (PIMAGE_NT_HEADERS64)&u.abBuf[offNtHdrs];
                PIMAGE_NT_HEADERS32 pNtHdrs32 = (PIMAGE_NT_HEADERS32)&u.abBuf[offNtHdrs];
                if (pNtHdrs64->Signature == IMAGE_NT_SIGNATURE)
                {
                    SUP_DPRINTF(("    NT Headers:      %#x\n", offNtHdrs));
                    SUP_DPRINTF(("    Timestamp:       %#x\n", pNtHdrs64->FileHeader.TimeDateStamp));
                    SUP_DPRINTF(("    Machine:         %#x%s\n", pNtHdrs64->FileHeader.Machine,
                                 pNtHdrs64->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 ? " - i386"
                                 : pNtHdrs64->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 ? " - amd64" : ""));
                    SUP_DPRINTF(("    Timestamp:       %#x\n", pNtHdrs64->FileHeader.TimeDateStamp));
                    SUP_DPRINTF(("    Image Version:   %u.%u\n",
                                 pNtHdrs64->OptionalHeader.MajorImageVersion, pNtHdrs64->OptionalHeader.MinorImageVersion));
                    SUP_DPRINTF(("    SizeOfImage:     %#x (%u)\n", pNtHdrs64->OptionalHeader.SizeOfImage, pNtHdrs64->OptionalHeader.SizeOfImage));

                    /*
                     * Very crude way to extract info from the file version resource.
                     */
                    PIMAGE_SECTION_HEADER paSectHdrs = (PIMAGE_SECTION_HEADER)(  (uintptr_t)&pNtHdrs64->OptionalHeader
                                                                               + pNtHdrs64->FileHeader.SizeOfOptionalHeader);
                    IMAGE_DATA_DIRECTORY  RsrcDir = { 0, 0 };
                    if (   pNtHdrs64->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64)
                        && pNtHdrs64->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_RESOURCE)
                        RsrcDir = pNtHdrs64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
                    else if (   pNtHdrs64->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER32)
                             && pNtHdrs32->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_RESOURCE)
                        RsrcDir = pNtHdrs32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
                    SUP_DPRINTF(("    Resource Dir:    %#x LB %#x\n", RsrcDir.VirtualAddress, RsrcDir.Size));
                    if (   RsrcDir.VirtualAddress > offNtHdrs
                        && RsrcDir.Size > 0
                        &&    (uintptr_t)&u + sizeof(u) - (uintptr_t)paSectHdrs
                           >= pNtHdrs64->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER) )
                    {
                        offRead.QuadPart = 0;
                        for (uint32_t i = 0; i < pNtHdrs64->FileHeader.NumberOfSections; i++)
                            if (   paSectHdrs[i].VirtualAddress - RsrcDir.VirtualAddress < paSectHdrs[i].SizeOfRawData
                                && paSectHdrs[i].PointerToRawData > offNtHdrs)
                            {
                                offRead.QuadPart = paSectHdrs[i].PointerToRawData
                                                 + (paSectHdrs[i].VirtualAddress - RsrcDir.VirtualAddress);
                                break;
                            }
                        if (offRead.QuadPart > 0)
                        {
                            RT_ZERO(u);
                            rcNt = NtReadFile(hFile, NULL /*hEvent*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/, &Ios,
                                              &u, (ULONG)sizeof(u), &offRead, NULL);
                            if (NT_SUCCESS(rcNt) && NT_SUCCESS(Ios.Status))
                            {
                                static const struct { PCRTUTF16 pwsz; size_t cb; } s_abFields[] =
                                {
#define MY_WIDE_STR_TUPLE(a_sz)     { L ## a_sz, sizeof(L ## a_sz) - sizeof(RTUTF16) }
                                    MY_WIDE_STR_TUPLE("ProductName"),
                                    MY_WIDE_STR_TUPLE("ProductVersion"),
                                    MY_WIDE_STR_TUPLE("FileVersion"),
                                    MY_WIDE_STR_TUPLE("SpecialBuild"),
                                    MY_WIDE_STR_TUPLE("PrivateBuild"),
                                    MY_WIDE_STR_TUPLE("FileDescription"),
#undef MY_WIDE_STR_TUPLE
                                };
                                for (uint32_t i = 0; i < RT_ELEMENTS(s_abFields); i++)
                                {
                                    size_t          cwcLeft = (sizeof(u) - s_abFields[i].cb - 10) / sizeof(RTUTF16);
                                    PCRTUTF16       pwc     = u.awcBuf;
                                    RTUTF16 const   wcFirst = *s_abFields[i].pwsz;
                                    while (cwcLeft-- > 0)
                                    {
                                        if (   pwc[0] == 1 /* wType == text */
                                            && pwc[1] == wcFirst)
                                        {
                                            if (memcmp(pwc + 1, s_abFields[i].pwsz, s_abFields[i].cb + sizeof(RTUTF16)) == 0)
                                            {
                                                size_t cwcField = s_abFields[i].cb / sizeof(RTUTF16);
                                                pwc     += cwcField + 2;
                                                cwcLeft -= cwcField + 2;
                                                for (uint32_t iPadding = 0; iPadding < 3; iPadding++, pwc++, cwcLeft--)
                                                    if (*pwc)
                                                        break;
                                                int rc = RTUtf16ValidateEncodingEx(pwc, cwcLeft,
                                                                                   RTSTR_VALIDATE_ENCODING_ZERO_TERMINATED);
                                                if (RT_SUCCESS(rc))
                                                    SUP_DPRINTF(("    %ls:%*s %ls",
                                                                 s_abFields[i].pwsz, cwcField < 15 ? 15 - cwcField : 0, "", pwc));
                                                else
                                                    SUP_DPRINTF(("    %ls:%*s rc=%Rrc",
                                                                 s_abFields[i].pwsz, cwcField < 15 ? 15 - cwcField : 0, "", rc));

                                                break;
                                            }
                                        }
                                        pwc++;
                                    }
                                }
                            }
                            else
                                SUP_DPRINTF(("    NtReadFile @%#llx -> %#x %#x\n", offRead.QuadPart, rcNt, Ios.Status));
                        }
                        else
                            SUP_DPRINTF(("    Resource section not found.\n"));
                    }
                }
                else
                    SUP_DPRINTF(("    Nt Headers @%#x: Invalid signature\n", offNtHdrs));
            }
            else
                SUP_DPRINTF(("    Nt Headers @%#x: out side buffer\n", offNtHdrs));
        }
        else
            SUP_DPRINTF(("    NtReadFile @0 -> %#x %#x\n", rcNt, Ios.Status));
        NtClose(hFile);
    }
}


/**
 * Scans the Driver directory for drivers which may invade our processes.
 *
 * @returns Mask of SUPHARDNT_ADVERSARY_XXX flags.
 *
 * @remarks The enumeration of \Driver normally requires administrator
 *          privileges.  So, the detection we're doing here isn't always gonna
 *          work just based on that.
 *
 * @todo    Find drivers in \FileSystems as well, then we could detect VrNsdDrv
 *          from ViRobot APT Shield 2.0.
 */
static uint32_t supR3HardenedWinFindAdversaries(void)
{
    static const struct
    {
        uint32_t    fAdversary;
        const char *pszDriver;
    } s_aDrivers[] =
    {
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360,        "SRTSPX" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360,        "SymDS" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360,        "SymEvent" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360,        "SymIRON" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360,        "SymNetS" },

        { SUPHARDNT_ADVERSARY_AVAST,                "aswHwid" },
        { SUPHARDNT_ADVERSARY_AVAST,                "aswMonFlt" },
        { SUPHARDNT_ADVERSARY_AVAST,                "aswRdr2" },
        { SUPHARDNT_ADVERSARY_AVAST,                "aswRvrt" },
        { SUPHARDNT_ADVERSARY_AVAST,                "aswSnx" },
        { SUPHARDNT_ADVERSARY_AVAST,                "aswsp" },
        { SUPHARDNT_ADVERSARY_AVAST,                "aswStm" },
        { SUPHARDNT_ADVERSARY_AVAST,                "aswVmm" },

        { SUPHARDNT_ADVERSARY_TRENDMICRO,           "tmcomm" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO,           "tmactmon" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO,           "tmevtmgr" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO,           "tmtdi" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO,           "tmebc64" },  /* Titanium internet security, not officescan. */
        { SUPHARDNT_ADVERSARY_TRENDMICRO,           "tmeevw" },   /* Titanium internet security, not officescan. */
        { SUPHARDNT_ADVERSARY_TRENDMICRO,           "tmciesc" },  /* Titanium internet security, not officescan. */

        { SUPHARDNT_ADVERSARY_MCAFEE,               "cfwids" },
        { SUPHARDNT_ADVERSARY_MCAFEE,               "McPvDrv" },
        { SUPHARDNT_ADVERSARY_MCAFEE,               "mfeapfk" },
        { SUPHARDNT_ADVERSARY_MCAFEE,               "mfeavfk" },
        { SUPHARDNT_ADVERSARY_MCAFEE,               "mfefirek" },
        { SUPHARDNT_ADVERSARY_MCAFEE,               "mfehidk" },
        { SUPHARDNT_ADVERSARY_MCAFEE,               "mfencbdc" },
        { SUPHARDNT_ADVERSARY_MCAFEE,               "mfewfpk" },

        { SUPHARDNT_ADVERSARY_KASPERSKY,            "kl1" },
        { SUPHARDNT_ADVERSARY_KASPERSKY,            "klflt" },
        { SUPHARDNT_ADVERSARY_KASPERSKY,            "klif" },
        { SUPHARDNT_ADVERSARY_KASPERSKY,            "KLIM6" },
        { SUPHARDNT_ADVERSARY_KASPERSKY,            "klkbdflt" },
        { SUPHARDNT_ADVERSARY_KASPERSKY,            "klmouflt" },
        { SUPHARDNT_ADVERSARY_KASPERSKY,            "kltdi" },
        { SUPHARDNT_ADVERSARY_KASPERSKY,            "kneps" },

        { SUPHARDNT_ADVERSARY_MBAM,                 "MBAMWebAccessControl" },
        { SUPHARDNT_ADVERSARY_MBAM,                 "mbam" },
        { SUPHARDNT_ADVERSARY_MBAM,                 "mbamchameleon" },
        { SUPHARDNT_ADVERSARY_MBAM,                 "mwav" },
        { SUPHARDNT_ADVERSARY_MBAM,                 "mbamswissarmy" },

        { SUPHARDNT_ADVERSARY_AVG,                  "avgfwfd" },
        { SUPHARDNT_ADVERSARY_AVG,                  "avgtdia" },

        { SUPHARDNT_ADVERSARY_PANDA,                "PSINAflt" },
        { SUPHARDNT_ADVERSARY_PANDA,                "PSINFile" },
        { SUPHARDNT_ADVERSARY_PANDA,                "PSINKNC" },
        { SUPHARDNT_ADVERSARY_PANDA,                "PSINProc" },
        { SUPHARDNT_ADVERSARY_PANDA,                "PSINProt" },
        { SUPHARDNT_ADVERSARY_PANDA,                "PSINReg" },
        { SUPHARDNT_ADVERSARY_PANDA,                "PSKMAD" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSAlpc" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSHttp" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNShttps" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSIds" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSNAHSL" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSpicc" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSPihsw" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSPop3" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSProt" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSPrv" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSSmtp" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNSStrm" },
        { SUPHARDNT_ADVERSARY_PANDA,                "NNStlsc" },

        { SUPHARDNT_ADVERSARY_MSE,                  "NisDrv" },

        /*{ SUPHARDNT_ADVERSARY_COMODO, "cmdguard" }, file system */
        { SUPHARDNT_ADVERSARY_COMODO, "inspect" },
        { SUPHARDNT_ADVERSARY_COMODO, "cmdHlp" },

    };

    static const struct
    {
        uint32_t    fAdversary;
        PCRTUTF16   pwszFile;
    } s_aFiles[] =
    {
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\SysPlant.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\sysfer.dll" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\sysferThunk.dll" },

        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\N360x64\\1505000.013\\ccsetx64.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\N360x64\\1505000.013\\ironx64.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\N360x64\\1505000.013\\srtsp64.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\N360x64\\1505000.013\\srtspx64.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\N360x64\\1505000.013\\symds64.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\N360x64\\1505000.013\\symefa64.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\N360x64\\1505000.013\\symelam.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\N360x64\\1505000.013\\symnets.sys" },
        { SUPHARDNT_ADVERSARY_SYMANTEC_N360, L"\\SystemRoot\\System32\\drivers\\symevent64x86.sys" },

        { SUPHARDNT_ADVERSARY_AVAST, L"\\SystemRoot\\System32\\drivers\\aswHwid.sys" },
        { SUPHARDNT_ADVERSARY_AVAST, L"\\SystemRoot\\System32\\drivers\\aswMonFlt.sys" },
        { SUPHARDNT_ADVERSARY_AVAST, L"\\SystemRoot\\System32\\drivers\\aswRdr2.sys" },
        { SUPHARDNT_ADVERSARY_AVAST, L"\\SystemRoot\\System32\\drivers\\aswRvrt.sys" },
        { SUPHARDNT_ADVERSARY_AVAST, L"\\SystemRoot\\System32\\drivers\\aswSnx.sys" },
        { SUPHARDNT_ADVERSARY_AVAST, L"\\SystemRoot\\System32\\drivers\\aswsp.sys" },
        { SUPHARDNT_ADVERSARY_AVAST, L"\\SystemRoot\\System32\\drivers\\aswStm.sys" },
        { SUPHARDNT_ADVERSARY_AVAST, L"\\SystemRoot\\System32\\drivers\\aswVmm.sys" },

        { SUPHARDNT_ADVERSARY_TRENDMICRO, L"\\SystemRoot\\System32\\drivers\\tmcomm.sys" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO, L"\\SystemRoot\\System32\\drivers\\tmactmon.sys" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO, L"\\SystemRoot\\System32\\drivers\\tmevtmgr.sys" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO, L"\\SystemRoot\\System32\\drivers\\tmtdi.sys" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO, L"\\SystemRoot\\System32\\drivers\\tmebc64.sys" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO, L"\\SystemRoot\\System32\\drivers\\tmeevw.sys" },
        { SUPHARDNT_ADVERSARY_TRENDMICRO, L"\\SystemRoot\\System32\\drivers\\tmciesc.sys" },

        { SUPHARDNT_ADVERSARY_MCAFEE, L"\\SystemRoot\\System32\\drivers\\cfwids.sys" },
        { SUPHARDNT_ADVERSARY_MCAFEE, L"\\SystemRoot\\System32\\drivers\\McPvDrv.sys" },
        { SUPHARDNT_ADVERSARY_MCAFEE, L"\\SystemRoot\\System32\\drivers\\mfeapfk.sys" },
        { SUPHARDNT_ADVERSARY_MCAFEE, L"\\SystemRoot\\System32\\drivers\\mfeavfk.sys" },
        { SUPHARDNT_ADVERSARY_MCAFEE, L"\\SystemRoot\\System32\\drivers\\mfefirek.sys" },
        { SUPHARDNT_ADVERSARY_MCAFEE, L"\\SystemRoot\\System32\\drivers\\mfehidk.sys" },
        { SUPHARDNT_ADVERSARY_MCAFEE, L"\\SystemRoot\\System32\\drivers\\mfencbdc.sys" },
        { SUPHARDNT_ADVERSARY_MCAFEE, L"\\SystemRoot\\System32\\drivers\\mfewfpk.sys" },

        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\drivers\\kl1.sys" },
        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\drivers\\klflt.sys" },
        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\drivers\\klif.sys" },
        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\drivers\\klim6.sys" },
        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\drivers\\klkbdflt.sys" },
        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\drivers\\klmouflt.sys" },
        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\drivers\\kltdi.sys" },
        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\drivers\\kneps.sys" },
        { SUPHARDNT_ADVERSARY_KASPERSKY, L"\\SystemRoot\\System32\\klfphc.dll" },

        { SUPHARDNT_ADVERSARY_MBAM, L"\\SystemRoot\\System32\\drivers\\MBAMSwissArmy.sys" },
        { SUPHARDNT_ADVERSARY_MBAM, L"\\SystemRoot\\System32\\drivers\\mwac.sys" },
        { SUPHARDNT_ADVERSARY_MBAM, L"\\SystemRoot\\System32\\drivers\\mbamchameleon.sys" },
        { SUPHARDNT_ADVERSARY_MBAM, L"\\SystemRoot\\System32\\drivers\\mbam.sys" },

        { SUPHARDNT_ADVERSARY_AVG, L"\\SystemRoot\\System32\\drivers\\avgrkx64.sys" },
        { SUPHARDNT_ADVERSARY_AVG, L"\\SystemRoot\\System32\\drivers\\avgmfx64.sys" },
        { SUPHARDNT_ADVERSARY_AVG, L"\\SystemRoot\\System32\\drivers\\avgidsdrivera.sys" },
        { SUPHARDNT_ADVERSARY_AVG, L"\\SystemRoot\\System32\\drivers\\avgidsha.sys" },
        { SUPHARDNT_ADVERSARY_AVG, L"\\SystemRoot\\System32\\drivers\\avgtdia.sys" },
        { SUPHARDNT_ADVERSARY_AVG, L"\\SystemRoot\\System32\\drivers\\avgloga.sys" },
        { SUPHARDNT_ADVERSARY_AVG, L"\\SystemRoot\\System32\\drivers\\avgldx64.sys" },
        { SUPHARDNT_ADVERSARY_AVG, L"\\SystemRoot\\System32\\drivers\\avgdiska.sys" },

        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\PSINAflt.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\PSINFile.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\PSINKNC.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\PSINProc.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\PSINProt.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\PSINReg.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\PSKMAD.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSAlpc.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSHttp.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNShttps.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSIds.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSNAHSL.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSpicc.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSPihsw.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSPop3.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSProt.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSPrv.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSSmtp.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNSStrm.sys" },
        { SUPHARDNT_ADVERSARY_PANDA, L"\\SystemRoot\\System32\\drivers\\NNStlsc.sys" },

        { SUPHARDNT_ADVERSARY_MSE, L"\\SystemRoot\\System32\\drivers\\MpFilter.sys" },
        { SUPHARDNT_ADVERSARY_MSE, L"\\SystemRoot\\System32\\drivers\\NisDrvWFP.sys" },

        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\drivers\\cmdguard.sys" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\drivers\\cmderd.sys" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\drivers\\inspect.sys" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\drivers\\cmdhlp.sys" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\drivers\\cfrmd.sys" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\drivers\\hmd.sys" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\guard64.dll" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\cmdvrt64.dll" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\cmdkbd64.dll" },
        { SUPHARDNT_ADVERSARY_COMODO, L"\\SystemRoot\\System32\\cmdcsr.dll" },

        { SUPHARDNT_ADVERSARY_ZONE_ALARM, L"\\SystemRoot\\System32\\drivers\\vsdatant.sys" },
        { SUPHARDNT_ADVERSARY_ZONE_ALARM, L"\\SystemRoot\\System32\\AntiTheftCredentialProvider.dll" },
    };

    uint32_t fFound = 0;

    /*
     * Open the driver object directory.
     */
    UNICODE_STRING NtDirName = RTNT_CONSTANT_UNISTR(L"\\Driver");

    OBJECT_ATTRIBUTES ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &NtDirName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

    HANDLE hDir;
    NTSTATUS rcNt = NtOpenDirectoryObject(&hDir, DIRECTORY_QUERY | FILE_LIST_DIRECTORY, &ObjAttr);
#ifdef VBOX_STRICT
    SUPR3HARDENED_ASSERT_NT_SUCCESS(rcNt);
#endif
    if (NT_SUCCESS(rcNt))
    {
        /*
         * Enumerate it, looking for the driver.
         */
        ULONG    uObjDirCtx = 0;
        for (;;)
        {
            uint32_t    abBuffer[_64K + _1K];
            ULONG       cbActual;
            rcNt = NtQueryDirectoryObject(hDir,
                                          abBuffer,
                                          sizeof(abBuffer) - 4, /* minus four for string terminator space. */
                                          FALSE /*ReturnSingleEntry */,
                                          FALSE /*RestartScan*/,
                                          &uObjDirCtx,
                                          &cbActual);
            if (!NT_SUCCESS(rcNt) || cbActual < sizeof(OBJECT_DIRECTORY_INFORMATION))
                break;

            POBJECT_DIRECTORY_INFORMATION pObjDir = (POBJECT_DIRECTORY_INFORMATION)abBuffer;
            while (pObjDir->Name.Length != 0)
            {
                WCHAR wcSaved = pObjDir->Name.Buffer[pObjDir->Name.Length / sizeof(WCHAR)];
                pObjDir->Name.Buffer[pObjDir->Name.Length / sizeof(WCHAR)] = '\0';

                for (uint32_t i = 0; i < RT_ELEMENTS(s_aDrivers); i++)
                    if (RTUtf16ICmpAscii(pObjDir->Name.Buffer, s_aDrivers[i].pszDriver) == 0)
                    {
                        fFound |= s_aDrivers[i].fAdversary;
                        SUP_DPRINTF(("Found driver %s (%#x)\n", s_aDrivers[i].pszDriver, s_aDrivers[i].fAdversary));
                        break;
                    }

                pObjDir->Name.Buffer[pObjDir->Name.Length / sizeof(WCHAR)] = wcSaved;

                /* Next directory entry. */
                pObjDir++;
            }
        }

        NtClose(hDir);
    }
    else
        SUP_DPRINTF(("NtOpenDirectoryObject failed on \\Driver: %#x\n", rcNt));

    /*
     * Look for files.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aFiles); i++)
    {
        HANDLE              hFile  = RTNT_INVALID_HANDLE_VALUE;
        IO_STATUS_BLOCK     Ios    = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        UNICODE_STRING      UniStrName;
        UniStrName.Buffer = (WCHAR *)s_aFiles[i].pwszFile;
        UniStrName.Length = (USHORT)(RTUtf16Len(s_aFiles[i].pwszFile) * sizeof(WCHAR));
        UniStrName.MaximumLength = UniStrName.Length + sizeof(WCHAR);
        InitializeObjectAttributes(&ObjAttr, &UniStrName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);
        rcNt = NtCreateFile(&hFile, GENERIC_READ, &ObjAttr, &Ios, NULL /* Allocation Size*/,  FILE_ATTRIBUTE_NORMAL,
                            FILE_SHARE_READ, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL /*EaBuffer*/, 0 /*EaLength*/);
        if (NT_SUCCESS(rcNt) && NT_SUCCESS(Ios.Status))
        {
            fFound |= s_aFiles[i].fAdversary;
            NtClose(hFile);
        }
    }

    /*
     * Log details.
     */
    SUP_DPRINTF(("supR3HardenedWinFindAdversaries: %#x\n", fFound));
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aFiles); i++)
        if (fFound & s_aFiles[i].fAdversary)
            supR3HardenedLogFileInfo(s_aFiles[i].pwszFile, true /* fAdversarial */);

    return fFound;
}


extern "C" int main(int argc, char **argv, char **envp);

/**
 * The executable entry point.
 *
 * This is normally taken care of by the C runtime library, but we don't want to
 * get involved with anything as complicated like the CRT in this setup.  So, we
 * it everything ourselves, including parameter parsing.
 */
extern "C" void __stdcall suplibHardenedWindowsMain(void)
{
    RTEXITCODE rcExit = RTEXITCODE_FAILURE;

    g_cSuplibHardenedWindowsMainCalls++;
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_WIN_EP_CALLED;

    /*
     * Initialize the NTDLL API wrappers. This aims at bypassing patched NTDLL
     * in all the processes leading up the VM process.
     */
    supR3HardenedWinInitImports();
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED;

    /*
     * Notify the parent process that we're probably capable of reporting our
     * own errors.
     */
    if (g_ProcParams.hEvtParent || g_ProcParams.hEvtChild)
    {
        SUPR3HARDENED_ASSERT(g_fSupEarlyVmProcessInit);
        NtSetEvent(g_ProcParams.hEvtParent, NULL);
        NtClose(g_ProcParams.hEvtParent);
        NtClose(g_ProcParams.hEvtChild);
        g_ProcParams.hEvtParent = NULL;
        g_ProcParams.hEvtChild  = NULL;
    }
    else
        SUPR3HARDENED_ASSERT(!g_fSupEarlyVmProcessInit);

    /*
     * After having resolved imports we patch the LdrInitializeThunk code so
     * that it's more difficult to invade our privacy by CreateRemoteThread.
     * We'll re-enable this after opening the driver or temporarily while respawning.
     */
    supR3HardenedWinDisableThreadCreation();

    /*
     * Init g_uNtVerCombined. (The code is shared with SUPR3.lib and lives in
     * SUPHardenedVerfiyImage-win.cpp.)
     */
    supR3HardenedWinInitVersion();
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_WIN_VERSION_INITIALIZED;

    /*
     * Convert the arguments to UTF-8 and open the log file if specified.
     * This must be done as early as possible since the code below may fail.
     */
    PUNICODE_STRING pCmdLineStr = &NtCurrentPeb()->ProcessParameters->CommandLine;
    int    cArgs;
    char **papszArgs = suplibCommandLineToArgvWStub(pCmdLineStr->Buffer, pCmdLineStr->Length / sizeof(WCHAR), &cArgs);

    supR3HardenedOpenLog(&cArgs, papszArgs);

    /*
     * Log information about important system files.
     */
    supR3HardenedLogFileInfo(L"\\SystemRoot\\System32\\ntdll.dll", false /* fAdversarial */);
    supR3HardenedLogFileInfo(L"\\SystemRoot\\System32\\kernel32.dll", false /* fAdversarial */);
    supR3HardenedLogFileInfo(L"\\SystemRoot\\System32\\KernelBase.dll", false /* fAdversarial */);
    supR3HardenedLogFileInfo(L"\\SystemRoot\\System32\\apisetschema.dll", false /* fAdversarial */);

    /*
     * Scan the system for adversaries, logging information about them.
     */
    g_fSupAdversaries = supR3HardenedWinFindAdversaries();

    /*
     * Get the executable name.
     */
    DWORD cwcExecName = GetModuleFileNameW(GetModuleHandleW(NULL), g_wszSupLibHardenedExePath,
                                           RT_ELEMENTS(g_wszSupLibHardenedExePath));
    if (cwcExecName >= RT_ELEMENTS(g_wszSupLibHardenedExePath))
        supR3HardenedFatalMsg("suplibHardenedWindowsMain", kSupInitOp_Integrity, VERR_BUFFER_OVERFLOW,
                              "The executable path is too long.");

    /* The NT version. */
    HANDLE hFile = CreateFileW(g_wszSupLibHardenedExePath, GENERIC_READ, FILE_SHARE_READ, NULL /*pSecurityAttributes*/,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL /*hTemplateFile*/);
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE)
        supR3HardenedFatalMsg("suplibHardenedWindowsMain", kSupInitOp_Integrity, RTErrConvertFromWin32(RtlGetLastWin32Error()),
                              "Error opening the executable: %u (%ls).", RtlGetLastWin32Error());
    RT_ZERO(g_SupLibHardenedExeNtPath);
    ULONG cbIgn;
    NTSTATUS rcNt = NtQueryObject(hFile, ObjectNameInformation, &g_SupLibHardenedExeNtPath,
                                  sizeof(g_SupLibHardenedExeNtPath) - sizeof(WCHAR), &cbIgn);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatalMsg("suplibHardenedWindowsMain", kSupInitOp_Integrity, RTErrConvertFromNtStatus(rcNt),
                              "NtQueryObject -> %#x (on %ls)\n", rcNt, g_wszSupLibHardenedExePath);
    NtClose(hFile);

    /* The NT executable name offset / dir path length. */
    g_offSupLibHardenedExeNtName = g_SupLibHardenedExeNtPath.UniStr.Length / sizeof(WCHAR);
    while (   g_offSupLibHardenedExeNtName > 1
           && g_SupLibHardenedExeNtPath.UniStr.Buffer[g_offSupLibHardenedExeNtName - 1] != '\\' )
        g_offSupLibHardenedExeNtName--;

    /*
     * Call the C/C++ main function.
     */
    SUP_DPRINTF(("Calling main()\n"));
    rcExit = (RTEXITCODE)main(cArgs, papszArgs, NULL);

    /*
     * Exit the process (never return).
     */
    SUP_DPRINTF(("Terminating the normal way: rcExit=%d\n", rcExit));
    suplibHardenedExit(rcExit);
}


/**
 * Reports an error to the parent process via the process parameter structure.
 *
 * @param   rc                  The status code to report.
 * @param   pszFormat           The format string.
 * @param   va                  The format arguments.
 */
DECLHIDDEN(void) supR3HardenedWinReportErrorToParent(int rc, const char *pszFormat, va_list va)
{
    RTStrPrintfV(g_ProcParams.szErrorMsg, sizeof(g_ProcParams.szErrorMsg), pszFormat, va);
    g_ProcParams.rc = RT_SUCCESS(rc) ? VERR_INTERNAL_ERROR_2 : rc;

    NTSTATUS rcNt = NtSetEvent(g_ProcParams.hEvtParent, NULL);
    if (NT_SUCCESS(rcNt))
    {
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -300000000; /* 30 second */
        NTSTATUS rcNt = NtWaitForSingleObject(g_ProcParams.hEvtChild, FALSE /*Alertable*/, &Timeout);
        NtClearEvent(g_ProcParams.hEvtChild);
    }
}


/**
 * Routine called by the supR3HardenedVmProcessInitThunk assembly routine when
 * LdrInitializeThunk is executed in during process initialization.
 *
 * This initializes the VM process, hooking NTDLL APIs and opening the device
 * driver before any other DLLs gets loaded into the process.  This greately
 * reduces and controls the trusted code base of the process compared to the
 * opening it from SUPR3HardenedMain, avoid issues with so call protection
 * software that is in the habit of patching half of the ntdll and kernel32
 * APIs in the process, making it almost indistinguishable from software that is
 * up to no good.  Once we've opened vboxdrv, the process should be locked down
 * so thighly that only kernel software and csrss can mess with the process.
 */
DECLASM(uintptr_t) supR3HardenedVmProcessInit(void)
{
    /*
     * Only let the first thread thru.
     */
    if (!ASMAtomicCmpXchgU32((uint32_t volatile *)&g_enmSupR3HardenedMainState,
                             SUPR3HARDENEDMAINSTATE_WIN_VM_INIT_CALLED,
                             SUPR3HARDENEDMAINSTATE_NOT_YET_CALLED))
    {
        NtTerminateThread(0, 0);
        return 0x22;  /* crash */
    }
    g_fSupEarlyVmProcessInit = true;

    /*
     * Initialize the NTDLL imports that we consider usable before the
     * process has been initialized.
     */
    supR3HardenedWinInitImportsEarly(g_ProcParams.uNtDllAddr);
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_WIN_EARLY_IMPORTS_RESOLVED;

    /*
     * Init g_uNtVerCombined as well as we can at this point.
     */
    supR3HardenedWinInitVersion();

    /*
     * Wait on the parent process to dispose of the full access process and
     * thread handles.
     */
    LARGE_INTEGER Timeout;
    Timeout.QuadPart = -600000000; /* 60 second */
    NTSTATUS rcNt = NtWaitForSingleObject(g_ProcParams.hEvtChild, FALSE /*Alertable*/, &Timeout);
    if (NT_SUCCESS(rcNt))
        rcNt = NtClearEvent(g_ProcParams.hEvtChild);
    if (!NT_SUCCESS(rcNt))
    {
        NtTerminateProcess(NtCurrentProcess(), 0x42);
        return 0x42; /* crash */
    }

    /*
     * Convert the arguments to UTF-8 so we can open the log file if specified.
     * Note! This leaks memory at present.
     */
    PUNICODE_STRING pCmdLineStr = &NtCurrentPeb()->ProcessParameters->CommandLine;
    int    cArgs;
    char **papszArgs = suplibCommandLineToArgvWStub(pCmdLineStr->Buffer, pCmdLineStr->Length / sizeof(WCHAR), &cArgs);
    supR3HardenedOpenLog(&cArgs, papszArgs);
    SUP_DPRINTF(("supR3HardenedVmProcessInit: uNtDllAddr=%p\n", g_ProcParams.uNtDllAddr));

    /*
     * Determine the executable path and name.  Will NOT determine the windows style
     * executable path here as we don't need it.
     */
    SIZE_T cbActual = 0;
    rcNt = NtQueryVirtualMemory(NtCurrentProcess(), &g_ProcParams, MemorySectionName, &g_SupLibHardenedExeNtPath,
                                sizeof(g_SupLibHardenedExeNtPath) - sizeof(WCHAR), &cbActual);
    if (   !NT_SUCCESS(rcNt)
        || g_SupLibHardenedExeNtPath.UniStr.Length == 0
        || g_SupLibHardenedExeNtPath.UniStr.Length & 1)
        supR3HardenedFatal("NtQueryVirtualMemory/MemorySectionName failed in supR3HardenedVmProcessInit: %#x\n", rcNt);

    /* The NT executable name offset / dir path length. */
    g_offSupLibHardenedExeNtName = g_SupLibHardenedExeNtPath.UniStr.Length / sizeof(WCHAR);
    while (   g_offSupLibHardenedExeNtName > 1
           && g_SupLibHardenedExeNtPath.UniStr.Buffer[g_offSupLibHardenedExeNtName - 1] != '\\' )
        g_offSupLibHardenedExeNtName--;

    /*
     * Initialize the image verification stuff (hooks LdrLoadDll and NtCreateSection).
     */
    supR3HardenedWinInit(0, false /*fAvastKludge*/);

    /*
     * Open the driver.
     */
    SUP_DPRINTF(("supR3HardenedVmProcessInit: Opening vboxdrv...\n"));
    supR3HardenedMainOpenDevice();
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_WIN_EARLY_DEVICE_OPENED;

    /*
     * Restore the LdrInitializeThunk code so we can initialize the process
     * normally when we return.
     */
    SUP_DPRINTF(("supR3HardenedVmProcessInit: Restoring LdrIntiailizeThunk...\n"));
    PSUPHNTLDRCACHEENTRY pLdrEntry;
    int rc = supHardNtLdrCacheOpen("ntdll.dll", &pLdrEntry);
    if (RT_FAILURE(rc))
        supR3HardenedFatal("supR3HardenedVmProcessInit: supHardNtLdrCacheOpen failed on NTDLL: %Rrc\n", rc);

    uint8_t *pbBits;
    rc = supHardNtLdrCacheEntryGetBits(pLdrEntry, &pbBits, g_ProcParams.uNtDllAddr, NULL, NULL, NULL /*pErrInfo*/);
    if (RT_FAILURE(rc))
        supR3HardenedFatal("supR3HardenedVmProcessInit: supHardNtLdrCacheEntryGetBits failed on NTDLL: %Rrc\n", rc);

    RTLDRADDR uValue;
    rc = RTLdrGetSymbolEx(pLdrEntry->hLdrMod, pbBits, g_ProcParams.uNtDllAddr, UINT32_MAX, "LdrInitializeThunk", &uValue);
    if (RT_FAILURE(rc))
        supR3HardenedFatal("supR3HardenedVmProcessInit: Failed to find LdrInitializeThunk (%Rrc).\n", rc);

    PVOID pvLdrInitThunk = (PVOID)(uintptr_t)uValue;
    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pvLdrInitThunk, 16, PAGE_EXECUTE_READWRITE));
    memcpy(pvLdrInitThunk, pbBits + ((uintptr_t)uValue - g_ProcParams.uNtDllAddr), 16);
    SUPR3HARDENED_ASSERT_NT_SUCCESS(supR3HardenedWinProtectMemory(pvLdrInitThunk, 16, PAGE_EXECUTE_READ));

    SUP_DPRINTF(("supR3HardenedVmProcessInit: Returning to LdrIntiailizeThunk...\n"));
    return (uintptr_t)pvLdrInitThunk;
}

