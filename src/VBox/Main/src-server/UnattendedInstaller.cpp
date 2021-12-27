/* $Id$ */
/** @file
 * UnattendedInstaller class and it's descendants implementation
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MAIN_UNATTENDED
#include "LoggingNew.h"
#include "VirtualBoxBase.h"
#include "VirtualBoxErrorInfoImpl.h"
#include "AutoCaller.h"
#include <VBox/com/ErrorInfo.h>

#include "UnattendedImpl.h"
#include "UnattendedInstaller.h"
#include "UnattendedScript.h"

#include <VBox/err.h>
#include <iprt/ctype.h>
#include <iprt/fsisomaker.h>
#include <iprt/fsvfs.h>
#include <iprt/getopt.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/vfs.h>
#ifdef RT_OS_SOLARIS
# undef ES /* Workaround for someone dragging the namespace pollutor sys/regset.h. Sigh. */
#endif
#include <iprt/formats/iso9660.h>
#include <iprt/formats/fat.h>
#include <iprt/cpp/path.h>


using namespace std;


/* static */ UnattendedInstaller *UnattendedInstaller::createInstance(VBOXOSTYPE enmOsType,
                                                                      const Utf8Str &strGuestOsType,
                                                                      const Utf8Str &strDetectedOSVersion,
                                                                      const Utf8Str &strDetectedOSFlavor,
                                                                      const Utf8Str &strDetectedOSHints,
                                                                      Unattended *pParent)
{
    UnattendedInstaller *pUinstaller = NULL;

    if (strGuestOsType.find("Windows") != RTCString::npos)
    {
        if (enmOsType >= VBOXOSTYPE_WinVista)
            pUinstaller = new UnattendedWindowsXmlInstaller(pParent);
        else
            pUinstaller = new UnattendedWindowsSifInstaller(pParent);
    }
    else if (enmOsType >= VBOXOSTYPE_OS2 && enmOsType < VBOXOSTYPE_Linux)
        pUinstaller = new UnattendedOs2Installer(pParent, strDetectedOSHints);
    else
    {
        if (enmOsType == VBOXOSTYPE_Debian || enmOsType == VBOXOSTYPE_Debian_x64)
            pUinstaller = new UnattendedDebianInstaller(pParent);
        else if (enmOsType >= VBOXOSTYPE_Ubuntu && enmOsType <= VBOXOSTYPE_Ubuntu_x64)
            pUinstaller = new UnattendedUbuntuInstaller(pParent);
        else if (enmOsType >= VBOXOSTYPE_RedHat && enmOsType <= VBOXOSTYPE_RedHat_x64)
        {
            if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "8") >= 0)
                pUinstaller = new UnattendedRhel8Installer(pParent);
            else if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "7") >= 0)
                pUinstaller = new UnattendedRhel7Installer(pParent);
            else if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "6") >= 0)
                pUinstaller = new UnattendedRhel6Installer(pParent);
            else if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "5") >= 0)
                pUinstaller = new UnattendedRhel5Installer(pParent);
            else if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "4") >= 0)
                pUinstaller = new UnattendedRhel4Installer(pParent);
            else if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "3") >= 0)
                pUinstaller = new UnattendedRhel3Installer(pParent);
            else
                pUinstaller = new UnattendedRhel6Installer(pParent);
        }
        else if (enmOsType >= VBOXOSTYPE_FedoraCore && enmOsType <= VBOXOSTYPE_FedoraCore_x64)
            pUinstaller = new UnattendedFedoraInstaller(pParent);
        else if (enmOsType >= VBOXOSTYPE_Oracle && enmOsType <= VBOXOSTYPE_Oracle_x64)
        {
            if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "8") >= 0)
                pUinstaller = new UnattendedOracleLinux8Installer(pParent);
            else if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "7") >= 0)
                pUinstaller = new UnattendedOracleLinux7Installer(pParent);
            else if (RTStrVersionCompare(strDetectedOSVersion.c_str(), "6") >= 0)
                pUinstaller = new UnattendedOracleLinux6Installer(pParent);
            else
                pUinstaller = new UnattendedOracleLinux6Installer(pParent);
        }
#if 0 /* doesn't work, so convert later. */
        else if (enmOsType == VBOXOSTYPE_OpenSUSE || enmOsType == VBOXOSTYPE_OpenSUSE_x64)
            pUinstaller = new UnattendedSuseInstaller(new UnattendedSUSEXMLScript(pParent), pParent);
#endif
    }
    RT_NOREF_PV(strDetectedOSFlavor);
    RT_NOREF_PV(strDetectedOSHints);
    return pUinstaller;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*
*
*  Implementation Unattended functions
*
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 *
 * UnattendedInstaller public methods
 *
 */
UnattendedInstaller::UnattendedInstaller(Unattended *pParent,
                                         const char *pszMainScriptTemplateName, const char *pszPostScriptTemplateName,
                                         const char *pszMainScriptFilename,     const char *pszPostScriptFilename,
                                         DeviceType_T enmBootDevice  /*= DeviceType_DVD */)
    : mMainScript(pParent, pszMainScriptTemplateName, pszMainScriptFilename)
    , mPostScript(pParent, pszPostScriptTemplateName, pszPostScriptFilename)
    , mpParent(pParent)
    , meBootDevice(enmBootDevice)
{
    AssertPtr(pParent);
    Assert(*pszMainScriptTemplateName);
    Assert(*pszMainScriptFilename);
    Assert(*pszPostScriptTemplateName);
    Assert(*pszPostScriptFilename);
    Assert(enmBootDevice == DeviceType_DVD || enmBootDevice == DeviceType_Floppy);
}

UnattendedInstaller::~UnattendedInstaller()
{
    mpParent = NULL;
}

HRESULT UnattendedInstaller::initInstaller()
{
    /*
     * Calculate the full main script template location.
     */
    if (mpParent->i_getScriptTemplatePath().isNotEmpty())
        mStrMainScriptTemplate = mpParent->i_getScriptTemplatePath();
    else
    {
        int vrc = RTPathAppPrivateNoArchCxx(mStrMainScriptTemplate);
        if (RT_SUCCESS(vrc))
            vrc = RTPathAppendCxx(mStrMainScriptTemplate, "UnattendedTemplates");
        if (RT_SUCCESS(vrc))
            vrc = RTPathAppendCxx(mStrMainScriptTemplate, mMainScript.getDefaultTemplateFilename());
        if (RT_FAILURE(vrc))
            return mpParent->setErrorBoth(E_FAIL, vrc,
                                          tr("Failed to construct path to the unattended installer script templates (%Rrc)"),
                                          vrc);
    }

    /*
     * Calculate the full post script template location.
     */
    if (mpParent->i_getPostInstallScriptTemplatePath().isNotEmpty())
        mStrPostScriptTemplate = mpParent->i_getPostInstallScriptTemplatePath();
    else
    {
        int vrc = RTPathAppPrivateNoArchCxx(mStrPostScriptTemplate);
        if (RT_SUCCESS(vrc))
            vrc = RTPathAppendCxx(mStrPostScriptTemplate, "UnattendedTemplates");
        if (RT_SUCCESS(vrc))
            vrc = RTPathAppendCxx(mStrPostScriptTemplate, mPostScript.getDefaultTemplateFilename());
        if (RT_FAILURE(vrc))
            return mpParent->setErrorBoth(E_FAIL, vrc,
                                          tr("Failed to construct path to the unattended installer script templates (%Rrc)"),
                                          vrc);
    }

    /*
     * Construct paths we need.
     */
    if (isAuxiliaryFloppyNeeded())
    {
        mStrAuxiliaryFloppyFilePath = mpParent->i_getAuxiliaryBasePath();
        mStrAuxiliaryFloppyFilePath.append("aux-floppy.img");
    }
    if (isAuxiliaryIsoNeeded())
    {
        mStrAuxiliaryIsoFilePath = mpParent->i_getAuxiliaryBasePath();
        if (!isAuxiliaryIsoIsVISO())
            mStrAuxiliaryIsoFilePath.append("aux-iso.iso");
        else
            mStrAuxiliaryIsoFilePath.append("aux-iso.viso");
    }

    /*
     * Check that we've got the minimum of data available.
     */
    if (mpParent->i_getIsoPath().isEmpty())
        return mpParent->setError(E_INVALIDARG, tr("Cannot proceed with an empty installation ISO path"));
    if (mpParent->i_getUser().isEmpty())
        return mpParent->setError(E_INVALIDARG, tr("Empty user name is not allowed"));
    if (mpParent->i_getPassword().isEmpty())
        return mpParent->setError(E_INVALIDARG, tr("Empty password is not allowed"));

    LogRelFunc(("UnattendedInstaller::savePassedData(): \n"));
    return S_OK;
}

#if 0  /* Always in AUX ISO */
bool UnattendedInstaller::isAdditionsIsoNeeded() const
{
    /* In the VISO case, we'll add the additions to the VISO in a subdir. */
    return !isAuxiliaryIsoIsVISO() && mpParent->i_getInstallGuestAdditions();
}

bool UnattendedInstaller::isValidationKitIsoNeeded() const
{
    /* In the VISO case, we'll add the validation kit to the VISO in a subdir. */
    return !isAuxiliaryIsoIsVISO() && mpParent->i_getInstallTestExecService();
}
#endif

bool UnattendedInstaller::isAuxiliaryIsoNeeded() const
{
    /* In the VISO case we use the AUX ISO for GAs and TXS. */
    return isAuxiliaryIsoIsVISO()
        && (   mpParent->i_getInstallGuestAdditions()
            || mpParent->i_getInstallTestExecService());
}


