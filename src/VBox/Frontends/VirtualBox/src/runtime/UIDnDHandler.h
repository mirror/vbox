/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDHandler class declaration..
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

#ifndef ___UIDnDHandler_h___
#define ___UIDnDHandler_h___

/* Qt includes: */
#include <QMimeData>
#include <QMutex>
#include <QStringList>

/* COM includes: */
#include "COMEnums.h"
#include "CDnDTarget.h"
#include "CDnDSource.h"

/* Forward declarations: */
class QMimeData;

class UIDnDMIMEData;
class UISession;

class UIDnDHandler: public QObject
{
    Q_OBJECT;

public:

    UIDnDHandler(UISession *pSession, QWidget *pParent);
    virtual ~UIDnDHandler(void);

    /**
     * Current operation mode. 
     * Note: The operation mode is independent of the machine's overall 
     *       drag and drop mode. 
     */
    typedef enum DNDMODE
    {
        /** Unknown mode. */
        DNDMODE_UNKNOWN     = 0,
        /** Host to guest. */
        DNDMODE_HOSTTOGUEST = 1,
        /** Guest to host. */
        DNDMODE_GUESTTOHOST = 2,
        /** @todo Implement guest to guest. */
        /** The usual 32-bit type blow up. */
        DNDMODE_32BIT_HACK = 0x7fffffff
    } DNDMODE;

    /* Frontend -> Target. */
    Qt::DropAction             dragEnter(ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData);
    Qt::DropAction             dragMove (ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData);
    Qt::DropAction             dragDrop (ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData);
    void                       dragLeave(ulong screenId);

    /* Source -> Frontend. */
    int                        dragIsPending(ulong screenId);
    int                        dragStart(const QStringList &lstFormats, Qt::DropAction defAction, Qt::DropActions actions);
    int                        retrieveData(Qt::DropAction  dropAction, const QString &strMimeType, QVariant::Type vaType, QVariant &vaData);

public:

    static KDnDAction          toVBoxDnDAction(Qt::DropAction action);
    static QVector<KDnDAction> toVBoxDnDActions(Qt::DropActions actions);
    static Qt::DropAction      toQtDnDAction(KDnDAction action);
    static Qt::DropActions     toQtDnDActions(const QVector<KDnDAction> &vecActions);

protected:

    /** Pointer to UI session. */
    UISession        *m_pSession;
    /** Pointer to parent widget. */
    QWidget          *m_pParent;

    /** Drag and drop source instance. */
    CDnDSource        m_dndSource;
    /** Drag and drop target instance. */
    CDnDTarget        m_dndTarget;
    /** Current transfer direction. */
    DNDMODE           m_enmMode;
    /** Flag indicating if a drag operation is pending currently. */
    bool              m_fIsPending;
    QMutex            m_ReadLock;
    QMutex            m_WriteLock;

    /** List of formats supported by the source. */
    QStringList       m_lstFormats;
    /** Default drop action from the source. */
    Qt::DropAction    m_defAction;
    /** List of allowed drop actions from the source. */
    Qt::DropActions   m_actions;

#ifndef RT_OS_WINDOWS
    /** Pointer to MIMEData instance used for handling
     *  own MIME times on non-Windows host OSes. */
    UIDnDMIMEData    *m_pMIMEData;
    friend class UIDnDMIMEData;
#endif
};
#endif /* ___UIDnDHandler_h___ */

