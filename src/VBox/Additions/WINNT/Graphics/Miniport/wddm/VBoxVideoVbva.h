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
 */
#ifndef ___VBoxVideoVbva_h___
#define ___VBoxVideoVbva_h___

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

int vboxVbvaEnable (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaDisable (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaDestroy(PDEVICE_EXTENSION pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaCreate(struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva, ULONG offBuffer, ULONG cbBuffer, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vboxVbvaReportCmdOffset (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva, uint32_t offCmd);
int vboxVbvaReportDirtyRect (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva, RECT *pRectOrig);
BOOL vboxVbvaBufferBeginUpdate (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva);
void vboxVbvaBufferEndUpdate (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva);

#define VBOXVBVA_OP(_op, _pdext, _pvbva, _arg) \
        do { \
            if (vboxVbvaBufferBeginUpdate(_pdext, _pvbva)) \
            { \
                vboxVbva##_op(_pdext, _pvbva, _arg); \
                vboxVbvaBufferEndUpdate(_pdext, _pvbva); \
            } \
        } while (0)

#define VBOXVBVA_OP_WITHLOCK_ATDPC(_op, _pdext, _pvbva, _arg) \
        do { \
            KeAcquireSpinLockAtDpcLevel(&(_pvbva)->Lock);  \
            VBOXVBVA_OP(_op, _pdext, _pvbva, _arg);        \
            KeReleaseSpinLockFromDpcLevel(&(_pvbva)->Lock);\
        } while (0)

#define VBOXVBVA_OP_WITHLOCK(_op, _pdext, _pvbva, _arg) \
        do { \
            KIRQL OldIrql; \
            KeAcquireSpinLock(&(_pvbva)->Lock, &OldIrql);  \
            VBOXVBVA_OP(_op, _pdext, _pvbva, _arg);        \
            KeReleaseSpinLock(&(_pvbva)->Lock, OldIrql);   \
        } while (0)


#endif /* #ifndef ___VBoxVideoVbva_h___ */
