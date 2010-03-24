/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBuffer class and subclasses declarations
 */

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

#ifndef ___UIFrameBufferQGL_h___
#define ___UIFrameBufferQGL_h___

/* Global includes */
#include "UIFrameBuffer.h"
#if defined (VBOX_GUI_USE_QGLFB)
#include "VBoxFBOverlay.h"

class UIFrameBufferQGL : public UIFrameBuffer
{
public:

    UIFrameBufferQGL(UIMachineView *pMachineView);

    STDMETHOD(NotifyUpdate) (ULONG uX, ULONG uY, ULONG uW, ULONG uH);
#ifdef VBOXQGL_PROF_BASE
    STDMETHOD(RequestResize) (ULONG uScreenId, ULONG uPixelFormat,
                              BYTE *pVRAM, ULONG uBitsPerPixel, ULONG uBytesPerLine,
                              ULONG uWidth, ULONG uHeight, BOOL *pbFinished);
#endif

#ifdef VBOX_WITH_VIDEOHWACCEL
    STDMETHOD(ProcessVHWACommand)(BYTE *pbCommand);
#endif

    ulong pixelFormat() { return vboxWidget()->vboxPixelFormat(); }
    bool usesGuestVRAM() { return vboxWidget()->vboxUsesGuestVRAM(); }

    uchar *address() { return vboxWidget()->vboxAddress(); }
    ulong bitsPerPixel() { return vboxWidget()->vboxBitsPerPixel(); }
    ulong bytesPerLine() { return vboxWidget()->vboxBytesPerLine(); }

    void paintEvent (QPaintEvent *pEvent);
    void resizeEvent (UIResizeEvent *pEvent);
    void doProcessVHWACommand(QEvent *pEvent);

private:

    class VBoxGLWidget* vboxWidget();

    class VBoxVHWACommandElementProcessor m_cmdPipe;
};
#endif

#endif // !___UIFrameBufferQGL_h___

