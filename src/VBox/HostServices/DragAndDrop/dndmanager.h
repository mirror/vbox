/** @file
 * Drag and Drop manager.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
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

typedef DECLCALLBACK(int) FNDNDPROGRESS(uint32_t uState, uint32_t uPercentage, int rc, void *pvUser);
typedef FNDNDPROGRESS *PFNDNDPROGRESS;

/**
 * DnD message class. This class forms the base of all other more specialized
 * message classes.
 */
class DnDMessage : public HGCM::Message
{
public:

    DnDMessage(void)
    {
    }

    DnDMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM aParms[])
        : Message(uMsg, cParms, aParms) { }

    virtual ~DnDMessage(void) { }
};

/**
 * DnD message class for generic messages which didn't need any special
 * handling.
 */
class DnDGenericMessage: public DnDMessage
{
public:
    DnDGenericMessage(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
        : DnDMessage(uMsg, cParms, paParms) { }
};

/**
 * DnD message class for informing the guest to cancel any current (and pending) activities.
 */
class DnDHGCancelMessage: public DnDMessage
{
public:

    DnDHGCancelMessage(void)
    {
        int rc2 = initData(DragAndDropSvc::HOST_DND_HG_EVT_CANCEL,
                           0 /* cParms */, 0 /* aParms */);
        AssertRC(rc2);
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
        : m_pfnProgressCallback(pfnProgressCallback)
        , m_pvProgressUser(pvProgressUser)
    {}

    virtual ~DnDManager(void)
    {
        Reset();
    }

    int AddMsg(DnDMessage *pMessage, bool fAppend = true);
    int AddMsg(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fAppend = true);

    int GetNextMsgInfo(uint32_t *puType, uint32_t *pcParms);
    int GetNextMsg(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    void Reset(void);

protected:

    /** DnD message queue (FIFO). */
    RTCList<DnDMessage *> m_queueMsg;
    /** Pointer to host progress callback. Optional, can be NULL. */
    PFNDNDPROGRESS        m_pfnProgressCallback;
    /** Pointer to progress callback user context. Can be NULL if not used. */
    void                 *m_pvProgressUser;
};
#endif /* ___VBox_HostService_DnD_dndmanager_h */

