/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudGetInstanceInfo class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UITaskCloudGetInstanceInfo_h
#define FEQT_INCLUDED_SRC_manager_UITaskCloudGetInstanceInfo_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMap>
#include <QMutex>

/* GUI includes: */
#include "UICloudMachine.h"
#include "UITask.h"

/* COM includes: */
#include "CVirtualBoxErrorInfo.h"


/** UITask extension used to get cloud instance state. */
class UITaskCloudGetInstanceInfo : public UITask
{
    Q_OBJECT;

public:

    /** Constructs task taking @a guiCloudMachine as data.
      * @param  guiCloudMachine  Brings the cloud VM wrapper. */
    UITaskCloudGetInstanceInfo(const UICloudMachine &guiCloudMachine);

    /** Returns error info. */
    CVirtualBoxErrorInfo errorInfo();

protected:

    /** Contains the task body. */
    virtual void run() /* override */;

private:

    /** Holds the mutex to access m_guiCloudMachine member. */
    mutable QMutex  m_mutex;

    /** Holds the cloud client object. */
    UICloudMachine  m_guiCloudMachine;

    /** Holds the error info object. */
    CVirtualBoxErrorInfo  m_comErrorInfo;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UITaskCloudGetInstanceInfo_h */
