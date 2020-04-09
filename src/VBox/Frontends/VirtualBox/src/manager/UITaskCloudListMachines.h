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
#include "CCloudClient.h"
#include "CCloudMachine.h"

/* Forward declaratiuons: */
class UIChooserNode;

/** UITask extension used to list cloud machines. */
class UITaskCloudListMachines : public UITask
{
    Q_OBJECT;

public:

    /** Constructs update task taking @a comCloudClient and @a pParentNode as data.
      * @param  comCloudClient  Brings the cloud client object.
      * @param  m_pParentNode   Brings the parent node reference. */
    UITaskCloudListMachines(const CCloudClient &comCloudClient, UIChooserNode *pParentNode);

    /** Returns cloud client object. */
    CCloudClient cloudClient() const { return m_comCloudClient; }
    /** Returns parent node reference. */
    UIChooserNode *parentNode() const { return m_pParentNode; }

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

    /** Holds the cloud client object. */
    CCloudClient   m_comCloudClient;
    /** Holds the parent node reference. */
    UIChooserNode *m_pParentNode;

    /** Holds the error info. */
    QString  m_strErrorInfo;

    /** Holds the task result. */
    QVector<CCloudMachine>  m_result;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UITaskCloudListMachines_h */
