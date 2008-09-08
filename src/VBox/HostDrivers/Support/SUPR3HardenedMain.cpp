/* $Id$ */
/** @file
 * VirtualBox Support Library - Hardened main().
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#if defined(RT_OS_OS2)
# define INCL_BASE
# define INCL_ERRORS
# include <os2.h>
# include <stdio.h>

#elif RT_OS_WINDOWS
# include <Windows.h>
# include <stdio.h>

#else /* UNIXes */
# include <iprt/types.h> /* stdint fun on darwin. */

# include <stdio.h>
# include <stdlib.h>
# include <dlfcn.h>
# include <limits.h>
# include <errno.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <stdio.h>
# include <sys/types.h>
# include <pwd.h>
# ifdef RT_OS_DARWIN
#  include <mach-o/dyld.h>
# endif

#endif

#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/string.h>
#include <iprt/param.h>

#include "SUPLibInternal.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def SUP_HARDENED_SUID
 * Whether we're employing set-user-ID-on-execute in the hardening.
 */
#if !defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS) && !defined(RT_OS_L4)
# define SUP_HARDENED_SUID
#else
# undef  SUP_HARDENED_SUID
#endif

/** @def SUP_HARDENED_SYM
 * Decorate a symbol that's resolved dynamically.
 */
#ifdef RT_OS_OS2
# define SUP_HARDENED_SYM(sym)  "_" ## sym
#else
# define SUP_HARDENED_SYM(sym)  sym
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef DECLCALLBACK(int) FNTRUSTEDMAIN(int argc, char **argv, char **envp);
typedef FNTRUSTEDMAIN *PFNTRUSTEDMAIN;

/** @see RTR3InitEx */
typedef DECLCALLBACK(int) FNRTR3INITEX(uint32_t iVersion, const char *pszProgramPath, bool fInitSUPLib);
typedef FNRTR3INITEX *PFNRTR3INITEX;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The pre-init data we pass on to SUPR3 (residing in VBoxRT). */
static SUPPREINITDATA g_SupPreInitData;
/** The progam executable path. */
static char g_szSupLibHardenedExePath[RTPATH_MAX];
/** The program directory path. */
static char g_szSupLibHardenedDirPath[RTPATH_MAX];

/** The program name. */
static const char *g_pszSupLibHardenedProgName;


/**
 * @copydoc RTPathStripFilename.
 */
static void suplibHardenedPathStripFilename(char *pszPath)
{
    char *psz = pszPath;
    char *pszLastSep = pszPath;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastSep = psz + 1;
                break;

            case '\\':
#endif
            case '/':
                pszLastSep = psz;
                break;

            /* the end */
            case '\0':
                if (pszLastSep == pszPath)
                    *pszLastSep++ = '.';
                *pszLastSep = '\0';
                return;
        }
    }
    /* will never get here */
}


/**
 * @copydoc RTPathFilename
 */
DECLHIDDEN(char *) supR3HardenedPathFilename(const char *pszPath)
{
    const char *psz = pszPath;
    const char *pszLastComp = pszPath;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastComp = psz + 1;
                break;

            case '\\':
#endif
            case '/':
                pszLastComp = psz + 1;
                break;

            /* the end */
            case '\0':
                if (*pszLastComp)
                    return (char *)(void *)pszLastComp;
                return NULL;
        }
    }

    /* will never get here */
    return NULL;
}


/**
 * @copydoc RTPathAppPrivateNoArch
 */
