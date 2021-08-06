/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationCenter class implementation.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#include <QApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QSignalTransition>
#include <QState>
#include <QStateMachine>
#include <QStyle>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UINotificationCenter.h"
#include "UINotificationObjectItem.h"
#include "UINotificationModel.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/** QScrollArea extension to make notification scroll-area more versatile. */
class UINotificationScrollArea : public QScrollArea
{
    Q_OBJECT;

public:

    /** Creates notification scroll-area passing @a pParent to the base-class. */
    UINotificationScrollArea(QWidget *pParent = 0);

    /** Returns minimum size-hint. */
    virtual QSize minimumSizeHint() const /* override final */;

    /** Assigns scrollable @a pWidget.
      * @note  Keep in mind that's an override, but NOT a virtual method. */
    void setWidget(QWidget *pWidget);

protected:

    /** Preprocesses @a pEvent for registered @a pWatched object. */
    virtual bool eventFilter(QObject *pWatched, QEvent *pEvent) /* override final */;
};


/*********************************************************************************************************************************
*   Class UINotificationScrollArea implementation.                                                                               *
*********************************************************************************************************************************/

UINotificationScrollArea::UINotificationScrollArea(QWidget *pParent /* = 0 */)
    : QScrollArea(pParent)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

QSize UINotificationScrollArea::minimumSizeHint() const
{
    /* So, here is the logic,
     * we are taking width from widget if it's present,
     * while keeping height calculated by the base-class. */
    const QSize msh = QScrollArea::minimumSizeHint();
    return widget() ? QSize(widget()->minimumSizeHint().width(), msh.height()) : msh;
}

void UINotificationScrollArea::setWidget(QWidget *pWidget)
{
    /* We'd like to listen for a new widget's events: */
    if (widget())
        widget()->removeEventFilter(this);
    pWidget->installEventFilter(this);

    /* Call to base-class: */
    QScrollArea::setWidget(pWidget);
}

bool UINotificationScrollArea::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* For listened widget: */
    if (pWatched == widget())
    {
        switch (pEvent->type())
        {
            /* We'd like to handle layout-request events: */
            case QEvent::LayoutRequest:
                updateGeometry();
                break;
            default:
                break;
        }
    }

    /* Call to base-class: */
    return QScrollArea::eventFilter(pWatched, pEvent);
}


/*********************************************************************************************************************************
*   Class UINotificationCenter implementation.                                                                                   *
*********************************************************************************************************************************/

/* static */
UINotificationCenter *UINotificationCenter::s_pInstance = 0;

/* static */
void UINotificationCenter::create(QWidget *pParent)
{
    AssertReturnVoid(!s_pInstance);
    new UINotificationCenter(pParent);
}

/* static */
void UINotificationCenter::destroy()
{
    AssertPtrReturnVoid(s_pInstance);
    delete s_pInstance;
}

/* static */
UINotificationCenter *UINotificationCenter::instance()
{
    return s_pInstance;
}

void UINotificationCenter::invoke()
{
    emit sigOpen();
}

QUuid UINotificationCenter::append(UINotificationObject *pObject)
{
    /* Open if not yet: */
    if (!m_pOpenButton->isChecked())
        m_pOpenButton->animateClick();

    AssertPtrReturn(m_pModel, QUuid());
    return m_pModel->appendObject(pObject);
}

void UINotificationCenter::revoke(const QUuid &uId)
{
    AssertReturnVoid(!uId.isNull());
    return m_pModel->revokeObject(uId);
}

UINotificationCenter::UINotificationCenter(QWidget *pParent)
    : QWidget(pParent)
    , m_pModel(0)
    , m_pLayoutMain(0)
    , m_pLayoutOpenButton(0)
    , m_pOpenButton(0)
    , m_pLayoutItems(0)
    , m_pStateMachineSliding(0)
    , m_iAnimatedValue(0)
{
    s_pInstance = this;
    prepare();
}

UINotificationCenter::~UINotificationCenter()
{
    s_pInstance = 0;
}

