/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoBrowserBase class implementation.
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
#include <QHeaderView>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QTableView>
#include <QTreeView>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIToolBar.h"
#include "UIVisoBrowserBase.h"


/*********************************************************************************************************************************
*   UILocationSelector definition.                                                                                   *
*********************************************************************************************************************************/

class UILocationSelector : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
public:

    UILocationSelector(QWidget *pParent = 0);
    int  lineEditWidth() const;
    void updateLineEditText(const QString &strText);

signals:

    void sigExpandCollapseTreeView();

protected:

    virtual void retranslateUi() /* override */;
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
    virtual bool eventFilter(QObject *pObj, QEvent *pEvent) /* override */;

private:

    void prepareWidget();
    QLineEdit   *m_pLineEdit;
    QGridLayout *m_pMainLayout;
    QIToolButton *m_pExpandButton;
};

/*********************************************************************************************************************************
*   UILocationSelector implementation.                                                                                   *
*********************************************************************************************************************************/

UILocationSelector::UILocationSelector(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pLineEdit(0)
    , m_pMainLayout(0)
    , m_pExpandButton(0)
{
    prepareWidget();
}

int UILocationSelector::lineEditWidth() const
{
    if (!m_pLineEdit)
        return 0;
    return m_pLineEdit->width();
}

void UILocationSelector::updateLineEditText(const QString &strText)
{
    if (!m_pLineEdit)
        return;
    m_pLineEdit->setText(strText);
}

void UILocationSelector::paintEvent(QPaintEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::paintEvent(pEvent);
}

void UILocationSelector::retranslateUi()
{
}

bool UILocationSelector::eventFilter(QObject *pObj, QEvent *pEvent)
{
    if (pObj == m_pLineEdit)
    {
        if(pEvent->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *pMouseEvent = dynamic_cast<QMouseEvent*>(pEvent);
            if (pMouseEvent && pMouseEvent->button() == Qt::LeftButton)
                emit sigExpandCollapseTreeView();
        }
    }
    /* Pass the events to event system for further processing: */
    return false;
}

void UILocationSelector::prepareWidget()
{
    m_pMainLayout = new QGridLayout;
    if (!m_pMainLayout)
        return;
    m_pMainLayout->setSpacing(0);
    m_pMainLayout->setContentsMargins(0,0,0,0);

    m_pLineEdit = new QLineEdit;
    if (m_pLineEdit)
    {
        m_pMainLayout->addWidget(m_pLineEdit, 0, 0, 1, 4);
        m_pLineEdit->setReadOnly(true);
        m_pLineEdit->installEventFilter(this);
    }

    m_pExpandButton = new QIToolButton;
    if (m_pExpandButton)
    {
        m_pMainLayout->addWidget(m_pExpandButton, 0, 4, 1, 1);
        m_pExpandButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_disabled_16px.png"));
        connect(m_pExpandButton, &QIToolButton::clicked,
                this, &UILocationSelector::sigExpandCollapseTreeView);

    }
    setLayout(m_pMainLayout);
}

/*********************************************************************************************************************************
*   UIVisoBrowserBase implementation.                                                                                   *
*********************************************************************************************************************************/

UIVisoBrowserBase::UIVisoBrowserBase(QWidget *pParent /* = 0 */, QMenu *pMenu /*= 0*/)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pTreeView(0)
    , m_pVerticalToolBar(0)
    , m_pMenu(pMenu)
    , m_pMainLayout(0)
    , m_pLocationSelector(0)
{
}

UIVisoBrowserBase::~UIVisoBrowserBase()
{
}

bool UIVisoBrowserBase::isTreeViewVisible() const
{
    if (!m_pTreeView)
        return false;
    return m_pTreeView->isVisible();
}

void UIVisoBrowserBase::hideTreeView()
{
    if (isTreeViewVisible())
        sltExpandCollapseTreeView();
}

void UIVisoBrowserBase::prepareObjects()
{
    m_pMainLayout = new QGridLayout;
    setLayout(m_pMainLayout);

    if (!m_pMainLayout)
        return;

    m_pMainLayout->setRowStretch(1, 2);
    m_pLocationSelector = new UILocationSelector;
    if (m_pLocationSelector)
    {
        m_pMainLayout->addWidget(m_pLocationSelector, 0, 0, 1, 4);
    }

    m_pTreeView = new QTreeView(this);
    if (m_pTreeView)
    {
        m_pTreeView->hide();
        m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
        //m_pTreeView->setAlternatingRowColors(true);
        m_pTreeView->header()->hide();
        m_pTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        m_pTreeView->setFrameStyle(QFrame::Panel | QFrame::Plain);
        m_pTreeView->installEventFilter(this);
    }

    m_pVerticalToolBar = new UIToolBar;
    if (m_pVerticalToolBar)
    {
        m_pVerticalToolBar->setOrientation(Qt::Vertical);
        m_pMainLayout->addWidget(m_pVerticalToolBar, 0, 5, 6, 1);
    }
}

void UIVisoBrowserBase::prepareConnections()
{
    if (m_pTreeView)
    {
        connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &UIVisoBrowserBase::sltHandleTreeSelectionChanged);
        connect(m_pTreeView, &QTreeView::clicked,
                this, &UIVisoBrowserBase::sltHandleTreeItemClicked);
    }
    if (m_pLocationSelector)
        connect(m_pLocationSelector, &UILocationSelector::sigExpandCollapseTreeView,
                this, &UIVisoBrowserBase::sltExpandCollapseTreeView);
}

