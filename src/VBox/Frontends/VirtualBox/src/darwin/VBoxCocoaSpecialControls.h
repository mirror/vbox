/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxCocoaSpecialControls class declaration
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

#ifndef ___darwin_VBoxCocoaSpecialControls_h__
#define ___darwin_VBoxCocoaSpecialControls_h__

/* VBox includes */
#include "VBoxCocoaHelper.h"

/* Qt includes */
#include <QMacCocoaViewContainer>

/* Add typedefs for Cocoa types */
ADD_COCOA_NATIVE_REF (NSButton);
ADD_COCOA_NATIVE_REF (NSSegmentedControl);
ADD_COCOA_NATIVE_REF (NSSearchField);

class VBoxCocoaButton: public QMacCocoaViewContainer
{
    Q_OBJECT;

public:
    enum CocoaButtonType
    {
        HelpButton,
        CancelButton
    };

    VBoxCocoaButton (CocoaButtonType aType, QWidget *aParent = 0);
    QSize sizeHint() const;

    void setText (const QString& aText);
    void setToolTip (const QString& aTip);

    void onClicked();

signals:
    void clicked (bool checked = false);

private:
    /* Private member vars */
    NativeNSButtonRef mNativeRef;
};

class VBoxCocoaSegmentedButton: public QMacCocoaViewContainer
{
    Q_OBJECT;

public:
    VBoxCocoaSegmentedButton (int aCount, QWidget *aParent = 0);
    QSize sizeHint() const;

    void setTitle (int aSegment, const QString &aTitle);

    void setToolTip (int aSegment, const QString &aTip);

    void setEnabled (int aSegment, bool fEnabled);

    void animateClick (int aSegment);

    void onClicked (int aSegment);

signals:
    void clicked (int aSegment, bool aChecked = false);

private:
    /* Private member vars */
    NativeNSSegmentedControlRef mNativeRef;
};

class VBoxCocoaSearchField: public QMacCocoaViewContainer
{
    Q_OBJECT;

public:
    VBoxCocoaSearchField (QWidget* aParent = 0);
    QSize sizeHint() const;

    QString text() const;
    void insert (const QString &aText);
    void setToolTip (const QString &aTip);
    void selectAll();

    void markError();
    void unmarkError();

    void onTextChanged (const QString &aText);

signals:
    void textChanged (const QString& aText);

private:
    /* Private member vars */
    NativeNSSearchFieldRef mNativeRef;
};

#endif /* ___darwin_VBoxCocoaSpecialControls_h__ */

