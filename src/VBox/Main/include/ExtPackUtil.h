/* $Id$ */
/** @file
 * VirtualBox Main - Extension Pack Utilities and definitions, VBoxC, VBoxSVC, ++.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef ____H_EXTPACKUTIL
#define ____H_EXTPACKUTIL

#include <iprt/cpp/ministring.h>
#include <iprt/fs.h>


/** @name VBOX_EXTPACK_DESCRIPTION_NAME
 * The name of the description file in an extension pack.  */
#define VBOX_EXTPACK_DESCRIPTION_NAME   "ExtPack.xml"
/** @name VBOX_EXTPACK_SUFFIX
 * The suffix of a extension pack tarball. */
#define VBOX_EXTPACK_SUFFIX             ".vbox-extpack"


/**
 * Description of an extension pack.
 *
 * This is the internal representation of the ExtPack.xml.
 */
typedef struct VBOXEXTPACKDESC
{
    /** The name. */
    iprt::MiniString    strName;
    /** The description. */
    iprt::MiniString    strDescription;
    /** The version string. */
    iprt::MiniString    strVersion;
    /** The internal revision number. */
    uint32_t            uRevision;
    /** The name of the main module. */
    iprt::MiniString    strMainModule;
} VBOXEXTPACKDESC;

/** Pointer to a extension pack description. */
typedef VBOXEXTPACKDESC *PVBOXEXTPACKDESC;
/** Pointer to a const extension pack description. */
typedef VBOXEXTPACKDESC const *PCVBOXEXTPACKDESC;


iprt::MiniString *VBoxExtPackLoadDesc(const char *a_pszDir, PVBOXEXTPACKDESC a_pExtPackDesc, PRTFSOBJINFO a_pObjInfo);
bool    VBoxExtPackIsValidName(const char *pszName);
bool    VBoxExtPackIsValidVersionString(const char *pszName);
bool    VBoxExtPackIsValidMainModuleString(const char *pszMainModule);


#endif

