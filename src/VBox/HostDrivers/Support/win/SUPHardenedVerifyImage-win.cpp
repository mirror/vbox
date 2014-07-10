/* $Id$ */
/** @file
 * VirtualBox Support Library/Driver - Hardened Image Verification, Windows.
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
#ifdef IN_RING0
# define IPRT_NT_MAP_TO_ZW
# include <iprt/nt/nt.h>
# include <ntimage.h>
#else
# include <iprt/nt/nt-and-windows.h>
# include "Wintrust.h"
# include "Softpub.h"
# include "mscat.h"
# ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
#  define LOAD_LIBRARY_SEARCH_SYSTEM32           0x800
# endif
#endif

#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/ctype.h>
#include <iprt/ldr.h>
#include <iprt/log.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/crypto/pkcs7.h>
#include <iprt/crypto/store.h>

#ifdef IN_RING0
# include "SUPDrvInternal.h"
#else
# include "SUPLibInternal.h"
#endif
#include "win/SUPHardenedVerify-win.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The size of static hash (output) buffers.
 * Avoids dynamic allocations and cleanups for of small buffers as well as extra
 * calls for getting the appropriate buffer size.  The largest digest in regular
 * use by current windows version is SHA-512, we double this and hope it's
 * enough a good while. */
#define SUPHARDNTVI_MAX_CAT_HASH_SIZE   128


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * SUP image verifier loader reader instance.
 */
typedef struct SUPHNTVIRDR
{
    /** The core reader structure. */
    RTLDRREADER Core;
    /** The file handle . */
    HANDLE      hFile;
    /** Current file offset. */
    RTFOFF      off;
    /** The file size. */
    RTFOFF      cbFile;
    /** Flags for the verification callback, SUPHNTVI_F_XXX. */
    uint32_t    fFlags;
    /** The executable timstamp in second since unix epoch. */
    uint64_t    uTimestamp;
    /** Log name. */
    char        szFilename[1];
} SUPHNTVIRDR;
/** Pointer to an SUP image verifier loader reader instance. */
typedef SUPHNTVIRDR *PSUPHNTVIRDR;


#ifdef IN_RING3
typedef LONG (WINAPI * PFNWINVERIFYTRUST)(HWND hwnd, GUID const *pgActionID, PVOID pWVTData);
typedef BOOL (WINAPI * PFNCRYPTCATADMINACQUIRECONTEXT)(HCATADMIN *phCatAdmin, const GUID *pGuidSubsystem, DWORD dwFlags);
typedef BOOL (WINAPI * PFNCRYPTCATADMINACQUIRECONTEXT2)(HCATADMIN *phCatAdmin, const GUID *pGuidSubsystem, PCWSTR pwszHashAlgorithm,
                                                        struct _CERT_STRONG_SIGN_PARA const *pStrongHashPolicy, DWORD dwFlags);
typedef BOOL (WINAPI * PFNCRYPTCATADMINCALCHASHFROMFILEHANDLE)(HANDLE hFile, DWORD *pcbHash, BYTE *pbHash, DWORD dwFlags);
typedef BOOL (WINAPI * PFNCRYPTCATADMINCALCHASHFROMFILEHANDLE2)(HCATADMIN hCatAdmin, HANDLE hFile, DWORD *pcbHash,
                                                                BYTE *pbHash, DWORD dwFlags);
typedef HCATINFO (WINAPI *PFNCRYPTCATADMINENUMCATALOGFROMHASH)(HCATADMIN hCatAdmin, BYTE *pbHash, DWORD cbHash,
                                                               DWORD dwFlags, HCATINFO *phPrevCatInfo);
typedef BOOL (WINAPI * PFNCRYPTCATADMINRELEASECATALOGCONTEXT)(HCATADMIN hCatAdmin, HCATINFO hCatInfo, DWORD dwFlags);
typedef BOOL (WINAPI * PFNCRYPTCATDADMINRELEASECONTEXT)(HCATADMIN hCatAdmin, DWORD dwFlags);
typedef BOOL (WINAPI * PFNCRYPTCATCATALOGINFOFROMCONTEXT)(HCATINFO hCatInfo, CATALOG_INFO *psCatInfo, DWORD dwFlags);

typedef HCERTSTORE (WINAPI *PFNCERTOPENSTORE)(PCSTR pszStoreProvider, DWORD dwEncodingType, HCRYPTPROV_LEGACY hCryptProv,
                                              DWORD dwFlags, const void *pvParam);
typedef BOOL (WINAPI *PFNCERTCLOSESTORE)(HCERTSTORE hCertStore, DWORD dwFlags);
typedef PCCERT_CONTEXT (WINAPI *PFNCERTENUMCERTIFICATESINSTORE)(HCERTSTORE hCertStore, PCCERT_CONTEXT pPrevCertContext);
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The build certificate. */
static RTCRX509CERTIFICATE  g_BuildX509Cert;

/** Store for root software publisher certificates. */
static RTCRSTORE            g_hSpcRootStore = NIL_RTCRSTORE;
/** Store for root NT kernel certificates. */
static RTCRSTORE            g_hNtKernelRootStore = NIL_RTCRSTORE;

/** Store containing SPC, NT kernel signing, and timestamp root certificates. */
static RTCRSTORE            g_hSpcAndNtKernelRootStore = NIL_RTCRSTORE;
/** Store for supplemental certificates for use with
 * g_hSpcAndNtKernelRootStore. */
static RTCRSTORE            g_hSpcAndNtKernelSuppStore = NIL_RTCRSTORE;

/** The full \\SystemRoot\\System32 path. */
SUPSYSROOTDIRBUF            g_System32NtPath;
/** The full \\SystemRoot\\WinSxS path. */
SUPSYSROOTDIRBUF            g_WinSxSNtPath;

/** Set after we've retrived other SPC root certificates from the system. */
static bool                 g_fHaveOtherRoots = false;

#if defined(IN_RING3) && !defined(IN_SUP_HARDENED_R3)
/** Combined windows NT version number.  See SUP_MAKE_NT_VER_COMBINED and
 *  SUP_MAKE_NT_VER_SIMPLE. */
uint32_t                    g_uNtVerCombined;
#endif

#ifdef IN_RING3
/** Timestamp hack working around issues with old DLLs that we ship.
 * See supHardenedWinVerifyImageByHandle() for details.  */
static uint64_t             g_uBuildTimestampHack = 0;
#endif

#ifdef IN_RING3
/** Pointer to WinVerifyTrust. */
PFNWINVERIFYTRUST                       g_pfnWinVerifyTrust;
/** Pointer to CryptCATAdminAcquireContext. */
PFNCRYPTCATADMINACQUIRECONTEXT          g_pfnCryptCATAdminAcquireContext;
/** Pointer to CryptCATAdminAcquireContext2 if available. */
PFNCRYPTCATADMINACQUIRECONTEXT2         g_pfnCryptCATAdminAcquireContext2;
/** Pointer to CryptCATAdminCalcHashFromFileHandle. */
PFNCRYPTCATADMINCALCHASHFROMFILEHANDLE  g_pfnCryptCATAdminCalcHashFromFileHandle;
/** Pointer to CryptCATAdminCalcHashFromFileHandle2. */
PFNCRYPTCATADMINCALCHASHFROMFILEHANDLE2 g_pfnCryptCATAdminCalcHashFromFileHandle2;
/** Pointer to CryptCATAdminEnumCatalogFromHash. */
PFNCRYPTCATADMINENUMCATALOGFROMHASH     g_pfnCryptCATAdminEnumCatalogFromHash;
/** Pointer to CryptCATAdminReleaseCatalogContext. */
PFNCRYPTCATADMINRELEASECATALOGCONTEXT   g_pfnCryptCATAdminReleaseCatalogContext;
/** Pointer to CryptCATAdminReleaseContext. */
PFNCRYPTCATDADMINRELEASECONTEXT         g_pfnCryptCATAdminReleaseContext;
/** Pointer to CryptCATCatalogInfoFromContext. */
PFNCRYPTCATCATALOGINFOFROMCONTEXT       g_pfnCryptCATCatalogInfoFromContext;
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
#ifdef IN_RING3
static int supR3HardNtViCallWinVerifyTrust(HANDLE hFile, PCRTUTF16 pwszName, uint32_t fFlags, PRTERRINFO pErrInfo,
                                           PFNWINVERIFYTRUST pfnWinVerifyTrust);
static int supR3HardNtViCallWinVerifyTrustCatFile(HANDLE hFile, PCRTUTF16 pwszName, uint32_t fFlags, PRTERRINFO pErrInfo,
                                                  PFNWINVERIFYTRUST pfnWinVerifyTrust);
#endif




/** @copydoc RTLDRREADER::pfnRead */
static DECLCALLBACK(int) supHardNtViRdrRead(PRTLDRREADER pReader, void *pvBuf, size_t cb, RTFOFF off)
{
    PSUPHNTVIRDR pNtViRdr = (PSUPHNTVIRDR)pReader;
    Assert(pNtViRdr->Core.uMagic == RTLDRREADER_MAGIC);

    if ((ULONG)cb != cb)
        return VERR_OUT_OF_RANGE;


    /*
     * For some reason I'm getting occational read error in an XP VM with
     * STATUS_FAILED_DRIVER_ENTRY.  Redoing the call again works in the
     * debugger, so try do that automatically.
     */
    for (uint32_t iTry = 0;; iTry++)
    {
        LARGE_INTEGER offNt;
        offNt.QuadPart = off;

        IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        NTSTATUS rcNt = NtReadFile(pNtViRdr->hFile,
                                   NULL /*hEvent*/,
                                   NULL /*ApcRoutine*/,
                                   NULL /*ApcContext*/,
                                   &Ios,
                                   pvBuf,
                                   (ULONG)cb,
                                   &offNt,
                                   NULL);
        if (NT_SUCCESS(rcNt))
            rcNt = Ios.Status;
        if (NT_SUCCESS(rcNt))
        {
            if (Ios.Information == cb)
            {
                pNtViRdr->off = off + cb;
                return VINF_SUCCESS;
            }
#ifdef IN_RING3
            supR3HardenedError(VERR_READ_ERROR, false,
                               "supHardNtViRdrRead: Only got %#zx bytes when requesting %#zx bytes at %#llx in '%s'.\n",
                               Ios.Information, off, cb, pNtViRdr->szFilename);
#endif
            pNtViRdr->off = -1;
            return VERR_READ_ERROR;
        }

        /*
         * Delay a little before we retry?
         */
#ifdef IN_RING3
        if (iTry == 0)
            NtYieldExecution();
        else if (iTry >= 1)
        {
            LARGE_INTEGER Time;
            Time.QuadPart = -1000000 / 100; /* 1ms in 100ns units, relative time. */
            NtDelayExecution(TRUE, &Time);
        }
#endif
        /*
         * Before we give up, we'll try split up the request in case the
         * kernel is low on memory or similar.  For simplicity reasons, we do
         * this in a recursion fashion.
         */
        if (iTry >= 2)
        {
            if (cb >= _8K)
            {
                size_t const cbBlock = RT_ALIGN_Z(cb / 4, 512);
                while (cb > 0)
                {
                    size_t cbThisRead = RT_MIN(cb, cbBlock);
                    int rc = supHardNtViRdrRead(&pNtViRdr->Core, pvBuf, cbThisRead, off);
                    if (RT_FAILURE(rc))
                        return rc;
                    off  += cbThisRead;
                    cb   -= cbThisRead;
                    pvBuf = (uint8_t *)pvBuf + cbThisRead;
                }
                return VINF_SUCCESS;
            }

#ifdef IN_RING3
            supR3HardenedError(VERR_READ_ERROR, false, "supHardNtViRdrRead: Error %#x reading %#zx bytes at %#llx in '%s'.\n",
                               rcNt, off, cb, pNtViRdr->szFilename);
#endif
            pNtViRdr->off = -1;
            return VERR_READ_ERROR;
        }
    }
}


