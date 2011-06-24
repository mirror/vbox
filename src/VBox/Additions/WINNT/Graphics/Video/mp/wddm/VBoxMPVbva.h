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

#ifndef ___VBoxMPVbva_h___
#define ___VBoxMPVbva_h___

typedef struct VBOXVBVAINFO
{
    VBOXVIDEOOFFSET offVBVA;
    uint32_t cbVBVA;
    VBVABUFFER *pVBVA;
    BOOL  fHwBufferOverflow;
    VBVARECORD *pRecord;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId;
    KSPIN_LOCK Lock;
} VBOXVBVAINFO;

int vboxVbvaEnable(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaDisable(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaDestroy(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaCreate(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva, ULONG offBuffer, ULONG cbBuffer, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vboxVbvaReportCmdOffset(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva, uint32_t offCmd);
int vboxVbvaReportDirtyRect(PVBOXMP_DEVEXT pDevExt, struct VBOXWDDM_SOURCE *pSrc, RECT *pRectOrig);
BOOL vboxVbvaBufferBeginUpdate(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva);
void vboxVbvaBufferEndUpdate(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva);

#define VBOXVBVA_OP(_op, _pdext, _psrc, _arg) \
        do { \
            if (vboxVbvaBufferBeginUpdate(_pdext, &(_psrc)->Vbva)) \
            { \
                vboxVbva##_op(_pdext, _psrc, _arg); \
                vboxVbvaBufferEndUpdate(_pdext, &(_psrc)->Vbva); \
            } \
        } while (0)

#define VBOXVBVA_OP_WITHLOCK_ATDPC(_op, _pdext, _psrc, _arg) \
        do { \
            Assert(KeGetCurrentIrql() == DISPATCH_LEVEL); \
            KeAcquireSpinLockAtDpcLevel(&(_psrc)->Vbva.Lock);  \
            VBOXVBVA_OP(_op, _pdext, _psrc, _arg);        \
            KeReleaseSpinLockFromDpcLevel(&(_psrc)->Vbva.Lock);\
        } while (0)

#define VBOXVBVA_OP_WITHLOCK(_op, _pdext, _psrc, _arg) \
        do { \
            KIRQL OldIrql; \
            KeAcquireSpinLock(&(_psrc)->Vbva.Lock, &OldIrql);  \
            VBOXVBVA_OP(_op, _pdext, _psrc, _arg);        \
            KeReleaseSpinLock(&(_psrc)->Vbva.Lock, OldIrql);   \
        } while (0)


#endif /* #ifndef ___VBoxMPVbva_h___ */
