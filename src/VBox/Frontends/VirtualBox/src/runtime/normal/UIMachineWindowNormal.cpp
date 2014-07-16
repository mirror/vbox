/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowNormal class implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#include <QDesktopWidget>
#include <QMenuBar>
#include <QTimer>
#include <QContextMenuEvent>
#include <QResizeEvent>

#include <QStylePainter>
#include <QStyleOption>
#include <QPaintEvent>
#include <QPainter>
#include <QDrag>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIExtraDataManager.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIIndicatorsPool.h"
#include "UIKeyboardHandler.h"
#include "UIMouseHandler.h"
#include "UIMachineLogic.h"
#include "UIMachineWindowNormal.h"
#include "UIMachineView.h"
#include "QIStatusBar.h"
#include "QIStatusBarIndicator.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# include "UIImageTools.h"
#endif /* Q_WS_MAC */

#include "QIWithRetranslateUI.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIConverter.h"
#include "UIAnimationFramework.h"

/* COM includes: */
#include "CConsole.h"
#include "CMediumAttachment.h"
#include "CUSBController.h"
#include "CUSBDeviceFilters.h"


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
    /** Holds whether button is checked. */
    bool m_fChecked;
    /** Holds whether button is hovered. */
    bool m_fHovered;
    /** Holds the last mouse-press position. */
    QPoint m_mousePressPosition;
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
    /* Cache button size-hint: */
    m_size = icon.availableSizes().first();
    /* Cache pixmap of same size: */
    m_pixmap = icon.pixmap(m_size);

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
    QStyleOption option;
    option.initFrom(this);
    option.rect = QRect(0, 0, width(), height());
    /* Remember checked-state: */
    if (m_fChecked)
        option.state |= QStyle::State_On;
    /* Draw check-box for hovered-state: */
    if (m_fHovered)
        painter.drawPrimitive(QStyle::PE_IndicatorCheckBox, option);
    /* Draw pixmap for unhovered-state: */
    else
        painter.drawItemPixmap(option.rect, Qt::AlignCenter, m_pixmap);
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


/** QWidget reimplementation
  * providing user with possibility to edit status-bar layout. */
class UIStatusBarEditorWindow : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(QRect startGeometry READ startGeometry);
    Q_PROPERTY(QRect finalGeometry READ finalGeometry);

signals:

    /** Notifies about window shown. */
    void sigShown();
    /** Commands window to expand. */
    void sigExpand();
    /** Commands window to collapse. */
    void sigCollapse();

public:

    /** Constructor, passes @a pParent to the QIRichToolButton constructor.
      * @param rect is used to define initial cached parent geometry.
      * @param statusBarRect is used to define initial cached status-bar geometry. */
    UIStatusBarEditorWindow(QWidget *pParent, const QRect &rect, const QRect &statusBarRect);

private slots:

    /** Mark window as expanded. */
    void sltMarkAsExpanded() { m_fExpanded = true; }
    /** Mark window as collapsed. */
    void sltMarkAsCollapsed() { close(); m_fExpanded = false; }

    /** Handles parent geometry change. */
    void sltParentGeometryChanged(const QRect &rect);

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
    /** Prepare animation routine. */
    void prepareAnimation();

    /** Updates status buttons. */
    void updateStatusButtons();
    /** Updates animation. */
    void updateAnimation();
    /** Update geometry. */
    void adjustGeometry();

    /** Retranslation routine. */
    virtual void retranslateUi();

    /** Show event handler. */
    virtual void showEvent(QShowEvent *pEvent);
    /** Close event handler. */
    virtual void closeEvent(QCloseEvent *pEvent);

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

    /** Returns cached start-geometry. */
    QRect startGeometry() const { return m_startGeometry; }
    /** Returns cached final-geometry. */
    QRect finalGeometry() const { return m_finalGeometry; }

    /** @name Geometry
      * @{ */
        /** Holds the cached parent geometry. */
        QRect m_rect;
        /** Holds the cached status-bar geometry. */
        QRect m_statusBarRect;
    /** @} */

    /** @name Geometry: Animation
      * @{ */
        /** Holds the expand/collapse animation instance. */
        UIAnimation *m_pAnimation;
        /** Holds whether window is expanded. */
        bool m_fExpanded;
        /** Holds the cached start-geometry. */
        QRect m_startGeometry;
        /** Holds the cached final-geometry. */
        QRect m_finalGeometry;
    /** @} */

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

