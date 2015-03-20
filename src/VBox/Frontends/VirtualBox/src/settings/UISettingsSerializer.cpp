/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsSerializer class implementation.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
# include <QTimer>
/* GUI includes: */
# include "UISettingsSerializer.h"
# include "UISettingsPage.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UISettingsSerializer* UISettingsSerializer::m_spInstance = 0;

UISettingsSerializer::UISettingsSerializer(QObject *pParent, SerializationDirection direction,
                                           const QVariant &data, const UISettingsPageList &pages)
    : QThread(pParent)
    , m_direction(direction)
    , m_data(data)
    , m_fSavingComplete(m_direction == Load)
    , m_fAllowToDestroySerializer(m_direction == Load)
    , m_iIdOfHighPriorityPage(-1)
{
    /* Prepare instance: */
    m_spInstance = this;

    /* Copy the page(s) from incoming list to our map: */
    for (int iPageIndex = 0; iPageIndex < pages.size(); ++iPageIndex)
    {
        UISettingsPage *pPage = pages[iPageIndex];
        m_pages.insert(pPage->id(), pPage);
    }

    /* Connecting this signals: */
    connect(this, SIGNAL(sigNotifyAboutPageProcessed(int)), this, SLOT(sltHandleProcessedPage(int)), Qt::QueuedConnection);
    connect(this, SIGNAL(sigNotifyAboutPagesProcessed()), this, SLOT(sltHandleProcessedPages()), Qt::QueuedConnection);
    connect(this, SIGNAL(finished()), this, SLOT(sltDestroySerializer()), Qt::QueuedConnection);
    /* Connecting parent signals: */
    connect(this, SIGNAL(sigNotifyAboutProcessStarted()), parent(), SLOT(sltHandleProcessStarted()), Qt::QueuedConnection);
    connect(this, SIGNAL(sigNotifyAboutPageProcessed(int)), parent(), SLOT(sltHandlePageProcessed()), Qt::QueuedConnection);
}

UISettingsSerializer::~UISettingsSerializer()
{
    /* If serializer is being destructed by it's parent,
     * thread could still be running, we have to wait
     * for it to be finished! */
    if (isRunning())
        wait();

    /* Cleanup instance: */
    m_spInstance = 0;
}

void UISettingsSerializer::raisePriorityOfPage(int iPageId)
{
    /* If that page is present and was not processed already =>
     * we should remember which page should be processed next: */
    if (m_pages.contains(iPageId) && !(m_pages[iPageId]->processed()))
        m_iIdOfHighPriorityPage = iPageId;
}

void UISettingsSerializer::start(Priority priority /* = InheritPriority */)
{
    /* Notify listeners about we are starting: */
    emit sigNotifyAboutProcessStarted();

    /* If serializer saves settings: */
    if (m_direction == Save)
    {
        /* We should update internal page cache first: */
        foreach (UISettingsPage *pPage, m_pages.values())
            pPage->putToCache();
    }

    /* Start async serializing thread: */
    QThread::start(priority);

    /* If serializer saves settings: */
    if (m_direction == Save)
    {
        /* We should block calling thread until all pages will be saved: */
        while (!m_fSavingComplete)
        {
            /* Lock mutex initially: */
            m_mutex.lock();
            /* Perform idle-processing every 100ms,
             * and waiting for direct wake up signal: */
            m_condition.wait(&m_mutex, 100);
            /* Process queued signals posted to GUI thread: */
            qApp->processEvents();
            /* Unlock mutex finally: */
            m_mutex.unlock();
        }
        /* Allow to destroy serializer finally: */
        m_fAllowToDestroySerializer = true;
    }
}

void UISettingsSerializer::sltHandleProcessedPage(int iPageId)
{
    /* If serializer loads settings: */
    if (m_direction == Load)
    {
        /* If such page present: */
        if (m_pages.contains(iPageId))
        {
            /* We should fetch internal page cache: */
            UISettingsPage *pSettingsPage = m_pages[iPageId];
            pSettingsPage->setValidatorBlocked(true);
            pSettingsPage->getFromCache();
            pSettingsPage->setValidatorBlocked(false);
        }
    }
}

void UISettingsSerializer::sltHandleProcessedPages()
{
    /* If serializer saves settings: */
    if (m_direction == Save)
    {
        /* We should flag GUI thread to unlock itself: */
        if (!m_fSavingComplete)
            m_fSavingComplete = true;
    }
    /* If serializer loads settings: */
    else
    {
        /* We have to do initial validation finally: */
        foreach (UISettingsPage *pPage, m_pages.values())
            pPage->revalidate();
    }
}

void UISettingsSerializer::sltDestroySerializer()
{
    /* If not yet all events were processed,
     * we should postpone destruction for now: */
    if (!m_fAllowToDestroySerializer)
        QTimer::singleShot(0, this, SLOT(sltDestroySerializer()));
    else
        deleteLater();
}

void UISettingsSerializer::run()
{
    /* Initialize COM for other thread: */
    COMBase::InitializeCOM(false);

    /* Mark all the pages initially as NOT processed: */
    foreach (UISettingsPage *pPage, m_pages.values())
        pPage->setProcessed(false);

    /* Iterate over the all left settings pages: */
    UISettingsPageMap pages(m_pages);
    while (!pages.empty())
    {
        /* Get required page pointer, protect map by mutex while getting pointer: */
        UISettingsPage *pPage = m_iIdOfHighPriorityPage != -1 && pages.contains(m_iIdOfHighPriorityPage) ?
                                pages.value(m_iIdOfHighPriorityPage) : *pages.begin();
        /* Reset request of high priority: */
        if (m_iIdOfHighPriorityPage != -1)
            m_iIdOfHighPriorityPage = -1;
        /* Process this page if its enabled: */
        if (pPage->isEnabled())
        {
            if (m_direction == Load)
                pPage->loadToCacheFrom(m_data);
            if (m_direction == Save)
                pPage->saveFromCacheTo(m_data);
        }
        /* Remember what page was processed: */
        pPage->setProcessed(true);
        /* Remove processed page from our map: */
        pages.remove(pPage->id());
        /* Notify listeners about page was processed: */
        emit sigNotifyAboutPageProcessed(pPage->id());
        /* If serializer saves settings => wake up GUI thread: */
        if (m_direction == Save)
            m_condition.wakeAll();
        /* Break further processing if page had failed: */
        if (pPage->failed())
            break;
    }
    /* Notify listeners about all pages were processed: */
    emit sigNotifyAboutPagesProcessed();
    /* If serializer saves settings => wake up GUI thread: */
    if (m_direction == Save)
        m_condition.wakeAll();

    /* Deinitialize COM for other thread: */
    COMBase::CleanupCOM();
}