HRESULT UnattendedInstaller::prepareUnattendedScripts()
{
    LogFlow(("UnattendedInstaller::prepareUnattendedScripts()\n"));

    /*
     * The script template editor calls setError, so status codes just needs to
     * be passed on to the caller.  Do the same for both scripts.
     */
    HRESULT hrc = mMainScript.read(getTemplateFilePath());
    if (SUCCEEDED(hrc))
    {
        hrc = mMainScript.parse();
        if (SUCCEEDED(hrc))
        {
            /* Ditto for the post script. */
            hrc = mPostScript.read(getPostTemplateFilePath());
            if (SUCCEEDED(hrc))
            {
                hrc = mPostScript.parse();
                if (SUCCEEDED(hrc))
                {
                    LogFlow(("UnattendedInstaller::prepareUnattendedScripts: returns S_OK\n"));
                    return S_OK;
                }
                LogFlow(("UnattendedInstaller::prepareUnattendedScripts: parse failed on post script (%Rhrc)\n", hrc));
            }
            else
                LogFlow(("UnattendedInstaller::prepareUnattendedScripts: error reading post install script template file (%Rhrc)\n", hrc));
        }
        else
            LogFlow(("UnattendedInstaller::prepareUnattendedScripts: parse failed (%Rhrc)\n", hrc));
    }
    else
        LogFlow(("UnattendedInstaller::prepareUnattendedScripts: error reading installation script template file (%Rhrc)\n", hrc));
    return hrc;
}

HRESULT UnattendedInstaller::prepareMedia(bool fOverwrite /*=true*/)
{
    LogRelFlow(("UnattendedInstaller::prepareMedia:\n"));
    HRESULT hrc = S_OK;
    if (isAuxiliaryFloppyNeeded())
        hrc = prepareAuxFloppyImage(fOverwrite);
    if (SUCCEEDED(hrc))
    {
        if (isAuxiliaryIsoNeeded())
        {
            hrc = prepareAuxIsoImage(fOverwrite);
            if (FAILED(hrc))
            {
                LogRelFlow(("UnattendedInstaller::prepareMedia: prepareAuxIsoImage failed\n"));

                /* Delete the floppy image if we created one.  */
                if (isAuxiliaryFloppyNeeded())
                    RTFileDelete(getAuxiliaryFloppyFilePath().c_str());
            }
        }
    }
    LogRelFlow(("UnattendedInstaller::prepareMedia: returns %Rrc\n", hrc));
    return hrc;
}

/*
 *
 * UnattendedInstaller protected methods
 *
 */
HRESULT UnattendedInstaller::prepareAuxFloppyImage(bool fOverwrite)
{
    Assert(isAuxiliaryFloppyNeeded());

    /*
     * Create the image.
     */
    RTVFSFILE hVfsFile;
    HRESULT hrc = newAuxFloppyImage(getAuxiliaryFloppyFilePath().c_str(), fOverwrite, &hVfsFile);
    if (SUCCEEDED(hrc))
    {
        /*
         * Open the FAT file system so we can copy files onto the floppy.
         */
        RTERRINFOSTATIC ErrInfo;
        RTVFS           hVfs;
        int vrc = RTFsFatVolOpen(hVfsFile, false /*fReadOnly*/,  0 /*offBootSector*/, &hVfs, RTErrInfoInitStatic(&ErrInfo));
        RTVfsFileRelease(hVfsFile);
        if (RT_SUCCESS(vrc))
        {
            /*
             * Call overridable method to copies the files onto it.
             */
            hrc = copyFilesToAuxFloppyImage(hVfs);

            /*
             * Release the VFS.  On failure, delete the floppy image so the operation can
             * be repeated in non-overwrite mode and so that we don't leave any mess behind.
             */
            RTVfsRelease(hVfs);
        }
        else if (RTErrInfoIsSet(&ErrInfo.Core))
            hrc = mpParent->setErrorBoth(E_FAIL, vrc,
                                         tr("Failed to open FAT file system on newly created floppy image '%s': %Rrc: %s"),
                                         getAuxiliaryFloppyFilePath().c_str(), vrc, ErrInfo.Core.pszMsg);
        else
            hrc = mpParent->setErrorBoth(E_FAIL, vrc,
                                         tr("Failed to open FAT file system onnewly created floppy image '%s': %Rrc"),
                                         getAuxiliaryFloppyFilePath().c_str(), vrc);
        if (FAILED(hrc))
            RTFileDelete(getAuxiliaryFloppyFilePath().c_str());
    }
    return hrc;
}

HRESULT UnattendedInstaller::newAuxFloppyImage(const char *pszFilename, bool fOverwrite, PRTVFSFILE phVfsFile)
{
    /*
     * Open the image file.
     */
    HRESULT     hrc;
    RTVFSFILE   hVfsFile;
    uint64_t    fOpen = RTFILE_O_READWRITE | RTFILE_O_DENY_ALL | (0660 << RTFILE_O_CREATE_MODE_SHIFT);
    if (fOverwrite)
        fOpen |= RTFILE_O_CREATE_REPLACE;
    else
        fOpen |= RTFILE_O_OPEN;
    int vrc = RTVfsFileOpenNormal(pszFilename, fOpen, &hVfsFile);
    if (RT_SUCCESS(vrc))
    {
        /*
         * Format it.
         */
        vrc = RTFsFatVolFormat144(hVfsFile, false /*fQuick*/);
        if (RT_SUCCESS(vrc))
        {
            *phVfsFile = hVfsFile;
            LogRelFlow(("UnattendedInstaller::newAuxFloppyImage: created and formatted  '%s'\n", pszFilename));
            return S_OK;
        }

        hrc = mpParent->setErrorBoth(E_FAIL, vrc, tr("Failed to format floppy image '%s': %Rrc"), pszFilename, vrc);
        RTVfsFileRelease(hVfsFile);
        RTFileDelete(pszFilename);
    }
    else
        hrc = mpParent->setErrorBoth(E_FAIL, vrc, tr("Failed to create floppy image '%s': %Rrc"), pszFilename, vrc);
    return hrc;
}

HRESULT UnattendedInstaller::copyFilesToAuxFloppyImage(RTVFS hVfs)
{
    HRESULT hrc = addScriptToFloppyImage(&mMainScript, hVfs);
    if (SUCCEEDED(hrc))
        hrc = addScriptToFloppyImage(&mPostScript, hVfs);
    return hrc;
}

HRESULT UnattendedInstaller::addScriptToFloppyImage(BaseTextScript *pEditor, RTVFS hVfs)
{
    /*
     * Open the destination file.
     */
    HRESULT   hrc;
    RTVFSFILE hVfsFileDst;
    int vrc = RTVfsFileOpen(hVfs, pEditor->getDefaultFilename(),
                            RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_ALL
                            | (0660 << RTFILE_O_CREATE_MODE_SHIFT),
                            &hVfsFileDst);
    if (RT_SUCCESS(vrc))
    {
        /*
         * Save the content to a string.
         */
        Utf8Str strScript;
        hrc = pEditor->saveToString(strScript);
        if (SUCCEEDED(hrc))
        {
            /*
             * Write the string.
             */
            vrc = RTVfsFileWrite(hVfsFileDst, strScript.c_str(), strScript.length(), NULL);
            if (RT_SUCCESS(vrc))
                hrc = S_OK; /* done */
            else
                hrc = mpParent->setErrorBoth(E_FAIL, vrc,
                                             tr("Error writing %zu bytes to '%s' in floppy image '%s': %Rrc",
                                                "", strScript.length()),
                                             strScript.length(), pEditor->getDefaultFilename(),
                                             getAuxiliaryFloppyFilePath().c_str());
        }
        RTVfsFileRelease(hVfsFileDst);
    }
    else
        hrc = mpParent->setErrorBoth(E_FAIL, vrc,
                                     tr("Error creating '%s' in floppy image '%s': %Rrc"),
                                     pEditor->getDefaultFilename(), getAuxiliaryFloppyFilePath().c_str());
    return hrc;
}

HRESULT UnattendedInstaller::addFileToFloppyImage(RTVFS hVfs, const char *pszSrc, const char *pszDst)
{
    HRESULT hrc;

    /*
     * Open the source file.
     */
    RTVFSIOSTREAM hVfsIosSrc;
    int vrc = RTVfsIoStrmOpenNormal(pszSrc, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE, &hVfsIosSrc);
    if (RT_SUCCESS(vrc))
    {
        /*
         * Open the destination file.
         */
        RTVFSFILE hVfsFileDst;
        vrc = RTVfsFileOpen(hVfs, pszDst,
                            RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_ALL | (0660 << RTFILE_O_CREATE_MODE_SHIFT),
                            &hVfsFileDst);
        if (RT_SUCCESS(vrc))
        {
            /*
             * Do the copying.
             */
            RTVFSIOSTREAM hVfsIosDst = RTVfsFileToIoStream(hVfsFileDst);
            vrc = RTVfsUtilPumpIoStreams(hVfsIosSrc, hVfsIosDst, 0);
            if (RT_SUCCESS(vrc))
                hrc = S_OK;
            else
                hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error writing copying '%s' to floppy image '%s': %Rrc"),
                                             pszSrc, getAuxiliaryFloppyFilePath().c_str(), vrc);
            RTVfsIoStrmRelease(hVfsIosDst);
            RTVfsFileRelease(hVfsFileDst);
        }
        else
            hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error opening '%s' on floppy image '%s' for writing: %Rrc"),
                                         pszDst, getAuxiliaryFloppyFilePath().c_str(), vrc);

        RTVfsIoStrmRelease(hVfsIosSrc);
    }
    else
        hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error opening '%s' for copying onto floppy image '%s': %Rrc"),
                                     pszSrc, getAuxiliaryFloppyFilePath().c_str(), vrc);
    return hrc;
}

