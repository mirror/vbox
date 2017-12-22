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

#ifndef ___UIVMLogViewerSearchPanel_h___
#define ___UIVMLogViewerSearchPanel_h___

/* Qt includes: */
#include <QTextDocument>
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QCheckBox;
class QHBoxLayout;
class QLabel;
class QSpacerItem;
class UIMiniCancelButton;
class UIRoundRectSegmentedButton;
class UISearchField;
class UIVMLogViewerWidget;

/** QWidget extension
  * providing GUI for search-panel in VM Log-Viewer. */
class UIVMLogViewerSearchPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigHighlightingUpdated();

public:

    /** Constructs search-panel by passing @a pParent to the QWidget base-class constructor.
      * @param  pViewer  Specifies instance of VM Log-Viewer. */
    UIVMLogViewerSearchPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);
    /** Resets the saech position and starts a new search. */
    void refresh();
    void reset();
    const QVector<float> &getMatchLocationVector() const;

protected:

    virtual void hideEvent(QHideEvent* pEvent) /* override */;

private slots:

    /** Handles find next/back action triggering.
      * @param  iButton  Specifies id of next/back button. */
    void find(int iButton);
    /** Handles textchanged signal from search-editor.
      * @param  strSearchString  Specifies search-string. */
    void findCurrent(const QString &strSearchString);
    void sltHighlightAllCheckBox();

private:

    enum SearchDirection { ForwardSearch, BackwardSearch };
    /** Prepares search-panel. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    void prepareConnections();

    /** Handles translation event. */
    void retranslateUi();

    /** Handles Qt key-press @a pEevent. */
    void keyPressEvent(QKeyEvent *pEvent);
    /** Handles Qt @a pEvent, used for keyboard processing. */
    bool eventFilter(QObject *pObject, QEvent *pEvent);
    /** Handles Qt show @a pEvent. */
    void showEvent(QShowEvent *pEvent);


    /** Search routine.
      * @param  eDirection     Specifies the seach direction
      * @param  highlight      if false highlight function is not called
                               thus we avoid calling highlighting for the same string repeatedly. */
    void search(SearchDirection eDirection, bool highlight);
    /** Forward search routine wrapper. */
    void findNext();
    /** Backward search routine wrapper. */
    void findBack();
    void highlightAll(QTextDocument *pDocument, const QString &searchString);
    /** Controls the visibility of the warning icon and info labels.
     Also marks the search editor in case of no match.*/
    void configureInfoLabels();
    /** Constructs the find flags for QTextDocument::find function. */
    QTextDocument::FindFlags constructFindFlags(SearchDirection eDirection);

    /** Holds the reference to the VM Log-Viewer this search-panel belongs to. */
    UIVMLogViewerWidget *m_pViewer;
    /** Holds the instance of main-layout we create. */
    QHBoxLayout *m_pMainLayout;
    /** Holds the instance of close-button we create. */
    UIMiniCancelButton *m_pCloseButton;
    /** Holds the instance of search-label we create. */
    QLabel *m_pSearchLabel;
    /** Holds the instance of search-editor we create. */
    UISearchField *m_pSearchEditor;
    /** Holds the instance of next/back button-box we create. */
    UIRoundRectSegmentedButton *m_pNextPrevButtons;
    /** Holds the instance of case-sensitive checkbox we create. */
    QCheckBox   *m_pCaseSensitiveCheckBox;
    QCheckBox   *m_pMatchWholeWordCheckBox;
    QCheckBox   *m_pHighlightAllCheckBox;
    /** Holds the instance of warning spacer-item we create. */
    QSpacerItem *m_pWarningSpacer;
    /** Holds the instance of warning icon we create. */
    QLabel      *m_pWarningIcon;
    /** Holds the instance of info label we create. */
    QLabel      *m_pInfoLabel;
    /** Holds the instance of spacer item we create. */
    QSpacerItem *m_pSpacerItem;
    /** Holds the position where we start the next search. */
    int          m_iSearchPosition;
    /** Holds the number of the matches for the string.
     -1: highLightAll function is not called
      0: no matches found
      n > 0: n matches found. */
    int          m_iMatchCount;
    /** Stores relative positions of the lines of the matches. The values are [0,1] 0 being the first line 1 being the last. */
    QVector<float> m_matchLocationVector;
};


#endif /* !___UIVMLogViewerSearchPanel_h___ */
