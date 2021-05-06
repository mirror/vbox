/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMInformationDialog class declaration.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_runtime_information_UIVMInformationDialog_h
#define FEQT_INCLUDED_SRC_runtime_information_UIVMInformationDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIWithRestorableGeometry.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSession.h"

/* Forward declarations: */
class QITabWidget;
class QIDialogButtonBox;
class UIMachineWindow;

/* Type definitions: */
typedef QIWithRestorableGeometry<QMainWindow> QMainWindowWithRestorableGeometry;
typedef QIWithRetranslateUI<QMainWindowWithRestorableGeometry> QMainWindowWithRestorableGeometryAndRetranslateUi;


/** QMainWindow subclass providing user
  * with the dialog unifying VM details and statistics. */
class UIVMInformationDialog : public QMainWindowWithRestorableGeometryAndRetranslateUi
{
    Q_OBJECT;

signals:

    void sigClose();

public:

    /** Constructs information dialog for passed @a pMachineWindow. */
    UIVMInformationDialog(UIMachineWindow *pMachineWindow);
    /** Destructs information dialog. */
    ~UIVMInformationDialog();

    /** Returns whether the dialog should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;
    virtual void closeEvent(QCloseEvent *pEvent) /* override */;
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
    virtual void moveEvent(QMoveEvent *pEvent) /* override */;

private slots:

    /** Handles tab-widget page change. */
    void sltHandlePageChanged(int iIndex);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares this. */
    void prepareThis();
    /** Prepares central-widget. */
    void prepareCentralWidget();
    /** Prepares tab-widget. */
    void prepareTabWidget();
    /** Prepares tab with @a iTabIndex. */
    void prepareTab(int iTabIndex);
    /** Prepares button-box. */
    void prepareButtonBox();
    void loadDialogGeometry();
    void saveDialogGeometry();

    /** @name Widget variables.
     * @{ */
    /** Holds the dialog tab-widget instance. */
    QITabWidget                  *m_pTabWidget;
    /** Holds the map of dialog tab instances. */
    QMap<int, QWidget*>           m_tabs;
    /** Holds the dialog button-box instance. */
    QIDialogButtonBox            *m_pButtonBox;
    /** Holds the machine-window reference. */
    UIMachineWindow              *m_pMachineWindow;
    /** @} */
    bool m_fCloseEmitted;
};

#endif /* !FEQT_INCLUDED_SRC_runtime_information_UIVMInformationDialog_h */
