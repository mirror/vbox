/* $Id$ */
/** @file
 * Qt GUI - Utility Classes and Functions specific to Darwin.
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



#include "VBoxUtils.h"
#include "VBoxFrameBuffer.h"
#include "VBoxConsoleView.h"

#include <iprt/assert.h>
#include <iprt/mem.h>

/* Qt includes */
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QApplication>
#include <QToolBar>

#if QT_VERSION < 0x040400
extern void qt_mac_set_menubar_icons(bool b);
#endif /* QT_VERSION < 0x040400 */

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
    return aPixmap->toMacCGImageRef();
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

void darwinSetShowToolBarButton (QToolBar *aToolBar, bool aShow)
{
    QWidget *parent = aToolBar->parentWidget();
    if (parent)
    {
        int err = ChangeWindowAttributes (::darwinToWindowRef (parent), aShow ? kWindowToolbarButtonAttribute:kWindowNoAttributes,
                                                                        aShow ? kWindowNoAttributes:kWindowToolbarButtonAttribute);
        AssertCarbonOSStatus (err);
    }
}

void darwinWindowAnimateResize (QWidget *aWidget, const QRect &aTarget)
{
    HIRect r = ::darwinToHIRect (aTarget);
    TransitionWindowWithOptions (::darwinToWindowRef (aWidget), kWindowSlideTransitionEffect, kWindowResizeTransitionAction, &r, false, NULL);
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
 * Creates a dock badge image.
 *
 * The badge will be placed on the right hand size and vertically centered
 * after having been scaled up to 32x32.
 *
 * @returns CGImageRef for the new image. (Remember to release it when finished with it.)
 * @param   aSource     The source name.
 */
CGImageRef darwinCreateDockBadge (const char *aSource)
{
    /* Create a transparent image in size 128x128. */
    QImage transImage (128, 128, QImage::Format_ARGB32);
    transImage.fill (qRgba (0, 0, 0, 0));
    /* load the badge */
    QImage badge (aSource);
    Assert (!badge.isNull());
    /* resize it and copy it onto the background. */
    if (badge.width() < 32)
        badge = badge.scaled (32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QPainter painter (&transImage);
    painter.drawImage (QPoint ((transImage.width() - badge.width()) / 2.0,
                               (transImage.height() - badge.height()) / 2.0),
                       badge);
    painter.end();
    /* Convert it to a CGImage. */
    return ::darwinToCGImageRef (&transImage);
}

/* Import private function to capture the window content of any given window. */
CG_EXTERN_C_BEGIN
typedef int CGSWindowID;
typedef void *CGSConnectionID;
CG_EXTERN CGSWindowID GetNativeWindowFromWindowRef(WindowRef ref);
CG_EXTERN CGSConnectionID CGSMainConnectionID(void);
CG_EXTERN void CGContextCopyWindowCaptureContentsToRect(CGContextRef c, CGRect dstRect, CGSConnectionID connection, CGSWindowID window, int zero);
CG_EXTERN_C_END

/**
 * Updates the dock preview image.
 *
 * This method is a callback that updates the 128x128 preview image of the VM window in the dock.
 *
 * @param   aVMImage   the vm screen as a CGImageRef
 * @param   aOverlayImage   an optional icon overlay image to add at the bottom right of the icon
 * @param   aStateImage   an optional state overlay image to add at the center of the icon
 */
void darwinUpdateDockPreview (QWidget *aMainWindow, CGImageRef aVMImage, CGImageRef aOverlayImage, CGImageRef aStateImage /*= NULL*/)
{
    Assert (aVMImage);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert (cs);

    /* Calc the size of the dock icon image and fit it into 128x128 */
    int targetWidth = 128;
    int targetHeight = 128;
    int scaledWidth;
    int scaledHeight;
    float aspect = static_cast <float> (CGImageGetWidth (aVMImage)) / CGImageGetHeight (aVMImage);
    if (aspect > 1.0)
    {
        scaledWidth = targetWidth;
        scaledHeight = targetHeight / aspect;
    }
    else
    {
        scaledWidth = targetWidth * aspect;
        scaledHeight = targetHeight;
    }
    CGRect iconRect = CGRectMake ((targetWidth - scaledWidth) / 2.0,
                                  (targetHeight - scaledHeight) / 2.0,
                                  scaledWidth, scaledHeight);
    /* Create the context to draw on */
    CGContextRef context = BeginCGContextForApplicationDockTile();
    Assert (context);
    /* Clear the background */
    CGContextSetBlendMode (context, kCGBlendModeNormal);
    CGContextClearRect (context, CGRectMake (0, 0, 128, 128));
    /* rounded corners */
//        CGContextSetLineJoin (context, kCGLineJoinRound);
//        CGContextSetShadow (context, CGSizeMake (10, 5), 1);
//        CGContextSetAllowsAntialiasing (context, true);
    /* some little boarder */
    iconRect = CGRectInset (iconRect, 1, 1);
    /* gray stroke */
    CGContextSetRGBStrokeColor (context, 225.0/255.0, 218.0/255.0, 211.0/255.0, 1);
    iconRect = CGRectInset (iconRect, 6, 6);
    CGContextStrokeRectWithWidth (context, iconRect, 12);
    iconRect = CGRectInset (iconRect, 5, 5);
    /* black stroke */
    CGContextSetRGBStrokeColor (context, 0.0, 0.0, 0.0, 1.0);
    CGContextStrokeRectWithWidth (context, iconRect, 2);
    /* vm content */
    iconRect = CGRectInset (iconRect, 1, 1);
    CGContextDrawImage (context, iconRect, aVMImage);
    /* Process the content of any external OpenGL windows. */
    WindowRef w = darwinToWindowRef (aMainWindow);
    WindowGroupRef g = GetWindowGroup (w);
    WindowGroupContentOptions wgco = kWindowGroupContentsReturnWindows | kWindowGroupContentsRecurse | kWindowGroupContentsVisible;
    ItemCount c = CountWindowGroupContents (g, wgco);
    float a1 = iconRect.size.width / static_cast <float> (CGImageGetWidth (aVMImage));
    float a2 = iconRect.size.height / static_cast <float> (CGImageGetHeight (aVMImage));
    HIViewRef mainView = HIViewGetRoot (w);
    Rect tmpR;
    GetWindowBounds (w, kWindowContentRgn, &tmpR);
    HIRect mainRect = CGRectMake (tmpR.left, tmpR.top, tmpR.right-tmpR.left, tmpR.bottom-tmpR.top);
    for (ItemCount i = 0; i <= c; ++i)
    {
        WindowRef wc;
        OSStatus status = GetIndexedWindow (g, i, wgco, &wc);
        if (status == noErr &&
            wc != w)
        {
            WindowClass winClass;
            status = GetWindowClass (wc, &winClass);
            if (status == noErr &&
                winClass == kOverlayWindowClass)
            {
                Rect tmpR1;
                GetWindowBounds (wc, kWindowContentRgn, &tmpR1);
                HIRect rect;
                rect.size.width = (tmpR1.right-tmpR1.left) * a1;
                rect.size.height = (tmpR1.bottom-tmpR1.top) * a2;
                rect.origin.x = iconRect.origin.x + (tmpR1.left - mainRect.origin.x) * a1;
                rect.origin.y = targetHeight - (iconRect.origin.y + (tmpR1.top - mainRect.origin.y) * a2) - rect.size.height;
                /* This is a big, bad hack. The following functions aren't
                 * documented nor official supported by apple. But its the only way
                 * to capture the OpenGL content of a window without fiddling
                 * around with gPixelRead or something like that. */
                CGSWindowID wid = GetNativeWindowFromWindowRef(wc);
                CGContextCopyWindowCaptureContentsToRect(context, rect, CGSMainConnectionID(), wid, 0);
            }
        }
    }

    /* the state image at center */
    if (aStateImage)
    {
        CGRect stateRect = CGRectMake ((targetWidth - CGImageGetWidth (aStateImage)) / 2.0,
                                       (targetHeight - CGImageGetHeight (aStateImage)) / 2.0,
                                       CGImageGetWidth (aStateImage),
                                       CGImageGetHeight (aStateImage));
        CGContextDrawImage (context, stateRect, aStateImage);
    }
    /* the overlay image at bottom/right */
    if (aOverlayImage)
    {

        CGRect overlayRect = CGRectMake (targetWidth - CGImageGetWidth (aOverlayImage),
                                         0,
                                         CGImageGetWidth (aOverlayImage),
                                         CGImageGetHeight (aOverlayImage));
        CGContextDrawImage (context, overlayRect, aOverlayImage);
    }
    /* This flush updates the dock icon */
    CGContextFlush (context);
    EndCGContextForApplicationDockTile (context);

    CGColorSpaceRelease (cs);
}

/**
 * Updates the dock preview image.
 *
 * This method is a callback that updates the 128x128 preview image of the VM window in the dock.
 *
 * @param   aFrameBuffer    The guest frame buffer.
 * @param   aOverlayImage   an optional icon overlay image to add at the bottom right of the icon
 */
void darwinUpdateDockPreview (QWidget *aMainWindow, VBoxFrameBuffer *aFrameBuffer, CGImageRef aOverlayImage)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    Assert (cs);
    /* Create the image copy of the framebuffer */
    CGDataProviderRef dp = CGDataProviderCreateWithData (aFrameBuffer, aFrameBuffer->address(), aFrameBuffer->bitsPerPixel() / 8 * aFrameBuffer->width() * aFrameBuffer->height(), NULL);
    Assert (dp);
    CGImageRef ir = CGImageCreate (aFrameBuffer->width(), aFrameBuffer->height(), 8, 32, aFrameBuffer->bytesPerLine(), cs,
                                   kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host, dp, 0, false,
                                   kCGRenderingIntentDefault);
    /* Update the dock preview icon */
    ::darwinUpdateDockPreview (aMainWindow, ir, aOverlayImage);
    /* Release the temp data and image */
    CGDataProviderRelease (dp);
    CGImageRelease (ir);
    CGColorSpaceRelease (cs);
}

QString darwinSystemLanguage()
{
    /* Get the locales supported by our bundle */
    CFArrayRef supportedLocales = CFBundleCopyBundleLocalizations (CFBundleGetMainBundle());
    /* Check them against the languages currently selected by the user */
    CFArrayRef preferredLocales = CFBundleCopyPreferredLocalizationsFromArray (supportedLocales);
    /* Get the one which is on top */
    CFStringRef localeId = (CFStringRef)CFArrayGetValueAtIndex (preferredLocales, 0);
    /* Convert them to a C-string */
    char localeName[20];
    CFStringGetCString (localeId, localeName, sizeof (localeName), kCFStringEncodingUTF8);
    /* Some cleanup */
    CFRelease (supportedLocales);
    CFRelease (preferredLocales);
    QString id(localeName);
    /* Check for some misbehavior */
    if (id.isEmpty() ||
        id.toLower() == "english")
        id = "en";
    return id;
}

bool darwinIsMenuOpen()
{
    MenuTrackingData outData;
    return (GetMenuTrackingData (NULL, &outData) != menuNotFoundErr);
}

void darwinDisableIconsInMenus()
{
    /* No icons in the menu of a mac application. */
#if QT_VERSION < 0x040400
    qt_mac_set_menubar_icons (false);
#else /* QT_VERSION < 0x040400 */
    /* Available since Qt 4.4 only */
    QApplication::instance()->setAttribute (Qt::AA_DontShowIconsInMenus, true);
#endif /* QT_VERSION >= 0x040400 */
}

/* Currently not used! */
OSStatus darwinRegionHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData)
{
    NOREF (aInHandlerCallRef);

    OSStatus status = eventNotHandledErr;

    switch (GetEventKind (aInEvent))
    {
        case kEventWindowGetRegion:
        {
            WindowRegionCode code;
            RgnHandle rgn;

            /* which region code is being queried? */
            GetEventParameter (aInEvent, kEventParamWindowRegionCode, typeWindowRegionCode, NULL, sizeof (code), NULL, &code);

            /* if it is the opaque region code then set the region to Empty and return noErr to stop the propagation */
            if (code == kWindowOpaqueRgn)
            {
                printf("test1\n");
                GetEventParameter (aInEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (rgn), NULL, &rgn);
                SetEmptyRgn (rgn);
                status = noErr;
            }
            /* if the content of the whole window is queried return a copy of our saved region. */
            else if (code == (kWindowStructureRgn))// || kWindowGlobalPortRgn || kWindowUpdateRgn))
            {
                printf("test2\n");
                GetEventParameter (aInEvent, kEventParamRgnHandle, typeQDRgnHandle, NULL, sizeof (rgn), NULL, &rgn);
                QRegion *pRegion = static_cast <QRegion*> (aInUserData);
                if (!pRegion->isEmpty() && pRegion)
                {
                    CopyRgn (pRegion->handle(), rgn);
                    status = noErr;
                }
            }
            break;
        }
        case kEventControlDraw:
        {
            printf("test3\n");
            CGContextRef ctx;
            HIRect bounds;

            GetEventParameter (aInEvent, kEventParamCGContextRef, typeCGContextRef, NULL, sizeof (ctx), NULL, &ctx);
            HIViewGetBounds ((HIViewRef)aInUserData, &bounds);

            CGContextClearRect (ctx, bounds);
            status = noErr;
            break;
        }
    }

    return status;
}

