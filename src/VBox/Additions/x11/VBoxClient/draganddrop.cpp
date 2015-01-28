/** @file
 * X11 guest client - Drag and Drop.
 */

/*
 * Copyright (C) 2011-2014 Oracle Corporation
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
#ifdef VBOX_DND_WITH_XTEST
# include <X11/extensions/XTest.h>
#endif

#include <iprt/asm.h>
#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <iprt/cpp/mtlist.h>
#include <iprt/cpp/ministring.h>

#include <limits.h>

# ifdef LOG_GROUP
 # undef LOG_GROUP
# endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>

#include "VBox/HostServices/DragAndDropSvc.h"

#include "VBoxClient.h"

/* Enable this define to see the proxy window(s) when debugging
 * their behavior. Don't have this enabled in release builds! */
#ifdef DEBUG
//# define VBOX_DND_DEBUG_WND
#endif

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
 * info of the MIME types and allowed actions and send this back to the host.
 * On a drop request from the host, we query for the selection and should get
 * the data in the specified mime-type. This data is send back to the host.
 * After that we send a XdndLeave event to the source window.
 *
 * To-do:
 * - Cancelling (e.g. with ESC key) doesn't work.
 *
 * To-do:
 * - INCR (incremental transfers) support.
 * - Make this much more robust for crashes of the other party.
 * - Really check for the Xdnd version and the supported features.
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
        Uninitialized = 0,
        Initialized,
        Dragging,
        Dropped
    };

    enum Mode
    {
        Unknown = 0,
        HG,
        GH
    };

    DragInstance(Display *pDisplay, DragAndDropService *pParent);

public:

    int  init(uint32_t u32ScreenId);
    void uninit(void);
    void reset(void);

    /* X11 message processing. */
    int onX11ClientMessage(const XEvent &e);
    int onX11SelectionNotify(const XEvent &e);
    int onX11SelectionRequest(const XEvent &e);
    int onX11Event(const XEvent &e);
    bool waitForX11Msg(XEvent &evX, int iType, RTMSINTERVAL uTimeoutMS = 100);
    bool waitForX11ClientMsg(XClientMessageEvent &evMsg, Atom aType, RTMSINTERVAL uTimeoutMS = 100);

    /* H->G */
    int  hgEnter(const RTCList<RTCString> &formats, uint32_t actions);
    int  hgMove(uint32_t u32xPos, uint32_t u32yPos, uint32_t action);
    int  hgDrop();
    int  hgDataReceived(void *pvData, uint32_t cData);

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
    /* G->H */
    int  ghIsDnDPending();
    int  ghDropped(const RTCString &strFormat, uint32_t action);
#endif

    /* X11 helpers. */
    int  mouseCursorMove(int iPosX, int iPosY) const;
    void mouseButtonSet(Window wndDest, int rx, int ry, int iButton, bool fPress) const;
    int proxyWinShow(int *piRootX = NULL, int *piRootY = NULL, bool fMouseMove = false) const;
    int proxyWinHide(void);
    void registerForEvents(Window w) const;

    void setActionsWindowProperty(Window wndThis, const RTCList<Atom> &lstActions) const;
    void clearActionsWindowProperty(Window wndThis) const;
    void setFormatsWindowProperty(Window wndThis, Atom property) const;
    void clearFormatsWindowProperty(Window wndThis) const;

    RTCList<Atom>        toAtomList(const RTCList<RTCString> &lstFormats) const;
    RTCList<Atom>        toAtomList(void *pvData, uint32_t cbData) const;
    static Atom          toX11Action(uint32_t uAction);
    static RTCList<Atom> toX11Actions(uint32_t uActions);
    static uint32_t      toHGCMAction(Atom atom);
    static uint32_t      toHGCMActions(const RTCList<Atom> &actionsList);

protected:

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
    /** Deferred host to guest selection event for sending to the
     *  target window as soon as data from the host arrived. */
    XEvent              m_eventHgSelection;
    /** Current operation mode. */
    Mode                m_mode;
    /** Current state of operation mode. */
    State               m_state;
    /** The instance's own X event queue. */
    RTCMTList<XEvent>   m_eventQueue;
    /** Critical section for providing serialized access to list
     *  event queue's contents. */
    RTCRITSECT          m_eventQueueCS;
    /** Event for notifying this instance in case of a new
     *  event. */
    RTSEMEVENT          m_hEventSem;
    /** List of allowed formats. */
    RTCList<RTCString>  m_lstAllowedFormats;
};

/*******************************************************************************
 *
 * DragAndDropService Declaration
 *
 ******************************************************************************/

class DragAndDropService
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

    int run(bool fDaemonised = false);

private:
    int x11DragAndDropInit(void);
    static int hgcmEventThread(RTTHREAD hThread, void *pvUser);
    static int x11EventThread(RTTHREAD hThread, void *pvUser);

    void clearEventQueue();

    /* Usually XCheckMaskEvent could be used for querying selected x11 events.
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

    /** Our (thread-safe) event queue with
     *  mixed events (DnD HGCM / X11). */
    RTCMTList<DnDEvent>    m_eventQueue;
    /** Critical section for providing serialized access to list
     *  event queue's contents. */
    RTCRITSECT           m_eventQueueCS;
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
    proxyWinHide();

    /* If we are currently the Xdnd selection owner, clear that. */
    Window w = XGetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection));
    if (w == m_wndProxy)
        XSetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection), None, CurrentTime);

    /* Clear any other DnD specific data on the proxy win. */
    clearFormatsWindowProperty(m_wndProxy);
    clearActionsWindowProperty(m_wndProxy);

    /* Reset the internal state. */
    m_actions.clear();
    m_formats.clear();
    m_wndCur = 0;
    m_curVer = -1;
    m_state  = Initialized;
    m_eventQueue.clear();
}

