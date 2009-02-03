/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declarations of utility classes and functions
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef ___VBoxUtils_h___
#define ___VBoxUtils_h___

/* Qt includes */
#include <QMouseEvent>
#include <QWidget>
#include <QTextBrowser>

/**
 *  Simple class that filters out all key presses and releases
 *  got while the Alt key is pressed. For some very strange reason,
 *  QLineEdit accepts those combinations that are not used as accelerators,
 *  and inserts the corresponding characters to the entry field.
 */
class QIAltKeyFilter : protected QObject
{
    Q_OBJECT;

public:

    QIAltKeyFilter (QObject *aParent) :QObject (aParent) {}

    void watchOn (QObject *aObject) { aObject->installEventFilter (this); }

protected:

    bool eventFilter (QObject * /* aObject */, QEvent *aEvent)
    {
        if (aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::KeyRelease)
        {
            QKeyEvent *event = static_cast<QKeyEvent *> (aEvent);
            if (event->modifiers() & Qt::AltModifier)
                return true;
        }
        return false;
    }
};

/**
 *  Simple class which simulates focus-proxy rule redirecting widget
 *  assigned shortcut to desired widget.
 */
class QIFocusProxy : protected QObject
{
    Q_OBJECT;

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

/**
 *  QTextEdit reimplementation to feat some extended requirements.
 */
class QRichTextEdit : public QTextEdit
{
    Q_OBJECT;

public:

    QRichTextEdit (QWidget *aParent) : QTextEdit (aParent) {}

    void setViewportMargins (int aLeft, int aTop, int aRight, int aBottom)
    {
        QTextEdit::setViewportMargins (aLeft, aTop, aRight, aBottom);
    }
};

/**
 *  QTextBrowser reimplementation to feat some extended requirements.
 */
class QRichTextBrowser : public QTextBrowser
{
    Q_OBJECT;

public:

    QRichTextBrowser (QWidget *aParent) : QTextBrowser (aParent) {}

    void setViewportMargins (int aLeft, int aTop, int aRight, int aBottom)
    {
        QTextBrowser::setViewportMargins (aLeft, aTop, aRight, aBottom);
    }
};

#ifdef Q_WS_MAC
class QImage;
class QPixmap;
class QToolBar;
class VBoxFrameBuffer;

# ifdef QT_MAC_USE_COCOA
/** @todo Carbon -> Cocoa */
# else /* !QT_MAC_USE_COCOA */
#  undef PAGE_SIZE
#  undef PAGE_SHIFT
#  include <Carbon/Carbon.h>

/* Asserts if a != noErr and prints the error code */
#  define AssertCarbonOSStatus(a) AssertMsg ((a) == noErr, ("Carbon OSStatus: %d\n", static_cast<int> (a)))

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
DECLINLINE(HIViewRef) darwinToHIViewRef (QWidget *aWidget)
{
    return HIViewRef(aWidget->winId());
}

/**
 * Returns a reference to the Window of the HIView.
 *
 * @returns WindowRef of the HIView.
 * @param   aViewRef  Reference to the HIView
 */
DECLINLINE(WindowRef) darwinToWindowRef (HIViewRef aViewRef)
{
    return reinterpret_cast<WindowRef> (HIViewGetWindow(aViewRef));
}

/**
 * Returns a reference to the Window of the QWidget.
 *
 * @returns WindowRef of the QWidget.
 * @param   aWidget   Pointer to the QWidget
 */
DECLINLINE(WindowRef) darwinToWindowRef (QWidget *aWidget)
{
    return ::darwinToWindowRef (::darwinToHIViewRef (aWidget));
}

/**
 * Returns a reference to the CGContext of the QWidget.
 *
 * @returns CGContextRef of the QWidget.
 * @param   aWidget      Pointer to the QWidget
 */
DECLINLINE(CGContextRef) darwinToCGContextRef (QWidget *aWidget)
{
    return static_cast<CGContext *> (aWidget->macCGHandle());
}

/**
 * Converts a QRect to a HIRect.
 *
 * @returns HIRect for the converted QRect.
 * @param   aRect  the QRect to convert
 */
DECLINLINE(HIRect) darwinToHIRect (const QRect &aRect)
{
    return CGRectMake (aRect.x(), aRect.y(), aRect.width(), aRect.height());
}
#endif /* !QT_MAC_USE_COCOA */

QString darwinSystemLanguage (void);

bool darwinIsMenuOpen (void);

void darwinSetShowToolBarButton (QToolBar *aToolBar, bool aShow);

void darwinWindowAnimateResize (QWidget *aWidget, const QRect &aTarget);

/* Proxy icon creation */
QPixmap darwinCreateDragPixmap (const QPixmap& aPixmap, const QString &aText);

/* Icons in the menu of an mac application are unusual. */
void darwinDisableIconsInMenus (void);

#ifdef QT_MAC_USE_COCOA
/** @todo Carbon -> Cocoa  */
#else  /* !QT_MAC_USE_COCOA */
/* Experimental region handler for the seamless mode */
OSStatus darwinRegionHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData);

/* Handler for the OpenGL overlay window stuff & the possible messages. */
enum
{
    /* Event classes */
    kEventClassVBox        = 'vbox',
    /* Event kinds */
    kEventVBoxShowWindow   = 'swin',
    kEventVBoxMoveWindow   = 'mwin',
    kEventVBoxResizeWindow = 'rwin',
    kEventVBoxUpdateDock   = 'udck'
};
OSStatus darwinOverlayWindowHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData);

#  ifdef DEBUG
void darwinDebugPrintEvent (const char *aPrefix, EventRef aEvent);
#  endif
# endif /* !QT_MAC_USE_COCOA*/
#endif /* Q_WS_MAC */

#endif // !___VBoxUtils_h___

