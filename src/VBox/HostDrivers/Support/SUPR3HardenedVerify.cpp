/* $Id$ */
/** @file
 * VirtualBox Support Library - Verification of Hardened Installation.
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
# include <stdlib.h>
# include <unistd.h>
# include <sys/fcntl.h>
# include <sys/errno.h>
# include <sys/syslimits.h>

#elif defined(RT_OS_WINDOWS)
# include <Windows.h>
# include <stdio.h>

#else /* UNIXes */
# include <sys/types.h>
# include <stdio.h>
# include <stdlib.h>
# include <dlfcn.h>
# include <fcntl.h>
# include <limits.h>
# include <errno.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <sys/fcntl.h>
# include <stdio.h>
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
*   Global Variables                                                           *
*******************************************************************************/
/**
 * The files that gets verified.
 *
 * @todo This needs reviewing against the linux packages.
 * @todo The excessive use of kSupID_SharedLib needs to be reviewed at some point. For
 *       the time being we're building the linux packages with SharedLib pointing to
 *       AppPrivArch (lazy bird).
 */
static SUPINSTFILE const    g_aSupInstallFiles[] =
{
    /*  type,         dir,                      fOpt,  pszFile                */
    /* ---------------------------------------------------------------------- */
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VMMR0.r0" },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VBoxDDR0.r0" },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VBoxDD2R0.r0" },

    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VMMGC.gc" },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VBoxDDGC.gc" },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VBoxDD2GC.gc" },

    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxRT"  SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxVMM"  SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxREM"  SUPLIB_DLL_SUFF },
#if HC_ARCH_BITS == 32
    {   kSupIFT_Dll,  kSupID_SharedLib,         true, "VBoxREM32"  SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         true, "VBoxREM64"  SUPLIB_DLL_SUFF },
#endif
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxDD"  SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxDD2"  SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxDDU"  SUPLIB_DLL_SUFF },

//#ifdef VBOX_WITH_DEBUGGER_GUI
    {   kSupIFT_Dll,  kSupID_SharedLib,         true, "VBoxDbg"  SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         true, "VBoxDbg3" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_SHARED_CLIPBOARD
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxSharedClipboard" SUPLIB_DLL_SUFF },
//#endif
//#ifdef VBOX_WITH_SHARED_FOLDERS
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxSharedFolders" SUPLIB_DLL_SUFF },
//#endif
//#ifdef VBOX_WITH_GUEST_PROPS
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxGuestPropSvc" SUPLIB_DLL_SUFF },
//#endif
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxSharedCrOpenGL" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxOGLhostcrutil" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxOGLhosterrorspu" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxOGLrenderspu" SUPLIB_DLL_SUFF },

    {   kSupIFT_Exe,  kSupID_AppBin,            true, "VBoxManage" SUPLIB_EXE_SUFF },

