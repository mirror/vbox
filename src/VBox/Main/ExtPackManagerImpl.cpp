/* $Id$ */
/** @file
 * VirtualBox Main - interface for Extension Packs, VBoxSVC & VBoxC.
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
#include "ExtPackManagerImpl.h"
#include "ExtPackUtil.h"

#include <iprt/buildconfig.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/ldr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/process.h>
#include <iprt/string.h>

#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/log.h>
#include <VBox/sup.h>
#include <VBox/version.h>
#include "AutoCaller.h"
#include "Global.h"
#include "SystemPropertiesImpl.h"
#include "VirtualBoxImpl.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name VBOX_EXTPACK_HELPER_NAME
 * The name of the utility application we employ to install and uninstall the
 * extension packs.  This is a set-uid-to-root binary on unixy platforms, which
 * is why it has to be a separate application.
 */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
# define VBOX_EXTPACK_HELPER_NAME       "VBoxExtPackHelper.exe"
#else
# define VBOX_EXTPACK_HELPER_NAME       "VBoxExtPackHelper"
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Private extension pack data.
 */
struct ExtPack::Data
{
public:
    /** The extension pack descriptor (loaded from the XML, mostly). */
    VBOXEXTPACKDESC     Desc;
    /** The file system object info of the XML file.
     * This is for detecting changes and save time in refresh().  */
    RTFSOBJINFO         ObjInfoDesc;
    /** Whether it's usable or not. */
    bool                fUsable;
    /** Why it is unusable. */
    Utf8Str             strWhyUnusable;

    /** Where the extension pack is located. */
    Utf8Str             strExtPackPath;
    /** The file system object info of the extension pack directory.
     * This is for detecting changes and save time in refresh().  */
    RTFSOBJINFO         ObjInfoExtPack;
    /** The full path to the main module. */
    Utf8Str             strMainModPath;
    /** The file system object info of the main module.
     * This is used to determin whether to bother try reload it. */
    RTFSOBJINFO         ObjInfoMainMod;
    /** The module handle of the main extension pack module. */
    RTLDRMOD            hMainMod;

    /** The helper callbacks for the extension pack. */
    VBOXEXTPACKHLP      Hlp;
    /** Pointer back to the extension pack object (for Hlp methods). */
    ExtPack            *pThis;
    /** The extension pack registration structure. */
    PCVBOXEXTPACKREG    pReg;
    /** Pointer to the VirtualBox object (for VRDE registration). */
    VirtualBox         *pVirtualBox;
};

/** List of extension packs. */
typedef std::list< ComObjPtr<ExtPack> > ExtPackList;

/**
 * Private extension pack manager data.
 */
struct ExtPackManager::Data
{
    /** The directory where the extension packs are installed. */
    Utf8Str     strBaseDir;
    /** The directory where the extension packs can be dropped for automatic
     * installation. */
    Utf8Str     strDropZoneDir;
    /** The directory where the certificates this installation recognizes are
     * stored. */
    Utf8Str     strCertificatDirPath;
    /** The list of installed extension packs. */
    ExtPackList llInstalledExtPacks;
    /** Pointer to the VirtualBox object, our parent. */
    VirtualBox *pVirtualBox;
};


DEFINE_EMPTY_CTOR_DTOR(ExtPack)

/**
 * Called by ComObjPtr::createObject when creating the object.
 *
 * Just initialize the basic object state, do the rest in init().
 *
 * @returns S_OK.
 */
HRESULT ExtPack::FinalConstruct()
{
    m = NULL;
    return S_OK;
}

/**
 * Initializes the extension pack by reading its file.
 *
 * @returns COM status code.
 * @param   a_pVirtualBox   Pointer to the VirtualBox object (grandparent).
 * @param   a_pszName       The name of the extension pack.  This is also the
 *                          name of the subdirector under @a a_pszParentDir
 *                          where the extension pack is installed.
 * @param   a_pszParentDir  The parent directory.
 */
HRESULT ExtPack::init(VirtualBox *a_pVirtualBox, const char *a_pszName, const char *a_pszParentDir)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    static const VBOXEXTPACKHLP s_HlpTmpl =
    {
        /* u32Version           = */ VBOXEXTPACKHLP_VERSION,
        /* uVBoxFullVersion     = */ VBOX_FULL_VERSION,
        /* uVBoxVersionRevision = */ 0,
        /* u32Padding           = */ 0,
        /* pszVBoxVersion       = */ "",
        /* pfnFindModule        = */ ExtPack::hlpFindModule,
        /* pfnGetFilePath       = */ ExtPack::hlpGetFilePath,
        /* u32EndMarker         = */ VBOXEXTPACKHLP_VERSION
    };

    /*
     * Figure out where we live and allocate + initialize our private data.
     */
    char szDir[RTPATH_MAX];
    int vrc = RTPathJoin(szDir, sizeof(szDir), a_pszParentDir, a_pszName);
    AssertLogRelRCReturn(vrc, E_FAIL);

    m = new Data;
    m->Desc.strName                 = a_pszName;
    RT_ZERO(m->ObjInfoDesc);
    m->fUsable                      = false;
    m->strWhyUnusable               = tr("ExtPack::init failed");
    m->strExtPackPath               = szDir;
    RT_ZERO(m->ObjInfoExtPack);
    m->strMainModPath.setNull();
    RT_ZERO(m->ObjInfoMainMod);
    m->hMainMod                     = NIL_RTLDRMOD;
    m->Hlp                          = s_HlpTmpl;
    m->Hlp.pszVBoxVersion           = RTBldCfgVersion();
    m->Hlp.uVBoxInternalRevision    = RTBldCfgRevision();
    m->pThis                        = this;
    m->pReg                         = NULL;
    m->pVirtualBox                  = a_pVirtualBox;

    /*
     * Probe the extension pack (this code is shared with refresh()).
     */
    probeAndLoad();

    autoInitSpan.setSucceeded();
    return S_OK;
}

/**
 * COM cruft.
 */
void ExtPack::FinalRelease()
{
    uninit();
}

/**
 * Do the actual cleanup.
 */
void ExtPack::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone() && m != NULL)
    {
        if (m->hMainMod != NIL_RTLDRMOD)
        {
            AssertPtr(m->pReg);
            if (m->pReg->pfnUnload != NULL)
                m->pReg->pfnUnload(m->pReg);

            RTLdrClose(m->hMainMod);
            m->hMainMod = NIL_RTLDRMOD;
            m->pReg = NULL;
        }

        VBoxExtPackFreeDesc(&m->Desc);

        delete m;
        m = NULL;
    }
}


/**
 * Calls the installed hook.
 *
 * @param   a_pVirtualBox       The VirtualBox interface.
 * @remarks Caller holds the extension manager lock.
 */
void    ExtPack::callInstalledHook(IVirtualBox *a_pVirtualBox)
{
    if (   m->hMainMod != NIL_RTLDRMOD
        && m->pReg->pfnInstalled)
        m->pReg->pfnInstalled(m->pReg, a_pVirtualBox);
}

/**
 * Calls the uninstall hook and closes the module.
 *
 * @returns S_OK or COM error status with error information.
 * @param   a_pVirtualBox       The VirtualBox interface.
 * @param   a_fForcedRemoval    When set, we'll ignore complaints from the
 *                              uninstall hook.
 * @remarks The caller holds the manager's write lock.
 */
HRESULT ExtPack::callUninstallHookAndClose(IVirtualBox *a_pVirtualBox, bool a_fForcedRemoval)
{
    HRESULT hrc = S_OK;

    if (m->hMainMod != NIL_RTLDRMOD)
    {
        if (m->pReg->pfnUninstall && !a_fForcedRemoval)
        {
            int vrc = m->pReg->pfnUninstall(m->pReg, a_pVirtualBox);
            if (RT_FAILURE(vrc))
            {
                LogRel(("ExtPack pfnUninstall returned %Rrc for %s\n", vrc, m->Desc.strName.c_str()));
                if (!a_fForcedRemoval)
                    hrc = setError(E_FAIL, tr("pfnUninstall returned %Rrc"), vrc);
            }
        }
        if (SUCCEEDED(hrc))
        {
            RTLdrClose(m->hMainMod);
            m->hMainMod = NIL_RTLDRMOD;
            m->pReg = NULL;
        }
    }

    return hrc;
}

/**
 * Calls the pfnVirtualBoxReady hook.
 *
 * @param   a_pVirtualBox       The VirtualBox interface.
 * @remarks Caller holds the extension manager lock.
 */
