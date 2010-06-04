/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QISplitter class implementation
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>

/* Local includes */
#include "QISplitter.h"

/* A simple shaded line. */
class QIShadeSplitterHandle: public QSplitterHandle
{
    Q_OBJECT;

public:

    QIShadeSplitterHandle(Qt::Orientation aOrientation, QISplitter *aParent)
      :QSplitterHandle(aOrientation, aParent)
    {}

protected:

    void paintEvent(QPaintEvent *aEvent)
    {
        QPainter painter(this);
        QLinearGradient gradient;
        if (orientation() == Qt::Horizontal)
        {
            gradient.setStart(rect().left(), rect().height() / 2);
            gradient.setFinalStop(rect().right(), rect().height() / 2);
        }
        else
        {
            gradient.setStart(rect().width() / 2, rect().top());
            gradient.setFinalStop(rect().width() / 2, rect().bottom());
        }
        painter.fillRect(aEvent->rect(), QBrush (gradient));
    }
};

#ifdef RT_OS_DARWIN
/* The Mac OS X version. */
class QIDarwinSplitterHandle: public QSplitterHandle
{
    Q_OBJECT;

public:

    QIDarwinSplitterHandle(Qt::Orientation aOrientation, QISplitter *aParent)
      :QSplitterHandle(aOrientation, aParent)
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

    void paintEvent(QPaintEvent * /* aEvent */)
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
        }else
        {
            painter.setPen(topColor);
            painter.drawLine(0, 0, 0, height());
        }
    }
};
#endif /* RT_OS_DARWIN */

QISplitter::QISplitter(QWidget *aParent /* = 0 */)
    : QSplitter(aParent)
    , mPolished(false)
    , m_type(Shade)
{
    qApp->installEventFilter (this);
}

QISplitter::QISplitter(Qt::Orientation orientation /* = Qt::Horizontal */, QWidget *pParent /* = 0 */)
    : QSplitter(orientation, pParent)
    , mPolished(false)
    , m_type(Shade)
{
    qApp->installEventFilter (this);
}

bool QISplitter::eventFilter(QObject *aWatched, QEvent *aEvent)
{
    if (aWatched == handle(1))
    {
        switch (aEvent->type())
        {
            case QEvent::MouseButtonDblClick:
                restoreState (mBaseState);
                break;
            default:
                break;
        }
    }

    return QSplitter::eventFilter(aWatched, aEvent);
}

void QISplitter::showEvent(QShowEvent *aEvent)
{
    if (!mPolished)
    {
        mPolished = true;
        mBaseState = saveState();
    }

    return QSplitter::showEvent(aEvent);
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
    }else
        return new QIShadeSplitterHandle(orientation(), this);
}

#include "QISplitter.moc"

