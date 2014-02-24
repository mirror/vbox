/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDnDDrag class implementation. This class acts as a wrapper
 * for OS-dependent guest->host drag'n drop operations.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
#include "UIDnDDrag.h"
#include "UIDnDHandler.h"
#include "UIDnDMIMEData.h"
#include "UIMessageCenter.h"
#ifdef RT_OS_WINDOWS
# include "UIDnDDropSource_win.h"
# include "UIDnDDataObject_win.h"
#endif

UIDnDDrag::UIDnDDrag(CSession &session, const QStringList &lstFormats,
                     Qt::DropAction defAction, Qt::DropActions actions,
                     QWidget *pParent)
    : m_pParent(pParent)
    , m_session(session)
    , m_lstFormats(lstFormats)
    , m_defAction(defAction)
    , m_actions(actions)
#ifndef RT_OS_WINDOWS
    , pMData(NULL)
#endif
{
    LogFlowFunc(("m_defAction=0x%x, m_actions=0x%x\n", m_defAction, m_actions));
    LogFlowFunc(("Number of formats: %d\n", m_lstFormats.size()));
#ifdef DEBUG
    for (int i = 0; i < m_lstFormats.size(); i++)
        LogFlowFunc(("\tFormat %d: %s\n", i, m_lstFormats.at(i).toAscii().constData()));
#endif
}

int UIDnDDrag::DoDragDrop(void)
{
    LogFlowFunc(("pParent=0x%p\n", m_pParent));

    int rc = VINF_SUCCESS;
    try
    {
#ifdef RT_OS_WINDOWS
        UIDnDDropSource *pDropSource = new UIDnDDropSource(this /* pParent */);
        UIDnDDataObject *pDataObject = new UIDnDDataObject(m_lstFormats, this /* pParent */);

        DWORD dwOKEffects = DROPEFFECT_NONE;
        if (m_actions)
        {
            if (m_actions & Qt::CopyAction)
                dwOKEffects |= DROPEFFECT_COPY;
            if (m_actions & Qt::MoveAction)
                dwOKEffects |= DROPEFFECT_MOVE;
            if (m_actions & Qt::LinkAction)
                dwOKEffects |= DROPEFFECT_LINK;
        }

        DWORD dwEffect;
        HRESULT hr = ::DoDragDrop(pDataObject, pDropSource,
                                  dwOKEffects, &dwEffect);
        LogFlowThisFunc(("DoDragDrop ended with hr=%Rhrc, dwEffect=%RI32\n",
                         hr, dwEffect));

        if (pDropSource)
            pDropSource->Release();
        if (pDataObject)
            pDataObject->Release();
#else
        QDrag *pDrag = new QDrag(m_pParent);

        /* pMData is transfered to the QDrag object, so no need for deletion. */
        pMData = new UIDnDMimeData(m_session, m_lstFormats,
                                   m_defAction, m_actions, m_pParent);

        /* Inform this object that MIME data from the guest is available so that
         * it can update the MIME data object accordingly. */
        connect(pMData, SIGNAL(sigDataAvailable(QString)),
                this, SLOT(sltDataAvailable(QString)), Qt::DirectConnection);

        /* Inform the MIME data object of any changes in the current action. */
        connect(pDrag, SIGNAL(actionChanged(Qt::DropAction)),
                pMData, SLOT(sltDropActionChanged(Qt::DropAction)));

        /* Fire it up.
         *
         * On Windows this will start a modal operation using OLE's
         * DoDragDrop() method, so this call will block until the DnD operation
         * is finished. */
        pDrag->setMimeData(pMData);
        Qt::DropAction dropAction = pDrag->exec(m_actions, m_defAction);
        LogFlowFunc(("dropAction=%ld\n", UIDnDHandler::toVBoxDnDAction(dropAction)));

         /* Note: The UIDnDMimeData object will not be not accessible here anymore,
         *        since QDrag had its ownership and deleted it after the (blocking)
         *        QDrag::exec() call. */
#endif /* !RT_OS_WINDOWS */
    }
    catch (std::bad_alloc)
    {
        rc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int UIDnDDrag::RetrieveData(const QString &strMimeType,
                            QVariant::Type vaType, QVariant &vaData)
{
    LogFlowFunc(("Retrieving data as type=%s (variant type=%ld)\n",
                 strMimeType.toAscii().constData(), vaType));

    int rc = VINF_SUCCESS;
    CGuest guest = m_session.GetConsole().GetGuest();

    /* Start getting the data from the guest. First inform the guest we
     * want the data in the specified MIME type. */
    CProgress progress = guest.DragGHDropped(strMimeType,
                                             UIDnDHandler::toVBoxDnDAction(m_defAction));
    if (guest.isOk())
    {
        msgCenter().showModalProgressDialog(progress,
                                            tr("Retrieving data ..."), ":/progress_dnd_gh_90px.png",
                                            m_pParent);
        if (!progress.GetCanceled())
        {
            rc =   (   progress.isOk()
                    && progress.GetResultCode() == 0)
                 ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Fudge; do a GetResultCode() to rc translation. */

            if (RT_SUCCESS(rc))
            {
                /* After the data successfully arrived from the guest, we query it from Main. */
                QVector<uint8_t> vecData = guest.DragGHGetData();
                if (!vecData.isEmpty())
                {
                    switch (vaType)
                    {
                        case QVariant::String:
                        {
                            vaData = QVariant(QString(reinterpret_cast<const char*>(vecData.constData())));
                            break;
                        }

                        case QVariant::ByteArray:
                        {
                            QByteArray ba(reinterpret_cast<const char*>(vecData.constData()), vecData.size());
                            vaData = QVariant(ba);
                            break;
                        }

                        case QVariant::StringList:
                        {
                            QString strData = QString(reinterpret_cast<const char*>(vecData.constData()));
                            QStringList lstString = strData.split("\r\n", QString::SkipEmptyParts);

                            vaData = QVariant(lstString);
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
                msgCenter().cannotDropData(progress, m_pParent);
        }
        else
            rc = VERR_CANCELLED;
    }
    else
    {
        msgCenter().cannotDropData(guest, m_pParent);
        rc = VERR_GENERAL_FAILURE; /** @todo Fudge; do a GetResultCode() to rc translation. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifndef RT_OS_WINDOWS
void UIDnDDrag::sltDataAvailable(const QString &mimeType)
{
    LogFlowFunc(("pMData=0x%p, mimeType=%s\n",
                 pMData, mimeType.toAscii().constData()));

    if (pMData)
        pMData->setData(mimeType);
}
#endif

#include "UIDnDDrag.moc"

