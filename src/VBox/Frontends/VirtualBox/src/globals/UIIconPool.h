/** @file
 * VBox Qt GUI - UIIconPool class declaration.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIIconPool_h___
#define ___UIIconPool_h___

/* Qt includes: */
#include <QIcon>

/** General icon-pool which provides GUI with:
  * 1. necessary cached icons and
  * 2. ways to dynamically open/read/create icons at runtime
  *    depending on current style. */
class UIIconPool
{
public:

    /** Default icon types. */
    enum UIDefaultIconType
    {
        /* Message-box related stuff: */
        UIDefaultIconType_MessageBoxInformation,
        UIDefaultIconType_MessageBoxQuestion,
        UIDefaultIconType_MessageBoxWarning,
        UIDefaultIconType_MessageBoxCritical,
        /* Dialog related stuff: */
        UIDefaultIconType_DialogCancel,
        UIDefaultIconType_DialogHelp,
        UIDefaultIconType_ArrowBack,
        UIDefaultIconType_ArrowForward
    };

    /** Creates icon from passed pixmap names for
      * @a strNormal, @a strDisabled and @a strActive icon states. */
    static QIcon iconSet(const QString &strNormal,
                         const QString &strDisabled = QString(),
                         const QString &strActive = QString());

    /** Creates icon from passed pixmap names for
      * @a strNormal, @a strDisabled, @a strActive icon states and
      * their analogs for toggled-off case. Used for toggle actions. */
    static QIcon iconSetOnOff(const QString &strNormal, const QString strNormalOff,
                              const QString &strDisabled = QString(), const QString &strDisabledOff = QString(),
                              const QString &strActive = QString(), const QString &strActiveOff = QString());

    /** Creates icon from passed pixmap names for
      * @a strNormal, @a strDisabled, @a strActive icon states and
      * their analogs for small-icon case. Used for setting pages. */
    static QIcon iconSetFull(const QSize &size, const QSize &smallSize,
                             const QString &strNormal, const QString &strSmallNormal,
                             const QString &strDisabled = QString(), const QString &strSmallDisabled = QString(),
                             const QString &strActive = QString(), const QString &strSmallActive = QString());

    /** Creates icon from passed pixmaps for
      * @a normal, @a disabled and @a active icon states. */
    static QIcon iconSet(const QPixmap &normal,
                         const QPixmap &disabled = QPixmap(),
                         const QPixmap &active = QPixmap());

    /** Creates icon of passed @a defaultIconType
      * based on passed @a pWidget style (if any) or application style (otherwise). */
    static QIcon defaultIcon(UIDefaultIconType defaultIconType, const QWidget *pWidget = 0);

private:

    /** Icon-pool constructor. */
    UIIconPool() {};

    /** Icon-pool destructor. */
    ~UIIconPool() {};
};

#endif /* !___UIIconPool_h___ */
