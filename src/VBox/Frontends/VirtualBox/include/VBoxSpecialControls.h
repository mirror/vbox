/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSpecialButtons declarations
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

#ifndef ___VBoxSpecialControls_h__
#define ___VBoxSpecialControls_h__

/* VBox includes */
#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QPushButton>

#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS

/* VBox includes */
#include "VBoxCocoaSpecialControls.h"

/********************************************************************************
 *
 * A mini cancel button in the native Cocoa version.
 *
 ********************************************************************************/
class VBoxMiniCancelButton: public QAbstractButton
{
    Q_OBJECT;

public:
    VBoxMiniCancelButton (QWidget *aParent = 0);

    void setToolTip (const QString &aTip) { mButton->setToolTip (aTip); }

protected:
    void paintEvent (QPaintEvent * /* aEvent */) {}

private:
    VBoxCocoaButton *mButton;
};

/********************************************************************************
 *
 * A help button in the native Cocoa version.
 *
 ********************************************************************************/
class VBoxHelpButton: public QPushButton
{
    Q_OBJECT;

public:
    VBoxHelpButton (QWidget *aParent = 0);

    void setToolTip (const QString &aTip) { mButton->setToolTip (aTip); }

    void initFrom (QPushButton * /* aOther */) {}

protected:
    void paintEvent (QPaintEvent * /* aEvent */) {}

private:
    VBoxCocoaButton *mButton;
};

/********************************************************************************
 *
 * A segmented button in the native Cocoa version.
 *
 ********************************************************************************/
class VBoxSegmentedButton: public VBoxCocoaSegmentedButton
{
    Q_OBJECT;

public:
    VBoxSegmentedButton (int aCount, QWidget *aParent = 0);

    void setIcon (int /* aSegment */, const QIcon & /* aIcon */) {}
};

/********************************************************************************
 *
 * A search field in the native Cocoa version.
 *
 ********************************************************************************/
class VBoxSearchField: public VBoxCocoaSearchField
{
    Q_OBJECT;

public:
    VBoxSearchField (QWidget *aParent = 0);
};

#else /* VBOX_DARWIN_USE_NATIVE_CONTROLS */

/* VBox includes */
#include "QIToolButton.h"

/* Qt includes */
#include <QLineEdit>

/* Qt forward declarations */
class QSignalMapper;

/********************************************************************************
 *
 * A mini cancel button for the other OS's.
 *
 ********************************************************************************/
class VBoxMiniCancelButton: public QIWithRetranslateUI<QIToolButton>
{
    Q_OBJECT;

public:
    VBoxMiniCancelButton (QWidget *aParent = 0);

protected:
    void retranslateUi() {};
};

/********************************************************************************
 *
 * A help button for the other OS's.
 *
 ********************************************************************************/
class VBoxHelpButton: public QIWithRetranslateUI<QPushButton>
{
    Q_OBJECT;

public:
    VBoxHelpButton (QWidget *aParent = 0);
#ifdef Q_WS_MAC
    ~VBoxHelpButton();
    QSize sizeHint() const;
#endif /* Q_WS_MAC */

    void initFrom (QPushButton *aOther);

protected:
    void retranslateUi();

#ifdef Q_WS_MAC
    void paintEvent (QPaintEvent *aEvent);

    bool hitButton (const QPoint &pos) const;

    void mousePressEvent (QMouseEvent *aEvent);
    void mouseReleaseEvent (QMouseEvent *aEvent);
    void leaveEvent (QEvent *aEvent);

private:
    /* Private member vars */
    bool mButtonPressed;

    QSize mSize;
    QPixmap *mNormalPixmap;
    QPixmap *mPressedPixmap;
    QImage *mMask;
    QRect mBRect;
#endif /* Q_WS_MAC */
};

/********************************************************************************
 *
 * A segmented button for the other OS's.
 *
 ********************************************************************************/
class VBoxSegmentedButton: public QWidget
{
    Q_OBJECT;

public:
    VBoxSegmentedButton (int aCount, QWidget *aParent = 0);
    ~VBoxSegmentedButton();

    void setTitle (int aSegment, const QString &aTitle);
    void setToolTip (int aSegment, const QString &aTip);
    void setIcon (int aSegment, const QIcon &aIcon);
    void setEnabled (int aSegment, bool fEnabled);

    void animateClick (int aSegment);

signals:
    void clicked (int aSegment);

private:
    /* Private member vars */
    QList<QIToolButton*> mButtons;
    QSignalMapper *mSignalMapper;
};

/********************************************************************************
 *
 * A search field  for the other OS's.
 *
 ********************************************************************************/
class VBoxSearchField: public QLineEdit
{
    Q_OBJECT;

public:
    VBoxSearchField (QWidget *aParent = 0);

    void markError();
    void unmarkError();

private:
    /* Private member vars */
    QBrush mBaseBrush;
};

#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */

#endif /* ___VBoxSpecialControls_h__ */

