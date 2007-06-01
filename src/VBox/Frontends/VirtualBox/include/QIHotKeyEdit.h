/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIHotKeyEdit class declaration
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __QIHotKeyEdit_h__
#define __QIHotKeyEdit_h__

#include <qlabel.h>
#if defined(Q_WS_X11)
#include <qmap.h>
#endif
#if defined(Q_WS_MAC)
#include <Carbon/Carbon.h>
#endif


class QIHotKeyEdit : public QLabel
{
    Q_OBJECT

public:

    QIHotKeyEdit( QWidget * parent, const char * name = 0 );
    virtual ~QIHotKeyEdit();

    void setKey( int keyval );
    int key() const { return keyval; }

    QString symbolicName() const { return symbname; }

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

#if defined(Q_WS_X11)
    static void languageChange(void);
#endif
    static QString keyName (int key);
    static bool isValidKey (int k);

public slots:

    void clear();

protected:

#if defined(Q_WS_WIN32)
    bool winEvent( MSG *msg );
#elif defined(Q_WS_X11)
    bool x11Event( XEvent *event );
#elif defined(Q_WS_MAC)
    static pascal OSStatus darwinEventHandlerProc( EventHandlerCallRef inHandlerCallRef,
                                                   EventRef inEvent, void *inUserData );
    bool darwinKeyboardEvent( EventRef inEvent );
#endif

    void focusInEvent( QFocusEvent * );
    void focusOutEvent( QFocusEvent * );

    void drawContents( QPainter * p );

private:

    void updateText();

    int keyval;
    QString symbname;

    QColorGroup true_acg;

#if defined(Q_WS_X11)
    static QMap<QString, QString> keyNames;
#endif

#if defined(Q_WS_MAC)
    /** Event handler reference. NULL if the handler isn't installed. */
    EventHandlerRef m_darwinEventHandlerRef;
    /** The current modifier key mask. Used to figure out which modifier
     *  key was pressed when we get a kEventRawKeyModifiersChanged event. */
    UInt32 m_darwinKeyModifiers;
#endif

    static const char *NoneSymbName;
};

#endif // __QIHotKeyEdit_h__