/** @copydoc RTLDRREADER::pfnTell */
static DECLCALLBACK(RTFOFF) supHardNtViRdrTell(PRTLDRREADER pReader)
{
    PSUPHNTVIRDR pNtViRdr = (PSUPHNTVIRDR)pReader;
    Assert(pNtViRdr->Core.uMagic == RTLDRREADER_MAGIC);
    return pNtViRdr->off;
}


/** @copydoc RTLDRREADER::pfnSize */
static DECLCALLBACK(RTFOFF) supHardNtViRdrSize(PRTLDRREADER pReader)
{
    PSUPHNTVIRDR pNtViRdr = (PSUPHNTVIRDR)pReader;
    Assert(pNtViRdr->Core.uMagic == RTLDRREADER_MAGIC);
    return pNtViRdr->cbFile;
}


/** @copydoc RTLDRREADER::pfnLogName */
static DECLCALLBACK(const char *) supHardNtViRdrLogName(PRTLDRREADER pReader)
{
    PSUPHNTVIRDR pNtViRdr = (PSUPHNTVIRDR)pReader;
    return pNtViRdr->szFilename;
}


/** @copydoc RTLDRREADER::pfnMap */
static DECLCALLBACK(int) supHardNtViRdrMap(PRTLDRREADER pReader, const void **ppvBits)
{
    return VERR_NOT_SUPPORTED;
}


/** @copydoc RTLDRREADER::pfnUnmap */
static DECLCALLBACK(int) supHardNtViRdrUnmap(PRTLDRREADER pReader, const void *pvBits)
{
    return VERR_NOT_SUPPORTED;
}


/** @copydoc RTLDRREADER::pfnDestroy */
static DECLCALLBACK(int) supHardNtViRdrDestroy(PRTLDRREADER pReader)
{
    PSUPHNTVIRDR pNtViRdr = (PSUPHNTVIRDR)pReader;
    Assert(pNtViRdr->Core.uMagic == RTLDRREADER_MAGIC);

    pNtViRdr->Core.uMagic = ~RTLDRREADER_MAGIC;
    pNtViRdr->hFile = NULL;

    RTMemFree(pNtViRdr);
    return VINF_SUCCESS;
}


/**
 * Creates a loader reader instance for the given NT file handle.
 *
 * @returns iprt status code.
 * @param   hFile           Native NT file handle.
 * @param   pwszName        Optional file name.
 * @param   fFlags          Flags, SUPHNTVI_F_XXX.
 * @param   ppNtViRdr       Where to store the reader instance on success.
 */
static int supHardNtViRdrCreate(HANDLE hFile, PCRTUTF16 pwszName, uint32_t fFlags, PSUPHNTVIRDR *ppNtViRdr)
{
    /*
     * Try determine the size of the file.
     */
    IO_STATUS_BLOCK             Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
    FILE_STANDARD_INFORMATION   StdInfo;
    NTSTATUS rcNt = NtQueryInformationFile(hFile, &Ios, &StdInfo, sizeof(StdInfo), FileStandardInformation);
    if (!NT_SUCCESS(rcNt) || !NT_SUCCESS(Ios.Status))
        return VERR_LDRVI_FILE_LENGTH_ERROR;

    /*
     * Calc the file name length and allocate memory for the reader instance.
     */
    size_t cchFilename = 0;
    if (pwszName)
        cchFilename = RTUtf16CalcUtf8Len(pwszName);

    int rc = VERR_NO_MEMORY;
    PSUPHNTVIRDR pNtViRdr = (PSUPHNTVIRDR)RTMemAllocZ(sizeof(*pNtViRdr) + cchFilename);
    if (!pNtViRdr)
        return VERR_NO_MEMORY;

    /*
     * Initialize the structure.
     */
    if (cchFilename)
    {
        char *pszName = &pNtViRdr->szFilename[0];
        rc = RTUtf16ToUtf8Ex(pwszName, RTSTR_MAX, &pszName, cchFilename + 1, NULL);
        AssertStmt(RT_SUCCESS(rc), pNtViRdr->szFilename[0] = '\0');
    }
    else
        pNtViRdr->szFilename[0] = '\0';

    pNtViRdr->Core.uMagic     = RTLDRREADER_MAGIC;
    pNtViRdr->Core.pfnRead    = supHardNtViRdrRead;
    pNtViRdr->Core.pfnTell    = supHardNtViRdrTell;
    pNtViRdr->Core.pfnSize    = supHardNtViRdrSize;
    pNtViRdr->Core.pfnLogName = supHardNtViRdrLogName;
    pNtViRdr->Core.pfnMap     = supHardNtViRdrMap;
    pNtViRdr->Core.pfnUnmap   = supHardNtViRdrUnmap;
    pNtViRdr->Core.pfnDestroy = supHardNtViRdrDestroy;
    pNtViRdr->hFile           = hFile;
    pNtViRdr->off             = 0;
    pNtViRdr->cbFile          = StdInfo.EndOfFile.QuadPart;
    pNtViRdr->fFlags          = fFlags;
    *ppNtViRdr = pNtViRdr;
    return VINF_SUCCESS;
}


/**
 * Simple case insensitive UTF-16 / ASCII path compare.
 *
 * @returns true if equal, false if not.
 * @param   pwszLeft            The UTF-16 path string.
 * @param   pszRight            The ascii string.
 */
static bool supHardViUtf16PathIsEqual(PCRTUTF16 pwszLeft, const char *pszRight)
{
    for (;;)
    {
        RTUTF16 wc = *pwszLeft++;
        uint8_t b  = *pszRight++;
        if (b != wc)
        {
            if (wc >= 0x80)
                return false;
            wc = RT_C_TO_LOWER(wc);
            if (wc != b)
            {
                b = RT_C_TO_LOWER(b);
                if (wc != b)
                {
                    if (wc == '/')
                        wc = '\\';
                    if (b == '/')
                        b = '\\';
                    if (wc != b)
                        return false;
                }
            }
        }
        if (!b)
            return true;
    }
}


/**
 * Simple case insensitive UTF-16 / ASCII ends-with path predicate.
 *
 * @returns true if equal, false if not.
 * @param   pwsz                The UTF-16 path string.
 * @param   pszSuffix           The ascii suffix string.
 */
static bool supHardViUtf16PathEndsWith(PCRTUTF16 pwsz, const char *pszSuffix)
{
    size_t cwc       = RTUtf16Len(pwsz);
    size_t cchSuffix = strlen(pszSuffix);
    if (cwc >= cchSuffix)
        return supHardViUtf16PathIsEqual(pwsz + cwc - cchSuffix, pszSuffix);
    return false;
}


/**
 * Simple case insensitive UTF-16 / ASCII starts-with path predicate.
 *
 * @returns true if starts with given string, false if not.
 * @param   pwsz                The UTF-16 path string.
 * @param   pszPrefix           The ascii prefix string.
 */
static bool supHardViUtf16PathStartsWith(PCRTUTF16 pwszLeft, const char *pszRight)
{
    for (;;)
    {
        RTUTF16 wc = *pwszLeft++;
        uint8_t b  = *pszRight++;
        if (b != wc)
        {
            if (!b)
                return true;
            if (wc >= 0x80 || wc == 0)
                return false;
            wc = RT_C_TO_LOWER(wc);
            if (wc != b)
            {
                b = RT_C_TO_LOWER(b);
                if (wc != b)
                {
                    if (wc == '/')
                        wc = '\\';
                    if (b == '/')
                        b = '\\';
                    if (wc != b)
                        return false;
                }
            }
        }
    }
}


/**
 * Counts slashes in the given UTF-8 path string.
 *
 * @returns Number of slashes.
 * @param   pwsz                The UTF-16 path string.
 */
static uint32_t supHardViUtf16PathCountSlashes(PCRTUTF16 pwsz)
{
    uint32_t cSlashes = 0;
    RTUTF16 wc;
    while ((wc = *pwsz++) != '\0')
        if (wc == '/' || wc == '\\')
            cSlashes++;
    return cSlashes;
}


/**
 * Checks if the unsigned DLL is fine or not.
 *
 * @returns VINF_LDRVI_NOT_SIGNED or @a rc.
 * @param   hLdrMod             The loader module handle.
 * @param   pwszName            The NT name of the DLL/EXE.
 * @param   fFlags              Flags.
 * @param   rc                  The status code..
 */
static int supHardNtViCheckIfNotSignedOk(RTLDRMOD hLdrMod, PCRTUTF16 pwszName, uint32_t fFlags, int rc)
{
    if (fFlags & (SUPHNTVI_F_REQUIRE_BUILD_CERT | SUPHNTVI_F_REQUIRE_KERNEL_CODE_SIGNING))
        return rc;

    /*
     * Version macros.
     */
    uint32_t const uNtVer = g_uNtVerCombined;
#define IS_XP()    ( uNtVer >= SUP_MAKE_NT_VER_SIMPLE(5, 1) && uNtVer < SUP_MAKE_NT_VER_SIMPLE(5, 2) )
#define IS_W2K3()  ( uNtVer >= SUP_MAKE_NT_VER_SIMPLE(5, 2) && uNtVer < SUP_MAKE_NT_VER_SIMPLE(5, 3) )
#define IS_VISTA() ( uNtVer >= SUP_MAKE_NT_VER_SIMPLE(6, 0) && uNtVer < SUP_MAKE_NT_VER_SIMPLE(6, 1) )
#define IS_W70()   ( uNtVer >= SUP_MAKE_NT_VER_SIMPLE(6, 1) && uNtVer < SUP_MAKE_NT_VER_SIMPLE(6, 2) )
#define IS_W80()   ( uNtVer >= SUP_MAKE_NT_VER_SIMPLE(6, 2) && uNtVer < SUP_MAKE_NT_VER_SIMPLE(6, 3) )
#define IS_W81()   ( uNtVer >= SUP_MAKE_NT_VER_SIMPLE(6, 3) && uNtVer < SUP_MAKE_NT_VER_SIMPLE(6, 4) )

    /*
     * The System32 directory.
     *
     * System32 is full of unsigned DLLs shipped by microsoft, graphics
     * hardware vendors, input device/method vendors and whatnot else that
     * actually needs to be loaded into a process for it to work correctly.
     * We have to ASSUME that anything our process attempts to load from
     * System32 is trustworthy and that the Windows system with the help of
     * anti-virus software make sure there is nothing evil lurking in System32
     * or being loaded from it.
     *
     * A small measure of protection is to list DLLs we know should be signed
     * and decline loading unsigned versions of them, assuming they have been
     * replaced by an adversary with evil intentions.
     */
    PCRTUTF16 pwsz;
    uint32_t cwcName = (uint32_t)RTUtf16Len(pwszName);
    uint32_t cwcOther = g_System32NtPath.UniStr.Length / sizeof(WCHAR);
    if (   cwcName > cwcOther
        && RTPATH_IS_SLASH(pwszName[cwcOther])
        && memcmp(pwszName, g_System32NtPath.UniStr.Buffer, g_System32NtPath.UniStr.Length) == 0)
    {
        pwsz = pwszName + cwcOther + 1;

        /* Core DLLs. */
        if (supHardViUtf16PathIsEqual(pwsz, "ntdll.dll"))
            return uNtVer < SUP_NT_VER_VISTA ? VINF_LDRVI_NOT_SIGNED : rc;
        if (supHardViUtf16PathIsEqual(pwsz, "kernel32.dll"))
            return uNtVer < SUP_NT_VER_W81 ? VINF_LDRVI_NOT_SIGNED : rc;
        if (supHardViUtf16PathIsEqual(pwsz, "kernelbase.dll"))
            return IS_W80() || IS_W70() ? VINF_LDRVI_NOT_SIGNED : rc;
        if (supHardViUtf16PathIsEqual(pwsz, "apisetschema.dll"))
            return IS_W70() ? VINF_LDRVI_NOT_SIGNED : rc;
        if (supHardViUtf16PathIsEqual(pwsz, "apphelp.dll"))
            return uNtVer < SUP_MAKE_NT_VER_SIMPLE(6, 4) ? VINF_LDRVI_NOT_SIGNED : rc;

#ifndef IN_RING0
# if 0 /* Allow anything below System32 that WinVerifyTrust thinks is fine. */
        /* The ATI drivers load system drivers into the process, allow this,
           but reject anything else from a subdirectory. */
        uint32_t cSlashes = supHardViUtf16PathCountSlashes(pwsz);
        if (cSlashes > 0)
        {
            if (   cSlashes == 1
                && supHardViUtf16PathStartsWith(pwsz, "drivers\\ati")
                && (   supHardViUtf16PathEndsWith(pwsz, ".sys")
                    || supHardViUtf16PathEndsWith(pwsz, ".dll") ) )
                return VINF_LDRVI_NOT_SIGNED;
            return rc;
        }
# endif

        /* Check that this DLL isn't supposed to be signed on this windows
           version.  If it should, it's likely to be a fake. */
        /** @todo list of signed dlls for various windows versions.  */

        /** @todo check file permissions? TrustedInstaller is supposed to be involved
         *        with all of them. */
        return VINF_LDRVI_NOT_SIGNED;
#else
        return rc;
#endif
    }

#ifndef IN_RING0
    /*
     * The WinSxS white list.
     *
     * Just like with System32 there are potentially a number of DLLs that
     * could be required from WinSxS.  However, so far only comctl32.dll
     * variations have been required.  So, we limit ourselves to explicit
     * whitelisting of unsigned families of DLLs.
     */
    cwcOther = g_WinSxSNtPath.UniStr.Length / sizeof(WCHAR);
    if (   cwcName > cwcOther
        && RTPATH_IS_SLASH(pwszName[cwcOther])
        && memcmp(pwszName, g_WinSxSNtPath.UniStr.Buffer, g_WinSxSNtPath.UniStr.Length) == 0)
    {
        pwsz = pwszName + cwcOther + 1;
        cwcName -= cwcOther + 1;

        /* The WinSxS layout means everything worth loading is exactly one level down. */
        uint32_t cSlashes = supHardViUtf16PathCountSlashes(pwsz);
        if (cSlashes != 1)
            return rc;

# if 0 /* See below */
        /* The common controls mess. */
# ifdef RT_ARCH_AMD64
        if (supHardViUtf16PathStartsWith(pwsz, "amd64_microsoft.windows.common-controls_"))
# elif defined(RT_ARCH_X86)
        if (supHardViUtf16PathStartsWith(pwsz, "x86_microsoft.windows.common-controls_"))
# else
#  error "Unsupported architecture"
# endif
        {
            if (supHardViUtf16PathEndsWith(pwsz, "\\comctl32.dll"))
                return VINF_LDRVI_NOT_SIGNED;
        }
# endif

        /* Allow anything slightly microsoftish from WinSxS. W2K3 wanted winhttp.dll early on... */
# ifdef RT_ARCH_AMD64
        if (supHardViUtf16PathStartsWith(pwsz, "amd64_microsoft."))
# elif defined(RT_ARCH_X86)
        if (supHardViUtf16PathStartsWith(pwsz, "x86_microsoft."))
# else
#  error "Unsupported architecture"
# endif
        {
            return VINF_LDRVI_NOT_SIGNED;
        }

        return rc;
    }
#endif

    return rc;
}


