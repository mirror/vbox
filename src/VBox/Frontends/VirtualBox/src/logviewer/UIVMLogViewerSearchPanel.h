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

public:

    /** Constructs search-panel by passing @a pParent to the QWidget base-class constructor.
      * @param  pViewer  Specifies instance of VM Log-Viewer. */
    UIVMLogViewerSearchPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);

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
    /** Handles Qt hide @a pEvent. */
    void hideEvent(QHideEvent *pEvent);

    /** Search routine.
      * @param  eDirection     Specifies the seach direction */
    void search(SearchDirection eDirection);
    /** Forward search routine wrapper. */
    void findNext();
    /** Backward search routine wrapper. */
    void findBack();
    void highlightAll(QTextDocument *pDocument, const QTextDocument::FindFlags &findFlags, const QString &searchString);
    /** Shows/hides the search border warning using @a fHide as hint. */
    void toggleWarning(bool fHide);
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
    /** Holds the instance of warning label we create. */
    QLabel      *m_pWarningLabel;
    /** Holds the instance of spacer item we create. */
    QSpacerItem *m_pSpacerItem;
    /** Holds the position where we start the next search. */
    int          m_iSearchPosition;
};


#endif /* !___UIVMLogViewerSearchPanel_h___ */
