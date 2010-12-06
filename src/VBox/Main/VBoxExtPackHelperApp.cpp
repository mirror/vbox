/* $Id$ */
/** @file
 * VirtualBox Main - Extension Pack Helper Application, usually set-uid-to-root.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "include/ExtPackUtil.h"

#include <iprt/buildconfig.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/manifest.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/vfs.h>
#include <iprt/zip.h>
#include <iprt/cpp/ministring.h>

#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/sup.h>
#include <VBox/version.h>

#if defined(RT_OS_DARWIN)
# include <sys/types.h>
# include <unistd.h>                    /* geteuid */
#endif

#ifdef RT_OS_WINDOWS
# define _WIN32_WINNT 0x0501
# include <Objbase.h>                   /* CoInitializeEx */
# include <Windows.h>                   /* ShellExecuteEx, ++ */
# ifdef DEBUG
#  include <Sddl.h>
# endif
#endif

#ifdef RT_OS_DARWIN
# include <Security/Authorization.h>
# include <Security/AuthorizationTags.h>
# include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(RT_OS_DARWIN) || defined(RT_OS_WINDOWS)
# include <stdio.h>
# include <errno.h>
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Enable elevation on Windows and Darwin. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_DARWIN) || defined(DOXYGEN_RUNNING)
# define WITH_ELEVATION
#endif

/** The maximum entry name length.
 * Play short and safe. */
#define VBOX_EXTPACK_MAX_ENTRY_NAME_LENGTH      128


#ifdef IN_RT_R3
/* Override RTAssertShouldPanic to prevent gdb process creation. */
RTDECL(bool) RTAssertShouldPanic(void)
{
    return true;
}
#endif



/**
 * Handle the special standard options when these are specified after the
 * command.
 *
 * @param   ch          The option character.
 */
static RTEXITCODE DoStandardOption(int ch)
{
    switch (ch)
    {
        case 'h':
        {
            RTMsgInfo(VBOX_PRODUCT " Extension Pack Helper App\n"
                      "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
                      "All rights reserved.\n"
                      "\n"
                      "This NOT intended for general use, please use VBoxManage instead\n"
                      "or call the IExtPackManager API directly.\n"
                      "\n"
                      "Usage: %s <command> [options]\n"
                      "Commands:\n"
                      "    install --base-dir <dir> --cert-dir <dir> --name <name> \\\n"
                      "        --tarball <tarball> --tarball-fd <fd>\n"
                      "    uninstall --base-dir <dir> --name <name>\n"
                      "    cleanup --base-dir <dir>\n"
                      , RTProcShortName());
            return RTEXITCODE_SUCCESS;
        }

        case 'V':
            RTPrintf("%sr%d\n", VBOX_VERSION_STRING, RTBldCfgRevision());
            return RTEXITCODE_SUCCESS;

        default:
            AssertFailedReturn(RTEXITCODE_FAILURE);
    }
}


/**
 * Checks if the cerficiate directory is valid.
 *
 * @returns true if it is valid, false if it isn't.
 * @param   pszCertDir          The certificate directory to validate.
 */
static bool IsValidCertificateDir(const char *pszCertDir)
{
    /*
     * Just be darn strict for now.
     */
    char szCorrect[RTPATH_MAX];
    int rc = RTPathAppPrivateNoArch(szCorrect, sizeof(szCorrect));
    if (RT_FAILURE(rc))
        return false;
    rc = RTPathAppend(szCorrect, sizeof(szCorrect), VBOX_EXTPACK_CERT_DIR);
    if (RT_FAILURE(rc))
        return false;

    return RTPathCompare(szCorrect, pszCertDir) == 0;
}


/**
 * Checks if the base directory is valid.
 *
 * @returns true if it is valid, false if it isn't.
 * @param   pszBaesDir          The base directory to validate.
 */
static bool IsValidBaseDir(const char *pszBaseDir)
{
    /*
     * Just be darn strict for now.
     */
    char szCorrect[RTPATH_MAX];
    int rc = RTPathAppPrivateArch(szCorrect, sizeof(szCorrect));
    if (RT_FAILURE(rc))
        return false;
    rc = RTPathAppend(szCorrect, sizeof(szCorrect), VBOX_EXTPACK_INSTALL_DIR);
    if (RT_FAILURE(rc))
        return false;

    return RTPathCompare(szCorrect, pszBaseDir) == 0;
}


/**
 * Cleans up a temporary extension pack directory.
 *
 * This is used by 'uninstall', 'cleanup' and in the failure path of 'install'.
 *
 * @returns The program exit code.
 * @param   pszDir          The directory to clean up.  The caller is
 *                          responsible for making sure this is valid.
 * @param   fTemporary      Whether this is a temporary install directory or
 *                          not.
 */
static RTEXITCODE RemoveExtPackDir(const char *pszDir, bool fTemporary)
{
    /** @todo May have to undo 555 modes here later.  */
    int rc = RTDirRemoveRecursive(pszDir, RTDIRRMREC_F_CONTENT_AND_DIR);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                              "Failed to delete the %sextension pack directory: %Rrc ('%s')",
                              fTemporary ? "temporary " : "", rc, pszDir);
    return RTEXITCODE_SUCCESS;
}


/**
 * Rewinds the tarball file handle and creates a gunzip | tar chain that
 * results in a filesystem stream.
 *
 * @returns success or failure, message displayed on failure.
 * @param   hTarballFile    The handle to the tarball file.
 * @param   phTarFss        Where to return the filesystem stream handle.
 */
