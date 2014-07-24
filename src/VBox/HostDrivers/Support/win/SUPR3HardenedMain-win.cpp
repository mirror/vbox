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
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/zero.h>

#include "SUPLibInternal.h"
#include "win/SUPHardenedVerify-win.h"
#include "../SUPDrvIOC.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The first argument of a respawed stub when respawned for the first time.
 * This just needs to be unique enough to avoid most confusion with real
 * executable names,  there are other checks in place to make sure we've respanwed. */
#define SUPR3_RESPAWN_1_ARG0  "81954AF5-4D2F-31EB-A142-B7AF187A1C41-suplib-2ndchild"

/** The first argument of a respawed stub when respawned for the second time.
 * This just needs to be unique enough to avoid most confusion with real
 * executable names,  there are other checks in place to make sure we've respanwed. */
#define SUPR3_RESPAWN_2_ARG0  "81954AF5-4D2F-31EB-A142-B7AF187A1C41-suplib-3rdchild"

/** Unconditional assertion. */
#define SUPR3HARDENED_ASSERT(a_Expr) \
    do { \
        if (!(a_Expr)) \
            supR3HardenedFatal("%s: %s", __FUNCTION__, #a_Expr); \
    } while (0)

/** Unconditional assertion of NT_SUCCESS. */
#define SUPR3HARDENED_ASSERT_NT_SUCCESS(a_Expr) \
    do { \
        NTSTATUS rcNtAssert = (a_Expr); \
        if (!NT_SUCCESS(rcNtAssert)) \
            supR3HardenedFatal("%s: %s -> %#x", __FUNCTION__, #a_Expr, rcNtAssert); \
    } while (0)

/** Unconditional assertion of a WIN32 API returning non-FALSE. */
#define SUPR3HARDENED_ASSERT_WIN32_SUCCESS(a_Expr) \
    do { \
        BOOL fRcAssert = (a_Expr); \
        if (fRcAssert == FALSE) \
            supR3HardenedFatal("%s: %s -> %#x", __FUNCTION__, #a_Expr, GetLastError()); \
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
    /** The file handle. */
    HANDLE                  hFile;
    /** If fIndexNumber is set, this is an file system internal file identifier. */
    LARGE_INTEGER           IndexNumber;
    /** The path hash value. */
    uint32_t                uHash;
    /** The verification result. */
    int                     rc;
    /** Whether IndexNumber is valid  */
    bool                    fIndexNumberValid;
    /** cwcPath * sizeof(RTUTF16). */
    uint16_t                cbPath;
    /** The full path of this entry (variable size).  */
    RTUTF16                 wszPath[1];
} VERIFIERCACHEENTRY;
/** Pointer to an image verifier path entry. */
typedef VERIFIERCACHEENTRY *PVERIFIERCACHEENTRY;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
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
/** The hash table of verifier cache . */
static VERIFIERCACHEENTRY * volatile g_apVerifierCache[128];
/** @ */

/** Static error info structure used during init. */
static RTERRINFOSTATIC      g_ErrInfoStatic;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
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
 * Allocate zero filled memory on the heap.
 *
 * @returns Pointer to the memory.  Will never return NULL, triggers a fatal
 *          error instead.
 * @param   cb                  The number of bytes to allocate.
 */
DECLHIDDEN(void *) suplibHardenedAllocZ(size_t cb)
{
    void *pv = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
    if (!pv)
        supR3HardenedFatal("HeapAlloc failed to allocate %zu bytes.\n", cb);
    return pv;
}


/**
 * Reallocates memory on the heap.
 *
 * @returns Pointer to the resized memory block.  Will never return NULL,
 *          triggers a fatal error instead.
 * @param   pvOld               The old memory block.
 * @param   cbNew               The new block size.
 */
DECLHIDDEN(void *) suplibHardenedReAlloc(void *pvOld, size_t cbNew)
{
    if (!pvOld)
        return suplibHardenedAllocZ(cbNew);
    void *pv = HeapReAlloc(GetProcessHeap(), 0 /*dwFlags*/, pvOld, cbNew);
    if (!pv)
        supR3HardenedFatal("HeapReAlloc failed to allocate %zu bytes.\n", cbNew);
    return pv;
}


/**
 * Frees memory allocated by suplibHardenedAlloc, suplibHardenedAllocZ or
 * suplibHardenedReAlloc.
 *
 * @param   pv                  Pointer to the memeory to be freed.
 */
DECLHIDDEN(void) suplibHardenedFree(void *pv)
{
    if (pv)
        HeapFree(GetProcessHeap(), 0 /* dwFlags*/, pv);
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
            && GetLastError() == ERROR_INVALID_PARAMETER)
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
 * Calculates the hash value for the given UTF-16 string.
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
        uHash = wc + (uHash << 6) + (uHash << 16) - uHash;
    }
    return uHash;
}


/**
 * Inserts the given verifier result into the cache.
 *
 * @param   pUniStr             The full path of the image.
 * @param   hFile               The file handle - must either be entered into
 *                              the cache or closed.
 * @param   rc                  The verifier result.
 * @param   fCacheable          Whether this is a cacheable result.  Passed in
 *                              here instead of being handled by the caller to
 *                              save code duplication.
 */
static void supR3HardenedWinVerifyCacheInsert(PCUNICODE_STRING pUniStr, HANDLE hFile, int rc, bool fCacheable)
{
    /*
     * Don't cache anything until we've got the WinVerifyTrust API up and running.
     */
    if (   g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_VERIFY_TRUST_READY
        && fCacheable)
    {
        /*
         * Allocate and initalize a new entry.
         */
        PVERIFIERCACHEENTRY pEntry = (PVERIFIERCACHEENTRY)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                                                    sizeof(VERIFIERCACHEENTRY) + pUniStr->Length);
        if (pEntry)
        {
            pEntry->pNext   = NULL;
            pEntry->hFile   = hFile;
            pEntry->rc      = rc;
            pEntry->uHash   = supR3HardenedWinVerifyCacheHashPath(pUniStr);
            pEntry->cbPath  = pUniStr->Length;
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
                    return;
                PVERIFIERCACHEENTRY pOther = *ppEntry;
                if (!pOther)
                    continue;
                if (   pOther->uHash  == pEntry->uHash
                    && pOther->cbPath == pEntry->cbPath
                    && memcmp(pOther->wszPath, pEntry->wszPath, pEntry->cbPath) == 0)
                    break;
                ppEntry = &pOther->pNext;
            }

            /* Duplicate entry. */
#ifdef DEBUG_bird
            __debugbreak();
#endif
            HeapFree(GetProcessHeap(), 0 /* dwFlags*/, pEntry);
        }
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
            && memcmp(pCur->wszPath, pwszPath, cbPath) == 0)
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
        while ((wc = *pwszFixEnd) != '\0' && wc != '\\' && wc != '//')
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
                                   "supR3HardenedMonitor_NtCreateSection: NtQueryObject -> %#x (fImage=%d fExecMap=%d fExecProt=%d)\n",
                                   fImage, fExecMap, fExecProt);
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
                SUP_DPRINTF(("supR3HardenedMonitor_NtCreateSection: cache hit (%Rrc) on %ls\n", pCacheHit->rc, pCacheHit->wszPath));
                if (RT_SUCCESS(pCacheHit->rc))
                    return g_pfnNtCreateSectionReal(phSection, fAccess, pObjAttribs, pcbSection, fProtect, fAttribs, hFile);
                supR3HardenedError(VINF_SUCCESS, false,
                                   "supR3HardenedMonitor_NtCreateSection: cached rc=%Rrc fImage=%d fExecMap=%d fExecProt=%d %ls\n",
                                   pCacheHit->rc, fImage, fExecMap, fExecProt, uBuf.UniStr.Buffer);
                return STATUS_TRUST_FAILURE;
            }

            /*
             * On XP the loader might hand us handles with just FILE_EXECUTE and
             * SYNCRHONIZE, the means reading will fail later on.  So, we might
             * have to reopen the file here in order to validate it - annoying.
             */
            HANDLE hMyFile = NULL;
            rcNt = NtDuplicateObject(NtCurrentProcess(), hFile, NtCurrentProcess(),
                                     &hMyFile,
                                     FILE_READ_DATA | SYNCHRONIZE,
                                     0 /* Handle attributes*/, 0 /* Options */);
            if (!NT_SUCCESS(rcNt))
            {
                if (rcNt == STATUS_ACCESS_DENIED)
                {
                    HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
                    IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;

                    OBJECT_ATTRIBUTES   ObjAttr;
                    InitializeObjectAttributes(&ObjAttr, &uBuf.UniStr, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

                    rcNt = NtCreateFile(&hMyFile,
                                        FILE_READ_DATA | SYNCHRONIZE,
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
                    if (!NT_SUCCESS(rcNt))
                    {
                        supR3HardenedError(VINF_SUCCESS, false,
                                           "supR3HardenedMonitor_NtCreateSection: Failed to duplicate and open the file: rcNt=%#x hFile=%p %ls\n",
                                           rcNt, hFile, uBuf.UniStr.Buffer);
                        return rcNt;
                    }
                }
                else
                {
#ifdef DEBUG

                    supR3HardenedError(VINF_SUCCESS, false, "supR3HardenedMonitor_NtCreateSection: NtDuplicateObject(,%#x,) failed: %#x\n", hFile, rcNt);
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
                    SUP_DPRINTF(("supR3HardenedMonitor_NtCreateSection: Applying the drop-exec-kludge for '%ls'\n", uBuf.UniStr.Buffer));
                    if (fAccess & SECTION_MAP_EXECUTE)
                        fAccess = (fAccess & ~SECTION_MAP_EXECUTE) | SECTION_MAP_READ;
                    if (fProtect & PAGE_EXECUTE)
                        fProtect = (fProtect & ~PAGE_EXECUTE) | PAGE_READONLY;
                    fProtect = (fProtect & ~UINT32_C(0xf0)) | ((fProtect & UINT32_C(0xe0)) >> 4);
                    return g_pfnNtCreateSectionReal(phSection, fAccess, pObjAttribs, pcbSection, fProtect, fAttribs, hFile);
                }
            }

            /*
             * Check the path.  We don't allow DLLs to be loaded from just anywhere:
             *      1. System32   - normal code or cat signing.
             *      2. WinSxS     - normal code or cat signing.
             *      3. VirtualBox - kernel code signing and integrity checks.
             */
            bool fSystem32 = false;
            Assert(g_SupLibHardenedExeNtPath.UniStr.Buffer[g_offSupLibHardenedExeNtName - 1] == '\\');
            uint32_t fFlags = 0;
            if (   uBuf.UniStr.Length > g_System32NtPath.UniStr.Length
                && memcmp(uBuf.UniStr.Buffer, g_System32NtPath.UniStr.Buffer, g_System32NtPath.UniStr.Length) == 0
                && uBuf.UniStr.Buffer[g_System32NtPath.UniStr.Length / sizeof(WCHAR)] == '\\')
            {
                fSystem32 = true;
                fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION;
            }
            else if (   uBuf.UniStr.Length > g_WinSxSNtPath.UniStr.Length
                     && memcmp(uBuf.UniStr.Buffer, g_WinSxSNtPath.UniStr.Buffer, g_WinSxSNtPath.UniStr.Length) == 0
                     && uBuf.UniStr.Buffer[g_WinSxSNtPath.UniStr.Length / sizeof(WCHAR)] == '\\')
                fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION;
            else if (   uBuf.UniStr.Length > g_offSupLibHardenedExeNtName
                     && memcmp(uBuf.UniStr.Buffer, g_SupLibHardenedExeNtPath.UniStr.Buffer,
                               g_offSupLibHardenedExeNtName * sizeof(WCHAR)) == 0)
                fFlags |= SUPHNTVI_F_REQUIRE_KERNEL_CODE_SIGNING | SUPHNTVI_F_REQUIRE_SIGNATURE_ENFORCEMENT;
#ifdef VBOX_PERMIT_MORE
            else if (supHardViIsAppPatchDir(uBuf.UniStr.Buffer, uBuf.UniStr.Length / sizeof(WCHAR)))
                fFlags |= SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION;
#endif
#ifdef VBOX_PERMIT_VISUAL_STUDIO_PROFILING
            /* Hack to allow profiling our code with Visual Studio. */
            else if (   uBuf.UniStr.Length > sizeof(L"\\SamplingRuntime.dll")
                     && memcmp(uBuf.UniStr.Buffer + (uBuf.UniStr.Length - sizeof(L"\\SamplingRuntime.dll") + sizeof(WCHAR)) / sizeof(WCHAR),
                               L"\\SamplingRuntime.dll", sizeof(L"\\SamplingRuntime.dll") - sizeof(WCHAR)) == 0 )
            {
                if (hMyFile != hFile)
                    NtClose(hMyFile);
                return g_pfnNtCreateSectionReal(phSection, fAccess, pObjAttribs, pcbSection, fProtect, fAttribs, hFile);
            }
#endif
            else
            {
                supR3HardenedError(VINF_SUCCESS, false,
                                   "supR3HardenedMonitor_NtCreateSection: Not a trusted location: '%ls' (fImage=%d fExecMap=%d fExecProt=%d)\n",
                                    uBuf.UniStr.Buffer, fImage, fExecMap, fExecProt);
                if (hMyFile != hFile)
                    NtClose(hMyFile);
                return STATUS_TRUST_FAILURE;
            }

            /*
             * Do the verification. For better error message we borrow what's
             * left of the path buffer for an RTERRINFO buffer.
             */
            RTERRINFO ErrInfo;
            RTErrInfoInit(&ErrInfo, (char *)&uBuf.abBuffer[cbNameBuf], sizeof(uBuf) - cbNameBuf);

            bool fCacheable = true;
            int rc = supHardenedWinVerifyImageByHandle(hMyFile, uBuf.UniStr.Buffer, fFlags, &fCacheable, &ErrInfo);
            if (RT_FAILURE(rc))
            {
                supR3HardenedError(VINF_SUCCESS, false,
                                   "supR3HardenedMonitor_NtCreateSection: rc=%Rrc fImage=%d fExecMap=%d fExecProt=%d %ls: %s\n",
                                   rc, fImage, fExecMap, fExecProt, uBuf.UniStr.Buffer, ErrInfo.pszMsg);
                if (hMyFile != hFile)
                    NtClose(hMyFile);
                return STATUS_TRUST_FAILURE;
            }
            if (hMyFile != hFile)
                supR3HardenedWinVerifyCacheInsert(&uBuf.UniStr, hMyFile, rc, fCacheable);
        }
    }

    /*
     * Call checked out OK, call the original.
     */
    return g_pfnNtCreateSectionReal(phSection, fAccess, pObjAttribs, pcbSection, fProtect, fAttribs, hFile);
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


