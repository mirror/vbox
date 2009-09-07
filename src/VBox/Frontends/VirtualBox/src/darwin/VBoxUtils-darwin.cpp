/* $Id$ */
/** @file
 * Qt GUI - Utility Classes and Functions specific to Darwin.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include "VBoxUtils-darwin.h"

#include <iprt/mem.h>

#include <QApplication>
#include <QWidget>
#include <QToolBar>
#include <QPainter>
#include <QPixmap>

#include <Carbon/Carbon.h>

#if QT_VERSION < 0x040400
extern void qt_mac_set_menubar_icons(bool b);
#endif /* QT_VERSION < 0x040400 */

NativeViewRef darwinToNativeView (QWidget *aWidget)
{
    return reinterpret_cast<NativeViewRef>(aWidget->winId());
}

NativeWindowRef darwinToNativeWindow (QWidget *aWidget)
{
    return ::darwinToNativeWindowImpl (::darwinToNativeView (aWidget));
}

NativeWindowRef darwinToNativeWindow (NativeViewRef aView)
{
    return ::darwinToNativeWindowImpl (aView);
}

NativeViewRef darwinToNativeView (NativeWindowRef aWindow)
{
    return ::darwinToNativeViewImpl (aWindow);
}

void darwinSetShowsToolbarButton (QToolBar *aToolBar, bool aEnabled)
{
    QWidget *parent = aToolBar->parentWidget();
    if (parent)
        ::darwinSetShowsToolbarButtonImpl (::darwinToNativeWindow (parent), aEnabled);
}

void darwinSetHidesAllTitleButtons (QWidget *aWidget)
{
#ifdef QT_MAC_USE_COCOA
    /* Currently only necessary in the Cocoa version */
    ::darwinSetHidesAllTitleButtonsImpl (::darwinToNativeWindow (aWidget));
#else /* QT_MAC_USE_COCOA */
    NOREF (aWidget);
#endif /* !QT_MAC_USE_COCOA */
}

void darwinSetShowsWindowTransparent (QWidget *aWidget, bool aEnabled)
{
    ::darwinSetShowsWindowTransparentImpl (::darwinToNativeWindow (aWidget), aEnabled);
}

void darwinWindowAnimateResize (QWidget *aWidget, const QRect &aTarget)
{
    ::darwinWindowAnimateResizeImpl (::darwinToNativeWindow (aWidget), aTarget.x(), aTarget.y(), aTarget.width(), aTarget.height());
}

void darwinWindowInvalidateShape (QWidget *aWidget)
{
#ifdef QT_MAC_USE_COCOA
    /* Here a simple update is enough! */
    aWidget->update();
#else /* QT_MAC_USE_COCOA */
    ::darwinWindowInvalidateShapeImpl (::darwinToNativeWindow (aWidget));
#endif /* QT_MAC_USE_COCOA */
}
;
void darwinWindowInvalidateShadow (QWidget *aWidget)
{
#ifdef QT_MAC_USE_COCOA
    ::darwinWindowInvalidateShadowImpl (::darwinToNativeWindow (aWidget));
#else /* QT_MAC_USE_COCOA */
    NOREF (aWidget);
#endif /* QT_MAC_USE_COCOA */
}

void darwinSetShowsResizeIndicator (QWidget *aWidget, bool aEnabled)
{
    ::darwinSetShowsResizeIndicatorImpl (::darwinToNativeWindow (aWidget), aEnabled);
}

QString darwinSystemLanguage (void)
{
    /* Get the locales supported by our bundle */
    CFArrayRef supportedLocales = ::CFBundleCopyBundleLocalizations (::CFBundleGetMainBundle());
    /* Check them against the languages currently selected by the user */
    CFArrayRef preferredLocales = ::CFBundleCopyPreferredLocalizationsFromArray (supportedLocales);
    /* Get the one which is on top */
    CFStringRef localeId = (CFStringRef)::CFArrayGetValueAtIndex (preferredLocales, 0);
    /* Convert them to a C-string */
    char localeName[20];
    ::CFStringGetCString (localeId, localeName, sizeof (localeName), kCFStringEncodingUTF8);
    /* Some cleanup */
    ::CFRelease (supportedLocales);
    ::CFRelease (preferredLocales);
    QString id(localeName);
    /* Check for some misbehavior */
    if (id.isEmpty() ||
        id.toLower() == "english")
        id = "en";
    return id;
}

