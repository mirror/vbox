/* $Id$ */
/** @file
 * VirtualBox COM class implementation: Guest Drag and Drop parts
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

#include "GuestImpl.h"
#include "AutoCaller.h"

#ifdef VBOX_WITH_DRAG_AND_DROP
# include "ConsoleImpl.h"
# include "ProgressImpl.h"
# include "GuestDnDImpl.h"

# include <iprt/dir.h>
# include <iprt/path.h>
# include <iprt/stream.h>
# include <iprt/semaphore.h>
# include <iprt/cpp/utils.h>

# include <VMMDev.h>

# include <VBox/com/list.h>
# include <VBox/GuestHost/DragAndDrop.h>
# include <VBox/HostServices/DragAndDropSvc.h>

# ifdef LOG_GROUP
 # undef LOG_GROUP
# endif
# define LOG_GROUP LOG_GROUP_GUEST_DND
# include <VBox/log.h>

/* How does this work:
 *
 * Drag and Drop is handled over the internal HGCM service for the host <->
 * guest communication. Beside that we need to map the Drag and Drop protocols
 * of the various OS's we support to our internal channels, this is also highly
 * communicative in both directions. Unfortunately HGCM isn't really designed
 * for that. Next we have to foul some of the components. This includes to
 * trick X11 on the guest side, but also Qt needs to be tricked on the host
 * side a little bit.
 *
 * The following components are involved:
 *
 * 1. GUI: Uses the Qt classes for Drag and Drop and mainly forward the content
 *    of it to the Main IGuest interface (see UIDnDHandler.cpp).
 * 2. Main: Public interface for doing Drag and Drop. Also manage the IProgress
 *    interfaces for blocking the caller by showing a progress dialog (see
 *    this file).
 * 3. HGCM service: Handle all messages from the host to the guest at once and
 *    encapsulate the internal communication details (see dndmanager.cpp and
 *    friends).
 * 4. Guest additions: Split into the platform neutral part (see
 *    VBoxGuestR3LibDragAndDrop.cpp) and the guest OS specific parts.
 *    Receive/send message from/to the HGCM service and does all guest specific
 *    operations. Currently only X11 is supported (see draganddrop.cpp within
 *    VBoxClient).
 *
 * Host -> Guest:
 * 1. There are DnD Enter, Move, Leave events which are send exactly like this
 *    to the guest. The info includes the pos, mimetypes and allowed actions.
 *    The guest has to respond with an action it would accept, so the GUI could
 *    change the cursor.
 * 2. On drop, first a drop event is send. If this is accepted a drop data
 *    event follows. This blocks the GUI and shows some progress indicator.
 *
 * Guest -> Host:
 * 1. The GUI is asking the guest if a DnD event is pending when the user moves
 *    the cursor out of the view window. If so, this returns the mimetypes and
 *    allowed actions.
 * (2. On every mouse move this is asked again, to make sure the DnD event is
 *     still valid.)
 * 3. On drop the host request the data from the guest. This blocks the GUI and
 *    shows some progress indicator.
 *
 * Some hints:
 * m_sstrAllowedMimeTypes here in this file defines the allowed mime-types.
 * This is necessary because we need special handling for some of the
 * mime-types. E.g. for URI lists we need to transfer the actual dirs and
 * files. Text EOL may to be changed. Also unknown mime-types may need special
 * handling as well, which may lead to undefined behavior in the host/guest, if
 * not done.
 *
 * Dropping of a directory, means recursively transferring _all_ the content.
 *
 * Directories and files are placed into the user's temporary directory on the
 * guest (e.g. /tmp/VirtualBox Dropped Files). We can't delete them after the
 * DnD operation, because we didn't know what the DnD target does with it. E.g.
 * it could just be opened in place. This could lead ofc to filling up the disk
 * within the guest. To inform the user about this, a small app could be
 * developed which scans this directory regularly and inform the user with a
 * tray icon hint (and maybe the possibility to clean this up instantly). The
 * same has to be done in the G->H direction when it is implemented.
 *
 * Of course only regularly files are supported. Symlinks are resolved and
 * transfered as regularly files. First we don't know if the other side support
 * symlinks at all and second they could point to somewhere in a directory tree
 * which not exists on the other side.
 *
 * The code tries to preserve the file modes of the transfered dirs/files. This
 * is useful (and maybe necessary) for two things:
 * 1. If a file is executable, it should be also after the transfer, so the
 *    user can just execute it, without manually tweaking the modes first.
 * 2. If a dir/file is not accessible by group/others in the host, it shouldn't
 *    be in the guest.
 * In any case, the user mode is always set to rwx (so that we can access it
 * ourself, in e.g. for a cleanup case after cancel).
 *
 * Cancel is supported in both directions and cleans up all previous steps
 * (thats is: deleting already transfered dirs/files).
 *
 * In general I propose the following changes in the VBox HGCM infrastructure
 * for the future:
 * - Currently it isn't really possible to send messages to the guest from the
 *   host. The host informs the guest just that there is something, the guest
 *   than has to ask which message and depending on that send the appropriate
 *   message to the host, which is filled with the right data.
 * - There is no generic interface for sending bigger memory blocks to/from the
 *   guest. This is now done here, but I guess was also necessary for e.g.
 *   guest execution. So something generic which brake this up into smaller
 *   blocks and send it would be nice (with all the error handling and such
 *   ofc).
 * - I developed a "protocol" for the DnD communication here. So the host and
 *   the guest have always to match in the revision. This is ofc bad, because
 *   the additions could be outdated easily. So some generic protocol number
 *   support in HGCM for asking the host and the guest of the support version,
 *   would be nice. Ofc at least the host should be able to talk to the guest,
 *   even when the version is below the host one.
 * All this stuff would be useful for the current services, but also for future
 * onces.
 *
 * Todo:
 * - Dragging out of the guest (partly done)
 *   - ESC doesn't really work (on Windows guests it's already implemented)
 *   - transfer of URIs (that is the files and patching of the data)
 *   - testing in a multi monitor setup
 *   ... in any case it seems a little bit difficult to handle from the Qt
 *   side. Maybe also a host specific implementation becomes necessary ...
 *   this would be really worst ofc.
 * - Win guest support (maybe there have to be done a mapping between the
 *   official mime-types and Win Clipboard formats (see QWindowsMime, for an
 *   idea), for VBox internally only mime-types should be used)
 * - EOL handling on text (should be shared with the clipboard code)
 * - add configuration (GH, HG, Bidirectional, None), like for the clipboard
 * - HG->GH and GH->HG-switch: Handle the case the user drags something out of
 *   the guest and than return to the source view (or another window in the
 *   multiple guest screen scenario).
 * - add support for more mime-types (especially images, csv)
 * - test unusual behavior:
 *   - DnD service crash in the guest during a DnD op (e.g. crash of VBoxClient or X11)
 *   - not expected order of the events between HGCM and the guest
 * - Security considerations: We transfer a lot of memory between the guest and
 *   the host and even allow the creation of dirs/files. Maybe there should be
 *   limits introduced to preventing DOS attacks or filling up all the memory
 *   (both in the host and the guest).
 * - test, test, test ...
 */

