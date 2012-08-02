/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QISplitter class implementation
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
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
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>

/* GUI includes: */
#include "QISplitter.h"

/* A simple shaded line: */
class QIShadeSplitterHandle: public QSplitterHandle
{
    Q_OBJECT;

public:

    QIShadeSplitterHandle(Qt::Orientation orientation, QISplitter *pParent)
        : QSplitterHandle(orientation, pParent)
    {}

protected:

    void paintEvent(QPaintEvent *pEvent)
    {
        QPainter painter(this);
        QLinearGradient gradient;
        QColor color = palette().color(QPalette::Window);
        QGradientStop point1(0, color);
        QGradientStop point2(0.5, color.darker(115));
        QGradientStop point3(1, color);
        QGradientStops stops;
        stops << point1 << point2 << point3;
        gradient.setStops(stops);
        if (orientation() == Qt::Horizontal)
        {
            gradient.setStart(rect().left(), 0);
            gradient.setFinalStop(rect().right(), 0);
        }
        else
        {
            gradient.setStart(0, rect().top());
            gradient.setFinalStop(0, rect().bottom());
        }
        painter.fillRect(pEvent->rect(), gradient);
    }
};

#ifdef RT_OS_DARWIN
/* The Mac OS X version. */
class QIDarwinSplitterHandle: public QSplitterHandle
{
    Q_OBJECT;

public:

    QIDarwinSplitterHandle(Qt::Orientation orientation, QISplitter *pParent)
        : QSplitterHandle(orientation, pParent)
    {}

    QSize sizeHint() const
    {
        QSize parent = QSplitterHandle::sizeHint();
        if (orientation() == Qt::Vertical)
            return parent + QSize(0, 3);
        else
            return QSize(1, parent.height());
    }

protected:

    void paintEvent(QPaintEvent*)
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
};
#endif /* RT_OS_DARWIN */

QISplitter::QISplitter(QWidget *pParent /* = 0 */)
    : QSplitter(pParent)
    , m_fPolished(false)
    , m_type(Shade)
#ifdef Q_WS_MAC
    , m_fHandleGrabbed(false)
#endif /* Q_WS_MAC */
{
    qApp->installEventFilter(this);
}

QISplitter::QISplitter(Qt::Orientation orientation, QWidget *pParent /* = 0 */)
    : QSplitter(orientation, pParent)
    , m_fPolished(false)
    , m_type(Shade)
#ifdef Q_WS_MAC
    , m_fHandleGrabbed(false)
#endif /* Q_WS_MAC */
{
    qApp->installEventFilter (this);
}

bool QISplitter::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched == handle(1))
    {
        switch (pEvent->type())
        {
            case QEvent::MouseButtonDblClick:
                restoreState(m_baseState);
                break;
            default:
                break;
        }
    }
#ifdef Q_WS_MAC
    /* Special handling on the Mac. Cause there the horizontal handle is only 1
     * pixel wide, its hard to catch. Therefor we make some invisible area
     * around the handle and forwarding the mouse events to the handle, if the
     * user presses the left mouse button. */
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
                            /* If we have a handle position hit and the left
                             * button is pressed, start the grabbing. */
                            if (   fMarginHit
                                && pMouseEvent->buttons().testFlag(Qt::LeftButton))
                            {
                                m_fHandleGrabbed = true;
                                setCursor(Qt::SplitHCursor);
                                qApp->postEvent(pHandle, new QMouseEvent(newME));
                                return true;
                            }
                        }
                        else if(pEvent->type() == QEvent::MouseMove)
                        {
                            /* If we are in the near of the handle or currently
                             * dragging, forward the mouse event. */
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
#endif /* Q_WS_MAC */

    return QSplitter::eventFilter(pWatched, pEvent);
}

void QISplitter::showEvent(QShowEvent *pEvent)
{
    if (!m_fPolished)
    {
        m_fPolished = true;
        m_baseState = saveState();
    }

    return QSplitter::showEvent(pEvent);
}

QSplitterHandle* QISplitter::createHandle()
{
    if (m_type == Native)
    {
#ifdef RT_OS_DARWIN
        return new QIDarwinSplitterHandle(orientation(), this);
#else /* RT_OS_DARWIN */
        return new QSplitterHandle(orientation(), this);
#endif /* RT_OS_DARWIN */
    }
    else
        return new QIShadeSplitterHandle(orientation(), this);
}

#include "QISplitter.moc"