void darwinDisableIconsInMenus (void)
{
    /* No icons in the menu of a mac application. */
#if QT_VERSION < 0x040400
    qt_mac_set_menubar_icons (false);
#else /* QT_VERSION < 0x040400 */
    /* Available since Qt 4.4 only */
    QApplication::instance()->setAttribute (Qt::AA_DontShowIconsInMenus, true);
#endif /* QT_VERSION >= 0x040400 */
}

int darwinWindowToolBarHeight (QWidget *aWidget)
{
#ifndef QT_MAC_USE_COCOA
    return ::darwinWindowToolBarHeight (::darwinToNativeWindow (aWidget));
#else /* QT_MAC_USE_COCOA */
    NOREF (aWidget);
    return 0;
#endif /* QT_MAC_USE_COCOA */
}

CGContextRef darwinToCGContextRef (QWidget *aWidget)
{
    return static_cast<CGContext *> (aWidget->macCGHandle());
}

/* Proxy icon creation */
QPixmap darwinCreateDragPixmap (const QPixmap& aPixmap, const QString &aText)
{
    QFontMetrics fm (qApp->font());
    QRect tbRect = fm.boundingRect (aText);
    const int h = qMax (aPixmap.height(), fm.ascent() + 1);
    const int m = 2;
    QPixmap dragPixmap (aPixmap.width() + tbRect.width() + m, h);
    dragPixmap.fill (Qt::transparent);
    QPainter painter (&dragPixmap);
    painter.drawPixmap (0, qAbs (h - aPixmap.height()) / 2.0, aPixmap);
    painter.setPen (Qt::white);
    painter.drawText (QRect (aPixmap.width() + m, 1, tbRect.width(), h - 1), Qt::AlignLeft | Qt::AlignVCenter, aText);
    painter.setPen (Qt::black);
    painter.drawText (QRect (aPixmap.width() + m, 0, tbRect.width(), h - 1), Qt::AlignLeft | Qt::AlignVCenter, aText);
    painter.end();
    return dragPixmap;
}

/**
 * Callback for deleting the QImage object when CGImageCreate is done
 * with it (which is probably not until the returned CFGImageRef is released).
 *
 * @param   info        Pointer to the QImage.
 */
static void darwinDataProviderReleaseQImage (void *info, const void *, size_t)
{
    QImage *qimg = (QImage *)info;
    delete qimg;
}

/**
 * Converts a QPixmap to a CGImage.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aPixmap     Pointer to the QPixmap instance to convert.
 */
CGImageRef darwinToCGImageRef (const QImage *aImage)
{
    QImage *imageCopy = new QImage (*aImage);
    /** @todo this code assumes 32-bit image input, the lazy bird convert image to 32-bit method is anything but optimal... */
    if (imageCopy->format() != QImage::Format_ARGB32)
        *imageCopy = imageCopy->convertToFormat (QImage::Format_ARGB32);
    Assert (!imageCopy->isNull());

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef dp = CGDataProviderCreateWithData (imageCopy, aImage->bits(), aImage->numBytes(), darwinDataProviderReleaseQImage);

    CGBitmapInfo bmpInfo = kCGImageAlphaFirst | kCGBitmapByteOrder32Host;
    CGImageRef ir = CGImageCreate (imageCopy->width(), imageCopy->height(), 8, 32, imageCopy->bytesPerLine(), cs,
                                   bmpInfo, dp, 0 /*decode */, 0 /* shouldInterpolate */,
                                   kCGRenderingIntentDefault);
    CGColorSpaceRelease (cs);
    CGDataProviderRelease (dp);

    Assert (ir);
    return ir;
}