class DnDGuestResponse
{

public:

    DnDGuestResponse(const ComObjPtr<Guest>& pGuest);

    virtual ~DnDGuestResponse(void);

public:

    int notifyAboutGuestResponse(void);
    int waitForGuestResponse(RTMSINTERVAL msTimeout = 500);

    void setDefAction(uint32_t a) { m_defAction = a; }
    uint32_t defAction(void) const { return m_defAction; }

    void setAllActions(uint32_t a) { m_allActions = a; }
    uint32_t allActions() const { return m_allActions; }

    void setFormat(const Utf8Str &strFormat) { m_strFormat = strFormat; }
    Utf8Str format(void) const { return m_strFormat; }

    void setDropDir(const Utf8Str &strDropDir) { m_strDropDir = strDropDir; }
    Utf8Str dropDir(void) const { return m_strDropDir; }

    int dataAdd(const void *pvData, uint32_t cbData, uint32_t *pcbCurSize);
    int dataSetStatus(size_t cbDataAdd, size_t cbDataTotal = 0);
    void reset(void);
    const void *data(void) { return m_pvData; }
    size_t size(void) const { return m_cbData; }

    int setProgress(unsigned uPercentage, uint32_t uState, int rcOp = VINF_SUCCESS);
    HRESULT resetProgress(const ComObjPtr<Guest>& pParent);
    HRESULT queryProgressTo(IProgress **ppProgress);

public:

    Utf8Str errorToString(const ComObjPtr<Guest>& pGuest, int guestRc);

private:
    RTSEMEVENT           m_EventSem;
    uint32_t             m_defAction;
    uint32_t             m_allActions;
    Utf8Str              m_strFormat;

    /** The actual MIME data.*/
    void                *m_pvData;
    /** Size (in bytes) of MIME data. */
    uint32_t             m_cbData;

    size_t               m_cbDataCurrent;
    size_t               m_cbDataTotal;
    /** Dropped files directory on the host. */
    Utf8Str              m_strDropDir;

    ComObjPtr<Guest>     m_parent;
    ComObjPtr<Progress>  m_progress;
};

/** @todo This class needs a major cleanup. Later. */
class GuestDnDPrivate
{
private:

    /** @todo Currently we only support one response. Maybe this needs to be extended at some time. */
    GuestDnDPrivate(GuestDnD *q, const ComObjPtr<Guest>& pGuest)
        : q_ptr(q)
        , p(pGuest)
        , m_pDnDResponse(new DnDGuestResponse(pGuest))
    {}
    virtual ~GuestDnDPrivate(void) { delete m_pDnDResponse; }

    DnDGuestResponse *response(void) const { return m_pDnDResponse; }

    HRESULT adjustCoords(ULONG uScreenId, ULONG *puX, ULONG *puY) const;
    int hostCall(uint32_t u32Function, uint32_t cParms, PVBOXHGCMSVCPARM paParms) const;

    /* Static helpers. */
    static RTCString           toFormatString(ComSafeArrayIn(IN_BSTR, formats));
    static void                toFormatSafeArray(const RTCString &strFormats, ComSafeArrayOut(BSTR, formats));

    static DragAndDropAction_T toMainAction(uint32_t uAction);
    static void                toMainActions(uint32_t uActions, ComSafeArrayOut(DragAndDropAction_T, actions));
    static uint32_t            toHGCMAction(DragAndDropAction_T action);
    static void                toHGCMActions(DragAndDropAction_T inDefAction, uint32_t *pOutDefAction, ComSafeArrayIn(DragAndDropAction_T, inAllowedActions), uint32_t *pOutAllowedActions);

    /* Private q and parent pointer */
    GuestDnD                        *q_ptr;
    ComObjPtr<Guest>                 p;

    /* Private helper members. */
    static const RTCList<RTCString>  m_sstrAllowedMimeTypes;
    DnDGuestResponse                *m_pDnDResponse;

    friend class GuestDnD;
};

/* What mime-types are supported by VirtualBox.
 * Note: If you add something here, make sure you test it with all guest OS's!
 ** @todo Make this MIME list configurable / extendable (by extra data?). Currently
 *        this is done hardcoded on every guest platform (POSIX/Windows).
 */
/* static */
const RTCList<RTCString> GuestDnDPrivate::m_sstrAllowedMimeTypes = RTCList<RTCString>()
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