/**
 * @callback_method_impl{RTCRPKCS7VERIFYCERTCALLBACK,
 * Standard code signing.  Use this for Microsoft SPC.}
 */
static DECLCALLBACK(int) supHardNtViCertVerifyCallback(PCRTCRX509CERTIFICATE pCert, RTCRX509CERTPATHS hCertPaths,
                                                       void *pvUser, PRTERRINFO pErrInfo)
{
    PSUPHNTVIRDR pNtViRdr = (PSUPHNTVIRDR)pvUser;
    Assert(pNtViRdr->Core.uMagic == RTLDRREADER_MAGIC);

    /*
     * If there is no certificate path build & validator associated with this
     * callback, it must be because of the build certificate.  We trust the
     * build certificate without any second thoughts.
     */
    if (hCertPaths == NIL_RTCRX509CERTPATHS)
    {
        if (RTCrX509Certificate_Compare(pCert, &g_BuildX509Cert) == 0) /* healthy paranoia */
            return VINF_SUCCESS;
        return RTErrInfoSetF(pErrInfo, VERR_SUP_VP_NOT_BUILD_CERT_IPE, "Not valid kernel code signature.");
    }

    /*
     * Standard code signing capabilites required.
     */
    int rc = RTCrPkcs7VerifyCertCallbackCodeSigning(pCert, hCertPaths, NULL, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        /*
         * If kernel signing, a valid certificate path must be anchored by the
         * microsoft kernel signing root certificate.
         */
        if (pNtViRdr->fFlags & SUPHNTVI_F_REQUIRE_KERNEL_CODE_SIGNING)
        {
            uint32_t cPaths = RTCrX509CertPathsGetPathCount(hCertPaths);
            uint32_t cFound = 0;
            uint32_t cValid = 0;
            for (uint32_t iPath = 0; iPath < cPaths; iPath++)
            {
                bool                            fTrusted;
                PCRTCRX509NAME                  pSubject;
                PCRTCRX509SUBJECTPUBLICKEYINFO  pPublicKeyInfo;
                int                             rcVerify;
                rc = RTCrX509CertPathsQueryPathInfo(hCertPaths, iPath, &fTrusted, NULL /*pcNodes*/, &pSubject, &pPublicKeyInfo,
                                                    NULL, NULL /*pCertCtx*/, &rcVerify);
                AssertRCBreak(rc);

                if (RT_SUCCESS(rcVerify))
                {
                    Assert(fTrusted);
                    cValid++;

                    /*
                     * Search the kernel signing root store for a matching anchor.
                     */
                    RTCRSTORECERTSEARCH Search;
                    rc = RTCrStoreCertFindBySubjectOrAltSubjectByRfc5280(g_hNtKernelRootStore, pSubject, &Search);
                    AssertRCBreak(rc);

                    PCRTCRCERTCTX pCertCtx;
                    while ((pCertCtx = RTCrStoreCertSearchNext(g_hNtKernelRootStore, &Search)) != NULL)
                    {
                        PCRTCRX509SUBJECTPUBLICKEYINFO pCertPubKeyInfo = NULL;
                        if (pCertCtx->pCert)
                            pCertPubKeyInfo = &pCertCtx->pCert->TbsCertificate.SubjectPublicKeyInfo;
                        else if (pCertCtx->pTaInfo)
                            pCertPubKeyInfo = &pCertCtx->pTaInfo->PubKey;
                        else
                            pCertPubKeyInfo = NULL;
                        if (   pCertPubKeyInfo
                            && RTCrX509SubjectPublicKeyInfo_Compare(pCertPubKeyInfo, pPublicKeyInfo) == 0)
                            cFound++;
                        RTCrCertCtxRelease(pCertCtx);
                    }

                    int rc2 = RTCrStoreCertSearchDestroy(g_hNtKernelRootStore, &Search); AssertRC(rc2);
                }
            }
            if (RT_SUCCESS(rc) && cFound == 0)
                rc = RTErrInfoSetF(pErrInfo, VERR_SUP_VP_NOT_VALID_KERNEL_CODE_SIGNATURE, "Not valid kernel code signature.");
            if (RT_SUCCESS(rc) && cValid < 2 && g_fHaveOtherRoots)
                rc = RTErrInfoSetF(pErrInfo, VERR_SUP_VP_UNEXPECTED_VALID_PATH_COUNT,
                                   "Expected at least %u valid paths, not %u.", 2, cValid);
        }
    }

    /*
     * More requirements? NT5 build lab?
     */

    return rc;
}


static DECLCALLBACK(int) supHardNtViCallback(RTLDRMOD hLdrMod, RTLDRSIGNATURETYPE enmSignature,
                                             void const *pvSignature, size_t cbSignature,
                                             PRTERRINFO pErrInfo, void *pvUser)
{
    /*
     * Check out the input.
     */
    PSUPHNTVIRDR pNtViRdr = (PSUPHNTVIRDR)pvUser;
    Assert(pNtViRdr->Core.uMagic == RTLDRREADER_MAGIC);

    AssertReturn(cbSignature == sizeof(RTCRPKCS7CONTENTINFO), VERR_INTERNAL_ERROR_5);
    PCRTCRPKCS7CONTENTINFO pContentInfo = (PCRTCRPKCS7CONTENTINFO)pvSignature;
    AssertReturn(RTCrPkcs7ContentInfo_IsSignedData(pContentInfo), VERR_INTERNAL_ERROR_5);
    AssertReturn(pContentInfo->u.pSignedData->SignerInfos.cItems == 1, VERR_INTERNAL_ERROR_5);
    PCRTCRPKCS7SIGNERINFO pSignerInfo = &pContentInfo->u.pSignedData->SignerInfos.paItems[0];

    /*
     * If special certificate requirements, check them out before validating
     * the signature.
     */
    if (pNtViRdr->fFlags & SUPHNTVI_F_REQUIRE_BUILD_CERT)
    {
        if (!RTCrX509Certificate_MatchIssuerAndSerialNumber(&g_BuildX509Cert,
                                                            &pSignerInfo->IssuerAndSerialNumber.Name,
                                                            &pSignerInfo->IssuerAndSerialNumber.SerialNumber))
            return RTErrInfoSet(pErrInfo, VERR_SUP_VP_NOT_SIGNED_WITH_BUILD_CERT, "Not signed with the build certificate.");
    }

    /*
     * Verify the signature.
     */
    RTTIMESPEC ValidationTime;
    RTTimeSpecSetSeconds(&ValidationTime, pNtViRdr->uTimestamp);

    return RTCrPkcs7VerifySignedData(pContentInfo, 0, g_hSpcAndNtKernelSuppStore, g_hSpcAndNtKernelRootStore, &ValidationTime,
                                     supHardNtViCertVerifyCallback, pNtViRdr, pErrInfo);
}


/**
 * Checks if it's safe to call WinVerifyTrust or whether we might end up in an
 * infinite recursion.
 *
 * @returns true if ok, false if not.
 * @param   hFile               The file name.
 * @param   pwszName            The executable name.
 */
static bool supR3HardNtViCanCallWinVerifyTrust(HANDLE hFile, PCRTUTF16 pwszName)
{
     /*
      * Recursion preventions hacks:
      *  - Don't try call WinVerifyTrust on Wintrust.dll when called from the
      *    create section hook. CRYPT32.DLL tries to load WinTrust.DLL in some cases.
      */
     size_t cwcName = RTUtf16Len(pwszName);
     if (   hFile != NULL
         && cwcName > g_System32NtPath.UniStr.Length / sizeof(WCHAR)
         && !memcmp(pwszName, g_System32NtPath.UniStr.Buffer, g_System32NtPath.UniStr.Length)
         && supHardViUtf16PathIsEqual(&pwszName[g_System32NtPath.UniStr.Length / sizeof(WCHAR)], "\\wintrust.dll"))
         return false;

     return true;
}


/**
 * Verifies the given executable image.
 *
 * @returns IPRT status code.
 * @param   hFile       File handle to the executable file.
 * @param   pwszName    Full NT path to the DLL in question, used for dealing
 *                      with unsigned system dlls as well as for error/logging.
 * @param   fFlags      Flags, SUPHNTVI_F_XXX.
 * @param   pfCacheable Where to return whether the result can be cached.  A
 *                      valid value is always returned. Optional.
 * @param   pErrInfo    Pointer to error info structure. Optional.
 */
