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

/** The minimum length (strlen) of a extension pack name. */
#define VBOX_EXTPACK_NAME_MIN_LEN       6
/** The max length (strlen) of a extension pack name. */
#define VBOX_EXTPACK_NAME_MAX_LEN       64

/** The architecture-dependent application data subdirectory where the
 * extension packs are installed.  Relative to RTPathAppPrivateArch. */
#define VBOX_EXTPACK_INSTALL_DIR        "ExtensionPacks"
/** The architecture-independent application data subdirectory where the
 * certificates are installed.  Relative to RTPathAppPrivateNoArch. */
#define VBOX_EXTPACK_CERT_DIR           "Certificates"


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
iprt::MiniString *VBoxExtPackExtractNameFromTarballPath(const char *pszTarball);
bool    VBoxExtPackIsValidName(const char *pszName);
bool    VBoxExtPackIsValidVersionString(const char *pszName);
bool    VBoxExtPackIsValidMainModuleString(const char *pszMainModule);


#endif

