/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDnDMIMEData class implementation
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
#include <QMimeData>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

/* GUI includes: */
#include "UIDnDMIMEData.h"
#include "UIMessageCenter.h"

UIDnDMimeData::UIDnDMimeData(CSession &session, QStringList formats,
                             Qt::DropAction defAction, Qt::DropActions actions,
                             QWidget *pParent)
    : m_pParent(pParent)
    , m_session(session)
    , m_lstFormats(formats)
    , m_defAction(defAction)
    , m_actions(actions)
    , m_enmState(Dragging)
    , m_data(QVariant::Invalid)
{
    LogFlowThisFuncEnter();

    /*
     * This is unbelievable hacky, but I didn't find another way. Stupid
     * Qt QDrag interface is so less verbose, that we in principle know
     * nothing about what happens when the user drag something around. It
     * is possible that the target on the host requests data
     * (@sa retrieveData) while the mouse button still is pressed. This
     * isn't something we should support, because it would mean transferring
     * the data from the guest while the mouse is still moving (think of
     * transferring a 2GB file from the guest to the host ...). So the idea is
     * to detect the mouse release event and only after this happened, allow
     * data to be retrieved. Unfortunately the QDrag object eats all events
     * while a drag is going on (see QDragManager in the Qt src's).
     *
     * So what we now are going to do is installing an event filter after the
     * QDrag::exec is called, so that this event filter then would be
     * the last in the event filter queue and therefore called before the
     * one installed by the QDrag object (which then in turn would
     * munch all events).
     *
     ** @todo Test this on all supported platforms (X11 works).
     *
     * Note: On Windows the above hack is not needed because as soon as Qt calls
     *       OLE's DoDragDrop routine internally (via QtDrag::exec), no mouse
     *       events will come through anymore. At this point DoDragDrop is modal
     *       and will take care of all the input handling. */
#ifndef RT_OS_WINDOWS
    QTimer::singleShot(0, this, SLOT(sltInstallEventFilter()));
#endif

#ifdef DEBUG
    LogFlowFunc(("Number of formats: %d\n", formats.size()));
    for (int i = 0; i < formats.size(); i++)
        LogFlowFunc(("\tFormat %d: %s\n", i, formats.at(i).toAscii().constData()));
#endif
}

void UIDnDMimeData::sltDropActionChanged(Qt::DropAction dropAction)
{
    LogFlowFunc(("dropAction=0x%x\n", dropAction));
    m_defAction = dropAction;
}

QStringList UIDnDMimeData::formats(void) const
{
    return m_lstFormats;
}

bool UIDnDMimeData::hasFormat(const QString &mimeType) const
{
    bool fRc = m_lstFormats.contains(mimeType);
    LogFlowFunc(("%s: %RTbool (QtMimeData: %RTbool)\n",
                 mimeType.toStdString().c_str(),
                 fRc, QMimeData::hasFormat(mimeType)));
    return fRc;
}