void ExtPack::callVirtualBoxReadyHook(IVirtualBox *a_pVirtualBox)
{
    if (   m->hMainMod != NIL_RTLDRMOD
        && m->pReg->pfnVirtualBoxReady)
        m->pReg->pfnVirtualBoxReady(m->pReg, a_pVirtualBox);
}

/**
 * Calls the pfnVMCreate hook.
 *
 * @param   a_pVirtualBox       The VirtualBox interface.
 * @param   a_pMachine          The machine interface of the new VM.
 * @remarks Caller holds the extension manager lock.
 */
void ExtPack::callVmCreatedHook(IVirtualBox *a_pVirtualBox, IMachine *a_pMachine)
{
    if (   m->hMainMod != NIL_RTLDRMOD
        && m->pReg->pfnVMCreated)
        m->pReg->pfnVMCreated(m->pReg, a_pVirtualBox, a_pMachine);
}

/**
 * Calls the pfnVMConfigureVMM hook.
 *
 * @returns VBox status code, LogRel called on failure.
 * @param   a_pConsole          The console interface.
 * @param   a_pVM               The VM handle.
 * @remarks Caller holds the extension manager lock.
 */
int ExtPack::callVmConfigureVmmHook(IConsole *a_pConsole, PVM a_pVM)
{
    if (   m->hMainMod != NIL_RTLDRMOD
        && m->pReg->pfnVMConfigureVMM)
    {
        int vrc = m->pReg->pfnVMConfigureVMM(m->pReg, a_pConsole, a_pVM);
        if (RT_FAILURE(vrc))
        {
            LogRel(("ExtPack pfnVMConfigureVMM returned %Rrc for %s\n", vrc, m->Desc.strName.c_str()));
            return vrc;
        }
    }
    return VINF_SUCCESS;
}

/**
 * Calls the pfnVMPowerOn hook.
 *
 * @returns VBox status code, LogRel called on failure.
 * @param   a_pConsole          The console interface.
 * @param   a_pVM               The VM handle.
 * @remarks Caller holds the extension manager lock.
 */
int ExtPack::callVmPowerOnHook(IConsole *a_pConsole, PVM a_pVM)
{
    if (   m->hMainMod != NIL_RTLDRMOD
        && m->pReg->pfnVMPowerOn)
    {
        int vrc = m->pReg->pfnVMPowerOn(m->pReg, a_pConsole, a_pVM);
        if (RT_FAILURE(vrc))
        {
            LogRel(("ExtPack pfnVMPowerOn returned %Rrc for %s\n", vrc, m->Desc.strName.c_str()));
            return vrc;
        }
    }
    return VINF_SUCCESS;
}

/**
 * Calls the pfnVMPowerOff hook.
 *
 * @param   a_pConsole          The console interface.
 * @param   a_pVM               The VM handle.
 * @remarks Caller holds the extension manager lock.
 */
void ExtPack::callVmPowerOffHook(IConsole *a_pConsole, PVM a_pVM)
{
    if (   m->hMainMod != NIL_RTLDRMOD
        && m->pReg->pfnVMPowerOff)
        m->pReg->pfnVMPowerOff(m->pReg, a_pConsole, a_pVM);
}

/**
 * Check if the extension pack is usable and has an VRDE module.
 *
 * @returns S_OK or COM error status with error information.
 *
 * @remarks Caller holds the extension manager lock for reading, no locking
 *          necessary.
 */
HRESULT ExtPack::checkVrde(void)
{
    HRESULT hrc;
    if (m->fUsable)
    {
        if (m->Desc.strVrdeModule.isNotEmpty())
            hrc = S_OK;
        else
            hrc = setError(E_FAIL, tr("The extension pack '%s' does not include a VRDE module"), m->Desc.strName.c_str());
    }
    else
        hrc = setError(E_FAIL, tr("%s"), m->strWhyUnusable.c_str());
    return hrc;
}

/**
 * Same as checkVrde(), except that it also resolves the path to the module.
 *
 * @returns S_OK or COM error status with error information.
 * @param   a_pstrVrdeLibrary  Where to return the path on success.
 *
 * @remarks Caller holds the extension manager lock for reading, no locking
 *          necessary.
 */
HRESULT ExtPack::getVrdpLibraryName(Utf8Str *a_pstrVrdeLibrary)
{
    HRESULT hrc = checkVrde();
    if (SUCCEEDED(hrc))
    {
        if (findModule(m->Desc.strVrdeModule.c_str(), NULL, VBOXEXTPACKMODKIND_R3,
                       a_pstrVrdeLibrary, NULL /*a_pfNative*/, NULL /*a_pObjInfo*/))
            hrc = S_OK;
        else
            hrc = setError(E_FAIL, tr("Failed to locate the VRDE module '%s' in extension pack '%s'"),
                           m->Desc.strVrdeModule.c_str(), m->Desc.strName.c_str());
    }
    return hrc;
}

/**
 * Check if this extension pack wishes to be the default VRDE provider.
 *
 * @returns @c true if it wants to and it is in a usable state, otherwise
 *          @c false.
 *
 * @remarks Caller holds the extension manager lock for reading, no locking
 *          necessary.
 */
bool ExtPack::wantsToBeDefaultVrde(void) const
{
    return m->fUsable
        && m->Desc.strVrdeModule.isNotEmpty();
}

/**
 * Refreshes the extension pack state.
 *
 * This is called by the manager so that the on disk changes are picked up.
 *
 * @returns S_OK or COM error status with error information.
 * @param   pfCanDelete     Optional can-delete-this-object output indicator.
 * @remarks Caller holds the extension manager lock for writing.
 */
