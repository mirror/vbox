/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDHandler class declaration..
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

#ifndef ___UIDnDHandler_h___
#define ___UIDnDHandler_h___

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QMimeData;
class CDnDSource;
class CDnDTarget;
class CGuest;
class CSession;
class UIDnDMimeData;

class UIDnDHandler: public QObject
{
    Q_OBJECT;

public:

    /* Singleton factory. */
    static UIDnDHandler *instance(void)
    {
        if (!m_pInstance)
            m_pInstance = new UIDnDHandler();
        return m_pInstance;
    }

    static void destroy(void)
    {
        if (m_pInstance)
        {
            delete m_pInstance;
            m_pInstance = NULL;
        }
    }

    /**
     * Current operation mode.
     */
    enum Direction
    {
        /** Unknown mode. */
        Unknown = 0,
        /** Host to guest. */
        HostToGuest,
        /** Guest to host. */
        GuestToHost
        /** @todo Implement guest to guest. */
    };

    /* Frontend -> Target. */
    Qt::DropAction             dragEnter(CDnDTarget &dndTarget, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget *pParent = NULL);
    Qt::DropAction             dragMove (CDnDTarget &dndTarget, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget *pParent = NULL);
    Qt::DropAction             dragDrop (CSession &session, CDnDTarget &dndTarget, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget *pParent = NULL);
    void                       dragLeave(CDnDTarget &dndTarget, ulong screenId, QWidget *pParent = NULL);

    /* Source -> Frontend. */
    int                        dragIsPending(CSession &session, CDnDSource &dndSource, ulong screenId, QWidget *pParent = NULL);

public:

    static KDnDAction          toVBoxDnDAction(Qt::DropAction action);
    static QVector<KDnDAction> toVBoxDnDActions(Qt::DropActions actions);
    static Qt::DropAction      toQtDnDAction(KDnDAction action);
    static Qt::DropActions     toQtDnDActions(const QVector<KDnDAction> &vecActions);

private:

    UIDnDHandler(void);
    virtual ~UIDnDHandler(void) {}

private:

    /** Static pointer to singleton instance. */
    static UIDnDHandler *m_pInstance;
};

/** Gets the singleton instance of the drag'n drop UI helper class. */
#define DnDHandler() UIDnDHandler::instance()

#endif /* ___UIDnDHandler_h___ */

