/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSearchWidget class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_UIMediumSearchWidget_h
#define FEQT_INCLUDED_SRC_medium_UIMediumSearchWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QTreeWidgetItem;
class QIComboBox;
class QIToolButton;
class QITreeWidget;
class UISearchLineEdit;


/** QWidget extension providing a simple way to enter a earch term and search type for medium searching
 *  in virtual media manager, medium selection dialog, etc. */
class  SHARED_LIBRARY_STUFF UIMediumSearchWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigPerformSearch();

public:

    enum SearchType
    {
        SearchByName,
        SearchByUUID,
        SearchByMax
    };

public:

    UIMediumSearchWidget(QWidget *pParent = 0);
    SearchType searchType() const;
    QString searchTerm() const;
    void    search(QITreeWidget* pTreeWidget);

 protected:

    void retranslateUi() /* override */;
    virtual void showEvent(QShowEvent *pEvent) /* override */;

 private slots:

    void sltShowNextMatchingItem();
    void sltShowPreviousMatchingItem();

private:

    void    prepareWidgets();
    void    markUnmarkItems(QList<QTreeWidgetItem*> &itemList, bool fMark);
    void    setUnderlineItemText(QTreeWidgetItem* pItem, bool fUnderline);
    /** Increases (or decreases if @p fNext is false) the m_iScrollToIndex and
     *  takes care of the necessary decoration changes to mark the current item. */
    void    goToNextPrevious(bool fNext);
    /** Updates the feedback text of th line edit that shows # of matches. */
    void    updateSearchLineEdit(int iMatchCount, int iScrollToIndex);
    QIComboBox       *m_pSearchComboxBox;
    UISearchLineEdit *m_pSearchTermLineEdit;
    QIToolButton     *m_pShowNextMatchButton;
    QIToolButton     *m_pShowPreviousMatchButton;

    QList<QTreeWidgetItem*> m_matchedItemList;
    QITreeWidget           *m_pTreeWidget;
    int                     m_iScrollToIndex;
};

#endif /* !FEQT_INCLUDED_SRC_medium_UIMediumSearchWidget_h */
