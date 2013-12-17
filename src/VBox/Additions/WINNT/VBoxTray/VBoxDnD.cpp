/* $Id$ */
/** @file
 * VBoxDnD.cpp - Windows-specific bits of the drag'n drop service.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <windows.h>
#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxDnD.h"

#include <VBox/VBoxGuestLib.h>
#include "VBox/HostServices/DragAndDropSvc.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/ldr.h>
#include <iprt/list.h>
#include <iprt/mem.h>

#include <iprt/cpp/mtlist.h>
#include <iprt/cpp/ministring.h>

#include <iprt/cpp/mtlist.h>

#include <VBoxGuestInternal.h>

/* Enable this define to see the proxy window(s) when debugging
 * their behavior. Don't have this enabled in release builds! */
#ifdef DEBUG
//# define VBOX_DND_DEBUG_WND
#endif

#define WM_VBOXTRAY_DND_MESSAGE       WM_APP + 401

static LRESULT CALLBACK vboxDnDWndProcInstance(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK vboxDnDWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct VBOXDNDCONTEXT;
class VBoxDnDWnd;

/*
 * A drag'n drop event from the host.
 */
typedef struct VBOXDNDEVENT
{
    /** The actual event data. */
    VBGLR3DNDHGCMEVENT Event;

} VBOXDNDEVENT, *PVBOXDNDEVENT;

/**
 * DnD context data.
 */
typedef struct VBOXDNDCONTEXT
{
    /** Pointer to the service environment. */
    const VBOXSERVICEENV      *pEnv;
    /** Shutdown indicator. */
    bool                       fShutdown;
    /** Thread handle for main event queue
     *  processing. */
    RTTHREAD                   hEvtQueue;
    /** The DnD main event queue. */
    RTCMTList<VBOXDNDEVENT>    lstEvtQueue;
    /** Semaphore for waiting on main event queue
     *  events. */
    RTSEMEVENT                 hEvtQueueSem;
    /** List of drag'n drop windows. At
     *  the moment only one source is supported. */
    RTCMTList<VBoxDnDWnd*>     lstWnd;

} VBOXDNDCONTEXT, *PVBOXDNDCONTEXT;
static VBOXDNDCONTEXT gCtx = {0};

/**
 * Everything which is required to successfully start
 * a drag'n drop operation via DoDragDrop().
 */
typedef struct VBOXDNDSTARTUPINFO
{
    /** Our DnD data object, holding
     *  the raw DnD data. */
    VBoxDnDDataObject         *pDataObject;
    /** The drop source for sending the
     *  DnD request to a IDropTarget. */
    VBoxDnDDropSource         *pDropSource;
    /** The DnD effects which are wanted / allowed. */
    DWORD                      dwOKEffects;

} VBOXDNDSTARTUPINFO, *PVBOXDNDSTARTUPINFO;

/**
 * Class for handling a DnD proxy window.
 ** @todo Unify this and VBoxClient's DragInstance!
 */
class VBoxDnDWnd
{
    enum State
    {
        Uninitialized,
        Initialized,
        Dragging,
        Dropped,
        Canceled
    };

    enum Mode
    {
        Unknown,
        HG,
        GH
    };

public:

    VBoxDnDWnd(void);
    virtual ~VBoxDnDWnd(void);

public:

    int Initialize(PVBOXDNDCONTEXT pContext);

public:

    /** The window's thread for the native message pump and
     *  OLE context. */
    static int Thread(RTTHREAD hThread, void *pvUser);

public:

    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM lParam);
    /** The per-instance wndproc routine. */
    LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:

    int DragRelease(void);

    int OnCreate(void);
    void OnDestroy(void);

    /* H->G */
    int OnHgEnter(const RTCList<RTCString> &formats, uint32_t uAllActions);
    int OnHgMove(uint32_t u32xPos, uint32_t u32yPos, uint32_t uAllActions);
    int OnHgDrop(void);
    int OnHgLeave(void);
    int OnHgDataReceived(const void *pvData, uint32_t cData);
    int OnHgCancel(void);

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
    /* G->H */
    int OnGhIsDnDPending(void);
    int OnGhDropped(const RTCString &strFormat, uint32_t action);
#endif

    int ProcessEvent(PVBOXDNDEVENT pEvent);

protected:

    void reset(void);

public: /** @todo Make protected! */

    /** Pointer to DnD context. */
    PVBOXDNDCONTEXT            pContext;
    RTCRITSECT                 mCritSect;
    RTSEMEVENT                 mEventSem;
#ifdef RT_OS_WINDOWS
    /** The window's handle. */
    HWND                       hWnd;
    /** List of allowed MIME types this
     *  client can handle. Make this a per-instance
     *  property so that we can selectively allow/forbid
     *  certain types later on runtime. */
    RTCList<RTCString>         lstAllowedFormats;
    /** List of formats for the current
     *  drag'n drop operation. */
    RTCList<RTCString>         lstFormats;
    /** Flags of all current drag'n drop
     *  actions allowed. */
    uint32_t                   uAllActions;
    /** The startup information required
     *  for the actual DoDragDrop() call. */
    VBOXDNDSTARTUPINFO         startupInfo;
    bool                       mfMouseButtonDown;
#else
    /** @todo */
#endif

    /** The window's own HGCM client ID. */
    uint32_t                   mClientID;
    Mode                       mMode;
    State                      mState;
    RTCString                  mFormatRequested;
    RTCList<RTCString>         mLstFormats;
    RTCList<RTCString>         mLstActions;
};

