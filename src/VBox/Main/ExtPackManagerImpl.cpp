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
#include <iprt/manifest.h>
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
# define VBOX_EXTPACK_HELPER_NAME       "VBoxExtPackHelperApp.exe"
#else
# define VBOX_EXTPACK_HELPER_NAME       "VBoxExtPackHelperApp"
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
struct ExtPackBaseData
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
};

/**
 * Private extension pack data.
 */
struct ExtPackFile::Data : public ExtPackBaseData
{
public:
    /** The path to the tarball. */
    Utf8Str             strExtPackFile;
    /** The file handle of the extension pack file. */
    RTFILE              hExtPackFile;
    /** Our manifest for the tarball. */
    RTMANIFEST          hOurManifest;
    /** Pointer to the extension pack manager. */
    ComObjPtr<ExtPackManager> ptrExtPackMgr;

    RTMEMEF_NEW_AND_DELETE_OPERATORS();
};

/**
 * Private extension pack data.
 */
struct ExtPack::Data : public ExtPackBaseData
{
public:
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
    /** The current context. */
    VBOXEXTPACKCTX      enmContext;
    /** Set if we've made the pfnVirtualBoxReady or pfnConsoleReady call. */
    bool                fMadeReadyCall;

    RTMEMEF_NEW_AND_DELETE_OPERATORS();
};

/** List of extension packs. */
typedef std::list< ComObjPtr<ExtPack> > ExtPackList;

/**
 * Private extension pack manager data.
 */
struct ExtPackManager::Data
{
    /** The directory where the extension packs are installed. */
    Utf8Str             strBaseDir;
    /** The directory where the certificates this installation recognizes are
     * stored. */
    Utf8Str             strCertificatDirPath;
    /** The list of installed extension packs. */
    ExtPackList         llInstalledExtPacks;
    /** Pointer to the VirtualBox object, our parent. */
    VirtualBox         *pVirtualBox;
    /** The current context. */
    VBOXEXTPACKCTX      enmContext;

    RTMEMEF_NEW_AND_DELETE_OPERATORS();
};


DEFINE_EMPTY_CTOR_DTOR(ExtPackFile)

/**
 * Called by ComObjPtr::createObject when creating the object.
 *
 * Just initialize the basic object state, do the rest in initWithDir().
 *
 * @returns S_OK.
 */
HRESULT ExtPackFile::FinalConstruct()
{
    m = NULL;
    return S_OK;
}

/**
 * Initializes the extension pack by reading its file.
 *
 * @returns COM status code.
 * @param   a_pszFile       The path to the extension pack file.
 * @param   a_pExtPackMgr   Pointer to the extension pack manager.
 */
HRESULT ExtPackFile::initWithFile(const char *a_pszFile, ExtPackManager *a_pExtPackMgr)
{
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /*
     * Allocate + initialize our private data.
     */
    m = new ExtPackFile::Data;
    m->Desc.strName                 = NULL;
    RT_ZERO(m->ObjInfoDesc);
    m->fUsable                      = false;
    m->strWhyUnusable               = tr("ExtPack::init failed");
    m->strExtPackFile               = a_pszFile;
    m->hExtPackFile                 = NIL_RTFILE;
    m->hOurManifest                 = NIL_RTMANIFEST;
    m->ptrExtPackMgr                = a_pExtPackMgr;

    iprt::MiniString *pstrTarName = VBoxExtPackExtractNameFromTarballPath(a_pszFile);
    if (pstrTarName)
    {
        m->Desc.strName = *pstrTarName;
        delete pstrTarName;
        pstrTarName = NULL;
    }

    autoInitSpan.setSucceeded();

    /*
     * Try open the extension pack and check that it is a regular file.
     */
    int vrc = RTFileOpen(&m->hExtPackFile, a_pszFile,
                         RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN);
    if (RT_FAILURE(vrc))
    {
        if (vrc == VERR_FILE_NOT_FOUND || vrc == VERR_PATH_NOT_FOUND)
            return initFailed(tr("'%s' file not found"), a_pszFile);
        return initFailed(tr("RTFileOpen('%s',,) failed with %Rrc"), a_pszFile, vrc);
    }

    RTFSOBJINFO ObjInfo;
    vrc = RTFileQueryInfo(m->hExtPackFile, &ObjInfo, RTFSOBJATTRADD_UNIX);
    if (RT_FAILURE(vrc))
        return initFailed(tr("RTFileQueryInfo failed with %Rrc on '%s'"), vrc, a_pszFile);
    if (!RTFS_IS_FILE(ObjInfo.Attr.fMode))
        return initFailed(tr("Not a regular file: %s"), a_pszFile);

    /*
     * Validate the tarball and extract the XML file.
     */
    char        szError[8192];
    RTVFSFILE   hXmlFile;
    vrc = VBoxExtPackValidateTarball(m->hExtPackFile, NULL /*pszExtPackName*/, a_pszFile,
                                     szError, sizeof(szError), &m->hOurManifest, &hXmlFile);
    if (RT_FAILURE(vrc))
        return initFailed(tr("%s"), szError);

    /*
     * Parse the XML.
     */
    iprt::MiniString strSavedName(m->Desc.strName);
    iprt::MiniString *pStrLoadErr = VBoxExtPackLoadDescFromVfsFile(hXmlFile, &m->Desc, &m->ObjInfoDesc);
    RTVfsFileRelease(hXmlFile);
    if (pStrLoadErr != NULL)
    {
        m->strWhyUnusable.printf(tr("Failed to the xml file: %s"), pStrLoadErr->c_str());
        m->Desc.strName = strSavedName;
        delete pStrLoadErr;
        return S_OK;
    }

    /*
     * Match the tarball name with the name from the XML.
     */
    /** @todo drop this restriction after the old install interface is
     *        dropped. */
    if (!strSavedName.equalsIgnoreCase(m->Desc.strName))
        return initFailed(tr("Extension pack name mismatch between the downloaded file and the XML inside it (xml='%s' file='%s')"),
                          m->Desc.strName.c_str(), strSavedName.c_str());

    m->fUsable = true;
    m->strWhyUnusable.setNull();
    return S_OK;
}

