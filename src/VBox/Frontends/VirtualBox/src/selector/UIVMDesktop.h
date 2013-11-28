/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMDesktop class declarations
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

#ifndef __UIVMDesktop_h__
#define __UIVMDesktop_h__

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class CMachine;
class UIVMDesktopPrivate;
class UITexturedSegmentedButton;
class UIVMItem;
class VBoxSnapshotsWgt;
class UIToolBar;
class QStackedLayout;

/* Class representing widget which contains three panes:
 * 1. Information pane reflecting base information about VirtualBox,
 * 2. Inaccessible machine pane reflecting information about
 *    currently chosen inaccessible VM and allowing to operate over it,
 * 3. Snapshot pane allowing to operate over the snapshots. */
class UIVMDesktop: public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /* Notifier: Current widget stuff: */
    void sigCurrentChanged(int iWidgetIndex);

public:

    /* Constructor: */
    UIVMDesktop(UIToolBar *pToolBar, QAction *pRefreshAction, QWidget *pParent);

    /* API: Current pane index: */
    int widgetIndex() const;

    /* Helpers: Update stuff: */
    void updateDetailsText(const QString &strText);
    void updateDetailsError(const QString &strError);
    void updateSnapshots(UIVMItem *pVMItem, const CMachine& machine);
    void lockSnapshots();

private slots:

    /** Initialization handler. */
    void sltInit();

private:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Variables: */
    QStackedLayout *m_pStackedLayout;
    UITexturedSegmentedButton *m_pHeaderBtn;
    UIVMDesktopPrivate *m_pDesktopPrivate;
    VBoxSnapshotsWgt *m_pSnapshotsPane;
};

#endif /* !__UIVMDesktop_h__ */

