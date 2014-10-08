/* $Id$ */
/** @file
 * Private guest drag and drop code, used by GuestDnDTarget +
 * GuestDnDSource.
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
# include "GuestDnDPrivate.h"

# include <algorithm>

# include <iprt/dir.h>
# include <iprt/path.h>
# include <iprt/stream.h>
# include <iprt/semaphore.h>
# include <iprt/cpp/utils.h>

# include <VMMDev.h>

# include <VBox/GuestHost/DragAndDrop.h>
# include <VBox/HostServices/DragAndDropSvc.h>

# ifdef LOG_GROUP
#  undef LOG_GROUP
# endif
# define LOG_GROUP LOG_GROUP_GUEST_DND
# include <VBox/log.h>

/**
 * Overview:
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
 * 2. On drop, first a drop event is sent. If this is accepted a drop data
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
 * m_strSupportedFormats here in this file defines the allowed mime-types.
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
 ** @todo
 * - ESC doesn't really work (on Windows guests it's already implemented)
 *   ... in any case it seems a little bit difficult to handle from the Qt
 *   side. Maybe also a host specific implementation becomes necessary ...
 *   this would be really worst ofc.
 * - Add support for more mime-types (especially images, csv)
 * - Test unusual behavior:
 *   - DnD service crash in the guest during a DnD op (e.g. crash of VBoxClient or X11)
 *   - Not expected order of the events between HGCM and the guest
 * - Security considerations: We transfer a lot of memory between the guest and
 *   the host and even allow the creation of dirs/files. Maybe there should be
 *   limits introduced to preventing DOS attacks or filling up all the memory
 *   (both in the host and the guest).
 */

GuestDnDResponse::GuestDnDResponse(const ComObjPtr<Guest>& pGuest)
    : m_EventSem(NIL_RTSEMEVENT)
    , m_defAction(0)
    , m_allActions(0)
    , m_pvData(0)
    , m_cbData(0)
    , m_cbDataCurrent(0)
    , m_cbDataTotal(0)
    , m_hFile(NIL_RTFILE)
    , m_parent(pGuest)
{
    int rc = RTSemEventCreate(&m_EventSem);
    AssertRC(rc);
}

GuestDnDResponse::~GuestDnDResponse(void)
{
    reset();

    int rc = RTSemEventDestroy(m_EventSem);
    AssertRC(rc);
}

int GuestDnDResponse::dataAdd(const void *pvData, uint32_t cbData,
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
Utf8Str GuestDnDResponse::errorToString(const ComObjPtr<Guest>& pGuest, int guestRc)
{
    Utf8Str strError;

    switch (guestRc)
    {
        case VERR_ACCESS_DENIED:
            strError += Utf8StrFmt(pGuest->tr("For one or more guest files or directories selected for transferring to the host your guest "
                                              "user does not have the appropriate access rights for. Please make sure that all selected "
                                              "elements can be accessed and that your guest user has the appropriate rights."));
            break;

        case VERR_NOT_FOUND:
            /* Should not happen due to file locking on the guest, but anyway ... */
            strError += Utf8StrFmt(pGuest->tr("One or more guest files or directories selected for transferring to the host were not"
                                              "found on the guest anymore. This can be the case if the guest files were moved and/or"
                                              "altered while the drag'n drop operation was in progress."));
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

int GuestDnDResponse::notifyAboutGuestResponse(void)
{
    return RTSemEventSignal(m_EventSem);
}

void GuestDnDResponse::reset(void)
{
    LogFlowThisFuncEnter();

    m_defAction = 0;
    m_allActions = 0;

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

    if (m_hFile != NIL_RTFILE)
    {
        RTFileClose(m_hFile);
        m_hFile = NIL_RTFILE;
    }
    m_strFile = "";
}

HRESULT GuestDnDResponse::resetProgress(const ComObjPtr<Guest>& pParent)
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

int GuestDnDResponse::setProgress(unsigned uPercentage,
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
                hr = m_progress->i_notifyComplete(VBOX_E_IPRT_ERROR,
                                                  COM_IIDOF(IGuest),
                                                  m_parent->getComponentName(),
                                                  GuestDnDResponse::errorToString(m_parent, rcOp).c_str());
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
                    hr = m_progress->i_notifyComplete(S_OK);
            }
        }
    }

    return vrc;
}

int GuestDnDResponse::dataSetStatus(size_t cbDataAdd, size_t cbDataTotal /* = 0 */)
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

HRESULT GuestDnDResponse::queryProgressTo(IProgress **ppProgress)
{
    return m_progress.queryInterfaceTo(ppProgress);
}

