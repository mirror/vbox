/** @file
 *
 * Seamless mode:
 * Linux guest.
 */

/*
 * Copyright (C) 2008 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*****************************************************************************
*   Header files                                                             *
*****************************************************************************/

#include <iprt/log.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <VBox/VBoxGuest.h>

#include "seamless-guest.h"

#include <X11/Xatom.h>
#include <X11/Xmu/WinUtil.h>

/*****************************************************************************
* Static functions                                                           *
*****************************************************************************/

static unsigned char *XXGetProperty (Display *aDpy, Window aWnd, Atom aPropType,
                                    const char *aPropName, unsigned long *nItems)
{
    Atom propNameAtom = XInternAtom (aDpy, aPropName,
                                     True /* only_if_exists */);
    if (propNameAtom == None)
    {
        return NULL;
    }

    Atom actTypeAtom = None;
    int actFmt = 0;
    unsigned long nBytesAfter = 0;
    unsigned char *propVal = 0;
    int rc = XGetWindowProperty (aDpy, aWnd, propNameAtom,
                                 0, LONG_MAX, False /* delete */,
                                 aPropType, &actTypeAtom, &actFmt,
                                 nItems, &nBytesAfter, &propVal);
    if (rc != Success)
        return NULL;

    return propVal;
}

/**
  * Initialise the guest and ensure that it is capable of handling seamless mode
  *
  * @returns true if it can handle seamless, false otherwise
  */
int VBoxGuestSeamlessX11::init(VBoxGuestSeamlessObserver *pObserver)
{
    int rc = VINF_SUCCESS;

    if (0 != mObserver)  /* Assertion */
    {
        LogRel(("VBoxClient: ERROR: attempt to initialise seamless guest object twice!\n"));
        return VERR_INTERNAL_ERROR;
    }
    if (!mDisplay.init())
    {
        LogRel(("VBoxClient: seamless guest object failed to acquire a connection to the display.\n"));
        return VERR_ACCESS_DENIED;
    }
    mObserver = pObserver;
    return rc;
}

/**
 * Read information about currently visible windows in the guest and subscribe to X11
 * events about changes to this information.
 *
 * @note This class does not contain its own event thread, so an external thread must
 *       call nextEvent() for as long as events are wished.
 * @todo This function should switch the guest to fullscreen mode.
 */
int VBoxGuestSeamlessX11::start(void)
{
    int rc = VINF_SUCCESS;
    /** Dummy values for XShapeQueryExtension */
    int error, event;

    mSupportsShape = XShapeQueryExtension(mDisplay, &event, &error);
    mEnabled = true;
    monitorClientList();
    rebuildWindowTree();
    return rc;
}

/** Stop reporting seamless events to the host.  Free information about guest windows
    and stop requesting updates. */
void VBoxGuestSeamlessX11::stop(void)
{
    mEnabled = false;
    unmonitorClientList();
    freeWindowTree();
}

void VBoxGuestSeamlessX11::monitorClientList(void)
{
    XSelectInput(mDisplay, DefaultRootWindow(mDisplay.get()), SubstructureNotifyMask);
}

void VBoxGuestSeamlessX11::unmonitorClientList(void)
{
    XSelectInput(mDisplay, DefaultRootWindow(mDisplay.get()), 0);
}

/**
 * Recreate the table of toplevel windows of clients on the default root window of the
 * X server.
 */
void VBoxGuestSeamlessX11::rebuildWindowTree(void)
{
    freeWindowTree();
    addClients(DefaultRootWindow(mDisplay.get()));
}


/**
 * Look at the list of children of a virtual root window and add them to the list of clients
 * if they belong to a client which is not a virtual root.
 *
 * @param hRoot the virtual root window to be examined
 */
