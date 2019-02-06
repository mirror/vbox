/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSearchWidget class implementation.
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

/* Qt includes: */
#include <QLineEdit>
#include <QPushButton>
#include <QPainter>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIComboBox.h"
#include "QIDialogButtonBox.h"
#include "QIToolButton.h"
#include "QITreeWidget.h"
#include "UIFDCreationDialog.h"
#include "UIIconPool.h"
#include "UIMediumItem.h"
#include "UIMediumSearchWidget.h"
#include "UIToolBar.h"

#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */


/*********************************************************************************************************************************
*   FilterByNameUUID definition/implementation.                                                                                  *
*********************************************************************************************************************************/

class FilterByNameUUID : public QITreeWidgetItemFilter
{

public:

    FilterByNameUUID(UIMediumSearchWidget::SearchType enmSearchType, const QString &strSearchTerm)
        : m_enmSearchType(enmSearchType)
        , m_strSearchTerm(strSearchTerm){}
    virtual ~FilterByNameUUID(){}
    virtual bool operator()(QTreeWidgetItem *pItem) const
    {
        if (!pItem || m_strSearchTerm.isEmpty())
            return false;
        if (pItem->type() != QITreeWidgetItem::ItemType)
            return false;

        UIMediumItem *pMediumItem = dynamic_cast<UIMediumItem*>(pItem);
        if (!pMediumItem)
            return false;
        if (m_enmSearchType == UIMediumSearchWidget::SearchByUUID &&
            !pMediumItem->id().toString().contains(m_strSearchTerm, Qt::CaseInsensitive))
            return false;
        if (m_enmSearchType == UIMediumSearchWidget::SearchByName &&
            !pMediumItem->name().contains(m_strSearchTerm, Qt::CaseInsensitive))
            return false;
        return true;
    }

private:

    UIMediumSearchWidget::SearchType m_enmSearchType;
    QString m_strSearchTerm;
};


/*********************************************************************************************************************************
*   UISearchLineEdit definition             .                                                                                    *
*********************************************************************************************************************************/

class UISearchLineEdit : public QLineEdit
{

    Q_OBJECT;

public:

    UISearchLineEdit(QWidget *pParent = 0);
    void setMatchCount(int iMatchCount);
    void setScroolToIndex(int iScrollToIndex);

protected:

    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private:

    int m_iMatchCount;
    int m_iScrollToIndex;
};


/*********************************************************************************************************************************
*   UISearchLineEdit implementation         .                                                                                    *
*********************************************************************************************************************************/

UISearchLineEdit::UISearchLineEdit(QWidget *pParent /* = 0 */)
    :QLineEdit(pParent)
    , m_iMatchCount(0)
    , m_iScrollToIndex(-1)
{
}

void UISearchLineEdit::paintEvent(QPaintEvent *pEvent)
{
    QLineEdit::paintEvent(pEvent);


    QPainter painter(this);

    QFont pfont = font();
    QString strText = QString("%1/%2").arg(QString::number(m_iScrollToIndex + 1)).arg(QString::number(m_iMatchCount));
    QSize textSize(QApplication::fontMetrics().width(strText),
                   QApplication::fontMetrics().height());

    /* Dont draw anything if we dont have enough space: */
    if (textSize.width() > 0.5 * width())
        return;
    int iTopMargin = (height() - textSize.height()) / 2;
    int iRightMargin = iTopMargin;


    QColor fontColor(Qt::darkGray);
    painter.setPen(fontColor);
    painter.setFont(pfont);

    painter.drawText(QRect(width() - textSize.width() - iRightMargin, iTopMargin, textSize.width(), textSize.height()),
                     Qt::AlignCenter | Qt::AlignVCenter, strText);
}

void UISearchLineEdit::setMatchCount(int iMatchCount)
{
    if (m_iMatchCount == iMatchCount)
        return;
    m_iMatchCount = iMatchCount;
    repaint();
}