DECLHIDDEN(int) supR3HardenedPathAppPrivateNoArch(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_PRIVATE)
    const char *pszSrcPath = RTPATH_APP_PRIVATE;
    size_t cchPathPrivateNoArch = strlen(pszSrcPath);
    if (cchPathPrivateNoArch >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppPrivateNoArch: Buffer overflow, %lu >= %lu\n",
                            (unsigned long)cchPathPrivateNoArch, (unsigned long)cchPath);
    memcpy(pszPath, pszSrcPath, cchPathPrivateNoArch + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathProgram(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathAppPrivateArch
 */
DECLHIDDEN(int) supR3HardenedPathAppPrivateArch(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_PRIVATE_ARCH)
    const char *pszSrcPath = RTPATH_APP_PRIVATE_ARCH;
    size_t cchPathPrivateArch = strlen(pszSrcPath);
    if (cchPathPrivateArch >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppPrivateArch: Buffer overflow, %lu >= %lu\n",
                            (unsigned long)cchPathPrivateArch, (unsigned long)cchPath);
    memcpy(pszPath, pszSrcPath, cchPathPrivateArch + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathProgram(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathSharedLibs
 */
DECLHIDDEN(int) supR3HardenedPathSharedLibs(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_SHARED_LIBS)
    const char *pszSrcPath = RTPATH_SHARED_LIBS;
    size_t cchPathSharedLibs = strlen(pszSrcPath);
    if (cchPathSharedLibs >= cchPath)
        supR3HardenedFatal("supR3HardenedPathSharedLibs: Buffer overflow, %lu >= %lu\n",
                            (unsigned long)cchPathSharedLibs, (unsigned long)cchPath);
    memcpy(pszPath, pszSrcPath, cchPathSharedLibs + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathProgram(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathAppDocs
 */
DECLHIDDEN(int) supR3HardenedPathAppDocs(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_DOCS)
    const char *pszSrcPath = RTPATH_APP_DOCS;
    size_t cchPathAppDocs = strlen(pszSrcPath);
    if (cchPathAppDocs >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppDocs: Buffer overflow, %lu >= %lu\n",
                            (unsigned long)cchPathAppDocs, (unsigned long)cchPath);
    memcpy(pszPath, pszSrcPath, cchPathAppDocs + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathProgram(pszPath, cchPath);
#endif
}


/**
 * Returns the full path to the executable.
 *
 * @returns IPRT status code.
 * @param   pszPath     Where to store it.
 * @param   cchPath     How big that buffer is.
 */
static void supR3HardenedGetFullExePath(void)
{
    /*
     * Get the program filename.
     *
     * Most UNIXes have no API for obtaining the executable path, but provides a symbolic
     * link in the proc file system that tells who was exec'ed. The bad thing about this
     * is that we have to use readlink, one of the weirder UNIX APIs.
     *
     * Darwin, OS/2 and Windows all have proper APIs for getting the program file name.
     */
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD) || defined(RT_OS_SOLARIS)
# ifdef RT_OS_LINUX
    int cchLink = readlink("/proc/self/exe", &g_szSupLibHardenedExePath[0], sizeof(g_szSupLibHardenedExePath) - 1);
# elif defined(RT_OS_SOLARIS)
    char szFileBuf[PATH_MAX + 1];
    sprintf(szFileBuf, "/proc/%ld/path/a.out", (long)getpid());
    int cchLink = readlink(szFileBuf, &g_szSupLibHardenedExePath[0], sizeof(g_szSupLibHardenedExePath) - 1);
# else /* RT_OS_FREEBSD: */
    int cchLink = readlink("/proc/curproc/file", &g_szSupLibHardenedExePath[0], sizeof(g_szSupLibHardenedExePath) - 1);
# endif
    if (cchLink < 0 || cchLink == sizeof(g_szSupLibHardenedExePath) - 1)
        supR3HardenedFatal("supR3HardenedPathProgram: couldn't read \"%s\", errno=%d cchLink=%d\n",
                            g_szSupLibHardenedExePath, errno, cchLink);
    g_szSupLibHardenedExePath[cchLink] = '\0';

#elif defined(RT_OS_OS2) || defined(RT_OS_L4)
    _execname(g_szSupLibHardenedExePath, sizeof(g_szSupLibHardenedExePath));

#elif defined(RT_OS_DARWIN)
    const char *pszImageName = _dyld_get_image_name(0);
    if (!pszImageName)
        supR3HardenedFatal("supR3HardenedPathProgram: _dyld_get_image_name(0) failed\n");
    size_t cchImageName = strlen(pszImageName);
    if (!cchImageName || cchImageName >= sizeof(g_szSupLibHardenedExePath))
        supR3HardenedFatal("supR3HardenedPathProgram: _dyld_get_image_name(0) failed, cchImageName=%d\n", cchImageName);
    memcpy(g_szSupLibHardenedExePath, pszImageName, cchImageName + 1);

#elif defined(RT_OS_WINDOWS)
    HMODULE hExe = GetModuleHandle(NULL);
    if (!GetModuleFileName(hExe, &g_szSupLibHardenedExePath[0], sizeof(g_szSupLibHardenedExePath)))
        supR3HardenedFatal("supR3HardenedPathProgram: GetModuleFileName failed, rc=%d\n", GetLastError());
#else
# error needs porting.
#endif

    /*
     * Strip off the filename part (RTPathStripFilename()).
     */
    strcpy(g_szSupLibHardenedDirPath, g_szSupLibHardenedExePath);
    suplibHardenedPathStripFilename(g_szSupLibHardenedDirPath);
}


#ifdef RT_OS_LINUX
/**
 * Checks if we can read /proc/self/exe.
 *
 * This is used on linux to see if we have to call init
 * with program path or not.
 *
 * @returns true / false.
 */
static bool supR3HardenedMainIsProcSelfExeAccssible(void)
{
    char szPath[RTPATH_MAX];
    int cchLink = readlink("/proc/self/exe", szPath, sizeof(szPath));
    return cchLink != -1;
}
#endif /* RT_OS_LINUX */



/**
 * @copydoc RTPathProgram
 */
DECLHIDDEN(int) supR3HardenedPathProgram(char *pszPath, size_t cchPath)
{
    /*
     * Lazy init (probably not required).
     */
    if (!g_szSupLibHardenedDirPath[0])
        supR3HardenedGetFullExePath();

    /*
     * Calc the length and check if there is space before copying.
     */
    unsigned cch = strlen(g_szSupLibHardenedDirPath) + 1;
    if (cch <= cchPath)
    {
        memcpy(pszPath, g_szSupLibHardenedDirPath, cch + 1);
        return VINF_SUCCESS;
    }

    supR3HardenedFatal("supR3HardenedPathProgram: Buffer too small (%u < %u)\n", cchPath, cch);
    return VERR_BUFFER_OVERFLOW;
}



DECLHIDDEN(void) supR3HardenedFatalV(const char *pszFormat, va_list va)
{
    fprintf(stderr, "%s: ", g_pszSupLibHardenedProgName);
    vfprintf(stderr, pszFormat, va);
    for (;;)
#ifdef _MSC_VER
        exit(1);
#else
        _Exit(1);
#endif
}


DECLHIDDEN(void) supR3HardenedFatal(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedFatalV(pszFormat, va);
    va_end(va);
}


DECLHIDDEN(int) supR3HardenedErrorV(int rc, bool fFatal, const char *pszFormat, va_list va)
{
    if (fFatal)
        supR3HardenedFatalV(pszFormat, va);

    fprintf(stderr, "%s: ", g_pszSupLibHardenedProgName);
    vfprintf(stderr, pszFormat, va);
    return rc;
}


DECLHIDDEN(int) supR3HardenedError(int rc, bool fFatal, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedErrorV(rc, fFatal, pszFormat, va);
    va_end(va);
    return rc;
}


/**
 * Wrapper around snprintf which will throw a fatal error on buffer overflow.
 *
 * @returns Number of chars in the result string.
 * @param   pszDst          The destination buffer.
 * @param   cchDst          The size of the buffer.
 * @param   pszFormat       The format string.
 * @param   ...             Format arguments.
 */
static size_t supR3HardenedStrPrintf(char *pszDst, size_t cchDst, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
#ifdef _MSC_VER
    int cch = _vsnprintf(pszDst, cchDst, pszFormat, va);
#else
    int cch = vsnprintf(pszDst, cchDst, pszFormat, va);
#endif
    va_end(va);
    if ((unsigned)cch >= cchDst || cch < 0)
        supR3HardenedFatal("supR3HardenedStrPrintf: buffer overflow, %d >= %lu\n", cch, (long)cchDst);
    return cch;
}


/**
 * Attempts to open /dev/vboxdrv (or equvivalent).
 *
 * @remarks This function will not return on failure.
 */
static void supR3HardenedMainOpenDevice(void)
{
    int rc = suplibOsInit(&g_SupPreInitData.Data, false);
    if (RT_SUCCESS(rc))
        return;

    switch (rc)
    {
        case VERR_VM_DRIVER_NOT_INSTALLED:
            supR3HardenedFatal("supR3HardenedMainOpenDevice: VERR_VM_DRIVER_NOT_INSTALLED\n");
        case VERR_VM_DRIVER_NOT_ACCESSIBLE:
            supR3HardenedFatal("supR3HardenedMainOpenDevice: VERR_VM_DRIVER_NOT_ACCESSIBLE\n");
        case VERR_VM_DRIVER_LOAD_ERROR:
            supR3HardenedFatal("supR3HardenedMainOpenDevice: VERR_VM_DRIVER_LOAD_ERROR\n");
        case VERR_VM_DRIVER_OPEN_ERROR:
            supR3HardenedFatal("supR3HardenedMainOpenDevice: VERR_VM_DRIVER_OPEN_ERROR\n");
        case VERR_VM_DRIVER_VERSION_MISMATCH:
            supR3HardenedFatal("supR3HardenedMainOpenDevice: VERR_VM_DRIVER_VERSION_MISMATCH\n");
        case VERR_ACCESS_DENIED:
            supR3HardenedFatal("supR3HardenedMainOpenDevice: VERR_ACCESS_DENIED\n");
        default:
            supR3HardenedFatal("supR3HardenedMainOpenDevice: rc=%d\n", rc);
    }
}


/**
 * Loads the VBoxRT DLL/SO/DYLIB, hands it the open driver,
 * and calls RTR3Init.
 *
 * @param   fFlags      The SUPR3HardenedMain fFlags argument, passed to supR3PreInit.
 *
 * @remarks VBoxRT contains both IPRT and SUPR3.
 * @remarks This function will not return on failure.
 */
static void supR3HardenedMainInitRuntime(uint32_t fFlags)
{
    /*
     * Construct the name.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedPathSharedLibs(szPath, sizeof(szPath) - sizeof("/VBoxRT" SUPLIB_DLL_SUFF));
    strcat(szPath, "/VBoxRT" SUPLIB_DLL_SUFF);

    /*
     * Open it and resolve the symbols.
     */
#if defined(RT_OS_WINDOWS)
    /** @todo consider using LOAD_WITH_ALTERED_SEARCH_PATH here! */
    HMODULE hMod = LoadLibraryEx(szPath, NULL /*hFile*/, 0 /* dwFlags */);
    if (!hMod)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: LoadLibraryEx(\"%s\",,) failed, rc=%d\n",
                            szPath, GetLastError());
    PFNRTR3INITEX pfnRTInitEx = (PFNRTR3INITEX)GetProcAddress(hMod, SUP_HARDENED_SYM("RTR3InitEx"));
    if (!pfnRTInitEx)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"RTR3InitEx\" not found in \"%s\" (rc=%d)\n",
                            szPath, GetLastError());

    PFNSUPR3PREINIT pfnSUPPreInit = (PFNSUPR3PREINIT)GetProcAddress(hMod, SUP_HARDENED_SYM("supR3PreInit"));
    if (!pfnSUPPreInit)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"supR3PreInit\" not found in \"%s\" (rc=%d)\n",
                            szPath, GetLastError());

#else
    /* the dlopen crowd */
    void *pvMod = dlopen(szPath, RTLD_NOW | RTLD_GLOBAL);
    if (!pvMod)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: dlopen(\"%s\",) failed: %s\n",
                            szPath, dlerror());
    PFNRTR3INITEX pfnRTInitEx = (PFNRTR3INITEX)(uintptr_t)dlsym(pvMod, SUP_HARDENED_SYM("RTR3InitEx"));
    if (!pfnRTInitEx)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"RTR3InitEx\" not found in \"%s\"!\ndlerror: %s\n",
                            szPath, dlerror());
    PFNSUPR3PREINIT pfnSUPPreInit = (PFNSUPR3PREINIT)(uintptr_t)dlsym(pvMod, SUP_HARDENED_SYM("supR3PreInit"));
    if (!pfnSUPPreInit)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"supR3PreInit\" not found in \"%s\"!\ndlerror: %s\n",
                            szPath, dlerror());
#endif

    /*
     * Make the calls.
     */
    supR3HardenedGetPreInitData(&g_SupPreInitData);
    int rc = pfnSUPPreInit(&g_SupPreInitData, fFlags);
    if (RT_FAILURE(rc))
        supR3HardenedFatal("supR3PreInit: Failed with rc=%d\n", rc);
    const char *pszExePath = NULL;
#ifdef RT_OS_LINUX
    if (!supR3HardenedMainIsProcSelfExeAccssible())
        pszExePath = g_szSupLibHardenedExePath;
#endif
    rc = pfnRTInitEx(0, pszExePath, !(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV));
    if (RT_FAILURE(rc))
        supR3HardenedFatal("RTR3Init: Failed with rc=%d\n", rc);
}


/**
 * Loads the DLL/SO/DYLIB containing the actual program and
 * resolves the TrustedMain symbol.
 *
 * @returns Pointer to the trusted main of the actual program.
 * @param   pszProgName     The program name.
 * @remarks This function will not return on failure.
 */
static PFNTRUSTEDMAIN supR3HardenedMainGetTrustedMain(const char *pszProgName)
{
    /*
     * Construct the name.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedPathAppPrivateArch(szPath, sizeof(szPath) - 10);
    size_t cch = strlen(szPath);
    supR3HardenedStrPrintf(&szPath[cch], sizeof(szPath) - cch, "/%s%s", pszProgName, SUPLIB_DLL_SUFF);

    /*
     * Open it and resolve the symbol.
     */
#if defined(RT_OS_WINDOWS)
    /** @todo consider using LOAD_WITH_ALTERED_SEARCH_PATH here! */
    HMODULE hMod = LoadLibraryEx(szPath, NULL /*hFile*/, 0 /* dwFlags */);
    if (!hMod)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: LoadLibraryEx(\"%s\",,) failed, rc=%d\n",
                            szPath, GetLastError());
    FARPROC pfn = GetProcAddress(hMod, SUP_HARDENED_SYM("TrustedMain"));
    if (!pfn)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"TrustedMain\" not found in \"%s\" (rc=%d)\n",
                            szPath, GetLastError());
    return (PFNTRUSTEDMAIN)pfn;

#else
    /* the dlopen crowd */
    void *pvMod = dlopen(szPath, RTLD_NOW | RTLD_GLOBAL);
    if (!pvMod)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: dlopen(\"%s\",) failed: %s\n",
                            szPath, dlerror());
    void *pvSym = dlsym(pvMod, SUP_HARDENED_SYM("TrustedMain"));
    if (!pvSym)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"TrustedMain\" not found in \"%s\"!\ndlerror: %s\n",
                            szPath, dlerror());
    return (PFNTRUSTEDMAIN)(uintptr_t)pvSym;