VBoxDnDWnd::VBoxDnDWnd(void)
    : hWnd(NULL),
      mfMouseButtonDown(false),
      mClientID(UINT32_MAX),
      mMode(Unknown),
      mState(Uninitialized)
{
    RT_ZERO(startupInfo);
}

VBoxDnDWnd::~VBoxDnDWnd(void)
{
    /** @todo Shutdown crit sect / event etc! */

    reset();
}

int VBoxDnDWnd::DragRelease(void)
{
    /* Release mouse button in the guest to start the "drop"
     * action at the current mouse cursor position. */
    INPUT Input[1] = { 0 };
    Input[0].type       = INPUT_MOUSE;
    Input[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, Input, sizeof(INPUT));

    return VINF_SUCCESS;
}

int VBoxDnDWnd::Initialize(PVBOXDNDCONTEXT pContext)
{
    AssertPtrReturn(pContext, VERR_INVALID_POINTER);

    /* Save the context. */
    this->pContext = pContext;

    int rc = RTSemEventCreate(&mEventSem);
    if (RT_SUCCESS(rc))
        rc = RTCritSectInit(&mCritSect);

    if (RT_SUCCESS(rc))
    {
        /* Message pump thread for our proxy window. */
        rc = RTThreadCreate(&gCtx.hEvtQueue, VBoxDnDWnd::Thread, this,
                            0, RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE,
                            "VBoxTrayDnDWnd");
        if (RT_FAILURE(rc))
            LogRel(("DnD: Failed to start proxy window thread, rc=%Rrc\n", rc));
        /** @todo Wait for thread to be started! */
    }

    LogFlowThisFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}

/**
 * Thread for handling the window's message pump.
 *
 * @return  IPRT status code.
 * @param   hThread
 * @param   pvUser
 */
/* static */
int VBoxDnDWnd::Thread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    VBoxDnDWnd *pThis = (VBoxDnDWnd*)pvUser;
    AssertPtr(pThis);

    PVBOXDNDCONTEXT pContext = pThis->pContext;
    AssertPtr(pContext);
    AssertPtr(pContext->pEnv);

    HINSTANCE hInstance = pContext->pEnv->hInstance;
    Assert(hInstance != 0);

    /* Create our proxy window. */
    WNDCLASSEX wndClass;
    RT_ZERO(wndClass);

    wndClass.cbSize        = sizeof(WNDCLASSEX);
    wndClass.lpfnWndProc   = vboxDnDWndProc;
    wndClass.lpszClassName = "VBoxTrayDnDWnd";
    wndClass.hInstance     = hInstance;
    wndClass.style         = CS_NOCLOSE;
#ifdef VBOX_DND_DEBUG_WND
    wndClass.style        |= CS_HREDRAW | CS_VREDRAW;
    wndClass.hbrBackground = (HBRUSH)(CreateSolidBrush(RGB(255, 0, 0)));
#else
    wndClass.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