DnDGuestResponse::DnDGuestResponse(const ComObjPtr<Guest>& pGuest)
  : m_EventSem(NIL_RTSEMEVENT)
  , m_defAction(0)
  , m_allActions(0)
  , m_pvData(0)
  , m_cbData(0)
  , m_cbDataCurrent(0)
  , m_cbDataTotal(0)
  , m_parent(pGuest)
{
    int rc = RTSemEventCreate(&m_EventSem);
    AssertRC(rc);
}

DnDGuestResponse::~DnDGuestResponse(void)
{
    reset();
    int rc = RTSemEventDestroy(m_EventSem);
    AssertRC(rc);
}

int DnDGuestResponse::dataAdd(const void *pvData, uint32_t cbData,
                              uint32_t *pcbCurSize)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    /* pcbCurSize is optional. */

    int rc = VINF_SUCCESS;

    /** @todo Make reallocation scheme a bit smarter here. */
    m_pvData = RTMemRealloc(m_pvData, m_cbData + cbData);
    if (m_pvData)
    {
        memcpy(&static_cast<uint8_t*>(m_pvData)[m_cbData],
               pvData, cbData);
        m_cbData += cbData;

        if (pcbCurSize)
            *pcbCurSize = m_cbData;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/* static */
Utf8Str DnDGuestResponse::errorToString(const ComObjPtr<Guest>& pGuest, int guestRc)
{
    Utf8Str strError;

    switch (guestRc)
    {
        case VERR_ACCESS_DENIED:
            strError += Utf8StrFmt(pGuest->tr("For one or more guest files or directories selected for transferring to the host your guest "
                                              "user does not have the appropriate access rights for. Please make sure that all selected "
                                              "elements can be accessed and that your guest user has the appropriate rights."));
            break;

        case VERR_SHARING_VIOLATION:
            strError += Utf8StrFmt(pGuest->tr("One or more guest files or directories selected for transferring to the host were locked. "
                                              "Please make sure that all selected elements can be accessed and that your guest user has "
                                              "the appropriate rights."));
            break;

        default:
            strError += Utf8StrFmt("Drag'n drop guest error (%Rrc)", guestRc);
            break;
    }

    return strError;
}

int DnDGuestResponse::notifyAboutGuestResponse(void)
{
    return RTSemEventSignal(m_EventSem);
}

void DnDGuestResponse::reset(void)
{
    m_strDropDir = "";
    m_strFormat = "";

    if (m_pvData)
    {
        RTMemFree(m_pvData);
        m_pvData = NULL;
    }
    m_cbData = 0;

    m_cbDataCurrent = 0;
    m_cbDataTotal = 0;
}

HRESULT DnDGuestResponse::resetProgress(const ComObjPtr<Guest>& pParent)
{
    m_progress.setNull();
    HRESULT rc = m_progress.createObject();
    if (SUCCEEDED(rc))
    {
        rc = m_progress->init(static_cast<IGuest*>(pParent),
                              Bstr(pParent->tr("Dropping data")).raw(),
                              TRUE);
    }
    return rc;
}

int DnDGuestResponse::setProgress(unsigned uPercentage,
                                  uint32_t uState, int rcOp /* = VINF_SUCCESS */)
{
    LogFlowFunc(("uPercentage=%RU32, uState=%RU32, rcOp=%Rrc\n",
                 uPercentage, uState, rcOp));

    int vrc = VINF_SUCCESS;
    if (!m_progress.isNull())
    {
        BOOL fCompleted;
        HRESULT hr = m_progress->COMGETTER(Completed)(&fCompleted);
        if (!fCompleted)
        {
            if (uState == DragAndDropSvc::DND_PROGRESS_ERROR)
            {
                hr = m_progress->notifyComplete(VBOX_E_IPRT_ERROR,
                                                COM_IIDOF(IGuest),
                                                m_parent->getComponentName(),
                                                DnDGuestResponse::errorToString(m_parent, rcOp).c_str());
                reset();
            }
            else if (uState == DragAndDropSvc::DND_PROGRESS_CANCELLED)
            {
                hr = m_progress->Cancel();
                if (SUCCEEDED(hr))
                    vrc = VERR_CANCELLED;

                reset();
            }
            else /* uState == DragAndDropSvc::DND_PROGRESS_RUNNING */
            {
                hr = m_progress->SetCurrentOperationProgress(uPercentage);
                AssertComRC(hr);
                if (   uState      == DragAndDropSvc::DND_PROGRESS_COMPLETE
                    || uPercentage >= 100)
                    hr = m_progress->notifyComplete(S_OK);
            }
        }
    }

    return vrc;
}

int DnDGuestResponse::dataSetStatus(size_t cbDataAdd, size_t cbDataTotal /* = 0 */)
{
    if (cbDataTotal)
    {
#ifndef DEBUG_andy
        AssertMsg(m_cbDataTotal <= cbDataTotal, ("New data size must not be smaller (%zu) than old value (%zu)\n",
                                                 cbDataTotal, m_cbDataTotal));
#endif
        LogFlowFunc(("Updating total data size from %zu to %zu\n", m_cbDataTotal, cbDataTotal));
        m_cbDataTotal = cbDataTotal;
    }
    AssertMsg(m_cbDataTotal, ("m_cbDataTotal must not be <= 0\n"));

    m_cbDataCurrent += cbDataAdd;
    unsigned int cPercentage = RT_MIN(m_cbDataCurrent * 100.0 / m_cbDataTotal, 100);

    /** @todo Don't use anonymous enums (uint32_t). */
    uint32_t uStatus = DragAndDropSvc::DND_PROGRESS_RUNNING;
    if (m_cbDataCurrent >= m_cbDataTotal)
        uStatus = DragAndDropSvc::DND_PROGRESS_COMPLETE;

#ifdef DEBUG_andy
    LogFlowFunc(("Updating transfer status (%zu/%zu), status=%ld\n",
                 m_cbDataCurrent, m_cbDataTotal, uStatus));
#else
    AssertMsg(m_cbDataCurrent <= m_cbDataTotal,
              ("More data transferred (%zu) than initially announced (%zu), cbDataAdd=%zu\n",
              m_cbDataCurrent, m_cbDataTotal, cbDataAdd));
#endif
    int rc = setProgress(cPercentage, uStatus);

    /** @todo For now we instantly confirm the cancel. Check if the
     *        guest should first clean up stuff itself and than really confirm
     *        the cancel request by an extra message. */
    if (rc == VERR_CANCELLED)
        rc = setProgress(100, DragAndDropSvc::DND_PROGRESS_CANCELLED);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

HRESULT DnDGuestResponse::queryProgressTo(IProgress **ppProgress)
{
    return m_progress.queryInterfaceTo(ppProgress);
}

int DnDGuestResponse::waitForGuestResponse(RTMSINTERVAL msTimeout /*= 500 */)
{
    int rc = RTSemEventWait(m_EventSem, msTimeout);
#ifdef DEBUG_andy
    LogFlowFunc(("msTimeout=%RU32, rc=%Rrc\n", msTimeout, rc));
#endif
    return rc;
}

///////////////////////////////////////////////////////////////////////////////

HRESULT GuestDnDPrivate::adjustCoords(ULONG uScreenId, ULONG *puX, ULONG *puY) const
{
    /* For multi-monitor support we need to add shift values to the coordinates
     * (depending on the screen number). */
    ComPtr<IDisplay> pDisplay;
    HRESULT hr = p->mParent->COMGETTER(Display)(pDisplay.asOutParam());
    if (FAILED(hr))
        return hr;

    ComPtr<IFramebuffer> pFramebuffer;
    LONG xShift, yShift;
    hr = pDisplay->GetFramebuffer(uScreenId, pFramebuffer.asOutParam(),
                                  &xShift, &yShift);
    if (FAILED(hr))
        return hr;

    *puX += xShift;
    *puY += yShift;

    return hr;
}

int GuestDnDPrivate::hostCall(uint32_t u32Function, uint32_t cParms,
                              PVBOXHGCMSVCPARM paParms) const
{
    VMMDev *pVMMDev = NULL;
    {
        /* Make sure mParent is valid, so set the read lock while using.
         * Do not keep this lock while doing the actual call, because in the meanwhile
         * another thread could request a write lock which would be a bad idea ... */
        AutoReadLock alock(p COMMA_LOCKVAL_SRC_POS);

        /* Forward the information to the VMM device. */
        AssertPtr(p->mParent);
        pVMMDev = p->mParent->getVMMDev();
    }

    if (!pVMMDev)
        throw p->setError(VBOX_E_VM_ERROR,
                          p->tr("VMM device is not available (is the VM running?)"));

    LogFlowFunc(("hgcmHostCall uMsg=%RU32, cParms=%RU32\n", u32Function, cParms));
    int rc = pVMMDev->hgcmHostCall("VBoxDragAndDropSvc",
                                   u32Function,
                                   cParms, paParms);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("hgcmHostCall failed with rc=%Rrc\n", rc));
        throw p->setError(VBOX_E_IPRT_ERROR,
                          p->tr("hgcmHostCall failed (%Rrc)"), rc);
    }

    return rc;
}