int GuestDnDResponse::waitForGuestResponse(RTMSINTERVAL msTimeout /*= 500 */)
{
    int rc = RTSemEventWait(m_EventSem, msTimeout);
#ifdef DEBUG_andy
    LogFlowFunc(("msTimeout=%RU32, rc=%Rrc\n", msTimeout, rc));
#endif
    return rc;
}

int GuestDnDResponse::writeToFile(const char *pszPath, size_t cbPath,
                                  void *pvData, size_t cbData, uint32_t fMode)
{
    /** @todo Support locking more than one file at a time! We
     *        might want to have a table in DnDGuestImpl which
     *        keeps those file pointers around, or extend the
     *        actual protocol for explicit open calls.
     *
     *        For now we only keep one file open at a time, so if
     *        a client does alternating writes to different files
     *        this function will close the old and re-open the new
     *        file on every call. */
    int rc;
    if (   m_hFile == NIL_RTFILE
        || m_strFile != pszPath)
    {
        char *pszFile = RTPathJoinA(m_strDropDir.c_str(), pszPath);
        if (pszFile)
        {
            RTFILE hFile;
            /** @todo Respect fMode!  */
            rc = RTFileOpen(&hFile, pszFile,
                              RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE
                            | RTFILE_O_WRITE | RTFILE_O_APPEND);
            if (RT_SUCCESS(rc))
            {
                LogFlowFunc(("Opening \"%s\" (fMode=0x%x) for writing ...\n",
                             pszFile, fMode));

                m_hFile = hFile;
                m_strFile = pszPath;
            }

            RTStrFree(pszFile);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(m_hFile, pvData, cbData,
                         NULL /* No partial writes */);

        if (RT_SUCCESS(rc))
            rc = dataSetStatus(cbData);
    }

    return rc;
}

///////////////////////////////////////////////////////////////////////////////

GuestDnD* GuestDnD::s_pInstance = NULL;

GuestDnD::GuestDnD(const ComObjPtr<Guest> &pGuest)
    : m_pGuest(pGuest)
{
    LogFlowFuncEnter();

    m_pResponse = new GuestDnDResponse(pGuest);

    /* List of supported default MIME types. */
    const com::Utf8Str arrEntries[] = { VBOX_DND_FORMATS_DEFAULT };
    for (size_t i = 0; i < RT_ELEMENTS(arrEntries); i++)
        m_strDefaultFormats.push_back(arrEntries[i]);
}

GuestDnD::~GuestDnD(void)
{
    LogFlowFuncEnter();

    if (m_pResponse)
        delete m_pResponse;
}

int GuestDnD::adjustScreenCoordinates(ULONG uScreenId, ULONG *puX, ULONG *puY) const
{
    /** @todo r=andy Save the current screen's shifting coordinates to speed things up.
     *               Only query for new offsets when the screen ID has changed. */

    /* For multi-monitor support we need to add shift values to the coordinates
     * (depending on the screen number). */
    ComObjPtr<Console> pConsole = m_pGuest->i_getConsole();
    ComPtr<IDisplay> pDisplay;
    HRESULT hr = pConsole->COMGETTER(Display)(pDisplay.asOutParam());
    if (FAILED(hr))
        return hr;

    ULONG dummy;
    LONG xShift, yShift;
    GuestMonitorStatus_T monitorStatus;
    hr = pDisplay->GetScreenResolution(uScreenId, &dummy, &dummy, &dummy,
                                       &xShift, &yShift, &monitorStatus);
    if (FAILED(hr))
        return hr;

    if (puX)
        *puX += xShift;
    if (puY)
        *puY += yShift;

#ifdef DEBUG_andy
    LogFlowFunc(("uScreenId=%RU32, x=%RU32, y=%RU32\n",
                 uScreenId, puX ? *puX : 0, puY ? *puY : 0));
#endif
    return VINF_SUCCESS;
}

int GuestDnD::hostCall(uint32_t u32Function, uint32_t cParms, PVBOXHGCMSVCPARM paParms) const
{
    Assert(!m_pGuest.isNull());
    ComObjPtr<Console> pConsole = m_pGuest->i_getConsole();

    /* Forward the information to the VMM device. */
    Assert(!pConsole.isNull());
    VMMDev *pVMMDev = pConsole->i_getVMMDev();
    if (!pVMMDev)
        return VERR_COM_OBJECT_NOT_FOUND;

    int rc = pVMMDev->hgcmHostCall("VBoxDragAndDropSvc", u32Function,
                                   cParms, paParms);
    LogFlowFunc(("uMsg=%RU32, cParms=%RU32, rc=%Rrc\n",
                 u32Function, cParms, rc));
    return rc;
}

/* static */
com::Utf8Str GuestDnD::toFormatString(const std::vector<com::Utf8Str> &lstSupportedFormats,
                                      const std::vector<com::Utf8Str> &lstFormats)
{
    com::Utf8Str strFormat;
    for (size_t i = 0; i < lstFormats.size(); ++i)
    {
        const com::Utf8Str &f = lstFormats.at(i);
        /* Only keep allowed format types. */
        if (std::find(lstSupportedFormats.begin(),
                      lstSupportedFormats.end(), f) != lstSupportedFormats.end())
            strFormat += f + "\r\n";
    }

    return strFormat;
}

/* static */
void GuestDnD::toFormatVector(const std::vector<com::Utf8Str> &lstSupportedFormats,
                              const com::Utf8Str &strFormats,
                              std::vector<com::Utf8Str> &vecformats)
{
    RTCList<RTCString> lstFormats = strFormats.split("\r\n");
    size_t i = 0;
    while (i < lstFormats.size())
    {
        /* Only keep allowed format types. */
        if (std::find(lstSupportedFormats.begin(),
                      lstSupportedFormats.end(), lstFormats.at(i)) == lstSupportedFormats.end())
            lstFormats.removeAt(i);
        else
            ++i;
    }

    for (i = 0; i < lstFormats.size(); i++)
    {
        const Utf8Str &f = lstFormats.at(i);
        if (std::find(lstSupportedFormats.begin(),
                      lstSupportedFormats.end(), f) != lstSupportedFormats.end())
            vecformats.push_back(lstFormats[i]);
    }
}

/* static */
uint32_t GuestDnD::toHGCMAction(DnDAction_T enmAction)
{
    uint32_t uAction = DND_IGNORE_ACTION;
    switch (enmAction)
    {
        case DnDAction_Copy:
            uAction = DND_COPY_ACTION;
            break;
        case DnDAction_Move:
            uAction = DND_MOVE_ACTION;
            break;
        case DnDAction_Link:
            /* For now it doesn't seems useful to allow a link
               action between host & guest. Later? */
        case DnDAction_Ignore:
            /* Ignored. */
            break;
        default:
            AssertMsgFailed(("Action %RU32 not recognized!\n", enmAction));
            break;
    }

    return uAction;
}

/* static */
void GuestDnD::toHGCMActions(DnDAction_T enmDefAction,
                             uint32_t *puDefAction,
                             const std::vector<DnDAction_T> vecAllowedActions,
                             uint32_t *puAllowedActions)
{
    if (puDefAction)
        *puDefAction = toHGCMAction(enmDefAction);
    if (puAllowedActions)
    {
        *puAllowedActions = DND_IGNORE_ACTION;

        /* First convert the allowed actions to a bit array. */
        for (size_t i = 0; i < vecAllowedActions.size(); ++i)
            *puAllowedActions |= toHGCMAction(vecAllowedActions[i]);

        /* Second check if the default action is a valid action and if not so try
         * to find an allowed action. */
        if (isDnDIgnoreAction(*puAllowedActions))
        {
            if (hasDnDCopyAction(*puAllowedActions))
                *puAllowedActions = DND_COPY_ACTION;
            else if (hasDnDMoveAction(*puAllowedActions))
                *puAllowedActions = DND_MOVE_ACTION;
        }
    }
}

/* static */
DnDAction_T GuestDnD::toMainAction(uint32_t uAction)
{
    /* For now it doesn't seems useful to allow a
     * link action between host & guest. Maybe later! */
    return (isDnDCopyAction(uAction) ? (DnDAction_T)DnDAction_Copy :
            isDnDMoveAction(uAction) ? (DnDAction_T)DnDAction_Move :
            (DnDAction_T)DnDAction_Ignore);
}

/* static */
void GuestDnD::toMainActions(uint32_t uActions,
                             std::vector<DnDAction_T> &vecActions)
{
    /* For now it doesn't seems useful to allow a
     * link action between host & guest. Maybe later! */
    RTCList<DnDAction_T> lstActions;
    if (hasDnDCopyAction(uActions))
        lstActions.append(DnDAction_Copy);
    if (hasDnDMoveAction(uActions))
        lstActions.append(DnDAction_Move);

    for (size_t i = 0; i < lstActions.size(); ++i)
        vecActions.push_back(lstActions.at(i));
}

/* static */
DECLCALLBACK(int) GuestDnD::notifyDnDDispatcher(void *pvExtension, uint32_t u32Function,
                                                void *pvParms, uint32_t cbParms)
{
    LogFlowFunc(("pvExtension=%p, u32Function=%RU32, pvParms=%p, cbParms=%RU32\n",
                 pvExtension, u32Function, pvParms, cbParms));

    GuestDnD *pGuestDnD = reinterpret_cast<GuestDnD*>(pvExtension);
    AssertPtrReturn(pGuestDnD, VERR_INVALID_POINTER);

    GuestDnDResponse *pResp = pGuestDnD->m_pResponse;
    AssertPtrReturn(pResp, VERR_INVALID_POINTER);

    int rc;
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
            DragAndDropSvc::PVBOXDNDCBHGEVTPROGRESSDATA pCBData =
                reinterpret_cast <DragAndDropSvc::PVBOXDNDCBHGEVTPROGRESSDATA>(pvParms);
            AssertPtr(pCBData);
            AssertReturn(sizeof(DragAndDropSvc::VBOXDNDCBHGEVTPROGRESSDATA) == cbParms, VERR_INVALID_PARAMETER);
            AssertReturn(DragAndDropSvc::CB_MAGIC_DND_HG_EVT_PROGRESS == pCBData->hdr.u32Magic, VERR_INVALID_PARAMETER);

            rc = pResp->setProgress(pCBData->uPercentage, pCBData->uState, pCBData->rc);
            break;
        }

# ifdef VBOX_WITH_DRAG_AND_DROP_GH
        case DragAndDropSvc::GUEST_DND_GH_ACK_PENDING:
        {
            DragAndDropSvc::PVBOXDNDCBGHACKPENDINGDATA pCBData =
                reinterpret_cast <DragAndDropSvc::PVBOXDNDCBGHACKPENDINGDATA>(pvParms);
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
# endif /* VBOX_WITH_DRAG_AND_DROP_GH */
        default:
            rc = VERR_NOT_SUPPORTED; /* Tell the guest. */
            break;
    }

    LogFlowFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}