static RTEXITCODE OpenTarFss(RTFILE hTarballFile, PRTVFSFSSTREAM phTarFss)
{
    /*
     * Rewind the file and set up a VFS chain for it.
     */
    int rc = RTFileSeek(hTarballFile, 0, RTFILE_SEEK_BEGIN, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed seeking to the start of the tarball: %Rrc\n", rc);

    RTVFSIOSTREAM hTarballIos;
    rc = RTVfsIoStrmFromRTFile(hTarballFile, RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN, true /*fLeaveOpen*/,
                               &hTarballIos);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsIoStrmFromRTFile failed: %Rrc\n", rc);

    RTVFSIOSTREAM hGunzipIos;
    rc = RTZipGzipDecompressIoStream(hTarballIos, 0 /*fFlags*/, &hGunzipIos);
    if (RT_SUCCESS(rc))
    {
        RTVFSFSSTREAM hTarFss;
        rc = RTZipTarFsStreamFromIoStream(hGunzipIos, 0 /*fFlags*/, &hTarFss);
        if (RT_SUCCESS(rc))
        {
            RTVfsIoStrmRelease(hGunzipIos);
            RTVfsIoStrmRelease(hTarballIos);
            *phTarFss = hTarFss;
            return RTEXITCODE_SUCCESS;
        }
        RTMsgError("RTZipTarFsStreamFromIoStream failed: %Rrc\n", rc);
        RTVfsIoStrmRelease(hGunzipIos);
    }
    else
        RTMsgError("RTZipGzipDecompressIoStream failed: %Rrc\n", rc);
    RTVfsIoStrmRelease(hTarballIos);
    return RTEXITCODE_FAILURE;
}


/**
 * Sets the permissions of the temporary extension pack directory just before
 * renaming it.
 *
 * By default the temporary directory is only accessible by root, this function
 * will make it world readable and browseable.
 *
 * @returns The program exit code.
 * @param   pszDir              The temporary extension pack directory.
 */
static RTEXITCODE SetExtPackPermissions(const char *pszDir)
{
    RTMsgInfo("Setting permissions...");
#if !defined(RT_OS_WINDOWS)
     int rc = RTPathSetMode(pszDir, 0755);
     if (RT_FAILURE(rc))
         return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to set directory permissions: %Rrc ('%s')", rc, pszDir);
#else
        /** @todo  */
#endif

    return RTEXITCODE_SUCCESS;
}


/**
 * Verifies the manifest and its signature.
 *
 * @returns Program exit code, failure with message.
 * @param   hManifestFile   The xml from the extension pack.
 * @param   pszExtPackName  The expected extension pack name.
 */
static RTEXITCODE VerifyXml(RTVFSFILE hXmlFile, const char *pszExtPackName)
{
    /** @todo implement XML verification. */
    return RTEXITCODE_SUCCESS;
}


/**
 * Verifies the manifest and its signature.
 *
 * @returns Program exit code, failure with message.
 * @param   hOurManifest    The manifest we compiled.
 * @param   hManifestFile   The manifest file in the extension pack.
 * @param   hSignatureFile  The manifest signature file.
 */
static RTEXITCODE VerifyManifestAndSignature(RTMANIFEST hOurManifest, RTVFSFILE hManifestFile, RTVFSFILE hSignatureFile)
{
    /*
     * Read the manifest from the extension pack.
     */
    int rc = RTVfsFileSeek(hManifestFile, 0, RTFILE_SEEK_BEGIN, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsFileSeek failed: %Rrc", rc);

    RTMANIFEST hTheirManifest;
    rc = RTManifestCreate(0 /*fFlags*/, &hTheirManifest);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTManifestCreate failed: %Rrc", rc);

    RTVFSIOSTREAM hVfsIos = RTVfsFileToIoStream(hManifestFile);
    rc = RTManifestReadStandard(hTheirManifest, hVfsIos);
    RTVfsIoStrmRelease(hVfsIos);
    if (RT_SUCCESS(rc))
    {
        /*
         * Compare the manifests.
         */
        static const char *s_apszIgnoreEntries[] =
        {
            VBOX_EXTPACK_MANIFEST_NAME,
            VBOX_EXTPACK_SIGNATURE_NAME,
            "./" VBOX_EXTPACK_MANIFEST_NAME,
            "./" VBOX_EXTPACK_SIGNATURE_NAME,
            NULL
        };
        char szError[RTPATH_MAX];
        rc = RTManifestEqualsEx(hOurManifest, hTheirManifest, &s_apszIgnoreEntries[0], NULL,
                                RTMANIFEST_EQUALS_IGN_MISSING_ATTRS /*fFlags*/,
                                szError, sizeof(szError));
        if (RT_SUCCESS(rc))
        {
            /*
             * Validate the manifest file signature.
             */
            /** @todo implement signature stuff */

        }
        else if (rc == VERR_NOT_EQUAL && szError[0])
            RTMsgError("Manifest mismatch: %s", szError);
        else
            RTMsgError("RTManifestEqualsEx failed: %Rrc", rc);
#if 0
        RTVFSIOSTREAM hVfsIosStdOut = NIL_RTVFSIOSTREAM;
        RTVfsIoStrmFromStdHandle(RTHANDLESTD_OUTPUT, RTFILE_O_WRITE, true, &hVfsIosStdOut);
        RTVfsIoStrmWrite(hVfsIosStdOut, "Our:\n", sizeof("Our:\n") - 1, true, NULL);
        RTManifestWriteStandard(hOurManifest, hVfsIosStdOut);
        RTVfsIoStrmWrite(hVfsIosStdOut, "Their:\n", sizeof("Their:\n") - 1, true, NULL);
        RTManifestWriteStandard(hTheirManifest, hVfsIosStdOut);
#endif
    }
    else
        RTMsgError("Error parsing '%s': %Rrc", VBOX_EXTPACK_MANIFEST_NAME, rc);

    RTManifestRelease(hTheirManifest);
    return RT_SUCCESS(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


/**
 * Validates a name in an extension pack.
 *
 * We restrict the charset to try make sure the extension pack can be unpacked
 * on all file systems.
 *
 * @returns Program exit code, failure with message.
 * @param   pszName             The name to validate.
 */
static RTEXITCODE ValidateNameInExtPack(const char *pszName)
{
    if (RTPathStartsWithRoot(pszName))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "'%s': starts with root spec", pszName);

    const char *pszErr = NULL;
    const char *psz = pszName;
    int ch;
    while ((ch = *psz) != '\0')
    {
        /* Character set restrictions. */
        if (ch < 0 || ch >= 128)
        {
            pszErr = "Only 7-bit ASCII allowed";
            break;
        }
        if (ch <= 31 || ch == 127)
        {
            pszErr = "No control characters are not allowed";
            break;
        }
        if (ch == '\\')
        {
            pszErr = "Only backward slashes are not allowed";
            break;
        }
        if (strchr("'\":;*?|[]<>(){}", ch))
        {
            pszErr = "The characters ', \", :, ;, *, ?, |, [, ], <, >, (, ), { and } are not allowed";
            break;
        }

        /* Take the simple way out and ban all ".." sequences. */
        if (   ch     == '.'
            && psz[1] == '.')
        {
            pszErr = "Double dot sequence are not allowed";
            break;
        }

        /* Keep the tree shallow or the hardening checks will fail. */
        if (psz - pszName > VBOX_EXTPACK_MAX_ENTRY_NAME_LENGTH)
        {
            pszErr = "Too long";
            break;
        }

        /* advance */
        psz++;
    }

    if (pszErr)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Bad member name '%s' (pos %zu): %s", pszName, (size_t)(psz - pszName), pszErr);
    return RTEXITCODE_SUCCESS;
}


/**
 * Validates a file in an extension pack.
 *
 * @returns Program exit code, failure with message.
 * @param   pszName             The name of the file.
 * @param   hVfsObj             The VFS object.
 */
static RTEXITCODE ValidateFileInExtPack(const char *pszName, RTVFSOBJ hVfsObj)
{
    RTEXITCODE rcExit = ValidateNameInExtPack(pszName);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        RTFSOBJINFO ObjInfo;
        int rc = RTVfsObjQueryInfo(hVfsObj, &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_SUCCESS(rc))
        {
            if (ObjInfo.cbObject >= 9*_1G64)
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "'%s': too large (%'RU64 bytes)",
                                        pszName, (uint64_t)ObjInfo.cbObject);
            if (!RTFS_IS_FILE(ObjInfo.Attr.fMode))
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE,
                                        "The alleged file '%s' has a mode mask saying differently (%RTfmode)",
                                        pszName, ObjInfo.Attr.fMode);
        }
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsObjQueryInfo failed on '%s': %Rrc", pszName, rc);
    }
    return rcExit;
}


/**
 * Validates a directory in an extension pack.
 *
 * @returns Program exit code, failure with message.
 * @param   pszName             The name of the directory.
 * @param   hVfsObj             The VFS object.
 */
static RTEXITCODE ValidateDirInExtPack(const char *pszName, RTVFSOBJ hVfsObj)
{
    RTEXITCODE rcExit = ValidateNameInExtPack(pszName);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        RTFSOBJINFO ObjInfo;
        int rc = RTVfsObjQueryInfo(hVfsObj, &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_SUCCESS(rc))
        {
            if (!RTFS_IS_DIRECTORY(ObjInfo.Attr.fMode))
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE,
                                        "The alleged directory '%s' has a mode mask saying differently (%RTfmode)",
                                        pszName, ObjInfo.Attr.fMode);
        }
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsObjQueryInfo failed on '%s': %Rrc", pszName, rc);
    }
    return rcExit;
}


/**
 * Validates a member of an extension pack.
 *
 * @returns Program exit code, failure with message.
 * @param   pszName             The name of the directory.
 * @param   enmType             The object type.
 * @param   hVfsObj             The VFS object.
 */
static RTEXITCODE ValidateMemberOfExtPack(const char *pszName, RTVFSOBJTYPE enmType, RTVFSOBJ hVfsObj)
{
    RTEXITCODE rcExit;
    if (   enmType == RTVFSOBJTYPE_FILE
        || enmType == RTVFSOBJTYPE_IO_STREAM)
        rcExit = ValidateFileInExtPack(pszName, hVfsObj);
    else if (   enmType == RTVFSOBJTYPE_DIR
             || enmType == RTVFSOBJTYPE_BASE)
        rcExit = ValidateDirInExtPack(pszName, hVfsObj);
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "'%s' is not a file or directory (enmType=%d)", pszName, enmType);
    return rcExit;
}


/**
 * Validates the extension pack tarball prior to unpacking.
 *
 * Operations performed:
 *      - Hardening checks.
 *
 * @returns The program exit code.
 * @param   pszDir              The directory where the extension pack has been
 *                              unpacked.
 * @param   pszExtPackName      The expected extension pack name.
 * @param   pszTarball          The name of the tarball in case we have to
 *                              complain about something.
 */
static RTEXITCODE ValidateUnpackedExtPack(const char *pszDir, const char *pszTarball, const char *pszExtPackName)
{
    RTMsgInfo("Validating unpacked extension pack...");

    char szErr[4096+1024];
    int rc = SUPR3HardenedVerifyDir(pszDir, true /*fRecursive*/, true /*fCheckFiles*/, szErr, sizeof(szErr));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Hardening check failed with %Rrc: %s", rc, szErr);
    return RTEXITCODE_SUCCESS;
}


/**
 * Unpacks a directory from an extension pack tarball.
 *
 * @returns Program exit code, failure with message.
 * @param   pszDstDirName   The name of the unpacked directory.
 * @param   hVfsObj         The source object for the directory.
 */
static RTEXITCODE UnpackExtPackDir(const char *pszDstDirName, RTVFSOBJ hVfsObj)
{
    int rc = RTDirCreate(pszDstDirName, 0755);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create directory '%s': %Rrc", pszDstDirName, rc);
    /** @todo Ownership tricks on windows? */
    return RTEXITCODE_SUCCESS;
}


/**
 * Unpacks a file from an extension pack tarball.
 *
 * @returns Program exit code, failure with message.
 * @param   pszName         The name in the tarball.
 * @param   pszDstFilename  The name of the unpacked file.
 * @param   hVfsIosSrc      The source stream for the file.
 * @param   hUnpackManifest The manifest to add the file digest to.
 */
