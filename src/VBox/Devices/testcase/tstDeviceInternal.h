/** @file
 * tstDevice: Shared definitions between the framework and the shim library.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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

#ifndef ___tstDeviceInternal_h
#define ___tstDeviceInternal_h

#include <VBox/types.h>
#include <iprt/assert.h>
#include <iprt/list.h>

#include "tstDevicePlugin.h"
#include "tstDeviceVMMInternal.h"

RT_C_DECLS_BEGIN

/**
 * PDM module descriptor type.
 */
typedef enum TSTDEVPDMMODTYPE
{
    /** Invalid module type. */
    TSTDEVPDMMODTYPE_INVALID = 0,
    /** Ring 3 module. */
    TSTDEVPDMMODTYPE_R3,
    /** Ring 0 module. */
    TSTDEVPDMMODTYPE_R0,
    /** Raw context module. */
    TSTDEVPDMMODTYPE_RC,
    /** 32bit hack. */
    TSTDEVPDMMODTYPE_32BIT_HACK = 0x7fffffff
} TSTDEVPDMMODTYPE;

/**
 * Registered I/O port access handler.
 */
typedef struct RTDEVDUTIOPORT
{
    /** Node for the list of registered handlers. */
    RTLISTNODE                      NdIoPorts;
    /** Start I/O port the handler is for. */
    RTIOPORT                        PortStart;
    /** Number of ports handled. */
    RTIOPORT                        cPorts;
    /** Opaque user data - R3. */
    void                            *pvUserR3;
    /** Out handler - R3. */
    PFNIOMIOPORTOUT                 pfnOutR3;
    /** In handler - R3. */
    PFNIOMIOPORTIN                  pfnInR3;
    /** Out string handler - R3. */
    PFNIOMIOPORTOUTSTRING           pfnOutStrR3;
    /** In string handler - R3. */
    PFNIOMIOPORTINSTRING            pfnInStrR3;

    /** Opaque user data - R0. */
    void                            *pvUserR0;
    /** Out handler - R3. */
    PFNIOMIOPORTOUT                 pfnOutR0;
    /** In handler - R3. */
    PFNIOMIOPORTIN                  pfnInR0;
    /** Out string handler - R3. */
    PFNIOMIOPORTOUTSTRING           pfnOutStrR0;
    /** In string handler - R3. */
    PFNIOMIOPORTINSTRING            pfnInStrR0;

    /** @todo: RC */
} RTDEVDUTIOPORT;
/** Pointer to a registered I/O port handler. */
typedef RTDEVDUTIOPORT *PRTDEVDUTIOPORT;
/** Pointer to a const I/O port handler. */
typedef const RTDEVDUTIOPORT *PCRTDEVDUTIOPORT;

/**
 * The contex the device under test is currently in.
 */
typedef enum TSTDEVDUTCTX
{
    /** Invalid context. */
    TSTDEVDUTCTX_INVALID = 0,
    /** R3 context. */
    TSTDEVDUTCTX_R3,
    /** R0 context. */
    TSTDEVDUTCTX_R0,
    /** RC context. */
    TSTDEVDUTCTX_RC,
    /** 32bit hack. */
    TSTDEVDUTCTX_32BIT_HACK = 0x7fffffff
} TSTDEVDUTCTX;

/**
 * Device under test instance data.
 */
typedef struct TSTDEVDUTINT
{
    /** Pointer to the testcase this device is part of. */
    PCTSTDEVTESTCASEREG             pTestcaseReg;
    /** Pointer to the PDM device instance. */
    PPDMDEVINS                      pDevIns;
    /** Current device context. */
    TSTDEVDUTCTX                    enmCtx;
    /** List of registered I/O port handlers. */
    RTLISTANCHOR                    LstIoPorts;
    /** List of timers registered. */
    RTLISTANCHOR                    LstTimers;
} TSTDEVDUTINT;
/** Pointer to the internal device under test instance data. */
typedef TSTDEVDUTINT *PDEVDUTINT;

DECLHIDDEN(int) tstDevPdmLdrGetSymbol(PDEVDUTINT pThis, const char *pszR0Mod, TSTDEVPDMMODTYPE enmModType,
                                      const char *pszSymbol, PFNRT *ppfn);

RT_C_DECLS_END

#endif