HRESULT UnattendedInstaller::prepareAuxIsoImage(bool fOverwrite)
{
    /*
     * Open the original installation ISO.
     */
    RTVFS   hVfsOrgIso;
    HRESULT hrc = openInstallIsoImage(&hVfsOrgIso);
    if (SUCCEEDED(hrc))
    {
        /*
         * The next steps depends on the kind of image we're making.
         */
        if (!isAuxiliaryIsoIsVISO())
        {
            RTFSISOMAKER hIsoMaker;
            hrc = newAuxIsoImageMaker(&hIsoMaker);
            if (SUCCEEDED(hrc))
            {
                hrc = addFilesToAuxIsoImageMaker(hIsoMaker, hVfsOrgIso);
                if (SUCCEEDED(hrc))
                    hrc = finalizeAuxIsoImage(hIsoMaker, getAuxiliaryIsoFilePath().c_str(), fOverwrite);
                RTFsIsoMakerRelease(hIsoMaker);
            }
        }
        else
        {
            RTCList<RTCString> vecFiles(0);
            RTCList<RTCString> vecArgs(0);
            try
            {
                vecArgs.append() = "--iprt-iso-maker-file-marker-bourne-sh";
                RTUUID Uuid;
                int vrc = RTUuidCreate(&Uuid); AssertRC(vrc);
                char szTmp[RTUUID_STR_LENGTH + 1];
                vrc = RTUuidToStr(&Uuid, szTmp, sizeof(szTmp)); AssertRC(vrc);
                vecArgs.append() = szTmp;
                vecArgs.append() = "--file-mode=0444";
                vecArgs.append() = "--dir-mode=0555";
            }
            catch (std::bad_alloc &)
            {
                hrc = E_OUTOFMEMORY;
            }
            if (SUCCEEDED(hrc))
            {
                hrc = addFilesToAuxVisoVectors(vecArgs, vecFiles, hVfsOrgIso, fOverwrite);
                if (SUCCEEDED(hrc))
                    hrc = finalizeAuxVisoFile(vecArgs, getAuxiliaryIsoFilePath().c_str(), fOverwrite);

                if (FAILED(hrc))
                    for (size_t i = 0; i < vecFiles.size(); i++)
                        RTFileDelete(vecFiles[i].c_str());
            }
        }
        RTVfsRelease(hVfsOrgIso);
    }
    return hrc;
}

HRESULT UnattendedInstaller::openInstallIsoImage(PRTVFS phVfsIso, uint32_t fFlags /*= 0*/)
{
    /* Open the file. */
    const char *pszIsoPath = mpParent->i_getIsoPath().c_str();
    RTVFSFILE hOrgIsoFile;
    int vrc = RTVfsFileOpenNormal(pszIsoPath, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE, &hOrgIsoFile);
    if (RT_FAILURE(vrc))
        return mpParent->setErrorBoth(E_FAIL, vrc, tr("Failed to open ISO image '%s' (%Rrc)"), pszIsoPath, vrc);

    /* Pass the file to the ISO file system interpreter. */
    RTERRINFOSTATIC ErrInfo;
    vrc = RTFsIso9660VolOpen(hOrgIsoFile, fFlags, phVfsIso, RTErrInfoInitStatic(&ErrInfo));
    RTVfsFileRelease(hOrgIsoFile);
    if (RT_SUCCESS(vrc))
        return S_OK;
    if (RTErrInfoIsSet(&ErrInfo.Core))
        return mpParent->setErrorBoth(E_FAIL, vrc, tr("ISO reader fail to open '%s' (%Rrc): %s"),
                                      pszIsoPath, vrc, ErrInfo.Core.pszMsg);
    return mpParent->setErrorBoth(E_FAIL, vrc, tr("ISO reader fail to open '%s' (%Rrc)"), pszIsoPath, vrc);
}

HRESULT UnattendedInstaller::newAuxIsoImageMaker(PRTFSISOMAKER phIsoMaker)
{
    int vrc = RTFsIsoMakerCreate(phIsoMaker);
    if (RT_SUCCESS(vrc))
        return S_OK;
    return mpParent->setErrorBoth(E_FAIL, vrc, tr("RTFsIsoMakerCreate failed (%Rrc)"), vrc);
}

HRESULT UnattendedInstaller::addFilesToAuxIsoImageMaker(RTFSISOMAKER hIsoMaker, RTVFS hVfsOrgIso)
{
    RT_NOREF(hVfsOrgIso);

    /*
     * Add the two scripts to the image with default names.
     */
    HRESULT hrc = addScriptToIsoMaker(&mMainScript, hIsoMaker);
    if (SUCCEEDED(hrc))
        hrc = addScriptToIsoMaker(&mPostScript, hIsoMaker);
    return hrc;
}

HRESULT UnattendedInstaller::addScriptToIsoMaker(BaseTextScript *pEditor, RTFSISOMAKER hIsoMaker,
                                                 const char *pszDstFilename /*= NULL*/)
{
    /*
     * Calc default destination filename if desired.
     */
    RTCString strDstNameBuf;
    if (!pszDstFilename)
    {
        try
        {
            strDstNameBuf = RTPATH_SLASH_STR;
            strDstNameBuf.append(pEditor->getDefaultTemplateFilename());
            pszDstFilename = strDstNameBuf.c_str();
        }
        catch (std::bad_alloc &)
        {
            return E_OUTOFMEMORY;
        }
    }

    /*
     * Create a memory file for the script.
     */
    Utf8Str strScript;
    HRESULT hrc = pEditor->saveToString(strScript);
    if (SUCCEEDED(hrc))
    {
        RTVFSFILE hVfsScriptFile;
        size_t    cchScript = strScript.length();
        int vrc = RTVfsFileFromBuffer(RTFILE_O_READ, strScript.c_str(), strScript.length(), &hVfsScriptFile);
        strScript.setNull();
        if (RT_SUCCESS(vrc))
        {
            /*
             * Add it to the ISO.
             */
            vrc = RTFsIsoMakerAddFileWithVfsFile(hIsoMaker, pszDstFilename, hVfsScriptFile, NULL);
            RTVfsFileRelease(hVfsScriptFile);
            if (RT_SUCCESS(vrc))
                hrc = S_OK;
            else
                hrc = mpParent->setErrorBoth(E_FAIL, vrc,
                                             tr("RTFsIsoMakerAddFileWithVfsFile failed on the script '%s' (%Rrc)"),
                                             pszDstFilename, vrc);
        }
        else
            hrc = mpParent->setErrorBoth(E_FAIL, vrc,
                                         tr("RTVfsFileFromBuffer failed on the %zu byte script '%s' (%Rrc)", "", cchScript),
                                         cchScript, pszDstFilename, vrc);
    }
    return hrc;
}

HRESULT UnattendedInstaller::finalizeAuxIsoImage(RTFSISOMAKER hIsoMaker, const char *pszFilename, bool fOverwrite)
{
    /*
     * Finalize the image.
     */
    int vrc = RTFsIsoMakerFinalize(hIsoMaker);
    if (RT_FAILURE(vrc))
        return mpParent->setErrorBoth(E_FAIL, vrc, tr("RTFsIsoMakerFinalize failed (%Rrc)"), vrc);

    /*
     * Open the destination file.
     */
    uint64_t fOpen = RTFILE_O_WRITE | RTFILE_O_DENY_ALL;
    if (fOverwrite)
        fOpen |= RTFILE_O_CREATE_REPLACE;
    else
        fOpen |= RTFILE_O_CREATE;
    RTVFSFILE hVfsDstFile;
    vrc = RTVfsFileOpenNormal(pszFilename, fOpen, &hVfsDstFile);
    if (RT_FAILURE(vrc))
    {
        if (vrc == VERR_ALREADY_EXISTS)
            return mpParent->setErrorBoth(E_FAIL, vrc, tr("The auxiliary ISO image file '%s' already exists"),
                                          pszFilename);
        return mpParent->setErrorBoth(E_FAIL, vrc, tr("Failed to open the auxiliary ISO image file '%s' for writing (%Rrc)"),
                                      pszFilename, vrc);
    }

    /*
     * Get the source file from the image maker.
     */
    HRESULT   hrc;
    RTVFSFILE hVfsSrcFile;
    vrc = RTFsIsoMakerCreateVfsOutputFile(hIsoMaker, &hVfsSrcFile);
    if (RT_SUCCESS(vrc))
    {
        RTVFSIOSTREAM hVfsSrcIso = RTVfsFileToIoStream(hVfsSrcFile);
        RTVFSIOSTREAM hVfsDstIso = RTVfsFileToIoStream(hVfsDstFile);
        if (   hVfsSrcIso != NIL_RTVFSIOSTREAM
            && hVfsDstIso != NIL_RTVFSIOSTREAM)
        {
            vrc = RTVfsUtilPumpIoStreams(hVfsSrcIso, hVfsDstIso, 0 /*cbBufHint*/);
            if (RT_SUCCESS(vrc))
                hrc = S_OK;
            else
                hrc = mpParent->setErrorBoth(E_FAIL, vrc, tr("Error writing auxiliary ISO image '%s' (%Rrc)"),
                                             pszFilename, vrc);
        }
        else
            hrc = mpParent->setErrorBoth(E_FAIL, VERR_INTERNAL_ERROR_2,
                                         tr("Internal Error: Failed to case VFS file to VFS I/O stream"));
        RTVfsIoStrmRelease(hVfsSrcIso);
        RTVfsIoStrmRelease(hVfsDstIso);
    }
    else
        hrc = mpParent->setErrorBoth(E_FAIL, vrc, tr("RTFsIsoMakerCreateVfsOutputFile failed (%Rrc)"), vrc);
    RTVfsFileRelease(hVfsSrcFile);
    RTVfsFileRelease(hVfsDstFile);
    if (FAILED(hrc))
        RTFileDelete(pszFilename);
    return hrc;
}

