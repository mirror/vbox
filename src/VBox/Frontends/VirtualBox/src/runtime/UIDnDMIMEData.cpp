/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDMIMEData class implementation.
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
# include <QFileInfo>
# include <QMimeData>
# include <QStringList>
# include <QUrl>

/* GUI includes: */
# include "UIDnDMIMEData.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>


UIDnDMIMEData::UIDnDMIMEData(UIDnDHandler *pDnDHandler,
                             QStringList lstFormats, Qt::DropAction defAction, Qt::DropActions actions)
    : m_pDnDHandler(pDnDHandler)
    , m_lstFormats(lstFormats)
    , m_defAction(defAction)
    , m_curAction(Qt::IgnoreAction)
    , m_actions(actions)
    , m_enmState(Dragging)
    , m_vaData(QVariant::Invalid)
#ifdef RT_OS_DARWIN
    , m_fCanDrop(false)
#endif
{
    LogFlowThisFuncEnter();

#ifdef DEBUG
    LogFlowFunc(("Number of formats: %d\n", m_lstFormats.size()));
    for (int i = 0; i < m_lstFormats.size(); i++)
        LogFlowFunc(("\tFormat %d: %s\n", i, m_lstFormats.at(i).toAscii().constData()));
#endif

#ifdef RT_OS_DARWIN
    connect(this, SIGNAL(notifyDropped()), this, SLOT(sltDropped()));
#endif
}

QStringList UIDnDMIMEData::formats(void) const
{
    LogFlowFuncEnter();
    return m_lstFormats;
}

bool UIDnDMIMEData::hasFormat(const QString &strMIMEType) const
{
    bool fRc = false;

#ifdef RT_OS_DARWIN
    if (strMIMEType.compare("application/x-qt-mime-type-name", Qt::CaseInsensitive) == 0)
        fRc = true;
#endif

    if (!fRc)
        fRc = m_curAction != Qt::IgnoreAction; 

    LogFlowFunc(("%s: %RTbool (QtMimeData: %RTbool, curAction=0x%x)\n",
                 strMIMEType.toStdString().c_str(), fRc, QMimeData::hasFormat(strMIMEType), m_curAction));

#ifdef RT_OS_DARWIN
    /*
     * On OS X hasFormat() only seems to get called on a successful 
     * drop, that is, if the host knows (and the user accepts) the drop operation. 
     *  
     * As we can't do here much since this is a const'ed function we're emitting a 
     * signal to ourselves in order to let us know that we can start receiving data 
     * from the guest within the next retrieveData() call. 
     */
    emit notifyDropped();
#endif

    return fRc;
}

#ifdef RT_OS_DARWIN
void UIDnDMIMEData::sltDropped(void)
{
    LogFlowFuncEnter();
    m_fCanDrop = true;
}
#endif

/**
 * Called by Qt's drag'n drop operation (QDrag) for retrieving the actual drag'n drop
 * data in case of a successful drag'n drop operation.
 *
 * @param strMIMEType           MIME type string.
 * @param vaType                Variant containing the actual data based on the the MIME type.
 *
 * @return QVariant
 */
