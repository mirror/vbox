/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDnDHandler class declaration
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

/* Forward declarations. */
class QMimeData;
class CSession;
class CGuest;

/** @todo Check for making this a full static class when possible. */
class UIDnDHandler: public QObject
{
public:
    /* Singleton factory. */
    static UIDnDHandler* instance(void)
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

    /* Host -> Guest. */
    Qt::DropAction dragHGEnter(CGuest &guest, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget *pParent = NULL);
    Qt::DropAction dragHGMove (CGuest &guest, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget *pParent = NULL);
    Qt::DropAction dragHGDrop (CGuest &guest, ulong screenId, int x, int y, Qt::DropAction proposedAction, Qt::DropActions possibleActions, const QMimeData *pMimeData, QWidget *pParent = NULL);
    void           dragHGLeave(CGuest &guest, ulong screenId, QWidget *pParent = NULL);

    /* Guest -> Host. */
    int            dragGHPending(CSession &session, ulong screenId, QWidget *pParent = NULL);

private:
    static UIDnDHandler *m_pInstance;

    UIDnDHandler(void);
    virtual ~UIDnDHandler(void) {}

    /* Private helpers. */
    static KDragAndDropAction          toVBoxDnDAction(Qt::DropAction action);
    static QVector<KDragAndDropAction> toVBoxDnDActions(Qt::DropActions actions);
    static Qt::DropAction              toQtDnDAction(KDragAndDropAction action);
    static Qt::DropActions             toQtDnDActions(const QVector<KDragAndDropAction> &vecActions);

    friend class UIDnDMimeData;
};

#define gDnD UIDnDHandler::instance()

#endif /* ___UIDnDHandler_h___ */

