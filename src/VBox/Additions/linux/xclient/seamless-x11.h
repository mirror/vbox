/** @file
 *
 * Seamless mode:
 * Linux guest.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __Additions_linux_seamless_x11_h
# define __Additions_linux_seamless_x11_h

#include "seamless-guest.h"

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include <map>
#include <vector>

#define VIRTUAL_ROOTS_PROP "_NET_VIRTUAL_ROOTS"

/**
 * Wrapper class around the VBoxGuestX11Pointer to provide reference semantics.
 * See auto_ptr in the C++ <memory> header.
 */
template <class T>
struct VBoxGuestX11PointerRef
{
    T *mValue;

    VBoxGuestX11PointerRef(T* pValue) { mValue = pValue; }
};

/** An auto pointer for pointers which have to be XFree'd. */
template <class T>
class VBoxGuestX11Pointer
{
private:
    T *mValue;
public:
    VBoxGuestX11Pointer(T *pValue = 0) { mValue = pValue; }
    ~VBoxGuestX11Pointer() { if (0 != mValue) XFree(mValue); }

    /** release method to get the pointer's value and "reset" the pointer. */
    T *release(void) { T *pTmp = mValue; mValue = 0; return pTmp; }

    /** reset the pointer value to zero or to another pointer. */
    void reset(T* pValue = 0) { if (pValue != mValue) { XFree(mValue); mValue = pValue; } }

    /** Copy constructor */
    VBoxGuestX11Pointer(VBoxGuestX11Pointer &orig) { mValue = orig.release(); }

    /** Copy from equivalent class */
    template <class T1>
    VBoxGuestX11Pointer(VBoxGuestX11Pointer<T1> &orig) { mValue = orig.release(); }

    /** Assignment operator. */
    VBoxGuestX11Pointer& operator=(VBoxGuestX11Pointer &orig)
    {
        reset(orig.release());
        return *this;
    }

    /** Assignment from equivalent class. */
    template <class T1>
    VBoxGuestX11Pointer& operator=(VBoxGuestX11Pointer<T1> &orig)
    {
        reset(orig.release);
        return *this;
    }

    /** Assignment from a pointer. */
    VBoxGuestX11Pointer& operator=(T *pValue)
    {
        if (0 != mValue)
        {
            XFree(mValue);
        }
        mValue = pValue;
        return *this;
    }

    /** Dereference with * operator. */
    T &operator*() { return *mValue; }

    /** Dereference with -> operator. */
    T *operator->() { return mValue; }

    /** Accessing the value inside. */
    T *get(void) { return mValue; }

    /** Convert a reference structure into an X11 pointer. */
    VBoxGuestX11Pointer(VBoxGuestX11PointerRef<T> ref) { mValue = ref.mValue; }

    /** Assign from a reference structure into an X11 pointer. */
    VBoxGuestX11Pointer& operator=(VBoxGuestX11PointerRef<T> ref)
    {
        if (ref.mValue != mValue)
        {
            XFree(mValue);
            mValue = ref.mValue;
        }
        return *this;
    }

    /** Typecast an X11 pointer to a reference structure. */
    template <class T1>
    operator VBoxGuestX11PointerRef<T1>() { return VBoxGuestX11PointerRef<T1>(release()); }

    /** Typecast an X11 pointer to an X11 pointer around a different type. */
    template <class T1>
    operator VBoxGuestX11Pointer<T1>() { return VBoxGuestX11Pointer<T1>(release()); }
};

/**
 * Wrapper class around an X11 display pointer which takes care of closing the display
 * when it is destroyed at the latest.
 */
class VBoxGuestX11Display
{
private:
    Display *mDisplay;
public:
    VBoxGuestX11Display(char *name = 0)
    {
        mDisplay = XOpenDisplay(name);
    }
    operator Display *() { return mDisplay; }
    Display *get(void) { return mDisplay; }
    bool isValid(void) { return (0 != mDisplay); }
    int close(void) { return XCloseDisplay(mDisplay); }
    ~VBoxGuestX11Display() { close(); }
};

/** Structure containing information about a guest window's position and visible area.
    Used inside of VBoxGuestWindowList. */
struct VBoxGuestWinInfo {
public:
    /** Is this window currently mapped? */
    bool mMapped;
    /** Co-ordinates in the guest screen. */
    int mx, my;
    /** Window dimensions. */
    int mwidth, mheight;
    /** Number of rectangles used to represent the visible area. */
    int mcRects;
    /** Rectangles representing the visible area.  These must be allocated by XMalloc
        and will be freed automatically if non-null when the class is destroyed. */
    VBoxGuestX11Pointer<XRectangle> mapRects;
    /** The index of the virtual root that this window is a child of. */
    int mParent;
    /** Constructor. */
    VBoxGuestWinInfo(bool mapped, int x, int y, int w, int h, int cRects,
                     VBoxGuestX11Pointer<XRectangle> rects, int parent)
            : mapRects(rects)
    {
        mMapped = mapped; mx = x; my = y; mwidth = w; mheight = h; mcRects = cRects;
        mParent = parent;
    }

private:
    // We don't want a copy constructor or assignment operator
    VBoxGuestWinInfo(const VBoxGuestWinInfo&);
    VBoxGuestWinInfo& operator=(const VBoxGuestWinInfo&);
};

/**
 * This class is just a wrapper around a map of structures containing information about
 * the windows on the guest system.  It has a function for adding a structure (see addWindow),
 * for removing it by window handle (see removeWindow) and an iterator for
 * going through the list.
 */
class VBoxGuestWindowList
{
private:
    // We don't want a copy constructor or an assignment operator
    VBoxGuestWindowList(const VBoxGuestWindowList&);
    VBoxGuestWindowList& operator=(const VBoxGuestWindowList&);