/**
 * Converts a QPixmap to a CGImage.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aPixmap     Pointer to the QPixmap instance to convert.
 */
CGImageRef darwinToCGImageRef (const QPixmap *aPixmap)
{
    /* It seems Qt releases the memory to an returned CGImageRef when the
     * associated QPixmap is destroyed. This shouldn't happen as long a
     * CGImageRef has a retrain count. As a workaround we make a real copy. */
    int bitmapBytesPerRow = aPixmap->width() * 4;
    int bitmapByteCount = (bitmapBytesPerRow * aPixmap->height());
    /* Create a memory block for the temporary image. It is initialized by zero
     * which means black & zero alpha. */
    void *pBitmapData = RTMemAllocZ (bitmapByteCount);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    /* Create a context to paint on */
    CGContextRef ctx = CGBitmapContextCreate (pBitmapData,
                                              aPixmap->width(),
                                              aPixmap->height(),
                                              8,
                                              bitmapBytesPerRow,
                                              cs,
                                              kCGImageAlphaPremultipliedFirst);
    /* Get the CGImageRef from Qt */
    CGImageRef qtPixmap = aPixmap->toMacCGImageRef();
    /* Draw the image from Qt & convert the context back to a new CGImageRef. */
    CGContextDrawImage (ctx, CGRectMake (0, 0, aPixmap->width(), aPixmap->height()), qtPixmap);
    CGImageRef newImage = CGBitmapContextCreateImage (ctx);
    /* Now release all used resources */
    CGImageRelease (qtPixmap);
    CGContextRelease (ctx);
    CGColorSpaceRelease (cs);
    RTMemFree (pBitmapData);

    /* Return the new CGImageRef */
    return newImage;
}

/**
 * Loads an image using Qt and converts it to a CGImage.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aSource     The source name.
 */
CGImageRef darwinToCGImageRef (const char *aSource)
{
    QPixmap qpm (QString(":/") + aSource);
    Assert (!qpm.isNull());
    return ::darwinToCGImageRef (&qpm);
}

/********************************************************************************
 *
 * Old carbon stuff. Have to converted soon!
 *
 ********************************************************************************/

/* Event debugging stuff. Borrowed from Knuts Qt patch. */
#if defined (DEBUG)

