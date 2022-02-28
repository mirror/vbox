/* $Id$ */
/** @file
 * VBox Qt GUI - UIProgressTaskReadCloudMachineList class declaration.
 */

/*
 * Copyright (C) 2020-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIProgressTaskReadCloudMachineList_h
#define FEQT_INCLUDED_SRC_manager_UIProgressTaskReadCloudMachineList_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UICloudEntityKey.h"
#include "UIProgressTask.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"
#include "CCloudMachine.h"

/** UIProgressTask extension performing read cloud machine list task. */
class UIProgressTaskReadCloudMachineList : public UIProgressTask
{
    Q_OBJECT;

public:

    /** Constructs read cloud machine list task passing @a pParent to the base-class.
      * @param  guiCloudProfileKey  Brings cloud profile description key.
      * @param  fWithRefresh        Brings whether cloud machine should be refreshed as well. */
    UIProgressTaskReadCloudMachineList(QObject *pParent, const UICloudEntityKey &guiCloudProfileKey, bool fWithRefresh);

    /** Returns cloud profile description key. */
    UICloudEntityKey cloudProfileKey() const;

    /** Returns resulting cloud machine-wrapper vector. */
    QVector<CCloudMachine> machines() const;

    /** Returns error message. */
    QString errorMessage() const;

protected:

    /** Creates and returns started progress-wrapper required to init UIProgressObject. */
    virtual CProgress createProgress() RT_OVERRIDE;
    /** Handles finished @a comProgress wrapper. */
    virtual void handleProgressFinished(CProgress &comProgress) RT_OVERRIDE;

private:

    /** Holds the cloud profile description key. */
    UICloudEntityKey  m_guiCloudProfileKey;
    /** Holds whether cloud machine should be refreshed as well. */
    bool              m_fWithRefresh;

    /** Holds the cloud client-wrapper. */
    CCloudClient            m_comCloudClient;
    /** Holds the resulting cloud machine-wrapper vector. */
    QVector<CCloudMachine>  m_machines;

    /** Holds the error message. */
    QString  m_strErrorMessage;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIProgressTaskReadCloudMachineList_h */
