/** @file
 * VBox Qt GUI - UIIndicatorsPool class declaration.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIIndicatorsPool_h___
#define ___UIIndicatorsPool_h___

/* Qt includes: */
#include <QWidget>
#include <QList>
#include <QMap>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class UISession;
class CSession;
class QIStatusBarIndicator;
class QHBoxLayout;
class QTimer;

/** QWidget extension
  * providing Runtime UI with status-bar indicators. */
class UIIndicatorsPool : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about context menu request.
      * @param indicatorType reflects which type of indicator it is,
      * @param position      reflects contex-menu position. */
    void sigContextMenuRequest(IndicatorType indicatorType, const QPoint &position);

public:

    /** Constructor, passes @a pParent to the QWidget constructor.
      * @param pSession is used to retrieve appearance information. */
    UIIndicatorsPool(UISession *pSession, QWidget *pParent = 0);
    /** Destructor. */
    ~UIIndicatorsPool();

    /** Returns indicator corresponding to passed @a indicatorType. */
    QIStatusBarIndicator* indicator(IndicatorType indicatorType) const { return m_pool.value(indicatorType); }

    /** Updates appearance for passed @a indicatorType. */
    void updateAppearance(IndicatorType indicatorType);

    /** Defines whether indicator-states auto-update is @a fEnabled. */
    void setAutoUpdateIndicatorStates(bool fEnabled);

private slots:

    /** Handles indicator-states auto-update. */
    void sltAutoUpdateIndicatorStates();

    /** Handles context-menu request. */
    void sltContextMenuRequest(QIStatusBarIndicator *pIndicator, QContextMenuEvent *pEvent);

private:

    /** Prepare routine. */
    void prepare();
    /** Prepare connections routine: */
    void prepareConnections();
    /** Prepare contents routine. */
    void prepareContents();
    /** Prepare update-timer routine: */
    void prepareUpdateTimer();

    /** Update pool routine. */
    void updatePool();

    /** Cleanup update-timer routine: */
    void cleanupUpdateTimer();
    /** Cleanup contents routine. */
    void cleanupContents();
    /** Cleanup routine. */
    void cleanup();

    /** Returns position for passed @a indicatorType. */
    int indicatorPosition(IndicatorType indicatorType) const;

    /** Updates state for passed @a pIndicator through @a deviceType. */
    void updateIndicatorStateForDevice(QIStatusBarIndicator *pIndicator, KDeviceType deviceType);

    /** Holds the UI session reference. */
    UISession *m_pSession;
    /** Holds the session reference. */
    CSession &m_session;
    /** Holds the cached configuration. */
    QList<IndicatorType> m_configuration;
    /** Holds cached indicator instances. */
    QMap<IndicatorType, QIStatusBarIndicator*> m_pool;
    /** Holds the main-layout instance. */
    QHBoxLayout *m_pMainLayout;
    /** Holds the auto-update timer instance. */
    QTimer *m_pTimerAutoUpdate;
};

#endif /* !___UIIndicatorsPool_h___ */
