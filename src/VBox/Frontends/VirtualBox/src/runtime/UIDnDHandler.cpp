/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDHandler class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#include <QApplication>
#include <QKeyEvent>
#include <QStringList>
#include <QTimer>
#include <QUrl>

/* VirtualBox interface declarations: */
#ifndef VBOX_WITH_XPCOM
# include "VirtualBox.h"
#else /* !VBOX_WITH_XPCOM */
# include "VirtualBox_XPCOM.h"
#endif /* VBOX_WITH_XPCOM */

/* GUI includes: */
# include "UIDnDHandler.h"
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
# include "CDnDSource.h"
# ifdef RT_OS_WINDOWS
#  include "UIDnDDataObject_win.h"
#  include "UIDnDDropSource_win.h"
# endif
# include "UIDnDMIMEData.h"
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */
#include "UIMessageCenter.h"
#include "UISession.h"

/* COM includes: */
# include "CConsole.h"
# include "CGuest.h"
# include "CGuestDnDSource.h"
# include "CGuestDnDTarget.h"
# include "CSession.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>


UIDnDHandler::UIDnDHandler(UISession *pSession, QWidget *pParent)
    : m_pSession(pSession)
    , m_pParent(pParent)
    , m_enmMode(DNDMODE_UNKNOWN)
    , m_fIsPending(false)
#ifndef RT_OS_WINDOWS
    , m_pMIMEData(NULL)
#endif
{
    AssertPtr(pSession);
    m_dndSource = static_cast<CDnDSource>(pSession->guest().GetDnDSource());
    m_dndTarget = static_cast<CDnDTarget>(pSession->guest().GetDnDTarget());
}

UIDnDHandler::~UIDnDHandler(void)
{
}

/*
 * Frontend -> Target.
 */

Qt::DropAction UIDnDHandler::dragEnter(ulong screenID, int x, int y,
                                       Qt::DropAction proposedAction, Qt::DropActions possibleActions,
                                       const QMimeData *pMimeData)
{
    LogFlowFunc(("enmMode=%RU32, screenID=%RU32, x=%d, y=%d, action=%ld\n",
                 m_enmMode, screenID, x, y, toVBoxDnDAction(proposedAction)));

    if (   m_enmMode != DNDMODE_UNKNOWN
        && m_enmMode != DNDMODE_HOSTTOGUEST)
        return Qt::IgnoreAction;

    /* Ask the guest for starting a DnD event. */
    KDnDAction result = m_dndTarget.Enter(screenID,
                                          x,
                                          y,
                                          toVBoxDnDAction(proposedAction),
                                          toVBoxDnDActions(possibleActions),
                                          pMimeData->formats().toVector());
    if (m_dndTarget.isOk())
        setMode(DNDMODE_HOSTTOGUEST);

    /* Set the DnD action returned by the guest. */
    return toQtDnDAction(result);
}

Qt::DropAction UIDnDHandler::dragMove(ulong screenID, int x, int y,
                                      Qt::DropAction proposedAction, Qt::DropActions possibleActions,
                                      const QMimeData *pMimeData)
{
    LogFlowFunc(("enmMode=%RU32, screenID=%RU32, x=%d, y=%d, action=%ld\n",
                 m_enmMode, screenID, x, y, toVBoxDnDAction(proposedAction)));

    if (m_enmMode != DNDMODE_HOSTTOGUEST)
        return Qt::IgnoreAction;

    /* Notify the guest that the mouse has been moved while doing
     * a drag'n drop operation. */
    KDnDAction result = m_dndTarget.Move(screenID,
                                         x,
                                         y,
                                         toVBoxDnDAction(proposedAction),
                                         toVBoxDnDActions(possibleActions),
                                         pMimeData->formats().toVector());
    /* Set the DnD action returned by the guest. */
    return toQtDnDAction(result);
}

