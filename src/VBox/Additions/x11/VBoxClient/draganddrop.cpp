/* $Id$ */
/** @file
 * X11 guest client - Drag and drop implementation.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
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
#include <X11/Xutil.h>
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

/**
 * For X11 guest xDnD is used. See http://www.acc.umu.se/~vatten/XDND.html for
 * a walk trough.
 *
 * Host -> Guest:
 *     For X11 this means mainly forwarding all the events from HGCM to the
 *     appropriate X11 events. There exists a proxy window, which is invisible and
 *     used for all the X11 communication. On a HGCM Enter event, we set our proxy
 *     window as XdndSelection owner with the given mime-types. On every HGCM move
 *     event, we move the X11 mouse cursor to the new position and query for the
 *     window below that position. Depending on if it is XdndAware, a new window or
 *     a known window, we send the appropriate X11 messages to it. On HGCM drop, we
 *     send a XdndDrop message to the current window and wait for a X11
 *     SelectionMessage from the target window. Because we didn't have the data in
 *     the requested mime-type, yet, we save that message and ask the host for the
 *     data. When the data is successfully received from the host, we put the data
 *     as a property to the window and send a X11 SelectionNotify event to the
 *     target window.
 *
 * Guest -> Host:
 *     This is a lot more trickery than H->G. When a pending event from HGCM
 *     arrives, we ask if there currently is an owner of the XdndSelection
 *     property. If so, our proxy window is shown (1x1, but without backing store)
 *     and some mouse event is triggered. This should be followed by an XdndEnter
 *     event send to the proxy window. From this event we can fetch the necessary
 *     info of the MIME types and allowed actions and send this back to the host.
 *     On a drop request from the host, we query for the selection and should get
 *     the data in the specified mime-type. This data is send back to the host.
 *     After that we send a XdndLeave event to the source window.
 *
 ** @todo Cancelling (e.g. with ESC key) doesn't work.
 ** @todo INCR (incremental transfers) support.
 ** @todo Really check for the Xdnd version and the supported features.
 ** @todo Either get rid of the xHelpers class or properly unify the code with the drag instance class.
 */

#define VBOX_XDND_VERSION    (4)
#define VBOX_MAX_XPROPERTIES (LONG_MAX-1)

/**
 * Structure for storing new X11 events and HGCM messages
 * into a single vent queue.
 */
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

/** List of Atoms. */
#define VBoxDnDAtomList RTCList<Atom>

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

    inline RTCString xAtomListToString(const VBoxDnDAtomList &formatList)
    {
        RTCString format;
        for (size_t i = 0; i < formatList.size(); ++i)
            format += xAtomToString(formatList.at(i)) + "\r\n";
        return format;
    }

    RTCString xErrorToString(int xRc) const;
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

xHelpers *xHelpers::m_pInstance = NULL;

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

RTCString xHelpers::xErrorToString(int xRc) const
{
    switch (xRc)
    {
        case Success:           return RTCStringFmt("%d (Success)", xRc);           break;
        case BadRequest:        return RTCStringFmt("%d (BadRequest)", xRc);        break;
        case BadValue:          return RTCStringFmt("%d (BadValue)", xRc);          break;
        case BadWindow:         return RTCStringFmt("%d (BadWindow)", xRc);         break;
        case BadPixmap:         return RTCStringFmt("%d (BadPixmap)", xRc);         break;
        case BadAtom:           return RTCStringFmt("%d (BadAtom)", xRc);           break;
        case BadCursor:         return RTCStringFmt("%d (BadCursor)", xRc);         break;
        case BadFont:           return RTCStringFmt("%d (BadFont)", xRc);           break;
        case BadMatch:          return RTCStringFmt("%d (BadMatch)", xRc);          break;
        case BadDrawable:       return RTCStringFmt("%d (BadDrawable)", xRc);       break;
        case BadAccess:         return RTCStringFmt("%d (BadAccess)", xRc);         break;
        case BadAlloc:          return RTCStringFmt("%d (BadAlloc)", xRc);          break;
        case BadColor:          return RTCStringFmt("%d (BadColor)", xRc);          break;
        case BadGC:             return RTCStringFmt("%d (BadGC)", xRc);             break;
        case BadIDChoice:       return RTCStringFmt("%d (BadIDChoice)", xRc);       break;
        case BadName:           return RTCStringFmt("%d (BadName)", xRc);           break;
        case BadLength:         return RTCStringFmt("%d (BadLength)", xRc);         break;
        case BadImplementation: return RTCStringFmt("%d (BadImplementation)", xRc); break;
    }
    return RTCStringFmt("%d (unknown)", xRc);
}

/** todo Make this iterative. */
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

#ifdef DEBUG
# define VBOX_DND_FN_DECL_LOG(x) inline x /* For LogFlowXXX logging. */
#else
# define VBOX_DND_FN_DECL_LOG(x) x
#endif

/**
 * Class for handling a single drag and drop operation, that is,
 * one source and one target at a time.
 *
 * For now only one DragInstance will exits when the app is running.
 */
class DragInstance
{
public:

    enum State
    {
        Uninitialized = 0,
        Initialized,
        Dragging,
        Dropped,
        State_32BIT_Hack = 0x7fffffff
    };

    enum Mode
    {
        Unknown = 0,
        HG,
        GH,
        Mode_32Bit_Hack = 0x7fffffff
    };

    DragInstance(Display *pDisplay, DragAndDropService *pParent);

public:

    int  init(uint32_t u32ScreenId);
    void uninit(void);
    void reset(void);

    /* Logging. */
    VBOX_DND_FN_DECL_LOG(void) logError(const char *pszFormat, ...);

    /* X11 message processing. */
    int onX11ClientMessage(const XEvent &e);
    int onX11SelectionNotify(const XEvent &e);
    int onX11SelectionRequest(const XEvent &e);
    int onX11Event(const XEvent &e);
    bool waitForX11Msg(XEvent &evX, int iType, RTMSINTERVAL uTimeoutMS = 100);
    bool waitForX11ClientMsg(XClientMessageEvent &evMsg, Atom aType, RTMSINTERVAL uTimeoutMS = 100);

    /* Host -> Guest handling. */
    int hgEnter(const RTCList<RTCString> &formats, uint32_t actions);
    int hgLeave(void);
    int hgMove(uint32_t u32xPos, uint32_t u32yPos, uint32_t action);
    int hgDrop(void);
    int hgDataReceived(const void *pvData, uint32_t cData);

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
    /* Guest -> Host handling. */
    int ghIsDnDPending(void);
    int ghDropped(const RTCString &strFormat, uint32_t action);
#endif

    /* X11 helpers. */
    int  mouseCursorMove(int iPosX, int iPosY) const;
    void mouseButtonSet(Window wndDest, int rx, int ry, int iButton, bool fPress);
    int proxyWinShow(int *piRootX = NULL, int *piRootY = NULL, bool fMouseMove = false) const;
    int proxyWinHide(void);

    void wndXDnDClearActionList(Window wndThis) const;
    void wndXDnDClearFormatList(Window wndThis) const;
    int wndXDnDGetActionList(Window wndThis, VBoxDnDAtomList &lstActions) const;
    int wndXDnDGetFormatList(Window wndThis, VBoxDnDAtomList &lstTypes) const;
    int wndXDnDSetActionList(Window wndThis, const VBoxDnDAtomList &lstActions) const;
    int wndXDnDSetFormatList(Window wndThis, Atom atmProp, const VBoxDnDAtomList &lstFormats) const;

    int             toAtomList(const RTCList<RTCString> &lstFormats, VBoxDnDAtomList &lstAtoms) const;
    int             toAtomList(const void *pvData, uint32_t cbData, VBoxDnDAtomList &lstAtoms) const;
    static Atom     toAtomAction(uint32_t uAction);
    static int      toAtomActions(uint32_t uActions, VBoxDnDAtomList &lstAtoms);
    static uint32_t toHGCMAction(Atom atom);
    static uint32_t toHGCMActions(const VBoxDnDAtomList &actionsList);

protected:

