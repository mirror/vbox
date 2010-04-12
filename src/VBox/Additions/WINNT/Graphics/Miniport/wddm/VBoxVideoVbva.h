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
} VBOXVBVAINFO;

int vboxVbvaEnable (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaDisable (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva);
int vboxVbvaCreate(struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva, ULONG offBuffer, ULONG cbBuffer, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId);
int vboxVbvaReportCmdOffset (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva, uint32_t offCmd);
int vboxVbvaReportDirtyRect (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva, RECT *pRectOrig);
BOOL vboxVbvaBufferBeginUpdate (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva);
void vboxVbvaBufferEndUpdate (struct _DEVICE_EXTENSION* pDevExt, VBOXVBVAINFO *pVbva);

#define VBOXVBVA_OP(_op, _pdext, _pvbva, _arg) \
        if (vboxVbvaBufferBeginUpdate(_pdext, _pvbva)) \
        { \
            vboxVbva##_op(_pdext, _pvbva, _arg); \
            vboxVbvaBufferEndUpdate(_pdext, _pvbva); \
        }

#endif /* #ifndef ___VBoxVideoVbva_h___ */
