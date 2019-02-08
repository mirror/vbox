/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsScrollBar class implementation.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QSignalTransition>
#include <QState>
#include <QStateMachine>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTimerEvent>

/* GUI includes: */
#include "UIGraphicsButton.h"
#include "UIGraphicsScrollBar.h"
#include "UIIconPool.h"


/** QIGraphicsWidget subclass providing GUI with graphics scroll-bar taken. */
class UIGraphicsScrollBarToken : public QIGraphicsWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about mouse moved to certain @a pos. */
    void sigMouseMoved(const QPointF &pos);

public:

    /** Constructs graphics scroll-bar token passing @a pParent to the base-class. */
    UIGraphicsScrollBarToken(QIGraphicsWidget *pParent = 0);

    /** Returns minimum size-hint. */
    virtual QSizeF minimumSizeHint() const /* override */;

protected:

    /** Performs painting using passed @a pPainter, @a pOptions and optionally specified @a pWidget. */
    virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *pWidget = 0) /* override */;

    /** Handles mouse-press @a pEvent. */
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;
    /** Handles mouse-release @a pEvent. */
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Updates scroll-bar extent value. */
    void updateExtent();

    /** Holds the scroll-bar extent. */
    int  m_iExtent;
};


/*********************************************************************************************************************************
*   Class UIGraphicsScrollBarToken implementation.                                                                               *
*********************************************************************************************************************************/

UIGraphicsScrollBarToken::UIGraphicsScrollBarToken(QIGraphicsWidget *pParent /* = 0 */)
    : QIGraphicsWidget(pParent)
{
    prepare();
}

QSizeF UIGraphicsScrollBarToken::minimumSizeHint() const
{
    return QSizeF(m_iExtent, m_iExtent);
}

void UIGraphicsScrollBarToken::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *)
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    const QPalette pal = palette();
    QColor backgroundColor = pal.color(QPalette::Active, QPalette::Window);

    /* Draw background: */
    pPainter->fillRect(pOptions->rect, backgroundColor);

    /* Restore painter: */
    pPainter->restore();
}

void UIGraphicsScrollBarToken::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::mousePressEvent(pEvent);

    /* Accept event to be able to receive mouse move events: */
    pEvent->accept();
}

void UIGraphicsScrollBarToken::mouseMoveEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::mouseMoveEvent(pEvent);

    /* Let listeners know about our mouse move events. */
    emit sigMouseMoved(mapToParent(pEvent->pos()));
}

void UIGraphicsScrollBarToken::prepare()
{
    setAcceptHoverEvents(true);

    updateExtent();
    resize(minimumSizeHint());
}

void UIGraphicsScrollBarToken::updateExtent()
{
    m_iExtent = QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    updateGeometry();
}


/*********************************************************************************************************************************
*   Class UIGraphicsScrollBar implementation.                                                                                    *
*********************************************************************************************************************************/

UIGraphicsScrollBar::UIGraphicsScrollBar(Qt::Orientation enmOrientation, QGraphicsScene *pScene)
    : m_enmOrientation(enmOrientation)
    , m_iExtent(-1)
    , m_iStep(1)
    , m_iMinimum(0)
    , m_iMaximum(100)
    , m_iValue(0)
    , m_pButton1(0)
    , m_pButton2(0)
    , m_pToken(0)
    , m_fHovered(false)
    , m_iHoverOnTimerId(0)
    , m_iHoverOffTimerId(0)
    , m_iAnimatedValue(0)
{
    pScene->addItem(this);
    prepare();
}

UIGraphicsScrollBar::UIGraphicsScrollBar(Qt::Orientation enmOrientation, QIGraphicsWidget *pParent /* = 0 */)
    : QIGraphicsWidget(pParent)
    , m_enmOrientation(enmOrientation)
    , m_iExtent(-1)
    , m_iStep(1)
    , m_iMinimum(0)
    , m_iMaximum(100)
    , m_iValue(0)
    , m_pButton1(0)
    , m_pButton2(0)
    , m_pToken(0)
    , m_fHovered(false)
    , m_iHoverOnTimerId(0)
    , m_iHoverOffTimerId(0)
    , m_iAnimatedValue(0)
{
    prepare();
}

