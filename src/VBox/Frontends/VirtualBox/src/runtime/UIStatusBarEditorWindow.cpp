/* $Id$ */
/** @file
 * VBox Qt GUI - UIStatusBarEditorWindow class implementation.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
#include <QStylePainter>
#include <QStyleOption>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QStatusBar>
#include <QPainter>
#include <QPixmap>
#include <QDrag>
#include <QList>
#include <QMap>

/* GUI includes: */
#include "UIStatusBarEditorWindow.h"
#include "UIExtraDataManager.h"
#include "UIMachineWindow.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "QIWithRetranslateUI.h"
#include "QIToolButton.h"
#include "VBoxGlobal.h"


/** QWidget extension
  * used as status-bar editor button. */
class UIStatusBarEditorButton : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies about click. */
    void sigClick();

    /** Notifies about drag-object destruction. */
    void sigDragObjectDestroy();

public:

    /** Holds the mime-type for the D&D system. */
    static const QString MimeType;

    /** Constructor for the button of passed @a type. */
    UIStatusBarEditorButton(IndicatorType type);

    /** Returns button type. */
    IndicatorType type() const { return m_type; }

    /** Returns button size-hint. */
    QSize sizeHint() const { return m_size; }

    /** Defines whether button is @a fChecked. */
    void setChecked(bool fChecked);

private:

    /** Retranslation routine. */
    virtual void retranslateUi();

    /** Paint-event handler. */
    virtual void paintEvent(QPaintEvent *pEvent);

    /** Mouse-press event handler. */
    virtual void mousePressEvent(QMouseEvent *pEvent);
    /** Mouse-release event handler. */
    virtual void mouseReleaseEvent(QMouseEvent *pEvent);
    /** Mouse-enter event handler. */
    virtual void enterEvent(QEvent *pEvent);
    /** Mouse-leave event handler. */
    virtual void leaveEvent(QEvent *pEvent);
    /** Mouse-move event handler. */
    virtual void mouseMoveEvent(QMouseEvent *pEvent);

    /** Holds the button type. */
    IndicatorType m_type;
    /** Holds the button size. */
    QSize m_size;
    /** Holds the button pixmap. */
    QPixmap m_pixmap;
    /** Holds the button pixmap size. */
    QSize m_pixmapSize;
    /** Holds whether button is checked. */
    bool m_fChecked;
    /** Holds whether button is hovered. */
    bool m_fHovered;
    /** Holds the last mouse-press position. */
    QPoint m_mousePressPosition;
};


/** QWidget reimplementation
  * used as status-bar editor widget. */
class UIStatusBarEditorWidget : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies about Cancel button click. */
    void sigCancelClicked();

public:

    /** Constructor, passes @a pParent to the QWidget constructor. */
    UIStatusBarEditorWidget(QWidget *pParent = 0);

private slots:

    /** Handles configuration change. */
    void sltHandleConfigurationChange();

    /** Handles button click. */
    void sltHandleButtonClick();

    /** Handles drag object destroy. */
    void sltHandleDragObjectDestroy();