/**
 * Protected helper that formats the strExtPackFile value.
 *
 * @returns S_OK
 * @param   a_pszWhyFmt         Why it failed, format string.
 * @param   ...                 The format arguments.
 */
HRESULT ExtPackFile::initFailed(const char *a_pszWhyFmt, ...)
{
    va_list va;
    va_start(va, a_pszWhyFmt);
    m->strExtPackFile.printfV(a_pszWhyFmt, va);
    va_end(va);
    return S_OK;
}

/**
 * COM cruft.
 */
void ExtPackFile::FinalRelease()
{
    uninit();
}

/**
 * Do the actual cleanup.
 */
void ExtPackFile::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (!autoUninitSpan.uninitDone() && m != NULL)
    {
        VBoxExtPackFreeDesc(&m->Desc);
        RTFileClose(m->hExtPackFile);
        m->hExtPackFile = NIL_RTFILE;
        RTManifestRelease(m->hOurManifest);
        m->hOurManifest = NIL_RTMANIFEST;

        delete m;
        m = NULL;
    }
}

STDMETHODIMP ExtPackFile::COMGETTER(Name)(BSTR *a_pbstrName)
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

STDMETHODIMP ExtPackFile::COMGETTER(Description)(BSTR *a_pbstrDescription)
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

STDMETHODIMP ExtPackFile::COMGETTER(Version)(BSTR *a_pbstrVersion)
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

STDMETHODIMP ExtPackFile::COMGETTER(Revision)(ULONG *a_puRevision)
{
    CheckComArgOutPointerValid(a_puRevision);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_puRevision = m->Desc.uRevision;
    return hrc;
}

STDMETHODIMP ExtPackFile::COMGETTER(VRDEModule)(BSTR *a_pbstrVrdeModule)
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

STDMETHODIMP ExtPackFile::COMGETTER(PlugIns)(ComSafeArrayOut(IExtPackPlugIn *, a_paPlugIns))
{
    /** @todo implement plug-ins. */
#ifdef VBOX_WITH_XPCOM
    NOREF(a_paPlugIns);
    NOREF(a_paPlugInsSize);
#endif
    ReturnComNotImplemented();
}

STDMETHODIMP ExtPackFile::COMGETTER(Usable)(BOOL *a_pfUsable)
{
    CheckComArgOutPointerValid(a_pfUsable);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_pfUsable = m->fUsable;
    return hrc;
}

STDMETHODIMP ExtPackFile::COMGETTER(WhyUnusable)(BSTR *a_pbstrWhy)
{
    CheckComArgOutPointerValid(a_pbstrWhy);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        m->strWhyUnusable.cloneTo(a_pbstrWhy);
    return hrc;
}

STDMETHODIMP ExtPackFile::COMGETTER(ShowLicense)(BOOL *a_pfShowIt)
{
    CheckComArgOutPointerValid(a_pfShowIt);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_pfShowIt = m->Desc.fShowLicense;
    return hrc;
}

STDMETHODIMP ExtPackFile::COMGETTER(License)(BSTR *a_pbstrHtmlLicense)
{
    Bstr bstrHtml("html");
    return QueryLicense(Bstr::Empty.raw(), Bstr::Empty.raw(), bstrHtml.raw(), a_pbstrHtmlLicense);
}

