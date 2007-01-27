/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * InnoTek Qt extensions: QIStateIndicator class declaration
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

#ifndef __QIStateIndicator_h__
#define __QIStateIndicator_h__

#include <qframe.h>
#include <qpixmap.h>

#include <qintdict.h>

class QIStateIndicator : public QFrame
{
    Q_OBJECT

public:

    QIStateIndicator (
        int state,
        QWidget *parent, const char *name = 0, WFlags f = 0
    );

    virtual QSize sizeHint() const;

    int state () const { return st; }

    QPixmap stateIcon (int st) const;
    void setStateIcon (int st, const QPixmap &pm);

public slots:

    void setState (int s);
    void setState (bool s) { setState ((int) s); }

signals:

    void mouseDoubleClicked (QIStateIndicator *indicator, QMouseEvent *e);
    void contextMenuRequested (QIStateIndicator *indicator, QContextMenuEvent *e);

protected:

    virtual void drawContents (QPainter *p);

    virtual void mouseDoubleClickEvent (QMouseEvent * e);
    virtual void contextMenuEvent (QContextMenuEvent * e);

private:

    int st;
    QSize sz;

    QIntDict <QPixmap> state_icons;
};

#endif // __QIStateIndicator_h__
