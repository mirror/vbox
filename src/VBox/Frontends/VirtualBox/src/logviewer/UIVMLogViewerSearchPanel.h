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
    /** Forward search routine wrapper. */
    void findNext();
    /** Backward search routine wrapper. */
    void findPrevious();

private:

    enum SearchDirection { ForwardSearch, BackwardSearch };

    /** Clear the highlighting */
    void clearHighlighting();

    /** Search routine.
      * @param  eDirection     Specifies the seach direction
      * @param  highlight      if false highlight function is not called
                               thus we avoid calling highlighting for the same string repeatedly. */
    void search(SearchDirection eDirection, bool highlight);
    void highlightAll(QTextDocument *pDocument, const QString &searchString);
    /** Constructs the find flags for QTextDocument::find function. */
    QTextDocument::FindFlags constructFindFlags(SearchDirection eDirection) const;
    /** Searches the whole document and return the number of matches to the current search term. */
    int countMatches(QTextDocument *pDocument, const QString &searchString) const;
    void setMatchCount(int iCount);
    /** Holds the instance of search-editor we create. */
    UISearchLineEdit *m_pSearchEditor;

    QIToolButton *m_pNextButton;
    QIToolButton *m_pPreviousButton;
    /** Holds the instance of case-sensitive checkbox we create. */
    QCheckBox    *m_pCaseSensitiveCheckBox;
    QCheckBox    *m_pMatchWholeWordCheckBox;
    QCheckBox    *m_pHighlightAllCheckBox;

    /** Holds the position where we start the next search. */
    int          m_iSearchCursorPosition;
    /** Holds the number of the matches for the string. 0 for no matches. */
    int          m_iMatchCount;

    /** Stores relative positions of the lines of the matches. The values are [0,1]
        0 being the first line 1 being the last. */
    QVector<float> m_matchLocationVector;
};


#endif /* !FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerSearchPanel_h */