/* Same as ExtPack::QueryLicense, should really explore the subject of base classes here... */
STDMETHODIMP ExtPackFile::QueryLicense(IN_BSTR a_bstrPreferredLocale, IN_BSTR a_bstrPreferredLanguage, IN_BSTR a_bstrFormat,
                                       BSTR *a_pbstrLicense)
{
    /*
     * Validate input.
     */
    CheckComArgOutPointerValid(a_pbstrLicense);
    CheckComArgNotNull(a_bstrPreferredLocale);
    CheckComArgNotNull(a_bstrPreferredLanguage);
    CheckComArgNotNull(a_bstrFormat);

    Utf8Str strPreferredLocale(a_bstrPreferredLocale);
    if (strPreferredLocale.length() != 2 && strPreferredLocale.length() != 0)
        return setError(E_FAIL, tr("The preferred locale is a two character string or empty."));

    Utf8Str strPreferredLanguage(a_bstrPreferredLanguage);
    if (strPreferredLanguage.length() != 2 && strPreferredLanguage.length() != 0)
        return setError(E_FAIL, tr("The preferred lanuage is a two character string or empty."));

    Utf8Str strFormat(a_bstrFormat);
    if (   !strFormat.equals("html")
        && !strFormat.equals("rtf")
        && !strFormat.equals("txt"))
        return setError(E_FAIL, tr("The license format can only have the values 'html', 'rtf' and 'txt'."));

    /*
     * Combine the options to form a file name before locking down anything.
     */
    char szName[sizeof("ExtPack-license-de_DE.html") + 2];
    if (strPreferredLocale.isNotEmpty() && strPreferredLanguage.isNotEmpty())
        RTStrPrintf(szName, sizeof(szName), "ExtPack-license-%s_%s.%s",
                    strPreferredLocale.c_str(), strPreferredLanguage.c_str(), strFormat.c_str());
    else if (strPreferredLocale.isNotEmpty())
        RTStrPrintf(szName, sizeof(szName), "ExtPack-license-%s.%s",  strPreferredLocale.c_str(), strFormat.c_str());
    else if (strPreferredLanguage.isNotEmpty())
        RTStrPrintf(szName, sizeof(szName), "ExtPack-license-_%s.%s", strPreferredLocale.c_str(), strFormat.c_str());
    else
        RTStrPrintf(szName, sizeof(szName), "ExtPack-license.%s",     strFormat.c_str());

    /*
     * Effectuate the query.
     */
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        /*
         * Look it up in the manifest before scanning the tarball for it
         */
        if (RTManifestEntryExists(m->hOurManifest, szName))
        {
            RTVFSFSSTREAM   hTarFss;
            char            szError[8192];
            int vrc = VBoxExtPackOpenTarFss(m->hExtPackFile, szError, sizeof(szError), &hTarFss);
            if (RT_SUCCESS(vrc))
            {
                for (;;)
                {
                    /* Get the first/next. */
                    char           *pszName;
                    RTVFSOBJ        hVfsObj;
                    RTVFSOBJTYPE    enmType;
                    vrc = RTVfsFsStrmNext(hTarFss, &pszName, &enmType, &hVfsObj);
                    if (RT_FAILURE(vrc))
                    {
                        if (vrc != VERR_EOF)
                            hrc = setError(VBOX_E_IPRT_ERROR, tr("RTVfsFsStrmNext failed: %Rrc"), vrc);
                        else
                            hrc = setError(E_UNEXPECTED, tr("'%s' was found in the manifest but not in the tarball"), szName);
                        break;
                    }

                    /* Is this it? */
                    const char *pszAdjName = pszName[0] == '.' && pszName[1] == '/' ? &pszName[2] : pszName;
                    if (   !strcmp(pszAdjName, szName)
                        && (   enmType == RTVFSOBJTYPE_IO_STREAM
                            || enmType == RTVFSOBJTYPE_FILE))
                    {
                        RTVFSIOSTREAM hVfsIos = RTVfsObjToIoStream(hVfsObj);
                        RTVfsObjRelease(hVfsObj);
                        RTStrFree(pszName);

                        /* Load the file into memory. */
                        RTFSOBJINFO ObjInfo;
                        vrc = RTVfsIoStrmQueryInfo(hVfsIos, &ObjInfo, RTFSOBJATTRADD_NOTHING);
                        if (RT_SUCCESS(vrc))
                        {
                            size_t cbFile = (size_t)ObjInfo.cbObject;
                            void  *pvFile = RTMemAllocZ(cbFile + 1);
                            if (pvFile)
                            {
                                vrc = RTVfsIoStrmRead(hVfsIos, pvFile, cbFile, true /*fBlocking*/, NULL);
                                if (RT_SUCCESS(vrc))
                                {
                                    /* try translate it into a string we can return. */
                                    Bstr bstrLicense((const char *)pvFile, cbFile);
                                    if (bstrLicense.isNotEmpty())
                                    {
                                        bstrLicense.detachTo(a_pbstrLicense);
                                        hrc = S_OK;
                                    }
                                    else
                                        hrc = setError(VBOX_E_IPRT_ERROR,
                                                       tr("The license file '%s' is empty or contains invalid UTF-8 encoding"),
                                                       szName);
                                }
                                else
                                    hrc = setError(VBOX_E_IPRT_ERROR, tr("Failed to read '%s': %Rrc"), szName, vrc);
                                RTMemFree(pvFile);
                            }
                            else
                                hrc = setError(E_OUTOFMEMORY, tr("Failed to allocate %zu bytes for '%s'"), cbFile, szName);
                        }
                        else
                            hrc = setError(VBOX_E_IPRT_ERROR, tr("RTVfsIoStrmQueryInfo on '%s': %Rrc"), szName, vrc);
                        RTVfsIoStrmRelease(hVfsIos);
                        break;
                    }

                    /* Release current. */
                    RTVfsObjRelease(hVfsObj);
                    RTStrFree(pszName);
                }
                RTVfsFsStrmRelease(hTarFss);
            }
            else
                hrc = setError(VBOX_E_OBJECT_NOT_FOUND, tr("%s"), szError);
        }
        else
            hrc = setError(VBOX_E_OBJECT_NOT_FOUND, tr("The license file '%s' was not found in '%s'"),
                           szName, m->strExtPackFile.c_str());
    }
    return hrc;
}

STDMETHODIMP ExtPackFile::COMGETTER(FilePath)(BSTR *a_pbstrPath)
{
    CheckComArgOutPointerValid(a_pbstrPath);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        m->strExtPackFile.cloneTo(a_pbstrPath);
    return hrc;
}

STDMETHODIMP ExtPackFile::Install(void)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        if (m->fUsable)
            hrc = m->ptrExtPackMgr->doInstall(this);
        else
            hrc = setError(E_FAIL, "%s", m->strWhyUnusable.c_str());
    }
    return hrc;
}





DEFINE_EMPTY_CTOR_DTOR(ExtPack)

/**
 * Called by ComObjPtr::createObject when creating the object.
 *
 * Just initialize the basic object state, do the rest in initWithDir().
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
 * @param   a_enmContext    The context we're in.
 * @param   a_pszName       The name of the extension pack.  This is also the
 *                          name of the subdirector under @a a_pszParentDir
 *                          where the extension pack is installed.
 * @param   a_pszDir        The extension pack directory name.
 */
