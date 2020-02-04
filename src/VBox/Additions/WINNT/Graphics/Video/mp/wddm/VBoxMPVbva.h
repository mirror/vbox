/* $Id$ */
/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVbva_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVbva_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>  /* for VBOXCALL */

typedef struct VBOXVBVAINFO
{
    VBVABUFFERCONTEXT Vbva;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId;
    KSPIN_LOCK Lock;
} VBOXVBVAINFO;

int vboxVbvaEnable(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaDisable(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaDestroy(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaCreate(PVBOXMP_DEVEXT pDevExt, VBOXVBVAINFO *pVbva, ULONG offBuffer, ULONG cbBuffer, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vboxVbvaReportDirtyRect(PVBOXMP_DEVEXT pDevExt, struct VBOXWDDM_SOURCE *pSrc, RECT *pRectOrig);

#define VBOXVBVA_OP(_op, _pdext, _psrc, _arg) \
        do { \
            if (VBoxVBVABufferBeginUpdate(&(_psrc)->Vbva.Vbva, &VBoxCommonFromDeviceExt(_pdext)->guestCtx)) \
            { \
                vboxVbva##_op(_pdext, _psrc, _arg); \
                VBoxVBVABufferEndUpdate(&(_psrc)->Vbva.Vbva); \
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

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_VBoxMPVbva_h */
