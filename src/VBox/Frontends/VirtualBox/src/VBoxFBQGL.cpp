/* $Id$ */
/** @file
 * VBoxFBQGL Opengl-based FrameBuffer implementation
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#if defined (VBOX_GUI_USE_QGLFB)

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
#define LOG_GROUP LOG_GROUP_GUI

#include "VBoxFrameBuffer.h"

/* Qt includes */
#include <QGLWidget>

#ifdef VBOX_WITH_VIDEOHWACCEL
#include <VBox/VBoxVideo.h>
#endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** @class VBoxQGLFrameBuffer
 *
 *  The VBoxQImageFrameBuffer class is a class that implements the IFrameBuffer
 *  interface and uses QImage as the direct storage for VM display data. QImage
 *  is then converted to QPixmap and blitted to the console view widget.
 */

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP VBoxQGLFrameBuffer::NotifyUpdate (ULONG aX, ULONG aY,
                                                  ULONG aW, ULONG aH)
{
//    /* We're not on the GUI thread and update() isn't thread safe in
//     * Qt 4.3.x on the Win, Qt 3.3.x on the Mac (4.2.x is),
//     * on Linux (didn't check Qt 4.x there) and probably on other
//     * non-DOS platforms, so post the event instead. */
#ifdef VBOXQGL_PROF_BASE
    QApplication::postEvent (mView,
                             new VBoxRepaintEvent (aX, aY, aW, aH));
#else
    QRect r(aX, aY, aW, aH);
    mCmdPipe.postCmd(VBOXVHWA_PIPECMD_PAINT, &r, 0);
#endif
    return S_OK;
}

#ifdef VBOXQGL_PROF_BASE
STDMETHODIMP VBoxQGLFrameBuffer::RequestResize (ULONG aScreenId, ULONG aPixelFormat,
                              BYTE *aVRAM, ULONG aBitsPerPixel, ULONG aBytesPerLine,
                              ULONG aWidth, ULONG aHeight,
                              BOOL *aFinished)
{
    aWidth = VBOXQGL_PROF_WIDTH;
    aHeight = VBOXQGL_PROF_HEIGHT;
    VBoxFrameBuffer::RequestResize (aScreenId, aPixelFormat,
            aVRAM, aBitsPerPixel, aBytesPerLine,
            aWidth, aHeight,
            aFinished);

//    if(aVRAM)
    {
        for(;;)
        {
            ULONG aX = 0;
            ULONG aY = 0;
            ULONG aW = aWidth;
            ULONG aH = aHeight;
            NotifyUpdate (aX, aY, aW, aH);
            RTThreadSleep(40);
        }
    }
    return S_OK;
}
#endif

VBoxGLWidget* VBoxQGLFrameBuffer::vboxWidget()
{
    return (VBoxGLWidget*)mView->viewport();
}

void VBoxQGLFrameBuffer::paintEvent (QPaintEvent *pe)
{
    Q_UNUSED(pe);
    VBoxGLWidget * pw = vboxWidget();
    pw->makeCurrent();

    QRect vp(mView->contentsX(), mView->contentsY(), pw->width(), pw->height());
    if(vp != pw->vboxViewport())
    {
        pw->vboxDoUpdateViewport(vp);
    }

    pw->performDisplayAndSwap(true);
}

void VBoxQGLFrameBuffer::resizeEvent (VBoxResizeEvent *re)
{
    mWdt = re->width();
    mHgt = re->height();

    vboxWidget()->vboxResizeEvent(re);
}

/* processing the VHWA command, called from the GUI thread */
void VBoxQGLFrameBuffer::doProcessVHWACommand(QEvent * pEvent)
{
    Q_UNUSED(pEvent);
    vboxWidget()->vboxProcessVHWACommands(&mCmdPipe);
}


#ifdef VBOX_WITH_VIDEOHWACCEL

STDMETHODIMP VBoxQGLFrameBuffer::ProcessVHWACommand(BYTE *pCommand)
{
    VBOXVHWACMD * pCmd = (VBOXVHWACMD*)pCommand;
//    Assert(0);
    /* indicate that we process and complete the command asynchronously */
    pCmd->Flags |= VBOXVHWACMD_FLAG_HG_ASYNCH;
    /* post the command to the GUI thread for processing */
//    QApplication::postEvent (mView,
//                             new VBoxVHWACommandProcessEvent (pCmd));
    mCmdPipe.postCmd(VBOXVHWA_PIPECMD_VHWA, pCmd, 0);
    return S_OK;
}

#endif

#endif

