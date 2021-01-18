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

class UITest : public QWidget
{
    Q_OBJECT;
public:
    UITest(QWidget *pParent = 0)
        :QWidget(pParent)
    {}
    ~UITest(){}
};

/*********************************************************************************************************************************
*   UIToolTitleWidget definition.                                                                                    *
*********************************************************************************************************************************/

class UIToolBoxTitleWidget : public QWidget
{

    Q_OBJECT;

signals:

    void sigShowPageWidget();

public:

    UIToolBoxTitleWidget(QWidget *pParent = 0);
    void setText(const QString &strText);

private:

    void prepare();
    QHBoxLayout *m_pLayout;
    QLabel      *m_pTitleLabel;
};

/*********************************************************************************************************************************
*   UIToolBoxPage definition.                                                                                    *
*********************************************************************************************************************************/

class UIToolBoxPage : public QWidget
{

    Q_OBJECT;

signals:

    void sigShowPageWidget();

public:

    UIToolBoxPage(QWidget *pParent = 0);
    void setTitle(const QString &strTitle);
    /* @p pWidget's ownership is transferred to the page. */
    void setWidget(QWidget *pWidget);
    void setTitleBackgroundColor(const QColor &color);
    void setPageWidgetVisible(bool fVisible);
    int index() const;
    void setIndex(int iIndex);

protected:

    virtual bool eventFilter(QObject *pWatched, QEvent *pEvent) /* override */;

private:

    void prepare();
    QVBoxLayout *m_pLayout;
    UIToolBoxTitleWidget      *m_pTitleWidget;
    QWidget     *m_pWidget;
    int          m_iIndex;
};

/*********************************************************************************************************************************
*   UIToolTitleWidget implementation.                                                                                    *
*********************************************************************************************************************************/

UIToolBoxTitleWidget::UIToolBoxTitleWidget(QWidget *pParent /* = 0 */)
    :QWidget(pParent)
{
    prepare();
}

void UIToolBoxTitleWidget::setText(const QString &strText)
{
    if (m_pTitleLabel)
        m_pTitleLabel->setText(strText);
}

void UIToolBoxTitleWidget::prepare()
{
    m_pLayout = new QHBoxLayout(this);
    m_pLayout->setContentsMargins(1.f * qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                  .4f * qApp->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                  1.f * qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                  .4f * qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin));

    m_pTitleLabel = new QLabel;
    m_pLayout->addWidget(m_pTitleLabel);
}

/*********************************************************************************************************************************
*   UIToolBoxPage implementation.                                                                                    *
*********************************************************************************************************************************/

UIToolBoxPage::UIToolBoxPage(QWidget *pParent /* = 0 */)
    :QWidget(pParent)
    , m_pLayout(0)
    , m_pTitleWidget(0)
    , m_pWidget(0)
    , m_iIndex(0)

{
    prepare();
}

void UIToolBoxPage::setTitle(const QString &strTitle)
{
    if (!m_pTitleWidget)
        return;
    m_pTitleWidget->setText(strTitle);
}

void UIToolBoxPage::prepare()
{
    m_pLayout = new QVBoxLayout(this);
    m_pLayout->setContentsMargins(0, 0, 0, 0);
    m_pTitleWidget = new UIToolBoxTitleWidget;
    m_pTitleWidget->installEventFilter(this);
    m_pLayout->addWidget(m_pTitleWidget);
}

void UIToolBoxPage::setWidget(QWidget *pWidget)
{
    if (!m_pLayout || !pWidget)
        return;
    m_pWidget = pWidget;
    m_pLayout->addWidget(m_pWidget);
    m_pWidget->hide();
}

void UIToolBoxPage::setTitleBackgroundColor(const QColor &color)
{
    if (!m_pTitleWidget)
        return;
    QPalette palette = m_pTitleWidget->palette();
    palette.setColor(QPalette::Window, color);
    m_pTitleWidget->setPalette(palette);
    m_pTitleWidget->setAutoFillBackground(true);
}

void UIToolBoxPage::setPageWidgetVisible(bool fVisible)
{
    if (m_pWidget)
        m_pWidget->setVisible(fVisible);
}

int UIToolBoxPage::index() const
{
    return m_iIndex;
}

void UIToolBoxPage::setIndex(int iIndex)
{
    m_iIndex = iIndex;
}

bool UIToolBoxPage::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched == m_pTitleWidget && pEvent->type() == QEvent::MouseButtonPress)
        emit sigShowPageWidget();
    return QWidget::eventFilter(pWatched, pEvent);

}

/*********************************************************************************************************************************
*   UIToolBox implementation.                                                                                    *
*********************************************************************************************************************************/

UIToolBox::UIToolBox(QWidget *pParent /*  = 0 */)
    : QIWithRetranslateUI<QFrame>(pParent)
{
    prepare();
}

bool UIToolBox::insertItem(int iIndex, QWidget *pWidget, const QString &strTitle)
{
    if (m_pages.contains(iIndex))
        return false;
    UIToolBoxPage *pNewPage = new UIToolBoxPage;

    pNewPage->setWidget(pWidget);
    pNewPage->setIndex(iIndex);
    pNewPage->setTitle(strTitle);

    const QPalette pal = palette();
    QColor tabBackgroundColor = pal.color(QPalette::Active, QPalette::Highlight).lighter(110);
    pNewPage->setTitleBackgroundColor(tabBackgroundColor);

    m_pages[iIndex] = pNewPage;
    m_pMainLayout->insertWidget(iIndex, pNewPage);

    connect(pNewPage, &UIToolBoxPage::sigShowPageWidget,
            this, &UIToolBox::sltHandleShowPageWidget);

    return iIndex;
}

void UIToolBox::setItemEnabled(int iIndex, bool fEnabled)
{
    Q_UNUSED(fEnabled);
    Q_UNUSED(iIndex);
}

void UIToolBox::setItemText(int iIndex, const QString &strTitle)
{
    QMap<int, UIToolBoxPage*>::iterator iterator = m_pages.find(iIndex);
    if (iterator == m_pages.end())
        return;
    iterator.value()->setTitle(strTitle);
}

void UIToolBox::setItemIcon(int iIndex, const QIcon &icon)
{
    Q_UNUSED(iIndex);
    Q_UNUSED(icon);
}

void UIToolBox::setPageVisible(int iIndex)
{
    QMap<int, UIToolBoxPage*>::iterator iterator = m_pages.find(iIndex);
    if (iterator == m_pages.end())
        return;
    foreach(UIToolBoxPage *pPage, m_pages)
        pPage->setPageWidgetVisible(false);

    iterator.value()->setPageWidgetVisible(true);
}

void UIToolBox::retranslateUi()
{
}

void UIToolBox::prepare()
{
    m_pMainLayout = new QVBoxLayout(this);
    //m_pMainLayout->setContentsMargins(0, 0, 0, 0);

    retranslateUi();
}

void UIToolBox::sltHandleShowPageWidget()
{
    UIToolBoxPage *pPage = qobject_cast<UIToolBoxPage*>(sender());
    if (!pPage)
        return;
    setPageVisible(pPage->index());
}

#include "UIToolBox.moc"