int DragInstance::init(uint32_t u32ScreenId)
{
    int rc;

    do
    {
        uninit();

        rc = VbglR3DnDConnect(&m_uClientID);
        if (RT_FAILURE(rc))
            break;

        rc = RTSemEventCreate(&m_hEventSem);
        if (RT_FAILURE(rc))
            break;

        rc = RTCritSectInit(&m_eventQueueCS);
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
        attr.event_mask            =   EnterWindowMask  | LeaveWindowMask
                                     | ButtonMotionMask | ButtonPressMask | ButtonReleaseMask;
        attr.do_not_propagate_mask = NoEventMask;
        attr.override_redirect     = True;
#ifdef VBOX_DND_DEBUG_WND
        attr.background_pixel      = WhitePixel(m_pDisplay, m_screenId);
#endif
        m_wndProxy = XCreateWindow(m_pDisplay, m_wndRoot                 /* Parent */,
                                   0, 0,                                 /* Position */
                                   1, 1,                                 /* Width + height */
                                   0,                                    /* Border width */
                                   CopyFromParent,                       /* Depth */
                                   InputOnly,                            /* Class */
                                   CopyFromParent,                       /* Visual */
                                   CWOverrideRedirect | CWDontPropagate, /* Value mask */
                                   &attr                                 /* Attributes for value mask */);
        if (!m_wndProxy)
        {
            rc = VERR_GENERAL_FAILURE;
            break;
        }

        LogFlowThisFunc(("Created proxy window 0x%x at m_wndRoot=0x%x ...\n",
                         m_wndProxy, m_wndRoot));

        /* Set the window's name for easier lookup. */
        XStoreName(m_pDisplay, m_wndProxy, "VBoxClientWndDnD");

        /* Make the new window Xdnd aware. */
        Atom ver = VBOX_XDND_VERSION;
        XChangeProperty(m_pDisplay, m_wndProxy, xAtom(XA_XdndAware), XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&ver), 1);
    } while (0);

    if (RT_SUCCESS(rc))
    {
        m_state = Initialized;
    }
    else
        LogRel(("DnD: Initializing drag instance for screen %RU32 failed with rc=%Rrc\n",
                u32ScreenId, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragInstance::onX11ClientMessage(const XEvent &e)
{
    AssertReturn(e.type == ClientMessage, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("m_mode=%d, m_state=%d\n", m_mode, m_state));
    LogFlowThisFunc(("Event wnd=%#x, msg=%s\n",
                     e.xclient.window,
                     xAtomToString(e.xclient.message_type).c_str()));
    int rc;

    switch (m_mode)
    {
        case HG:
        {
            /* Client messages are used to inform us about the status of a XdndAware
             * window, in response of some events we send to them. */
            if (   e.xclient.message_type == xAtom(XA_XdndStatus)
                && m_wndCur               == static_cast<Window>(e.xclient.data.l[0]))
            {
                /* The XdndStatus message tell us if the window will accept the DnD
                 * event and with which action. We immediately send this info down to
                 * the host as a response of a previous DnD message. */
                LogFlowThisFunc(("XA_XdndStatus wnd=%#x, accept=%RTbool, action=%s\n",
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
                rc = VINF_SUCCESS;

                /* This message is send on a un/successful DnD drop request. */
                LogFlowThisFunc(("XA_XdndFinished: wnd=%#x, success=%RTbool, action=%s\n",
                                 e.xclient.data.l[0],
                                 ASMBitTest(&e.xclient.data.l[1], 0),
                                 xAtomToString(e.xclient.data.l[2]).c_str()));
                reset();
            }
            else
            {
                LogFlowThisFunc(("Unhandled: wnd=%#x, msg=%s\n",
                                 e.xclient.data.l[0], xAtomToString(e.xclient.message_type).c_str()));
                rc = VERR_NOT_SUPPORTED;
            }

            break;
        }

        case GH:
        {
            LogFlowThisFunc(("Enqueuing ClientMessage\n"));

            m_eventQueue.append(e);
            rc = RTSemEventSignal(m_hEventSem);
            break;
        }

        default:
        {
            LogFlowThisFunc(("Unhandled: wnd=%#x, msg=%s\n",
                             e.xclient.data.l[0], xAtomToString(e.xclient.message_type).c_str()));
            rc = VERR_INVALID_STATE;
            break;
        }
    }

    LogFlowThisFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}

int DragInstance::onX11SelectionNotify(const XEvent &e)
{
    AssertReturn(e.type == SelectionNotify, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("m_mode=%d, m_state=%d\n", m_mode, m_state));

    int rc = VINF_SUCCESS;

    switch (m_mode)
    {
        case GH:
        {
            if (m_state == Dropped)
            {
                LogFlowThisFunc(("Enqueuing SelectionNotify\n"));

                m_eventQueue.append(e);
                rc = RTSemEventSignal(m_hEventSem);
            }
            break;
        }

        default:
        {
            LogFlowThisFunc(("Unhandled: wnd=%#x, msg=%s\n",
                             e.xclient.data.l[0], xAtomToString(e.xclient.message_type).c_str()));
            rc = VERR_INVALID_STATE;
            break;
        }
    }

    LogFlowThisFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}


int DragInstance::onX11SelectionRequest(const XEvent &e)
{
    AssertReturn(e.type == SelectionRequest, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("m_mode=%d, m_state=%d\n", m_mode, m_state));
    LogFlowThisFunc(("Event owner=%#x, requestor=%#x, selection=%s, target=%s, prop=%s, time=%u\n",
                     e.xselectionrequest.owner,
                     e.xselectionrequest.requestor,
                     xAtomToString(e.xselectionrequest.selection).c_str(),
                     xAtomToString(e.xselectionrequest.target).c_str(),
                     xAtomToString(e.xselectionrequest.property).c_str(),
                     e.xselectionrequest.time));
    int rc;

    switch (m_mode)
    {
        case HG:
        {
            rc = VINF_SUCCESS;

            /*
             * A window is asking for some data. Normally here the data would be copied
             * into the selection buffer and send to the requestor. Obviously we can't
             * do that, because we first need to ask the host for the data of the
             * requested MIME type. This is done and later answered with the correct
             * data -- see DragInstance::hgDataReceived().
             */

            /* Is the requestor asking for the possible MIME types? */
            if (e.xselectionrequest.target == xAtom(XA_TARGETS))
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
            /* Is the requestor asking for a specific MIME type (we support)? */
            else if (m_formats.contains(e.xselectionrequest.target))
            {
                LogFlowThisFunc(("wnd=%#x asking for data, format=%s\n",
                                 e.xselectionrequest.requestor, xAtomToString(e.xselectionrequest.target).c_str()));

                /* If so, we need to inform the host about this request. Save the
                 * selection request event for later use. */
                if (   m_state != Dropped)
                    //        || m_curWin != e.xselectionrequest.requestor)
                {
                    LogFlowThisFunc(("Wrong state, refusing request\n"));

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
                    LogFlowThisFunc(("Saving selection notify message\n"));

                    memcpy(&m_eventHgSelection, &e, sizeof(XEvent));

                    RTCString strFormat = xAtomToString(e.xselectionrequest.target);
                    Assert(strFormat.isNotEmpty());
                    rc = VbglR3DnDHGRequestData(m_uClientID, strFormat.c_str());
                    LogFlowThisFunc(("Requesting data from host as \"%s\", rc=%Rrc\n",
                                     strFormat.c_str(), rc));
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

            break;
        }

        default:
        {
            LogFlowThisFunc(("Unhandled: wnd=%#x, msg=%s\n",
                             e.xclient.data.l[0], xAtomToString(e.xclient.message_type).c_str()));
            rc = VERR_INVALID_STATE;
            break;
        }
    }

    LogFlowThisFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}

int DragInstance::onX11Event(const XEvent &e)
{
    int rc;

    LogFlowThisFunc(("X11 event, type=%d\n", e.type));
    switch (e.type)
    {
        case ButtonPress:
            LogFlowThisFunc(("ButtonPress\n"));
            rc = VINF_SUCCESS;
            break;

        case ButtonRelease:
            LogFlowThisFunc(("ButtonRelease\n"));
            rc = VINF_SUCCESS;
            break;

        case ClientMessage:
            rc = onX11ClientMessage(e);
            break;

        case SelectionClear:
           LogFlowThisFunc(("SelectionClear\n"));
           reset();
           rc = VINF_SUCCESS;
           break;

        case SelectionNotify:
            rc = onX11SelectionNotify(e);
            break;

        case SelectionRequest:
            rc = onX11SelectionRequest(e);
            break;

        /*case MotionNotify:
          hide();
          break;*/

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    LogFlowThisFunc(("rc=%Rrc\n", rc));
    return rc;
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
bool DragInstance::waitForX11Msg(XEvent &evX, int iType,
                                 RTMSINTERVAL uTimeoutMS /* = 100 */)
{
    LogFlowThisFunc(("iType=%d, uTimeoutMS=%RU32, cEventQueue=%zu\n",
                     iType, uTimeoutMS, m_eventQueue.size()));

    bool fFound = false;
    const uint64_t uiStart = RTTimeMilliTS();
    do
    {
        /* Check if there is a client message in the queue. */
        for (size_t i = 0; i < m_eventQueue.size(); i++)
        {
            int rc2 = RTCritSectEnter(&m_eventQueueCS);
            if (RT_SUCCESS(rc2))
            {
                XEvent e = m_eventQueue.at(i);

                fFound = e.type == iType;
                if (fFound)
                {
                    m_eventQueue.removeAt(i);
                    evX = e;
                }

                rc2 = RTCritSectLeave(&m_eventQueueCS);
                AssertRC(rc2);

                if (fFound)
                    break;
            }
        }

        if (fFound)
            break;

        int rc2 = RTSemEventWait(m_hEventSem, 25 /* ms */);
        if (   RT_FAILURE(rc2)
            && rc2 != VERR_TIMEOUT)
        {
            LogFlowFunc(("Waiting failed with rc=%Rrc\n", rc2));
            break;
        }
    }
    while (RTTimeMilliTS() - uiStart < uTimeoutMS);

    LogFlowThisFunc(("Returning fFound=%RTbool, msRuntime=%RU64\n",
                     fFound, RTTimeMilliTS() - uiStart));
    return fFound;
}

bool DragInstance::waitForX11ClientMsg(XClientMessageEvent &evMsg, Atom aType,
                                       RTMSINTERVAL uTimeoutMS /*= 100 */)
{
    LogFlowThisFunc(("aType=%s, uTimeoutMS=%RU32, cEventQueue=%zu\n",
                     xAtomToString(aType).c_str(), uTimeoutMS, m_eventQueue.size()));

    bool fFound = false;
    const uint64_t uiStart = RTTimeMilliTS();
    do
    {
        /* Check if there is a client message in the queue. */
        for (size_t i = 0; i < m_eventQueue.size(); i++)
        {
            int rc2 = RTCritSectEnter(&m_eventQueueCS);
            if (RT_SUCCESS(rc2))
            {
                XEvent e = m_eventQueue.at(i);
                if (e.type == ClientMessage)
                {
                    m_eventQueue.removeAt(i);
                    evMsg = e.xclient;

                    fFound = true;
                }

                rc2 = RTCritSectLeave(&m_eventQueueCS);
                AssertRC(rc2);

                if (fFound)
                    break;
            }
        }

        if (fFound)
            break;

        int rc2 = RTSemEventWait(m_hEventSem, 25 /* ms */);
        if (   RT_FAILURE(rc2)
            && rc2 != VERR_TIMEOUT)
        {
            LogFlowFunc(("Waiting failed with rc=%Rrc\n", rc2));
            break;
        }
    }
    while (RTTimeMilliTS() - uiStart < uTimeoutMS);

    LogFlowThisFunc(("Returning fFound=%RTbool, msRuntime=%RU64\n",
                     fFound, RTTimeMilliTS() - uiStart));
    return fFound;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

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
    mouseCursorMove(u32xPos, u32yPos);

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
    if (   wndCursor != m_wndCur
        && m_curVer != -1)
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
        LogFlowThisFunc(("strAction=%s\n", xAtomToString(pa).c_str()));

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

    if (   wndCursor == None
        && newVer    == -1)
    {
        /* No window to process, so send a ignore ack event to the host. */
        rc = VbglR3DnDHGAcknowledgeOperation(m_uClientID, DND_IGNORE_ACTION);
    }

    m_wndCur = wndCursor;
    m_curVer = RT_MIN(VBOX_XDND_VERSION, newVer);

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
     * and afterwards inform the host about the new status.
     */
    XEvent s;
    RT_ZERO(s);
    s.xselection.type      = SelectionNotify;
    s.xselection.display   = m_eventHgSelection.xselection.display;
//    s.xselection.owner     = m_selEvent.xselectionrequest.owner;
    s.xselection.time      = m_eventHgSelection.xselectionrequest.time;
    s.xselection.selection = m_eventHgSelection.xselectionrequest.selection;
    s.xselection.requestor = m_eventHgSelection.xselectionrequest.requestor;
    s.xselection.target    = m_eventHgSelection.xselectionrequest.target;
    s.xselection.property  = m_eventHgSelection.xselectionrequest.property;

    LogFlowThisFunc(("owner=%#x, requestor=%#x, sel_atom=%s, target_atom=%s, prop_atom=%s, time=%u\n",
                     m_eventHgSelection.xselectionrequest.owner,
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
int DragInstance::ghIsDnDPending(void)
{
    int rc = VINF_SUCCESS;
    Window wndSelection = XGetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection));
    LogFlowThisFunc(("Checking pending wndSelection=%#x, wndProxy=%#x\n", wndSelection, m_wndProxy));

    /* Is this another window which has a Xdnd selection than our current one? */
    if (   wndSelection
        && wndSelection != m_wndProxy)
    {
        m_mode  = GH;

        /* Map the window on the current cursor position, which should provoke
         * an XdndEnter event. */
        proxyWinShow();

        XEvent e;
        if (waitForX11Msg(e, ClientMessage))
        {
            int xRc;
            XClientMessageEvent *pEventClient = reinterpret_cast<XClientMessageEvent*>(&e);
            AssertPtr(pEventClient);

            LogFlowThisFunc(("Received event=%s\n",
                             gX11->xAtomToString(pEventClient->message_type).c_str()));

            if (pEventClient->message_type == xAtom(XA_XdndEnter))
            {
                Atom type = None;
                int f;
                unsigned long n, a;
                unsigned char *ret = 0;

                reset();

                m_state = Dragging;
                m_wndCur = wndSelection;
                Assert(m_wndCur == (Window)pEventClient->data.l[0]);

                LogFlowThisFunc(("XA_XdndEnter\n"));
#ifdef DEBUG
                XWindowAttributes xwa;
                XGetWindowAttributes(m_pDisplay, m_wndCur, &xwa);
                LogFlowThisFunc(("m_wndCur=%#x, x=%d, y=%d, width=%d, height=%d\n",
                                 m_wndCur, xwa.x, xwa.y, xwa.width, xwa.height));
#endif
                /* Check if the MIME types are in the message itself or if we need
                 * to fetch the XdndTypeList property from the window. */
                if (!ASMBitTest(&pEventClient->data.l[1], 0))
                {
                    for (int i = 2; i < 5; ++i)
                    {
                        LogFlowThisFunc(("Received format via message: %s\n",
                                         gX11->xAtomToString(pEventClient->data.l[i]).c_str()));

                        m_formats.append(pEventClient->data.l[i]);
                    }
                }
                else
                {
                    xRc = XGetWindowProperty(m_pDisplay, wndSelection,
                                             xAtom(XA_XdndTypeList),
                                             0, VBOX_MAX_XPROPERTIES,
                                             False, XA_ATOM, &type, &f, &n, &a, &ret);
                    if (   xRc == Success
                        && n > 0
                        && ret)
                    {
                        Atom *data = reinterpret_cast<Atom*>(ret);
                        for (unsigned i = 0; i < RT_MIN(VBOX_MAX_XPROPERTIES, n); ++i)
                        {
                            LogFlowThisFunc(("Received format via XdndTypeList: %s\n",
                                             gX11->xAtomToString(data[i]).c_str()));

                            m_formats.append(data[i]);
                        }

                        XFree(ret);
                    }
                }

                /* Fetch the possible list of actions, if this property is set. */
                xRc = XGetWindowProperty(m_pDisplay, wndSelection,
                                         xAtom(XA_XdndActionList),
                                         0, VBOX_MAX_XPROPERTIES,
                                         False, XA_ATOM, &type, &f, &n, &a, &ret);
                if (   xRc == Success
                    && n > 0
                    && ret)
                {
                    Atom *data = reinterpret_cast<Atom*>(ret);
                    for (unsigned i = 0; i < RT_MIN(VBOX_MAX_XPROPERTIES, n); ++i)
                    {
                        LogFlowThisFunc(("Received action: %s\n",
                                         gX11->xAtomToString(data[i]).c_str()));

                        m_actions.append(data[i]);
                    }

                    XFree(ret);
                }

                /* Acknowledge the event by sending a Status msg back to the
                 * window. */
                XClientMessageEvent m;
                RT_ZERO(m);
                m.type         = ClientMessage;
                m.display      = m_pDisplay;
                m.window       = m_wndCur;
                m.message_type = xAtom(XA_XdndStatus);
                m.format       = 32;
                m.data.l[0]    = m_wndProxy;
                m.data.l[1]    = RT_BIT(0); /* Accept the drop. */
                m.data.l[4]    = xAtom(XA_XdndActionCopy); /** @todo Make the accepted action configurable. */

                xRc = XSendEvent(m_pDisplay, m_wndCur,
                                 False, 0, reinterpret_cast<XEvent*>(&m));
                if (RT_UNLIKELY(xRc == 0))
                    LogFlowThisFunc(("Error sending xEvent\n"));
            }
            /* Did the source tell us where the cursor currently is? */
            else if (pEventClient->message_type == xAtom(XA_XdndPosition))
            {
                LogFlowThisFunc(("XA_XdndPosition\n"));

                /* Reply with a XdndStatus message to tell the source whether
                 * the data can be dropped or not. */
                XClientMessageEvent m;
                RT_ZERO(m);
                m.type         = ClientMessage;
                m.display      = m_pDisplay;
                m.window       = m_wndCur;
                m.message_type = xAtom(XA_XdndStatus);
                m.format       = 32;
                m.data.l[0]    = m_wndProxy;
                m.data.l[1]    = RT_BIT(0); /* Accept the drop. */
                m.data.l[4]    = xAtom(XA_XdndActionCopy); /** @todo Make the accepted action configurable. */

                xRc = XSendEvent(m_pDisplay, m_wndCur,
                                 False, 0, reinterpret_cast<XEvent*>(&m));
                if (RT_UNLIKELY(xRc == 0))
                    LogFlowThisFunc(("Error sending xEvent\n"));
            }
            else if (pEventClient->message_type == xAtom(XA_XdndLeave))
            {
                LogFlowThisFunc(("XA_XdndLeave\n"));
            }
        }

        /* Do we need to acknowledge at least one format to the host? */
        if (!m_formats.isEmpty())
        {
            RTCString strFormats = gX11->xAtomListToString(m_formats);
            uint32_t uDefAction = DND_COPY_ACTION; /** @todo Handle default action! */
            uint32_t uAllActions = toHGCMActions(m_actions);

            rc = VbglR3DnDGHAcknowledgePending(m_uClientID, uDefAction, uAllActions, strFormats.c_str());
            LogFlowThisFunc(("Acknowledging m_uClientID=%RU32, allActions=0x%x, strFormats=%s, rc=%Rrc\n",
                             m_uClientID, uAllActions, strFormats.c_str(), rc));
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DragInstance::ghDropped(const RTCString &strFormat, uint32_t uAction)
{
    LogFlowThisFunc(("strFormat=%s, uAction=%RU32\n",
                     strFormat.c_str(), uAction));

    int rc = VINF_SUCCESS;

    m_state = Dropped;

    /* Show the proxy window, so that the source will find it. */
    int iRootX, iRootY;
    proxyWinShow(&iRootX, &iRootY);
    XFlush(m_pDisplay);

#ifdef DEBUG
    XWindowAttributes xwa;
    XGetWindowAttributes(m_pDisplay, m_wndCur, &xwa);
    LogFlowThisFunc(("m_wndCur=%#x, x=%d, y=%d, width=%d, height=%d\n",
                     m_wndCur, xwa.x, xwa.y, xwa.width, xwa.height));

    iRootX = xwa.x;
    iRootY = xwa.y;
#endif

    /* We send a fake release event to the current window, cause
     * this should have the grab. */
    mouseButtonSet(m_wndCur /* Source window */, iRootX, iRootY,
                   1 /* Button */, false /* Release button */);

    /* The fake button release event should lead to a XdndDrop event from the
     * source. Because of showing our proxy window, other Xdnd events can
     * occur before, e.g. a XdndPosition event. We are not interested
     * in those, so just try to get the right one. */

    XClientMessageEvent evDnDDrop;
    bool fDrop = waitForX11ClientMsg(evDnDDrop, xAtom(XA_XdndDrop),
                                     5 * 1000 /* Timeout in ms */);
    if (fDrop)
    {
        /* Request to convert the selection in the specific format and
         * place it to our proxy window as property. */
        Window wndSource = evDnDDrop.data.l[0]; /* Source window which sent the message. */
        Assert(wndSource == m_wndCur);
        Atom aFormat  = gX11->stringToxAtom(strFormat.c_str());

        XConvertSelection(m_pDisplay, xAtom(XA_XdndSelection),
                          aFormat, xAtom(XA_XdndSelection),
                          m_wndProxy, evDnDDrop.data.l[2]);

        /* Wait for the selection notify event. */
        XEvent evSelNotify;
        RT_ZERO(evSelNotify);
        if (waitForX11Msg(evSelNotify, SelectionNotify))
        {
            bool fCancel = false;

            /* Make some paranoid checks. */
            if (   evSelNotify.xselection.type      == SelectionNotify
                && evSelNotify.xselection.display   == m_pDisplay
                && evSelNotify.xselection.selection == xAtom(XA_XdndSelection)
                && evSelNotify.xselection.requestor == m_wndProxy
                && evSelNotify.xselection.target    == aFormat)
            {
                LogFlowThisFunc(("Selection notfiy (from wnd=%#x)\n", m_wndCur));

                Atom aPropType;
                int iPropFormat;
                unsigned long cItems, cbRemaining;
                unsigned char *pcData = NULL;
                XGetWindowProperty(m_pDisplay, m_wndProxy,
                                   xAtom(XA_XdndSelection) /* Property */,
                                   0 /* Offset */,
                                   VBOX_MAX_XPROPERTIES /* Length of 32-bit multiples */,
                                   True /* Delete property? */,
                                   AnyPropertyType, /* Property type */
                                   &aPropType, &iPropFormat, &cItems, &cbRemaining, &pcData);

                LogFlowThisFunc(("strType=%s, iPropFormat=%d, cItems=%RU32, cbRemaining=%RU32\n",
                                 gX11->xAtomToString(aPropType).c_str(), iPropFormat, cItems, cbRemaining));

                if (   aPropType   != None
                    && pcData      != NULL
                    && iPropFormat >= 8
                    && cItems      >  0
                    && cbRemaining == 0)
                {
                    size_t cbData = cItems * (iPropFormat / 8);
                    LogFlowThisFunc(("cbData=%zu\n", cbData));

                    /* For whatever reason some of the string MIME types are not
                     * zero terminated. Check that and correct it when necessary,
                     * because the guest side wants this in any case. */
                    if (   m_lstAllowedFormats.contains(strFormat)
                        && pcData[cbData - 1] != '\0')
                    {
                        unsigned char *pvDataTmp = static_cast<unsigned char*>(RTMemAlloc(cbData + 1));
                        if (pvDataTmp)
                        {
                            memcpy(pvDataTmp, pcData, cbData);
                            pvDataTmp[cbData++] = '\0';

                            rc = VbglR3DnDGHSendData(m_uClientID, strFormat.c_str(),
                                                     pvDataTmp, cbData);
                            RTMemFree(pvDataTmp);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    else
                    {
                        /* Send the raw data to the host. */
                        rc = VbglR3DnDGHSendData(m_uClientID, strFormat.c_str(),
                                                 pcData, cbData);
                    }

                    LogFlowThisFunc(("Sent strFormat=%s with rc=%Rrc\n",
                                     strFormat.c_str(), rc));

                    if (RT_SUCCESS(rc))
                    {
                        /* Confirm the result of the transfer to the source window. */
                        XClientMessageEvent m;
                        RT_ZERO(m);
                        m.type         = ClientMessage;
                        m.display      = m_pDisplay;
                        m.window       = m_wndProxy;
                        m.message_type = xAtom(XA_XdndFinished);
                        m.format       = 32;
                        m.data.l[0]    = m_wndProxy; /* Target window. */
                        m.data.l[1]    = 0; /* Don't accept the drop to not make the guest stuck. */
                        m.data.l[2]    = RT_SUCCESS(rc) ? toX11Action(uAction) : None; /* Action used on success */

                        int xrc = XSendEvent(m_pDisplay, wndSource, True, NoEventMask, reinterpret_cast<XEvent*>(&m));
                        if (RT_UNLIKELY(xrc == 0))
                            LogFlowThisFunc(("Error sending xEvent\n"));
                    }
                    else
                        fCancel = true;
                }
                else
                {
                    if (aPropType == xAtom(XA_INCR))
                    {
                        /** @todo Support incremental transfers. */
                        AssertMsgFailed(("Incrementally transfers are not supported yet\n"));
                        rc = VERR_NOT_IMPLEMENTED;
                    }
                    else
                    {
                        AssertMsgFailed(("Not supported data type (%s)\n",
                                         gX11->xAtomToString(aPropType).c_str()));
                        rc = VERR_INVALID_PARAMETER;
                    }

                    fCancel = true;
                }

                if (fCancel)
                {
                    LogFlowFunc(("Cancelling drop ...\n"));

                    /* Cancel this. */
                    XClientMessageEvent m;
                    RT_ZERO(m);
                    m.type         = ClientMessage;
                    m.display      = m_pDisplay;
                    m.window       = m_wndProxy;
                    m.message_type = xAtom(XA_XdndFinished);
                    m.format       = 32;
                    m.data.l[0]    = m_wndProxy; /* Target window. */
                    m.data.l[1]    = 0; /* Don't accept the drop to not make the guest stuck. */

                    int xrc = XSendEvent(m_pDisplay, wndSource, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
                    if (RT_UNLIKELY(xrc == 0))
                        LogFlowThisFunc(("Error sending cancel event\n"));
                }

                /* Cleanup. */
                if (pcData)
                    XFree(pcData);
            }
            else
                rc = VERR_INVALID_PARAMETER;
        }
        else
            rc = VERR_TIMEOUT;
    }
    else
        rc = VERR_TIMEOUT;

    /* Inform the host on error. */
    if (RT_FAILURE(rc))
    {
        int rc2 = VbglR3DnDGHSendError(m_uClientID, rc);
        AssertRC(rc2);
    }

    /* At this point, we have either successfully transfered any data or not.
     * So reset our internal state because we are done here for this transaction. */
    reset();

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

/*
 * Helpers
 */

int DragInstance::mouseCursorMove(int iPosX, int iPosY) const
{
    LogFlowThisFunc(("iPosX=%d, iPosY=%d\n", iPosX, iPosY));

    /* Move the guest pointer to the DnD position, so we can find the window
     * below that position. */
    XWarpPointer(m_pDisplay, None, m_wndRoot, 0, 0, 0, 0, iPosX, iPosY);
    return VINF_SUCCESS;
}

void DragInstance::mouseButtonSet(Window wndDest, int rx, int ry, int iButton, bool fPress) const
{
    LogFlowThisFunc(("wndDest=%#x, rx=%d, ry=%d, iBtn=%d, fPress=%RTbool\n",
                     wndDest, rx, ry, iButton, fPress));
#if 0
 // Create and setting up the event
    XEvent event;
    memset (&event, 0, sizeof (event));
    event.xbutton.button = iButton;
    event.xbutton.same_screen = True;
    event.xbutton.subwindow = DefaultRootWindow (m_pDisplay);

    while (event.xbutton.subwindow)
    {
        event.xbutton.window = event.xbutton.subwindow;
        XQueryPointer (m_pDisplay, event.xbutton.window,
                        &event.xbutton.root, &event.xbutton.subwindow,
                        &event.xbutton.x_root, &event.xbutton.y_root,
                        &event.xbutton.x, &event.xbutton.y,
                        &event.xbutton.state);
    }
    // Press
    event.type = ButtonPress;
    if (XSendEvent (m_pDisplay, PointerWindow, True, ButtonPressMask, &event) == 0)
        LogFlowThisFunc(("Error sending XTestFakeButtonEvent event\n"));
    XFlush (m_pDisplay);

    // Release
    event.type = ButtonRelease;
    if (XSendEvent (m_pDisplay, PointerWindow, True, ButtonReleaseMask, &event) == 0)
        LogFlowThisFunc(("Error sending XTestFakeButtonEvent event\n"));
    XFlush (m_pDisplay);

#else
#ifdef VBOX_DND_WITH_XTEST
    /** @todo Make this check run only once. */
    int ev, er, ma, mi;
    if (XTestQueryExtension(m_pDisplay, &ev, &er, &ma, &mi))
    {
        LogFlowThisFunc(("XText extension available\n"));

        int xrc = XTestFakeButtonEvent(m_pDisplay, 1, fPress ? True : False, CurrentTime);
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending XTestFakeButtonEvent event\n"));
        XFlush(m_pDisplay);
    }
    else
    {
#endif
        LogFlowThisFunc(("XText extension not available or disabled\n"));

        XButtonEvent eBtn;
        RT_ZERO(eBtn);

        eBtn.display      = m_pDisplay;
        eBtn.root         = m_wndRoot;
        eBtn.window       = wndDest;
        eBtn.subwindow    = None;
        eBtn.same_screen  = True;
        eBtn.time         = CurrentTime;
        eBtn.button       = iButton;
        eBtn.state       |= iButton == 1 ? Button1Mask /*:
                            iButton == 2 ? Button2MotionMask :
                            iButton == 3 ? Button3MotionMask :
                            iButton == 4 ? Button4MotionMask :
                            iButton == 5 ? Button5MotionMask*/ : 0;
        eBtn.type         = fPress ? ButtonPress : ButtonRelease;
        eBtn.send_event   = False;
        eBtn.x_root       = rx;
        eBtn.y_root       = ry;

        //XTranslateCoordinates(m_pDisplay, eBtn.root, eBtn.window, eBtn.x_root, eBtn.y_root, &eBtn.x, &eBtn.y, &eBtn.subwindow);
#if 0
        int xrc = XSendEvent(m_pDisplay, eBtn.window, True /* fPropagate */,
                               fPress
                             ? ButtonPressMask : ButtonReleaseMask,
                             reinterpret_cast<XEvent*>(&eBtn));
#else
        int xrc = XSendEvent(m_pDisplay, eBtn.window, False /* fPropagate */,
                             0 /* Mask */,
                             reinterpret_cast<XEvent*>(&eBtn));
#endif
        if (RT_UNLIKELY(xrc == 0))
            LogFlowThisFunc(("Error sending XSendEvent\n"));

#ifdef DEBUG
        Window wndTemp, wndChild;
        int wx, wy; unsigned int mask;
        XQueryPointer(m_pDisplay, m_wndRoot, &wndTemp, &wndChild, &rx, &ry, &wx, &wy, &mask);
        LogFlowFunc(("cursorRootX=%d, cursorRootY=%d\n", rx, ry));
#endif

#ifdef VBOX_DND_WITH_XTEST
    }
#endif
#endif
}

int DragInstance::proxyWinShow(int *piRootX /*= NULL*/,
                               int *piRootY /*= NULL*/,
                               bool fMouseMove /*= false */) const
{
    /* piRootX is optional. */
    /* piRootY is optional. */

    LogFlowThisFuncEnter();

    int iRootX, iRootY;
    int iChildX, iChildY;
    unsigned int iMask;
    Window wndRoot, wndChild;

//    XTestGrabControl(m_pDisplay, False);
    Bool fInRootWnd = XQueryPointer(m_pDisplay, m_wndRoot, &wndRoot, &wndChild,
                                    &iRootX, &iRootY,
                                    &iChildX, &iChildY, &iMask);
#ifdef DEBUG_andy
    LogFlowThisFunc(("fInRootWnd=%RTbool, wndRoot=0x%x, wndChild=0x%x, iRootX=%d, iRootY=%d\n",
                     RT_BOOL(fInRootWnd), wndRoot, wndChild, iRootX, iRootY));
#endif

    if (piRootX)
        *piRootX = iRootX;
    if (piRootY)
        *piRootY = iRootY;

    XSynchronize(m_pDisplay, True /* Enable sync */);

    XMapWindow(m_pDisplay, m_wndProxy);
    XRaiseWindow(m_pDisplay, m_wndProxy);
    XMoveResizeWindow(m_pDisplay, m_wndProxy, iRootX, iRootY, 100, 100);

    if (fMouseMove)
        XWarpPointer(m_pDisplay, None, m_wndRoot, 0, 0, 0, 0, iRootX, iRootY);

    XSynchronize(m_pDisplay, False /* Disable sync */);
//    XTestGrabControl(m_pDisplay, True);

    return VINF_SUCCESS; /** @todo Add error checking. */
}

int DragInstance::proxyWinHide(void)
{
    LogFlowFuncEnter();

    XUnmapWindow(m_pDisplay, m_wndProxy);
    m_eventQueue.clear();

    return VINF_SUCCESS; /** @todo Add error checking. */
}

/* Currently not used. */
/** @todo Is this function still needed? */
void DragInstance::registerForEvents(Window wndThis) const
{
    if (wndThis == m_wndProxy)
        return;

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

void DragInstance::setActionsWindowProperty(Window wndThis,
                                            const RTCList<Atom> &lstActions) const
{
    if (lstActions.isEmpty())
        return;

    XChangeProperty(m_pDisplay, wndThis,
                    xAtom(XA_XdndActionList),
                    XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(lstActions.raw()),
                    lstActions.size());
}

void DragInstance::clearActionsWindowProperty(Window wndThis) const
{
    XDeleteProperty(m_pDisplay, wndThis, xAtom(XA_XdndActionList));
}

void DragInstance::setFormatsWindowProperty(Window wndThis,
                                            Atom property) const
{
    if (m_formats.isEmpty())
        return;

    /* We support TARGETS and the data types. */
    RTCList<Atom> targets(m_formats.size() + 1);
    targets.append(xAtom(XA_TARGETS));
    targets.append(m_formats);

    /* Add the property with the property data to the window. */
    XChangeProperty(m_pDisplay, wndThis, property,
                    XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(targets.raw()),
                    targets.size());
}

void DragInstance::clearFormatsWindowProperty(Window wndThis) const
{
    XDeleteProperty(m_pDisplay, wndThis,
                    xAtom(XA_XdndTypeList));
}

RTCList<Atom> DragInstance::toAtomList(const RTCList<RTCString> &lstFormats) const
{
    RTCList<Atom> atomList;
    for (size_t i = 0; i < lstFormats.size(); ++i)
        atomList.append(XInternAtom(m_pDisplay, lstFormats.at(i).c_str(), False));

    return atomList;
}

RTCList<Atom> DragInstance::toAtomList(void *pvData, uint32_t cbData) const
{
    if (   !pvData
        || !cbData)
        return RTCList<Atom>();

    char *pszStr = (char*)pvData;
    uint32_t cbStr = cbData;

    RTCList<Atom> lstAtom;
    while (cbStr)
    {
        size_t cbSize = RTStrNLen(pszStr, cbStr);

        /* Create a copy with max N chars, so that we are on the save side,
         * even if the data isn't zero terminated. */
        char *pszTmp = RTStrDupN(pszStr, cbSize);
        LogFlowThisFunc(("f: %s\n", pszTmp));
        lstAtom.append(XInternAtom(m_pDisplay, pszTmp, False));
        RTStrFree(pszTmp);
        pszStr  += cbSize + 1;
        cbStr   -= cbSize + 1;
    }

    return lstAtom;
}

/* static */
Atom DragInstance::toX11Action(uint32_t uAction)
{
    /* Ignore is None. */
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

/** @todo Replace by DnDURIList?  */
RTCList<RTCString> toStringList(const void *pvData, uint32_t cbData)
{
    if (   !pvData
        || !cbData)
        return RTCList<RTCString>();

    const char *pszStr = (char*)pvData;
    uint32_t cbStr = cbData;

    RTCList<RTCString> lstString;
    while (cbStr > 0)
    {
        size_t cbSize = RTStrNLen(pszStr, cbStr);

        /* Create a copy with max N chars, so that we are on the save side,
         * even if the data isn't zero terminated. */
        char *pszTmp = RTStrDupN(pszStr, cbSize);
        lstString.append(pszTmp);
        RTStrFree(pszTmp);
        pszStr  += cbSize + 1;
        cbStr   -= cbSize + 1;
    }

    return lstString;
}

/** @todo Is this function really needed?  */
void DragAndDropService::clearEventQueue(void)
{
    LogFlowThisFuncEnter();
    m_eventQueue.clear();
}

/*******************************************************************************
 * DragAndDropService implementation.
 ******************************************************************************/

int DragAndDropService::run(bool fDaemonised /* = false */)
{
    LogFlowThisFunc(("fDaemonised=%RTbool\n", fDaemonised));

    int rc;
    do
    {
        /* Initialize X11 DnD. */
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

        LogRel(("DnD: Started\n"));

        /* Enter the main event processing loop. */
        do
        {
            DnDEvent e;
            RT_ZERO(e);

            LogFlowFunc(("Waiting for new event ...\n"));
            rc = RTSemEventWait(m_hEventSem, RT_INDEFINITE_WAIT);
            if (RT_FAILURE(rc))
                break;

            AssertMsg(m_eventQueue.size(),
                      ("Event queue is empty when it shouldn't\n"));

            e = m_eventQueue.first();
            m_eventQueue.removeFirst();

            if (e.type == DnDEvent::HGCM_Type)
            {
                LogFlowThisFunc(("HGCM event, type=%RU32\n", e.hgcm.uType));
                switch (e.hgcm.uType)
                {
                    case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
                    {
                        if (e.hgcm.cbFormats)
                        {
                            RTCList<RTCString> lstFormats
                                = RTCString(e.hgcm.pszFormats, e.hgcm.cbFormats - 1).split("\r\n");
                            m_pCurDnD->hgEnter(lstFormats, e.hgcm.u.a.uAllActions);

                            /* Enter is always followed by a move event. */
                        }
                        else /* Invalid parameter, skip. */
                            break;
                        /* Not breaking unconditionally is intentional. See comment above. */
                    }
                    case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
                    {
                        m_pCurDnD->hgMove(e.hgcm.u.a.uXpos, e.hgcm.u.a.uYpos,
                                          e.hgcm.u.a.uDefAction);
                        break;
                    }
                    case DragAndDropSvc::HOST_DND_HG_EVT_LEAVE:
                    {
                        m_pCurDnD->reset();
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
                    case DragAndDropSvc::HOST_DND_GH_REQ_PENDING:
                    {
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
                        m_pCurDnD->ghIsDnDPending();
#endif
                        break;
                    }
                    case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
                    {
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
                        m_pCurDnD->ghDropped(e.hgcm.pszFormats,
                                             e.hgcm.u.a.uDefAction);
#endif
                        break;
                    }

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
            else if (e.type == DnDEvent::X11_Type)
            {
                m_pCurDnD->onX11Event(e.x11);
            }
            else
                AssertMsgFailed(("Unknown event queue type %d\n", e.type));

            /* Make sure that any X11 requests have actually been sent to the
             * server, since we are waiting for responses using poll() on
             * another thread which will not automatically trigger flushing. */
            XFlush(m_pDisplay);

        } while (!ASMAtomicReadBool(&m_fSrvStopping));

    } while (0);

    LogRel(("DnD: Stopped with rc=%Rrc\n", rc));
    return rc;
}

int DragAndDropService::x11DragAndDropInit(void)
{
    /* Initialise the guest library. */
    int rc = VbglR3InitUser();
    if (RT_FAILURE(rc))
        VBClFatalError(("Failed to connect to the VirtualBox kernel service, rc=%Rrc\n", rc));
    /* Connect to the x11 server. */
    m_pDisplay = XOpenDisplay(NULL);
    if (!m_pDisplay)
        /** @todo Correct errors. */
        return VERR_NOT_FOUND;

    xHelpers *pHelpers = xHelpers::getInstance(m_pDisplay);
    if (!pHelpers)
        return VERR_NO_MEMORY;

    do
    {
        rc = RTSemEventCreate(&m_hEventSem);
        if (RT_FAILURE(rc))
            break;

        rc = RTCritSectInit(&m_eventQueueCS);
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

    /* No clean-up code for now, as we have no good way of testing it and things
     * should get cleaned up when the user process/X11 client exits. */
    if (RT_FAILURE(rc))
        LogRel(("DnD: Failed to start, rc=%Rrc\n", rc));

    return rc;
}

/* static */
int DragAndDropService::hgcmEventThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    DragAndDropService *pThis = static_cast<DragAndDropService*>(pvUser);
    AssertPtr(pThis);

    uint32_t uClientID;
    int rc = VbglR3DnDConnect(&uClientID);
    if (RT_FAILURE(rc))
    {
        LogRel(("DnD: Unable to connect to drag'n drop service, rc=%Rrc\n", rc));
        return rc;
    }

    /* Number of invalid messages skipped in a row. */
    int cMsgSkippedInvalid = 0;
    DnDEvent e;

    do
    {
        RT_ZERO(e);
        e.type = DnDEvent::HGCM_Type;

        /* Wait for new events. */
        rc = VbglR3DnDProcessNextMessage(uClientID, &e.hgcm);
        if (RT_SUCCESS(rc))
        {
            cMsgSkippedInvalid = 0; /* Reset skipped messages count. */

            LogFlowFunc(("Adding new HGCM event ...\n"));
            pThis->m_eventQueue.append(e);

            rc = RTSemEventSignal(pThis->m_hEventSem);
            if (RT_FAILURE(rc))
                break;
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
                LogRel(("DnD: Too many invalid/skipped messages from host, exiting ...\n"));
                break;
            }
        }

    } while (!ASMAtomicReadBool(&pThis->m_fSrvStopping));

    VbglR3DnDDisconnect(uClientID);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
int DragAndDropService::x11EventThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    DragAndDropService *pThis = static_cast<DragAndDropService*>(pvUser);
    AssertPtr(pThis);

    int rc = VINF_SUCCESS;

    DnDEvent e;
    do
    {
        /*
         * Wait for new events. We can't use XIfEvent here, cause this locks
         * the window connection with a mutex and if no X11 events occurs this
         * blocks any other calls we made to X11. So instead check for new
         * events and if there are not any new one, sleep for a certain amount
         * of time.
         */
        if (XEventsQueued(pThis->m_pDisplay, QueuedAfterFlush) > 0)
        {
            RT_ZERO(e);
            e.type = DnDEvent::X11_Type;

            /* XNextEvent will block until a new X event becomes available. */
            XNextEvent(pThis->m_pDisplay, &e.x11);
            {
#ifdef DEBUG
                switch (e.x11.type)
                {
                    case ClientMessage:
                    {
                        XClientMessageEvent *pEvent = reinterpret_cast<XClientMessageEvent*>(&e);
                        AssertPtr(pEvent);

                        RTCString strType = xAtomToString(pEvent->message_type);
                        LogFlowFunc(("ClientMessage: %s (%RU32), serial=%RU32, wnd=%#x\n", strType.c_str(),
                                     pEvent->message_type, pEvent->serial, pEvent->window));

                        if (pEvent->message_type == xAtom(XA_XdndPosition))
                        {
                            int32_t dwPos = pEvent->data.l[2];
                            int32_t dwAction = pEvent->data.l[4];

                            LogFlowFunc(("XA_XdndPosition x=%RI32, y=%RI32, dwAction=%RI32\n",
                                         RT_HIWORD(dwPos), RT_LOWORD(dwPos), dwAction));
                        }
                        else if (pEvent->message_type == xAtom(XA_XdndDrop))
                        {
                            LogFlowFunc(("XA_XdndDrop\n"));
                        }

                        break;
                    }

                    default:
                        LogFlowFunc(("Received X event type=%d\n", e.x11.type));
                        break;
                }
#endif
                LogFlowFunc(("Adding new X11 event ...\n"));

                /* At the moment we only have one drag instance. */
                DragInstance *pInstance = pThis->m_pCurDnD;

                AssertPtr(pInstance);
                pInstance->onX11Event(e.x11);
            }
        }
        else
            RTThreadSleep(25 /* ms */);

    } while (!ASMAtomicReadBool(&pThis->m_fSrvStopping));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/** Drag and drop magic number, start of a UUID. */
#define DRAGANDDROPSERVICE_MAGIC 0x67c97173

/** VBoxClient service class wrapping the logic for the service while
 *  the main VBoxClient code provides the daemon logic needed by all services.
 */
struct DRAGANDDROPSERVICE
{
    /** The service interface. */
    struct VBCLSERVICE *pInterface;
    /** Magic number for sanity checks. */
    uint32_t magic;
    /** Service object. */
    DragAndDropService mDragAndDrop;
};

static const char *getPidFilePath()
{
    return ".vboxclient-draganddrop.pid";
}

static int run(struct VBCLSERVICE **ppInterface, bool fDaemonised)
{
    struct DRAGANDDROPSERVICE *pSelf = (struct DRAGANDDROPSERVICE *)ppInterface;

    if (pSelf->magic != DRAGANDDROPSERVICE_MAGIC)
        VBClFatalError(("Bad display service object!\n"));
    return pSelf->mDragAndDrop.run(fDaemonised);
}

static void cleanup(struct VBCLSERVICE **ppInterface)
{
    NOREF(ppInterface);
    VbglR3Term();
}

struct VBCLSERVICE vbclDragAndDropInterface =
{
    getPidFilePath,
    VBClServiceDefaultHandler, /* init */
    run,
    VBClServiceDefaultHandler, /* pause */
    VBClServiceDefaultHandler, /* resume */
    cleanup
};

/* Static factory */
struct VBCLSERVICE **VBClGetDragAndDropService(void)
{
    struct DRAGANDDROPSERVICE *pService =
        (struct DRAGANDDROPSERVICE *)RTMemAlloc(sizeof(*pService));

    if (!pService)
        VBClFatalError(("Out of memory\n"));
    pService->pInterface = &vbclDragAndDropInterface;
    pService->magic = DRAGANDDROPSERVICE_MAGIC;
    new(&pService->mDragAndDrop) DragAndDropService();
    return &pService->pInterface;
}