bool UINotificationCenter::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* For parent object only: */
    if (pObject == parent())
    {
        /* Handle required event types: */
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* When parent being resized we want
                 * to adjust overlay accordingly. */
                adjustGeometry();
                break;
            }
            default:
                break;
        }
    }

    /* Call to base-class: */
    return QWidget::eventFilter(pObject, pEvent);
}

bool UINotificationCenter::event(QEvent *pEvent)
{
    /* Handle required event types: */
    switch (pEvent->type())
    {
        /* When we are being asked to update layout
         * we want to adjust overlay accordingly. */
        case QEvent::LayoutRequest:
        {
            adjustGeometry();
            break;
        }
        /* When we are being resized or moved we want
         * to adjust transparency mask accordingly. */
        case QEvent::Move:
        case QEvent::Resize:
        {
            adjustMask();
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QWidget::event(pEvent);
}

void UINotificationCenter::paintEvent(QPaintEvent *pEvent)
{
    /* Sanity check: */
    AssertPtrReturnVoid(pEvent);

    /* Prepare painter: */
    QPainter painter(this);

    /* Limit painting with incoming rectangle: */
    painter.setClipRect(pEvent->rect());

    /* Paint background: */
    paintBackground(&painter);
    paintFrame(&painter);
}

void UINotificationCenter::sltHandleOpenButtonToggled(bool fToggled)
{
    if (fToggled)
        emit sigOpen();
    else
        emit sigClose();
}

void UINotificationCenter::sltModelChanged()
{
    /* Cleanup layout first: */
    while (QLayoutItem *pChild = m_pLayoutItems->takeAt(0))
    {
        delete pChild->widget();
        delete pChild;
    }

    /* Populate model contents again: */
    foreach (const QUuid &uId, m_pModel->ids())
        m_pLayoutItems->addWidget(UINotificationItem::create(this, m_pModel->objectById(uId)));

    /* Since there is a scroll-area expanded up to whole
     * height, we will have to align items added above up. */
    m_pLayoutItems->addStretch();
}

void UINotificationCenter::prepare()
{
    /* Listen for parent events: */
    parent()->installEventFilter(this);

    /* Prepare the rest of stuff: */
    prepareModel();
    prepareWidgets();
    prepareStateMachineSliding();
}

void UINotificationCenter::prepareModel()
{
    m_pModel = new UINotificationModel(this);
    if (m_pModel)
        connect(m_pModel, &UINotificationModel::sigChanged,
                this, &UINotificationCenter::sltModelChanged);
}

void UINotificationCenter::prepareWidgets()
{
    /* Prepare main layout: */
    m_pLayoutMain = new QVBoxLayout(this);
    if (m_pLayoutMain)
    {
        /* Prepare open-button layout: */
        m_pLayoutOpenButton = new QHBoxLayout;
        if (m_pLayoutOpenButton)
        {
            m_pLayoutOpenButton->setContentsMargins(0, 0, 0, 0);

            /* Prepare open-button: */
            m_pOpenButton = new QIToolButton(this);
            if (m_pOpenButton)
            {
                m_pOpenButton->setIcon(UIIconPool::iconSet(":/reset_warnings_16px.png"));
                m_pOpenButton->setCheckable(true);
                connect(m_pOpenButton, &QIToolButton::toggled, this, &UINotificationCenter::sltHandleOpenButtonToggled);
                m_pLayoutOpenButton->addWidget(m_pOpenButton);
            }

            m_pLayoutOpenButton->addStretch();

            /* Add to layout: */
            m_pLayoutMain->addLayout(m_pLayoutOpenButton);
        }

        /* Create items scroll-area: */
        UINotificationScrollArea *pScrollAreaItems = new UINotificationScrollArea(this);
        if (pScrollAreaItems)
        {
            /* Prepare items widget: */
            QWidget *pWidgetItems = new QWidget(pScrollAreaItems);
            if (pWidgetItems)
            {
                /* Prepare items layout: */
                m_pLayoutItems = new QVBoxLayout(pWidgetItems);
                if (m_pLayoutItems)
                    m_pLayoutItems->setContentsMargins(0, 0, 0, 0);

                /* Add to scroll-area: */
                pScrollAreaItems->setWidget(pWidgetItems);
            }

            /* Configure items scroll-area: */
            pScrollAreaItems->setWidgetResizable(true);
            pScrollAreaItems->setFrameShape(QFrame::NoFrame);
            pScrollAreaItems->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
            pScrollAreaItems->viewport()->setAutoFillBackground(false);
            pScrollAreaItems->widget()->setAutoFillBackground(false);

            /* Add to layout: */
            m_pLayoutMain->addWidget(pScrollAreaItems);
        }
    }
}

void UINotificationCenter::prepareStateMachineSliding()
{
    /* Create sliding animation state-machine: */
    m_pStateMachineSliding = new QStateMachine(this);
    if (m_pStateMachineSliding)
    {
        /* Create 'closed' state: */
        QState *pStateClosed = new QState(m_pStateMachineSliding);
        /* Create 'opened' state: */
        QState *pStateOpened = new QState(m_pStateMachineSliding);

        /* Configure 'closed' state: */
        if (pStateClosed)
        {
            /* When we entering closed state => we assigning animatedValue to 0: */
            pStateClosed->assignProperty(this, "animatedValue", 0);

            /* Add state transitions: */
            QSignalTransition *pClosedToOpened = pStateClosed->addTransition(this, SIGNAL(sigOpen()), pStateOpened);
            if (pClosedToOpened)
            {
                /* Create forward animation: */
                QPropertyAnimation *pAnimationForward = new QPropertyAnimation(this, "animatedValue", this);
                if (pAnimationForward)
                {
                    pAnimationForward->setEasingCurve(QEasingCurve::InCubic);
                    pAnimationForward->setDuration(300);
                    pAnimationForward->setStartValue(0);
                    pAnimationForward->setEndValue(100);

                    /* Add to transition: */
                    pClosedToOpened->addAnimation(pAnimationForward);
                }
            }
        }

        /* Configure 'opened' state: */
        if (pStateOpened)
        {
            /* When we entering opened state => we assigning animatedValue to 100: */
            pStateOpened->assignProperty(this, "animatedValue", 100);

            /* Add state transitions: */
            QSignalTransition *pOpenedToClosed = pStateOpened->addTransition(this, SIGNAL(sigClose()), pStateClosed);
            if (pOpenedToClosed)
            {
                /* Create backward animation: */
                QPropertyAnimation *pAnimationBackward = new QPropertyAnimation(this, "animatedValue", this);
                if (pAnimationBackward)
                {
                    pAnimationBackward->setEasingCurve(QEasingCurve::InCubic);
                    pAnimationBackward->setDuration(300);
                    pAnimationBackward->setStartValue(100);
                    pAnimationBackward->setEndValue(0);

                    /* Add to transition: */
                    pOpenedToClosed->addAnimation(pAnimationBackward);
                }
            }
        }

        /* Initial state is 'closed': */
        m_pStateMachineSliding->setInitialState(pStateClosed);
        /* Start state-machine: */
        m_pStateMachineSliding->start();
    }
}

void UINotificationCenter::paintBackground(QPainter *pPainter)
{
    /* Acquire palette: */
    const bool fActive = parentWidget() && parentWidget()->isActiveWindow();
    const QPalette pal = QApplication::palette();

    /* Gather suitable color: */
    QColor backgroundColor = pal.color(fActive ? QPalette::Active : QPalette::Inactive, QPalette::Window).darker(120);
    backgroundColor.setAlpha((double)animatedValue() / 100 * 220);

    /* Acquire pixel metric: */
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

    /* Adjust rectangle: */
    QRect rectAdjusted = rect();
    rectAdjusted.adjust(iMetric, iMetric, 0, -iMetric);

    /* Paint background: */
    pPainter->fillRect(rectAdjusted, backgroundColor);
}

void UINotificationCenter::paintFrame(QPainter *pPainter)
{
    /* Acquire palette: */
    const bool fActive = parentWidget() && parentWidget()->isActiveWindow();
    QPalette pal = QApplication::palette();

    /* Gather suitable colors: */
    QColor color1 = pal.color(fActive ? QPalette::Active : QPalette::Inactive, QPalette::Window).lighter(110);
    color1.setAlpha(0);
    QColor color2 = pal.color(fActive ? QPalette::Active : QPalette::Inactive, QPalette::Window).darker(200);

    /* Acquire pixel metric: */
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

    /* Top-left corner: */
    QRadialGradient grad1(QPointF(iMetric, iMetric), iMetric);
    {
        grad1.setColorAt(0, color2);
        grad1.setColorAt(1, color1);
    }
    /* Bottom-left corner: */
    QRadialGradient grad2(QPointF(iMetric, height() - iMetric), iMetric);
    {
        grad2.setColorAt(0, color2);
        grad2.setColorAt(1, color1);
    }

    /* Top line: */
    QLinearGradient grad3(QPointF(iMetric, 0), QPointF(iMetric, iMetric));
    {
        grad3.setColorAt(0, color1);
        grad3.setColorAt(1, color2);
    }
    /* Bottom line: */
    QLinearGradient grad4(QPointF(iMetric, height()), QPointF(iMetric, height() - iMetric));
    {
        grad4.setColorAt(0, color1);
        grad4.setColorAt(1, color2);
    }
    /* Left line: */
    QLinearGradient grad5(QPointF(0, height() - iMetric), QPointF(iMetric, height() - iMetric));
    {
        grad5.setColorAt(0, color1);
        grad5.setColorAt(1, color2);
    }

    /* Paint shape/shadow: */
    pPainter->fillRect(QRect(0,       0,                  iMetric,           iMetric),                grad1);
    pPainter->fillRect(QRect(0,       height() - iMetric, iMetric,           iMetric),                grad2);
    pPainter->fillRect(QRect(iMetric, 0,                  width() - iMetric, iMetric),                grad3);
    pPainter->fillRect(QRect(iMetric, height() - iMetric, width() - iMetric, iMetric),                grad4);
    pPainter->fillRect(QRect(0,       iMetric,            iMetric,           height() - iMetric * 2), grad5);
}

void UINotificationCenter::setAnimatedValue(int iValue)
{
    m_iAnimatedValue = iValue;
    adjustGeometry();
}

int UINotificationCenter::animatedValue() const
{
    return m_iAnimatedValue;
}

void UINotificationCenter::adjustGeometry()
{
    /* Acquire parent width and height: */
    QWidget *pParent = parentWidget();
    const int iParentWidth = pParent->width();
    const int iParentHeight = pParent->height();

    /* Acquire minimum width (includes margins by default): */
    int iMinimumWidth = minimumSizeHint().width();
    /* Acquire minimum button width (including margins manually): */
    int iL, iT, iR, iB;
    m_pLayoutMain->getContentsMargins(&iL, &iT, &iR, &iB);
    const int iMinimumButtonWidth = m_pOpenButton->minimumSizeHint().width() + iL + iR;

    /* Make sure we have some default width if there is no contents: */
    iMinimumWidth = qMax(iMinimumWidth, 200);

    /* Move and resize notification-center finally: */
    move(iParentWidth - (iMinimumButtonWidth + (double)animatedValue() / 100 * (iMinimumWidth - iMinimumButtonWidth)), 0);
    resize(iMinimumWidth, iParentHeight);
}

void UINotificationCenter::adjustMask()
{
    /* We do include open-button mask only if center is opened or animated to be: */
    QRegion region;
    if (!m_iAnimatedValue)
        region += QRect(m_pOpenButton->mapToParent(QPoint(0, 0)), m_pOpenButton->size());
    setMask(region);
}


#include "UINotificationCenter.moc"
