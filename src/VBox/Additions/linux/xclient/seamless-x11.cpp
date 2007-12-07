/** @file
 *
 * Seamless mode:
 * Linux guest.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
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

/*****************************************************************************
* Static functions                                                           *
*****************************************************************************/

static VBoxGuestX11Pointer<unsigned char> XXGetProperty (Display *aDpy, Window aWnd,
                                                         Atom aPropType, const char *aPropName,
                                                         unsigned long *nItems)
{
    Atom propNameAtom = XInternAtom (aDpy, aPropName,
                                     True /* only_if_exists */);
    if (propNameAtom == None)
    {
        return VBoxGuestX11Pointer<unsigned char>(0);
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
        return VBoxGuestX11Pointer<unsigned char>(0);

    return VBoxGuestX11Pointer<unsigned char>(propVal);
}

/**
  * Initialise the guest and ensure that it is capable of handling seamless mode
  *
  * @returns true if it can handle seamless, false otherwise
  */
int VBoxGuestSeamlessX11::init(VBoxGuestSeamlessObserver *pObserver)
{
    int rc = VINF_SUCCESS;
    /** Dummy values for XShapeQueryExtension */
    int error, event;

    if (0 != mObserver)  /* Assertion */
    {
        LogRelThisFunc(("ERROR: attempt to initialise service twice!\n"));
        return VERR_INTERNAL_ERROR;
    }
    if (!mDisplay.isValid())
    {
        LogRelThisFunc(("Failed to acquire a connection to the display.\n"));
        return VERR_ACCESS_DENIED;
    }
    if (!XShapeQueryExtension(mDisplay, &event, &error))
    {
        LogFlowFunc(("X11 shape extension not supported, returning.\n"));
        return VERR_NOT_SUPPORTED;
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

    isEnabled = true;
    monitorDesktopWindows();
    return rc;
}

/** Stop reporting seamless events to the host.  Free information about guest windows
    and stop requesting updates. */
void VBoxGuestSeamlessX11::stop(void)
{
    isEnabled = false;
    freeWindowTree();
}

void VBoxGuestSeamlessX11::monitorDesktopWindows(void)
{
    VBoxGuestX11Pointer<unsigned char> virtualRoots;
    unsigned long nItems;

    XSelectInput(mDisplay, DefaultRootWindow(mDisplay.get()), PropertyChangeMask);
    virtualRoots = XXGetProperty(mDisplay, DefaultRootWindow(mDisplay.get()), XA_CARDINAL,
                                 VIRTUAL_ROOTS_PROP, &nItems);
    if ((0 != virtualRoots.get()) && (0 != nItems))
    {
        rebuildWindowTree();
    }
}

void VBoxGuestSeamlessX11::rebuildWindowTree(void)
{
    VBoxGuestX11Pointer<unsigned char> virtualRoots;
    Window *desktopWindows;
    unsigned long nItems;

    freeWindowTree();
    virtualRoots = XXGetProperty(mDisplay, DefaultRootWindow(mDisplay.get()), XA_CARDINAL,
                                 VIRTUAL_ROOTS_PROP, &nItems);
    desktopWindows = reinterpret_cast<Window *>(virtualRoots.get());
    for (unsigned i = 0; i < nItems; ++i)
    {
        addDesktopWindow(desktopWindows[i]);
    }
    /* This must be done last, so that we do not treat the desktop windows just added as
       child windows. */
    addDesktopWindow(DefaultRootWindow(mDisplay.get()));
}

/**
 * Store information about a desktop window and register for structure events on it.
 * If it is mapped, go through the list of it's children and add information about
 * mapped children to the tree of visible windows, making sure that those windows are
 * not already in our list of desktop windows.
 *
 * @param   hWin     the window concerned - should be a "desktop" window
 */
void VBoxGuestSeamlessX11::addDesktopWindow(Window hWin)
{
    unsigned int cChildren = 0;
    VBoxGuestX11Pointer<Window> children;
    XWindowAttributes winAttrib;
    /** Dummies */
    Window root, parent, *pChildren;

    if (!XGetWindowAttributes(mDisplay, hWin, &winAttrib))
    {
        LogRelFunc(("Failed to get the window attributes for window %d\n", hWin));
        return;
    }
    mDesktopWindows.push_back(VBoxGuestDesktopInfo(hWin, winAttrib.x, winAttrib.y,
                              (IsUnmapped != winAttrib.map_state)));
    XSelectInput(mDisplay, hWin,
                 StructureNotifyMask | SubstructureNotifyMask | PropertyChangeMask);
    XQueryTree(mDisplay, hWin, &root, &parent, &pChildren, &cChildren);
    children = pChildren;
    if (0 == children.get())
    {
        LogRelFunc(("Failed to get the tree of active X11 windows.\n"));
    }
    else
    {
        for (unsigned int i = 0; i < cChildren; ++i)
        {
            bool found = false;

            for (unsigned int j = 0; j < mDesktopWindows.size() && !found; ++j)
            {
                if (children.get()[i] == mDesktopWindows[j].mWin)
                {
                    found = true;
                }
            }
            if (!found)
            {
                addWindowToList(children.get()[i], hWin);
            }
        }
    }
}

void VBoxGuestSeamlessX11::addWindowToList(const Window hWin, const Window hParent)
{
    XWindowAttributes winAttrib;
    VBoxGuestX11Pointer<XRectangle> rects;
    int cRects, iOrdering;
    unsigned iParent;
    bool isVisible = false, found = false;

    if (!XGetWindowAttributes(mDisplay, hWin, &winAttrib))
    {
        LogRelFunc(("Failed to get the window attributes for window %d\n", hWin));
        return;
    }
    XShapeSelectInput(mDisplay, hWin, ShapeNotify);
    rects = XShapeGetRectangles(mDisplay, hWin, ShapeClip, &cRects, &iOrdering);
    if (0 == rects.get())
    {
        cRects = 0;
    }
    if (IsViewable == winAttrib.map_state)
    {
        isVisible = true;
    }
    for (iParent = 0; iParent < mDesktopWindows.size() && !found; ++iParent)
    {
        if (hParent == mDesktopWindows[iParent].mWin)
        {
            found = true;
        }
    }
    if (found)
    {
        mGuestWindows.addWindow(hWin, isVisible, winAttrib.x, winAttrib.y,
                                winAttrib.width, winAttrib.height, cRects, rects, hParent);
    }
}

/**
 * Free all information in the tree of visible windows
 */
void VBoxGuestSeamlessX11::freeWindowTree(void)
{
    for (unsigned int i = 0; i != mDesktopWindows.size(); ++i)
    {
        XSelectInput(mDisplay, mDesktopWindows[i].mWin, 0);
    }
    mDesktopWindows.clear();
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
        doPropertyEvent(&event.xproperty);
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
 * Handle a configuration event in the seamless event thread by setting the new position and
 * updating the host's rectangle information.
 *
 * @param event the X11 event structure
 */
void VBoxGuestSeamlessX11::doConfigureEvent(const XConfigureEvent *event)
{
    bool found = false;
    unsigned i = 0;

    for (i = 0; i < mDesktopWindows.size() && !found; ++i)
    {
        if (event->window == mDesktopWindows[i].mWin)
        {
            found = true;
        }
    }
    if (found)
    {
        mDesktopWindows[i].mx = event->x;
        mDesktopWindows[i].my = event->y;
    }
    else
    {
        VBoxGuestWindowList::iterator iter;

        iter = mGuestWindows.find(event->window);
        if (iter != mGuestWindows.end())
        {
            iter->second->mx = event->x;
            iter->second->my = event->y;
        }
    }
}

/**
 * Handle a map event in the seamless event thread by adding the guest window to the list of
 * visible windows and updating the host's rectangle information.
 *
 * @param event the X11 event structure
 */
void VBoxGuestSeamlessX11::doMapEvent(const XMapEvent *event)
{
    VBoxGuestWindowList::iterator iter;
    bool found = false;

    /* Is one of our desktop windows? */
    for (unsigned i = 0; i < mDesktopWindows.size() && !found; ++i)
    {
        if (event->window == mDesktopWindows[i].mWin)
        {
            mDesktopWindows[i].mMapped = true;
            found = true;
        }
    }
    if (!found)
    {
        /* Make sure that the window is not already present in the tree */
        iter = mGuestWindows.find(event->window);
        if (iter != mGuestWindows.end())
        {
            LogRelFunc(("Warning: MapNotify event received for a window listed as mapped\n"));
            mGuestWindows.removeWindow(event->window);
        }
        addWindowToList(event->window, event->event);
    }
}

/**
 * If the list of virtual root windows changes, completely rescan visible windows.
 *
 * @param event the X11 event structure
 */
void VBoxGuestSeamlessX11::doPropertyEvent(const XPropertyEvent *event)
{
    if (XInternAtom(mDisplay, VIRTUAL_ROOTS_PROP, true) == event->atom)
    {
        rebuildWindowTree();
    }
}

/**
 * Handle a window shape change event in the seamless event thread by changing the set of
 * visible rectangles for the window in the list of visible guest windows and updating the
 * host's rectangle information.
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
 * Handle an unmap event in the seamless event thread by removing the guest window from the
 * list of visible windows and updating the host's rectangle information.
 *
 * @param event the X11 event structure
 */
void VBoxGuestSeamlessX11::doUnmapEvent(const XUnmapEvent *event)
{
    VBoxGuestWindowList::iterator iter;
    bool found = false;

    /* Is this is one of our desktop windows? */
    for (unsigned i = 0; i < mDesktopWindows.size() && !found; ++i)
    {
        if (event->window == mDesktopWindows[i].mWin)
        {
            mDesktopWindows[i].mMapped = false;
            found = true;
        }
    }
    if (!found)
    {
        /* Make sure that the window is not already present in the tree */
        iter = mGuestWindows.find(event->window);
        if (iter != mGuestWindows.end())
        {
            mGuestWindows.removeWindow(event->window);
        }
        else
        {
            LogRelFunc(("Warning: UnmapNotify event received for a window not listed as mapped\n"));
        }
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
        Window hParent = it->second->mParent;
        unsigned iParent;
        bool found = false;

        for (iParent = 0; iParent < mDesktopWindows.size() && !found; ++iParent)
        {
            if (mDesktopWindows[iParent].mWin == hParent)
            {
                found = true;
            }
        }
        if (found && mDesktopWindows[iParent - 1].mMapped && it->second->mMapped)
        {
            for (int i = 0; i < it->second->mcRects; ++i)
            {
                RTRECT rect;
                rect.xLeft   =   it->second->mx
                              + it->second->mapRects.get()[i].x
                              + mDesktopWindows[iParent].mx;
                rect.yBottom =   it->second->my
                              + it->second->mapRects.get()[i].y
                              + it->second->mapRects.get()[i].height
                              + mDesktopWindows[iParent].my;
                rect.xRight  =   it->second->mx
                              + it->second->mapRects.get()[i].x
                              + it->second->mapRects.get()[i].width
                              + mDesktopWindows[iParent].mx;
                rect.yTop    =   it->second->my
                              + it->second->mapRects.get()[i].y
                              + mDesktopWindows[iParent].my;
                apRects.get()->push_back(rect);
            }
            cRects += it->second->mcRects;
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
    XClientMessageEvent clientMessage = { ClientMessage };  /* Other members set to zero. */
    XSendEvent(mDisplay, DefaultRootWindow(mDisplay.get()), false, StructureNotifyMask,
               reinterpret_cast<XEvent *>(&clientMessage));
    return true;
}
