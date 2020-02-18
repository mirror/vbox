/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudGetInstanceState class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UITaskCloudGetInstanceState_h
#define FEQT_INCLUDED_SRC_manager_UITaskCloudGetInstanceState_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMutex>

/* GUI includes: */
#include "UITask.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"
#include "CVirtualBoxErrorInfo.h"


/** UITask extension used to get cloud instance state. */
class UITaskCloudGetInstanceState : public UITask
{
    Q_OBJECT;

public:

    /** Constructs task taking @a comCloudClient and @a strId as data.
      * @param  comCloudClient  Brings the cloud client object.
      * @param  strId           Brings the cloud VM id. */
    UITaskCloudGetInstanceState(const CCloudClient &comCloudClient, const QString &strId);

    /** Returns error info. */
    CVirtualBoxErrorInfo errorInfo();

    /** Returns the task result. */
    QString result() const;

protected:

    /** Contains the task body. */
    virtual void run() /* override */;

private:

    /** Holds the mutex to access result. */
    mutable QMutex  m_mutex;

    /** Holds the cloud client object. */
    CCloudClient  m_comCloudClient;
    /** Holds the cloud VM id. */
    QString       m_strId;

    /** Holds the error info object. */
    CVirtualBoxErrorInfo  m_comErrorInfo;

    /** Holds the task result. */
    QString  m_strResult;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UITaskCloudGetInstanceState_h */