/**
 * Install hooks for intercepting calls dealing with mapping shared libraries
 * into the process.
 *
 * This allows us to prevent undesirable shared libraries from being loaded.
 *
 * @remarks We assume we're alone in this process, so no seralizing trickery is
 *          necessary when installing the patch.
 */
DECLHIDDEN(void) supR3HardenedWinInstallHooks(void)
{
    NTSTATUS rcNt;

#ifndef VBOX_WITHOUT_DEBUGGER_CHECKS
    /*
     * Install a anti debugging hack before we continue.  This prevents most
     * notifications from ending up in the debugger.
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
     * Locate the routine.
     */
    HMODULE hmodNtDll = GetModuleHandleW(L"NTDLL");
    SUPR3HARDENED_ASSERT(hmodNtDll != NULL);

    FARPROC pfnNtCreateSection = GetProcAddress(hmodNtDll, "NtCreateSection");
    SUPR3HARDENED_ASSERT(pfnNtCreateSection != NULL);
    SUPR3HARDENED_ASSERT(pfnNtCreateSection == (FARPROC)NtCreateSection);

    uint8_t * const pbNtCreateSection = (uint8_t *)(uintptr_t)pfnNtCreateSection;

#ifdef RT_ARCH_AMD64
    /*
     * For 64-bit hosts we need some memory within a +/-2GB range of the
     * actual function to be able to patch it.
     */
    size_t cbMem = _4K;
    void  *pvMem = supR3HardenedWinAllocHookMemory((uintptr_t)pfnNtCreateSection,
                                                   (uintptr_t)pfnNtCreateSection - _2G + PAGE_SIZE,
                                                   -1, cbMem);
    if (!pvMem)
    {
        pvMem = supR3HardenedWinAllocHookMemory((uintptr_t)pfnNtCreateSection,
                                                (uintptr_t)pfnNtCreateSection + _2G - PAGE_SIZE,
                                                1, cbMem);
        if (!pvMem)
            supR3HardenedFatalMsg("supR3HardenedWinInstallHooks", kSupInitOp_Misc, VERR_NO_MEMORY,
                                  "Failed to allocate memory within the +/-2GB range from NTDLL.\n");
    }
    uintptr_t *puJmpTab = (uintptr_t *)pvMem;

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

    if (pfnCallReal)
    {
        g_pfnNtCreateSectionJmpBack         = (PFNRT)(uintptr_t)(pbNtCreateSection + offJmpBack);
        *(PFNRT *)&g_pfnNtCreateSectionReal = pfnCallReal;
        *puJmpTab                           = (uintptr_t)supR3HardenedMonitor_NtCreateSection;

        DWORD dwOldProt;
        SUPR3HARDENED_ASSERT_WIN32_SUCCESS(VirtualProtectEx(NtCurrentProcess(), pbNtCreateSection, 16,
                                                            PAGE_EXECUTE_READWRITE, &dwOldProt));
        pbNtCreateSection[0] = 0xff;
        pbNtCreateSection[1] = 0x25;
        *(uint32_t *)&pbNtCreateSection[2] = (uint32_t)((uintptr_t)puJmpTab - (uintptr_t)&pbNtCreateSection[2+4]);

        SUPR3HARDENED_ASSERT_WIN32_SUCCESS(VirtualProtectEx(NtCurrentProcess(), pbNtCreateSection, 16,
                                                            PAGE_EXECUTE_READ, &dwOldProt));
        return;
    }

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
           (   (   pbNtCreateSection[ 5] == 0xba /* mov edx, offset SharedUserData!SystemCallStub */
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

    if (pfnCallReal)
    {
        g_pfnNtCreateSectionJmpBack         = (PFNRT)(uintptr_t)(pbNtCreateSection + offJmpBack);
        *(PFNRT *)&g_pfnNtCreateSectionReal = pfnCallReal;

        DWORD dwOldProt;
        SUPR3HARDENED_ASSERT_WIN32_SUCCESS(VirtualProtectEx(NtCurrentProcess(), pbNtCreateSection, 16,
                                                            PAGE_EXECUTE_READWRITE, &dwOldProt));
        pbNtCreateSection[0] = 0xe9;
        *(uint32_t *)&pbNtCreateSection[1] = (uintptr_t)supR3HardenedMonitor_NtCreateSection
                                           - (uintptr_t)&pbNtCreateSection[1+4];

        SUPR3HARDENED_ASSERT_WIN32_SUCCESS(VirtualProtectEx(NtCurrentProcess(), pbNtCreateSection, 16,
                                                            PAGE_EXECUTE_READ, &dwOldProt));
        return;
    }
#endif

    supR3HardenedFatalMsg("supR3HardenedWinInstallHooks", kSupInitOp_Misc, VERR_NO_MEMORY,
                          "Failed to install NtCreateSection monitor: %x %x %x %x  %x %x %x %x  %x %x %x %x  %x %x %x %x\n "
#ifdef RT_ARCH_X86
                          "(It is also possible you are running 32-bit VirtualBox under 64-bit windows.)\n"
#endif
                          ,
                          pbNtCreateSection[0],  pbNtCreateSection[1],  pbNtCreateSection[2],  pbNtCreateSection[3],
                          pbNtCreateSection[4],  pbNtCreateSection[5],  pbNtCreateSection[6],  pbNtCreateSection[7],
                          pbNtCreateSection[8],  pbNtCreateSection[9],  pbNtCreateSection[10], pbNtCreateSection[11],
                          pbNtCreateSection[12], pbNtCreateSection[13], pbNtCreateSection[14], pbNtCreateSection[15]);
}


/**
 * Verifies the process integrity.
 */
DECLHIDDEN(void) supR3HardenedWinVerifyProcess(void)
{
    RTErrInfoInitStatic(&g_ErrInfoStatic);
    int rc = supHardenedWinVerifyProcess(NtCurrentProcess(), NtCurrentThread(), &g_ErrInfoStatic.Core);
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

    ULONG fDeny  = DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL;
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
    PSECURITY_DESCRIPTOR pSecDesc = (PSECURITY_DESCRIPTOR)suplibHardenedAllocZ(SECURITY_DESCRIPTOR_MIN_LENGTH);
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

    PRTUTF16 pwszCmdLine = (PRTUTF16)HeapAlloc(GetProcessHeap(), 0 /* dwFlags*/, (cwcCmdLine + 1) * sizeof(RTUTF16));
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
    PVOID pvLdrInitThunk      = (PVOID)((uintptr_t)LdrInitializeThunk + pThis->uNtDllAddr - pThis->uNtDllParentAddr);
    PVOID pvNtTerminateThread = (PVOID)((uintptr_t)NtTerminateThread  + pThis->uNtDllAddr - pThis->uNtDllParentAddr);
    SUP_DPRINTF(("supR3HardNtPuChTriggerInitialImageEvents: pvLdrInitThunk=%p pvNtTerminateThread=%p\n",
                 pvLdrInitThunk, pvNtTerminateThread));

    /*
     * Back up the thunk code.
     */
    uint8_t abBackup[16];
    SIZE_T  cbIgnored;
    NTSTATUS rcNt = NtReadVirtualMemory(pThis->hProcess, pvLdrInitThunk, abBackup, sizeof(abBackup), &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                             "NtReadVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    /*
     * Cook up replacement code that calls NtTerminateThread.
     */
    uint8_t abReplacement[sizeof(abBackup)] ;
    memcpy(abReplacement, abBackup, sizeof(abReplacement));

#ifdef RT_ARCH_AMD64
    abReplacement[0] = 0x31;    /* xor ecx, ecx */
    abReplacement[1] = 0xc9;
    abReplacement[2] = 0x31;    /* xor edx, edx */
    abReplacement[3] = 0xd2;
    abReplacement[4] = 0xe8;    /* call near NtTerminateThread */
    *(int32_t *)&abReplacement[5] = (int32_t)((intptr_t)pvNtTerminateThread - ((intptr_t)pvLdrInitThunk + 9));
    abReplacement[9] = 0xcc;    /* int3 */
#elif defined(RT_ARCH_X86)
    abReplacement[0] = 0x6a;    /* push 0 */
    abReplacement[1] = 0x00;
    abReplacement[2] = 0x6a;    /* push 0 */
    abReplacement[3] = 0x00;
    abReplacement[4] = 0xe8;    /* call near NtTerminateThread */
    *(int32_t *)&abReplacement[5] = (int32_t)((intptr_t)pvNtTerminateThread - ((intptr_t)pvLdrInitThunk + 9));
    abReplacement[9] = 0xcc;    /* int3 */
#else
# error "Unsupported arch."
#endif

    /*
     * Install the replacment code.
     */
    PVOID  pvProt   = pvLdrInitThunk;
    SIZE_T cbProt   = 16;
    ULONG  fOldProt = 0;
    rcNt = NtProtectVirtualMemory(pThis->hProcess, &pvProt, &cbProt, PAGE_EXECUTE_READWRITE, &fOldProt);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                             "NtProtectVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    rcNt = NtWriteVirtualMemory(pThis->hProcess, pvLdrInitThunk, abReplacement, sizeof(abReplacement), &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                             "NtWriteVirtualMemory/LdrInitializeThunk failed: %#x", rcNt);

    /*
     * Create the thread, waiting 10 seconds for it to complete.
     */
    CLIENT_ID Thread2Id;
    HANDLE hThread2;
    rcNt = RtlCreateUserThread(pThis->hProcess,
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
     * Restore the original thunk code and protection.
     */
    rcNt = NtWriteVirtualMemory(pThis->hProcess, pvLdrInitThunk, abBackup, sizeof(abBackup), &cbIgnored);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                             "NtWriteVirtualMemory/LdrInitializeThunk[restore] failed: %#x", rcNt);

    pvProt   = pvLdrInitThunk;
    cbProt   = 16;
    rcNt = NtProtectVirtualMemory(pThis->hProcess, &pvProt, &cbProt, fOldProt, &fOldProt);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                             "NtProtectVirtualMemory/LdrInitializeThunk[restore] failed: %#x", rcNt);

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


DECLINLINE(PIMAGE_NT_HEADERS) supR3HardNtPuChFindNtHeaders(uint8_t *pbBuf, size_t cbBuf)
{
    PIMAGE_DOS_HEADER pMzHdr = (PIMAGE_DOS_HEADER)pbBuf;
    if (cbBuf >= sizeof(*pMzHdr))
    {
        if (pMzHdr->e_magic == IMAGE_DOS_SIGNATURE)
        {
            if (pMzHdr->e_lfanew >= cbBuf)
                return NULL;
            cbBuf -= pMzHdr->e_lfanew;
            pbBuf += pMzHdr->e_lfanew;
        }
    }

    PIMAGE_NT_HEADERS pNtHdrs = (PIMAGE_NT_HEADERS)pbBuf;
    if (cbBuf >= sizeof(IMAGE_NT_HEADERS))
    {
        if (pNtHdrs->Signature == IMAGE_NT_SIGNATURE)
            return pNtHdrs;
    }
    return NULL;
}


static uint32_t supR3HardNtPuChFindFirstDiff(void const *pvBuf1, void const *pvBuf2, size_t cbBuf)
{
    uint8_t const *pabBuf1 = (uint8_t const *)pvBuf1;
    uint8_t const *pabBuf2 = (uint8_t const *)pvBuf2;
    uint32_t off = 0;
    while (off < cbBuf && pabBuf1[off] == pabBuf2[off])
        off++;
    return off;
}


static NTSTATUS supR3HardNtPuChRestoreImageBits(PSUPR3HARDNTPUCH pThis, PVOID pvChildAddr,
                                                void const *pvFileBits, size_t cbToRestore, uint32_t fCorrectProtection)
{
    PVOID  pvProt   = pvChildAddr;
    SIZE_T cbProt   = cbToRestore;
    ULONG  fOldProt = 0;
    NTSTATUS rcNt = NtProtectVirtualMemory(pThis->hProcess, &pvProt, &cbProt, PAGE_READWRITE, &fOldProt);
    if (NT_SUCCESS(rcNt))
    {
        SIZE_T cbIgnored;
        rcNt = NtWriteVirtualMemory(pThis->hProcess, pvChildAddr, pvFileBits, cbToRestore, &cbIgnored);

        pvProt = pvChildAddr;
        cbProt = cbToRestore;
        NTSTATUS rcNt2 = NtProtectVirtualMemory(pThis->hProcess, &pvProt, &cbProt, fCorrectProtection, &fOldProt);
        if (NT_SUCCESS(rcNt))
            rcNt = rcNt2;
    }
    return rcNt;
}


static int supR3HardNtPuChSanitizeImage(PSUPR3HARDNTPUCH pThis, PMEMORY_BASIC_INFORMATION pMemInfo)
{
    /*
     * Get the image name.
     */
    union
    {
        UNICODE_STRING UniStr;
        uint8_t abPadding[4096];
    } uBuf;
    SIZE_T cbActual;
    NTSTATUS rcNt = NtQueryVirtualMemory(pThis->hProcess,
                                         pMemInfo->BaseAddress,
                                         MemorySectionName,
                                         &uBuf,
                                         sizeof(uBuf) - sizeof(WCHAR),
                                         &cbActual);
    if (!NT_SUCCESS(rcNt))
        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                             "NtQueryVirtualMemory/MemorySectionName failed for %p: %#x", pMemInfo->BaseAddress, rcNt);
    uBuf.UniStr.Buffer[uBuf.UniStr.Length / sizeof(WCHAR)] = '\0';
    SUP_DPRINTF(("supR3HardNtPuChSanitizeImage: %p '%ls'\n", pMemInfo->BaseAddress, uBuf.UniStr.Buffer));


    /*
     * Open the file.
     */
    HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;

    OBJECT_ATTRIBUTES ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &uBuf.UniStr, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

    rcNt = NtCreateFile(&hFile,
                        FILE_READ_DATA | SYNCHRONIZE,
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
        return RTErrInfoSetF(pThis->pErrInfo, RTErrConvertFromNtStatus(rcNt),
                             "NtCreateFile returned %#x opening '%ls'.", rcNt, uBuf.UniStr.Buffer);

    /*
     * Read the headers, ASSUMES the stub isn't pushing the optional header out
     * of a 4KB buffer.  Also ASSUMEs that the file is more than 4KB in size.
     */
    int             rc;
    uint8_t         abFile[_4K];
    LARGE_INTEGER   off;
    off.QuadPart = 0;
    rcNt = NtReadFile(hFile, NULL, NULL, NULL, &Ios, abFile, sizeof(abFile), &off, NULL);
    if (NT_SUCCESS(rcNt))
        rcNt = Ios.Status;
    if (NT_SUCCESS(rcNt))
    {
        PIMAGE_NT_HEADERS pNtFile = supR3HardNtPuChFindNtHeaders(abFile, sizeof(abFile));
        if (pNtFile)
        {
            /*
             * Read the first 4KB of the process memory.
             */
            uint8_t abProc[_4K];
            SIZE_T cbActualMem;
            rcNt = NtReadVirtualMemory(pThis->hProcess, pMemInfo->BaseAddress, abProc, sizeof(abProc), &cbActualMem);
            if (NT_SUCCESS(rcNt))
            {
                PIMAGE_NT_HEADERS pNtProc = (PIMAGE_NT_HEADERS)&abProc[(uint8_t *)pNtFile - &abFile[0]];
                pNtFile->OptionalHeader.ImageBase = pNtProc->OptionalHeader.ImageBase;

                size_t cbCompare = RT_MIN(pNtFile->OptionalHeader.SizeOfHeaders, sizeof(abProc));
                if (cbCompare < sizeof(abFile))
                    RT_BZERO(&abFile[cbCompare], sizeof(abFile) - cbCompare);
                if (!memcmp(abFile, abProc, cbCompare))
                    rc = VINF_SUCCESS;
                else
                {
                    SUP_DPRINTF(("supR3HardNtPuChSanitizeImage: Header diff @%#x in ('%ls')\n",
                                 supR3HardNtPuChFindFirstDiff(abFile, abProc, sizeof(abProc)), uBuf.UniStr.Buffer));
                    rc = supR3HardNtPuChRestoreImageBits(pThis, pMemInfo->BaseAddress, abFile, cbCompare, PAGE_READONLY);
                }
            }
            else
                rc = RTErrInfoSetF(pThis->pErrInfo, RTErrConvertFromNtStatus(rcNt),
                                   "NtReadVirtualMemory returned %#x read 4KB at %p ('%ls').", rcNt,
                                   pMemInfo->BaseAddress, uBuf.UniStr.Buffer);
        }
        else
            rc = RTErrInfoSetF(pThis->pErrInfo, RTErrConvertFromNtStatus(rcNt),
                               "No PE header in the first 4KB of '%ls'.", rcNt, uBuf.UniStr.Buffer);
    }
    else
        rc = RTErrInfoSetF(pThis->pErrInfo, RTErrConvertFromNtStatus(rcNt),
                           "NtReadFile returned %#x reading the header of '%ls'.", rcNt, uBuf.UniStr.Buffer);

    NtClose(hFile);

    return rc;
}


static int supR3HardNtPuChSanitizeMemory(PSUPR3HARDNTPUCH pThis)
{
    /*
     * Find and remove/disable any unwanted executable memory.
     */
    uint32_t    cXpExceptions = 0;
    uintptr_t   cbAdvance = 0;
    uintptr_t   uPtrWhere = 0;
    for (uint32_t i = 0; i < 1024; i++)
    {
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
        //SUP_DPRINTF(("supR3HardNtPuChSanitizeMemory: %p (%p LB %#zx): type=%#010x prot=%#06x state=%#07x aprot=%#06x abase=%p\n",
        //             uPtrWhere, MemInfo.BaseAddress, MemInfo.RegionSize,MemInfo.Type,
        //             MemInfo.Protect, MemInfo.State, MemInfo.AllocationBase, MemInfo.AllocationProtect));

        if (   MemInfo.Type == SEC_IMAGE
            || MemInfo.Type == SEC_PROTECTED_IMAGE
            || MemInfo.Type == (SEC_IMAGE | SEC_PROTECTED_IMAGE))
        {
            /*
             * Restore modified parts of the image from file.
             */
            if (MemInfo.BaseAddress == MemInfo.AllocationBase)
            {
                int rc = supR3HardNtPuChSanitizeImage(pThis, &MemInfo);
                if (RT_FAILURE(rc))
                    return rc;
            }
        }
        /*
         * Executable memory outside an image is evil by definition.
         */
        else if (MemInfo.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
        {
            /*
             * XP, W2K3 exception: Ignore the CSRSS read-only region as best we can.
             */
            if (   (MemInfo.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
                   == PAGE_EXECUTE_READ
                && cXpExceptions == 0
                && (uintptr_t)MemInfo.BaseAddress >= UINT32_C(0x78000000)
                /* && MemInfo.BaseAddress == pPeb->ReadOnlySharedMemoryBase */
                && g_uNtVerCombined < SUP_MAKE_NT_VER_SIMPLE(6, 0) )
                cXpExceptions++;
#ifndef VBOX_PERMIT_VISUAL_STUDIO_PROFILING
            else
            {
                /*
                 * Free any private executable memory (sysplant.sys allocates executable memory).
                 */
                if (MemInfo.Type == MEM_PRIVATE)
                {
                    SUP_DPRINTF(("supR3HardNtPuChSanitizeMemory: Freeing exec mem at %p (%p LB %#zx)\n",
                                 uPtrWhere, MemInfo.BaseAddress, MemInfo.RegionSize));
                    PVOID   pvFree = MemInfo.BaseAddress;
                    SIZE_T  cbFree = MemInfo.RegionSize;
                    rcNt = NtFreeVirtualMemory(pThis->hProcess, &pvFree, &cbFree, MEM_RELEASE);
                    if (!NT_SUCCESS(rcNt))
                        return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                                             "NtFreeVirtualMemory (%p LB %#zx) failed: %#x",
                                             MemInfo.BaseAddress, MemInfo.RegionSize, rcNt);
                }
                /*
                 * Unmap mapped memory, failing that, drop exec privileges.
                 */
                else if (MemInfo.Type == MEM_MAPPED)
                {
                    SUP_DPRINTF(("supR3HardNtPuChSanitizeMemory: Unmapping exec mem at %p (%p/%p LB %#zx)\n",
                                 uPtrWhere, MemInfo.AllocationBase, MemInfo.BaseAddress, MemInfo.RegionSize));
                    rcNt = NtUnmapViewOfSection(pThis->hProcess, MemInfo.AllocationBase);
                    if (!NT_SUCCESS(rcNt))
                    {
                        PVOID  pvCopy = MemInfo.BaseAddress;
                        SIZE_T cbCopy = MemInfo.RegionSize;
                        NTSTATUS rcNt2 = NtProtectVirtualMemory(pThis->hProcess, &pvCopy, &cbCopy, PAGE_NOACCESS, NULL);
                        if (!NT_SUCCESS(rcNt2))
                            rcNt2 = NtProtectVirtualMemory(pThis->hProcess, &pvCopy, &cbCopy, PAGE_READONLY, NULL);
                        if (!NT_SUCCESS(rcNt2))
                            return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                                                 "NtUnmapViewOfSection (%p/%p LB %#zx) failed: %#x (%#x)",
                                                 MemInfo.AllocationBase, MemInfo.BaseAddress, MemInfo.RegionSize, rcNt, rcNt2);
                    }
                }
                else
                    return RTErrInfoSetF(pThis->pErrInfo, VERR_GENERAL_FAILURE,
                                         "Unknown executable memory type %#x at %p/%p LB %#zx",
                                         MemInfo.Type, MemInfo.AllocationBase, MemInfo.BaseAddress, MemInfo.RegionSize);
            }
#endif
        }

        /*
         * Advance.
         */
        cbAdvance = MemInfo.RegionSize;
        if (uPtrWhere + cbAdvance <= uPtrWhere)
            break;
        uPtrWhere += MemInfo.RegionSize;
    }

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
                    if (   uBuf.UniStr.Length > g_System32NtPath.UniStr.Length
                        && memcmp(uBuf.UniStr.Buffer, g_System32NtPath.UniStr.Buffer, g_System32NtPath.UniStr.Length) == 0
                        && uBuf.UniStr.Buffer[g_System32NtPath.UniStr.Length / sizeof(WCHAR)] == '\\')
                    {
                        if (RTUtf16ICmpAscii(&uBuf.UniStr.Buffer[g_System32NtPath.UniStr.Length / sizeof(WCHAR) + 1],
                                             "ntdll.dll") == 0)
                        {
                            pThis->uNtDllAddr = (uintptr_t)MemInfo.AllocationBase;
                            SUP_DPRINTF(("supR3HardNtPuChFindNtdll: uNtDllParentAddr=%p uNtDllChildAddr=%p\n",
                                         pThis->uNtDllParentAddr, pThis->uNtDllAddr));
                            return;
                        }
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



static int supR3HardenedWinPurifyChild(HANDLE hProcess, HANDLE hThread, PRTERRINFO pErrInfo)
{
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

    /*
     * Do the work.
     */
    int rc = supR3HardNtPuChScrewUpPebForInitialImageEvents(&This);
    if (RT_SUCCESS(rc))
        rc = supR3HardNtPuChTriggerInitialImageEvents(&This);
    if (RT_SUCCESS(rc))
        rc = supR3HardNtPuChSanitizePeb(&This);
    if (RT_SUCCESS(rc))
        rc = supR3HardNtPuChSanitizeMemory(&This);

    return rc;
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

    SiEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    SiEx.StartupInfo.hStdInput  = pParentProcParams->StandardInput;
    SiEx.StartupInfo.hStdOutput = pParentProcParams->StandardOutput;
    SiEx.StartupInfo.hStdError  = pParentProcParams->StandardError;

    /*
     * Construct the command line and launch the process.
     */
    PRTUTF16 pwszCmdLine = supR3HardenedWinConstructCmdLine(NULL, iWhich);

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
                              GetLastError(), pwszCmdLine);

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


    /*
     * Clean up the process.
     */
    int rc = supR3HardenedWinPurifyChild(hProcess, hThread, RTErrInfoInitStatic(&g_ErrInfoStatic));
    if (RT_FAILURE(rc))
    {
        NtTerminateProcess(hProcess, DBG_TERMINATE_PROCESS);
        supR3HardenedError(rc, true /*fFatal*/, "%s", g_ErrInfoStatic.szMsg);
    }

    /*
     * Start the process execution.
     */
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
    ULONG fRights = SYNCHRONIZE;
    if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0)) /* Introduced in Vista. */
        fRights |= PROCESS_QUERY_LIMITED_INFORMATION;
    else
        fRights |= PROCESS_QUERY_INFORMATION;
    rcNt = NtDuplicateObject(NtCurrentProcess(), hProcess,
                             NtCurrentProcess(), &hProcWait,
                             fRights, 0 /*HandleAttributes*/, 0);
    if (rcNt == STATUS_ACCESS_DENIED)
        rcNt = NtDuplicateObject(NtCurrentProcess(), hProcess,
                                 NtCurrentProcess(), &hProcWait,
                                 SYNCHRONIZE, 0 /*HandleAttributes*/, 0);
    if (!NT_SUCCESS(rcNt))
    {
        /* Failure is unacceptable, kill the process. */
        NtTerminateProcess(hProcess, RTEXITCODE_FAILURE);
        supR3HardenedError(rcNt, false /*fFatal*/, "NtDuplicateObject failed on child process handle: %#x\n", rcNt);

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
                supR3HardenedError(rcNt, false /*fFatal*/,
                                   "NtDuplicateObject failed and we failed to kill child: rcNt=%u rcNtWait=%u hProcess=%p\n",
                                   rcNt, rcNtWait, hProcess);
        }
        supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Misc, VERR_INVALID_NAME,
                              "NtDuplicateObject failed on child process handle: %#x\n", rcNt);
    }

    SUPR3HARDENED_ASSERT_NT_SUCCESS(NtClose(hProcess));
    hProcess = NULL;

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

    if (hParent != NULL)
    {
        for (;;)
        {
            HANDLE ahHandles[2] = { hProcWait, hParent };
            rcNt = NtWaitForMultipleObjects(2, &ahHandles[0], WaitAnyObject, TRUE /*Alertable*/, NULL /*pTimeout*/);
            if (   rcNt == STATUS_WAIT_0
                || rcNt == STATUS_WAIT_0 + 1
                || rcNt == STATUS_ABANDONED_WAIT_0
                || rcNt == STATUS_ABANDONED_WAIT_0 + 1)
                break;
            if (   rcNt != STATUS_TIMEOUT
                && rcNt != STATUS_USER_APC
                && rcNt != STATUS_ALERTED)
                supR3HardenedFatal("NtWaitForMultipleObjects returned %#x\n", rcNt);
        }
        NtClose(hParent);
    }
    else
    {
        /*
         * Wait for the process to terminate.
         */
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
    }

    /*
     * Proxy the termination code of the child, if it exited already.
     */
    NTSTATUS rcNt2 = NtQueryInformationProcess(hProcWait, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
    if (   !NT_SUCCESS(rcNt2)
        || BasicInfo.ExitStatus == STATUS_PENDING)
        BasicInfo.ExitStatus = RTEXITCODE_FAILURE;

    NtClose(hProcWait);
    SUP_DPRINTF(("supR3HardenedWinDoReSpawn(%d): Quitting: ExitCode=%#x rcNt=%#x\n", BasicInfo.ExitStatus, rcNt));
    suplibHardenedExit((RTEXITCODE)BasicInfo.ExitStatus);
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
    UNICODE_STRING NtDirName;
    NtDirName.Buffer = L"\\Driver";
    NtDirName.MaximumLength = sizeof(L"\\Driver");
    NtDirName.Length = NtDirName.MaximumLength - sizeof(WCHAR);

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
        int rc = VERR_OPEN_FAILED;
        if (SUP_NT_STATUS_IS_VBOX(rcNt)) /* See VBoxDrvNtErr2NtStatus. */
            rc = SUP_NT_STATUS_TO_VBOX(rcNt);
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
            supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Driver, VERR_OPEN_FAILED,
                                  "NtCreateFile(%ls) failed: %#x%s (%u retries)\n", s_wszName, rcNt, pszDefine, iTry);
        }
        supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Driver, rc,
                              "NtCreateFile(%ls) failed: %Rrc (rcNt=%#x)\n", s_wszName, rc, rcNt);
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
 */