HRESULT UnattendedInstaller::addFilesToAuxVisoVectors(RTCList<RTCString> &rVecArgs, RTCList<RTCString> &rVecFiles,
                                                      RTVFS hVfsOrgIso, bool fOverwrite)
{
    RT_NOREF(hVfsOrgIso);

    /*
     * Save and add the scripts.
     */
    HRESULT hrc = addScriptToVisoVectors(&mMainScript, rVecArgs, rVecFiles, fOverwrite);
    if (SUCCEEDED(hrc))
        hrc = addScriptToVisoVectors(&mPostScript, rVecArgs, rVecFiles, fOverwrite);
    if (SUCCEEDED(hrc))
    {
        try
        {
            /*
             * If we've got a Guest Additions ISO, add its content to a /vboxadditions dir.
             */
            if (mpParent->i_getInstallGuestAdditions())
            {
                rVecArgs.append().append("--push-iso=").append(mpParent->i_getAdditionsIsoPath());
                rVecArgs.append() = "/vboxadditions=/";
                rVecArgs.append() = "--pop";
            }

            /*
             * If we've got a Validation Kit ISO, add its content to a /vboxvalidationkit dir.
             */
            if (mpParent->i_getInstallTestExecService())
            {
                rVecArgs.append().append("--push-iso=").append(mpParent->i_getValidationKitIsoPath());
                rVecArgs.append() = "/vboxvalidationkit=/";
                rVecArgs.append() = "--pop";
            }
        }
        catch (std::bad_alloc &)
        {
            hrc = E_OUTOFMEMORY;
        }
    }
    return hrc;
}

HRESULT UnattendedInstaller::addScriptToVisoVectors(BaseTextScript *pEditor, RTCList<RTCString> &rVecArgs,
                                                    RTCList<RTCString> &rVecFiles, bool fOverwrite)
{
    /*
     * Calc the aux script file name.
     */
    RTCString strScriptName;
    try
    {
        strScriptName = mpParent->i_getAuxiliaryBasePath();
        strScriptName.append(pEditor->getDefaultFilename());
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    /*
     * Save it.
     */
    HRESULT hrc = pEditor->save(strScriptName.c_str(), fOverwrite);
    if (SUCCEEDED(hrc))
    {
        /*
         * Add it to the vectors.
         */
        try
        {
            rVecArgs.append().append('/').append(pEditor->getDefaultFilename()).append('=').append(strScriptName);
            rVecFiles.append(strScriptName);
        }
        catch (std::bad_alloc &)
        {
            RTFileDelete(strScriptName.c_str());
            hrc = E_OUTOFMEMORY;
        }
    }
    return hrc;
}

HRESULT UnattendedInstaller::finalizeAuxVisoFile(RTCList<RTCString> const &rVecArgs, const char *pszFilename, bool fOverwrite)
{
    /*
     * Create a C-style argument vector and turn that into a command line string.
     */
    size_t const cArgs     = rVecArgs.size();
    const char **papszArgs = (const char **)RTMemTmpAlloc((cArgs + 1) * sizeof(const char *));
    if (!papszArgs)
        return E_OUTOFMEMORY;
    for (size_t i = 0; i < cArgs; i++)
        papszArgs[i] = rVecArgs[i].c_str();
    papszArgs[cArgs] = NULL;

    char *pszCmdLine;
    int vrc = RTGetOptArgvToString(&pszCmdLine, papszArgs, RTGETOPTARGV_CNV_QUOTE_BOURNE_SH);
    RTMemTmpFree(papszArgs);
    if (RT_FAILURE(vrc))
        return mpParent->setErrorBoth(E_FAIL, vrc, tr("RTGetOptArgvToString failed (%Rrc)"), vrc);

    /*
     * Open the file.
     */
    HRESULT  hrc;
    uint64_t fOpen = RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_DENY_READ;
    if (fOverwrite)
        fOpen |= RTFILE_O_CREATE_REPLACE;
    else
        fOpen |= RTFILE_O_CREATE;
    RTFILE hFile;
    vrc = RTFileOpen(&hFile, pszFilename, fOpen);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTFileWrite(hFile, pszCmdLine, strlen(pszCmdLine), NULL);
        if (RT_SUCCESS(vrc))
            vrc = RTFileClose(hFile);
        else
            RTFileClose(hFile);
        if (RT_SUCCESS(vrc))
            hrc = S_OK;
        else
            hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error writing '%s' (%Rrc)"), pszFilename, vrc);
    }
    else
        hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to create '%s' (%Rrc)"), pszFilename, vrc);

    RTStrFree(pszCmdLine);
    return hrc;
}

HRESULT UnattendedInstaller::loadAndParseFileFromIso(RTVFS hVfsOrgIso, const char *pszFilename, AbstractScript *pEditor)
{
    HRESULT   hrc;
    RTVFSFILE hVfsFile;
    int vrc = RTVfsFileOpen(hVfsOrgIso, pszFilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE, &hVfsFile);
    if (RT_SUCCESS(vrc))
    {
        hrc = pEditor->readFromHandle(hVfsFile, pszFilename);
        RTVfsFileRelease(hVfsFile);
        if (SUCCEEDED(hrc))
            hrc = pEditor->parse();
    }
    else
        hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to open '%s' on the ISO '%s' (%Rrc)"),
                                     pszFilename, mpParent->i_getIsoPath().c_str(), vrc);
    return hrc;
}



/*********************************************************************************************************************************
*   Implementation of UnattendedOs2Installer                                                                                     *
*********************************************************************************************************************************/

UnattendedOs2Installer::UnattendedOs2Installer(Unattended *pParent, Utf8Str const &rStrHints)
    : UnattendedInstaller(pParent,
                          "os2_response_files.rsp", "os2_postinstall.cmd",
                          "OS2.RSP",                "VBOXPOST.CMD",
                          DeviceType_Floppy)
{
    Assert(!isOriginalIsoNeeded());
    Assert(isAuxiliaryFloppyNeeded());
    Assert(isAuxiliaryIsoIsVISO());
    Assert(bootFromAuxiliaryIso());
    mStrAuxiliaryInstallDir = "S:\\";

    /* Extract the info from the hints variable: */
    RTCList<RTCString, RTCString *> Pairs = rStrHints.split(" ");
    size_t i = Pairs.size();
    Assert(i > 0);
    while (i -- > 0)
    {
        RTCString const rStr = Pairs[i];
        if (rStr.startsWith("OS2SE20.SRC="))
            mStrOs2Images = rStr.substr(sizeof("OS2SE20.SRC=") - 1);
        else
            AssertMsgFailed(("Unknown hint: %s\n", rStr.c_str()));
    }
}


HRESULT UnattendedOs2Installer::replaceAuxFloppyImageBootSector(RTVFSFILE hVfsFile) RT_NOEXCEPT
{
    /*
     * Find the bootsector.  Because the ArcaOS ISOs doesn't contain any floppy
     * images, we cannot just lift it off one of those.  Instead we'll locate it
     * in the SYSINSTX.COM utility, i.e. the tool which installs it onto floppies
     * and harddisks.  The SYSINSTX.COM utility is a NE executable and we don't
     * have issues with compressed pages like with LX images.
     *
     * The utility seems always to be located on disk 0.
     */
    RTVFS   hVfsOrgIso;
    HRESULT hrc = openInstallIsoImage(&hVfsOrgIso);
    if (SUCCEEDED(hrc))
    {
        char szPath[256];
        int vrc = RTPathJoin(szPath, sizeof(szPath), mStrOs2Images.c_str(), "DISK_0/SYSINSTX.COM");
        if (RT_SUCCESS(vrc))
        {
            RTVFSFILE hVfsSysInstX;
            vrc = RTVfsFileOpen(hVfsOrgIso, szPath, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE, &hVfsSysInstX);
            if (RT_SUCCESS(vrc))
            {
                /*
                 * Scan the image looking for a 512 block ending with a DOS signature
                 * and starting with a three byte jump followed by an OEM name string.
                 */
                uint8_t *pbBootSector = NULL;
                RTFOFF   off          = 0;
                bool     fEof         = false;
                uint8_t  abBuf[_8K]   = {0};
                do
                {
                    /* Read the next chunk. */
                    memmove(abBuf, &abBuf[sizeof(abBuf) - 512], 512); /* Move up the last 512 (all zero the first time around). */
                    size_t cbRead = 0;
                    vrc = RTVfsFileReadAt(hVfsSysInstX, off, &abBuf[512], sizeof(abBuf) - 512, &cbRead);
                    if (RT_FAILURE(vrc))
                        break;
                    fEof = cbRead != sizeof(abBuf) - 512;
                    off += cbRead;

                    /* Scan it. */
                    size_t   cbLeft = sizeof(abBuf);
                    uint8_t *pbCur  = abBuf;
                    while (cbLeft >= 512)
                    {
                        /* Look for the DOS signature (0x55 0xaa) at the end of the sector: */
                        uint8_t *pbHit = (uint8_t *)memchr(pbCur + 510, 0x55, cbLeft - 510 - 1);
                        if (!pbHit)
                            break;
                        if (pbHit[1] == 0xaa)
                        {
                            uint8_t *pbStart = pbHit - 510;
                            if (   pbStart[0] == 0xeb        /* JMP imm8 */
                                && pbStart[1] >= 3 + 8 + sizeof(FATEBPB) - 2 /* must jump after the FATEBPB */
                                && RT_C_IS_ALNUM(pbStart[3]) /* ASSUME OEM string starts with two letters (typically 'IBM x.y')*/
                                && RT_C_IS_ALNUM(pbStart[4]))
                            {
                                FATEBPB *pBpb = (FATEBPB *)&pbStart[3 + 8];
                                if (   pBpb->bExtSignature == FATEBPB_SIGNATURE
                                    && (   memcmp(pBpb->achType, "FAT     ", sizeof(pBpb->achType)) == 0
                                        || memcmp(pBpb->achType, FATEBPB_TYPE_FAT12, sizeof(pBpb->achType)) == 0))
                                {
                                    pbBootSector = pbStart;
                                    break;
                                }
                            }
                        }

                        /* skip */
                        pbCur  = pbHit - 510 + 1;
                        cbLeft = (uintptr_t)&abBuf[sizeof(abBuf)] - (uintptr_t)pbCur;
                    }
                } while (!fEof);

                if (pbBootSector)
                {
                    if (pbBootSector != abBuf)
                        pbBootSector = (uint8_t *)memmove(abBuf, pbBootSector, 512);

                    /*
                     * We've now got a bootsector.  So, we need to copy the EBPB
                     * from the destination image before replacing it.
                     */
                    vrc = RTVfsFileReadAt(hVfsFile, 0, &abBuf[512], 512, NULL);
                    if (RT_SUCCESS(vrc))
                    {
                        memcpy(&pbBootSector[3 + 8], &abBuf[512 + 3 + 8], sizeof(FATEBPB));

                        /*
                         * Write it.
                         */
                        vrc = RTVfsFileWriteAt(hVfsFile, 0, pbBootSector, 512, NULL);
                        if (RT_SUCCESS(vrc))
                        {
                            LogFlowFunc(("Successfully installed new bootsector\n"));
                            hrc = S_OK;
                        }
                        else
                            hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to write bootsector: %Rrc"), vrc);
                    }
                    else
                        hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to read bootsector: %Rrc"), vrc);
                }
                else if (RT_FAILURE(vrc))
                    hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error reading SYSINSTX.COM: %Rrc"), vrc);
                else
                    hrc = mpParent->setErrorBoth(E_FAIL, VERR_NOT_FOUND,
                                                 tr("Unable to locate bootsector template in SYSINSTX.COM"));
                RTVfsFileRelease(hVfsSysInstX);
            }
            else
                hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to open SYSINSTX.COM: %Rrc"), vrc);
        }
        else
            hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to construct SYSINSTX.COM path"));
        RTVfsRelease(hVfsOrgIso);
    }
    return hrc;

}

