/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDnDHandler class implementation
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

/* Qt includes: */
#include <QApplication>
#include <QKeyEvent>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

/* GUI includes: */
#include "UIDnDHandler.h"
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
# include "UIDnDDrag.h"
#endif
#include "UIMessageCenter.h"

/* COM includes: */
#include "CSession.h"
#include "CConsole.h"
#include "CGuest.h"

UIDnDHandler *UIDnDHandler::m_pInstance = NULL;

UIDnDHandler::UIDnDHandler(void)
    : mMode(Unknown)
{
}

/*
 * Host -> Guest
 */

Qt::DropAction UIDnDHandler::dragHGEnter(CGuest &guest, ulong screenId, int x, int y,
                                         Qt::DropAction proposedAction, Qt::DropActions possibleActions,
                                         const QMimeData *pMimeData, QWidget * /* pParent = NULL */)
{
    if (mMode == GH) /* Wrong mode? Bail out. */
        return Qt::IgnoreAction;

    LogFlowFunc(("screenId=%RU32, x=%d, y=%d, action=%ld\n",
                 screenId, x, y, toVBoxDnDAction(proposedAction)));

    /* Ask the guest for starting a DnD event. */
    KDragAndDropAction result = guest.DragHGEnter(screenId,
                                                  x,
                                                  y,
                                                  toVBoxDnDAction(proposedAction),
                                                  toVBoxDnDActions(possibleActions),
                                                  pMimeData->formats().toVector());
    /* Set the DnD action returned by the guest. */
    return toQtDnDAction(result);
}

Qt::DropAction UIDnDHandler::dragHGMove(CGuest &guest, ulong screenId, int x, int y,
                                        Qt::DropAction proposedAction, Qt::DropActions possibleActions,
                                        const QMimeData *pMimeData, QWidget * /* pParent = NULL */)
{
    if (mMode == GH) /* Wrong mode? Bail out. */
        return Qt::IgnoreAction;

#ifdef DEBUG_andy
    LogFlowFunc(("screenId=%RU32, x=%d, y=%d, action=%ld\n",
                 screenId, x, y, toVBoxDnDAction(proposedAction)));
#endif

    /* Notify the guest that the mouse has been moved while doing
     * a drag'n drop operation. */
    KDragAndDropAction result = guest.DragHGMove(screenId,
                                                 x,
                                                 y,
                                                 toVBoxDnDAction(proposedAction),
                                                 toVBoxDnDActions(possibleActions),
                                                 pMimeData->formats().toVector());
    /* Set the DnD action returned by the guest. */
    return toQtDnDAction(result);
}

Qt::DropAction UIDnDHandler::dragHGDrop(CGuest &guest, ulong screenId, int x, int y,
                                        Qt::DropAction proposedAction, Qt::DropActions possibleActions,
                                        const QMimeData *pMimeData, QWidget *pParent /* = NULL */)
{
    if (mMode == GH) /* Wrong mode? Bail out. */
        return Qt::IgnoreAction;

    LogFlowFunc(("screenId=%RU32, x=%d, y=%d, action=%ld\n",
                 screenId, x, y, toVBoxDnDAction(proposedAction)));

    /* The format the guest requests. */
    QString format;
    /* Ask the guest for dropping data. */
    KDragAndDropAction result = guest.DragHGDrop(screenId,
                                                 x,
                                                 y,
                                                 toVBoxDnDAction(proposedAction),
                                                 toVBoxDnDActions(possibleActions),
                                                 pMimeData->formats().toVector(), format);
    /* Has the guest accepted the drop event? */
    if (result != KDragAndDropAction_Ignore)
    {
        /* Get the actual MIME data in the requested format. */
        const QByteArray &d = pMimeData->data(format);
        if (   !d.isEmpty()
            && !format.isEmpty())
        {
            /* Convert the actual MIME data to a vector (needed
             * for the COM wrapper). */
            QVector<uint8_t> dv(d.size());
            memcpy(dv.data(), d.constData(), d.size());

            CProgress progress = guest.DragHGPutData(screenId, format, dv);
            if (guest.isOk())
            {
                msgCenter().showModalProgressDialog(progress,
                                                    tr("Dropping data ..."), ":/progress_dnd_hg_90px.png",
                                                    pParent);
                if (   !progress.GetCanceled()
                    && (   !progress.isOk()
                        ||  progress.GetResultCode() != 0))
                {
                    msgCenter().cannotDropData(progress, pParent);
                    result = KDragAndDropAction_Ignore;
                }
            }
            else
            {
                msgCenter().cannotDropData(guest, pParent);
                result = KDragAndDropAction_Ignore;
            }
        }
    }

    return toQtDnDAction(result);
}

void UIDnDHandler::dragHGLeave(CGuest &guest, ulong screenId, QWidget * /* pParent = NULL */)
{
    if (mMode == GH) /* Wrong mode? Bail out. */
        return;

    LogFlowFunc(("screenId=%RU32\n", screenId));
    guest.DragHGLeave(screenId);
}

