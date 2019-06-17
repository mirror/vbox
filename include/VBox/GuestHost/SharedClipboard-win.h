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

#include <VBox/GuestHost/SharedClipboard.h>

# ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
#  include <iprt/cpp/ministring.h> /* For RTCString. */
#  include <iprt/win/shlobj.h> /* For DROPFILES and friends. */
#  include <oleidl.h>

# include <VBox/GuestHost/SharedClipboard-uri.h>
# endif

#ifndef WM_CLIPBOARDUPDATE
# define WM_CLIPBOARDUPDATE 0x031D
#endif

#define VBOX_CLIPBOARD_WNDCLASS_NAME         "VBoxSharedClipboardClass"

/** See: https://docs.microsoft.com/en-us/windows/desktop/dataxchg/html-clipboard-format
 *       Do *not* change the name, as this will break compatbility with other (legacy) applications! */
#define VBOX_CLIPBOARD_WIN_REGFMT_HTML       "HTML Format"

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
typedef struct _VBOXCLIPBOARDWINAPINEW
{
    PFNADDCLIPBOARDFORMATLISTENER    pfnAddClipboardFormatListener;
    PFNREMOVECLIPBOARDFORMATLISTENER pfnRemoveClipboardFormatListener;
} VBOXCLIPBOARDWINAPINEW, *PVBOXCLIPBOARDWINAPINEW;

/**
 * Structure for keeping variables which are needed to drive the old clipboard API.
 */
typedef struct _VBOXCLIPBOARDWINAPIOLD
{
    /** Timer ID for the refresh timer. */
    UINT                   timerRefresh;
    /** Whether "pinging" the clipboard chain currently is in progress or not. */
    bool                   fCBChainPingInProcess;
} VBOXCLIPBOARDWINAPIOLD, *PVBOXCLIPBOARDWINAPIOLD;

/**
 * Structure for maintaining a Shared Clipboard context on Windows platforms.
 */
typedef struct _VBOXCLIPBOARDWINCTX
{
    /** Window handle of our (invisible) clipbaord window. */
    HWND                        hWnd;
    /** Window handle which is next to us in the clipboard chain. */
    HWND                        hWndNextInChain;
    /** Window handle of the clipboard owner *if* we are the owner. */
    HWND                        hWndClipboardOwnerUs;
    /** Structure for maintaining the new clipboard API. */
    VBOXCLIPBOARDWINAPINEW      newAPI;
    /** Structure for maintaining the old clipboard API. */
    VBOXCLIPBOARDWINAPIOLD      oldAPI;
} VBOXCLIPBOARDWINCTX, *PVBOXCLIPBOARDWINCTX;

int VBoxClipboardWinOpen(HWND hWnd);
int VBoxClipboardWinClose(void);
int VBoxClipboardWinClear(void);

int VBoxClipboardWinCheckAndInitNewAPI(PVBOXCLIPBOARDWINAPINEW pAPI);
bool VBoxClipboardWinIsNewAPI(PVBOXCLIPBOARDWINAPINEW pAPI);

int VBoxClipboardWinChainAdd(PVBOXCLIPBOARDWINCTX pCtx);
int VBoxClipboardWinChainRemove(PVBOXCLIPBOARDWINCTX pCtx);
VOID CALLBACK VBoxClipboardWinChainPingProc(HWND hWnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult);
LRESULT VBoxClipboardWinChainPassToNext(PVBOXCLIPBOARDWINCTX pWinCtx, UINT msg, WPARAM wParam, LPARAM lParam);

VBOXCLIPBOARDFORMAT VBoxClipboardWinClipboardFormatToVBox(UINT uFormat);
int VBoxClipboardWinGetFormats(PVBOXCLIPBOARDWINCTX pCtx, PVBOXCLIPBOARDFORMAT pfFormats);

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int VBoxClipboardWinDropFilesToStringList(DROPFILES *pDropFiles, char **ppszData, size_t *pcbData);
#endif

