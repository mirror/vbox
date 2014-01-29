/** @file
 * X11 guest client - Drag and Drop.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
//#include <X11/extensions/XTest.h>

#include <iprt/thread.h>
#include <iprt/asm.h>
#include <iprt/time.h>

#include <iprt/cpp/mtlist.h>
#include <iprt/cpp/ministring.h>

#include <limits.h>

#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>

#include "VBox/HostServices/DragAndDropSvc.h"

#include "VBoxClient.h"

/* For X11 guest xDnD is used. See http://www.acc.umu.se/~vatten/XDND.html for
 * a walk trough.
 *
 * H->G:
 * For X11 this means mainly forwarding all the events from HGCM to the
 * appropriate X11 events. There exists a proxy window, which is invisible and
 * used for all the X11 communication. On a HGCM Enter event, we set our proxy
 * window as XdndSelection owner with the given mime-types. On every HGCM move
 * event, we move the X11 mouse cursor to the new position and query for the
 * window below that position. Depending on if it is XdndAware, a new window or
 * a known window, we send the appropriate X11 messages to it. On HGCM drop, we
 * send a XdndDrop message to the current window and wait for a X11
 * SelectionMessage from the target window. Because we didn't have the data in
 * the requested mime-type, yet, we save that message and ask the host for the
 * data. When the data is successfully received from the host, we put the data
 * as a property to the window and send a X11 SelectionNotify event to the
 * target window.
 *
 * G->H:
 * This is a lot more trickery than H->G. When a pending event from HGCM
 * arrives, we ask if there currently is an owner of the XdndSelection
 * property. If so, our proxy window is shown (1x1, but without backing store)
 * and some mouse event is triggered. This should be followed by an XdndEnter
 * event send to the proxy window. From this event we can fetch the necessary
 * info of the mime-types and allowed actions and send this back to the host.
 * On a drop request from the host, we query for the selection and should get
 * the data in the specified mime-type. This data is send back to the host.
 * After that we send a XdndLeave event to the source window.
 * Todo:
 * - this isn't finished, yet. Currently the mouse isn't correctly released
 *   in the guest (both, when the drop was successfully or canceled).
 * - cancel (e.g. with the ESC key) doesn't work
 *
 * Todo:
 * - XdndProxy window support
 * - INCR support
 * - make this much more robust for crashes of the other party
 * - really check for the Xdnd version and the supported features
 */

#define VBOX_XDND_VERSION    (4)
#define VBOX_MAX_XPROPERTIES (LONG_MAX-1)

/* Shared struct used for adding new X11 events and HGCM messages to a single
 * event queue. */
struct DnDEvent
{
    enum DnDEventType
    {
        HGCM_Type = 1,
        X11_Type
    };
    DnDEventType type;
    union
    {
        VBGLR3DNDHGCMEVENT hgcm;
        XEvent x11;
    };
};

enum XA_Type
{
    /* States */
    XA_WM_STATE = 0,
    /* Properties */
    XA_TARGETS,
    XA_MULTIPLE,
    XA_INCR,
    /* Mime Types */
    XA_image_bmp,
    XA_image_jpg,
    XA_image_tiff,
    XA_image_png,
    XA_text_uri_list,
    XA_text_uri,
    XA_text_plain,
    XA_TEXT,
    /* xDnD */
    XA_XdndSelection,
    XA_XdndAware,
    XA_XdndEnter,
    XA_XdndLeave,
    XA_XdndTypeList,
    XA_XdndActionList,
    XA_XdndPosition,
    XA_XdndActionCopy,
    XA_XdndActionMove,
    XA_XdndActionLink,
    XA_XdndStatus,
    XA_XdndDrop,
    XA_XdndFinished,
    /* Our own stop marker */
    XA_dndstop,
    /* End marker */
    XA_End
};

class DragAndDropService;

/*******************************************************************************
 *
 * xHelpers Declaration
 *
 ******************************************************************************/

class xHelpers
{
public:

    static xHelpers *getInstance(Display *pDisplay = 0)
    {
        if (!m_pInstance)
        {
            AssertPtrReturn(pDisplay, NULL);
            m_pInstance = new xHelpers(pDisplay);
        }

        return m_pInstance;
    }

    inline Display *display()    const { return m_pDisplay; }
    inline Atom xAtom(XA_Type e) const { return m_xAtoms[e]; }

    inline Atom stringToxAtom(const char *pcszString) const
    {
        return XInternAtom(m_pDisplay, pcszString, False);
    }
    inline RTCString xAtomToString(Atom atom) const
    {
        if (atom == None) return "None";

        char* pcsAtom = XGetAtomName(m_pDisplay, atom);
        RTCString strAtom(pcsAtom);
        XFree(pcsAtom);

        return strAtom;
    }

    inline RTCString xAtomListToString(const RTCList<Atom> &formatList)
    {
        RTCString format;
        for (size_t i = 0; i < formatList.size(); ++i)
            format += xAtomToString(formatList.at(i)) + "\r\n";
        return format;
    }

    RTCString xErrorToString(int xrc) const;
    Window applicationWindowBelowCursor(Window parentWin) const;

private:
    xHelpers(Display *pDisplay)
      : m_pDisplay(pDisplay)
    {
        /* Not all x11 atoms we use are defined in the headers. Create the
         * additional one we need here. */
        for (int i = 0; i < XA_End; ++i)
            m_xAtoms[i] = XInternAtom(m_pDisplay, m_xAtomNames[i], False);
    };

    /* Private member vars */
    static xHelpers   *m_pInstance;
    Display           *m_pDisplay;
    Atom               m_xAtoms[XA_End];
    static const char *m_xAtomNames[XA_End];
};

/* Some xHelpers convenience defines. */
#define gX11 xHelpers::getInstance()
#define xAtom(xa) gX11->xAtom((xa))
#define xAtomToString(xa) gX11->xAtomToString((xa))

/*******************************************************************************
 *
 * xHelpers Implementation
 *
 ******************************************************************************/

xHelpers *xHelpers::m_pInstance = 0;
/* Has to be in sync with the XA_Type enum. */
const char *xHelpers::m_xAtomNames[] =
{
    /* States */
    "WM_STATE",
    /* Properties */
    "TARGETS",
    "MULTIPLE",
    "INCR",
    /* Mime Types */
    "image/bmp",
    "image/jpg",
    "image/tiff",
    "image/png",
    "text/uri-list",
    "text/uri",
    "text/plain",
    "TEXT",
    /* xDnD */
    "XdndSelection",
    "XdndAware",
    "XdndEnter",
    "XdndLeave",
    "XdndTypeList",
    "XdndActionList",
    "XdndPosition",
    "XdndActionCopy",
    "XdndActionMove",
    "XdndActionLink",
    "XdndStatus",
    "XdndDrop",
    "XdndFinished",
    /* Our own stop marker */
    "dndstop"
};

RTCString xHelpers::xErrorToString(int xrc) const
{
    switch (xrc)
    {
        case Success:           return RTCStringFmt("%d (Success)", xrc); break;
        case BadRequest:        return RTCStringFmt("%d (BadRequest)", xrc); break;
        case BadValue:          return RTCStringFmt("%d (BadValue)", xrc); break;
        case BadWindow:         return RTCStringFmt("%d (BadWindow)", xrc); break;
        case BadPixmap:         return RTCStringFmt("%d (BadPixmap)", xrc); break;
        case BadAtom:           return RTCStringFmt("%d (BadAtom)", xrc); break;
        case BadCursor:         return RTCStringFmt("%d (BadCursor)", xrc); break;
        case BadFont:           return RTCStringFmt("%d (BadFont)", xrc); break;
        case BadMatch:          return RTCStringFmt("%d (BadMatch)", xrc); break;
        case BadDrawable:       return RTCStringFmt("%d (BadDrawable)", xrc); break;
        case BadAccess:         return RTCStringFmt("%d (BadAccess)", xrc); break;
        case BadAlloc:          return RTCStringFmt("%d (BadAlloc)", xrc); break;
        case BadColor:          return RTCStringFmt("%d (BadColor)", xrc); break;
        case BadGC:             return RTCStringFmt("%d (BadGC)", xrc); break;
        case BadIDChoice:       return RTCStringFmt("%d (BadIDChoice)", xrc); break;
        case BadName:           return RTCStringFmt("%d (BadName)", xrc); break;
        case BadLength:         return RTCStringFmt("%d (BadLength)", xrc); break;
        case BadImplementation: return RTCStringFmt("%d (BadImplementation)", xrc); break;
    }
    return RTCStringFmt("%d (unknown)", xrc);
}