int UIDnDHandler::dragGHPending(CSession &session, ulong screenId, QWidget *pParent /* = NULL */)
{
    int rc;
#ifdef VBOX_WITH_DRAG_AND_DROP_GH
    if (mMode == HG) /* Wrong mode? Bail out. */
        return VERR_WRONG_ORDER;

    /*
     * How this works: Host is asking the guest if there is any DnD
     * operation pending, when the mouse leaves the guest window
     * (DragGHPending). On return there is some info about a running DnD
     * operation (or defaultAction is KDragAndDropAction_Ignore if not). With
     * this information we create a Qt QDrag object with our own QMimeType
     * implementation and call exec. Please note, this *blocks* until the DnD
     * operation has finished.
     */
    CGuest guest = session.GetConsole().GetGuest();
    QVector<QString> vecFmtGuest;
    QVector<KDragAndDropAction> vecActions;
    KDragAndDropAction defaultAction = guest.DragGHPending(screenId, vecFmtGuest, vecActions);
    LogFlowFunc(("defaultAction=%d, numFormats=%d\n", defaultAction, vecFmtGuest.size()));

    QStringList lstFmtNative;
    if (defaultAction != KDragAndDropAction_Ignore)
    {
        /*
         * Do guest -> host format conversion, if needed.
         * On X11 this already maps to the Xdnd protocol.
         ** @todo What about the MacOS Carbon Drag Manager? Needs testing.
         *
         * See: https://www.iana.org/assignments/media-types/media-types.xhtml
         */
        LogFlowFunc(("Number of guest formats: %d\n", vecFmtGuest.size()));
        for (int i = 0; i < vecFmtGuest.size(); i++)
        {
            const QString &strFmtGuest = vecFmtGuest.at(i);
            LogFlowFunc(("\tFormat %d: %s\n", i,
                         strFmtGuest.toAscii().constData()));
# ifdef RT_OS_WINDOWS
            /* CF_TEXT */
            if (   strFmtGuest.contains("text/plain", Qt::CaseInsensitive)
                && !lstFmtNative.contains("text/plain"))
            {
                lstFmtNative << "text/plain";
            }
            /* CF_HDROP */
            else if (   strFmtGuest.contains("text/uri-list", Qt::CaseInsensitive)
                     && !lstFmtNative.contains("text/uri-list"))
            {
                lstFmtNative << "text/uri-list";
            }
# else
            /* On non-Windows just do a 1:1 mapping. */
            lstFmtNative << strFmtGuest;
#  ifdef RT_OS_MACOS
            /** @todo Does the mapping apply here? Don't think so ... */
#  endif
# endif /* !RT_OS_WINDOWS */
        }

        LogFlowFunc(("Number of native formats: %d\n", lstFmtNative.size()));
# ifdef DEBUG
        for (int i = 0; i < lstFmtNative.size(); i++)
            LogFlowFunc(("\tFormat %d: %s\n", i, lstFmtNative.at(i).toAscii().constData()));
# endif
    }

    if (!lstFmtNative.isEmpty())
    {
        UIDnDDrag *pDrag = new UIDnDDrag(session, lstFmtNative,
                                         toQtDnDAction(defaultAction),
                                         toQtDnDActions(vecActions), pParent);
        rc = pDrag->DoDragDrop();

        delete pDrag;
    }
    else
        rc = VINF_SUCCESS;
#else /* !VBOX_WITH_DRAG_AND_DROP_GH */
    NOREF(session);
    NOREF(screenId);
    NOREF(pParent);

    rc = VERR_NOT_SUPPORTED;
#endif /* VBOX_WITH_DRAG_AND_DROP_GH */

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/*
 * Drag and Drop helper methods
 */

/* static */
KDragAndDropAction UIDnDHandler::toVBoxDnDAction(Qt::DropAction action)
{
    if (action == Qt::CopyAction)
        return KDragAndDropAction_Copy;
    if (action == Qt::MoveAction)
        return KDragAndDropAction_Move;
    if (action == Qt::LinkAction)
        return KDragAndDropAction_Link;

    return KDragAndDropAction_Ignore;
}

/* static */
QVector<KDragAndDropAction> UIDnDHandler::toVBoxDnDActions(Qt::DropActions actions)
{
    QVector<KDragAndDropAction> vbActions;
    if (actions.testFlag(Qt::IgnoreAction))
        vbActions << KDragAndDropAction_Ignore;
    if (actions.testFlag(Qt::CopyAction))
        vbActions << KDragAndDropAction_Copy;
    if (actions.testFlag(Qt::MoveAction))
        vbActions << KDragAndDropAction_Move;
    if (actions.testFlag(Qt::LinkAction))
        vbActions << KDragAndDropAction_Link;

    return vbActions;
}

/* static */
Qt::DropAction UIDnDHandler::toQtDnDAction(KDragAndDropAction action)
{
    Qt::DropAction dropAct = Qt::IgnoreAction;
    if (action == KDragAndDropAction_Copy)
        dropAct = Qt::CopyAction;
    if (action == KDragAndDropAction_Move)
        dropAct = Qt::MoveAction;
    if (action == KDragAndDropAction_Link)
        dropAct = Qt::LinkAction;

    LogFlowFunc(("dropAct=0x%x\n", dropAct));
    return dropAct;
}

/* static */
Qt::DropActions UIDnDHandler::toQtDnDActions(const QVector<KDragAndDropAction> &vecActions)
{
    Qt::DropActions dropActs = Qt::IgnoreAction;
    for (int i = 0; i < vecActions.size(); i++)
    {
        switch (vecActions.at(i))
        {
            case KDragAndDropAction_Ignore:
                dropActs |= Qt::IgnoreAction;
                break;
            case KDragAndDropAction_Copy:
                dropActs |= Qt::CopyAction;
                break;
            case KDragAndDropAction_Move:
                dropActs |= Qt::MoveAction;
                break;
            case KDragAndDropAction_Link:
                dropActs |= Qt::LinkAction;
                break;
            default:
                break;
        }
    }

    LogFlowFunc(("dropActions=0x%x\n", int(dropActs)));
    return dropActs;
}