HRESULT ExtPack::initWithDir(VBOXEXTPACKCTX a_enmContext, const char *a_pszName, const char *a_pszDir)
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
        /* pfnGetContext        = */ ExtPack::hlpGetContext,
        /* pfnReserved1         = */ ExtPack::hlpReservedN,
        /* pfnReserved2         = */ ExtPack::hlpReservedN,
        /* pfnReserved3         = */ ExtPack::hlpReservedN,
        /* pfnReserved4         = */ ExtPack::hlpReservedN,
        /* pfnReserved5         = */ ExtPack::hlpReservedN,
        /* pfnReserved6         = */ ExtPack::hlpReservedN,
        /* pfnReserved7         = */ ExtPack::hlpReservedN,
        /* pfnReserved8         = */ ExtPack::hlpReservedN,
        /* pfnReserved9         = */ ExtPack::hlpReservedN,
        /* u32EndMarker         = */ VBOXEXTPACKHLP_VERSION
    };

    /*
     * Allocate + initialize our private data.
     */
    m = new Data;
    m->Desc.strName                 = a_pszName;
    RT_ZERO(m->ObjInfoDesc);
    m->fUsable                      = false;
    m->strWhyUnusable               = tr("ExtPack::init failed");
    m->strExtPackPath               = a_pszDir;
    RT_ZERO(m->ObjInfoExtPack);
    m->strMainModPath.setNull();
    RT_ZERO(m->ObjInfoMainMod);
    m->hMainMod                     = NIL_RTLDRMOD;
    m->Hlp                          = s_HlpTmpl;
    m->Hlp.pszVBoxVersion           = RTBldCfgVersion();
    m->Hlp.uVBoxInternalRevision    = RTBldCfgRevision();
    m->pThis                        = this;
    m->pReg                         = NULL;
    m->enmContext                   = a_enmContext;
    m->fMadeReadyCall               = false;

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
 * @returns true if we left the lock, false if we didn't.
 * @param   a_pVirtualBox       The VirtualBox interface.
 * @param   a_pLock             The write lock held by the caller.
 */
bool    ExtPack::callInstalledHook(IVirtualBox *a_pVirtualBox, AutoWriteLock *a_pLock)
{
    if (   m != NULL
        && m->hMainMod != NIL_RTLDRMOD)
    {
        if (m->pReg->pfnInstalled)
        {
            ComPtr<ExtPack> ptrSelfRef = this;
            a_pLock->release();
            m->pReg->pfnInstalled(m->pReg, a_pVirtualBox);
            a_pLock->acquire();
            return true;
        }
    }
    return false;
}

/**
 * Calls the uninstall hook and closes the module.
 *
 * @returns S_OK or COM error status with error information.
 * @param   a_pVirtualBox       The VirtualBox interface.
 * @param   a_fForcedRemoval    When set, we'll ignore complaints from the
 *                              uninstall hook.
 * @remarks The caller holds the manager's write lock, not released.
 */
HRESULT ExtPack::callUninstallHookAndClose(IVirtualBox *a_pVirtualBox, bool a_fForcedRemoval)
{
    HRESULT hrc = S_OK;

    if (   m != NULL
        && m->hMainMod != NIL_RTLDRMOD)
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
 * @returns true if we left the lock, false if we didn't.
 * @param   a_pVirtualBox       The VirtualBox interface.
 * @param   a_pLock             The write lock held by the caller.
 */
bool ExtPack::callVirtualBoxReadyHook(IVirtualBox *a_pVirtualBox, AutoWriteLock *a_pLock)
{
    if (    m != NULL
        &&  m->fUsable
        && !m->fMadeReadyCall)
    {
        m->fMadeReadyCall = true;
        if (m->pReg->pfnVirtualBoxReady)
        {
            ComPtr<ExtPack> ptrSelfRef = this;
            a_pLock->release();
            m->pReg->pfnVirtualBoxReady(m->pReg, a_pVirtualBox);
            a_pLock->acquire();
            return true;
        }
    }
    return false;
}

/**
 * Calls the pfnConsoleReady hook.
 *
 * @returns true if we left the lock, false if we didn't.
 * @param   a_pConsole          The Console interface.
 * @param   a_pLock             The write lock held by the caller.
 */
bool ExtPack::callConsoleReadyHook(IConsole *a_pConsole, AutoWriteLock *a_pLock)
{
    if (    m != NULL
        &&  m->fUsable
        && !m->fMadeReadyCall)
    {
        m->fMadeReadyCall = true;
        if (m->pReg->pfnConsoleReady)
        {
            ComPtr<ExtPack> ptrSelfRef = this;
            a_pLock->release();
            m->pReg->pfnConsoleReady(m->pReg, a_pConsole);
            a_pLock->acquire();
            return true;
        }
    }
    return false;
}

/**
 * Calls the pfnVMCreate hook.
 *
 * @returns true if we left the lock, false if we didn't.
 * @param   a_pVirtualBox       The VirtualBox interface.
 * @param   a_pMachine          The machine interface of the new VM.
 * @param   a_pLock             The write lock held by the caller.
 */
bool ExtPack::callVmCreatedHook(IVirtualBox *a_pVirtualBox, IMachine *a_pMachine, AutoWriteLock *a_pLock)
{
    if (   m != NULL
        && m->fUsable)
    {
        if (m->pReg->pfnVMCreated)
        {
            ComPtr<ExtPack> ptrSelfRef = this;
            a_pLock->release();
            m->pReg->pfnVMCreated(m->pReg, a_pVirtualBox, a_pMachine);
            a_pLock->acquire();
            return true;
        }
    }
    return false;
}

/**
 * Calls the pfnVMConfigureVMM hook.
 *
 * @returns true if we left the lock, false if we didn't.
 * @param   a_pConsole          The console interface.
 * @param   a_pVM               The VM handle.
 * @param   a_pLock             The write lock held by the caller.
 * @param   a_pvrc              Where to return the status code of the
 *                              callback.  This is always set.  LogRel is
 *                              called on if a failure status is returned.
 */
bool ExtPack::callVmConfigureVmmHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock, int *a_pvrc)
{
    *a_pvrc = VINF_SUCCESS;
    if (   m != NULL
        && m->fUsable)
    {
        if (m->pReg->pfnVMConfigureVMM)
        {
            ComPtr<ExtPack> ptrSelfRef = this;
            a_pLock->release();
            int vrc = m->pReg->pfnVMConfigureVMM(m->pReg, a_pConsole, a_pVM);
            *a_pvrc = vrc;
            a_pLock->acquire();
            if (RT_FAILURE(vrc))
                LogRel(("ExtPack pfnVMConfigureVMM returned %Rrc for %s\n", vrc, m->Desc.strName.c_str()));
            return true;
        }
    }
    return false;
}

