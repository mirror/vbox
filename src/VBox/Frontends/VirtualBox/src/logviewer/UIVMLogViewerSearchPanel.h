/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerSearchPanel_h
#define FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerSearchPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QTextDocument>

/* GUI includes: */
#include "UIVMLogViewerPanel.h"

/* Forward declarations: */
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QWidget;
class QIToolButton;
class UISearchLineEdit;
class UIVMLogViewerSearchField;
class UIVMLogViewerWidget;

/** UIVMLogViewerPanel extension
  * providing GUI for search-panel in VM Log-Viewer. */
class UIVMLogViewerSearchPanel : public UIVMLogViewerPanel
{
    Q_OBJECT;

signals:

    void sigHighlightingUpdated();
    void sigSearchUpdated();

public:

    /** Constructs search-panel by passing @a pParent to the QWidget base-class constructor.
      * @param  pViewer  Specifies instance of VM Log-Viewer. */
    UIVMLogViewerSearchPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);
    /** Resets the search position and starts a new search. */
    void refresh();
    void reset();
    const QVector<float> &matchLocationVector() const;
    virtual QString panelName() const /* override */;
    /** Returns the number of the matches to the current search. */
    int matchCount() const;

protected:

    virtual void prepareWidgets() /* override */;
    virtual void prepareConnections() /* override */;
    virtual void retranslateUi() /* override */;
    /** Handles Qt key-press @a pEevent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;
    /** Handles Qt @a pEvent, used for keyboard processing. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    virtual void showEvent(QShowEvent *pEvent) /* override */;
    virtual void hideEvent(QHideEvent* pEvent) /* override */;

private slots:

    /** Handles textchanged signal from search-editor.
      * @param  strSearchString  Specifies search-string. */
    void sltSearchTextChanged(const QString &strSearchString);
    void sltHighlightAllCheckBox();
    void sltCaseSentitiveCheckBox();
    void sltMatchWholeWordCheckBox();
    void sltSelectNextPreviousMatch();

private:

    enum SearchDirection { ForwardSearch, BackwardSearch };

    /** Clear the highlighting */
    void clearHighlighting();

    /** Search routine.
      * @param  eDirection     Specifies the seach direction
      * @param  highlight      if false highlight function is not called
                               thus we avoid calling highlighting for the same string repeatedly. */
    void performSearch(SearchDirection eDirection, bool highlight);
    void highlightAll(QTextDocument *pDocument, const QString &searchString);
    void findAll(QTextDocument *pDocument, const QString &searchString);
    void selectMatch(int iMatchIndex, const QString &searchString);
    void moveSelection(bool fForward);

    /** Constructs the find flags for QTextDocument::find function. */
    QTextDocument::FindFlags constructFindFlags(SearchDirection eDirection) const;
    /** Searches the whole document and return the number of matches to the current search term. */
    int countMatches(QTextDocument *pDocument, const QString &searchString) const;
    /** Holds the instance of search-editor we create. */
    UISearchLineEdit *m_pSearchEditor;

    QIToolButton *m_pNextButton;
    QIToolButton *m_pPreviousButton;
    /** Holds the instance of case-sensitive checkbox we create. */
    QCheckBox    *m_pCaseSensitiveCheckBox;
    QCheckBox    *m_pMatchWholeWordCheckBox;
    QCheckBox    *m_pHighlightAllCheckBox;
    /** Stores relative positions of the lines of the matches wrt. total # of lines. The values are in [0,1]
        0 being the first line 1 being the last. */
    QVector<float> m_matchLocationVector;
    /** Document positions of the cursors within th document for all matches. */
    QVector<int>   m_matchedCursorPosition;
    /** The index of the curently selected item within m_matchedCursorPosition. */
    int            m_iSelectedMatchIndex;
};


#endif /* !FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerSearchPanel_h */
