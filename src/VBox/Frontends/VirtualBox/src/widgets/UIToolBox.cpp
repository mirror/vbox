/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolBox class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QCheckBox>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UICommon.h"
#include "UIIconPool.h"
#include "UIToolBox.h"
#include "UIWizardNewVM.h"

/*********************************************************************************************************************************
*   UIToolBoxPage definition.                                                                                    *
*********************************************************************************************************************************/

class UIToolBoxPage : public QWidget
{

    Q_OBJECT;

signals:

    void sigShowPageWidget();

public:

    UIToolBoxPage(bool fEnableCheckBoxEnabled = false, QWidget *pParent = 0);
    void setTitle(const QString &strTitle);
    /* @p pWidget's ownership is transferred to the page. */
    void setWidget(QWidget *pWidget);
    void setTitleBackgroundColor(const QColor &color);
    void setExpanded(bool fExpanded);
    int index() const;
    void setIndex(int iIndex);
    int totalHeight() const;
    int titleHeight() const;
    int pageWidgetHeight() const;
    void setTitleIcon(const QIcon &icon, const QString &strToolTip);

protected:

    virtual bool eventFilter(QObject *pWatched, QEvent *pEvent) /* override */;

private slots:

    void sltHandleEnableToggle(int iState);

private:

    void prepare(bool fEnableCheckBoxEnabled);
    void setExpandCollapseIcon();

    bool         m_fExpanded;
    QVBoxLayout *m_pLayout;
    QWidget     *m_pTitleContainerWidget;
    QLabel      *m_pTitleLabel;
    QLabel      *m_pIconLabel;
    QLabel      *m_pExpandCollapseIconLabel;
    QCheckBox   *m_pEnableCheckBox;

    QWidget     *m_pWidget;
    int          m_iIndex;
    bool         m_fExpandCollapseIconVisible;
    QIcon        m_expandCollapseIcon;
};

/*********************************************************************************************************************************
*   UIToolBoxPage implementation.                                                                                    *
*********************************************************************************************************************************/

UIToolBoxPage::UIToolBoxPage(bool fEnableCheckBoxEnabled /* = false */, QWidget *pParent /* = 0 */)
    :QWidget(pParent)
    , m_fExpanded(false)
    , m_pLayout(0)
    , m_pTitleContainerWidget(0)
    , m_pTitleLabel(0)
    , m_pIconLabel(0)
    , m_pExpandCollapseIconLabel(0)
    , m_pEnableCheckBox(0)
    , m_pWidget(0)
    , m_iIndex(0)
    , m_fExpandCollapseIconVisible(true)
{
    prepare(fEnableCheckBoxEnabled);
}

void UIToolBoxPage::setTitle(const QString &strTitle)
{
    if (!m_pTitleLabel)
        return;
    m_pTitleLabel->setText(strTitle);
}

void UIToolBoxPage::prepare(bool fEnableCheckBoxEnabled)
{
    m_expandCollapseIcon = UIIconPool::iconSet(":/expanding_collapsing_16px.png");

    m_pLayout = new QVBoxLayout(this);
    m_pLayout->setContentsMargins(0, 0, 0, 0);

    m_pTitleContainerWidget = new QWidget;
    m_pTitleContainerWidget->installEventFilter(this);
    QHBoxLayout *pTitleLayout = new QHBoxLayout(m_pTitleContainerWidget);
    pTitleLayout->setContentsMargins(qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                     .4f * qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                     qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                     .4f * qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));

    m_pExpandCollapseIconLabel = new QLabel;
    if (m_pExpandCollapseIconLabel)
        pTitleLayout->addWidget(m_pExpandCollapseIconLabel);

    if (fEnableCheckBoxEnabled)
    {
        m_pEnableCheckBox = new QCheckBox;
        pTitleLayout->addWidget(m_pEnableCheckBox);
        connect(m_pEnableCheckBox, &QCheckBox::stateChanged, this, &UIToolBoxPage::sltHandleEnableToggle);
    }

    m_pTitleLabel = new QLabel;
    m_pTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    pTitleLayout->addWidget(m_pTitleLabel);
    m_pIconLabel = new QLabel;
    pTitleLayout->addWidget(m_pIconLabel, Qt::AlignLeft);
    pTitleLayout->addStretch();
    m_pTitleContainerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_pLayout->addWidget(m_pTitleContainerWidget);

    setExpandCollapseIcon();
}

