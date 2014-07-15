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

#include "SUPLibInternal.h"
#include "win/SUPHardenedVerify-win.h"
#include "../SUPDrvIOC.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The first argument of a respawed stub argument.
 * This just needs to be unique enough to avoid most confusion with real
 * executable names,  there are other checks in place to make sure we've respanwed. */
#define SUPR3_RESPAWN_ARG0  "81954AF5-4D2F-31EB-A142-B7AF187A1C41-suplib-2ndchild"

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
    HANDLE const hProc                 = GetCurrentProcess();

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
                void *pvMem = VirtualAllocEx(hProc, (void *)uCur, cbAlloc, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
                if (pvMem)
                {
                    if (  iDirection > 0
                        ?    (uintptr_t)pvMem >= uStart
                          && (uintptr_t)pvMem <= uEnd
                        :    (uintptr_t)pvMem >= uEnd
                          && (uintptr_t)pvMem <= uStart)
                        return pvMem;
                    VirtualFreeEx(hProc, pvMem, cbAlloc, MEM_RELEASE);
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
#ifndef VBOX_WITHOUT_DEBUGGER_CHECKS
    /*
     * Install a anti debugging hack before we continue.  This prevents most
     * notifications from ending up in the debugger.
     */
    NTSTATUS rcNt = NtSetInformationThread(GetCurrentThread(), ThreadHideFromDebugger, NULL, 0);
    if (!NT_SUCCESS(rcNt))
        supR3HardenedFatalMsg("supR3HardenedWinInstallHooks", kSupInitOp_Misc, VERR_NO_MEMORY,
                              "NtSetInformationThread/ThreadHideFromDebugger failed: %#x\n", rcNt);
#endif

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
        SUPR3HARDENED_ASSERT_WIN32_SUCCESS(VirtualProtectEx(GetCurrentProcess(), pbNtCreateSection, 16,
                                                            PAGE_EXECUTE_READWRITE, &dwOldProt));
        pbNtCreateSection[0] = 0xff;
        pbNtCreateSection[1] = 0x25;
        *(uint32_t *)&pbNtCreateSection[2] = (uint32_t)((uintptr_t)puJmpTab - (uintptr_t)&pbNtCreateSection[2+4]);

        SUPR3HARDENED_ASSERT_WIN32_SUCCESS(VirtualProtectEx(GetCurrentProcess(), pbNtCreateSection, 16,
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
        SUPR3HARDENED_ASSERT_WIN32_SUCCESS(VirtualProtectEx(GetCurrentProcess(), pbNtCreateSection, 16,
                                                            PAGE_EXECUTE_READWRITE, &dwOldProt));
        pbNtCreateSection[0] = 0xe9;
        *(uint32_t *)&pbNtCreateSection[1] = (uintptr_t)supR3HardenedMonitor_NtCreateSection
                                           - (uintptr_t)&pbNtCreateSection[1+4];

        SUPR3HARDENED_ASSERT_WIN32_SUCCESS(VirtualProtectEx(GetCurrentProcess(), pbNtCreateSection, 16,
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
    int rc = supHardenedWinVerifyProcess(GetCurrentProcess(), GetCurrentThread(), &g_ErrInfoStatic.Core);
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
    SUPR3HARDENED_ASSERT_NT_SUCCESS(NtOpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken));
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
 * Construct the new command line.  Since argc/argv are both derived from
 * GetCommandLineW (see suplibHardenedWindowsMain), we skip the argument
 * by argument UTF-8 -> UTF-16 conversion and quoting by going to the
 * original source.
 *
 * The executable name, though, is replaced in case it's not a fullly
 * qualified path.
 *
 * The re-spawn indicator is added immediately after the executable name
 * so that we don't get tripped up missing close quote chars in the last
 * argument.
 *
 * @returns Pointer to a command line string (heap).
 */
static PRTUTF16 supR3HardenedWinConstructCmdLine(void)
{
    /*
     * Get the command line and skip the executable name.
     */
    PCRTUTF16 pwszArgs = GetCommandLineW();

    /* Skip leading space (shouldn't be any, but whatever). */
    while (suplibCommandLineIsArgSeparator(*pwszArgs))
        pwszArgs++;
    SUPR3HARDENED_ASSERT(*pwszArgs != '\0');

    /* Walk to the end of it. */
    int fQuoted = false;
    do
    {
        if (*pwszArgs == '"')
        {
            fQuoted = !fQuoted;
            pwszArgs++;
        }
        else if (*pwszArgs != '\\' || (pwszArgs[1] != '\\' && pwszArgs[1] != '"'))
            pwszArgs++;
        else
        {
            unsigned cSlashes = 0;
            do
                cSlashes++;
            while (*++pwszArgs == '\\');
            if (*pwszArgs == '"' && (cSlashes & 1))
                pwszArgs++; /* odd number of slashes == escaped quote */
        }
    } while (*pwszArgs && (fQuoted || !suplibCommandLineIsArgSeparator(*pwszArgs)));

    /* Skip trailing spaces. */
    while (suplibCommandLineIsArgSeparator(*pwszArgs))
        pwszArgs++;

    /*
     * Allocate a new buffer.
     */
    size_t cwcArgs    = suplibHardenedWStrLen(pwszArgs);
    size_t cwcCmdLine = (sizeof(SUPR3_RESPAWN_ARG0) - 1) / sizeof(SUPR3_RESPAWN_ARG0[0]) /* Respawn exe name. */
                      + !!cwcArgs + cwcArgs; /* if arguments present, add space + arguments. */
    PRTUTF16 pwszCmdLine = (PRTUTF16)HeapAlloc(GetProcessHeap(), 0 /* dwFlags*/, (cwcCmdLine + 1) * sizeof(RTUTF16));
    SUPR3HARDENED_ASSERT(pwszCmdLine != NULL);

    /*
     * Construct the new command line.
     */
    PRTUTF16 pwszDst = pwszCmdLine;
    for (const char *pszSrc = SUPR3_RESPAWN_ARG0; *pszSrc; pszSrc++)
        *pwszDst++ = *pszSrc;

    if (cwcArgs)
    {
        *pwszDst++ = ' ';
        suplibHardenedMemCopy(pwszDst, pwszArgs, cwcArgs * sizeof(RTUTF16));
        pwszDst += cwcArgs;
    }

    *pwszDst = '\0';
    SUPR3HARDENED_ASSERT(pwszDst - pwszCmdLine == cwcCmdLine);

    return pwszCmdLine;
}


/**
 * Does the actually respawning.
 *
 * @returns Exit code (if we get that far).
 */
static int supR3HardenedWinDoReSpawn(void)
{
    SUPR3HARDENED_ASSERT(g_cSuplibHardenedWindowsMainCalls == 1);

    /*
     * Configure the startup info and creation flags.
     */
    DWORD dwCreationFlags = 0;

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
    SiEx.StartupInfo.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    SiEx.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    SiEx.StartupInfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    /*
     * Set up security descriptors.
     */
    SECURITY_ATTRIBUTES ProcessSecAttrs;
    MYSECURITYCLEANUP   ProcessSecAttrsCleanup;
    supR3HardenedInitSecAttrs(&ProcessSecAttrs, &ProcessSecAttrsCleanup, true /*fProcess*/);

    SECURITY_ATTRIBUTES ThreadSecAttrs;
    MYSECURITYCLEANUP   ThreadSecAttrsCleanup;
    supR3HardenedInitSecAttrs(&ThreadSecAttrs, &ThreadSecAttrsCleanup, false /*fProcess*/);

    /*
     * Construct the command line and launch the process.
     */
    PRTUTF16 pwszCmdLine = supR3HardenedWinConstructCmdLine();

    PROCESS_INFORMATION ProcessInfo;
    if (!CreateProcessW(g_wszSupLibHardenedExePath,
                        pwszCmdLine,
                        &ProcessSecAttrs,
                        &ThreadSecAttrs,
                        TRUE /*fInheritHandles*/,
                        dwCreationFlags,
                        NULL /*pwszzEnvironment*/,
                        NULL /*pwszCurDir*/,
                        &SiEx.StartupInfo,
                        &ProcessInfo))
        supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Misc, VERR_INVALID_NAME,
                              "Error relaunching VirtualBox VM process: %u\n"
                              "Command line: '%ls'",
                              GetLastError(), pwszCmdLine);

    /*
     * Close the unrestricted access handles.  Since we need to wait on the
     * child process, we'll reopen the process with limited access before doing
     * away with the process handle returned by CreateProcess.
     */
    SUPR3HARDENED_ASSERT(CloseHandle(ProcessInfo.hThread));
    ProcessInfo.hThread = NULL;

    HANDLE hProcWait;
    DWORD dwRights = SYNCHRONIZE;
    if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0)) /* Introduced in Vista. */
        dwRights |= PROCESS_QUERY_LIMITED_INFORMATION;
    else
        dwRights |= PROCESS_QUERY_INFORMATION;
    if (!DuplicateHandle(GetCurrentProcess(),
                         ProcessInfo.hProcess,
                         GetCurrentProcess(),
                         &hProcWait,
                         SYNCHRONIZE,
                         FALSE /*fInheritHandle*/,
                         0))
    {
        /* This is unacceptable, kill the process. */
        DWORD dwErr = GetLastError();
        TerminateProcess(ProcessInfo.hProcess, RTEXITCODE_FAILURE);
        supR3HardenedError(dwErr, false /*fFatal*/, "DuplicateHandle failed on child process handle: %u\n", dwErr);

        DWORD dwExit;
        BOOL fExitOk = GetExitCodeProcess(ProcessInfo.hProcess, &dwExit)
                    && dwExit != STILL_ACTIVE;
        if (!fExitOk)
        {
            DWORD dwStartTick = GetTickCount();
            DWORD dwWait;
            do
            {
                TerminateProcess(ProcessInfo.hProcess, RTEXITCODE_FAILURE);
                dwWait  = WaitForSingleObject(ProcessInfo.hProcess, 1000);
                fExitOk = GetExitCodeProcess(ProcessInfo.hProcess, &dwExit)
                       && dwExit != STILL_ACTIVE;
            } while (   !fExitOk
                     && (dwWait == WAIT_TIMEOUT || dwWait == WAIT_IO_COMPLETION)
                     && GetTickCount() - dwStartTick < 60 * 1000);
            if (fExitOk)
                supR3HardenedError(dwErr, false /*fFatal*/,
                                   "DuplicateHandle failed and we failed to kill child: dwErr=%u dwWait=%u err=%u hProcess=%p\n",
                                   dwErr, dwWait, GetLastError(), ProcessInfo.hProcess);
        }
        supR3HardenedFatalMsg("supR3HardenedWinReSpawn", kSupInitOp_Misc, VERR_INVALID_NAME,
                              "DuplicateHandle failed on child process handle: %u\n", dwErr);
    }

    SUPR3HARDENED_ASSERT(CloseHandle(ProcessInfo.hProcess));
    ProcessInfo.hProcess = NULL;

    /*
     * Wait for the process to terminate and proxy the termination code.
     */
    for (;;)
    {
        SetLastError(NO_ERROR);
        DWORD dwWait = WaitForSingleObject(hProcWait, INFINITE);
        if (   dwWait == WAIT_OBJECT_0
            || dwWait == WAIT_ABANDONED_0)
            break;
        if (   dwWait != WAIT_TIMEOUT
            && dwWait != WAIT_IO_COMPLETION)
            supR3HardenedFatal("WaitForSingleObject returned %#x (last error %#x)\n", dwWait, GetLastError());
    }

    DWORD dwExit;
    if (   !GetExitCodeProcess(hProcWait, &dwExit)
        || dwExit == STILL_ACTIVE)
        dwExit = RTEXITCODE_FAILURE;

    CloseHandle(hProcWait);
    suplibHardenedExit((RTEXITCODE)dwExit);
}


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
 * Called by the main code if supR3HardenedWinIsReSpawnNeeded returns @c true.
 *
 * @returns Program exit code.
 */
DECLHIDDEN(int) supR3HardenedWinReSpawn(void)
{
    /*
     * Open the stub device.  Retry if we think driver might still be
     * initializing (STATUS_NO_SUCH_DEVICE + \Drivers\VBoxDrv).
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

    /*
     * Respawn the process with kernel protection for the new process.
     */
    return supR3HardenedWinDoReSpawn();
}


/**
 * Checks if re-spawning is required, replacing the respawn argument if not.
 *
 * @returns true if required, false if not. In the latter case, the first
 *          argument in the vector is replaced.
 * @param   cArgs               The number of arguments.
 * @param   papszArgs           Pointer to the argument vector.
 */
DECLHIDDEN(bool) supR3HardenedWinIsReSpawnNeeded(int cArgs, char **papszArgs)
{
    SUPR3HARDENED_ASSERT(g_cSuplibHardenedWindowsMainCalls == 1);

    if (cArgs < 1)
        return true;
    if (suplibHardenedStrCmp(papszArgs[0], SUPR3_RESPAWN_ARG0))
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
 * @param   pwszCmdLine         The UTF-16 windows command line to parse.
 * @param   pcArgs              Where to return the number of arguments.
 */
static char **suplibCommandLineToArgvWStub(PCRTUTF16 pwszCmdLine, int *pcArgs)
{
    /*
     * Convert the command line string to UTF-8.
     */
    int   cbNeeded = WideCharToMultiByte(CP_UTF8, 0 /*dwFlags*/, pwszCmdLine, -1, NULL /*pszDst*/, 0 /*cbDst*/,
                                         NULL /*pchDefChar*/, NULL /* pfUsedDefChar */);
    SUPR3HARDENED_ASSERT(cbNeeded > 0);
    int   cbAllocated = cbNeeded + 16;
    char *pszCmdLine = (char *)suplibHardenedAllocZ(cbAllocated);

    SUPR3HARDENED_ASSERT(WideCharToMultiByte(CP_UTF8, 0 /*dwFlags*/, pwszCmdLine, -1,
                                             pszCmdLine, cbAllocated - 1,
                                             NULL /*pchDefChar*/, NULL /* pfUsedDefChar */) == cbNeeded);

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
    RTEXITCODE  rcExit = RTEXITCODE_FAILURE;

    g_cSuplibHardenedWindowsMainCalls++;

    /*
     * Init g_uNtVerCombined. (The code is shared with SUPR3.lib and lives in
     * SUPHardenedVerfiyImage-win.cpp.)
     */
    supR3HardenedWinInitVersion();

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
    CloseHandle(hFile);

    /* The NT executable name offset / dir path length. */
    g_offSupLibHardenedExeNtName = g_SupLibHardenedExeNtPath.UniStr.Length / sizeof(WCHAR);
    while (   g_offSupLibHardenedExeNtName > 1
           && g_SupLibHardenedExeNtPath.UniStr.Buffer[g_offSupLibHardenedExeNtName - 1] != '\\' )
        g_offSupLibHardenedExeNtName--;

    /*
     * Convert the arguments to UTF-8 and call the C/C++ main function.
     */
    int    cArgs;
    char **papszArgs = suplibCommandLineToArgvWStub(GetCommandLineW(), &cArgs);

    rcExit = (RTEXITCODE)main(cArgs, papszArgs, NULL);

    /*
     * Exit the process (never return).
     */
    suplibHardenedExit(rcExit);
}

