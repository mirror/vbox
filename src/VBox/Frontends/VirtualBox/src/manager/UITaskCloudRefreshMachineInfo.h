/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudRefreshMachineInfo class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UITaskCloudRefreshMachineInfo_h
#define FEQT_INCLUDED_SRC_manager_UITaskCloudRefreshMachineInfo_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMap>
#include <QMutex>

/* GUI includes: */
#include "UITask.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudMachine.h"


/** UITask extension used to get cloud instance state. */
class UITaskCloudRefreshMachineInfo : public UITask
{
    Q_OBJECT;

public:

    /** Constructs task taking @a comCloudMachine as data.
      * @param  comCloudMachine  Brings the cloud VM wrapper. */
    UITaskCloudRefreshMachineInfo(const CCloudMachine &comCloudMachine);

    /** Returns error info. */
    QString errorInfo();

protected:

    /** Contains the task body. */
    virtual void run() /* override */;

private:

    /** Holds the mutex to access m_comCloudMachine member. */
    mutable QMutex  m_mutex;

    /** Holds the cloud machine object. */
    CCloudMachine  m_comCloudMachine;

    /** Holds the error info. */
    QString  m_strErrorInfo;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UITaskCloudRefreshMachineInfo_h */