DECLHIDDEN(int) supHardenedWinVerifyImageByHandle(HANDLE hFile, PCRTUTF16 pwszName, uint32_t fFlags,
                                                  bool *pfCacheable, PRTERRINFO pErrInfo)
{
    /* Clear the cacheable indicator as it needs to be valid in all return paths. */
    if (pfCacheable)
        *pfCacheable = false;

#ifdef IN_RING3
    /* Check that the caller has performed the necessary library initialization. */
    if (!RTCrX509Certificate_IsPresent(&g_BuildX509Cert))
        return RTErrInfoSet(pErrInfo, VERR_WRONG_ORDER,
                            "supHardenedWinVerifyImageByHandle: supHardenedWinInitImageVerifier was not called.");
#endif

    /*
     * Create a reader instance.
     */
    PSUPHNTVIRDR pNtViRdr;
    int rc = supHardNtViRdrCreate(hFile, pwszName, fFlags, &pNtViRdr);
    if (RT_SUCCESS(rc))
    {
        /*
         * Open the image.
         */
        RTLDRMOD hLdrMod;
        rc = RTLdrOpenWithReader(&pNtViRdr->Core, RTLDR_O_FOR_VALIDATION,
                                 fFlags & SUPHNTVI_F_RC_IMAGE ? RTLDRARCH_X86_32 : RTLDRARCH_HOST,
                                 &hLdrMod, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            /*
             * Verify it.
             *
             * The PKCS #7 SignedData signature is checked in the callback. Any
             * signing certificate restrictions are also enforced there.
             *
             * For the time being, we use the executable timestamp as the
             * certificate validation date.  We must query that first to avoid
             * potential issues re-entering the loader code from the callback.
             *
             * Update: Save the first timestamp we validate with build cert and
             *         use this as a minimum timestamp for further build cert
             *         validations.  This works around issues with old DLLs that
             *         we sign against with our certificate (crt, sdl, qt).
             */
            rc = RTLdrQueryProp(hLdrMod, RTLDRPROP_TIMESTAMP_SECONDS, &pNtViRdr->uTimestamp, sizeof(pNtViRdr->uTimestamp));
            if (RT_SUCCESS(rc))
            {
#ifdef IN_RING3 /* Hack alert! (see above) */
                if (   (fFlags & SUPHNTVI_F_REQUIRE_KERNEL_CODE_SIGNING)
                    && (fFlags & SUPHNTVI_F_REQUIRE_SIGNATURE_ENFORCEMENT)
                    && pNtViRdr->uTimestamp < g_uBuildTimestampHack)
                    pNtViRdr->uTimestamp = g_uBuildTimestampHack;
#endif

                rc = RTLdrVerifySignature(hLdrMod, supHardNtViCallback, pNtViRdr, pErrInfo);

#ifdef IN_RING3 /* Hack alert! (see above) */
                if ((fFlags & SUPHNTVI_F_REQUIRE_BUILD_CERT) && g_uBuildTimestampHack == 0 && RT_SUCCESS(rc))
                    g_uBuildTimestampHack = pNtViRdr->uTimestamp;
#endif

                /*
                 * Microsoft doesn't sign a whole bunch of DLLs, so we have to
                 * ASSUME that a bunch of system DLLs are fine.
                 */
                if (rc == VERR_LDRVI_NOT_SIGNED)
                    rc = supHardNtViCheckIfNotSignedOk(hLdrMod, pwszName, fFlags, rc);
                if (RT_FAILURE(rc))
                    RTErrInfoAddF(pErrInfo, rc, ": %ls", pwszName);

                /*
                 * Check for the signature checking enforcement, if requested to do so.
                 */
                if (RT_SUCCESS(rc) && (fFlags & SUPHNTVI_F_REQUIRE_SIGNATURE_ENFORCEMENT))
                {
                    bool fEnforced = false;
                    int rc2 = RTLdrQueryProp(hLdrMod, RTLDRPROP_SIGNATURE_CHECKS_ENFORCED, &fEnforced, sizeof(fEnforced));
                    if (RT_FAILURE(rc2))
                        rc = RTErrInfoSetF(pErrInfo, rc2, "Querying RTLDRPROP_SIGNATURE_CHECKS_ENFORCED failed on %ls: %Rrc.",
                                           pwszName, rc2);
                    else if (!fEnforced)
                        rc = RTErrInfoSetF(pErrInfo, VERR_SUP_VP_SIGNATURE_CHECKS_NOT_ENFORCED,
                                           "The image '%ls' was not linked with /IntegrityCheck.", pwszName);
                }
            }
            else
                RTErrInfoSetF(pErrInfo, rc, "RTLdrQueryProp/RTLDRPROP_TIMESTAMP_SECONDS failed on %ls: %Rrc", pwszName, rc);

             int rc2 = RTLdrClose(hLdrMod); AssertRC(rc2);

#ifdef IN_RING3
             /*
              * Call the windows verify trust API if we've resolved it.
              */
             if (   g_pfnWinVerifyTrust
                 && supR3HardNtViCanCallWinVerifyTrust(hFile, pwszName))
             {
                 if (pfCacheable)
                     *pfCacheable = g_pfnWinVerifyTrust != NULL;
                 if (rc != VERR_LDRVI_NOT_SIGNED)
                 {
                     if (rc == VINF_LDRVI_NOT_SIGNED)
                     {
                         if (fFlags & SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION)
                         {
                             int rc2 = supR3HardNtViCallWinVerifyTrustCatFile(hFile, pwszName, fFlags, pErrInfo,
                                                                              g_pfnWinVerifyTrust);
                             SUP_DPRINTF(("supR3HardNtViCallWinVerifyTrustCatFile -> %d (org %d)\n", rc2, rc));
                             rc = rc2;
                         }
                         else
                         {
                             AssertFailed();
                             rc = VERR_LDRVI_NOT_SIGNED;
                         }
                     }
                     else if (RT_SUCCESS(rc))
                         rc = supR3HardNtViCallWinVerifyTrust(hFile, pwszName, fFlags, pErrInfo, g_pfnWinVerifyTrust);
                     else
                     {
                         int rc2 = supR3HardNtViCallWinVerifyTrust(hFile, pwszName, fFlags, pErrInfo, g_pfnWinVerifyTrust);
                         AssertMsg(RT_FAILURE_NP(rc2),
                                   ("rc=%Rrc, rc2=%Rrc %s", rc, rc2, pErrInfo ? pErrInfo->pszMsg : "<no-err-info>"));
                     }
                 }
             }
#else
             if (pfCacheable)
                 *pfCacheable = true;
#endif /* IN_RING3 */
        }
        else
            supHardNtViRdrDestroy(&pNtViRdr->Core);
    }
    SUP_DPRINTF(("supHardenedWinVerifyImageByHandle: -> %d (%ls)\n", rc, pwszName));
    return rc;
}


#ifdef IN_RING3
/**
 * supHardenedWinVerifyImageByHandle version without the name.
 *
 * The name is derived from the handle.
 *
 * @returns IPRT status code.
 * @param   hFile       File handle to the executable file.
 * @param   fFlags      Flags, SUPHNTVI_F_XXX.
 * @param   pErrInfo    Pointer to error info structure. Optional.
 */
DECLHIDDEN(int) supHardenedWinVerifyImageByHandleNoName(HANDLE hFile, uint32_t fFlags, PRTERRINFO pErrInfo)
{
    /*
     * Determine the NT name and call the verification function.
     */
    union
    {
        UNICODE_STRING UniStr;
        uint8_t abBuffer[(MAX_PATH + 8 + 1) * 2];
    } uBuf;

    ULONG cbIgn;
    NTSTATUS rcNt = NtQueryObject(hFile,
                                  ObjectNameInformation,
                                  &uBuf,
                                  sizeof(uBuf) - sizeof(WCHAR),
                                  &cbIgn);
    if (NT_SUCCESS(rcNt))
        uBuf.UniStr.Buffer[uBuf.UniStr.Length / sizeof(WCHAR)] = '\0';
    else
        uBuf.UniStr.Buffer = (WCHAR *)L"TODO3";

    return supHardenedWinVerifyImageByHandle(hFile, uBuf.UniStr.Buffer, fFlags, NULL /*pfCacheable*/, pErrInfo);
}
#endif /* IN_RING3 */


/**
 * Retrieves the full official path to the system root or one of it's sub
 * directories.
 *
 * This code is also used by the support driver.
 *
 * @returns VBox status code.
 * @param   pvBuf               The output buffer.  This will contain a
 *                              UNICODE_STRING followed (at the kernel's
 *                              discretion) the string buffer.
 * @param   cbBuf               The size of the buffer @a pvBuf points to.
 * @param   enmDir              Which directory under the system root we're
 *                              interested in.
 * @param   pErrInfo            Pointer to error info structure. Optional.
 */
DECLHIDDEN(int) supHardNtGetSystemRootDir(void *pvBuf, uint32_t cbBuf, SUPHARDNTSYSROOTDIR enmDir, PRTERRINFO pErrInfo)
{
    HANDLE              hFile = RTNT_INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;

    UNICODE_STRING      NtName;
    switch (enmDir)
    {
        case kSupHardNtSysRootDir_System32:
        {
            static const WCHAR  s_wszNameSystem32[] = L"\\SystemRoot\\System32\\";
            NtName.Buffer        = (PWSTR)s_wszNameSystem32;
            NtName.Length        = sizeof(s_wszNameSystem32) - sizeof(WCHAR);
            NtName.MaximumLength = sizeof(s_wszNameSystem32);
            break;
        }
        case kSupHardNtSysRootDir_WinSxS:
        {
            static const WCHAR  s_wszNameWinSxS[] = L"\\SystemRoot\\WinSxS\\";
            NtName.Buffer        = (PWSTR)s_wszNameWinSxS;
            NtName.Length        = sizeof(s_wszNameWinSxS) - sizeof(WCHAR);
            NtName.MaximumLength = sizeof(s_wszNameWinSxS);
            break;
        }
        default:
            AssertFailed();
            return VERR_INVALID_PARAMETER;
    }

    OBJECT_ATTRIBUTES ObjAttr;
    InitializeObjectAttributes(&ObjAttr, &NtName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

    NTSTATUS rcNt = NtCreateFile(&hFile,
                                 FILE_READ_DATA | SYNCHRONIZE,
                                 &ObjAttr,
                                 &Ios,
                                 NULL /* Allocation Size*/,
                                 FILE_ATTRIBUTE_NORMAL,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 FILE_OPEN,
                                 FILE_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT,
                                 NULL /*EaBuffer*/,
                                 0 /*EaLength*/);
    if (NT_SUCCESS(rcNt))
        rcNt = Ios.Status;
    if (NT_SUCCESS(rcNt))
    {
        ULONG cbIgn;
        rcNt = NtQueryObject(hFile,
                             ObjectNameInformation,
                             pvBuf,
                             cbBuf - sizeof(WCHAR),
                             &cbIgn);
        NtClose(hFile);
        if (NT_SUCCESS(rcNt))
        {
            PUNICODE_STRING pUniStr = (PUNICODE_STRING)pvBuf;
            if (pUniStr->Length > 0)
            {
                /* Make sure it's terminated so it can safely be printed.*/
                pUniStr->Buffer[pUniStr->Length / sizeof(WCHAR)] = '\0';
                return VINF_SUCCESS;
            }

            return RTErrInfoSetF(pErrInfo, VERR_SUP_VP_SYSTEM32_PATH,
                                 "NtQueryObject returned an empty path for '%ls'", NtName.Buffer);
        }
        return RTErrInfoSetF(pErrInfo, VERR_SUP_VP_SYSTEM32_PATH, "NtQueryObject failed on '%ls' dir: %#x", NtName.Buffer, rcNt);
    }
    return RTErrInfoSetF(pErrInfo, VERR_SUP_VP_SYSTEM32_PATH, "Failure to open '%ls': %#x", NtName.Buffer, rcNt);
}


/**
 * Initialize one certificate entry.
 *
 * @returns VBox status code.
 * @param   pCert               The X.509 certificate representation to init.
 * @param   pabCert             The raw DER encoded certificate.
 * @param   cbCert              The size of the raw certificate.
 * @param   pErrInfo            Where to return extended error info. Optional.
 * @param   pszErrorTag         Error tag.
 */
static int supHardNtViCertInit(PRTCRX509CERTIFICATE pCert, unsigned char const *pabCert, unsigned cbCert,
                               PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    AssertReturn(cbCert > 16 && cbCert < _128K,
                 RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_3, "%s: cbCert=%#x out of range", pszErrorTag, cbCert));
    AssertReturn(!RTCrX509Certificate_IsPresent(pCert),
                 RTErrInfoSetF(pErrInfo, VERR_WRONG_ORDER, "%s: Certificate already decoded?", pszErrorTag));

    RTASN1CURSORPRIMARY PrimaryCursor;
    RTAsn1CursorInitPrimary(&PrimaryCursor, pabCert, cbCert, pErrInfo, &g_RTAsn1DefaultAllocator, RTASN1CURSOR_FLAGS_DER, NULL);
    int rc = RTCrX509Certificate_DecodeAsn1(&PrimaryCursor.Cursor, 0, pCert, pszErrorTag);
    if (RT_SUCCESS(rc))
        rc = RTCrX509Certificate_CheckSanity(pCert, 0, pErrInfo, pszErrorTag);
    return rc;
}