void UIVisoBrowserBase::updateLocationSelectorText(const QString &strText)
{
    if (!m_pLocationSelector)
        return;
    m_pLocationSelector->updateLineEditText(strText);
}

void UIVisoBrowserBase::resizeEvent(QResizeEvent *pEvent)
{
    QIWithRetranslateUI<QWidget>::resizeEvent(pEvent);
    if (m_pTreeView)
        updateTreeViewGeometry(m_pTreeView->isVisible());
}

/* Close the tree view when it recieves focus-out and enter key press event: */
bool UIVisoBrowserBase::eventFilter(QObject *pObj, QEvent *pEvent)
{
    if (pObj == m_pTreeView)
    {
        if(pEvent->type() == QEvent::KeyPress)
        {
            QKeyEvent *pKeyEvent = dynamic_cast<QKeyEvent*>(pEvent);
            if (pKeyEvent &&
                (pKeyEvent->key() == Qt::Key_Return ||
                 pKeyEvent->key() == Qt::Key_Enter))
            {
                sltExpandCollapseTreeView();
            }
        }
        else if (pEvent->type() == QEvent::FocusOut)
        {
                sltExpandCollapseTreeView();
        }
    }
    return false;
}

void UIVisoBrowserBase::keyPressEvent(QKeyEvent *pEvent)
{
    if (pEvent->key() == Qt::Key_Escape)
    {
        if (m_pTreeView->isVisible())
            updateTreeViewGeometry(false);

    }
    QIWithRetranslateUI<QWidget>::keyPressEvent(pEvent);
}


void UIVisoBrowserBase::sltHandleTableViewItemDoubleClick(const QModelIndex &index)
{
    tableViewItemDoubleClick(index);
}

void UIVisoBrowserBase::sltHandleTreeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected);
    QModelIndexList indices = selected.indexes();
    if (indices.empty())
        return;
    QModelIndex selectedIndex = indices[0];
    treeSelectionChanged(selectedIndex);
}

void UIVisoBrowserBase::sltHandleTreeItemClicked(const QModelIndex &modelIndex)
{
    if (!m_pTreeView)
        return;
    m_pTreeView->setExpanded(modelIndex, true);
    m_pTreeView->hide();
    emit sigTreeViewVisibilityChanged(m_pTreeView->isVisible());
}

void UIVisoBrowserBase::sltExpandCollapseTreeView()
{
    if (!m_pTreeView)
        return;
    updateTreeViewGeometry(!m_pTreeView->isVisible());
}

void UIVisoBrowserBase::updateTreeViewGeometry(bool fShow)
{
    if (!m_pTreeView)
        return;

    if (!fShow)
    {
        m_pTreeView->hide();
        emit sigTreeViewVisibilityChanged(m_pTreeView->isVisible());
        m_pTreeView->clearFocus();
            return;
    }
    if (!m_pLocationSelector)
        return;

    int iy = m_pLocationSelector->y() + m_pLocationSelector->height();
    int ix = m_pLocationSelector->x();
    int iWidth = m_pLocationSelector->lineEditWidth();
    m_pTreeView-> move(ix, iy);
    m_pTreeView->raise();
    m_pTreeView->resize(iWidth, 0.75 * height());
    m_pTreeView->show();
    m_pTreeView->setFocus();
    emit sigTreeViewVisibilityChanged(m_pTreeView->isVisible());
}

#include "UIVisoBrowserBase.moc"