OSStatus darwinOverlayWindowHandler (EventHandlerCallRef aInHandlerCallRef, EventRef aInEvent, void *aInUserData)
{
    if (!aInUserData)
        return ::CallNextEventHandler (aInHandlerCallRef, aInEvent);

    UInt32 eventClass = ::GetEventClass (aInEvent);
    UInt32 eventKind = ::GetEventKind (aInEvent);
    /* For debugging events */
    /*
    if (!(eventClass == 'cute'))
        ::darwinDebugPrintEvent ("view: ", aInEvent);
    */
    VBoxConsoleView *view = static_cast<VBoxConsoleView *> (aInUserData);

    if (eventClass == kEventClassVBox)
    {
        if (eventKind == kEventVBoxShowWindow)
        {
//            printf ("ShowWindow requested\n");
            WindowRef w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            ShowWindow (w);
            SelectWindow (w);
            return noErr;
        }
        if (eventKind == kEventVBoxMoveWindow)
        {
//            printf ("MoveWindow requested\n");
            WindowPtr w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            HIPoint p;
            if (GetEventParameter (aInEvent, kEventParamOrigin, typeHIPoint, NULL, sizeof (p), NULL, &p) != noErr)
                return noErr;
            ChangeWindowGroupAttributes (GetWindowGroup (w), 0, kWindowGroupAttrMoveTogether);
            QPoint p1 = view->mapToGlobal (QPoint (p.x, p.y));
            MoveWindow (w, p1.x(), p1.y(), true);
            ChangeWindowGroupAttributes (GetWindowGroup (w), kWindowGroupAttrMoveTogether, 0);
            return noErr;
        }
        if (eventKind == kEventVBoxResizeWindow)
        {
//            printf ("ResizeWindow requested\n");
            WindowPtr w;
            if (GetEventParameter (aInEvent, kEventParamWindowRef, typeWindowRef, NULL, sizeof (w), NULL, &w) != noErr)
                return noErr;
            HISize s;
            if (GetEventParameter (aInEvent, kEventParamDimensions, typeHISize, NULL, sizeof (s), NULL, &s) != noErr)
                return noErr;
            ChangeWindowGroupAttributes (GetWindowGroup (w), 0, kWindowGroupAttrMoveTogether);
            SizeWindow (w, s.width, s.height, true);
            ChangeWindowGroupAttributes (GetWindowGroup (w), kWindowGroupAttrMoveTogether, 0);
            return noErr;
        }
        if (eventKind == kEventVBoxUpdateDock)
        {
//            printf ("UpdateDock requested\n");
            view->updateDockIcon();
            return noErr;
        }
    }

    return ::CallNextEventHandler (aInHandlerCallRef, aInEvent);
}