Qt::DropAction UIDnDHandler::dragDrop(ulong screenID, int x, int y,
                                      Qt::DropAction proposedAction, Qt::DropActions possibleActions,
                                      const QMimeData *pMimeData)
{
    LogFlowFunc(("enmMode=%RU32, screenID=%RU32, x=%d, y=%d, action=%ld\n",
                 m_enmMode, screenID, x, y, toVBoxDnDAction(proposedAction)));

    if (m_enmMode != DNDMODE_HOSTTOGUEST)
        return Qt::IgnoreAction;

    /* The format the guest requests. */
    QString format;
    /* Ask the guest for dropping data. */
    KDnDAction result = m_dndTarget.Drop(screenID,
                                         x,
                                         y,
                                         toVBoxDnDAction(proposedAction),
                                         toVBoxDnDActions(possibleActions),
                                         pMimeData->formats().toVector(), format);

    /* Has the guest accepted the drop event? */
    if (   m_dndTarget.isOk()
        && result != KDnDAction_Ignore)
    {
        /* Get the actual MIME data in the requested format. */
        AssertPtr(pMimeData);
        const QByteArray &d = pMimeData->data(format);
        if (   !d.isEmpty()
            && !format.isEmpty())
        {
            /* Convert the actual MIME data to a vector (needed for the COM wrapper). */
            QVector<uint8_t> dv(d.size());
            memcpy(dv.data(), d.constData(), d.size());

            CProgress progress = m_dndTarget.SendData(screenID, format, dv);

            if (m_dndTarget.isOk())
            {
                LogFlowFunc(("Transferring data to guest ...\n"));

                msgCenter().showModalProgressDialog(progress,
                                                    tr("Dropping data ..."), ":/progress_dnd_hg_90px.png",
                                                    m_pParent);

                LogFlowFunc(("Transfer fCompleted=%RTbool, fCanceled=%RTbool, hr=%Rhrc\n",
                             progress.GetCompleted(), progress.GetCanceled(), progress.GetResultCode()));

                BOOL fCanceled = progress.GetCanceled();
                if (   !fCanceled
                    && (   !progress.isOk()
                        ||  progress.GetResultCode() != 0))
                {
                    msgCenter().cannotDropDataToGuest(progress, m_pParent);
                    result = KDnDAction_Ignore;
                }
            }
            else
            {
                msgCenter().cannotDropDataToGuest(m_dndTarget, m_pParent);
                result = KDnDAction_Ignore;
            }
        }
    }

    /*
     * Since the mouse button has been release this in any case marks
     * the end of the current transfer direction. So reset the current
     * mode as well here.
     */
    setMode(DNDMODE_UNKNOWN);

    return toQtDnDAction(result);
}

void UIDnDHandler::dragLeave(ulong screenID)
{
    LogFlowFunc(("enmMode=%RU32, screenID=%RU32\n", m_enmMode, screenID));

    if (m_enmMode == DNDMODE_HOSTTOGUEST)
    {
        m_dndTarget.Leave(screenID);
        setMode(DNDMODE_UNKNOWN);
    }
}

/*
 * Source -> Frontend.
 */

