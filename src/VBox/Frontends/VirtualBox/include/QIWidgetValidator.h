/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIWidgetValidator class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QIWidgetValidator_h__
#define __QIWidgetValidator_h__

#include <limits.h>

#include <qobject.h>
#include <qvalidator.h>

class QIWidgetValidator : public QObject
{
    Q_OBJECT

public:
    QIWidgetValidator( QWidget *widget, QObject *parent = 0, const char *name = 0 );
    ~QIWidgetValidator();

    QWidget *widget() const { return wgt; }
    bool isValid() const;
    void rescan();

    void setOtherValid( bool valid ) { otherValid = valid; }
    bool isOtherValid() const { return otherValid; }

signals:
    void validityChanged( const QIWidgetValidator *wval );
    void isValidRequested( QIWidgetValidator *wval );

public slots:
    void revalidate() { doRevalidate(); }

private:
    QWidget *wgt;
    bool otherValid;

private slots:
    void doRevalidate() { emit validityChanged( this ); }
};

class QIULongValidator : public QValidator
{
public:

    QIULongValidator (QObject *aParent, const char *aName = 0)
        : QValidator (aParent, aName)
        , mBottom (0), mTop (ULONG_MAX) {}

    QIULongValidator (ulong aMinimum, ulong aMaximum,
                      QObject *aParent, const char *aName = 0)
        : QValidator (aParent, aName)
        , mBottom (aMinimum), mTop (aMaximum) {}

    ~QIULongValidator() {}

    State validate (QString &aInput, int &aPos) const;
    void setBottom (ulong aBottom) { setRange (aBottom, mTop); }
    void setTop (ulong aTop) { setRange (mBottom, aTop); }
    void setRange (ulong aBottom, ulong aTop) { mBottom = aBottom; mTop = aTop; }
    ulong bottom() const { return mBottom; }
    ulong top() const { return mTop; }

private:

    ulong mBottom;
    ulong mTop;
};

#endif // __QIWidgetValidator_h__