/* Event debugging stuff. Borrowed from Knuts Qt patch. */
#ifdef DEBUG

# define MY_CASE(a) case a: return #a
const char * DarwinDebugEventName (UInt32 ekind)
{
    switch (ekind)
    {
        MY_CASE(kEventWindowUpdate                );
        MY_CASE(kEventWindowDrawContent           );
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
        MY_CASE(kEventWindowClickDragRgn          );
        MY_CASE(kEventWindowClickResizeRgn        );
        MY_CASE(kEventWindowClickCollapseRgn      );
        MY_CASE(kEventWindowClickCloseRgn         );
        MY_CASE(kEventWindowClickZoomRgn          );
        MY_CASE(kEventWindowClickContentRgn       );
        MY_CASE(kEventWindowClickProxyIconRgn     );
        MY_CASE(kEventWindowClickToolbarButtonRgn );
        MY_CASE(kEventWindowClickStructureRgn     );
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
        MY_CASE(kEventWindowHandleContentClick    );
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

void darwinDebugPrintEvent (const char *psz, EventRef event)
{
  if (!event)
      return;
  UInt32 ekind = GetEventKind (event), eclass = GetEventClass (event);
  if (eclass == kEventClassWindow)
  {
      switch (ekind)
      {
          case kEventWindowDrawContent:
          case kEventWindowUpdate:
          case kEventWindowBoundsChanged:
              break;
          default:
          {
              WindowRef wid = NULL;
              GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &wid);
              QWidget *widget = QWidget::find((WId)wid);
              printf("%d %s: (%s) %#x win=%p wid=%p (%s)\n", (int)time(NULL), psz, darwinDebugClassName (eclass), (uint)ekind, wid, widget, DarwinDebugEventName (ekind));
              break;
          }
      }
  }
  else if (eclass == kEventClassCommand)
  {
      WindowRef wid = NULL;
      GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &wid);
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
      printf("%d %s: %#x(%s) %#x (kEventClassKeyboard)\n", (int)time(NULL), psz, (uint)eclass, darwinDebugClassName (eclass), (uint)ekind);

  else
      printf("%d %s: %#x(%s) %#x\n", (int)time(NULL), psz, (uint)eclass, darwinDebugClassName (eclass), (uint)ekind);
}

#endif /* DEBUG */