void VBoxGuestSeamlessX11::addClients(const Window hRoot)
{
    /** Unused out parameters of XQueryTree */
    Window hRealRoot, hParent;
    /** The list of children of the root supplied, raw pointer */
    Window *phChildrenRaw;
    /** The list of children of the root supplied, auto-pointer */
    VBoxGuestX11Pointer<Window> phChildren;
    /** The number of children of the root supplied */
    unsigned cChildren;

    if (!XQueryTree(mDisplay.get(), hRoot, &hRealRoot, &hParent, &phChildrenRaw, &cChildren))
        return;
    phChildren = phChildrenRaw;
    for (unsigned i = 0; i < cChildren; ++i)
    {
        Window hClient = XmuClientWindow(mDisplay.get(), phChildren.get()[i]);
        if (!isVirtualRoot(hClient))
            addClientWindow(phChildren.get()[i]);
    }
}


/**
 * Checks whether a window is a virtual root.
 * @returns true if it is, false otherwise
 * @param hWin the window to be examined
 */
bool VBoxGuestSeamlessX11::isVirtualRoot(Window hWin)
{
    unsigned char *windowTypeRaw;
    VBoxGuestX11Pointer<Atom> windowType;
    unsigned long ulCount;
    bool rc = false;

    windowTypeRaw = XXGetProperty(mDisplay, hWin, XA_ATOM, WM_TYPE_PROP, &ulCount);
    windowType = reinterpret_cast<Atom *>(windowTypeRaw);
    if (   (ulCount != 0)
        && (*windowType == XInternAtom(mDisplay, WM_TYPE_DESKTOP_PROP, True)))
        rc = true;
    return rc;
}


void VBoxGuestSeamlessX11::addClientWindow(const Window hWin)
{
    XWindowAttributes winAttrib;
    VBoxGuestX11Pointer<XRectangle> rects;
    int cRects = 0, iOrdering;

    if (!XGetWindowAttributes(mDisplay, hWin, &winAttrib))
    {
        LogRelFunc(("VBoxClient: Failed to get the window attributes for window %d\n", hWin));
        return;
    }
    if (mSupportsShape)
    {
        XShapeSelectInput(mDisplay, hWin, ShapeNotify);
        rects = XShapeGetRectangles(mDisplay, hWin, ShapeClip, &cRects, &iOrdering);
        if (0 == rects.get())
        {
            cRects = 0;
        }
    }
    mGuestWindows.addWindow(hWin, winAttrib.map_state != IsUnmapped, winAttrib.x, winAttrib.y,
                            winAttrib.width, winAttrib.height, cRects, rects);
}

/**
 * Free all information in the tree of visible windows
 */
void VBoxGuestSeamlessX11::freeWindowTree(void)
{
    /* We use post-increment in the operation to prevent the iterator from being invalidated. */
    for (VBoxGuestWindowList::iterator it = mGuestWindows.begin(); it != mGuestWindows.end();
                 mGuestWindows.removeWindow(it++))
    {
        XShapeSelectInput(mDisplay, it->first, 0);
    }
}

/**
 * Waits for a position or shape-related event from guest windows 
 *
 * @note Called from the guest event thread.
 */
void VBoxGuestSeamlessX11::nextEvent(void)
{
    XEvent event;

    /* Start by sending information about the current window setup to the host.  We do this
       here because we want to send all such information from a single thread. */
    mObserver->notify();
    XNextEvent(mDisplay, &event);
    switch (event.type)
    {
    case ConfigureNotify:
        doConfigureEvent(&event.xconfigure);
        break;
    case MapNotify:
        doMapEvent(&event.xmap);
        break;
    case PropertyNotify:
        // doPropertyEvent(&event.xproperty);
        break;
    case ShapeNotify:
        doShapeEvent(reinterpret_cast<XShapeEvent *>(&event));
        break;
    case UnmapNotify:
        doUnmapEvent(&event.xunmap);
        break;
    default:
        break;
    }
}

/**
 * Handle a configuration event in the seamless event thread by setting the new position.
 *
 * @param event the X11 event structure
 */
void VBoxGuestSeamlessX11::doConfigureEvent(const XConfigureEvent *event)
{
    VBoxGuestWindowList::iterator iter;

    iter = mGuestWindows.find(event->window);
    if (iter != mGuestWindows.end())
    {
        iter->second->mX = event->x;
        iter->second->mY = event->y;
        iter->second->mWidth = event->width;
        iter->second->mHeight = event->height;
    }
}

