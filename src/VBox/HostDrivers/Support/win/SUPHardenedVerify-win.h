/* $Id$ */
/** @file
 * VirtualBox Support Library/Driver - Hardened Verification, Windows.
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

#ifndef ___win_SUPHardenedVerify_win_h
#define ___win_SUPHardenedVerify_win_h

#include <iprt/types.h>
#include <iprt/crypto/x509.h>
#ifndef SUP_CERTIFICATES_ONLY
# ifdef RT_OS_WINDOWS
#  include <iprt/ldr.h>
# endif
#endif


RT_C_DECLS_BEGIN

#ifndef SUP_CERTIFICATES_ONLY
# ifdef RT_OS_WINDOWS
DECLHIDDEN(int)      supHardenedWinInitImageVerifier(PRTERRINFO pErrInfo);
DECLHIDDEN(void)     supHardenedWinTermImageVerifier(void);

typedef enum SUPHARDNTVPKIND
{
    SUPHARDNTVPKIND_VERIFY_ONLY = 1,
    SUPHARDNTVPKIND_CHILD_PURIFICATION,
    SUPHARDNTVPKIND_SELF_PURIFICATION,
    SUPHARDNTVPKIND_32BIT_HACK = 0x7fffffff
} SUPHARDNTVPKIND;
DECLHIDDEN(int)      supHardenedWinVerifyProcess(HANDLE hProcess, HANDLE hThread, SUPHARDNTVPKIND enmKind, PRTERRINFO pErrInfo);

DECLHIDDEN(bool)     supHardViIsAppPatchDir(PCRTUTF16 pwszPath, uint32_t cwcName);

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
DECLHIDDEN(int) supHardNtViRdrCreate(HANDLE hFile, PCRTUTF16 pwszName, uint32_t fFlags, PSUPHNTVIRDR *ppNtViRdr);
DECLHIDDEN(int) supHardenedWinVerifyImageByHandle(HANDLE hFile, PCRTUTF16 pwszName, uint32_t fFlags, bool *pfCacheable, PRTERRINFO pErrInfo);
DECLHIDDEN(int) supHardenedWinVerifyImageByHandleNoName(HANDLE hFile, uint32_t fFlags, PRTERRINFO pErrInfo);
DECLHIDDEN(int) supHardenedWinVerifyImageByLdrMod(RTLDRMOD hLdrMod, PCRTUTF16 pwszName, PSUPHNTVIRDR pNtViRdr,
                                                  bool *pfCacheable, PRTERRINFO pErrInfo);
/** @name SUPHNTVI_F_XXX - Flags for supHardenedWinVerifyImageByHandle.
 * @{ */
/** The signing certificate must be the same as the one the VirtualBox build
 * was signed with. */
#  define SUPHNTVI_F_REQUIRE_BUILD_CERT             RT_BIT(0)
/** Require kernel code signing level. */
#  define SUPHNTVI_F_REQUIRE_KERNEL_CODE_SIGNING    RT_BIT(1)
/** Require the image to force the memory mapper to do signature checking. */
#  define SUPHNTVI_F_REQUIRE_SIGNATURE_ENFORCEMENT  RT_BIT(2)
/** Whether to allow image verification by catalog file. */
#  define SUPHNTVI_F_ALLOW_CAT_FILE_VERIFICATION    RT_BIT(3)
/** Resource image, could be any bitness. */
#  define SUPHNTVI_F_RESOURCE_IMAGE                 RT_BIT(30)
/** Raw-mode context image, always 32-bit. */
#  define SUPHNTVI_F_RC_IMAGE                       RT_BIT(31)
/** @} */

/** Which directory under the system root to get. */
typedef enum SUPHARDNTSYSROOTDIR
{
    kSupHardNtSysRootDir_System32 = 0,
    kSupHardNtSysRootDir_WinSxS,
} SUPHARDNTSYSROOTDIR;

DECLHIDDEN(int) supHardNtGetSystemRootDir(void *pvBuf, uint32_t cbBuf, SUPHARDNTSYSROOTDIR enmDir, PRTERRINFO pErrInfo);

#  ifndef SUPHNTVI_NO_NT_STUFF