#endif
}


/**
 * Secure main.
 *
 * This is used for the set-user-ID-on-execute binaries on unixy systems
 * and when using the open-vboxdrv-via-root-service setup on Windows.
 *
 * This function will perform the integrity checks of the VirtualBox
 * installation, open the support driver, open the root service (later),
 * and load the DLL corresponding to \a pszProgName and execute its main
 * function.
 *
 * @returns Return code appropriate for main().
 *
 * @param   pszProgName     The program name. This will be used to figure out which
 *                          DLL/SO/DYLIB to load and execute.
 * @param   fFlags          Flags.
 * @param   argc            The argument count.
 * @param   argv            The argument vector.
 * @param   envp            The environment vector.
 */
DECLHIDDEN(int) SUPR3HardenedMain(const char *pszProgName, uint32_t fFlags, int argc, char **argv, char **envp)
{
    /*
     * Note! At this point there is no IPRT, so we will have to stick
     * to basic CRT functions that everyone agree upon.
     */
    g_pszSupLibHardenedProgName = pszProgName;
    g_SupPreInitData.u32Magic = SUPPREINITDATA_MAGIC;
    g_SupPreInitData.Data.hDevice = NIL_RTFILE;
    g_SupPreInitData.u32EndMagic = SUPPREINITDATA_MAGIC;

#ifdef SUP_HARDENED_SUID
    /*
     * Check that we're root, if we aren't then the installation is butchered.
     */
    uid_t const uid = getuid();
    gid_t const gid = getgid();
    if (geteuid() != 0 /* root */)
        supR3HardenedFatal("SUPR3HardenedMain: effective uid is not root (euid=%d egid=%d uid=%d gid=%d)\n",
                           geteuid(), getegid(), uid, gid);

# ifdef RT_OS_LINUX
    /*
     * On linux we have to make sure the path is initialized because we
     * *might* not be able to access /proc/self/exe after the seteuid call.
     */
    supR3HardenedGetFullExePath();
# endif
#endif

    /*
     * Validate the installation.
     */
    supR3HardenedVerifyAll(true /* fFatal */, false /* fLeaveFilesOpen */, pszProgName);

    /*
     * Open the vboxdrv device.
     */
    if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV))
        supR3HardenedMainOpenDevice();

    /*
     * Open the root service connection.
     */
    //if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_SVC))
        //supR3HardenedMainOpenService(&g_SupPreInitData, true /* fFatal */);

