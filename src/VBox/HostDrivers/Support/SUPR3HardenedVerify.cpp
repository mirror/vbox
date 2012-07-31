/* $Id$ */
/** @file
 * VirtualBox Support Library - Verification of Hardened Installation.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
# include <dirent.h>
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
#include <iprt/asm.h>
#include <iprt/ctype.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include "SUPLibInternal.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The max path length acceptable for a trusted path. */
#define SUPR3HARDENED_MAX_PATH      260U

#ifdef RT_OS_SOLARIS
# define dirfd(d) ((d)->d_fd)
#endif


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
    /*  type,         dir,                       fOpt, "pszFile"              */
    /* ---------------------------------------------------------------------- */
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VMMR0.r0" },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VBoxDDR0.r0" },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VBoxDD2R0.r0" },

#ifdef VBOX_WITH_RAW_MODE
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VMMGC.gc" },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VBoxDDGC.gc" },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,       false, "VBoxDD2GC.gc" },
#endif

    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxRT" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxVMM" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxREM" SUPLIB_DLL_SUFF },
#if HC_ARCH_BITS == 32
    {   kSupIFT_Dll,  kSupID_SharedLib,          true, "VBoxREM32" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,          true, "VBoxREM64" SUPLIB_DLL_SUFF },
#endif
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxDD" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxDD2" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,         false, "VBoxDDU" SUPLIB_DLL_SUFF },

//#ifdef VBOX_WITH_DEBUGGER_GUI
    {   kSupIFT_Dll,  kSupID_SharedLib,          true, "VBoxDbg" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,          true, "VBoxDbg3" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_SHARED_CLIPBOARD
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxSharedClipboard" SUPLIB_DLL_SUFF },
//#endif
//#ifdef VBOX_WITH_SHARED_FOLDERS
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxSharedFolders" SUPLIB_DLL_SUFF },
//#endif
//#ifdef VBOX_WITH_DRAG_AND_DROP
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxDragAndDropSvc" SUPLIB_DLL_SUFF },
//#endif
//#ifdef VBOX_WITH_GUEST_PROPS
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxGuestPropSvc" SUPLIB_DLL_SUFF },
//#endif
//#ifdef VBOX_WITH_GUEST_CONTROL
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxGuestControlSvc" SUPLIB_DLL_SUFF },
//#endif
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxSharedCrOpenGL" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxOGLhostcrutil" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxOGLhosterrorspu" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxOGLrenderspu" SUPLIB_DLL_SUFF },

    {   kSupIFT_Exe,  kSupID_AppBin,             true, "VBoxManage" SUPLIB_EXE_SUFF },

#ifdef VBOX_WITH_MAIN
    {   kSupIFT_Exe,  kSupID_AppBin,            false, "VBoxSVC" SUPLIB_EXE_SUFF },
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

    {   kSupIFT_Dll,  kSupID_SharedLib,          true, "VRDPAuth" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,          true, "VBoxAuth" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_SharedLib,          true, "VBoxVRDP" SUPLIB_DLL_SUFF },