/* static */
RTCString GuestDnDPrivate::toFormatString(ComSafeArrayIn(IN_BSTR, formats))
{
    const RTCList<Utf8Str> formatList(ComSafeArrayInArg(formats));
    RTCString strFormat;
    for (size_t i = 0; i < formatList.size(); ++i)
    {
        const RTCString &f = formatList.at(i);
        /* Only keep allowed format types. */
        if (m_sstrAllowedMimeTypes.contains(f))
            strFormat += f + "\r\n";
    }
    return strFormat;
}

/* static */
void GuestDnDPrivate::toFormatSafeArray(const RTCString &strFormats, ComSafeArrayOut(BSTR, formats))
{
    RTCList<RTCString> list = strFormats.split("\r\n");
    size_t i = 0;
    while (i < list.size())
    {
        /* Only keep allowed format types. */
        if (!m_sstrAllowedMimeTypes.contains(list.at(i)))
            list.removeAt(i);
        else
            ++i;
    }
    /* Create a safe array out of the cleaned list. */
    com::SafeArray<BSTR> sfaFormats(list.size());
    for (i = 0; i < list.size(); ++i)
    {
        const RTCString &f = list.at(i);
        if (m_sstrAllowedMimeTypes.contains(f))
        {
            Bstr bstr(f);
            bstr.detachTo(&sfaFormats[i]);
        }
    }
    sfaFormats.detachTo(ComSafeArrayOutArg(formats));
}

/* static */
uint32_t GuestDnDPrivate::toHGCMAction(DragAndDropAction_T action)
{
    uint32_t a = DND_IGNORE_ACTION;
    switch (action)
    {
        case DragAndDropAction_Copy:   a = DND_COPY_ACTION; break;
        case DragAndDropAction_Move:   a = DND_MOVE_ACTION; break;
        case DragAndDropAction_Link:   /* For now it doesn't seems useful to allow a link action between host & guest. Maybe later! */
        case DragAndDropAction_Ignore: /* Ignored */ break;
        default: AssertMsgFailed(("Action %u not recognized!\n", action)); break;
    }
    return a;
}