/**
 * Calls the pfnVMPowerOn hook.
 *
 * @returns true if we left the lock, false if we didn't.
 * @param   a_pConsole          The console interface.
 * @param   a_pVM               The VM handle.
 * @param   a_pLock             The write lock held by the caller.
 * @param   a_pvrc              Where to return the status code of the
 *                              callback.  This is always set.  LogRel is
 *                              called on if a failure status is returned.
 */
bool ExtPack::callVmPowerOnHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock, int *a_pvrc)
{
    *a_pvrc = VINF_SUCCESS;
    if (   m != NULL
        && m->fUsable)
    {
        if (m->pReg->pfnVMPowerOn)
        {
            ComPtr<ExtPack> ptrSelfRef = this;
            a_pLock->release();
            int vrc = m->pReg->pfnVMPowerOn(m->pReg, a_pConsole, a_pVM);
            *a_pvrc = vrc;
            a_pLock->acquire();
            if (RT_FAILURE(vrc))
                LogRel(("ExtPack pfnVMPowerOn returned %Rrc for %s\n", vrc, m->Desc.strName.c_str()));
            return true;
        }
    }
    return false;
}

/**
 * Calls the pfnVMPowerOff hook.
 *
 * @returns true if we left the lock, false if we didn't.
 * @param   a_pConsole          The console interface.
 * @param   a_pVM               The VM handle.
 * @param   a_pLock             The write lock held by the caller.
 */