UIStatusBarEditorWindow::UIStatusBarEditorWindow(QWidget *pParent, const QRect &rect, const QRect &statusBarRect)
    : QIWithRetranslateUI2<QWidget>(pParent, Qt::Tool | Qt::FramelessWindowHint)
    , m_rect(rect), m_statusBarRect(statusBarRect)
    , m_pAnimation(0), m_fExpanded(false)
    , m_pMainLayout(0), m_pButtonLayout(0)
    , m_pButtonClose(0)
    , m_pButtonDropToken(0)
    , m_fDropAfterTokenButton(true)
{
    /* Prepare: */
    prepare();
}

void UIStatusBarEditorWindow::sltParentGeometryChanged(const QRect &rect)
{
    /* Update rectangle: */
    m_rect = rect;
    /* Update animation: */
    updateAnimation();
    /* Adjust geometry: */
    adjustGeometry();
}

void UIStatusBarEditorWindow::sltHandleConfigurationChange()
{
    /* Update status buttons: */
    updateStatusButtons();
}

void UIStatusBarEditorWindow::sltHandleButtonClick()
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

void UIStatusBarEditorWindow::sltHandleDragObjectDestroy()
{
    /* Reset token: */
    m_pButtonDropToken = 0;
    m_fDropAfterTokenButton = true;
    /* Update: */
    update();
}