# define MY_CASE(a) case a: return #a
const char * DarwinDebugEventName (UInt32 ekind)
{
    switch (ekind)
    {
# if !__LP64__
        MY_CASE(kEventWindowUpdate                );
        MY_CASE(kEventWindowDrawContent           );
# endif
        MY_CASE(kEventWindowActivated             );
        MY_CASE(kEventWindowDeactivated           );
        MY_CASE(kEventWindowHandleActivate        );
        MY_CASE(kEventWindowHandleDeactivate      );
        MY_CASE(kEventWindowGetClickActivation    );
        MY_CASE(kEventWindowGetClickModality      );
        MY_CASE(kEventWindowShowing               );
        MY_CASE(kEventWindowHiding                );
        MY_CASE(kEventWindowShown                 );
        MY_CASE(kEventWindowHidden                );
        MY_CASE(kEventWindowCollapsing            );
        MY_CASE(kEventWindowCollapsed             );
        MY_CASE(kEventWindowExpanding             );
        MY_CASE(kEventWindowExpanded              );
        MY_CASE(kEventWindowZoomed                );
        MY_CASE(kEventWindowBoundsChanging        );
        MY_CASE(kEventWindowBoundsChanged         );
        MY_CASE(kEventWindowResizeStarted         );
        MY_CASE(kEventWindowResizeCompleted       );
        MY_CASE(kEventWindowDragStarted           );
        MY_CASE(kEventWindowDragCompleted         );
        MY_CASE(kEventWindowClosed                );
        MY_CASE(kEventWindowTransitionStarted     );
        MY_CASE(kEventWindowTransitionCompleted   );
# if !__LP64__
        MY_CASE(kEventWindowClickDragRgn          );
        MY_CASE(kEventWindowClickResizeRgn        );
        MY_CASE(kEventWindowClickCollapseRgn      );
        MY_CASE(kEventWindowClickCloseRgn         );
        MY_CASE(kEventWindowClickZoomRgn          );
        MY_CASE(kEventWindowClickContentRgn       );
        MY_CASE(kEventWindowClickProxyIconRgn     );
        MY_CASE(kEventWindowClickToolbarButtonRgn );
        MY_CASE(kEventWindowClickStructureRgn     );
# endif
        MY_CASE(kEventWindowCursorChange          );
        MY_CASE(kEventWindowCollapse              );
        MY_CASE(kEventWindowCollapseAll           );
        MY_CASE(kEventWindowExpand                );
        MY_CASE(kEventWindowExpandAll             );
        MY_CASE(kEventWindowClose                 );
        MY_CASE(kEventWindowCloseAll              );
        MY_CASE(kEventWindowZoom                  );
        MY_CASE(kEventWindowZoomAll               );
        MY_CASE(kEventWindowContextualMenuSelect  );
        MY_CASE(kEventWindowPathSelect            );
        MY_CASE(kEventWindowGetIdealSize          );
        MY_CASE(kEventWindowGetMinimumSize        );
        MY_CASE(kEventWindowGetMaximumSize        );
        MY_CASE(kEventWindowConstrain             );
# if !__LP64__
        MY_CASE(kEventWindowHandleContentClick    );
# endif
        MY_CASE(kEventWindowGetDockTileMenu       );
        MY_CASE(kEventWindowProxyBeginDrag        );
        MY_CASE(kEventWindowProxyEndDrag          );
        MY_CASE(kEventWindowToolbarSwitchMode     );
        MY_CASE(kEventWindowFocusAcquired         );
        MY_CASE(kEventWindowFocusRelinquish       );
        MY_CASE(kEventWindowFocusContent          );
        MY_CASE(kEventWindowFocusToolbar          );
        MY_CASE(kEventWindowFocusDrawer           );
        MY_CASE(kEventWindowSheetOpening          );
        MY_CASE(kEventWindowSheetOpened           );
        MY_CASE(kEventWindowSheetClosing          );
        MY_CASE(kEventWindowSheetClosed           );
        MY_CASE(kEventWindowDrawerOpening         );
        MY_CASE(kEventWindowDrawerOpened          );
        MY_CASE(kEventWindowDrawerClosing         );
        MY_CASE(kEventWindowDrawerClosed          );
        MY_CASE(kEventWindowDrawFrame             );
        MY_CASE(kEventWindowDrawPart              );
        MY_CASE(kEventWindowGetRegion             );
        MY_CASE(kEventWindowHitTest               );
        MY_CASE(kEventWindowInit                  );
        MY_CASE(kEventWindowDispose               );
        MY_CASE(kEventWindowDragHilite            );
        MY_CASE(kEventWindowModified              );
        MY_CASE(kEventWindowSetupProxyDragImage   );
        MY_CASE(kEventWindowStateChanged          );
        MY_CASE(kEventWindowMeasureTitle          );
        MY_CASE(kEventWindowDrawGrowBox           );
        MY_CASE(kEventWindowGetGrowImageRegion    );
        MY_CASE(kEventWindowPaint                 );
    }
    static char s_sz[64];
    sprintf(s_sz, "kind=%u", (uint)ekind);
    return s_sz;
}
# undef MY_CASE

/* Convert a class into the 4 char code defined in
 * 'Developer/Headers/CFMCarbon/CarbonEvents.h' to
 * identify the event. */
const char * darwinDebugClassName (UInt32 eclass)
{
    char *pclass = (char*)&eclass;
    static char s_sz[11];
    sprintf(s_sz, "class=%c%c%c%c", pclass[3],
                                    pclass[2],
                                    pclass[1],
                                    pclass[0]);
    return s_sz;
}