# ifdef VBOX_WITH_DRAG_AND_DROP_GH
int GuestDnD::onGHSendData(GuestDnDResponse *pResp,
                           const void *pvData, size_t cbData, size_t cbTotalSize)
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

int GuestDnD::onGHSendDir(GuestDnDResponse *pResp,
                          const char *pszPath, size_t cbPath, uint32_t fMode)
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

int GuestDnD::onGHSendFile(GuestDnDResponse *pResp,
                           const char *pszPath, size_t cbPath,
                           void *pvData, size_t cbData, uint32_t fMode)
{
    AssertPtrReturn(pResp, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbPath, VERR_INVALID_PARAMETER);

    LogFlowFunc(("pszPath=%s, cbPath=%zu, fMode=0x%x\n",
                 pszPath, cbPath, fMode));

    int rc = pResp->writeToFile(pszPath, cbPath,
                                pvData, cbData, fMode);
    LogFlowFuncLeaveRC(rc);
    return rc;
}
# endif /* VBOX_WITH_DRAG_AND_DROP_GH */

///////////////////////////////////////////////////////////////////////////////

GuestDnDBase::GuestDnDBase(void)
{
    m_strFormats = GuestDnDInst()->defaultFormats();
}

HRESULT GuestDnDBase::isFormatSupported(const com::Utf8Str &aFormat, BOOL *aSupported)
{
    *aSupported = std::find(m_strFormats.begin(),
                            m_strFormats.end(), aFormat) != m_strFormats.end()
                ? TRUE : FALSE;
    return S_OK;
}

HRESULT GuestDnDBase::getFormats(std::vector<com::Utf8Str> &aFormats)
{
    aFormats = m_strFormats;

    return S_OK;
}

HRESULT GuestDnDBase::addFormats(const std::vector<com::Utf8Str> &aFormats)
{
    for (size_t i = 0; i < aFormats.size(); ++i)
    {
        Utf8Str strFormat = aFormats.at(i);
        if (std::find(m_strFormats.begin(),
                      m_strFormats.end(), strFormat) == m_strFormats.end())
        {
            m_strFormats.push_back(strFormat);
        }
    }

    return S_OK;
}

HRESULT GuestDnDBase::removeFormats(const std::vector<com::Utf8Str> &aFormats)
{
    for (size_t i = 0; i < aFormats.size(); ++i)
    {
        Utf8Str strFormat = aFormats.at(i);
        std::vector<com::Utf8Str>::iterator itFormat = std::find(m_strFormats.begin(),
                                                                 m_strFormats.end(), strFormat);
        if (itFormat != m_strFormats.end())
            m_strFormats.erase(itFormat);
    }

    return S_OK;
}

#endif /* VBOX_WITH_DRAG_AND_DROP */