static RTEXITCODE UnpackExtPackFile(const char *pszName, const char *pszDstFilename,
                                    RTVFSIOSTREAM hVfsIosSrc, RTMANIFEST hUnpackManifest)
{
    /*
     * Query the object info, we'll need it for buffer sizing as well as
     * setting the file mode.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTVfsIoStrmQueryInfo(hVfsIosSrc, &ObjInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsIoStrmQueryInfo failed with %Rrc on '%s'", rc, pszDstFilename);

    /*
     * Create the file.
     */
    uint32_t fFlags = RTFILE_O_WRITE | RTFILE_O_DENY_ALL | RTFILE_O_CREATE | (0600 << RTFILE_O_CREATE_MODE_SHIFT);
    RTFILE   hFile;
    rc = RTFileOpen(&hFile, pszDstFilename, fFlags);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create '%s': %Rrc", pszDstFilename, rc);

    /*
     * Create a I/O stream for the destination file, stack a manifest entry
     * creator on top of it.
     */
    RTVFSIOSTREAM hVfsIosDst2;
    rc = RTVfsIoStrmFromRTFile(hFile, fFlags, true /*fLeaveOpen*/, &hVfsIosDst2);
    if (RT_SUCCESS(rc))
    {
        RTVFSIOSTREAM hVfsIosDst;
        rc = RTManifestEntryAddPassthruIoStream(hUnpackManifest, hVfsIosDst2, pszName,
                                                RTMANIFEST_ATTR_SIZE | RTMANIFEST_ATTR_SHA256,
                                                false /*fReadOrWrite*/, &hVfsIosDst);
        RTVfsIoStrmRelease(hVfsIosDst2);
        if (RT_SUCCESS(rc))
        {
            /*
             * Pump the data thru.
             */
            rc = RTVfsUtilPumpIoStreams(hVfsIosSrc, hVfsIosDst, (uint32_t)RT_MIN(ObjInfo.cbObject, _1G));
            if (RT_SUCCESS(rc))
            {
                rc = RTManifestPtIosAddEntryNow(hVfsIosDst);
                if (RT_SUCCESS(rc))
                {
                    RTVfsIoStrmRelease(hVfsIosDst);
                    hVfsIosDst = NIL_RTVFSIOSTREAM;

                    /*
                     * Set the mode mask.
                     */
                    ObjInfo.Attr.fMode &= ~(RTFS_UNIX_IWOTH | RTFS_UNIX_IWGRP);
                    rc = RTFileSetMode(hFile, ObjInfo.Attr.fMode);
                    /** @todo Windows needs to do more here, I think. */
                    if (RT_SUCCESS(rc))
                    {
                        RTFileClose(hFile);
                        return RTEXITCODE_SUCCESS;
                    }

                    RTMsgError("Failed to set the mode of '%s' to %RTfmode: %Rrc", pszDstFilename, ObjInfo.Attr.fMode, rc);
                }
                else
                    RTMsgError("RTManifestPtIosAddEntryNow failed for '%s': %Rrc", pszDstFilename, rc);
            }
            else
                RTMsgError("RTVfsUtilPumpIoStreams failed for '%s': %Rrc", pszDstFilename, rc);
            RTVfsIoStrmRelease(hVfsIosDst);
        }
        else
            RTMsgError("RTManifestEntryAddPassthruIoStream failed: %Rrc", rc);
    }
    else
        RTMsgError("RTVfsIoStrmFromRTFile failed: %Rrc", rc);
    RTFileClose(hFile);
    return RTEXITCODE_FAILURE;
}


/**
 * Unpacks the extension pack into the specified directory.
 *
 * This will apply ownership and permission changes to all the content, the
 * exception is @a pszDirDst which will be handled by SetExtPackPermissions.
 *
 * @returns The program exit code.
 * @param   hTarballFile        The tarball to unpack.
 * @param   pszDirDst           Where to unpack it.
 * @param   hValidManifest      The manifest we've validated.
 * @param   pszTarball          The name of the tarball in case we have to
 *                              complain about something.
 */
static RTEXITCODE UnpackExtPack(RTFILE hTarballFile, const char *pszDirDst, RTMANIFEST hValidManifest,
                                const char *pszTarball)
{
    RTMsgInfo("Unpacking extension pack into '%s'...", pszDirDst);

    /*
     * Set up the destination path.
     */
    char szDstPath[RTPATH_MAX];
    int rc = RTPathAbs(pszDirDst, szDstPath, sizeof(szDstPath) - VBOX_EXTPACK_MAX_ENTRY_NAME_LENGTH - 2);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathAbs('%s',,) failed: %Rrc", pszDirDst, rc);
    size_t offDstPath = RTPathStripTrailingSlash(szDstPath);
    szDstPath[offDstPath++] = '/';
    szDstPath[offDstPath]   = '\0';

    /*
     * Open the tar.gz filesystem stream and set up an manifest in-memory file.
     */
    RTVFSFSSTREAM hTarFss;
    RTEXITCODE rcExit = OpenTarFss(hTarballFile, &hTarFss);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    RTMANIFEST hUnpackManifest;
    rc = RTManifestCreate(0 /*fFlags*/, &hUnpackManifest);
    if (RT_SUCCESS(rc))
    {
        /*
         * Process the tarball (would be nice to move this to a function).
         */
        for (;;)
        {
            /*
             * Get the next stream object.
             */
            char           *pszName;
            RTVFSOBJ        hVfsObj;
            RTVFSOBJTYPE    enmType;
            rc = RTVfsFsStrmNext(hTarFss, &pszName, &enmType, &hVfsObj);
            if (RT_FAILURE(rc))
            {
                if (rc != VERR_EOF)
                    rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsFsStrmNext failed: %Rrc", rc);
                break;
            }
            const char     *pszAdjName = pszName[0] == '.' && pszName[1] == '/' ? &pszName[2] : pszName;

            /*
             * Check the type & name validity then unpack it.
             */
            rcExit = ValidateMemberOfExtPack(pszName, enmType, hVfsObj);
            if (rcExit == RTEXITCODE_SUCCESS)
            {
                szDstPath[offDstPath] = '\0';
                rc = RTStrCopy(&szDstPath[offDstPath], sizeof(szDstPath) - offDstPath, pszAdjName);
                if (RT_SUCCESS(rc))
                {
                    if (   enmType == RTVFSOBJTYPE_FILE
                        || enmType == RTVFSOBJTYPE_IO_STREAM)
                    {
                        RTVFSIOSTREAM hVfsIos = RTVfsObjToIoStream(hVfsObj);
                        rcExit = UnpackExtPackFile(pszAdjName, szDstPath, hVfsIos, hUnpackManifest);
                        RTVfsIoStrmRelease(hVfsIos);
                    }
                    else if (*pszAdjName && strcmp(pszAdjName, "."))
                        rcExit = UnpackExtPackDir(szDstPath, hVfsObj);
                }
                else
                    rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Name is too long: '%s' (%Rrc)", pszAdjName, rc);
            }

            /*
             * Clean up and break out on failure.
             */
            RTVfsObjRelease(hVfsObj);
            RTStrFree(pszName);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;
        }

        /*
         * Check that what we just extracted matches the already verified
         * manifest.
         */
        if (rcExit == RTEXITCODE_SUCCESS)
        {
            char szError[RTPATH_MAX];
            rc = RTManifestEqualsEx(hUnpackManifest, hValidManifest, NULL /*papszIgnoreEntries*/, NULL /*papszIgnoreAttr*/,
                                    0 /*fFlags*/, szError, sizeof(szError));
            if (RT_SUCCESS(rc))
                rc = RTEXITCODE_SUCCESS;
            else if (rc == VERR_NOT_EQUAL && szError[0])
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Manifest mismatch: %s", szError);
            else
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTManifestEqualsEx failed: %Rrc", rc);
        }
#if 0
        RTVFSIOSTREAM hVfsIosStdOut = NIL_RTVFSIOSTREAM;
        RTVfsIoStrmFromStdHandle(RTHANDLESTD_OUTPUT, RTFILE_O_WRITE, true, &hVfsIosStdOut);
        RTVfsIoStrmWrite(hVfsIosStdOut, "Unpack:\n", sizeof("Unpack:\n") - 1, true, NULL);
        RTManifestWriteStandard(hUnpackManifest, hVfsIosStdOut);
        RTVfsIoStrmWrite(hVfsIosStdOut, "Valid:\n", sizeof("Valid:\n") - 1, true, NULL);
        RTManifestWriteStandard(hValidManifest, hVfsIosStdOut);
#endif
        RTManifestRelease(hUnpackManifest);
    }
    RTVfsFsStrmRelease(hTarFss);

    return rcExit;
}