/**
 * Handle a map event in the seamless event thread.
 *
 * @param event the X11 event structure
 */
void VBoxGuestSeamlessX11::doMapEvent(const XMapEvent *event)
{
    VBoxGuestWindowList::iterator iter;

    iter = mGuestWindows.find(event->window);
    if (mGuestWindows.end() == iter)
    {
        addClientWindow(event->window);
    }
}

/**
 * Handle a window shape change event in the seamless event thread.
 *
 * @param event the X11 event structure
 */
void VBoxGuestSeamlessX11::doShapeEvent(const XShapeEvent *event)
{
    VBoxGuestWindowList::iterator iter;
    VBoxGuestX11Pointer<XRectangle> rects;
    int cRects, iOrdering;

    iter = mGuestWindows.find(event->window);
    if (iter != mGuestWindows.end())
    {
        rects = XShapeGetRectangles(mDisplay, iter->first, ShapeClip, &cRects, &iOrdering);
        if (0 == rects.get())
        {
            cRects = 0;
        }
        iter->second->mapRects = rects;
        iter->second->mcRects = cRects;
    }
}

/**
 * Handle an unmap event in the seamless event thread.
 *
 * @param event the X11 event structure
 */
void VBoxGuestSeamlessX11::doUnmapEvent(const XUnmapEvent *event)
{
    VBoxGuestWindowList::iterator iter;

    iter = mGuestWindows.find(event->window);
    if (mGuestWindows.end() != iter)
    {
        mGuestWindows.removeWindow(iter);
    }
}

/**
 * Sends an updated list of visible rectangles to the host
 */
std::auto_ptr<std::vector<RTRECT> > VBoxGuestSeamlessX11::getRects(void)
{
    unsigned cRects = 0;
    std::auto_ptr<std::vector<RTRECT> > apRects(new std::vector<RTRECT>);

    if (0 != mcRects)
    {
            apRects.get()->reserve(mcRects * 2);
    }
    for (VBoxGuestWindowList::iterator it = mGuestWindows.begin();
         it != mGuestWindows.end(); ++it)
    {
        if (it->second->mMapped)
        {
            if (it->second->mcRects > 0)
            {
                for (int i = 0; i < it->second->mcRects; ++i)
                {
                    RTRECT rect;
                    rect.xLeft   =   it->second->mX
                                   + it->second->mapRects.get()[i].x;
                    rect.yBottom =   it->second->mY
                                   + it->second->mapRects.get()[i].y
                                   + it->second->mapRects.get()[i].height;
                    rect.xRight  =   it->second->mX
                                   + it->second->mapRects.get()[i].x
                                   + it->second->mapRects.get()[i].width;
                    rect.yTop    =   it->second->mY
                                   + it->second->mapRects.get()[i].y;
                    apRects.get()->push_back(rect);
                }
                cRects += it->second->mcRects;
            }
            else
            {
                RTRECT rect;
                rect.xLeft   =  it->second->mX;
                rect.yBottom =  it->second->mY
                              + it->second->mHeight;
                rect.xRight  =  it->second->mX
                              + it->second->mWidth;
                rect.yTop    =  it->second->mY;
                apRects.get()->push_back(rect);
                ++cRects;
            }
        }
    }
    mcRects = cRects;
    return apRects;
}

/**
 * Send a client event to wake up the X11 seamless event loop prior to stopping it.
 *
 * @note This function should only be called from the host event thread.
 */
bool VBoxGuestSeamlessX11::interruptEvent(void)
{
    /* Message contents set to zero. */
    XClientMessageEvent clientMessage = { ClientMessage, 0, 0, 0, 0, 0, 8 };

    if (0 != XSendEvent(mDisplay, DefaultRootWindow(mDisplay.get()), false, PropertyChangeMask,
                   reinterpret_cast<XEvent *>(&clientMessage)))
    {
        XFlush(mDisplay);
        return true;
    }
    return false;
}