    // Private class members
    std::map<Window, VBoxGuestWinInfo *> mWindows;

public:
    // Just proxy iterators to map::iterator
    typedef std::map<Window, VBoxGuestWinInfo *>::const_iterator const_iterator;
    typedef std::map<Window, VBoxGuestWinInfo *>::iterator iterator;

    // Constructor
    VBoxGuestWindowList(void) {}
    // Destructor
    ~VBoxGuestWindowList()
    {
        /* We use post-increment in the operation to prevent the iterator from being invalidated. */
        for (iterator it = begin(); it != end(); removeWindow(it++));
    }

    // Standard operations
    const_iterator begin() const { return mWindows.begin(); }
    iterator begin() { return mWindows.begin(); }
    const_iterator end() const { return mWindows.end(); }
    iterator end() { return mWindows.end(); }
    const_iterator find(Window win) const { return mWindows.find(win); }
    iterator find(Window win) { return mWindows.find(win); }

    void addWindow(Window hWin, bool isMapped, int x, int y, int w, int h, int cRects,
                   VBoxGuestX11Pointer<XRectangle> rects, int parent)
    {
        VBoxGuestWinInfo *pInfo = new VBoxGuestWinInfo(isMapped, x, y, w, h, cRects,
                                                       rects, parent);
        mWindows.insert(std::pair<Window, VBoxGuestWinInfo *>(hWin, pInfo));
    }

    void removeWindow(iterator it)
    {
        delete it->second;
        mWindows.erase(it);
    }

    void removeWindow(Window hWin)
    {
        removeWindow(find(hWin));
    }
};

/** Structure containing information about a windows handle and position, for keeping
    track of desktop windows.  Used internally by VBoxGuestSeamlessX11. */
struct VBoxGuestDesktopInfo
{
    /** The Window handle for this window. */
    Window mWin;
    /** Co-ordinates relative to the root window (I hope!). */
    int mx, my;
    /** Is this window mapped? */
    bool mMapped;

    /** Constructor */
    VBoxGuestDesktopInfo(Window hWin, int x, int y, bool mapped)
    {
        mWin = hWin;
        mx = x;
        my = y;
        mMapped = mapped;
    }
};

class VBoxGuestSeamlessX11;

class VBoxGuestSeamlessX11 : public VBoxGuestSeamlessGuest
{
private:
    // We don't want a copy constructor or assignment operator
    VBoxGuestSeamlessX11(const VBoxGuestSeamlessX11&);
    VBoxGuestSeamlessX11& operator=(const VBoxGuestSeamlessX11&);

    // Private member variables
    /** Pointer to the observer class. */
    VBoxGuestSeamlessObserver *mObserver;
    /** Our connection to the X11 display we are running on. */
    VBoxGuestX11Display mDisplay;
    /** Vector to keep track of which windows are to be treated as desktop windows. */
    std::vector<VBoxGuestDesktopInfo> mDesktopWindows;
    /** Class to keep track of visible guest windows. */
    VBoxGuestWindowList mGuestWindows;
    /** Keeps track of the total number of rectangles needed for the visible area of all
        guest windows on the last call to getRects.  Used for pre-allocating space in
        the vector of rectangles passed to the host. */
    int mcRects;
    /** Is seamles mode currently enabled?  */
    bool isEnabled;

    // Private methods

    // Methods to handle X11 events
    void doConfigureEvent(const XConfigureEvent *event);
    void doMapEvent(const XMapEvent *event);
    void doPropertyEvent(const XPropertyEvent *event);
    void doUnmapEvent(const XUnmapEvent *event);
    void doShapeEvent(const XShapeEvent *event);

    // Methods to manage guest window information
    /**
     * Store information about a desktop window and register for structure events on it.
     * If it is mapped, go through the list of it's children and add information about
     * mapped children to the tree of visible windows, making sure that those windows are
     * not already in our list of desktop windows.
     *
     * @param   hWin     the window concerned - should be a "desktop" window
     */
    void addDesktopWindow(Window hWin);
    bool addNonDesktopWindow(Window hWin);
    void addWindowToList(Window hWin, Window hParent);
    void monitorDesktopWindows(void);
    void rebuildWindowTree(void);
    void freeWindowTree(void);
    void updateHostSeamlessInfo(void);

public:
    /**
     * Initialise the guest and ensure that it is capable of handling seamless mode
     * @param   pObserver Observer class to connect host and guest interfaces
     *
     * @returns iprt status code
     */
    int init(VBoxGuestSeamlessObserver *pObserver);

    /**
     * Shutdown seamless event monitoring.
     */
    void uninit(void)
    {
        if (0 != mObserver)
        {
            stop();
        }
        mObserver = 0;
    }

    /**
     * Initialise seamless event reporting in the guest.
     *
     * @returns IPRT status code
     */
    int start(void);
    /** Stop reporting seamless events. */
    void stop(void);
    /** Get the current list of visible rectangles. */
    std::auto_ptr<std::vector<RTRECT> > getRects(void);

    /** Process next event in the X11 event queue - called by the event thread. */
    void nextEvent(void);
    /** Send ourselves an X11 client event to wake up the event thread - called by
        the event thread. */
    bool interruptEvent(void);

    VBoxGuestSeamlessX11(void)
    {
        mObserver = 0; mcRects = 0; isEnabled = false;
    }

    ~VBoxGuestSeamlessX11() { uninit(); }
};

typedef VBoxGuestSeamlessX11 VBoxGuestSeamlessGuestImpl;

#endif /* __Additions_linux_seamless_x11_h not defined */
