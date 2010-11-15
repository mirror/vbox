/** @file
 * VirtualBox - Extension Pack Interface.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_ExtPack_ExtPack_h
#define ___VBox_ExtPack_ExtPack_h

#include <VBox/types.h>

/** @def VBOXEXTPACK_IF_CS
 * Selects 'class' on 'struct' for interface references.
 * @param I         The interface name
 */
#if defined(__cplusplus) && !defined(RT_OS_WINDOWS)
# define VBOXEXTPACK_IF_CS(I)   class I
#else
# define VBOXEXTPACK_IF_CS(I)   struct I
#endif

VBOXEXTPACK_IF_CS(IConsole);
VBOXEXTPACK_IF_CS(IMachine);
VBOXEXTPACK_IF_CS(IVirtualBox);


/** Pointer to const helpers passed to the VBoxExtPackRegister() call. */
typedef const struct VBOXEXTPACKHLP *PCVBOXEXTPACKHLP;
/**
 * Extension pack helpers passed to VBoxExtPackRegister().
 *
 * This will be valid until the module is unloaded.
 */
typedef struct VBOXEXTPACKHLP
{
    /** Interface version.
     * This is set to VBOXEXTPACKHLP_VERSION. */
    uint32_t                    u32Version;

    /** The VirtualBox full version (see VBOX_FULL_VERSION).  */
    uint32_t                    uVBoxFullVersion;
    /** The VirtualBox subversion tree revision.  */
    uint32_t                    uVBoxInternalRevision;
    /** Explicit alignment padding, must be zero. */
    uint32_t                    u32Padding;
    /** Pointer to the version string (read-only). */
    const char                 *pszVBoxVersion;

    /**
     * Finds a module belonging to this extension pack.
     *
     * @returns VBox status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pszName         The module base name.
     * @param   pszExt          The extension. If NULL the default ring-3
     *                          library extension will be used.
     * @param   pszFound        Where to return the path to the module on
     *                          success.
     * @param   cbFound         The size of the buffer @a pszFound points to.
     * @param   pfNative        Where to return the native/agnostic indicator.
     */
    DECLR3CALLBACKMEMBER(int, pfnFindModule,(PCVBOXEXTPACKHLP pHlp, const char *pszName, const char *pszExt,
                                             char *pszFound, size_t cbFound, bool *pfNative));

    /**
     * Gets the path to a file belonging to this extension pack.
     *
     * @returns VBox status code.
     * @retval  VERR_INVALID_POINTER if any of the pointers are invalid.
     * @retval  VERR_BUFFER_OVERFLOW if the buffer is too small.  The buffer
     *          will contain nothing.
     *
     * @param   pHlp            Pointer to this helper structure.
     * @param   pszFilename     The filename.
     * @param   pszPath         Where to return the path to the file on
     *                          success.
     * @param   cbPath          The size of the buffer @a pszPath.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetFilePath,(PCVBOXEXTPACKHLP pHlp, const char *pszFilename, char *pszPath, size_t cbPath));

    /**
     * Registers a VRDE library (IVirtualBox::VRDERegisterLibrary wrapper).
     *
     * @returns VBox status code.
     * @param   pHlp            Pointer to this helper structure.
     * @param   pszName         The module base name.  This will be found using
     *                          the pfnFindModule algorithm.
     * @param   fSetDefault     Whether to make it default if no other default
     *                          is set.
     *
     * @remarks This helper should be called from pfnVirtualBoxReady as it may
     *          cause trouble when called from pfnInstalled.
     */
    DECLR3CALLBACKMEMBER(int, pfnRegisterVrde,(PCVBOXEXTPACKHLP pHlp, const char *pszName, bool fSetDefault));

    /** End of structure marker (VBOXEXTPACKHLP_VERSION). */
    uint32_t                    u32EndMarker;
} VBOXEXTPACKHLP;
/** Current version of the VBOXEXTPACKHLP structure.  */
#define VBOXEXTPACKHLP_VERSION          RT_MAKE_U32(1, 0)


/** Pointer to the extension pack callback table. */
typedef struct VBOXEXTPACKREG const *PCVBOXEXTPACKREG;
/**
 * Callback table returned by VBoxExtPackRegister.
 *
 * This must be valid until the extension pack main module is unloaded.
 */
