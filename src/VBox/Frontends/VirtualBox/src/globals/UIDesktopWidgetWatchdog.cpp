/* $Id$ */
/** @file
 * VBox Qt GUI - UIDesktopWidgetWatchdog class implementation.
 */

/*
 * Copyright (C) 2015-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QApplication>
# include <QDesktopWidget>
# if QT_VERSION >= 0x050000
#  include <QScreen>
# endif /* QT_VERSION >= 0x050000 */

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIDesktopWidgetWatchdog.h"

/* Other VBox includes: */
# include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QWidget extension used as
  * an invisible window on the basis of which we
  * can calculate available host-screen geometry. */
class UIInvisibleWindow : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about host-screen available-geometry was calulated.
      * @param iHostScreenIndex  holds the index of the host-screen this window created for.
      * @param availableGeometry holds the available-geometry of the host-screen this window created for. */
    void sigHostScreenAvailableGeometryCalculated(int iHostScreenIndex, QRect availableGeometry);

public:

    /** Constructs invisible window for the host-screen with @a iHostScreenIndex. */
    UIInvisibleWindow(int iHostScreenIndex);

private:

    /** Resize @a pEvent handler. */
    void resizeEvent(QResizeEvent *pEvent);

    /** Holds the index of the host-screen this window created for. */
    int m_iHostScreenIndex;
};


/*********************************************************************************************************************************
*   Class UIInvisibleWindow implementation.                                                                                      *
*********************************************************************************************************************************/

UIInvisibleWindow::UIInvisibleWindow(int iHostScreenIndex)
    : QWidget(0, Qt::Window | Qt::FramelessWindowHint)
    , m_iHostScreenIndex(iHostScreenIndex)
{
    /* Resize to minimum size of 1 pixel: */
    resize(1, 1);
    /* Apply visual and mouse-event mask for that 1 pixel: */
    setMask(QRect(0, 0, 1, 1));
#ifdef VBOX_WS_X11
    /* For composite WMs make this 1 pixel transparent: */
    if (vboxGlobal().isCompositingManagerRunning())
        setAttribute(Qt::WA_TranslucentBackground);
#else /* !VBOX_WS_X11 */
    /* Make this 1 pixel transparent: */
    setAttribute(Qt::WA_TranslucentBackground);
#endif /* !VBOX_WS_X11 */
}

void UIInvisibleWindow::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QWidget::resizeEvent(pEvent);

    /* Ignore 'not-yet-shown' case: */
    if (!isVisible())
        return;

    /* Notify listeners about host-screen available-geometry was calulated: */
    emit sigHostScreenAvailableGeometryCalculated(m_iHostScreenIndex, QRect(x(), y(), width(), height()));
}


/*********************************************************************************************************************************
*   Class UIDesktopWidgetWatchdog implementation.                                                                                *
*********************************************************************************************************************************/

UIDesktopWidgetWatchdog::UIDesktopWidgetWatchdog(QObject *pParent)
    : QObject(pParent)
{
    /* Prepare: */
    prepare();
}

UIDesktopWidgetWatchdog::~UIDesktopWidgetWatchdog()
{
    /* Cleanup: */
    cleanup();
}

int UIDesktopWidgetWatchdog::screenCount() const
{
    /* Redirect call to desktop-widget: */
    return QApplication::desktop()->screenCount();
}

const QRect UIDesktopWidgetWatchdog::screenGeometry(int iHostScreenIndex /* = -1 */) const
{
    /* Make sure index is valid: */
    if (iHostScreenIndex < 0 || iHostScreenIndex >= screenCount())
        iHostScreenIndex = QApplication::desktop()->primaryScreen();
    AssertReturn(iHostScreenIndex >= 0 && iHostScreenIndex < screenCount(), QRect());

    /* Redirect call to desktop-widget: */
    return QApplication::desktop()->screenGeometry(iHostScreenIndex);
}

const QRect UIDesktopWidgetWatchdog::availableGeometry(int iHostScreenIndex /* = -1 */) const
{
    /* Make sure index is valid: */
    if (iHostScreenIndex < 0 || iHostScreenIndex >= screenCount())
        iHostScreenIndex = QApplication::desktop()->primaryScreen();
    AssertReturn(iHostScreenIndex >= 0 && iHostScreenIndex < screenCount(), QRect());

    /* Return cached available-geometry: */
    return m_availableGeometryData.value(iHostScreenIndex);
}

#if QT_VERSION < 0x050000

void UIDesktopWidgetWatchdog::sltHandleHostScreenCountChanged(int cHostScreenCount)
{
//    printf("UIDesktopWidgetWatchdog::sltHandleHostScreenCountChanged(%d)\n", cHostScreenCount);

    /* Update host-screen configuration: */
    updateHostScreenConfiguration(cHostScreenCount);
}

#else /* QT_VERSION >= 0x050000 */

void UIDesktopWidgetWatchdog::sltHostScreenAdded(QScreen *pHostScreen)
{
//    printf("UIDesktopWidgetWatchdog::sltHostScreenAdded(%d)\n", screenCount());

    /* Update host-screen configuration: */
    updateHostScreenConfiguration();
}

void UIDesktopWidgetWatchdog::sltHostScreenRemoved(QScreen *pHostScreen)
{
//    printf("UIDesktopWidgetWatchdog::sltHostScreenRemoved(%d)\n", screenCount());

    /* Update host-screen configuration: */
    updateHostScreenConfiguration();
}

#endif /* QT_VERSION >= 0x050000 */