QSizeF UIGraphicsScrollBar::minimumSizeHint() const
{
    /* Calculate minimum size-hint depending on orientation: */
    switch (m_enmOrientation)
    {
        case Qt::Horizontal: return QSizeF(3 * m_iExtent, m_iExtent);
        case Qt::Vertical:   return QSizeF(m_iExtent, 3 * m_iExtent);
    }

    /* Call to base-class: */
    return QIGraphicsWidget::minimumSizeHint();
}

void UIGraphicsScrollBar::setStep(int iStep)
{
    m_iStep = iStep;
}

int UIGraphicsScrollBar::step() const
{
    return m_iStep;
}

void UIGraphicsScrollBar::setMinimum(int iMinimum)
{
    m_iMinimum = iMinimum;
    if (m_iMaximum < m_iMinimum)
        m_iMaximum = m_iMinimum;
    if (m_iValue < m_iMinimum)
    {
        m_iValue = m_iMinimum;
        emit sigValueChanged(m_iValue);
    }
    layoutToken();
}

int UIGraphicsScrollBar::minimum() const
{
    return m_iMinimum;
}

void UIGraphicsScrollBar::setMaximum(int iMaximum)
{
    m_iMaximum = iMaximum;
    if (m_iMinimum > m_iMaximum)
        m_iMinimum = m_iMaximum;
    if (m_iValue > m_iMaximum)
    {
        m_iValue = m_iMaximum;
        emit sigValueChanged(m_iValue);
    }
    layoutToken();
}

int UIGraphicsScrollBar::maximum() const
{
    return m_iMaximum;
}

void UIGraphicsScrollBar::setValue(int iValue)
{
    if (iValue > m_iMaximum)
        iValue = m_iMaximum;
    if (iValue < m_iMinimum)
        iValue = m_iMinimum;
    m_iValue = iValue;
    emit sigValueChanged(m_iValue);
    layoutToken();
}

int UIGraphicsScrollBar::value() const
{
    return m_iValue;
}

void UIGraphicsScrollBar::resizeEvent(QGraphicsSceneResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::resizeEvent(pEvent);

    /* Layout widgets: */
    layoutWidgets();
}

void UIGraphicsScrollBar::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *)
{
    /* Acquire rectangle: */
    const QRect rectangle = pOptions->rect;
    /* Paint background: */
    paintBackground(pPainter, rectangle);
}

void UIGraphicsScrollBar::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::mousePressEvent(pEvent);

    /* Mark event accepted so that it couldn't
     * influence underlying widgets: */
    pEvent->accept();

    /* Redirect to token move handler: */
    sltTokenMoved(pEvent->pos());
    layoutToken();
}

void UIGraphicsScrollBar::hoverMoveEvent(QGraphicsSceneHoverEvent *)
{
    /* Only if not yet hovered, that way we
     * make sure trigger emitted just once: */
    if (!m_fHovered)
    {
        /* Start hover-on timer, handled in timerEvent() below: */
        m_iHoverOnTimerId = startTimer(400);
        m_fHovered = true;
    }
    /* Update in any case: */
    update();
}

void UIGraphicsScrollBar::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    /* Only if it's still hovered, that way we
     * make sure trigger emitted just once: */
    if (m_fHovered)
    {
        /* Start hover-off timer, handled in timerEvent() below: */
        m_iHoverOffTimerId = startTimer(1000);
        m_fHovered = false;
    }
    /* Update in any case: */
    update();
}

