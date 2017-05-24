/** @file
 * VBox Host Guest Shared Memory Interface (HGSMI) command contexts.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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


#ifndef ___VBox_Graphics_HGSMIContext_h___
#define ___VBox_Graphics_HGSMIContext_h___

#include <HGSMI.h>
#include <HGSMIChSetup.h>
#include <VBoxVideoIPRT.h>

#ifdef VBOX_WDDM_MINIPORT
# include "wddm/VBoxMPShgsmi.h"
 typedef VBOXSHGSMI HGSMIGUESTCMDHEAP;
# define HGSMIGUESTCMDHEAP_GET(_p) (&(_p)->Heap)
#else
 typedef HGSMIHEAP HGSMIGUESTCMDHEAP;
# define HGSMIGUESTCMDHEAP_GET(_p) (_p)
#endif

RT_C_DECLS_BEGIN

/**
 * Structure grouping the context needed for submitting commands to the host
 * via HGSMI
 */
typedef struct HGSMIGUESTCOMMANDCONTEXT
{
    /** Information about the memory heap located in VRAM from which data
     * structures to be sent to the host are allocated. */
    HGSMIGUESTCMDHEAP heapCtx;
    /** The I/O port used for submitting commands to the host by writing their
     * offsets into the heap. */
    RTIOPORT port;
} HGSMIGUESTCOMMANDCONTEXT, *PHGSMIGUESTCOMMANDCONTEXT;


/**
 * Structure grouping the context needed for receiving commands from the host
 * via HGSMI
 */
typedef struct HGSMIHOSTCOMMANDCONTEXT
{
    /** Information about the memory area located in VRAM in which the host
     * places data structures to be read by the guest. */
    HGSMIAREA areaCtx;
    /** Convenience structure used for matching host commands to handlers. */
    /** @todo handlers are registered individually in code rather than just
     * passing a static structure in order to gain extra flexibility.  There is
     * currently no expected usage case for this though.  Is the additional
     * complexity really justified? */
    HGSMICHANNELINFO channels;
    /** Flag to indicate that one thread is currently processing the command
     * queue. */
    volatile bool fHostCmdProcessing;
    /* Pointer to the VRAM location where the HGSMI host flags are kept. */
    volatile HGSMIHOSTFLAGS *pfHostFlags;
    /** The I/O port used for receiving commands from the host as offsets into
     * the memory area and sending back confirmations (command completion,
     * IRQ acknowlegement). */
    RTIOPORT port;
} HGSMIHOSTCOMMANDCONTEXT, *PHGSMIHOSTCOMMANDCONTEXT;

/** @name HGSMI context initialisation APIs.
 * @{ */

/** @todo we should provide a cleanup function too as part of the API */
DECLHIDDEN(int)      VBoxHGSMISetupGuestContext(PHGSMIGUESTCOMMANDCONTEXT pCtx,
                                                void *pvGuestHeapMemory,
                                                uint32_t cbGuestHeapMemory,
                                                uint32_t offVRAMGuestHeapMemory,
                                                const HGSMIENV *pEnv);
DECLHIDDEN(void)     VBoxHGSMISetupHostContext(PHGSMIHOSTCOMMANDCONTEXT pCtx,
                                               void *pvBaseMapping,
                                               uint32_t offHostFlags,
                                               void *pvHostAreaMapping,
                                               uint32_t offVRAMHostArea,
                                               uint32_t cbHostArea);

/** @}  */

RT_C_DECLS_END

#endif
