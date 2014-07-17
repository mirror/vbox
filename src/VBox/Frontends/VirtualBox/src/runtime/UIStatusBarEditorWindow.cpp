/* $Id$ */
/** @file
 * VBox Qt GUI - UIStatusBarEditorWindow class implementation.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
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
#include <QPainter>
#include <QPixmap>
#include <QDrag>

/* GUI includes: */
#include "UIStatusBarEditorWindow.h"
#include "UIAnimationFramework.h"
#include "UIExtraDataManager.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "QIToolButton.h"
#include "VBoxGlobal.h"
#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* Q_WS_MAC */

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
#if defined(Q_WS_MAC) || defined(Q_WS_WIN)
    /* Make sure we have no background
     * until the first one paint-event: */
    setAttribute(Qt::WA_NoSystemBackground);
#endif /* Q_WS_MAC || Q_WS_WIN */
#if defined(Q_WS_MAC)
    /* Using native API to enable translucent background for the Mac host.
     * - We also want to disable window-shadows which is possible
     *   using Qt::WA_MacNoShadow only since Qt 4.8,
     *   while minimum supported version is 4.7.1 for now: */
    ::darwinSetShowsWindowTransparent(this, true);
#elif defined(Q_WS_WIN)
    /* Using Qt API to enable translucent background:
     * - Under Win host Qt conflicts with 3D stuff (black seamless regions).
     * - Under Mac host Qt doesn't allows to disable window-shadows
     *   until version 4.8, but minimum supported version is 4.7.1 for now.
     * - Under x11 host Qt has it broken with KDE 4.9 (black background): */
    setAttribute(Qt::WA_TranslucentBackground);
#endif /* Q_WS_WIN */
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
    /* Prepare geometry: */
    prepareGeometry();
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

void UIStatusBarEditorWindow::prepareGeometry()
{
    /* Prepare geometry based on minimum size-hint: */
    connect(this, SIGNAL(sigShown()), this, SLOT(sltActivateWindow()), Qt::QueuedConnection);
    const QSize msh = minimumSizeHint();
    setGeometry(m_rect.x(), m_rect.y() + m_rect.height() - m_statusBarRect.height() - msh.height(),
                qMax(m_rect.width(), msh.width()), msh.height());
#ifdef Q_WS_WIN
    raise();
#endif /* Q_WS_WIN */
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
#ifdef Q_WS_WIN
    raise();
#endif /* Q_WS_WIN */
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

    /* Paint frames: */
    painter.fillRect(QRect(5, 5, width() - 5 * 2, height() - 5), color0);
    painter.fillRect(QRect(0, 0, 5, 5), grad1);
    painter.fillRect(QRect(width() - 5, 0, 5, 5), grad2);
    painter.fillRect(QRect(5, 0, width() - 5 * 2, 5), grad3);
    painter.fillRect(QRect(0, 5, 5, height() - 5), grad4);
    painter.fillRect(QRect(width() - 5, 5, 5, height() - 5), grad5);
#if defined(Q_WS_WIN) || defined(Q_WS_X11)
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

#include "UIStatusBarEditorWindow.moc"

