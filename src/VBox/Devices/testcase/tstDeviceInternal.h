/** @file
 * tstDevice: Shared definitions between the framework and the shim library.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_testcase_tstDeviceInternal_h
#define VBOX_INCLUDED_SRC_testcase_tstDeviceInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/param.h>
#include <VBox/types.h>
#include <iprt/assert.h>
#include <iprt/list.h>
#include <iprt/semaphore.h>
#include <iprt/critsect.h>

#include "tstDevicePlugin.h"

RT_C_DECLS_BEGIN


/** Converts PDM device instance to the device under test structure. */
#define TSTDEV_PDMDEVINS_2_DUT(a_pDevIns) ((a_pDevIns)->Internal.s.pDut)

/** Forward declaration of internal test device instance data. */
typedef struct TSTDEVDUTINT *PTSTDEVDUTINT;


/**
 * CFGM node structure.
 */
typedef struct CFGMNODE
{
    /** Device under test this CFGM node is for. */
    PTSTDEVDUTINT        pDut;
    /** @todo: */
} CFGMNODE;


/**
 * Private device instance data.
 */
typedef struct PDMDEVINSINTR3
{
    /** Pointer to the device under test the PDM device instance is for. */
    PTSTDEVDUTINT                   pDut;
} PDMDEVINSINTR3;
AssertCompile(sizeof(PDMDEVINSINTR3) <= (HC_ARCH_BITS == 32 ? 72 : 112 + 0x28));

/**
 * Private device instance data.
 */
typedef struct PDMDEVINSINTR0
{
    /** Pointer to the device under test the PDM device instance is for. */
    PTSTDEVDUTINT                   pDut;
} PDMDEVINSINTR0;
AssertCompile(sizeof(PDMDEVINSINTR0) <= (HC_ARCH_BITS == 32 ? 72 : 112 + 0x28));

/**
 * Private device instance data.
 */
typedef struct PDMDEVINSINTRC
{
    /** Pointer to the device under test the PDM device instance is for. */
    PTSTDEVDUTINT                   pDut;
} PDMDEVINSINTRC;
AssertCompile(sizeof(PDMDEVINSINTRC) <= (HC_ARCH_BITS == 32 ? 72 : 112 + 0x28));

typedef struct PDMPCIDEVINT
{
    bool                            fRegistered;
} PDMPCIDEVINT;


/**
 * Internal PDM critical section structure.
 */
typedef struct PDMCRITSECTINT
{
    /** The actual critical section used for emulation. */
    RTCRITSECT           CritSect;
} PDMCRITSECTINT;
AssertCompile(sizeof(PDMCRITSECTINT) <= (HC_ARCH_BITS == 32 ? 0x80 : 0xc0));


/**
 * MM Heap allocation.
 */
typedef struct TSTDEVMMHEAPALLOC
{
    /** Node for the list of allocations. */
    RTLISTNODE                      NdMmHeap;
    /** Pointer to the device under test the allocation was made for. */
    PTSTDEVDUTINT                   pDut;
    /** Size of the allocation. */
    size_t                          cbAlloc;
    /** Start of the real allocation. */
    uint8_t                         abAlloc[RT_FLEXIBLE_ARRAY];
} TSTDEVMMHEAPALLOC;
/** Pointer to a MM Heap allocation. */
typedef TSTDEVMMHEAPALLOC *PTSTDEVMMHEAPALLOC;
/** Pointer to a const MM Heap allocation. */
typedef const TSTDEVMMHEAPALLOC *PCTSTDEVMMHEAPALLOC;

AssertCompileMemberAlignment(TSTDEVMMHEAPALLOC, abAlloc, HC_ARCH_BITS == 64 ? 16 : 8);


#define PDMCRITSECTINT_DECLARED
#define PDMDEVINSINT_DECLARED
#define PDMPCIDEVINT_DECLARED
#define VMM_INCLUDED_SRC_include_VMInternal_h
#define VMM_INCLUDED_SRC_include_VMMInternal_h
RT_C_DECLS_END
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmpci.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/tm.h>
RT_C_DECLS_BEGIN


/**
 * TM timer structure.
 */