void UIDesktopWidgetWatchdog::sltHandleHostScreenResized(int iHostScreenIndex)
{
//    printf("UIDesktopWidgetWatchdog::sltHandleHostScreenResized(%d)\n", iHostScreenIndex);

    /* Update host-screen available-geometry: */
    updateHostScreenAvailableGeometry(iHostScreenIndex);
}

void UIDesktopWidgetWatchdog::sltHandleHostScreenAvailableGeometryCalculated(int iHostScreenIndex, QRect availableGeometry)
{
//    printf("UIDesktopWidgetWatchdog::sltHandleHostScreenAvailableGeometryCalculated(%d): %dx%d x %dx%d\n",
//           iHostScreenIndex, availableGeometry.x(), availableGeometry.y(), availableGeometry.width(), availableGeometry.height());

    /* Apply received data: */
    m_availableGeometryData[iHostScreenIndex] = availableGeometry;
    /* Forget finished worker: */
    AssertPtrReturnVoid(m_availableGeometryWorkers.value(iHostScreenIndex));
    m_availableGeometryWorkers.value(iHostScreenIndex)->disconnect();
    m_availableGeometryWorkers.value(iHostScreenIndex)->deleteLater();
    m_availableGeometryWorkers[iHostScreenIndex] = 0;
}

void UIDesktopWidgetWatchdog::prepare()
{
    /* Prepare connections: */
#if QT_VERSION < 0x050000
    connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), this, SLOT(sltHandleHostScreenCountChanged(int)));
#else /* QT_VERSION >= 0x050000 */
    connect(qApp, SIGNAL(screenAdded(QScreen *)), this, SLOT(sltHostScreenAdded(QScreen *)));
    connect(qApp, SIGNAL(screenRemoved(QScreen *)), this, SLOT(sltHostScreenRemoved(QScreen *)));
#endif /* QT_VERSION >= 0x050000 */
    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(sltHandleHostScreenResized(int)));

    /* Update host-screen configuration: */
    updateHostScreenConfiguration();
}

void UIDesktopWidgetWatchdog::cleanup()
{
    /* Cleanup connections: */
#if QT_VERSION < 0x050000
    disconnect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), this, SLOT(sltHandleHostScreenCountChanged(int)));
#else /* QT_VERSION >= 0x050000 */
    disconnect(qApp, SIGNAL(screenAdded(QScreen *)), this, SLOT(sltHostScreenAdded(QScreen *)));
    disconnect(qApp, SIGNAL(screenRemoved(QScreen *)), this, SLOT(sltHostScreenRemoved(QScreen *)));
#endif /* QT_VERSION >= 0x050000 */
    disconnect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(sltHandleHostScreenResized(int)));

    /* Cleanup existing workers finally: */
    cleanupExistingWorkers();
}

void UIDesktopWidgetWatchdog::updateHostScreenConfiguration(int cHostScreenCount /* = -1 */)
{
    /* Acquire new host-screen count: */
    if (cHostScreenCount == -1)
        cHostScreenCount = screenCount();

    /* Cleanup existing workers first: */
    cleanupExistingWorkers();

    /* Resize workers vectors to new host-screen count: */
    m_availableGeometryWorkers.resize(cHostScreenCount);
    m_availableGeometryData.resize(cHostScreenCount);

    /* Update host-screen available-geometry for each particular host-screen: */
    for (int iHostScreenIndex = 0; iHostScreenIndex < cHostScreenCount; ++iHostScreenIndex)
        updateHostScreenAvailableGeometry(iHostScreenIndex);
}

void UIDesktopWidgetWatchdog::updateHostScreenAvailableGeometry(int iHostScreenIndex)
{
    /* Make sure index is valid: */
    if (iHostScreenIndex < 0 || iHostScreenIndex >= screenCount())
        iHostScreenIndex = QApplication::desktop()->primaryScreen();
    AssertReturnVoid(iHostScreenIndex >= 0 && iHostScreenIndex < screenCount());

    /* Create invisible frame-less window worker: */
    UIInvisibleWindow *pWorker = new UIInvisibleWindow(iHostScreenIndex);
    AssertPtrReturnVoid(pWorker);
    {
        /* Remember created worker (replace if necessary): */
        if (m_availableGeometryWorkers.value(iHostScreenIndex))
            delete m_availableGeometryWorkers.value(iHostScreenIndex);
        m_availableGeometryWorkers[iHostScreenIndex] = pWorker;

        /* Get the screen-geometry: */
        const QRect hostScreenGeometry = screenGeometry(iHostScreenIndex);
        /* Use the screen-geometry as the temporary value for available-geometry: */
        m_availableGeometryData[iHostScreenIndex] = hostScreenGeometry;

        /* Connect worker listener: */
        connect(pWorker, SIGNAL(sigHostScreenAvailableGeometryCalculated(int, QRect)),
                this, SLOT(sltHandleHostScreenAvailableGeometryCalculated(int, QRect)));

        /* Place worker to corresponding host-screen: */
        pWorker->move(hostScreenGeometry.topLeft());
        /* And finally, maximize it: */
        pWorker->showMaximized();
    }
}

void UIDesktopWidgetWatchdog::cleanupExistingWorkers()
{
    /* Destroy existing workers: */
    qDeleteAll(m_availableGeometryWorkers);
    /* And clear their vectors: */
    m_availableGeometryWorkers.clear();
    m_availableGeometryData.clear();
}

#include "UIDesktopWidgetWatchdog.moc"