private:

    /** Prepare routine. */
    void prepare();
    /** Prepare status buttons routine. */
    void prepareStatusButtons();
    /** Prepare status button routine. */
    void prepareStatusButton(IndicatorType type);

    /** Updates status buttons. */
    void updateStatusButtons();

    /** Retranslation routine. */
    virtual void retranslateUi();

    /** Paint event handler. */
    virtual void paintEvent(QPaintEvent *pEvent);

    /** Drag-enter event handler. */
    virtual void dragEnterEvent(QDragEnterEvent *pEvent);
    /** Drag-move event handler. */
    virtual void dragMoveEvent(QDragMoveEvent *pEvent);
    /** Drag-leave event handler. */
    virtual void dragLeaveEvent(QDragLeaveEvent *pEvent);
    /** Drop event handler. */
    virtual void dropEvent(QDropEvent *pEvent);

    /** Returns position for passed @a type. */
    int position(IndicatorType type) const;

    /** @name Contents
      * @{ */
        /** Holds the main-layout instance. */
        QHBoxLayout *m_pMainLayout;
        /** Holds the button-layout instance. */
        QHBoxLayout *m_pButtonLayout;
        /** Holds the close-button instance. */
        QIToolButton *m_pButtonClose;
        /** Holds status-bar buttons. */
        QMap<IndicatorType, UIStatusBarEditorButton*> m_buttons;
    /** @} */

    /** @name Contents: Restrictions
      * @{ */
        /** Holds the cached status-bar button restrictions. */
        QList<IndicatorType> m_restrictions;
    /** @} */

    /** @name Contents: Order
      * @{ */
        /** Holds the cached status-bar button order. */
        QList<IndicatorType> m_order;
        /** Holds the token-button to drop dragged-button nearby. */
        UIStatusBarEditorButton *m_pButtonDropToken;
        /** Holds whether dragged-button should be dropped <b>after</b> the token-button. */
        bool m_fDropAfterTokenButton;
    /** @} */
};


/* static */
const QString UIStatusBarEditorButton::MimeType = QString("application/virtualbox;value=IndicatorType");

UIStatusBarEditorButton::UIStatusBarEditorButton(IndicatorType type)
    : m_type(type)
    , m_fChecked(false)
    , m_fHovered(false)
{
    /* Track mouse events: */
    setMouseTracking(true);

    /* Prepare icon for assigned type: */
    const QIcon icon = gpConverter->toIcon(m_type);
    m_pixmapSize = icon.availableSizes().first();
    m_pixmap = icon.pixmap(m_pixmapSize);

    /* Cache button size-hint: */
    QStyleOptionButton option;
    option.initFrom(this);
    const QRect minRect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option);
    m_size = m_pixmapSize.expandedTo(minRect.size());

    /* Translate finally: */
    retranslateUi();
}

void UIStatusBarEditorButton::setChecked(bool fChecked)
{
    /* Update 'checked' state: */
    m_fChecked = fChecked;
    /* Update: */
    update();
}

void UIStatusBarEditorButton::retranslateUi()
{
    /* Translate tool-tip: */
    setToolTip(tr("<nobr><b>Click</b> to toggle indicator presence.</nobr><br>"
                  "<nobr><b>Drag&Drop</b> to change indicator position.</nobr>"));
}

void UIStatusBarEditorButton::paintEvent(QPaintEvent*)
{
    /* Create style-painter: */
    QStylePainter painter(this);
    /* Prepare option set for check-box: */
    QStyleOptionButton option;
    option.initFrom(this);
    /* Use the size of 'this': */
    option.rect = QRect(0, 0, width(), height());
    /* But do not use hover bit of 'this' since
     * we already have another hovered-state representation: */
    if (option.state & QStyle::State_MouseOver)
        option.state &= ~QStyle::State_MouseOver;
    /* Remember checked-state: */
    if (m_fChecked)
        option.state |= QStyle::State_On;
    /* Draw check-box for hovered-state: */
    if (m_fHovered)
        painter.drawControl(QStyle::CE_CheckBox, option);
    /* Draw pixmap for unhovered-state: */
    else
    {
        QRect pixmapRect = QRect(QPoint(0, 0), m_pixmapSize);
        pixmapRect.moveCenter(option.rect.center());
        painter.drawItemPixmap(pixmapRect, Qt::AlignCenter, m_pixmap);
    }
}

void UIStatusBarEditorButton::mousePressEvent(QMouseEvent *pEvent)
{
    /* We are interested in left button only: */
    if (pEvent->button() != Qt::LeftButton)
        return;

    /* Remember mouse-press position: */
    m_mousePressPosition = pEvent->globalPos();
}

