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

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#include "UIDnDDrag.moc"