/**
 * Validates the extension pack tarball prior to unpacking.
 *
 * Operations performed:
 *      - Mandatory files.
 *      - Manifest check.
 *      - Manifest seal check.
 *      - XML check, match name.
 *
 * @returns The program exit code.
 * @param   hTarballFile        The handle to open the @a pszTarball file.
 * @param   pszExtPackName      The name of the extension pack name.
 * @param   pszTarball          The name of the tarball in case we have to
 *                              complain about something.
 * @param   phValidManifest     Where to return the handle to fully validated
 *                              the manifest for the extension pack.  This
 *                              includes all files.
 *
 * @todo    This function is a bit too long and should be split up if possible.
 */
static RTEXITCODE ValidateExtPackTarball(RTFILE hTarballFile, const char *pszExtPackName, const char *pszTarball,
                                         PRTMANIFEST phValidManifest)
{
    *phValidManifest = NIL_RTMANIFEST;
    RTMsgInfo("Validating extension pack '%s' ('%s')...", pszTarball, pszExtPackName);

    /*
     * Open the tar.gz filesystem stream and set up an manifest in-memory file.
     */
    RTVFSFSSTREAM hTarFss;
    RTEXITCODE rcExit = OpenTarFss(hTarballFile, &hTarFss);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    RTMANIFEST hOurManifest;
    int rc = RTManifestCreate(0 /*fFlags*/, &hOurManifest);
    if (RT_SUCCESS(rc))
    {
        /*
         * Process the tarball (would be nice to move this to a function).
         */
        RTVFSFILE hXmlFile      = NIL_RTVFSFILE;
        RTVFSFILE hManifestFile = NIL_RTVFSFILE;
        RTVFSFILE hSignatureFile= NIL_RTVFSFILE;
        for (;;)
        {
            /*
             * Get the next stream object.
             */
            char           *pszName;
            RTVFSOBJ        hVfsObj;
            RTVFSOBJTYPE    enmType;
            rc = RTVfsFsStrmNext(hTarFss, &pszName, &enmType, &hVfsObj);
            if (RT_FAILURE(rc))
            {
                if (rc != VERR_EOF)
                    rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsFsStrmNext failed: %Rrc", rc);
                break;
            }
            const char     *pszAdjName = pszName[0] == '.' && pszName[1] == '/' ? &pszName[2] : pszName;

            /*
             * Check the type & name validity.
             */
            rcExit = ValidateMemberOfExtPack(pszName, enmType, hVfsObj);
            if (rcExit == RTEXITCODE_SUCCESS)
            {
                /*
                 * Check if this is one of the standard files.
                 */
                PRTVFSFILE  phVfsFile;
                if (!strcmp(pszAdjName, VBOX_EXTPACK_DESCRIPTION_NAME))
                    phVfsFile = &hXmlFile;
                else if (!strcmp(pszAdjName, VBOX_EXTPACK_MANIFEST_NAME))
                    phVfsFile = &hManifestFile;
                else if (!strcmp(pszAdjName, VBOX_EXTPACK_SIGNATURE_NAME))
                    phVfsFile = &hSignatureFile;
                else
                    phVfsFile = NULL;
                if (phVfsFile)
                {
                    /*
                     * Make sure it's a file and that it isn't too large.
                     */
                    if (*phVfsFile != NIL_RTVFSFILE)
                        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "There can only be one '%s'", pszAdjName);
                    else if (enmType != RTVFSOBJTYPE_IO_STREAM && enmType != RTVFSOBJTYPE_FILE)
                        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Standard member '%s' is not a file", pszAdjName);
                    else
                    {
                        RTFSOBJINFO ObjInfo;
                        rc = RTVfsObjQueryInfo(hVfsObj, &ObjInfo, RTFSOBJATTRADD_NOTHING);
                        if (RT_SUCCESS(rc))
                        {
                            if (!RTFS_IS_FILE(ObjInfo.Attr.fMode))
                                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Standard member '%s' is not a file", pszAdjName);
                            else if (ObjInfo.cbObject >= _1M)
                                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE,
                                                        "Standard member '%s' is too large: %'RU64 bytes (max 1 MB)",
                                                        pszAdjName, (uint64_t)ObjInfo.cbObject);
                            else
                            {
                                /*
                                 * Make an in memory copy of the stream.
                                 */
                                RTVFSIOSTREAM hVfsIos = RTVfsObjToIoStream(hVfsObj);
                                rc = RTVfsMemorizeIoStreamAsFile(hVfsIos, RTFILE_O_READ, phVfsFile);
                                if (RT_SUCCESS(rc))
                                {
                                    /*
                                     * To simplify the code below, replace
                                     * hVfsObj with the memorized file.
                                     */
                                    RTVfsObjRelease(hVfsObj);
                                    hVfsObj = RTVfsObjFromFile(*phVfsFile);
                                }
                                else
                                    rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE,
                                                            "RTVfsMemorizeIoStreamAsFile failed on '%s': %Rrc", pszName, rc);
                                RTVfsIoStrmRelease(hVfsIos);
                            }
                        }
                        else
                            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTVfsObjQueryInfo failed on '%s': %Rrc", pszName, rc);
                    }
                }
            }

            /*
             * Add any I/O stream to the manifest
             */
            if (   rcExit == RTEXITCODE_SUCCESS
                && (   enmType == RTVFSOBJTYPE_FILE
                    || enmType == RTVFSOBJTYPE_IO_STREAM))
            {
                RTVFSIOSTREAM hVfsIos = RTVfsObjToIoStream(hVfsObj);
                rc = RTManifestEntryAddIoStream(hOurManifest, hVfsIos, pszAdjName, RTMANIFEST_ATTR_SIZE | RTMANIFEST_ATTR_SHA256);
                if (RT_FAILURE(rc))
                    rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTManifestEntryAddIoStream failed on '%s': %Rrc", pszAdjName, rc);
                RTVfsIoStrmRelease(hVfsIos);
            }

            /*
             * Clean up and break out on failure.
             */
            RTVfsObjRelease(hVfsObj);
            RTStrFree(pszName);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;
        }

        /*
         * If we've successfully processed the tarball, verify that the
         * mandatory files are present.
         */
        if (rcExit == RTEXITCODE_SUCCESS)
        {
            if (hXmlFile == NIL_RTVFSFILE)
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Mandator file '%s' is missing", VBOX_EXTPACK_DESCRIPTION_NAME);
            if (hManifestFile == NIL_RTVFSFILE)
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Mandator file '%s' is missing", VBOX_EXTPACK_MANIFEST_NAME);
            if (hSignatureFile == NIL_RTVFSFILE)
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Mandator file '%s' is missing", VBOX_EXTPACK_SIGNATURE_NAME);
        }

        /*
         * Check the manifest and it's signature.
         */
        if (rcExit == RTEXITCODE_SUCCESS)
            rcExit = VerifyManifestAndSignature(hOurManifest, hManifestFile, hSignatureFile);

        /*
         * Check the XML.
         */
        if (rcExit == RTEXITCODE_SUCCESS)
            rcExit = VerifyXml(hXmlFile, pszExtPackName);

        /*
         * Release objects and stuff.
         */
        if (rcExit == RTEXITCODE_SUCCESS)
            *phValidManifest = hOurManifest;
        else
            RTManifestRelease(hOurManifest);

        RTVfsFileRelease(hXmlFile);
        RTVfsFileRelease(hManifestFile);
        RTVfsFileRelease(hSignatureFile);
    }
    RTVfsFsStrmRelease(hTarFss);

    return rcExit;
}


/**
 * The 2nd part of the installation process.
 *
 * @returns The program exit code.
 * @param   pszBaseDir          The base directory.
 * @param   pszCertDir          The certificat directory.
 * @param   pszTarball          The tarball name.
 * @param   hTarballFile        The handle to open the @a pszTarball file.
 * @param   hTarballFileOpt     The tarball file handle (optional).
 * @param   pszName             The extension pack name.
 * @param   pszMangledName      The mangled extension pack name.
 */
