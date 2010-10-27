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



#if 0
/** Pointer to callbacks provided to the VBoxDriverRegister() call. */
typedef struct PDMDRVREGCB *PPDMDRVREGCB;
/** Pointer to const callbacks provided to the VBoxDriverRegister() call. */
typedef const struct PDMDRVREGCB *PCPDMDRVREGCB;

/**
 * Callbacks for VBoxExtPackRegister().
 */
typedef struct PCVBOXEXTPACKREGCB
{
    /** Interface version.
     * This is set to VBOXEXTPACK_REG_CB_VERSION. */
    uint32_t                    u32Version;

    /**
     * Registers a driver with the current VM instance.
     *
     * @returns VBox status code.
     * @param   pCallbacks      Pointer to the callback table.
     * @param   pReg            Pointer to the driver registration record.
     *                          This data must be permanent and readonly.
     */
    DECLR3CALLBACKMEMBER(int, pfnGet,(PCVBOXEXTPACKREGCB pCallbacks, PCPDMDRVREG pReg));

    /** End of structure marker (VBOXEXTPACK_REG_CB_VERSION). */
    uint32_t                    u32EndMarker;
} PCVBOXEXTPACKREGCB;

/** Current version of the PDMDRVREGCB structure.  */
#define VBOXEXTPACK_REG_CB_VERSION          RT_MAKE_U32(1, 0)


/**
 * The VBoxExtPackRegister callback function.
 *
 * PDM will invoke this function after loading a driver module and letting
 * the module decide which drivers to register and how to handle conflicts.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
typedef DECLCALLBACK(int) FNVBOXEXTPACKREGISTER(PCVBOXEXTPACKREGCB pCallbacks, uint32_t u32Version);
#endif

#endif


