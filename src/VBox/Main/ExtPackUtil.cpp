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
#include "ExtPackUtil.h"

#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/cpp/xml.h>

#include <VBox/log.h>


/**
 * Reads the extension pack description.
 *
 * @returns NULL on success, pointer to an error message on failure (caller
 *          deletes it).
 * @param   a_pszDir        The directory containing the description file.
 * @param   a_pExtPackDesc  Where to store the description file.
 * @param   a_pObjInfo      Where to store the object info for the file (unix
 *                          attribs). Optional.
 */
iprt::MiniString *VBoxExtPackLoadDesc(const char *a_pszDir, PVBOXEXTPACKDESC a_pExtPackDesc, PRTFSOBJINFO a_pObjInfo)
{
    /*
     * Clear the description.
     */
    a_pExtPackDesc->strName.setNull();
    a_pExtPackDesc->strDescription.setNull();
    a_pExtPackDesc->strVersion.setNull();
    a_pExtPackDesc->uRevision = 0;
    a_pExtPackDesc->strMainModule.setNull();

    /*
     * Validate, open and parse the XML file.
     */
    char szFilePath[RTPATH_MAX];
    int vrc = RTPathJoin(szFilePath, sizeof(szFilePath), a_pszDir, VBOX_EXTPACK_DESCRIPTION_NAME);
    if (RT_FAILURE(vrc))
        return new iprt::MiniString("No VirtualBoxExtensionPack element");

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
    xml::ElementNode *pRoot = Doc.getRootElement();

    /*
     * Get the main element and check its version.
     */
    const xml::ElementNode *pVBoxExtPackElm = pRoot->findChildElement(NULL, "VirtualBoxExtensionPack");
    if (!pVBoxExtPackElm)
        return new iprt::MiniString("No VirtualBoxExtensionPack element");

    iprt::MiniString strFormatVersion;
    if (!pVBoxExtPackElm->getAttributeValue("version", strFormatVersion))
        return new iprt::MiniString("Missing format version");
    if (!strFormatVersion.equals("1.0"))
        return &(new iprt::MiniString("Unsupported format version: "))->append(strFormatVersion);

    /*
     * Read and validate the name.
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
    if (!VBoxExtPackIsValidMainModuleString(pszVersion))
        return &(new iprt::MiniString("Invalid main module string: "))->append(pszMainModule);

    /*
     * Everything seems fine, fill in the return values and return successfully.
     */
    a_pExtPackDesc->strName         = pszName;
    a_pExtPackDesc->strDescription  = pszDesc;
    a_pExtPackDesc->strVersion      = pszVersion;
    a_pExtPackDesc->uRevision       = uRevision;
    a_pExtPackDesc->strMainModule   = pszMainModule;

    return NULL;
}


/**
 * Validates the extension pack name.
 *
 * @returns true if valid, false if not.
 * @param   pszName             The name to validate.
 */
bool VBoxExtPackIsValidName(const char *pszName)
{
    /* This must match the code in the extension manager. */
    if (!pszName || *pszName == '\0')
        return false;
    while (RT_C_IS_ALNUM(*pszName) || *pszName == ' ')
        pszName++;
    return *pszName == '\0';
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
    if (!RT_C_IS_DIGIT(*pszVersion))
        return false;
    for (;;)
    {
        while (RT_C_IS_DIGIT(*pszVersion))
            pszVersion++;
        if (*pszVersion != '.')
            break;
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
 * Validates the extension pack main module string.
 *
 * @returns true if valid, false if not.
 * @param   pszMainModule       The main module string to validate.
 */
bool VBoxExtPackIsValidMainModuleString(const char *pszMainModule)
{
    if (!pszMainModule || *pszMainModule == '\0')
        return false;

    /* Restricted charset, no extensions (dots). */
    while (   RT_C_IS_ALNUM(*pszMainModule)
           || *pszMainModule == '-'
           || *pszMainModule == '_')
        pszMainModule++;

    return *pszMainModule == '\0';
}

