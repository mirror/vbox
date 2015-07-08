/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDMIMEData class declaration.
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

#ifndef ___UIDnDMIMEData_h___
#define ___UIDnDMIMEData_h___

/* Qt includes: */
#include <QMimeData>

/* COM includes: */
#include "COMEnums.h"
#include "CConsole.h"
#include "CDnDSource.h"
#include "CGuest.h"
#include "CSession.h"

#include "UIDnDHandler.h"

/** @todo Subclass QWindowsMime / QMacPasteboardMime
 *  to register own/more MIME types. */

/**
 * Own implementation of QMimeData for starting and
 * handling all guest-to-host transfers.
 */
class UIDnDMIMEData: public QMimeData
{
    Q_OBJECT;

    enum State
    {
        /** Host is in dragging state, without
         *  having retrieved the metadata from the guest yet. */
        Dragging = 0,
        /** There has been a "dropped" action which indicates
         *  that the guest can continue sending more data (if any)
         *  over to the host, based on the (MIME) metadata. */
        Dropped,
        /** The operation has been canceled. */
        Canceled,
        /** An error occurred. */
        Error,
        /** The usual 32-bit type blow up. */
        State_32BIT_Hack = 0x7fffffff
    };

public:

    UIDnDMIMEData(UIDnDHandler *pDnDHandler, QStringList formats, Qt::DropAction defAction, Qt::DropActions actions);

signals:

     int getData(const QString &strMIMEType, QVariant::Type vaType, QVariant &vaData) const;

#ifdef RT_OS_DARWIN
     void notifyDropped(void) const;
#endif

public slots:

    /**
     * Slot indicating that the current drop target has been changed. 
     * @note Does not work on OS X. 
     */
    void sltDropActionChanged(Qt::DropAction dropAction);

#ifdef RT_OS_DARWIN
    /**
     * Slot indicating that the host wants us to drop the 
     * data from the guest to the host. 
     */
    void sltDropped(void);
#endif

protected:
    /** @name Overridden functions of QMimeData.
     * @{ */
    virtual QStringList formats(void) const;

    virtual bool hasFormat(const QString &mimeType) const;

    virtual QVariant retrieveData(const QString &strMIMEType, QVariant::Type vaType) const;
    /** @}  */

protected:

    /** @name Internal helper functions.
     * @{ */

    /**
     * Sets the object's MIME data according to the given
     * MIME type and data.
     *
     * @returns IPRT status code.
     * @param   strMIMEType             MIME type to set.
     * @param   vaData                  Data to set.
     * @remark
     */
    int setData(const QString &strMIMEType, const QVariant &vaData);
    /** @}  */

protected:

    UIDnDHandler     *m_pDnDHandler;

    QStringList       m_lstFormats;
    /** Default action on successful drop operation. */
    Qt::DropAction    m_defAction;
    /** Current action, based on QDrag's status. */
    Qt::DropAction    m_curAction;
    Qt::DropActions   m_actions;

    mutable State     m_enmState;
    mutable QVariant  m_vaData;

#ifdef RT_OS_DARWIN
    /** Flag indicating whether we can drop data from the
     *  guest to the host or not. */
    bool              m_fCanDrop;
#endif
};

#endif /* ___UIDnDMIMEData_h___ */

