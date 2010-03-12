/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#include "SHGSMIHost.h"
#include <VBox/VBoxVideo.h>

/*
 * VBOXSHGSMI made on top HGSMI and allows receiving notifications
 * about G->H command completion
 */
static bool vboxSHGSMICommandCanCompleteSynch (PVBOXSHGSMIHEADER pHdr)
{
    return !(pHdr->fFlags & VBOXSHGSMI_FLAG_GH_ASYNCH_FORCE);
}

static int vboxSHGSMICommandCompleteAsynch (PHGSMIINSTANCE pIns, PVBOXSHGSMIHEADER pHdr)
{
    bool bDoIrq = !!(pHdr->fFlags & VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ)
            || !!(pHdr->fFlags & VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ_FORCE);
    return HGSMICompleteGuestCommand(pIns, pHdr, bDoIrq);
}

void VBoxSHGSMICommandMarkAsynchCompletion (void *pvData)
{
    PVBOXSHGSMIHEADER pHdr = VBoxSHGSMIBufferHeader (pvData);
    Assert(!(pHdr->fFlags & VBOXSHGSMI_FLAG_HG_ASYNCH));
    pHdr->fFlags |= VBOXSHGSMI_FLAG_HG_ASYNCH;
}

int VBoxSHGSMICommandComplete (PHGSMIINSTANCE pIns, void *pvData)
{
    PVBOXSHGSMIHEADER pHdr = VBoxSHGSMIBufferHeader (pvData);
    if (!(pHdr->fFlags & VBOXSHGSMI_FLAG_HG_ASYNCH) /* <- check if synchronous completion */
            && vboxSHGSMICommandCanCompleteSynch(pHdr)) /* <- check if can complete synchronously */
        return VINF_SUCCESS;
    pHdr->fFlags |= VBOXSHGSMI_FLAG_HG_ASYNCH;
    return vboxSHGSMICommandCompleteAsynch(pIns, pHdr);
}