HRESULT UnattendedOs2Installer::newAuxFloppyImage(const char *pszFilename, bool fOverwrite, PRTVFSFILE phVfsFile)
{
    /*
     * Open the image file.
     */
    HRESULT     hrc;
    RTVFSFILE   hVfsFile;
    uint64_t    fOpen = RTFILE_O_READWRITE | RTFILE_O_DENY_ALL | (0660 << RTFILE_O_CREATE_MODE_SHIFT);
    if (fOverwrite)
        fOpen |= RTFILE_O_CREATE_REPLACE;
    else
        fOpen |= RTFILE_O_OPEN;
    int vrc = RTVfsFileOpenNormal(pszFilename, fOpen, &hVfsFile);
    if (RT_SUCCESS(vrc))
    {
        /*
         * Format it.
         */
        vrc = RTFsFatVolFormat288(hVfsFile, false /*fQuick*/);
        if (RT_SUCCESS(vrc))
        {
            /*
             * Now we install the OS/2 boot sector on it.
             */
            hrc = replaceAuxFloppyImageBootSector(hVfsFile);
            if (SUCCEEDED(hrc))
            {
                *phVfsFile = hVfsFile;
                LogRelFlow(("UnattendedInstaller::newAuxFloppyImage: created and formatted  '%s'\n", pszFilename));
                return S_OK;
            }
        }
        else
            hrc = mpParent->setErrorBoth(E_FAIL, vrc, tr("Failed to format floppy image '%s': %Rrc"), pszFilename, vrc);
        RTVfsFileRelease(hVfsFile);
        RTFileDelete(pszFilename);
    }
    else
        hrc = mpParent->setErrorBoth(E_FAIL, vrc, tr("Failed to create floppy image '%s': %Rrc"), pszFilename, vrc);
    return hrc;
}


HRESULT UnattendedOs2Installer::splitResponseFile() RT_NOEXCEPT
{
    if (mVecSplitFiles.size() == 0)
    {
#if 0
        Utf8Str strResponseFile;
        int vrc = strResponseFile.assignNoThrow(mpParent->i_getAuxiliaryBasePath());
        if (RT_SUCCESS(vrc))
            vrc = strResponseFile.appendNoThrow(mMainScript.getDefaultFilename());
        if (RT_SUCCESS(vrc))
            return splitFile(strResponseFile.c_str(), mVecSplitFiles);
        return mpParent->setErrorVrc(vrc);
#else
        return splitFile(&mMainScript, mVecSplitFiles);
#endif
    }
    return S_OK;
}