typedef struct VBOXEXTPACKREG
{
    /** Interface version.
     * This is set to VBOXEXTPACKREG_VERSION. */
    uint32_t                    u32Version;

    /**
     * Hook for doing setups after the extension pack was installed.
     *
     * This is called in the context of the per-user service (VBoxSVC).
     *
     * @param   pThis       Pointer to this structure.
     * @param   pVirtualBox The VirtualBox interface.
     */
    DECLCALLBACKMEMBER(void, pfnInstalled)(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);

    /**
     * Hook for cleaning up before the extension pack is uninstalled.
     *
     * This is called in the context of the per-user service (VBoxSVC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pVirtualBox The VirtualBox interface.
     */
    DECLCALLBACKMEMBER(int, pfnUninstall)(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);

    /**
     * Hook for doing work after the VirtualBox object is ready.
     *
     * This is called in the context of the per-user service (VBoxSVC).  There
     * will not be any similar call from the VM process.
     *
     * @param   pThis       Pointer to this structure.
     * @param   pVirtualBox The VirtualBox interface.
     */
    DECLCALLBACKMEMBER(void, pfnVirtualBoxReady)(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);

    /**
     * Hook for doing work before unloading.
     *
     * This is called both in the context of the per-user service (VBoxSVC) and
     * in context of the VM process (VBoxC).
     *
     * @param   pThis       Pointer to this structure.
     *
     * @remarks The helpers are not available at this point in time.
     * @remarks This is not called on uninstall, then pfnUninstall will be the
     *          last callback.
     */
    DECLCALLBACKMEMBER(void, pfnUnload)(PCVBOXEXTPACKREG pThis);

    /**
     * Hook for changing the default VM configuration upon creation.
     *
     * This is called in the context of the per-user service (VBoxSVC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pVirtualBox The VirtualBox interface.
     * @param   pMachine    The machine interface.
     */
    DECLCALLBACKMEMBER(int, pfnVMCreated)(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox,
                                          VBOXEXTPACK_IF_CS(IMachine) *pMachine);

    /**
     * Hook for configuring the VMM for a VM.
     *
     * This is called in the context of the VM process (VBoxC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pConsole    The console interface.
     * @param   pVM         The VM handle.
     */
    DECLCALLBACKMEMBER(int, pfnVMConfigureVMM)(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);

    /**
     * Hook for doing work right before powering on the VM.
     *
     * This is called in the context of the VM process (VBoxC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pConsole    The console interface.
     * @param   pVM         The VM handle.
     */
    DECLCALLBACKMEMBER(int, pfnVMPowerOn)(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);

    /**
     * Hook for doing work after powering on the VM.
     *
     * This is called in the context of the VM process (VBoxC).
     *
     * @param   pThis       Pointer to this structure.
     * @param   pConsole    The console interface.
     * @param   pVM         The VM handle.  Can be NULL.
     */
    DECLCALLBACKMEMBER(void, pfnVMPowerOff)(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IConsole) *pConsole, PVM pVM);

    /**
     * Query the IUnknown interface to an object in the main module.
     *
     * This is can be called in any context.
     *
     * @returns IUnknown pointer (referenced) on success, NULL on failure.
     * @param   pThis       Pointer to this structure.
     * @param   pObjectId   Pointer to the object ID (UUID).
     */
    DECLCALLBACKMEMBER(void *, pfnQueryObject)(PCVBOXEXTPACKREG pThis, PCRTUUID pObjectId);

    /** End of structure marker (VBOXEXTPACKREG_VERSION). */
    uint32_t                    u32EndMarker;
} VBOXEXTPACKREG;
/** Current version of the VBOXEXTPACKREG structure.  */
#define VBOXEXTPACKREG_VERSION        RT_MAKE_U32(1, 0)


/**
 * The VBoxExtPackRegister callback function.
 *
 * PDM will invoke this function after loading a driver module and letting
 * the module decide which drivers to register and how to handle conflicts.
 *
 * @returns VBox status code.
 * @param   pHlp            Pointer to the extension pack helper function
 *                          table.  This is valid until the module is unloaded.
 * @param   ppReg           Where to return the pointer to the registration
 *                          structure containing all the hooks.  This structure
 *                          be valid and unchanged until the module is unloaded
 *                          (i.e. use some static const data for it).
 * @param   pszErr          Error message buffer for explaining any failure.
 * @param   cbErr           The size of the error message buffer.
 */
typedef DECLCALLBACK(int) FNVBOXEXTPACKREGISTER(PCVBOXEXTPACKHLP pHlp, PCVBOXEXTPACKREG *ppReg, char *pszErr, size_t cbErr);
/** Pointer to a FNVBOXEXTPACKREGISTER. */
typedef FNVBOXEXTPACKREGISTER *PFNVBOXEXTPACKREGISTER;

/** The name of the main module entry point. */
#define VBOX_EXTPACK_MAIN_MOD_ENTRY_POINT   "VBoxExtPackRegister"


#endif