/* Todo: make this iterative */
Window xHelpers::applicationWindowBelowCursor(Window wndParent) const
{
    /* No parent, nothing to do. */
    if(wndParent == 0)
        return 0;

    Window wndApp = 0;
    int cProps = -1;
    /* Fetch all x11 window properties of the parent window. */
    Atom *pProps = XListProperties(m_pDisplay, wndParent, &cProps);
    if (cProps > 0)
    {
        /* We check the window for the WM_STATE property. */
        for (int i = 0; i < cProps; ++i)
            if (pProps[i] == xAtom(XA_WM_STATE))
            {
                /* Found it. */
                wndApp = wndParent;
                break;
            }
        /* Cleanup */
        XFree(pProps);
    }

    if (!wndApp)
    {
        Window wndChild, wndTemp;
        int tmp;
        unsigned int utmp;
        /* Query the next child window of the parent window at the current
         * mouse position. */
        XQueryPointer(m_pDisplay, wndParent, &wndTemp, &wndChild, &tmp, &tmp, &tmp, &tmp, &utmp);
        /* Recursive call our self to dive into the child tree. */
        wndApp = applicationWindowBelowCursor(wndChild);
    }

    return wndApp;
}

/*******************************************************************************
 *
 * DragInstance Declaration
 *
 ******************************************************************************/

/* For now only one DragInstance will exits when the app is running. In the
 * future the support for having more than one D&D operation supported at the
 * time will be necessary. */
class DragInstance
{
public:
    enum State
    {
        Uninitialized,
        Initialized,
        Dragging,
        Dropped
    };

    enum Mode
    {
        Unknown,
        HG,
        GH
    };

    DragInstance(Display *pDisplay, DragAndDropService *pParent);
    int  init(uint32_t u32ScreenId);
    void uninit();
    void reset();

    /* H->G */
    int  hgEnter(const RTCList<RTCString> &formats, uint32_t actions);
    int  hgMove(uint32_t u32xPos, uint32_t u32yPos, uint32_t action);
    int  hgX11ClientMessage(const XEvent& e);
    int  hgDrop();
    int  hgX11SelectionRequest(const XEvent& e);
    int  hgDataReceived(void *pvData, uint32_t cData);

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
    /* G->H */
    int  ghIsDnDPending();
    int  ghDropped(const RTCString &strFormat, uint32_t action);
#endif

    /* X11 helpers */
    int  moveCursor(uint32_t u32xPos, uint32_t u32yPos);
    void sendButtonEvent(Window w, int rx, int ry, int button, bool fPress) const;
    void showProxyWin(int &rx, int &ry) const;
    void hideProxyWin() const;
    void registerForEvents(Window w) const;

    void setActionsWindowProperty(Window win, const RTCList<Atom> &actionList) const;
    void clearActionsWindowProperty(Window win) const;
    void setFormatsWindowProperty(Window win, Atom property) const;
    void clearFormatsWindowProperty(Window win) const;

    RTCList<Atom>        toAtomList(const RTCList<RTCString> &formatList) const;
    RTCList<Atom>        toAtomList(void *pvData, uint32_t cData) const;
    static Atom          toX11Action(uint32_t uAction);
    static RTCList<Atom> toX11Actions(uint32_t uActions);
    static uint32_t      toHGCMAction(Atom atom);
    static uint32_t      toHGCMActions(const RTCList<Atom> &actionsList);

    /* Member vars */
    uint32_t            m_uClientID;
    DragAndDropService *m_pParent;
    Display            *m_pDisplay;
    int                 m_screenId;
    Screen             *m_pScreen;
    Window              m_wndRoot;
    Window              m_wndProxy;
    Window              m_wndCur;
    long                m_curVer;
    RTCList<Atom>       m_formats;
    RTCList<Atom>       m_actions;

    XEvent              m_selEvent;

    Mode                m_mode;
    State               m_state;

    static const RTCList<RTCString> m_sstrStringMimeTypes;
};

/*******************************************************************************
 *
 * DragAndDropService Declaration
 *
 ******************************************************************************/

class DragAndDropService : public VBoxClient::Service
{
public:
    DragAndDropService(void)
      : m_pDisplay(0)
      , m_hHGCMThread(NIL_RTTHREAD)
      , m_hX11Thread(NIL_RTTHREAD)
      , m_hEventSem(NIL_RTSEMEVENT)
      , m_pCurDnD(0)
      , m_fSrvStopping(false)
    {}

    virtual const char *getPidFilePath() { return ".vboxclient-draganddrop.pid"; }

    /** @todo Move this part in VbglR3 and just provide a callback for the platform-specific
              notification stuff, since this is very similar to the VBoxTray code. */
    virtual int run(bool fDaemonised = false);

    virtual void cleanup(void)
    {
        /* Cleanup */
        x11DragAndDropTerm();
    };

private:
    int x11DragAndDropInit(void);
    int x11DragAndDropTerm(void);
    static int hgcmEventThread(RTTHREAD hThread, void *pvUser);
    static int x11EventThread(RTTHREAD hThread, void *pvUser);

    bool waitForXMsg(XEvent &ecm, int type, uint32_t uiMaxMS = 100);
    void clearEventQueue();
    /* Usually XCheckMaskEvent could be used for queering selected x11 events.
     * Unfortunately this doesn't work exactly with the events we need. So we
     * use this predicate method below and XCheckIfEvent. */
    static bool isDnDRespondEvent(Display * /* pDisplay */, XEvent *pEvent, char *pUser)
    {
        if (!pEvent)
            return false;
        if (   pEvent->type == SelectionClear
            || pEvent->type == ClientMessage
            || pEvent->type == MotionNotify
            || pEvent->type == SelectionRequest)
//            || (   pEvent->type == ClientMessage
//                && reinterpret_cast<XClientMessageEvent*>(pEvent)->window == reinterpret_cast<Window>(pUser))
//            || (   pEvent->type == SelectionRequest
//                && reinterpret_cast<XSelectionRequestEvent*>(pEvent)->requestor == reinterpret_cast<Window>(pUser)))
            return true;
        return false;
    }

    /* Private member vars */
    Display             *m_pDisplay;

    RTCMTList<DnDEvent>  m_eventQueue;
    RTTHREAD             m_hHGCMThread;
    RTTHREAD             m_hX11Thread;
    RTSEMEVENT           m_hEventSem;
    DragInstance        *m_pCurDnD;
    bool                 m_fSrvStopping;

    friend class DragInstance;
};

/*******************************************************************************
 *
 * DragInstanc Implementation
 *
 ******************************************************************************/

DragInstance::DragInstance(Display *pDisplay, DragAndDropService *pParent)
  : m_uClientID(0)
  , m_pParent(pParent)
  , m_pDisplay(pDisplay)
  , m_pScreen(0)
  , m_wndRoot(0)
  , m_wndProxy(0)
  , m_wndCur(0)
  , m_curVer(-1)
  , m_mode(Unknown)
  , m_state(Uninitialized)
{
    uninit();
}

void DragInstance::uninit(void)
{
    reset();
    if (m_wndProxy != 0)
        XDestroyWindow(m_pDisplay, m_wndProxy);

    if (m_uClientID)
    {
        VbglR3DnDDisconnect(m_uClientID);
        m_uClientID = 0;
    }

    m_state    = Uninitialized;
    m_screenId = -1;
    m_pScreen  = 0;
    m_wndRoot  = 0;
    m_wndProxy = 0;
}

void DragInstance::reset(void)
{
    /* Hide the proxy win. */
    hideProxyWin();
    /* If we are currently the Xdnd selection owner, clear that. */
    Window w = XGetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection));
    if (w == m_wndProxy)
        XSetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection), None, CurrentTime);
    /* Clear any other DnD specific data on the proxy win. */
    clearFormatsWindowProperty(m_wndProxy);
    clearActionsWindowProperty(m_wndProxy);
    /* Reset the internal state. */
    m_formats.clear();
    m_wndCur = 0;
    m_curVer = -1;
    m_state  = Initialized;
}