/* static */
void GuestDnDPrivate::toHGCMActions(DragAndDropAction_T inDefAction,
                                    uint32_t *pOutDefAction,
                                    ComSafeArrayIn(DragAndDropAction_T, inAllowedActions),
                                    uint32_t *pOutAllowedActions)
{
    const com::SafeArray<DragAndDropAction_T> sfaInActions(ComSafeArrayInArg(inAllowedActions));

    /* Defaults */
    *pOutDefAction = toHGCMAction(inDefAction);;
    *pOutAllowedActions = DND_IGNORE_ACTION;

    /* First convert the allowed actions to a bit array. */
    for (size_t i = 0; i < sfaInActions.size(); ++i)
        *pOutAllowedActions |= toHGCMAction(sfaInActions[i]);

    /* Second check if the default action is a valid action and if not so try
     * to find an allowed action. */
    if (isDnDIgnoreAction(*pOutDefAction))
    {
        if (hasDnDCopyAction(*pOutAllowedActions))
            *pOutDefAction = DND_COPY_ACTION;
        else if (hasDnDMoveAction(*pOutAllowedActions))
            *pOutDefAction = DND_MOVE_ACTION;
    }
}

/* static */
DragAndDropAction_T GuestDnDPrivate::toMainAction(uint32_t uAction)
{
    /* For now it doesn't seems useful to allow a
     * link action between host & guest. Maybe later! */
    return (isDnDCopyAction(uAction) ? (DragAndDropAction_T)DragAndDropAction_Copy :
            isDnDMoveAction(uAction) ? (DragAndDropAction_T)DragAndDropAction_Move :
            (DragAndDropAction_T)DragAndDropAction_Ignore);
}

/* static */
void GuestDnDPrivate::toMainActions(uint32_t uActions,
                                    ComSafeArrayOut(DragAndDropAction_T, actions))
{
    /* For now it doesn't seems useful to allow a
     * link action between host & guest. Maybe later! */
    RTCList<DragAndDropAction_T> list;
    if (hasDnDCopyAction(uActions))
        list.append(DragAndDropAction_Copy);
    if (hasDnDMoveAction(uActions))
        list.append(DragAndDropAction_Move);

    com::SafeArray<DragAndDropAction_T> sfaActions(list.size());
    for (size_t i = 0; i < list.size(); ++i)
        sfaActions[i] = list.at(i);
    sfaActions.detachTo(ComSafeArrayOutArg(actions));
}

GuestDnD::GuestDnD(const ComObjPtr<Guest>& pGuest)
    : d_ptr(new GuestDnDPrivate(this, pGuest))
{
}

GuestDnD::~GuestDnD(void)
{
    delete d_ptr;
}

HRESULT GuestDnD::dragHGEnter(ULONG uScreenId, ULONG uX, ULONG uY,
                              DragAndDropAction_T defaultAction,
                              ComSafeArrayIn(DragAndDropAction_T, allowedActions),
                              ComSafeArrayIn(IN_BSTR, formats),
                              DragAndDropAction_T *pResultAction)
{
    DPTR(GuestDnD);
    const ComObjPtr<Guest> &p = d->p;

    /* Default is ignoring */
    *pResultAction = DragAndDropAction_Ignore;

    /* Check & convert the drag & drop actions */
    uint32_t uDefAction      = 0;
    uint32_t uAllowedActions = 0;
    d->toHGCMActions(defaultAction, &uDefAction, ComSafeArrayInArg(allowedActions), &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
        return S_OK;

    /* Make a flat data string out of the mime-type list. */
    RTCString strFormats = d->toFormatString(ComSafeArrayInArg(formats));
    /* If there is no valid mime-type, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    try
    {
        /* Adjust the coordinates in a multi-monitor setup. */
        d->adjustCoords(uScreenId, &uX, &uY);

        VBOXHGCMSVCPARM paParms[7];
        int i = 0;
        paParms[i++].setUInt32(uScreenId);
        paParms[i++].setUInt32(uX);
        paParms[i++].setUInt32(uY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        d->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_ENTER,
                    i,
                    paParms);

        DnDGuestResponse *pResp = d->response();
        /* This blocks until the request is answered (or timeout). */
        if (pResp->waitForGuestResponse() == VERR_TIMEOUT)
            return S_OK;

        /* Copy the response info */
        *pResultAction = d->toMainAction(pResp->defAction());
        LogFlowFunc(("*pResultAction=%ld\n", *pResultAction));
    }
    catch (HRESULT hr2)
    {
        hr = hr2;
    }

    return hr;
}

HRESULT GuestDnD::dragHGMove(ULONG uScreenId, ULONG uX, ULONG uY,
                             DragAndDropAction_T defaultAction,
                             ComSafeArrayIn(DragAndDropAction_T, allowedActions),
                             ComSafeArrayIn(IN_BSTR, formats),
                             DragAndDropAction_T *pResultAction)
{
    DPTR(GuestDnD);
    const ComObjPtr<Guest> &p = d->p;

    /* Default is ignoring */
    *pResultAction = DragAndDropAction_Ignore;

    /* Check & convert the drag & drop actions */
    uint32_t uDefAction      = 0;
    uint32_t uAllowedActions = 0;
    d->toHGCMActions(defaultAction, &uDefAction, ComSafeArrayInArg(allowedActions), &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
        return S_OK;

    /* Make a flat data string out of the mime-type list. */
    RTCString strFormats = d->toFormatString(ComSafeArrayInArg(formats));
    /* If there is no valid mime-type, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    try
    {
        /* Adjust the coordinates in a multi-monitor setup. */
        d->adjustCoords(uScreenId, &uX, &uY);

        VBOXHGCMSVCPARM paParms[7];
        int i = 0;
        paParms[i++].setUInt32(uScreenId);
        paParms[i++].setUInt32(uX);
        paParms[i++].setUInt32(uY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        d->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_MOVE,
                    i,
                    paParms);

        DnDGuestResponse *pResp = d->response();
        /* This blocks until the request is answered (or timeout). */
        if (pResp->waitForGuestResponse() == VERR_TIMEOUT)
            return S_OK;

        /* Copy the response info */
        *pResultAction = d->toMainAction(pResp->defAction());
        LogFlowFunc(("*pResultAction=%ld\n", *pResultAction));
    }
    catch (HRESULT hr2)
    {
        hr = hr2;
    }

    return hr;
}