void UISearchLineEdit::setScroolToIndex(int iScrollToIndex)
{
    if (m_iScrollToIndex == iScrollToIndex)
        return;
    m_iScrollToIndex = iScrollToIndex;
    repaint();
}


/*********************************************************************************************************************************
*   UIMediumSearchWidget implementation      .                                                                                   *
*********************************************************************************************************************************/

UIMediumSearchWidget::UIMediumSearchWidget(QWidget *pParent)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pSearchComboxBox(0)
    , m_pSearchTermLineEdit(0)
    , m_pShowNextMatchButton(0)
    , m_pShowPreviousMatchButton(0)
    , m_pTreeWidget(0)
    , m_iScrollToIndex(-1)
{
    prepareWidgets();
}

void UIMediumSearchWidget::prepareWidgets()
{
    QHBoxLayout *pLayout = new QHBoxLayout;
    setLayout(pLayout);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);

    m_pSearchComboxBox = new QIComboBox;
    if (m_pSearchComboxBox)
    {
        m_pSearchComboxBox->setEditable(false);
        m_pSearchComboxBox->insertItem(SearchByName, "Search By Name");
        m_pSearchComboxBox->insertItem(SearchByUUID, "Search By UUID");
        pLayout->addWidget(m_pSearchComboxBox);

        connect(m_pSearchComboxBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
                this, &UIMediumSearchWidget::sigPerformSearch);

    }

    m_pSearchTermLineEdit = new UISearchLineEdit;
    if (m_pSearchTermLineEdit)
    {
        m_pSearchTermLineEdit->setClearButtonEnabled(false);
        pLayout->addWidget(m_pSearchTermLineEdit);
        connect(m_pSearchTermLineEdit, &QLineEdit::textChanged,
                this, &UIMediumSearchWidget::sigPerformSearch);
    }

    m_pShowPreviousMatchButton = new QIToolButton;
    if (m_pShowPreviousMatchButton)
    {
        m_pShowPreviousMatchButton->setIcon(UIIconPool::iconSet(":/log_viewer_search_backward_16px.png", ":/log_viewer_search_backward_disabled_16px.png"));
        connect(m_pShowPreviousMatchButton, &QIToolButton::clicked, this, &UIMediumSearchWidget::sltShowPreviousMatchingItem);
        pLayout->addWidget(m_pShowPreviousMatchButton);
    }
    m_pShowNextMatchButton = new QIToolButton;
    if (m_pShowNextMatchButton)
    {
        m_pShowNextMatchButton->setIcon(UIIconPool::iconSet(":/log_viewer_search_forward_16px.png", ":/log_viewer_search_forward_disabled_16px.png"));
        connect(m_pShowNextMatchButton, &QIToolButton::clicked, this, &UIMediumSearchWidget:: sltShowNextMatchingItem);
        pLayout->addWidget(m_pShowNextMatchButton);
    }

    retranslateUi();
}

UIMediumSearchWidget::SearchType UIMediumSearchWidget::searchType() const
{
    if (!m_pSearchComboxBox || m_pSearchComboxBox->currentIndex() >= static_cast<int>(SearchByMax))
        return SearchByMax;
    return static_cast<SearchType>(m_pSearchComboxBox->currentIndex());
}

QString UIMediumSearchWidget::searchTerm() const
{
    if (!m_pSearchTermLineEdit)
        return QString();
    return m_pSearchTermLineEdit->text();
}

void UIMediumSearchWidget::search(QITreeWidget* pTreeWidget)
{
    if (!pTreeWidget)
        return;

    m_pTreeWidget = pTreeWidget;
    QList<QTreeWidgetItem*> allItems = pTreeWidget->filterItems(QITreeWidgetItemFilter());
    markUnmarkItems(allItems, false);

    m_matchedItemList = pTreeWidget->filterItems(FilterByNameUUID(searchType(), searchTerm()));
    markUnmarkItems(m_matchedItemList, true);
    if (!m_matchedItemList.isEmpty())
    {
        m_iScrollToIndex = -1;
        goToNextPrevious(true);
    }
    else
        m_iScrollToIndex = -1;
    updateSearchLineEdit(m_matchedItemList.size(), m_iScrollToIndex);
}