//#ifdef VBOX_WITH_HEADLESS
    {   kSupIFT_Exe,  kSupID_AppBin,             true, "VBoxHeadless" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxHeadless" SUPLIB_DLL_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxVideoRecFB" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_QTGUI
    {   kSupIFT_Exe,  kSupID_AppBin,             true, "VirtualBox" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VirtualBox" SUPLIB_DLL_SUFF },
# if !defined(RT_OS_DARWIN) && !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
    {   kSupIFT_Dll,  kSupID_SharedLib,          true, "VBoxKeyboard" SUPLIB_DLL_SUFF },
# endif
//#endif

//#ifdef VBOX_WITH_VBOXSDL
    {   kSupIFT_Exe,  kSupID_AppBin,             true, "VBoxSDL" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxSDL" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_VBOXBFE
    {   kSupIFT_Exe,  kSupID_AppBin,             true, "VBoxBFE" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxBFE" SUPLIB_DLL_SUFF },
//#endif

//#ifdef VBOX_WITH_WEBSERVICES
    {   kSupIFT_Exe,  kSupID_AppBin,             true, "vboxwebsrv" SUPLIB_EXE_SUFF },
//#endif

#ifdef RT_OS_LINUX
    {   kSupIFT_Exe,  kSupID_AppBin,             true, "VBoxTunctl" SUPLIB_EXE_SUFF },
#endif

//#ifdef VBOX_WITH_NETFLT
    {   kSupIFT_Exe,  kSupID_AppBin,             true, "VBoxNetDHCP" SUPLIB_EXE_SUFF },
    {   kSupIFT_Dll,  kSupID_AppPrivArch,        true, "VBoxNetDHCP" SUPLIB_DLL_SUFF },
//#endif
};


/** Array parallel to g_aSupInstallFiles containing per-file status info. */
static SUPVERIFIEDFILE  g_aSupVerifiedFiles[RT_ELEMENTS(g_aSupInstallFiles)];

/** Array index by install directory specifier containing info about verified directories. */
static SUPVERIFIEDDIR   g_aSupVerifiedDirs[kSupID_End];


/**
 * Assembles the path to a directory.
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
            rc = supR3HardenedPathExecDir(pszDst, cchDst);
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
    if (RT_SUCCESS(rc) && fWithFilename)
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
DECLHIDDEN(int) supR3HardenedVerifyFixedDir(SUPINSTDIR enmDir, bool fFatal)
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
    int rc = supR3HardenedVerifyFixedDir(pFile->enmDir, fFatal);
    if (RT_SUCCESS(rc))
    {
        char szPath[RTPATH_MAX];
        rc = supR3HardenedMakeFilePath(pFile, szPath, sizeof(szPath), true /*fWithFilename*/, fFatal);
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
 *          On verification failure, an error code will be returned when fFatal is clear,
 *          otherwise the program will be terminated.
 *
 * @param   pszFilename         The filename.
 * @param   fFatal              Whether validation failures should be treated as
 *                              fatal (true) or not (false).
 */
DECLHIDDEN(int) supR3HardenedVerifyFixedFile(const char *pszFilename, bool fFatal)
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
                int rc2 = supR3HardenedPathExecDir(szFilename, sizeof(szFilename) - cchProgName - sizeof(SUPLIB_EXE_SUFF));
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
 *          On verification failure, an error code will be returned when fFatal is clear,
 *          otherwise the program will be terminated.
 *
 * @param   fFatal              Whether validation failures should be treated as
 *                              fatal (true) or not (false).
 * @param   fLeaveFilesOpen     If set, all the verified files are left open.
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
 * Copies the N messages into the error buffer and returns @a rc.
 *
 * @returns Returns @a rc
 * @param   rc                  The return code.
 * @param   pErrInfo            The error info structure.
 * @param   cMsgs               The number of messages in the ellipsis.
 * @param   ...                 Message parts.
 */
static int supR3HardenedSetErrorN(int rc, PRTERRINFO pErrInfo, unsigned cMsgs, ...)
{
    if (pErrInfo)
    {
        size_t cbErr  = pErrInfo->cbMsg;
        char  *pszErr = pErrInfo->pszMsg;

        va_list va;
        va_start(va, cMsgs);
        while (cMsgs-- > 0 && cbErr > 0)
        {
            const char *pszMsg = va_arg(va,  const char *);
            size_t cchMsg = VALID_PTR(pszMsg) ? strlen(pszMsg) : 0;
            if (cchMsg >= cbErr)
                cchMsg = cbErr - 1;
            memcpy(pszErr, pszMsg, cchMsg);
            pszErr[cchMsg] = '\0';
            pszErr += cchMsg;
            cbErr -= cchMsg;
        }
        va_end(va);

        pErrInfo->rc      = rc;
        pErrInfo->fFlags |= RTERRINFO_FLAGS_SET;
    }

    return rc;
}


/**
 * Copies the three messages into the error buffer and returns @a rc.
 *
 * @returns Returns @a rc
 * @param   rc                  The return code.
 * @param   pErrInfo            The error info structure.
 * @param   pszMsg1             The first message part.
 * @param   pszMsg2             The second message part.
 * @param   pszMsg3             The third message part.
 */
static int supR3HardenedSetError3(int rc, PRTERRINFO pErrInfo, const char *pszMsg1,
                                  const char *pszMsg2, const char *pszMsg3)
{
    return supR3HardenedSetErrorN(rc, pErrInfo, 3, pszMsg1, pszMsg2, pszMsg3);
}

#ifdef SOME_UNUSED_FUNCTION

/**
 * Copies the two messages into the error buffer and returns @a rc.
 *
 * @returns Returns @a rc
 * @param   rc                  The return code.
 * @param   pErrInfo            The error info structure.
 * @param   pszMsg1             The first message part.
 * @param   pszMsg2             The second message part.
 */
static int supR3HardenedSetError2(int rc, PRTERRINFO pErrInfo, const char *pszMsg1,
                                  const char *pszMsg2)
{
    return supR3HardenedSetErrorN(rc, pErrInfo, 2, pszMsg1, pszMsg2);
}


/**
 * Copies the error message to the error buffer and returns @a rc.
 *
 * @returns Returns @a rc
 * @param   rc                  The return code.
 * @param   pErrInfo            The error info structure.
 * @param   pszMsg              The message.
 */
static int supR3HardenedSetError(int rc, PRTERRINFO pErrInfo, const char *pszMsg)
{
    return supR3HardenedSetErrorN(rc, pErrInfo, 1, pszMsg);
}

#endif /* SOME_UNUSED_FUNCTION */

/**
 * Output from a successfull supR3HardenedVerifyPathSanity call.
 */
typedef struct SUPR3HARDENEDPATHINFO
{
    /** The length of the path in szCopy. */
    uint16_t        cch;
    /** The number of path components. */
    uint16_t        cComponents;
    /** Set if the path ends with slash, indicating that it's a directory
     * reference and not a file reference.  The slash has been removed from
     * the copy. */
    bool            fDirSlash;
    /** The offset where each path component starts, i.e. the char after the
     * slash.  The array has cComponents + 1 entries, where the final one is
     * cch + 1 so that one can always terminate the current component by
     * szPath[aoffComponent[i] - 1] = '\0'. */
    uint16_t        aoffComponents[32+1];
    /** A normalized copy of the path.
     * Reserve some extra space so we can be more relaxed about overflow
     * checks and terminator paddings, especially when recursing. */
    char            szPath[SUPR3HARDENED_MAX_PATH * 2];
} SUPR3HARDENEDPATHINFO;
/** Pointer to a parsed path. */
typedef SUPR3HARDENEDPATHINFO *PSUPR3HARDENEDPATHINFO;


/**
 * Verifies that the path is absolutely sane, it also parses the path.
 *
 * A sane path starts at the root (w/ drive letter on DOS derived systems) and
 * does not have any relative bits (/../) or unnecessary slashes (/bin//ls).
 * Sane paths are less or equal to SUPR3HARDENED_MAX_PATH bytes in length.  UNC
 * paths are not supported.
 *
 * @returns VBox status code.
 * @param   pszPath             The path to check.
 * @param   pErrInfo            The error info structure.
 * @param   pInfo               Where to return a copy of the path along with
 *                              parsing information.
 */
static int supR3HardenedVerifyPathSanity(const char *pszPath, PRTERRINFO pErrInfo, PSUPR3HARDENEDPATHINFO pInfo)
{
    const char *pszSrc = pszPath;
    char       *pszDst = pInfo->szPath;

    /*
     * Check that it's an absolute path and copy the volume/root specifier.
     */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (   RT_C_IS_ALPHA(pszSrc[0])
        || pszSrc[1] != ':'
        || !RTPATH_IS_SLASH(pszSrc[2]))
        return supR3HardenedSetError3(VERR_SUPLIB_PATH_NOT_ABSOLUTE, pErrInfo, "The path is not absolute: '", pszPath, "'");

    *pszDst++ = RT_C_TO_UPPER(pszSrc[0]);
    *pszDst++ = ':';
    *pszDst++ = RTPATH_SLASH;
    pszSrc += 3;

#else
    if (!RTPATH_IS_SLASH(pszSrc[0]))
        return supR3HardenedSetError3(VERR_SUPLIB_PATH_NOT_ABSOLUTE, pErrInfo, "The path is not absolute: '", pszPath, "'");

    *pszDst++ = RTPATH_SLASH;
    pszSrc += 1;
#endif

    /*
     * No path specifying the root or something very shortly thereafter will
     * be approved of.
     */
    if (pszSrc[0] == '\0')
        return supR3HardenedSetError3(VERR_SUPLIB_PATH_IS_ROOT, pErrInfo, "The path is root: '", pszPath, "'");
    if (   pszSrc[1] == '\0'
        || pszSrc[2] == '\0')
        return supR3HardenedSetError3(VERR_SUPLIB_PATH_TOO_SHORT, pErrInfo, "The path is too short: '", pszPath, "'");

    /*
     * Check each component.  No parent references or double slashes.
     */
    pInfo->cComponents = 0;
    pInfo->fDirSlash   = false;
    while (pszSrc[0])
    {
        /* Sanity checks. */
        if (RTPATH_IS_SLASH(pszSrc[0])) /* can be relaxed if we care. */
            return supR3HardenedSetError3(VERR_SUPLIB_PATH_NOT_CLEAN, pErrInfo,
                                          "The path is not clean of double slashes: '", pszPath, "'");
        if (   pszSrc[0] == '.'
            && pszSrc[1] == '.'
            && RTPATH_IS_SLASH(pszSrc[2]))
            return supR3HardenedSetError3(VERR_SUPLIB_PATH_NOT_ABSOLUTE, pErrInfo,
                                          "The path is not absolute: '", pszPath, "'");

        /* Record the start of the component. */
        if (pInfo->cComponents >= RT_ELEMENTS(pInfo->aoffComponents) - 1)
            return supR3HardenedSetError3(VERR_SUPLIB_PATH_TOO_MANY_COMPONENTS, pErrInfo,
                                          "The path has too many components: '", pszPath, "'");
        pInfo->aoffComponents[pInfo->cComponents++] = pszDst - &pInfo->szPath[0];

        /* Traverse to the end of the component, copying it as we go along. */
        while (pszSrc[0])
        {
            if (RTPATH_IS_SLASH(pszSrc[0]))
            {
                pszSrc++;
                if (*pszSrc)
                    *pszDst++ = RTPATH_SLASH;
                else
                    pInfo->fDirSlash = true;
                break;
            }
            *pszDst++ = *pszSrc++;
            if ((uintptr_t)(pszDst - &pInfo->szPath[0]) >= SUPR3HARDENED_MAX_PATH)
                return supR3HardenedSetError3(VERR_SUPLIB_PATH_TOO_LONG, pErrInfo,
                                              "The path is too long: '", pszPath, "'");
        }
    }

    /* Terminate the string and enter its length. */
    pszDst[0] = '\0';
    pszDst[1] = '\0';                   /* for aoffComponents */
    pInfo->cch = (uint16_t)(pszDst - &pInfo->szPath[0]);
    pInfo->aoffComponents[pInfo->cComponents] = pInfo->cch + 1;

    return VINF_SUCCESS;
}


/**
 * The state information collected by supR3HardenedVerifyFsObject.
 *
 * This can be used to verify that a directory we've opened for enumeration is
 * the same as the one that supR3HardenedVerifyFsObject just verified.  It can
 * equally be used to verify a native specfied by the user.
 */
typedef struct SUPR3HARDENEDFSOBJSTATE
{
#ifdef RT_OS_WINDOWS
    /** Not implemented for windows yet. */
    char            chTodo;
#else
    /** The stat output. */
    struct stat     Stat;
#endif
} SUPR3HARDENEDFSOBJSTATE;
/** Pointer to a file system object state. */
typedef SUPR3HARDENEDFSOBJSTATE *PSUPR3HARDENEDFSOBJSTATE;
/** Pointer to a const file system object state. */
typedef SUPR3HARDENEDFSOBJSTATE const *PCSUPR3HARDENEDFSOBJSTATE;


/**
 * Query information about a file system object by path.
 *
 * @returns VBox status code, error buffer filled on failure.
 * @param   pszPath             The path to the object.
 * @param   pFsObjState         Where to return the state information.
 * @param   pErrInfo            The error info structure.
 */
static int supR3HardenedQueryFsObjectByPath(char const *pszPath, PSUPR3HARDENEDFSOBJSTATE pFsObjState, PRTERRINFO pErrInfo)
{
#if defined(RT_OS_WINDOWS)
    /** @todo Windows hardening. */
    pFsObjState->chTodo = 0;
    return VINF_SUCCESS;

#else
    /*
     * Stat the object, do not follow links.
     */
    if (lstat(pszPath, &pFsObjState->Stat) != 0)
    {
        /* Ignore access errors */
        if (errno != EACCES)
            return supR3HardenedSetErrorN(VERR_SUPLIB_STAT_FAILED, pErrInfo,
                                          5, "stat failed with ", strerror(errno), " on: '", pszPath, "'");
    }

    /*
     * Read ACLs.
     */
    /** @todo */

    return VINF_SUCCESS;
#endif
}


/**
 * Query information about a file system object by native handle.
 *
 * @returns VBox status code, error buffer filled on failure.
 * @param   hNative             The native handle to the object @a pszPath
 *                              specifies and this should be verified to be the
 *                              same file system object.
 * @param   pFsObjState         Where to return the state information.
 * @param   pszPath             The path to the object. (For the error message
 *                              only.)
 * @param   pErrInfo            The error info structure.
 */
static int supR3HardenedQueryFsObjectByHandle(RTHCUINTPTR hNative, PSUPR3HARDENEDFSOBJSTATE pFsObjState,
                                              char const *pszPath, PRTERRINFO pErrInfo)
{
#if defined(RT_OS_WINDOWS)
    /** @todo Windows hardening. */
    pFsObjState->chTodo = 0;
    return VINF_SUCCESS;

#else
    /*
     * Stat the object, do not follow links.
     */
    if (fstat((int)hNative, &pFsObjState->Stat) != 0)
        return supR3HardenedSetErrorN(VERR_SUPLIB_STAT_FAILED, pErrInfo,
                                      5, "fstat failed with ", strerror(errno), " on '", pszPath, "'");

    /*
     * Read ACLs.
     */
    /** @todo */

    return VINF_SUCCESS;
#endif
}


/**
 * Verifies that the file system object indicated by the native handle is the
 * same as the one @a pFsObjState indicates.
 *
 * @returns VBox status code, error buffer filled on failure.
 * @param   pFsObjState1        File system object information/state by path.
 * @param   pFsObjState2        File system object information/state by handle.
 * @param   pszPath             The path to the object @a pFsObjState
 *                              describes.  (For the error message.)
 * @param   pErrInfo            The error info structure.
 */
static int supR3HardenedIsSameFsObject(PCSUPR3HARDENEDFSOBJSTATE pFsObjState1, PCSUPR3HARDENEDFSOBJSTATE pFsObjState2,
                                       const char *pszPath, PRTERRINFO pErrInfo)
{
#if defined(RT_OS_WINDOWS)
    /** @todo Windows hardening. */
    return VINF_SUCCESS;

#elif defined(RT_OS_OS2)
    return VINF_SUCCESS;

#else
    /*
     * Compare the ino+dev, then the uid+gid and finally the important mode
     * bits.  Technically the first one should be enough, but we're paranoid.
     */
    if (   pFsObjState1->Stat.st_ino != pFsObjState2->Stat.st_ino
        || pFsObjState1->Stat.st_dev != pFsObjState2->Stat.st_dev)
        return supR3HardenedSetError3(VERR_SUPLIB_NOT_SAME_OBJECT, pErrInfo,
                                      "The native handle is not the same as '", pszPath, "' (ino/dev)");
    if (   pFsObjState1->Stat.st_uid != pFsObjState2->Stat.st_uid
        || pFsObjState1->Stat.st_gid != pFsObjState2->Stat.st_gid)
        return supR3HardenedSetError3(VERR_SUPLIB_NOT_SAME_OBJECT, pErrInfo,
                                      "The native handle is not the same as '", pszPath, "' (uid/gid)");
    if (   (pFsObjState1->Stat.st_mode & (S_IFMT | S_IWUSR | S_IWGRP | S_IWOTH))
        != (pFsObjState2->Stat.st_mode & (S_IFMT | S_IWUSR | S_IWGRP | S_IWOTH)))
        return supR3HardenedSetError3(VERR_SUPLIB_NOT_SAME_OBJECT, pErrInfo,
                                      "The native handle is not the same as '", pszPath, "' (mode)");
    return VINF_SUCCESS;
#endif
}


/**
 * Verifies a file system object (file or directory).
 *
 * @returns VBox status code, error buffer filled on failure.
 * @param   pFsObjState         The file system object information/state to be
 *                              verified.
 * @param   fDir                Whether this is a directory or a file.
 * @param   fRelaxed            Whether we can be more relaxed about this
 *                              directory (only used for grand parent
 *                              directories).
 * @param   pszPath             The path to the object. For error messages and
 *                              securing a couple of hacks.
 * @param   pErrInfo            The error info structure.
 */
static int supR3HardenedVerifyFsObject(PCSUPR3HARDENEDFSOBJSTATE pFsObjState, bool fDir, bool fRelaxed,
                                       const char *pszPath, PRTERRINFO pErrInfo)
{
#if defined(RT_OS_WINDOWS)
    /** @todo Windows hardening. */
    NOREF(pFsObjState); NOREF(fDir); NOREF(fRelaxed); NOREF(pszPath); NOREF(pErrInfo);
    return VINF_SUCCESS;

#elif defined(RT_OS_OS2)
    /* No hardening here - it's a single user system. */
    NOREF(pFsObjState); NOREF(fDir); NOREF(fRelaxed); NOREF(pszPath); NOREF(pErrInfo);
    return VINF_SUCCESS;

#else
    /*
     * The owner must be root.
     *
     * This can be extended to include predefined system users if necessary.
     */
    if (pFsObjState->Stat.st_uid != 0)
        return supR3HardenedSetError3(VERR_SUPLIB_OWNER_NOT_ROOT, pErrInfo, "The owner is not root: '", pszPath, "'");

    /*
     * The object type must be directory or file, no symbolic links or other
     * risky stuff (sorry dude, but we're paranoid on purpose here).
     */
    if (   !S_ISDIR(pFsObjState->Stat.st_mode)
        && !S_ISREG(pFsObjState->Stat.st_mode))
    {
        if (S_ISLNK(pFsObjState->Stat.st_mode))
            return supR3HardenedSetError3(VERR_SUPLIB_SYMLINKS_ARE_NOT_PERMITTED, pErrInfo,
                                          "Symlinks are not permitted: '", pszPath, "'");
        return supR3HardenedSetError3(VERR_SUPLIB_NOT_DIR_NOT_FILE, pErrInfo,
                                      "Not regular file or directory: '", pszPath, "'");
    }
    if (fDir != !!S_ISDIR(pFsObjState->Stat.st_mode))
    {
        if (S_ISDIR(pFsObjState->Stat.st_mode))
            return supR3HardenedSetError3(VERR_SUPLIB_IS_DIRECTORY, pErrInfo,
                                          "Expected file but found directory: '", pszPath, "'");
        return supR3HardenedSetError3(VERR_SUPLIB_IS_FILE, pErrInfo,
                                      "Expected directory but found file: '", pszPath, "'");
    }

    /*
     * The group does not matter if it does not have write access, if it has
     * write access it must be group 0 (root/wheel/whatever).
     *
     * This can be extended to include predefined system groups or groups that
     * only root is member of.
     */
    if (   (pFsObjState->Stat.st_mode & S_IWGRP)
        && pFsObjState->Stat.st_gid != 0)
    {
#ifdef RT_OS_DARWIN
        /* HACK ALERT: On Darwin /Applications is root:admin with admin having
           full access. So, to work around we relax the hardening a bit and
           permit grand parents and beyond to be group writable by admin. */
        /** @todo dynamically resolve the admin group? */
        bool fBad = !fRelaxed || pFsObjState->Stat.st_gid != 80 /*admin*/ || strcmp(pszPath, "/Applications");

#elif defined(RT_OS_FREEBSD)
        /* HACK ALERT: PC-BSD 9 has group-writable /usr/pib directory which is
           similar to /Applications on OS X (see above).
           On FreeBSD root is normally the only member of this group, on
           PC-BSD the default user is a member. */
        /** @todo dynamically resolve the operator group? */
        bool fBad = !fRelaxed || pFsObjState->Stat.st_gid != 5 /*operator*/ || strcmp(pszPath, "/usr/pbi");
        NOREF(fRelaxed);
#else
        NOREF(fRelaxed);
        bool fBad = true;
#endif
        if (fBad)
            return supR3HardenedSetError3(VERR_SUPLIB_WRITE_NON_SYS_GROUP, pErrInfo,
                                          "The group is not a system group and it has write access to '", pszPath, "'");
    }

    /*
     * World must not have write access.  There is no relaxing this rule.
     */
    if (pFsObjState->Stat.st_mode & S_IWOTH)
        return supR3HardenedSetError3(VERR_SUPLIB_WORLD_WRITABLE, pErrInfo,
                                      "World writable: '", pszPath, "'");

    /*
     * Check the ACLs.
     */
    /** @todo */

    return VINF_SUCCESS;
#endif
}


/**
 * Verifies that the file system object indicated by the native handle is the
 * same as the one @a pFsObjState indicates.
 *
 * @returns VBox status code, error buffer filled on failure.
 * @param   hNative             The native handle to the object @a pszPath
 *                              specifies and this should be verified to be the
 *                              same file system object.
 * @param   pFsObjState         The information/state returned by a previous
 *                              query call.
 * @param   pszPath             The path to the object @a pFsObjState
 *                              describes.  (For the error message.)
 * @param   pErrInfo            The error info structure.
 */
static int supR3HardenedVerifySameFsObject(RTHCUINTPTR hNative, PCSUPR3HARDENEDFSOBJSTATE pFsObjState,
                                           const char *pszPath, PRTERRINFO pErrInfo)
{
    SUPR3HARDENEDFSOBJSTATE FsObjState2;
    int rc = supR3HardenedQueryFsObjectByHandle(hNative, &FsObjState2, pszPath, pErrInfo);
    if (RT_SUCCESS(rc))
        rc = supR3HardenedIsSameFsObject(pFsObjState, &FsObjState2, pszPath, pErrInfo);
    return rc;
}


/**
 * Does the recursive directory enumeration.
 *
 * @returns VBox status code, error buffer filled on failure.
 * @param   pszDirPath          The path buffer containing the subdirectory to
 *                              enumerate followed by a slash (this is never
 *                              the root slash).  The buffer is RTPATH_MAX in
 *                              size and anything starting at @a cchDirPath
 *                              - 1 and beyond is scratch space.
 * @param   cchDirPath          The length of the directory path + slash.
 * @param   pFsObjState         Pointer to the file system object state buffer.
 *                              On input this will hold the stats for
 *                              the directory @a pszDirPath indicates and will
 *                              be used to verified that we're opening the same
 *                              thing.
 * @param   fRecursive          Whether to recurse into subdirectories.
 * @param   pErrInfo            The error info structure.
 */
static int supR3HardenedVerifyDirRecursive(char *pszDirPath, size_t cchDirPath, PSUPR3HARDENEDFSOBJSTATE pFsObjState,
                                           bool fRecursive, PRTERRINFO pErrInfo)
{
#if defined(RT_OS_WINDOWS)
    /** @todo Windows hardening. */
    return VINF_SUCCESS;

#elif defined(RT_OS_OS2)
    /* No hardening here - it's a single user system. */
    return VINF_SUCCESS;

#else
    /*
     * Open the directory.  Now, we could probably eliminate opendir here
     * and go down on kernel API level (open + getdents for instance), however
     * that's not very portable and hopefully not necessary.
     */
    DIR *pDir = opendir(pszDirPath);
    if (!pDir)
    {
        /* Ignore access errors. */
        if (errno == EACCES)
            return VINF_SUCCESS;
        return supR3HardenedSetErrorN(VERR_SUPLIB_DIR_ENUM_FAILED, pErrInfo,
                                      5, "opendir failed with ", strerror(errno), " on '", pszDirPath, "'");
    }
    if (dirfd(pDir) != -1)
    {
        int rc = supR3HardenedVerifySameFsObject(dirfd(pDir), pFsObjState, pszDirPath, pErrInfo);
        if (RT_FAILURE(rc))
        {
            closedir(pDir);
            return rc;
        }
    }

    /*
     * Enumerate the directory, check all the requested bits.
     */
    int rc = VINF_SUCCESS;
    for (;;)
    {
        pszDirPath[cchDirPath] = '\0';  /* for error messages. */

        struct dirent Entry;
        struct dirent *pEntry;
        int iErr = readdir_r(pDir, &Entry, &pEntry);
        if (iErr)
        {
            rc = supR3HardenedSetErrorN(VERR_SUPLIB_DIR_ENUM_FAILED, pErrInfo,
                                        5, "readdir_r failed with ", strerror(iErr), " in '", pszDirPath, "'");
            break;
        }
        if (!pEntry)
            break;

        /*
         * Check the length and copy it into the path buffer so it can be
         * stat()'ed.
         */
        size_t cchName = strlen(pEntry->d_name);
        if (cchName + cchDirPath > SUPR3HARDENED_MAX_PATH)
        {
            rc = supR3HardenedSetErrorN(VERR_SUPLIB_PATH_TOO_LONG, pErrInfo,
                                        4, "Path grew too long during recursion: '", pszDirPath, pEntry->d_name, "'");
            break;
        }
        memcpy(&pszDirPath[cchName], pEntry->d_name, cchName + 1);

        /*
         * Query the information about the entry and verify it.
         * (We don't bother skipping '.' and '..' at this point, a little bit
         * of extra checks doesn't hurt and neither requires relaxed handling.)
         */
        rc = supR3HardenedQueryFsObjectByPath(pszDirPath, pFsObjState, pErrInfo);
        if (RT_SUCCESS(rc))
            break;
        rc = supR3HardenedVerifyFsObject(pFsObjState, S_ISDIR(pFsObjState->Stat.st_mode), false /*fRelaxed*/,
                                         pszDirPath, pErrInfo);
        if (RT_FAILURE(rc))
            break;

        /*
         * Recurse into subdirectories if requested.
         */
        if (    fRecursive
            &&  S_ISDIR(pFsObjState->Stat.st_mode)
            &&  strcmp(pEntry->d_name, ".")
            &&  strcmp(pEntry->d_name, ".."))
        {
            pszDirPath[cchDirPath + cchName]     = RTPATH_SLASH;
            pszDirPath[cchDirPath + cchName + 1] = '\0';

            rc = supR3HardenedVerifyDirRecursive(pszDirPath, cchDirPath + cchName + 1, pFsObjState,
                                                 fRecursive, pErrInfo);
            if (RT_FAILURE(rc))
                break;
        }
    }

    closedir(pDir);
    return VINF_SUCCESS;
#endif
}


/**
 * Worker for SUPR3HardenedVerifyDir.
 *
 * @returns See SUPR3HardenedVerifyDir.
 * @param   pszDirPath          See SUPR3HardenedVerifyDir.
 * @param   fRecursive          See SUPR3HardenedVerifyDir.
 * @param   fCheckFiles         See SUPR3HardenedVerifyDir.
 * @param   pErrInfo            See SUPR3HardenedVerifyDir.
 */
DECLHIDDEN(int) supR3HardenedVerifyDir(const char *pszDirPath, bool fRecursive, bool fCheckFiles, PRTERRINFO pErrInfo)
{
    /*
     * Validate the input path and parse it.
     */
    SUPR3HARDENEDPATHINFO Info;
    int rc = supR3HardenedVerifyPathSanity(pszDirPath, pErrInfo, &Info);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Verify each component from the root up.
     */
    SUPR3HARDENEDFSOBJSTATE FsObjState;
    uint32_t const          cComponents = Info.cComponents;
    for (uint32_t iComponent = 0; iComponent < cComponents; iComponent++)
    {
        bool fRelaxed = iComponent + 2 < cComponents;
        Info.szPath[Info.aoffComponents[iComponent + 1] - 1] = '\0';
        rc = supR3HardenedQueryFsObjectByPath(Info.szPath, &FsObjState, pErrInfo);
        if (RT_SUCCESS(rc))
            rc = supR3HardenedVerifyFsObject(&FsObjState, true /*fDir*/, fRelaxed, Info.szPath, pErrInfo);
        if (RT_FAILURE(rc))
            return rc;
        Info.szPath[Info.aoffComponents[iComponent + 1] - 1] = iComponent + 1 != cComponents ? RTPATH_SLASH : '\0';
    }

    /*
     * Check files and subdirectories if requested.
     */
    if (fCheckFiles || fRecursive)
    {
        Info.szPath[Info.cch]     = RTPATH_SLASH;
        Info.szPath[Info.cch + 1] = '\0';
        return supR3HardenedVerifyDirRecursive(Info.szPath, Info.cch + 1, &FsObjState,
                                               fRecursive, pErrInfo);
    }

    return VINF_SUCCESS;
}


/**
 * Verfies a file.
 *
 * @returns VBox status code, error buffer filled on failure.
 * @param   pszFilename         The file to verify.
 * @param   hNativeFile         Handle to the file, verify that it's the same
 *                              as we ended up with when verifying the path.
 *                              RTHCUINTPTR_MAX means NIL here.
 * @param   pErrInfo            Where to return extended error information.
 *                              Optional.
 */
DECLHIDDEN(int) supR3HardenedVerifyFile(const char *pszFilename, RTHCUINTPTR hNativeFile, PRTERRINFO pErrInfo)
{
    /*
     * Validate the input path and parse it.
     */
    SUPR3HARDENEDPATHINFO Info;
    int rc = supR3HardenedVerifyPathSanity(pszFilename, pErrInfo, &Info);
    if (RT_FAILURE(rc))
        return rc;
    if (Info.fDirSlash)
        return supR3HardenedSetError3(VERR_SUPLIB_IS_DIRECTORY, pErrInfo,
                                      "The file path specifies a directory: '", pszFilename, "'");

    /*
     * Verify each component from the root up.
     */
    SUPR3HARDENEDFSOBJSTATE FsObjState;
    uint32_t const          cComponents = Info.cComponents;
    for (uint32_t iComponent = 0; iComponent < cComponents; iComponent++)
    {
        bool fFinal   = iComponent + 1 == cComponents;
        bool fRelaxed = iComponent + 2 < cComponents;
        Info.szPath[Info.aoffComponents[iComponent + 1] - 1] = '\0';
        rc = supR3HardenedQueryFsObjectByPath(Info.szPath, &FsObjState, pErrInfo);
        if (RT_SUCCESS(rc))
            rc = supR3HardenedVerifyFsObject(&FsObjState, !fFinal /*fDir*/, fRelaxed, Info.szPath, pErrInfo);
        if (RT_FAILURE(rc))
            return rc;
        Info.szPath[Info.aoffComponents[iComponent + 1] - 1] = !fFinal ? RTPATH_SLASH : '\0';
    }

    /*
     * Verify the file.
     */
    if (hNativeFile != RTHCUINTPTR_MAX)
        return supR3HardenedVerifySameFsObject(hNativeFile, &FsObjState, Info.szPath, pErrInfo);
    return VINF_SUCCESS;
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