void UIToolBoxPage::setWidget(QWidget *pWidget)
{
    if (!m_pLayout || !pWidget)
        return;
    m_pWidget = pWidget;
    m_pLayout->addWidget(m_pWidget);

    if (m_pEnableCheckBox)
        m_pWidget->setEnabled(m_pEnableCheckBox->checkState() == Qt::Checked);

    m_pWidget->hide();
}

void UIToolBoxPage::setTitleBackgroundColor(const QColor &color)
{
    if (!m_pTitleLabel)
        return;
    QPalette palette = m_pTitleContainerWidget->palette();
    palette.setColor(QPalette::Window, color);
    m_pTitleContainerWidget->setPalette(palette);
    m_pTitleContainerWidget->setAutoFillBackground(true);
}

void UIToolBoxPage::setExpanded(bool fVisible)
{
    if (m_pWidget)
        m_pWidget->setVisible(fVisible);
    m_fExpanded = fVisible;
    setExpandCollapseIcon();
}

int UIToolBoxPage::index() const
{
    return m_iIndex;
}

void UIToolBoxPage::setIndex(int iIndex)
{
    m_iIndex = iIndex;
}

int UIToolBoxPage::totalHeight() const
{
    return pageWidgetHeight() + titleHeight();
}

void UIToolBoxPage::setTitleIcon(const QIcon &icon, const QString &strToolTip)
{
    if (!m_pIconLabel)
        return;
    if (icon.isNull())
    {
        m_pIconLabel->setPixmap(QPixmap());
        return;
    }
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_pIconLabel->setPixmap(icon.pixmap(windowHandle(), QSize(iMetric, iMetric)));
    m_pIconLabel->setToolTip(strToolTip);
}

int UIToolBoxPage::titleHeight() const
{
    if (m_pTitleContainerWidget && m_pTitleContainerWidget->sizeHint().isValid())
        return m_pTitleContainerWidget->sizeHint().height();
    return 0;
}

int UIToolBoxPage::pageWidgetHeight() const
{
    if (m_pWidget && m_pWidget->isVisible() && m_pWidget->sizeHint().isValid())
        return m_pWidget->sizeHint().height();
    return 0;
}

bool UIToolBoxPage::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched == m_pTitleContainerWidget)
    {
        if (pEvent->type() == QEvent::MouseButtonPress)
            emit sigShowPageWidget();
    }
    return QWidget::eventFilter(pWatched, pEvent);

}

void UIToolBoxPage::sltHandleEnableToggle(int iState)
{
    if (m_pWidget)
        m_pWidget->setEnabled(iState == Qt::Checked);
}

void UIToolBoxPage::setExpandCollapseIcon()
{
    if (!m_pExpandCollapseIconLabel)
        return;
    if (!m_fExpandCollapseIconVisible)
    {
        m_pExpandCollapseIconLabel->setVisible(false);
        return;
    }
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    if (m_fExpanded)
    {
        QTransform transform;
        transform.rotate(90);
        m_pExpandCollapseIconLabel->setPixmap(m_expandCollapseIcon.pixmap(windowHandle(), QSize(iMetric, iMetric)).transformed(transform));
    }
    else
    {
        m_pExpandCollapseIconLabel->setPixmap(m_expandCollapseIcon.pixmap(windowHandle(), QSize(iMetric, iMetric)));
    }
}