int VBoxClipboardWinGetCFHTMLHeaderValue(const char *pszSrc, const char *pszOption, uint32_t *puValue);
bool VBoxClipboardWinIsCFHTML(const char *pszSource);
int VBoxClipboardWinConvertCFHTMLToMIME(const char *pszSource, const uint32_t cch, char **ppszOutput, uint32_t *pcbOutput);
int VBoxClipboardWinConvertMIMEToCFHTML(const char *pszSource, size_t cb, char **ppszOutput, uint32_t *pcbOutput);

void VBoxClipboardWinDump(const void *pv, size_t cb, VBOXCLIPBOARDFORMAT u32Format);

LRESULT VBoxClipboardWinHandleWMChangeCBChain(PVBOXCLIPBOARDWINCTX pWinCtx, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int VBoxClipboardWinHandleWMDestroy(PVBOXCLIPBOARDWINCTX pWinCtx);
int VBoxClipboardWinHandleWMRenderAllFormats(PVBOXCLIPBOARDWINCTX pWinCtx, HWND hWnd);
int VBoxClipboardWinHandleWMTimer(PVBOXCLIPBOARDWINCTX pWinCtx);

int VBoxClipboardWinAnnounceFormats(PVBOXCLIPBOARDWINCTX pWinCtx, VBOXCLIPBOARDFORMATS fFormats);
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
int VBoxClipboardWinURIReadMain(PVBOXCLIPBOARDWINCTX pWinCtx, PSHAREDCLIPBOARDURICTX pURICtx,
                                PSHAREDCLIPBOARDPROVIDERCREATIONCTX pProviderCtx, VBOXCLIPBOARDFORMATS fFormats);
#endif

# ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
class SharedClipboardURIList;
#  ifndef FILEGROUPDESCRIPTOR
class FILEGROUPDESCRIPTOR;
#  endif

class VBoxClipboardWinDataObject : public IDataObject //, public IDataObjectAsyncCapability
{
public:

    enum Status
    {
        Uninitialized = 0,
        Initialized
    };

    enum FormatIndex
    {
        /** File descriptor, ANSI version. */
        FormatIndex_FileDescriptorA = 0,
        /** File descriptor, Unicode version. */
        FormatIndex_FileDescriptorW,
        /** File contents. */
        FormatIndex_FileContents
    };

public:

    VBoxClipboardWinDataObject(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                               LPFORMATETC pFormatEtc = NULL, LPSTGMEDIUM pStgMed = NULL, ULONG cFormats = 0);
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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_WIN_ASYNC
public: /* IDataObjectAsyncCapability methods. */

    STDMETHOD(EndOperation)(HRESULT hResult, IBindCtx* pbcReserved, DWORD dwEffects);
    STDMETHOD(GetAsyncMode)(BOOL* pfIsOpAsync);
    STDMETHOD(InOperation)(BOOL* pfInAsyncOp);
    STDMETHOD(SetAsyncMode)(BOOL fDoOpAsync);
    STDMETHOD(StartOperation)(IBindCtx* pbcReserved);
#endif /* VBOX_WITH_SHARED_CLIPBOARD_WIN_ASYNC */

public:

    int Init(void);
    void OnTransferComplete(int rc = VINF_SUCCESS);
    void OnTransferCanceled();

public:

    static const char* ClipboardFormatToString(CLIPFORMAT fmt);

protected:

    static int Thread(RTTHREAD hThread, void *pvUser);

    int copyToHGlobal(const void *pvData, size_t cbData, UINT fFlags, HGLOBAL *phGlobal);
    int createFileGroupDescriptorFromURIList(const SharedClipboardURIList &URIList, bool fUnicode, HGLOBAL *phGlobal);

    bool lookupFormatEtc(LPFORMATETC pFormatEtc, ULONG *puIndex);
    void registerFormat(LPFORMATETC pFormatEtc, CLIPFORMAT clipFormat, TYMED tyMed = TYMED_HGLOBAL,
                        LONG lindex = -1, DWORD dwAspect = DVASPECT_CONTENT, DVTARGETDEVICE *pTargetDevice = NULL);

protected:

    Status                      m_enmStatus;
    LONG                        m_lRefCount;
    ULONG                       m_cFormats;
    LPFORMATETC                 m_pFormatEtc;
    LPSTGMEDIUM                 m_pStgMedium;
    PSHAREDCLIPBOARDURITRANSFER m_pTransfer;
    IStream                    *m_pStream;
    ULONG                       m_uObjIdx;
};

class VBoxClipboardWinEnumFormatEtc : public IEnumFORMATETC
{
public:

    VBoxClipboardWinEnumFormatEtc(LPFORMATETC pFormatEtc, ULONG cFormats);
    virtual ~VBoxClipboardWinEnumFormatEtc(void);

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IEnumFORMATETC methods. */

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

/**
 * Own IStream implementation to implement file-based clipboard operations
 * through HGCM. Needed on Windows hosts and guests.
 */
class VBoxClipboardWinStreamImpl : public IStream
{
public:

    VBoxClipboardWinStreamImpl(VBoxClipboardWinDataObject *pParent,
                               PSHAREDCLIPBOARDURITRANSFER pTransfer, uint64_t uObjIdx);
    virtual ~VBoxClipboardWinStreamImpl(void);

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IStream methods. */

    STDMETHOD(Clone)(IStream** ppStream);
    STDMETHOD(Commit)(DWORD dwFrags);
    STDMETHOD(CopyTo)(IStream* pDestStream, ULARGE_INTEGER nBytesToCopy, ULARGE_INTEGER* nBytesRead, ULARGE_INTEGER* nBytesWritten);
    STDMETHOD(LockRegion)(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes,DWORD dwFlags);
    STDMETHOD(Read)(void* pvBuffer, ULONG nBytesToRead, ULONG* nBytesRead);
    STDMETHOD(Revert)(void);
    STDMETHOD(Seek)(LARGE_INTEGER nMove, DWORD dwOrigin, ULARGE_INTEGER* nNewPos);
    STDMETHOD(SetSize)(ULARGE_INTEGER nNewSize);
    STDMETHOD(Stat)(STATSTG* statstg, DWORD dwFlags);
    STDMETHOD(UnlockRegion)(ULARGE_INTEGER nStart, ULARGE_INTEGER nBytes, DWORD dwFlags);
    STDMETHOD(Write)(const void* pvBuffer, ULONG nBytesToRead, ULONG* nBytesRead);

public: /* Own methods. */

    static HRESULT Create(VBoxClipboardWinDataObject *pParent,
                          PSHAREDCLIPBOARDURITRANSFER pTransfer, uint64_t uObjIdx, IStream **ppStream);

private:

    /** Pointer to the parent data object. */
    VBoxClipboardWinDataObject *m_pParent;
    /** The stream object's current reference count. */
    LONG                        m_lRefCount;
    /** Pointer to the associated URI transfer object. */
    PSHAREDCLIPBOARDURITRANSFER m_pURITransfer;
    /** Index of the object to handle within the associated URI transfer object. */
    uint64_t                    m_uObjIdx;
};

/**
 * Class for Windows-specifics for maintaining a single URI transfer.
 * Set as pvUser / cbUser in SHAREDCLIPBOARDURICTX.
 */
class SharedClipboardWinURITransferCtx
{
public:
    SharedClipboardWinURITransferCtx()
        : pDataObj(NULL) { }

    virtual ~SharedClipboardWinURITransferCtx()
    {
        if (pDataObj)
            delete pDataObj;
    }

    /** Pointer to data object to use for this transfer.
     *  Can be NULL if not being used. */
    VBoxClipboardWinDataObject *pDataObj;
};
# endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */
#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_win_h */

