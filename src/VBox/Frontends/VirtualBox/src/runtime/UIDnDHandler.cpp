/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDnDHandler class implementation
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
#include <QMimeData>
#include <QStringList>
#include <QTimer>

/* GUI includes: */
#include "UIDnDHandler.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CSession.h"
#include "CConsole.h"
#include "CGuest.h"

UIDnDHandler *UIDnDHandler::m_pInstance = 0;

UIDnDHandler::UIDnDHandler()
{
}

/*
 * Host -> Guest
 */

Qt::DropAction UIDnDHandler::dragHGEnter(CGuest &guest, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget * /* pParent = 0 */)
{
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

Qt::DropAction UIDnDHandler::dragHGMove(CGuest &guest, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget * /* pParent = 0 */)
{
    /* Ask the guest for starting a DnD event. */
    KDragAndDropAction result = guest.DragHGMove(screenId,
                                                 x,
                                                 y,
                                                 toVBoxDnDAction(proposedAction),
                                                 toVBoxDnDActions(possibleActions),
                                                 pMimeData->formats().toVector());
    /* Set the DnD action returned by the guest. */
    return toQtDnDAction(result);
}

Qt::DropAction UIDnDHandler::dragHGDrop(CGuest &guest, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget *pParent /* = 0 */)
{
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
        /* Get the actually data */
        const QByteArray &d = pMimeData->data(format);
        if (   !d.isEmpty()
            && !format.isEmpty())
        {
            /* We need the data in the vector format. */
            QVector<uint8_t> dv(d.size());
            memcpy(dv.data(), d.constData(), d.size());

            CProgress progress = guest.DragHGPutData(screenId, format, dv);
            if (    guest.isOk()
                && !progress.isNull())
            {
                msgCenter().showModalProgressDialog(progress, tr("Dropping data ..."), ":/progress_dnd_hg_90px.png", pParent,  true);
                if (!progress.GetCanceled() && progress.isOk() && progress.GetResultCode() != 0)
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

void UIDnDHandler::dragHGLeave(CGuest &guest, ulong screenId, QWidget * /* pParent = 0 */)
{
    guest.DragHGLeave(screenId);
}

/*
 * Guest -> Host
 */

class UIDnDMimeData: public QMimeData
{
    Q_OBJECT;

    enum State
    {
        Dragging,
        Dropped,
        Finished,
        Canceled
    };

public:
    UIDnDMimeData(CSession &session, QStringList formats, Qt::DropAction defAction, Qt::DropActions actions, QWidget *pParent)
      : m_pParent(pParent)
      , m_session(session)
      , m_formats(formats)
      , m_defAction(defAction)
      , m_actions(actions)
      , m_fState(Dragging)
    {
        /* This is unbelievable hacky, but I didn't found another way. Stupid
         * Qt QDrag interface is so less verbose, that we in principle know
         * nothing about what happens when the user drag something around. It
         * is possible that the target request data (s. retrieveData) while the
         * mouse button is still pressed. This isn't something we can support,
         * cause it would mean transferring the data from the guest while the
         * mouse is still moving (thing of a 2GB file ...). So the idea is to
         * detect the mouse release event and only after this happened, allow
         * data to be retrieved. Unfortunately the QDrag object eats all events
         * while a drag is going on (see QDragManager in the Qt src's). So what
         * we do, is installing an event filter after the QDrag::exec is called
         * to be last in the event filter queue and therefore called before the
         * one installed by the QDrag object.
         *
         * Todo: test this on all supported platforms (X11 works) */
        QTimer::singleShot(0, this, SLOT(sltInstallEventFilter()));
    }

    virtual QStringList formats() const { return m_formats; }
    virtual bool hasFormat(const QString &mimeType) const { return m_formats.contains(mimeType); }

public slots:
    void sltActionChanged(Qt::DropAction action) { m_defAction = action; }

protected:
    virtual QVariant retrieveData(const QString &mimeType, QVariant::Type type) const
    {
        /* Mouse button released? */
        if (m_fState != Dropped)
            return m_data;

        /* Supported types. See below in the switch statement. */
        if (!(   type == QVariant::String
              || type == QVariant::ByteArray))
            return QVariant();

        CGuest guest = m_session.GetConsole().GetGuest();
        /* No, start getting the data from the guest. First inform the guest we
         * want the data in the specified mime-type. */
        CProgress progress = guest.DragGHDropped(mimeType, UIDnDHandler::toVBoxDnDAction(m_defAction));
        if (    guest.isOk()
            && !progress.isNull())
        {
            msgCenter().showModalProgressDialog(progress, tr("Dropping data ..."), ":/progress_dnd_gh_90px.png", m_pParent, true);
            if (!progress.GetCanceled() && progress.isOk() && progress.GetResultCode() != 0)
                msgCenter().cannotDropData(progress, m_pParent);
            else if (!progress.GetCanceled())
            {
                /* After the data is successfully arrived from the guest, we
                 * query it from Main. */
                QVector<uint8_t> data = guest.DragGHGetData();
                if (!data.isEmpty())
                {
//                    printf("qt data (%d, %d, '%s'): %s\n", data.size(), type, qPrintable(mimeType), data.data());
                    /* Todo: not sure what to add here more: needs more testing. */
                    switch (type)
                    {
                        case QVariant::String:    m_data = QVariant(QString(reinterpret_cast<const char*>(data.data()))); break;
                        case QVariant::ByteArray:
                        {
                            QByteArray ba(reinterpret_cast<const char*>(data.constData()), data.size());
                            m_data = QVariant(ba);
                            break;
                        }
                        default: break;
                    }
                }
                m_fState = Finished;
            }
            if (progress.GetCanceled())
                m_fState = Canceled;
        }
        else
            msgCenter().cannotDropData(guest, m_pParent);
        return m_data;
    }

    bool eventFilter(QObject * /* pObject */, QEvent *pEvent)
    {
        switch (pEvent->type())
        {
            case QEvent::MouseButtonRelease: m_fState = Dropped; break;
            case QEvent::KeyPress:
            {
                if (static_cast<QKeyEvent*>(pEvent)->key() == Qt::Key_Escape)
                    m_fState = Canceled;
                break;
            }
        }

        /* Propagate the event further. */
        return false;
    }

private slots:
    void sltInstallEventFilter() { qApp->installEventFilter(this); }

private:
    /* Private members. */
    QWidget          *m_pParent;
    CSession          m_session;
    QStringList       m_formats;
    Qt::DropAction    m_defAction;
    Qt::DropActions   m_actions;
    mutable State     m_fState;
    mutable QVariant  m_data;
};

void UIDnDHandler::dragGHPending(CSession &session, ulong screenId, QWidget *pParent /* = 0 */)
{
    /* How does this work: Host is asking the guest if there is any DnD
     * operation pending, when the mouse leaves the guest window
     * (DragGHPending). On return there is some info about a running DnD
     * operation (or defaultAction is KDragAndDropAction_Ignore if not). With
     * this information we create a Qt QDrag object with our own QMimeType
     * implementation and call exec. Please note, this *blocks* until the DnD
     * operation has finished. */
    CGuest guest = session.GetConsole().GetGuest();
    QVector<QString> formats;
    QVector<KDragAndDropAction> actions;
    KDragAndDropAction defaultAction = guest.DragGHPending(screenId, formats, actions);

    if (    defaultAction != KDragAndDropAction_Ignore
        && !formats.isEmpty())
    {
        QDrag *pDrag = new QDrag(pParent);
        /* pMData is transfered to the QDrag object, so no need for deletion. */
        UIDnDMimeData *pMData = new UIDnDMimeData(session, formats.toList(), toQtDnDAction(defaultAction), toQtDnDActions(actions), pParent);
        /* Inform the mime data object of any changes in the current action. */
        connect(pDrag, SIGNAL(actionChanged(Qt::DropAction)),
                pMData, SLOT(sltActionChanged(Qt::DropAction)));
        pDrag->setMimeData(pMData);
        /* Fire it up. */
        pDrag->exec(toQtDnDActions(actions), toQtDnDAction(defaultAction));
    }
}

/*
 * Drag and Drop helper methods
 */

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

Qt::DropAction UIDnDHandler::toQtDnDAction(KDragAndDropAction action)
{
    if (action == KDragAndDropAction_Copy)
        return Qt::CopyAction;
    if (action == KDragAndDropAction_Move)
        return Qt::MoveAction;
    if (action == KDragAndDropAction_Link)
        return Qt::LinkAction;

    return Qt::IgnoreAction;
}

Qt::DropActions UIDnDHandler::toQtDnDActions(const QVector<KDragAndDropAction> &actions)
{
    Qt::DropActions a = 0;
    for (int i = 0; i < actions.size(); ++i)
    {
        switch (actions.at(i))
        {
            case KDragAndDropAction_Ignore: a |= Qt::IgnoreAction; break;
            case KDragAndDropAction_Copy:   a |= Qt::CopyAction; break;
            case KDragAndDropAction_Move:   a |= Qt::MoveAction; break;
            case KDragAndDropAction_Link:   a |= Qt::LinkAction; break;
        }
    }
    return a;
}

#include "UIDnDHandler.moc"