void UIStatusBarEditorButton::mouseReleaseEvent(QMouseEvent *pEvent)
{
    /* We are interested in left button only: */
    if (pEvent->button() != Qt::LeftButton)
        return;

    /* Forget mouse-press position: */
    m_mousePressPosition = QPoint();

    /* Notify about click: */
    emit sigClick();
}

void UIStatusBarEditorButton::enterEvent(QEvent*)
{
    /* Make sure button isn't hovered: */
    if (m_fHovered)
        return;

    /* Invert hovered state: */
    m_fHovered = true;
    /* Update: */
    update();
}

void UIStatusBarEditorButton::leaveEvent(QEvent*)
{
    /* Make sure button is hovered: */
    if (!m_fHovered)
        return;

    /* Invert hovered state: */
    m_fHovered = false;
    /* Update: */
    update();
}

void UIStatusBarEditorButton::mouseMoveEvent(QMouseEvent *pEvent)
{
    /* Make sure item isn't already dragged: */
    if (m_mousePressPosition.isNull())
        return QWidget::mouseMoveEvent(pEvent);

    /* Make sure item is really dragged: */
    if (QLineF(pEvent->globalPos(), m_mousePressPosition).length() <
        QApplication::startDragDistance())
        return QWidget::mouseMoveEvent(pEvent);

    /* Revoke hovered state: */
    m_fHovered = false;
    /* Update: */
    update();

    /* Initialize dragging: */
    m_mousePressPosition = QPoint();
    QDrag *pDrag = new QDrag(this);
    connect(pDrag, SIGNAL(destroyed(QObject*)), this, SIGNAL(sigDragObjectDestroy()));
    QMimeData *pMimeData = new QMimeData;
    pMimeData->setData(MimeType, gpConverter->toInternalString(m_type).toLatin1());
    pDrag->setMimeData(pMimeData);
    pDrag->setPixmap(m_pixmap);
    pDrag->exec();
}


UIStatusBarEditorWidget::UIStatusBarEditorWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QWidget>(pParent)
    , m_pMainLayout(0), m_pButtonLayout(0)
    , m_pButtonClose(0)
    , m_pButtonDropToken(0)
    , m_fDropAfterTokenButton(true)
{
    /* Prepare: */
    prepare();
}

void UIStatusBarEditorWidget::sltHandleConfigurationChange()
{
    /* Update status buttons: */
    updateStatusButtons();
}

void UIStatusBarEditorWidget::sltHandleButtonClick()
{
    /* Make sure sender is valid: */
    UIStatusBarEditorButton *pButton = qobject_cast<UIStatusBarEditorButton*>(sender());
    AssertPtrReturnVoid(pButton);

    /* Get sender type: */
    const IndicatorType type = pButton->type();

    /* Load current status-bar indicator restrictions: */
    QList<IndicatorType> restrictions =
        gEDataManager->restrictedStatusBarIndicators(vboxGlobal().managedVMUuid());

    /* Invert restriction for sender type: */
    if (restrictions.contains(type))
        restrictions.removeAll(type);
    else
        restrictions.append(type);

    /* Save updated status-bar indicator restrictions: */
    gEDataManager->setRestrictedStatusBarIndicators(restrictions, vboxGlobal().managedVMUuid());
}

void UIStatusBarEditorWidget::sltHandleDragObjectDestroy()
{
    /* Reset token: */
    m_pButtonDropToken = 0;
    m_fDropAfterTokenButton = true;
    /* Update: */
    update();
}

