/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UIDialogPanel_h
#define FEQT_INCLUDED_SRC_globals_UIDialogPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>
#include <QKeySequence>
/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHBoxLayout;
class QIToolButton;


/** QWidget extension acting as the base class for all the dialog panels like file manager, logviewer etc. */
class SHARED_LIBRARY_STUFF UIDialogPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIDialogPanel(QWidget *pParent = 0);
    void setCloseButtonShortCut(QKeySequence shortCut);
    virtual QString panelName() const = 0;

signals:

    void sigHidePanel(UIDialogPanel *pPanel);

protected:

    virtual void prepare();
    virtual void prepareWidgets();
    virtual void prepareConnections();

    /* Access functions for children classes. */
    QHBoxLayout*               mainLayout();

    /** Handles the translation event. */
    void retranslateUi() /* override */;

    /** Handles the Qt show @a pEvent. */
    void showEvent(QShowEvent *pEvent) /* override */;
    /** Handles the Qt hide @a pEvent. */
    void hideEvent(QHideEvent *pEvent) /* override */;
    void addVerticalSeparator();

private:

    QHBoxLayout   *m_pMainLayout;
    QIToolButton  *m_pCloseButton;
};

#endif /* !FEQT_INCLUDED_SRC_globals_UIDialogPanel_h */