HRESULT UnattendedOs2Installer::copyFilesToAuxFloppyImage(RTVFS hVfs)
{
    /*
     * Make sure we've split the files already.
     */
    HRESULT hrc = splitResponseFile();
    if (FAILED(hrc))
        return hrc;

    /*
     * We need to copy over the files needed to boot OS/2.
     */
    static struct
    {
        bool        fMandatory;
        const char *apszNames[2];
        const char *apszDisks[3];
        const char *pszMinVer;
        const char *pszMaxVer;
    } const s_aFiles[] =
    {
        { true, { "OS2BOOT",      NULL          }, { "DISK_0", NULL,     NULL }, "2.1",  NULL }, /* 2.0 did not have OS2BOOT */
        { true, { "OS2LDR",       NULL          }, { "DISK_0", NULL,     NULL }, NULL,   NULL },
        { true, { "OS2LDR.MSG",   NULL          }, { "DISK_0", NULL,     NULL }, NULL,   NULL },
        { true, { "OS2KRNL",      "OS2KRNLI"    }, { "DISK_0", NULL,     NULL }, NULL,   NULL },
        { true, { "OS2DUMP",      NULL          }, { "DISK_0", NULL,     NULL }, NULL,   NULL },
        //
        { true, { "ANSICALL.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "BKSCALLS.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "BMSCALLS.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "BVHINIT.DLL",  NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "BVSCALLS.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "CDFS.IFS",     NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "CLOCK01.SYS",  NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "COUNT437.SYS", "COUNTRY.SYS" }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "DOS.SYS",      NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "DOSCALL1.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "IBM1FLPY.ADD", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "IBM1S506.ADD", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "IBMIDECD.FLT", NULL          }, { "DISK_1", "DISK_2", NULL }, "4.0",  NULL }, /* not in 2.1 & Warp3  */
        { true, { "IBMKBD.SYS", "KBD01.SYS"/*?*/}, { "DISK_1", "DISK_2", NULL }, NULL,   NULL},
        { true, { "ISAPNP.SNP",   NULL          }, { "DISK_1", "DISK_2", NULL }, "4.0",  NULL }, /* not in 2.1 */
        { true, { "KBDBASE.SYS",  NULL          }, { "DISK_1", "DISK_2", NULL }, "3.0",  NULL }, /* not in 2.1 */
        { true, { "KBDCALLS.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "KEYBOARD.DCP", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "MOUCALLS.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "MSG.DLL",      NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "NAMPIPES.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "NLS.DLL",      NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "OS2CDROM.DMD", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "OS2CHAR.DLL",  NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "OS2DASD.DMD",  NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "OS2LVM.DMD",   NULL          }, { "DISK_1", "DISK_2", NULL }, "4.5",  NULL },
        { true, { "OS2VER",       NULL          }, { "DISK_0", NULL,     NULL }, NULL,   NULL },
        { true, { "PNP.SYS",      NULL          }, { "DISK_1", "DISK_2", NULL }, "4.0",  NULL },
        { true, { "QUECALLS.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "RESOURCE.SYS", NULL          }, { "DISK_1", "DISK_2", NULL }, "3.0",  NULL }, /* not in 2.1*/
        { true, { "SCREEN01.SYS", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "SESMGR.DLL",   NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "TESTCFG.SYS",  NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "VIO437.DCP",   "VTBL850.DCP" }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
        { true, { "VIOCALLS.DLL", NULL          }, { "DISK_1", "DISK_2", NULL }, NULL,   NULL },
    };


    RTVFS hVfsOrgIso;
    hrc = openInstallIsoImage(&hVfsOrgIso);
    if (SUCCEEDED(hrc))
    {
        for (size_t i = 0; i < RT_ELEMENTS(s_aFiles); i++)
        {
            bool fCopied = false;
            for (size_t iDisk = 0; iDisk < RT_ELEMENTS(s_aFiles[i].apszDisks) && s_aFiles[i].apszDisks[iDisk] && !fCopied; iDisk++)
            {
                for (size_t iName = 0; iName < RT_ELEMENTS(s_aFiles[i].apszNames) && s_aFiles[i].apszNames[iName]; iName++)
                {
                    char szPath[256];
                    int vrc = RTPathJoin(szPath, sizeof(szPath), mStrOs2Images.c_str(), s_aFiles[i].apszDisks[iDisk]);
                    if (RT_SUCCESS(vrc))
                        vrc = RTPathAppend(szPath, sizeof(szPath), s_aFiles[i].apszNames[iName]);
                    AssertRCBreakStmt(vrc, hrc = mpParent->setErrorBoth(E_FAIL, vrc, tr("RTPathJoin/Append failed for %s: %Rrc"),
                                                                        s_aFiles[i].apszNames[iName], vrc));
                    RTVFSFILE hVfsSrc;
                    vrc = RTVfsFileOpen(hVfsOrgIso, szPath, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE, &hVfsSrc);
                    if (RT_SUCCESS(vrc))
                    {
                        RTVFSFILE hVfsDst;
                        vrc = RTVfsFileOpen(hVfs, s_aFiles[i].apszNames[0],
                                            RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE
                                            | (0755 << RTFILE_O_CREATE_MODE_SHIFT), &hVfsDst);
                        if (RT_SUCCESS(vrc))
                        {
                            RTVFSIOSTREAM hVfsIosSrc = RTVfsFileToIoStream(hVfsSrc);
                            RTVFSIOSTREAM hVfsIosDst = RTVfsFileToIoStream(hVfsDst);
                            vrc = RTVfsUtilPumpIoStreams(hVfsIosSrc, hVfsIosDst, 0);
                            RTVfsIoStrmRelease(hVfsIosDst);
                            RTVfsFileRelease(hVfsDst);
                            RTVfsIoStrmRelease(hVfsIosSrc);
                            if (RT_FAILURE(vrc))
                                hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to write %s to the floppy: %Rrc"),
                                                             s_aFiles[i].apszNames, vrc);
                        }
                        else
                            hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to open %s on floppy: %Rrc"),
                                                         s_aFiles[i].apszNames, vrc);

                        RTVfsFileRelease(hVfsSrc);
                        fCopied = true;
                        break;
                    }
                }
            }
            if (FAILED(hrc))
                break;
            if (!fCopied)
            {
                /** @todo do version filtering.   */
                hrc = mpParent->setErrorBoth(E_FAIL, VERR_FILE_NOT_FOUND,
                                             tr("Failed to locate '%s' needed for the install floppy"), s_aFiles[i].apszNames[0]);
                break;
            }
        }
        RTVfsRelease(hVfsOrgIso);
    }

    /*
     * In addition, we need to add a CONFIG.SYS and the startup script.
     */
    if (SUCCEEDED(hrc))
    {
        Utf8Str strSrc;
        try
        {
            strSrc = mpParent->i_getAuxiliaryBasePath();
            strSrc.append("CONFIG.SYS");
        }
        catch (std::bad_alloc &)
        {
            return E_OUTOFMEMORY;
        }
        hrc = addFileToFloppyImage(hVfs, strSrc.c_str(), "CONFIG.SYS");
    }

    /*
     * We also want a ALTF2ON.$$$ file so we can see which drivers are loaded
     * and where it might get stuck.
     */
    if (SUCCEEDED(hrc))
    {
        RTVFSFILE hVfsFile;
        int vrc = RTVfsFileOpen(hVfs, "ALTF2ON.$$$",
                                RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE
                                | (0755 << RTFILE_O_CREATE_MODE_SHIFT), &hVfsFile);
        if (RT_SUCCESS(vrc))
        {
            /** @todo buggy fat vfs: cannot write empty files */
            RTVfsFileWrite(hVfsFile, RT_STR_TUPLE("\r\n"), NULL);
            RTVfsFileRelease(hVfsFile);
        }
        else
            hrc = mpParent->setErrorBoth(E_FAIL, VERR_FILE_NOT_FOUND, tr("Failed to create 'ALTF2ON.$$$' on the install floppy"));
    }

    return hrc;
}

HRESULT UnattendedOs2Installer::addFilesToAuxVisoVectors(RTCList<RTCString> &rVecArgs, RTCList<RTCString> &rVecFiles,
                                                         RTVFS hVfsOrgIso, bool fOverwrite)
{
    /*
     * Make sure we've split the files already.
     */
    HRESULT hrc = splitResponseFile();
    if (FAILED(hrc))
        return hrc;

    /*
     * Add our stuff to the vectors.
     */
    try
    {
        /* Remaster ISO. */
        rVecArgs.append() = "--no-file-mode";
        rVecArgs.append() = "--no-dir-mode";

        rVecArgs.append() = "--import-iso";
        rVecArgs.append(mpParent->i_getIsoPath());

        rVecArgs.append() = "--file-mode=0444";
        rVecArgs.append() = "--dir-mode=0555";

        /* Add the boot floppy to the ISO: */
        rVecArgs.append() = "--eltorito-new-entry";
        rVecArgs.append() = "--eltorito-add-image";
        rVecArgs.append(mStrAuxiliaryFloppyFilePath);
        rVecArgs.append() = "--eltorito-floppy-288";


        /* Add the response files and postinstall files to the ISO: */
        Utf8Str const &rStrAuxPrefix = mpParent->i_getAuxiliaryBasePath();
        size_t i = mVecSplitFiles.size();
        while (i-- > 0)
        {
            RTCString const &rStrFile = mVecSplitFiles[i];
            rVecArgs.append().assign("VBoxCID/").append(rStrFile).append('=').append(rStrAuxPrefix).append(rStrFile);
        }
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    /*
     * Call parent.
     */
    return UnattendedInstaller::addFilesToAuxVisoVectors(rVecArgs, rVecFiles, hVfsOrgIso, fOverwrite);
}

/**
 * Helper for splitFile.
 */
const char *splitFileLocateSubstring(const char *pszSrc, size_t cchSrc, const char *pszSubstring, size_t cchSubstring)
{
    char const ch0 = *pszSubstring;
    while (cchSrc >= cchSubstring)
    {
        const char *pszHit0 = (const char *)memchr(pszSrc, ch0, cchSrc - cchSubstring + 1);
        if (pszHit0)
        {
            if (memcmp(pszHit0, pszSubstring, cchSubstring) == 0)
                return pszHit0;
        }
        else
            break;
        cchSrc -= (size_t)(pszHit0 - pszSrc) + 1;
        pszSrc  = pszHit0 + 1;
    }
    return NULL;
}

/**
 * Worker for splitFile().
 */
HRESULT UnattendedOs2Installer::splitFileInner(const char *pszFileToSplit, RTCList<RTCString> &rVecSplitFiles,
                                               const char *pszSrc, size_t cbLeft) RT_NOEXCEPT
{
    static const char  s_szPrefix[] = "@@VBOX_SPLITTER_";
    const char * const pszStart     = pszSrc;
    const char * const pszEnd       = &pszSrc[cbLeft];
    while (cbLeft > 0)
    {
        /*
         * Locate the next split start marker (everything before it is ignored).
         */
        const char *pszMarker = splitFileLocateSubstring(pszSrc, cbLeft, s_szPrefix, sizeof(s_szPrefix) - 1);
        if (pszMarker)
            pszMarker += sizeof(s_szPrefix) - 1;
        else
            break;
        if (strncmp(pszMarker, RT_STR_TUPLE("START[")) != 0)
            return mpParent->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                          tr("Unexpected splitter tag in '%s' at offset %p: @@VBOX_SPLITTER_%.64s"),
                                          pszFileToSplit, pszMarker - pszStart, pszMarker);
        pszMarker += sizeof("START[") - 1;
        const char *pszTail = splitFileLocateSubstring(pszMarker, (size_t)(pszEnd - pszMarker), RT_STR_TUPLE("]@@"));
        if (   !pszTail
            || pszTail - pszMarker > 64
            || memchr(pszMarker, '\\', pszTail - pszMarker)
            || memchr(pszMarker, '/', pszTail - pszMarker)
            || memchr(pszMarker, ':', pszTail - pszMarker)
           )
            return mpParent->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                          tr("Malformed splitter tag in '%s' at offset %p: @@VBOX_SPLITTER_START[%.64s"),
                                          pszFileToSplit, (size_t)(pszEnd - pszMarker), pszMarker);
        int vrc = RTStrValidateEncodingEx(pszMarker, pszTail - pszMarker, RTSTR_VALIDATE_ENCODING_EXACT_LENGTH);
        if (RT_FAILURE(vrc))
            return mpParent->setErrorBoth(E_FAIL, vrc,
                                          tr("Malformed splitter tag in '%s' at offset %p: @@VBOX_SPLITTER_START[%.*Rhxs"),
                                          pszFileToSplit, (size_t)(pszEnd - pszMarker), pszTail - pszMarker, pszMarker);
        const char *pszFilename;
        try
        {
            pszFilename = rVecSplitFiles.append().assign(pszMarker, pszTail - pszMarker).c_str();
        }
        catch (std::bad_alloc &)
        {
            return E_OUTOFMEMORY;
        }
        const char *pszDocStart = pszTail + sizeof("]@@") - 1;
        while (RT_C_IS_SPACE(*pszDocStart))
            if (*pszDocStart++ == '\n')
                break;

        /* Advance. */
        pszSrc = pszDocStart;
        cbLeft = pszEnd - pszDocStart;

        /*
         * Locate the matching end marker (there cannot be any other markers inbetween).
         */
        const char * const pszDocEnd = pszMarker = splitFileLocateSubstring(pszSrc, cbLeft, s_szPrefix, sizeof(s_szPrefix) - 1);
        if (pszMarker)
            pszMarker += sizeof(s_szPrefix) - 1;
        else
            return mpParent->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                          tr("No END splitter tag for '%s' in '%s'"), pszFilename, pszFileToSplit);
        if (strncmp(pszMarker, RT_STR_TUPLE("END[")) != 0)
            return mpParent->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                          tr("Unexpected splitter tag in '%s' at offset %p: @@VBOX_SPLITTER_%.64s"),
                                          pszFileToSplit, (size_t)(pszEnd - pszMarker), pszMarker);
        pszMarker += sizeof("END[") - 1;
        size_t const cchFilename = strlen(pszFilename);
        if (   strncmp(pszMarker, pszFilename, cchFilename) != 0
            || pszMarker[cchFilename] != ']'
            || pszMarker[cchFilename + 1] != '@'
            || pszMarker[cchFilename + 2] != '@')
            return mpParent->setErrorBoth(E_FAIL, VERR_PARSE_ERROR,
                                          tr("Mismatching splitter tag for '%s' in '%s' at offset %p: @@VBOX_SPLITTER_END[%.64Rhxs"),
                                          pszFilename, pszFileToSplit, (size_t)(pszEnd - pszMarker), pszMarker);

        /* Advance. */
        pszSrc = pszMarker + cchFilename + sizeof("]@@") - 1;
        cbLeft = (size_t)(pszEnd - pszSrc);

        /*
         * Write out the file.
         */
        Utf8Str strDstFilename;
        vrc = strDstFilename.assignNoThrow(mpParent->i_getAuxiliaryBasePath());
        if (RT_SUCCESS(vrc))
            vrc = strDstFilename.appendNoThrow(pszFilename);
        if (RT_SUCCESS(vrc))
        {
            RTFILE hFile;
            vrc = RTFileOpen(&hFile, strDstFilename.c_str(), RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE);
            if (RT_SUCCESS(vrc))
            {
                vrc = RTFileWrite(hFile, pszDocStart, (size_t)(pszDocEnd - pszDocStart), NULL);
                if (RT_SUCCESS(vrc))
                    vrc = RTFileClose(hFile);
                else
                    RTFileClose(hFile);
                if (RT_FAILURE(vrc))
                    return mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Error writing '%s' (split out from '%s'): %Rrc"),
                                                  strDstFilename.c_str(), pszFileToSplit, vrc);
            }
            else
                return mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc,
                                              tr("File splitter failed to open output file '%s' in '%s': %Rrc (%s)"),
                                              pszFilename, pszFileToSplit, vrc, strDstFilename.c_str());
        }
        else
            return mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc,
                                          tr("File splitter failed to construct path for '%s' in '%s': %Rrc"),
                                          pszFilename, pszFileToSplit, vrc);
    }

    return S_OK;
}

