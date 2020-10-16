/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudListMachines class declaration.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UITaskCloudListMachines_h
#define FEQT_INCLUDED_SRC_manager_UITaskCloudListMachines_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMutex>

/* GUI includes: */
#include "UITask.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudMachine.h"

/** UITask extension used to list cloud machines. */
class UITaskCloudListMachines : public UITask
{
    Q_OBJECT;

public:

    /** Constructs task taking @a strProviderShortName and @a strProfileName as data.
      * @param  strProviderShortName  Brings the provider short name.
      * @param  strProfileName        Brings the profile name.
      * @param  fWithRefresh          Brings whether the task includes refresh. */
    UITaskCloudListMachines(const QString &strProviderShortName,
                            const QString &strProfileName,
                            bool fWithRefresh);

    /** Returns provider short name. */
    QString providerShortName() const { return m_strProviderShortName; }
    /** Returns profile name. */
    QString profileName() const { return m_strProfileName; }
    /** Returns whether the task includes refresh. */
    bool withRefresh() const { return m_fWithRefresh; }

    /** Returns error info. */
    QString errorInfo() const;

    /** Returns the task result. */
    QVector<CCloudMachine> result() const;

protected:

    /** Contains the task body. */
    virtual void run() /* override */;

private:

    /** Holds the mutex to access result. */
    mutable QMutex  m_mutex;

    /** Holds the provider short name. */
    const QString  m_strProviderShortName;
    /** Holds the profile name. */
    const QString  m_strProfileName;
    /** Holds whether the task includes refresh. */
    const bool     m_fWithRefresh;

    /** Holds the error info. */
    QString  m_strErrorInfo;

    /** Holds the task result. */
    QVector<CCloudMachine>  m_result;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UITaskCloudListMachines_h */