void UIGraphicsScrollBar::timerEvent(QTimerEvent *pEvent)
{
    /* Kill timer in any case: */
    const int iTimerId = pEvent->timerId();
    killTimer(iTimerId);

    /* If that is hover-on timer: */
    if (m_iHoverOnTimerId != 0 && iTimerId == m_iHoverOnTimerId)
    {
        /* Wait for timer no more: */
        m_iHoverOnTimerId = 0;
        /* Emit hover-on trigger if hovered: */
        if (m_fHovered)
            emit sigHoverEnter();
        /* Update in any case: */
        update();
    }

    else

    /* If that is hover-off timer: */
    if (m_iHoverOffTimerId != 0 && iTimerId == m_iHoverOffTimerId)
    {
        /* Wait for timer no more: */
        m_iHoverOffTimerId = 0;
        /* Emit hover-off trigger if not hovered: */
        if (!m_fHovered)
            emit sigHoverLeave();
        /* Update in any case: */
        update();
    }
}

void UIGraphicsScrollBar::sltButton1Clicked()
{
    setValue(value() - m_iStep);
}

void UIGraphicsScrollBar::sltButton2Clicked()
{
    setValue(value() + m_iStep);
}

void UIGraphicsScrollBar::sltTokenMoved(const QPointF &pos)
{
    /* Depending on orientation: */
    double dRatio = 0;
    switch (m_enmOrientation)
    {
        case Qt::Horizontal:
        {
            /* We have to adjust the X coord of the token, leaving Y unchanged: */
            int iX = pos.x();
            const int iMin = m_iExtent;
            const int iMax = size().width() - 2 * m_iExtent;
            if (iX < iMin)
                iX = iMin;
            if (iX > iMax)
                iX = iMax;
            /* We also calculating new ratio to update value same way: */
            dRatio = (double)(iX - iMin) / (iMax - iMin);
            m_pToken->setPos(iX, 0);
            break;
        }
        case Qt::Vertical:
        {
            /* We have to adjust the Y coord of the token, leaving X unchanged: */
            int iY = pos.y();
            const int iMin = m_iExtent;
            const int iMax = size().height() - 2 * m_iExtent;
            if (iY < iMin)
                iY = iMin;
            if (iY > iMax)
                iY = iMax;
            /* We also calculating new ratio to update value same way: */
            dRatio = (double)(iY - iMin) / (iMax - iMin);
            m_pToken->setPos(0, iY);
            break;
        }
    }

    /* Update value according to calculated ratio: */
    m_iValue = dRatio * (m_iMaximum - m_iMinimum) + m_iMinimum;
    emit sigValueChanged(m_iValue);
}

void UIGraphicsScrollBar::sltStateLeftDefault()
{
    m_pButton1->hide();
    m_pButton2->hide();
    m_pToken->hide();
}

void UIGraphicsScrollBar::sltStateLeftHovered()
{
    m_pButton1->hide();
    m_pButton2->hide();
    m_pToken->hide();
}

void UIGraphicsScrollBar::sltStateEnteredDefault()
{
    m_pButton1->hide();
    m_pButton2->hide();
    m_pToken->hide();
}

void UIGraphicsScrollBar::sltStateEnteredHovered()
{
    m_pButton1->show();
    m_pButton2->show();
    m_pToken->show();
}

void UIGraphicsScrollBar::prepare()
{
    /* Configure self: */
    setAcceptHoverEvents(true);

    /* Prepare/layout widgets: */
    prepareWidgets();
    updateExtent();
    layoutWidgets();

    /* Prepare animation: */
    prepareAnimation();
}