typedef struct TMTIMER
{
    /** List of timers created by the device. */
    RTLISTNODE           NdDevTimers;
    /** Clock this timer belongs to. */
    TMCLOCK              enmClock;
    /** Callback to call when the timer expires. */
    PFNTMTIMERDEV        pfnCallbackDev;
    /** Opaque user data to pass to the callback. */
    void                 *pvUser;
    /** Flags. */
    uint32_t             fFlags;
    /** Assigned critical section. */
    PPDMCRITSECT         pCritSect;
    /** @todo: */
} TMTIMER;


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
    PFNIOMIOPORTNEWOUT              pfnOutR3;
    /** In handler - R3. */
    PFNIOMIOPORTNEWIN               pfnInR3;
    /** Out string handler - R3. */
    PFNIOMIOPORTNEWOUTSTRING        pfnOutStrR3;
    /** In string handler - R3. */
    PFNIOMIOPORTNEWINSTRING         pfnInStrR3;

    /** Opaque user data - R0. */
    void                            *pvUserR0;
    /** Out handler - R0. */
    PFNIOMIOPORTNEWOUT              pfnOutR0;
    /** In handler - R0. */
    PFNIOMIOPORTNEWIN               pfnInR0;
    /** Out string handler - R0. */
    PFNIOMIOPORTNEWOUTSTRING        pfnOutStrR0;
    /** In string handler - R0. */
    PFNIOMIOPORTNEWINSTRING         pfnInStrR0;

#ifdef TSTDEV_SUPPORTS_RC
    /** Opaque user data - RC. */
    void                            *pvUserRC;
    /** Out handler - RC. */
    PFNIOMIOPORTNEWOUT              pfnOutRC;
    /** In handler - RC. */
    PFNIOMIOPORTNEWIN               pfnInRC;
    /** Out string handler - RC. */
    PFNIOMIOPORTNEWOUTSTRING        pfnOutStrRC;
    /** In string handler - RC. */
    PFNIOMIOPORTNEWINSTRING         pfnInStrRC;
#endif
} RTDEVDUTIOPORT;
/** Pointer to a registered I/O port handler. */
typedef RTDEVDUTIOPORT *PRTDEVDUTIOPORT;
/** Pointer to a const I/O port handler. */
typedef const RTDEVDUTIOPORT *PCRTDEVDUTIOPORT;


/**
 * Registered SSM handlers.
 */
typedef struct TSTDEVDUTSSM
{
    /** Node for the list of registered SSM handlers. */
    RTLISTNODE                      NdSsm;
    /** Version */
    uint32_t                        uVersion;
    PFNSSMDEVLIVEPREP               pfnLivePrep;
    PFNSSMDEVLIVEEXEC               pfnLiveExec;
    PFNSSMDEVLIVEVOTE               pfnLiveVote;
    PFNSSMDEVSAVEPREP               pfnSavePrep;
    PFNSSMDEVSAVEEXEC               pfnSaveExec;
    PFNSSMDEVSAVEDONE               pfnSaveDone;
    PFNSSMDEVLOADPREP               pfnLoadPrep;
    PFNSSMDEVLOADEXEC               pfnLoadExec;
    PFNSSMDEVLOADDONE               pfnLoadDone;
} TSTDEVDUTSSM;
/** Pointer to the registered SSM handlers. */
typedef TSTDEVDUTSSM *PTSTDEVDUTSSM;
/** Pointer to a const SSM handler. */
typedef const TSTDEVDUTSSM *PCTSTDEVDUTSSM;


/**
 * The Support Driver session state.
 */
typedef struct TSTDEVSUPDRVSESSION
{
    /** Pointer to the owning device under test instance. */
    PTSTDEVDUTINT                   pDut;
    /** List of event semaphores. */
    RTLISTANCHOR                    LstSupSem;
} TSTDEVSUPDRVSESSION;
/** Pointer to the Support Driver session state. */
typedef TSTDEVSUPDRVSESSION *PTSTDEVSUPDRVSESSION;

/** Converts a Support Driver session handle to the internal state. */
#define TSTDEV_PSUPDRVSESSION_2_PTSTDEVSUPDRVSESSION(a_pSession) ((PTSTDEVSUPDRVSESSION)(a_pSession))
/** Converts the internal session state to a Support Driver session handle. */
#define TSTDEV_PTSTDEVSUPDRVSESSION_2_PSUPDRVSESSION(a_pSession) ((PSUPDRVSESSION)(a_pSession))

