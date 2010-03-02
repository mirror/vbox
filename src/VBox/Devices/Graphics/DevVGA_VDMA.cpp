/** @file
 * Video DMA (VDMA) support.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
//#include <VBox/VMMDev.h>
#include <VBox/pdmdev.h>
#include <VBox/VBoxVideo.h>

#include "DevVGA.h"
#include "HGSMI/SHGSMIHost.h"
#include "HGSMI/HGSMIHostHlp.h"

typedef struct VBOXVDMAPIPE
{
    HGSMILIST List;

    RTSEMEVENT hEvent;
    /* critical section for accessing pipe properties */
    RTCRITSECT hCritSect;
    /* true iff the other end needs Event notification */
    bool bNeedNotify;
} VBOXVDMAPIPE, *PVBOXVDMAPIPE;

typedef struct VBOXVDMAHOST
{
    VBOXVDMAPIPE Pipe;
    RTTHREAD hWorkerThread;
} VBOXVDMAHOST, *PVBOXVDMAHOST;

int vboxVDMAConstruct(PVGASTATE pVGAState, struct VBOXVDMAHOST **ppVdma)
{
    return VINF_SUCCESS;
}

int vboxVDMADestruct(PVGASTATE pVGAState, struct VBOXVDMAHOST **pVdma)
{
    return VINF_SUCCESS;
}

void vboxVDMAControl(PVGASTATE pVGAState, struct VBOXVDMAHOST *pVdma, PVBOXVDMA_CTL pCmd)
{
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;

    switch (pCmd->enmCtl)
    {
        case VBOXVDMA_CTL_TYPE_ENABLE:
            pCmd->i32Result = VINF_SUCCESS;
            break;
        case VBOXVDMA_CTL_TYPE_DISABLE:
            pCmd->i32Result = VINF_SUCCESS;
            break;
        case VBOXVDMA_CTL_TYPE_FLUSH:
            pCmd->i32Result = VINF_SUCCESS;
            break;
        default:
            AssertBreakpoint();
            pCmd->i32Result = VERR_NOT_SUPPORTED;
    }

    int rc = VBoxSHGSMICommandCompleteSynch (pIns, pCmd);
    AssertRC(rc);
}

void vboxVDMACommand(PVGASTATE pVGAState, struct VBOXVDMAHOST *pVdma, PVBOXVDMACBUF_DR pCmd)
{
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    pCmd->rc = VINF_SUCCESS;

    /* @todo: submit command for processing thread */

    int rc = VBoxSHGSMICommandCompleteSynch (pIns, pCmd);
    AssertRC(rc);
}