    /** The instance's own DnD context. */
    VBGLR3GUESTDNDCMDCTX        m_dndCtx;
    /** Pointer to service instance. */
    DragAndDropService         *m_pParent;
    /** Pointer to X display operating on. */
    Display                    *m_pDisplay;
    /** X screen ID to operate on. */
    int                         m_screenId;
    /** Pointer to X screen operating on. */
    Screen                     *m_pScreen;
    /** Root window handle. */
    Window                      m_wndRoot;
    /** Proxy window handle. */
    Window                      m_wndProxy;
    /** Current source/target window handle. */
    Window                      m_wndCur;
    /** The XDnD protocol version the current
     *  source/target window is using. */
    long                        m_curVer;
    /** List of (Atom) formats the source window supports. */
    VBoxDnDAtomList             m_lstFormats;
    /** List of (Atom) actions the source window supports. */
    VBoxDnDAtomList             m_lstActions;
    /** Deferred host to guest selection event for sending to the
     *  target window as soon as data from the host arrived. */
    XEvent                      m_eventHgSelection;
    /** Current operation mode. */
    Mode                        m_enmMode;
    /** Current state of operation mode. */
    State                       m_enmState;
    /** The instance's own X event queue. */
    RTCMTList<XEvent>           m_eventQueue;
    /** Critical section for providing serialized access to list
     *  event queue's contents. */
    RTCRITSECT                  m_eventQueueCS;
    /** Event for notifying this instance in case of a new
     *  event. */
    RTSEMEVENT                  m_hEventSem;
    /** List of allowed formats. */
    RTCList<RTCString>          m_lstAllowedFormats;
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

    int dragAndDropInit(void);
    static int hgcmEventThread(RTTHREAD hThread, void *pvUser);
    static int x11EventThread(RTTHREAD hThread, void *pvUser);

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
    : m_pParent(pParent)
    , m_pDisplay(pDisplay)
    , m_pScreen(0)
    , m_wndRoot(0)
    , m_wndProxy(0)
    , m_wndCur(0)
    , m_curVer(-1)
    , m_enmMode(Unknown)
    , m_enmState(Uninitialized)
{
    uninit();
}

/**
 * Unitializes (destroys) this drag instance.
 */
void DragInstance::uninit(void)
{
    reset();
    if (m_wndProxy != 0)
        XDestroyWindow(m_pDisplay, m_wndProxy);

    VbglR3DnDDisconnect(&m_dndCtx);

    m_enmState    = Uninitialized;
    m_screenId = -1;
    m_pScreen  = 0;
    m_wndRoot  = 0;
    m_wndProxy = 0;
}

/**
 * Resets this drag instance.
 */
void DragInstance::reset(void)
{
    LogFlowFuncEnter();

    /* Hide the proxy win. */
    proxyWinHide();

    /* If we are currently the Xdnd selection owner, clear that. */
    Window w = XGetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection));
    if (w == m_wndProxy)
        XSetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection), None, CurrentTime);

    /* Clear any other DnD specific data on the proxy window. */
    wndXDnDClearFormatList(m_wndProxy);
    wndXDnDClearActionList(m_wndProxy);

    /* Reset the internal state. */
    m_lstActions.clear();
    m_lstFormats.clear();
    m_wndCur    = 0;
    m_curVer    = -1;
    m_enmState  = Initialized;
    m_enmMode   = Unknown;
    m_eventQueue.clear();
}

/**
 * Initializes this drag instance.
 *
 * @return  IPRT status code.
 * @param   u32ScreenID             X' screen ID to use.
 */
int DragInstance::init(uint32_t u32ScreenID)
{
    int rc;

    do
    {
        uninit();

        rc = VbglR3DnDConnect(&m_dndCtx);
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
        if ((int)u32ScreenID > ScreenCount(m_pDisplay))
        {
            rc = VERR_INVALID_PARAMETER;
            break;
        }
#if 0
        /* Get the screen number from the x11 server. */
        pDrag->screen = ScreenOfDisplay(m_pDisplay, u32ScreenId);
        if (!pDrag->screen)
        {
            rc = VERR_GENERAL_FAILURE;
            break;
        }
#endif
        m_screenId = u32ScreenID;

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
        attr.override_redirect     = True;
        attr.do_not_propagate_mask = NoEventMask;
#ifdef VBOX_DND_DEBUG_WND
        attr.background_pixel      = XWhitePixel(m_pDisplay, m_screenId);
        attr.border_pixel          = XBlackPixel(m_pDisplay, m_screenId);
        m_wndProxy = XCreateWindow(m_pDisplay, m_wndRoot                     /* Parent */,
                                   100, 100,                                 /* Position */
                                   100, 100,                                 /* Width + height */
                                   2,                                        /* Border width */
                                   CopyFromParent,                           /* Depth */
                                   InputOutput,                              /* Class */
                                   CopyFromParent,                           /* Visual */
                                     CWBackPixel
                                   | CWBorderPixel
                                   | CWOverrideRedirect
                                   | CWDontPropagate,                        /* Value mask */
                                   &attr);                                   /* Attributes for value mask */
#else
        m_wndProxy = XCreateWindow(m_pDisplay, m_wndRoot                 /* Parent */,
                                   0, 0,                                 /* Position */
                                   1, 1,                                 /* Width + height */
                                   0,                                    /* Border width */
                                   CopyFromParent,                       /* Depth */
                                   InputOnly,                            /* Class */
                                   CopyFromParent,                       /* Visual */
                                   CWOverrideRedirect | CWDontPropagate, /* Value mask */
                                   &attr);                               /* Attributes for value mask */
#endif
        if (!m_wndProxy)
        {
            LogRel(("DnD: Error creating proxy window\n"));
            rc = VERR_GENERAL_FAILURE;
            break;
        }

#ifdef VBOX_DND_DEBUG_WND
        XFlush(m_pDisplay);
        XMapWindow(m_pDisplay, m_wndProxy);
        XRaiseWindow(m_pDisplay, m_wndProxy);
        XFlush(m_pDisplay);
#endif
        LogFlowThisFunc(("Created proxy window 0x%x at m_wndRoot=0x%x ...\n", m_wndProxy, m_wndRoot));

        /* Set the window's name for easier lookup. */
        XStoreName(m_pDisplay, m_wndProxy, "VBoxClientWndDnD");

        /* Make the new window Xdnd aware. */
        Atom ver = VBOX_XDND_VERSION;
        XChangeProperty(m_pDisplay, m_wndProxy, xAtom(XA_XdndAware), XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&ver), 1);
    } while (0);

    if (RT_SUCCESS(rc))
    {
        m_enmState = Initialized;
    }
    else
        LogRel(("DnD: Initializing drag instance for screen %RU32 failed with rc=%Rrc\n",
                u32ScreenID, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Logs an error message to the (release) logging instance.
 *
 * @param   pszFormat               Format string to log.
 */
VBOX_DND_FN_DECL_LOG(void) DragInstance::logError(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    AssertPtr(psz);
    LogFlowFunc(("%s", psz));
    LogRel2(("DnD: %s", psz));

    RTStrFree(psz);
}

/**
 * Callback handler for a generic client message from a window.
 *
 * @return  IPRT status code.
 * @param   e                       X11 event to handle.
 */
int DragInstance::onX11ClientMessage(const XEvent &e)
{
    AssertReturn(e.type == ClientMessage, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("mode=%d, state=%d\n", m_enmMode, m_enmState));
    LogFlowThisFunc(("Event wnd=%#x, msg=%s\n", e.xclient.window, xAtomToString(e.xclient.message_type).c_str()));

    int rc;

    switch (m_enmMode)
    {
        case HG:
        {
            /*
             * Client messages are used to inform us about the status of a XdndAware
             * window, in response of some events we send to them.
             */
            if (   e.xclient.message_type == xAtom(XA_XdndStatus)
                && m_wndCur               == static_cast<Window>(e.xclient.data.l[0]))
            {
                /*
                 * The XdndStatus message tell us if the window will accept the DnD
                 * event and with which action. We immediately send this info down to
                 * the host as a response of a previous DnD message.
                 */
                LogFlowThisFunc(("XA_XdndStatus: wnd=%#x, accept=%RTbool, action=%s\n",
                                 e.xclient.data.l[0],
                                 ASMBitTest(&e.xclient.data.l[1], 0),
                                 xAtomToString(e.xclient.data.l[4]).c_str()));

                uint32_t uAction = DND_IGNORE_ACTION;
                /** @todo Compare this with the allowed actions. */
                if (ASMBitTest(&e.xclient.data.l[1], 0))
                    uAction = toHGCMAction(static_cast<Atom>(e.xclient.data.l[4]));

                rc = VbglR3DnDHGAcknowledgeOperation(&m_dndCtx, uAction);
            }
            else if (e.xclient.message_type == xAtom(XA_XdndFinished))
            {
                rc = VINF_SUCCESS;

                /* This message is sent on an un/successful DnD drop request. */
                LogFlowThisFunc(("XA_XdndFinished: wnd=%#x, success=%RTbool, action=%s\n",
                                 e.xclient.data.l[0],
                                 ASMBitTest(&e.xclient.data.l[1], 0),
                                 xAtomToString(e.xclient.data.l[2]).c_str()));

                proxyWinHide();
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
        case Unknown: /* Mode not set (yet), just add what we got. */
        {
            m_eventQueue.append(e);
            rc = RTSemEventSignal(m_hEventSem);
            break;
        }

        default:
        {
            AssertMsgFailed(("Drag and drop mode not implemented: %RU32\n", m_enmMode));
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }
    }

    LogFlowThisFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}

/**
 * Callback handler for a XDnD selection notify from a window. This is needed
 * to let the us know if a certain window has drag'n drop data to share with us,
 * e.g. our proxy window.
 *
 * @return  IPRT status code.
 * @param   e                       X11 event to handle.
 */
int DragInstance::onX11SelectionNotify(const XEvent &e)
{
    AssertReturn(e.type == SelectionNotify, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("m_mode=%d, m_state=%d\n", m_enmMode, m_enmState));

    int rc;

    switch (m_enmMode)
    {
        case GH:
        {
            if (m_enmState == Dropped)
            {
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

/**
 * Callback handler for a XDnD selection request from a window. This is needed
 * to retrieve the data required to complete the actual drag'n drop operation.
 *
 * @returns IPRT status code.
 * @param   e                       X11 event to handle.
 */
int DragInstance::onX11SelectionRequest(const XEvent &e)
{
    AssertReturn(e.type == SelectionRequest, VERR_INVALID_PARAMETER);

    LogFlowThisFunc(("m_mode=%d, m_state=%d\n", m_enmMode, m_enmState));
    LogFlowThisFunc(("Event owner=%#x, requestor=%#x, selection=%s, target=%s, prop=%s, time=%u\n",
                     e.xselectionrequest.owner,
                     e.xselectionrequest.requestor,
                     xAtomToString(e.xselectionrequest.selection).c_str(),
                     xAtomToString(e.xselectionrequest.target).c_str(),
                     xAtomToString(e.xselectionrequest.property).c_str(),
                     e.xselectionrequest.time));
    int rc;

    bool fSendEvent  = false;
    Atom atmTarget   = None;
    Atom atmProperty = None;

    switch (m_enmMode)
    {
        case HG:
        {
            rc = VINF_SUCCESS;

#ifdef DEBUG
            XTextProperty propName;
            XGetWMName(m_pDisplay, e.xselectionrequest.requestor, &propName);
#endif
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
                LogFlowThisFunc(("wnd=%#x ('%s') asking for target list\n",
                                 e.xselectionrequest.requestor, propName.value ? (const char *)propName.value : "<No name>"));

                /* If so, set the window property with the formats on the requestor
                 * window. */
                rc = wndXDnDSetFormatList(e.xselectionrequest.requestor, e.xselectionrequest.property, m_lstFormats);
                if (RT_SUCCESS(rc))
                {
                    atmTarget   = e.xselectionrequest.target;
                    atmProperty = e.xselectionrequest.property;

                    fSendEvent  = true;
                }
            }
            /* Is the requestor asking for a specific MIME type (we support)? */
            else if (m_lstFormats.contains(e.xselectionrequest.target))
            {
                LogFlowThisFunc(("wnd=%#x ('%s') asking for data, format=%s\n",
                                 e.xselectionrequest.requestor, propName.value ? (const char *)propName.value : "<No name>",
                                 xAtomToString(e.xselectionrequest.target).c_str()));

                /* If so, we need to inform the host about this request. Save the
                 * selection request event for later use. */
                if (m_enmState != Dropped)
                {
                    LogFlowThisFunc(("Wrong state (%RU32), refusing request\n", m_enmState));

                    atmTarget   = None;
                    atmProperty = e.xselectionrequest.property;

                    fSendEvent  = true;
                }
                else
                {
                    LogFlowThisFunc(("Saving selection notify message of wnd=%#x ('%s')\n",
                                     e.xselectionrequest.requestor, propName.value ? (const char *)propName.value : "<No name>"));

                    memcpy(&m_eventHgSelection, &e, sizeof(XEvent));

                    RTCString strFormat = xAtomToString(e.xselectionrequest.target);
                    Assert(strFormat.isNotEmpty());

                    rc = VbglR3DnDHGRequestData(&m_dndCtx, strFormat.c_str());
                    LogFlowThisFunc(("Requested data from host as \"%s\", rc=%Rrc\n", strFormat.c_str(), rc));
                }
            }
            /* Anything else. */
            else
            {
                LogFlowThisFunc(("Refusing unknown command of wnd=%#x ('%s')\n", e.xselectionrequest.requestor, 
                                 propName.value ? (const char *)propName.value : "<No name>"));

                /* We don't understand this request message and therefore answer with an
                 * refusal messages. */
                fSendEvent = true;
            }

            if (   RT_SUCCESS(rc)
                && fSendEvent)
            {
                XEvent s;
                RT_ZERO(s);
                s.xselection.type      = SelectionNotify;
                s.xselection.display   = e.xselection.display;
                s.xselection.time      = e.xselectionrequest.time;
                s.xselection.selection = e.xselectionrequest.selection;
                s.xselection.requestor = e.xselectionrequest.requestor;
                s.xselection.target    = atmTarget;
                s.xselection.property  = atmProperty;

                int xRc = XSendEvent(e.xselection.display, e.xselectionrequest.requestor, False, 0, &s);
                if (RT_UNLIKELY(xRc == 0))
                    logError("Error sending SelectionNotify(1) event to wnd=%#x: %s\n", e.xselectionrequest.requestor,
                             gX11->xErrorToString(xRc).c_str());
            }

#ifdef DEBUG
            if (propName.value)
                XFree(propName.value);
#endif
            break;
        }

        default:
        {
            LogFlowThisFunc(("Unhandled message for wnd=%#x: %s\n",
                             e.xclient.data.l[0], xAtomToString(e.xclient.message_type).c_str()));
            rc = VERR_INVALID_STATE;
            break;
        }
    }

    LogFlowThisFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}

/**
 * Handles X11 events, called by x11EventThread.
 *
 * @returns IPRT status code.
 * @param   e                       X11 event to handle.
 */
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
/**
 * Waits for an X11 event of a specific type.
 *
 * @returns IPRT status code.
 * @param   evX                     Reference where to store the event into.
 * @param   iType                   Event type to wait for.
 * @param   uTimeoutMS              Timeout (in ms) to wait for the event.
 */
bool DragInstance::waitForX11Msg(XEvent &evX, int iType, RTMSINTERVAL uTimeoutMS /* = 100 */)
{
    LogFlowThisFunc(("iType=%d, uTimeoutMS=%RU32, cEventQueue=%zu\n", iType, uTimeoutMS, m_eventQueue.size()));

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

    LogFlowThisFunc(("Returning fFound=%RTbool, msRuntime=%RU64\n", fFound, RTTimeMilliTS() - uiStart));
    return fFound;
}

/**
 * Waits for an X11 client message of a specific type.
 *
 * @returns IPRT status code.
 * @param   evMsg                   Reference where to store the event into.
 * @param   aType                   Event type to wait for.
 * @param   uTimeoutMS              Timeout (in ms) to wait for the event.
 */
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
                    /** @todo Check is aType matches the event's type! */

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

    LogFlowThisFunc(("Returning fFound=%RTbool, msRuntime=%RU64\n", fFound, RTTimeMilliTS() - uiStart));
    return fFound;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

/*
 * Host -> Guest
 */

/**
 * Host -> Guest: Event signalling that the host's (mouse) cursor just entered the VM's (guest's) display
 *                area.
 *
 * @returns IPRT status code.
 * @param   lstFormats              List of supported formats from the host.
 * @param   uActions                (ORed) List of supported actions from the host.
 */
int DragInstance::hgEnter(const RTCList<RTCString> &lstFormats, uint32_t uActions)
{
    LogFlowThisFunc(("mode=%RU32, state=%RU32\n", m_enmMode, m_enmState));

    if (m_enmMode != Unknown)
        return VERR_INVALID_STATE;

    reset();

#ifdef DEBUG
    LogFlowThisFunc(("uActions=0x%x, lstFormats=%zu: ", uActions, lstFormats.size()));
    for (size_t i = 0; i < lstFormats.size(); ++i)
        LogFlow(("'%s' ", lstFormats.at(i).c_str()));
    LogFlow(("\n"));
#endif

    int rc;

    do
    {
        rc = toAtomList(lstFormats, m_lstFormats);
        if (RT_FAILURE(rc))
            break;

        /* If we have more than 3 formats we have to use the type list extension. */
        if (m_lstFormats.size() > 3)
        {
            rc = wndXDnDSetFormatList(m_wndProxy, xAtom(XA_XdndTypeList), m_lstFormats);
            if (RT_FAILURE(rc))
                break;
        }

        /* Announce the possible actions. */
        VBoxDnDAtomList lstActions;
        rc = toAtomActions(uActions, lstActions);
        if (RT_FAILURE(rc))
            break;
        rc = wndXDnDSetActionList(m_wndProxy, lstActions);

        /* Set the DnD selection owner to our window. */
        XSetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection), m_wndProxy, CurrentTime);

        m_enmMode  = HG;
        m_enmState = Dragging;

    } while (0);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Host -> Guest: Event signalling that the host's (mouse) cursor has left the VM's (guest's)
 *                display area.
 */
int DragInstance::hgLeave(void)
{
    if (m_enmMode == HG) /* Only reset if in the right operation mode. */
        reset();

    return VINF_SUCCESS;
}

/**
 * Host -> Guest: Event signalling that the host's (mouse) cursor has been moved within the VM's
 *                (guest's) display area.
 *
 * @returns IPRT status code.
 * @param   u32xPos                 Relative X position within the guest's display area.
 * @param   u32yPos                 Relative Y position within the guest's display area.
 * @param   uDefaultAction          Default action the host wants to perform on the guest
 *                                  as soon as the operation successfully finishes.
 */
int DragInstance::hgMove(uint32_t u32xPos, uint32_t u32yPos, uint32_t uDefaultAction)
{
    LogFlowThisFunc(("mode=%RU32, state=%RU32\n", m_enmMode, m_enmState));
    LogFlowThisFunc(("u32xPos=%RU32, u32yPos=%RU32, uAction=%RU32\n", u32xPos, u32yPos, uDefaultAction));

    if (   m_enmMode  != HG
        || m_enmState != Dragging)
    {
        return VERR_INVALID_STATE;
    }

    int rc  = VINF_SUCCESS;
    int xRc = Success;

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
        xRc = XGetWindowProperty(m_pDisplay, wndCursor, xAtom(XA_XdndAware),
                                 0, 2, False, AnyPropertyType,
                                 &atmp, &fmt, &cItems, &cbRemaining, &pcData);

        if (RT_UNLIKELY(xRc != Success))
            logError("Error getting properties of cursor window=%#x: %s\n", wndCursor, gX11->xErrorToString(xRc).c_str());
        else
        {
            if (RT_UNLIKELY(pcData == NULL || fmt != 32 || cItems != 1))
                LogFlowThisFunc(("Wrong properties: pcData=%#x, iFmt=%d, cItems=%ul\n", pcData, fmt, cItems));
            else
            {
                newVer = reinterpret_cast<long*>(pcData)[0];
#ifdef DEBUG
                XTextProperty propName;
                if (XGetWMName(m_pDisplay, wndCursor, &propName))
                {
                    LogFlowThisFunc(("Current: wndCursor=%#x '%s', XdndAware=%ld\n", wndCursor, propName.value, newVer));
                    XFree(propName.value);
                }
#endif
            }

            XFree(pcData);
        }
    }

    /*
     * Is the window under the cursor another one than our current one?
     * Cancel the current drop.
     */
    if (   wndCursor != m_wndCur
        && m_curVer  != -1)
    {
        LogFlowThisFunc(("XA_XdndLeave: window=%#x\n", m_wndCur));

        /* We left the current XdndAware window. Announce this to the current indow. */
        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = m_wndCur;
        m.message_type = xAtom(XA_XdndLeave);
        m.format       = 32;
        m.data.l[0]    = m_wndProxy;                    /* Source window. */

        xRc = XSendEvent(m_pDisplay, m_wndCur, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xRc == 0))
            logError("Error sending XA_XdndLeave event to old window=%#x: %s\n", m_wndCur, gX11->xErrorToString(xRc).c_str());
    }

    /*
     * Do we have a new window which now is under the cursor?
     */
    if (   wndCursor != m_wndCur
        && newVer    != -1)
    {
        LogFlowThisFunc(("XA_XdndEnter: window=%#x\n", wndCursor));

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
        m.data.l[0]    = m_wndProxy;                    /* Source window. */
        m.data.l[1]    = RT_MAKE_U32_FROM_U8(
                         /* Bit 0 is set if the source supports more than three data types. */
                         m_lstFormats.size() > 3 ? 1 : 0,
                         /* Reserved for future use. */
                         0, 0,
                         /* Protocol version to use. */
                         RT_MIN(VBOX_XDND_VERSION, newVer));
        m.data.l[2]    = m_lstFormats.value(0, None);   /* First data type to use. */
        m.data.l[3]    = m_lstFormats.value(1, None);   /* Second data type to use. */
        m.data.l[4]    = m_lstFormats.value(2, None);   /* Third data type to use. */

        xRc = XSendEvent(m_pDisplay, wndCursor, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xRc == 0))
            logError("Error sending XA_XdndEnter event to window=%#x: %s\n", wndCursor, gX11->xErrorToString(xRc).c_str());
    }

    if (newVer != -1)
    {
        LogFlowThisFunc(("XA_XdndPosition: xPos=%RU32, yPos=%RU32 to window=%#x\n", u32xPos, u32yPos, wndCursor));

        /*
         * Send a XdndPosition event with the proposed action to the guest.
         */
        Atom pa = toAtomAction(uDefaultAction);
        LogFlowThisFunc(("strAction=%s\n", xAtomToString(pa).c_str()));

        XClientMessageEvent m;
        RT_ZERO(m);
        m.type         = ClientMessage;
        m.display      = m_pDisplay;
        m.window       = wndCursor;
        m.message_type = xAtom(XA_XdndPosition);
        m.format       = 32;
        m.data.l[0]    = m_wndProxy;                    /* X window ID of source window. */
        m.data.l[2]    = RT_MAKE_U32(u32yPos, u32xPos); /* Cursor coordinates relative to the root window. */
        m.data.l[3]    = CurrentTime;                   /* Timestamp for retrieving data. */
        m.data.l[4]    = pa;                            /* Actions requested by the user. */

        xRc = XSendEvent(m_pDisplay, wndCursor, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
        if (RT_UNLIKELY(xRc == 0))
            logError("Error sending XA_XdndPosition event to current window=%#x: %s\n", wndCursor, gX11->xErrorToString(xRc).c_str());
    }

    if (   wndCursor == None
        && newVer    == -1)
    {
        /* No window to process, so send a ignore ack event to the host. */
        rc = VbglR3DnDHGAcknowledgeOperation(&m_dndCtx, DND_IGNORE_ACTION);
    }
    else
    {
        m_wndCur = wndCursor;
        m_curVer = RT_MIN(VBOX_XDND_VERSION, newVer);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Host -> Guest: Event signalling that the host has dropped the data over the VM (guest) window.
 *
 * @returns IPRT status code.
 */
int DragInstance::hgDrop(void)
{
    LogFlowThisFunc(("wndCur=%#x, wndProxy=%#x, mode=%RU32, state=%RU32\n", m_wndCur, m_wndProxy, m_enmMode, m_enmState));

    if (   m_enmMode  != HG
        || m_enmState != Dragging)
    {
        return VERR_INVALID_STATE;
    }

    int rc = VINF_SUCCESS;

    /*
     * Send a drop event to the current window and reset our DnD status.
     */
    XClientMessageEvent m;
    RT_ZERO(m);
    m.type         = ClientMessage;
    m.display      = m_pDisplay;
    m.window       = m_wndCur;
    m.message_type = xAtom(XA_XdndDrop);
    m.format       = 32;
    m.data.l[0]    = m_wndProxy;                        /* Source window. */
    m.data.l[2]    = CurrentTime;                       /* Timestamp. */

    int xRc = XSendEvent(m_pDisplay, m_wndCur, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
    if (RT_UNLIKELY(xRc == 0))
        logError("Error sending XA_XdndDrop event to wndCur=%#x: %s\n", m_wndCur, gX11->xErrorToString(xRc).c_str());

    m_wndCur = None;
    m_curVer = -1;

    m_enmState = Dropped;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Host -> Guest: Event signalling that the host has sent drag'n drop (MIME) data
 *                to the guest for further processing.
 *
 * @returns IPRT status code.
 * @param   pvData                  Pointer to (MIME) data from host.
 * @param   cbData                  Size (in bytes) of data from host.
 */
int DragInstance::hgDataReceived(const void *pvData, uint32_t cbData)
{
    LogFlowThisFunc(("mode=%RU32, state=%RU32\n", m_enmMode, m_enmState));

    if (   m_enmMode  != HG
        || m_enmState != Dropped)
    {
        return VERR_INVALID_STATE;
    }

    if (RT_UNLIKELY(   pvData == NULL
                    || cbData  == 0))
    {
        return VERR_INVALID_PARAMETER;
    }

    /* Make a copy of the data. The X server will become the new owner. */
    void *pvNewData = RTMemAlloc(cbData);
    if (RT_UNLIKELY(!pvNewData))
        return VERR_NO_MEMORY;

    memcpy(pvNewData, pvData, cbData);

    /*
     * The host has sent us the DnD data in the requested MIME type. This allows us
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
                    reinterpret_cast<const unsigned char*>(pvNewData), cbData);
    int xRc = XSendEvent(s.xselection.display, s.xselection.requestor, True, 0, &s);
    if (RT_UNLIKELY(xRc == 0))
        logError("Error sending SelectionNotify(2) event to window=%#x: %s\n",
                 s.xselection.requestor, gX11->xErrorToString(xRc).c_str());

    /* We're finally done, reset. */
    reset();

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
/**
 * Guest -> Host: Event signalling that the host is asking whether there is a pending
 *                drag event on the guest (to the host).
 *
 * @returns IPRT status code.
 */
int DragInstance::ghIsDnDPending(void)
{
    LogFlowThisFunc(("mode=%RU32, state=%RU32\n", m_enmMode, m_enmState));

    if (m_enmMode == HG)
        return VERR_INVALID_STATE;

    int rc = VINF_SUCCESS;
    Window wndSelection = XGetSelectionOwner(m_pDisplay, xAtom(XA_XdndSelection));
    LogFlowThisFunc(("Checking pending wndSelection=%#x, wndProxy=%#x\n", wndSelection, m_wndProxy));

    /* Is this another window which has a Xdnd selection than our current one? */
    if (   wndSelection
        && wndSelection != m_wndProxy)
    {
        /* Map the window on the current cursor position, which should provoke
         * an XdndEnter event. */
        proxyWinShow(NULL, NULL, true);

        XEvent e;
        if (waitForX11Msg(e, ClientMessage))
        {
            bool fAcceptDrop = false;

            int xRc;
            XClientMessageEvent *pEventClient = reinterpret_cast<XClientMessageEvent*>(&e);
            AssertPtr(pEventClient);

            LogFlowThisFunc(("Received event=%s\n",
                             gX11->xAtomToString(pEventClient->message_type).c_str()));

            if (pEventClient->message_type == xAtom(XA_XdndEnter))
            {
                LogFlowThisFunc(("XA_XdndEnter\n"));

                /*
                 * Prepare everything for our new window.
                 */
                reset();

                /*
                 * Update our state and the window handle to process.
                 */
                m_enmMode   = GH;
                m_enmState  = Dragging;
                m_wndCur    = wndSelection;
                Assert(m_wndCur == (Window)pEventClient->data.l[0]);
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

                        m_lstFormats.append(pEventClient->data.l[i]);
                    }
                }
                else
                {
                    rc = wndXDnDGetFormatList(wndSelection, m_lstFormats);
                }

                /*
                 * Fetch the actions.
                 */
                rc = wndXDnDGetActionList(wndSelection, m_lstActions);

                fAcceptDrop = true;
            }
            /* Did the source tell us where the cursor currently is? */
            else if (pEventClient->message_type == xAtom(XA_XdndPosition))
            {
                LogFlowThisFunc(("XA_XdndPosition\n"));
                fAcceptDrop = true;
            }
            else if (pEventClient->message_type == xAtom(XA_XdndLeave))
            {
                LogFlowThisFunc(("XA_XdndLeave\n"));
            }

            if (fAcceptDrop)
            {
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
                {
                    logError("Error sending position XA_XdndStatus event to current window=%#x: %s\n",
                              m_wndCur, gX11->xErrorToString(xRc).c_str());
                }
            }
        }

        /* Do we need to acknowledge at least one format to the host? */
        if (!m_lstFormats.isEmpty())
        {
            RTCString strFormats = gX11->xAtomListToString(m_lstFormats);
            uint32_t uDefAction = DND_COPY_ACTION; /** @todo Handle default action! */
            uint32_t uAllActions = toHGCMActions(m_lstActions);

            rc = VbglR3DnDGHAcknowledgePending(&m_dndCtx, uDefAction, uAllActions, strFormats.c_str());
            LogFlowThisFunc(("Acknowledging m_uClientID=%RU32, allActions=0x%x, strFormats=%s, rc=%Rrc\n",
                             m_dndCtx.uClientID, uAllActions, strFormats.c_str(), rc));
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Guest -> Host: Event signalling that the host has dropped the item(s) on the
 *                host side.
 *
 * @returns IPRT status code.
 * @param   strFormat               Requested format to send to the host.
 * @param   uAction                 Requested action to perform on the guest.
 */
int DragInstance::ghDropped(const RTCString &strFormat, uint32_t uAction)
{
    LogFlowThisFunc(("mode=%RU32, state=%RU32, strFormat=%s, uAction=%RU32\n",
                     m_enmMode, m_enmState, strFormat.c_str(), uAction));

    if (   m_enmMode  != GH
        || m_enmState != Dragging)
    {
        return VERR_INVALID_STATE;
    }

    int rc = VINF_SUCCESS;

    m_enmState = Dropped;

    /* Show the proxy window, so that the source will find it. */
    int iRootX, iRootY;
    proxyWinShow(&iRootX, &iRootY);
    XFlush(m_pDisplay);

#ifdef DEBUG
    XWindowAttributes xwa;
    XGetWindowAttributes(m_pDisplay, m_wndCur, &xwa);
    LogFlowThisFunc(("wndCur=%#x, x=%d, y=%d, width=%d, height=%d\n", m_wndCur, xwa.x, xwa.y, xwa.width, xwa.height));
#endif

    /* We send a fake release event to the current window, cause
     * this should have the grab. */
    mouseButtonSet(m_wndCur /* Destination window */, iRootX, iRootY,
                   1 /* Button */, false /* fPress */);

    /**
     * The fake button release event above should lead to a XdndDrop event from the
     * source. Because of showing our proxy window, other Xdnd events can
     * occur before, e.g. a XdndPosition event. We are not interested
     * in those, so just try to get the right one.
     */

    XClientMessageEvent evDnDDrop;
    bool fDrop = waitForX11ClientMsg(evDnDDrop, xAtom(XA_XdndDrop), 5 * 1000 /* Timeout in ms */);
    if (fDrop)
    {
        LogFlowThisFunc(("XA_XdndDrop\n"));

        /* Request to convert the selection in the specific format and
         * place it to our proxy window as property. */
        Window wndSource = evDnDDrop.data.l[0]; /* Source window which has sent the message. */
        Assert(wndSource == m_wndCur);
        Atom aFormat  = gX11->stringToxAtom(strFormat.c_str());

        XConvertSelection(m_pDisplay, xAtom(XA_XdndSelection), aFormat, xAtom(XA_XdndSelection),
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
                int xRc = XGetWindowProperty(m_pDisplay, m_wndProxy,
                                             xAtom(XA_XdndSelection)  /* Property */,
                                             0                        /* Offset */,
                                             VBOX_MAX_XPROPERTIES     /* Length of 32-bit multiples */,
                                             True                     /* Delete property? */,
                                             AnyPropertyType,         /* Property type */
                                             &aPropType, &iPropFormat, &cItems, &cbRemaining, &pcData);
                if (RT_UNLIKELY(xRc != Success))
                    LogFlowThisFunc(("Error getting XA_XdndSelection property of proxy window=%#x: %s\n",
                                     m_wndProxy, gX11->xErrorToString(xRc).c_str()));

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

                            rc = VbglR3DnDGHSendData(&m_dndCtx, strFormat.c_str(), pvDataTmp, cbData);
                            RTMemFree(pvDataTmp);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    else
                    {
                        /* Send the raw data to the host. */
                        rc = VbglR3DnDGHSendData(&m_dndCtx, strFormat.c_str(), pcData, cbData);
                    }

                    LogFlowThisFunc(("Sent strFormat=%s, rc=%Rrc\n", strFormat.c_str(), rc));

                    if (RT_SUCCESS(rc))
                    {
                        /* Confirm the result of the transfer to the target window. */
                        XClientMessageEvent m;
                        RT_ZERO(m);
                        m.type         = ClientMessage;
                        m.display      = m_pDisplay;
                        m.window       = wndSource;
                        m.message_type = xAtom(XA_XdndFinished);
                        m.format       = 32;
                        m.data.l[0]    = m_wndProxy;                   /* Target window. */
                        m.data.l[1]    = 0;                            /* Don't accept the drop to not make the guest stuck. */
                        m.data.l[2]    = RT_SUCCESS(rc)
                                       ? toAtomAction(uAction) : None; /* Action used on success */

                        xRc = XSendEvent(m_pDisplay, wndSource, True, NoEventMask, reinterpret_cast<XEvent*>(&m));
                        if (RT_UNLIKELY(xRc == 0))
                            LogFlowThisFunc(("Error sending XA_XdndFinished event to proxy window=%#x: %s\n",
                                             m_wndProxy, gX11->xErrorToString(xRc).c_str()));
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
                        LogFlowFunc(("Not supported data type: %s\n", gX11->xAtomToString(aPropType).c_str()));
                        rc = VERR_NOT_SUPPORTED;
                    }

                    fCancel = true;
                }

                if (fCancel)
                {
                    LogFlowFunc(("Cancelling drop ...\n"));

                    /* Cancel the operation -- inform the source window. */
                    XClientMessageEvent m;
                    RT_ZERO(m);
                    m.type         = ClientMessage;
                    m.display      = m_pDisplay;
                    m.window       = m_wndProxy;
                    m.message_type = xAtom(XA_XdndLeave);
                    m.format       = 32;
                    m.data.l[0]    = wndSource;         /* Source window. */

                    xRc = XSendEvent(m_pDisplay, wndSource, False, NoEventMask, reinterpret_cast<XEvent*>(&m));
                    if (RT_UNLIKELY(xRc == 0))
                    {
                        logError("Error sending XA_XdndLeave event to proxy window=%#x: %s\n",
                                  m_wndProxy, gX11->xErrorToString(xRc).c_str());
                    }
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
        int rc2 = VbglR3DnDGHSendError(&m_dndCtx, rc);
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

/**
 * Moves the mouse pointer to a specific position.
 *
 * @returns IPRT status code.
 * @param   iPosX                   Absolute X coordinate.
 * @param   iPosY                   Absolute Y coordinate.
 */
int DragInstance::mouseCursorMove(int iPosX, int iPosY) const
{
    int iScreenID = XDefaultScreen(m_pDisplay);
    /** @todo What about multiple screens? Test this! */

    const int iScrX = DisplayWidth(m_pDisplay, iScreenID);
    const int iScrY = XDisplayHeight(m_pDisplay, iScreenID);

    iPosX = RT_CLAMP(iPosX, 0, iScrX);
    iPosY = RT_CLAMP(iPosY, 0, iScrY);

    LogFlowThisFunc(("iPosX=%d, iPosY=%d\n", iPosX, iPosY));

    /* Move the guest pointer to the DnD position, so we can find the window
     * below that position. */
    XWarpPointer(m_pDisplay, None, m_wndRoot, 0, 0, 0, 0, iPosX, iPosY);
    return VINF_SUCCESS;
}

/**
 * Sends a mouse button event to a specific window.
 *
 * @param   wndDest                 Window to send the mouse button event to.
 * @param   rx                      X coordinate relative to the root window's origin.
 * @param   ry                      Y coordinate relative to the root window's origin.
 * @param   iButton                 Mouse button to press/release.
 * @param   fPress                  Whether to press or release the mouse button.
 */
void DragInstance::mouseButtonSet(Window wndDest, int rx, int ry, int iButton, bool fPress)
{
    LogFlowThisFunc(("wndDest=%#x, rx=%d, ry=%d, iBtn=%d, fPress=%RTbool\n",
                     wndDest, rx, ry, iButton, fPress));

#ifdef VBOX_DND_WITH_XTEST
    /** @todo Make this check run only once. */
    int ev, er, ma, mi;
    if (XTestQueryExtension(m_pDisplay, &ev, &er, &ma, &mi))
    {
        LogFlowThisFunc(("XText extension available\n"));

        int xRc = XTestFakeButtonEvent(m_pDisplay, 1, fPress ? True : False, CurrentTime);
        if (RT_UNLIKELY(xRc == 0))
            logError("Error sending XTestFakeButtonEvent event: %s\n", gX11->xErrorToString(xRc).c_str());
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
        int xRc = XSendEvent(m_pDisplay, eBtn.window, True /* fPropagate */,
                               fPress
                             ? ButtonPressMask : ButtonReleaseMask,
                             reinterpret_cast<XEvent*>(&eBtn));
#else
        int xRc = XSendEvent(m_pDisplay, eBtn.window, False /* fPropagate */,
                             0 /* Mask */, reinterpret_cast<XEvent*>(&eBtn));
        if (RT_UNLIKELY(xRc == 0))
            logError("Error sending XButtonEvent event to window=%#x: %s\n", wndDest, gX11->xErrorToString(xRc).c_str());
#endif

#ifdef DEBUG
        Window wndTemp, wndChild;
        int wx, wy; unsigned int mask;
        XQueryPointer(m_pDisplay, m_wndRoot, &wndTemp, &wndChild, &rx, &ry, &wx, &wy, &mask);
        LogFlowThisFunc(("cursorRootX=%d, cursorRootY=%d\n", rx, ry));
#endif

#ifdef VBOX_DND_WITH_XTEST
    }
#endif
}

/**
 * Shows the (invisible) proxy window. The proxy window is needed for intercepting
 * drags from the host to the guest or from the guest to the host. It acts as a proxy
 * between the host and the actual (UI) element on the guest OS.
 *
 * To not make it miss any actions this window gets spawned across the entire guest
 * screen (think of an umbrella) to (hopefully) capture everything. A proxy window
 * which follows the cursor would be far too slow here.
 *
 * @returns IPRT status code.
 * @param   piRootX                 X coordinate relative to the root window's origin. Optional.
 * @param   piRootY                 Y coordinate relative to the root window's origin. Optional.
 * @param   fMouseMove              Whether to move the mouse cursor to the root window's origin or not.
 */
int DragInstance::proxyWinShow(int *piRootX /*= NULL*/,
                               int *piRootY /*= NULL*/,
                               bool fMouseMove /*= false */) const
{
    /* piRootX is optional. */
    /* piRootY is optional. */

    LogFlowThisFuncEnter();

    int rc = VINF_SUCCESS;

#if 0
    XTestGrabControl(m_pDisplay, False);
#endif

    /* Get the mouse pointer position and determine if we're on the same screen as the root window
     * and return the current child window beneath our mouse pointer, if any. */
    int iRootX, iRootY;
    int iChildX, iChildY;
    unsigned int iMask;
    Window wndRoot, wndChild;
    Bool fInRootWnd = XQueryPointer(m_pDisplay, m_wndRoot, &wndRoot, &wndChild,
                                    &iRootX, &iRootY,
                                    &iChildX, &iChildY, &iMask);

    LogFlowThisFunc(("fInRootWnd=%RTbool, wndRoot=0x%x, wndChild=0x%x, iRootX=%d, iRootY=%d\n",
                     RT_BOOL(fInRootWnd), wndRoot, wndChild, iRootX, iRootY));

    if (piRootX)
        *piRootX = iRootX;
    if (piRootY)
        *piRootY = iRootY;

    XSynchronize(m_pDisplay, True /* Enable sync */);

    /* Bring our proxy window into foreground. */
    XMapWindow(m_pDisplay, m_wndProxy);
    XRaiseWindow(m_pDisplay, m_wndProxy);

    /* Spawn our proxy window over the entire screen, making it an easy drop target for the host's cursor. */
    int iScreenID = XDefaultScreen(m_pDisplay);
    XMoveResizeWindow(m_pDisplay, m_wndProxy, 0, 0, XDisplayWidth(m_pDisplay, iScreenID), XDisplayHeight(m_pDisplay, iScreenID));
    /** @todo What about multiple screens? Test this! */

    if (fMouseMove)
        rc = mouseCursorMove(iRootX, iRootY);

    XSynchronize(m_pDisplay, False /* Disable sync */);

#if 0
    XTestGrabControl(m_pDisplay, True);
#endif

    return rc;
}

/**
 * Hides the (invisible) proxy window.
 */
int DragInstance::proxyWinHide(void)
{
    LogFlowFuncEnter();

#ifndef VBOX_DND_DEBUG_WND
    XUnmapWindow(m_pDisplay, m_wndProxy);
#endif
    m_eventQueue.clear();

    return VINF_SUCCESS; /** @todo Add error checking. */
}

/**
 * Clear a window's supported/accepted actions list.
 *
 * @param   wndThis                 Window to clear the list for.
 */
void DragInstance::wndXDnDClearActionList(Window wndThis) const
{
    XDeleteProperty(m_pDisplay, wndThis, xAtom(XA_XdndActionList));
}

/**
 * Clear a window's supported/accepted formats list.
 *
 * @param   wndThis                 Window to clear the list for.
 */
void DragInstance::wndXDnDClearFormatList(Window wndThis) const
{
    XDeleteProperty(m_pDisplay, wndThis, xAtom(XA_XdndTypeList));
}

/**
 * Retrieves a window's supported/accepted XDnD actions.
 *
 * @returns IPRT status code.
 * @param   wndThis                 Window to retrieve the XDnD actions for.
 * @param   lstActions              Reference to VBoxDnDAtomList to store the action into.
 */
int DragInstance::wndXDnDGetActionList(Window wndThis, VBoxDnDAtomList &lstActions) const
{
    Atom iActType = None;
    int iActFmt;
    unsigned long cItems, cbData;
    unsigned char *pcbData = NULL;

    /* Fetch the possible list of actions, if this property is set. */
    int xRc = XGetWindowProperty(m_pDisplay, wndThis,
                                 xAtom(XA_XdndActionList),
                                 0, VBOX_MAX_XPROPERTIES,
                                 False, XA_ATOM, &iActType, &iActFmt, &cItems, &cbData, &pcbData);
    if (xRc != Success)
    {
        LogFlowThisFunc(("Error getting XA_XdndActionList atoms from window=%#x: %s\n",
                         wndThis, gX11->xErrorToString(xRc).c_str()));
        return VERR_NOT_FOUND;
    }

    if (   cItems > 0
        && pcbData)
    {
        Atom *paData = reinterpret_cast<Atom *>(pcbData);

        for (unsigned i = 0; i < RT_MIN(VBOX_MAX_XPROPERTIES, cItems); i++)
        {
            LogFlowThisFunc(("Received action: %s\n",
                             gX11->xAtomToString(paData[i]).c_str()));

            lstActions.append(paData[i]);
        }

        XFree(pcbData);
    }

    return VINF_SUCCESS;
}

/**
 * Retrieves a window's supported/accepted XDnD formats.
 *
 * @returns IPRT status code.
 * @param   wndThis                 Window to retrieve the XDnD formats for.
 * @param   lstTypes                Reference to VBoxDnDAtomList to store the formats into.
 */
int DragInstance::wndXDnDGetFormatList(Window wndThis, VBoxDnDAtomList &lstTypes) const
{
    Atom iActType = None;
    int iActFmt;
    unsigned long cItems, cbData;
    unsigned char *pcbData = NULL;

    int xRc = XGetWindowProperty(m_pDisplay, wndThis,
                             xAtom(XA_XdndTypeList),
                             0, VBOX_MAX_XPROPERTIES,
                             False, XA_ATOM, &iActType, &iActFmt, &cItems, &cbData, &pcbData);
    if (xRc != Success)
    {
        LogFlowThisFunc(("Error getting XA_XdndTypeList atoms from window=%#x: %s\n",
                         wndThis, gX11->xErrorToString(xRc).c_str()));
        return VERR_NOT_FOUND;
    }

    if (   cItems > 0
        && pcbData)
    {
        Atom *paData = reinterpret_cast<Atom*>(pcbData);

        for (unsigned i = 0; i < RT_MIN(VBOX_MAX_XPROPERTIES, cItems); i++)
        {
            LogFlowThisFunc(("Received format via XdndTypeList: %s\n",
                             gX11->xAtomToString(paData[i]).c_str()));

            lstTypes.append(paData[i]);
        }

        XFree(pcbData);
    }

    return VINF_SUCCESS;
}

/**
 * Sets (replaces) a window's XDnD accepted/allowed actions.
 *
 * @returns IPRT status code.
 * @param   wndThis                 Window to set the format list for.
 * @param   lstActions              Reference to list of XDnD actions to set.
 *
 * @remark
 */
int DragInstance::wndXDnDSetActionList(Window wndThis, const VBoxDnDAtomList &lstActions) const
{
    if (lstActions.isEmpty())
        return VINF_SUCCESS;

    XChangeProperty(m_pDisplay, wndThis,
                    xAtom(XA_XdndActionList),
                    XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(lstActions.raw()),
                    lstActions.size());

    return VINF_SUCCESS;
}

/**
 * Sets (replaces) a window's XDnD accepted format list.
 *
 * @returns IPRT status code.
 * @param   wndThis                 Window to set the format list for.
 * @param   atmProp                 Property to set.
 * @param   lstFormats              Reference to list of XDnD formats to set.
 */
int DragInstance::wndXDnDSetFormatList(Window wndThis, Atom atmProp, const VBoxDnDAtomList &lstFormats) const
{
    if (lstFormats.isEmpty())
        return VINF_SUCCESS;

    /* We support TARGETS and the data types. */
    VBoxDnDAtomList lstFormatsExt(lstFormats.size() + 1);
    lstFormatsExt.append(xAtom(XA_TARGETS));
    lstFormatsExt.append(lstFormats);

    /* Add the property with the property data to the window. */
    XChangeProperty(m_pDisplay, wndThis, atmProp,
                    XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(lstFormatsExt.raw()),
                    lstFormatsExt.size());

    return VINF_SUCCESS;
}

/**
 * Converts a RTCString list to VBoxDnDAtomList list.
 *
 * @returns IPRT status code.
 * @param   lstFormats              Reference to RTCString list to convert.
 * @param   lstAtoms                Reference to VBoxDnDAtomList list to store results in.
 */
int DragInstance::toAtomList(const RTCList<RTCString> &lstFormats, VBoxDnDAtomList &lstAtoms) const
{
    for (size_t i = 0; i < lstFormats.size(); ++i)
        lstAtoms.append(XInternAtom(m_pDisplay, lstFormats.at(i).c_str(), False));

    return VINF_SUCCESS;
}

/**
 * Converts a raw-data string list to VBoxDnDAtomList list.
 *
 * @returns IPRT status code.
 * @param   pvData                  Pointer to string data to convert.
 * @param   cbData                  Size (in bytes) to convert.
 * @param   lstAtoms                Reference to VBoxDnDAtomList list to store results in.
 */
int DragInstance::toAtomList(const void *pvData, uint32_t cbData, VBoxDnDAtomList &lstAtoms) const
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    const char *pszStr = (char *)pvData;
    uint32_t cbStr = cbData;

    int rc = VINF_SUCCESS;

    VBoxDnDAtomList lstAtom;
    while (cbStr)
    {
        size_t cbSize = RTStrNLen(pszStr, cbStr);

        /* Create a copy with max N chars, so that we are on the save side,
         * even if the data isn't zero terminated. */
        char *pszTmp = RTStrDupN(pszStr, cbSize);
        if (!pszTmp)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        lstAtom.append(XInternAtom(m_pDisplay, pszTmp, False));
        RTStrFree(pszTmp);

        pszStr  += cbSize + 1;
        cbStr   -= cbSize + 1;
    }

    return rc;
}

/**
 * Converts a HGCM-based drag'n drop action to a Atom-based drag'n drop action.
 *
 * @returns Converted Atom-based drag'n drop action.
 * @param   uActions                HGCM drag'n drop actions to convert.
 */
/* static */
Atom DragInstance::toAtomAction(uint32_t uAction)
{
    /* Ignore is None. */
    return (isDnDCopyAction(uAction) ? xAtom(XA_XdndActionCopy) :
            isDnDMoveAction(uAction) ? xAtom(XA_XdndActionMove) :
            isDnDLinkAction(uAction) ? xAtom(XA_XdndActionLink) :
            None);
}

/**
 * Converts HGCM-based drag'n drop actions to a VBoxDnDAtomList list.
 *
 * @returns IPRT status code.
 * @param   uActions                HGCM drag'n drop actions to convert.
 * @param   lstAtoms                Reference to VBoxDnDAtomList to store actions in.
 */
/* static */
int DragInstance::toAtomActions(uint32_t uActions, VBoxDnDAtomList &lstAtoms)
{
    if (hasDnDCopyAction(uActions))
        lstAtoms.append(xAtom(XA_XdndActionCopy));
    if (hasDnDMoveAction(uActions))
        lstAtoms.append(xAtom(XA_XdndActionMove));
    if (hasDnDLinkAction(uActions))
        lstAtoms.append(xAtom(XA_XdndActionLink));

    return VINF_SUCCESS;
}

/**
 * Converts an Atom-based drag'n drop action to a HGCM drag'n drop action.
 *
 * @returns HGCM drag'n drop action.
 * @param   atom                    Atom-based drag'n drop action to convert.
 */
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

/**
 * Converts an VBoxDnDAtomList list to an HGCM action list.
 *
 * @returns ORed HGCM action list.
 * @param   actionsList             List of Atom-based actions to convert.
 */
/* static */
uint32_t DragInstance::toHGCMActions(const VBoxDnDAtomList &lstActions)
{
    uint32_t uActions = DND_IGNORE_ACTION;

    for (size_t i = 0; i < lstActions.size(); ++i)
        uActions |= toHGCMAction(lstActions.at(i));

    return uActions;
}

/*******************************************************************************
 * DragAndDropService implementation.
 ******************************************************************************/

/**
 * Main loop for the drag and drop service which does the HGCM message
 * processing and routing to the according drag and drop instance(s).
 *
 * @returns IPRT status code.
 * @param   fDaemonised             Whether to run in daemonized or not. Does not
 *                                  apply for this service.
 */
int DragAndDropService::run(bool fDaemonised /* = false */)
{
    LogFlowThisFunc(("fDaemonised=%RTbool\n", fDaemonised));

    int rc;
    do
    {
        /* Initialize drag and drop. */
        rc = dragAndDropInit();
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
                            RTCList<RTCString> lstFormats = RTCString(e.hgcm.pszFormats, e.hgcm.cbFormats - 1).split("\r\n");
                            rc = m_pCurDnD->hgEnter(lstFormats, e.hgcm.u.a.uAllActions);
                            /* Enter is always followed by a move event. */
                        }
                        else
                        {
                            rc = VERR_INVALID_PARAMETER;
                            break;
                        }
                        /* Not breaking unconditionally is intentional. See comment above. */
                    }
                    case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
                    {
                        rc = m_pCurDnD->hgMove(e.hgcm.u.a.uXpos, e.hgcm.u.a.uYpos, e.hgcm.u.a.uDefAction);
                        break;
                    }
                    case DragAndDropSvc::HOST_DND_HG_EVT_LEAVE:
                    {
                        rc = m_pCurDnD->hgLeave();
                        break;
                    }
                    case DragAndDropSvc::HOST_DND_HG_EVT_DROPPED:
                    {
                        rc = m_pCurDnD->hgDrop();
                        break;
                    }
                    case DragAndDropSvc::HOST_DND_HG_SND_DATA:
                    {
                        rc = m_pCurDnD->hgDataReceived(e.hgcm.u.b.pvData, e.hgcm.u.b.cbData);
                        break;
                    }
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
                    case DragAndDropSvc::HOST_DND_GH_REQ_PENDING:
                    {
                        rc = m_pCurDnD->ghIsDnDPending();
                        break;
                    }
                    case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
                    {
                        rc = m_pCurDnD->ghDropped(e.hgcm.pszFormats, e.hgcm.u.a.uDefAction);
                        break;
                    }
#endif
                    default:
                    {
                        LogFlowThisFunc(("Unsupported message: %RU32\n", e.hgcm.uType));
                        rc = VERR_NOT_SUPPORTED;
                        break;
                    }
                }

                LogFlowFunc(("Message %RU32 processed with %Rrc\n", e.hgcm.uType, rc));
                if (   RT_FAILURE(rc)
                    /*
                     * Note: The hgXXX and ghXXX functions of the DnD instance above may return
                     *       VERR_INVALID_STATE in case we're not in the expected state they want
                     *       to operate in. As the user might drag content back and forth to/from
                     *       the host/guest we don't want to reset the overall state here in case
                     *       a VERR_INVALID_STATE occurs. Just continue in our initially set mode.
                     */
                    && rc != VERR_INVALID_STATE)
                {
                    m_pCurDnD->logError("Error: Processing message %RU32 failed with %Rrc\n", e.hgcm.uType, rc);

                    /* If anything went wrong, do a reset and start over. */
                    m_pCurDnD->reset();
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

            /*
             * Make sure that any X11 requests have actually been sent to the
             * server, since we are waiting for responses using poll() on
             * another thread which will not automatically trigger flushing.
             */
            XFlush(m_pDisplay);

        } while (!ASMAtomicReadBool(&m_fSrvStopping));

    } while (0);

    LogRel(("DnD: Stopped with rc=%Rrc\n", rc));
    return rc;
}

/**
 * Initializes the drag and drop instance.
 *
 * @returns IPRT status code.
 */
int DragAndDropService::dragAndDropInit(void)
{
    /* Initialise the guest library. */
    int rc = VbglR3InitUser();
    if (RT_FAILURE(rc))
        VBClFatalError(("DnD: Failed to connect to the VirtualBox kernel service, rc=%Rrc\n", rc));

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
                            "dndHGCM");
        if (RT_FAILURE(rc))
            break;

        /* Event thread for events coming from the x11 system. */
        rc = RTThreadCreate(&m_hX11Thread, x11EventThread, this,
                            0, RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE,
                            "dndX11");
    } while (0);

    /* No clean-up code for now, as we have no good way of testing it and things
     * should get cleaned up when the user process/X11 client exits. */
    if (RT_FAILURE(rc))
        LogRel(("DnD: Failed to start, rc=%Rrc\n", rc));

    return rc;
}

/**
 * Static callback function for HGCM message processing thread. An internal
 * message queue will be filled which then will be processed by the according
 * drag'n drop instance.
 *
 * @returns IPRT status code.
 * @param   hThread                 Thread handle to use.
 * @param   pvUser                  Pointer to DragAndDropService instance to use.
 */
/* static */
int DragAndDropService::hgcmEventThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_PARAMETER);
    DragAndDropService *pThis = static_cast<DragAndDropService*>(pvUser);
    AssertPtr(pThis);

    /* This thread has an own DnD context, e.g. an own client ID. */
    VBGLR3GUESTDNDCMDCTX dndCtx;

    int rc = VbglR3DnDConnect(&dndCtx);
    if (RT_FAILURE(rc))
        LogRel(("DnD: Unable to connect to drag and drop service, rc=%Rrc\n", rc));
    /* Not RT_FAILURE: VINF_PERMISSION_DENIED is host service not present. */
    if (rc != VINF_SUCCESS)
        return rc;

    /* Number of invalid messages skipped in a row. */
    int cMsgSkippedInvalid = 0;
    DnDEvent e;

    do
    {
        RT_ZERO(e);
        e.type = DnDEvent::HGCM_Type;

        /* Wait for new events. */
        rc = VbglR3DnDProcessNextMessage(&dndCtx, &e.hgcm);
        if (   RT_SUCCESS(rc)
            || rc == VERR_CANCELLED)
        {
            cMsgSkippedInvalid = 0; /* Reset skipped messages count. */
            pThis->m_eventQueue.append(e);

            rc = RTSemEventSignal(pThis->m_hEventSem);
            if (RT_FAILURE(rc))
                break;
        }
        else
        {
            LogFlowFunc(("Processing next message failed with rc=%Rrc\n", rc));

            /* Old(er) hosts either are broken regarding DnD support or otherwise
             * don't support the stuff we do on the guest side, so make sure we
             * don't process invalid messages forever. */
            if (rc == VERR_INVALID_PARAMETER)
                cMsgSkippedInvalid++;
            if (cMsgSkippedInvalid > 32)
            {
                LogRel(("DnD: Too many invalid/skipped messages from host, exiting ...\n"));
                break;
            }
        }

    } while (!ASMAtomicReadBool(&pThis->m_fSrvStopping));

    VbglR3DnDDisconnect(&dndCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Static callback function for X11 message processing thread. All X11 messages
 * will be directly routed to the according drag'n drop instance.
 *
 * @returns IPRT status code.
 * @param   hThread                 Thread handle to use.
 * @param   pvUser                  Pointer to DragAndDropService instance to use.
 */
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
    uint32_t uMagic;
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

    if (pSelf->uMagic != DRAGANDDROPSERVICE_MAGIC)
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

/* Static factory. */
struct VBCLSERVICE **VBClGetDragAndDropService(void)
{
    struct DRAGANDDROPSERVICE *pService =
        (struct DRAGANDDROPSERVICE *)RTMemAlloc(sizeof(*pService));

    if (!pService)
        VBClFatalError(("Out of memory\n"));
    pService->pInterface = &vbclDragAndDropInterface;
    pService->uMagic = DRAGANDDROPSERVICE_MAGIC;
    new(&pService->mDragAndDrop) DragAndDropService();
    return &pService->pInterface;
}

