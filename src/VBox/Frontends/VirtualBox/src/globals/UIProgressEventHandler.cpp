/* $Id$ */
/** @file
 * VBox Qt GUI - UIProgressEventHandler class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UIExtraDataManager.h"
#include "UIMainEventListener.h"
#include "UIProgressEventHandler.h"
#include "VBoxGlobal.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* VBOX_WS_MAC */

UIProgressEventHandler::UIProgressEventHandler(QObject *pParent, const CProgress &comProgress)
    : QObject(pParent)
    , m_comProgress(comProgress)
{
    /* Prepare: */
    prepare();
}

UIProgressEventHandler::~UIProgressEventHandler()
{
    /* Cleanup: */
    cleanup();
}

void UIProgressEventHandler::prepare()
{
    /* Prepare: */
    prepareListener();
    prepareConnections();
}

void UIProgressEventHandler::prepareListener()
{
    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

    /* Get CProgress event source: */
    CEventSource comEventSourceProgress = m_comProgress.GetEventSource();
    AssertWrapperOk(comEventSourceProgress);

    /* Enumerate all the required event-types: */
    QVector<KVBoxEventType> eventTypes;
    eventTypes
        << KVBoxEventType_OnProgressPercentageChanged
        << KVBoxEventType_OnProgressTaskCompleted;

    /* Register event listener for CProgress event source: */
    comEventSourceProgress.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);
    AssertWrapperOk(comEventSourceProgress);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        m_pQtListener->getWrapped()->registerSource(comEventSourceProgress, m_comEventListener);
    }
}

void UIProgressEventHandler::prepareConnections()
{
    /* Create direct (sync) connections for signals of main listener: */
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigProgressPercentageChange,
            this, &UIProgressEventHandler::sigProgressPercentageChange,
            Qt::DirectConnection);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigProgressTaskComplete,
            this, &UIProgressEventHandler::sigProgressTaskComplete,
            Qt::DirectConnection);
}

void UIProgressEventHandler::cleanupConnections()
{
    /* Nothing for now. */
}

void UIProgressEventHandler::cleanupListener()
{
    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Unregister everything: */
        m_pQtListener->getWrapped()->unregisterSources();
    }

    /* Make sure VBoxSVC is available: */
    if (!vboxGlobal().isVBoxSVCAvailable())
        return;

    /* Get CProgress event source: */
    CEventSource comEventSourceProgress = m_comProgress.GetEventSource();
    AssertWrapperOk(comEventSourceProgress);

    /* Unregister event listener for CProgress event source: */
    comEventSourceProgress.UnregisterListener(m_comEventListener);
}

void UIProgressEventHandler::cleanup()
{
    /* Cleanup: */
    cleanupConnections();
    cleanupListener();
}