HRESULT ExtPack::refresh(bool *pfCanDelete)
{
    if (pfCanDelete)
        *pfCanDelete = false;

    AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS); /* for the COMGETTERs */

    /*
     * Has the module been deleted?
     */
    RTFSOBJINFO ObjInfoExtPack;
    int vrc = RTPathQueryInfoEx(m->strExtPackPath.c_str(), &ObjInfoExtPack, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
    if (   RT_FAILURE(vrc)
        || !RTFS_IS_DIRECTORY(ObjInfoExtPack.Attr.fMode))
    {
        if (pfCanDelete)
            *pfCanDelete = true;
        return S_OK;
    }

    /*
     * We've got a directory, so try query file system object info for the
     * files we are interested in as well.
     */
    RTFSOBJINFO ObjInfoDesc;
    char        szDescFilePath[RTPATH_MAX];
    vrc = RTPathJoin(szDescFilePath, sizeof(szDescFilePath), m->strExtPackPath.c_str(), VBOX_EXTPACK_DESCRIPTION_NAME);
    if (RT_SUCCESS(vrc))
        vrc = RTPathQueryInfoEx(szDescFilePath, &ObjInfoDesc, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
    if (RT_FAILURE(vrc))
        RT_ZERO(ObjInfoDesc);

    RTFSOBJINFO ObjInfoMainMod;
    if (m->strMainModPath.isNotEmpty())
        vrc = RTPathQueryInfoEx(m->strMainModPath.c_str(), &ObjInfoMainMod, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
    if (m->strMainModPath.isEmpty() || RT_FAILURE(vrc))
        RT_ZERO(ObjInfoMainMod);

    /*
     * If we have a usable module already, just verify that things haven't
     * changed since we loaded it.
     */
    if (m->fUsable)
    {
        /** @todo not important, so it can wait. */
    }
    /*
     * Ok, it is currently not usable.  If anything has changed since last time
     * reprobe the extension pack.
     */
    else if (   !objinfoIsEqual(&ObjInfoDesc,    &m->ObjInfoDesc)
             || !objinfoIsEqual(&ObjInfoMainMod, &m->ObjInfoMainMod)
             || !objinfoIsEqual(&ObjInfoExtPack, &m->ObjInfoExtPack) )
        probeAndLoad();

    return S_OK;
}

/**
 * Probes the extension pack, loading the main dll and calling its registration
 * entry point.
 *
 * This updates the state accordingly, the strWhyUnusable and fUnusable members
 * being the most important ones.
 */
void ExtPack::probeAndLoad(void)
{
    m->fUsable = false;

    /*
     * Query the file system info for the extension pack directory.  This and
     * all other file system info we save is for the benefit of refresh().
     */
    int vrc = RTPathQueryInfoEx(m->strExtPackPath.c_str(), &m->ObjInfoExtPack, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
    if (RT_FAILURE(vrc))
    {
        m->strWhyUnusable.printf(tr("RTPathQueryInfoEx on '%s' failed: %Rrc"), m->strExtPackPath.c_str(), vrc);
        return;
    }
    if (!RTFS_IS_DIRECTORY(m->ObjInfoExtPack.Attr.fMode))
    {
        if (RTFS_IS_SYMLINK(m->ObjInfoExtPack.Attr.fMode))
            m->strWhyUnusable.printf(tr("'%s' is a symbolic link, this is not allowed"), m->strExtPackPath.c_str(), vrc);
        else if (RTFS_IS_FILE(m->ObjInfoExtPack.Attr.fMode))
            m->strWhyUnusable.printf(tr("'%s' is a symbolic file, not a directory"), m->strExtPackPath.c_str(), vrc);
        else
            m->strWhyUnusable.printf(tr("'%s' is not a directory (fMode=%#x)"), m->strExtPackPath.c_str(), m->ObjInfoExtPack.Attr.fMode);
        return;
    }

    char szErr[2048];
    RT_ZERO(szErr);
    vrc = SUPR3HardenedVerifyDir(m->strExtPackPath.c_str(), true /*fRecursive*/, true /*fCheckFiles*/, szErr, sizeof(szErr));
    if (RT_FAILURE(vrc))
    {
        m->strWhyUnusable.printf(tr("%s (rc=%Rrc)"), szErr, vrc);
        return;
    }

    /*
     * Read the description file.
     */
    iprt::MiniString strSavedName(m->Desc.strName);
    iprt::MiniString *pStrLoadErr = VBoxExtPackLoadDesc(m->strExtPackPath.c_str(), &m->Desc, &m->ObjInfoDesc);
    if (pStrLoadErr != NULL)
    {
        m->strWhyUnusable.printf(tr("Failed to load '%s/%s': %s"),
                                 m->strExtPackPath.c_str(), VBOX_EXTPACK_DESCRIPTION_NAME, pStrLoadErr->c_str());
        m->Desc.strName = strSavedName;
        delete pStrLoadErr;
        return;
    }

    /*
     * Make sure the XML name and directory matches.
     */
    if (!m->Desc.strName.equalsIgnoreCase(strSavedName))
    {
        m->strWhyUnusable.printf(tr("The description name ('%s') and directory name ('%s') does not match"),
                                 m->Desc.strName.c_str(), strSavedName.c_str());
        m->Desc.strName = strSavedName;
        return;
    }

    /*
     * Load the main DLL and call the predefined entry point.
     */
    bool fIsNative;
    if (!findModule(m->Desc.strMainModule.c_str(), NULL /* default extension */, VBOXEXTPACKMODKIND_R3,
                    &m->strMainModPath, &fIsNative, &m->ObjInfoMainMod))
    {
        m->strWhyUnusable.printf(tr("Failed to locate the main module ('%s')"), m->Desc.strMainModule.c_str());
        return;
    }

    vrc = SUPR3HardenedVerifyPlugIn(m->strMainModPath.c_str(), szErr, sizeof(szErr));
    if (RT_FAILURE(vrc))
    {
        m->strWhyUnusable.printf(tr("%s"), szErr);
        return;
    }

    if (fIsNative)
    {
        vrc = RTLdrLoad(m->strMainModPath.c_str(), &m->hMainMod);
        if (RT_FAILURE(vrc))
        {
            m->hMainMod = NIL_RTLDRMOD;
            m->strWhyUnusable.printf(tr("Failed to locate load the main module ('%s'): %Rrc"),
                                           m->strMainModPath.c_str(), vrc);
            return;
        }
    }
    else
    {
        m->strWhyUnusable.printf(tr("Only native main modules are currently supported"));
        return;
    }

    /*
     * Resolve the predefined entry point.
     */
    PFNVBOXEXTPACKREGISTER pfnRegistration;
    vrc = RTLdrGetSymbol(m->hMainMod, VBOX_EXTPACK_MAIN_MOD_ENTRY_POINT, (void **)&pfnRegistration);
    if (RT_SUCCESS(vrc))
    {
        RT_ZERO(szErr);
        vrc = pfnRegistration(&m->Hlp, &m->pReg, szErr, sizeof(szErr) - 16);
        if (   RT_SUCCESS(vrc)
            && szErr[0] == '\0'
            && VALID_PTR(m->pReg))
        {
            if (   m->pReg->u32Version   == VBOXEXTPACKREG_VERSION
                && m->pReg->u32EndMarker == VBOXEXTPACKREG_VERSION)
            {
                if (   (!m->pReg->pfnInstalled       || RT_VALID_PTR(m->pReg->pfnInstalled))
                    && (!m->pReg->pfnUninstall       || RT_VALID_PTR(m->pReg->pfnUninstall))
                    && (!m->pReg->pfnVirtualBoxReady || RT_VALID_PTR(m->pReg->pfnVirtualBoxReady))
                    && (!m->pReg->pfnUnload          || RT_VALID_PTR(m->pReg->pfnUnload))
                    && (!m->pReg->pfnVMCreated       || RT_VALID_PTR(m->pReg->pfnVMCreated))
                    && (!m->pReg->pfnVMConfigureVMM  || RT_VALID_PTR(m->pReg->pfnVMConfigureVMM))
                    && (!m->pReg->pfnVMPowerOn       || RT_VALID_PTR(m->pReg->pfnVMPowerOn))
                    && (!m->pReg->pfnVMPowerOff      || RT_VALID_PTR(m->pReg->pfnVMPowerOff))
                    && (!m->pReg->pfnQueryObject     || RT_VALID_PTR(m->pReg->pfnQueryObject))
                    )
                {
                    /*
                     * We're good!
                     */
                    m->fUsable = true;
                    m->strWhyUnusable.setNull();
                    return;
                }

                m->strWhyUnusable = tr("The registration structure contains on or more invalid function pointers");
            }
            else
                m->strWhyUnusable.printf(tr("Unsupported registration structure version %u.%u"),
                                         RT_HIWORD(m->pReg->u32Version), RT_LOWORD(m->pReg->u32Version));
        }
        else
        {
            szErr[sizeof(szErr) - 1] = '\0';
            m->strWhyUnusable.printf(tr("%s returned %Rrc, pReg=%p szErr='%s'"),
                                     VBOX_EXTPACK_MAIN_MOD_ENTRY_POINT, vrc, m->pReg, szErr);
        }
        m->pReg = NULL;
    }
    else
        m->strWhyUnusable.printf(tr("Failed to resolve exported symbol '%s' in the main module: %Rrc"),
                                 VBOX_EXTPACK_MAIN_MOD_ENTRY_POINT, vrc);

    RTLdrClose(m->hMainMod);
    m->hMainMod = NIL_RTLDRMOD;
}

/**
 * Finds a module.
 *
 * @returns true if found, false if not.
 * @param   a_pszName           The module base name (no extension).
 * @param   a_pszExt            The extension. If NULL we use default
 *                              extensions.
 * @param   a_enmKind           The kind of module to locate.
 * @param   a_pStrFound         Where to return the path to the module we've
 *                              found.
 * @param   a_pfNative          Where to return whether this is a native module
 *                              or an agnostic one. Optional.
 * @param   a_pObjInfo          Where to return the file system object info for
 *                              the module. Optional.
 */
bool ExtPack::findModule(const char *a_pszName, const char *a_pszExt, VBOXEXTPACKMODKIND a_enmKind,
                         Utf8Str *a_pStrFound, bool *a_pfNative, PRTFSOBJINFO a_pObjInfo) const
{
    /*
     * Try the native path first.
     */
    char szPath[RTPATH_MAX];
    int vrc = RTPathJoin(szPath, sizeof(szPath), m->strExtPackPath.c_str(), RTBldCfgTargetDotArch());
    AssertLogRelRCReturn(vrc, false);
    vrc = RTPathAppend(szPath, sizeof(szPath), a_pszName);
    AssertLogRelRCReturn(vrc, false);
    if (!a_pszExt)
    {
        const char *pszDefExt;
        switch (a_enmKind)
        {
            case VBOXEXTPACKMODKIND_RC: pszDefExt = ".rc"; break;
            case VBOXEXTPACKMODKIND_R0: pszDefExt = ".r0"; break;
            case VBOXEXTPACKMODKIND_R3: pszDefExt = RTLdrGetSuff(); break;
            default:
                AssertFailedReturn(false);
        }
        vrc = RTStrCat(szPath, sizeof(szPath), pszDefExt);
        AssertLogRelRCReturn(vrc, false);
    }

    RTFSOBJINFO ObjInfo;
    if (!a_pObjInfo)
        a_pObjInfo = &ObjInfo;
    vrc = RTPathQueryInfo(szPath, a_pObjInfo, RTFSOBJATTRADD_UNIX);
    if (RT_SUCCESS(vrc) && RTFS_IS_FILE(a_pObjInfo->Attr.fMode))
    {
        if (a_pfNative)
            *a_pfNative = true;
        *a_pStrFound = szPath;
        return true;
    }

    /*
     * Try the platform agnostic modules.
     */
    /* gcc.x86/module.rel */
    char szSubDir[32];
    RTStrPrintf(szSubDir, sizeof(szSubDir), "%s.%s", RTBldCfgCompiler(), RTBldCfgTargetArch());
    vrc = RTPathJoin(szPath, sizeof(szPath), m->strExtPackPath.c_str(), szSubDir);
    AssertLogRelRCReturn(vrc, false);
    vrc = RTPathAppend(szPath, sizeof(szPath), a_pszName);
    AssertLogRelRCReturn(vrc, false);
    if (!a_pszExt)
    {
        vrc = RTStrCat(szPath, sizeof(szPath), ".rel");
        AssertLogRelRCReturn(vrc, false);
    }
    vrc = RTPathQueryInfo(szPath, a_pObjInfo, RTFSOBJATTRADD_UNIX);
    if (RT_SUCCESS(vrc) && RTFS_IS_FILE(a_pObjInfo->Attr.fMode))
    {
        if (a_pfNative)
            *a_pfNative = false;
        *a_pStrFound = szPath;
        return true;
    }

    /* x86/module.rel */
    vrc = RTPathJoin(szPath, sizeof(szPath), m->strExtPackPath.c_str(), RTBldCfgTargetArch());
    AssertLogRelRCReturn(vrc, false);
    vrc = RTPathAppend(szPath, sizeof(szPath), a_pszName);
    AssertLogRelRCReturn(vrc, false);
    if (!a_pszExt)
    {
        vrc = RTStrCat(szPath, sizeof(szPath), ".rel");
        AssertLogRelRCReturn(vrc, false);
    }
    vrc = RTPathQueryInfo(szPath, a_pObjInfo, RTFSOBJATTRADD_UNIX);
    if (RT_SUCCESS(vrc) && RTFS_IS_FILE(a_pObjInfo->Attr.fMode))
    {
        if (a_pfNative)
            *a_pfNative = false;
        *a_pStrFound = szPath;
        return true;
    }

    return false;
}

/**
 * Compares two file system object info structures.
 *
 * @returns true if equal, false if not.
 * @param   pObjInfo1           The first.
 * @param   pObjInfo2           The second.
 * @todo    IPRT should do this, really.
 */
/* static */ bool ExtPack::objinfoIsEqual(PCRTFSOBJINFO pObjInfo1, PCRTFSOBJINFO pObjInfo2)
{
    if (!RTTimeSpecIsEqual(&pObjInfo1->ModificationTime,   &pObjInfo2->ModificationTime))
        return false;
    if (!RTTimeSpecIsEqual(&pObjInfo1->ChangeTime,         &pObjInfo2->ChangeTime))
        return false;
    if (!RTTimeSpecIsEqual(&pObjInfo1->BirthTime,          &pObjInfo2->BirthTime))
        return false;
    if (pObjInfo1->cbObject                              != pObjInfo2->cbObject)
        return false;
    if (pObjInfo1->Attr.fMode                            != pObjInfo2->Attr.fMode)
        return false;
    if (pObjInfo1->Attr.enmAdditional                    == pObjInfo2->Attr.enmAdditional)
    {
        switch (pObjInfo1->Attr.enmAdditional)
        {
            case RTFSOBJATTRADD_UNIX:
                if (pObjInfo1->Attr.u.Unix.uid           != pObjInfo2->Attr.u.Unix.uid)
                    return false;
                if (pObjInfo1->Attr.u.Unix.gid           != pObjInfo2->Attr.u.Unix.gid)
                    return false;
                if (pObjInfo1->Attr.u.Unix.INodeIdDevice != pObjInfo2->Attr.u.Unix.INodeIdDevice)
                    return false;
                if (pObjInfo1->Attr.u.Unix.INodeId       != pObjInfo2->Attr.u.Unix.INodeId)
                    return false;
                if (pObjInfo1->Attr.u.Unix.GenerationId  != pObjInfo2->Attr.u.Unix.GenerationId)
                    return false;
                break;
            default:
                break;
        }
    }
    return true;
}


/**
 * @interface_method_impl{VBOXEXTPACKHLP,pfnFindModule}
 */
/*static*/ DECLCALLBACK(int)
ExtPack::hlpFindModule(PCVBOXEXTPACKHLP pHlp, const char *pszName, const char *pszExt, VBOXEXTPACKMODKIND enmKind,
                       char *pszFound, size_t cbFound, bool *pfNative)
{
    /*
     * Validate the input and get our bearings.
     */
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pszExt, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFound, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfNative, VERR_INVALID_POINTER);
    AssertReturn(enmKind > VBOXEXTPACKMODKIND_INVALID && enmKind < VBOXEXTPACKMODKIND_END, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pHlp, VERR_INVALID_POINTER);
    AssertReturn(pHlp->u32Version == VBOXEXTPACKHLP_VERSION, VERR_INVALID_POINTER);
    ExtPack::Data *m = RT_FROM_CPP_MEMBER(pHlp, Data, Hlp);
    AssertPtrReturn(m, VERR_INVALID_POINTER);
    ExtPack *pThis = m->pThis;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    /*
     * This is just a wrapper around findModule.
     */
    Utf8Str strFound;
    if (pThis->findModule(pszName, pszExt, enmKind, &strFound, pfNative, NULL))
        return RTStrCopy(pszFound, cbFound, strFound.c_str());
    return VERR_FILE_NOT_FOUND;
}

/*static*/ DECLCALLBACK(int)
ExtPack::hlpGetFilePath(PCVBOXEXTPACKHLP pHlp, const char *pszFilename, char *pszPath, size_t cbPath)
{
    /*
     * Validate the input and get our bearings.
     */
    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath > 0, VERR_BUFFER_OVERFLOW);

    AssertPtrReturn(pHlp, VERR_INVALID_POINTER);
    AssertReturn(pHlp->u32Version == VBOXEXTPACKHLP_VERSION, VERR_INVALID_POINTER);
    ExtPack::Data *m = RT_FROM_CPP_MEMBER(pHlp, Data, Hlp);
    AssertPtrReturn(m, VERR_INVALID_POINTER);
    ExtPack *pThis = m->pThis;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    /*
     * This is a simple RTPathJoin, no checking if things exists or anything.
     */
    int vrc = RTPathJoin(pszPath, cbPath, pThis->m->strExtPackPath.c_str(), pszFilename);
    if (RT_FAILURE(vrc))
        RT_BZERO(pszPath, cbPath);
    return vrc;
}





STDMETHODIMP ExtPack::COMGETTER(Name)(BSTR *a_pbstrName)
{
    CheckComArgOutPointerValid(a_pbstrName);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        Bstr str(m->Desc.strName);
        str.cloneTo(a_pbstrName);
    }
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(Description)(BSTR *a_pbstrDescription)
{
    CheckComArgOutPointerValid(a_pbstrDescription);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        Bstr str(m->Desc.strDescription);
        str.cloneTo(a_pbstrDescription);
    }
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(Version)(BSTR *a_pbstrVersion)
{
    CheckComArgOutPointerValid(a_pbstrVersion);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        Bstr str(m->Desc.strVersion);
        str.cloneTo(a_pbstrVersion);
    }
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(Revision)(ULONG *a_puRevision)
{
    CheckComArgOutPointerValid(a_puRevision);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_puRevision = m->Desc.uRevision;
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(VRDEModule)(BSTR *a_pbstrVrdeModule)
{
    CheckComArgOutPointerValid(a_pbstrVrdeModule);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        Bstr str(m->Desc.strVrdeModule);
        str.cloneTo(a_pbstrVrdeModule);
    }
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(PlugIns)(ComSafeArrayOut(IExtPackPlugIn *, a_paPlugIns))
{
    /** @todo implement plug-ins. */
#ifdef VBOX_WITH_XPCOM
    NOREF(a_paPlugIns);
    NOREF(a_paPlugInsSize);
#endif
    ReturnComNotImplemented();
}

STDMETHODIMP ExtPack::COMGETTER(Usable)(BOOL *a_pfUsable)
{
    CheckComArgOutPointerValid(a_pfUsable);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_pfUsable = m->fUsable;
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(WhyUnusable)(BSTR *a_pbstrWhy)
{
    CheckComArgOutPointerValid(a_pbstrWhy);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        m->strWhyUnusable.cloneTo(a_pbstrWhy);
    return hrc;
}

STDMETHODIMP ExtPack::QueryObject(IN_BSTR a_bstrObjectId, IUnknown **a_ppUnknown)
{
    com::Guid ObjectId;
    CheckComArgGuid(a_bstrObjectId, ObjectId);
    CheckComArgOutPointerValid(a_ppUnknown);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        if (   m->pReg
            && m->pReg->pfnQueryObject)
        {
            void *pvUnknown = m->pReg->pfnQueryObject(m->pReg, ObjectId.raw());
            if (pvUnknown)
                *a_ppUnknown = (IUnknown *)pvUnknown;
            else
                hrc = E_NOINTERFACE;
        }
        else
            hrc = E_NOINTERFACE;
    }
    return hrc;
}




DEFINE_EMPTY_CTOR_DTOR(ExtPackManager)

/**
 * Called by ComObjPtr::createObject when creating the object.
 *
 * Just initialize the basic object state, do the rest in init().
 *
 * @returns S_OK.
 */
HRESULT ExtPackManager::FinalConstruct()
{
    m = NULL;
    return S_OK;
}

/**
 * Initializes the extension pack manager.
 *
 * @returns COM status code.
 * @param   a_pVirtualBox           Pointer to the VirtualBox object.
 * @param   a_pszDropZoneDir        The path to the drop zone directory.
 * @param   a_fCheckDropZone        Whether to check the drop zone for new
 *                                  extensions or not.  Only VBoxSVC does this
 *                                  and then only when wanted.
 */
HRESULT ExtPackManager::init(VirtualBox *a_pVirtualBox, const char *a_pszDropZoneDir, bool a_fCheckDropZone)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /*
     * Figure some stuff out before creating the instance data.
     */
    char szBaseDir[RTPATH_MAX];
    int rc = RTPathAppPrivateArch(szBaseDir, sizeof(szBaseDir));
    AssertLogRelRCReturn(rc, E_FAIL);
    rc = RTPathAppend(szBaseDir, sizeof(szBaseDir), VBOX_EXTPACK_INSTALL_DIR);
    AssertLogRelRCReturn(rc, E_FAIL);

    char szCertificatDir[RTPATH_MAX];
    rc = RTPathAppPrivateNoArch(szCertificatDir, sizeof(szCertificatDir));
    AssertLogRelRCReturn(rc, E_FAIL);
    rc = RTPathAppend(szCertificatDir, sizeof(szCertificatDir), VBOX_EXTPACK_CERT_DIR);
    AssertLogRelRCReturn(rc, E_FAIL);

    /*
     * Allocate and initialize the instance data.
     */
    m = new Data;
    m->strBaseDir           = szBaseDir;
    m->strCertificatDirPath = szCertificatDir;
    m->strDropZoneDir       = a_pszDropZoneDir;
    m->pVirtualBox          = a_pVirtualBox;

    /*
     * Go looking for extensions.  The RTDirOpen may fail if nothing has been
     * installed yet, or if root is paranoid and has revoked our access to them.
     *
     * We ASSUME that there are no files, directories or stuff in the directory
     * that exceed the max name length in RTDIRENTRYEX.
     */
    HRESULT hrc = S_OK;
    PRTDIR pDir;
    int vrc = RTDirOpen(&pDir, szBaseDir);
    if (RT_SUCCESS(vrc))
    {
        for (;;)
        {
            RTDIRENTRYEX Entry;
            vrc = RTDirReadEx(pDir, &Entry, NULL /*pcbDirEntry*/, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
            if (RT_FAILURE(vrc))
            {
                AssertLogRelMsg(vrc == VERR_NO_MORE_FILES, ("%Rrc\n", vrc));
                break;
            }
            if (   RTFS_IS_DIRECTORY(Entry.Info.Attr.fMode)
                && strcmp(Entry.szName, ".")  != 0
                && strcmp(Entry.szName, "..") != 0
                && VBoxExtPackIsValidName(Entry.szName) )
            {
                /*
                 * All directories are extensions, the shall be nothing but
                 * extensions in this subdirectory.
                 */
                ComObjPtr<ExtPack> NewExtPack;
                HRESULT hrc2 = NewExtPack.createObject();
                if (SUCCEEDED(hrc2))
                    hrc2 = NewExtPack->init(m->pVirtualBox, Entry.szName, szBaseDir);
                if (SUCCEEDED(hrc2))
                    m->llInstalledExtPacks.push_back(NewExtPack);
                else if (SUCCEEDED(rc))
                    hrc = hrc2;
            }
        }
        RTDirClose(pDir);
    }
    /* else: ignore, the directory probably does not exist or something. */

    /*
     * Look for things in the drop zone.
     */
    if (SUCCEEDED(hrc) && a_fCheckDropZone)
        processDropZone();

    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    return hrc;
}

/**
 * COM cruft.
 */
void ExtPackManager::FinalRelease()
{
    uninit();
}

/**
 * Do the actual cleanup.
 */
void ExtPackManager::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone() && m != NULL)
    {
        delete m;
        m = NULL;
    }
}