#ifdef SUP_HARDENED_SUID
    /*
     * Drop any root privileges we might be holding.
     *
     * Try use setre[ug]id since this will clear the save uid/gid and thus
     * leave fewer traces behind that libs like GTK+ may pick up.
     */
    uid_t euid, ruid, suid;
    gid_t egid, rgid, sgid;
# if defined(RT_OS_DARWIN)
    /* The really great thing here is that setreuid isn't available on
       OS X 10.4, libc emulates it. While 10.4 have a sligtly different and
       non-standard setuid implementation compared to 10.5, the following
       works the same way with both version since we're super user (10.5 req).
       The following will set all three variants of the group and user IDs. */
    setgid(gid);
    setuid(uid);
    euid = geteuid();
    ruid = suid = getuid();
    egid = getegid();
    rgid = sgid = getgid();

# elif defined(RT_OS_SOLARIS)
    /* Solaris doesn't have setresuid, but the setreuid interface is BSD
       compatible and will set the saved uid to euid when we pass it a ruid
       that isn't -1 (which we do). */
    setregid(gid, gid);
    setreuid(uid, uid);
    euid = geteuid();
    ruid = suid = getuid();
    egid = getegid();
    rgid = sgid = getgid();

# else
    /* This is the preferred one, full control no questions about semantics.
       PORTME: If this isn't work, try join one of two other gangs above. */
    setresgid(gid, gid, gid);
    setresuid(uid, uid, uid);
    if (getresuid(&ruid, &euid, &suid) != 0)
    {
        euid = geteuid();
        ruid = suid = getuid();
    }
    if (getresgid(&rgid, &egid, &sgid) != 0)
    {
        egid = getegid();
        rgid = sgid = getgid();
    }
# endif

    /* Check that it worked out all right. */
    if (    euid != uid
        ||  ruid != uid
        ||  suid != uid
        ||  egid != gid
        ||  rgid != gid
        ||  sgid != gid)
        supR3HardenedFatal("SUPR3HardenedMain: failed to drop root privileges!"
                           " (euid=%d ruid=%d suid=%d  egid=%d rgid=%d sgid=%d; wanted uid=%d and gid=%d)\n",
                           euid, ruid, suid, egid, rgid, sgid, uid, gid);
#endif

    /*
     * Load the IPRT, hand the SUPLib part the open driver and
     * call RTR3Init.
     */
    supR3HardenedMainInitRuntime(fFlags);

    /*
     * Load the DLL/SO/DYLIB containing the actual program
     * and pass control to it.
     */
    PFNTRUSTEDMAIN pfnTrustedMain = supR3HardenedMainGetTrustedMain(pszProgName);
    return pfnTrustedMain(argc, argv, envp);
}


