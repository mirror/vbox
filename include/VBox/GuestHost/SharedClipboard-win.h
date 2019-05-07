/** @file
 * Shared Clipboard - Common Guest and Host Code, for Windows OSes.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VBOX_INCLUDED_GuestHost_SharedClipboard_win_h
#define VBOX_INCLUDED_GuestHost_SharedClipboard_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/win/windows.h>

# ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
#  include <iprt/cpp/ministring.h> /* For RTCString. */
# endif

#ifndef WM_CLIPBOARDUPDATE
# define WM_CLIPBOARDUPDATE 0x031D
#endif

#define VBOX_CLIPBOARD_WNDCLASS_NAME        "VBoxSharedClipboardClass"

#define VBOX_CLIPBOARD_WIN_REGFMT_HTML      "VBox HTML Format"

/** Default timeout (in ms) for passing down messages down the clipboard chain. */
#define VBOX_CLIPBOARD_CBCHAIN_TIMEOUT_MS   5000

/** Sets announced clipboard formats from the host. */
#define VBOX_CLIPBOARD_WM_SET_FORMATS       WM_USER
/** Reads data from the clipboard and sends it to the host. */
#define VBOX_CLIPBOARD_WM_READ_DATA         WM_USER + 1

/* Dynamically load clipboard functions from User32.dll. */
typedef BOOL WINAPI FNADDCLIPBOARDFORMATLISTENER(HWND);
typedef FNADDCLIPBOARDFORMATLISTENER *PFNADDCLIPBOARDFORMATLISTENER;

typedef BOOL WINAPI FNREMOVECLIPBOARDFORMATLISTENER(HWND);
typedef FNREMOVECLIPBOARDFORMATLISTENER *PFNREMOVECLIPBOARDFORMATLISTENER;

/**
 * Structure for keeping function pointers for the new clipboard API.
 * If the new API is not available, those function pointer are NULL.
 */
typedef struct VBOXCLIPBOARDWINAPINEW
{
    PFNADDCLIPBOARDFORMATLISTENER    pfnAddClipboardFormatListener;
    PFNREMOVECLIPBOARDFORMATLISTENER pfnRemoveClipboardFormatListener;
} VBOXCLIPBOARDWINAPINEW, *PVBOXCLIPBOARDWINAPINEW;

/**
 * Structure for keeping variables which are needed to drive the old clipboard API.
 */
typedef struct VBOXCLIPBOARDWINAPIOLD
{
    /** Timer ID for the refresh timer. */
    UINT                   timerRefresh;
    /** Whether "pinging" the clipboard chain currently is in progress or not. */
    bool                   fCBChainPingInProcess;
} VBOXCLIPBOARDWINAPIOLD, *PVBOXCLIPBOARDWINAPIOLD;

typedef struct VBOXCLIPBOARDWINCTX
{
    /** Window handle of our (invisible) clipbaord window. */
    HWND                   hWnd;
    /** Window handle which is next to us in the clipboard chain. */
    HWND                   hWndNextInChain;
    /** Structure for maintaining the new clipboard API. */
    VBOXCLIPBOARDWINAPINEW newAPI;
    /** Structure for maintaining the old clipboard API. */
    VBOXCLIPBOARDWINAPIOLD oldAPI;
} VBOXCLIPBOARDWINCTX, *PVBOXCLIPBOARDWINCTX;

int VBoxClipboardWinOpen(HWND hWnd);
int VBoxClipboardWinClose(void);
int VBoxClipboardWinClear(void);
int VBoxClipboardWinCheckAndInitNewAPI(PVBOXCLIPBOARDWINAPINEW pAPI);
bool VBoxClipboardWinIsNewAPI(PVBOXCLIPBOARDWINAPINEW pAPI);
int VBoxClipboardWinAddToCBChain(PVBOXCLIPBOARDWINCTX pCtx);
int VBoxClipboardWinRemoveFromCBChain(PVBOXCLIPBOARDWINCTX pCtx);
VOID CALLBACK VBoxClipboardWinChainPingProc(HWND hWnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult);
int VBoxClipboardWinGetFormats(PVBOXCLIPBOARDWINCTX pCtx, uint32_t *puFormats);

# ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
class VBoxClipboardWinDataObject : public IDataObject
{
public:

    enum Status
    {
        Uninitialized = 0,
        Initialized,
        Dropping,
        Dropped,
        Aborted
    };

public:

    VBoxClipboardWinDataObject(LPFORMATETC pFormatEtc = NULL, LPSTGMEDIUM pStgMed = NULL, ULONG cFormats = 0);
    virtual ~VBoxClipboardWinDataObject(void);

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IDataObject methods. */

    STDMETHOD(GetData)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium);
    STDMETHOD(GetDataHere)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium);
    STDMETHOD(QueryGetData)(LPFORMATETC pFormatEtc);
    STDMETHOD(GetCanonicalFormatEtc)(LPFORMATETC pFormatEct,  LPFORMATETC pFormatEtcOut);
    STDMETHOD(SetData)(LPFORMATETC pFormatEtc, LPSTGMEDIUM pMedium, BOOL fRelease);
    STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc);
    STDMETHOD(DAdvise)(LPFORMATETC pFormatEtc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
    STDMETHOD(DUnadvise)(DWORD dwConnection);
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppEnumAdvise);

public:

    static const char* ClipboardFormatToString(CLIPFORMAT fmt);

    int Abort(void);
    void SetStatus(Status status);
    int Signal(const RTCString &strFormat, const void *pvData, uint32_t cbData);

protected:

    bool LookupFormatEtc(LPFORMATETC pFormatEtc, ULONG *puIndex);
    static HGLOBAL MemDup(HGLOBAL hMemSource);
    void RegisterFormat(LPFORMATETC pFormatEtc, CLIPFORMAT clipFormat, TYMED tyMed = TYMED_HGLOBAL,
                        LONG lindex = -1, DWORD dwAspect = DVASPECT_CONTENT, DVTARGETDEVICE *pTargetDevice = NULL);

    Status      mStatus;
    LONG        mRefCount;
    ULONG       mcFormats;
    LPFORMATETC mpFormatEtc;
    LPSTGMEDIUM mpStgMedium;
    RTSEMEVENT  mEventDropped;
    RTCString   mstrFormat;
    void       *mpvData;
    uint32_t    mcbData;
};

class VBoxClipboardWinEnumFormatEtc : public IEnumFORMATETC
{
public:

    VBoxClipboardWinEnumFormatEtc(LPFORMATETC pFormatEtc, ULONG cFormats);
    virtual ~VBoxClipboardWinEnumFormatEtc(void);

public:

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    STDMETHOD(Next)(ULONG cFormats, LPFORMATETC pFormatEtc, ULONG *pcFetched);
    STDMETHOD(Skip)(ULONG cFormats);
    STDMETHOD(Reset)(void);
    STDMETHOD(Clone)(IEnumFORMATETC **ppEnumFormatEtc);

public:

    static void CopyFormat(LPFORMATETC pFormatDest, LPFORMATETC pFormatSource);
    static HRESULT CreateEnumFormatEtc(UINT cFormats, LPFORMATETC pFormatEtc, IEnumFORMATETC **ppEnumFormatEtc);

private:

    LONG        m_lRefCount;
    ULONG       m_nIndex;
    ULONG       m_nNumFormats;
    LPFORMATETC m_pFormatEtc;
};

#  if 0
class VBoxClipboardWinStreamImpl : public IDataObject
{

};
#  endif

# endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */
#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_win_h */