bool ExtPack::callVmPowerOffHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock)
{
    if (   m != NULL
        && m->fUsable)
    {
        if (m->pReg->pfnVMPowerOff)
        {
            ComPtr<ExtPack> ptrSelfRef = this;
            a_pLock->release();
            m->pReg->pfnVMPowerOff(m->pReg, a_pConsole, a_pVM);
            a_pLock->acquire();
            return true;
        }
    }
    return false;
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
    if (   m != NULL
        && m->fUsable)
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
 *
 * @param   a_pfCanDelete       Optional can-delete-this-object output indicator.
 *
 * @remarks Caller holds the extension manager lock for writing.
 * @remarks Only called in VBoxSVC.
 */
HRESULT ExtPack::refresh(bool *a_pfCanDelete)
{
    if (a_pfCanDelete)
        *a_pfCanDelete = false;

    AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS); /* for the COMGETTERs */

    /*
     * Has the module been deleted?
     */
    RTFSOBJINFO ObjInfoExtPack;
    int vrc = RTPathQueryInfoEx(m->strExtPackPath.c_str(), &ObjInfoExtPack, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
    if (   RT_FAILURE(vrc)
        || !RTFS_IS_DIRECTORY(ObjInfoExtPack.Attr.fMode))
    {
        if (a_pfCanDelete)
            *a_pfCanDelete = true;
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
    m->fMadeReadyCall = false;

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
            if (   VBOXEXTPACK_IS_MAJOR_VER_EQUAL(m->pReg->u32Version, VBOXEXTPACKREG_VERSION)
                && m->pReg->u32EndMarker == m->pReg->u32Version)
            {
                if (   (!m->pReg->pfnInstalled       || RT_VALID_PTR(m->pReg->pfnInstalled))
                    && (!m->pReg->pfnUninstall       || RT_VALID_PTR(m->pReg->pfnUninstall))
                    && (!m->pReg->pfnVirtualBoxReady || RT_VALID_PTR(m->pReg->pfnVirtualBoxReady))
                    && (!m->pReg->pfnConsoleReady    || RT_VALID_PTR(m->pReg->pfnConsoleReady))
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

/*static*/ DECLCALLBACK(VBOXEXTPACKCTX)
ExtPack::hlpGetContext(PCVBOXEXTPACKHLP pHlp)
{
    /*
     * Validate the input and get our bearings.
     */
    AssertPtrReturn(pHlp, VBOXEXTPACKCTX_INVALID);
    AssertReturn(pHlp->u32Version == VBOXEXTPACKHLP_VERSION, VBOXEXTPACKCTX_INVALID);
    ExtPack::Data *m = RT_FROM_CPP_MEMBER(pHlp, Data, Hlp);
    AssertPtrReturn(m, VBOXEXTPACKCTX_INVALID);
    ExtPack *pThis = m->pThis;
    AssertPtrReturn(pThis, VBOXEXTPACKCTX_INVALID);

    return pThis->m->enmContext;
}

/*static*/ DECLCALLBACK(int)
ExtPack::hlpReservedN(PCVBOXEXTPACKHLP pHlp)
{
    /*
     * Validate the input and get our bearings.
     */
    AssertPtrReturn(pHlp, VERR_INVALID_POINTER);
    AssertReturn(pHlp->u32Version == VBOXEXTPACKHLP_VERSION, VERR_INVALID_POINTER);
    ExtPack::Data *m = RT_FROM_CPP_MEMBER(pHlp, Data, Hlp);
    AssertPtrReturn(m, VERR_INVALID_POINTER);
    ExtPack *pThis = m->pThis;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    return VERR_NOT_IMPLEMENTED;
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

STDMETHODIMP ExtPack::COMGETTER(ShowLicense)(BOOL *a_pfShowIt)
{
    CheckComArgOutPointerValid(a_pfShowIt);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
        *a_pfShowIt = m->Desc.fShowLicense;
    return hrc;
}

STDMETHODIMP ExtPack::COMGETTER(License)(BSTR *a_pbstrHtmlLicense)
{
    Bstr bstrHtml("html");
    return QueryLicense(Bstr::Empty.raw(), Bstr::Empty.raw(), bstrHtml.raw(), a_pbstrHtmlLicense);
}

STDMETHODIMP ExtPack::QueryLicense(IN_BSTR a_bstrPreferredLocale, IN_BSTR a_bstrPreferredLanguage, IN_BSTR a_bstrFormat,
                                   BSTR *a_pbstrLicense)
{
    /*
     * Validate input.
     */
    CheckComArgOutPointerValid(a_pbstrLicense);
    CheckComArgNotNull(a_bstrPreferredLocale);
    CheckComArgNotNull(a_bstrPreferredLanguage);
    CheckComArgNotNull(a_bstrFormat);

    Utf8Str strPreferredLocale(a_bstrPreferredLocale);
    if (strPreferredLocale.length() != 2 && strPreferredLocale.length() != 0)
        return setError(E_FAIL, tr("The preferred locale is a two character string or empty."));

    Utf8Str strPreferredLanguage(a_bstrPreferredLanguage);
    if (strPreferredLanguage.length() != 2 && strPreferredLanguage.length() != 0)
        return setError(E_FAIL, tr("The preferred lanuage is a two character string or empty."));

    Utf8Str strFormat(a_bstrFormat);
    if (   !strFormat.equals("html")
        && !strFormat.equals("rtf")
        && !strFormat.equals("txt"))
        return setError(E_FAIL, tr("The license format can only have the values 'html', 'rtf' and 'txt'."));

    /*
     * Combine the options to form a file name before locking down anything.
     */
    char szName[sizeof("ExtPack-license-de_DE.html") + 2];
    if (strPreferredLocale.isNotEmpty() && strPreferredLanguage.isNotEmpty())
        RTStrPrintf(szName, sizeof(szName), "ExtPack-license-%s_%s.%s",
                    strPreferredLocale.c_str(), strPreferredLanguage.c_str(), strFormat.c_str());
    else if (strPreferredLocale.isNotEmpty())
        RTStrPrintf(szName, sizeof(szName), "ExtPack-license-%s.%s",  strPreferredLocale.c_str(), strFormat.c_str());
    else if (strPreferredLanguage.isNotEmpty())
        RTStrPrintf(szName, sizeof(szName), "ExtPack-license-_%s.%s", strPreferredLocale.c_str(), strFormat.c_str());
    else
        RTStrPrintf(szName, sizeof(szName), "ExtPack-license.%s",     strFormat.c_str());

    /*
     * Effectuate the query.
     */
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock autoLock(this COMMA_LOCKVAL_SRC_POS); /* paranoia */

        char szPath[RTPATH_MAX];
        int vrc = RTPathJoin(szPath, sizeof(szPath), m->strExtPackPath.c_str(), szName);
        if (RT_SUCCESS(vrc))
        {
            void   *pvFile;
            size_t  cbFile;
            vrc = RTFileReadAllEx(szPath, 0, RTFOFF_MAX, RTFILE_RDALL_O_DENY_READ, &pvFile, &cbFile);
            if (RT_SUCCESS(vrc))
            {
                Bstr bstrLicense((const char *)pvFile, cbFile);
                if (bstrLicense.isNotEmpty())
                {
                    bstrLicense.detachTo(a_pbstrLicense);
                    hrc = S_OK;
                }
                else
                    hrc = setError(VBOX_E_IPRT_ERROR, tr("The license file '%s' is empty or contains invalid UTF-8 encoding"),
                                   szPath);
                RTFileReadAllFree(pvFile, cbFile);
            }
            else if (vrc == VERR_FILE_NOT_FOUND || vrc == VERR_PATH_NOT_FOUND)
                hrc = setError(VBOX_E_OBJECT_NOT_FOUND, tr("The license file '%s' was not found in extension pack '%s'"),
                               szName, m->Desc.strName.c_str());
            else
                hrc = setError(VBOX_E_FILE_ERROR, tr("Failed to open the license file '%s': %Rrc"), szPath, vrc);
        }
        else
            hrc = setError(VBOX_E_IPRT_ERROR, tr("RTPathJoin failed: %Rrc"), vrc);
    }
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
 * @param   a_enmContext            The context we're in.
 */
HRESULT ExtPackManager::initExtPackManager(VirtualBox *a_pVirtualBox, VBOXEXTPACKCTX a_enmContext)
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
    m->pVirtualBox          = a_pVirtualBox;
    m->enmContext           = a_enmContext;

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
                && VBoxExtPackIsValidMangledName(Entry.szName) )
            {
                /*
                 * All directories are extensions, the shall be nothing but
                 * extensions in this subdirectory.
                 */
                char szExtPackDir[RTPATH_MAX];
                vrc = RTPathJoin(szExtPackDir, sizeof(szExtPackDir), m->strBaseDir.c_str(), Entry.szName);
                AssertLogRelRC(vrc);
                if (RT_SUCCESS(vrc))
                {
                    iprt::MiniString *pstrName = VBoxExtPackUnmangleName(Entry.szName, RTSTR_MAX);
                    AssertLogRel(pstrName);
                    if (pstrName)
                    {
                        ComObjPtr<ExtPack> NewExtPack;
                        HRESULT hrc2 = NewExtPack.createObject();
                        if (SUCCEEDED(hrc2))
                            hrc2 = NewExtPack->initWithDir(a_enmContext, pstrName->c_str(), szExtPackDir);
                        delete pstrName;
                        if (SUCCEEDED(hrc2))
                            m->llInstalledExtPacks.push_back(NewExtPack);
                        else if (SUCCEEDED(rc))
                            hrc = hrc2;
                    }
                    else
                        hrc = E_UNEXPECTED;
                }
                else
                    hrc = E_UNEXPECTED;
            }
        }
        RTDirClose(pDir);
    }
    /* else: ignore, the directory probably does not exist or something. */

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
    CheckComArgOutSafeArrayPointerValid(a_paExtPacks);
    Assert(m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON);

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
    Assert(m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON);

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

STDMETHODIMP ExtPackManager::OpenExtPackFile(IN_BSTR a_bstrTarball, IExtPackFile **a_ppExtPackFile)
{
    CheckComArgNotNull(a_bstrTarball);
    CheckComArgOutPointerValid(a_ppExtPackFile);
    Utf8Str strTarball(a_bstrTarball);
    AssertReturn(m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON, E_UNEXPECTED);

    ComObjPtr<ExtPackFile> NewExtPackFile;
    HRESULT hrc = NewExtPackFile.createObject();
    if (SUCCEEDED(hrc))
        hrc = NewExtPackFile->initWithFile(strTarball.c_str(), this);
    if (SUCCEEDED(hrc))
        NewExtPackFile.queryInterfaceTo(a_ppExtPackFile);

    return hrc;
}

STDMETHODIMP ExtPackManager::Uninstall(IN_BSTR a_bstrName, BOOL a_fForcedRemoval)
{
    CheckComArgNotNull(a_bstrName);
    Utf8Str strName(a_bstrName);
    Assert(m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON);

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
                                hrc = setError(E_FAIL,
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

        /*
         * Do VirtualBoxReady callbacks now for any freshly installed
         * extension pack (old ones will not be called).
         */
        if (m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON)
        {
            autoLock.release();
            callAllVirtualBoxReadyHooks();
        }
    }

    return hrc;
}

STDMETHODIMP ExtPackManager::Cleanup(void)
{
    Assert(m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON);

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
    Assert(m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON);

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
                char    achBuf[1024];
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
                    AssertLogRelMsgStmt(vrc == VERR_PROCESS_RUNNING, ("%Rrc\n", vrc), hProcess = NIL_RTPROCESS);
            }
        } while (   hPipeR != NIL_RTPIPE
                 || hProcess != NIL_RTPROCESS);

        LogRel(("ExtPack: enmReason=%d iStatus=%d stderr='%s'\n",
                ProcStatus.enmReason, ProcStatus.iStatus, offStdErrBuf ? pszStdErrBuf : ""));

        /*
         * Look for rcExit=RTEXITCODE_SUCCESS at the end of the error output,
         * cut it as it is only there to attest the success.
         */
        if (offStdErrBuf > 0)
        {
            RTStrStripR(pszStdErrBuf);
            offStdErrBuf = strlen(pszStdErrBuf);
        }

        if (    offStdErrBuf > 0
             && !strcmp(pszStdErrBuf, "rcExit=RTEXITCODE_SUCCESS"))
        {
            *pszStdErrBuf = '\0';
            offStdErrBuf  = 0;
        }
        else if (   ProcStatus.enmReason == RTPROCEXITREASON_NORMAL
                 && ProcStatus.iStatus   == 0)
            ProcStatus.iStatus = 666;

        /*
         * Compose the status code and, on failure, error message.
         */
        if (   ProcStatus.enmReason == RTPROCEXITREASON_NORMAL
            && ProcStatus.iStatus   == 0
            && offStdErrBuf         == 0)
            hrc = S_OK;
        else if (ProcStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {
            AssertMsg(ProcStatus.iStatus != 0, ("%s\n", pszStdErrBuf));
            hrc = setError(E_FAIL, tr("The installer failed with exit code %d: %s"),
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
            && pExtPackData->Desc.strName.equalsIgnoreCase(a_pszName))
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
            && pExtPackData->Desc.strName.equalsIgnoreCase(a_pszName))
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
 *
 * @param   a_pszName           The extension to update..
 * @param   a_fUnsuableIsError  If @c true, report an unusable extension pack
 *                              as an error.
 * @param   a_ppExtPack         Where to store the pointer to the extension
 *                              pack of it is still around after the refresh.
 *                              This is optional.
 *
 * @remarks Caller holds the extension manager lock.
 * @remarks Only called in VBoxSVC.
 */
HRESULT ExtPackManager::refreshExtPack(const char *a_pszName, bool a_fUnsuableIsError, ExtPack **a_ppExtPack)
{
    Assert(m->pVirtualBox != NULL); /* Only called from VBoxSVC. */

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
         * sensitivie file systems (a_pszName is case insensitive and mangled).
         */
        char szDir[RTPATH_MAX];
        int vrc = VBoxExtPackCalcDir(szDir, sizeof(szDir), m->strBaseDir.c_str(), a_pszName);
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
                const char *pszMangledName = RTPathFilename(szDir);
                for (;;)
                {
                    vrc = RTDirReadEx(pDir, &Entry, NULL /*pcbDirEntry*/, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                    if (RT_FAILURE(vrc))
                    {
                        AssertLogRelMsg(vrc == VERR_NO_MORE_FILES, ("%Rrc\n", vrc));
                        break;
                    }
                    if (   RTFS_IS_DIRECTORY(Entry.Info.Attr.fMode)
                        && !RTStrICmp(Entry.szName, pszMangledName))
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
            ComObjPtr<ExtPack> ptrNewExtPack;
            hrc = ptrNewExtPack.createObject();
            if (SUCCEEDED(hrc))
                hrc = ptrNewExtPack->initWithDir(m->enmContext, a_pszName, szDir);
            if (SUCCEEDED(hrc))
            {
                m->llInstalledExtPacks.push_back(ptrNewExtPack);
                if (ptrNewExtPack->m->fUsable)
                    LogRel(("ExtPackManager: Found extension pack '%s'.\n", a_pszName));
                else
                    LogRel(("ExtPackManager: Found bad extension pack '%s': %s\n",
                            a_pszName, ptrNewExtPack->m->strWhyUnusable.c_str() ));
                pExtPack = ptrNewExtPack;
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
 * Worker for IExtPackFile::Install.
 *
 * @returns COM status code.
 * @param   a_pExtPackFile  The extension pack file, caller checks that it's
 *                          usable.
 */
HRESULT ExtPackManager::doInstall(ExtPackFile *a_pExtPackFile)
{
    AssertReturn(m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON, E_UNEXPECTED);
    iprt::MiniString const * const pStrName     = &a_pExtPackFile->m->Desc.strName;
    iprt::MiniString const * const pStrTarball  = &a_pExtPackFile->m->strExtPackFile;

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);

        /*
         * Refresh the data we have on the extension pack as it
         * may be made stale by direct meddling or some other user.
         */
        ExtPack *pExtPack;
        hrc = refreshExtPack(pStrName->c_str(), false /*a_fUnsuableIsError*/, &pExtPack);
        if (SUCCEEDED(hrc) && !pExtPack)
        {
            /*
             * Run the privileged helper binary that performs the actual
             * installation.  Then create an object for the packet (we do this
             * even on failure, to be on the safe side).
             */
/** @todo add a hash (SHA-256) of the tarball or maybe just the manifest. */
            hrc = runSetUidToRootHelper("install",
                                        "--base-dir",   m->strBaseDir.c_str(),
                                        "--cert-dir",   m->strCertificatDirPath.c_str(),
                                        "--name",       pStrName->c_str(),
                                        "--tarball",    pStrTarball->c_str(),
                                        NULL);
            if (SUCCEEDED(hrc))
            {
                hrc = refreshExtPack(pStrName->c_str(), true /*a_fUnsuableIsError*/, &pExtPack);
                if (SUCCEEDED(hrc))
                {
                    LogRel(("ExtPackManager: Successfully installed extension pack '%s'.\n", pStrName->c_str()));
                    pExtPack->callInstalledHook(m->pVirtualBox, &autoLock);
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

        /*
         * Do VirtualBoxReady callbacks now for any freshly installed
         * extension pack (old ones will not be called).
         */
        if (m->enmContext == VBOXEXTPACKCTX_PER_USER_DAEMON)
        {
            autoLock.release();
            callAllVirtualBoxReadyHooks();
        }
    }

    return hrc;
}


/**
 * Calls the pfnVirtualBoxReady hook for all working extension packs.
 *
 * @remarks The caller must not hold any locks.
 */
void ExtPackManager::callAllVirtualBoxReadyHooks(void)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (FAILED(hrc))
        return;
    AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);
    ComPtr<ExtPackManager> ptrSelfRef = this;

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         /* advancing below */)
    {
        if ((*it)->callVirtualBoxReadyHook(m->pVirtualBox, &autoLock))
            it = m->llInstalledExtPacks.begin();
        else
            it++;
    }
}

/**
 * Calls the pfnConsoleReady hook for all working extension packs.
 *
 * @param   a_pConsole          The console interface.
 * @remarks The caller must not hold any locks.
 */
void ExtPackManager::callAllConsoleReadyHooks(IConsole *a_pConsole)
{
    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (FAILED(hrc))
        return;
    AutoWriteLock autoLock(this COMMA_LOCKVAL_SRC_POS);
    ComPtr<ExtPackManager> ptrSelfRef = this;

    for (ExtPackList::iterator it = m->llInstalledExtPacks.begin();
         it != m->llInstalledExtPacks.end();
         /* advancing below */)
    {
        if ((*it)->callConsoleReadyHook(a_pConsole, &autoLock))
            it = m->llInstalledExtPacks.begin();
        else
            it++;
    }
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
    AutoWriteLock           autoLock(this COMMA_LOCKVAL_SRC_POS);
    ComPtr<ExtPackManager>  ptrSelfRef = this; /* paranoia */
    ExtPackList             llExtPacks = m->llInstalledExtPacks;

    for (ExtPackList::iterator it = llExtPacks.begin(); it != llExtPacks.end(); it++)
        (*it)->callVmCreatedHook(m->pVirtualBox, a_pMachine, &autoLock);
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
    AutoWriteLock           autoLock(this COMMA_LOCKVAL_SRC_POS);
    ComPtr<ExtPackManager>  ptrSelfRef = this; /* paranoia */
    ExtPackList             llExtPacks = m->llInstalledExtPacks;

    for (ExtPackList::iterator it = llExtPacks.begin(); it != llExtPacks.end(); it++)
    {
        int vrc;
        (*it)->callVmConfigureVmmHook(a_pConsole, a_pVM, &autoLock, &vrc);
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
    AutoWriteLock           autoLock(this COMMA_LOCKVAL_SRC_POS);
    ComPtr<ExtPackManager>  ptrSelfRef = this; /* paranoia */
    ExtPackList             llExtPacks = m->llInstalledExtPacks;

    for (ExtPackList::iterator it = llExtPacks.begin(); it != llExtPacks.end(); it++)
    {
        int vrc;
        (*it)->callVmPowerOnHook(a_pConsole, a_pVM, &autoLock, &vrc);
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
    AutoWriteLock           autoLock(this COMMA_LOCKVAL_SRC_POS);
    ComPtr<ExtPackManager>  ptrSelfRef = this; /* paranoia */
    ExtPackList             llExtPacks = m->llInstalledExtPacks;

    for (ExtPackList::iterator it = llExtPacks.begin(); it != llExtPacks.end(); it++)
        (*it)->callVmPowerOffHook(a_pConsole, a_pVM, &autoLock);
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
