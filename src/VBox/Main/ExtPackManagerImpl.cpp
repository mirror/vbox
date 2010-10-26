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

#include <iprt/dir.h>
#include <iprt/ldr.h>
#include <iprt/param.h>
#include <iprt/path.h>

#include <VBox/com/array.h>
#include "AutoCaller.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Private extension pack data.
 */
struct ExtPack::Data
{
    /** @name IExtPack Attributes.
     * @{ */
    /** The name. */
    Utf8Str     strName;
    /** The version string. */
    Utf8Str     strVersion;
    /** The revision. */
    uint32_t    uRevision;
    /** Whether it's usable or not. */
    bool        fUsable;
    /** Why it is unusable. */
    Utf8Str     strWhyUnusable;
    /** @}  */

    /** Where the extension pack is located. */
    Utf8Str     strPath;
    /** The module handle of the main extension pack module. */
    RTLDRMOD    hMainMod;
};

/** List of extension packs. */
typedef std::list< ComObjPtr<ExtPack> > ExtPackList;

/**
 * Private extension pack manager data.
 */
struct ExtPackManager::Data
{
    /** Where the directory where the extension packs are installed. */
    Utf8Str     strBasePath;
    /** The list of installed extension packs. */
    ExtPackList llInstalledExtPacks;
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
 * @param   a_pszName       The name of the extension pack.  This is also the
 *                          name of the subdirector under @a a_pszParentDir
 *                          where the extension pack is installed.
 * @param   a_pszParentDir  The parent directory.
 */
HRESULT ExtPack::init(const char *a_pszName, const char *a_pszParentDir)
{
    /*
     * Figure out where we live and allocate+initialize our private data.
     */
    char szDir[RTPATH_MAX];
    int vrc = RTPathJoin(szDir, sizeof(szDir), a_pszParentDir, a_pszName);
    AssertLogRelRCReturn(vrc, E_FAIL);

    m = new Data;
    m->strName          = a_pszName;
    m->strVersion       = "";
    m->uRevision        = 0;
    m->fUsable          = false;
    m->strWhyUnusable   = tr("ExtPack::init failed");

    m->strPath          = szDir;
    m->hMainMod         = NIL_RTLDRMOD;

    /*
     * Read the description file.
     */


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
            RTLdrClose(m->hMainMod);
            m->hMainMod = NIL_RTLDRMOD;
        }

        delete m;
        m = NULL;
    }
}




STDMETHODIMP ExtPack::COMGETTER(Name)(BSTR *a_pbstrName)
{
    CheckComArgOutPointerValid(a_pbstrName);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        m->strName.cloneTo(a_pbstrName);
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(Version)(BSTR *a_pbstrVersion)
{
    CheckComArgOutPointerValid(a_pbstrVersion);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        m->strVersion.cloneTo(a_pbstrVersion);
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(Revision)(ULONG *a_puRevision)
{
    CheckComArgOutPointerValid(a_puRevision);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_puRevision = m->uRevision;
    return hrc;
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



void *ExtPack::getCallbackTable()
{
    return NULL;
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
 */
HRESULT ExtPackManager::init()
{
    /*
     * Figure some stuff out before creating the instance data.
     */
    char szBasePath[RTPATH_MAX];
    int rc = RTPathAppPrivateArch(szBasePath, sizeof(szBasePath));
    AssertLogRelRCReturn(rc, E_FAIL);
    rc = RTPathAppend(szBasePath, sizeof(szBasePath), "ExtensionPacks");
    AssertLogRelRCReturn(rc, E_FAIL);

    /*
     * Allocate and initialize the instance data.
     */
    m = new Data;
    m->strBasePath = szBasePath;

    /*
     * Go looking for extensions.  The RTDirOpen may fail if nothing has been
     * installed yet, or if root is paranoid and has revoked our access to them.
     *
     * We ASSUME that there are no files, directories or stuff in the directory
     * that exceed the max name length in RTDIRENTRYEX.
     */
    PRTDIR pDir;
    int vrc = RTDirOpen(&pDir, szBasePath);
    if (RT_FAILURE(vrc))
        return S_OK;
    HRESULT hrc = S_OK;
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
            && strcmp(Entry.szName, "..") != 0 )
        {
            /*
             * All directories are extensions, the shall be nothing but
             * extensions in this subdirectory.
             */
            ComObjPtr<ExtPack> NewExtPack;
            HRESULT hrc2 = NewExtPack.createObject();
            if (SUCCEEDED(hrc2))
                hrc2 = NewExtPack->init(Entry.szName, szBasePath);
            if (SUCCEEDED(hrc2))
                m->llInstalledExtPacks.push_back(NewExtPack);
            else if (SUCCEEDED(rc))
                hrc = hrc2;
        }
    }
    RTDirClose(pDir);

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
        /** @todo do unload notifications */

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

        hrc = VBOX_E_OBJECT_NOT_FOUND;
        for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
             it != m->llInstalledExtPacks.end();
             it++)
        {
            ExtPack::Data *pExtPackData = (*it)->m;
            if (   pExtPackData
                && pExtPackData->strName.compare(strName) == 0)
            {
                (*it).queryInterfaceTo(a_pExtPack);
                hrc = S_OK;
                break;
            }
        }
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Install(IN_BSTR a_bstrTarball, BSTR *a_pbstrName)
{
    NOREF(a_bstrTarball); NOREF(a_pbstrName);
    return E_NOTIMPL;
}

STDMETHODIMP ExtPackManager::Uninstall(IN_BSTR a_bstrName, BOOL a_fForcedRemoval)
{
    NOREF(a_bstrName); NOREF(a_fForcedRemoval);
    return E_NOTIMPL;
}



int ExtPackManager::callAllConfigHooks(IConsole *a_pConsole, PVM a_pVM)
{
    NOREF(a_pConsole); NOREF(a_pVM);
    return VINF_SUCCESS;
}

int ExtPackManager::callAllNewMachineHooks(IMachine *a_pMachine)
{
    NOREF(a_pMachine);
    return VINF_SUCCESS;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