HRESULT GuestDnD::dragHGLeave(ULONG uScreenId)
{
    DPTR(GuestDnD);
    const ComObjPtr<Guest> &p = d->p;

    HRESULT hr = S_OK;

    try
    {
        d->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_LEAVE,
                    0,
                    NULL);

        DnDGuestResponse *pResp = d->response();
        /* This blocks until the request is answered (or timeout). */
        pResp->waitForGuestResponse();
    }
    catch (HRESULT hr2)
    {
        hr = hr2;
    }

    return hr;
}

HRESULT GuestDnD::dragHGDrop(ULONG uScreenId, ULONG uX, ULONG uY,
                             DragAndDropAction_T defaultAction,
                             ComSafeArrayIn(DragAndDropAction_T, allowedActions),
                             ComSafeArrayIn(IN_BSTR, formats),
                             BSTR *pstrFormat,
                             DragAndDropAction_T *pResultAction)
{
    DPTR(GuestDnD);
    const ComObjPtr<Guest> &p = d->p;

    /* Default is ignoring */
    *pResultAction = DragAndDropAction_Ignore;

    /* Check & convert the drag & drop actions */
    uint32_t uDefAction      = 0;
    uint32_t uAllowedActions = 0;
    d->toHGCMActions(defaultAction, &uDefAction, ComSafeArrayInArg(allowedActions), &uAllowedActions);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uDefAction))
        return S_OK;

    /* Make a flat data string out of the mime-type list. */
    RTCString strFormats = d->toFormatString(ComSafeArrayInArg(formats));
    /* If there is no valid mime-type, ignore this request. */
    if (strFormats.isEmpty())
        return S_OK;

    HRESULT hr = S_OK;

    try
    {
        /* Adjust the coordinates in a multi-monitor setup. */
        d->adjustCoords(uScreenId, &uX, &uY);

        VBOXHGCMSVCPARM paParms[7];
        int i = 0;
        paParms[i++].setUInt32(uScreenId);
        paParms[i++].setUInt32(uX);
        paParms[i++].setUInt32(uY);
        paParms[i++].setUInt32(uDefAction);
        paParms[i++].setUInt32(uAllowedActions);
        paParms[i++].setPointer((void*)strFormats.c_str(), strFormats.length() + 1);
        paParms[i++].setUInt32(strFormats.length() + 1);

        d->hostCall(DragAndDropSvc::HOST_DND_HG_EVT_DROPPED,
                    i,
                    paParms);

        DnDGuestResponse *pResp = d->response();
        /* This blocks until the request is answered (or timeout). */
        if (pResp->waitForGuestResponse() == VERR_TIMEOUT)
            return S_OK;

        /* Copy the response info */
        *pResultAction = d->toMainAction(pResp->defAction());
        Bstr(pResp->format()).cloneTo(pstrFormat);

        LogFlowFunc(("*pResultAction=%ld\n", *pResultAction));
    }
    catch (HRESULT hr2)
    {
        hr = hr2;
    }

    return hr;
}

HRESULT GuestDnD::dragHGPutData(ULONG uScreenId, IN_BSTR bstrFormat,
                                ComSafeArrayIn(BYTE, data), IProgress **ppProgress)
{
    DPTR(GuestDnD);
    const ComObjPtr<Guest> &p = d->p;

    HRESULT hr = S_OK;

    try
    {
        Utf8Str strFormat(bstrFormat);
        com::SafeArray<BYTE> sfaData(ComSafeArrayInArg(data));

        VBOXHGCMSVCPARM paParms[5];
        int i = 0;
        paParms[i++].setUInt32(uScreenId);
        paParms[i++].setPointer((void*)strFormat.c_str(), (uint32_t)strFormat.length() + 1);
        paParms[i++].setUInt32((uint32_t)strFormat.length() + 1);
        paParms[i++].setPointer((void*)sfaData.raw(), (uint32_t)sfaData.size());
        paParms[i++].setUInt32((uint32_t)sfaData.size());

        DnDGuestResponse *pResp = d->response();
        /* Reset any old progress status. */
        pResp->resetProgress(p);

        /* Note: The actual data transfer of files/directoies is performed by the
         *       DnD host service. */
        d->hostCall(DragAndDropSvc::HOST_DND_HG_SND_DATA,
                    i,
                    paParms);

        /* Query the progress object to the caller. */
        pResp->queryProgressTo(ppProgress);
    }
    catch (HRESULT hr2)
    {
        hr = hr2;
    }

    return hr;
}

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
HRESULT GuestDnD::dragGHPending(ULONG uScreenId,
                                ComSafeArrayOut(BSTR, formats),
                                ComSafeArrayOut(DragAndDropAction_T, allowedActions),
                                DragAndDropAction_T *pDefaultAction)
{
    DPTR(GuestDnD);
    const ComObjPtr<Guest> &p = d->p;

    /* Default is ignoring */
    *pDefaultAction = DragAndDropAction_Ignore;

    HRESULT hr = S_OK;

    try
    {
        VBOXHGCMSVCPARM paParms[1];
        int i = 0;
        paParms[i++].setUInt32(uScreenId);

        d->hostCall(DragAndDropSvc::HOST_DND_GH_REQ_PENDING,
                    i,
                    paParms);

        /* This blocks until the request is answered (or timed out). */
        DnDGuestResponse *pResp = d->response();
        if (pResp->waitForGuestResponse() == VERR_TIMEOUT)
            return S_OK;

        if (isDnDIgnoreAction(pResp->defAction()))
            return S_OK;

        /* Fetch the default action to use. */
        *pDefaultAction = d->toMainAction(pResp->defAction());
        d->toFormatSafeArray(pResp->format(), ComSafeArrayOutArg(formats));
        d->toMainActions(pResp->allActions(), ComSafeArrayOutArg(allowedActions));

        LogFlowFunc(("*pDefaultAction=0x%x\n", *pDefaultAction));
    }
    catch (HRESULT hr2)
    {
        hr = hr2;
    }

    return hr;
}