static RTEXITCODE DoInstall2(const char *pszBaseDir, const char *pszCertDir, const char *pszTarball,
                             RTFILE hTarballFile, RTFILE hTarballFileOpt,
                             const char *pszName, const char *pszMangledName)
{
    /*
     * Do some basic validation of the tarball file.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTFileQueryInfo(hTarballFile, &ObjInfo, RTFSOBJATTRADD_UNIX);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTFileQueryInfo failed with %Rrc on '%s'", rc, pszTarball);
    if (!RTFS_IS_FILE(ObjInfo.Attr.fMode))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Not a regular file: %s", pszTarball);

    if (hTarballFileOpt != NIL_RTFILE)
    {
        RTFSOBJINFO ObjInfo2;
        rc = RTFileQueryInfo(hTarballFileOpt, &ObjInfo2, RTFSOBJATTRADD_UNIX);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTFileQueryInfo failed with %Rrc on --tarball-fd", rc);
        if (   ObjInfo.Attr.u.Unix.INodeIdDevice != ObjInfo2.Attr.u.Unix.INodeIdDevice
            || ObjInfo.Attr.u.Unix.INodeId       != ObjInfo2.Attr.u.Unix.INodeId)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "--tarball and --tarball-fd does not match");
    }

    /*
     * Construct the paths to the two directories we'll be using.
     */
    char szFinalPath[RTPATH_MAX];
    rc = RTPathJoin(szFinalPath, sizeof(szFinalPath), pszBaseDir, pszMangledName);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                              "Failed to construct the path to the final extension pack directory: %Rrc", rc);

    char szTmpPath[RTPATH_MAX];
    rc = RTPathJoin(szTmpPath, sizeof(szTmpPath) - 64, pszBaseDir, pszMangledName);
    if (RT_SUCCESS(rc))
    {
        size_t cchTmpPath = strlen(szTmpPath);
        RTStrPrintf(&szTmpPath[cchTmpPath], sizeof(szTmpPath) - cchTmpPath, "-_-inst-%u", (uint32_t)RTProcSelf());
    }
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                              "Failed to construct the path to the temporary extension pack directory: %Rrc", rc);

    /*
     * Check that they don't exist at this point in time.
     */
    rc = RTPathQueryInfoEx(szFinalPath, &ObjInfo, RTFSOBJATTRADD_NOTHING,  RTPATH_F_ON_LINK);
    if (RT_SUCCESS(rc) && RTFS_IS_DIRECTORY(ObjInfo.Attr.fMode))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "The extension pack is already installed. You must uninstall the old one first.");
    if (RT_SUCCESS(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
                              "Found non-directory file system object where the extension pack would be installed ('%s')",
                              szFinalPath);
    if (rc != VERR_FILE_NOT_FOUND && rc != VERR_PATH_NOT_FOUND)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Unexpected RTPathQueryInfoEx status code %Rrc for '%s'", rc, szFinalPath);

    rc = RTPathQueryInfoEx(szTmpPath, &ObjInfo, RTFSOBJATTRADD_NOTHING,  RTPATH_F_ON_LINK);
    if (rc != VERR_FILE_NOT_FOUND && rc != VERR_PATH_NOT_FOUND)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Unexpected RTPathQueryInfoEx status code %Rrc for '%s'", rc, szFinalPath);

    /*
     * Create the temporary directory and prepare the extension pack within it.
     * If all checks out correctly, rename it to the final directory.
     */
    RTDirCreate(pszBaseDir, 0755);
    rc = RTDirCreate(szTmpPath, 0700);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create temporary directory: %Rrc ('%s')", rc, szTmpPath);

    RTMANIFEST hValidManifest = NIL_RTMANIFEST;
    RTEXITCODE rcExit = ValidateExtPackTarball(hTarballFile, pszName, pszTarball, &hValidManifest);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = UnpackExtPack(hTarballFile, szTmpPath, hValidManifest, pszTarball);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = ValidateUnpackedExtPack(szTmpPath, pszTarball, pszName);
    if (rcExit == RTEXITCODE_SUCCESS)
        rcExit = SetExtPackPermissions(szTmpPath);
    RTManifestRelease(hValidManifest);

    if (rcExit == RTEXITCODE_SUCCESS)
    {
        rc = RTDirRename(szTmpPath, szFinalPath, RTPATHRENAME_FLAGS_NO_REPLACE);
        if (RT_SUCCESS(rc))
            RTMsgInfo("Successfully installed '%s' (%s)", pszName, pszTarball);
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE,
                                    "Failed to rename the temporary directory to the final one: %Rrc ('%s' -> '%s')",
                                    rc, szTmpPath, szFinalPath);
    }

    /*
     * Clean up the temporary directory on failure.
     */
    if (rcExit != RTEXITCODE_SUCCESS)
        RemoveExtPackDir(szTmpPath, true /*fTemporary*/);

    return rcExit;
}


/**
 * Implements the 'install' command.
 *
 * @returns The program exit code.
 * @param   argc            The number of program arguments.
 * @param   argv            The program arguments.
 */
