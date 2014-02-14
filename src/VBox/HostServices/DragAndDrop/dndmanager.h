/** @file
 * Drag and Drop manager.
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

#ifndef ___VBox_HostService_DnD_dndmanager_h
#define ___VBox_HostService_DnD_dndmanager_h

#include <VBox/GuestHost/DragAndDrop.h>
#include <VBox/HostServices/Service.h>
#include <VBox/HostServices/DragAndDropSvc.h>

#include <iprt/cpp/ministring.h>
#include <iprt/cpp/list.h>

typedef DECLCALLBACK(int) FNDNDPROGRESS(unsigned uPercentage, uint32_t uState, int rc, void *pvUser);
typedef FNDNDPROGRESS *PFNDNDPROGRESS;

/**
 * DnD message class. This class forms the base of all other more specialized
 * message classes.
 */
class DnDMessage
{
public:

    DnDMessage(void)
        : m_pNextMsg(NULL)
    {
    }

    virtual ~DnDMessage(void)
    {
        clearNextMsg();
    }

    virtual HGCM::Message* nextHGCMMessage(void)
    {
        return m_pNextMsg;
    }

    virtual int currentMessageInfo(uint32_t *puMsg, uint32_t *pcParms)
    {
        AssertPtrReturn(puMsg, VERR_INVALID_POINTER);
        AssertPtrReturn(pcParms, VERR_INVALID_POINTER);

        if (!m_pNextMsg)
            return VERR_NO_DATA;

        *puMsg = m_pNextMsg->message();
        *pcParms = m_pNextMsg->paramsCount();

        return VINF_SUCCESS;
    }

    virtual int currentMessage(uint32_t uMsg, uint32_t cParms,
                               VBOXHGCMSVCPARM paParms[])
    {
        if (!m_pNextMsg)
            return VERR_NO_DATA;

        int rc = m_pNextMsg->getData(uMsg, cParms, paParms);

        clearNextMsg();

        return rc;
    }

    virtual void clearNextMsg(void)
    {
        if (m_pNextMsg)
        {
            delete m_pNextMsg;
            m_pNextMsg = NULL;
        }
    }

    virtual bool isMessageWaiting(void) const { return m_pNextMsg != NULL; }

protected:

    HGCM::Message *m_pNextMsg;
};

/**
 * DnD message class for generic messages which didn't need any special
 * handling.
 */
class DnDGenericMessage: public DnDMessage
{
public:
    DnDGenericMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
    {
        m_pNextMsg = new HGCM::Message(uMsg, cParms, paParms);
    }
};

/**
 * DnD message class for informing the guest about a new drop data event.
 */
class DnDHGSendDataMessage: public DnDMessage
{
public:

    DnDHGSendDataMessage(uint32_t uMsg, uint32_t cParms,
                         VBOXHGCMSVCPARM paParms[],
                         PFNDNDPROGRESS pfnProgressCallback, void *pvProgressUser);

    virtual ~DnDHGSendDataMessage(void);

    HGCM::Message* nextHGCMMessage(void);
    int currentMessageInfo(uint32_t *puMsg, uint32_t *pcParms);
    int currentMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    bool isMessageWaiting(void) const { return !!m_pNextPathMsg; }

protected:

    static DECLCALLBACK(int) progressCallback(size_t cbDone, void *pvUser);

    DnDMessage         *m_pNextPathMsg;

    DnDURIList          m_lstURI;
    /* Total message size (in bytes). */
    size_t              m_cbTotal;
    /* Transferred message size (in bytes). */
    size_t              m_cbTransfered;

    PFNDNDPROGRESS      m_pfnProgressCallback;
    void               *m_pvProgressUser;
};

/**
 * DnD message class for informing the guest to cancel any currently and
 * pending activities.
 */
class DnDHGCancelMessage: public DnDMessage
{
public:

    DnDHGCancelMessage(void)
    {
        m_pNextMsg
            = new HGCM::Message(DragAndDropSvc::HOST_DND_HG_EVT_CANCEL,
                                0 /* cParms */, 0 /* aParms */);
    }
};

/**
 * DnD manager. Manage creation and queuing of messages for the various DnD
 * messages types.
 */
class DnDManager
{
public:

    DnDManager(PFNDNDPROGRESS pfnProgressCallback, void *pvProgressUser)
        : m_pCurMsg(0)
        , m_fOpInProcess(false)
        , m_pfnProgressCallback(pfnProgressCallback)
        , m_pvProgressUser(pvProgressUser)
    {}

    virtual ~DnDManager(void)
    {
        clear();
    }

    int addMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    HGCM::Message *nextHGCMMessage(void);
    int nextMessageInfo(uint32_t *puMsg, uint32_t *pcParms);
    int nextMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    void clear(void);

    bool hasActiveOperation(void) const { return m_fOpInProcess; }

private:
    DnDMessage           *m_pCurMsg;
    RTCList<DnDMessage*>  m_dndMessageQueue;

    bool                  m_fOpInProcess;

    /* Progress stuff */
    PFNDNDPROGRESS        m_pfnProgressCallback;
    void                 *m_pvProgressUser;
};
#endif /* ___VBox_HostService_DnD_dndmanager_h */

