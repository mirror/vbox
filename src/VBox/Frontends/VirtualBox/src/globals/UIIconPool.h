/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIIconPool class declarations
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIIconPool_h__
#define __UIIconPool_h__

/* Global includes */
#include <QIcon>
#include <QStyle>

class UIIconPool
{
public:

    enum UIDefaultIcon
    {
        MessageBoxInformationIcon,
        MessageBoxQuestionIcon,
        MessageBoxWarningIcon,
        MessageBoxCriticalIcon,
        DialogCancelIcon,
        DialogHelpIcon,
        ArrowBackIcon,
        ArrowForwardIcon
    };

    static QIcon iconSet(const QPixmap &normal,
                         const QPixmap &disabled = QPixmap(),
                         const QPixmap &active = QPixmap());
    static QIcon iconSet(const QString &strNormal,
                         const QString &strDisabled = QString(),
                         const QString &strActive = QString());
    static QIcon iconSetOnOff(const QString &strNormal, const QString strNormalOff,
                              const QString &strDisabled = QString(),
                              const QString &strDisabledOff = QString(),
                              const QString &strActive = QString(),
                              const QString &strActiveOff = QString());
    static QIcon iconSetFull(const QSize &normalSize, const QSize &smallSize,
                             const QString &strNormal, const QString &strSmallNormal,
                             const QString &strDisabled = QString(),
                             const QString &strSmallDisabled = QString(),
                             const QString &strActive = QString(),
                             const QString &strSmallActive = QString());

    static QIcon defaultIcon(UIDefaultIcon def, const QWidget *pWidget = 0);

private:

    UIIconPool() {};
    UIIconPool(const UIIconPool& /* pool */) {};
    ~UIIconPool() {};
};

#endif /* !__UIIconPool_h__ */

