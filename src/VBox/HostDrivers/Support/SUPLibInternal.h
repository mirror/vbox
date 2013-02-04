/* $Id$ */
/** @file
 * VirtualBox Support Library - Internal header.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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

#ifndef ___SUPLibInternal_h___
#define ___SUPLibInternal_h___

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/stdarg.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def SUPLIB_DLL_SUFF
 * The (typical) DLL/DYLIB/SO suffix. */
#if defined(RT_OS_DARWIN)
# define SUPLIB_DLL_SUFF    ".dylib"
#elif defined(RT_OS_L4)
# define SUPLIB_DLL_SUFF    ".s.so"
#elif defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define SUPLIB_DLL_SUFF    ".dll"
#else
# define SUPLIB_DLL_SUFF    ".so"
#endif

#ifdef RT_OS_SOLARIS
/** Number of dummy files to open (2:ip4, 1:ip6, 1:extra) see
 *  @bugref{4650}. */
# define SUPLIB_FLT_DUMMYFILES 4
#endif

/** @def SUPLIB_EXE_SUFF
 * The (typical) executable suffix. */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define SUPLIB_EXE_SUFF    ".exe"
#else
# define SUPLIB_EXE_SUFF    ""
#endif

/** @def SUP_HARDENED_SUID
 * Whether we're employing set-user-ID-on-execute in the hardening.
 */
#if !defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS) && !defined(RT_OS_L4)
# define SUP_HARDENED_SUID
#else
# undef  SUP_HARDENED_SUID
#endif

#ifdef IN_SUP_HARDENED_R3
/** @name Make the symbols in SUPR3HardenedStatic different from the VBoxRT ones.
 * We cannot rely on DECLHIDDEN to make this separation for us since it doesn't
 * work with all GCC versions. So, we resort to old fashion precompiler hacking.
 * @{
 */
# define supR3HardenedPathAppPrivateNoArch supR3HardenedStaticPathAppPrivateNoArch
# define supR3HardenedPathAppPrivateArch   supR3HardenedStaticPathAppPrivateArch
# define supR3HardenedPathSharedLibs       supR3HardenedStaticPathSharedLibs
# define supR3HardenedPathAppDocs          supR3HardenedStaticPathAppDocs
# define supR3HardenedPathExecDir          supR3HardenedStaticPathExecDir
# define supR3HardenedPathFilename         supR3HardenedStaticPathFilename
# define supR3HardenedFatalV               supR3HardenedStaticFatalV
# define supR3HardenedFatal                supR3HardenedStaticFatal
# define supR3HardenedFatalMsgV            supR3HardenedStaticFatalMsgV
# define supR3HardenedFatalMsg             supR3HardenedStaticFatalMsg
# define supR3HardenedErrorV               supR3HardenedStaticErrorV
# define supR3HardenedError                supR3HardenedStaticError
# define supR3HardenedVerifyAll            supR3HardenedStaticVerifyAll
# define supR3HardenedVerifyFixedDir       supR3HardenedStaticVerifyFixedDir
# define supR3HardenedVerifyFixedFile      supR3HardenedStaticVerifyFixedFile
# define supR3HardenedVerifyDir            supR3HardenedStaticVerifyDir
# define supR3HardenedVerifyFile           supR3HardenedStaticVerifyFile
# define supR3HardenedGetPreInitData       supR3HardenedStaticGetPreInitData
# define supR3HardenedRecvPreInitData      supR3HardenedStaticRecvPreInitData
/** @} */
#endif /* IN_SUP_HARDENED_R3 */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * The type of an installed file.
 */
typedef enum SUPINSTFILETYPE
{
    kSupIFT_Invalid = 0,
    kSupIFT_Exe,
    kSupIFT_Dll,
    kSupIFT_Sys,
    kSupIFT_Script,
    kSupIFT_Data,
    kSupIFT_End
} SUPINSTFILETYPE;

/**
 * Installation directory specifier.
 */
typedef enum SUPINSTDIR
{
    kSupID_Invalid = 0,
    kSupID_Bin,
    kSupID_AppBin,
    kSupID_SharedLib,
    kSupID_AppPrivArch,
    kSupID_AppPrivArchComp,
    kSupID_AppPrivNoArch,
    kSupID_End
} SUPINSTDIR;

/**
 * Installed file.
 */
