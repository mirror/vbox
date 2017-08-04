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
# include <QDrag>
# include <QDragEnterEvent>
# include <QDragMoveEvent>
# include <QDropEvent>
# include <QEvent>
# include <QHBoxLayout>
# include <QLabel>
# include <QMimeData>
# include <QMouseEvent>
# include <QStyleOption>
# include <QPainter>
# ifdef VBOX_WS_MAC
#  include <QStackedLayout>
# endif
# include <QStyle>
# include <QToolButton>

/* GUI includes: */
# include "UIIconPool.h"
# include "UITabBar.h"

# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Forward declarations: */
class QApplication;
class QDrag;
class QEvent;
class QHBoxLayout;
class QLabel;
class QMimeData;
class QMouseEvent;
#ifdef VBOX_WS_MAC
class QStackedLayout;
#endif
class QStyle;
class QStyleOption;
class QToolButton;


/** Our own skinnable implementation of tabs for tab-bar. */
class UITabBarItem : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about item was clicked. */
    void sigClicked(UITabBarItem *pItem);

    /** Notifies about item close button was clicked. */
    void sigCloseClicked(UITabBarItem *pItem);

    /** Notifies about drag-object destruction. */
    void sigDragObjectDestroy();

public:

    /** Holds the mime-type for the D&D system. */
    static const QString MimeType;

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

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

    /** Handles mouse-press @a pEvent. */
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    /** Handles mouse-release @a pEvent. */
    virtual void mouseReleaseEvent(QMouseEvent *pEvent) /* override */;
    /** Handles mouse-move @a pEvent. */
    virtual void mouseMoveEvent(QMouseEvent *pEvent) /* override */;
    /** Handles mouse-enter @a pEvent. */
    virtual void enterEvent(QEvent *pEvent) /* override */;
    /** Handles mouse-leave @a pEvent. */
    virtual void leaveEvent(QEvent *pEvent) /* override */;

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
    QHBoxLayout    *m_pLayout;
#ifdef VBOX_WS_MAC
    /** Holds the stacked layout instance. */
    QStackedLayout *m_pLayoutStacked;
#endif

    /** Holds the icon label instance. */
    QLabel      *m_pLabelIcon;
    /** Holds the name label instance. */
    QLabel      *m_pLabelName;
    /** Holds the close button instance. */
    QToolButton *m_pButtonClose;

    /** Holds the last mouse-press position. */
    QPoint m_mousePressPosition;
};


/*********************************************************************************************************************************
*   Class UITabBarItem implementation.                                                                                           *
*********************************************************************************************************************************/

/* static */
const QString UITabBarItem::MimeType = QString("application/virtualbox;value=TabID");

UITabBarItem::UITabBarItem(const QUuid &uuid, const QIcon &icon /* = QIcon() */, const QString &strName /* = QString() */)
    : m_uuid(uuid)
    , m_icon(icon)
    , m_strName(strName)
    , m_fCurrent(false)
    , m_fHovered(false)
    , m_pLayout(0)
#ifdef VBOX_WS_MAC
    , m_pLayoutStacked(0)
#endif
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

void UITabBarItem::paintEvent(QPaintEvent * /* pEvent */)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare palette colors: */
    const QPalette pal = palette();
#ifdef VBOX_WS_MAC
    const QColor color0 = m_fCurrent ? pal.color(QPalette::Highlight).lighter(110)
                        : m_fHovered ? pal.color(QPalette::Highlight).lighter(120)
                        :              pal.color(QPalette::Window);
#else /* !VBOX_WS_MAC */
    const QColor color0 = m_fCurrent ? pal.color(QPalette::Highlight).lighter(150)
                        : m_fHovered ? pal.color(QPalette::Highlight).lighter(170)
                        :              pal.color(QPalette::Window);
#endif /* !VBOX_WS_MAC */
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

void UITabBarItem::mousePressEvent(QMouseEvent *pEvent)
{
    /* We are interested in left button only: */
    if (pEvent->button() != Qt::LeftButton)
        return QWidget::mousePressEvent(pEvent);

    /* Remember mouse-press position: */
    m_mousePressPosition = pEvent->globalPos();
}

void UITabBarItem::mouseReleaseEvent(QMouseEvent *pEvent)
{
    /* We are interested in left button only: */
    if (pEvent->button() != Qt::LeftButton)
        return QWidget::mouseReleaseEvent(pEvent);

    /* Forget mouse-press position: */
    m_mousePressPosition = QPoint();

    /* Notify listeners about the item was clicked: */
    emit sigClicked(this);
}