QVariant UIDnDMIMEData::retrieveData(const QString &strMIMEType, QVariant::Type vaType) const
{
    LogFlowFunc(("state=%RU32, curAction=0x%x, defAction=0x%x, mimeType=%s, type=%d (%s)\n",
                 m_enmState, m_curAction, m_defAction, strMIMEType.toStdString().c_str(), vaType, QVariant::typeToName(vaType)));

    bool fCanDrop = true; /* Accept by default. */

#ifdef RT_OS_WINDOWS
    /* 
     * On Windows this function will be called several times by Qt's
     * OLE-specific internals to figure out which data formats we have
     * to offer. So just assume we can drop data here for a start.
     */ 
#elif defined(RT_OS_DARWIN)
    /*
     * On OS X we rely on an internal flag which gets set in the overriden 
     * hasFormat() function. That function only gets called on a successful drop 
     * so that we can tell if we have to start receiving data the next time 
     * we come by here. 
     */
    fCanDrop = m_fCanDrop;
#else
    /* 
     * On non-Windows our state gets updated via an own event filter
     * (see UIDnDMimeData::eventFilter). This filter will update the current
     * operation state for us (based on the mouse buttons). 
     */ 
    if (m_curAction == Qt::IgnoreAction)
    {
        LogFlowFunc(("Current drop action is 0x%x, so can't drop yet\n", m_curAction));
        fCanDrop = false;
    }
#endif

    if (fCanDrop)
    {
        /* Do we support the requested MIME type? */
        if (!m_lstFormats.contains(strMIMEType))
        {
            LogRel(("DnD: Unsupported MIME type '%s'\n", strMIMEType.toStdString().c_str()));
            fCanDrop = false;
        }

        /* Supported types. See below in the switch statement. */
        if (!(
              /* Plain text. */
                 vaType == QVariant::String
              /* Binary data. */
              || vaType == QVariant::ByteArray
                 /* URI list. */
              || vaType == QVariant::List))
        {
            LogRel(("DnD: Unsupported data type '%s'\n", QVariant::typeToName(vaType)));
            fCanDrop = false;
        }
    }

    LogRel3(("DnD: State=%ld, Action=0x%x, fCanDrop=%RTbool\n", m_enmState, m_curAction, fCanDrop));

    if (fCanDrop)
    {
        QVariant vaData;
        int rc = emit getData(strMIMEType, vaType, vaData);

        LogRel3(("DnD: Returning data of type=%s (requested MIME type=%s, requested type=%s), rc=%Rrc\n",
                 vaData.typeName() ? vaData.typeName() : "<Invalid>",
                 strMIMEType.toStdString().c_str(),
                 QVariant::typeToName(vaType) ? QVariant::typeToName(vaType) : "<Invalid>", rc));

        if (RT_SUCCESS(rc))
            return vaData;
    }

#ifdef RT_OS_DARWIN
    LogFlowFunc(("Returning data for Cocoa\n"));
    return QMimeData::retrieveData(strMIMEType, vaType);
#else
    LogFlowFunc(("Skipping request, state=%RU32 ...\n", m_enmState));
    return QVariant(QVariant::Invalid); /* Return a NULL variant. */
#endif
}

int UIDnDMIMEData::setData(const QString &strMIMEType, const QVariant &vaData)
{
    LogFlowFunc(("mimeType=%s, dataType=%s\n",
                 strMIMEType.toAscii().constData(), vaData.typeName()));

    int rc = VINF_SUCCESS;

    switch (m_vaData.type())
    {
        case QVariant::String: /* Plain text. */
        {
            QMimeData::setText(vaData.toString());
            break;
        }

        case QVariant::ByteArray: /* Raw byte data. */
        {
            QMimeData::setData(strMIMEType, vaData.toByteArray());
            break;
        }

        case QVariant::StringList: /* URI. */
        {
            QList<QVariant> lstData = vaData.toList();
            QList<QUrl> lstURL;
            for (int i = 0; i < lstData.size(); i++)
            {
                QFileInfo fileInfo(lstData.at(i).toString());

                LogFlowFunc(("\tURL: %s (fExists=%RTbool, fIsDir=%RTbool, cb=%RU64)\n",
                             fileInfo.absoluteFilePath().constData(), fileInfo.exists(),
                             fileInfo.isDir(), fileInfo.size()));

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

/**
 * Issued by the QDrag object as soon as the current drop action has changed.
 *
 * @param dropAction            New drop action to use.
 */
void UIDnDMIMEData::sltDropActionChanged(Qt::DropAction dropAction)
{
    LogFlowFunc(("dropAction=0x%x\n", dropAction));
    m_curAction = dropAction;
}