HRESULT UnattendedOs2Installer::splitFile(const char *pszFileToSplit, RTCList<RTCString> &rVecSplitFiles) RT_NOEXCEPT
{
    /*
     * Read the whole source file into memory, making sure it's zero terminated.
     */
    HRESULT hrc;
    void   *pvSrc;
    size_t  cbSrc;
    int vrc = RTFileReadAllEx(pszFileToSplit, 0 /*off*/, _16M /*cbMax*/,
                              RTFILE_RDALL_F_TRAILING_ZERO_BYTE | RTFILE_RDALL_F_FAIL_ON_MAX_SIZE | RTFILE_RDALL_O_DENY_WRITE,
                              &pvSrc, &cbSrc);
    if (RT_SUCCESS(vrc))
    {
        /*
         * Do the actual splitting in a worker function to avoid needing to
         * thing about calling RTFileReadAllFree in error paths.
         */
        hrc = splitFileInner(pszFileToSplit, rVecSplitFiles, (const char *)pvSrc, cbSrc);
        RTFileReadAllFree(pvSrc, cbSrc);
    }
    else
        hrc = mpParent->setErrorBoth(VBOX_E_FILE_ERROR, vrc, tr("Failed to read '%s' for splitting up: %Rrc"),
                                     pszFileToSplit, vrc);
    return hrc;
}

