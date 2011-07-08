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

#ifndef ____H_EXTPACKMANAGERIMPL
#define ____H_EXTPACKMANAGERIMPL

#include "VirtualBoxBase.h"
#include <VBox/ExtPack/ExtPack.h>
#include <iprt/fs.h>

/**
 * An extension pack file.
 */
class ATL_NO_VTABLE ExtPackFile :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IExtPackFile)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(ExtPackFile, IExtPackFile)
    DECLARE_NOT_AGGREGATABLE(ExtPackFile)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(ExtPackFile)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IExtPackFile)
        COM_INTERFACE_ENTRY(IExtPackBase)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(ExtPackFile)

    HRESULT     FinalConstruct();
    void        FinalRelease();
    HRESULT     initWithFile(const char *a_pszFile, class ExtPackManager *a_pExtPackMgr, VirtualBox *a_pVirtualBox);
    void        uninit();
    RTMEMEF_NEW_AND_DELETE_OPERATORS();
    /** @}  */

    /** @name IExtPackBase interfaces
     * @{ */
    STDMETHOD(COMGETTER(Name))(BSTR *a_pbstrName);
    STDMETHOD(COMGETTER(Description))(BSTR *a_pbstrDescription);
    STDMETHOD(COMGETTER(Version))(BSTR *a_pbstrVersion);
    STDMETHOD(COMGETTER(Revision))(ULONG *a_puRevision);
    STDMETHOD(COMGETTER(VRDEModule))(BSTR *a_pbstrVrdeModule);
    STDMETHOD(COMGETTER(PlugIns))(ComSafeArrayOut(IExtPackPlugIn *, a_paPlugIns));
    STDMETHOD(COMGETTER(Usable))(BOOL *a_pfUsable);
    STDMETHOD(COMGETTER(WhyUnusable))(BSTR *a_pbstrWhy);
    STDMETHOD(COMGETTER(ShowLicense))(BOOL *a_pfShowIt);
    STDMETHOD(COMGETTER(License))(BSTR *a_pbstrHtmlLicense);
    STDMETHOD(QueryLicense)(IN_BSTR a_bstrPreferredLocale, IN_BSTR a_bstrPreferredLanguage,
                            IN_BSTR a_bstrFormat, BSTR *a_pbstrLicense);
    /** @}  */

    /** @name IExtPackFile interfaces
     * @{ */
    STDMETHOD(COMGETTER(FilePath))(BSTR *a_pbstrPath);
    STDMETHOD(Install)(BOOL a_fReplace, IN_BSTR a_bstrDisplayInfo, IProgress **a_ppProgress);
    /** @}  */

private:
    /** @name Misc init helpers
     * @{ */
    HRESULT     initFailed(const char *a_pszWhyFmt, ...);
    /** @} */

private:
    struct Data;
    /** Pointer to the private instance. */
    Data *m;

    friend class ExtPackManager;
};


/**
 * An installed extension pack.
 */
class ATL_NO_VTABLE ExtPack :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IExtPack)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(ExtPack, IExtPack)
    DECLARE_NOT_AGGREGATABLE(ExtPack)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(ExtPack)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IExtPack)
        COM_INTERFACE_ENTRY(IExtPackBase)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(ExtPack)

    HRESULT     FinalConstruct();
    void        FinalRelease();
    HRESULT     initWithDir(VBOXEXTPACKCTX a_enmContext, const char *a_pszName, const char *a_pszDir);
    void        uninit();
    RTMEMEF_NEW_AND_DELETE_OPERATORS();
    /** @}  */

    /** @name IExtPackBase interfaces
     * @{ */
    STDMETHOD(COMGETTER(Name))(BSTR *a_pbstrName);
    STDMETHOD(COMGETTER(Description))(BSTR *a_pbstrDescription);
    STDMETHOD(COMGETTER(Version))(BSTR *a_pbstrVersion);
    STDMETHOD(COMGETTER(Revision))(ULONG *a_puRevision);
    STDMETHOD(COMGETTER(VRDEModule))(BSTR *a_pbstrVrdeModule);
    STDMETHOD(COMGETTER(PlugIns))(ComSafeArrayOut(IExtPackPlugIn *, a_paPlugIns));
    STDMETHOD(COMGETTER(Usable))(BOOL *a_pfUsable);
    STDMETHOD(COMGETTER(WhyUnusable))(BSTR *a_pbstrWhy);
    STDMETHOD(COMGETTER(ShowLicense))(BOOL *a_pfShowIt);
    STDMETHOD(COMGETTER(License))(BSTR *a_pbstrHtmlLicense);
    STDMETHOD(QueryLicense)(IN_BSTR a_bstrPreferredLocale, IN_BSTR a_bstrPreferredLanguage,
                            IN_BSTR a_bstrFormat, BSTR *a_pbstrLicense);
    /** @}  */

    /** @name IExtPack interfaces
     * @{ */
    STDMETHOD(QueryObject)(IN_BSTR a_bstrObjectId, IUnknown **a_ppUnknown);
    /** @}  */

    /** @name Internal interfaces used by ExtPackManager.
     * @{ */
    bool        callInstalledHook(IVirtualBox *a_pVirtualBox, AutoWriteLock *a_pLock, PRTERRINFO pErrInfo);
    HRESULT     callUninstallHookAndClose(IVirtualBox *a_pVirtualBox, bool a_fForcedRemoval);
    bool        callVirtualBoxReadyHook(IVirtualBox *a_pVirtualBox, AutoWriteLock *a_pLock);
    bool        callConsoleReadyHook(IConsole *a_pConsole, AutoWriteLock *a_pLock);
    bool        callVmCreatedHook(IVirtualBox *a_pVirtualBox, IMachine *a_pMachine, AutoWriteLock *a_pLock);
    bool        callVmConfigureVmmHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock, int *a_pvrc);
    bool        callVmPowerOnHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock, int *a_pvrc);
    bool        callVmPowerOffHook(IConsole *a_pConsole, PVM a_pVM, AutoWriteLock *a_pLock);
    HRESULT     checkVrde(void);
    HRESULT     getVrdpLibraryName(Utf8Str *a_pstrVrdeLibrary);
    bool        wantsToBeDefaultVrde(void) const;
    HRESULT     refresh(bool *pfCanDelete);
    /** @}  */