static int supHardNtViCertStoreAddArray(RTCRSTORE hStore, PCSUPTAENTRY paCerts, unsigned cCerts, PRTERRINFO pErrInfo)
{
    for (uint32_t i = 0; i < cCerts; i++)
    {
        int rc = RTCrStoreCertAddEncoded(hStore, RTCRCERTCTX_F_ENC_TAF_DER, paCerts[i].pch, paCerts[i].cb, pErrInfo);
        if (RT_FAILURE(rc))
            return rc;
    }
    return VINF_SUCCESS;
}


/**
 * Initialize a certificate table.
 *
 * @param   phStore             Where to return the store pointer.
 * @param   paCerts1            Pointer to the first certificate table.
 * @param   cCerts1             Entries in the first certificate table.
 * @param   paCerts2            Pointer to the second certificate table.
 * @param   cCerts2             Entries in the second certificate table.
 * @param   paCerts3            Pointer to the third certificate table.
 * @param   cCerts3             Entries in the third certificate table.
 * @param   pErrInfo            Where to return extended error info. Optional.
 * @param   pszErrorTag         Error tag.
 */
static int supHardNtViCertStoreInit(PRTCRSTORE phStore,
                                    PCSUPTAENTRY paCerts1, unsigned cCerts1,
                                    PCSUPTAENTRY paCerts2, unsigned cCerts2,
                                    PCSUPTAENTRY paCerts3, unsigned cCerts3,
                                    PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    AssertReturn(*phStore == NIL_RTCRSTORE, VERR_WRONG_ORDER);

    int rc = RTCrStoreCreateInMem(phStore, cCerts1 + cCerts2);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pErrInfo, rc, "RTCrStoreCreateMemoryStore failed: %Rrc", rc);

    rc = supHardNtViCertStoreAddArray(*phStore, paCerts1, cCerts1, pErrInfo);
    if (RT_SUCCESS(rc))
        rc = supHardNtViCertStoreAddArray(*phStore, paCerts2, cCerts2, pErrInfo);
    if (RT_SUCCESS(rc))
        rc = supHardNtViCertStoreAddArray(*phStore, paCerts3, cCerts3, pErrInfo);
    return rc;
}


/**
 * This initializes the certificates globals so we don't have to reparse them
 * every time we need to verify an image.
 *
 * @returns IPRT status code.
 * @param   pErrInfo            Where to return extended error info. Optional.
 */
DECLHIDDEN(int) supHardenedWinInitImageVerifier(PRTERRINFO pErrInfo)
{
    AssertReturn(!RTCrX509Certificate_IsPresent(&g_BuildX509Cert), VERR_WRONG_ORDER);

    /*
     * Get the system root paths.
     */
    int rc = supHardNtGetSystemRootDir(&g_System32NtPath, sizeof(g_System32NtPath), kSupHardNtSysRootDir_System32, pErrInfo);
    if (RT_SUCCESS(rc))
        rc = supHardNtGetSystemRootDir(&g_WinSxSNtPath, sizeof(g_WinSxSNtPath), kSupHardNtSysRootDir_WinSxS, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize it, leaving the cleanup to the termination call.
         */
        rc = supHardNtViCertInit(&g_BuildX509Cert, g_abSUPBuildCert, g_cbSUPBuildCert, pErrInfo, "BuildCertificate");
        if (RT_SUCCESS(rc))
            rc = supHardNtViCertStoreInit(&g_hSpcRootStore, g_aSUPSpcRootTAs, g_cSUPSpcRootTAs,
                                          NULL, 0, NULL, 0, pErrInfo, "SpcRoot");
        if (RT_SUCCESS(rc))
            rc = supHardNtViCertStoreInit(&g_hNtKernelRootStore, g_aSUPNtKernelRootTAs, g_cSUPNtKernelRootTAs,
                                          NULL, 0, NULL, 0, pErrInfo, "NtKernelRoot");
        if (RT_SUCCESS(rc))
            rc = supHardNtViCertStoreInit(&g_hSpcAndNtKernelRootStore,
                                          g_aSUPSpcRootTAs, g_cSUPSpcRootTAs,
                                          g_aSUPNtKernelRootTAs, g_cSUPNtKernelRootTAs,
                                          g_aSUPTimestampTAs, g_cSUPTimestampTAs,
                                          pErrInfo, "SpcAndNtKernelRoot");
        if (RT_SUCCESS(rc))
            rc = supHardNtViCertStoreInit(&g_hSpcAndNtKernelSuppStore,
                                          NULL, 0, NULL, 0, NULL, 0,
                                          pErrInfo, "SpcAndNtKernelSupplemental");

#if 0 /* For the time being, always trust the build certificate. It bypasses the timestamp issues of CRT and SDL. */
        /* If the build certificate is a test singing certificate, it must be a
           trusted root or we'll fail to validate anything. */
        if (   RT_SUCCESS(rc)
            && RTCrX509Name_Compare(&g_BuildX509Cert.TbsCertificate.Subject, &g_BuildX509Cert.TbsCertificate.Issuer) == 0)
#else
        if (RT_SUCCESS(rc))
#endif
            rc = RTCrStoreCertAddEncoded(g_hSpcAndNtKernelRootStore, RTCRCERTCTX_F_ENC_X509_DER,
                                         g_abSUPBuildCert, g_cbSUPBuildCert, pErrInfo);

        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        supHardenedWinTermImageVerifier();
    }
    return rc;
}


/**
 * Releases resources allocated by supHardenedWinInitImageVerifier.
 */
DECLHIDDEN(void) supHardenedWinTermImageVerifier(void)
{
    if (RTCrX509Certificate_IsPresent(&g_BuildX509Cert))
        RTAsn1VtDelete(&g_BuildX509Cert.SeqCore.Asn1Core);

    RTCrStoreRelease(g_hSpcAndNtKernelSuppStore);
    g_hSpcAndNtKernelSuppStore = NIL_RTCRSTORE;
    RTCrStoreRelease(g_hSpcAndNtKernelRootStore);
    g_hSpcAndNtKernelRootStore = NIL_RTCRSTORE;

    RTCrStoreRelease(g_hNtKernelRootStore);
    g_hNtKernelRootStore = NIL_RTCRSTORE;
    RTCrStoreRelease(g_hSpcRootStore);
    g_hSpcRootStore = NIL_RTCRSTORE;
}

#ifdef IN_RING3

/**
 * This is a hardcoded list of certificates we thing we might need.
 *
 * @returns true if wanted, false if not.
 * @param   pCert               The certificate.
 */
