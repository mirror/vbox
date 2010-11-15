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
 * An extension pack.
 *
 * This
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
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IExtPack)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(ExtPack)

    HRESULT     FinalConstruct();
    void        FinalRelease();
    HRESULT     init(VirtualBox *a_pVirtualBox, const char *a_pszName, const char *a_pszParentDir);
    void        uninit();
    /** @}  */

    /** @name IExtPack interfaces
     * @{ */
    STDMETHOD(COMGETTER(Name))(BSTR *a_pbstrName);
    STDMETHOD(COMGETTER(Description))(BSTR *a_pbstrDescription);
    STDMETHOD(COMGETTER(Version))(BSTR *a_pbstrVersion);
    STDMETHOD(COMGETTER(Revision))(ULONG *a_puRevision);
    STDMETHOD(COMGETTER(PlugIns))(ComSafeArrayOut(IExtPackPlugIn *, a_paPlugIns));
    STDMETHOD(COMGETTER(Usable))(BOOL *a_pfUsable);
    STDMETHOD(COMGETTER(WhyUnusable))(BSTR *a_pbstrWhy);
    STDMETHOD(QueryObject)(IN_BSTR a_bstrObjectId, IUnknown **a_ppUnknown);
    /** @}  */

    /** @name Internal interfaces used by ExtPackManager.
     * @{ */
    void        callInstalledHook(IVirtualBox *a_pVirtualBox);
    HRESULT     callUninstallHookAndClose(IVirtualBox *a_pVirtualBox, bool a_fForcedRemoval);
    void        callVirtualBoxReadyHook(IVirtualBox *a_pVirtualBox);
    void        callVmCreatedHook(IVirtualBox *a_pVirtualBox, IMachine *a_pMachine);
    int         callVmConfigureVmmHook(IConsole *a_pConsole, PVM a_pVM);
    int         callVmPowerOnHook(IConsole *a_pConsole, PVM a_pVM);
    void        callVmPowerOffHook(IConsole *a_pConsole, PVM a_pVM);
    HRESULT     refresh(bool *pfCanDelete);
    /** @}  */

protected:
    /** @name Internal helper methods.
     * @{ */
    void        probeAndLoad(void);
    bool        findModule(const char *a_pszName, const char *a_pszExt,
                           Utf8Str *a_ppStrFound, bool *a_pfNative, PRTFSOBJINFO a_pObjInfo) const;
    static bool objinfoIsEqual(PCRTFSOBJINFO pObjInfo1, PCRTFSOBJINFO pObjInfo2);
    /** @}  */

    /** @name Extension Pack Helpers
     * @{ */
    static DECLCALLBACK(int)    hlpFindModule(PCVBOXEXTPACKHLP pHlp, const char *pszName, const char *pszExt,
                                              char *pszFound, size_t cbFound, bool *pfNative);
    static DECLCALLBACK(int)    hlpGetFilePath(PCVBOXEXTPACKHLP pHlp, const char *pszFilename, char *pszPath, size_t cbPath);
    static DECLCALLBACK(int)    hlpRegisterVrde(PCVBOXEXTPACKHLP pHlp, const char *pszName, bool fSetDefault);
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
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IExtPackManager)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(ExtPackManager)

    HRESULT     FinalConstruct();
    void        FinalRelease();
    HRESULT     init(VirtualBox *a_pVirtualBox, const char *a_pszDropZonePath, bool a_fCheckDropZone);
    void        uninit();
    /** @}  */

    /** @name IExtPack interfaces
     * @{ */
    STDMETHOD(COMGETTER(InstalledExtPacks))(ComSafeArrayOut(IExtPack *, a_paExtPacks));
    STDMETHOD(Find)(IN_BSTR a_bstrName, IExtPack **a_pExtPack);
    STDMETHOD(Install)(IN_BSTR a_bstrTarball, BSTR *a_pbstrName);
    STDMETHOD(Uninstall)(IN_BSTR a_bstrName, BOOL a_fForcedRemoval);
    STDMETHOD(Cleanup)(void);
    STDMETHOD(QueryAllPlugInsForFrontend)(IN_BSTR a_bstrFrontend, ComSafeArrayOut(BSTR, a_pabstrPlugInModules));
    /** @}  */

    /** @name Internal interfaces used by other Main classes.
     * @{ */
    void        processDropZone(void);
    void        callAllVirtualBoxReadyHooks(void);
    void        callAllVmCreatedHooks(IMachine *a_pMachine);
    int         callAllVmConfigureVmmHooks(IConsole *a_pConsole, PVM a_pVM);
    int         callAllVmPowerOnHooks(IConsole *a_pConsole, PVM a_pVM);
    void        callAllVmPowerOffHooks(IConsole *a_pConsole, PVM a_pVM);
    /** @}  */

private:
    HRESULT     runSetUidToRootHelper(const char *a_pszCommand, ...);
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
