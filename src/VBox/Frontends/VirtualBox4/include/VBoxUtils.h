/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxUtils_h__
#define __VBoxUtils_h__

/* Qt includes */
#include <QMouseEvent>
#include <Q3ListView>

/**
 *  Simple ListView filter to disable unselecting all items by clicking in the
 *  unused area of the list (which is actually very annoying for the Single
 *  selection mode).
 */
class QIListViewSelectionPreserver : protected QObject
{
public:

    QIListViewSelectionPreserver (QObject *parent, Q3ListView *alv)
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
            QMouseEvent *me = static_cast<QMouseEvent *> (e);
            if (!lv->itemAt (me->pos()))
                return true;
        }

        return false;
    }

private:

    Q3ListView *lv;
};

/**
 *  Simple class that filters out presses and releases of the given key
 *  directed to a widget (the widget acts like if it would never handle
 *  this key).
 */
class QIKeyFilter : protected QObject
{
public:

    QIKeyFilter (QObject *aParent, Qt::Key aKey) : QObject (aParent), mKey (aKey) {}

    void watchOn (QObject *o) { o->installEventFilter (this); }

protected:

    bool eventFilter (QObject * /*o*/, QEvent *e)
    {
        if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)
        {
            QKeyEvent *ke = static_cast<QKeyEvent *> (e);
            if (ke->key() == mKey ||
                (mKey == Qt::Key_Enter && ke->key() == Qt::Key_Return))
            {
                ke->ignore();
                return false;
            }
        }

        return false;
    }

    Qt::Key mKey;
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
            QKeyEvent *ke = static_cast<QKeyEvent *> (e);
            if (ke->modifiers() & Qt::AltModifier)
                return true;
        }
        return false;
    }
};

/**
 *  Watches the given widget and makes sure the minimum widget size set by the layout
 *  manager does never get smaller than the previous minimum size set by the
 *  layout manager. This way, widgets with dynamic contents (i.e. text on some
 *  toggle buttons) will be able only to grow, never shrink, to avoid flicker
 *  during alternate contents updates (Pause -> Resume -> Pause -> ...).
 *
 *  @todo not finished
 */
class QIConstraintKeeper : public QObject
{
    Q_OBJECT

public:

    QIConstraintKeeper (QWidget *aParent) : QObject (aParent)
    {
        aParent->setMinimumSize (aParent->size());
        aParent->installEventFilter (this);
    }

private:

    bool eventFilter (QObject *aObject, QEvent *aEvent)
    {
        if (aObject == parent() && aEvent->type() == QEvent::Resize)
        {
            QResizeEvent *ev = static_cast<QResizeEvent *> (aEvent);
            QSize oldSize = ev->oldSize();
            QSize newSize = ev->size();
            int maxWidth = newSize.width() > oldSize.width() ?
                newSize.width() : oldSize.width();
            int maxHeight = newSize.height() > oldSize.height() ?
                newSize.height() : oldSize.height();
            if (maxWidth > oldSize.width() || maxHeight > oldSize.height())
                qobject_cast<QWidget *> (parent())->setMinimumSize (maxWidth, maxHeight);
        }
        return QObject::eventFilter (aObject, aEvent);
    }
};

/**
 *  Simple class which simulates focus-proxy rule redirecting widget
 *  assigned shortcur to desired widget.
 */
class QIFocusProxy : protected QObject
{
public:

    QIFocusProxy (QWidget *aFrom, QWidget *aTo)
        : QObject (aFrom), mFrom (aFrom), mTo (aTo)
    {
        mFrom->installEventFilter (this);
    }

protected:

    bool eventFilter (QObject *aObject, QEvent *aEvent)
    {
        if (aObject == mFrom && aEvent->type() == QEvent::Shortcut)
        {
            mTo->setFocus();
            return true;
        }
        return QObject::eventFilter (aObject, aEvent);
    }

    QWidget *mFrom;
    QWidget *mTo;
};

#ifdef Q_WS_MAC
# undef PAGE_SIZE
# undef PAGE_SHIFT
# include <Carbon/Carbon.h>

/* Asserts if a != noErr and prints the error code */
#define AssertCarbonOSStatus(a) AssertMsg ((a) == noErr, ("Carbon OSStatus: %d\n", static_cast<int> (a)))

class QImage;
class QPixmap;
class VBoxFrameBuffer;

/* Converting stuff */
CGImageRef darwinToCGImageRef (const QImage *aImage);
CGImageRef darwinToCGImageRef (const QPixmap *aPixmap);
CGImageRef darwinToCGImageRef (const char *aSource);

/**
 * Returns a reference to the HIView of the QWidget.
 *
 * @returns HIViewRef of the QWidget.
 * @param   aWidget   Pointer to the QWidget
 */
inline HIViewRef darwinToHIViewRef (QWidget *aWidget)
{
    return HIViewRef(aWidget->winId());
}

/**
 * Returns a reference to the Window of the HIView.
 *
 * @returns WindowRef of the HIView.
 * @param   aViewRef  Reference to the HIView
 */
inline WindowRef darwinToWindowRef (HIViewRef aViewRef)
{
    return reinterpret_cast<WindowRef> (HIViewGetWindow(aViewRef)); 
}

/**
 * Returns a reference to the Window of the QWidget.
 *
 * @returns WindowRef of the QWidget.
 * @param   aWidget   Pointer to the QWidget
 */
inline WindowRef darwinToWindowRef (QWidget *aWidget)
{
    return ::darwinToWindowRef (::darwinToHIViewRef (aWidget));
}

/**
 * Returns a reference to the CGContext of the QWidget.
 *
 * @returns CGContextRef of the QWidget.
 * @param   aWidget      Pointer to the QWidget
 */
inline CGContextRef darwinToCGContextRef (QWidget *aWidget)
{
    return static_cast<CGContext *> (aWidget->macCGHandle());
}

/**
 * Converts a QRect to a HIRect.
 *
 * @returns HIRect for the converted QRect.
 * @param   aRect  the QRect to convert
 */
inline HIRect darwinToHIRect (const QRect &aRect)
{
    return CGRectMake (aRect.x(), aRect.y(), aRect.width(), aRect.height());
}

/* Special routines for the dock handling */
CGImageRef darwinCreateDockBadge (const char *aSource);
void darwinUpdateDockPreview (CGImageRef aVMImage, CGImageRef aOverlayImage, CGImageRef aStateImage = NULL);
void darwinUpdateDockPreview (VBoxFrameBuffer *aFrameBuffer, CGImageRef aOverlayImage);

/* Experimental region handler for the seamless mode */
OSStatus darwinRegionHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData);

#endif /* Q_WS_MAC */

#endif // __VBoxUtils_h__

