/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxMPShgsmi_h___
#define ___VBoxMPShgsmi_h___

#include <iprt/cdefs.h>
#include <VBox/VBoxVideo.h>
#include "../../common/VBoxVideoTools.h"

typedef DECLCALLBACK(void) FNVBOXSHGSMICMDCOMPLETION(struct _HGSMIHEAP * pHeap, void *pvCmd, void *pvContext);
typedef FNVBOXSHGSMICMDCOMPLETION *PFNVBOXSHGSMICMDCOMPLETION;

typedef DECLCALLBACK(void) FNVBOXSHGSMICMDCOMPLETION_IRQ(struct _HGSMIHEAP * pHeap, void *pvCmd, void *pvContext,
                                        PFNVBOXSHGSMICMDCOMPLETION *ppfnCompletion, void **ppvCompletion);
typedef FNVBOXSHGSMICMDCOMPLETION_IRQ *PFNVBOXSHGSMICMDCOMPLETION_IRQ;


const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynchEvent(struct _HGSMIHEAP * pHeap, PVOID pvBuff, RTSEMEVENT hEventSem);
const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepSynch(struct _HGSMIHEAP * pHeap, PVOID pCmd);
const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynch(struct _HGSMIHEAP * pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION pfnCompletion, PVOID pvCompletion, uint32_t fFlags);
const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynchIrq(struct _HGSMIHEAP * pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION_IRQ pfnCompletion, PVOID pvCompletion, uint32_t fFlags);

void VBoxSHGSMICommandDoneAsynch(struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader);
int VBoxSHGSMICommandDoneSynch(struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader);
void VBoxSHGSMICommandCancelAsynch(struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader);
void VBoxSHGSMICommandCancelSynch(struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader);

DECLINLINE(HGSMIOFFSET) VBoxSHGSMICommandOffset(struct _HGSMIHEAP * pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    return HGSMIHeapBufferOffset(pHeap, (void*)pHeader);
}

void* VBoxSHGSMICommandAlloc(struct _HGSMIHEAP * pHeap, HGSMISIZE cbData, uint8_t u8Channel, uint16_t u16ChannelInfo);
void VBoxSHGSMICommandFree(struct _HGSMIHEAP * pHeap, void *pvBuffer);
int VBoxSHGSMICommandProcessCompletion(struct _HGSMIHEAP * pHeap, VBOXSHGSMIHEADER* pCmd, bool bIrq, PVBOXVTLIST pPostProcessList);
int VBoxSHGSMICommandPostprocessCompletion(struct _HGSMIHEAP * pHeap, PVBOXVTLIST pPostProcessList);

#endif /* #ifndef ___VBoxMPShgsmi_h___ */