/** Typical system root directory buffer. */
typedef struct SUPSYSROOTDIRBUF
{
    UNICODE_STRING  UniStr;
    WCHAR           awcBuffer[260];
} SUPSYSROOTDIRBUF;
extern SUPSYSROOTDIRBUF g_System32NtPath;
extern SUPSYSROOTDIRBUF g_WinSxSNtPath;
extern SUPSYSROOTDIRBUF g_SupLibHardenedExeNtPath;
extern uint32_t         g_offSupLibHardenedExeNtName;

/** Pointer to NtQueryVirtualMemory. */
typedef NTSTATUS (NTAPI *PFNNTQUERYVIRTUALMEMORY)(HANDLE, void const *, MEMORY_INFORMATION_CLASS, PVOID, SIZE_T, PSIZE_T);
extern PFNNTQUERYVIRTUALMEMORY g_pfnNtQueryVirtualMemory;

#  endif /* SUPHNTVI_NO_NT_STUFF */

/** Creates a combined NT version number for simple comparisons. */
#define SUP_MAKE_NT_VER_COMBINED(a_uMajor, a_uMinor, a_uBuild, a_uSpMajor, a_uSpMinor) \
    (   ((uint32_t)((a_uMajor) & UINT32_C(0xf))    << 28) \
      | ((uint32_t)((a_uMinor) & UINT32_C(0xf))    << 24) \
      | ((uint32_t)((a_uBuild) & UINT32_C(0xffff)) << 8) \
      | ((uint32_t)((a_uSpMajor) & UINT32_C(0xf))  << 4) \
      | RT_MIN((uint32_t)(a_uSpMinor), UINT32_C(0xf)) )
/** Simple version of SUP_MAKE_NT_VER_COMBINED. */
#define SUP_MAKE_NT_VER_SIMPLE(a_uMajor, a_uMinor) SUP_MAKE_NT_VER_COMBINED(a_uMajor, a_uMinor, 0, 0, 0)
extern uint32_t         g_uNtVerCombined;

/** Combined NT version number for XP. */
#define SUP_NT_VER_XP       SUP_MAKE_NT_VER_SIMPLE(5,1)
/** Combined NT version number for Windows server 2003 & XP64. */
#define SUP_NT_VER_W2K3     SUP_MAKE_NT_VER_SIMPLE(5,2)
/** Combined NT version number for Vista. */
#define SUP_NT_VER_VISTA    SUP_MAKE_NT_VER_SIMPLE(6,0)
/** Combined NT version number for Windows 7. */
#define SUP_NT_VER_W70      SUP_MAKE_NT_VER_SIMPLE(6,1)
/** Combined NT version number for Windows 8.0. */
#define SUP_NT_VER_W80      SUP_MAKE_NT_VER_SIMPLE(6,2)
/** Combined NT version number for Windows 8.1. */
#define SUP_NT_VER_W81      SUP_MAKE_NT_VER_SIMPLE(6,3)

# endif

# ifndef IN_SUP_HARDENED_R3
#  include <iprt/mem.h>
#  include <iprt/string.h>

#  define suplibHardenedAllocZ       RTMemAllocZ
#  define suplibHardenedReAlloc      RTMemRealloc
#  define suplibHardenedFree         RTMemFree
#  define suplibHardenedMemComp      memcmp
#  define suplibHardenedMemCopy      memcpy
#  define suplibHardenedMemSet       memset
#  define suplibHardenedStrCopy      strcpy
#  define suplibHardenedStrLen       strlen
#  define suplibHardenedStrCat       strcat
#  define suplibHardenedStrCmp       strcmp
#  define suplibHardenedStrNCmp      strncmp
# else   /* IN_SUP_HARDENED_R3 */
#  include <iprt/mem.h>
#if 0
#  define memcmp                     suplibHardenedMemComp
#  define memcpy                     suplibHardenedMemCopy
#  define memset                     suplibHardenedMemSet
#  define strcpy                     suplibHardenedStrCopy
#  define strlen                     suplibHardenedStrLen
#  define strcat                     suplibHardenedStrCat
#  define strcmp                     suplibHardenedStrCmp
#  define strncmp                    suplibHardenedStrNCmp
#endif
DECLHIDDEN(void *)  suplibHardenedAllocZ(size_t cb);
DECLHIDDEN(void *)  suplibHardenedReAlloc(void *pvOld, size_t cbNew);
DECLHIDDEN(void)    suplibHardenedFree(void *pv);
# endif  /* IN_SUP_HARDENED_R3 */

#endif /* SUP_CERTIFICATES_ONLY */

RT_C_DECLS_END

#endif