typedef struct SUPINSTFILE
{
    /** File type. */
    SUPINSTFILETYPE enmType;
    /** Install directory. */
    SUPINSTDIR enmDir;
    /** Optional (true) or mandatory (false. */
    bool fOptional;
    /** File name. */
    const char *pszFile;
} SUPINSTFILE;
typedef SUPINSTFILE *PSUPINSTFILE;
typedef SUPINSTFILE const *PCSUPINSTFILE;

/**
 * Status data for a verified file.
 */
typedef struct SUPVERIFIEDFILE
{
    /** The file handle or descriptor. -1 if not open. */
    intptr_t    hFile;
    /** Whether the file has been validated. */
    bool        fValidated;
} SUPVERIFIEDFILE;
typedef SUPVERIFIEDFILE *PSUPVERIFIEDFILE;
typedef SUPVERIFIEDFILE const *PCSUPVERIFIEDFILE;

/**
 * Status data for a verified directory.
 */
typedef struct SUPVERIFIEDDIR
{
    /** The directory handle or descriptor. -1 if not open. */
    intptr_t    hDir;
    /** Whether the directory has been validated. */
    bool        fValidated;
} SUPVERIFIEDDIR;
typedef SUPVERIFIEDDIR *PSUPVERIFIEDDIR;
typedef SUPVERIFIEDDIR const *PCSUPVERIFIEDDIR;


/**
 * SUPLib instance data.
 *
 * This is data that is passed from the static to the dynamic SUPLib
 * in a hardened setup.
 */
typedef struct SUPLIBDATA
{
    /** The device handle. */
#if defined(RT_OS_WINDOWS)
    void               *hDevice;
#else
    int                 hDevice;
#endif
    /** Indicates whether we have unrestricted (true) or restricted access to the
     * support device. */
    bool                fUnrestricted;
#if   defined(RT_OS_DARWIN)
    /** The connection to the VBoxSupDrv service. */
    uintptr_t           uConnection;
#elif defined(RT_OS_LINUX)
    /** Indicates whether madvise(,,MADV_DONTFORK) works. */
    bool                fSysMadviseWorks;
#elif defined(RT_OS_SOLARIS)
    /** Extra dummy file descriptors to prevent growing file-descriptor table on
     *  clean up (see @bugref{4650}). */
    int                 ahDummy[SUPLIB_FLT_DUMMYFILES];
#elif defined(RT_OS_WINDOWS)
#endif
} SUPLIBDATA;
/** Pointer to the pre-init data. */
typedef SUPLIBDATA *PSUPLIBDATA;
/** Pointer to const pre-init data. */
typedef SUPLIBDATA const *PCSUPLIBDATA;

/** The NIL value of SUPLIBDATA::hDevice. */
#if defined(RT_OS_WINDOWS)
# define SUP_HDEVICE_NIL NULL
#else
# define SUP_HDEVICE_NIL (-1)
#endif


/**
 * Pre-init data that is handed over from the hardened executable stub.
 */
typedef struct SUPPREINITDATA
{
    /** Magic value (SUPPREINITDATA_MAGIC). */
    uint32_t            u32Magic;
    /** The SUPLib instance data. */
    SUPLIBDATA          Data;
    /** The number of entries in paInstallFiles and paVerifiedFiles. */
    size_t              cInstallFiles;
    /** g_aSupInstallFiles. */
    PCSUPINSTFILE       paInstallFiles;
    /** g_aSupVerifiedFiles. */
    PCSUPVERIFIEDFILE   paVerifiedFiles;
    /** The number of entries in paVerifiedDirs. */
    size_t              cVerifiedDirs;
    /** g_aSupVerifiedDirs. */
    PCSUPVERIFIEDDIR    paVerifiedDirs;
    /** Magic value (SUPPREINITDATA_MAGIC). */
    uint32_t            u32EndMagic;
} SUPPREINITDATA;
typedef SUPPREINITDATA *PSUPPREINITDATA;
typedef SUPPREINITDATA const *PCSUPPREINITDATA;

/** Magic value for SUPPREINITDATA::u32Magic and SUPPREINITDATA::u32EndMagic. */
#define SUPPREINITDATA_MAGIC    UINT32_C(0xbeef0001)

/** @copydoc supR3PreInit */
typedef DECLCALLBACK(int) FNSUPR3PREINIT(PSUPPREINITDATA pPreInitData, uint32_t fFlags);
/** Pointer to supR3PreInit. */
typedef FNSUPR3PREINIT *PFNSUPR3PREINIT;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
extern DECLHIDDEN(uint32_t)     g_u32Cookie;
extern DECLHIDDEN(uint32_t)     g_u32SessionCookie;
extern DECLHIDDEN(SUPLIBDATA)   g_supLibData;


/*******************************************************************************
*   OS Specific Function                                                       *
*******************************************************************************/
RT_C_DECLS_BEGIN
int     suplibOsInstall(void);
int     suplibOsUninstall(void);
int     suplibOsInit(PSUPLIBDATA pThis, bool fPreInited, bool fUnrestricted);
int     suplibOsTerm(PSUPLIBDATA pThis);
int     suplibOsIOCtl(PSUPLIBDATA pThis, uintptr_t uFunction, void *pvReq, size_t cbReq);
int     suplibOsIOCtlFast(PSUPLIBDATA pThis, uintptr_t uFunction, uintptr_t idCpu);
int     suplibOsPageAlloc(PSUPLIBDATA pThis, size_t cPages, void **ppvPages);
int     suplibOsPageFree(PSUPLIBDATA pThis, void *pvPages, size_t cPages);
int     suplibOsQueryVTxSupported(void);


/**
 * Performs the pre-initialization of the support library.
 *
 * This is dynamically resolved and invoked by the static library before it
 * calls RTR3InitEx and thereby SUPR3Init.
 *
 * @returns IPRT status code.
 * @param   pPreInitData    The pre init data.
 * @param   fFlags          The SUPR3HardenedMain flags.
 */
DECLEXPORT(int) supR3PreInit(PSUPPREINITDATA pPreInitData, uint32_t fFlags);


/** @copydoc RTPathAppPrivateNoArch */
DECLHIDDEN(int)    supR3HardenedPathAppPrivateNoArch(char *pszPath, size_t cchPath);
/** @copydoc RTPathAppPrivateArch */
DECLHIDDEN(int)    supR3HardenedPathAppPrivateArch(char *pszPath, size_t cchPath);
/** @copydoc RTPathSharedLibs */
DECLHIDDEN(int)    supR3HardenedPathSharedLibs(char *pszPath, size_t cchPath);
/** @copydoc RTPathAppDocs */
DECLHIDDEN(int)    supR3HardenedPathAppDocs(char *pszPath, size_t cchPath);
/** @copydoc RTPathExecDir */
DECLHIDDEN(int)    supR3HardenedPathExecDir(char *pszPath, size_t cchPath);
/** @copydoc RTPathFilename */
DECLHIDDEN(char *) supR3HardenedPathFilename(const char *pszPath);

/**
 * Display a fatal error and try call TrustedError or quit.
 */
DECLHIDDEN(void)   supR3HardenedFatalMsgV(const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, va_list va);

/**
 * Display a fatal error and try call TrustedError or quit.
 */
DECLHIDDEN(void)   supR3HardenedFatalMsg(const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, ...);

/**
 * Display a fatal error and quit.
 */
DECLHIDDEN(void)   supR3HardenedFatalV(const char *pszFormat, va_list va);

/**
 * Display a fatal error and quit.
 */
DECLHIDDEN(void)   supR3HardenedFatal(const char *pszFormat, ...);

/**
 * Display an error which may or may not be fatal.
 */
DECLHIDDEN(int)    supR3HardenedErrorV(int rc, bool fFatal, const char *pszFormat, va_list va);

/**
 * Display an error which may or may not be fatal.
 */
DECLHIDDEN(int)     supR3HardenedError(int rc, bool fFatal, const char *pszFormat, ...);
DECLHIDDEN(int)     supR3HardenedVerifyAll(bool fFatal, bool fLeaveFilesOpen, const char *pszProgName);
DECLHIDDEN(int)     supR3HardenedVerifyFixedDir(SUPINSTDIR enmDir, bool fFatal);
DECLHIDDEN(int)     supR3HardenedVerifyFixedFile(const char *pszFilename, bool fFatal);
DECLHIDDEN(int)     supR3HardenedVerifyDir(const char *pszDirPath, bool fRecursive, bool fCheckFiles, PRTERRINFO pErrInfo);
DECLHIDDEN(int)     supR3HardenedVerifyFile(const char *pszFilename, RTHCUINTPTR hNativeFile, PRTERRINFO pErrInfo);
DECLHIDDEN(void)    supR3HardenedGetPreInitData(PSUPPREINITDATA pPreInitData);
DECLHIDDEN(int)     supR3HardenedRecvPreInitData(PCSUPPREINITDATA pPreInitData);


SUPR3DECL(int)      supR3PageLock(void *pvStart, size_t cPages, PSUPPAGE paPages);
SUPR3DECL(int)      supR3PageUnlock(void *pvStart);

RT_C_DECLS_END


#endif

