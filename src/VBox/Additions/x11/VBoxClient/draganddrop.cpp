/** @file
 * X11 guest client - Drag and Drop.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
#include <X11/extensions/XTest.h>

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
 * arrives, we asks if there is currently an owner of the XdndSelection
 * property. If so, our proxy window is shown (1x1, but without backing store)
 * and some mouse event is triggered. This should be followed by an XdndEnter
 * event send to the proxy window. From this event we can fetch the necessary
 * info of the mime-types and allowed actions and send this back to the host.
 * On a drop request from the host, we query for the selection and should get
 * the data in the specified mime-type. This data is send back to the host.
 * After that we send a XdndLeave event to the source window.
 * Todo:
 * - this isn't finished, yet. Currently the mouse isn't correctly released
 * in the guest (both, when the drop was successfully or canceled).
 * - cancel (e.g. with the ESC key) doesn't work
 *
 * Todo:
 * - XdndProxy window support
 * - INCR support
 * - make this much more robust for crashes of the other party
 * - really check for the Xdnd version and the supported features
 */

#define VERBOSE 1

#if defined(VERBOSE) && defined(DEBUG_poetzsch)
# include <iprt/stream.h>
# define DO(s) RTPrintf s
#else
# define DO(s) do {} while(0)
//# define DO(s) Log s
#endif

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

    static xHelpers *instance(Display *pDisplay = 0)
    {
        if (!m_pInstance)
        {
            AssertPtrReturn(pDisplay, 0);
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
#define gX11 xHelpers::instance()
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
Window xHelpers::applicationWindowBelowCursor(Window parentWin) const
{
    /* No parent, nothing to do. */
    if(parentWin == 0)
        return 0;

    Window appWin = 0;
    int cProps = -1;
    /* Fetch all x11 window properties of the parent window. */
    Atom *pProps = XListProperties(m_pDisplay, parentWin, &cProps);
    if (cProps > 0)
    {
        /* We check the window for the WM_STATE property. */
        for(int i = 0; i < cProps; ++i)
            if(pProps[i] == xAtom(XA_WM_STATE))
            {
                /* Found it. */
                appWin = parentWin;
                break;
            }
        /* Cleanup */
        XFree(pProps);
    }

    if (!appWin)
    {
        Window childWin, wtmp;
        int tmp;
        unsigned int utmp;
        /* Query the next child window of the parent window at the current
         * mouse position. */
        XQueryPointer(m_pDisplay, parentWin, &wtmp, &childWin, &tmp, &tmp, &tmp, &tmp, &utmp);
        /* Recursive call our self to dive into the child tree. */
        appWin = applicationWindowBelowCursor(childWin);
    }

    return appWin;
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
    DragAndDropService *m_pParent;
    Display            *m_pDisplay;
    int                 m_screenId;
    Screen             *m_pScreen;
    Window              m_rootWin;
    Window              m_proxyWin;
    Window              m_curWin;
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
    DragAndDropService()
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

    virtual void cleanup()
    {
        /* Cleanup */
        x11DragAndDropTerm();
        VbglR3DnDTerm();
    };

private:
    int x11DragAndDropInit();
    int x11DragAndDropTerm();
    static int hgcmEventThread(RTTHREAD hThread, void *pvUser);
    static int x11EventThread(RTTHREAD hThread, void *pvUser);

    bool waitForXMsg(XEvent &ecm, int type, uint32_t uiMaxMS = 100);
    void clearEventQueue();
    /* Usually XCheckMaskEvent could be used for queering selected x11 events.
     * Unfortunately this doesn't work exactly with the events we need. So we
     * use this predicate method below and XCheckIfEvent. */
    static Bool isDnDRespondEvent(Display * /* pDisplay */, XEvent *pEvent, char *pUser)
    {
        if (!pEvent)
            return False;
        if (   pEvent->type == SelectionClear
            || pEvent->type == ClientMessage
            || pEvent->type == MotionNotify
            || pEvent->type == SelectionRequest)
//            || (   pEvent->type == ClientMessage
//                && reinterpret_cast<XClientMessageEvent*>(pEvent)->window == reinterpret_cast<Window>(pUser))
//            || (   pEvent->type == SelectionRequest
//                && reinterpret_cast<XSelectionRequestEvent*>(pEvent)->requestor == reinterpret_cast<Window>(pUser)))
            return True;
        return False;
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
  : m_pParent(pParent)
  , m_pDisplay(pDisplay)
  , m_pScreen(0)
  , m_rootWin(0)
  , m_proxyWin(0)
  , m_curWin(0)
  , m_curVer(-1)
  , m_mode(Unknown)
  , m_state(Uninitialized)
{
    uninit();
}

void DragInstance::uninit()
{
    reset();
    if (m_proxyWin != 0)
        XDestroyWindow(m_pDisplay, m_proxyWin);
    m_state    = Uninitialized;
    m_screenId = -1;
    m_pScreen  = 0;
    m_rootWin  = 0;
    m_proxyWin = 0;
}

void DragInstance::reset()
{
    /* Hide the proxy win. */
    hideProxyWin();
    /* If we are currently the Xdnd selection owner, clear that. */
    Window w = XGetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection));
    if (w == m_proxyWin)
        XSetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection), None, CurrentTime);
    /* Clear any other DnD specific data on the proxy win. */
    clearFormatsWindowProperty(m_proxyWin);
    clearActionsWindowProperty(m_proxyWin);
    /* Reset the internal state. */
    m_formats.clear();
    m_curWin = 0;
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
    int rc = VINF_SUCCESS;
    do
    {
        uninit();
        /* Enough screens configured in the x11 server? */
        if ((int)u32ScreenId > ScreenCount(m_pDisplay))
        {
            rc = VERR_GENERAL_FAILURE;
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
        m_rootWin = RootWindow(m_pDisplay, m_screenId);
        if (!m_rootWin)
        {
            rc = VERR_GENERAL_FAILURE;
            break;
        }
        /* Create an invisible window which will act as proxy for the DnD
         * operation. This window will be used for both the GH and HG
         * direction. */
        XSetWindowAttributes attr;
        RT_ZERO(attr);
        attr.do_not_propagate_mask = 0;
        attr.override_redirect     = True;
//        attr.background_pixel      = WhitePixel(m_pDisplay, m_screenId);
        m_proxyWin = XCreateWindow(m_pDisplay, m_rootWin, 0, 0, 1, 1, 0,
                                   CopyFromParent, InputOnly, CopyFromParent,
                                   CWOverrideRedirect | CWDontPropagate,
                                   &attr);

//        m_proxyWin = XCreateSimpleWindow(m_pDisplay, m_rootWin, 0, 0, 50, 50, 0, WhitePixel(m_pDisplay, m_screenId), WhitePixel(m_pDisplay, m_screenId));

        if (!m_proxyWin)
        {
            rc = VERR_GENERAL_FAILURE;
            break;
        }
        /* Make the new window Xdnd aware. */
        Atom ver = VBOX_XDND_VERSION;
        XChangeProperty(m_pDisplay, m_proxyWin, xAtom(XA_XdndAware), XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&ver), 1);
    }while(0);

    m_state = Initialized;

    return rc;
}