void UITabBarItem::mouseMoveEvent(QMouseEvent *pEvent)
{
    /* Make sure item isn't already dragged: */
    if (m_mousePressPosition.isNull())
        return QWidget::mouseMoveEvent(pEvent);

    /* Make sure item is now being dragged: */
    if (QLineF(pEvent->globalPos(), m_mousePressPosition).length() < QApplication::startDragDistance())
        return QWidget::mouseMoveEvent(pEvent);

    /* Revoke hovered state: */
#ifdef VBOX_WS_MAC
    m_pLayoutStacked->setCurrentWidget(m_pLabelIcon);
#endif
    m_fHovered = false;
    /* And call for repaint: */
    update();

    /* Initialize dragging: */
    m_mousePressPosition = QPoint();
    QDrag *pDrag = new QDrag(this);
    connect(pDrag, &QObject::destroyed, this, &UITabBarItem::sigDragObjectDestroy);
    QMimeData *pMimeData = new QMimeData;
    pMimeData->setData(MimeType, uuid().toByteArray());
    pDrag->setMimeData(pMimeData);
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    pDrag->setPixmap(icon().pixmap(iMetric, iMetric));
    pDrag->exec();
}

void UITabBarItem::enterEvent(QEvent *pEvent)
{
    /* Make sure button isn't hovered: */
    if (m_fHovered)
        return QWidget::enterEvent(pEvent);

    /* Invert hovered state: */
#ifdef VBOX_WS_MAC
    m_pLayoutStacked->setCurrentWidget(m_pButtonClose);
#endif
    m_fHovered = true;
    /* And call for repaint: */
    update();
}

void UITabBarItem::leaveEvent(QEvent *pEvent)
{
    /* Make sure button is hovered: */
    if (!m_fHovered)
        return QWidget::enterEvent(pEvent);

    /* Invert hovered state: */
#ifdef VBOX_WS_MAC
    m_pLayoutStacked->setCurrentWidget(m_pLabelIcon);
#endif
    m_fHovered = false;
    /* And call for repaint: */
    update();
}

void UITabBarItem::prepare()
{
    /* Configure self: */
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    /* Create main layout: */
    m_pLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pLayout);
    {
        /* Invent pixel metric: */
        const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        const int iMargin = iMetric / 2;
        const int iSpacing = iMargin / 2;
#ifdef VBOX_WS_MAC
        const int iMetricCloseButton = iMetric * 3 / 4;
#else
        const int iMetricCloseButton = iMetric * 2 / 3;
#endif

        /* Configure layout: */
#ifdef VBOX_WS_MAC
        m_pLayout->setContentsMargins(iMargin + iSpacing, iMargin, iMargin + iSpacing, iMargin);
#else
        m_pLayout->setContentsMargins(iMargin + iSpacing, iMargin, iMargin, iMargin);
#endif
        m_pLayout->setSpacing(iSpacing);

        /* Create icon label: */
        m_pLabelIcon = new QLabel;
        AssertPtrReturnVoid(m_pLabelIcon);
        {
            /* Configure label: */
            m_pLabelIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_pLabelIcon->setPixmap(m_icon.pixmap(iMetric));
        }

        /* Create name label: */
        m_pLabelName = new QLabel;
        AssertPtrReturnVoid(m_pLabelName);
        {
            /* Configure label: */
            m_pLabelName->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_pLabelName->setText(m_strName);
        }

        /* Create close button: */
        m_pButtonClose = new QToolButton;
        AssertPtrReturnVoid(m_pButtonClose);
        {
            /* Configure button: */
            m_pButtonClose->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_pButtonClose->setIconSize(QSize(iMetricCloseButton, iMetricCloseButton));
            m_pButtonClose->setIcon(UIIconPool::iconSet(":/close_16px.png"));
#ifdef VBOX_WS_MAC
            m_pButtonClose->setStyleSheet("QToolButton { border: 0px }");
#else
            m_pButtonClose->setAutoRaise(true);
#endif
            connect(m_pButtonClose, &QToolButton::clicked, this, &UITabBarItem::sltCloseClicked);
        }

#ifdef VBOX_WS_MAC
        /* Create stacked-layout: */
        m_pLayoutStacked = new QStackedLayout(m_pLayout);
        {
            m_pLayoutStacked->setAlignment(Qt::AlignCenter);

            /* Add icon-label and close-button into stacked-layout: */
            m_pLayoutStacked->addWidget(m_pLabelIcon);
            m_pLayoutStacked->addWidget(m_pButtonClose);
            m_pLayoutStacked->setAlignment(m_pLabelIcon, Qt::AlignCenter);
            m_pLayoutStacked->setAlignment(m_pButtonClose, Qt::AlignCenter);

            /* Add stacked-layout into main-layout: */
            m_pLayout->addLayout(m_pLayoutStacked);
        }

        /* Add name-label into main-layout: */
        m_pLayout->addWidget(m_pLabelName);
#else /* !VBOX_WS_MAC */
        /* Add everything into main-layout: */
        m_pLayout->addWidget(m_pLabelIcon);
        m_pLayout->addWidget(m_pLabelName);
        m_pLayout->addWidget(m_pButtonClose);
#endif /* !VBOX_WS_MAC */
    }
}


/*********************************************************************************************************************************
*   Class UITabBar implementation.                                                                                               *
*********************************************************************************************************************************/