void UIGraphicsScrollBar::prepareWidgets()
{
    /* Create buttons depending on orientation: */
    switch (m_enmOrientation)
    {
        case Qt::Horizontal:
        {
            m_pButton1 = new UIGraphicsButton(this, UIIconPool::iconSet(":/arrow_left_10px.png"));
            m_pButton2 = new UIGraphicsButton(this, UIIconPool::iconSet(":/arrow_right_10px.png"));
            break;
        }
        case Qt::Vertical:
        {
            m_pButton1 = new UIGraphicsButton(this, UIIconPool::iconSet(":/arrow_up_10px.png"));
            m_pButton2 = new UIGraphicsButton(this, UIIconPool::iconSet(":/arrow_down_10px.png"));
            break;
        }
    }

    if (m_pButton1)
    {
        /* We use 10px icons, not 16px, let buttons know that: */
        m_pButton1->setIconScaleIndex((double)10 / 16);
        /* Also we want to have buttons react on mouse presses,
         * not mouse releases, this also implies auto-repeat feature: */
        m_pButton1->setClickPolicy(UIGraphicsButton::ClickPolicy_OnPress);
        connect(m_pButton1, &UIGraphicsButton::sigButtonClicked,
                this, &UIGraphicsScrollBar::sltButton1Clicked);
    }
    if (m_pButton2)
    {
        /* We use 10px icons, not 16px, let buttons know that: */
        m_pButton2->setIconScaleIndex((double)10 / 16);
        /* Also we want to have buttons react on mouse presses,
         * not mouse releases, this also implies auto-repeat feature: */
        m_pButton2->setClickPolicy(UIGraphicsButton::ClickPolicy_OnPress);
        connect(m_pButton2, &UIGraphicsButton::sigButtonClicked,
                this, &UIGraphicsScrollBar::sltButton2Clicked);
    }

    /* Create token: */
    m_pToken = new UIGraphicsScrollBarToken(this);
    if (m_pToken)
        connect(m_pToken, &UIGraphicsScrollBarToken::sigMouseMoved,
                this, &UIGraphicsScrollBar::sltTokenMoved);
}

void UIGraphicsScrollBar::prepareAnimation()
{
    /* Create hovering animation machine: */
    QStateMachine *pHoveringMachine = new QStateMachine(this);
    if (pHoveringMachine)
    {
        /* Create 'default' state: */
        QState *pStateDefault = new QState(pHoveringMachine);
        /* Create 'hovered' state: */
        QState *pStateHovered = new QState(pHoveringMachine);

        /* Configure 'default' state: */
        if (pStateDefault)
        {
            /* When we entering default state => we assigning animatedValue to 0: */
            pStateDefault->assignProperty(this, "animatedValue", 0);
            connect(pStateDefault, &QState::propertiesAssigned, this, &UIGraphicsScrollBar::sltStateEnteredDefault);

            /* Add state transitions: */
            QSignalTransition *pDefaultToHovered = pStateDefault->addTransition(this, SIGNAL(sigHoverEnter()), pStateHovered);
            if (pDefaultToHovered)
            {
                connect(pDefaultToHovered, &QSignalTransition::triggered, this, &UIGraphicsScrollBar::sltStateLeftDefault);

                /* Create forward animation: */
                QPropertyAnimation *pHoveringAnimationForward = new QPropertyAnimation(this, "animatedValue", this);
                if (pHoveringAnimationForward)
                {
                    pHoveringAnimationForward->setDuration(200);
                    pHoveringAnimationForward->setStartValue(0);
                    pHoveringAnimationForward->setEndValue(100);

                    /* Add to transition: */
                    pDefaultToHovered->addAnimation(pHoveringAnimationForward);
                }
            }
        }

        /* Configure 'hovered' state: */
        if (pStateHovered)
        {
            /* When we entering hovered state => we assigning animatedValue to 100: */
            pStateHovered->assignProperty(this, "animatedValue", 100);
            connect(pStateHovered, &QState::propertiesAssigned, this, &UIGraphicsScrollBar::sltStateEnteredHovered);

            /* Add state transitions: */
            QSignalTransition *pHoveredToDefault = pStateHovered->addTransition(this, SIGNAL(sigHoverLeave()), pStateDefault);
            if (pHoveredToDefault)
            {
                connect(pHoveredToDefault, &QSignalTransition::triggered, this, &UIGraphicsScrollBar::sltStateLeftHovered);

                /* Create backward animation: */
                QPropertyAnimation *pHoveringAnimationBackward = new QPropertyAnimation(this, "animatedValue", this);
                if (pHoveringAnimationBackward)
                {
                    pHoveringAnimationBackward->setDuration(200);
                    pHoveringAnimationBackward->setStartValue(100);
                    pHoveringAnimationBackward->setEndValue(0);

                    /* Add to transition: */
                    pHoveredToDefault->addAnimation(pHoveringAnimationBackward);
                }
            }
        }

        /* Initial state is 'default': */
        pHoveringMachine->setInitialState(pStateDefault);
        /* Start state-machine: */
        pHoveringMachine->start();
    }
}