void darwinDebugPrintEvent (const char *psz, EventRef evtRef)
{
  if (!evtRef)
      return;
  UInt32 ekind = GetEventKind (evtRef), eclass = GetEventClass (evtRef);
  if (eclass == kEventClassWindow)
  {
      switch (ekind)
      {
# if !__LP64__
          case kEventWindowDrawContent:
          case kEventWindowUpdate:
# endif
          case kEventWindowBoundsChanged:
              break;
          default:
          {
              WindowRef wid = NULL;
              GetEventParameter(evtRef, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &wid);
              QWidget *widget = QWidget::find((WId)wid);
              printf("%d %s: (%s) %#x win=%p wid=%p (%s)\n", (int)time(NULL), psz, darwinDebugClassName (eclass), (uint)ekind, wid, widget, DarwinDebugEventName (ekind));
              break;
          }
      }
  }
  else if (eclass == kEventClassCommand)
  {
      WindowRef wid = NULL;
      GetEventParameter(evtRef, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &wid);
      QWidget *widget = QWidget::find((WId)wid);
      const char *name = "Unknown";
      switch (ekind)
      {
          case kEventCommandProcess:
              name = "kEventCommandProcess";
              break;
          case kEventCommandUpdateStatus:
              name = "kEventCommandUpdateStatus";
              break;
      }
      printf("%d %s: (%s) %#x win=%p wid=%p (%s)\n", (int)time(NULL), psz, darwinDebugClassName (eclass), (uint)ekind, wid, widget, name);
  }
  else if (eclass == kEventClassKeyboard)
  {
      printf("%d %s: %#x(%s) %#x (kEventClassKeyboard)", (int)time(NULL), psz, (uint)eclass, darwinDebugClassName (eclass), (uint)ekind);

      UInt32 keyCode = 0;
      ::GetEventParameter (evtRef, kEventParamKeyCode, typeUInt32, NULL,
                           sizeof (keyCode), NULL, &keyCode);
      printf(" keyCode=%d (%#x) ", (int)keyCode, (unsigned)keyCode);

      char macCharCodes[8] = {0,0,0,0, 0,0,0,0};
      ::GetEventParameter (evtRef, kEventParamKeyCode, typeChar, NULL,
                           sizeof (macCharCodes), NULL, &macCharCodes[0]);
      printf(" macCharCodes={");
      for (unsigned i =0; i < 8 && macCharCodes[i]; i++)
          printf( i == 0 ? "%02x" : ",%02x", macCharCodes[i]);
      printf("}");

      UInt32 modifierMask = 0;
      ::GetEventParameter (evtRef, kEventParamKeyModifiers, typeUInt32, NULL,
                           sizeof (modifierMask), NULL, &modifierMask);
      printf(" modifierMask=%08x", (unsigned)modifierMask);

      UniChar keyUnicodes[8] = {0,0,0,0, 0,0,0,0};
      ::GetEventParameter (evtRef, kEventParamKeyUnicodes, typeUnicodeText, NULL,
                           sizeof (keyUnicodes), NULL, &keyUnicodes[0]);
      printf(" keyUnicodes={");
      for (unsigned i =0; i < 8 && keyUnicodes[i]; i++)
          printf( i == 0 ? "%02x" : ",%02x", keyUnicodes[i]);
      printf("}");

      UInt32 keyboardType = 0;
      ::GetEventParameter (evtRef, kEventParamKeyboardType, typeUInt32, NULL,
                           sizeof (keyboardType), NULL, &keyboardType);
      printf(" keyboardType=%08x", (unsigned)keyboardType);

      EventHotKeyID evtHotKeyId = {0,0};
      ::GetEventParameter (evtRef, typeEventHotKeyID, typeEventHotKeyID, NULL,
                           sizeof (evtHotKeyId), NULL, &evtHotKeyId);
      printf(" evtHotKeyId={signature=%08x, .id=%08x}", (unsigned)evtHotKeyId.signature, (unsigned)evtHotKeyId.id);
      printf("\n");
  }
  else
      printf("%d %s: %#x(%s) %#x\n", (int)time(NULL), psz, (uint)eclass, darwinDebugClassName (eclass), (uint)ekind);
}

#endif /* DEBUG */

