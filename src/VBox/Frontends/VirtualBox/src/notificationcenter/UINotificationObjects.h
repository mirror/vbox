/* $Id$ */
/** @file
 * VBox Qt GUI - Various UINotificationObjects declarations.
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

#ifndef FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjects_h
#define FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjects_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UINotificationObject.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/** UINotificationProgress extension for medium move functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMediumMove : public UINotificationProgress
{
    Q_OBJECT;

public:

    /** Constructs medium move notification-progress.
      * @param  comMedium  Brings the medium being moved.
      * @param  strFrom    Brings the initial location.
      * @param  strTo      Brings the final location. */
    UINotificationProgressMediumMove(const CMedium &comMedium,
                                     const QString &strFrom,
                                     const QString &strTo);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private:

    /** Holds the medium being moved. */
    CMedium  m_comMedium;
    /** Holds the initial location. */
    QString  m_strFrom;
    /** Holds the final location. */
    QString  m_strTo;
};

/** UINotificationProgress extension for medium copy functionality. */
class SHARED_LIBRARY_STUFF UINotificationProgressMediumCopy : public UINotificationProgress
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a comMedium was copied. */
    void sigMediumCopied(const CMedium &comMedium);

public:

    /** Constructs medium move notification-progress.
      * @param  comSource  Brings the medium being copied.
      * @param  comTarget  Brings the medium being the target.
      * @param  variants   Brings the target medium options. */
    UINotificationProgressMediumCopy(const CMedium &comSource,
                                     const CMedium &comTarget,
                                     const QVector<KMediumVariant> &variants);

protected:

    /** Returns object name. */
    virtual QString name() const /* override final */;
    /** Returns object details. */
    virtual QString details() const /* override final */;
    /** Creates and returns started progress-wrapper. */
    virtual CProgress createProgress(COMResult &comResult) /* override final */;

private slots:

    /** Handles signal about progress being finished. */
    void sltHandleProgressFinished();

private:

    /** Holds the medium being copied. */
    CMedium                  m_comSource;
    /** Holds the medium being the target. */
    CMedium                  m_comTarget;
    /** Holds the target medium options. */
    QVector<KMediumVariant>  m_variants;
};

#endif /* !FEQT_INCLUDED_SRC_notificationcenter_UINotificationObjects_h */