static bool supR3HardenedWinIsDesiredRootCA(PCRTCRX509CERTIFICATE pCert)
{
    /*
     * Check that it's a plausible root certificate.
     */
    if (!RTCrX509Certificate_IsSelfSigned(pCert))
        return false;
    if (RTAsn1Integer_UnsignedCompareWithU32(&pCert->TbsCertificate.T0.Version, 3) > 0)
    {
        if (   !(pCert->TbsCertificate.T3.fExtKeyUsage & RTCRX509CERT_KEY_USAGE_F_KEY_CERT_SIGN)
            && (pCert->TbsCertificate.T3.fFlags & RTCRX509TBSCERTIFICATE_F_PRESENT_KEY_USAGE) )
            return false;
        if (   pCert->TbsCertificate.T3.pBasicConstraints
            && !pCert->TbsCertificate.T3.pBasicConstraints->CA.fValue)
            return false;
    }
    if (pCert->TbsCertificate.SubjectPublicKeyInfo.SubjectPublicKey.cBits < 256) /* mostly for u64KeyId reading. */
        return false;

    /*
     * Array of names and key clues of the certificates we want.
     */
    static struct
    {
        uint64_t    u64KeyId;
        const char *pszName;
    } const s_aWanted[] =
    {
        /* SPC */
        { UINT64_C(0xffffffffffffffff), "C=US, O=VeriSign, Inc., OU=Class 3 Public Primary Certification Authority" },
        { UINT64_C(0xffffffffffffffff), "L=Internet, O=VeriSign, Inc., OU=VeriSign Commercial Software Publishers CA" },
        { UINT64_C(0x491857ead79dde00), "C=US, O=The Go Daddy Group, Inc., OU=Go Daddy Class 2 Certification Authority" },

        /* TS */
        { UINT64_C(0xffffffffffffffff), "O=Microsoft Trust Network, OU=Microsoft Corporation, OU=Microsoft Time Stamping Service Root, OU=Copyright (c) 1997 Microsoft Corp." },
        { UINT64_C(0xffffffffffffffff), "O=VeriSign Trust Network, OU=VeriSign, Inc., OU=VeriSign Time Stamping Service Root, OU=NO LIABILITY ACCEPTED, (c)97 VeriSign, Inc." },
        { UINT64_C(0xffffffffffffffff), "C=ZA, ST=Western Cape, L=Durbanville, O=Thawte, OU=Thawte Certification, CN=Thawte Timestamping CA" },

        /* Additional Windows 8.1 list: */
        { UINT64_C(0x5ad46780fa5df300), "DC=com, DC=microsoft, CN=Microsoft Root Certificate Authority" },
        { UINT64_C(0x3be670c1bd02a900), "OU=Copyright (c) 1997 Microsoft Corp., OU=Microsoft Corporation, CN=Microsoft Root Authority" },
        { UINT64_C(0x4d3835aa4180b200), "C=US, ST=Washington, L=Redmond, O=Microsoft Corporation, CN=Microsoft Root Certificate Authority 2011" },
        { UINT64_C(0x646e3fe3ba08df00), "C=US, O=MSFT, CN=Microsoft Authenticode(tm) Root Authority" },
        { UINT64_C(0xece4e4289e08b900), "C=US, ST=Washington, L=Redmond, O=Microsoft Corporation, CN=Microsoft Root Certificate Authority 2010" },
        { UINT64_C(0x59faf1086271bf00), "C=US, ST=Arizona, L=Scottsdale, O=GoDaddy.com, Inc., CN=Go Daddy Root Certificate Authority - G2" },
        { UINT64_C(0x3d98ab22bb04a300), "C=IE, O=Baltimore, OU=CyberTrust, CN=Baltimore CyberTrust Root" },
        { UINT64_C(0x91e3728b8b40d000), "C=GB, ST=Greater Manchester, L=Salford, O=COMODO CA Limited, CN=COMODO Certification Authority" },
        { UINT64_C(0x61a3a33f81aace00), "C=US, ST=UT, L=Salt Lake City, O=The USERTRUST Network, OU=http://www.usertrust.com, CN=UTN-USERFirst-Object" },
        { UINT64_C(0x9e5bc2d78b6a3636), "C=ZA, ST=Western Cape, L=Cape Town, O=Thawte Consulting cc, OU=Certification Services Division, CN=Thawte Premium Server CA, Email=premium-server@thawte.com" },
        { UINT64_C(0xf4fd306318ccda00), "C=US, O=GeoTrust Inc., CN=GeoTrust Global CA" },
        { UINT64_C(0xa0ee62086758b15d), "C=US, O=Equifax, OU=Equifax Secure Certificate Authority" },
        { UINT64_C(0x8ff6fc03c1edbd00), "C=US, ST=Arizona, L=Scottsdale, O=Starfield Technologies, Inc., CN=Starfield Root Certificate Authority - G2" },
        { UINT64_C(0xa3ce8d99e60eda00), "C=BE, O=GlobalSign nv-sa, OU=Root CA, CN=GlobalSign Root CA" },
        { UINT64_C(0xa671e9fec832b700), "C=US, O=Starfield Technologies, Inc., OU=Starfield Class 2 Certification Authority" },
        { UINT64_C(0xa8de7211e13be200), "C=US, O=DigiCert Inc, OU=www.digicert.com, CN=DigiCert Global Root CA" },
        { UINT64_C(0x0ff3891b54348328), "C=US, O=Entrust.net, OU=www.entrust.net/CPS incorp. by ref. (limits liab.), OU=(c) 1999 Entrust.net Limited, CN=Entrust.netSecure Server Certification Authority" },
        { UINT64_C(0x7ae89c50f0b6a00f), "C=US, O=GTE Corporation, OU=GTE CyberTrust Solutions, Inc., CN=GTE CyberTrust Global Root" },
        { UINT64_C(0xd45980fbf0a0ac00), "C=US, O=thawte, Inc., OU=Certification Services Division, OU=(c) 2006 thawte, Inc. - For authorized use only, CN=thawte Primary Root CA" },
        { UINT64_C(0x9e5bc2d78b6a3636), "C=ZA, ST=Western Cape, L=Cape Town, O=Thawte Consulting cc, OU=Certification Services Division, CN=Thawte Premium Server CA, Email=premium-server@thawte.com" },
        { UINT64_C(0x7c4fd32ec1b1ce00), "C=PL, O=Unizeto Sp. z o.o., CN=Certum CA" },
        { UINT64_C(0xd4fbe673e5ccc600), "C=US, O=DigiCert Inc, OU=www.digicert.com, CN=DigiCert High Assurance EV Root CA" },
        { UINT64_C(0x16e64d2a56ccf200), "C=US, ST=Arizona, L=Scottsdale, O=Starfield Technologies, Inc., OU=http://certificates.starfieldtech.com/repository/, CN=Starfield Services Root Certificate Authority" },
        { UINT64_C(0x6e2ba21058eedf00), "C=US, ST=UT, L=Salt Lake City, O=The USERTRUST Network, OU=http://www.usertrust.com, CN=UTN - DATACorp SGC" },
        { UINT64_C(0xb28612a94b4dad00), "O=Entrust.net, OU=www.entrust.net/CPS_2048 incorp. by ref. (limits liab.), OU=(c) 1999 Entrust.net Limited, CN=Entrust.netCertification Authority (2048)" },
        { UINT64_C(0x357a29080824af00), "C=US, O=VeriSign, Inc., OU=VeriSign Trust Network, OU=(c) 2006 VeriSign, Inc. - For authorized use only, CN=VeriSign Class3 Public Primary Certification Authority - G5" },
        { UINT64_C(0x466cbc09db88c100), "C=IL, O=StartCom Ltd., OU=Secure Digital Certificate Signing, CN=StartCom Certification Authority" },
        { UINT64_C(0x9259c8abe5ca713a), "L=ValiCert Validation Network, O=ValiCert, Inc., OU=ValiCert Class 2 Policy Validation Authority, CN=http://www.valicert.com/, Email=info@valicert.com" },
        { UINT64_C(0x1f78fc529cbacb00), "C=US, O=VeriSign, Inc., OU=VeriSign Trust Network, OU=(c) 1999 VeriSign, Inc. - For authorized use only, CN=VeriSign Class3 Public Primary Certification Authority - G3" },
        { UINT64_C(0x8043e4ce150ead00), "C=US, O=DigiCert Inc, OU=www.digicert.com, CN=DigiCert Assured ID Root CA" },
        { UINT64_C(0x00f2e6331af7b700), "C=SE, O=AddTrust AB, OU=AddTrust External TTP Network, CN=AddTrust External CA Root" },
    };


    uint64_t const u64KeyId = pCert->TbsCertificate.SubjectPublicKeyInfo.SubjectPublicKey.uBits.pu64[1];
    uint32_t i = RT_ELEMENTS(s_aWanted);
    while (i-- > 0)
        if (   s_aWanted[i].u64KeyId == u64KeyId
            || s_aWanted[i].u64KeyId == UINT64_MAX)
            if (RTCrX509Name_MatchWithString(&pCert->TbsCertificate.Subject, s_aWanted[i].pszName))
                return true;

#ifdef DEBUG_bird
    char szTmp[512];
    szTmp[sizeof(szTmp) - 1] = '\0';
    RTCrX509Name_FormatAsString(&pCert->TbsCertificate.Issuer, szTmp, sizeof(szTmp) - 1, NULL);
    SUP_DPRINTF(("supR3HardenedWinIsDesiredRootCA: %#llx %s\n", u64KeyId, szTmp));
#endif
    return false;
}

/**
 * Called by supR3HardenedWinResolveVerifyTrustApiAndHookThreadCreation to
 * import selected root CAs from the system certificate store.
 *
 * These certificates permits us to correctly validate third party DLLs.
 *
 * @param   fLoadLibraryFlags       The LoadLibraryExW flags that the caller
 *                                  found to work.  Avoids us having to retry on
 *                                  ERROR_INVALID_PARAMETER.
 */
static void supR3HardenedWinRetrieveTrustedRootCAs(DWORD fLoadLibraryFlags)
{
    uint32_t cAdded = 0;

    /*
     * Load crypt32.dll and resolve the APIs we need.
     */
    HMODULE hCrypt32 = LoadLibraryExW(L"\\\\.\\GLOBALROOT\\SystemRoot\\System32\\crypt32.dll", NULL, fLoadLibraryFlags);
    if (!hCrypt32)
        supR3HardenedFatal("Error loading 'crypt32.dll': %u", GetLastError());

#define RESOLVE_CRYPT32_API(a_Name, a_pfnType) \
    a_pfnType pfn##a_Name = (a_pfnType)GetProcAddress(hCrypt32, #a_Name); \
    if (pfn##a_Name == NULL) supR3HardenedFatal("Error locating '" #a_Name "' in 'crypt32.dll': %u", GetLastError())
    RESOLVE_CRYPT32_API(CertOpenStore, PFNCERTOPENSTORE);
    RESOLVE_CRYPT32_API(CertCloseStore, PFNCERTCLOSESTORE);
    RESOLVE_CRYPT32_API(CertEnumCertificatesInStore, PFNCERTENUMCERTIFICATESINSTORE);
#undef RESOLVE_CRYPT32_API

    /*
     * Open the root store and look for the certificates we wish to use.
     */
    DWORD fOpenStore = CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG;
    HCERTSTORE hStore = pfnCertOpenStore(CERT_STORE_PROV_SYSTEM_W, PKCS_7_ASN_ENCODING | X509_ASN_ENCODING,
                                         NULL /* hCryptProv = default */, CERT_SYSTEM_STORE_LOCAL_MACHINE | fOpenStore, L"Root");
    if (!hStore)
        hStore = pfnCertOpenStore(CERT_STORE_PROV_SYSTEM_W, PKCS_7_ASN_ENCODING | X509_ASN_ENCODING,
                                  NULL /* hCryptProv = default */, CERT_SYSTEM_STORE_CURRENT_USER | fOpenStore, L"Root");
    if (hStore)
    {
        PCCERT_CONTEXT pCurCtx  = NULL;
        while ((pCurCtx = pfnCertEnumCertificatesInStore(hStore, pCurCtx)) != NULL)
        {
            if (pCurCtx->dwCertEncodingType & X509_ASN_ENCODING)
            {
                RTASN1CURSORPRIMARY PrimaryCursor;
                RTAsn1CursorInitPrimary(&PrimaryCursor, pCurCtx->pbCertEncoded, pCurCtx->cbCertEncoded, NULL /*pErrInfo*/,
                                        &g_RTAsn1DefaultAllocator, RTASN1CURSOR_FLAGS_DER, "CurCtx");
                RTCRX509CERTIFICATE MyCert;
                int rc = RTCrX509Certificate_DecodeAsn1(&PrimaryCursor.Cursor, 0, &MyCert, "Cert");
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    if (supR3HardenedWinIsDesiredRootCA(&MyCert))
                    {
                        rc = RTCrStoreCertAddEncoded(g_hSpcRootStore, RTCRCERTCTX_F_ENC_X509_DER,
                                                     pCurCtx->pbCertEncoded, pCurCtx->cbCertEncoded, NULL /*pErrInfo*/);
                        AssertRC(rc);

                        rc = RTCrStoreCertAddEncoded(g_hSpcAndNtKernelRootStore, RTCRCERTCTX_F_ENC_X509_DER,
                                                     pCurCtx->pbCertEncoded, pCurCtx->cbCertEncoded, NULL /*pErrInfo*/);
                        AssertRC(rc);
                        cAdded++;
                    }

                    RTCrX509Certificate_Delete(&MyCert);
                }
            }
        }
        pfnCertCloseStore(hStore, CERT_CLOSE_STORE_CHECK_FLAG);
        g_fHaveOtherRoots = true;
    }
    SUP_DPRINTF(("supR3HardenedWinRetrieveTrustedRootCAs: cAdded=%u\n", cAdded));
}


/**
 * Resolves the WinVerifyTrust API after the process has been verified and
 * installs a thread creation hook.
 *
 * The WinVerifyTrust API is used in addition our own Authenticode verification
 * code.  If the image has the IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY flag
 * set, it will be checked again by the kernel.  All our image has this flag set
 * and we require all VBox extensions to have it set as well.  In effect, the
 * authenticode signature will be checked two or three times.
 */
DECLHIDDEN(void) supR3HardenedWinResolveVerifyTrustApiAndHookThreadCreation(void)
{
# ifdef IN_SUP_HARDENED_R3
    /*
     * Load our the support library DLL that does the thread hooking as the
     * security API may trigger the creation of COM worker threads (or
     * whatever they are).
     *
     * The thread creation hook makes the threads very slippery to debuggers by
     * irreversably disabling most (if not all) debug events for them.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedPathSharedLibs(szPath, sizeof(szPath) - sizeof("/VBoxSupLib.DLL"));
    suplibHardenedStrCat(szPath, "/VBoxSupLib.DLL");
    HMODULE hSupLibMod = (HMODULE)supR3HardenedWinLoadLibrary(szPath, true /*fSystem32Only*/);
    if (hSupLibMod == NULL)
        supR3HardenedFatal("Error loading '%s': %u", szPath, GetLastError());
# endif

    /*
     * Resolve it.
     */
    DWORD fFlags = 0;
    if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0))
       fFlags = LOAD_LIBRARY_SEARCH_SYSTEM32;
    HMODULE hWintrust = LoadLibraryExW(L"\\\\.\\GLOBALROOT\\SystemRoot\\System32\\Wintrust.dll", NULL, fFlags);
    if (   hWintrust == NULL
        && fFlags
        && g_uNtVerCombined < SUP_MAKE_NT_VER_SIMPLE(6, 2)
        && GetLastError() == ERROR_INVALID_PARAMETER)
    {
        fFlags = 0;
        hWintrust = LoadLibraryExW(L"\\\\.\\GLOBALROOT\\SystemRoot\\System32\\Wintrust.dll", NULL, fFlags);
    }
    if (hWintrust == NULL)
        supR3HardenedFatal("Error loading 'Wintrust.dll': %u", GetLastError());

#define RESOLVE_CRYPT_API(a_Name, a_pfnType, a_uMinWinVer) \
    do { \
        g_pfn##a_Name = (a_pfnType)GetProcAddress(hWintrust, #a_Name); \
        if (g_pfn##a_Name == NULL && (a_uMinWinVer) < g_uNtVerCombined) \
            supR3HardenedFatal("Error locating '" #a_Name "' in 'Wintrust.dll': %u", GetLastError()); \
    } while (0)

    PFNWINVERIFYTRUST pfnWinVerifyTrust = (PFNWINVERIFYTRUST)GetProcAddress(hWintrust, "WinVerifyTrust");
    if (!pfnWinVerifyTrust)
        supR3HardenedFatal("Error locating 'WinVerifyTrust' in 'Wintrust.dll': %u", GetLastError());

    RESOLVE_CRYPT_API(CryptCATAdminAcquireContext,           PFNCRYPTCATADMINACQUIRECONTEXT,          0);
    RESOLVE_CRYPT_API(CryptCATAdminCalcHashFromFileHandle,   PFNCRYPTCATADMINCALCHASHFROMFILEHANDLE,  0);
    RESOLVE_CRYPT_API(CryptCATAdminEnumCatalogFromHash,      PFNCRYPTCATADMINENUMCATALOGFROMHASH,     0);
    RESOLVE_CRYPT_API(CryptCATAdminReleaseCatalogContext,    PFNCRYPTCATADMINRELEASECATALOGCONTEXT,   0);
    RESOLVE_CRYPT_API(CryptCATAdminReleaseContext,           PFNCRYPTCATDADMINRELEASECONTEXT,         0);
    RESOLVE_CRYPT_API(CryptCATCatalogInfoFromContext,        PFNCRYPTCATCATALOGINFOFROMCONTEXT,       0);

    RESOLVE_CRYPT_API(CryptCATAdminAcquireContext2,          PFNCRYPTCATADMINACQUIRECONTEXT2,         SUP_NT_VER_W80);
    RESOLVE_CRYPT_API(CryptCATAdminCalcHashFromFileHandle2,  PFNCRYPTCATADMINCALCHASHFROMFILEHANDLE2, SUP_NT_VER_W80);

    /*
     * Call it on ourselves and ntdll to make sure it loads all the providers
     * now, we would otherwise geting into recursive trouble in the
     * NtCreateSection hook.
     */
# ifdef IN_SUP_HARDENED_R3
    RTERRINFOSTATIC ErrInfoStatic;
    RTErrInfoInitStatic(&ErrInfoStatic);
    int rc = supR3HardNtViCallWinVerifyTrust(NULL, g_SupLibHardenedExeNtPath.UniStr.Buffer, 0,
                                             &ErrInfoStatic.Core, pfnWinVerifyTrust);
    if (RT_FAILURE(rc))
        supR3HardenedFatal("WinVerifyTrust failed on stub executable: %s", ErrInfoStatic.szMsg);
# endif

    if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0)) /* ntdll isn't signed on XP, assuming this is the case on W2K3 for now. */
        supR3HardNtViCallWinVerifyTrust(NULL, L"\\SystemRoot\\System32\\ntdll.dll", 0, NULL, pfnWinVerifyTrust);
    supR3HardNtViCallWinVerifyTrustCatFile(NULL, L"\\SystemRoot\\System32\\ntdll.dll", 0, NULL, pfnWinVerifyTrust);

    g_pfnWinVerifyTrust = pfnWinVerifyTrust;

    /*
     * Now, get trusted root CAs so we can verify a broader scope of signatures.
     */
    supR3HardenedWinRetrieveTrustedRootCAs(fFlags);
}


static int supR3HardNtViNtToWinPath(PCRTUTF16 pwszNtName, PCRTUTF16 *ppwszWinPath,
                                    PRTUTF16 pwszWinPathBuf, size_t cwcWinPathBuf)
{
    static const RTUTF16 s_wszPrefix[] = L"\\\\.\\GLOBALROOT";

    if (*pwszNtName != '\\' && *pwszNtName != '/')
        return VERR_PATH_DOES_NOT_START_WITH_ROOT;

    size_t cwcNtName = RTUtf16Len(pwszNtName);
    if (RT_ELEMENTS(s_wszPrefix) + cwcNtName > cwcWinPathBuf)
        return VERR_FILENAME_TOO_LONG;

    memcpy(pwszWinPathBuf, s_wszPrefix, sizeof(s_wszPrefix));
    memcpy(&pwszWinPathBuf[sizeof(s_wszPrefix) / sizeof(RTUTF16) - 1], pwszNtName, (cwcNtName + 1) * sizeof(RTUTF16));
    *ppwszWinPath = pwszWinPathBuf;
    return VINF_SUCCESS;
}


/**
 * Calls WinVerifyTrust to verify an PE image.
 *
 * @returns VBox status code.
 * @param   hFile               File handle to the executable file.
 * @param   pwszName            Full NT path to the DLL in question, used for
 *                              dealing with unsigned system dlls as well as for
 *                              error/logging.
 * @param   fFlags              Flags, SUPHNTVI_F_XXX.
 * @param   pErrInfo            Pointer to error info structure. Optional.
 * @param   pfnWinVerifyTrust   Pointer to the API.
 */
static int supR3HardNtViCallWinVerifyTrust(HANDLE hFile, PCRTUTF16 pwszName, uint32_t fFlags, PRTERRINFO pErrInfo,
                                           PFNWINVERIFYTRUST pfnWinVerifyTrust)
{
    /*
     * Convert the name into a Windows name.
     */
    RTUTF16 wszWinPathBuf[MAX_PATH];
    PCRTUTF16 pwszWinPath;
    int rc = supR3HardNtViNtToWinPath(pwszName, &pwszWinPath, wszWinPathBuf, RT_ELEMENTS(wszWinPathBuf));
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pErrInfo, rc, "Bad path passed to supR3HardNtViCallWinVerifyTrust: rc=%Rrc '%ls'", rc, pwszName);

    /*
     * Construct input parameters and call the API.
     */
    WINTRUST_FILE_INFO FileInfo;
    RT_ZERO(FileInfo);
    FileInfo.cbStruct = sizeof(FileInfo);
    FileInfo.pcwszFilePath = pwszWinPath;
    FileInfo.hFile = hFile;

    GUID PolicyActionGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA TrustData;
    RT_ZERO(TrustData);
    TrustData.cbStruct = sizeof(TrustData);
    TrustData.fdwRevocationChecks = WTD_REVOKE_NONE;  /* Keep simple for now. */
    TrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    TrustData.dwUIChoice = WTD_UI_NONE;
    TrustData.dwProvFlags = 0;
    if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0))
        TrustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    else
        TrustData.dwProvFlags = WTD_REVOCATION_CHECK_NONE;
    TrustData.dwUnionChoice = WTD_CHOICE_FILE;
    TrustData.pFile = &FileInfo;

    HRESULT hrc = pfnWinVerifyTrust(NULL /*hwnd*/, &PolicyActionGuid, &TrustData);
    if (hrc == S_OK)
        rc = VINF_SUCCESS;
    else
    {
        /*
         * Failed. Format a nice error message.
         */
# ifdef DEBUG_bird
        __debugbreak();
# endif
        const char *pszErrConst = NULL;
        switch (hrc)
        {
            case TRUST_E_SYSTEM_ERROR:             pszErrConst = "TRUST_E_SYSTEM_ERROR";          break;
            case TRUST_E_NO_SIGNER_CERT:           pszErrConst = "TRUST_E_NO_SIGNER_CERT";        break;
            case TRUST_E_COUNTER_SIGNER:           pszErrConst = "TRUST_E_COUNTER_SIGNER";        break;
            case TRUST_E_CERT_SIGNATURE:           pszErrConst = "TRUST_E_CERT_SIGNATURE";        break;
            case TRUST_E_TIME_STAMP:               pszErrConst = "TRUST_E_TIME_STAMP";            break;
            case TRUST_E_BAD_DIGEST:               pszErrConst = "TRUST_E_BAD_DIGEST";            break;
            case TRUST_E_BASIC_CONSTRAINTS:        pszErrConst = "TRUST_E_BASIC_CONSTRAINTS";     break;
            case TRUST_E_FINANCIAL_CRITERIA:       pszErrConst = "TRUST_E_FINANCIAL_CRITERIA";    break;
            case TRUST_E_PROVIDER_UNKNOWN:         pszErrConst = "TRUST_E_PROVIDER_UNKNOWN";      break;
            case TRUST_E_ACTION_UNKNOWN:           pszErrConst = "TRUST_E_ACTION_UNKNOWN";        break;
            case TRUST_E_SUBJECT_FORM_UNKNOWN:     pszErrConst = "TRUST_E_SUBJECT_FORM_UNKNOWN";  break;
            case TRUST_E_SUBJECT_NOT_TRUSTED:      pszErrConst = "TRUST_E_SUBJECT_NOT_TRUSTED";   break;
            case TRUST_E_NOSIGNATURE:              pszErrConst = "TRUST_E_NOSIGNATURE";           break;
            case TRUST_E_FAIL:                     pszErrConst = "TRUST_E_FAIL";                  break;
            case TRUST_E_EXPLICIT_DISTRUST:        pszErrConst = "TRUST_E_EXPLICIT_DISTRUST";     break;
        }
        if (pszErrConst)
            rc = RTErrInfoSetF(pErrInfo, VERR_LDRVI_UNSUPPORTED_ARCH,
                               "WinVerifyTrust failed with hrc=%s on '%ls'", pszErrConst, pwszName);
        else
            rc = RTErrInfoSetF(pErrInfo, VERR_LDRVI_UNSUPPORTED_ARCH,
                               "WinVerifyTrust failed with hrc=%Rhrc on '%ls'", hrc, pwszName);
    }

    /* clean up state data. */
    TrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    FileInfo.hFile = NULL;
    hrc = pfnWinVerifyTrust(NULL /*hwnd*/, &PolicyActionGuid, &TrustData);

    return rc;
}


/**
 * Calls WinVerifyTrust to verify an PE image via catalog files.
 *
 * @returns VBox status code.
 * @param   hFile               File handle to the executable file.
 * @param   pwszName            Full NT path to the DLL in question, used for
 *                              dealing with unsigned system dlls as well as for
 *                              error/logging.
 * @param   fFlags              Flags, SUPHNTVI_F_XXX.
 * @param   pErrInfo            Pointer to error info structure. Optional.
 * @param   pfnWinVerifyTrust   Pointer to the API.
 */