protected:
    /** @name Internal helper methods.
     * @{ */
    void        probeAndLoad(void);
    bool        findModule(const char *a_pszName, const char *a_pszExt, VBOXEXTPACKMODKIND a_enmKind,
                           Utf8Str *a_ppStrFound, bool *a_pfNative, PRTFSOBJINFO a_pObjInfo) const;
    static bool objinfoIsEqual(PCRTFSOBJINFO pObjInfo1, PCRTFSOBJINFO pObjInfo2);
    /** @}  */

    /** @name Extension Pack Helpers
     * @{ */
    static DECLCALLBACK(int)    hlpFindModule(PCVBOXEXTPACKHLP pHlp, const char *pszName, const char *pszExt,
                                              VBOXEXTPACKMODKIND enmKind, char *pszFound, size_t cbFound, bool *pfNative);
    static DECLCALLBACK(int)    hlpGetFilePath(PCVBOXEXTPACKHLP pHlp, const char *pszFilename, char *pszPath, size_t cbPath);
    static DECLCALLBACK(VBOXEXTPACKCTX) hlpGetContext(PCVBOXEXTPACKHLP pHlp);
    static DECLCALLBACK(int)    hlpReservedN(PCVBOXEXTPACKHLP pHlp);
    /** @}  */

private:
    struct Data;
    /** Pointer to the private instance. */
    Data *m;

    friend class ExtPackManager;
};


/**
 * Extension pack manager.
 */
class ATL_NO_VTABLE ExtPackManager :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IExtPackManager)
{
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(ExtPackManager, IExtPackManager)
    DECLARE_NOT_AGGREGATABLE(ExtPackManager)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(ExtPackManager)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IExtPackManager)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(ExtPackManager)

    HRESULT     FinalConstruct();
    void        FinalRelease();
    HRESULT     initExtPackManager(VirtualBox *a_pVirtualBox, VBOXEXTPACKCTX a_enmContext);
    void        uninit();
    RTMEMEF_NEW_AND_DELETE_OPERATORS();
    /** @}  */

    /** @name IExtPack interfaces
     * @{ */
    STDMETHOD(COMGETTER(InstalledExtPacks))(ComSafeArrayOut(IExtPack *, a_paExtPacks));
    STDMETHOD(Find)(IN_BSTR a_bstrName, IExtPack **a_pExtPack);
    STDMETHOD(OpenExtPackFile)(IN_BSTR a_bstrTarball, IExtPackFile **a_ppExtPackFile);
    STDMETHOD(Uninstall)(IN_BSTR a_bstrName, BOOL a_fForcedRemoval, IN_BSTR a_bstrDisplayInfo, IProgress **a_ppProgress);
    STDMETHOD(Cleanup)(void);
    STDMETHOD(QueryAllPlugInsForFrontend)(IN_BSTR a_bstrFrontend, ComSafeArrayOut(BSTR, a_pabstrPlugInModules));
    STDMETHOD(IsExtPackUsable(IN_BSTR a_bstrExtPack, BOOL *aUsable));
    /** @}  */

    /** @name Internal interfaces used by other Main classes.
     * @{ */
    static DECLCALLBACK(int) doInstallThreadProc(RTTHREAD hThread, void *pvJob);
    HRESULT     doInstall(ExtPackFile *a_pExtPackFile, bool a_fReplace, Utf8Str const *a_pstrDisplayInfo);
    static DECLCALLBACK(int) doUninstallThreadProc(RTTHREAD hThread, void *pvJob);
    HRESULT     doUninstall(const Utf8Str *a_pstrName, bool a_fForcedRemoval, const Utf8Str *a_pstrDisplayInfo);
    void        callAllVirtualBoxReadyHooks(void);
    void        callAllConsoleReadyHooks(IConsole *a_pConsole);
    void        callAllVmCreatedHooks(IMachine *a_pMachine);
    int         callAllVmConfigureVmmHooks(IConsole *a_pConsole, PVM a_pVM);
    int         callAllVmPowerOnHooks(IConsole *a_pConsole, PVM a_pVM);
    void        callAllVmPowerOffHooks(IConsole *a_pConsole, PVM a_pVM);
    HRESULT     checkVrdeExtPack(Utf8Str const *a_pstrExtPack);
    int         getVrdeLibraryPathForExtPack(Utf8Str const *a_pstrExtPack, Utf8Str *a_pstrVrdeLibrary);
    HRESULT     getDefaultVrdeExtPack(Utf8Str *a_pstrExtPack);
    bool        isExtPackUsable(const char *a_pszExtPack);
    void        dumpAllToReleaseLog(void);
    /** @}  */

private:
    HRESULT     runSetUidToRootHelper(Utf8Str const *a_pstrDisplayInfo, const char *a_pszCommand, ...);
    ExtPack    *findExtPack(const char *a_pszName);
    void        removeExtPack(const char *a_pszName);
    HRESULT     refreshExtPack(const char *a_pszName, bool a_fUnsuableIsError, ExtPack **a_ppExtPack);

private:
    struct Data;
    /** Pointer to the private instance. */
    Data *m;
};

#endif
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
