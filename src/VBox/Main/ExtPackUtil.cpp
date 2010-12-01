/* $Id$ */
/** @file
 * VirtualBox Main - Extension Pack Utilities and definitions, VBoxC, VBoxSVC, ++.
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

#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/cpp/xml.h>

#include <VBox/log.h>


/**
 * Worker for VBoxExtPackLoadDesc that loads the plug-in descriptors.
 *
 * @returns Same as VBoxExtPackLoadDesc.
 * @param   pVBoxExtPackElm
 * @param   pcPlugIns           Where to return the number of plug-ins in the
 *                              array.
 * @param   paPlugIns           Where to return the plug-in descriptor array.
 *                              (RTMemFree it even on failure)
 */
static iprt::MiniString *
vboxExtPackLoadPlugInDescs(const xml::ElementNode *pVBoxExtPackElm,
                           uint32_t *pcPlugIns, PVBOXEXTPACKPLUGINDESC *paPlugIns)
{
    *pcPlugIns = 0;
    *paPlugIns = NULL;

    /** @todo plug-ins */
    NOREF(pVBoxExtPackElm);

    return NULL;
}

/**
 * Reads the extension pack descriptor.
 *
 * @returns NULL on success, pointer to an error message on failure (caller
 *          deletes it).
 * @param   a_pszDir        The directory containing the description file.
 * @param   a_pExtPackDesc  Where to store the extension pack descriptor.
 * @param   a_pObjInfo      Where to store the object info for the file (unix
 *                          attribs). Optional.
 */
