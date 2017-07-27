/* $Id$ */
/** @file
 * VBox Qt GUI - UITabBar class implementation.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QApplication>
# include <QEvent>
# include <QHBoxLayout>
# include <QLabel>
# include <QPainter>
# include <QStyle>

/* GUI includes: */
# include "QIToolButton.h"
# include "UIIconPool.h"
# include "UITabBar.h"

# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Forward declarations: */
class QApplication;
class QEvent;
class QHBoxLayout;
class QLabel;
class QStyle;


/** Our own skinnable implementation of tabs for tab-bar. */
class UITabBarItem : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about item was clicked. */
    void sigClicked(UITabBarItem *pItem);

    /** Notifies about item close button was clicked. */
    void sigCloseClicked(UITabBarItem *pItem);

public:

    /** Creates tab-bar item on the basis of passed @a uuid, @a icon and @a strName. */
    UITabBarItem(const QUuid &uuid, const QIcon &icon = QIcon(), const QString &strName = QString());

    /** Returns item ID. */
    const QUuid uuid() const { return m_uuid; }
    /** Returns item icon. */
    const QIcon icon() const { return m_icon; }
    /** Returns item name. */
    const QString name() const { return m_strName; }

    /** Marks item @a fCurrent. */
    void setCurrent(bool fCurrent);

protected:

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private slots:

    /** Handles close button click. */
    void sltCloseClicked() { emit sigCloseClicked(this); }

private:

    /** Prepares all. */
    void prepare();

    /** Holds the item ID. */
    const QUuid  m_uuid;
    /** Holds the item icon. */
    QIcon        m_icon;
    /** Holds the item name. */
    QString      m_strName;

    /** Holds whether the item is current. */
    bool  m_fCurrent;
    /** Holds whether the item is hovered. */
    bool  m_fHovered;

    /** Holds the main layout instance. */
    QHBoxLayout  *m_pLayout;
    /** Holds the icon label instance. */
    QLabel       *m_pLabelIcon;
    /** Holds the name label instance. */
    QLabel       *m_pLabelName;
    /** Holds the close button instance. */
    QIToolButton *m_pButtonClose;
};


/*********************************************************************************************************************************
*   Class UITabBarItem implementation.                                                                                           *
*********************************************************************************************************************************/

UITabBarItem::UITabBarItem(const QUuid &uuid, const QIcon &icon /* = QIcon() */, const QString &strName /* = QString() */)
    : m_uuid(uuid)
    , m_icon(icon)
    , m_strName(strName)
    , m_fCurrent(false)
    , m_fHovered(false)
    , m_pLayout(0)
    , m_pLabelIcon(0)
    , m_pLabelName(0)
    , m_pButtonClose(0)
{
    /* Prepare: */
    prepare();
}

void UITabBarItem::setCurrent(bool fCurrent)
{
    /* Remember the state: */
    m_fCurrent = fCurrent;
    /* And call for repaint: */
    update();
}

bool UITabBarItem::event(QEvent *pEvent)
{
    /* Handle known event types: */
    switch (pEvent->type())
    {
        /* Update the hovered state on/off: */
        case QEvent::Enter: m_fHovered = true; update(); break;
        case QEvent::Leave: m_fHovered = false; update(); break;

        /* Notify listeners about the item was clicked: */
        case QEvent::MouseButtonRelease: emit sigClicked(this); break;

        default: break;
    }
    /* Call to base-class: */
    return QWidget::event(pEvent);
}