#endif

    int rc = VINF_SUCCESS;
    if (!RegisterClassEx(&wndClass))
    {
        DWORD dwErr = GetLastError();
        LogFlowFunc(("Unable to register proxy window class, error=%ld\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
    }

    if (RT_SUCCESS(rc))
    {
        DWORD dwExStyle = WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
        DWORD dwStyle = WS_POPUPWINDOW;
#ifdef VBOX_DND_DEBUG_WND
        dwExStyle &= ~WS_EX_TRANSPARENT;
        dwStyle |= WS_VISIBLE | WS_OVERLAPPEDWINDOW;
#endif
        pThis->hWnd =
            CreateWindowEx(dwExStyle,
                           "VBoxTrayDnDWnd", "VBoxTrayDnDWnd",
                           dwStyle,
#ifdef VBOX_DND_DEBUG_WND
                           CW_USEDEFAULT, CW_USEDEFAULT, 200, 200, NULL, NULL,
#else
                           -200, -200, 100, 100, NULL, NULL,
#endif
                           hInstance, pThis /* lParm */);
        if (!pThis->hWnd)
        {
            DWORD dwErr = GetLastError();
            LogFlowFunc(("Unable to create proxy window, error=%ld\n", dwErr));
            rc = RTErrConvertFromWin32(dwErr);
        }
        else
        {
#ifndef VBOX_DND_DEBUG_WND
            SetWindowPos(pThis->hWnd, HWND_TOPMOST, -200, -200, 0, 0,
                           SWP_NOACTIVATE | SWP_HIDEWINDOW
                         | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);
            LogFlowFunc(("Proxy window created, hWnd=0x%x\n", pThis->hWnd));
#else
            LogFlowFunc(("Debug proxy window created, hWnd=0x%x\n", pThis->hWnd));
#endif

        }
    }

    if (RT_SUCCESS(rc))
    {
        OleInitialize(NULL);

        bool fShutdown = false;

        do
        {
            MSG uMsg;
            while (GetMessage(&uMsg, 0, 0, 0))
            {
                TranslateMessage(&uMsg);
                DispatchMessage(&uMsg);
            }

            if (ASMAtomicReadBool(&pContext->fShutdown))
                fShutdown = true;

            if (fShutdown)
            {
                LogFlowFunc(("Cancelling ...\n"));
                break;
            }

            /** @todo Immediately drop on failure? */

        } while (RT_SUCCESS(rc));

        OleUninitialize();
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
BOOL CALLBACK VBoxDnDWnd::MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor,
                                          LPRECT lprcMonitor, LPARAM lParam)
{
    LPRECT pRect = (LPRECT)lParam;
    AssertPtrReturn(pRect, FALSE);

    AssertPtr(lprcMonitor);
    LogFlowFunc(("Monitor is %ld,%ld,%ld,%ld\n",
                 lprcMonitor->left, lprcMonitor->top,
                 lprcMonitor->right, lprcMonitor->bottom));

    /* Build up a simple bounding box to hold the entire (virtual) screen. */
    if (pRect->left > lprcMonitor->left)
        pRect->left = lprcMonitor->left;
    if (pRect->right < lprcMonitor->right)
        pRect->right = lprcMonitor->right;
    if (pRect->top > lprcMonitor->top)
        pRect->top = lprcMonitor->top;
    if (pRect->bottom < lprcMonitor->bottom)
        pRect->bottom = lprcMonitor->bottom;

    return TRUE;
}

LRESULT CALLBACK VBoxDnDWnd::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            int rc = OnCreate();
            if (RT_FAILURE(rc))
                return FALSE;
            return TRUE;
        }

        case WM_CLOSE:
        {
            OnDestroy();

            DestroyWindow(hWnd);
            PostQuitMessage(0);
            return 0;
        }

        case WM_LBUTTONDOWN:
            LogFlowThisFunc(("WM_LBUTTONDOWN\n"));
            mfMouseButtonDown = true;
            return 0;

        case WM_LBUTTONUP:
            LogFlowThisFunc(("WM_LBUTTONUP\n"));
            mfMouseButtonDown = false;
            return 0;

        /* Will only be called once; after the first mouse move, this
         * window will be hidden! */
        case WM_MOUSEMOVE:
        {
            LogFlowThisFunc(("WM_MOUSEMOVE: mfMouseButtonDown=%RTbool, mState=%ld\n",
                         mfMouseButtonDown, mState));

            /* Dragging not started yet? Kick it off ... */
            if (   mfMouseButtonDown
                && (mState != Dragging))
            {
                mState = Dragging;

#ifdef VBOX_DND_DEBUG_WND
                /* Delay hinding the proxy window a bit when debugging, to see
                 * whether the desired range is covered correctly. */
                RTThreadSleep(5000);
#endif
                ShowWindow(hWnd, SW_HIDE);

                int rc = VINF_SUCCESS;

                LogFlowThisFunc(("Starting drag'n drop: uAllActions=0x%x, dwOKEffects=0x%x ...\n",
                                 uAllActions, startupInfo.dwOKEffects));

                AssertPtr(startupInfo.pDataObject);
                AssertPtr(startupInfo.pDropSource);
                DWORD dwEffect;
                HRESULT hr = DoDragDrop(startupInfo.pDataObject, startupInfo.pDropSource,
                                        startupInfo.dwOKEffects, &dwEffect);
                LogFlowThisFunc(("rc=%Rhrc, dwEffect=%RI32\n", hr, dwEffect));
                switch (hr)
                {
                    case DRAGDROP_S_DROP:
                        mState = Dropped;
                        break;

                    case DRAGDROP_S_CANCEL:
                        mState = Canceled;
                        break;

                    default:
                        LogFlowThisFunc(("Drag'n drop failed with %Rhrc\n", hr));
                        mState = Canceled;
                        rc = VERR_GENERAL_FAILURE; /** @todo Find a better status code. */
                        break;
                }

                int rc2 = RTCritSectEnter(&mCritSect);
                if (RT_SUCCESS(rc2))
                {
                    startupInfo.pDropSource->Release();
                    startupInfo.pDataObject->Release();

                    RT_ZERO(startupInfo);

                    RTCritSectLeave(&mCritSect);
                }

                LogFlowThisFunc(("Drag'n drop resulted in mState=%ld, rc=%Rrc\n",
                             mState, rc));
            }

            return 0;
        }

        case WM_VBOXTRAY_DND_MESSAGE:
        {
            VBOXDNDEVENT *pEvent = (PVBOXDNDEVENT)lParam;
            AssertPtr(pEvent);

            LogFlowThisFunc(("Received uType=%RU32, uScreenID=%RU32\n",
                         pEvent->Event.uType, pEvent->Event.uScreenId));

            int rc;
            switch (pEvent->Event.uType)
            {
                case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
                {
                    LogFlowThisFunc(("HOST_DND_HG_EVT_ENTER\n"));

                    if (pEvent->Event.cbFormats)
                    {
                        RTCList<RTCString> lstFormats =
                            RTCString(pEvent->Event.pszFormats, pEvent->Event.cbFormats - 1).split("\r\n");
                        rc = OnHgEnter(lstFormats, pEvent->Event.u.a.uAllActions);
                    }
                    else
                    {
                        AssertMsgFailed(("cbFormats is 0\n"));
                        rc = VERR_INVALID_PARAMETER;
                    }

                    /* Note: After HOST_DND_HG_EVT_ENTER there immediately is a move
                     *       event, so fall through is intentional here. */
                }

                case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
                {
                    LogFlowThisFunc(("HOST_DND_HG_EVT_MOVE: %d,%d\n",
                                 pEvent->Event.u.a.uXpos, pEvent->Event.u.a.uYpos));

                    rc = OnHgMove(pEvent->Event.u.a.uXpos, pEvent->Event.u.a.uYpos, pEvent->Event.u.a.uDefAction);
                    break;
                }

                case DragAndDropSvc::HOST_DND_HG_EVT_LEAVE:
                {
                    LogFlowThisFunc(("HOST_DND_HG_EVT_LEAVE\n"));

                    rc = OnHgLeave();
                    break;
                }

                case DragAndDropSvc::HOST_DND_HG_EVT_DROPPED:
                {
                    LogFlowThisFunc(("HOST_DND_HG_EVT_DROPPED\n"));

                    rc = OnHgDrop();
                    break;
                }

                case DragAndDropSvc::HOST_DND_HG_SND_DATA:
                {
                    LogFlowThisFunc(("HOST_DND_HG_SND_DATA\n"));

                    rc = OnHgDataReceived(pEvent->Event.u.b.pvData, pEvent->Event.u.b.cbData);
                    break;
                }

                case DragAndDropSvc::HOST_DND_HG_EVT_CANCEL:
                {
                    LogFlowThisFunc(("HOST_DND_HG_EVT_CANCEL\n"));

                    rc = OnHgCancel();
                    break;
                }

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
                case DragAndDropSvc::HOST_DND_GH_REQ_PENDING:
                {
                    LogFlowThisFunc(("HOST_DND_GH_REQ_PENDING\n"));

                    break;
                }

                case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
                {
                    LogFlowThisFunc(("HOST_DND_GH_EVT_DROPPED\n"));

                    break;
                }

                case DragAndDropSvc::GUEST_DND_GH_EVT_ERROR:
                {
                    LogFlowThisFunc(("GUEST_DND_GH_EVT_ERROR\n"));

                    break;
                }
#endif
                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }

            /* Some messages require cleanup. */
            switch (pEvent->Event.uType)
            {
                case DragAndDropSvc::HOST_DND_HG_EVT_ENTER:
                case DragAndDropSvc::HOST_DND_HG_EVT_MOVE:
                case DragAndDropSvc::HOST_DND_HG_EVT_DROPPED:
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
                case DragAndDropSvc::HOST_DND_GH_EVT_DROPPED:
#endif
                {
                    if (pEvent->Event.pszFormats)
                        RTMemFree(pEvent->Event.pszFormats);
                    break;
                }

                case DragAndDropSvc::HOST_DND_HG_SND_DATA:
                {
                    if (pEvent->Event.pszFormats)
                        RTMemFree(pEvent->Event.pszFormats);
                    if (pEvent->Event.u.b.pvData)
                        RTMemFree(pEvent->Event.u.b.pvData);
                    break;
                }

                default:
                    /* Ignore. */
                    break;
            }

            LogFlowThisFunc(("Processing event %RU32 resulted in rc=%Rrc\n",
                         pEvent->Event.uType, rc));
            if (pEvent)
                RTMemFree(pEvent);
            return 0;
        }

        default:
            break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int VBoxDnDWnd::OnCreate(void)
{
    int rc = VbglR3DnDConnect(&mClientID);
    if (RT_FAILURE(rc))
    {
        LogFlowThisFunc(("Connection to host service failed, rc=%Rrc\n", rc));
        return rc;
    }

    LogFlowThisFunc(("Client ID=%RU32\n", mClientID));
    return rc;
}

void VBoxDnDWnd::OnDestroy(void)
{
    VbglR3DnDDisconnect(mClientID);
}

int VBoxDnDWnd::OnHgEnter(const RTCList<RTCString> &lstFormats, uint32_t uAllActions)
{
    reset();

    mState = Initialized;

#ifdef DEBUG
    LogFlowThisFunc(("uActions=0x%x, lstFormats=%zu: ", uAllActions, lstFormats.size()));
    for (size_t i = 0; i < lstFormats.size(); i++)
        LogFlow(("'%s' ", lstFormats.at(i).c_str()));
    LogFlow(("\n"));
#endif

    /* Save all allowed actions. */
    this->uAllActions = uAllActions;

    /*
     * Install our allowed MIME types.
     ** @todo See todo for m_sstrAllowedMimeTypes in GuestDnDImpl.cpp.
     */
    const RTCList<RTCString> lstAllowedMimeTypes = RTCList<RTCString>()
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
    this->lstAllowedFormats = lstAllowedMimeTypes;

    /*
     * Check MIME compatibility with this client.
     */
    LogFlowThisFunc(("Supported MIME types:\n"));
    for (size_t i = 0; i < lstFormats.size(); i++)
    {
        bool fSupported = lstAllowedFormats.contains(lstFormats.at(i));
        if (fSupported)
            this->lstFormats.append(lstFormats.at(i));
        LogFlowThisFunc(("\t%s: %RTbool\n", lstFormats.at(i).c_str(), fSupported));
    }

    /*
     * Prepare the startup info for DoDragDrop().
     */
    int rc = VINF_SUCCESS;
    try
    {
        /* Translate our drop actions into
         * allowed Windows drop effects. */
        startupInfo.dwOKEffects = DROPEFFECT_NONE;
        if (uAllActions)
        {
            if (uAllActions & DND_COPY_ACTION)
                startupInfo.dwOKEffects |= DROPEFFECT_COPY;
            if (uAllActions & DND_MOVE_ACTION)
                startupInfo.dwOKEffects |= DROPEFFECT_MOVE;
            if (uAllActions & DND_LINK_ACTION)
                startupInfo.dwOKEffects |= DROPEFFECT_LINK;
        }

        startupInfo.pDropSource = new VBoxDnDDropSource(this);
        startupInfo.pDataObject = new VBoxDnDDataObject();
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }

    /*
     * Prepare the proxy window.
     */
    RECT r;
    RT_ZERO(r);

    if (RT_SUCCESS(rc))
    {
        HDC hDC = GetDC(NULL);
        BOOL fRc = EnumDisplayMonitors(hDC, NULL, VBoxDnDWnd::MonitorEnumProc, (LPARAM)&r);
        if (!fRc)
            rc = VERR_NOT_FOUND;
        ReleaseDC(NULL, hDC);
    }

    if (RT_FAILURE(rc))
    {
        /* If multi-monitor enumeration failed above, try getting at least the
         * primary monitor as a fallback. */
        MONITORINFO monitor_info;
        monitor_info.cbSize = sizeof(monitor_info);
        if (GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST),
                           &monitor_info))
        {

            r = monitor_info.rcMonitor;
            rc = VINF_SUCCESS;
        }
    }

    if (RT_SUCCESS(rc))
    {
        SetWindowPos(hWnd, HWND_TOPMOST,
                     r.left,
                     r.top,
                     r.right - r.left,
                     r.bottom - r.top,
#ifdef VBOX_DND_DEBUG_WND
                     SWP_SHOWWINDOW);
#else
                     SWP_SHOWWINDOW | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOACTIVATE);