void UIStatusBarEditorWidget::prepare()
{
    /* Track D&D events: */
    setAcceptDrops(true);

    /* Create main-layout: */
    m_pMainLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    {
        /* Configure main-layout: */
#ifdef Q_WS_MAC
        /* Standard margins on Mac OS X are too big: */
        m_pMainLayout->setContentsMargins(10, 10, 10, 5);
#else /* !Q_WS_MAC */
        /* Standard margins on Windows/X11: */
        int iLeft, iTop, iRight, iBottom;
        m_pMainLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
        if (iBottom >= 5)
            iBottom -= 5;
        m_pMainLayout->setContentsMargins(iLeft, iTop, iRight, iBottom);
#endif /* !Q_WS_MAC */
        m_pMainLayout->setSpacing(0);
        /* Create close-button: */
        m_pButtonClose = new QIToolButton;
        AssertPtrReturnVoid(m_pButtonClose);
        {
            /* Configure close-button: */
            m_pButtonClose->setFocusPolicy(Qt::StrongFocus);
            m_pButtonClose->setMinimumSize(QSize(1, 1));
            m_pButtonClose->setShortcut(Qt::Key_Escape);
            m_pButtonClose->setIcon(UIIconPool::iconSet(":/ok_16px.png"));
            connect(m_pButtonClose, SIGNAL(clicked(bool)), this, SIGNAL(sigCancelClicked()));
            /* Add close-button into main-layout: */
            m_pMainLayout->addWidget(m_pButtonClose);
        }
        /* Insert stretch: */
        m_pMainLayout->addStretch();
        /* Create button-layout: */
        m_pButtonLayout = new QHBoxLayout;
        AssertPtrReturnVoid(m_pButtonLayout);
        {
            /* Configure button-layout: */
            m_pButtonLayout->setContentsMargins(0, 0, 0, 0);
            m_pButtonLayout->setSpacing(5);
            /* Add button-layout into main-layout: */
            m_pMainLayout->addLayout(m_pButtonLayout);
        }
        /* Prepare status buttons: */
        prepareStatusButtons();
    }

    /* Translate contents: */
    retranslateUi();
}

void UIStatusBarEditorWidget::prepareStatusButtons()
{
    /* Create status buttons: */
    for (int i = IndicatorType_Invalid; i < IndicatorType_Max; ++i)
    {
        /* Get current type: */
        const IndicatorType type = (IndicatorType)i;
        /* Skip inappropriate types: */
        if (type == IndicatorType_Invalid || type == IndicatorType_KeyboardExtension)
            continue;
        /* Create status button: */
        prepareStatusButton(type);
    }

    /* Listen for the status-bar configuration changes: */
    connect(gEDataManager, SIGNAL(sigStatusBarConfigurationChange()),
            this, SLOT(sltHandleConfigurationChange()));

    /* Update status buttons: */
    updateStatusButtons();
}

void UIStatusBarEditorWidget::prepareStatusButton(IndicatorType type)
{
    /* Create status button: */
    UIStatusBarEditorButton *pButton = new UIStatusBarEditorButton(type);
    AssertPtrReturnVoid(pButton);
    {
        /* Configure status button: */
        connect(pButton, SIGNAL(sigClick()), this, SLOT(sltHandleButtonClick()));
        connect(pButton, SIGNAL(sigDragObjectDestroy()), this, SLOT(sltHandleDragObjectDestroy()));
        /* Add status button into button-layout: */
        m_pButtonLayout->addWidget(pButton);
        /* Insert status button into map: */
        m_buttons.insert(type, pButton);
    }
}

void UIStatusBarEditorWidget::updateStatusButtons()
{
    /* Recache status-bar configuration: */
    m_restrictions = gEDataManager->restrictedStatusBarIndicators(vboxGlobal().managedVMUuid());
    m_order = gEDataManager->statusBarIndicatorOrder(vboxGlobal().managedVMUuid());
    for (int iType = IndicatorType_Invalid; iType < IndicatorType_Max; ++iType)
        if (iType != IndicatorType_Invalid && iType != IndicatorType_KeyboardExtension &&
            !m_order.contains((IndicatorType)iType))
            m_order << (IndicatorType)iType;

    /* Update configuration for all the status buttons: */
    foreach (const IndicatorType &type, m_order)
    {
        /* Get button: */
        UIStatusBarEditorButton *pButton = m_buttons.value(type);
        /* Update button 'checked' state: */
        pButton->setChecked(!m_restrictions.contains(type));
        /* Make sure it have valid position: */
        const int iWantedIndex = position(type);
        const int iActualIndex = m_pButtonLayout->indexOf(pButton);
        if (iActualIndex != iWantedIndex)
        {
            /* Re-inject button into main-layout at proper position: */
            m_pButtonLayout->removeWidget(pButton);
            m_pButtonLayout->insertWidget(iWantedIndex, pButton);
        }
    }
}

