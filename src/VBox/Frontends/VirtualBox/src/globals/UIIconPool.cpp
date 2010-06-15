/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIIconPool class implementation
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

/* Local includes */
#include "UIIconPool.h"

/* Global includes */
#include <iprt/assert.h>

#include <QApplication>
#include <QWidget>

/* static */
QIcon UIIconPool::iconSet(const QPixmap &normal,
                          const QPixmap &disabled /* = QPixmap() */,
                          const QPixmap &active /* = QPixmap() */)
{
    QIcon iconSet;

    Assert(!normal.isNull());
    iconSet.addPixmap(normal, QIcon::Normal);

    if (!disabled.isNull())
        iconSet.addPixmap(disabled, QIcon::Disabled);

    if (!active.isNull())
        iconSet.addPixmap(active, QIcon::Active);

    return iconSet;
}

/* static */
QIcon UIIconPool::iconSet(const QString &strNormal,
                          const QString &strDisabled /* = QString() */,
                          const QString &strActive /* = QString() */)
{
    QIcon iconSet;

    Assert(!strNormal.isEmpty());
    iconSet.addFile(strNormal, QSize(),
                    QIcon::Normal);
    if (!strDisabled.isEmpty())
        iconSet.addFile(strDisabled, QSize(),
                        QIcon::Disabled);
    if (!strActive.isEmpty())
        iconSet.addFile(strActive, QSize(),
                        QIcon::Active);
    return iconSet;
}

/* static */
QIcon UIIconPool::iconSetOnOff(const QString &strNormal, const QString strNormalOff,
                               const QString &strDisabled /* = QString() */,
                               const QString &strDisabledOff /* = QString() */,
                               const QString &strActive /* = QString() */,
                               const QString &strActiveOff /* = QString() */)
{
    QIcon iconSet;

    Assert(!strNormal.isEmpty());
    iconSet.addFile(strNormal, QSize(), QIcon::Normal, QIcon::On);
    if (!strNormalOff.isEmpty())
        iconSet.addFile(strNormalOff, QSize(), QIcon::Normal, QIcon::Off);

    if (!strDisabled.isEmpty())
        iconSet.addFile(strDisabled, QSize(), QIcon::Disabled, QIcon::On);
    if (!strDisabledOff.isEmpty())
        iconSet.addFile(strDisabledOff, QSize(), QIcon::Disabled, QIcon::Off);

    if (!strActive.isEmpty())
        iconSet.addFile(strActive, QSize(), QIcon::Active, QIcon::On);
    if (!strActiveOff.isEmpty())
        iconSet.addFile(strActive, QSize(), QIcon::Active, QIcon::Off);

    return iconSet;
}

/* static */
QIcon UIIconPool::iconSetFull(const QSize &normalSize, const QSize &smallSize,
                              const QString &strNormal, const QString &strSmallNormal,
                              const QString &strDisabled /* = QString() */,
                              const QString &strSmallDisabled /* = QString() */,
                              const QString &strActive /* = QString() */,
                              const QString &strSmallActive /* = QString() */)
{
    QIcon iconSet;

    Assert(!strNormal.isEmpty());
    Assert(!strSmallNormal.isEmpty());
    iconSet.addFile(strNormal, normalSize, QIcon::Normal);
    iconSet.addFile(strSmallNormal, smallSize, QIcon::Normal);

    if (!strSmallDisabled.isEmpty())
    {
        iconSet.addFile(strDisabled, normalSize, QIcon::Disabled);
        iconSet.addFile(strSmallDisabled, smallSize, QIcon::Disabled);
    }

    if (!strSmallActive.isEmpty())
    {
        iconSet.addFile(strActive, normalSize, QIcon::Active);
        iconSet.addFile(strSmallActive, smallSize, QIcon::Active);
    }

    return iconSet;
}

/* static */
QIcon UIIconPool::defaultIcon(UIDefaultIcon def, const QWidget *pWidget /* = 0 */)
{
    QIcon icon;
    QStyle *pStyle = pWidget ? pWidget->style() : QApplication::style();
    switch (def)
    {
        case MessageBoxInformationIcon:
        {
            icon = pStyle->standardIcon(QStyle::SP_MessageBoxInformation, 0, pWidget);
            break;
        }
        case MessageBoxQuestionIcon:
        {
            icon = pStyle->standardIcon(QStyle::SP_MessageBoxQuestion, 0, pWidget);
            break;
        }
        case MessageBoxWarningIcon:
        {
#ifdef Q_WS_MAC
            /* At least in Qt 4.3.4/4.4 RC1 SP_MessageBoxWarning is the application
             * icon. So change this to the critical icon. (Maybe this would be
             * fixed in a later Qt version) */
            icon = pStyle->standardIcon(QStyle::SP_MessageBoxCritical, 0, pWidget);
#else /* Q_WS_MAC */
            icon = pStyle->standardIcon(QStyle::SP_MessageBoxWarning, 0, pWidget);
#endif /* !Q_WS_MAC */
            break;
        }
        case MessageBoxCriticalIcon:
        {
            icon = pStyle->standardIcon(QStyle::SP_MessageBoxCritical, 0, pWidget);
            break;
        }
        case ArrowBackIcon:
        {
            icon = pStyle->standardIcon(QStyle::SP_ArrowBack, 0, pWidget);
            if (icon.isNull())
                icon = iconSet(":/list_moveup_16px.png",
                               ":/list_moveup_disabled_16px.png");
            break;
        }
        case ArrowForwardIcon:
        {
            icon = pStyle->standardIcon(QStyle::SP_ArrowForward, 0, pWidget);
            if (icon.isNull())
                icon = iconSet(":/list_movedown_16px.png",
                               ":/list_movedown_disabled_16px.png");
            break;
        }
        default:
        {
            AssertMsgFailed(("Unknown default icon type!"));
            break;
        }
    }
    return icon;
}

