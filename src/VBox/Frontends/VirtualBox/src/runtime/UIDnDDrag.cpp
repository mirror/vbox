/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDDrag class implementation. This class acts as a wrapper
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QApplication>
# include <QFileInfo>
# include <QKeyEvent>
# include <QMimeData>
# include <QStringList>
# include <QTimer>
# include <QUrl>

/* GUI includes: */
# include "UIDnDDrag.h"
# include "UIDnDHandler.h"
# include "UIDnDMIMEData.h"
# include "UIMessageCenter.h"
# ifdef RT_OS_WINDOWS
#  include "UIDnDDropSource_win.h"
#  include "UIDnDDataObject_win.h"
# endif

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>


UIDnDDrag::UIDnDDrag(CSession &session,
                     CDnDSource &dndSource,
                     const QStringList &lstFormats,
                     Qt::DropAction defAction, Qt::DropActions actions,
                     QWidget *pParent /* = NULL */)
    : m_session(session)
    , m_dndSource(dndSource)
    , m_lstFormats(lstFormats)
    , m_defAction(defAction)
    , m_actions(actions)
    , m_pParent(pParent)
#ifndef RT_OS_WINDOWS
    , pMData(NULL)
#endif
{
    LogFlowFunc(("m_defAction=0x%x\n", m_defAction));
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
#ifdef RT_OS_WINDOWS
    UIDnDDropSource *pDropSource = new UIDnDDropSource(m_pParent);
    UIDnDDataObject *pDataObject = new UIDnDDataObject(m_session, m_dndSource, m_lstFormats, m_pParent);

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
    LogFlowFunc(("dwOKEffects=0x%x\n", dwOKEffects));
    HRESULT hr = ::DoDragDrop(pDataObject, pDropSource,
                              dwOKEffects, &dwEffect);
    LogFlowFunc(("DoDragDrop ended with hr=%Rhrc, dwEffect=%RI32\n",
                 hr, dwEffect));

    if (pDropSource)
        pDropSource->Release();
    if (pDataObject)
        pDataObject->Release();
#else
    QDrag *pDrag = new QDrag(m_pParent);

    /* pMData is transfered to the QDrag object, so no need for deletion. */
    pMData = new UIDnDMimeData(m_session, m_dndSource,
                               m_lstFormats, m_defAction, m_actions,
                               m_pParent);

    /* Inform the MIME data object of any changes in the current action. */
    connect(pDrag, SIGNAL(actionChanged(Qt::DropAction)),
            pMData, SLOT(sltDropActionChanged(Qt::DropAction)));

    /* Fire it up.
     *
     * On Windows this will start a modal operation using OLE's
     * DoDragDrop() method, so this call will block until the DnD operation
     * is finished. */
    pDrag->setMimeData(pMData);
#ifdef LOG_ENABLED
    Qt::DropAction dropAction = pDrag->exec(m_actions, m_defAction);
    LogFlowFunc(("dropAction=%ld\n", UIDnDHandler::toVBoxDnDAction(dropAction)));
#endif

     /* Note: The UIDnDMimeData object will not be not accessible here anymore,
     *        since QDrag had its ownership and deleted it after the (blocking)
     *        QDrag::exec() call. */
#endif /* !RT_OS_WINDOWS */

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/* static */
int UIDnDDrag::RetrieveData(const CSession &session,
                            CDnDSource &dndSource,
                            Qt::DropAction dropAction,
                            const QString &strMimeType,
                            QVariant::Type vaType, QVariant &vaData,
                            QWidget *pParent)
{
    LogFlowFunc(("Retrieving data as type=%s (variant type=%ld)\n",
                 strMimeType.toAscii().constData(), vaType));

    int rc = VINF_SUCCESS;
    CGuest guest = session.GetConsole().GetGuest();

    /* Start getting the data from the source. First inform the source we
     * want the data in the specified MIME type. */
    CProgress progress = dndSource.Drop(strMimeType,
                                        UIDnDHandler::toVBoxDnDAction(dropAction));
    if (guest.isOk())
    {
        msgCenter().showModalProgressDialog(progress,
                                            tr("Retrieving data ..."), ":/progress_dnd_gh_90px.png",
                                            pParent);
        if (!progress.GetCanceled())
        {
            rc =   (   progress.isOk()
                    && progress.GetResultCode() == 0)
                 ? VINF_SUCCESS : VERR_GENERAL_FAILURE; /** @todo Fudge; do a GetResultCode() to rc translation. */

            if (RT_SUCCESS(rc))
            {
                /* After we successfully retrieved data from
                * the source, we query it from Main. */
                QVector<uint8_t> vecData = dndSource.ReceiveData();
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
                msgCenter().cannotDropData(progress, pParent);
        }
        else
            rc = VERR_CANCELLED;
    }
    else
    {
        msgCenter().cannotDropData(guest, pParent);
        rc = VERR_GENERAL_FAILURE; /** @todo Fudge; do a GetResultCode() to rc translation. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

