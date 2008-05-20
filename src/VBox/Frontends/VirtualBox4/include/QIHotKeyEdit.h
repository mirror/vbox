/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIHotKeyEdit class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __QIHotKeyEdit_h__
#define __QIHotKeyEdit_h__

#include <qlabel.h>
//Added by qt3to4:
#include <QPalette>
#include <QFocusEvent>
#if defined(Q_WS_X11)
#include <qmap.h>
#endif
#if defined(Q_WS_MAC)
#include <Carbon/Carbon.h>
#endif

#if defined(Q_WS_PM)
/* Extra virtual keys returned by QIHotKeyEdit::virtualKey() */
#define VK_LSHIFT   VK_USERFIRST + 0
#define VK_LCTRL    VK_USERFIRST + 1
#define VK_LWIN     VK_USERFIRST + 2
#define VK_RWIN     VK_USERFIRST + 3
#define VK_WINMENU  VK_USERFIRST + 4
#define VK_FORWARD  VK_USERFIRST + 5
#define VK_BACKWARD VK_USERFIRST + 6
#endif

class QIHotKeyEdit : public QLabel
{
    Q_OBJECT

public:

    QIHotKeyEdit (QWidget *aParent, const char *aName = 0);
    virtual ~QIHotKeyEdit();

    void setKey (int aKeyVal);
    int key() const { return mKeyVal; }

    QString symbolicName() const { return mSymbName; }

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

#if defined (Q_WS_PM)
    static int virtualKey (QMSG *aMsg);
#endif

#if defined (Q_WS_PM) || defined (Q_WS_X11)
    static void retranslateUi();
#endif
    static QString keyName (int aKeyVal);
    static bool isValidKey (int aKeyVal);

public slots:

    void clear();

protected:

#if defined (Q_WS_WIN32)
    bool winEvent (MSG *msg);
#elif defined (Q_WS_PM)
    bool pmEvent (QMSG *aMsg);
#elif defined (Q_WS_X11)
    bool x11Event (XEvent *event);
#elif defined (Q_WS_MAC)
    static pascal OSStatus darwinEventHandlerProc (EventHandlerCallRef inHandlerCallRef,
                                                   EventRef inEvent, void *inUserData);
    bool darwinKeyboardEvent (EventRef inEvent);
#endif

    void focusInEvent (QFocusEvent *);
    void focusOutEvent (QFocusEvent *);

    void drawContents (QPainter *p);

private:

    void updateText();

    int mKeyVal;
    QString mSymbName;

    QColorGroup mTrueACG;

#if defined (Q_WS_PM)
    static QMap <int, QString> sKeyNames;
#elif defined (Q_WS_X11)
    static QMap <QString, QString> sKeyNames;
#endif

#if defined (Q_WS_MAC)
    /** Event handler reference. NULL if the handler isn't installed. */
    EventHandlerRef mDarwinEventHandlerRef;
    /** The current modifier key mask. Used to figure out which modifier
     *  key was pressed when we get a kEventRawKeyModifiersChanged event. */
    UInt32 mDarwinKeyModifiers;
#endif

    static const char *kNoneSymbName;
};

#endif // __QIHotKeyEdit_h__