HRESULT GuestDnD::dragGHDropped(IN_BSTR bstrFormat, DragAndDropAction_T action,
                                IProgress **ppProgress)
{
    DPTR(GuestDnD);
    const ComObjPtr<Guest> &p = d->p;

    Utf8Str strFormat(bstrFormat);
    HRESULT hr = S_OK;

    uint32_t uAction = d->toHGCMAction(action);
    /* If there is no usable action, ignore this request. */
    if (isDnDIgnoreAction(uAction))
        return S_OK;

    const char *pcszFormat = strFormat.c_str();
    bool fNeedsDropDir = DnDMIMENeedsDropDir(pcszFormat, strlen(pcszFormat));
    LogFlowFunc(("strFormat=%s, uAction=0x%x, fNeedsDropDir=%RTbool\n",
                 pcszFormat, uAction, fNeedsDropDir));

    DnDGuestResponse *pResp = d->response();
    AssertPtr(pResp);

    /* Reset any old data. */
    pResp->reset();
    pResp->resetProgress(p);

    /* Set the format we are going to retrieve to have it around
     * when retrieving the data later. */
    pResp->setFormat(strFormat);

    if (fNeedsDropDir)
    {
        char szDropDir[RTPATH_MAX];
        int rc = DnDDirCreateDroppedFiles(szDropDir, sizeof(szDropDir));
        if (RT_FAILURE(rc))
            return p->setError(VBOX_E_IPRT_ERROR,
                               p->tr("Unable to create the temporary drag'n drop directory \"%s\" (%Rrc)\n"),
                               szDropDir, rc);
        LogFlowFunc(("Dropped files directory on the host is: %s\n", szDropDir));

        pResp->setDropDir(szDropDir);
    }

    try
    {
        VBOXHGCMSVCPARM paParms[3];
        int i = 0;
        paParms[i++].setPointer((void*)strFormat.c_str(), (uint32_t)strFormat.length() + 1);
        paParms[i++].setUInt32((uint32_t)strFormat.length() + 1);
        paParms[i++].setUInt32(uAction);

        d->hostCall(DragAndDropSvc::HOST_DND_GH_EVT_DROPPED,
                    i,
                    paParms);

        /* Query the progress object to the caller. */
        pResp->queryProgressTo(ppProgress);
    }
    catch (HRESULT hr2)
    {
        hr = hr2;
    }

    return hr;
}

HRESULT GuestDnD::dragGHGetData(ComSafeArrayOut(BYTE, data))
{
    DPTR(GuestDnD);
    const ComObjPtr<Guest> &p = d->p;

    HRESULT hr = S_OK;

    DnDGuestResponse *pResp = d->response();
    if (pResp)
    {
        com::SafeArray<BYTE> sfaData;

        size_t cbData = pResp->size();
        if (cbData)
        {
            const void *pvData = pResp->data();
            AssertPtr(pvData);

            Utf8Str strFormat = pResp->format();
            LogFlowFunc(("strFormat=%s, cbData=%zu, pvData=0x%p\n",
                         strFormat.c_str(), cbData, pvData));

            if (DnDMIMEHasFileURLs(strFormat.c_str(), strFormat.length()))
            {
                LogFlowFunc(("strDropDir=%s\n", pResp->dropDir().c_str()));

                DnDURIList lstURI;
                int rc2 = lstURI.RootFromURIData(pvData, cbData, 0 /* fFlags */);
                if (RT_SUCCESS(rc2))
                {
                    Utf8Str strURIs = lstURI.RootToString(pResp->dropDir());
                    size_t cbURIs = strURIs.length();
                    if (sfaData.resize(cbURIs + 1 /* Include termination */))
                        memcpy(sfaData.raw(), strURIs.c_str(), cbURIs);
                    else
                        hr = E_OUTOFMEMORY;
                }
                else
                    hr = VBOX_E_IPRT_ERROR;

                LogFlowFunc(("Found %zu root URIs, rc=%Rrc\n", lstURI.RootCount(), rc2));
            }
            else
            {
                /* Copy the data into a safe array of bytes. */
                if (sfaData.resize(cbData))
                    memcpy(sfaData.raw(), pvData, cbData);
                else
                    hr = E_OUTOFMEMORY;
            }
        }

        /* Detach in any case, regardless of data size. */
        sfaData.detachTo(ComSafeArrayOutArg(data));

        /* Delete the data. */
        pResp->reset();
    }
    else
        hr = VBOX_E_INVALID_OBJECT_STATE;

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
}