/**
 * Support driver event semaphore.
 */
typedef struct TSTDEVSUPSEMEVENT
{
    /** Node for the event semaphore list. */
    RTLISTNODE                      NdSupSem;
    /** Flag whether this is multi event semaphore. */
    bool                            fMulti;
    /** Event smeaphore handles depending on the flag above. */
    union
    {
        RTSEMEVENT                  hSemEvt;
        RTSEMEVENTMULTI             hSemEvtMulti;
    } u;
} TSTDEVSUPSEMEVENT;
/** Pointer to a support event semaphore state. */
typedef TSTDEVSUPSEMEVENT *PTSTDEVSUPSEMEVENT;

/** Converts a Support event semaphore handle to the internal state. */
#define TSTDEV_SUPSEMEVENT_2_PTSTDEVSUPSEMEVENT(a_pSupSemEvt) ((PTSTDEVSUPSEMEVENT)(a_pSupSemEvt))
/** Converts the internal session state to a Support event semaphore handle. */
#define TSTDEV_PTSTDEVSUPSEMEVENT_2_SUPSEMEVENT(a_pSupSemEvt) ((SUPSEMEVENT)(a_pSupSemEvt))

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
 * PCI region descriptor.
 */
typedef struct TSTDEVDUTPCIREGION
{
    /** Size of the region. */
    RTGCPHYS                        cbRegion;
    /** Address space type. */
    PCIADDRESSSPACE                 enmType;
    /** Region mapping callback. */
    PFNPCIIOREGIONMAP               pfnRegionMap;
} TSTDEVDUTPCIREGION;
/** Pointer to a PCI region descriptor. */
typedef TSTDEVDUTPCIREGION *PTSTDEVDUTPCIREGION;
/** Pointer to a const PCI region descriptor. */
typedef const TSTDEVDUTPCIREGION *PCTSTDEVDUTPCIREGION;

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
    /** Critical section protecting the lists below. */
    RTCRITSECTRW                    CritSectLists;
    /** List of registered I/O port handlers. */
    RTLISTANCHOR                    LstIoPorts;
    /** List of timers registered. */
    RTLISTANCHOR                    LstTimers;
    /** List of registered MMIO regions. */
    RTLISTANCHOR                    LstMmio;
    /** List of MM Heap allocations. */
    RTLISTANCHOR                    LstMmHeap;
    /** List of PDM threads. */
    RTLISTANCHOR                    LstPdmThreads;
    /** List of SSM handlers (just one normally). */
    RTLISTANCHOR                    LstSsmHandlers;
    /** The SUP session we emulate. */
    TSTDEVSUPDRVSESSION             SupSession;
    /** The NOP critical section. */
    PDMCRITSECT                     CritSectNop;
    /** The VM state associated with this device. */
    PVM                             pVm;
    /** The registered PCI device instance if this is a PCI device. */
    PPDMPCIDEV                      pPciDev;
    /** PCI Region descriptors. */
    TSTDEVDUTPCIREGION              aPciRegions[VBOX_PCI_NUM_REGIONS];
} TSTDEVDUTINT;


extern const PDMDEVHLPR3 g_tstDevPdmDevHlpR3;


DECLHIDDEN(int) tstDevPdmLdrGetSymbol(PTSTDEVDUTINT pThis, const char *pszMod, TSTDEVPDMMODTYPE enmModType,
                                      const char *pszSymbol, PFNRT *ppfn);


DECLINLINE(int) tstDevDutLockShared(PTSTDEVDUTINT pThis)
{
    return RTCritSectRwEnterShared(&pThis->CritSectLists);
}

DECLINLINE(int) tstDevDutUnlockShared(PTSTDEVDUTINT pThis)
{
    return RTCritSectRwLeaveShared(&pThis->CritSectLists);
}

DECLINLINE(int) tstDevDutLockExcl(PTSTDEVDUTINT pThis)
{
    return RTCritSectRwEnterExcl(&pThis->CritSectLists);
}

DECLINLINE(int) tstDevDutUnlockExcl(PTSTDEVDUTINT pThis)
{
    return RTCritSectRwLeaveExcl(&pThis->CritSectLists);
}

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_testcase_tstDeviceInternal_h */