void UIMediumSearchWidget::retranslateUi()
{
    if (m_pSearchComboxBox)
    {
        m_pSearchComboxBox->setItemText(SearchByName, UIMediumSearchWidget::tr("Search By Name"));
        m_pSearchComboxBox->setItemText(SearchByUUID, UIMediumSearchWidget::tr("Search By UUID"));
        m_pSearchComboxBox->setToolTip(UIMediumSearchWidget::tr("Select the search type"));
    }
    if (m_pSearchTermLineEdit)
        m_pSearchTermLineEdit->setToolTip("Enter the search term and press Return");
    if (m_pShowPreviousMatchButton)
        m_pShowPreviousMatchButton->setToolTip("Show the previous item matching the search term");
    if (m_pShowNextMatchButton)
        m_pShowNextMatchButton->setToolTip("Show the next item matching the search term");
}

void UIMediumSearchWidget::showEvent(QShowEvent *pEvent)
{
    if (m_pSearchTermLineEdit)
        m_pSearchTermLineEdit->setFocus();
    QIWithRetranslateUI<QWidget>::showEvent(pEvent);
}

void UIMediumSearchWidget::markUnmarkItems(QList<QTreeWidgetItem*> &itemList, bool fMark)
{
    foreach (QTreeWidgetItem* pItem, itemList)
    {
        if (pItem->type() != QITreeWidgetItem::ItemType)
            continue;
        UIMediumItem *pMediumItem = static_cast<UIMediumItem*>(pItem);
        if (!pMediumItem)
            continue;
        QFont font = pMediumItem->font(0);
        font.setBold(fMark);
        pMediumItem->setFont(0, font);

        if (!fMark)
            setUnderlineItemText(pMediumItem, false);
    }
}

void UIMediumSearchWidget::setUnderlineItemText(QTreeWidgetItem* pItem, bool fUnderline)
{
    if (!pItem)
        return;
    QFont font = pItem->font(0);
    font.setUnderline(fUnderline);
    pItem->setFont(0, font);
}

void UIMediumSearchWidget::goToNextPrevious(bool fNext)
{
    if (!m_pTreeWidget || m_matchedItemList.isEmpty())
        return;

    if (m_iScrollToIndex >= 0 && m_iScrollToIndex < m_matchedItemList.size())
        setUnderlineItemText(m_matchedItemList[m_iScrollToIndex], false);

    if (fNext)
        ++m_iScrollToIndex;
    else
        --m_iScrollToIndex;

    if (m_iScrollToIndex >= m_matchedItemList.size())
        m_iScrollToIndex = 0;
    if (m_iScrollToIndex < 0)
        m_iScrollToIndex = m_matchedItemList.size() - 1;

    setUnderlineItemText(m_matchedItemList[m_iScrollToIndex], true);
    m_pTreeWidget->scrollTo(m_pTreeWidget->itemIndex(m_matchedItemList[m_iScrollToIndex]), QAbstractItemView::PositionAtCenter);
    updateSearchLineEdit(m_matchedItemList.size(), m_iScrollToIndex);
}

void UIMediumSearchWidget::sltShowNextMatchingItem()
{
    if (m_matchedItemList.isEmpty())
        return;
    goToNextPrevious(true);
}

void UIMediumSearchWidget::sltShowPreviousMatchingItem()
{
    goToNextPrevious(false);
}

void UIMediumSearchWidget::updateSearchLineEdit(int iMatchCount, int iScrollToIndex)
{
    if (!m_pSearchTermLineEdit)
        return;
    m_pSearchTermLineEdit->setMatchCount(iMatchCount);
    m_pSearchTermLineEdit->setScroolToIndex(iScrollToIndex);
}

#include "UIMediumSearchWidget.moc"