DECLHIDDEN(void) supR3HardenedWinInit(uint32_t fFlags)
{
    RTErrInfoInitStatic(&g_ErrInfoStatic);
    int rc = supHardenedWinInitImageVerifier(&g_ErrInfoStatic.Core);
    if (RT_FAILURE(rc))
        supR3HardenedFatalMsg("supR3HardenedWinInit", kSupInitOp_Misc, rc,
                              "supHardenedWinInitImageVerifier failed: %s", g_ErrInfoStatic.szMsg);

    if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV))
        supR3HardenedWinInstallHooks();

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
    char **papszArgs      = (char **)suplibHardenedAllocZ(sizeof(char *) * cArgsAllocated);
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
            papszArgs = (char **)suplibHardenedReAlloc(papszArgs, sizeof(char *) * cArgsAllocated);
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

    /*
     * Init g_uNtVerCombined. (The code is shared with SUPR3.lib and lives in
     * SUPHardenedVerfiyImage-win.cpp.)
     */
    supR3HardenedWinInitVersion();

    /*
     * Convert the arguments to UTF-8 and open the log file if specified.
     * This must be done as early as possible since the code below may fail.
     */
    PUNICODE_STRING pCmdLineStr = &NtCurrentPeb()->ProcessParameters->CommandLine;
    int    cArgs;
    char **papszArgs = suplibCommandLineToArgvWStub(pCmdLineStr->Buffer, pCmdLineStr->Length / sizeof(WCHAR), &cArgs);

    supR3HardenedOpenLog(&cArgs, papszArgs);

    /*
     * Get the executable name.
     */
    DWORD cwcExecName = GetModuleFileNameW(GetModuleHandle(NULL), g_wszSupLibHardenedExePath,
                                           RT_ELEMENTS(g_wszSupLibHardenedExePath));
    if (cwcExecName >= RT_ELEMENTS(g_wszSupLibHardenedExePath))
        supR3HardenedFatalMsg("suplibHardenedWindowsMain", kSupInitOp_Integrity, VERR_BUFFER_OVERFLOW,
                              "The executable path is too long.");

    /* The NT version. */
    HANDLE hFile = CreateFileW(g_wszSupLibHardenedExePath, GENERIC_READ, FILE_SHARE_READ, NULL /*pSecurityAttributes*/,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL /*hTemplateFile*/);
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE)
        supR3HardenedFatalMsg("suplibHardenedWindowsMain", kSupInitOp_Integrity, RTErrConvertFromWin32(GetLastError()),
                              "Error opening the executable: %u (%ls).", GetLastError());
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