int UIDnDHandler::dragStartInternal(const QStringList &lstFormats,
                                    Qt::DropAction defAction, Qt::DropActions actions)
{
    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_DRAG_AND_DROP_GH

    LogFlowFunc(("defAction=0x%x\n", defAction));
    LogFlowFunc(("Number of formats: %d\n", lstFormats.size()));
# ifdef DEBUG
    for (int i = 0; i < lstFormats.size(); i++)
        LogFlowFunc(("\tFormat %d: %s\n", i, lstFormats.at(i).toAscii().constData()));
# endif

# ifdef RT_OS_WINDOWS

    UIDnDDropSource *pDropSource = new UIDnDDropSource(m_pParent);
    if (!pDropSource)
        return VERR_NO_MEMORY;
    UIDnDDataObject *pDataObject = new UIDnDDataObject(this, lstFormats);
    if (!pDataObject)
        return VERR_NO_MEMORY;

    DWORD dwOKEffects = DROPEFFECT_NONE;
    if (actions)
    {
        if (actions & Qt::CopyAction)
            dwOKEffects |= DROPEFFECT_COPY;
        if (actions & Qt::MoveAction)
            dwOKEffects |= DROPEFFECT_MOVE;
        if (actions & Qt::LinkAction)
            dwOKEffects |= DROPEFFECT_LINK;
    }

    DWORD dwEffect;
    LogRel3(("DnD: dwOKEffects=0x%x\n", dwOKEffects));
    HRESULT hr = ::DoDragDrop(pDataObject, pDropSource, dwOKEffects, &dwEffect);
    LogRel3(("DnD: DoDragDrop ended with hr=%Rhrc, dwEffect=%RI32\n", hr, dwEffect));

    if (pDropSource)
        pDropSource->Release();
    if (pDataObject)
        pDataObject->Release();

# else /* !RT_OS_WINDOWS */

    QDrag *pDrag = new QDrag(m_pParent);
    if (!pDrag)
        return VERR_NO_MEMORY;

    /* Note: pMData is transferred to the QDrag object, so no need for deletion. */
    m_pMIMEData = new UIDnDMIMEData(this, lstFormats, defAction, actions);
    if (!m_pMIMEData)
    {
        delete pDrag;
        return VERR_NO_MEMORY;
    }

    /* Invoke this handler as data needs to be retrieved. */
    connect(m_pMIMEData, SIGNAL(getData(QString, QVariant::Type, QVariant&)),
            this, SLOT(sltGetData(QString, QVariant::Type, QVariant&)));

    /* Inform the MIME data object of any changes in the current action. */
    connect(pDrag, SIGNAL(actionChanged(Qt::DropAction)),
            m_pMIMEData, SLOT(sltDropActionChanged(Qt::DropAction)));

    /*
     * Set MIME data object and start the (modal) drag'n drop operation on the host.
     * This does not block Qt's event loop, however (on Windows it would).
     */
    pDrag->setMimeData(m_pMIMEData);
    LogFlowFunc(("Executing modal drag'n drop operation ...\n"));
    Qt::DropAction dropAction = pDrag->exec(actions, defAction);
    LogRel3(("DnD: Ended with dropAction=%ld\n", UIDnDHandler::toVBoxDnDAction(dropAction)));

    /* Note: The UIDnDMimeData object will not be not accessible here anymore,
     *       since QDrag had its ownership and deleted it after the (blocking)
     *       QDrag::exec() call. */

    /* pDrag will be cleaned up by Qt automatically. */

# endif /* !RT_OS_WINDOWS */

#else /* VBOX_WITH_DRAG_AND_DROP_GH */

    rc = VERR_NOT_SUPPORTED;

#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int UIDnDHandler::dragCheckPending(ulong screenID)
{
    int rc;
#ifdef VBOX_WITH_DRAG_AND_DROP_GH

    LogFlowFunc(("enmMode=%RU32, fIsPending=%RTbool, screenID=%RU32\n", m_enmMode, m_fIsPending, screenID));

    {
        QMutexLocker AutoReadLock(&m_ReadLock);

        if (   m_enmMode != DNDMODE_UNKNOWN
            && m_enmMode != DNDMODE_GUESTTOHOST) /* Wrong mode set? */
            return VINF_SUCCESS;

        if (m_fIsPending) /* Pending operation is in progress. */
            return VINF_SUCCESS;
    }

    QMutexLocker AutoWriteLock(&m_WriteLock);
    m_fIsPending = true;
    AutoWriteLock.unlock();

    /**
     * How this works: Source is asking the target if there is any DnD
     * operation pending, when the mouse leaves the guest window. On
     * return there is some info about a running DnD operation
     * (or defaultAction is KDnDAction_Ignore if not). With
     * this information we create a Qt QDrag object with our own QMimeType
     * implementation and call exec.
     *
     * Note: This function *blocks* until the actual drag'n drop operation
     *       has been finished (successfully or not)!
     */
    CGuest guest = m_pSession->guest();

    /* Clear our current data set. */
    m_dataSource.lstFormats.clear();
    m_dataSource.vecActions.clear();

    /* Ask the guest if there is a drag and drop operation pending (on the guest). */
    QVector<QString> vecFormats;
    m_dataSource.defaultAction = m_dndSource.DragIsPending(screenID, vecFormats, m_dataSource.vecActions);

    LogRel3(("DnD: Default action is: 0x%x\n", m_dataSource.defaultAction));
    LogRel3(("DnD: Number of supported guest actions: %d\n", m_dataSource.vecActions.size()));
        for (int i = 0; i < m_dataSource.vecActions.size(); i++)
            LogRel3(("\tAction %d: 0x%x\n", i, m_dataSource.vecActions.at(i)));

    LogRel3(("DnD: Number of supported guest formats: %d\n", vecFormats.size()));
        for (int i = 0; i < vecFormats.size(); i++)
        {
            const QString &strFmtGuest = vecFormats.at(i);
            LogRel3(("\tFormat %d: %s\n", i, strFmtGuest.toAscii().constData()));
        }

    if (   m_dataSource.defaultAction != KDnDAction_Ignore
        && vecFormats.size())
    {
        for (int i = 0; i < vecFormats.size(); i++)
        {
            const QString &strFormat = vecFormats.at(i);
            m_dataSource.lstFormats << strFormat;
        }

        rc = VINF_SUCCESS; /* There's a valid pending drag and drop operation on the guest. */
    }
    else /* No format data from the guest arrived yet. */
        rc = VERR_NO_DATA;

    AutoWriteLock.relock();
    m_fIsPending = false;
    AutoWriteLock.unlock();

#else /* !VBOX_WITH_DRAG_AND_DROP_GH */

    NOREF(screenID);

    rc = VERR_NOT_SUPPORTED;

#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int UIDnDHandler::dragStart(ulong screenID)
{
    int rc;
#ifdef VBOX_WITH_DRAG_AND_DROP_GH

    NOREF(screenID);

    LogFlowFuncEnter();

    /* Sanity checks. */
    if (   !m_dataSource.lstFormats.size()
        ||  m_dataSource.defaultAction == KDnDAction_Ignore
        || !m_dataSource.vecActions.size())
    {
        return VERR_INVALID_PARAMETER;
    }

    setMode(DNDMODE_GUESTTOHOST);

    rc = dragStartInternal(m_dataSource.lstFormats,
                           toQtDnDAction(m_dataSource.defaultAction), toQtDnDActions(m_dataSource.vecActions));

#else /* !VBOX_WITH_DRAG_AND_DROP_GH */

    NOREF(screenID);

    rc = VERR_NOT_SUPPORTED;

#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int UIDnDHandler::dragStop(ulong screenID)
{
    int rc;
#ifdef VBOX_WITH_DRAG_AND_DROP_GH

    NOREF(screenID);

    m_fIsPending = false;
    rc = VINF_SUCCESS;

#else /* !VBOX_WITH_DRAG_AND_DROP_GH */

    NOREF(screenID);

    rc = VERR_NOT_SUPPORTED;

#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int UIDnDHandler::retrieveData(      Qt::DropAction  dropAction,
                               const QString        &strMimeType,
                                     QVariant::Type  vaType,
                                     QVariant       &vaData)
{
    return retrieveDataInternal(dropAction, strMimeType, vaType, vaData);
}

int UIDnDHandler::retrieveDataInternal(      Qt::DropAction  dropAction,
                                       const QString        &strMimeType,
                                             QVariant::Type  vaType,
                                             QVariant       &vaData)
{
    LogFlowFunc(("Retrieving data as type=%s (variant type=%RU32)\n",
                 strMimeType.toAscii().constData(), vaType));

    int rc = VINF_SUCCESS;

    /* Start getting the data from the source. Request and transfer data
     * from the source and display a modal progress dialog while doing this. */
    Assert(!m_dndSource.isNull());
    CProgress progress = m_dndSource.Drop(strMimeType,
                                          UIDnDHandler::toVBoxDnDAction(dropAction));
    if (m_dndSource.isOk())
    {
        msgCenter().showModalProgressDialog(progress,
                                            tr("Retrieving data ..."), ":/progress_dnd_gh_90px.png",
                                            m_pParent);

        LogFlowFunc(("fCanceled=%RTbool, fCompleted=%RTbool, isOk=%RTbool, hrc=%Rhrc\n",
                     progress.GetCanceled(), progress.GetCompleted(), progress.isOk(), progress.GetResultCode()));

        if (!progress.GetCanceled())
        {
            rc =   (   progress.isOk()
                    && progress.GetResultCode() == 0)
                 ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Fudge; do a GetResultCode() to rc translation. */

            if (RT_SUCCESS(rc))
            {
                /* After we successfully retrieved data from the source we query it from Main. */
                QVector<uint8_t> vecData = m_dndSource.ReceiveData();
                if (!vecData.isEmpty())
                {
                    switch (vaType)
                    {
                        case QVariant::String:
                        {
                            vaData = QVariant::fromValue(QString(reinterpret_cast<const char *>(vecData.constData())));
                            Assert(vaData.type() == QVariant::Type::String);
                            break;
                        }

                        case QVariant::ByteArray:
                        {
                            QByteArray ba(reinterpret_cast<const char*>(vecData.constData()), vecData.size());

                            vaData = QVariant::fromValue(ba);
                            Assert(vaData.type() == QVariant::Type::ByteArray);
                            break;
                        }

                        case QVariant::StringList:
                        {
                            QString strData = QString(reinterpret_cast<const char*>(vecData.constData()));
                            QStringList lstString = strData.split("\r\n", QString::SkipEmptyParts);

                            vaData = QVariant::fromValue(lstString);
                            Assert(vaData.type() == QVariant::Type::StringList);
                            break;
                        }

                        default:
                            rc = VERR_NOT_SUPPORTED;
                            break;
                    }
                }
                else
                    rc = VERR_NO_DATA;
            }
            else
                msgCenter().cannotDropDataToHost(progress, m_pParent);
        }
        else /* Don't pop up a message. */
            rc = VERR_CANCELLED;
    }
    else
    {
        msgCenter().cannotDropDataToHost(m_dndSource, m_pParent);
        rc = VERR_GENERAL_FAILURE; /** @todo Fudge; do a GetResultCode() to rc translation. */
    }

    setMode(DNDMODE_UNKNOWN);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void UIDnDHandler::setMode(DNDMODE enmMode)
{
    QMutexLocker AutoWriteLock(&m_WriteLock);
    m_enmMode = enmMode;
    LogFlowFunc(("Mode is now: %RU32\n", m_enmMode));
}

int UIDnDHandler::sltGetData(const QString        &strMimeType,
                                   QVariant::Type  vaType,
                                   QVariant       &vaData)
{
    int rc = retrieveDataInternal(Qt::CopyAction, strMimeType, vaType, vaData);
    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
 * Drag and Drop helper methods
 */

/* static */
KDnDAction UIDnDHandler::toVBoxDnDAction(Qt::DropAction action)
{
    if (action == Qt::CopyAction)
        return KDnDAction_Copy;
    if (action == Qt::MoveAction)
        return KDnDAction_Move;
    if (action == Qt::LinkAction)
        return KDnDAction_Link;

    return KDnDAction_Ignore;
}

/* static */
QVector<KDnDAction> UIDnDHandler::toVBoxDnDActions(Qt::DropActions actions)
{
    QVector<KDnDAction> vbActions;
    if (actions.testFlag(Qt::IgnoreAction))
        vbActions << KDnDAction_Ignore;
    if (actions.testFlag(Qt::CopyAction))
        vbActions << KDnDAction_Copy;
    if (actions.testFlag(Qt::MoveAction))
        vbActions << KDnDAction_Move;
    if (actions.testFlag(Qt::LinkAction))
        vbActions << KDnDAction_Link;

    return vbActions;
}

/* static */
Qt::DropAction UIDnDHandler::toQtDnDAction(KDnDAction action)
{
    Qt::DropAction dropAct = Qt::IgnoreAction;
    if (action == KDnDAction_Copy)
        dropAct = Qt::CopyAction;
    if (action == KDnDAction_Move)
        dropAct = Qt::MoveAction;
    if (action == KDnDAction_Link)
        dropAct = Qt::LinkAction;

    LogFlowFunc(("dropAct=0x%x\n", dropAct));
    return dropAct;
}

/* static */
Qt::DropActions UIDnDHandler::toQtDnDActions(const QVector<KDnDAction> &vecActions)
{
    Qt::DropActions dropActs = Qt::IgnoreAction;
    for (int i = 0; i < vecActions.size(); i++)
    {
        switch (vecActions.at(i))
        {
            case KDnDAction_Ignore:
                dropActs |= Qt::IgnoreAction;
                break;
            case KDnDAction_Copy:
                dropActs |= Qt::CopyAction;
                break;
            case KDnDAction_Move:
                dropActs |= Qt::MoveAction;
                break;
            case KDnDAction_Link:
                dropActs |= Qt::LinkAction;
                break;
            default:
                break;
        }
    }

    LogFlowFunc(("dropActions=0x%x\n", int(dropActs)));
    return dropActs;
}