STDMETHODIMP ExtPackManager::COMGETTER(InstalledExtPacks)(ComSafeArrayOut(IExtPack *, a_paExtPacks))
{
    CheckComArgSafeArrayNotNull(a_paExtPacks);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        SafeIfaceArray<IExtPack> SaExtPacks(m->llInstalledExtPacks);
        SaExtPacks.detachTo(ComSafeArrayOutArg(a_paExtPacks));
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Find(IN_BSTR a_bstrName, IExtPack **a_pExtPack)
{
    CheckComArgNotNull(a_bstrName);
    CheckComArgOutPointerValid(a_pExtPack);
    Utf8Str strName(a_bstrName);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        ComPtr<ExtPack> ptrExtPack = findExtPack(strName.c_str());
        if (!ptrExtPack.isNull())
            ptrExtPack.queryInterfaceTo(a_pExtPack);
        else
            hrc = VBOX_E_OBJECT_NOT_FOUND;
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Install(IN_BSTR a_bstrTarball, BSTR *a_pbstrName)
{
    CheckComArgNotNull(a_bstrTarball);
    CheckComArgOutPointerValid(a_pbstrName);
    Utf8Str strTarball(a_bstrTarball);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        /*
         * Check that the file exists and that we can access it.
         */
        if (RTFileExists(strTarball.c_str()))
        {
            RTFILE hFile;
            int vrc = RTFileOpen(&hFile, strTarball.c_str(), RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN);
            if (RT_SUCCESS(vrc))
            {
                RTFSOBJINFO ObjInfo;
                vrc = RTFileQueryInfo(hFile, &ObjInfo, RTFSOBJATTRADD_NOTHING);
                if (   RT_SUCCESS(vrc)
                    && RTFS_IS_FILE(ObjInfo.Attr.fMode))
                {
                    /*
                     * Derive the name of the extension pack from the file
                     * name.  Certain restrictions are here placed on the
                     * tarball name.
                     */
                    iprt::MiniString *pStrName = VBoxExtPackExtractNameFromTarballPath(strTarball.c_str());
                    if (pStrName)
                    {
                        /*
                         * Refresh the data we have on the extension pack as it
                         * may be made stale by direct meddling or some other user.
                         */
                        ExtPack *pExtPack;
                        hrc = refreshExtPack(pStrName->c_str(), false /*a_fUnsuableIsError*/, &pExtPack);
                        if (SUCCEEDED(hrc) && !pExtPack)
                        {
                            /*
                             * Run the set-uid-to-root binary that performs the actual
                             * installation.  Then create an object for the packet (we
                             * do this even on failure, to be on the safe side).
                             */
                            char szTarballFd[64];
                            RTStrPrintf(szTarballFd, sizeof(szTarballFd), "0x%RX64",
                                        (uint64_t)RTFileToNative(hFile));

                            hrc = runSetUidToRootHelper("install",
                                                        "--base-dir",        m->strBaseDir.c_str(),
                                                        "--certificate-dir", m->strCertificatDirPath.c_str(),
                                                        "--name",            pStrName->c_str(),
                                                        "--tarball",         strTarball.c_str(),
                                                        "--tarball-fd",      &szTarballFd[0],
                                                        NULL);
                            if (SUCCEEDED(hrc))
                            {
                                hrc = refreshExtPack(pStrName->c_str(), true /*a_fUnsuableIsError*/, &pExtPack);
                                if (SUCCEEDED(hrc))
                                {
                                    LogRel(("ExtPackManager: Successfully installed extension pack '%s'.\n", pStrName->c_str()));
                                    pExtPack->callInstalledHook(m->pVirtualBox);
                                }
                            }
                            else
                            {
                                ErrorInfoKeeper Eik;
                                refreshExtPack(pStrName->c_str(), false /*a_fUnsuableIsError*/, NULL);
                            }
                        }
                        else if (SUCCEEDED(hrc))
                            hrc = setError(E_FAIL,
                                           tr("Extension pack '%s' is already installed."
                                              " In case of a reinstallation, please uninstall it first"),
                                           pStrName->c_str());
                        delete pStrName;
                    }
                    else
                        hrc = setError(E_FAIL, tr("Malformed '%s' file name"), strTarball.c_str());
                }
                else if (RT_SUCCESS(vrc))
                    hrc = setError(E_FAIL, tr("'%s' is not a regular file"), strTarball.c_str());
                else
                    hrc = setError(E_FAIL, tr("Failed to query info on '%s' (%Rrc)"), strTarball.c_str(), vrc);
                RTFileClose(hFile);
            }
            else
                hrc = setError(E_FAIL, tr("Failed to open '%s' (%Rrc)"), strTarball.c_str(), vrc);
        }
        else if (RTPathExists(strTarball.c_str()))
            hrc = setError(E_FAIL, tr("'%s' is not a regular file"), strTarball.c_str());
        else
            hrc = setError(E_FAIL, tr("File '%s' was inaccessible or not found"), strTarball.c_str());
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Uninstall(IN_BSTR a_bstrName, BOOL a_fForcedRemoval)
{
    CheckComArgNotNull(a_bstrName);
    Utf8Str strName(a_bstrName);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        /*
         * Refresh the data we have on the extension pack as it may be made
         * stale by direct meddling or some other user.
         */
        ExtPack *pExtPack;
        hrc = refreshExtPack(strName.c_str(), false /*a_fUnsuableIsError*/, &pExtPack);
        if (SUCCEEDED(hrc))
        {
            if (!pExtPack)
            {
                LogRel(("ExtPackManager: Extension pack '%s' is not installed, so nothing to uninstall.\n", strName.c_str()));
                hrc = S_OK;             /* nothing to uninstall */
            }
            else
            {
                /*
                 * Call the uninstall hook and unload the main dll.
                 */
                hrc = pExtPack->callUninstallHookAndClose(m->pVirtualBox, a_fForcedRemoval != FALSE);
                if (SUCCEEDED(hrc))
                {
                    /*
                     * Run the set-uid-to-root binary that performs the
                     * uninstallation.  Then refresh the object.
                     *
                     * This refresh is theorically subject to races, but it's of
                     * the don't-do-that variety.
                     */
                    const char *pszForcedOpt = a_fForcedRemoval ? "--forced" : NULL;
                    hrc = runSetUidToRootHelper("uninstall",
                                                "--base-dir", m->strBaseDir.c_str(),
                                                "--name",     strName.c_str(),
                                                pszForcedOpt, /* Last as it may be NULL. */
                                                NULL);
                    if (SUCCEEDED(hrc))
                    {
                        hrc = refreshExtPack(strName.c_str(), false /*a_fUnsuableIsError*/, &pExtPack);
                        if (SUCCEEDED(hrc))
                        {
                            if (!pExtPack)
                                LogRel(("ExtPackManager: Successfully uninstalled extension pack '%s'.\n", strName.c_str()));
                            else
                                hrc = setError(E_UNEXPECTED,
                                               tr("Uninstall extension pack '%s' failed under mysterious circumstances"),
                                               strName.c_str());
                        }
                    }
                    else
                    {
                        ErrorInfoKeeper Eik;
                        refreshExtPack(strName.c_str(), false /*a_fUnsuableIsError*/, NULL);
                    }
                }
            }
        }
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Cleanup(void)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        /*
         * Run the set-uid-to-root binary that performs the cleanup.
         *
         * Take the write lock to prevent conflicts with other calls to this
         * VBoxSVC instance.
         */
        AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);
        hrc = runSetUidToRootHelper("cleanup",
                                    "--base-dir", m->strBaseDir.c_str(),
                                    NULL);
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::QueryAllPlugInsForFrontend(IN_BSTR a_bstrFrontend, ComSafeArrayOut(BSTR, a_pabstrPlugInModules))
{
    CheckComArgNotNull(a_bstrFrontend);
    Utf8Str strName(a_bstrFrontend);
    CheckComArgOutSafeArrayPointerValid(a_pabstrPlugInModules);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        com::SafeArray<BSTR> saPaths((size_t)0);
        /** @todo implement plug-ins */
        saPaths.detachTo(ComSafeArrayOutArg(a_pabstrPlugInModules));
    }
    return hrc;
}


/**
 * Runs the helper application that does the privileged operations.
 *
 * @returns S_OK or a failure status with error information set.
 * @param   a_pszCommand        The command to execute.
 * @param   ...                 The argument strings that goes along with the
 *                              command. Maximum is about 16.  Terminated by a
 *                              NULL.
 */
HRESULT ExtPackManager::runSetUidToRootHelper(const char *a_pszCommand, ...)
{
    /*
     * Calculate the path to the helper application.
     */
    char szExecName[RTPATH_MAX];
    int vrc = RTPathAppPrivateArch(szExecName, sizeof(szExecName));
    AssertLogRelRCReturn(vrc, E_UNEXPECTED);

    vrc = RTPathAppend(szExecName, sizeof(szExecName), VBOX_EXTPACK_HELPER_NAME);
    AssertLogRelRCReturn(vrc, E_UNEXPECTED);

    /*
     * Convert the variable argument list to a RTProcCreate argument vector.
     */
    const char *apszArgs[20];
    unsigned    cArgs = 0;
    apszArgs[cArgs++] = &szExecName[0];
    apszArgs[cArgs++] = a_pszCommand;

    va_list va;
    va_start(va, a_pszCommand);
    const char *pszLastArg;
    LogRel(("ExtPack: Executing '%s'", szExecName));
    do
    {
        LogRel((" '%s'", apszArgs[cArgs - 1]));
        AssertReturn(cArgs < RT_ELEMENTS(apszArgs) - 1, E_UNEXPECTED);
        pszLastArg = va_arg(va, const char *);
        apszArgs[cArgs++] = pszLastArg;
    } while (pszLastArg != NULL);
    LogRel(("\n"));
    va_end(va);
    apszArgs[cArgs] = NULL;

    /*
     * Create a PIPE which we attach to stderr so that we can read the error
     * message on failure and report it back to the caller.
     */
    RTPIPE      hPipeR;
    RTHANDLE    hStdErrPipe;
    hStdErrPipe.enmType = RTHANDLETYPE_PIPE;
    vrc = RTPipeCreate(&hPipeR, &hStdErrPipe.u.hPipe, RTPIPE_C_INHERIT_WRITE);
    AssertLogRelRCReturn(vrc, E_UNEXPECTED);

    /*
     * Spawn the process.
     */
    HRESULT hrc;
    RTPROCESS hProcess;
    vrc = RTProcCreateEx(szExecName,
                         apszArgs,
                         RTENV_DEFAULT,
                         0 /*fFlags*/,
                         NULL /*phStdIn*/,
                         NULL /*phStdOut*/,
                         &hStdErrPipe,
                         NULL /*pszAsUser*/,
                         NULL /*pszPassword*/,
                         &hProcess);
    if (RT_SUCCESS(vrc))
    {
        vrc = RTPipeClose(hStdErrPipe.u.hPipe);
        hStdErrPipe.u.hPipe = NIL_RTPIPE;

        /*
         * Read the pipe output until the process completes.
         */
        RTPROCSTATUS    ProcStatus   = { -42, RTPROCEXITREASON_ABEND };
        size_t          cbStdErrBuf  = 0;
        size_t          offStdErrBuf = 0;
        char           *pszStdErrBuf = NULL;
        do
        {
            /*
             * Service the pipe. Block waiting for output or the pipe breaking
             * when the process terminates.
             */
            if (hPipeR != NIL_RTPIPE)
            {
                char    achBuf[16]; ///@todo 1024
                size_t  cbRead;
                vrc = RTPipeReadBlocking(hPipeR, achBuf, sizeof(achBuf), &cbRead);
                if (RT_SUCCESS(vrc))
                {
                    /* grow the buffer? */
                    size_t cbBufReq = offStdErrBuf + cbRead + 1;
                    if (   cbBufReq > cbStdErrBuf
                        && cbBufReq < _256K)
                    {
                        size_t cbNew = RT_ALIGN_Z(cbBufReq, 16); // 1024
                        void  *pvNew = RTMemRealloc(pszStdErrBuf, cbNew);
                        if (pvNew)
                        {
                            pszStdErrBuf = (char *)pvNew;
                            cbStdErrBuf  = cbNew;
                        }
                    }

                    /* append if we've got room. */
                    if (cbBufReq <= cbStdErrBuf)
                    {
                        memcpy(&pszStdErrBuf[offStdErrBuf], achBuf, cbRead);
                        offStdErrBuf = offStdErrBuf + cbRead;
                        pszStdErrBuf[offStdErrBuf] = '\0';
                    }
                }
                else
                {
                    AssertLogRelMsg(vrc == VERR_BROKEN_PIPE, ("%Rrc\n", vrc));
                    RTPipeClose(hPipeR);
                    hPipeR = NIL_RTPIPE;
                }
            }

            /*
             * Service the process.  Block if we have no pipe.
             */
            if (hProcess != NIL_RTPROCESS)
            {
                vrc = RTProcWait(hProcess,
                                 hPipeR == NIL_RTPIPE ? RTPROCWAIT_FLAGS_BLOCK : RTPROCWAIT_FLAGS_NOBLOCK,
                                 &ProcStatus);
                if (RT_SUCCESS(vrc))
                    hProcess = NIL_RTPROCESS;
                else
                    AssertLogRelMsgStmt(vrc != VERR_PROCESS_RUNNING, ("%Rrc\n", vrc), hProcess = NIL_RTPROCESS);
            }
        } while (   hPipeR != NIL_RTPIPE
                 || hProcess != NIL_RTPROCESS);

        /*
         * Compose the status code and, on failure, error message.
         */
        LogRel(("ExtPack: enmReason=%d iStatus=%d stderr='%s'\n",
                ProcStatus.enmReason, ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : ""));

        if (   ProcStatus.enmReason == RTPROCEXITREASON_NORMAL
            && ProcStatus.iStatus   == 0
            && offStdErrBuf         == 0)
            hrc = S_OK;
        else if (ProcStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {
            AssertMsg(ProcStatus.iStatus != 0, ("%s\n", pszStdErrBuf));
            hrc = setError(E_UNEXPECTED, tr("The installer failed with exit code %d: %s"),
                           ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : "");
        }
        else if (ProcStatus.enmReason == RTPROCEXITREASON_SIGNAL)
            hrc = setError(E_UNEXPECTED, tr("The installer was killed by signal #d (stderr: %s)"),
                           ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : "");
        else if (ProcStatus.enmReason == RTPROCEXITREASON_ABEND)
            hrc = setError(E_UNEXPECTED, tr("The installer aborted abnormally (stderr: %s)"),
                           offStdErrBuf ? pszStdErrBuf : "");
        else
            hrc = setError(E_UNEXPECTED, tr("internal error: enmReason=%d iStatus=%d stderr='%s'"),
                           ProcStatus.enmReason, ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : "");

        RTMemFree(pszStdErrBuf);
    }
    else
        hrc = setError(VBOX_E_IPRT_ERROR, tr("Failed to launch the helper application '%s' (%Rrc)"), szExecName, vrc);

    RTPipeClose(hPipeR);
    RTPipeClose(hStdErrPipe.u.hPipe);

    return hrc;
}

/**
 * Finds an installed extension pack.
 *
 * @returns Pointer to the extension pack if found, NULL if not. (No reference
 *          counting problem here since the caller must be holding the lock.)
 * @param   a_pszName       The name of the extension pack.
 */
ExtPack *ExtPackManager::findExtPack(const char *a_pszName)
{
    size_t cchName = strlen(a_pszName);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
    {
        ExtPack::Data *pExtPackData = (*it)->m;
        if (   pExtPackData
            && pExtPackData->Desc.strName.length() == cchName
            && pExtPackData->Desc.strName.compare(a_pszName, iprt::MiniString::CaseInsensitive) == 0)
            return (*it);
    }
    return NULL;
}

/**
 * Removes an installed extension pack from the internal list.
 *
 * The package is expected to exist!
 *
 * @param   a_pszName       The name of the extension pack.
 */
void ExtPackManager::removeExtPack(const char *a_pszName)
{
    size_t cchName = strlen(a_pszName);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
    {
        ExtPack::Data *pExtPackData = (*it)->m;
        if (   pExtPackData
            && pExtPackData->Desc.strName.length() == cchName
            && pExtPackData->Desc.strName.compare(a_pszName, iprt::MiniString::CaseInsensitive) == 0)
        {
            m->llInstalledExtPacks.erase(it);
            return;
        }
    }
    AssertMsgFailed(("%s\n", a_pszName));
}

/**
 * Refreshes the specified extension pack.
 *
 * This may remove the extension pack from the list, so any non-smart pointers
 * to the extension pack object may become invalid.
 *
 * @returns S_OK and *ppExtPack on success, COM status code and error message
 *          on failure.
 * @param   a_pszName           The extension to update..
 * @param   a_fUnsuableIsError  If @c true, report an unusable extension pack
 *                              as an error.
 * @param   a_ppExtPack         Where to store the pointer to the extension
 *                              pack of it is still around after the refresh.
 *                              This is optional.
 * @remarks Caller holds the extension manager lock.
 */
HRESULT ExtPackManager::refreshExtPack(const char *a_pszName, bool a_fUnsuableIsError, ExtPack **a_ppExtPack)
{
    HRESULT hrc;
    ExtPack *pExtPack = findExtPack(a_pszName);
    if (pExtPack)
    {
        /*
         * Refresh existing object.
         */
        bool fCanDelete;
        hrc = pExtPack->refresh(&fCanDelete);
        if (SUCCEEDED(hrc))
        {
            if (fCanDelete)
            {
                removeExtPack(a_pszName);
                pExtPack = NULL;
            }
        }
    }
    else
    {
        /*
         * Does the dir exist?  Make some special effort to deal with case
         * sensitivie file systems (a_pszName is case insensitive).
         */
        char szDir[RTPATH_MAX];
        int vrc = RTPathJoin(szDir, sizeof(szDir), m->strBaseDir.c_str(), a_pszName);
        AssertLogRelRCReturn(vrc, E_FAIL);

        RTDIRENTRYEX    Entry;
        RTFSOBJINFO     ObjInfo;
        vrc = RTPathQueryInfoEx(szDir, &ObjInfo, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
        bool fExists = RT_SUCCESS(vrc) && RTFS_IS_DIRECTORY(ObjInfo.Attr.fMode);
        if (!fExists)
        {
            PRTDIR pDir;
            vrc = RTDirOpen(&pDir, m->strBaseDir.c_str());
            if (RT_SUCCESS(vrc))
            {
                for (;;)
                {
                    vrc = RTDirReadEx(pDir, &Entry, NULL /*pcbDirEntry*/, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                    if (RT_FAILURE(vrc))
                    {
                        AssertLogRelMsg(vrc == VERR_NO_MORE_FILES, ("%Rrc\n", vrc));
                        break;
                    }
                    if (   RTFS_IS_DIRECTORY(Entry.Info.Attr.fMode)
                        && !RTStrICmp(Entry.szName, a_pszName))
                    {
                        /*
                         * The installed extension pack has a uses different case.
                         * Update the name and directory variables.
                         */
                        vrc = RTPathJoin(szDir, sizeof(szDir), m->strBaseDir.c_str(), Entry.szName); /* not really necessary */
                        AssertLogRelRCReturnStmt(vrc, E_UNEXPECTED, RTDirClose(pDir));
                        a_pszName = Entry.szName;
                        fExists   = true;
                        break;
                    }
                }
                RTDirClose(pDir);
            }
        }
        if (fExists)
        {
            /*
             * We've got something, create a new extension pack object for it.
             */
            ComObjPtr<ExtPack> NewExtPack;
            hrc = NewExtPack.createObject();
            if (SUCCEEDED(hrc))
                hrc = NewExtPack->init(m->pVirtualBox, a_pszName, m->strBaseDir.c_str());
            if (SUCCEEDED(hrc))
            {
                m->llInstalledExtPacks.push_back(NewExtPack);
                if (NewExtPack->m->fUsable)
                    LogRel(("ExtPackManager: Found extension pack '%s'.\n", a_pszName));
                else
                    LogRel(("ExtPackManager: Found bad extension pack '%s': %s\n",
                            a_pszName, NewExtPack->m->strWhyUnusable.c_str() ));
                pExtPack = NewExtPack;
            }
        }
        else
            hrc = S_OK;
    }

    /*
     * Report error if not usable, if that is desired.
     */
    if (   SUCCEEDED(hrc)
        && pExtPack
        && a_fUnsuableIsError
        && !pExtPack->m->fUsable)
        hrc = setError(E_FAIL, "%s", pExtPack->m->strWhyUnusable.c_str());

    if (a_ppExtPack)
        *a_ppExtPack = pExtPack;
    return hrc;
}


/**
 * Processes anything new in the drop zone.
 */
void ExtPackManager::processDropZone(void)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (FAILED(hrc))
        return;
    AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);

    if (m->strDropZoneDir.isEmpty())
        return;

    PRTDIR pDir;
    int vrc = RTDirOpen(&pDir, m->strDropZoneDir.c_str());
    if (RT_FAILURE(vrc))
        return;
    for (;;)
    {
        RTDIRENTRYEX Entry;
        vrc = RTDirReadEx(pDir, &Entry, NULL /*pcbDirEntry*/, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
        if (RT_FAILURE(vrc))
        {
            AssertMsg(vrc == VERR_NO_MORE_FILES, ("%Rrc\n", vrc));
            break;
        }

        /*
         * We're looking for files with the right extension.  Symbolic links
         * will be ignored.
         */
        if (   RTFS_IS_FILE(Entry.Info.Attr.fMode)
            && RTStrICmp(RTPathExt(Entry.szName), VBOX_EXTPACK_SUFFIX) == 0)
        {
            /* We create (and check for) a blocker file to prevent this
               extension pack from being installed more than once. */
            char szPath[RTPATH_MAX];
            vrc = RTPathJoin(szPath, sizeof(szPath), m->strDropZoneDir.c_str(), Entry.szName);
            if (RT_SUCCESS(vrc))
                vrc = RTPathAppend(szPath, sizeof(szPath), "-done");
            AssertRC(vrc);
            if (RT_SUCCESS(vrc))
            {
                RTFILE hFile;
                vrc = RTFileOpen(&hFile, szPath,  RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_CREATE);
                if (RT_SUCCESS(vrc))
                {
                    /* Construct the full path to the extension pack and invoke
                       the Install method to install it.  Write errors to the
                       done file. */
                    vrc = RTPathJoin(szPath, sizeof(szPath), m->strDropZoneDir.c_str(), Entry.szName); AssertRC(vrc);
                    Bstr strName;
                    hrc = Install(Bstr(szPath).raw(), strName.asOutParam());
                    if (SUCCEEDED(hrc))
                        RTFileWrite(hFile, "succeeded\n", sizeof("succeeded\n"), NULL);
                    else
                    {
                        Utf8Str strErr;
                        com::ErrorInfo Info;
                        if (Info.isFullAvailable())
                            strErr.printf("failed\n"
                                          "%ls\n"
                                          "Details: code %Rhrc (%#RX32), component %ls, interface %ls, callee %ls\n"
                                          ,
                                          Info.getText().raw(),
                                          Info.getResultCode(),
                                          Info.getResultCode(),
                                          Info.getComponent().raw(),
                                          Info.getInterfaceName().raw(),
                                          Info.getCalleeName().raw());
                        else
                            strErr.printf("failed\n"
                                          "hrc=%Rhrc (%#RX32)\n", hrc, hrc);
                        RTFileWrite(hFile, strErr.c_str(), strErr.length(), NULL);
                    }
                    RTFileClose(hFile);
                }
            }
        }
    } /* foreach dir entry */
    RTDirClose(pDir);
}


/**
 * Calls the pfnVirtualBoxReady hook for all working extension packs.
 */
void ExtPackManager::callAllVirtualBoxReadyHooks(void)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (FAILED(hrc))
        return;
    AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
        (*it)->callVirtualBoxReadyHook(m->pVirtualBox);
}

/**
 * Calls the pfnVMCreated hook for all working extension packs.
 *
 * @param   a_pMachine          The machine interface of the new VM.
 */
void ExtPackManager::callAllVmCreatedHooks(IMachine *a_pMachine)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (FAILED(hrc))
        return;
    AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
        (*it)->callVmCreatedHook(m->pVirtualBox, a_pMachine);
}

/**
 * Calls the pfnVMConfigureVMM hook for all working extension packs.
 *
 * @returns VBox status code.  Stops on the first failure, expecting the caller
 *          to signal this to the caller of the CFGM constructor.
 * @param   a_pConsole          The console interface for the VM.
 * @param   a_pVM               The VM handle.
 */
int ExtPackManager::callAllVmConfigureVmmHooks(IConsole *a_pConsole, PVM a_pVM)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (FAILED(hrc))
        return Global::vboxStatusCodeFromCOM(hrc);
    AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
    {
        int vrc = (*it)->callVmConfigureVmmHook(a_pConsole, a_pVM);
        if (RT_FAILURE(vrc))
            return vrc;
    }

    return VINF_SUCCESS;
}

/**
 * Calls the pfnVMPowerOn hook for all working extension packs.
 *
 * @returns VBox status code.  Stops on the first failure, expecting the caller
 *          to not power on the VM.
 * @param   a_pConsole          The console interface for the VM.
 * @param   a_pVM               The VM handle.
 */
int ExtPackManager::callAllVmPowerOnHooks(IConsole *a_pConsole, PVM a_pVM)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (FAILED(hrc))
        return Global::vboxStatusCodeFromCOM(hrc);
    AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
    {
        int vrc = (*it)->callVmPowerOnHook(a_pConsole, a_pVM);
        if (RT_FAILURE(vrc))
            return vrc;
    }

    return VINF_SUCCESS;
}

