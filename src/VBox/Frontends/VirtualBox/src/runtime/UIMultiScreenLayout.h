/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMultiScreenLayout class declaration
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMultiScreenLayout_h__
#define __UIMultiScreenLayout_h__

/* Qt includes: */
#include <QObject>

/* Forward declarations: */
class UIMachineLogic;
class QMenu;
class QAction;
template <class Key, class T> class QMap;

/* Multi-screen layout manager: */
class UIMultiScreenLayout : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Layout change stuff: */
    void sigScreenLayoutChanged();

public:

    /* Constructor/destructor: */
    UIMultiScreenLayout(UIMachineLogic *pMachineLogic);
    ~UIMultiScreenLayout();

    /* API: View-menu stuff: */
    void setViewMenu(QMenu *pViewMenu);

    /* API: Update stuff: */
    void update();

    /* API: Getters: */
    int hostScreenCount() const;
    int guestScreenCount() const;
    int hostScreenForGuestScreen(int iScreenId) const;
    quint64 memoryRequirements() const;
    bool isHostTaskbarCovert() const;

private slots:

    /* Handler: Screen change stuff: */
    void sltScreenLayoutChanged(QAction *pAction);

private:

    /* Other helpers: */
    quint64 memoryRequirements(const QMap<int, int> *pScreenLayout) const;

    /* Variables: */
    UIMachineLogic *m_pMachineLogic;
    int m_cGuestScreens;
    int m_cHostScreens;
    QMap<int, int> *m_pScreenMap;
    QList<QMenu*> m_screenMenuList;
};

#endif /* __UIMultiScreenLayout_h__ */

