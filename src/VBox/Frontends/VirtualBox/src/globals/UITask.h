/* $Id$ */
/** @file
 * VBox Qt GUI - UITask class declaration.
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UITask_h
#define FEQT_INCLUDED_SRC_globals_UITask_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QObject extension used as worker-thread task interface.
  * Describes task to be handled by the UIThreadPool object. */
class SHARED_LIBRARY_STUFF UITask : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a pTask complete. */
    void sigComplete(UITask *pTask);

public:

    /** Task types. */
    enum Type
    {
        Type_MediumEnumeration     = 1,
        Type_DetailsPopulation     = 2,
        Type_CloudAcquireInstances = 3,
    };

    /** Constructs the task of passed @a enmType. */
    UITask(UITask::Type enmType) : m_enmType(enmType) {}

    /** Returns the type of the task. */
    UITask::Type type() const { return m_enmType; }

    /** Starts the task. */
    void start();

protected:

    /** Contains the abstract task body. */
    virtual void run() = 0;

private:

    /** Holds the type of the task. */
    const UITask::Type  m_enmType;
};

#endif /* !FEQT_INCLUDED_SRC_globals_UITask_h */