void UIStatusBarEditorWindow::prepare()
{
    /* Do not count that window as important for application,
     * it will NOT be taken into account when other top-level windows will be closed: */
    setAttribute(Qt::WA_QuitOnClose, false);
    /* Delete window when closed: */
    setAttribute(Qt::WA_DeleteOnClose);
    /* Make window background translucent: */
    setAttribute(Qt::WA_TranslucentBackground);
    /* Track D&D events: */
    setAcceptDrops(true);

    /* Create main-layout: */
    m_pMainLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(m_pMainLayout);
    {
        /* Configure main-layout: */
        int iLeft, iTop, iRight, iBottom;
        m_pMainLayout->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
        if (iBottom >= 5)
            iBottom -= 5;
        m_pMainLayout->setContentsMargins(iLeft, iTop, iRight, iBottom);
        m_pMainLayout->setSpacing(0);
        /* Create close-button: */
        m_pButtonClose = new QIToolButton;
        AssertPtrReturnVoid(m_pButtonClose);
        {
            /* Configure close-button: */
            m_pButtonClose->setMinimumSize(QSize(1, 1));
            m_pButtonClose->setShortcut(Qt::Key_Escape);
            m_pButtonClose->setIcon(UIIconPool::iconSet(":/ok_16px.png"));
            connect(m_pButtonClose, SIGNAL(clicked(bool)), this, SLOT(close()));
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

    /* Prepare animation: */
    prepareAnimation();

    /* Activate window: */
    activateWindow();
}

void UIStatusBarEditorWindow::prepareStatusButtons()
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

void UIStatusBarEditorWindow::prepareStatusButton(IndicatorType type)
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

void UIStatusBarEditorWindow::prepareAnimation()
{
    /* Prepare geometry animation itself: */
    connect(this, SIGNAL(sigShown()), this, SIGNAL(sigExpand()), Qt::QueuedConnection);
    m_pAnimation = UIAnimation::installPropertyAnimation(this, "geometry", "startGeometry", "finalGeometry",
                                                         SIGNAL(sigExpand()), SIGNAL(sigCollapse()));
    connect(m_pAnimation, SIGNAL(sigStateEnteredStart()), this, SLOT(sltMarkAsCollapsed()));
    connect(m_pAnimation, SIGNAL(sigStateEnteredFinal()), this, SLOT(sltMarkAsExpanded()));
    /* Update animation: */
    updateAnimation();
}

void UIStatusBarEditorWindow::updateStatusButtons()
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

void UIStatusBarEditorWindow::updateAnimation()
{
    /* Calculate geometry animation boundaries
     * based on size-hint and minimum size-hint: */
    const QSize sh = sizeHint();
    const QSize msh = minimumSizeHint();
    m_startGeometry = QRect(m_rect.x(), m_rect.y() + m_rect.height() - m_statusBarRect.height() - msh.height(),
                            qMax(m_rect.width(), msh.width()), msh.height());
    m_finalGeometry = QRect(m_rect.x(), m_rect.y() + m_rect.height() - m_statusBarRect.height() - sh.height(),
                            qMax(m_rect.width(), sh.width()), sh.height());
    m_pAnimation->update();
}

void UIStatusBarEditorWindow::adjustGeometry()
{
    /* Adjust geometry based on size-hint: */
    const QSize sh = sizeHint();
    setGeometry(m_rect.x(), m_rect.y() + m_rect.height() - m_statusBarRect.height() - sh.height(),
                qMax(m_rect.width(), sh.width()), sh.height());
    raise();
}

void UIStatusBarEditorWindow::retranslateUi()
{
    /* Translate close-button: */
    m_pButtonClose->setToolTip(tr("Close"));
}

void UIStatusBarEditorWindow::showEvent(QShowEvent*)
{
    /* If window isn't expanded: */
    if (!m_fExpanded)
    {
        /* Start expand animation: */
        emit sigShown();
    }
}

void UIStatusBarEditorWindow::closeEvent(QCloseEvent *pEvent)
{
    /* If window isn't expanded: */
    if (!m_fExpanded)
    {
        /* Ignore close-event: */
        pEvent->ignore();
        return;
    }

    /* If animation state is Final: */
    const QString strAnimationState = property("AnimationState").toString();
    bool fAnimationComplete = strAnimationState == "Final";
    if (fAnimationComplete)
    {
        /* Ignore close-event: */
        pEvent->ignore();
        /* And start collapse animation: */
        emit sigCollapse();
    }
}

void UIStatusBarEditorWindow::paintEvent(QPaintEvent*)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare palette colors: */
    const QPalette pal = palette();
    QColor color0 = pal.color(QPalette::Window);
    QColor color1 = pal.color(QPalette::Window).lighter(110);
    color1.setAlpha(0);
    QColor color2 = pal.color(QPalette::Window).darker(200);
    QColor color3 = pal.color(QPalette::Window).darker(120);

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

    /* Paint frames: */
    painter.fillRect(QRect(5, 5, width() - 5 * 2, height() - 5), color0);
    painter.fillRect(QRect(0, 0, 5, 5), grad1);
    painter.fillRect(QRect(width() - 5, 0, 5, 5), grad2);
    painter.fillRect(QRect(5, 0, width() - 5 * 2, 5), grad3);
    painter.fillRect(QRect(0, 5, 5, height() - 5), grad4);
    painter.fillRect(QRect(width() - 5, 5, 5, height() - 5), grad5);
    painter.save();
    painter.setPen(color3);
    painter.drawLine(QLine(QPoint(5 + 1, 5 + 1),                      QPoint(width() - 1 - 5 - 1, 5 + 1)));
    painter.drawLine(QLine(QPoint(width() - 1 - 5 - 1, 5 + 1),        QPoint(width() - 1 - 5 - 1, height() - 1)));
    painter.drawLine(QLine(QPoint(width() - 1 - 5 - 1, height() - 1), QPoint(5 + 1, height() - 1)));
    painter.drawLine(QLine(QPoint(5 + 1, height() - 1),               QPoint(5 + 1, 5 + 1)));
    painter.restore();

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

void UIStatusBarEditorWindow::dragEnterEvent(QDragEnterEvent *pEvent)
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

void UIStatusBarEditorWindow::dragMoveEvent(QDragMoveEvent *pEvent)
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

void UIStatusBarEditorWindow::dragLeaveEvent(QDragLeaveEvent*)
{
    /* Reset token: */
    m_pButtonDropToken = 0;
    m_fDropAfterTokenButton = true;
    /* Update: */
    update();
}

void UIStatusBarEditorWindow::dropEvent(QDropEvent *pEvent)
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

int UIStatusBarEditorWindow::position(IndicatorType type) const
{
    int iPosition = 0;
    foreach (const IndicatorType &iteratedType, m_order)
        if (iteratedType == type)
            return iPosition;
        else
            ++iPosition;
    return iPosition;
}


UIMachineWindowNormal::UIMachineWindowNormal(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pIndicatorsPool(0)
{
}

void UIMachineWindowNormal::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineWindow::sltMachineStateChanged();

    /* Update pause and virtualization stuff: */
    updateAppearanceOf(UIVisualElement_PauseStuff | UIVisualElement_FeaturesStuff);
}

void UIMachineWindowNormal::sltMediumChange(const CMediumAttachment &attachment)
{
    /* Update corresponding medium stuff: */
    KDeviceType type = attachment.GetType();
    if (type == KDeviceType_HardDisk)
        updateAppearanceOf(UIVisualElement_HDStuff);
    if (type == KDeviceType_DVD)
        updateAppearanceOf(UIVisualElement_CDStuff);
    if (type == KDeviceType_Floppy)
        updateAppearanceOf(UIVisualElement_FDStuff);
}

void UIMachineWindowNormal::sltUSBControllerChange()
{
    /* Update USB stuff: */
    updateAppearanceOf(UIVisualElement_USBStuff);
}

void UIMachineWindowNormal::sltUSBDeviceStateChange()
{
    /* Update USB stuff: */
    updateAppearanceOf(UIVisualElement_USBStuff);
}

void UIMachineWindowNormal::sltNetworkAdapterChange()
{
    /* Update network stuff: */
    updateAppearanceOf(UIVisualElement_NetworkStuff);
}

void UIMachineWindowNormal::sltSharedFolderChange()
{
    /* Update shared-folders stuff: */
    updateAppearanceOf(UIVisualElement_SharedFolderStuff);
}

void UIMachineWindowNormal::sltVideoCaptureChange()
{
    /* Update video-capture stuff: */
    updateAppearanceOf(UIVisualElement_VideoCapture);
}

void UIMachineWindowNormal::sltCPUExecutionCapChange()
{
    /* Update virtualization stuff: */
    updateAppearanceOf(UIVisualElement_FeaturesStuff);
}

void UIMachineWindowNormal::sltShowStatusBarContextMenu(const QPoint &position)
{
    /* Prepare context-menu: */
    QMenu menu;
    /* Having just one action to configure status-bar: */
    QAction *pAction = menu.addAction(UIIconPool::iconSet(":/vm_settings_16px.png"),
                                      tr("Configure status-bar..."),
                                      this, SLOT(sltOpenStatusBarEditorWindow()));
    pAction->setEnabled(!uisession()->property("StatusBarEditorOpened").toBool());
    /* Execute context-menu: */
    menu.exec(statusBar()->mapToGlobal(position));
}

void UIMachineWindowNormal::sltOpenStatusBarEditorWindow()
{
    /* Prevent user from opening another one editor: */
    uisession()->setProperty("StatusBarEditorOpened", true);
    /* Create status-bar editor: */
    UIStatusBarEditorWindow *pStatusBarEditor =
        new UIStatusBarEditorWindow(this, m_normalGeometry, statusBar()->geometry());
    AssertPtrReturnVoid(pStatusBarEditor);
    {
        /* Configure status-bar editor: */
        connect(this, SIGNAL(sigGeometryChange(const QRect&)),
                pStatusBarEditor, SLOT(sltParentGeometryChanged(const QRect&)));
        connect(pStatusBarEditor, SIGNAL(destroyed(QObject*)),
                this, SLOT(sltStatusBarEditorWindowClosed()));
        /* Show window: */
        pStatusBarEditor->show();
    }
}

void UIMachineWindowNormal::sltStatusBarEditorWindowClosed()
{
    /* Allow user to open editor again: */
    uisession()->setProperty("StatusBarEditorOpened", QVariant());
}

void UIMachineWindowNormal::sltHandleIndicatorContextMenuRequest(IndicatorType indicatorType, const QPoint &position)
{
    /* Determine action depending on indicator-type: */
    UIAction *pAction = 0;
    switch (indicatorType)
    {
        case IndicatorType_HardDisks:     pAction = gActionPool->action(UIActionIndexRuntime_Menu_HardDisks);        break;
        case IndicatorType_OpticalDisks:  pAction = gActionPool->action(UIActionIndexRuntime_Menu_OpticalDevices);   break;
        case IndicatorType_FloppyDisks:   pAction = gActionPool->action(UIActionIndexRuntime_Menu_FloppyDevices);    break;
        case IndicatorType_USB:           pAction = gActionPool->action(UIActionIndexRuntime_Menu_USBDevices);       break;
        case IndicatorType_Network:       pAction = gActionPool->action(UIActionIndexRuntime_Menu_Network);          break;
        case IndicatorType_SharedFolders: pAction = gActionPool->action(UIActionIndexRuntime_Menu_SharedFolders);    break;
        case IndicatorType_VideoCapture:  pAction = gActionPool->action(UIActionIndexRuntime_Menu_VideoCapture);     break;
        case IndicatorType_Mouse:         pAction = gActionPool->action(UIActionIndexRuntime_Menu_MouseIntegration); break;
        case IndicatorType_Keyboard:      pAction = gActionPool->action(UIActionIndexRuntime_Menu_Keyboard);         break;
        default: break;
    }
    /* Raise action's context-menu: */
    if (pAction && pAction->isEnabled())
        pAction->menu()->exec(position);
}

void UIMachineWindowNormal::prepareSessionConnections()
{
    /* Call to base-class: */
    UIMachineWindow::prepareSessionConnections();

    /* Medium change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigMediumChange(const CMediumAttachment &)),
            this, SLOT(sltMediumChange(const CMediumAttachment &)));

    /* USB controller change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigUSBControllerChange()),
            this, SLOT(sltUSBControllerChange()));

    /* USB device state-change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigUSBDeviceStateChange(const CUSBDevice &, bool, const CVirtualBoxErrorInfo &)),
            this, SLOT(sltUSBDeviceStateChange()));

    /* Network adapter change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigNetworkAdapterChange(const CNetworkAdapter &)),
            this, SLOT(sltNetworkAdapterChange()));

    /* Shared folder change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigSharedFolderChange()),
            this, SLOT(sltSharedFolderChange()));

    /* Video capture change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigVideoCaptureChange()),
            this, SLOT(sltVideoCaptureChange()));

    /* CPU execution cap change updater: */
    connect(machineLogic()->uisession(), SIGNAL(sigCPUExecutionCapChange()),
            this, SLOT(sltCPUExecutionCapChange()));
}

void UIMachineWindowNormal::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

#ifndef Q_WS_MAC
    /* Prepare application menu-bar: */
    RuntimeMenuType restrictedMenus = gEDataManager->restrictedRuntimeMenuTypes(vboxGlobal().managedVMUuid());
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    setMenuBar(uisession()->newMenuBar(allowedMenus));
#endif /* !Q_WS_MAC */
}

void UIMachineWindowNormal::prepareStatusBar()
{
    /* Call to base-class: */
    UIMachineWindow::prepareStatusBar();

    /* Create status-bar: */
    setStatusBar(new QIStatusBar);
    AssertPtrReturnVoid(statusBar());
    {
#ifdef Q_WS_WIN
        /* Configure status-bar: */
        statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(statusBar(), SIGNAL(customContextMenuRequested(const QPoint&)),
                this, SLOT(sltShowStatusBarContextMenu(const QPoint&)));
#endif /* Q_WS_WIN */
        /* Create indicator-pool: */
        m_pIndicatorsPool = new UIIndicatorsPool(machineLogic()->uisession());
        AssertPtrReturnVoid(m_pIndicatorsPool);
        {
            /* Configure indicator-pool: */
            connect(m_pIndicatorsPool, SIGNAL(sigContextMenuRequest(IndicatorType, const QPoint&)),
                    this, SLOT(sltHandleIndicatorContextMenuRequest(IndicatorType, const QPoint&)));
            /* Add indicator-pool into status-bar: */
            statusBar()->addPermanentWidget(m_pIndicatorsPool, 0);
        }
    }

#ifdef Q_WS_MAC
    /* For the status-bar on Cocoa: */
    setUnifiedTitleAndToolBarOnMac(true);
#endif /* Q_WS_MAC */
}

void UIMachineWindowNormal::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

#ifdef VBOX_GUI_WITH_CUSTOMIZATIONS1
    /* The background has to go black: */
    QPalette palette(centralWidget()->palette());
    palette.setColor(centralWidget()->backgroundRole(), Qt::black);
    centralWidget()->setPalette(palette);
    centralWidget()->setAutoFillBackground(true);
    setAutoFillBackground(true);
#endif /* VBOX_GUI_WITH_CUSTOMIZATIONS1 */

#ifdef Q_WS_MAC
    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }
