/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerPanel_h
#define FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIDialogPanel.h"

/* Forward declarations: */
class QPlainTextEdit;
class QTextDocument;
class UIVMLogViewerWidget;
class UIVMLogViewerSearchPanel;
class UIVMLogViewerFilterPanel;

class UIVMLogViewerPanelNew : public QIWithRetranslateUI<UIDialogPanelBase>
{
    Q_OBJECT;

signals:

    void sigHighlightingUpdated();
    void sigSearchUpdated();
    void sigFilterApplied();

public:

    UIVMLogViewerPanelNew(QWidget *pParent, UIVMLogViewerWidget *pViewer);

    /** @name Search page pass through functions
      * @{ */
        void refreshSearch();
        QVector<float> matchLocationVector() const;
        /** Returns the number of the matches to the current search. */
        int matchCount() const;
    /** @} */

    /** @name Filter page pass through functions
      * @{ */
        void applyFilter();
    /** @} */


    enum Page
    {
        Page_Search = 0,
        Page_Filter,
        Page_Bookmark,
        Page_Preferences,
        Page_Max
    };

protected:

    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) override;

private:

    void retranslateUi() override;
    void prepare() override;

    UIVMLogViewerWidget *m_pViewer;

    UIVMLogViewerSearchPanel *m_pSearchWidget;
    UIVMLogViewerFilterPanel *m_pFilterWidget;
};

/** UIDialonPanel extension acting as the base class for UIVMLogViewerXXXPanel widgets. */
class UIVMLogViewerPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIVMLogViewerPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);

protected:

    virtual void retranslateUi() RT_OVERRIDE;

    virtual void prepareWidgets() = 0;
    virtual void prepareConnections()  = 0;

    /* Access functions for children classes. */
    UIVMLogViewerWidget        *viewer();
    const UIVMLogViewerWidget  *viewer() const;
    QTextDocument  *textDocument();
    QPlainTextEdit *textEdit();
    /* Return the unmodified log. */
    const QString *logString() const;

private:

    /** Holds the reference to VM Log-Viewer this panel belongs to. */
    UIVMLogViewerWidget *m_pViewer;
};

#endif /* !FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerPanel_h */
