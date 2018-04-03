/* $Id$ */
/** @file
 * VBox Qt GUI - UIDesktopWidgetWatchdog class declaration.
 */

/*
 * Copyright (C) 2015-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIDesktopWidgetWatchdog_h___
#define ___UIDesktopWidgetWatchdog_h___

/* Qt includes: */
#include <QObject>
#ifdef VBOX_WS_X11
# include <QVector>
# include <QRect>
#endif

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QScreen;

/** Singleton QObject extension used as desktop-widget
  * watchdog aware of the host-screen geometry changes. */
class SHARED_LIBRARY_STUFF UIDesktopWidgetWatchdog : public QObject
{
    Q_OBJECT;

    /** Constructs desktop-widget watchdog. */
    UIDesktopWidgetWatchdog();
    /** Destructs desktop-widget watchdog. */
    ~UIDesktopWidgetWatchdog();

signals:

    /** Notifies about host-screen count change to @a cHostScreenCount. */
    void sigHostScreenCountChanged(int cHostScreenCount);

    /** Notifies about resize for the host-screen with @a iHostScreenIndex. */
    void sigHostScreenResized(int iHostScreenIndex);

    /** Notifies about work-area resize for the host-screen with @a iHostScreenIndex. */
    void sigHostScreenWorkAreaResized(int iHostScreenIndex);

#ifdef VBOX_WS_X11
    /** Notifies about work-area recalculated for the host-screen with @a iHostScreenIndex. */
    void sigHostScreenWorkAreaRecalculated(int iHostScreenIndex);
#endif

public:

    /** Returns the static instance of the desktop-widget watchdog. */
    static UIDesktopWidgetWatchdog *instance() { return s_pInstance; }

    /** Creates the static instance of the desktop-widget watchdog. */
    static void create();
    /** Destroys the static instance of the desktop-widget watchdog. */
    static void destroy();

    /** Returns overall desktop width. */
    int overallDesktopWidth() const;
    /** Returns overall desktop height. */
    int overallDesktopHeight() const;

    /** Returns the number of host-screens currently available on the system. */
    int screenCount() const;

    /** Returns the index of the screen which contains contains @a pWidget. */
    int screenNumber(const QWidget *pWidget) const;
    /** Returns the index of the screen which contains contains @a point. */
    int screenNumber(const QPoint &point) const;

    /** Returns the geometry of the host-screen with @a iHostScreenIndex.
      * @note The default screen is used if @a iHostScreenIndex is -1. */
    const QRect screenGeometry(int iHostScreenIndex = -1) const;
    /** Returns the geometry of the host-screen which contains @a pWidget. */
    const QRect screenGeometry(const QWidget *pWidget) const;
    /** Returns the geometry of the host-screen which contains @a point. */
    const QRect screenGeometry(const QPoint &point) const;

    /** Returns the available-geometry of the host-screen with @a iHostScreenIndex.
      * @note The default screen is used if @a iHostScreenIndex is -1. */
    const QRect availableGeometry(int iHostScreenIndex = -1) const;
    /** Returns the available-geometry of the host-screen which contains @a pWidget. */
    const QRect availableGeometry(const QWidget *pWidget) const;
    /** Returns the available-geometry of the host-screen which contains @a point. */
    const QRect availableGeometry(const QPoint &point) const;

    /** Returns overall region unifying all the host-screen geometries. */
    const QRegion overallScreenRegion() const;
    /** Returns overall region unifying all the host-screen available-geometries. */
    const QRegion overallAvailableRegion() const;

#ifdef VBOX_WS_X11
    /** Qt5: X11: Returns whether no or fake screen detected. */
    bool isFakeScreenDetected() const;
#endif

    /** Returns device-pixel-ratio of the host-screen with @a iHostScreenIndex. */
    double devicePixelRatio(int iHostScreenIndex = -1);
    /** Returns device-pixel-ratio of the host-screen which contains @a pWidget. */
    double devicePixelRatio(QWidget *pWidget);

    /** Returns actual device-pixel-ratio of the host-screen with @a iHostScreenIndex. */
    double devicePixelRatioActual(int iHostScreenIndex = -1);
    /** Returns actual device-pixel-ratio of the host-screen which contains @a pWidget. */
    double devicePixelRatioActual(QWidget *pWidget);

private slots:

#if QT_VERSION == 0
    /** Stupid moc does not warn if it cannot find headers! */
    void QT_VERSION_NOT_DEFINED
#else /* QT_VERSION != 0 */
    /** Handles @a pHostScreen adding. */
    void sltHostScreenAdded(QScreen *pHostScreen);
    /** Handles @a pHostScreen removing. */
    void sltHostScreenRemoved(QScreen *pHostScreen);
    /** Handles host-screen resize to passed @a geometry. */
    void sltHandleHostScreenResized(const QRect &geometry);
    /** Handles host-screen work-area resize to passed @a availableGeometry. */
    void sltHandleHostScreenWorkAreaResized(const QRect &availableGeometry);
#endif /* QT_VERSION != 0 */

#ifdef VBOX_WS_X11
    /** Handles @a availableGeometry calculation result for the host-screen with @a iHostScreenIndex. */
    void sltHandleHostScreenAvailableGeometryCalculated(int iHostScreenIndex, QRect availableGeometry);
#endif

private:

    /** Prepare routine. */
    void prepare();
    /** Cleanup routine. */
    void cleanup();

    /** Holds the static instance of the desktop-widget watchdog. */
    static UIDesktopWidgetWatchdog *s_pInstance;

#ifdef VBOX_WS_X11
    /** Updates host-screen configuration according to new @a cHostScreenCount.
      * @note If cHostScreenCount is equal to -1 we have to acquire it ourselves. */
    void updateHostScreenConfiguration(int cHostScreenCount = -1);

    /** Update available-geometry for the host-screen with @a iHostScreenIndex. */
    void updateHostScreenAvailableGeometry(int iHostScreenIndex);

    /** Cleanups existing workers. */
    void cleanupExistingWorkers();

    /** Holds current host-screen available-geometries. */
    QVector<QRect> m_availableGeometryData;
    /** Holds current workers determining host-screen available-geometries. */
    QVector<QWidget*> m_availableGeometryWorkers;
#endif /* VBOX_WS_X11 */
};

/** 'Official' name for the desktop-widget watchdog singleton. */
#define gpDesktop UIDesktopWidgetWatchdog::instance()

#endif /* !___UIDesktopWidgetWatchdog_h___ */

