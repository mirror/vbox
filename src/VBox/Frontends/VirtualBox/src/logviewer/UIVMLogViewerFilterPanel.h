/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVMLogViewerFilterPanel_h___
#define ___UIVMLogViewerFilterPanel_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QComboBox;
class QHBoxLayout;
class QLabel;
class UIMiniCancelButton;
class UIVMLogViewerWidget;

/** QWidget extension
  * providing GUI for filter panel in VM Log Viewer. */
class UIVMLogViewerFilterPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs the filter-panel by passing @a pParent to the QWidget base-class constructor.
      * @param  pViewer  Specifies reference to the VM Log-Viewer this filter-panel belongs to. */
    UIVMLogViewerFilterPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);

public slots:

    /** Applies filter settings and filters the current log-page.
      * @param  iCurrentIndex  Specifies index of current log-page, but it is actually not used in the method. */
    void applyFilter(const int iCurrentIndex = 0);
private slots:

    /** Handles the textchanged event from filter editor. */
    void filter(const QString &strSearchString);

private:

    /** Prepares filter-panel. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Handles the translation event. */
    void retranslateUi();

    /** Handles Qt @a pEvent, used for keyboard processing. */
    bool eventFilter(QObject *pObject, QEvent *pEvent);
    /** Handles the Qt show @a pEvent. */
    void showEvent(QShowEvent *pEvent);
    /** Handles the Qt hide @a pEvent. */
    void hideEvent(QHideEvent *pEvent);

    /** Holds the reference to VM Log-Viewer this filter-panel belongs to. */
    UIVMLogViewerWidget *m_pViewer;
    /** Holds the instance of main-layout we create. */
    QHBoxLayout *m_pMainLayout;
    /** Holds the instance of close-button we create. */
    UIMiniCancelButton *m_pCloseButton;
    /** Holds the instance of filter-label we create. */
    QLabel *m_pFilterLabel;
    /** Holds instance of filter combo-box we create. */
    QComboBox *m_pFilterComboBox;
    /** Holds the filter text. */
    QString m_strFilterText;
};

#endif /* !___UIVMLogViewerFilterPanel_h___ */