/*
 * Host -> Guest
 */

int DragInstance::hgEnter(const RTCList<RTCString> &formats, uint32_t actions)
{
    int rc = VINF_SUCCESS;

    reset();
    DO(("DnD_ENTR: formats=%u: ", formats.size()));
#if defined(VERBOSE) && defined(DEBUG_poetzsch)
    for (size_t i = 0; i < formats.size(); ++i)
        DO(("'%s' ", formats.at(i).c_str()));
#endif /* DEBUG */
    DO(("\n"));

    m_formats = toAtomList(formats);

    /* If we have more than 3 formats we have to use the type list extension. */
    if (m_formats.size() > 3)
        setFormatsWindowProperty(m_proxyWin, xAtom(XA_XdndTypeList));

    /* Announce the possible actions */
    setActionsWindowProperty(m_proxyWin, toX11Actions(actions));

    /* Set the DnD selection owner to our window. */
    XSetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection), m_proxyWin, CurrentTime);

    m_mode  = HG;
    m_state = Dragging;

    return rc;
}

int DragInstance::hgMove(uint32_t u32xPos, uint32_t u32yPos, uint32_t action)
{
    DO(("DnD_MOVE: "));

    if (   m_mode  != HG
        || m_state != Dragging)
        return VERR_INVALID_STATE;

    int rc  = VINF_SUCCESS;
    int xrc = Success;

    /* Move the mouse cursor within the guest. */
    moveCursor(u32xPos, u32yPos);

    Window newWin = None; /* Default to _no_ window below the cursor. */
    long   newVer = -1;   /* This means the current window is _not_ XdndAware. */

    /* Search for the application window below the cursor. */
    newWin = gX11->applicationWindowBelowCursor(m_rootWin);
    if (newWin != None)
    {
        /* Temp stuff for the XGetWindowProperty call. */
        Atom atmp;
        int fmt;
        unsigned long cItems, cbRemaining;
        unsigned char *pcData = NULL;
        /* Query the XdndAware property from the window. We are interested in
         * the version and if it is XdndAware at all. */
        xrc = XGetWindowProperty(m_pDisplay, newWin, xAtom(XA_XdndAware), 0, 2, False, AnyPropertyType, &atmp, &fmt, &cItems, &cbRemaining, &pcData);
        if (RT_UNLIKELY(xrc != Success))
            DO(("DnD_MOVE: error in getting the window property (%s)\n", gX11->xErrorToString(xrc).c_str()));
        else
        {
            if (RT_UNLIKELY(pcData == NULL || fmt != 32 || cItems != 1))
                DO(("Prop=error[data=%#x,fmt=%u,items=%u] ", pcData, fmt, cItems));
            else
            {
                newVer = reinterpret_cast<long*>(pcData)[0];
                DO(("XdndAware=%u ", newVer));
            }
            XFree(pcData);
        }
    }

    if (newWin != m_curWin && m_curVer != -1)
    {
        DO(("leave=%#x ", m_curWin));

        /* We left the current XdndAware window. Announce this to the window. */

        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = m_curWin;
        m.message_type = xAtom(XA_XdndLeave);
        m.format       = 32;
        m.data.l[0]    = m_proxyWin;

        xrc = XSendEvent(m_pDisplay, m_curWin, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xrc == 0))
            DO(("DnD_MOVE: error sending xevent\n"));
    }

    if (newWin != m_curWin && newVer != -1)
    {
        DO(("enter=%#x ", newWin));

        /* We enter a new window. Announce the XdndEnter event to the new
         * window. The first three mime types are attached to the event (the
         * others could be requested by the XdndTypeList property from the
         * window itself). */

        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = newWin;
        m.message_type = xAtom(XA_XdndEnter);
        m.format       = 32;
        m.data.l[0]    = m_proxyWin;
        m.data.l[1]    = RT_MAKE_U32_FROM_U8(m_formats.size() > 3 ? 1 : 0, 0, 0, RT_MIN(VBOX_XDND_VERSION, newVer));
        m.data.l[2]    = m_formats.value(0, None);
        m.data.l[3]    = m_formats.value(1, None);
        m.data.l[4]    = m_formats.value(2, None);

        xrc = XSendEvent(m_pDisplay, newWin, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xrc == 0))
            DO(("DnD_MOVE: error sending xevent\n"));
    }

    if (newVer != -1)
    {
        DO(("move=%#x pos=%ux%u ", newWin, u32xPos, u32yPos));

        /* Send a XdndPosition event with the proposed action to the guest. */

        Atom pa = toX11Action(action);
        DO(("action='%s' ", xAtomToString(pa).c_str()));

        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = newWin;
        m.message_type = xAtom(XA_XdndPosition);
        m.format       = 32;
        m.data.l[0]    = m_proxyWin;
        m.data.l[2]    = RT_MAKE_U32(u32yPos, u32xPos);
        m.data.l[3]    = CurrentTime;
        m.data.l[4]    = pa;

        xrc = XSendEvent(m_pDisplay, newWin, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xrc == 0))
            DO(("DnD_MOVE: error sending xevent\n"));
    }
    if (newWin == None && newVer == -1)
        /* No window to process, so send a ignore ack event to the host. */
        rc = VbglR3DnDHGAcknowledgeOperation(DND_IGNORE_ACTION);

    m_curWin = newWin;
    m_curVer = RT_MIN(VBOX_XDND_VERSION, newVer);

    DO(("\n"));

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
        && m_curWin               == static_cast<Window>(e.xclient.data.l[0]))
    {
        /* The XdndStatus message tell us if the window will accept the DnD
         * event and with which action. We immediately send this info down to
         * the host as a response of a previous DnD message. */
        DO(("DnD_STAT: win=%#x,accept=%RTbool,action='%s'\n",
            e.xclient.data.l[0],
            ASMBitTest(&e.xclient.data.l[1], 0),
            xAtomToString(e.xclient.data.l[4]).c_str()));
        uint32_t uAction = DND_IGNORE_ACTION;
        /* Todo: compare this with the allowed actions. */
        if (ASMBitTest(&e.xclient.data.l[1], 0))
            uAction = toHGCMAction(static_cast<Atom>(e.xclient.data.l[4]));
        rc = VbglR3DnDHGAcknowledgeOperation(uAction);
    }
    else if (e.xclient.message_type == xAtom(XA_XdndFinished))
    {
        /* This message is send on a un/successful DnD drop request. */
        DO(("DnD_FINI: win=%#x,success=%RTbool,action='%s'\n",
            e.xclient.data.l[0],
            ASMBitTest(&e.xclient.data.l[1], 0),
            xAtomToString(e.xclient.data.l[2]).c_str()));
        reset();
    }
    else
        DO(("DnD_CLI: win=%#x,msg='%s'\n", e.xclient.data.l[0], xAtomToString(e.xclient.message_type).c_str()));
    return rc;
}