static int supR3HardNtViCallWinVerifyTrustCatFile(HANDLE hFile, PCRTUTF16 pwszName, uint32_t fFlags, PRTERRINFO pErrInfo,
                                                  PFNWINVERIFYTRUST pfnWinVerifyTrust)
{
    SUP_DPRINTF(("supR3HardNtViCallWinVerifyTrustCatFile: hFile=%p pwszName=%ls\n", hFile, pwszName));

    /*
     * Convert the name into a Windows name.
     */
    RTUTF16 wszWinPathBuf[MAX_PATH];
    PCRTUTF16 pwszWinPath;
    int rc = supR3HardNtViNtToWinPath(pwszName, &pwszWinPath, wszWinPathBuf, RT_ELEMENTS(wszWinPathBuf));
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pErrInfo, rc, "Bad path passed to supR3HardNtViCallWinVerifyTrustCatFile: rc=%Rrc '%ls'", rc, pwszName);

    /*
     * Open the file if we didn't get a handle.
     */
    HANDLE hFileClose = NULL;
    if (hFile == RTNT_INVALID_HANDLE_VALUE || hFile == NULL)
    {
        hFile = RTNT_INVALID_HANDLE_VALUE;
        IO_STATUS_BLOCK     Ios   = RTNT_IO_STATUS_BLOCK_INITIALIZER;

        UNICODE_STRING      NtName;
        NtName.Buffer = (PWSTR)pwszName;
        NtName.Length = (USHORT)(RTUtf16Len(pwszName) * sizeof(WCHAR));
        NtName.MaximumLength = NtName.Length + sizeof(WCHAR);

        OBJECT_ATTRIBUTES ObjAttr;
        InitializeObjectAttributes(&ObjAttr, &NtName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);

        NTSTATUS rcNt = NtCreateFile(&hFile,
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
            return RTErrInfoSetF(pErrInfo, RTErrConvertFromNtStatus(rcNt),
                                 "NtCreateFile returned %#x opening '%ls'.", rcNt, pwszName);
        hFileClose = hFile;
    }

    /*
     * On Windows 8.0 and later there are more than one digest choice.
     */
    rc = VERR_LDRVI_NOT_SIGNED;
    static struct
    {
        /** The digest algorithm name. */
        const WCHAR        *pszAlgorithm;
        /** Cached catalog admin handle. */
        HCATADMIN volatile  hCachedCatAdmin;
    } s_aHashes[] =
    {
        { NULL,      NULL },
        { L"SHA256", NULL },
    };
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aHashes); i++)
    {
        /*
         * Another loop for dealing with different trust provider policies
         * required for successfully validating different catalog signatures.
         */
        bool                fTryNextPolicy;
        uint32_t            iPolicy = 0;
        static const GUID   s_aPolicies[] =
        {
            DRIVER_ACTION_VERIFY,              /* Works with microsoft bits. Most frequently used, thus first. */
            WINTRUST_ACTION_GENERIC_VERIFY_V2, /* Works with ATI and other SPC kernel-code signed stuff. */
        };
        do
        {
            /*
             * Create a context.
             */
            fTryNextPolicy = false;
            BOOL fRc;
            HCATADMIN hCatAdmin = ASMAtomicXchgPtr(&s_aHashes[i].hCachedCatAdmin, NULL);
            if (hCatAdmin)
                fRc = TRUE;
            else if (g_pfnCryptCATAdminAcquireContext2)
                fRc = g_pfnCryptCATAdminAcquireContext2(&hCatAdmin, &s_aPolicies[iPolicy], s_aHashes[i].pszAlgorithm,
                                                        NULL /*pStrongHashPolicy*/, 0 /*dwFlags*/);
            else
                fRc = g_pfnCryptCATAdminAcquireContext(&hCatAdmin, &s_aPolicies[iPolicy], 0 /*dwFlags*/);
            if (fRc)
            {
                SUP_DPRINTF(("supR3HardNtViCallWinVerifyTrustCatFile: hCatAdmin=%p\n", hCatAdmin));

                /*
                 * Hash the file.
                 */
                BYTE  abHash[SUPHARDNTVI_MAX_CAT_HASH_SIZE];
                DWORD cbHash = sizeof(abHash);
                if (g_pfnCryptCATAdminCalcHashFromFileHandle2)
                    fRc = g_pfnCryptCATAdminCalcHashFromFileHandle2(hCatAdmin, hFile, &cbHash, abHash, 0 /*dwFlags*/);
                else
                    fRc = g_pfnCryptCATAdminCalcHashFromFileHandle(hFile, &cbHash, abHash, 0 /*dwFlags*/);
                if (fRc)
                {
                    /* Produce a string version of it that we can pass to WinVerifyTrust. */
                    RTUTF16 wszDigest[SUPHARDNTVI_MAX_CAT_HASH_SIZE * 2 + 1];
                    int rc2 = RTUtf16PrintHexBytes(wszDigest, RT_ELEMENTS(wszDigest), abHash, cbHash, RTSTRPRINTHEXBYTES_F_UPPER);
                    if (RT_SUCCESS(rc2))
                    {
                        SUP_DPRINTF(("supR3HardNtViCallWinVerifyTrustCatFile: cbHash=%u wszDigest=%ls\n", cbHash, wszDigest));

                        /*
                         * Enumerate catalog information that matches the hash.
                         */
                        uint32_t iCat = 0;
                        HCATINFO hCatInfoPrev = NULL;
                        do
                        {
                            /* Get the next match. */
                            HCATINFO hCatInfo = g_pfnCryptCATAdminEnumCatalogFromHash(hCatAdmin, abHash, cbHash, 0, &hCatInfoPrev);
                            if (!hCatInfo)
                            {
                                if (iCat == 0)
                                    SUP_DPRINTF(("supR3HardNtViCallWinVerifyTrustCatFile: CryptCATAdminEnumCatalogFromHash failed %u\n", GetLastError()));
                                break;
                            }
                            Assert(hCatInfoPrev == NULL);
                            hCatInfoPrev = hCatInfo;

                            /*
                             * Call WinVerifyTrust.
                             */
                            CATALOG_INFO CatInfo;
                            CatInfo.cbStruct = sizeof(CatInfo);
                            CatInfo.wszCatalogFile[0] = '\0';
                            if (g_pfnCryptCATCatalogInfoFromContext(hCatInfo, &CatInfo, 0 /*dwFlags*/))
                            {
                                WINTRUST_CATALOG_INFO WtCatInfo;
                                RT_ZERO(WtCatInfo);
                                WtCatInfo.cbStruct              = sizeof(WtCatInfo);
                                WtCatInfo.dwCatalogVersion      = 0;
                                WtCatInfo.pcwszCatalogFilePath  = CatInfo.wszCatalogFile;
                                WtCatInfo.pcwszMemberTag        = wszDigest;
                                WtCatInfo.pcwszMemberFilePath   = pwszWinPath;
                                WtCatInfo.pbCalculatedFileHash  = abHash;
                                WtCatInfo.cbCalculatedFileHash  = cbHash;
                                WtCatInfo.pcCatalogContext      = NULL;

                                WINTRUST_DATA TrustData;
                                RT_ZERO(TrustData);
                                TrustData.cbStruct              = sizeof(TrustData);
                                TrustData.fdwRevocationChecks   = WTD_REVOKE_NONE;  /* Keep simple for now. */
                                TrustData.dwStateAction         = WTD_STATEACTION_VERIFY;
                                TrustData.dwUIChoice            = WTD_UI_NONE;
                                TrustData.dwProvFlags           = 0;
                                if (g_uNtVerCombined >= SUP_MAKE_NT_VER_SIMPLE(6, 0))
                                    TrustData.dwProvFlags       = WTD_CACHE_ONLY_URL_RETRIEVAL;
                                else
                                    TrustData.dwProvFlags       = WTD_REVOCATION_CHECK_NONE;
                                TrustData.dwUnionChoice         = WTD_CHOICE_CATALOG;
                                TrustData.pCatalog              = &WtCatInfo;

                                HRESULT hrc = pfnWinVerifyTrust(NULL /*hwnd*/, &s_aPolicies[iPolicy], &TrustData);
                                SUP_DPRINTF(("supR3HardNtViCallWinVerifyTrustCatFile: WinVerifyTrust => %#x; cat=%ls\n", hrc, CatInfo.wszCatalogFile));

                                if (SUCCEEDED(hrc))
                                    rc = VINF_SUCCESS;
                                else if (hrc == TRUST_E_NOSIGNATURE)
                                { /* ignore because it's useless. */ }
                                else if (hrc == ERROR_INVALID_PARAMETER)
                                { /* This is returned if the given file isn't found in the catalog, it seems. */ }
                                else
                                {
                                    rc = RTErrInfoSetF(pErrInfo, VERR_SUP_VP_WINTRUST_CAT_FAILURE,
                                                       "WinVerifyTrust failed with hrc=%#x on '%ls' and .cat-file='%ls'.",
                                                       hrc, pwszWinPath, CatInfo.wszCatalogFile);
                                    fTryNextPolicy = (hrc == CERT_E_UNTRUSTEDROOT);
                                }

                                /* clean up state data. */
                                TrustData.dwStateAction = WTD_STATEACTION_CLOSE;
                                hrc = pfnWinVerifyTrust(NULL /*hwnd*/, &s_aPolicies[iPolicy], &TrustData);
                                Assert(SUCCEEDED(hrc));
                            }
                            else
                            {
                                rc = RTErrInfoSetF(pErrInfo, RTErrConvertFromWin32(GetLastError()),
                                                   "CryptCATCatalogInfoFromContext failed: %d [file=%s]",
                                                   GetLastError(), pwszName);
                                SUP_DPRINTF(("supR3HardNtViCallWinVerifyTrustCatFile: CryptCATCatalogInfoFromContext failed\n"));
                            }
                            iCat++;
                        } while (rc == VERR_LDRVI_NOT_SIGNED && iCat < 128);

                        if (hCatInfoPrev != NULL)
                            if (!g_pfnCryptCATAdminReleaseCatalogContext(hCatAdmin, hCatInfoPrev, 0 /*dwFlags*/))
                                AssertFailed();
                    }
                    else
                        rc = RTErrInfoSetF(pErrInfo, rc2, "RTUtf16PrintHexBytes failed: %Rrc", rc);
                }
                else
                    rc = RTErrInfoSetF(pErrInfo, RTErrConvertFromWin32(GetLastError()),
                                       "CryptCATAdminCalcHashFromFileHandle[2] failed: %d [file=%s]", GetLastError(), pwszName);

                if (!ASMAtomicCmpXchgPtr(&s_aHashes[i].hCachedCatAdmin, hCatAdmin, NULL))
                    if (!g_pfnCryptCATAdminReleaseContext(hCatAdmin, 0 /*dwFlags*/))
                        AssertFailed();
            }
            else
                rc = RTErrInfoSetF(pErrInfo, RTErrConvertFromWin32(GetLastError()),
                                   "CryptCATAdminAcquireContext[2] failed: %d [file=%s]", GetLastError(), pwszName);
             iPolicy++;
        } while (   fTryNextPolicy
                 && iPolicy < RT_ELEMENTS(s_aPolicies));

        /*
         * Only repeat if we've got g_pfnCryptCATAdminAcquireContext2 and can specify the hash algorithm.
         */
        if (!g_pfnCryptCATAdminAcquireContext2)
            break;
        if (rc != VERR_LDRVI_NOT_SIGNED)
            break;
    }

    if (hFileClose != NULL)
        NtClose(hFileClose);

    return rc;
}


/**
 * Initializes g_uNtVerCombined and g_NtVerInfo.
 * Called from suplibHardenedWindowsMain and suplibOsInit.
 */
DECLHIDDEN(void) supR3HardenedWinInitVersion(void)
{
    /*
     * Get the windows version.  Use RtlGetVersion as GetVersionExW and
     * GetVersion might not be telling the whole truth (8.0 on 8.1 depending on
     * the application manifest).
     */
    OSVERSIONINFOEXW NtVerInfo;

    suplibHardenedMemSet(&NtVerInfo, 0, sizeof(NtVerInfo));
    NtVerInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);
    if (!NT_SUCCESS(RtlGetVersion((PRTL_OSVERSIONINFOW)&NtVerInfo)))
    {
        suplibHardenedMemSet(&NtVerInfo, 0, sizeof(NtVerInfo));
        NtVerInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
        if (!NT_SUCCESS(RtlGetVersion((PRTL_OSVERSIONINFOW)&NtVerInfo)))
        {
            NtVerInfo.dwOSVersionInfoSize = sizeof(NtVerInfo);
            if (!GetVersionExW((OSVERSIONINFOW *)&NtVerInfo))
            {
                suplibHardenedMemSet(&NtVerInfo, 0, sizeof(NtVerInfo));
                DWORD dwVer = GetVersion();
                NtVerInfo.dwMajorVersion = RT_BYTE1(dwVer);
                NtVerInfo.dwMinorVersion = RT_BYTE2(dwVer);
                NtVerInfo.dwBuildNumber  = RT_BIT_32(31) & dwVer ? 0 : RT_HI_U16(dwVer);
            }
        }
    }
    g_uNtVerCombined = SUP_MAKE_NT_VER_COMBINED(NtVerInfo.dwMajorVersion, NtVerInfo.dwMinorVersion, NtVerInfo.dwBuildNumber,
                                                NtVerInfo.wServicePackMajor, NtVerInfo.wServicePackMinor);
}

#endif /* IN_RING3 */