#endif /* Q_WS_MAC */
}

void UIMachineWindowNormal::loadSettings()
{
    /* Call to base-class: */
    UIMachineWindow::loadSettings();

    /* Get machine: */
    CMachine m = machine();

    /* Load GUI customizations: */
    {
        VBoxGlobalSettings settings = vboxGlobal().settings();
#ifndef Q_WS_MAC
        menuBar()->setHidden(settings.isFeatureActive("noMenuBar"));
#endif /* !Q_WS_MAC */
        statusBar()->setHidden(settings.isFeatureActive("noStatusBar"));
        if (statusBar()->isHidden())
            m_pIndicatorsPool->setAutoUpdateIndicatorStates(false);
    }

    /* Load window geometry: */
    {
        /* Load extra-data: */
        QRect geo = gEDataManager->machineWindowGeometry(machineLogic()->visualStateType(),
                                                         m_uScreenId, vboxGlobal().managedVMUuid());

        /* If we do have proper geometry: */
        if (!geo.isNull())
        {
            /* If previous machine-state was SAVED: */
            if (m.GetState() == KMachineState_Saved)
            {
                /* Restore window geometry: */
                m_normalGeometry = geo;
                setGeometry(m_normalGeometry);
            }
            /* If previous machine-state was NOT SAVED: */
            else
            {
                /* Restore only window position: */
                m_normalGeometry = QRect(geo.x(), geo.y(), width(), height());
                setGeometry(m_normalGeometry);
                /* And normalize to the optimal-size: */
                normalizeGeometry(false);
            }

            /* Maximize (if necessary): */
            if (gEDataManager->machineWindowShouldBeMaximized(machineLogic()->visualStateType(),
                                                              m_uScreenId, vboxGlobal().managedVMUuid()))
                setWindowState(windowState() | Qt::WindowMaximized);
        }
        /* If we do NOT have proper geometry: */
        else
        {
            /* Get available geometry, for screen with (x,y) coords if possible: */
            QRect availableGeo = !geo.isNull() ? QApplication::desktop()->availableGeometry(QPoint(geo.x(), geo.y())) :
                                                 QApplication::desktop()->availableGeometry(this);

            /* Normalize to the optimal size: */
            normalizeGeometry(true);
            /* Move newly created window to the screen-center: */
            m_normalGeometry = geometry();
            m_normalGeometry.moveCenter(availableGeo.center());
            setGeometry(m_normalGeometry);
        }

        /* Normalize to the optimal size: */
#ifdef Q_WS_X11
        QTimer::singleShot(0, this, SLOT(sltNormalizeGeometry()));
#else /* !Q_WS_X11 */
        normalizeGeometry(true);
#endif /* !Q_WS_X11 */
    }
}