static RTEXITCODE DoInstall(int argc, char **argv)
{
    /*
     * Parse the parameters.
     *
     * Note! The --base-dir and --cert-dir are only for checking that the
     *       caller and this help applications have the same idea of where
     *       things are.  Likewise, the --name is for verifying assumptions
     *       the caller made about the name.  The optional --tarball-fd option
     *       is just for easing the paranoia on the user side.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--base-dir",     'b',   RTGETOPT_REQ_STRING },
        { "--cert-dir",     'c',   RTGETOPT_REQ_STRING },
        { "--name",         'n',   RTGETOPT_REQ_STRING },
        { "--tarball",      't',   RTGETOPT_REQ_STRING },
        { "--tarball-fd",   'd',   RTGETOPT_REQ_UINT64 }
    };
    RTGETOPTSTATE   GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTGetOptInit failed: %Rrc\n", rc);

    const char     *pszBaseDir      = NULL;
    const char     *pszCertDir      = NULL;
    const char     *pszName         = NULL;
    const char     *pszTarball      = NULL;
    RTFILE          hTarballFileOpt = NIL_RTFILE;
    RTGETOPTUNION   ValueUnion;
    int             ch;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'b':
                if (pszBaseDir)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many --base-dir options");
                pszBaseDir = ValueUnion.psz;
                if (!IsValidBaseDir(pszBaseDir))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid base directory: '%s'", pszBaseDir);
                break;

            case 'c':
                if (pszCertDir)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many --cert-dir options");
                pszCertDir = ValueUnion.psz;
                if (!IsValidCertificateDir(pszCertDir))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid certificate directory: '%s'", pszCertDir);
                break;

            case 'n':
                if (pszName)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many --name options");
                pszName = ValueUnion.psz;
                if (!VBoxExtPackIsValidName(pszName))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid extension pack name: '%s'", pszName);
                break;

            case 't':
                if (pszTarball)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many --tarball options");
                pszTarball = ValueUnion.psz;
                break;

            case 'd':
            {
                if (hTarballFileOpt != NIL_RTFILE)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many --tarball-fd options");
                RTHCUINTPTR hNative = (RTHCUINTPTR)ValueUnion.u64;
                if (hNative != ValueUnion.u64)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "The --tarball-fd value is out of range: %#RX64", ValueUnion.u64);
                rc = RTFileFromNative(&hTarballFileOpt, hNative);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "RTFileFromNative failed on --target-fd value: %Rrc", rc);
                break;
            }

            case 'h':
            case 'V':
                return DoStandardOption(ch);

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    if (!pszName)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Missing --name option");
    if (!pszBaseDir)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Missing --base-dir option");
    if (!pszCertDir)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Missing --cert-dir option");
    if (!pszTarball)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Missing --tarball option");

    /*
     * Ok, down to business.
     */
    iprt::MiniString *pstrMangledName = VBoxExtPackMangleName(pszName);
    if (!pstrMangledName)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to mangle name ('%s)", pszName);

    RTEXITCODE  rcExit;
    RTFILE      hTarballFile;
    rc = RTFileOpen(&hTarballFile, pszTarball, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
    {
        rcExit = DoInstall2(pszBaseDir, pszCertDir, pszTarball, hTarballFile, hTarballFileOpt,
                            pszName, pstrMangledName->c_str());
        RTFileClose(hTarballFile);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to open the extension pack tarball: %Rrc ('%s')", rc, pszTarball);

    delete pstrMangledName;
    return rcExit;
}


/**
 * Implements the 'uninstall' command.
 *
 * @returns The program exit code.
 * @param   argc            The number of program arguments.
 * @param   argv            The program arguments.
 */
static RTEXITCODE DoUninstall(int argc, char **argv)
{
    /*
     * Parse the parameters.
     *
     * Note! The --base-dir is only for checking that the caller and this help
     *       applications have the same idea of where things are.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--base-dir",     'b',   RTGETOPT_REQ_STRING },
        { "--name",         'n',   RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE   GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTGetOptInit failed: %Rrc\n", rc);

    const char     *pszBaseDir = NULL;
    const char     *pszName    = NULL;
    RTGETOPTUNION   ValueUnion;
    int             ch;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'b':
                if (pszBaseDir)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many --base-dir options");
                pszBaseDir = ValueUnion.psz;
                if (!IsValidBaseDir(pszBaseDir))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid base directory: '%s'", pszBaseDir);
                break;

            case 'n':
                if (pszName)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many --name options");
                pszName = ValueUnion.psz;
                if (!VBoxExtPackIsValidName(pszName))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid extension pack name: '%s'", pszName);
                break;

            case 'h':
            case 'V':
                return DoStandardOption(ch);

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    if (!pszName)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Missing --name option");
    if (!pszBaseDir)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Missing --base-dir option");

    /*
     * Mangle the name so we can construct the directory names.
     */
    iprt::MiniString *pstrMangledName = VBoxExtPackMangleName(pszName);
    if (!pstrMangledName)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to mangle name ('%s)", pszName);
    iprt::MiniString strMangledName(*pstrMangledName);
    delete pstrMangledName;

    /*
     * Ok, down to business.
     */
    /* Check that it exists. */
    char szExtPackDir[RTPATH_MAX];
    rc = RTPathJoin(szExtPackDir, sizeof(szExtPackDir), pszBaseDir, strMangledName.c_str());
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to construct extension pack path: %Rrc", rc);

    if (!RTDirExists(szExtPackDir))
    {
        RTMsgInfo("Extension pack not installed. Nothing to do.");
        return RTEXITCODE_SUCCESS;
    }

    /* Rename the extension pack directory before deleting it to prevent new
       VM processes from picking it up. */
    char szExtPackUnInstDir[RTPATH_MAX];
    rc = RTPathJoin(szExtPackUnInstDir, sizeof(szExtPackUnInstDir), pszBaseDir, strMangledName.c_str());
    if (RT_SUCCESS(rc))
        rc = RTStrCat(szExtPackUnInstDir, sizeof(szExtPackUnInstDir), "-_-uninst");
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to construct temporary extension pack path: %Rrc", rc);

    rc = RTDirRename(szExtPackDir, szExtPackUnInstDir, RTPATHRENAME_FLAGS_NO_REPLACE);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to rename the extension pack directory: %Rrc", rc);

    /* Recursively delete the directory content. */
    RTEXITCODE rcExit = RemoveExtPackDir(szExtPackUnInstDir, false /*fTemporary*/);
    if (rcExit == RTEXITCODE_SUCCESS)
        RTMsgInfo("Successfully removed extension pack '%s'\n", pszName);

    return rcExit;
}

/**
 * Implements the 'cleanup' command.
 *
 * @returns The program exit code.
 * @param   argc            The number of program arguments.
 * @param   argv            The program arguments.
 */
static RTEXITCODE DoCleanup(int argc, char **argv)
{
    /*
     * Parse the parameters.
     *
     * Note! The --base-dir is only for checking that the caller and this help
     *       applications have the same idea of where things are.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--base-dir",     'b',   RTGETOPT_REQ_STRING },
    };
    RTGETOPTSTATE   GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTGetOptInit failed: %Rrc\n", rc);

    const char     *pszBaseDir = NULL;
    RTGETOPTUNION   ValueUnion;
    int             ch;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'b':
                if (pszBaseDir)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Too many --base-dir options");
                pszBaseDir = ValueUnion.psz;
                if (!IsValidBaseDir(pszBaseDir))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid base directory: '%s'", pszBaseDir);
                break;

            case 'h':
            case 'V':
                return DoStandardOption(ch);

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }
    if (!pszBaseDir)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Missing --base-dir option");

    /*
     * Ok, down to business.
     */
    PRTDIR pDir;
    rc = RTDirOpen(&pDir, pszBaseDir);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed open the base directory: %Rrc ('%s')", rc, pszBaseDir);

    uint32_t    cCleaned = 0;
    RTEXITCODE  rcExit = RTEXITCODE_SUCCESS;
    for (;;)
    {
        RTDIRENTRYEX Entry;
        rc = RTDirReadEx(pDir, &Entry, NULL /*pcbDirEntry*/, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
        if (RT_FAILURE(rc))
        {
            if (rc != VERR_NO_MORE_FILES)
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTDirReadEx returns %Rrc", rc);
            break;
        }

        /*
         * Only directories which conform with our temporary install/uninstall
         * naming scheme are candidates for cleaning.
         */
        if (   RTFS_IS_DIRECTORY(Entry.Info.Attr.fMode)
            && strcmp(Entry.szName, ".")  != 0
            && strcmp(Entry.szName, "..") != 0)
        {
            bool fCandidate = false;
            char *pszMarker = strstr(Entry.szName, "-_-");
            if (   pszMarker
                && (   !strcmp(pszMarker, "-_-uninst")
                    || !strncmp(pszMarker, "-_-inst", sizeof("-_-inst") - 1)))
                fCandidate = VBoxExtPackIsValidMangledName(Entry.szName, pszMarker - &Entry.szName[0]);
            if (fCandidate)
            {
                /*
                 * Recursive delete, safe.
                 */
                char szPath[RTPATH_MAX];
                rc = RTPathJoin(szPath, sizeof(szPath), pszBaseDir, Entry.szName);
                if (RT_SUCCESS(rc))
                {
                    RTEXITCODE rcExit2 = RemoveExtPackDir(szPath, true /*fTemporary*/);
                    if (rcExit2 == RTEXITCODE_SUCCESS)
                        RTMsgInfo("Successfully removed '%s'.", Entry.szName);
                    else if (rcExit == RTEXITCODE_SUCCESS)
                        rcExit = rcExit2;
                }
                else
                    rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathJoin failed with %Rrc for '%s'", rc, Entry.szName);
                cCleaned++;
            }
        }
    }
    RTDirClose(pDir);
    if (!cCleaned)
        RTMsgInfo("Nothing to clean.");
    return rcExit;
}

#ifdef WITH_ELEVATION

/**
 * Copies the content of a file to a stream.
 *
 * @param   hSrc                The source file.
 * @param   pDst                The destination stream.
 * @param   fComplain           Whether to complain about errors (i.e. is this
 *                              stderr, if not keep the trap shut because it
 *                              may be missing when running under VBoxSVC.)
 */
static void CopyFileToStdXxx(RTFILE hSrc, PRTSTREAM pDst, bool fComplain)
{
    int rc;
    for (;;)
    {
        char abBuf[0x1000];
        size_t cbRead;
        rc = RTFileRead(hSrc, abBuf, sizeof(abBuf), &cbRead);
        if (RT_FAILURE(rc))
        {
            RTMsgError("RTFileRead failed: %Rrc", rc);
            break;
        }
        if (!cbRead)
            break;
        rc = RTStrmWrite(pDst, abBuf, cbRead);
        if (RT_FAILURE(rc))
        {
            if (fComplain)
                RTMsgError("RTStrmWrite failed: %Rrc", rc);
            break;
        }
    }
    rc = RTStrmFlush(pDst);
    if (RT_FAILURE(rc) && fComplain)
        RTMsgError("RTStrmFlush failed: %Rrc", rc);
}


/**
 * Relaunches ourselves as a elevated process using platform specific facilities.
 *
 * @returns Program exit code.
 * @param   pszExecPath         The executable path.
 * @param   cArgs               The number of arguments.
 * @param   papszArgs           The arguments.
 */
static RTEXITCODE RelaunchElevatedNative(const char *pszExecPath, int cArgs, const char * const *papszArgs)
{
    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
#ifdef RT_OS_WINDOWS

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    SHELLEXECUTEINFOW   Info;

    Info.cbSize = sizeof(Info);
    Info.fMask  = SEE_MASK_NOCLOSEPROCESS;
    Info.hwnd   = NULL;
    Info.lpVerb = L"runas";
    int rc = RTStrToUtf16(pszExecPath, (PRTUTF16 *)&Info.lpFile);
    if (RT_SUCCESS(rc))
    {
        char *pszCmdLine;
        rc = RTGetOptArgvToString(&pszCmdLine, &papszArgs[1], RTGETOPTARGV_CNV_QUOTE_MS_CRT);
        if (RT_SUCCESS(rc))
        {
            rc = RTStrToUtf16(pszCmdLine, (PRTUTF16 *)&Info.lpParameters);
            if (RT_SUCCESS(rc))
            {
                Info.lpDirectory = NULL;
                Info.nShow       = SW_SHOWMAXIMIZED;
                Info.hInstApp    = NULL;
                Info.lpIDList    = NULL;
                Info.lpClass     = NULL;
                Info.hkeyClass   = NULL;
                Info.dwHotKey    = 0;
                Info.hIcon       = INVALID_HANDLE_VALUE;
                Info.hProcess    = INVALID_HANDLE_VALUE;

                if (ShellExecuteExW(&Info))
                {
                    if (Info.hProcess != INVALID_HANDLE_VALUE)
                    {
                        /*
                         * Wait for the process, make sure the deal with messages.
                         */
                        for (;;)
                        {
                            DWORD dwRc = MsgWaitForMultipleObjects(1, &Info.hProcess, FALSE, 5000/*ms*/, QS_ALLEVENTS);
                            if (dwRc == WAIT_OBJECT_0)
                                break;
                            if (   dwRc != WAIT_TIMEOUT
                                && dwRc != WAIT_OBJECT_0 + 1)
                            {
                                RTMsgError("MsgWaitForMultipleObjects returned: %#x (%d), err=%u", dwRc, dwRc, GetLastError());
                                break;
                            }
                            MSG Msg;
                            while (PeekMessageW(&Msg, NULL, 0, 0, PM_REMOVE))
                            {
                                TranslateMessage(&Msg);
                                DispatchMessageW(&Msg);
                            }
                        }

                        DWORD dwExitCode;
                        if (GetExitCodeProcess(Info.hProcess, &dwExitCode))
                        {
                            if (dwExitCode >= 0 && dwExitCode < 128)
                                rcExit = (RTEXITCODE)dwExitCode;
                            else
                                rcExit = RTEXITCODE_FAILURE;
                        }
                        CloseHandle(Info.hProcess);
                    }
                    else
                        RTMsgError("ShellExecuteExW return INVALID_HANDLE_VALUE as Info.hProcess");
                }
                else
                    RTMsgError("ShellExecuteExW failed: %u (%#x)", GetLastError(), GetLastError());


                RTUtf16Free((PRTUTF16)Info.lpParameters);
            }
            RTStrFree(pszCmdLine);
        }

        RTUtf16Free((PRTUTF16)Info.lpFile);
    }
    else
        RTMsgError("RTStrToUtf16 failed: %Rc", rc);

#elif defined(RT_OS_DARWIN)
    char szIconName[RTPATH_MAX];
    int rc = RTPathAppPrivateArch(szIconName, sizeof(szIconName));
    if (RT_SUCCESS(rc))
        rc = RTPathAppend(szIconName, sizeof(szIconName), "../Resources/virtualbox.png");
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to construct icon path: %Rrc", rc);

    AuthorizationRef AuthRef;
    OSStatus orc = AuthorizationCreate(NULL, 0, kAuthorizationFlagDefaults, &AuthRef);
    if (orc == errAuthorizationSuccess)
    {
        /*
         * Preautorize the privileged execution of ourselves.
         */
        AuthorizationItem   AuthItem        = { kAuthorizationRightExecute, 0, NULL, 0 };
        AuthorizationRights AuthRights      = { 1, &AuthItem };

        static char         s_szPrompt[]    = "VirtualBox needs further rights to make changes to your installation.\n\n";
        AuthorizationItem   aAuthEnvItems[] =
        {
            { kAuthorizationEnvironmentPrompt, strlen(s_szPrompt), s_szPrompt, 0 },
            { kAuthorizationEnvironmentIcon,   strlen(szIconName), szIconName, 0 }
        };
        AuthorizationEnvironment AuthEnv    = { RT_ELEMENTS(aAuthEnvItems), aAuthEnvItems };

        orc = AuthorizationCopyRights(AuthRef, &AuthRights, &AuthEnv,
                                      kAuthorizationFlagPreAuthorize | kAuthorizationFlagInteractionAllowed
                                      | kAuthorizationFlagExtendRights,
                                      NULL);
        if (orc == errAuthorizationSuccess)
        {
            /*
             * Execute with extra permissions
             */
            FILE *pSocketStrm;
            orc = AuthorizationExecuteWithPrivileges(AuthRef, pszExecPath, kAuthorizationFlagDefaults,
                                                     (char * const *)&papszArgs[3],
                                                     &pSocketStrm);
            if (orc == errAuthorizationSuccess)
            {
                /*
                 * Read the output of the tool, the read will fail when it quits.
                 */
                for (;;)
                {
                    char achBuf[1024];
                    size_t cbRead = fread(achBuf, 1, sizeof(achBuf), pSocketStrm);
                    if (!cbRead)
                        break;
                    fwrite(achBuf, 1, cbRead, stdout);
                }
                rcExit = RTEXITCODE_SUCCESS;
                fclose(pSocketStrm);
            }
            else
                RTMsgError("AuthorizationExecuteWithPrivileges failed: %d", orc);
        }
        else if (orc == errAuthorizationCanceled)
            RTMsgError("Authorization canceled by the user");
        else
            RTMsgError("AuthorizationCopyRights failed: %d", orc);
        AuthorizationFree(AuthRef, kAuthorizationFlagDefaults);
    }
    else
        RTMsgError("AuthorizationCreate failed: %d", orc);

#else
# error "PORT ME"
#endif
    return rcExit;
}


/**
 * Relaunches ourselves as a elevated process using platform specific facilities.
 *
 * @returns Program exit code.
 * @param   argc                The number of arguments.
 * @param   argv                The arguments.
 */
static RTEXITCODE RelaunchElevated(int argc, char **argv)
{
    /*
     * We need the executable name later, so get it now when it's easy to quit.
     */
    char szExecPath[RTPATH_MAX];
    if (!RTProcGetExecutablePath(szExecPath,sizeof(szExecPath)))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTProcGetExecutablePath failed");

    /*
     * Create a couple of temporary files for stderr and stdout.
     */
    char szTempDir[RTPATH_MAX - sizeof("/stderr")];
    int rc = RTPathTemp(szTempDir, sizeof(szTempDir));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathTemp failed: %Rrc", rc);
    rc = RTPathAppend(szTempDir, sizeof(szTempDir), "VBoxExtPackHelper-XXXXXX");
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathAppend failed: %Rrc", rc);
    rc = RTDirCreateTemp(szTempDir);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTDirCreateTemp failed: %Rrc", rc);

    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
    char szStdOut[RTPATH_MAX];
    char szStdErr[RTPATH_MAX];
    rc = RTPathJoin(szStdOut, sizeof(szStdOut), szTempDir, "stdout");
    if (RT_SUCCESS(rc))
        rc = RTPathJoin(szStdErr, sizeof(szStdErr), szTempDir, "stderr");
    if (RT_SUCCESS(rc))
    {
        RTFILE hStdOut;
        rc = RTFileOpen(&hStdOut, szStdOut, RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_NONE
                        | (0600 << RTFILE_O_CREATE_MODE_SHIFT));
        if (RT_SUCCESS(rc))
        {
            RTFILE hStdErr;
            rc = RTFileOpen(&hStdErr, szStdErr, RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_NONE
                            | (0600 << RTFILE_O_CREATE_MODE_SHIFT));
            if (RT_SUCCESS(rc))
            {
                /*
                 * Insert the --elevated and stdout/err names into the argument
                 * list.  Note that darwin skips the --stdout bit, so don't
                 * change the order here.
                 */
                int          cArgs     = argc + 5 + 1;
                char const **papszArgs = (char const **)RTMemTmpAllocZ((cArgs + 1) * sizeof(const char *));
                if (papszArgs)
                {
                    int iDst = 0;
                    papszArgs[iDst++] = argv[0];
                    papszArgs[iDst++] = "--stdout";
                    papszArgs[iDst++] = szStdOut;
                    papszArgs[iDst++] = "--stderr";
                    papszArgs[iDst++] = szStdErr;
                    papszArgs[iDst++] = "--elevated";
                    for (int iSrc = 1; iSrc <= argc; iSrc++)
                        papszArgs[iDst++] = argv[iSrc];

                    /*
                     * Do the platform specific process execution (waiting included).
                     */
                    rcExit = RelaunchElevatedNative(szExecPath, cArgs, papszArgs);

                    /*
                     * Copy the standard files to our standard handles.
                     */
                    CopyFileToStdXxx(hStdErr, g_pStdErr, true /*fComplain*/);
                    CopyFileToStdXxx(hStdOut, g_pStdOut, false);

                    RTMemTmpFree(papszArgs);
                }

                RTFileClose(hStdErr);
                RTFileDelete(szStdErr);
            }
            RTFileClose(hStdOut);
            RTFileDelete(szStdOut);
        }
    }
    RTDirRemove(szTempDir);

    return rcExit;
}


/**
 * Checks if the process is elevated or not.
 *
 * @returns RTEXITCODE_SUCCESS if preconditions are fine,
 *          otherwise error message + RTEXITCODE_FAILURE.
 * @param   pfElevated      Where to store the elevation indicator.
 */
static RTEXITCODE ElevationCheck(bool *pfElevated)
{
    *pfElevated = false;

# if defined(RT_OS_WINDOWS)
    /** @todo This should probably check if UAC is diabled and if we are
     *  Administrator first. Also needs to check for Vista+ first, probably.
     */
    DWORD       cb;
    RTEXITCODE  rcExit = RTEXITCODE_SUCCESS;
    HANDLE      hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "OpenProcessToken failed: %u (%#x)", GetLastError(), GetLastError());

    /*
     * Check if we're member of the Administrators group. If we aren't, there
     * is no way to elevate ourselves to system admin.
     * N.B. CheckTokenMembership does not do the job here (due to attributes?).
     */
    BOOL                        fIsAdmin    = FALSE;
    SID_IDENTIFIER_AUTHORITY    NtAuthority = SECURITY_NT_AUTHORITY;
    PSID                        pAdminGrpSid;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminGrpSid))
    {
# ifdef DEBUG
        char *pszAdminGrpSid = NULL;
        ConvertSidToStringSid(pAdminGrpSid, &pszAdminGrpSid);
# endif

        if (   !GetTokenInformation(hToken, TokenGroups, NULL, 0, &cb)
            && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            PTOKEN_GROUPS pTokenGroups = (PTOKEN_GROUPS)RTMemAllocZ(cb);
            if (GetTokenInformation(hToken, TokenGroups, pTokenGroups, cb, &cb))
            {
                for (DWORD iGrp = 0; iGrp < pTokenGroups->GroupCount; iGrp++)
                {
# ifdef DEBUG
                    char *pszGrpSid = NULL;
                    ConvertSidToStringSid(pTokenGroups->Groups[iGrp].Sid, &pszGrpSid);
# endif
                    if (EqualSid(pAdminGrpSid, pTokenGroups->Groups[iGrp].Sid))
                    {
                        /* That it's listed is enough I think, ignore attributes. */
                        fIsAdmin = TRUE;
                        break;
                    }
                }
            }
            else
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "GetTokenInformation(TokenGroups,cb) failed: %u (%#x)", GetLastError(), GetLastError());
            RTMemFree(pTokenGroups);
        }
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "GetTokenInformation(TokenGroups,0) failed: %u (%#x)", GetLastError(), GetLastError());

        FreeSid(pAdminGrpSid);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "AllocateAndInitializeSid failed: %u (%#x)", GetLastError(), GetLastError());
    if (fIsAdmin)
    {
# if 1
        /*
         * Check the integrity level (Vista / UAC).
         */
#  define MY_SECURITY_MANDATORY_HIGH_RID 0x00003000L
#  define MY_TokenIntegrityLevel         ((TOKEN_INFORMATION_CLASS)25)
        if (   !GetTokenInformation(hToken, MY_TokenIntegrityLevel, NULL, 0, &cb)
            && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            PSID_AND_ATTRIBUTES pSidAndAttr = (PSID_AND_ATTRIBUTES)RTMemAlloc(cb);
            if (GetTokenInformation(hToken, MY_TokenIntegrityLevel, pSidAndAttr, cb, &cb))
            {
                DWORD dwIntegrityLevel = *GetSidSubAuthority(pSidAndAttr->Sid, *GetSidSubAuthorityCount(pSidAndAttr->Sid) - 1U);

                if (dwIntegrityLevel >= MY_SECURITY_MANDATORY_HIGH_RID)
                    *pfElevated = true;
            }
            else
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "GetTokenInformation failed: %u (%#x)", GetLastError(), GetLastError());
            RTMemFree(pSidAndAttr);
        }
        else if (   GetLastError() == ERROR_INVALID_PARAMETER
                 || GetLastError() == ERROR_NOT_SUPPORTED)
            *pfElevated = true; /* Older Windows version. */
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "GetTokenInformation failed: %u (%#x)", GetLastError(), GetLastError());

