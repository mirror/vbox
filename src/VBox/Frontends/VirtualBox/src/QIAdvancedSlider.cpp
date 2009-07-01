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

/* Qt includes */
#include <QVBoxLayout>

/* System includes */
#include <math.h>


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

void QIAdvancedSlider::setSnapingEnabled (bool aOn)
{
    mSnapingEnabled = aOn;
}

bool QIAdvancedSlider::isSnapingEnabled() const
{
    return mSnapingEnabled;
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
    mSnapingEnabled = true;

    QVBoxLayout *pMainLayout = new QVBoxLayout (this);
    mSlider = new QSlider (aOrientation, this);
    pMainLayout->addWidget (mSlider);

    connect(mSlider, SIGNAL (sliderMoved(int)), this, SLOT (prvSliderMoved(int)));
    connect(mSlider, SIGNAL (valueChanged(int)), this, SIGNAL (valueChanged(int)));
    connect(mSlider, SIGNAL (sliderPressed()), this, SIGNAL (sliderPressed()));
    connect(mSlider, SIGNAL (sliderReleased()), this, SIGNAL (sliderReleased()));
}

int QIAdvancedSlider::snapValue(int val)
{
    if (mSnapingEnabled &&
        val > 2)
    {
        float l2 = log2 ((float)val);
        float snap = 1.0/l2 * 2; /* How much border around the current snaping value (%) */
        int newVal = pow (2, qRound (l2)); /* The value to snap on */
        if (abs (newVal - val) < (snap * newVal)) /* Snap only if we are in a defined array */
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
