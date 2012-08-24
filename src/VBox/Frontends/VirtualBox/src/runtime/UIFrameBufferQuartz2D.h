/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBufferQuartz2D class declarations
 */

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

#ifndef ___UIFrameBufferQuartz2D_h___
#define ___UIFrameBufferQuartz2D_h___

#include <QtGlobal>

#if defined (Q_WS_MAC) && defined (VBOX_GUI_USE_QUARTZ2D)

#include "UIFrameBuffer.h"

#include <ApplicationServices/ApplicationServices.h>

/* Local forward declarations */
class UIMachineLogic;

class UIFrameBufferQuartz2D : public UIFrameBuffer
{
public:

    UIFrameBufferQuartz2D(UIMachineView *pMachineView);
    virtual ~UIFrameBufferQuartz2D();

    STDMETHOD(NotifyUpdate)(ULONG uX, ULONG uY, ULONG uW, ULONG uH);
    STDMETHOD(SetVisibleRegion)(BYTE *pRectangles, ULONG uCount);

    uchar* address() { return m_pDataAddress; }
    ulong bitsPerPixel() { return CGImageGetBitsPerPixel(m_image); }
    ulong bytesPerLine() { return CGImageGetBytesPerRow(m_image); }
    ulong pixelFormat() { return m_uPixelFormat; };
    bool usesGuestVRAM() { return m_pBitmapData == NULL; }

    const CGImageRef imageRef() const { return m_image; }

    void paintEvent(QPaintEvent *pEvent);
    void resizeEvent(UIResizeEvent *pEvent);

#ifdef VBOX_WITH_VIDEOHWACCEL
    void setView(UIMachineView *pView);
#endif

private:

    void clean();

    UIMachineLogic *m_pMachineLogic;
    bool m_fUsesGuestVRAM;
    uchar *m_pDataAddress;
    void *m_pBitmapData;
    ulong m_uPixelFormat;
    CGImageRef m_image;
    typedef struct
    {
        /** The size of this structure expressed in rcts entries. */
        ULONG allocated;
        /** The number of entries in the rcts array. */
        ULONG used;
        /** Variable sized array of the rectangle that makes up the region. */
        CGRect rcts[1];
    } RegionRects;
    /** The current valid region, all access is by atomic cmpxchg or atomic xchg.
     *
     * The protocol for updating and using this has to take into account that
     * the producer (SetVisibleRegion) and consumer (paintEvent) are running
     * on different threads. Therefore the producer will create a new RegionRects
     * structure before atomically replace the existing one. While the consumer
     * will read the value by atomically replace it by NULL, and then when its
     * done try restore it by cmpxchg. If the producer has already put a new
     * region there, it will be discarded (see mRegionUnused).
     */
    RegionRects * volatile mRegion;
    /** For keeping the unused region and thus avoid some RTMemAlloc/RTMemFree calls.
     * This is operated with atomic cmpxchg and atomic xchg. */
    RegionRects * volatile mRegionUnused;

};
#endif /* Q_WS_MAC && VBOX_GUI_USE_QUARTZ2D */

#endif // !___UIFrameBufferQuartz2D_h___