# else
        /*
         * Check elevation (Vista / UAC).
         */
        DWORD TokenIsElevated = 0;
        if (GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)/*TokenElevation*/ 20, &TokenIsElevated, sizeof(TokenIsElevated), &cb))
        {
            fElevated = TokenIsElevated != 0;
            if (fElevated)
            {
                enum
                {
                    MY_TokenElevationTypeDefault = 1,
                    MY_TokenElevationTypeFull,
                    MY_TokenElevationTypeLimited
                } enmType;
                if (GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)/*TokenElevationType*/ 18, &enmType, sizeof(enmType), &cb))
                     *pfElevated = enmType == MY_TokenElevationTypeFull;
                else
                    rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "GetTokenInformation failed: %u (%#x)", GetLastError(), GetLastError());
            }
        }
        else if (   GetLastError() == ERROR_INVALID_PARAMETER
                 && GetLastError() == ERROR_NOT_SUPPORTED)
            *pfElevated = true; /* Older Windows version. */
        else if (   GetLastError() != ERROR_INVALID_PARAMETER
                 && GetLastError() != ERROR_NOT_SUPPORTED)
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "GetTokenInformation failed: %u (%#x)", GetLastError(), GetLastError());
# endif
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Membership in the Administrators group is required to perform this action");

    CloseHandle(hToken);
    return rcExit;

