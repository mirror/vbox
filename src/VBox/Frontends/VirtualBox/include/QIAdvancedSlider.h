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

#ifndef __QIAdvancedSlider_h__
#define __QIAdvancedSlider_h__

/* Qt includes */
#include <QSlider>

class QIAdvancedSlider: public QWidget
{
    Q_OBJECT;

public:
    QIAdvancedSlider (QWidget *aParent = 0);
    QIAdvancedSlider (Qt::Orientation aOrientation, QWidget *aParent = 0);

    int value() const;

    void setRange (int aMinV, int aMaxV);

    void setMaximum (int aVal);
    int maximum() const;

    void setMinimum (int aVal);
    int minimum() const;

    void setPageStep (int aVal);
    int pageStep() const;

    void setSingleStep (int aVal);
    int singelStep() const;

    void setTickInterval (int aVal);
    int tickInterval() const;

    void setTickPosition (QSlider::TickPosition aPos);
    QSlider::TickPosition tickPosition() const;

    Qt::Orientation orientation() const;

    void setSnappingEnabled (bool aOn);
    bool isSnappingEnabled() const;

public slots:

    void setOrientation (Qt::Orientation aOrientation);
    void setValue (int aVal);

signals:
    void valueChanged (int);
    void sliderMoved (int);
    void sliderPressed();
    void sliderReleased();

private slots:

    void prvSliderMoved (int val);

private:

    void init (Qt::Orientation aOrientation = Qt::Horizontal);
    int snapValue(int val);

    /* Private member vars */
    QSlider *mSlider;
    bool mSnappingEnabled;
};

#endif /* __QIAdvancedSlider__ */