iprt::MiniString *VBoxExtPackLoadDesc(const char *a_pszDir, PVBOXEXTPACKDESC a_pExtPackDesc, PRTFSOBJINFO a_pObjInfo)
{
    /*
     * Clear the descriptor.
     */
    a_pExtPackDesc->strName.setNull();
    a_pExtPackDesc->strDescription.setNull();
    a_pExtPackDesc->strVersion.setNull();
    a_pExtPackDesc->uRevision = 0;
    a_pExtPackDesc->strMainModule.setNull();
    a_pExtPackDesc->strVrdeModule.setNull();
    a_pExtPackDesc->cPlugIns = 0;
    a_pExtPackDesc->paPlugIns = NULL;

    /*
     * Validate, open and parse the XML file.
     */
    char szFilePath[RTPATH_MAX];
    int vrc = RTPathJoin(szFilePath, sizeof(szFilePath), a_pszDir, VBOX_EXTPACK_DESCRIPTION_NAME);
    if (RT_FAILURE(vrc))
        return new iprt::MiniString("RTPathJoin failed with %Rrc", vrc);

    RTFSOBJINFO ObjInfo;
    vrc = RTPathQueryInfoEx(szFilePath, &ObjInfo,  RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
    if (RT_FAILURE(vrc))
        return &(new iprt::MiniString())->printf("RTPathQueryInfoEx failed with %Rrc", vrc);
    if (a_pObjInfo)
        *a_pObjInfo = ObjInfo;
    if (!RTFS_IS_FILE(ObjInfo.Attr.fMode))
    {
        if (RTFS_IS_SYMLINK(ObjInfo.Attr.fMode))
            return new iprt::MiniString("The XML file is symlinked, that is not allowed");
        return &(new iprt::MiniString)->printf("The XML file is not a file (fMode=%#x)", ObjInfo.Attr.fMode);
    }

    xml::Document       Doc;
    xml::XmlFileParser  Parser;
    try
    {
        Parser.read(szFilePath, Doc);
    }
    catch (xml::XmlError Err)
    {
        return new iprt::MiniString(Err.what());
    }

    /*
     * Get the main element and check its version.
     */
    const xml::ElementNode *pVBoxExtPackElm = Doc.getRootElement();
    if (   !pVBoxExtPackElm
        || strcmp(pVBoxExtPackElm->getName(), "VirtualBoxExtensionPack") != 0)
        return new iprt::MiniString("No VirtualBoxExtensionPack element");

    iprt::MiniString strFormatVersion;
    if (!pVBoxExtPackElm->getAttributeValue("version", strFormatVersion))
        return new iprt::MiniString("Missing format version");
    if (!strFormatVersion.equals("1.0"))
        return &(new iprt::MiniString("Unsupported format version: "))->append(strFormatVersion);

    /*
     * Read and validate mandatory bits.
     */
    const xml::ElementNode *pNameElm = pVBoxExtPackElm->findChildElement("Name");
    if (!pNameElm)
        return new iprt::MiniString("The 'Name' element is missing");
    const char *pszName = pNameElm->getValue();
    if (!VBoxExtPackIsValidName(pszName))
        return &(new iprt::MiniString("Invalid name: "))->append(pszName);

    const xml::ElementNode *pDescElm = pVBoxExtPackElm->findChildElement("Description");
    if (!pDescElm)
        return new iprt::MiniString("The 'Description' element is missing");
    const char *pszDesc = pDescElm->getValue();
    if (!pszDesc || *pszDesc == '\0')
        return new iprt::MiniString("The 'Description' element is empty");
    if (strpbrk(pszDesc, "\n\r\t\v\b") != NULL)
        return new iprt::MiniString("The 'Description' must not contain control characters");

    const xml::ElementNode *pVersionElm = pVBoxExtPackElm->findChildElement("Version");
    if (!pVersionElm)
        return new iprt::MiniString("The 'Version' element is missing");
    const char *pszVersion = pVersionElm->getValue();
    if (!pszVersion || *pszVersion == '\0')
        return new iprt::MiniString("The 'Version' element is empty");
    if (!VBoxExtPackIsValidVersionString(pszVersion))
        return &(new iprt::MiniString("Invalid version string: "))->append(pszVersion);

    uint32_t uRevision;
    if (!pVersionElm->getAttributeValue("revision", uRevision))
        uRevision = 0;

    const xml::ElementNode *pMainModuleElm = pVBoxExtPackElm->findChildElement("MainModule");
    if (!pMainModuleElm)
        return new iprt::MiniString("The 'MainModule' element is missing");
    const char *pszMainModule = pMainModuleElm->getValue();
    if (!pszMainModule || *pszMainModule == '\0')
        return new iprt::MiniString("The 'MainModule' element is empty");
    if (!VBoxExtPackIsValidModuleString(pszMainModule))
        return &(new iprt::MiniString("Invalid main module string: "))->append(pszMainModule);

    /*
     * The VRDE module, optional.
     * Accept both none and empty as tokens of no VRDE module.
     */
    const char *pszVrdeModule = NULL;
    const xml::ElementNode *pVrdeModuleElm = pVBoxExtPackElm->findChildElement("VRDEModule");
    if (pVrdeModuleElm)
    {
        pszVrdeModule = pVrdeModuleElm->getValue();
        if (!pszVrdeModule || *pszVrdeModule == '\0')
            pszVrdeModule = NULL;
        else if (!VBoxExtPackIsValidModuleString(pszVrdeModule))
            return &(new iprt::MiniString("Invalid VRDE module string: "))->append(pszVrdeModule);
    }

    /*
     * Parse plug-in descriptions.
     */
    uint32_t                cPlugIns  = 0;
    PVBOXEXTPACKPLUGINDESC  paPlugIns = NULL;
    iprt::MiniString *pstrRet = vboxExtPackLoadPlugInDescs(pVBoxExtPackElm, &cPlugIns, &paPlugIns);
    if (pstrRet)
    {
        RTMemFree(paPlugIns);
        return pstrRet;
    }

    /*
     * Everything seems fine, fill in the return values and return successfully.
     */
    a_pExtPackDesc->strName         = pszName;
    a_pExtPackDesc->strDescription  = pszDesc;
    a_pExtPackDesc->strVersion      = pszVersion;
    a_pExtPackDesc->uRevision       = uRevision;
    a_pExtPackDesc->strMainModule   = pszMainModule;
    a_pExtPackDesc->strVrdeModule   = pszVrdeModule;
    a_pExtPackDesc->cPlugIns        = cPlugIns;
    a_pExtPackDesc->paPlugIns       = paPlugIns;

    return NULL;
}


/**
 * Frees all resources associated with a extension pack descriptor.
 *
 * @param   a_pExtPackDesc      The extension pack descriptor which members
 *                              should be freed.
 */
void VBoxExtPackFreeDesc(PVBOXEXTPACKDESC a_pExtPackDesc)
{
    if (!a_pExtPackDesc)
        return;

    a_pExtPackDesc->strName.setNull();
    a_pExtPackDesc->strDescription.setNull();
    a_pExtPackDesc->strVersion.setNull();
    a_pExtPackDesc->uRevision = 0;
    a_pExtPackDesc->strMainModule.setNull();
    a_pExtPackDesc->strVrdeModule.setNull();
    a_pExtPackDesc->cPlugIns = 0;
    RTMemFree(a_pExtPackDesc->paPlugIns);
    a_pExtPackDesc->paPlugIns = NULL;
}


/**
 * Extract the extension pack name from the tarball path.
 *
 * @returns String containing the name on success, the caller must delete it.
 *          NULL if no valid name was found or if we ran out of memory.
 * @param   pszTarball          The path to the tarball.
 */
iprt::MiniString *VBoxExtPackExtractNameFromTarballPath(const char *pszTarball)
{
    /*
     * Skip ahead to the filename part and count the number of characters
     * that matches the criteria for a extension pack name.
     *
     * Trick: '_' == ' ' - filenames with spaces cause trouble.
     */
    const char *pszSrc = RTPathFilename(pszTarball);
    if (!pszSrc)
        return NULL;

    size_t off = 0;
    while (RT_C_IS_ALNUM(pszSrc[off]) || pszSrc[off] == ' ' || pszSrc[off] == '_')
        off++;

    /*
     * Check min and max name limits.
     */
    if (   off > VBOX_EXTPACK_NAME_MAX_LEN
        || off < VBOX_EXTPACK_NAME_MIN_LEN)
        return NULL;

    /*
     * Replace underscores, duplicate the string and return it.
     * (Unforuntately, iprt::ministring does not offer simple search & replace.)
     */
    char szTmp[VBOX_EXTPACK_NAME_MAX_LEN + 1];
    memcpy(szTmp, pszSrc, off);
    szTmp[off] = '\0';
    char *psz = szTmp;
    while ((psz = strchr(psz, '_')) != NULL)
        *psz++ = ' ';
    Assert(VBoxExtPackIsValidName(szTmp));

    iprt::MiniString *pStrRet = new iprt::MiniString(szTmp, off);
    return pStrRet;
}


/**
 * Validates the extension pack name.
 *
 * @returns true if valid, false if not.
 * @param   pszName             The name to validate.
 * @sa      VBoxExtPackExtractNameFromTarballPath
 */
bool VBoxExtPackIsValidName(const char *pszName)
{
    if (!pszName)
        return false;

    /*
     * Check the characters making up the name, only english alphabet
     * characters, decimal digits and spaces are allowed.
     */
    size_t off = 0;
    while (pszName[off])
    {
        if (!RT_C_IS_ALNUM(pszName[off]) && pszName[off] != ' ')
            return false;
        off++;
    }

    /*
     * Check min and max name limits.
     */
    if (   off > VBOX_EXTPACK_NAME_MAX_LEN
        || off < VBOX_EXTPACK_NAME_MIN_LEN)
        return false;

    return true;
}

/**
 * Validates the extension pack version string.
 *
 * @returns true if valid, false if not.
 * @param   pszVersion          The version string to validate.
 */
bool VBoxExtPackIsValidVersionString(const char *pszVersion)
{
    if (!pszVersion || *pszVersion == '\0')
        return false;

    /* 1.x.y.z... */
    for (;;)
    {
        if (!RT_C_IS_DIGIT(*pszVersion))
            return false;
        do
            pszVersion++;
        while (RT_C_IS_DIGIT(*pszVersion));
        if (*pszVersion != '.')
            break;
        pszVersion++;
    }

    /* upper case string + numbers indicating the build type */
    if (*pszVersion == '-' || *pszVersion == '_')
    {
        do
            pszVersion++;
        while (   RT_C_IS_DIGIT(*pszVersion)
               || RT_C_IS_UPPER(*pszVersion)
               || *pszVersion == '-'
               || *pszVersion == '_');
    }

    /* revision or nothing */
    if (*pszVersion != '\0')
    {
        if (*pszVersion != 'r')
            return false;
        do
            pszVersion++;
        while (RT_C_IS_DIGIT(*pszVersion));
    }

    return *pszVersion == '\0';
}

/**
 * Validates an extension pack module string.
 *
 * @returns true if valid, false if not.
 * @param   pszModule           The module string to validate.
 */
bool VBoxExtPackIsValidModuleString(const char *pszModule)
{
    if (!pszModule || *pszModule == '\0')
        return false;

    /* Restricted charset, no extensions (dots). */
    while (   RT_C_IS_ALNUM(*pszModule)
           || *pszModule == '-'
           || *pszModule == '_')
        pszModule++;

    return *pszModule == '\0';
}

