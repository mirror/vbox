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

#if defined(__cplusplus)
class IConsole;
class IMachine;
#else
typedef struct IConsole IConsole;
typedef struct IMachine IMachine;
#endif


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
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     */
    DECLCALLBACKMEMBER(int, pfnInstalled)(PCVBOXEXTPACKREG pThis);

    /**
     * Hook for cleaning up before the extension pack is uninstalled.
     *
     * This is called in the context of the per-user service (VBoxSVC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     */
    DECLCALLBACKMEMBER(int, pfnUninstall)(PCVBOXEXTPACKREG pThis);

    /**
     * Hook for changing the default VM configuration upon creation.
     *
     * This is called in the context of the per-user service (VBoxSVC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pMachine    The machine interface.
     */
    DECLCALLBACKMEMBER(int, pfnVMCreated)(PCVBOXEXTPACKREG pThis, IMachine *pMachine);

    /**
     * Hook for configuring the VMM for a VM.
     *
     * This is called in the context of the VM process (VBoxC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pVM         The VM handle.
     * @param   pConsole    The console interface.
     */
    DECLCALLBACKMEMBER(int, pfnVMConfigureVMM)(PCVBOXEXTPACKREG pThis, PVM pVM, IConsole *pConsole);

    /**
     * Hook for doing work right before powering on the VM.
     *
     * This is called in the context of the VM process (VBoxC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pVM         The VM handle.
     * @param   pConsole    The console interface.
     */
    DECLCALLBACKMEMBER(int, pfnVMPowerOn)(PCVBOXEXTPACKREG pThis, PVM pVM, IConsole *pConsole);

    /**
     * Hook for doing work after powering on the VM.
     *
     * This is called in the context of the VM process (VBoxC).
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to this structure.
     * @param   pVM         The VM handle.  Can be NULL.
     * @param   pConsole    The console interface.
     */
    DECLCALLBACKMEMBER(int, pfnVMPowerOff)(PCVBOXEXTPACKREG pThis, PVM pVM, IConsole *pConsole);

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


