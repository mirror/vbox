
/* $Id$ */
/** @file
 * VirtualBox Main - Guest file handling.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTFILEIMPL
#define ____H_GUESTFILEIMPL

#include "VirtualBoxBase.h"
#include "EventImpl.h"

#include "GuestFsObjInfoImpl.h"
#include "GuestCtrlImplPrivate.h"

class Console;
class GuestSession;
class GuestProcess;

/**
 * TODO
 */
class ATL_NO_VTABLE GuestFile :
    public VirtualBoxBase,
    public GuestObject,
    VBOX_SCRIPTABLE_IMPL(IGuestFile)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestFile, IGuestFile)
    DECLARE_NOT_AGGREGATABLE(GuestFile)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestFile)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestFile)
        COM_INTERFACE_ENTRY(IFile)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestFile)

    int     init(Console *pConsole, GuestSession *pSession, ULONG uFileID, const GuestFileOpenInfo &openInfo);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IFile interface.
     * @{ */
    STDMETHOD(COMGETTER(CreationMode))(ULONG *aCreationMode);
    STDMETHOD(COMGETTER(Disposition))(ULONG *aDisposition);
    STDMETHOD(COMGETTER(EventSource))(IEventSource ** aEventSource);
    STDMETHOD(COMGETTER(FileName))(BSTR *aFileName);
    STDMETHOD(COMGETTER(InitialSize))(LONG64 *aInitialSize);
    STDMETHOD(COMGETTER(Offset))(LONG64 *aOffset);
    STDMETHOD(COMGETTER(OpenMode))(ULONG *aOpenMode);
    STDMETHOD(COMGETTER(Status))(FileStatus_T *aStatus);

    STDMETHOD(Close)(void);
    STDMETHOD(QueryInfo)(IFsObjInfo **aInfo);
    STDMETHOD(Read)(ULONG aToRead, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(ReadAt)(LONG64 aOffset, ULONG aToRead, ULONG aTimeoutMS, ComSafeArrayOut(BYTE, aData));
    STDMETHOD(Seek)(LONG64 aOffset, FileSeekType_T aType);
    STDMETHOD(SetACL)(IN_BSTR aACL);
    STDMETHOD(Write)(ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    STDMETHOD(WriteAt)(LONG64 aOffset, ComSafeArrayIn(BYTE, aData), ULONG aTimeoutMS, ULONG *aWritten);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    int             callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    int             closeFile(int *pGuestRc);
    EventSource    *getEventSource(void) { return mEventSource; }
    static Utf8Str  guestErrorToString(int guestRc);
    int             onFileNotify(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int             onGuestDisconnected(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int             openFile(uint32_t uTimeoutMS, int *pGuestRc);
    int             readData(uint32_t uSize, uint32_t uTimeoutMS, void* pvData, uint32_t cbData, uint32_t* pcbRead);
    int             readDataAt(uint64_t uOffset, uint32_t uSize, uint32_t uTimeoutMS, void* pvData, size_t cbData, size_t* pcbRead);
    int             seekAt(uint64_t uOffset, GUEST_FILE_SEEKTYPE eSeekType, uint32_t uTimeoutMS, uint64_t *puOffset);
    static HRESULT  setErrorExternal(VirtualBoxBase *pInterface, int guestRc);
    int             setFileStatus(FileStatus_T fileStatus, int fileRc);
    int             waitForOffsetChange(GuestWaitEvent *pEvent, uint32_t uTimeoutMS, uint64_t *puOffset);
    int             waitForRead(GuestWaitEvent *pEvent, uint32_t uTimeoutMS, void *pvData, size_t cbData, uint32_t *pcbRead);
    int             waitForStatusChange(GuestWaitEvent *pEvent, uint32_t uTimeoutMS, FileStatus_T *pFileStatus, int *pGuestRc);
    int             waitForWrite(GuestWaitEvent *pEvent, uint32_t uTimeoutMS, uint32_t *pcbWritten);
    int             writeData(uint32_t uTimeoutMS, void *pvData, uint32_t cbData, uint32_t *pcbWritten);
    int             writeDataAt(uint64_t uOffset, uint32_t uTimeoutMS, void *pvData, uint32_t cbData, uint32_t *pcbWritten);
    /** @}  */

private:

    /** This can safely be used without holding any locks.
     * An AutoCaller suffices to prevent it being destroy while in use and
     * internally there is a lock providing the necessary serialization. */
    const ComObjPtr<EventSource> mEventSource;

    struct Data
    {
        /** The file's open info. */
        GuestFileOpenInfo       mOpenInfo;
        /** The file's initial size on open. */
        uint64_t                mInitialSize;
        /** The file's internal ID. */
        uint32_t                mID;
        /** The current file status. */
        FileStatus_T            mStatus;
        /** The last returned process status
         *  returned from the guest side. */
        int                     mLastError;
        /** The file's current offset. */
        uint64_t                mOffCurrent;
    } mData;
};

#endif /* !____H_GUESTFILEIMPL */