void UIStatusBarEditorWidget::retranslateUi()
{
    /* Translate close-button: */
    m_pButtonClose->setToolTip(tr("Close"));
}

void UIStatusBarEditorWidget::paintEvent(QPaintEvent*)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare palette colors: */
    const QPalette pal = palette();
    QColor color0 = pal.color(QPalette::Window);
    QColor color1 = pal.color(QPalette::Window).lighter(110);
    color1.setAlpha(0);
    QColor color2 = pal.color(QPalette::Window).darker(200);
#if defined(Q_WS_WIN) || defined(Q_WS_X11)
    QColor color3 = pal.color(QPalette::Window).darker(120);
#endif /* Q_WS_WIN || Q_WS_X11 */

    /* Left corner: */
    QRadialGradient grad1(QPointF(5, 5), 5);
    {
        grad1.setColorAt(0, color2);
        grad1.setColorAt(1, color1);
    }
    /* Right corner: */
    QRadialGradient grad2(QPointF(width() - 5, 5), 5);
    {
        grad2.setColorAt(0, color2);
        grad2.setColorAt(1, color1);
    }
    /* Top line: */
    QLinearGradient grad3(QPointF(5, 0), QPointF(5, 5));
    {
        grad3.setColorAt(0, color1);
        grad3.setColorAt(1, color2);
    }
    /* Left line: */
    QLinearGradient grad4(QPointF(0, 5), QPointF(5, 5));
    {
        grad4.setColorAt(0, color1);
        grad4.setColorAt(1, color2);
    }
    /* Right line: */
    QLinearGradient grad5(QPointF(width(), 5), QPointF(width() - 5, 5));
    {
        grad5.setColorAt(0, color1);
        grad5.setColorAt(1, color2);
    }

    /* Paint shape/shadow: */
    painter.fillRect(QRect(5, 5, width() - 5 * 2, height() - 5), color0); // background
    painter.fillRect(QRect(0,           0, 5, 5), grad1); // left corner
    painter.fillRect(QRect(width() - 5, 0, 5, 5), grad2); // right corner
    painter.fillRect(QRect(5, 0, width() - 5 * 2, 5), grad3); // bottom line
    painter.fillRect(QRect(0,           5, 5, height() - 5), grad4); // left line
    painter.fillRect(QRect(width() - 5, 5, 5, height() - 5), grad5); // right line

#if defined(Q_WS_WIN) || defined(Q_WS_X11)
    /* Paint frames: */
    painter.save();
    painter.setPen(color3);
    painter.drawLine(QLine(QPoint(5 + 1, 5 + 1),                      QPoint(width() - 1 - 5 - 1, 5 + 1)));
    painter.drawLine(QLine(QPoint(width() - 1 - 5 - 1, 5 + 1),        QPoint(width() - 1 - 5 - 1, height() - 1)));
    painter.drawLine(QLine(QPoint(width() - 1 - 5 - 1, height() - 1), QPoint(5 + 1, height() - 1)));
    painter.drawLine(QLine(QPoint(5 + 1, height() - 1),               QPoint(5 + 1, 5 + 1)));
    painter.restore();
#endif /* Q_WS_WIN || Q_WS_X11 */

    /* Paint drop token: */
    if (m_pButtonDropToken)
    {
        QStyleOption option;
        option.state |= QStyle::State_Horizontal;
        const QRect geo = m_pButtonDropToken->geometry();
        option.rect = !m_fDropAfterTokenButton ?
                      QRect(geo.topLeft() - QPoint(5, 5),
                            geo.bottomLeft() + QPoint(0, 5)) :
                      QRect(geo.topRight() - QPoint(0, 5),
                            geo.bottomRight() + QPoint(5, 5));
        QApplication::style()->drawPrimitive(QStyle::PE_IndicatorToolBarSeparator,
                                             &option, &painter);
    }
}

