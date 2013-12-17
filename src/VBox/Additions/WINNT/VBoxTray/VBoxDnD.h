/* $Id$ */
/** @file
 * VBoxDnD.h - Windows-specific bits of the drag'n drop service.
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

#ifndef __VBOXTRAYDND__H
#define __VBOXTRAYDND__H

#include <iprt/cpp/mtlist.h>
#include <iprt/cpp/ministring.h>

int                VBoxDnDInit    (const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread);
unsigned __stdcall VBoxDnDThread  (void *pInstance);
void               VBoxDnDStop    (const VBOXSERVICEENV *pEnv, void *pInstance);
void               VBoxDnDDestroy (const VBOXSERVICEENV *pEnv, void *pInstance);

class VBoxDnDWnd;

class VBoxDnDDataObject : public IDataObject
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

    VBoxDnDDataObject(FORMATETC *pFormatEtc = NULL, STGMEDIUM *pStgMed = NULL, ULONG cFormats = 0);
    virtual ~VBoxDnDDataObject(void);

public:

    static int CreateDataObject(FORMATETC *pFormatEtc, STGMEDIUM *pStgMeds,
                                ULONG cFormats, IDataObject **ppDataObject);
public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IDataObject methods. */

    STDMETHOD(GetData)(FORMATETC *pFormatEtc, STGMEDIUM *pMedium);
    STDMETHOD(GetDataHere)(FORMATETC *pFormatEtc, STGMEDIUM *pMedium);
    STDMETHOD(QueryGetData)(FORMATETC *pFormatEtc);
    STDMETHOD(GetCanonicalFormatEtc)(FORMATETC *pFormatEct,  FORMATETC *pFormatEtcOut);
    STDMETHOD(SetData)(FORMATETC *pFormatEtc, STGMEDIUM *pMedium, BOOL fRelease);
    STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppEnumFormatEtc);
    STDMETHOD(DAdvise)(FORMATETC *pFormatEtc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
    STDMETHOD(DUnadvise)(DWORD dwConnection);
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppEnumAdvise);

public:

    static const char* ClipboardFormatToString(CLIPFORMAT fmt);

    int Abort(void);
    void SetStatus(Status status);
    int Signal(const RTCString &strFormat, const void *pvData, uint32_t cbData);

protected:

    bool LookupFormatEtc(FORMATETC *pFormatEtc, ULONG *puIndex);
    static HGLOBAL MemDup(HGLOBAL hMemSource);
    void RegisterFormat(FORMATETC *pFormatEtc, CLIPFORMAT clipFormat, TYMED tyMed = TYMED_HGLOBAL,
                        LONG lindex = -1, DWORD dwAspect = DVASPECT_CONTENT, DVTARGETDEVICE *pTargetDevice = NULL);

    Status     mStatus;
    LONG       mRefCount;
    ULONG      mcFormats;
    FORMATETC *mpFormatEtc;
    STGMEDIUM *mpStgMedium;
    RTSEMEVENT mSemEvent;
    RTCString  mstrFormat;
    void      *mpvData;
    uint32_t   mcbData;
};

class VBoxDnDDropSource : public IDropSource
{
public:

    VBoxDnDDropSource(VBoxDnDWnd *pThis);
    virtual ~VBoxDnDDropSource(void);

public:

    static int CreateDropSource(VBoxDnDWnd *pParent, IDropSource **ppDropSource);

public:

    uint32_t GetCurrentAction(void) { return muCurAction; }

public: /* IUnknown methods. */

    STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

public: /* IDropSource methods. */

    STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD dwKeyState);
    STDMETHOD(GiveFeedback)(DWORD dwEffect);

protected:

    LONG mRefCount;
    VBoxDnDWnd *mpWndParent;
    uint32_t mClientID;
    DWORD mdwCurEffect;
    uint32_t muCurAction;
};

class VBoxDnDEnumFormatEtc : public IEnumFORMATETC
{
public:

	VBoxDnDEnumFormatEtc(FORMATETC *pFormatEtc, ULONG cFormats);
	virtual ~VBoxDnDEnumFormatEtc(void);

public:

	STDMETHOD(QueryInterface)(REFIID iid, void ** ppvObject);
	STDMETHOD_(ULONG, AddRef)(void);
	STDMETHOD_(ULONG, Release)(void);

	STDMETHOD(Next)(ULONG cFormats, FORMATETC *pFormatEtc, ULONG *pcFetched);
	STDMETHOD(Skip)(ULONG cFormats);
	STDMETHOD(Reset)(void);
	STDMETHOD(Clone)(IEnumFORMATETC ** ppEnumFormatEtc);

public:

    static void CopyFormat(FORMATETC *pFormatDest, FORMATETC *pFormatSource);
    static HRESULT CreateEnumFormatEtc(UINT cFormats, FORMATETC *pFormatEtc, IEnumFORMATETC **ppEnumFormatEtc);

private:

	LONG		m_lRefCount;
	ULONG		m_nIndex;
	ULONG		m_nNumFormats;
	FORMATETC * m_pFormatEtc;
};
#endif /* __VBOXTRAYDND__H */

