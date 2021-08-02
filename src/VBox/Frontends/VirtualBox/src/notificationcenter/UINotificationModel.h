/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationModel class declaration.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationModel_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationModel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QList>
#include <QMap>
#include <QObject>
#include <QUuid>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class UINotificationObject;

/** QObject-based notification-center model. */
class SHARED_LIBRARY_STUFF UINotificationModel : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about model has changed. */
    void sigChanged();

public:

    /** Constructs notification-center model passing @a pParent to the base-class. */
    UINotificationModel(QObject *pParent);
    /** Destructs notification-center model. */
    virtual ~UINotificationModel() /* override final */;

    /** Appens a notification @a pObject to internal storage. */
    QUuid appendObject(UINotificationObject *pObject);
    /** Revokes a notification object referenced by @a uId from intenal storage. */
    void revokeObject(const QUuid &uId);

    /** Returns a list of registered notification object IDs. */
    QList<QUuid> ids() const;
    /** Returns a notification object referenced by specified @a uId. */
    UINotificationObject *objectById(const QUuid &uId);

private slots:

    /** Handles request about to close sender() notification object. */
    void sltHandleAboutToClose();

    /** Handles broadcast request to detach COM stuff. */
    void sltDetachCOM();

private:

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Holds the list of registered notification object IDs. */
    QList<QUuid>                        m_ids;
    /** Holds the map of notification objects registered by ID. */
    QMap<QUuid, UINotificationObject*>  m_objects;
};

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationModel_h */