void UIStatusBarEditorWidget::dragEnterEvent(QDragEnterEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);
    /* Make sure mime-data format is valid: */
    if (!pMimeData->hasFormat(UIStatusBarEditorButton::MimeType))
        return;

    /* Accept drag-enter event: */
    pEvent->acceptProposedAction();
}

void UIStatusBarEditorWidget::dragMoveEvent(QDragMoveEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);
    /* Make sure mime-data format is valid: */
    if (!pMimeData->hasFormat(UIStatusBarEditorButton::MimeType))
        return;

    /* Reset token: */
    m_pButtonDropToken = 0;
    m_fDropAfterTokenButton = true;

    /* Get event position: */
    const QPoint pos = pEvent->pos();
    /* Search for most suitable button: */
    foreach (const IndicatorType &type, m_order)
    {
        m_pButtonDropToken = m_buttons.value(type);
        const QRect geo = m_pButtonDropToken->geometry();
        if (pos.x() < geo.center().x())
        {
            m_fDropAfterTokenButton = false;
            break;
        }
    }
    /* Update: */
    update();
}

void UIStatusBarEditorWidget::dragLeaveEvent(QDragLeaveEvent*)
{
    /* Reset token: */
    m_pButtonDropToken = 0;
    m_fDropAfterTokenButton = true;
    /* Update: */
    update();
}

void UIStatusBarEditorWidget::dropEvent(QDropEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);
    /* Make sure mime-data format is valid: */
    if (!pMimeData->hasFormat(UIStatusBarEditorButton::MimeType))
        return;

    /* Make sure token-button set: */
    if (!m_pButtonDropToken)
        return;

    /* Determine type of token-button: */
    const IndicatorType tokenType = m_pButtonDropToken->type();
    /* Determine type of dropped-button: */
    const QString strDroppedType =
        QString::fromLatin1(pMimeData->data(UIStatusBarEditorButton::MimeType));
    const IndicatorType droppedType =
        gpConverter->fromInternalString<IndicatorType>(strDroppedType);

    /* Make sure these types are different: */
    if (droppedType == tokenType)
        return;

    /* Load current status-bar indicator order and make sure it's complete: */
    QList<IndicatorType> order =
        gEDataManager->statusBarIndicatorOrder(vboxGlobal().managedVMUuid());
    for (int iType = IndicatorType_Invalid; iType < IndicatorType_Max; ++iType)
        if (iType != IndicatorType_Invalid && iType != IndicatorType_KeyboardExtension &&
            !order.contains((IndicatorType)iType))
            order << (IndicatorType)iType;

    /* Remove type of dropped-button: */
    order.removeAll(droppedType);
    /* Insert type of dropped-button into position of token-button: */
    int iPosition = order.indexOf(tokenType);
    if (m_fDropAfterTokenButton)
        ++iPosition;
    order.insert(iPosition, droppedType);

    /* Save updated status-bar indicator order: */
    gEDataManager->setStatusBarIndicatorOrder(order, vboxGlobal().managedVMUuid());
}

int UIStatusBarEditorWidget::position(IndicatorType type) const
{
    int iPosition = 0;
    foreach (const IndicatorType &iteratedType, m_order)
        if (iteratedType == type)
            return iPosition;
        else
            ++iPosition;
    return iPosition;
}


UIStatusBarEditorWindow::UIStatusBarEditorWindow(UIMachineWindow *pParent)
    : UISlidingToolBar(pParent, pParent->statusBar(), new UIStatusBarEditorWidget, UISlidingToolBar::Position_Bottom)
{
}

#include "UIStatusBarEditorWindow.moc"