const RTCList<RTCString> DragInstance::m_sstrStringMimeTypes = RTCList<RTCString>()
    /* Uri's */
    << "text/uri-list"
    /* Text */
    << "text/plain;charset=utf-8"
    << "UTF8_STRING"
    << "text/plain"
    << "COMPOUND_TEXT"
    << "TEXT"
    << "STRING"
    /* OpenOffice formates */
    << "application/x-openoffice-embed-source-xml;windows_formatname=\"Star Embed Source (XML)\""
    << "application/x-openoffice-drawing;windows_formatname=\"Drawing Format\"";

int DragInstance::init(uint32_t u32ScreenId)
{
    int rc;

    do
    {
        uninit();

        rc = VbglR3DnDConnect(&m_uClientID);
        if (RT_FAILURE(rc))
            break;

        /*
         * Enough screens configured in the x11 server?
         */
        if ((int)u32ScreenId > ScreenCount(m_pDisplay))
        {
            rc = VERR_INVALID_PARAMETER;
            break;
        }
        /* Get the screen number from the x11 server. */
//        pDrag->screen = ScreenOfDisplay(m_pDisplay, u32ScreenId);
//        if (!pDrag->screen)
//        {
//            rc = VERR_GENERAL_FAILURE;
//            break;
//        }
        m_screenId = u32ScreenId;
        /* Now query the corresponding root window of this screen. */
        m_wndRoot = RootWindow(m_pDisplay, m_screenId);
        if (!m_wndRoot)
        {
            rc = VERR_GENERAL_FAILURE;
            break;
        }

        /*
         * Create an invisible window which will act as proxy for the DnD
         * operation. This window will be used for both the GH and HG
         * direction.
         */
        XSetWindowAttributes attr;
        RT_ZERO(attr);
        attr.do_not_propagate_mask = 0;
        attr.override_redirect     = True;
#if 0
        attr.background_pixel      = WhitePixel(m_pDisplay, m_screenId);
#endif
        m_wndProxy = XCreateWindow(m_pDisplay, m_wndRoot, 0, 0, 1, 1, 0,
                                   CopyFromParent, InputOnly, CopyFromParent,
                                   CWOverrideRedirect | CWDontPropagate,
                                   &attr);
#ifdef DEBUG_andy
        m_wndProxy = XCreateSimpleWindow(m_pDisplay, m_wndRoot, 0, 0, 50, 50, 0,
                                         WhitePixel(m_pDisplay, m_screenId),
                                         WhitePixel(m_pDisplay, m_screenId));
#endif
        if (!m_wndProxy)
        {
            rc = VERR_GENERAL_FAILURE;
            break;
        }

        /* Make the new window Xdnd aware. */
        Atom ver = VBOX_XDND_VERSION;
        XChangeProperty(m_pDisplay, m_wndProxy, xAtom(XA_XdndAware), XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&ver), 1);
    } while (0);

    if (RT_SUCCESS(rc))
        m_state = Initialized;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
 * Host -> Guest
 */

int DragInstance::hgEnter(const RTCList<RTCString> &formats, uint32_t uActions)
{
    reset();

#ifdef DEBUG
    LogFlowThisFunc(("uActions=0x%x, lstFormats=%zu: ", uActions, formats.size()));
    for (size_t i = 0; i < formats.size(); ++i)
        LogFlow(("'%s' ", formats.at(i).c_str()));
    LogFlow(("\n"));
#endif

    m_formats = toAtomList(formats);

    /* If we have more than 3 formats we have to use the type list extension. */
    if (m_formats.size() > 3)
        setFormatsWindowProperty(m_wndProxy, xAtom(XA_XdndTypeList));

    /* Announce the possible actions */
    setActionsWindowProperty(m_wndProxy, toX11Actions(uActions));

    /* Set the DnD selection owner to our window. */
    XSetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection), m_wndProxy, CurrentTime);

    m_mode  = HG;
    m_state = Dragging;

    return VINF_SUCCESS;
}