#ifdef VBOX_WITH_MAIN
    {   kSupIFT_Exe,  kSupID_AppBin,            false, "VBoxSVC" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxSettings" SUPLIB_DLL_SUFF },
 #ifdef RT_OS_WINDOWS
    {   kSupIFT_Dll,  kSupID_AppPrivArchComp,   false, "VBoxC" SUPLIB_DLL_SUFF },
 #else
    {   kSupIFT_Exe,  kSupID_AppPrivArch,       false, "VBoxXPCOMIPCD" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxXPCOM" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArchComp,   false, "VBoxXPCOMIPCC" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArchComp,   false, "VBoxC" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArchComp,   false, "VBoxSVCM" SUPLIB_DLL_SUFF },
    {   kSupIFT_Data, kSupID_AppPrivArchComp,   false, "VBoxXPCOMBase.xpt" },
 #endif
#endif

//#ifdef VBOX_WITH_VRDP
    {   kSupIFT_Dll,  kSupID_SharedLib,         true, "VRDPAuth" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         true, "VBoxVRDP" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_HEADLESS
    {   kSupIFT_Exe,  kSupID_AppBin,            true, "VBoxHeadless" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxHeadless" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxFFmpegFB" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_QTGUI
    {   kSupIFT_Exe,  kSupID_AppBin,            true, "VirtualBox" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VirtualBox" SUPLIB_DLL_SUFF },
# if !defined(RT_OS_DARWIN) && !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
    {   kSupIFT_Dll,  kSupID_SharedLib,         true, "VBoxKeyboard" SUPLIB_DLL_SUFF },
# endif
//#endif

//#ifdef VBOX_WITH_VBOXSDL
    {   kSupIFT_Exe,  kSupID_AppBin,            true, "VBoxSDL" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxSDL" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_VBOXBFE
    {   kSupIFT_Exe,  kSupID_AppBin,            true, "VBoxBFE" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxBFE" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_WEBSERVICES
    {   kSupIFT_Exe,  kSupID_AppBin,            true, "vboxwebsrv" SUPLIB_EXE_SUFF },
//#endif

#ifdef RT_OS_LINUX
    {   kSupIFT_Exe,  kSupID_AppBin,            true, "VBoxTunctl" SUPLIB_EXE_SUFF },
#endif

//#ifdef VBOX_WITH_NETFLT
    {   kSupIFT_Exe,  kSupID_AppBin,            true, "VBoxNetDHCP" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       true, "VBoxNetDHCP" SUPLIB_DLL_SUFF },
//#endif
};


/** Array parallel to g_aSupInstallFiles containing per-file status info. */
static SUPVERIFIEDFILE  g_aSupVerifiedFiles[RT_ELEMENTS(g_aSupInstallFiles)];

/** Array index by install directory specifier containing info about verified directories. */
static SUPVERIFIEDDIR   g_aSupVerifiedDirs[kSupID_End];


/**
 * Assembles the path to a dirtory.
 *
 * @returns VINF_SUCCESS on success, some error code on failure (fFatal
 *          decides whether it returns or not).
 *
 * @param   enmDir              The directory.
 * @param   pszDst              Where to assemble the path.
 * @param   cchDst              The size of the buffer.
 * @param   fFatal              Whether failures should be treated as fatal (true) or not (false).
 */
static int supR3HardenedMakePath(SUPINSTDIR enmDir, char *pszDst, size_t cchDst, bool fFatal)
{
    int rc;
    switch (enmDir)
    {
        case kSupID_AppBin: /** @todo fix this AppBin crap (uncertain wtf some binaries actually are installed). */
        case kSupID_Bin:
            rc = supR3HardenedPathProgram(pszDst, cchDst);
            break;
        case kSupID_SharedLib:
            rc = supR3HardenedPathSharedLibs(pszDst, cchDst);
            break;
        case kSupID_AppPrivArch:
            rc = supR3HardenedPathAppPrivateArch(pszDst, cchDst);
            break;
        case kSupID_AppPrivArchComp:
            rc = supR3HardenedPathAppPrivateArch(pszDst, cchDst);
            if (RT_SUCCESS(rc))
            {
                size_t off = strlen(pszDst);
                if (cchDst - off >= sizeof("/components"))
                    memcpy(&pszDst[off], "/components", sizeof("/components"));
                else
                    rc = VERR_BUFFER_OVERFLOW;
            }
            break;
        case kSupID_AppPrivNoArch:
            rc = supR3HardenedPathAppPrivateNoArch(pszDst, cchDst);
            break;
        default:
            return supR3HardenedError(VERR_INTERNAL_ERROR, fFatal,
                                      "supR3HardenedMakePath: enmDir=%d\n", enmDir);
    }
    if (RT_FAILURE(rc))
        supR3HardenedError(rc, fFatal,
                           "supR3HardenedMakePath: enmDir=%d rc=%d\n", enmDir, rc);
    return rc;
}



/**
 * Assembles the path to a file table entry, with or without the actual filename.
 *
 * @returns VINF_SUCCESS on success, some error code on failure (fFatal
 *          decides whether it returns or not).
 *
 * @param   pFile               The file table entry.
 * @param   pszDst              Where to assemble the path.
 * @param   cchDst              The size of the buffer.
 * @param   fWithFilename       If set, the filename is included, otherwise it is omitted (no trailing slash).
 * @param   fFatal              Whether failures should be treated as fatal (true) or not (false).
 */
static int supR3HardenedMakeFilePath(PCSUPINSTFILE pFile, char *pszDst, size_t cchDst, bool fWithFilename, bool fFatal)
{
    /*
     * Combine supR3HardenedMakePath and the filename.
     */
    int rc = supR3HardenedMakePath(pFile->enmDir, pszDst, cchDst, fFatal);
    if (RT_SUCCESS(rc))
    {
        size_t cchFile = strlen(pFile->pszFile);
        size_t off = strlen(pszDst);
        if (cchDst - off >= cchFile + 2)
        {
            pszDst[off++] = '/';
            memcpy(&pszDst[off], pFile->pszFile, cchFile + 1);
        }
        else
            rc = supR3HardenedError(VERR_BUFFER_OVERFLOW, fFatal,
                                    "supR3HardenedMakeFilePath: pszFile=%s off=%lu\n",
                                    pFile->pszFile, (long)off);
    }
    return rc;
}


/**
 * Verifies a directory.
 *
 * @returns VINF_SUCCESS on success. On failure, an error code is returned if
 *          fFatal is clear and if it's set the function wont return.
 * @param   enmDir              The directory specifier.
 * @param   fFatal              Whether validation failures should be treated as
 *                              fatal (true) or not (false).
 */
DECLHIDDEN(int) supR3HardenedVerifyDir(SUPINSTDIR enmDir, bool fFatal)
{
    /*
     * Validate the index just to be on the safe side...
     */
    if (enmDir <= kSupID_Invalid || enmDir >= kSupID_End)
        return supR3HardenedError(VERR_INTERNAL_ERROR, fFatal,
                                  "supR3HardenedVerifyDir: enmDir=%d\n", enmDir);

    /*
     * Already validated?
     */
    if (g_aSupVerifiedDirs[enmDir].fValidated)
        return VINF_SUCCESS;  /** @todo revalidate? */

    /* initialize the entry. */
    if (g_aSupVerifiedDirs[enmDir].hDir != 0)
        supR3HardenedError(VERR_INTERNAL_ERROR, fFatal,
                           "supR3HardenedVerifyDir: hDir=%p enmDir=%d\n",
                           (void *)g_aSupVerifiedDirs[enmDir].hDir, enmDir);
    g_aSupVerifiedDirs[enmDir].hDir = -1;
    g_aSupVerifiedDirs[enmDir].fValidated = false;

    /*
     * Make the path and open the directory.
     */
    char szPath[RTPATH_MAX];
    int rc = supR3HardenedMakePath(enmDir, szPath, sizeof(szPath), fFatal);
    if (RT_SUCCESS(rc))
    {
#if defined(RT_OS_WINDOWS)
        HANDLE hDir = CreateFile(szPath,
                                 GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE,
                                 NULL,
                                 OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
                                 NULL);
        if (hDir != INVALID_HANDLE_VALUE)
        {
            /** @todo check the type */
            /* That's all on windows, for now at least... */
            g_aSupVerifiedDirs[enmDir].hDir = (intptr_t)hDir;
            g_aSupVerifiedDirs[enmDir].fValidated = true;
        }
        else
        {
            int err = GetLastError();
            rc = supR3HardenedError(VERR_PATH_NOT_FOUND, fFatal,
                                    "supR3HardenedVerifyDir: Failed to open \"%s\": err=%d\n",
                                    szPath, err);
        }
#else /* UNIXY */
        int fd = open(szPath, O_RDONLY, 0);
        if (fd >= 0)
        {
            /*
             * On unixy systems we'll make sure the directory is owned by root
             * and not writable by the group and user.
             */
            struct stat st;
            if (!fstat(fd, &st))
            {

                if (    st.st_uid == 0
                    &&  !(st.st_mode & (S_IWGRP | S_IWOTH))
                    &&  S_ISDIR(st.st_mode))
                {
                    g_aSupVerifiedDirs[enmDir].hDir = fd;
                    g_aSupVerifiedDirs[enmDir].fValidated = true;
                }
                else
                {
                    if (!S_ISDIR(st.st_mode))
                        rc = supR3HardenedError(VERR_NOT_A_DIRECTORY, fFatal,
                                                "supR3HardenedVerifyDir: \"%s\" is not a directory\n",
                                                szPath, (long)st.st_uid);
                    else if (st.st_uid)
                        rc = supR3HardenedError(VERR_ACCESS_DENIED, fFatal,
                                                "supR3HardenedVerifyDir: Cannot trust the directory \"%s\": not owned by root (st_uid=%ld)\n",
                                                szPath, (long)st.st_uid);
                    else
                        rc = supR3HardenedError(VERR_ACCESS_DENIED, fFatal,
                                                "supR3HardenedVerifyDir: Cannot trust the directory \"%s\": group and/or other writable (st_mode=0%lo)\n",
                                                szPath, (long)st.st_mode);
                    close(fd);
                }
            }
            else
            {
                int err = errno;
                rc = supR3HardenedError(VERR_ACCESS_DENIED, fFatal,
                                        "supR3HardenedVerifyDir: Failed to fstat \"%s\": %s (%d)\n",
                                        szPath, strerror(err), err);
                close(fd);
            }
        }
        else
        {
            int err = errno;
            rc = supR3HardenedError(VERR_PATH_NOT_FOUND, fFatal,
                                    "supR3HardenedVerifyDir: Failed to open \"%s\": %s (%d)\n",
                                    szPath, strerror(err), err);
        }
#endif /* UNIXY */
    }

    return rc;
}


/**
 * Verifies a file entry.
 *
 * @returns VINF_SUCCESS on success. On failure, an error code is returned if
 *          fFatal is clear and if it's set the function wont return.
 *
 * @param   iFile               The file table index of the file to be verified.
 * @param   fFatal              Whether validation failures should be treated as
 *                              fatal (true) or not (false).
 * @param   fLeaveFileOpen      Whether the file should be left open.
 */
static int supR3HardenedVerifyFileInternal(int iFile, bool fFatal, bool fLeaveFileOpen)
{
    PCSUPINSTFILE pFile = &g_aSupInstallFiles[iFile];
    PSUPVERIFIEDFILE pVerified = &g_aSupVerifiedFiles[iFile];

    /*
     * Already done?
     */
    if (pVerified->fValidated)
        return VINF_SUCCESS; /** @todo revalidate? */


    /* initialize the entry. */
    if (pVerified->hFile != 0)
        supR3HardenedError(VERR_INTERNAL_ERROR, fFatal,
                           "supR3HardenedVerifyFileInternal: hFile=%p (%s)\n",
                           (void *)pVerified->hFile, pFile->pszFile);
    pVerified->hFile = -1;
    pVerified->fValidated = false;

    /*
     * Verify the directory then proceed to open it.
     * (This'll make sure the directory is opened and that we can (later)
     *  use openat if we wish.)
     */
    int rc = supR3HardenedVerifyDir(pFile->enmDir, fFatal);
    if (RT_SUCCESS(rc))
    {
        char szPath[RTPATH_MAX];
        int rc = supR3HardenedMakeFilePath(pFile, szPath, sizeof(szPath), true, fFatal);
        if (RT_SUCCESS(rc))
        {
#if defined(RT_OS_WINDOWS)
            HANDLE hFile = CreateFile(szPath,
                                      GENERIC_READ,
                                      FILE_SHARE_READ,
                                      NULL,
                                      OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL,
                                      NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                /** @todo Check the type, and verify the signature (separate function so we can skip it). */
                {
                    /* it's valid. */
                    if (fLeaveFileOpen)
                        pVerified->hFile = (intptr_t)hFile;
                    else
                        CloseHandle(hFile);
                    pVerified->fValidated = true;
                }
            }
            else
            {
                int err = GetLastError();
                if (!pFile->fOptional || err != ERROR_FILE_NOT_FOUND)
                    rc = supR3HardenedError(VERR_PATH_NOT_FOUND, fFatal,
                                            "supR3HardenedVerifyFileInternal: Failed to open \"%s\": err=%d\n",
                                            szPath, err);
            }
#else /* UNIXY */
            int fd = open(szPath, O_RDONLY, 0);
            if (fd >= 0)
            {
                /*
                 * On unixy systems we'll make sure the directory is owned by root
                 * and not writable by the group and user.
                 */
                struct stat st;
                if (!fstat(fd, &st))
                {
                    if (    st.st_uid == 0
                        &&  !(st.st_mode & (S_IWGRP | S_IWOTH))
                        &&  S_ISREG(st.st_mode))
                    {
                        /* it's valid. */
                        if (fLeaveFileOpen)
                            pVerified->hFile = fd;
                        else
                            close(fd);
                        pVerified->fValidated = true;
                    }
                    else
                    {
                        if (!S_ISREG(st.st_mode))
                            rc = supR3HardenedError(VERR_IS_A_DIRECTORY, fFatal,
                                                    "supR3HardenedVerifyFileInternal: \"%s\" is not a regular file\n",
                                                    szPath, (long)st.st_uid);
                        else if (st.st_uid)
                            rc = supR3HardenedError(VERR_ACCESS_DENIED, fFatal,
                                                    "supR3HardenedVerifyFileInternal: Cannot trust the file \"%s\": not owned by root (st_uid=%ld)\n",
                                                    szPath, (long)st.st_uid);
                        else
                            rc = supR3HardenedError(VERR_ACCESS_DENIED, fFatal,
                                                    "supR3HardenedVerifyFileInternal: Cannot trust the file \"%s\": group and/or other writable (st_mode=0%lo)\n",
                                                    szPath, (long)st.st_mode);
                        close(fd);
                    }
                }
                else
                {
                    int err = errno;
                    rc = supR3HardenedError(VERR_ACCESS_DENIED, fFatal,
                                            "supR3HardenedVerifyFileInternal: Failed to fstat \"%s\": %s (%d)\n",
                                            szPath, strerror(err), err);
                    close(fd);
                }
            }
            else
            {
                int err = errno;
                if (!pFile->fOptional || err != ENOENT)
                    rc = supR3HardenedError(VERR_PATH_NOT_FOUND, fFatal,
                                            "supR3HardenedVerifyFileInternal: Failed to open \"%s\": %s (%d)\n",
                                            szPath, strerror(err), err);
            }
#endif /* UNIXY */
        }
    }

    return rc;
}


/**
 * Verifies that the specified table entry matches the given filename.
 *
 * @returns VINF_SUCCESS if matching. On mismatch fFatal indicates whether an
 *          error is returned or we terminate the application.
 *
 * @param   iFile               The file table index.
 * @param   pszFilename         The filename.
 * @param   fFatal              Whether validation failures should be treated as
 *                              fatal (true) or not (false).
 */
static int supR3HardenedVerifySameFile(int iFile, const char *pszFilename, bool fFatal)
{
    PCSUPINSTFILE pFile = &g_aSupInstallFiles[iFile];

    /*
     * Construct the full path for the file table entry
     * and compare it with the specified file.
     */
    char szName[RTPATH_MAX];
    int rc = supR3HardenedMakeFilePath(pFile, szName, sizeof(szName), true /*fWithFilename*/, fFatal);
    if (RT_FAILURE(rc))
        return rc;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (stricmp(szName, pszFilename))
#else
    if (strcmp(szName, pszFilename))
#endif
    {
        /*
         * Normalize the two paths and compare again.
         */
        rc = VERR_NOT_SAME_DEVICE;
#if defined(RT_OS_WINDOWS)
        LPSTR pszIgnored;
        char szName2[RTPATH_MAX];
        if (    GetFullPathName(szName, RT_ELEMENTS(szName2), &szName2[0], &pszIgnored)
            &&  GetFullPathName(pszFilename, RT_ELEMENTS(szName), &szName[0], &pszIgnored))
            if (!stricmp(szName2, szName))
                rc = VINF_SUCCESS;
#else
        AssertCompile(RTPATH_MAX >= PATH_MAX);
        char szName2[RTPATH_MAX];
        if (    realpath(szName, szName2) != NULL
            &&  realpath(pszFilename, szName) != NULL)
            if (!strcmp(szName2, szName))
                rc = VINF_SUCCESS;
#endif

        if (RT_FAILURE(rc))
        {
            supR3HardenedMakeFilePath(pFile, szName, sizeof(szName), true /*fWithFilename*/, fFatal);
            return supR3HardenedError(rc, fFatal,
                                      "supR3HardenedVerifySameFile: \"%s\" isn't the same as \"%s\"\n",
                                      pszFilename, szName);
        }
    }

    /*
     * Check more stuff like the stat info if it's an already open file?
     */



    return VINF_SUCCESS;
}


/**
 * Verifies a file.
 *
 * @returns VINF_SUCCESS on success.
 *          VERR_NOT_FOUND if the file isn't in the table, this isn't ever a fatal error.
 *          On verfication failure, an error code will be returned when fFatal is clear,
 *          otherwise the program will be termindated.
 *
 * @param   pszFilename         The filename.
 * @param   fFatal              Whether validation failures should be treated as
 *                              fatal (true) or not (false).
 */
DECLHIDDEN(int) supR3HardenedVerifyFile(const char *pszFilename, bool fFatal)
{
    /*
     * Lookup the file and check if it's the same file.
     */
    const char *pszName = supR3HardenedPathFilename(pszFilename);
    for (unsigned iFile = 0; iFile < RT_ELEMENTS(g_aSupInstallFiles); iFile++)
        if (!strcmp(pszName, g_aSupInstallFiles[iFile].pszFile))
        {
            int rc = supR3HardenedVerifySameFile(iFile, pszFilename, fFatal);
            if (RT_SUCCESS(rc))
                rc = supR3HardenedVerifyFileInternal(iFile, fFatal, false /* fLeaveFileOpen */);
            return rc;
        }

    return VERR_NOT_FOUND;
}


/**
 * Verifies a program, worker for supR3HardenedVerifyAll.
 *
 * @returns See supR3HardenedVerifyAll.
 * @param   pszProgName         See supR3HardenedVerifyAll.
 * @param   fFatal              See supR3HardenedVerifyAll.
 */
static int supR3HardenedVerifyProgram(const char *pszProgName, bool fFatal)
{
    /*
     * Search the table looking for the executable and the DLL/DYLIB/SO.
     */
    int             rc = VINF_SUCCESS;
    bool            fExe = false;
    bool            fDll = false;
    size_t const    cchProgName = strlen(pszProgName);
    for (unsigned iFile = 0; iFile < RT_ELEMENTS(g_aSupInstallFiles); iFile++)
        if (!strncmp(pszProgName, g_aSupInstallFiles[iFile].pszFile, cchProgName))
        {
            if (    g_aSupInstallFiles[iFile].enmType == kSupIFT_Dll
                &&  !strcmp(&g_aSupInstallFiles[iFile].pszFile[cchProgName], SUPLIB_DLL_SUFF))
            {
                /* This only has to be found (once). */
                if (fDll)
                    rc = supR3HardenedError(VERR_INTERNAL_ERROR, fFatal,
                                            "supR3HardenedVerifyProgram: duplicate DLL entry for \"%s\"\n", pszProgName);
                fDll = true;
            }
            else if (   g_aSupInstallFiles[iFile].enmType == kSupIFT_Exe
                     && !strcmp(&g_aSupInstallFiles[iFile].pszFile[cchProgName], SUPLIB_EXE_SUFF))
            {
                /* Here we'll have to check that the specific program is the same as the entry. */
                if (fExe)
                    rc = supR3HardenedError(VERR_INTERNAL_ERROR, fFatal,
                                            "supR3HardenedVerifyProgram: duplicate EXE entry for \"%s\"\n", pszProgName);
                fExe = true;

                char szFilename[RTPATH_MAX];
                int rc2 = supR3HardenedPathProgram(szFilename, sizeof(szFilename) - cchProgName - sizeof(SUPLIB_EXE_SUFF));
                if (RT_SUCCESS(rc2))
                {
                    strcat(szFilename, "/");
                    strcat(szFilename, g_aSupInstallFiles[iFile].pszFile);
                    supR3HardenedVerifySameFile(iFile, szFilename, fFatal);
                }
                else
                    rc = supR3HardenedError(rc2, fFatal,
                                            "supR3HardenedVerifyProgram: failed to query program path: rc=%d\n", rc2);
            }
        }

    /*
     * Check the findings.
     */
    if (!fDll && !fExe)
        rc = supR3HardenedError(VERR_NOT_FOUND, fFatal,
                                "supR3HardenedVerifyProgram: Couldn't find the program \"%s\"\n", pszProgName);
    else if (!fExe)
        rc = supR3HardenedError(VERR_NOT_FOUND, fFatal,
                                "supR3HardenedVerifyProgram: Couldn't find the EXE entry for \"%s\"\n", pszProgName);
    else if (!fDll)
        rc = supR3HardenedError(VERR_NOT_FOUND, fFatal,
                                "supR3HardenedVerifyProgram: Couldn't find the DLL entry for \"%s\"\n", pszProgName);
    return rc;
}



/**
 * Verifies all the known files.
 *
 * @returns VINF_SUCCESS on success.
 *          On verfication failure, an error code will be returned when fFatal is clear,
 *          otherwise the program will be termindated.
 *
 * @param   fFatal              Whether validation failures should be treated as
 *                              fatal (true) or not (false).
 * @param   fLeaveFilesOpen     If set, all the verfied files are left open.
 * @param   pszProgName         Optional program name. This is used by SUPR3HardenedMain
 *                              to verify that both the executable and corresponding
 *                              DLL/DYLIB/SO are valid.
 */
DECLHIDDEN(int) supR3HardenedVerifyAll(bool fFatal, bool fLeaveFilesOpen, const char *pszProgName)
{
    /*
     * The verify all the files.
     */
    int rc = VINF_SUCCESS;
    for (unsigned iFile = 0; iFile < RT_ELEMENTS(g_aSupInstallFiles); iFile++)
    {
        int rc2 = supR3HardenedVerifyFileInternal(iFile, fFatal, fLeaveFilesOpen);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }

    /*
     * Verify the program name if specified, that is to say, just check that
     * it's in the table (=> we've already verified it).
     */
    if (pszProgName)
    {
        int rc2 = supR3HardenedVerifyProgram(pszProgName, fFatal);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc2 = rc;
    }

    return rc;
}


/**
 * Gets the pre-init data for the hand-over to the other version
 * of this code.
 *
 * The reason why we pass this information on is that it contains
 * open directories and files. Later it may include even more info
 * (int the verified arrays mostly).
 *
 * The receiver is supR3HardenedRecvPreInitData.
 *
 * @param   pPreInitData    Where to store it.
 */
DECLHIDDEN(void) supR3HardenedGetPreInitData(PSUPPREINITDATA pPreInitData)
{
    pPreInitData->cInstallFiles = RT_ELEMENTS(g_aSupInstallFiles);
    pPreInitData->paInstallFiles = &g_aSupInstallFiles[0];
    pPreInitData->paVerifiedFiles = &g_aSupVerifiedFiles[0];

    pPreInitData->cVerifiedDirs = RT_ELEMENTS(g_aSupVerifiedDirs);
    pPreInitData->paVerifiedDirs = &g_aSupVerifiedDirs[0];
}


/**
 * Receives the pre-init data from the static executable stub.
 *
 * @returns VBox status code. Will not bitch on failure since the
 *          runtime isn't ready for it, so that is left to the exe stub.
 *
 * @param   pPreInitData    The hand-over data.
 */
DECLHIDDEN(int) supR3HardenedRecvPreInitData(PCSUPPREINITDATA pPreInitData)
{
    /*
     * Compare the array lengths and the contents of g_aSupInstallFiles.
     */
    if (    pPreInitData->cInstallFiles != RT_ELEMENTS(g_aSupInstallFiles)
        ||  pPreInitData->cVerifiedDirs != RT_ELEMENTS(g_aSupVerifiedDirs))
        return VERR_VERSION_MISMATCH;
    SUPINSTFILE const *paInstallFiles = pPreInitData->paInstallFiles;
    for (unsigned iFile = 0; iFile < RT_ELEMENTS(g_aSupInstallFiles); iFile++)
        if (    g_aSupInstallFiles[iFile].enmDir    != paInstallFiles[iFile].enmDir
            ||  g_aSupInstallFiles[iFile].enmType   != paInstallFiles[iFile].enmType
            ||  g_aSupInstallFiles[iFile].fOptional != paInstallFiles[iFile].fOptional
            ||  strcmp(g_aSupInstallFiles[iFile].pszFile, paInstallFiles[iFile].pszFile))
            return VERR_VERSION_MISMATCH;

    /*
     * Check that we're not called out of order.
     * If dynamic linking it screwed up, we may end up here.
     */
    if (    ASMMemIsAll8(&g_aSupVerifiedFiles[0], sizeof(g_aSupVerifiedFiles), 0) != NULL
        ||  ASMMemIsAll8(&g_aSupVerifiedDirs[0], sizeof(g_aSupVerifiedDirs), 0) != NULL)
        return VERR_WRONG_ORDER;

    /*
     * Copy the verification data over.
     */
    memcpy(&g_aSupVerifiedFiles[0], pPreInitData->paVerifiedFiles, sizeof(g_aSupVerifiedFiles));
    memcpy(&g_aSupVerifiedDirs[0], pPreInitData->paVerifiedDirs, sizeof(g_aSupVerifiedDirs));
    return VINF_SUCCESS;
}
