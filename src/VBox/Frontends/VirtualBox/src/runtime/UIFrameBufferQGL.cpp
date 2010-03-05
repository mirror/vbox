/* $Id$ */
/** @file
 * VBoxFBQGL Opengl-based FrameBuffer implementation
 */

/*
 * Copyright (C) 2009-2010 Sun Microsystems, Inc.
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
#if defined (VBOX_GUI_USE_QGL)

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
#define LOG_GROUP LOG_GROUP_GUI

/* Global includes */
#include "UIFrameBufferQGL.h"
#include "UIMachineView.h"

/* Qt includes */
#include <QGLWidget>

#ifdef VBOX_WITH_VIDEOHWACCEL
#include <VBox/VBoxVideo.h>
#endif
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** @class UIFrameBufferQGL
 *
 *  The VBoxQImageFrameBuffer class is a class that implements the IFrameBuffer
 *  interface and uses QImage as the direct storage for VM display data. QImage
 *  is then converted to QPixmap and blitted to the console view widget.
 */

UIFrameBufferQGL::UIFrameBufferQGL (UIMachineView *pMachineView) :
    UIFrameBuffer(pMachineView),
    m_cmdPipe(pMachineView)
{
//    mWidget = new GLWidget(aView->viewport());
#ifndef VBOXQGL_PROF_BASE
    UIResizeEvent event(FramebufferPixelFormat_Opaque, NULL, 0, 0, 640, 480);
    resizeEvent(&event);
#else
    UIResizeEvent event(FramebufferPixelFormat_Opaque, NULL, 0, 0, VBOXQGL_PROF_WIDTH, VBOXQGL_PROF_HEIGHT);
    resizeEvent(&event);
#endif
}

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP UIFrameBufferQGL::NotifyUpdate (ULONG aX, ULONG aY,
                                                  ULONG aW, ULONG aH)
{
//    /* We're not on the GUI thread and update() isn't thread safe in
//     * Qt 4.3.x on the Win, Qt 3.3.x on the Mac (4.2.x is),
//     * on Linux (didn't check Qt 4.x there) and probably on other
//     * non-DOS platforms, so post the event instead. */
#ifdef VBOXQGL_PROF_BASE
    QApplication::postEvent (m_pMachineView,
                             new VBoxRepaintEvent (aX, aY, aW, aH));
#else
    QRect r(aX, aY, aW, aH);
    m_cmdPipe.postCmd(VBOXVHWA_PIPECMD_PAINT, &r, 0);
#endif
    return S_OK;
}

#ifdef VBOXQGL_PROF_BASE
STDMETHODIMP UIFrameBufferQGL::RequestResize (ULONG aScreenId, ULONG aPixelFormat,
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

VBoxGLWidget* UIFrameBufferQGL::vboxWidget()
{
    return (VBoxGLWidget*)m_pMachineView->viewport();
}

void UIFrameBufferQGL::paintEvent (QPaintEvent *pe)
{
    Q_UNUSED(pe);
    VBoxGLWidget * pw = vboxWidget();
    pw->makeCurrent();

    QRect vp(m_pMachineView->contentsX(), m_pMachineView->contentsY(), pw->width(), pw->height());
    if(vp != pw->vboxViewport())
    {
        pw->vboxDoUpdateViewport(vp);
    }

    pw->performDisplayAndSwap(true);
}

void UIFrameBufferQGL::resizeEvent (UIResizeEvent *re)
{
    m_width = re->width();
    m_height = re->height();

    vboxWidget()->vboxResizeEvent(re);
}

/* processing the VHWA command, called from the GUI thread */
void UIFrameBufferQGL::doProcessVHWACommand(QEvent * pEvent)
{
    Q_UNUSED(pEvent);
    vboxWidget()->vboxProcessVHWACommands(&m_cmdPipe);
}


#ifdef VBOX_WITH_VIDEOHWACCEL

STDMETHODIMP UIFrameBufferQGL::ProcessVHWACommand(BYTE *pCommand)
{
    VBOXVHWACMD * pCmd = (VBOXVHWACMD*)pCommand;
//    Assert(0);
    /* indicate that we process and complete the command asynchronously */
    pCmd->Flags |= VBOXVHWACMD_FLAG_HG_ASYNCH;
    /* post the command to the GUI thread for processing */
//    QApplication::postEvent (m_pMachineView,
//                             new VBoxVHWACommandProcessEvent (pCmd));
    m_cmdPipe.postCmd(VBOXVHWA_PIPECMD_VHWA, pCmd, 0);
    return S_OK;
}

#endif

#endif

