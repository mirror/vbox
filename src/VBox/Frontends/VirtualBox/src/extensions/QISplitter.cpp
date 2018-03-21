/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: QISplitter class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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
# include <QPainter>
# include <QPaintEvent>

/* GUI includes: */
# include "QISplitter.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QSplitterHandle subclass representing shaded line. */
class QIShadeSplitterHandle : public QSplitterHandle
{
    Q_OBJECT;

public:

    /** Constructs shaded splitter handle passing @a enmOrientation and @a pParent to the base-class. */
    QIShadeSplitterHandle(Qt::Orientation enmOrientation, QISplitter *pParent);

    /** Defines colors to passed @a color1 and @a color2. */
    void configureColors(const QColor &color1, const QColor &color2);

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private:

    /** Holds the main color. */
    QColor m_color;
    /** Holds the color1. */
    QColor m_color1;
    /** Holds the color2. */
    QColor m_color2;
};


#ifdef VBOX_WS_MAC
/** QSplitterHandle subclass representing shaded line for macOS. */
class QIDarwinSplitterHandle : public QSplitterHandle
{
    Q_OBJECT;

public:

    /** Constructs shaded splitter handle passing @a enmOrientation and @a pParent to the base-class. */
    QIDarwinSplitterHandle(Qt::Orientation enmOrientation, QISplitter *pParent);

    /** Returns size-hint. */
    QSize sizeHint() const;

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
};
#endif /* VBOX_WS_MAC */


/*********************************************************************************************************************************
*   Class QIShadeSplitterHandle implementation.                                                                                  *
*********************************************************************************************************************************/

QIShadeSplitterHandle::QIShadeSplitterHandle(Qt::Orientation enmOrientation, QISplitter *pParent)
    : QSplitterHandle(enmOrientation, pParent)
{
    QPalette pal = qApp->palette();
    QColor windowColor = pal.color(QPalette::Active, QPalette::Window);
    QColor darkColor = pal.color(QPalette::Active, QPalette::Dark);
    m_color1 = windowColor;
    m_color2 = windowColor;
    m_color = darkColor;
}

void QIShadeSplitterHandle::configureColors(const QColor &color1, const QColor &color2)
{
    m_color1 = color1;
    m_color2 = color2;
    update();
}

void QIShadeSplitterHandle::paintEvent(QPaintEvent *pEvent)
{
    QPainter painter(this);
    QLinearGradient gradient;
    QGradientStop point1(0, m_color1);
    QGradientStop point2(0.5, m_color);
    QGradientStop point3(1, m_color2);
    QGradientStops stops;
    stops << point1 << point2 << point3;
    gradient.setStops(stops);
    if (orientation() == Qt::Horizontal)
    {
        gradient.setStart(rect().left() + 1, 0);
        gradient.setFinalStop(rect().right(), 0);
    }
    else
    {
        gradient.setStart(0, rect().top() + 1);
        gradient.setFinalStop(0, rect().bottom());
    }
    painter.fillRect(pEvent->rect(), gradient);
}


/*********************************************************************************************************************************
*   Class QIDarwinSplitterHandle implementation.                                                                                 *
*********************************************************************************************************************************/

#ifdef VBOX_WS_MAC

QIDarwinSplitterHandle::QIDarwinSplitterHandle(Qt::Orientation enmOrientation, QISplitter *pParent)
    : QSplitterHandle(enmOrientation, pParent)
{
}

QSize QIDarwinSplitterHandle::sizeHint() const
{
    QSize parent = QSplitterHandle::sizeHint();
    if (orientation() == Qt::Vertical)
        return parent + QSize(0, 3);
    else
        return QSize(1, parent.height());
}

void QIDarwinSplitterHandle::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QColor topColor(145, 145, 145);
    QColor bottomColor(142, 142, 142);
    QColor gradientStart(252, 252, 252);
    QColor gradientStop(223, 223, 223);

    if (orientation() == Qt::Vertical)
    {
        painter.setPen(topColor);
        painter.drawLine(0, 0, width(), 0);
        painter.setPen(bottomColor);
        painter.drawLine(0, height() - 1, width(), height() - 1);

        QLinearGradient linearGrad(QPointF(0, 0), QPointF(0, height() -3));
        linearGrad.setColorAt(0, gradientStart);
        linearGrad.setColorAt(1, gradientStop);
        painter.fillRect(QRect(QPoint(0,1), size() - QSize(0, 2)), QBrush(linearGrad));
    }
    else
    {
        painter.setPen(topColor);
        painter.drawLine(0, 0, 0, height());
    }
}

#endif /* VBOX_WS_MAC */


/*********************************************************************************************************************************
*   Class QISplitter  implementation.                                                                                            *
*********************************************************************************************************************************/

