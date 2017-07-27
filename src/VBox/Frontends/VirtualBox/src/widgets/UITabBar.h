/* $Id$ */
/** @file
 * VBox Qt GUI - UITabBar class declaration.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UITabBar_h___
#define ___UITabBar_h___

/* Qt includes: */
#include <QIcon>
#include <QString>
#include <QUuid>
#include <QWidget>

/* Forward declarations: */
class QHBoxLayout;
class QIcon;
class QString;
class QUuid;
class QWidget;
class UITabBarItem;


/** Our own skinnable implementation of tab-bar.
  * The idea is to make tab-bar analog which looks more interesting
  * on various platforms, allows for various skins, and tiny adjustments. */
class UITabBar : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about tab with @a uuid requested closing. */
    void sigTabRequestForClosing(const QUuid &uuid);
    /** Notifies about tab with @a uuid set to current. */
    void sigCurrentTabChanged(const QUuid &uuid);

public:

    /** Constructs tab-bar passing @a pParent to the base-class. */
    UITabBar(QWidget *pParent = 0);

    /** Adds new tab with passed @a icon and @a strName. @returns unique tab ID. */
    QUuid addTab(const QIcon &icon = QIcon(), const QString &strName = QString());

    /** Removes tab with passed @a uuid. */
    bool removeTab(const QUuid &uuid);

    /** Makes tab with passed @a uuid current. */
    bool setCurrent(const QUuid &uuid);

private slots:

    /** Handles request to make @a pItem current. */
    void sltHandleMakeChildCurrent(UITabBarItem *pItem);

    /** Handles request to close @a pItem. */
    void sltHandleChildClose(UITabBarItem *pItem);

private:

    /** Prepares all. */
    void prepare();

    /** Holds the main layout instance. */
    QHBoxLayout *m_pLayout;

    /** Holds the current item reference. */
    UITabBarItem *m_pCurrentItem;

    /** Holds the array of items instances. */
    QList<UITabBarItem*> m_aItems;
};

#endif /* !___UITabBar_h___ */