int DragInstance::hgMove(uint32_t u32xPos, uint32_t u32yPos, uint32_t uAction)
{
    LogFlowThisFunc(("u32xPos=%RU32, u32yPos=%RU32, uAction=%RU32\n",
                     u32xPos, u32yPos, uAction));

    if (   m_mode  != HG
        || m_state != Dragging)
        return VERR_INVALID_STATE;

    int rc  = VINF_SUCCESS;
    int xrc = Success;

    /* Move the mouse cursor within the guest. */
    moveCursor(u32xPos, u32yPos);

    long newVer = -1; /* This means the current window is _not_ XdndAware. */

    /* Search for the application window below the cursor. */
    Window wndCursor = gX11->applicationWindowBelowCursor(m_wndRoot);
    if (wndCursor != None)
    {
        /* Temp stuff for the XGetWindowProperty call. */
        Atom atmp;
        int fmt;
        unsigned long cItems, cbRemaining;
        unsigned char *pcData = NULL;

        /* Query the XdndAware property from the window. We are interested in
         * the version and if it is XdndAware at all. */
        xrc = XGetWindowProperty(m_pDisplay, wndCursor, xAtom(XA_XdndAware),
                                 0, 2, False, AnyPropertyType,
                                 &atmp, &fmt, &cItems, &cbRemaining, &pcData);
        if (RT_UNLIKELY(xrc != Success))
            LogFlowThisFunc(("Error in getting the window property: %s\n", gX11->xErrorToString(xrc).c_str()));
        else
        {
            if (RT_UNLIKELY(pcData == NULL || fmt != 32 || cItems != 1))
                LogFlowThisFunc(("Wrong properties pcData=%#x, iFmt=%u, cItems=%u\n", pcData, fmt, cItems));
            else
            {
                newVer = reinterpret_cast<long*>(pcData)[0];
                LogFlowThisFunc(("wndCursor=%#x, XdndAware=%u\n", newVer));
            }
            XFree(pcData);
        }
    }

    /*
     * Is the window under the cursor another one than our current one?
     */
    if (wndCursor != m_wndCur && m_curVer != -1)
    {
        LogFlowThisFunc(("Leaving window=%#x\n", m_wndCur));

        /* We left the current XdndAware window. Announce this to the window. */
        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = m_wndCur;
        m.message_type = xAtom(XA_XdndLeave);
        m.format       = 32;
        m.data.l[0]    = m_wndProxy;

        xrc = XSendEvent(m_pDisplay, m_wndCur, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending XA_XdndLeave to old window=%#x\n", m_wndCur));
    }

    /*
     * Do we have a new window which now is under the cursor?
     */
    if (wndCursor != m_wndCur && newVer != -1)
    {
        LogFlowThisFunc(("Entering window=%#x\n", wndCursor));

        /*
         * We enter a new window. Announce the XdndEnter event to the new
         * window. The first three mime types are attached to the event (the
         * others could be requested by the XdndTypeList property from the
         * window itself).
         */
        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = wndCursor;
        m.message_type = xAtom(XA_XdndEnter);
        m.format       = 32;
        m.data.l[0]    = m_wndProxy;
        m.data.l[1]    = RT_MAKE_U32_FROM_U8(m_formats.size() > 3 ? 1 : 0, 0, 0, RT_MIN(VBOX_XDND_VERSION, newVer));
        m.data.l[2]    = m_formats.value(0, None);
        m.data.l[3]    = m_formats.value(1, None);
        m.data.l[4]    = m_formats.value(2, None);

        xrc = XSendEvent(m_pDisplay, wndCursor, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending XA_XdndEnter to new window=%#x\n", wndCursor));
    }

    if (newVer != -1)
    {
        LogFlowThisFunc(("Moving window=%#x, xPos=%RU32, yPos=%RU32\n",
                         wndCursor, u32xPos, u32yPos));

        /*
         * Send a XdndPosition event with the proposed action to the guest.
         */
        Atom pa = toX11Action(uAction);
        LogFlowThisFunc(("strAction='%s' ", xAtomToString(pa).c_str()));

        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = wndCursor;
        m.message_type = xAtom(XA_XdndPosition);
        m.format       = 32;
        m.data.l[0]    = m_wndProxy;
        m.data.l[2]    = RT_MAKE_U32(u32yPos, u32xPos);
        m.data.l[3]    = CurrentTime;
        m.data.l[4]    = pa;

        xrc = XSendEvent(m_pDisplay, wndCursor, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending XA_XdndPosition to window=%#x\n", wndCursor));
    }

    if (wndCursor == None && newVer == -1)
    {
        /* No window to process, so send a ignore ack event to the host. */
        rc = VbglR3DnDHGAcknowledgeOperation(m_uClientID, DND_IGNORE_ACTION);
    }

    m_wndCur = wndCursor;
    m_curVer = RT_MIN(VBOX_XDND_VERSION, newVer);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragInstance::hgX11ClientMessage(const XEvent& e)
{
    if (   m_mode  != HG)
//        || m_state != Dragging)
        return VERR_INVALID_STATE;

    /* Client messages are used to inform us about the status of a XdndAware
     * window, in response of some events we send to them. */
    int rc = VINF_SUCCESS;
    if (   e.xclient.message_type == xAtom(XA_XdndStatus)
        && m_wndCur               == static_cast<Window>(e.xclient.data.l[0]))
    {
        /* The XdndStatus message tell us if the window will accept the DnD
         * event and with which action. We immediately send this info down to
         * the host as a response of a previous DnD message. */
        LogFlowThisFunc(("XA_XdndStatus wnd=%#x, accept=%RTbool, action='%s'\n",
                         e.xclient.data.l[0],
                         ASMBitTest(&e.xclient.data.l[1], 0),
                         xAtomToString(e.xclient.data.l[4]).c_str()));

        uint32_t uAction = DND_IGNORE_ACTION;
        /** @todo Compare this with the allowed actions. */
        if (ASMBitTest(&e.xclient.data.l[1], 0))
            uAction = toHGCMAction(static_cast<Atom>(e.xclient.data.l[4]));

        rc = VbglR3DnDHGAcknowledgeOperation(m_uClientID, uAction);
    }
    else if (e.xclient.message_type == xAtom(XA_XdndFinished))
    {
        /* This message is send on a un/successful DnD drop request. */
        LogFlowThisFunc(("XA_XdndFinished: wnd=%#x, success=%RTbool, action='%s'\n",
            e.xclient.data.l[0],
            ASMBitTest(&e.xclient.data.l[1], 0),
            xAtomToString(e.xclient.data.l[2]).c_str()));

        reset();
    }
    else
        LogFlowThisFunc(("Unhandled: wnd=%#x, msg='%s'\n",
                         e.xclient.data.l[0], xAtomToString(e.xclient.message_type).c_str()));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragInstance::hgDrop(void)
{
    LogFlowThisFunc(("wndCur=%#x, mMode=%RU32, mState=%RU32\n",
                     m_wndCur, m_mode, m_state));

    if (   m_mode  != HG
        || m_state != Dragging)
        return VERR_INVALID_STATE;

    int rc = VINF_SUCCESS;

    /* Send a drop event to the current window and reset our DnD status. */
    XClientMessageEvent m;
    RT_ZERO(m);
    m.type         = ClientMessage;
    m.display      = m_pDisplay;
    m.window       = m_wndCur;
    m.message_type = xAtom(XA_XdndDrop);
    m.format       = 32;
    m.data.l[0]    = m_wndProxy;
    m.data.l[2]    = CurrentTime;

    int xrc = XSendEvent(m_pDisplay, m_wndCur, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
    if (RT_UNLIKELY(xrc == 0))
        LogFlowThisFunc(("Error sending XA_XdndDrop to window=%#x\n", m_wndCur));

    m_wndCur = None;
    m_curVer = -1;

    m_state = Dropped;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragInstance::hgX11SelectionRequest(const XEvent& e)
{
    AssertReturn(e.type == SelectionRequest, VERR_INVALID_PARAMETER);

    if (   m_mode  != HG)
//        || m_state != D)
        return VERR_INVALID_STATE;

    LogFlowThisFunc(("owner=%#x, requestor=%#x, sel_atom='%s', tar_atom='%s', prop_atom='%s', time=%u\n",
                     e.xselectionrequest.owner,
                     e.xselectionrequest.requestor,
                     xAtomToString(e.xselectionrequest.selection).c_str(),
                     xAtomToString(e.xselectionrequest.target).c_str(),
                     xAtomToString(e.xselectionrequest.property).c_str(),
                     e.xselectionrequest.time));

    int rc = VINF_SUCCESS;

    /*
     * A window is asking for some data. Normally here the data would be copied
     * into the selection buffer and send to the requestor. Obviously we can't
     * do that, cause we first need to ask the host for the data of the
     * requested mime type. This is done and later answered with the correct
     * data (s. dataReceived).
     */

    /* Is the requestor asking for the possible mime types? */
    if(e.xselectionrequest.target == xAtom(XA_TARGETS))
    {
        LogFlowThisFunc(("wnd=%#x asking for target list\n", e.xselectionrequest.requestor));

        /* If so, set the window property with the formats on the requestor
         * window. */
        setFormatsWindowProperty(e.xselectionrequest.requestor, e.xselectionrequest.property);

        XEvent s;
        RT_ZERO(s);
        s.xselection.type      = SelectionNotify;
        s.xselection.display   = e.xselection.display;
        s.xselection.time      = e.xselectionrequest.time;
        s.xselection.selection = e.xselectionrequest.selection;
        s.xselection.requestor = e.xselectionrequest.requestor;
        s.xselection.target    = e.xselectionrequest.target;
        s.xselection.property  = e.xselectionrequest.property;

        int xrc = XSendEvent(e.xselection.display, e.xselectionrequest.requestor, False, 0, &s);
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending SelectionNotify event to wnd=%#x\n", e.xselectionrequest.requestor));
    }
    /* Is the requestor asking for a specific mime type (we support)? */
    else if(m_formats.contains(e.xselectionrequest.target))
    {
        LogFlowThisFunc(("wnd=%#x asking for data (format='%s')\n",
                         e.xselectionrequest.requestor, xAtomToString(e.xselectionrequest.target).c_str()));

        /* If so, we need to inform the host about this request. Save the
         * selection request event for later use. */
        if (   m_state != Dropped)
            //        || m_curWin != e.xselectionrequest.requestor)
        {
            LogFlowThisFunc(("Refusing ...\n"));

            XEvent s;
            RT_ZERO(s);
            s.xselection.type      = SelectionNotify;
            s.xselection.display   = e.xselection.display;
            s.xselection.time      = e.xselectionrequest.time;
            s.xselection.selection = e.xselectionrequest.selection;
            s.xselection.requestor = e.xselectionrequest.requestor;
            s.xselection.target    = None;
            s.xselection.property  = e.xselectionrequest.property;

            int xrc = XSendEvent(e.xselection.display, e.xselectionrequest.requestor, False, 0, &s);
            if (RT_UNLIKELY(xrc == 0))
                LogFlowThisFunc(("Error sending SelectionNotify event to wnd=%#x\n", e.xselectionrequest.requestor));
        }
        else
        {
            LogFlowThisFunc(("Copying data from host ...\n"));

            memcpy(&m_selEvent, &e, sizeof(XEvent));
            rc = VbglR3DnDHGRequestData(m_uClientID, xAtomToString(e.xselectionrequest.target).c_str());
        }
    }
    /* Anything else. */
    else
    {
        LogFlowThisFunc(("Refusing unknown command\n"));

        /* We don't understand this request message and therefore answer with an
         * refusal messages. */
        XEvent s;
        RT_ZERO(s);
        s.xselection.type      = SelectionNotify;
        s.xselection.display   = e.xselection.display;
        s.xselection.time      = e.xselectionrequest.time;
        s.xselection.selection = e.xselectionrequest.selection;
        s.xselection.requestor = e.xselectionrequest.requestor;
        s.xselection.target    = None; /* default is refusing */
        s.xselection.property  = None; /* default is refusing */
        int xrc = XSendEvent(e.xselection.display, e.xselectionrequest.requestor, False, 0, &s);
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending SelectionNotify event to wnd=%#x\n", e.xselectionrequest.requestor));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragInstance::hgDataReceived(void *pvData, uint32_t cData)
{
    if (   m_mode  != HG
        || m_state != Dropped)
        return VERR_INVALID_STATE;

    if (RT_UNLIKELY(   pvData == NULL
                    || cData  == 0))
        return VERR_INVALID_PARAMETER;

    if (RT_UNLIKELY(m_state != Dropped))
        return VERR_INVALID_STATE;

    /* Make a copy of the data. The xserver will become the new owner. */
    void *pvNewData = RTMemAlloc(cData);
    if (RT_UNLIKELY(!pvNewData))
        return VERR_NO_MEMORY;
    memcpy(pvNewData, pvData, cData);

    /*
     * The host send us the DnD data in the requested mime type. This allows us
     * to fill the XdndSelection property of the requestor window with the data
     * and afterwards inform him about the new status.
     */
    XEvent s;
    RT_ZERO(s);
    s.xselection.type      = SelectionNotify;
    s.xselection.display   = m_selEvent.xselection.display;
//    s.xselection.owner     = m_selEvent.xselectionrequest.owner;
    s.xselection.time      = m_selEvent.xselectionrequest.time;
    s.xselection.selection = m_selEvent.xselectionrequest.selection;
    s.xselection.requestor = m_selEvent.xselectionrequest.requestor;
    s.xselection.target    = m_selEvent.xselectionrequest.target;
    s.xselection.property  = m_selEvent.xselectionrequest.property;

    LogFlowThisFunc(("owner=%#x,requestor=%#x,sel_atom='%s',tar_atom='%s',prop_atom='%s',time=%u\n",
                     m_selEvent.xselectionrequest.owner,
                     s.xselection.requestor,
                     xAtomToString(s.xselection.selection).c_str(),
                     xAtomToString(s.xselection.target).c_str(),
                     xAtomToString(s.xselection.property).c_str(),
                     s.xselection.time));

    /* Fill up the property with the data. */
    XChangeProperty(s.xselection.display, s.xselection.requestor, s.xselection.property, s.xselection.target, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(pvNewData), cData);
    int xrc = XSendEvent(s.xselection.display, s.xselection.requestor, True, 0, &s);
    if (RT_UNLIKELY(xrc == 0))
        LogFlowThisFunc(("Error sending SelectionNotify event to wnd=%#x\n", s.xselection.requestor));

    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_DRAG_AND_DROP_GH
/*
 * Guest -> Host
 */

int DragInstance::ghIsDnDPending(void)
{
    int rc = VINF_SUCCESS;
    Window wndOwner = XGetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection));
    LogFlowThisFunc(("Checking pending wndOwner=%#x wndProxy=%#x\n", wndOwner, m_wndProxy));

    /* Is there someone own the Xdnd selection which aren't we. */
    if (   wndOwner
        && wndOwner != m_wndProxy)
    {
        /* Map the window on the current cursor position, which should provoke
         * an XdndEnter event. */
        int rx, ry;
        showProxyWin(rx, ry);
        XEvent e;
        if (m_pParent->waitForXMsg(e, ClientMessage))
        {
            int xrc = Success;
            XClientMessageEvent *clme = reinterpret_cast<XClientMessageEvent*>(&e);
            LogFlowThisFunc(("Next X event %s\n", gX11->xAtomToString(clme->message_type).c_str()));
            if (clme->message_type == xAtom(XA_XdndEnter))
            {
                Atom type = None;
                int f;
                unsigned long n, a;
                unsigned char *ret = 0;
                reset();

                m_formats.clear();
                m_actions.clear();
                m_wndCur = wndOwner;
                LogFlowThisFunc(("XA_XdndEnter\n"));
                /* Check if the mime types are in the msg itself or if we need
                 * to fetch the XdndTypeList property from the window. */
                if (!ASMBitTest(&clme->data.l[1], 0))
                {
                    for (int i = 2; i < 5; ++i)
                    {
                        LogFlowThisFunc(("Receive list msg: %s\n", gX11->xAtomToString(clme->data.l[i]).c_str()));
                        m_formats.append(clme->data.l[i]);
                    }
                }
                else
                {
                    xrc = XGetWindowProperty(m_pDisplay, wndOwner, xAtom(XA_XdndTypeList), 0, VBOX_MAX_XPROPERTIES, False, XA_ATOM, &type, &f, &n, &a, &ret);
                    if (   xrc == Success
                        && n > 0
                        && ret)
                    {
                        Atom *data = reinterpret_cast<Atom*>(ret);
                        for (int i = 0; i < RT_MIN(VBOX_MAX_XPROPERTIES, n); ++i)
                        {
                            LogFlowThisFunc(("Receive list: %s\n", gX11->xAtomToString(data[i]).c_str()));
                            m_formats.append(data[i]);
                        }
                        XFree(ret);
                    }
                }
                /* Fetch the possible list of actions, if this property is set. */
                xrc = XGetWindowProperty(m_pDisplay, wndOwner, xAtom(XA_XdndActionList), 0, VBOX_MAX_XPROPERTIES, False, XA_ATOM, &type, &f, &n, &a, &ret);
                if (   xrc == Success
                    && n > 0
                    && ret)
                {
                    Atom *data = reinterpret_cast<Atom*>(ret);
                    for (int i = 0; i < RT_MIN(VBOX_MAX_XPROPERTIES, n); ++i)
                    {
                        LogFlowThisFunc(("Receive actions: %s\n", gX11->xAtomToString(data[i]).c_str()));
                        m_actions.append(data[i]);
                    }
                    XFree(ret);
                }

                m_state = Dragging;
                m_mode  = GH;
                /* Acknowledge the event by sending a Status msg back to the
                 * window. */
                XClientMessageEvent m;
                RT_ZERO(m);
                m.type         = ClientMessage;
                m.display      = m_pDisplay;
                m.window       = clme->data.l[0];
                m.message_type = xAtom(XA_XdndStatus);
                m.format       = 32;
                m.data.l[0]    = m_wndProxy;
                m.data.l[1]    = 1;
                m.data.l[4]    = xAtom(XA_XdndActionCopy);
                xrc = XSendEvent(m_pDisplay, clme->data.l[0], False, 0, reinterpret_cast<XEvent*>(&m));
                if (RT_UNLIKELY(xrc == 0))
                    LogFlowThisFunc(("Error sending xevent\n"));
            }
            else if (clme->message_type == xAtom(XA_XdndPosition))
            {
                LogFlowThisFunc(("XA_XdndPosition\n"));
                XClientMessageEvent m;
                RT_ZERO(m);
                m.type         = ClientMessage;
                m.display      = m_pDisplay;
                m.window       = clme->data.l[0];
                m.message_type = xAtom(XA_XdndStatus);
                m.format       = 32;
                m.data.l[0]    = m_wndProxy;
                m.data.l[1]    = 1;
                m.data.l[4]    = clme->data.l[4];
                xrc = XSendEvent(m_pDisplay, clme->data.l[0], False, 0, reinterpret_cast<XEvent*>(&m));
                if (RT_UNLIKELY(xrc == 0))
                    LogFlowThisFunc(("Error sending xevent\n"));
            }
            else if (clme->message_type == xAtom(XA_XdndLeave))
            {
            }
        }
        hideProxyWin();

        rc = VbglR3DnDGHAcknowledgePending(DND_COPY_ACTION, toHGCMActions(m_actions),
                                           gX11->xAtomListToString(m_formats).c_str());
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragInstance::ghDropped(const RTCString &strFormat, uint32_t action)
{
    LogFlowThisFunc(("format='%s' action=%d\n", strFormat.c_str(), action));
    int rc = VINF_SUCCESS;

    /* Show the proxy window, so that the source will find it. */
    int rx, ry;
    showProxyWin(rx, ry);
    XFlush(m_pDisplay);
    /* We send a fake release event to the current window, cause
     * this should have the grab. */
    sendButtonEvent(m_wndCur, rx, ry, 1, false);
    /* The fake button release event, should lead to an XdndDrop event from the
     * source. Because of the showing of the proxy window, sometimes other Xdnd
     * events occurs before, like a XdndPosition event. We are not interested
     * in those, so try to get the right one. */
    XEvent e;
    XClientMessageEvent *clme = 0;
    RT_ZERO(e);
    int tries = 3;
    do
    {
        if (m_pParent->waitForXMsg(e, ClientMessage))
        {
            if (reinterpret_cast<XClientMessageEvent*>(&e)->message_type == xAtom(XA_XdndDrop))
            {
                clme = reinterpret_cast<XClientMessageEvent*>(&e);
                break;
            }
        }
    } while (tries--);
    if (clme)
    {
        /* Make some paranoid checks. */
        if (clme->message_type == xAtom(XA_XdndDrop))
        {
            /* Request to convert the selection in the specific format and
             * place it to our proxy window as property. */
            Window srcWin = m_wndCur;//clme->data.l[0];
            Atom aFormat  = gX11->stringToxAtom(strFormat.c_str());
            XConvertSelection(m_pDisplay, xAtom(XA_XdndSelection), aFormat, xAtom(XA_XdndSelection), m_wndProxy, clme->data.l[2]);
            /* Wait for the selection notify event. */
            RT_ZERO(e);
            if (m_pParent->waitForXMsg(e, SelectionNotify))
            {
                /* Make some paranoid checks. */
                if (   e.xselection.type      == SelectionNotify
                    && e.xselection.display   == m_pDisplay
                    && e.xselection.selection == xAtom(XA_XdndSelection)
                    && e.xselection.requestor == m_wndProxy
                    && e.xselection.target    == aFormat)
                {
                    LogFlowThisFunc(("Selection notfiy (from: %x)\n", m_wndCur));
                    Atom type;
                    int format;
                    unsigned long cItems, cbRemaining;
                    unsigned char *ucData = 0;
                    XGetWindowProperty(m_pDisplay, m_wndProxy, xAtom(XA_XdndSelection),
                                       0, VBOX_MAX_XPROPERTIES, True, AnyPropertyType,
                                       &type, &format, &cItems, &cbRemaining, &ucData);
                    LogFlowThisFunc(("%s %d %d %s\n", gX11->xAtomToString(type).c_str(), cItems, format, ucData));
                    if (   type        != None
                        && ucData      != NULL
                        && format      >= 8
                        && cItems      >  0
                        && cbRemaining == 0)
                    {
                        size_t cbData = cItems * (format / 8);
                        /* For whatever reason some of the string mime-types are not
                         * zero terminated. Check that and correct it when necessary,
                         * cause the guest side wants this always. */
                        if (   m_sstrStringMimeTypes.contains(strFormat)
                            && ucData[cbData - 1] != '\0')
                        {
                            LogFlowThisFunc(("Rebuild %u\n", cbData));
                            unsigned char *ucData1 = static_cast<unsigned char*>(RTMemAlloc(cbData + 1));
                            if (ucData1)
                            {
                                memcpy(ucData1, ucData, cbData);
                                ucData1[cbData++] = '\0';
                                /* Got the data and its fully transfered. */
                                rc = VbglR3DnDGHSendData(ucData1, cbData);
                                RTMemFree(ucData1);
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }
                        else
                            /* Just send the data to the host. */
                            rc = VbglR3DnDGHSendData(ucData, cbData);

                        LogFlowThisFunc(("send responce\n"));
                        /* Confirm the result of the transfer to the source window. */
                        XClientMessageEvent m;
                        RT_ZERO(m);
                        m.type         = ClientMessage;
                        m.display      = m_pDisplay;
                        m.window       = srcWin;
                        m.message_type = xAtom(XA_XdndFinished);
                        m.format       = 32;
                        m.data.l[0]    = m_wndProxy;
                        m.data.l[1]    = RT_SUCCESS(rc) ?                   1 : 0;    /* Confirm or deny success */
                        m.data.l[2]    = RT_SUCCESS(rc) ? toX11Action(action) : None; /* Action used on success */

                        int xrc = XSendEvent(m_pDisplay, srcWin, True, NoEventMask, reinterpret_cast<XEvent*>(&m));
                        if (RT_UNLIKELY(xrc == 0))
                            LogFlowThisFunc(("Error sending xevent\n"));
                    }
                    else
                    {
                        if (type == xAtom(XA_INCR))
                        {
                            /* Todo: */
                            AssertMsgFailed(("Incrementally transfers are not supported, yet\n"));
                            rc = VERR_NOT_IMPLEMENTED;
                        }
                        else
                        {
                            AssertMsgFailed(("Not supported data type\n"));
                            rc = VERR_INVALID_PARAMETER;
                        }
                        /* Cancel this. */
                        XClientMessageEvent m;
                        RT_ZERO(m);
                        m.type         = ClientMessage;
                        m.display      = m_pDisplay;
                        m.window       = srcWin;
                        m.message_type = xAtom(XA_XdndFinished);
                        m.format       = 32;
                        m.data.l[0]    = m_wndProxy;
                        m.data.l[1]    = 0;
                        m.data.l[2]    = None;
                        int xrc = XSendEvent(m_pDisplay, srcWin, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
                        if (RT_UNLIKELY(xrc == 0))
                            LogFlowThisFunc(("Error sending xevent\n"));
                        m_wndCur = 0;
                    }
                    /* Cleanup */
                    if (ucData)
                        XFree(ucData);
                }
                else
                    rc = VERR_INVALID_PARAMETER;
            }
            else
                rc = VERR_TIMEOUT;
        }
        else
            rc = VERR_WRONG_ORDER;
    }
    else
        rc = VERR_TIMEOUT;

    /* Inform the host on error */
    if (RT_FAILURE(rc))
        VbglR3DnDGHErrorEvent(rc);

    /* At this point, we have either successfully transfered any data or not.
     * So reset our internal state, cause we are done. */
    reset();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

/*
 * Helpers
 */

int DragInstance::moveCursor(uint32_t u32xPos, uint32_t u32yPos)
{
    /* Move the guest pointer to the DnD position, so we can find the window
     * below that position. */
    XWarpPointer(m_pDisplay, None, m_wndRoot, 0, 0, 0, 0, u32xPos, u32yPos);
    return VINF_SUCCESS;
}

void DragInstance::sendButtonEvent(Window w, int rx, int ry, int button, bool fPress) const
{
//    XTestFakeMotionEvent(m_pDisplay, -1, rx, ry, CurrentTime);
//    XTestFakeMotionEvent(m_pDisplay, -1, rx + 1, ry + 1, CurrentTime);
//    int rc = XTestFakeButtonEvent(m_pDisplay, 1, False, CurrentTime);
//    if (rc != 0)
    {
        XButtonEvent be;
        RT_ZERO(be);
        be.display      = m_pDisplay;
        be.root         = m_wndRoot;
        be.window       = w;
        be.subwindow    = None;
        be.same_screen  = True;
        be.time         = CurrentTime;
        be.button       = button;
        be.state       |= button == 1 ? Button1MotionMask :
                          button == 2 ? Button2MotionMask :
                          button == 3 ? Button3MotionMask :
                          button == 4 ? Button4MotionMask :
                          button == 5 ? Button5MotionMask : 0;
        be.type         = fPress ? ButtonPress : ButtonRelease;
        be.x_root       = rx;
        be.y_root       = ry;
        XTranslateCoordinates(m_pDisplay, be.root, be.window, be.x_root, be.y_root, &be.x, &be.y, &be.subwindow);
        int xrc = XSendEvent(m_pDisplay, be.window, True, ButtonPressMask, reinterpret_cast<XEvent*>(&be));
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending xevent\n"));
    }
}

void DragInstance::showProxyWin(int &rx, int &ry) const
{
    int cx, cy;
    unsigned int m;
    Window r, c;
//    XTestGrabControl(m_pDisplay, False);
    XQueryPointer(m_pDisplay, m_wndRoot, &r, &c, &rx, &ry, &cx, &cy, &m);
    XSynchronize(m_pDisplay, True);
    XMapWindow(m_pDisplay, m_wndProxy);
    XRaiseWindow(m_pDisplay, m_wndProxy);
    XMoveResizeWindow(m_pDisplay, m_wndProxy, rx, ry, 1, 1);
    XWarpPointer(m_pDisplay, None, m_wndRoot, 0, 0, 0, 0, rx , ry);
    XSynchronize(m_pDisplay, False);
//    XTestGrabControl(m_pDisplay, True);
}

void DragInstance::hideProxyWin(void) const
{
    XUnmapWindow(m_pDisplay, m_wndProxy);
}

/* Currently, not used */
void DragInstance::registerForEvents(Window wndThis) const
{
//    if (w == m_proxyWin)
//        return;

    LogFlowThisFunc(("%x\n", wndThis));
//    XSelectInput(m_pDisplay, w, Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask);//| SubstructureNotifyMask);
//    XSelectInput(m_pDisplay, w, ButtonMotionMask); //PointerMotionMask);
    XSelectInput(m_pDisplay, wndThis, PointerMotionMask); //PointerMotionMask);
    Window hRealRoot, hParent;
    Window *phChildrenRaw = NULL;
    unsigned cChildren;
    if (XQueryTree(m_pDisplay, wndThis, &hRealRoot, &hParent, &phChildrenRaw, &cChildren))
    {
        for (unsigned i = 0; i < cChildren; ++i)
            registerForEvents(phChildrenRaw[i]);
        XFree(phChildrenRaw);
    }
}

void DragInstance::setActionsWindowProperty(Window wndThis, const RTCList<Atom> &actionList) const
{
    if (actionList.isEmpty())
        return;

    XChangeProperty(m_pDisplay, wndThis, xAtom(XA_XdndActionList), XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(actionList.raw()), actionList.size());
}

void DragInstance::clearActionsWindowProperty(Window wndThis) const
{
    XDeleteProperty(m_pDisplay, wndThis, xAtom(XA_XdndActionList));
}

void DragInstance::setFormatsWindowProperty(Window wndThis, Atom property) const
{
    if (m_formats.isEmpty())
        return;

    /* We support TARGETS and the data types. */
    RTCList<Atom> targets(m_formats.size() + 1);
    targets.append(xAtom(XA_TARGETS));
    targets.append(m_formats);

    /* Add the property with the property data to the window. */
    XChangeProperty(m_pDisplay, wndThis, property, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(targets.raw()), targets.size());
}

void DragInstance::clearFormatsWindowProperty(Window wndThis) const
{
    XDeleteProperty(m_pDisplay, wndThis, xAtom(XA_XdndTypeList));
}

RTCList<Atom> DragInstance::toAtomList(const RTCList<RTCString> &formatList) const
{
    RTCList<Atom> atomList;
    for (size_t i = 0; i < formatList.size(); ++i)
        atomList.append(XInternAtom(m_pDisplay, formatList.at(i).c_str(), False));

    return atomList;
}

RTCList<Atom> DragInstance::toAtomList(void *pvData, uint32_t cData) const
{
    if (   !pvData
        || !cData)
        return RTCList<Atom>();
    char *pszStr = (char*)pvData;
    uint32_t cStr = cData;

    RTCList<Atom> atomList;
    while (cStr > 0)
    {
        size_t cSize = RTStrNLen(pszStr, cStr);
        /* Create a copy with max N chars, so that we are on the save side,
         * even if the data isn't zero terminated. */
        char *pszTmp = RTStrDupN(pszStr, cSize);
        LogFlowThisFunc(("f: %s\n", pszTmp));
        atomList.append(XInternAtom(m_pDisplay, pszTmp, False));
        RTStrFree(pszTmp);
        pszStr += cSize + 1;
        cStr   -= cSize + 1;
    }

    return atomList;
}

/* static */
Atom DragInstance::toX11Action(uint32_t uAction)
{
    /* Ignore is None */
    return (isDnDCopyAction(uAction) ? xAtom(XA_XdndActionCopy) :
            isDnDMoveAction(uAction) ? xAtom(XA_XdndActionMove) :
            isDnDLinkAction(uAction) ? xAtom(XA_XdndActionLink) :
            None);
}

/* static */
RTCList<Atom> DragInstance::toX11Actions(uint32_t uActions)
{
    RTCList<Atom> actionList;
    if (hasDnDCopyAction(uActions))
        actionList.append(xAtom(XA_XdndActionCopy));
    if (hasDnDMoveAction(uActions))
        actionList.append(xAtom(XA_XdndActionMove));
    if (hasDnDLinkAction(uActions))
        actionList.append(xAtom(XA_XdndActionLink));

    return actionList;
}

/* static */
uint32_t DragInstance::toHGCMAction(Atom atom)
{
    uint32_t uAction = DND_IGNORE_ACTION;
    if (atom == xAtom(XA_XdndActionCopy))
        uAction = DND_COPY_ACTION;
    else if (atom == xAtom(XA_XdndActionMove))
        uAction = DND_MOVE_ACTION;
    else if (atom == xAtom(XA_XdndActionLink))
        uAction = DND_LINK_ACTION;
    return uAction;
}

/* static */
uint32_t DragInstance::toHGCMActions(const RTCList<Atom> &actionsList)
{
    uint32_t uActions = DND_IGNORE_ACTION;
    for (size_t i = 0; i < actionsList.size(); ++i)
        uActions |= toHGCMAction(actionsList.at(i));
    return uActions;
}

/*******************************************************************************
 *
 * DragAndDropService Implementation
 *
 ******************************************************************************/

RTCList<RTCString> toStringList(void *pvData, uint32_t cData)
{
    if (   !pvData
        || !cData)
        return RTCList<RTCString>();
    char *pszStr = (char*)pvData;
    uint32_t cStr = cData;

    RTCList<RTCString> strList;
    while (cStr > 0)
    {
        size_t cSize = RTStrNLen(pszStr, cStr);
        /* Create a copy with max N chars, so that we are on the save side,
         * even if the data isn't zero terminated. */
        char *pszTmp = RTStrDupN(pszStr, cSize);
        strList.append(pszTmp);
        RTStrFree(pszTmp);
        pszStr += cSize + 1;
        cStr   -= cSize + 1;
    }

    return strList;
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH

bool DragAndDropService::waitForXMsg(XEvent &ecm, int type, uint32_t uiMaxMS /* = 100 */)
{
    const uint64_t uiStart = RTTimeProgramMilliTS();
    do
    {
        if (!m_eventQueue.isEmpty())
        {
            LogFlowThisFunc(("new msg size %d\n", m_eventQueue.size()));
            /* Check if there is a client message in the queue. */
            for (size_t i = 0; i < m_eventQueue.size(); ++i)
            {
                DnDEvent e = m_eventQueue.at(i);
                if(   e.type     == DnDEvent::X11_Type)
                    LogFlowThisFunc(("new msg\n"));
                if(   e.type     == DnDEvent::X11_Type
                   && e.x11.type == type)
                {
                    m_eventQueue.removeAt(i);
                    ecm = e.x11;
                    return true;
                }
            }
        }
        int rc = RTSemEventWait(m_hEventSem, 25);
//        if (RT_FAILURE(rc))
//            return false;
    }
    while (RTTimeProgramMilliTS() - uiStart < uiMaxMS);

    return false;
}

#endif

void DragAndDropService::clearEventQueue()
{
    m_eventQueue.clear();
}

int DragAndDropService::run(bool fDaemonised /* = false */)
{
    int rc = VINF_SUCCESS;
    LogRelFlowFunc(("\n"));

    do
    {
        /* Initialize X11 DND */
        rc = x11DragAndDropInit();
        if (RT_FAILURE(rc))
            break;

        m_pCurDnD = new DragInstance(m_pDisplay, this);
        if (!m_pCurDnD)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        /* Note: For multiple screen support in VBox it is not necessary to use
         * another screen number than zero. Maybe in the future it will become
         * necessary if VBox supports multiple X11 screens. */
        rc = m_pCurDnD->init(0);
        if (RT_FAILURE(rc))
            break;

        /* Loop over new events */
        do
        {
            DnDEvent e;
            RT_ZERO(e);
            if (m_eventQueue.isEmpty())
                rc = RTSemEventWait(m_hEventSem, RT_INDEFINITE_WAIT);
            if (!m_eventQueue.isEmpty())
            {
                e = m_eventQueue.first();
                m_eventQueue.removeFirst();
                LogFlowThisFunc(("new msg %d\n", e.type));
                if (e.type == DnDEvent::HGCM_Type)
                {
                    switch (e.hgcm.uType)
                    {
                        case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
                        {
                            RTCList<RTCString> formats = RTCString(e.hgcm.pszFormats, e.hgcm.cbFormats - 1).split("\r\n");
                            m_pCurDnD->hgEnter(formats, e.hgcm.u.a.uAllActions);
                            /* Enter is always followed by a move event. */
                        }
                        case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
                        {
                            m_pCurDnD->hgMove(e.hgcm.u.a.uXpos, e.hgcm.u.a.uYpos, e.hgcm.u.a.uDefAction);
                            break;
                        }
                        case DragAndDropSvc::HOST_DND_HG_EVT_LEAVE:
                        {
                            m_pCurDnD->reset();
                            /* Not sure if this is really right! */
                            clearEventQueue();
                            break;
                        }
                        case DragAndDropSvc::HOST_DND_HG_EVT_DROPPED:
                        {
                            m_pCurDnD->hgDrop();
                            break;
                        }
                        case DragAndDropSvc::HOST_DND_HG_SND_DATA:
                        {
                            m_pCurDnD->hgDataReceived(e.hgcm.u.b.pvData, e.hgcm.u.b.cbData);
                            break;
                        }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
                        case DragAndDropSvc::HOST_DND_GH_REQ_PENDING:
                        {
                            m_pCurDnD->ghIsDnDPending();
                            break;
                        }
                        case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
                        {
                            m_pCurDnD->ghDropped(e.hgcm.pszFormats, e.hgcm.u.a.uDefAction);
                            /* Not sure if this is really right! */
                            clearEventQueue();
                            break;
                        }
#endif
                        default:
                            LogFlowThisFunc(("Unknown message: %RU32\n", e.hgcm.uType));
                            break;
                    }

                    /* Some messages require cleanup. */
                    switch (e.hgcm.uType)
                    {
                        case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
                        case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
                        case DragAndDropSvc::HOST_DND_HG_EVT_DROPPED:
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
                        case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
#endif
                        {
                            if (e.hgcm.pszFormats)
                                RTMemFree(e.hgcm.pszFormats);
                            break;
                        }
                        case DragAndDropSvc::HOST_DND_HG_SND_DATA:
                        {
                            if (e.hgcm.pszFormats)
                                RTMemFree(e.hgcm.pszFormats);
                            if (e.hgcm.u.b.pvData)
                                RTMemFree(e.hgcm.u.b.pvData);
                            break;
                        }
                        default:
                            break;
                    }
                }
                else if(e.type == DnDEvent::X11_Type)
                {
                    LogFlowThisFunc(("X11 type: %u\n", e.x11.type));
                    /* Now the X11 event stuff */
                    switch (e.x11.type)
                    {
                        case SelectionRequest: m_pCurDnD->hgX11SelectionRequest(e.x11); break;
                        case ClientMessage:    m_pCurDnD->hgX11ClientMessage(e.x11); break;
                        case SelectionClear:   LogFlowThisFunc(("DnD_CLER\n")); break;
//                      case MotionNotify: m_pCurDnD->hide(); break;
                    }
                }
            }
        } while (!ASMAtomicReadBool(&m_fSrvStopping));
    } while (0);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragAndDropService::x11DragAndDropInit(void)
{
    /* Connect to the x11 server. */
    m_pDisplay = XOpenDisplay(NULL);
    if (!m_pDisplay)
        /* todo: correct errors */
        return VERR_NOT_FOUND;

    xHelpers *pHelpers = xHelpers::getInstance(m_pDisplay);
    if (!pHelpers)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;
    do
    {
        /* Signal a new event to our main loop. */
        rc = RTSemEventCreate(&m_hEventSem);
        if (RT_FAILURE(rc))
            break;
        /* Event thread for events coming from the HGCM device. */
        rc = RTThreadCreate(&m_hHGCMThread, hgcmEventThread, this,
                            0, RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE,
                            "HGCM-NOTIFY");
        if (RT_FAILURE(rc))
            break;
        /* Event thread for events coming from the x11 system. */
        rc = RTThreadCreate(&m_hX11Thread, x11EventThread, this,
                            0, RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE,
                            "X11-NOTIFY");
    } while (0);

    /* Cleanup on failure */
    if (RT_FAILURE(rc))
        x11DragAndDropTerm();

    return rc;
}

int DragAndDropService::x11DragAndDropTerm(void)
{
    /* Mark that we are stopping. */
    ASMAtomicWriteBool(&m_fSrvStopping, true);
    RTSemEventSignal(m_hEventSem);

    if (m_pDisplay)
    {
        /* Send a x11 client messages to the x11 event loop. */
        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = None;
        m.message_type = xAtom(XA_dndstop);
        m.format       = 32;
        int xrc = XSendEvent(m_pDisplay, None, True, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending xevent\n"));
    }

    /* We cannot signal the m_hHGCMThread as it is most likely waiting in vbglR3DoIOCtl() */
    /** @todo r=michael Don't we have a mechanism for cancelling HGCM calls
     *                  though? */
    /* Wait for our event threads to stop. */
    /** @todo r=michael This routine is generally called on the X11 thread,
     *                  protected by a mutex, so the following thread wait
     *                  makes us hang forever. */
    if (m_hX11Thread)
        RTThreadWait(m_hX11Thread, RT_INDEFINITE_WAIT, NULL);
    /* Cleanup */
    /* todo: This doesn't work. The semaphore was interrupted by the user
     * signal. It is not possible to destroy a semaphore while it is in interrupted state.
     * According to Frank, the cleanup stuff done here is done _wrong_. We just
     * should signal the main loop to stop and do the cleanup there. Needs
     * adoption in all VBoxClient::Service's. */
//    if (m_hEventSem)
//        RTSemEventDestroy(m_hEventSem);
    if (m_pDisplay)
        XCloseDisplay(m_pDisplay);
    return VINF_SUCCESS;
}

/* static */
int DragAndDropService::hgcmEventThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    DragAndDropService *pThis = static_cast<DragAndDropService*>(pvUser);
    DnDEvent e;

    uint32_t uClientID;
    int rc = VbglR3DnDConnect(&uClientID);
    if (RT_FAILURE(rc)) {
        LogFlowFunc(("Unable to connect to HGCM service, rc=%Rrc\n", rc));
        return rc;
    }

    /* Number of invalid messages skipped in a row. */
    int cMsgSkippedInvalid = 0;

    do
    {
        RT_ZERO(e);
        e.type = DnDEvent::HGCM_Type;
        /* Wait for new events */
        rc = VbglR3DnDProcessNextMessage(uClientID, &e.hgcm);
        if (RT_SUCCESS(rc))
        {
            cMsgSkippedInvalid = 0; /* Reset skipped messages count. */

            pThis->m_eventQueue.append(e);
            rc = RTSemEventSignal(pThis->m_hEventSem);
            if (RT_FAILURE(rc))
                return rc;
        }
        else
        {
            /* Old(er) hosts either are broken regarding DnD support or otherwise
             * don't support the stuff we do on the guest side, so make sure we
             * don't process invalid messages forever. */
            if (rc == VERR_INVALID_PARAMETER)
                cMsgSkippedInvalid++;
            if (cMsgSkippedInvalid > 3)
            {
                LogFlowFunc(("Too many invalid/skipped messages from host, exiting ...\n"));
                break;
            }
        }

    } while (!ASMAtomicReadBool(&pThis->m_fSrvStopping));

    VbglR3DnDDisconnect(uClientID);

    return VINF_SUCCESS;
}

/* static */
int DragAndDropService::x11EventThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    DragAndDropService *pThis = static_cast<DragAndDropService*>(pvUser);
    DnDEvent e;
    do
    {
        /* Wait for new events. We can't use XIfEvent here, cause this locks
         * the window connection with a mutex and if no X11 events occurs this
         * blocks any other calls we made to X11. So instead check for new
         * events and if there are not any new one, sleep for a certain amount
         * of time. */
        if (XEventsQueued(pThis->m_pDisplay, QueuedAfterFlush) > 0)
        {
            RT_ZERO(e);
            e.type = DnDEvent::X11_Type;
            XNextEvent(pThis->m_pDisplay, &e.x11);
#if 0
            /* We never detect the stop event here for some reason */
            /* Check for a stop message. */
            if (   e.x11.type == ClientMessage
                && e.x11.xclient.message_type == xAtom(XA_dndstop))
                break;
#endif
//            if (isDnDRespondEvent(pThis->m_pDisplay, &e.x11, 0))
            {
                /* Appending makes a copy of the event structure. */
                pThis->m_eventQueue.append(e);
                int rc = RTSemEventSignal(pThis->m_hEventSem);
                if (RT_FAILURE(rc))
                    return rc;
            }
        }
        else
            RTThreadSleep(25);
    } while (!ASMAtomicReadBool(&pThis->m_fSrvStopping));

    return VINF_SUCCESS;
}

/* Static factory */
VBoxClient::Service *VBoxClient::GetDragAndDropService(void)
{
    DragAndDropService *pService = new DragAndDropService();
    return pService;
}