void UIMachineWindowNormal::saveSettings()
{
    /* Save window geometry: */
    {
        gEDataManager->setMachineWindowGeometry(machineLogic()->visualStateType(),
                                                m_uScreenId, m_normalGeometry,
                                                isMaximizedChecked(), vboxGlobal().managedVMUuid());
    }

    /* Call to base-class: */
    UIMachineWindow::saveSettings();
}

bool UIMachineWindowNormal::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::Resize:
        {
            QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
            if (!isMaximizedChecked())
            {
                m_normalGeometry.setSize(pResizeEvent->size());
                emit sigGeometryChange(m_normalGeometry);
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position: */
                updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */
            }
            break;
        }
        case QEvent::Move:
        {
            if (!isMaximizedChecked())
            {
                m_normalGeometry.moveTo(geometry().x(), geometry().y());
                emit sigGeometryChange(m_normalGeometry);
#ifdef VBOX_WITH_DEBUGGER_GUI
                /* Update debugger window position: */
                updateDbgWindows();
#endif /* VBOX_WITH_DEBUGGER_GUI */
            }
            break;
        }
        case QEvent::WindowActivate:
            emit sigGeometryChange(m_normalGeometry);
            break;
        default:
            break;
    }
    return UIMachineWindow::event(pEvent);
}