HRESULT UnattendedOs2Installer::splitFile(BaseTextScript *pEditor, RTCList<RTCString> &rVecSplitFiles) RT_NOEXCEPT
{
    /*
     * Get the output from the editor.
     */
    Utf8Str strSrc;
    HRESULT hrc = pEditor->saveToString(strSrc);
    if (SUCCEEDED(hrc))
    {
        /*
         * Do the actual splitting.
         */
        hrc = splitFileInner(pEditor->getDefaultFilename(), rVecSplitFiles, strSrc.c_str(), strSrc.length());
    }
    return hrc;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*
*
*  Implementation UnattendedLinuxInstaller functions
*
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT UnattendedLinuxInstaller::editIsoLinuxCfg(GeneralTextScript *pEditor)
{
    try
    {
        /* Set timeouts to 10 seconds. */
        std::vector<size_t> vecLineNumbers = pEditor->findTemplate("timeout", RTCString::CaseInsensitive);
        for (size_t i = 0; i < vecLineNumbers.size(); ++i)
            if (pEditor->getContentOfLine(vecLineNumbers[i]).startsWithWord("timeout", RTCString::CaseInsensitive))
            {
                HRESULT hrc = pEditor->setContentOfLine(vecLineNumbers.at(i), "timeout 10");
                if (FAILED(hrc))
                    return hrc;
            }

        /* Comment out 'display <filename>' directives that's used for displaying files at boot time. */
        vecLineNumbers = pEditor->findTemplate("display", RTCString::CaseInsensitive);
        for (size_t i = 0; i < vecLineNumbers.size(); ++i)
            if (pEditor->getContentOfLine(vecLineNumbers[i]).startsWithWord("display", RTCString::CaseInsensitive))
            {
                HRESULT hrc = pEditor->prependToLine(vecLineNumbers.at(i), "#");
                if (FAILED(hrc))
                    return hrc;
            }

        /* Modify kernel parameters. */
        vecLineNumbers = pEditor->findTemplate("append", RTCString::CaseInsensitive);
        if (vecLineNumbers.size() > 0)
        {
            Utf8Str const &rStrAppend = mpParent->i_getExtraInstallKernelParameters().isNotEmpty()
                                      ? mpParent->i_getExtraInstallKernelParameters()
                                      : mStrDefaultExtraInstallKernelParameters;

            for (size_t i = 0; i < vecLineNumbers.size(); ++i)
                if (pEditor->getContentOfLine(vecLineNumbers[i]).startsWithWord("append", RTCString::CaseInsensitive))
                {
                    Utf8Str strLine = pEditor->getContentOfLine(vecLineNumbers[i]);

                    /* Do removals. */
                    if (mArrStrRemoveInstallKernelParameters.size() > 0)
                    {
                        size_t offStart = strLine.find("append") + 5;
                        while (offStart < strLine.length() && !RT_C_IS_SPACE(strLine[offStart]))
                            offStart++;
                        while (offStart < strLine.length() && RT_C_IS_SPACE(strLine[offStart]))
                            offStart++;
                        if (offStart < strLine.length())
                        {
                            for (size_t iRemove = 0; iRemove < mArrStrRemoveInstallKernelParameters.size(); iRemove++)
                            {
                                RTCString const &rStrRemove = mArrStrRemoveInstallKernelParameters[iRemove];
                                for (size_t off = offStart; off < strLine.length(); )
                                {
                                    Assert(!RT_C_IS_SPACE(strLine[off]));

                                    /* Find the end of word. */
                                    size_t offEnd = off + 1;
                                    while (offEnd < strLine.length() && !RT_C_IS_SPACE(strLine[offEnd]))
                                        offEnd++;

                                    /* Check if it matches. */
                                    if (RTStrSimplePatternNMatch(rStrRemove.c_str(), rStrRemove.length(),
                                                                 strLine.c_str() + off, offEnd - off))
                                    {
                                        while (off > 0 && RT_C_IS_SPACE(strLine[off - 1]))
                                            off--;
                                        strLine.erase(off, offEnd - off);
                                    }

                                    /* Advance to the next word. */
                                    off = offEnd;
                                    while (off < strLine.length() && RT_C_IS_SPACE(strLine[off]))
                                        off++;
                                }
                            }
                        }
                    }

                    /* Do the appending. */
                    if (rStrAppend.isNotEmpty())
                    {
                        if (!rStrAppend.startsWith(" ") && !strLine.endsWith(" "))
                            strLine.append(' ');
                        strLine.append(rStrAppend);
                    }

                    /* Update line. */
                    HRESULT hrc = pEditor->setContentOfLine(vecLineNumbers.at(i), strLine);
                    if (FAILED(hrc))
                        return hrc;
                }
        }
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*
*
*  Implementation UnattendedDebianInstaller functions
*
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Helper for checking if a file exists.
 * @todo promote to IPRT?
 */
static bool hlpVfsFileExists(RTVFS hVfs, const char *pszPath)
{
    RTFSOBJINFO ObjInfo;
    int vrc = RTVfsQueryPathInfo(hVfs, pszPath, &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_FOLLOW_LINK);
    return RT_SUCCESS(vrc) && RTFS_IS_FILE(ObjInfo.Attr.fMode);
}

HRESULT UnattendedDebianInstaller::addFilesToAuxVisoVectors(RTCList<RTCString> &rVecArgs, RTCList<RTCString> &rVecFiles,
                                                            RTVFS hVfsOrgIso, bool fOverwrite)
{
    /*
     * The txt.cfg file used to be called isolinux.txt (ubuntu 4.10
     * and possible others).
     */
    /** @todo Ubuntu 4.10 does not work, as we generate too long command lines
     *        and the kernel crashes immediately. */
    const char *pszIsoLinuxTxtCfg = "/isolinux/txt.cfg";
    if (   !hlpVfsFileExists(hVfsOrgIso, pszIsoLinuxTxtCfg)
        && hlpVfsFileExists(hVfsOrgIso, "/isolinux/isolinux.txt"))
        pszIsoLinuxTxtCfg             = "/isolinux/isolinux.txt";

    /*
     * VISO bits and filenames.
     */
    RTCString strIsoLinuxCfg;
    RTCString strTxtCfg;
    try
    {
        /* Remaster ISO. */
        rVecArgs.append() = "--no-file-mode";
        rVecArgs.append() = "--no-dir-mode";

        rVecArgs.append() = "--import-iso";
        rVecArgs.append(mpParent->i_getIsoPath());

        rVecArgs.append() = "--file-mode=0444";
        rVecArgs.append() = "--dir-mode=0555";

        /* Remove the two isolinux configure files we'll be replacing. */
        rVecArgs.append() = "isolinux/isolinux.cfg=:must-remove:";
        rVecArgs.append().assign(&pszIsoLinuxTxtCfg[1]).append("=:must-remove:");

        /* Add the replacement files. */
        strIsoLinuxCfg = mpParent->i_getAuxiliaryBasePath();
        strIsoLinuxCfg.append("isolinux-isolinux.cfg");
        rVecArgs.append().append("isolinux/isolinux.cfg=").append(strIsoLinuxCfg);

        strTxtCfg = mpParent->i_getAuxiliaryBasePath();
        strTxtCfg.append("isolinux-txt.cfg");
        rVecArgs.append().assign(&pszIsoLinuxTxtCfg[1]).append("=").append(strTxtCfg);
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    /*
     * Edit the isolinux.cfg file.
     */
    {
        GeneralTextScript Editor(mpParent);
        HRESULT hrc = loadAndParseFileFromIso(hVfsOrgIso, "/isolinux/isolinux.cfg", &Editor);
        if (SUCCEEDED(hrc))
            hrc = editIsoLinuxCfg(&Editor);
        if (SUCCEEDED(hrc))
        {
            hrc = Editor.save(strIsoLinuxCfg, fOverwrite);
            if (SUCCEEDED(hrc))
            {
                try
                {
                    rVecFiles.append(strIsoLinuxCfg);
                }
                catch (std::bad_alloc &)
                {
                    RTFileDelete(strIsoLinuxCfg.c_str());
                    hrc = E_OUTOFMEMORY;
                }
            }
        }
        if (FAILED(hrc))
            return hrc;
    }

    /*
     * Edit the txt.cfg file.
     */
    {
        GeneralTextScript Editor(mpParent);
        HRESULT hrc = loadAndParseFileFromIso(hVfsOrgIso, pszIsoLinuxTxtCfg, &Editor);
        if (SUCCEEDED(hrc))
            hrc = editDebianTxtCfg(&Editor);
        if (SUCCEEDED(hrc))
        {
            hrc = Editor.save(strTxtCfg, fOverwrite);
            if (SUCCEEDED(hrc))
            {
                try
                {
                    rVecFiles.append(strTxtCfg);
                }
                catch (std::bad_alloc &)
                {
                    RTFileDelete(strTxtCfg.c_str());
                    hrc = E_OUTOFMEMORY;
                }
            }
        }
        if (FAILED(hrc))
            return hrc;
    }

    /*
     * Call parent to add the preseed file from mAlg.
     */
    return UnattendedLinuxInstaller::addFilesToAuxVisoVectors(rVecArgs, rVecFiles, hVfsOrgIso, fOverwrite);
}

HRESULT UnattendedDebianInstaller::editDebianTxtCfg(GeneralTextScript *pEditor)
{
    try
    {
        /** @todo r=bird: Add some comments saying wtf you're actually up to here.
         *        Repeating what's clear from function calls and boasting the
         *        inteligence of the code isn't helpful. */
        //find all lines with "label" inside
        std::vector<size_t> vecLineNumbers = pEditor->findTemplate("label", RTCString::CaseInsensitive);
        for (size_t i = 0; i < vecLineNumbers.size(); ++i)
        {
            RTCString const &rContent = pEditor->getContentOfLine(vecLineNumbers[i]);

            // ASSUME: suppose general string looks like "label install", two words separated by " ".
            RTCList<RTCString> vecPartsOfcontent = rContent.split(" ");
            if (vecPartsOfcontent.size() > 1 && vecPartsOfcontent[1].contains("install")) /** @todo r=bird: case insensitive? */
            {
                std::vector<size_t> vecDefaultLineNumbers = pEditor->findTemplate("default", RTCString::CaseInsensitive);
                //handle the lines more intelligently
                for (size_t j = 0; j < vecDefaultLineNumbers.size(); ++j)
                {
                    Utf8Str strNewContent("default ");
                    strNewContent.append(vecPartsOfcontent[1]);
                    HRESULT hrc = pEditor->setContentOfLine(vecDefaultLineNumbers[j], strNewContent);
                    if (FAILED(hrc))
                        return hrc;
                }
            }
        }
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }
    return UnattendedLinuxInstaller::editIsoLinuxCfg(pEditor);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*
*
*  Implementation UnattendedRhel6Installer functions
*
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT UnattendedRhel6Installer::addFilesToAuxVisoVectors(RTCList<RTCString> &rVecArgs, RTCList<RTCString> &rVecFiles,
                                                               RTVFS hVfsOrgIso, bool fOverwrite)
{
    Utf8Str strIsoLinuxCfg;
    try
    {
#if 1
        /* Remaster ISO. */
        rVecArgs.append() = "--no-file-mode";
        rVecArgs.append() = "--no-dir-mode";

        rVecArgs.append() = "--import-iso";
        rVecArgs.append(mpParent->i_getIsoPath());

        rVecArgs.append() = "--file-mode=0444";
        rVecArgs.append() = "--dir-mode=0555";

        /* We replace isolinux.cfg with our edited version (see further down). */
        rVecArgs.append() = "isolinux/isolinux.cfg=:must-remove:";
        strIsoLinuxCfg = mpParent->i_getAuxiliaryBasePath();
        strIsoLinuxCfg.append("isolinux-isolinux.cfg");
        rVecArgs.append().append("isolinux/isolinux.cfg=").append(strIsoLinuxCfg);

#else
        /** @todo Maybe we should just remaster the ISO for redhat derivatives too?
         *        One less CDROM to mount. */
        /* Name the ISO. */
        rVecArgs.append() = "--volume-id=VBox Unattended Boot";

        /* Copy the isolinux directory from the original install ISO. */
        rVecArgs.append().append("--push-iso=").append(mpParent->i_getIsoPath());
        rVecArgs.append() = "/isolinux=/isolinux";
        rVecArgs.append() = "--pop";

        /* We replace isolinux.cfg with our edited version (see further down). */
        rVecArgs.append() = "/isolinux/isolinux.cfg=:must-remove:";

        strIsoLinuxCfg = mpParent->i_getAuxiliaryBasePath();
        strIsoLinuxCfg.append("isolinux-isolinux.cfg");
        rVecArgs.append().append("/isolinux/isolinux.cfg=").append(strIsoLinuxCfg);

        /* Configure booting /isolinux/isolinux.bin. */
        rVecArgs.append() = "--eltorito-boot";
        rVecArgs.append() = "/isolinux/isolinux.bin";
        rVecArgs.append() = "--no-emulation-boot";
        rVecArgs.append() = "--boot-info-table";
        rVecArgs.append() = "--boot-load-seg=0x07c0";
        rVecArgs.append() = "--boot-load-size=4";

        /* Make the boot catalog visible in the file system. */
        rVecArgs.append() = "--boot-catalog=/isolinux/vboxboot.cat";
#endif
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    /*
     * Edit isolinux.cfg and save it.
     */
    {
        GeneralTextScript Editor(mpParent);
        HRESULT hrc = loadAndParseFileFromIso(hVfsOrgIso, "/isolinux/isolinux.cfg", &Editor);
        if (SUCCEEDED(hrc))
            hrc = editIsoLinuxCfg(&Editor);
        if (SUCCEEDED(hrc))
        {
            hrc = Editor.save(strIsoLinuxCfg, fOverwrite);
            if (SUCCEEDED(hrc))
            {
                try
                {
                    rVecFiles.append(strIsoLinuxCfg);
                }
                catch (std::bad_alloc &)
                {
                    RTFileDelete(strIsoLinuxCfg.c_str());
                    hrc = E_OUTOFMEMORY;
                }
            }
        }
        if (FAILED(hrc))
            return hrc;
    }

    /*
     * Call parent to add the ks.cfg file from mAlg.
     */
    return UnattendedLinuxInstaller::addFilesToAuxVisoVectors(rVecArgs, rVecFiles, hVfsOrgIso, fOverwrite);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
/*
*
*
*  Implementation UnattendedSuseInstaller functions
*
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////
#if 0 /* doesn't work, so convert later */
/*
 *
 * UnattendedSuseInstaller protected methods
 *
*/
HRESULT UnattendedSuseInstaller::setUserData()
{
    HRESULT rc = S_OK;
    //here base class function must be called first
    //because user home directory is set after user name
    rc = UnattendedInstaller::setUserData();

    rc = mAlg->setField(USERHOMEDIR_ID, "");
    if (FAILED(rc))
        return rc;

    return rc;
}

/*
 *
 * UnattendedSuseInstaller private methods
 *
*/

HRESULT UnattendedSuseInstaller::iv_initialPhase()
{
    Assert(isAuxiliaryIsoNeeded());
    if (mParent->i_isGuestOs64Bit())
        mFilesAndDirsToExtractFromIso.append("boot/x86_64/loader/ ");
    else
        mFilesAndDirsToExtractFromIso.append("boot/i386/loader/ ");
    return extractOriginalIso(mFilesAndDirsToExtractFromIso);
}


HRESULT UnattendedSuseInstaller::setupScriptOnAuxiliaryCD(const Utf8Str &path)
{
    HRESULT rc = S_OK;

    GeneralTextScript isoSuseCfgScript(mpParent);
    rc = isoSuseCfgScript.read(path);
    rc = isoSuseCfgScript.parse();
    //fix linux core bootable parameters: add path to the preseed script

    std::vector<size_t> listOfLines = isoSuseCfgScript.findTemplate("append");
    for(unsigned int i=0; i<listOfLines.size(); ++i)
    {
        isoSuseCfgScript.appendToLine(listOfLines.at(i),
                                      " auto=true priority=critical autoyast=default instmode=cd quiet splash noprompt noshell --");
    }

    //find all lines with "label" inside
    listOfLines = isoSuseCfgScript.findTemplate("label");
    for(unsigned int i=0; i<listOfLines.size(); ++i)
    {
        Utf8Str content = isoSuseCfgScript.getContentOfLine(listOfLines.at(i));

        //suppose general string looks like "label linux", two words separated by " ".
        RTCList<RTCString> partsOfcontent = content.split(" ");

        if (partsOfcontent.at(1).contains("linux"))
        {
            std::vector<size_t> listOfDefault = isoSuseCfgScript.findTemplate("default");
            //handle the lines more intelligently
            for(unsigned int j=0; j<listOfDefault.size(); ++j)
            {
                Utf8Str newContent("default ");
                newContent.append(partsOfcontent.at(1));
                isoSuseCfgScript.setContentOfLine(listOfDefault.at(j), newContent);
            }
        }
    }

    rc = isoSuseCfgScript.save(path, true);

    LogRelFunc(("UnattendedSuseInstaller::setupScriptsOnAuxiliaryCD(): The file %s has been changed\n", path.c_str()));

    return rc;
}
#endif