void UIGraphicsScrollBar::updateExtent()
{
    /* Make sure extent value is not smaller than the button size: */
    m_iExtent = QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    m_iExtent = qMax(m_iExtent, (int)m_pButton1->minimumSizeHint().width());
    m_iExtent = qMax(m_iExtent, (int)m_pButton2->minimumSizeHint().width());
    updateGeometry();
}

void UIGraphicsScrollBar::layoutWidgets()
{
    layoutButtons();
    layoutToken();
}

void UIGraphicsScrollBar::layoutButtons()
{
    /* We are calculating proper button shift delta, because
     * button size can be smaller than scroll-bar extent value. */

    int iDelta1 = 0;
    if (m_iExtent > m_pButton1->minimumSizeHint().width())
        iDelta1 = (m_iExtent - m_pButton1->minimumSizeHint().width() + 1) / 2;
    m_pButton1->setPos(iDelta1, iDelta1);

    int iDelta2 = 0;
    if (m_iExtent > m_pButton2->minimumSizeHint().width())
        iDelta2 = (m_iExtent - m_pButton2->minimumSizeHint().width() + 1) / 2;
    m_pButton2->setPos(size().width() - m_iExtent + iDelta2, size().height() - m_iExtent + iDelta2);
}

void UIGraphicsScrollBar::layoutToken()
{
    m_pToken->setPos(actualTokenPosition());
}

QPoint UIGraphicsScrollBar::actualTokenPosition() const
{
    /* We calculating ratio on the basis of current/minimum/maximum values: */
    const double dRatio = m_iMaximum > m_iMinimum ? (double)(m_iValue - m_iMinimum) / (m_iMaximum - m_iMinimum) : 0;

    /* Prepare result: */
    QPoint position;

    /* Depending on orientation: */
    switch (m_enmOrientation)
    {
        case Qt::Horizontal:
        {
            /* We have to adjust the X coord of the token, leaving Y unchanged: */
            const int iMin = m_iExtent;
            const int iMax = size().width() - 2 * m_iExtent;
            int iX = dRatio * (iMax - iMin) + iMin;
            position = QPoint(iX, 0);
            break;
        }
        case Qt::Vertical:
        {
            /* We have to adjust the Y coord of the token, leaving X unchanged: */
            const int iMin = m_iExtent;
            const int iMax = size().height() - 2 * m_iExtent;
            int iY = dRatio * (iMax - iMin) + iMin;
            position = QPoint(0, iY);
            break;
        }
    }

    /* Return result: */
    return position;
}

void UIGraphicsScrollBar::paintBackground(QPainter *pPainter, const QRect &rectangle) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare color: */
    const QPalette pal = palette();

    /* Draw background: */
    QColor backgroundColor = pal.color(QPalette::Active, QPalette::Mid);
    backgroundColor.setAlpha(200);
    QRect actualRectangle = rectangle;
    actualRectangle.setLeft(actualRectangle.left() + .9 * actualRectangle.width() * ((double)100 - m_iAnimatedValue) / 100);
    pPainter->fillRect(actualRectangle, backgroundColor);

    /* Emulate token when necessary: */
    if (m_iAnimatedValue < 100)
    {
        QColor tokenColor = pal.color(QPalette::Active, QPalette::Window);
        tokenColor.setAlpha(200);
        QRect tokenRectangle = QRect(actualTokenPosition(), QSize(m_iExtent, m_iExtent));
        tokenRectangle.setLeft(tokenRectangle.left() + .9 * tokenRectangle.width() * ((double)100 - m_iAnimatedValue) / 100);
        pPainter->fillRect(tokenRectangle, tokenColor);
    }

    /* Restore painter: */
    pPainter->restore();
}


#include "UIGraphicsScrollBar.moc"