void UITabBarItem::paintEvent(QPaintEvent * /* pEvent */)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare palette colors: */
    const QPalette pal = palette();
    const QColor color0 = m_fCurrent ? pal.color(QPalette::Highlight).lighter(200)
                        : m_fHovered ? pal.color(QPalette::Highlight).lighter(220)
                        :              pal.color(QPalette::Window);
    QColor color1 = pal.color(QPalette::Window).lighter(110);
    color1.setAlpha(0);
    QColor color2 = pal.color(QPalette::Window).darker(200);

    /* Invent pixel metric: */
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

    /* Top-left corner: */
    QRadialGradient grad1(QPointF(iMetric, iMetric), iMetric);
    {
        grad1.setColorAt(0, color2);
        grad1.setColorAt(1, color1);
    }
    /* Top-right corner: */
    QRadialGradient grad2(QPointF(width() - iMetric, iMetric), iMetric);
    {
        grad2.setColorAt(0, color2);
        grad2.setColorAt(1, color1);
    }
    /* Bottom-left corner: */
    QRadialGradient grad3(QPointF(iMetric, height() - iMetric), iMetric);
    {
        grad3.setColorAt(0, color2);
        grad3.setColorAt(1, color1);
    }
    /* Botom-right corner: */
    QRadialGradient grad4(QPointF(width() - iMetric, height() - iMetric), iMetric);
    {
        grad4.setColorAt(0, color2);
        grad4.setColorAt(1, color1);
    }

    /* Top line: */
    QLinearGradient grad5(QPointF(iMetric, 0), QPointF(iMetric, iMetric));
    {
        grad5.setColorAt(0, color1);
        grad5.setColorAt(1, color2);
    }
    /* Bottom line: */
    QLinearGradient grad6(QPointF(iMetric, height()), QPointF(iMetric, height() - iMetric));
    {
        grad6.setColorAt(0, color1);
        grad6.setColorAt(1, color2);
    }
    /* Left line: */
    QLinearGradient grad7(QPointF(0, height() - iMetric), QPointF(iMetric, height() - iMetric));
    {
        grad7.setColorAt(0, color1);
        grad7.setColorAt(1, color2);
    }
    /* Right line: */
    QLinearGradient grad8(QPointF(width(), height() - iMetric), QPointF(width() - iMetric, height() - iMetric));
    {
        grad8.setColorAt(0, color1);
        grad8.setColorAt(1, color2);
    }

    /* Paint shape/shadow: */
    painter.fillRect(QRect(iMetric,           iMetric,            width() - iMetric * 2, height() - iMetric * 2), color0);
    painter.fillRect(QRect(0,                 0,                  iMetric,               iMetric),                grad1);
    painter.fillRect(QRect(width() - iMetric, 0,                  iMetric,               iMetric),                grad2);
    painter.fillRect(QRect(0,                 height() - iMetric, iMetric,               iMetric),                grad3);
    painter.fillRect(QRect(width() - iMetric, height() - iMetric, iMetric,               iMetric),                grad4);
    painter.fillRect(QRect(iMetric,           0,                  width() - iMetric * 2, iMetric),                grad5);
    painter.fillRect(QRect(iMetric,           height() - iMetric, width() - iMetric * 2, iMetric),                grad6);
    painter.fillRect(QRect(0,                 iMetric,            iMetric,               height() - iMetric * 2), grad7);
    painter.fillRect(QRect(width() - iMetric, iMetric,            iMetric,               height() - iMetric * 2), grad8);
}

void UITabBarItem::prepare()
{
    /* Create main layout: */
    m_pLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pLayout);
    {
        /* Invent pixel metric: */
        const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        const int iMargin = iMetric / 2;
        const int iSpacing = iMargin / 2;
        const int iMetricCloseButton = iMetric * 2 / 3;

        /* Configure layout: */
        m_pLayout->setContentsMargins(iMargin, 0, iMargin, 0);
        m_pLayout->setSpacing(iSpacing);

        /* Create icon label: */
        m_pLabelIcon = new QLabel;
        AssertPtrReturnVoid(m_pLabelIcon);
        {
            /* Configure label: */
            m_pLabelIcon->setPixmap(m_icon.pixmap(iMetric));

            /* Add into layout: */
            m_pLayout->addWidget(m_pLabelIcon);
        }

        /* Create name label: */
        m_pLabelName = new QLabel;
        AssertPtrReturnVoid(m_pLabelName);
        {
            /* Configure label: */
            m_pLabelName->setText(m_strName);

            /* Add into layout: */
            m_pLayout->addWidget(m_pLabelName);
        }

        /* Create close button: */
        m_pButtonClose = new QIToolButton;
        AssertPtrReturnVoid(m_pButtonClose);
        {
            /* Configure button: */
            m_pButtonClose->setIconSize(QSize(iMetricCloseButton, iMetricCloseButton));
            m_pButtonClose->setIcon(UIIconPool::iconSet(":/close_16px.png"));
            connect(m_pButtonClose, &QIToolButton::clicked, this, &UITabBarItem::sltCloseClicked);

            /* Add into layout: */
            m_pLayout->addWidget(m_pButtonClose);
        }
    }
}