int DragInstance::hgDrop()
{
    DO(("DnD_DROP: win=%#x\n", m_curWin));

    if (   m_mode  != HG
        || m_state != Dragging)
        return VERR_INVALID_STATE;

    int rc = VINF_SUCCESS;

    /* Send a drop event to the current window and reset our DnD status. */
    XClientMessageEvent m;
    RT_ZERO(m);
    m.type         = ClientMessage;
    m.display      = m_pDisplay;
    m.window       = m_curWin;
    m.message_type = xAtom(XA_XdndDrop);
    m.format       = 32;
    m.data.l[0]    = m_proxyWin;
    m.data.l[2]    = CurrentTime;

    int xrc = XSendEvent(m_pDisplay, m_curWin, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
    if (RT_UNLIKELY(xrc == 0))
        DO(("DnD_DROP: error sending xevent\n"));

    m_curWin = None;
    m_curVer = -1;

    m_state = Dropped;

    return rc;
}

int DragInstance::hgX11SelectionRequest(const XEvent& e)
{
    AssertReturn(e.type == SelectionRequest, VERR_INVALID_PARAMETER);

    if (   m_mode  != HG)
//        || m_state != D)
        return VERR_INVALID_STATE;

    DO(("DnD_SELR: owner=%#x,requestor=%#x,sel_atom='%s',tar_atom='%s',prop_atom='%s',time=%u\n",
        e.xselectionrequest.owner,
        e.xselectionrequest.requestor,
        xAtomToString(e.xselectionrequest.selection).c_str(),
        xAtomToString(e.xselectionrequest.target).c_str(),
        xAtomToString(e.xselectionrequest.property).c_str(),
        e.xselectionrequest.time));

    int rc = VINF_SUCCESS;

    /* A window is asking for some data. Normally here the data would be copied
     * into the selection buffer and send to the requestor. Obviously we can't
     * do that, cause we first need to ask the host for the data of the
     * requested mime type. This is done and later answered with the correct
     * data (s. dataReceived). */

    /* Is the requestor asking for the possible mime types? */
    if(e.xselectionrequest.target == xAtom(XA_TARGETS))
    {
        DO(("DnD_SELR: ask for target list\n"));
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
            DO(("DnD_SELR: error sending xevent\n"));
    }
    /* Is the requestor asking for a specific mime type (we support)? */
    else if(m_formats.contains(e.xselectionrequest.target))
    {
        DO(("DnD_SELR: ask for data (format='%s')\n", xAtomToString(e.xselectionrequest.target).c_str()));
        /* If so, we need to inform the host about this request. Save the
         * selection request event for later use. */
        if (   m_state != Dropped)
            //        || m_curWin != e.xselectionrequest.requestor)
        {
            DO(("DnD_SELR: refuse\n"));
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
                DO(("DnD_SELR: error sending xevent\n"));
        }
        else
        {
            memcpy(&m_selEvent, &e, sizeof(XEvent));
            rc = VbglR3DnDHGRequestData(xAtomToString(e.xselectionrequest.target).c_str());
        }
    }
    /* Anything else. */
    else
    {
        DO(("DnD_SELR: refuse\n"));
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
            DO(("DnD_SELR: error sending xevent\n"));
    }

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

    /* The host send us the DnD data in the requested mime type. This allows us
     * to fill the XdndSelection property of the requestor window with the data
     * and afterwards inform him about the new status. */
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

    DO(("DnD_SEND: owner=%#x,requestor=%#x,sel_atom='%s',tar_atom='%s',prop_atom='%s',time=%u\n",
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
        DO(("DnD_SEND: error sending xevent\n"));

    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_DRAG_AND_DROP_GH
/*
 * Guest -> Host
 */

int DragInstance::ghIsDnDPending()
{
    int rc = VINF_SUCCESS;
    Window w = XGetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection));
    DO(("Checking pending %X %X\n", w, m_proxyWin));
    /* Is there someone own the Xdnd selection which aren't we. */
    if (   w
        && w != m_proxyWin)
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
            DO(("next X event %s\n", gX11->xAtomToString(clme->message_type).c_str()));
            if (clme->message_type == xAtom(XA_XdndEnter))
            {
                Atom type = None;
                int f;
                unsigned long n, a;
                unsigned char *ret = 0;
                reset();

                m_formats.clear();
                m_actions.clear();
                m_curWin = w;
                DO(("XA_XdndEnter\n"));
                /* Check if the mime types are in the msg itself or if we need
                 * to fetch the XdndTypeList property from the window. */
                if (!ASMBitTest(&clme->data.l[1], 0))
                {
                    for (int i = 2; i < 5; ++i)
                    {
                        DO(("receive list msg: %s\n", gX11->xAtomToString(clme->data.l[i]).c_str()));
                        m_formats.append(clme->data.l[i]);
                    }
                }
                else
                {
                    xrc = XGetWindowProperty(m_pDisplay, w, xAtom(XA_XdndTypeList), 0, VBOX_MAX_XPROPERTIES, False, XA_ATOM, &type, &f, &n, &a, &ret);
                    if (   xrc == Success
                        && n > 0
                        && ret)
                    {
                        Atom *data = reinterpret_cast<Atom*>(ret);
                        for (int i = 0; i < RT_MIN(VBOX_MAX_XPROPERTIES, n); ++i)
                        {
                            DO(("receive list: %s\n", gX11->xAtomToString(data[i]).c_str()));
                            m_formats.append(data[i]);
                        }
                        XFree(ret);
                    }
                }
                /* Fetch the possible list of actions, if this property is set. */
                xrc = XGetWindowProperty(m_pDisplay, w, xAtom(XA_XdndActionList), 0, VBOX_MAX_XPROPERTIES, False, XA_ATOM, &type, &f, &n, &a, &ret);
                if (   xrc == Success
                    && n > 0
                    && ret)
                {
                    Atom *data = reinterpret_cast<Atom*>(ret);
                    for (int i = 0; i < RT_MIN(VBOX_MAX_XPROPERTIES, n); ++i)
                    {
                        DO(("receive actions: %s\n", gX11->xAtomToString(data[i]).c_str()));
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
                m.data.l[0]    = m_proxyWin;
                m.data.l[1]    = 1;
                m.data.l[4]    = xAtom(XA_XdndActionCopy);
                xrc = XSendEvent(m_pDisplay, clme->data.l[0], False, 0, reinterpret_cast<XEvent*>(&m));
                if (RT_UNLIKELY(xrc == 0))
                    DO(("DnD_PNDG: error sending xevent\n"));
            }
            else if (clme->message_type == xAtom(XA_XdndPosition))
            {
                DO(("XA_XdndPosition\n"));
                XClientMessageEvent m;
                RT_ZERO(m);
                m.type         = ClientMessage;
                m.display      = m_pDisplay;
                m.window       = clme->data.l[0];
                m.message_type = xAtom(XA_XdndStatus);
                m.format       = 32;
                m.data.l[0]    = m_proxyWin;
                m.data.l[1]    = 1;
                m.data.l[4]    = clme->data.l[4];
                xrc = XSendEvent(m_pDisplay, clme->data.l[0], False, 0, reinterpret_cast<XEvent*>(&m));
                if (RT_UNLIKELY(xrc == 0))
                    DO(("DnD_PNDG: error sending xevent\n"));
            }
            else if (clme->message_type == xAtom(XA_XdndLeave))
            {
            }
        }
        hideProxyWin();

        rc = VbglR3DnDGHAcknowledgePending(DND_COPY_ACTION, toHGCMActions(m_actions), gX11->xAtomListToString(m_formats).c_str());
    }
    return rc;
}

int DragInstance::ghDropped(const RTCString &strFormat, uint32_t action)
{
    DO(("DND_DRO: format='%s' action=%d\n", strFormat.c_str(), action));
    int rc = VINF_SUCCESS;

    /* Show the proxy window, so that the source will find it. */
    int rx, ry;
    showProxyWin(rx, ry);
    XFlush(m_pDisplay);
    /* We send a fake release event to the current window, cause
     * this should have the grab. */
    sendButtonEvent(m_curWin, rx, ry, 1, false);
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
    }while(tries--);
    if (clme)
    {
        /* Make some paranoid checks. */
        if (clme->message_type == xAtom(XA_XdndDrop))
        {
            /* Request to convert the selection in the specific format and
             * place it to our proxy window as property. */
            Window srcWin = m_curWin;//clme->data.l[0];
            Atom aFormat  = gX11->stringToxAtom(strFormat.c_str());
            XConvertSelection(m_pDisplay, xAtom(XA_XdndSelection), aFormat, xAtom(XA_XdndSelection), m_proxyWin, clme->data.l[2]);
            /* Wait for the selection notify event. */
            RT_ZERO(e);
            if (m_pParent->waitForXMsg(e, SelectionNotify))
            {
                /* Make some paranoid checks. */
                if (   e.xselection.type      == SelectionNotify
                    && e.xselection.display   == m_pDisplay
                    && e.xselection.selection == xAtom(XA_XdndSelection)
                    && e.xselection.requestor == m_proxyWin
                    && e.xselection.target    == aFormat)
                {
                    DO(("DND_DRO: selection notfiy (from: %x)\n", m_curWin));
                    Atom type;
                    int format;
                    unsigned long cItems, cbRemaining;
                    unsigned char *ucData = 0;
                    XGetWindowProperty(m_pDisplay, m_proxyWin, xAtom(XA_XdndSelection),
                                       0, VBOX_MAX_XPROPERTIES, True, AnyPropertyType,
                                       &type, &format, &cItems, &cbRemaining, &ucData);
                    DO(("DND_DRO: %s %d %d %s\n", gX11->xAtomToString(type).c_str(), cItems, format, ucData));
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
                            DO(("rebuild %u\n", cbData));
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

                        DO(("send responce\n"));
                        /* Confirm the result of the transfer to the source window. */
                        XClientMessageEvent m;
                        RT_ZERO(m);
                        m.type         = ClientMessage;
                        m.display      = m_pDisplay;
                        m.window       = srcWin;
                        m.message_type = xAtom(XA_XdndFinished);
                        m.format       = 32;
                        m.data.l[0]    = m_proxyWin;
                        m.data.l[1]    = RT_SUCCESS(rc) ?                   1 : 0;    /* Confirm or deny success */
                        m.data.l[2]    = RT_SUCCESS(rc) ? toX11Action(action) : None; /* Action used on success */

                        int xrc = XSendEvent(m_pDisplay, srcWin, True, NoEventMask, reinterpret_cast<XEvent*>(&m));
                        if (RT_UNLIKELY(xrc == 0))
                            DO(("DnD_DRO: error sending xevent\n"));
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
                        m.data.l[0]    = m_proxyWin;
                        m.data.l[1]    = 0;
                        m.data.l[2]    = None;
                        int xrc = XSendEvent(m_pDisplay, srcWin, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
                        if (RT_UNLIKELY(xrc == 0))
                            DO(("DnD_DRO: error sending xevent\n"));
                        m_curWin = 0;
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
    XWarpPointer(m_pDisplay, None, m_rootWin, 0, 0, 0, 0, u32xPos, u32yPos);
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
        be.root         = m_rootWin;
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
            DO(("DnD_BTN: error sending xevent\n"));
    }

}

void DragInstance::showProxyWin(int &rx, int &ry) const
{
    int cx, cy;
    unsigned int m;
    Window r, c;
//    XTestGrabControl(m_pDisplay, False);
    XQueryPointer(m_pDisplay, m_rootWin, &r, &c, &rx, &ry, &cx, &cy, &m);
    XSynchronize(m_pDisplay, True);
    XMapWindow(m_pDisplay, m_proxyWin);
    XRaiseWindow(m_pDisplay, m_proxyWin);
    XMoveResizeWindow(m_pDisplay, m_proxyWin, rx, ry, 1, 1);
    XWarpPointer(m_pDisplay, None, m_rootWin, 0, 0, 0, 0, rx , ry);
    XSynchronize(m_pDisplay, False);
//    XTestGrabControl(m_pDisplay, True);
}

void DragInstance::hideProxyWin() const
{
    XUnmapWindow(m_pDisplay, m_proxyWin);
}

/* Currently, not used */
void DragInstance::registerForEvents(Window w) const
{
//    if (w == m_proxyWin)
//        return;

    DO(("%x\n", w));
//    XSelectInput(m_pDisplay, w, Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask);//| SubstructureNotifyMask);
//    XSelectInput(m_pDisplay, w, ButtonMotionMask); //PointerMotionMask);
    XSelectInput(m_pDisplay, w, PointerMotionMask); //PointerMotionMask);
    Window hRealRoot, hParent;
    Window *phChildrenRaw = NULL;
    unsigned cChildren;
    if (XQueryTree(m_pDisplay, w, &hRealRoot, &hParent, &phChildrenRaw, &cChildren))
    {
        for (unsigned i = 0; i < cChildren; ++i)
            registerForEvents(phChildrenRaw[i]);
        XFree(phChildrenRaw);
    }
}

void DragInstance::setActionsWindowProperty(Window win, const RTCList<Atom> &actionList) const
{
    if (actionList.isEmpty())
        return;

    XChangeProperty(m_pDisplay, win, xAtom(XA_XdndActionList), XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(actionList.raw()), actionList.size());
}

void DragInstance::clearActionsWindowProperty(Window win) const
{
    XDeleteProperty(m_pDisplay, win, xAtom(XA_XdndActionList));
}

void DragInstance::setFormatsWindowProperty(Window win, Atom property) const
{
    if (m_formats.isEmpty())
        return;

    /* We support TARGETS and the data types. */
    RTCList<Atom> targets(m_formats.size() + 1);
    targets.append(xAtom(XA_TARGETS));
    targets.append(m_formats);

    /* Add the property with the property data to the window. */
    XChangeProperty(m_pDisplay, win, property, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(targets.raw()), targets.size());
}

void DragInstance::clearFormatsWindowProperty(Window win) const
{
    XDeleteProperty(m_pDisplay, win, xAtom(XA_XdndTypeList));
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
        DO(("f: %s\n", pszTmp));
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

bool DragAndDropService::waitForXMsg(XEvent &ecm, int type, uint32_t uiMaxMS /* = 100 */)
{
    const uint64_t uiStart = RTTimeProgramMilliTS();
    do
    {
        if (!m_eventQueue.isEmpty())
        {
            DO(("new msg size %d\n", m_eventQueue.size()));
            /* Check if there is a client message in the queue. */
            for (size_t i = 0; i < m_eventQueue.size(); ++i)
            {
                DnDEvent e = m_eventQueue.at(i);
                if(   e.type     == DnDEvent::X11_Type)
                    DO(("new msg\n"));
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
    while(RTTimeProgramMilliTS() - uiStart < uiMaxMS);

    return false;
}

void DragAndDropService::clearEventQueue()
{
    m_eventQueue.clear();
}

int DragAndDropService::run(bool fDaemonised /* = false */)
{
    int rc = VINF_SUCCESS;
    LogRelFlowFunc(("\n"));

    /* We need to initialize XLib with thread support, otherwise our
     * simultaneously access to the display makes trouble (has to be called
     * before any usage of XLib). */
    if (!XInitThreads())
        AssertMsgFailedReturn(("Failed to initialize thread-safe XLib.\n"), VERR_GENERAL_FAILURE);

    do
    {
        /* Initialize our service */
        rc = VbglR3DnDInit();
        if (RT_FAILURE(rc))
            break;

        /* Initialize X11 DND */
        rc = x11DragAndDropInit();
        if (RT_FAILURE(rc))
            break;

        m_pCurDnD = new DragInstance(m_pDisplay, this);
        /* Note: For multiple screen support in VBox it is not necessary to use
         * another screen number than zero. Maybe in the future it will become
         * necessary if VBox supports multiple X11 screens. */
        m_pCurDnD->init(0);
        /* Loop over new events */
        do
        {
            DnDEvent e;
            RT_ZERO(e);
            if (m_eventQueue.isEmpty())
                rc = RTSemEventWait(m_hEventSem, 50);
            if (!m_eventQueue.isEmpty())
            {
                e = m_eventQueue.first();
                m_eventQueue.removeFirst();
                DO(("new msg %d\n", e.type));
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
                    }

                }
                else if(e.type == DnDEvent::X11_Type)
                {
                    DO(("X11 type: %u\n", e.x11.type));
                    /* Now the X11 event stuff */
                    switch (e.x11.type)
                    {
                        case SelectionRequest: m_pCurDnD->hgX11SelectionRequest(e.x11); break;
                        case ClientMessage:    m_pCurDnD->hgX11ClientMessage(e.x11); break;
                        case SelectionClear:   DO(("DnD_CLER\n")); break;
//                      case MotionNotify: m_pCurDnD->hide(); break;
                    }
                }
            }
        }while(!ASMAtomicReadBool(&m_fSrvStopping));
    }while(0);

    LogRelFlowFunc(("returning %Rrc\n", rc));
    return rc;
}

int DragAndDropService::x11DragAndDropInit()
{
    /* Connect to the x11 server. */
    m_pDisplay = XOpenDisplay(NULL);
    if (!m_pDisplay)
        /* todo: correct errors */
        return VERR_NOT_FOUND;

    xHelpers::instance(m_pDisplay);

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
    }while(0);

    /* Cleanup on failure */
    if (RT_FAILURE(rc))
        x11DragAndDropTerm();

    return rc;
}

int DragAndDropService::x11DragAndDropTerm()
{
    /* Mark that we are stopping. */
    ASMAtomicWriteBool(&m_fSrvStopping, true);

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
        DO(("DnD_TERM: error sending xevent\n"));
    /* Wait for our event threads to stop. */
//    if (m_hX11Thread)
//        RTThreadWait(m_hX11Thread, RT_INDEFINITE_WAIT, 0);
//    if (m_hHGCMThread)
//        RTThreadWait(m_hHGCMThread, RT_INDEFINITE_WAIT, 0);
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
    DragAndDropService *pSrv = static_cast<DragAndDropService*>(pvUser);
    DnDEvent e;
    do
    {
        RT_ZERO(e);
        e.type = DnDEvent::HGCM_Type;
        /* Wait for new events */
        int rc = VbglR3DnDProcessNextMessage(&e.hgcm);
        if (RT_SUCCESS(rc))
        {
            pSrv->m_eventQueue.append(e);
            rc = RTSemEventSignal(pSrv->m_hEventSem);
            if (RT_FAILURE(rc))
                return rc;
        }
    }while(!ASMAtomicReadBool(&pSrv->m_fSrvStopping));

    return VINF_SUCCESS;
}

/* static */
int DragAndDropService::x11EventThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    DragAndDropService *pSrv = static_cast<DragAndDropService*>(pvUser);
    DnDEvent e;
    do
    {
        /* Wait for new events. We can't use XIfEvent here, cause this locks
         * the window connection with a mutex and if no X11 events occurs this
         * blocks any other calls we made to X11. So instead check for new
         * events and if there are not any new one, sleep for a certain amount
         * of time. */
        if (XEventsQueued(pSrv->m_pDisplay, QueuedAfterFlush) > 0)
        {
            RT_ZERO(e);
            e.type = DnDEvent::X11_Type;
            XNextEvent(pSrv->m_pDisplay, &e.x11);
            /* Check for a stop message. */
//            if (   e.x11.type == ClientMessage
//                && e.x11.xclient.message_type == xAtom(XA_dndstop))
//            {
//                break;
//            }
//            if (isDnDRespondEvent(pSrv->m_pDisplay, &e.x11, 0))
            {
                /* Appending makes a copy of the event structure. */
                pSrv->m_eventQueue.append(e);
                int rc = RTSemEventSignal(pSrv->m_hEventSem);
                if (RT_FAILURE(rc))
                    return rc;
            }
        }
        else
            RTThreadSleep(25);
    } while(!ASMAtomicReadBool(&pSrv->m_fSrvStopping));

    return VINF_SUCCESS;
}

/* Static factory */
VBoxClient::Service *VBoxClient::GetDragAndDropService()
{
    return new(DragAndDropService);
}