QVariant UIDnDMimeData::retrieveData(const QString &mimeType,
                                     QVariant::Type type) const
{
    LogFlowFunc(("m_enmState=%d, mimeType=%s, type=%d (%s)\n",
                 m_enmState, mimeType.toStdString().c_str(),
                 type, QVariant::typeToName(type)));

    bool fCanDrop = true;

#if 0
#ifdef RT_OS_WINDOWS
    Qt::MouseButtons mouseBtns = Qt::NoButton;
    bool fLeftBtnDown = RT_BOOL(GetAsyncKeyState(VK_LBUTTON) & 0x8000);
    if (fLeftBtnDown)
        mouseBtns |= Qt::LeftButton;
# ifdef DEBUG_andy
    LogFlowFunc(("mouseButtons=0x%x, GetAsyncKeyState(VK_LBUTTON)=%RTbool\n",
                 mouseBtns, fLeftBtnDown));
# endif
    if (mouseBtns == Qt::NoButton)
        m_enmState = Dropped;
#endif
#endif

#ifdef RT_OS_WINDOWS
    /* On Windows we only will get into this function if OLE's DoDragDrop
     * routine (called by QtDrag) decides that a drop event just happened.
     * So just update our internal state to reflect the same as on other
     * platforms. */
    fCanDrop = true;
#else
    /* Mouse button released? See eventFilter for more information. */
    if (m_enmState != Dropped)
        fCanDrop = false;
#endif

    /* Do we support the requested MIME type? */
    if (   fCanDrop
        && !m_lstFormats.contains(mimeType))
    {
        LogFlowFunc(("Unsupported MIME type=%s\n",
                     mimeType.toStdString().c_str()));
        fCanDrop = false;
    }

    /* Supported types. See below in the switch statement. */
    if (   fCanDrop
        && !(
             /* Plain text. */
                type == QVariant::String
             /* Binary data. */
             || type == QVariant::ByteArray
             /* URI list. */
             || type == QVariant::List))
    {
        LogFlowFunc(("Unsupported data type=%d (%s)\n",
                     type, QVariant::typeToName(type)));
        fCanDrop = false;
    }

    if (!fCanDrop)
    {
        LogFlowFunc(("Skipping request, m_enmState=%d ...\n",
                     m_enmState));
        return QMimeData::retrieveData(mimeType, type);
    }

    if (m_enmState == Dragging)
    {
        int rc = VINF_SUCCESS;

        CGuest guest = m_session.GetConsole().GetGuest();
        /* Start getting the data from the guest. First inform the guest we
         * want the data in the specified MIME type. */
        CProgress progress = guest.DragGHDropped(mimeType,
                                                 UIDnDHandler::toVBoxDnDAction(m_defAction));
        if (guest.isOk())
        {
            msgCenter().showModalProgressDialog(progress,
                                                tr("Retrieving data ..."), ":/progress_dnd_gh_90px.png",
                                                m_pParent);
            if (!progress.GetCanceled())
            {
                if (   progress.isOk()
                    && progress.GetResultCode() == 0)
                {
                    /** @todo What about retrieving bigger files? Loop? */

                    /* After the data successfully arrived from the guest, we query it from Main. */
                    QVector<uint8_t> data = guest.DragGHGetData();
                    if (!data.isEmpty())
                    {
                        switch (type)
                        {
                            case QVariant::String:
                            {
                                m_data = QVariant(QString(reinterpret_cast<const char*>(data.data())));
                                break;
                            }

                            case QVariant::ByteArray:
                            {
                                QByteArray ba(reinterpret_cast<const char*>(data.constData()), data.size());
                                m_data = QVariant(ba);
                                break;
                            }

                            case QVariant::List:
                            {
                                QString strData = QString(reinterpret_cast<const char*>(data.data()));
                                QStringList lstString = strData.split("\r\n", QString::SkipEmptyParts);

                                m_data = QVariant(lstString);
                                break;
                            }

                            default:
                                AssertMsgFailed(("Should never happen, d'oh!\n"));
                                rc = VERR_NOT_SUPPORTED;
                                break;
                        }
                    }
                    /** @todo How often to retry on empty data received? */

                    if (RT_SUCCESS(rc))
                        emit sigDataAvailable(mimeType);

                    m_enmState = DataRetrieved;
                }
                else
                    msgCenter().cannotDropData(progress, m_pParent);
            }
            else
                m_enmState = Canceled;
        }
        else
            msgCenter().cannotDropData(guest, m_pParent);
    }

    //return QMimeData::retrieveData(mimeType, type);
    return m_data;
}

#ifndef RT_OS_WINDOWS
bool UIDnDMimeData::eventFilter(QObject * /* pObject */, QEvent *pEvent)
{
    if (pEvent)
    {
        switch (pEvent->type())
        {
#ifdef DEBUG_andy
            case QEvent::MouseMove:
            {
                QMouseEvent *pMouseEvent = (QMouseEvent*)(pEvent);
                AssertPtr(pMouseEvent);
                LogFlowFunc(("MouseMove: x=%d, y=%d, buttons=0x%x\n",
                             pMouseEvent->globalX(), pMouseEvent->globalY(), pMouseEvent->buttons()));
                break;
            }
#endif
            case QEvent::MouseButtonRelease:
            {
                LogFlowFunc(("MouseButtonRelease\n"));
                m_enmState = Dropped;
                break;
            }

            case QEvent::KeyPress:
            {
                /* ESC pressed? */
                if (static_cast<QKeyEvent*>(pEvent)->key() == Qt::Key_Escape)
                {
                    LogFlowFunc(("ESC pressed, cancelling drag'n drop operation\n"));
                    m_enmState = Canceled;
                }
                break;
            }

            default:
                break;
        }
    }

    /* Propagate the event further. */
    return false;
}

void UIDnDMimeData::sltInstallEventFilter(void)
{
    LogFlowFunc(("Installing event filter ...\n"));
    AssertPtr(qApp);
    qApp->installEventFilter(this);
}
#endif /* RT_OS_WINDOWS */

int UIDnDMimeData::setData(const QString &mimeType)
{
    LogFlowFunc(("mimeType=%s, dataType=%s\n",
                 mimeType.toAscii().constData(), m_data.typeName()));

    int rc = VINF_SUCCESS;

    switch (m_data.type())
    {
        case QVariant::String: /* Plain text. */
        {
            QMimeData::setText(m_data.toString());
            break;
        }

        case QVariant::ByteArray: /* Raw byte data. */
        {
            QMimeData::setData(mimeType, m_data.toByteArray());
            break;
        }

        case QVariant::StringList: /* URI. */
        {
            QList<QVariant> lstData = m_data.toList();
            QList<QUrl> lstURL;
            for (int i = 0; i < lstData.size(); i++)
            {
                QString strURL = lstData.at(i).toString();
                LogFlowFunc(("\tURL: %s\n",
                             strURL.toAscii().constData()));
                lstURL << QUrl(strURL.toAscii());
            }
            LogFlowFunc(("Number of URLs: %d\n",  lstURL.size()));

            QMimeData::setUrls(f);
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#include "UIDnDMIMEData.moc"

