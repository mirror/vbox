/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationObject class declaration.
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

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationObject_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationObject_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QObject-based notification-object. */
class SHARED_LIBRARY_STUFF UINotificationObject : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies model about closing. */
    void sigAboutToClose();

public:

    /** Constructs notification-object. */
    UINotificationObject();

    /** Returns object name. */
    virtual QString name() const = 0;
    /** Returns object details. */
    virtual QString details() const = 0;
    /** Handles notification-object being added. */
    virtual void handle() = 0;

public slots:

    /** Notifies model about closing. */
    virtual void close();
};

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationObject_h */
