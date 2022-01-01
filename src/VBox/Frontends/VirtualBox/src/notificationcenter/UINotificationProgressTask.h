/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationProgressTask class declaration.
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationProgressTask_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationProgressTask_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIProgressTask.h"

/* Forward declarations: */
class UINotificationProgress;

/** UIProgressTask extension for notification-center needs executed the silent way.
  * That means no modal messages will arise, you'll be able to retreive error information via API
  * provided. createProgress() and handleProgressFinished() being reloaded to handle everythig
  * silently, you just need to implement UINotificationProgress::createProgress() instead. */
class SHARED_LIBRARY_STUFF UINotificationProgressTask : public UIProgressTask
{
    Q_OBJECT;

public:

    /** Creates notification progress task passing @a pParent to the base-class.
      * @param  pParent  Brings the notification progress this task belongs to. */
    UINotificationProgressTask(UINotificationProgress *pParent);

    /** Returns error message. */
    QString errorMessage() const;

protected:

    /** Creates and returns started progress-wrapper required to init UIProgressObject.
      * @note  You don't need to reload it, it uses pParent's createProgress()
      *        which should be reloaded in your pParent sub-class. */
    virtual CProgress createProgress() /* override final */;
    /** Handles finished @a comProgress wrapper.
      * @note  You don't need to reload it. */
    virtual void handleProgressFinished(CProgress &comProgress) /* override final */;

private:

    /** Holds the notification progress this task belongs to. */
    UINotificationProgress *m_pParent;

    /** Holds the error message. */
    QString  m_strErrorMessage;
};

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationProgressTask_h */