#endif
        LogFlowFunc(("Virtual screen is %ld,%ld,%ld,%ld (%ld x %ld)\n",
                     r.left, r.top, r.right, r.bottom,
                     r.right - r.left, r.bottom - r.top));
    }
    else
        LogRel(("DnD: Failed to determine virtual screen size, rc=%Rrc\n", rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int VBoxDnDWnd::OnHgMove(uint32_t u32xPos, uint32_t u32yPos, uint32_t uAction)
{
    LogFlowThisFunc(("u32xPos=%RU32, u32yPos=%RU32, uAction=0x%x\n",
                     u32xPos, u32yPos, uAction));

    /** @todo Multi-monitor setups? */
    int iScreenX = GetSystemMetrics(SM_CXSCREEN) - 1;
    int iScreenY = GetSystemMetrics(SM_CYSCREEN) - 1;

    INPUT Input[1] = { 0 };
    Input[0].type       = INPUT_MOUSE;
    Input[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
    Input[0].mi.dx      = u32xPos * (65535 / iScreenX);
    Input[0].mi.dy      = u32yPos * (65535 / iScreenY);
    SendInput(1, Input, sizeof(INPUT));

#ifdef DEBUG
    POINT p;
    GetCursorPos(&p);
    LogFlowThisFunc(("curX=%d, cury=%d\n", p.x, p.y));
#endif

    uint32_t uActionNotify = DND_IGNORE_ACTION;
    int rc = RTCritSectEnter(&mCritSect);
    if (RT_SUCCESS(rc))
    {
        if (   (Dragging == mState)
            && startupInfo.pDropSource)
            uActionNotify = startupInfo.pDropSource->GetCurrentAction();

        RTCritSectLeave(&mCritSect);
    }

    if (RT_SUCCESS(rc))
    {
        rc = VbglR3DnDHGAcknowledgeOperation(mClientID, uActionNotify);
        if (RT_FAILURE(rc))
            LogFlowThisFunc(("Acknowleding operation failed with rc=%Rrc\n", rc));
    }

    LogFlowThisFunc(("Returning uActionNotify=0x%x, rc=%Rrc\n", uActionNotify, rc));
    return rc;
}

int VBoxDnDWnd::OnHgLeave(void)
{
    LogFlowThisFunc(("mMode=%RU32, mState=%RU32\n", mMode, mState));
    LogRel(("DnD: Drag'n drop operation aborted\n"));

    reset();

    int rc = VINF_SUCCESS;

    /* Post ESC to our window to officially abort the
     * drag'n drop operation. */
    PostMessage(hWnd, WM_KEYDOWN, VK_ESCAPE, 0 /* lParam */);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int VBoxDnDWnd::OnHgDrop(void)
{
    LogFlowThisFunc(("mMode=%RU32, mState=%RU32\n", mMode, mState));

    int rc = VINF_SUCCESS;
    if (mState == Dragging)
    {
        Assert(lstFormats.size() >= 1);

        /** @todo What to do when multiple formats are available? */
        mFormatRequested = lstFormats.at(0);

        rc = RTCritSectEnter(&mCritSect);
        if (RT_SUCCESS(rc))
        {
            if (startupInfo.pDataObject)
                startupInfo.pDataObject->SetStatus(VBoxDnDDataObject::Dropping);
            else
                rc = VERR_NOT_FOUND;

            RTCritSectLeave(&mCritSect);
        }

        if (RT_SUCCESS(rc))
        {
            LogRel(("DnD: Requesting data as '%s' ...\n", mFormatRequested.c_str()));
            rc = VbglR3DnDHGRequestData(mClientID, mFormatRequested.c_str());
            if (RT_FAILURE(rc))
                LogFlowThisFunc(("Requesting data failed with rc=%Rrc\n", rc));
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int VBoxDnDWnd::OnHgDataReceived(const void *pvData, uint32_t cbData)
{
    LogFlowThisFunc(("mState=%ld, pvData=%p, cbData=%RU32\n",
                     mState, pvData, cbData));

    mState = Dropped;

    int rc = VINF_SUCCESS;
    if (pvData)
    {
        Assert(cbData);
        rc = RTCritSectEnter(&mCritSect);
        if (RT_SUCCESS(rc))
        {
            if (startupInfo.pDataObject)
                rc = startupInfo.pDataObject->Signal(mFormatRequested, pvData, cbData);
            else
                rc = VERR_NOT_FOUND;

            RTCritSectLeave(&mCritSect);
        }
    }

    int rc2 = DragRelease();
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int VBoxDnDWnd::OnHgCancel(void)
{
    int rc = RTCritSectEnter(&mCritSect);
    if (RT_SUCCESS(rc))
    {
        if (startupInfo.pDataObject)
            startupInfo.pDataObject->Abort();

        RTCritSectLeave(&mCritSect);
    }

    int rc2 = DragRelease();
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
int VBoxDnDWnd::OnGhIsDnDPending(void)
{
    return 0;
}

int VBoxDnDWnd::OnGhDropped(const RTCString &strFormat, uint32_t action)
{
    return 0;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

int VBoxDnDWnd::ProcessEvent(PVBOXDNDEVENT pEvent)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    PostMessage(hWnd, WM_VBOXTRAY_DND_MESSAGE,
                0 /* wParm */, (LPARAM)pEvent /* lParm */);

    return VINF_SUCCESS;
}

void VBoxDnDWnd::reset(void)
{
    LogFlowThisFunc(("mState=%ld\n", mState));

    lstAllowedFormats.clear();
    lstFormats.clear();
    uAllActions = DND_IGNORE_ACTION;
}

static LRESULT CALLBACK vboxDnDWndProcInstance(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR pUserData = GetWindowLongPtr(hWnd, GWLP_USERDATA);
    AssertPtrReturn(pUserData, 0);

    VBoxDnDWnd *pWnd = reinterpret_cast<VBoxDnDWnd *>(pUserData);
    if (pWnd)
        return pWnd->WndProc(hWnd, uMsg, wParam, lParam);

    return 0;
}

static LRESULT CALLBACK vboxDnDWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    /* Note: WM_NCCREATE is not the first ever message which arrives, but
     *       early enough for us. */
    if (uMsg == WM_NCCREATE)
    {
        LPCREATESTRUCT pCS = (LPCREATESTRUCT)lParam;
        AssertPtr(pCS);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCS->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)vboxDnDWndProcInstance);

        return vboxDnDWndProcInstance(hWnd, uMsg, wParam, lParam);
    }

    /* No window associated yet. */
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/**
 * Initializes drag'n drop.
 *
 * @return  IPRT status code.
 * @param   pEnv                        The DnD service's environment.
 * @param   ppInstance                  The instance pointer which refer to this object.
 * @param   pfStartThread               Pointer to flag whether the DnD service can be started or not.
 */
int VBoxDnDInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);
    /** ppInstance not used here. */
    AssertPtrReturn(pfStartThread, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    *pfStartThread = false;

    PVBOXDNDCONTEXT pCtx = &gCtx;

    int rc;

    /* Create the proxy window. At the moment we
     * only support one window at a time. */
    VBoxDnDWnd *pWnd = NULL;
    try
    {
        pWnd = new VBoxDnDWnd();
        rc = pWnd->Initialize(pCtx);

        /* Add proxy window to our proxy windows list. */
        if (RT_SUCCESS(rc))
            pCtx->lstWnd.append(pWnd);
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
        rc = RTSemEventCreate(&pCtx->hEvtQueueSem);
    if (RT_SUCCESS(rc))
    {
        /* Assign service environment to our context. */
        gCtx.pEnv = pEnv;
    }

    if (RT_SUCCESS(rc))
    {
        *ppInstance = pCtx;
        *pfStartThread = true;

        LogRel(("DnD: Drag'n drop service successfully started\n"));
        return VINF_SUCCESS;
    }

    LogRel(("DnD: Initializing drag'n drop service failed with rc=%Rrc\n", rc));
    return rc;
}

void VBoxDnDStop(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    AssertPtrReturnVoid(pEnv);
    AssertPtrReturnVoid(pInstance);

    LogFunc(("Stopping pInstance=%p\n", pInstance));

    PVBOXDNDCONTEXT pCtx = (PVBOXDNDCONTEXT)pInstance;
    AssertPtr(pCtx);

    /* Set shutdown indicator. */
    ASMAtomicWriteBool(&pCtx->fShutdown, true);

    /** @todo Notify / wait for HGCM thread! */
}

void VBoxDnDDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    AssertPtr(pEnv);
    AssertPtr(pInstance);

    LogFunc(("Destroying pInstance=%p\n", pInstance));

    PVBOXDNDCONTEXT pCtx = (PVBOXDNDCONTEXT)pInstance;
    AssertPtr(pCtx);

    int rc = VINF_SUCCESS;

    LogFunc(("Destroyed pInstance=%p, rc=%Rrc\n",
             pInstance, rc));
}

unsigned __stdcall VBoxDnDThread(void *pInstance)
{
    LogFlowFunc(("pInstance=%p\n", pInstance));

    PVBOXDNDCONTEXT pCtx = (PVBOXDNDCONTEXT)pInstance;
    AssertPtr(pCtx);

    uint32_t uClientID;
    int rc = VbglR3DnDConnect(&uClientID);
    if (RT_FAILURE(rc))
        return rc;

    /** @todo At the moment we only have one DnD proxy window. */
    Assert(pCtx->lstWnd.size() == 1);
    VBoxDnDWnd *pWnd = pCtx->lstWnd.first();
    AssertPtr(pWnd);

    do
    {
        PVBOXDNDEVENT pEvent = (PVBOXDNDEVENT)RTMemAlloc(sizeof(VBOXDNDEVENT));
        if (!pEvent)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        /* Note: pEvent will be free'd by the consumer later. */

        rc = VbglR3DnDProcessNextMessage(uClientID, &pEvent->Event);
        LogFlowFunc(("VbglR3DnDProcessNextMessage returned rc=%Rrc\n", rc));

        if (ASMAtomicReadBool(&pCtx->fShutdown))
            break;

        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("Received new event, type=%RU32\n", pEvent->Event.uType));

            int rc2 = pWnd->ProcessEvent(pEvent);
            if (RT_FAILURE(rc2))
                LogFlowFunc(("Processing event failed with rc=%Rrc\n", rc2));
        }
        else if (rc == VERR_CANCELLED)
        {
            int rc2 = pWnd->OnHgCancel();
            if (RT_FAILURE(rc2))
                LogFlowFunc(("Cancelling failed with rc=%Rrc\n", rc2));
        }
        else
            LogFlowFunc(("Processing next message failed with rc=%Rrc\n", rc));

        if (ASMAtomicReadBool(&pCtx->fShutdown))
            break;

    } while (true);

    LogFlowFunc(("Shutting down ...\n"));

    VbglR3DnDDisconnect(uClientID);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

