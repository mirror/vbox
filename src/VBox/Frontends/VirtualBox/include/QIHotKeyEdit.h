/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * InnoTek Qt extensions: QIHotKeyEdit class declaration
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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

class QIHotKeyEdit : public QLabel
{
    Q_OBJECT

public:

    QIHotKeyEdit( QWidget * parent, const char * name = 0 );

    void setKey( int keyval );
    int key() const { return keyval; }

    QString symbolicName() const { return symbname; }

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    static QString keyName (int key);
    static bool isValidKey (int k);

public slots:

    void clear();

protected:

#if defined(Q_WS_WIN32)
    bool winEvent( MSG *msg );
#elif defined(Q_WS_X11)
    bool x11Event( XEvent *event );
#endif

    void focusInEvent( QFocusEvent * );
    void focusOutEvent( QFocusEvent * );

    void drawContents( QPainter * p );

private:

    void updateText();

    int keyval;
    QString symbname;

    QColorGroup true_acg;

    static const char *NoneSymbName;
};

#endif // __QIHotKeyEdit_h__
