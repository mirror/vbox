/* $Id$ */
/** @file
 * VBox Qt GUI - UICocoaSpecialControls class declaration.
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

#ifndef ___UICocoaSpecialControls_h___
#define ___UICocoaSpecialControls_h___

/* Qt includes: */
#include <QMacCocoaViewContainer>
#include <QWidget>

/* GUI includes: */
#include "VBoxCocoaHelper.h"

/* Add typedefs for Cocoa types: */
ADD_COCOA_NATIVE_REF(NSButton);

/** QMacCocoaViewContainer extension,
  * used as cocoa button container. */
class UICocoaButton : public QMacCocoaViewContainer
{
    Q_OBJECT

signals:

    /** Notifies about button click and whether it's @a fChecked. */
    void clicked(bool fChecked = false);

public:

    /** Cocoa button types. */
    enum CocoaButtonType
    {
        HelpButton,
        CancelButton,
        ResetButton
    };

    /** Constructs cocoa button passing @a pParent to the base-class.
      * @param  enmType  Brings the button type. */
    UICocoaButton(QWidget *pParent, CocoaButtonType enmType);
    /** Destructs cocoa button. */
    ~UICocoaButton();

    /** Returns size-hint. */
    QSize sizeHint() const;

    /** Defines button @a strText. */
    void setText(const QString &strText);
    /** Defines button @a strToolTip. */
    void setToolTip(const QString &strToolTip);

    /** Handles button click. */
    void onClicked();

private:

    /** Returns native cocoa button reference. */
    NativeNSButtonRef nativeRef() const { return static_cast<NativeNSButtonRef>(cocoaView()); }
};

#endif /* !___UICocoaSpecialControls_h___ */