/*********************************************************************************************************************************
*   UIToolBox implementation.                                                                                    *
*********************************************************************************************************************************/

UIToolBox::UIToolBox(QWidget *pParent /*  = 0 */)
    : QIWithRetranslateUI<QFrame>(pParent)
    , m_iCurrentPageIndex(-1)
    , m_iPageCount(0)
{
    prepare();
}

bool UIToolBox::insertPage(int iIndex, QWidget *pWidget, const QString &strTitle, bool fAddEnableCheckBox /* = false */)
{
    if (m_pages.contains(iIndex))
        return false;

    /* Remove the stretch from the end of the layout: */
    QLayoutItem *pItem = m_pMainLayout->takeAt(m_pMainLayout->count() - 1);
    delete pItem;

    ++m_iPageCount;
    UIToolBoxPage *pNewPage = new UIToolBoxPage(fAddEnableCheckBox, 0);;

    pNewPage->setWidget(pWidget);
    pNewPage->setIndex(iIndex);
    pNewPage->setTitle(strTitle);

    const QPalette pal = palette();
    QColor tabBackgroundColor = pal.color(QPalette::Active, QPalette::Highlight).lighter(130);
    pNewPage->setTitleBackgroundColor(tabBackgroundColor);

    m_pages[iIndex] = pNewPage;
    m_pMainLayout->insertWidget(iIndex, pNewPage);

    connect(pNewPage, &UIToolBoxPage::sigShowPageWidget,
            this, &UIToolBox::sltHandleShowPageWidget);

    static int iMaxPageHeight = 0;
    int iTotalTitleHeight = 0;
    foreach(UIToolBoxPage *pPage, m_pages)
    {
        if (pWidget && pWidget->sizeHint().isValid())
            iMaxPageHeight = qMax(iMaxPageHeight, pWidget->sizeHint().height());
        iTotalTitleHeight += pPage->titleHeight();
    }
    setMinimumHeight(m_iPageCount * (qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin) +
                                     qApp->style()->pixelMetric(QStyle::PM_LayoutBottomMargin)) +
                     iTotalTitleHeight +
                     iMaxPageHeight);
    /* Add stretch at the end: */
    m_pMainLayout->addStretch();
    return iIndex;
}

void UIToolBox::setPageEnabled(int iIndex, bool fEnabled)
{
    Q_UNUSED(fEnabled);
    Q_UNUSED(iIndex);
}

void UIToolBox::setPageTitle(int iIndex, const QString &strTitle)
{
    QMap<int, UIToolBoxPage*>::iterator iterator = m_pages.find(iIndex);
    if (iterator == m_pages.end())
        return;
    iterator.value()->setTitle(strTitle);
}

void UIToolBox::setPageTitleIcon(int iIndex, const QIcon &icon, const QString &strIconToolTip /* = QString() */)
{
    QMap<int, UIToolBoxPage*>::iterator iterator = m_pages.find(iIndex);
    if (iterator == m_pages.end())
        return;
    iterator.value()->setTitleIcon(icon, strIconToolTip);
}

void UIToolBox::setCurrentPage(int iIndex)
{
    m_iCurrentPageIndex = iIndex;
    QMap<int, UIToolBoxPage*>::iterator iterator = m_pages.find(iIndex);
    if (iterator == m_pages.end())
        return;
    foreach(UIToolBoxPage *pPage, m_pages)
        pPage->setExpanded(false);

    iterator.value()->setExpanded(true);
}

void UIToolBox::retranslateUi()
{
}

void UIToolBox::prepare()
{
    m_pMainLayout = new QVBoxLayout(this);
    m_pMainLayout->addStretch();

    retranslateUi();
}

void UIToolBox::sltHandleShowPageWidget()
{
    UIToolBoxPage *pPage = qobject_cast<UIToolBoxPage*>(sender());
    if (!pPage)
        return;
    setCurrentPage(pPage->index());
    update();
}

#include "UIToolBox.moc"