void UIMachineWindowNormal::showInNecessaryMode()
{
    /* Make sure this window should be shown at all: */
    if (!uisession()->isScreenVisible(m_uScreenId))
        return hide();

    /* Make sure this window is not minimized: */
    if (isMinimized())
        return;

    /* Show in normal mode: */
    show();
}

/**
 * Adjusts machine-window size to correspond current guest screen size.
 * @param fAdjustPosition determines whether is it necessary to adjust position too.
 */
void UIMachineWindowNormal::normalizeGeometry(bool fAdjustPosition)
{
#ifndef VBOX_GUI_WITH_CUSTOMIZATIONS1
    /* Skip if maximized: */
    if (isMaximized())
        return;

    /* Calculate client window offsets: */
    QRect frameGeo = frameGeometry();
    QRect geo = geometry();
    int dl = geo.left() - frameGeo.left();
    int dt = geo.top() - frameGeo.top();
    int dr = frameGeo.right() - geo.right();
    int db = frameGeo.bottom() - geo.bottom();

    /* Get the best size w/o scroll-bars: */
    QSize s = sizeHint();

    /* Resize the frame to fit the contents: */
    s -= size();
    frameGeo.setRight(frameGeo.right() + s.width());
    frameGeo.setBottom(frameGeo.bottom() + s.height());

    /* Adjust position if necessary: */
    if (fAdjustPosition)
    {
        QRegion availableGeo;
        QDesktopWidget *dwt = QApplication::desktop();
        if (dwt->isVirtualDesktop())
            /* Compose complex available region: */
            for (int i = 0; i < dwt->numScreens(); ++i)
                availableGeo += dwt->availableGeometry(i);
        else
            /* Get just a simple available rectangle */
            availableGeo = dwt->availableGeometry(pos());

        frameGeo = VBoxGlobal::normalizeGeometry(frameGeo, availableGeo);
    }

    /* Finally, set the frame geometry: */
    setGeometry(frameGeo.left() + dl, frameGeo.top() + dt,
                frameGeo.width() - dl - dr, frameGeo.height() - dt - db);

#else /* !VBOX_GUI_WITH_CUSTOMIZATIONS1 */
    Q_UNUSED(fAdjustPosition);
#endif /* VBOX_GUI_WITH_CUSTOMIZATIONS1 */
}

