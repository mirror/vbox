/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIAdvancedSlider class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "QIAdvancedSlider.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QVBoxLayout>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>

/* System includes */
#include <math.h>

class CPrivateSlider: public QSlider
{
public:
    CPrivateSlider (Qt::Orientation aOrientation, QWidget *aParent = 0)
      : QSlider (aOrientation, aParent)
      , mOptColor (0x0, 0xff, 0x0, 0x3c)
      , mWrnColor (0xff, 0x54, 0x0, 0x3c)
      , mErrColor (0xff, 0x0, 0x0, 0x3c)
      , mMinOpt (-1)
      , mMaxOpt (-1)
      , mMinWrn (-1)
      , mMaxWrn (-1)
      , mMinErr (-1)
      , mMaxErr (-1)
    {}

    int positionForValue (int aVal) const
    {
        QStyleOptionSlider opt;
        initStyleOption (&opt);
        opt.subControls = QStyle::SC_All;
        int available = style()->pixelMetric (QStyle::PM_SliderSpaceAvailable, &opt, this);
        return QStyle::sliderPositionFromValue (opt.minimum, opt.maximum, aVal, available);
    }

    virtual void paintEvent ( QPaintEvent * event )
    {
        QPainter p(this);

        QStyleOptionSlider opt;
        initStyleOption (&opt);
        opt.subControls = QStyle::SC_All;

        int available = style()->pixelMetric (QStyle::PM_SliderSpaceAvailable, &opt, this);
        QSize s = size();

        QRect ticks = style()->subControlRect (QStyle::CC_Slider, &opt, QStyle::SC_SliderTickmarks, this);
        if (ticks.isNull() || ticks.isEmpty())
        {
            ticks = style()->subControlRect (QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this) | style()->subControlRect (QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
            ticks.setRect ((s.width() - available) / 2, ticks.bottom() + 1, available, s.height() - ticks.bottom() - 1);
        }
        if (mMinOpt != -1 &&
            mMaxOpt != -1)
        {
            int posMinOpt = QStyle::sliderPositionFromValue (opt.minimum, opt.maximum, mMinOpt, available);
            int posMaxOpt = QStyle::sliderPositionFromValue (opt.minimum, opt.maximum, mMaxOpt, available);
            p.fillRect (ticks.x() + posMinOpt, ticks.y(), posMaxOpt - posMinOpt + 1, ticks.height(), mOptColor);
        }
        if (mMinWrn != -1 &&
            mMaxWrn != -1)
        {
            int posMinWrn = QStyle::sliderPositionFromValue (opt.minimum, opt.maximum, mMinWrn, available);
            int posMaxWrn = QStyle::sliderPositionFromValue (opt.minimum, opt.maximum, mMaxWrn, available);
            p.fillRect (ticks.x() + posMinWrn, ticks.y(), posMaxWrn - posMinWrn + 1, ticks.height(), mWrnColor);
        }
        if (mMinErr != -1 &&
            mMaxErr != -1)
        {
            int posMinErr = QStyle::sliderPositionFromValue (opt.minimum, opt.maximum, mMinErr, available);
            int posMaxErr = QStyle::sliderPositionFromValue (opt.minimum, opt.maximum, mMaxErr, available);
            p.fillRect (ticks.x() + posMinErr, ticks.y(), posMaxErr - posMinErr + 1, ticks.height(), mErrColor);
        }

        QSlider::paintEvent (event);
    }

    QColor mOptColor;
    QColor mWrnColor;
    QColor mErrColor;

    int mMinOpt;
    int mMaxOpt;
    int mMinWrn;
    int mMaxWrn;
    int mMinErr;
    int mMaxErr;
};

QIAdvancedSlider::QIAdvancedSlider (QWidget *aParent /* = 0 */)
  : QWidget (aParent)
{
    init();
}

QIAdvancedSlider::QIAdvancedSlider (Qt::Orientation aOrientation, QWidget *aParent /* = 0 */)
  : QWidget (aParent)
{
    init (aOrientation);
}

int QIAdvancedSlider::value() const
{
    return mSlider->value();
}

void QIAdvancedSlider::setRange (int aMinV, int aMaxV)
{
    mSlider->setRange (aMinV, aMaxV);
}

void QIAdvancedSlider::setMaximum (int aVal)
{
    mSlider->setMaximum (aVal);
}

int QIAdvancedSlider::maximum() const
{
    return mSlider->maximum();
}

void QIAdvancedSlider::setMinimum (int aVal)
{
    mSlider->setMinimum (aVal);
}

int QIAdvancedSlider::minimum() const
{
    return mSlider->minimum();
}

void QIAdvancedSlider::setPageStep (int aVal)
{
    mSlider->setPageStep (aVal);
}

int QIAdvancedSlider::pageStep() const
{
    return mSlider->pageStep();
}

void QIAdvancedSlider::setSingleStep (int aVal)
{
    mSlider->setSingleStep (aVal);
}

int QIAdvancedSlider::singelStep() const
{
    return mSlider->singleStep();
}

void QIAdvancedSlider::setTickInterval (int aVal)
{
    mSlider->setTickInterval (aVal);
}

int QIAdvancedSlider::tickInterval() const
{
    return mSlider->tickInterval();
}

void QIAdvancedSlider::setTickPosition (QSlider::TickPosition aPos)
{
    mSlider->setTickPosition (aPos);
}

QSlider::TickPosition QIAdvancedSlider::tickPosition() const
{
    return mSlider->tickPosition();
}

Qt::Orientation QIAdvancedSlider::orientation() const
{
    return mSlider->orientation();
}

void QIAdvancedSlider::setSnappingEnabled (bool aOn)
{
    mSnappingEnabled = aOn;
}

bool QIAdvancedSlider::isSnappingEnabled() const
{
    return mSnappingEnabled;
}

void QIAdvancedSlider::setOptimalHint (int aMin, int aMax)
{
    mSlider->mMinOpt = aMin;
    mSlider->mMaxOpt = aMax;
}

void QIAdvancedSlider::setWarningHint (int aMin, int aMax)
{
    mSlider->mMinWrn = aMin;
    mSlider->mMaxWrn = aMax;
}

void QIAdvancedSlider::setErrorHint (int aMin, int aMax)
{
    mSlider->mMinErr = aMin;
    mSlider->mMaxErr = aMax;
}

void QIAdvancedSlider::setOrientation (Qt::Orientation aOrientation)
{
    mSlider->setOrientation (aOrientation);
}

void QIAdvancedSlider::setValue (int aVal)
{
    mSlider->setValue (aVal);
}

void QIAdvancedSlider::prvSliderMoved(int val)
{
    val = snapValue(val);
    mSlider->setValue(val);
    emit sliderMoved(val);
}

void QIAdvancedSlider::init (Qt::Orientation aOrientation /* = Qt::Horizontal */)
{
    mSnappingEnabled = false;

    QVBoxLayout *pMainLayout = new QVBoxLayout (this);
    VBoxGlobal::setLayoutMargin (pMainLayout, 0);
    mSlider = new CPrivateSlider (aOrientation, this);
    pMainLayout->addWidget (mSlider);

    connect(mSlider, SIGNAL (sliderMoved(int)), this, SLOT (prvSliderMoved(int)));
    connect(mSlider, SIGNAL (valueChanged(int)), this, SIGNAL (valueChanged(int)));
    connect(mSlider, SIGNAL (sliderPressed()), this, SIGNAL (sliderPressed()));
    connect(mSlider, SIGNAL (sliderReleased()), this, SIGNAL (sliderReleased()));
}

int QIAdvancedSlider::snapValue(int val)
{
    if (mSnappingEnabled &&
        val > 2)
    {
        float l2 = log ((float)val)/log (2.0);
        int newVal = pow ((float)2, (int)qRound (l2)); /* The value to snap on */
        int pos = mSlider->positionForValue (val); /* Get the relative screen pos for the original value */
        int newPos = mSlider->positionForValue (newVal); /* Get the relative screen pos for the snap value */
        if (abs (newPos - pos) < 5) /* 10 pixel snapping range */
        {
            val = newVal;
            if (val > mSlider->maximum())
                val = mSlider->maximum();
            else if (val < mSlider->minimum())
                val = mSlider->minimum();
        }
    }
    return val;
}

