/* $Id$ */
/** @file
 * VBox Qt GUI - UIDesktopWidgetWatchdog class implementation.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QApplication>
#include <QDesktopWidget>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIDesktopWidgetWatchdog.h"

/* Other VBox includes: */
#include <iprt/assert.h>


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

UIDesktopWidgetWatchdog::UIDesktopWidgetWatchdog(QObject *pParent)
    : QObject(pParent)
    , m_pDesktopWidget(QApplication::desktop())
    , m_cHostScreenCount(-1)
{
    /* Prepare: */
    prepare();
}

UIDesktopWidgetWatchdog::~UIDesktopWidgetWatchdog()
{
    /* Cleanup: */
    cleanup();
}

const QRect	UIDesktopWidgetWatchdog::screenGeometry(int iHostScreenIndex /* = -1 */) const
{
    /* Make sure index is valid: */
    if (iHostScreenIndex < 0 || iHostScreenIndex >= m_cHostScreenCount)
        iHostScreenIndex = m_pDesktopWidget->primaryScreen();
    AssertReturn(iHostScreenIndex >= 0 && iHostScreenIndex < m_cHostScreenCount, QRect());

    /* Redirect call to desktop-widget: */
    return m_pDesktopWidget->screenGeometry(iHostScreenIndex);
}

const QRect UIDesktopWidgetWatchdog::availableGeometry(int iHostScreenIndex /* = -1 */) const
{
    /* Make sure index is valid: */
    if (iHostScreenIndex < 0 || iHostScreenIndex >= m_cHostScreenCount)
        iHostScreenIndex = m_pDesktopWidget->primaryScreen();
    AssertReturn(iHostScreenIndex >= 0 && iHostScreenIndex < m_cHostScreenCount, QRect());

    /* Return cached available-geometry: */
    return m_availableGeometryData.value(iHostScreenIndex);
}

void UIDesktopWidgetWatchdog::sltUpdateHostScreenConfiguration(int cHostScreenCount /* = -1 */)
{
//    printf("UIDesktopWidgetWatchdog::sltUpdateHostScreenConfiguration(%d)\n", cHostScreenCount);

    /* Acquire new host-screen count: */
    m_cHostScreenCount = cHostScreenCount != -1 ? cHostScreenCount : m_pDesktopWidget->screenCount();

    /* Cleanup existing workers first: */
    foreach (QWidget *pWorker, m_availableGeometryWorkers)
        pWorker->disconnect();
    qDeleteAll(m_availableGeometryWorkers);
    m_availableGeometryWorkers.clear();
    m_availableGeometryData.clear();

    /* Resize workers vectors to new host-screen count: */
    m_availableGeometryWorkers.resize(m_cHostScreenCount);
    m_availableGeometryData.resize(m_cHostScreenCount);

    /* Calculate host-screen available-geometry for each particular host-screen: */
    for (int iHostScreenIndex = 0; iHostScreenIndex < m_cHostScreenCount; ++iHostScreenIndex)
        sltRecalculateHostScreenAvailableGeometry(iHostScreenIndex);
}

void UIDesktopWidgetWatchdog::sltRecalculateHostScreenAvailableGeometry(int iHostScreenIndex)
{
//    printf("UIDesktopWidgetWatchdog::sltRecalculateHostScreenAvailableGeometry(%d)\n", iHostScreenIndex);

    /* Make sure index is valid: */
    if (iHostScreenIndex < 0 || iHostScreenIndex >= m_cHostScreenCount)
        iHostScreenIndex = m_pDesktopWidget->primaryScreen();
    AssertReturnVoid(iHostScreenIndex >= 0 && iHostScreenIndex < m_cHostScreenCount);

    /* Create invisible frame-less window worker: */
    UIInvisibleWindow *pWorker = new UIInvisibleWindow(iHostScreenIndex);
    AssertPtrReturnVoid(pWorker);
    {
        /* Remember created worker: */
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

void UIDesktopWidgetWatchdog::sltHandleHostScreenAvailableGeometryCalculated(int iHostScreenIndex, QRect availableGeometry)
{
//    printf("UIDesktopWidgetWatchdog::sltHandleHostScreenAvailableGeometryCalculated(%d): %dx%d x %dx%d\n",
//           iHostScreenIndex, availableGeometry.x(), availableGeometry.y(), availableGeometry.width(), availableGeometry.height());

    /* Apply received data: */
    m_availableGeometryData[iHostScreenIndex] = availableGeometry;
    /* Forget finished worker: */
    AssertPtrReturnVoid(m_availableGeometryWorkers.value(iHostScreenIndex));
    m_availableGeometryWorkers.value(iHostScreenIndex)->deleteLater();
    m_availableGeometryWorkers[iHostScreenIndex] = 0;
}

void UIDesktopWidgetWatchdog::prepare()
{
    /* Prepare connections: */
    connect(m_pDesktopWidget, SIGNAL(screenCountChanged(int)), this, SLOT(sltUpdateHostScreenConfiguration(int)));
    connect(m_pDesktopWidget, SIGNAL(resized(int)), this, SLOT(sltRecalculateHostScreenAvailableGeometry(int)));

    /* Update host-screen configuration: */
    sltUpdateHostScreenConfiguration();
}

void UIDesktopWidgetWatchdog::cleanup()
{
    /* Cleanup connections: */
    disconnect(m_pDesktopWidget, SIGNAL(screenCountChanged(int)), this, SLOT(sltUpdateHostScreenConfiguration(int)));
    disconnect(m_pDesktopWidget, SIGNAL(resized(int)), this, SLOT(sltRecalculateHostScreenAvailableGeometry(int)));

    /* Cleanup existing workers finally: */
    foreach (QWidget *pWorker, m_availableGeometryWorkers)
        pWorker->disconnect();
    qDeleteAll(m_availableGeometryWorkers);
    m_availableGeometryWorkers.clear();
    m_availableGeometryData.clear();
}

#include "UIDesktopWidgetWatchdog.moc"