void UIMachineWindowNormal::updateAppearanceOf(int iElement)
{
    /* Call to base-class: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* Update machine window content: */
    if (iElement & UIVisualElement_PauseStuff)
    {
        if (!statusBar()->isHidden())
        {
            if (uisession()->isPaused())
                m_pIndicatorsPool->setAutoUpdateIndicatorStates(false);
            else if (uisession()->isRunning())
                m_pIndicatorsPool->setAutoUpdateIndicatorStates(true);
        }
    }
    if (iElement & UIVisualElement_HDStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_HardDisks);
    if (iElement & UIVisualElement_CDStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_OpticalDisks);
    if (iElement & UIVisualElement_FDStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_FloppyDisks);
    if (iElement & UIVisualElement_NetworkStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_Network);
    if (iElement & UIVisualElement_USBStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_USB);
    if (iElement & UIVisualElement_SharedFolderStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_SharedFolders);
    if (iElement & UIVisualElement_VideoCapture)
        m_pIndicatorsPool->updateAppearance(IndicatorType_VideoCapture);
    if (iElement & UIVisualElement_FeaturesStuff)
        m_pIndicatorsPool->updateAppearance(IndicatorType_Features);
}

bool UIMachineWindowNormal::isMaximizedChecked()
{
#ifdef Q_WS_MAC
    /* On the Mac the WindowStateChange signal doesn't seems to be delivered
     * when the user get out of the maximized state. So check this ourself. */
    return ::darwinIsWindowMaximized(this);
#else /* Q_WS_MAC */
    return isMaximized();
#endif /* !Q_WS_MAC */
}

#include "UIMachineWindowNormal.moc"