UITabBar::UITabBar(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pLayoutMain(0)
    , m_pLayoutTab(0)
    , m_pCurrentItem(0)
    , m_pItemToken(0)
    , m_fDropAfterTokenItem(0)
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
        connect(pItem, &UITabBarItem::sigClicked,           this, &UITabBar::sltHandleMakeChildCurrent);
        connect(pItem, &UITabBarItem::sigCloseClicked,      this, &UITabBar::sltHandleChildClose);
        connect(pItem, &UITabBarItem::sigDragObjectDestroy, this, &UITabBar::sltHandleDragObjectDestroy);
        /* Add item into layout and list: */
        m_pLayoutTab->insertWidget(0, pItem);
        m_aItems.prepend(pItem);
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

void UITabBar::paintEvent(QPaintEvent *pEvent)
{
    /* Call to base-class: */
    QWidget::paintEvent(pEvent);

    /* If we have a token item: */
    if (m_pItemToken)
    {
        /* Prepare painter: */
        QPainter painter(this);

        /* Paint drop token: */
        QStyleOption option;
        option.state |= QStyle::State_Horizontal;
        const QRect geo = m_pItemToken->geometry();
        option.rect = !m_fDropAfterTokenItem
                    ? QRect(geo.topLeft() - QPoint(5, 5),
                            geo.bottomLeft() + QPoint(0, 5))
                    : QRect(geo.topRight() - QPoint(0, 5),
                            geo.bottomRight() + QPoint(5, 5));
        QApplication::style()->drawPrimitive(QStyle::PE_IndicatorToolBarSeparator,
                                             &option, &painter);
    }
}

void UITabBar::dragEnterEvent(QDragEnterEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);

    /* Make sure mime-data format is valid: */
    if (!pMimeData->hasFormat(UITabBarItem::MimeType))
        return;

    /* Accept drag-enter event: */
    pEvent->acceptProposedAction();
}

void UITabBar::dragMoveEvent(QDragMoveEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);

    /* Make sure mime-data format is valid: */
    if (!pMimeData->hasFormat(UITabBarItem::MimeType))
        return;

    /* Reset token: */
    m_pItemToken = 0;
    m_fDropAfterTokenItem = true;

    /* Get event position: */
    const QPoint pos = pEvent->pos();
    /* Search for most suitable item: */
    foreach (UITabBarItem *pItem, m_aItems)
    {
        /* Advance token: */
        m_pItemToken = pItem;
        const QRect geo = m_pItemToken->geometry();
        if (pos.x() < geo.center().x())
        {
            m_fDropAfterTokenItem = false;
            break;
        }
    }

    /* Update: */
    update();
}

void UITabBar::dragLeaveEvent(QDragLeaveEvent * /* pEvent */)
{
    /* Reset token: */
    m_pItemToken = 0;
    m_fDropAfterTokenItem = true;

    /* Update: */
    update();
}

void UITabBar::dropEvent(QDropEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);

    /* Make sure mime-data format is valid: */
    if (!pMimeData->hasFormat(UITabBarItem::MimeType))
        return;

    /* Make sure token-item set: */
    if (!m_pItemToken)
        return;

    /* Determine ID of token-item: */
    const QUuid tokenUuid = m_pItemToken->uuid();
    /* Determine ID of dropped-item: */
    const QUuid droppedUuid = pMimeData->data(UITabBarItem::MimeType);

    /* Make sure these uuids are different: */
    if (droppedUuid == tokenUuid)
        return;

    /* Search for an item with dropped ID: */
    UITabBarItem *pItemDropped = 0;
    foreach (UITabBarItem *pItem, m_aItems)
    {
        if (pItem->uuid() == droppedUuid)
        {
            pItemDropped = pItem;
            break;
        }
    }

    /* Make sure dropped-item found: */
    if (!pItemDropped)
        return;

    /* Remove dropped-item: */
    m_aItems.removeAll(pItemDropped);
    m_pLayoutTab->removeWidget(pItemDropped);
    /* Insert dropped-item into position of token-item: */
    int iPosition = m_aItems.indexOf(m_pItemToken);
    AssertReturnVoid(iPosition != -1);
    if (m_fDropAfterTokenItem)
        ++iPosition;
    m_aItems.insert(iPosition, pItemDropped);
    m_pLayoutTab->insertWidget(iPosition, pItemDropped);
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

void UITabBar::sltHandleDragObjectDestroy()
{
    /* Reset token: */
    m_pItemToken = 0;
    m_fDropAfterTokenItem = true;

    /* Update: */
    update();
}

void UITabBar::prepare()
{
    /* Track D&D events: */
    setAcceptDrops(true);

    /* Create main layout: */
    m_pLayoutMain = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pLayoutMain);
    {
        /* Configure layout: */
        m_pLayoutMain->setSpacing(0);
        m_pLayoutMain->setContentsMargins(0, 0, 0, 0);

        // TODO: Workout stretch at the and as well,
        //       depending on which alignment is set.
        /* Add strech into beginning: */
        m_pLayoutMain->addStretch();

        /* Create tab layout: */
        m_pLayoutTab = new QHBoxLayout;
        AssertPtrReturnVoid(m_pLayoutTab);
        {
            /* Add into layout: */
            m_pLayoutMain->addLayout(m_pLayoutTab);
        }
    }
}

#include "UITabBar.moc"