# else
    /*
     * On Unixy systems, we check if the executable and the current user is
     * the same.  This heuristic works fine for both hardened and development
     * builds.
     */
    char szExecPath[RTPATH_MAX];
    if (RTProcGetExecutablePath(szExecPath, sizeof(szExecPath)) == NULL)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTProcGetExecutablePath failed");

    RTFSOBJINFO ObjInfo;
    int rc = RTPathQueryInfoEx(szExecPath, &ObjInfo, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathQueryInfoEx failed");

    *pfElevated = ObjInfo.Attr.u.Unix.uid == geteuid()
               || ObjInfo.Attr.u.Unix.uid == getuid();
    return RTEXITCODE_SUCCESS;
# endif
}

#endif /* WITH_ELEVATION */

int main(int argc, char **argv)
{
    /*
     * Initialize IPRT and check that we're correctly installed.
     */
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    char szErr[2048];
    rc = SUPR3HardenedVerifySelf(argv[0], true /*fInternal*/, szErr, sizeof(szErr));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "%s", szErr);

    /*
     * Elevation check.
     */
    RTEXITCODE rcExit;
#ifdef WITH_ELEVATION
    bool fElevated;
    rcExit = ElevationCheck(&fElevated);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
#endif

    /*
     * Parse the top level arguments until we find a command.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
#define CMD_INSTALL     1000
        { "install",    CMD_INSTALL,    RTGETOPT_REQ_NOTHING },
#define CMD_UNINSTALL   1001
        { "uninstall",  CMD_UNINSTALL,  RTGETOPT_REQ_NOTHING },
#define CMD_CLEANUP     1002
        { "cleanup",    CMD_CLEANUP,    RTGETOPT_REQ_NOTHING },
#ifdef WITH_ELEVATION
# define OPT_ELEVATED    1090
        { "--elevated", OPT_ELEVATED,   RTGETOPT_REQ_NOTHING },
# define OPT_STDOUT      1091
        { "--stdout",   OPT_STDOUT,     RTGETOPT_REQ_STRING  },
# define OPT_STDERR      1092
        { "--stderr",   OPT_STDERR,     RTGETOPT_REQ_STRING  },
#endif
    };
    RTGETOPTSTATE GetState;
    rc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTGetOptInit failed: %Rrc\n", rc);
    for (;;)
    {
        RTGETOPTUNION ValueUnion;
        int ch = RTGetOpt(&GetState, &ValueUnion);
        switch (ch)
        {
            case 0:
                return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No command specified");

            case CMD_INSTALL:
            case CMD_UNINSTALL:
            case CMD_CLEANUP:
            {
#ifdef WITH_ELEVATION
                if (!fElevated)
                    return RelaunchElevated(argc, argv);
#endif
                int         cCmdargs     = argc - GetState.iNext;
                char      **papszCmdArgs = argv + GetState.iNext;
                switch (ch)
                {
                    case CMD_INSTALL:
                        rcExit = DoInstall(  cCmdargs, papszCmdArgs);
                        break;
                    case CMD_UNINSTALL:
                        rcExit = DoUninstall(cCmdargs, papszCmdArgs);
                        break;
                    case CMD_CLEANUP:
                        rcExit = DoCleanup(  cCmdargs, papszCmdArgs);
                        break;
                    default:
                        AssertReleaseFailedReturn(RTEXITCODE_FAILURE);
                }

                /*
                 * Standard error should end with rcExit=RTEXITCODE_SUCCESS on
                 * success since the exit code may otherwise get lost in the
                 * process elevation fun.
                 */
                RTStrmFlush(g_pStdOut);
                RTStrmFlush(g_pStdErr);
                switch (rcExit)
                {
                    case RTEXITCODE_SUCCESS:
                        RTStrmPrintf(g_pStdErr, "rcExit=RTEXITCODE_SUCCESS\n");
                        break;
                    default:
                        RTStrmPrintf(g_pStdErr, "rcExit=%d\n", rcExit);
                        break;
                }
                RTStrmFlush(g_pStdErr);
                RTStrmFlush(g_pStdOut);
                return rcExit;
            }

#ifdef WITH_ELEVATION
            case OPT_ELEVATED:
                fElevated = true;
                break;

            case OPT_STDERR:
            case OPT_STDOUT:
            {
                FILE *pFile = freopen(ValueUnion.psz, "r+", ch == OPT_STDOUT ? stdout : stderr);
                if (!pFile)
                {
                    rc = RTErrConvertFromErrno(errno);
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "freopen on '%s': %Rrc", ValueUnion.psz, rc);
                }
                break;
            }
#endif

            case 'h':
            case 'V':
                return DoStandardOption(ch);

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
        /* not currently reached */
    }
    /* not reached */
}


#ifdef RT_OS_WINDOWS
extern "C" int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    NOREF(hPrevInstance); NOREF(nShowCmd); NOREF(lpCmdLine); NOREF(hInstance);
    return main(__argc, __argv);
}
#endif