int GuestDnD::onGHSendData(DnDGuestResponse *pResp,
                           const void *pvData, size_t cbData,
                           size_t cbTotalSize)
{
    AssertPtrReturn(pResp, VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    AssertReturn(cbTotalSize, VERR_INVALID_PARAMETER);

    int rc = pResp->dataAdd(pvData, cbData, NULL /* Current size */);
    if (RT_SUCCESS(rc))
        rc = pResp->dataSetStatus(cbData, cbTotalSize);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnD::onGHSendDir(DnDGuestResponse *pResp,
                          const char *pszPath, size_t cbPath,
                          uint32_t fMode)
{
    AssertPtrReturn(pResp, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath, VERR_INVALID_PARAMETER);

    LogFlowFunc(("pszPath=%s, cbPath=%zu, fMode=0x%x\n",
                 pszPath, cbPath, fMode));

    int rc;
    char *pszDir = RTPathJoinA(pResp->dropDir().c_str(), pszPath);
    if (pszDir)
    {
        rc = RTDirCreateFullPath(pszDir, fMode);
        RTStrFree(pszDir);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
        rc = pResp->dataSetStatus(cbPath);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int GuestDnD::onGHSendFile(DnDGuestResponse *pResp,
                           const char *pszPath, size_t cbPath,
                           void *pvData, size_t cbData, uint32_t fMode)
{
    AssertPtrReturn(pResp, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath, VERR_INVALID_PARAMETER);

    LogFlowFunc(("pszPath=%s, cbPath=%zu, fMode=0x%x\n",
                 pszPath, cbPath, fMode));

    /** @todo Add file locking between calls! */
    int rc;
    char *pszFile = RTPathJoinA(pResp->dropDir().c_str(), pszPath);
    if (pszFile)
    {
        RTFILE hFile;
        rc = RTFileOpen(&hFile, pszFile,
                        RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_WRITE | RTFILE_O_WRITE);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileWrite(hFile, pvData, cbData,
                             NULL /* No partial writes */);
            RTFileClose(hFile);
        }
        RTStrFree(pszFile);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
        rc = pResp->dataSetStatus(cbData);

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

/* static */
DECLCALLBACK(int) GuestDnD::notifyGuestDragAndDropEvent(void *pvExtension, uint32_t u32Function,
                                                        void *pvParms, uint32_t cbParms)
{
    LogFlowFunc(("pvExtension=%p, u32Function=%RU32, pvParms=%p, cbParms=%RU32\n",
                 pvExtension, u32Function, pvParms, cbParms));

    ComObjPtr<Guest> pGuest = reinterpret_cast<Guest*>(pvExtension);
    if (!pGuest->m_pGuestDnD)
        return VINF_SUCCESS;

    GuestDnD *pGuestDnD = pGuest->m_pGuestDnD;
    AssertPtr(pGuestDnD);

    GuestDnDPrivate *d = static_cast<GuestDnDPrivate*>(pGuest->m_pGuestDnD->d_ptr);
    const ComObjPtr<Guest> &p = d->p;

    DnDGuestResponse *pResp = d->response();
    if (pResp == NULL)
        return VERR_INVALID_PARAMETER;

    int rc = VINF_SUCCESS;
    switch (u32Function)
    {
        case DragAndDropSvc::GUEST_DND_HG_ACK_OP:
        {
            DragAndDropSvc::PVBOXDNDCBHGACKOPDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBHGACKOPDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBHGACKOPDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_HG_ACK_OP == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            pResp->setDefAction(pCBData->uAction);

            rc = pResp->notifyAboutGuestResponse();
            break;
        }

        case DragAndDropSvc::GUEST_DND_HG_REQ_DATA:
        {
            DragAndDropSvc::PVBOXDNDCBHGREQDATADATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBHGREQDATADATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBHGREQDATADATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_HG_REQ_DATA == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            pResp->setFormat(pCBData->pszFormat);

            rc = pResp->notifyAboutGuestResponse();
            break;
        }

        case DragAndDropSvc::GUEST_DND_HG_EVT_PROGRESS:
        {
            DragAndDropSvc::PVBOXDNDCBHGEVTPROGRESSDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBHGEVTPROGRESSDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBHGEVTPROGRESSDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_HG_EVT_PROGRESS == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pResp->setProgress(pCBData->uPercentage, pCBData->uState, pCBData->rc);
            break;
        }

#ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case DragAndDropSvc::GUEST_DND_GH_ACK_PENDING:
        {
            DragAndDropSvc::PVBOXDNDCBGHACKPENDINGDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBGHACKPENDINGDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBGHACKPENDINGDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_ACK_PENDING == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            pResp->setFormat(pCBData->pszFormat);
            pResp->setDefAction(pCBData->uDefAction);
            pResp->setAllActions(pCBData->uAllActions);

            rc = pResp->notifyAboutGuestResponse();
            break;
        }

        case DragAndDropSvc::GUEST_DND_GH_SND_DATA:
        {
            DragAndDropSvc::PVBOXDNDCBSNDDATADATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBSNDDATADATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBSNDDATADATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_SND_DATA == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuestDnD->onGHSendData(pResp, pCBData->pvData, pCBData->cbData,
                                         pCBData->cbTotalSize);
            break;
        }

        case DragAndDropSvc::GUEST_DND_GH_SND_DIR:
        {
            DragAndDropSvc::PVBOXDNDCBSNDDIRDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBSNDDIRDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBSNDDIRDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_SND_DIR == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuestDnD->onGHSendDir(pResp, pCBData->pszPath, pCBData->cbPath, pCBData->fMode);
            break;
        }

        case DragAndDropSvc::GUEST_DND_GH_SND_FILE:
        {
            DragAndDropSvc::PVBOXDNDCBSNDFILEDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBSNDFILEDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBSNDFILEDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_SND_FILE == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pGuestDnD->onGHSendFile(pResp, pCBData->pszFilePath, pCBData->cbFilePath,
                                         pCBData->pvData, pCBData->cbData, pCBData->fMode);
            break;
        }

        case DragAndDropSvc::GUEST_DND_GH_EVT_ERROR:
        {
            DragAndDropSvc::PVBOXDNDCBEVTERRORDATA pCBData = reinterpret_cast<DragAndDropSvc::PVBOXDNDCBEVTERRORDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBEVTERRORDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_GH_EVT_ERROR == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            /* Cleanup. */
            pResp->reset();
            rc = pResp->setProgress(100, DragAndDropSvc::DND_PROGRESS_ERROR, pCBData->rc);
            break;
        }
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
        default:
            rc = VERR_NOT_SUPPORTED; /* Tell the guest. */
            break;
    }

    LogFlowFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}
#endif /* VBOX_WITH_DRAG_AND_DROP */