/**
 * Calls the pfnVMPowerOff hook for all working extension packs.
 *
 * @param   a_pConsole          The console interface for the VM.
 * @param   a_pVM               The VM handle. Can be NULL.
 */
void ExtPackManager::callAllVmPowerOffHooks(IConsole *a_pConsole, PVM a_pVM)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (FAILED(hrc))
        return;
    AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         it++)
        (*it)->callVmPowerOnHook(a_pConsole, a_pVM);
}


/**
 * Checks that the specified extension pack contains a VRDE module and that it
 * is shipshape.
 *
 * @returns S_OK if ok, appropriate failure status code with details.
 * @param   a_pstrExtPack   The name of the extension pack.
 */
HRESULT ExtPackManager::checkVrdeExtPack(Utf8Str const *a_pstrExtPack)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        ExtPack *pExtPack = findExtPack(a_pstrExtPack->c_str());
        if (pExtPack)
            hrc = pExtPack->checkVrde();
        else
            hrc = setError(VBOX_E_OBJECT_NOT_FOUND, tr("No extension pack by the name '%s' was found"), a_pstrExtPack->c_str());
    }

    return hrc;
}

/**
 * Gets the full path to the VRDE library of the specified extension pack.
 *
 * This will do extacly the same as checkVrdeExtPack and then resolve the
 * library path.
 *
 * @returns S_OK if a path is returned, COM error status and message return if
 *          not.
 * @param   a_pstrExtPack       The extension pack.
 * @param   a_pstrVrdeLibrary   Where to return the path.
 */
int ExtPackManager::getVrdeLibraryPathForExtPack(Utf8Str const *a_pstrExtPack, Utf8Str *a_pstrVrdeLibrary)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        ExtPack *pExtPack = findExtPack(a_pstrExtPack->c_str());
        if (pExtPack)
            hrc = pExtPack->getVrdpLibraryName(a_pstrVrdeLibrary);
        else
            hrc = setError(VBOX_E_OBJECT_NOT_FOUND, tr("No extension pack by the name '%s' was found"), a_pstrExtPack->c_str());
    }

    return hrc;
}

/**
 * Gets the name of the default VRDE extension pack.
 *
 * @returns S_OK or some COM error status on red tape failure.
 * @param   a_pstrExtPack   Where to return the extension pack name.  Returns
 *                          empty if no extension pack wishes to be the default
 *                          VRDP provider.
 */
HRESULT ExtPackManager::getDefaultVrdeExtPack(Utf8Str *a_pstrExtPack)
{
    a_pstrExtPack->setNull();

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
             it != m->llInstalledExtPacks.end();
             it++)
        {
            if ((*it)->wantsToBeDefaultVrde())
            {
                *a_pstrExtPack = (*it)->m->Desc.strName;
                break;
            }
        }
    }
    return hrc;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
