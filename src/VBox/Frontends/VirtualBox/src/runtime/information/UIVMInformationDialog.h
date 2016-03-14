/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMInformationDialog class declaration.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVMInformationDialog_h___
#define ___UIVMInformationDialog_h___

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CSession.h"

/* Forward declarations: */
class QITabWidget;
class UIMachineWindow;
class QIDialogButtonBox;

/** QMainWindow based dialog providing user with VM details and statistics. */
class UIVMInformationDialog : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

public:

    /** Shows (and creates if necessary)
      * information-dialog for passed @a pMachineWindow. */
    static void invoke(UIMachineWindow *pMachineWindow);

protected:

    /** Information dialog constructor. */
    UIVMInformationDialog(UIMachineWindow *pMachineWindow);
    /** Information dialog destructor. */
    ~UIVMInformationDialog();

    /** Translation handler. */
    void retranslateUi();

    /** Common event-handler. */
    bool event(QEvent *pEvent);

private slots:

    /** Slot to destroy dialog immediately. */
    void suicide() { delete this; }
    /** Slot to handle tab-widget page change. */
    void sltHandlePageChanged(int iIndex);

private:

    /** General prepare helper. */
    void prepare();
    /** Prepare helper for dialog itself. */
    void prepareThis();
    /** Prepare helper for central-widget. */
    void prepareCentralWidget();
    /** Prepare helper for tab-widget. */
    void prepareTabWidget();
    /** Prepare helper for @a iTabIndex. */
    void prepareTab(int iTabIndex);
    /** Prepare helper for button-box. */
    void prepareButtonBox();
    /** Load settings helper. */
    void loadSettings();

    /** Save settings helper. */
    void saveSettings();
    /** General cleanup helper. */
    void cleanup();

    /** @name General variables.
     * @{ */
    /** Dialog instance pointer. */
    static UIVMInformationDialog *m_spInstance;
    /** Current dialog geometry. */
    QRect                  m_geometry;
    /** @} */

    /** @name Widget variables.
     * @{ */
    /** Dialog tab-widget. */
    QITabWidget               *m_pTabWidget;
    /** Dialog tabs map. */
    QMap<int, QWidget*>        m_tabs;
    /** Dialog button-box. */
    QIDialogButtonBox         *m_pButtonBox;
    /** machine-window. */
    UIMachineWindow         *m_pMachineWindow;
    /** @} */
};

#endif /* !___UIVMInformationDialog_h___ */

