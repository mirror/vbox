/* $Id$ */
/** @file
 * VBox Qt GUI - UITaskCloudGetSettingsForm class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UITaskCloudGetSettingsForm_h
#define FEQT_INCLUDED_SRC_manager_UITaskCloudGetSettingsForm_h
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
#include "CForm.h"

/** UITask extension used to get cloud machine settings form. */
class UITaskCloudGetSettingsForm : public UITask
{
    Q_OBJECT;

public:

    /** Constructs update task taking @a comCloudMachine as data.
      * @param  comCloudMachine  Brings the cloud machine object. */
    UITaskCloudGetSettingsForm(const CCloudMachine &comCloudMachine);

    /** Returns cloud machine object. */
    CCloudMachine cloudMachine() const { return m_comCloudMachine; }

    /** Returns error info. */
    QString errorInfo() const;

    /** Returns the task result. */
    CForm result() const;

protected:

    /** Contains the task body. */
    virtual void run() /* override */;

private:

    /** Holds the mutex to access result. */
    mutable QMutex  m_mutex;

    /** Holds the cloud machine object. */
    CCloudMachine  m_comCloudMachine;

    /** Holds the error info. */
    QString  m_strErrorInfo;

    /** Holds the task result. */
    CForm  m_comResult;
};

/** QObject extension used to receive and redirect result
  * of get cloud machine settings form task described above. */
class UIReceiverCloudGetSettingsForm : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about task is complete with certain comResult. */
    void sigTaskComplete(const CForm &comResult);

public:

    /** Constructs receiver passing @a pParent to the base-class. */
    UIReceiverCloudGetSettingsForm(QObject *pParent);

public slots:

    /** Handles thread-pool signal about @a pTask is complete. */
    void sltHandleTaskComplete(UITask *pTask);
};

#endif /* !FEQT_INCLUDED_SRC_manager_UITaskCloudGetSettingsForm_h */