QISplitter::QISplitter(QWidget *pParent /* = 0 */)
    : QSplitter(pParent)
    , m_type(Shade)
    , m_fPolished(false)
#ifdef VBOX_WS_MAC
    , m_fHandleGrabbed(false)
#endif /* VBOX_WS_MAC */
{
    qApp->installEventFilter(this);
}

QISplitter::QISplitter(Qt::Orientation enmOrientation, QWidget *pParent /* = 0 */)
    : QSplitter(enmOrientation, pParent)
    , m_type(Shade)
    , m_fPolished(false)
#ifdef VBOX_WS_MAC
    , m_fHandleGrabbed(false)
#endif /* VBOX_WS_MAC */
{
    qApp->installEventFilter (this);
}

bool QISplitter::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Handles events for handle: */
    if (pWatched == handle(1))
    {
        switch (pEvent->type())
        {
            /* Restore default position on double-click: */
            case QEvent::MouseButtonDblClick:
                restoreState(m_baseState);
                break;
            default:
                break;
        }
    }

#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // Special handling on the Mac. Cause there the horizontal handle is only 1
    // pixel wide, its hard to catch. Therefor we make some invisible area
    // around the handle and forwarding the mouse events to the handle, if the
    // user presses the left mouse button.
    else if (   m_type == Native
             && orientation() == Qt::Horizontal
             && count() > 1
             && qApp->activeWindow() == window())
    {
        switch (pEvent->type())
        {
            case QEvent::MouseButtonPress:
            case QEvent::MouseMove:
            {
                const int margin = 3;
                QMouseEvent *pMouseEvent = static_cast<QMouseEvent*>(pEvent);
                for (int i=1; i < count(); ++i)
                {
                    QWidget *pHandle = handle(i);
                    if (   pHandle
                        && pHandle != pWatched)
                    {
                        /* Create new mouse event with translated mouse positions. */
                        QMouseEvent newME(pMouseEvent->type(),
                                          pHandle->mapFromGlobal(pMouseEvent->globalPos()),
                                          pMouseEvent->button(),
                                          pMouseEvent->buttons(),
                                          pMouseEvent->modifiers());
                        /* Check if we hit the handle */
                        bool fMarginHit = QRect(pHandle->mapToGlobal(QPoint(0, 0)), pHandle->size()).adjusted(-margin, 0, margin, 0).contains(pMouseEvent->globalPos());
                        if (pEvent->type() == QEvent::MouseButtonPress)
                        {
                            /* If we have a handle position hit and the left button is pressed, start the grabbing. */
                            if (   fMarginHit
                                && pMouseEvent->buttons().testFlag(Qt::LeftButton))
                            {
                                m_fHandleGrabbed = true;
                                setCursor(Qt::SplitHCursor);
                                qApp->postEvent(pHandle, new QMouseEvent(newME));
                                return true;
                            }
                        }
                        else if (pEvent->type() == QEvent::MouseMove)
                        {
                            /* If we are in the near of the handle or currently dragging, forward the mouse event. */
                            if (   fMarginHit
                                || (   m_fHandleGrabbed
                                    && pMouseEvent->buttons().testFlag(Qt::LeftButton)))
                            {
                                setCursor(Qt::SplitHCursor);
                                qApp->postEvent(pHandle, new QMouseEvent(newME));
                                return true;
                            }
                            else
                            {
                                /* If not, reset the state. */
                                m_fHandleGrabbed = false;
                                setCursor(Qt::ArrowCursor);
                            }
                        }
                    }
                }
                break;
            }
            case QEvent::WindowDeactivate:
            case QEvent::MouseButtonRelease:
            {
                m_fHandleGrabbed = false;
                setCursor(Qt::ArrowCursor);
                break;
            }
            default:
                break;
        }
    }
#endif /* VBOX_WS_MAC */

    /* Call to base-class: */
    return QSplitter::eventFilter(pWatched, pEvent);
}

void QISplitter::showEvent(QShowEvent *pEvent)
{
    /* Remember default position: */
    if (!m_fPolished)
    {
        m_fPolished = true;
        m_baseState = saveState();
    }

    /* Call to base-class: */
    return QSplitter::showEvent(pEvent);
}

QSplitterHandle *QISplitter::createHandle()
{
    /* Create native handle: */
    if (m_type == Native)
    {
#ifdef VBOX_WS_MAC
        return new QIDarwinSplitterHandle(orientation(), this);
#else
        return new QSplitterHandle(orientation(), this);
#endif
    }
    /* Create our own handle: */
    else
    {
        QIShadeSplitterHandle *pHandle = new QIShadeSplitterHandle(orientation(), this);
        if (m_color1.isValid() && m_color2.isValid())
            pHandle->configureColors(m_color1, m_color2);
        return pHandle;
    }
}

#include "QISplitter.moc"

