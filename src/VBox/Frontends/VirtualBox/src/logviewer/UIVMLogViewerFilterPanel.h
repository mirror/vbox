/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerFilterPanel_h
#define FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerFilterPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
# include <QSet>

/* GUI includes: */
#include "UIVMLogViewerPanel.h"

/* Forward declarations: */
class QAbstractButton;
class QButtonGroup;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QIToolButton;
class QRadioButton;
class UIVMFilterLineEdit;


/** QWidget extension
  * providing GUI for filter panel in VM Log Viewer. */
class UIVMLogViewerFilterPanel : public UIVMLogViewerPanel
{
    Q_OBJECT;

signals:

    void sigFilterApplied();

public:

    /** Constructs the filter-panel by passing @a pParent to the QWidget base-class constructor.
      * @param  pViewer  Specifies reference to the VM Log-Viewer this filter-panel belongs to. */
    UIVMLogViewerFilterPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);
    virtual QString panelName() const RT_OVERRIDE;

public slots:

    /** Applies filter settings and filters the current log-page. */
    void applyFilter();

protected:

    virtual void prepareWidgets() RT_OVERRIDE;
    virtual void prepareConnections() RT_OVERRIDE;

    void retranslateUi() RT_OVERRIDE;
    /** Handles Qt @a pEvent, used for keyboard processing. */
    bool eventFilter(QObject *pObject, QEvent *pEvent) RT_OVERRIDE;
    void showEvent(QShowEvent *pEvent) RT_OVERRIDE;
    void hideEvent(QHideEvent *pEvent) RT_OVERRIDE;

private slots:

    /** Adds the new filter term and reapplies the filter. */
    void sltAddFilterTerm();
    /** Clear all the filter terms and reset the filtering. */
    void sltClearFilterTerms();
    /** Executes the necessary code to handle filter's boolean operator change ('And', 'Or'). */
    void sltOperatorButtonChanged(QAbstractButton *pButton);
    void sltRemoveFilterTerm(const QString &termString);

private:

    enum FilterOperatorButton{
        AndButton = 0,/* Don't change this value */
        OrButton,
        ButtonEnd
    };

    void prepareRadioButtonGroup();

    bool applyFilterTermsToString(const QString& string);
    void filter();
    /** Revert the document to original. */
    void resetFiltering();

    QLabel              *m_pFilterLabel;
    QComboBox           *m_pFilterComboBox;
    QButtonGroup        *m_pButtonGroup;
    QRadioButton        *m_pAndRadioButton;
    QRadioButton        *m_pOrRadioButton;
    QFrame              *m_pRadioButtonContainer;
    QIToolButton        *m_pAddFilterTermButton;
    QSet<QString>        m_filterTermSet;
    FilterOperatorButton m_eFilterOperatorButton;
    UIVMFilterLineEdit  *m_pFilterTermsLineEdit;
    QLabel              *m_pResultLabel;
    int                  m_iUnfilteredLineCount;
    int                  m_iFilteredLineCount;
};

#endif /* !FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerFilterPanel_h */