/*********************************************************************************************************************************
*   Class UITabBar implementation.                                                                                               *
*********************************************************************************************************************************/

UITabBar::UITabBar(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pLayout(0)
    , m_pCurrentItem(0)
{
    /* Prepare: */
    prepare();
}

QUuid UITabBar::addTab(const QIcon &icon /* = QIcon() */, const QString &strName /* = QString() */)
{
    /* Generate unique ID: */
    const QUuid uuid = QUuid::createUuid();
    /* Create new tab item: */
    UITabBarItem *pItem = new UITabBarItem(uuid, icon, strName);
    AssertPtrReturn(pItem, QUuid());
    {
        /* Configure item: */
        connect(pItem, &UITabBarItem::sigClicked,      this, &UITabBar::sltHandleMakeChildCurrent);
        connect(pItem, &UITabBarItem::sigCloseClicked, this, &UITabBar::sltHandleChildClose);
        /* Add item into layout and list: */
        m_pLayout->addWidget(pItem);
        m_aItems << pItem;
        /* Return unique ID: */
        return uuid;
    }
}

bool UITabBar::removeTab(const QUuid &uuid)
{
    /* Prepare result: */
    bool fSuccess = false;

    /* Do we need to bother about current item? */
    bool fMoveCurrent = m_pCurrentItem->uuid() == uuid;

    /* Search through all the items we have: */
    for (int i = 0; i < m_aItems.size(); ++i)
    {
        /* Get iterated item: */
        UITabBarItem *pItem = m_aItems.at(i);
        /* If that item is what we are looking for: */
        if (pItem->uuid() == uuid)
        {
            /* Delete it and wipe it from the list: */
            delete pItem;
            m_aItems[i] = 0;
            fSuccess = true;
        }
    }
    /* Flush wiped out items: */
    m_aItems.removeAll(0);

    /* If we had removed current item: */
    if (fMoveCurrent)
    {
        /* Mark it null initially: */
        m_pCurrentItem = 0;
        /* But choose something suitable if we have: */
        if (!m_aItems.isEmpty())
            sltHandleMakeChildCurrent(m_aItems.first());
    }

    /* Return result: */
    return fSuccess;
}

bool UITabBar::setCurrent(const QUuid &uuid)
{
    /* Prepare result: */
    bool fSuccess = false;

    /* Search through all the items we have: */
    for (int i = 0; i < m_aItems.size(); ++i)
    {
        /* Get iterated item: */
        UITabBarItem *pItem = m_aItems.at(i);
        /* If that item is what we are looking for: */
        if (pItem->uuid() == uuid)
        {
            /* Make it current: */
            sltHandleMakeChildCurrent(pItem);
            fSuccess = true;
            break;
        }
    }

    /* Return result: */
    return fSuccess;
}

void UITabBar::sltHandleMakeChildCurrent(UITabBarItem *pItem)
{
    /* Make sure item exists: */
    AssertPtrReturnVoid(pItem);

    /* Remove current mark from current item if exists: */
    if (m_pCurrentItem)
        m_pCurrentItem->setCurrent(false);

    /* Assign new current item: */
    m_pCurrentItem = pItem;

    /* Place current mark onto current item if exists: */
    if (m_pCurrentItem)
        m_pCurrentItem->setCurrent(true);

    /* Notify listeners: */
    emit sigCurrentTabChanged(pItem->uuid());
}

void UITabBar::sltHandleChildClose(UITabBarItem *pItem)
{
    /* Make sure item exists: */
    AssertPtrReturnVoid(pItem);

    /* Notify listeners: */
    emit sigTabRequestForClosing(pItem->uuid());
}

void UITabBar::prepare()
{
    /* Create main layout: */
    m_pLayout = new QHBoxLayout(this);
    if (m_pLayout)
    {
        /* Configure layout: */
        m_pLayout->setSpacing(0);
        m_pLayout->setContentsMargins(5, 5, 5, 5);

        // TODO: Workout stretch at the and as well,
        //       depending on which alignment is set.
        /* Add strech into beginning: */
        m_pLayout->addStretch();
    }
}

#include "UITabBar.moc"

