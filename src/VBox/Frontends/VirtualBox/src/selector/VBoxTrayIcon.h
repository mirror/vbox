/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxTrayIcon class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxTrayIcon_h__
#define __VBoxTrayIcon_h__

#ifdef VBOX_GUI_WITH_SYSTRAY

/* Global includes */
#include <QSystemTrayIcon>

/* Local forward declarations */
class UISelectorWindow;
class UIVMItem;
class UIVMItemModel;

/* Global forward declarations */
class QMenu;
class QAction;

class VBoxTrayIcon : public QSystemTrayIcon
{
    Q_OBJECT;

public:

    VBoxTrayIcon (UISelectorWindow* aParent, UIVMItemModel* aVMModel);
    virtual ~VBoxTrayIcon ();

    void refresh ();
    void retranslateUi ();

protected:

    UIVMItem* GetItem (QObject* aObject);

public slots:

    void trayIconShow (bool aShow = false);

private slots:

    void showSubMenu();
    void hideSubMenu ();

    void vmSettings();
    void vmDelete();
    void vmStart();
    void vmDiscard();
    void vmPause(bool aPause);
    void vmRefresh();
    void vmShowLogs();

private:

    bool mActive;           /* Is systray menu active/available? */

    /* The vm list model */
    UIVMItemModel *mVMModel;

    UISelectorWindow* mParent;
    QMenu *mTrayIconMenu;

    QAction *mShowSelectorAction;
    QAction *mHideSystrayMenuAction;
    QAction *mVmConfigAction;
    QAction *mVmDeleteAction;
    QAction *mVmStartAction;
    QAction *mVmDiscardAction;
    QAction *mVmPauseAction;
    QAction *mVmRefreshAction;
    QAction *mVmShowLogsAction;
};

#endif /* VBOX_GUI_WITH_SYSTRAY */

#endif /* __VBoxTrayIcon_h__ */

