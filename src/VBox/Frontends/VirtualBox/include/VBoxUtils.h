/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions
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

#ifndef __VBoxUtils_h__
#define __VBoxUtils_h__

#include <qobject.h>
#include <qevent.h>
#include <qlistview.h>

/**
 *  Simple ListView filter to disable unselecting all items by clicking in the
 *  unused area of the list (which is actually very annoying for the Single
 *  selection mode).
 */
class QIListViewSelectionPreserver : protected QObject
{
public:

    QIListViewSelectionPreserver (QObject *parent, QListView *alv)
        : QObject (parent), lv (alv)
    {
        lv->viewport()->installEventFilter (this);
    }

protected:

    bool eventFilter (QObject * /* o */, QEvent *e)
    {
        if (e->type() == QEvent::MouseButtonPress ||
            e->type() == QEvent::MouseButtonRelease ||
            e->type() == QEvent::MouseButtonDblClick)
        {
            QMouseEvent *me = (QMouseEvent *) e;
            if (!lv->itemAt (me->pos()))
                return true;
        }

        return false;
    }

private:

    QListView *lv;
};

/**
 *  Simple class that filters out presses and releases of the given key
 *  directed to a widget (the widget acts like if it would never handle
 *  this key).
 */
class QIKeyFilter : protected QObject
{
public:

    QIKeyFilter (QObject *aParent, Key aKey) : QObject (aParent), mKey (aKey) {}

    void watchOn (QObject *o) { o->installEventFilter (this); }

protected:

    bool eventFilter (QObject * /*o*/, QEvent *e)
    {
        if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)
        {
            QKeyEvent *ke = (QKeyEvent *) e;
            if (ke->key() == mKey ||
                (mKey == Qt::Key_Enter && ke->key() == Qt::Key_Return))
            {
                ke->ignore();
                return false;
            }
        }

        return false;
    }

    Key mKey;
};

/**
 *  Simple class that filters out all key presses and releases
 *  got while the Alt key is pressed. For some very strange reason,
 *  QLineEdit accepts those combinations that are not used as accelerators,
 *  and inserts the corresponding characters to the entry field.
 */
class QIAltKeyFilter : protected QObject
{
public:

    QIAltKeyFilter (QObject *aParent) : QObject (aParent) {}

    void watchOn (QObject *o) { o->installEventFilter (this); }

protected:

    bool eventFilter (QObject * /*o*/, QEvent *e)
    {
        if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)
        {
            QKeyEvent *ke = (QKeyEvent *) e;
            if (ke->state() & Qt::AltButton)
                return true;
        }
        return false;
    }
};

#endif // __VBoxUtils_h__

