/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDMIMEData class implementation.
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
#include <QFileInfo>
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
#include "UIDnDDrag.h"
#include "UIMessageCenter.h"

UIDnDMimeData::UIDnDMimeData(CSession &session,
                             CDnDSource &dndSource,
                             QStringList formats,
                             Qt::DropAction defAction, Qt::DropActions actions,
                             QWidget *pParent)
    : m_Session(session)
    , m_DnDSource(dndSource)
    , m_lstFormats(formats)
    , m_defAction(defAction)
    , m_actions(actions)
    , m_pParent(pParent)
    , m_enmState(Dragging)
    , m_vaData(QVariant::Invalid)
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

bool UIDnDMimeData::hasFormat(const QString &strMIMEType) const
{
    bool fRc = m_lstFormats.contains(strMIMEType);
    LogFlowFunc(("%s: %RTbool (QtMimeData: %RTbool)\n",
                 strMIMEType.toStdString().c_str(),
                 fRc, QMimeData::hasFormat(strMIMEType)));
    return fRc;
}

QVariant UIDnDMimeData::retrieveData(const QString &strMIMEType,
                                     QVariant::Type vaType)
{
    LogFlowFunc(("m_enmState=%d, mimeType=%s, type=%d (%s)\n",
                 m_enmState, strMIMEType.toStdString().c_str(),
                 vaType, QVariant::typeToName(vaType)));

    bool fCanDrop = true;

#ifdef RT_OS_WINDOWS
    /* On Windows this function will be called several times by Qt's
     * OLE-specific internals to figure out which data formats we have
     * to offer. So just assume we can drop data here for a start.* */
    fCanDrop = true;
#else
    /* On non-Windows our state gets updated via an own event filter
     * (see UIDnDMimeData::eventFilter). This filter will update the current
     * operation state for us (based on the mouse buttons). */
    if (m_enmState != Dropped)
        fCanDrop = false;
#endif

    /* Do we support the requested MIME type? */
    if (   fCanDrop
        && !m_lstFormats.contains(strMIMEType))
    {
        LogFlowFunc(("Unsupported MIME type=%s\n",
                     strMIMEType.toStdString().c_str()));
        fCanDrop = false;
    }

    /* Supported types. See below in the switch statement. */
    if (   fCanDrop
        && !(
             /* Plain text. */
                vaType == QVariant::String
             /* Binary data. */
             || vaType == QVariant::ByteArray
             /* URI list. */
             || vaType == QVariant::List))
    {
        LogFlowFunc(("Unsupported data type=%d (%s)\n",
                     vaType, QVariant::typeToName(vaType)));
        fCanDrop = false;
    }

    if (!fCanDrop)
    {
        LogFlowFunc(("Skipping request, m_enmState=%d ...\n",
                     m_enmState));
        return QMimeData::retrieveData(strMIMEType, vaType);
    }

    int rc = VINF_SUCCESS;
    if (m_enmState == Dropped)
    {
        rc = UIDnDDrag::RetrieveData(m_Session,
                                     m_DnDSource,
                                     m_defAction,
                                     strMIMEType, vaType, m_vaData,
                                     m_pParent);
        if (RT_SUCCESS(rc))
        {
            /* Tell ourselves that data became available. */
            emit sigDataAvailable(strMIMEType);
        }
        else
        {
            m_enmState = Canceled;
        }
    }

    LogFlowFunc(("Returning rc=%Rrc, m_enmState=%ld\n",
                 rc, m_enmState));
    return m_vaData;
}

#ifndef RT_OS_WINDOWS
bool UIDnDMimeData::eventFilter(QObject *pObject, QEvent *pEvent)
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
                LogFlowFunc(("MouseMove: x=%d, y=%d\n",
                             pMouseEvent->globalX(), pMouseEvent->globalY()));

                return true;
                /* Never reached. */
            }
#endif
            case QEvent::MouseButtonRelease:
            {
                LogFlowFunc(("MouseButtonRelease\n"));
                m_enmState = Dropped;

                return true;
                /* Never reached. */
            }

            case QEvent::KeyPress:
            {
                /* ESC pressed? */
                if (static_cast<QKeyEvent*>(pEvent)->key() == Qt::Key_Escape)
                {
                    LogFlowFunc(("ESC pressed, cancelling drag'n drop operation\n"));
                    m_enmState = Canceled;
                }

                return true;
                /* Never reached. */
            }

            default:
                break;
        }
    }

    return QObject::eventFilter(pObject, pEvent);
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
                 mimeType.toAscii().constData(), m_vaData.typeName()));

    int rc = VINF_SUCCESS;

    switch (m_vaData.type())
    {
        case QVariant::String: /* Plain text. */
        {
            QMimeData::setText(m_vaData.toString());
            break;
        }

        case QVariant::ByteArray: /* Raw byte data. */
        {
            QMimeData::setData(mimeType, m_vaData.toByteArray());
            break;
        }

        case QVariant::StringList: /* URI. */
        {
            QList<QVariant> lstData = m_vaData.toList();
            QList<QUrl> lstURL;
            for (int i = 0; i < lstData.size(); i++)
            {
                QFileInfo fileInfo(lstData.at(i).toString());
#ifdef DEBUG
                LogFlowFunc(("\tURL: %s (fExists=%RTbool, fIsDir=%RTbool, cb=%RU64)\n",
                             fileInfo.absoluteFilePath().constData(), fileInfo.exists(),
                             fileInfo.isDir(), fileInfo.size()));
#endif
                lstURL << QUrl::fromLocalFile(fileInfo.absoluteFilePath());
            }
            LogFlowFunc(("Number of URLs: %d\n",  lstURL.size()));

            if (RT_SUCCESS(rc))
                QMimeData::setUrls(lstURL);
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

