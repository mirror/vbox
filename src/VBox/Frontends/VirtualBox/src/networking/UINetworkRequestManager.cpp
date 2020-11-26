/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkRequestManager stuff implementation.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
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
#include <QWidget>
#include <QUrl>

/* GUI includes: */
#include "UICommon.h"
#include "UINetworkCustomer.h"
#include "UINetworkRequestManager.h"
#include "UINetworkRequestManagerWindow.h"
#include "UINetworkRequestManagerIndicator.h"
#include "UINetworkRequest.h"


UINetworkRequestManager* UINetworkRequestManager::s_pInstance = 0;

void UINetworkRequestManager::create()
{
    /* Check that instance do NOT exist: */
    if (s_pInstance)
        return;

    /* Create instance: */
    new UINetworkRequestManager;

    /* Prepare instance: */
    s_pInstance->prepare();
}

void UINetworkRequestManager::destroy()
{
    /* Check that instance exists: */
    if (!s_pInstance)
        return;

    /* Cleanup instance: */
    s_pInstance->cleanup();

    /* Destroy instance: */
    delete s_pInstance;
}

UINetworkRequestManagerWindow *UINetworkRequestManager::window() const
{
    return m_pNetworkManagerDialog;
}

UINetworkRequestManagerIndicator *UINetworkRequestManager::createIndicator() const
{
    /* For Selector UI only: */
    AssertReturn(uiCommon().uiType() == UICommon::UIType_SelectorUI, 0);

    /* Create network-manager state-indicator: */
    UINetworkRequestManagerIndicator *pNetworkManagerIndicator = new UINetworkRequestManagerIndicator;
    connect(pNetworkManagerIndicator, &UINetworkRequestManagerIndicator::sigMouseDoubleClick,
            this, &UINetworkRequestManager::show);
    connect(this, &UINetworkRequestManager::sigAddNetworkManagerIndicatorDescription,
            pNetworkManagerIndicator, &UINetworkRequestManagerIndicator::sltAddNetworkManagerIndicatorDescription);
    connect(this, &UINetworkRequestManager::sigRemoveNetworkManagerIndicatorDescription,
            pNetworkManagerIndicator, &UINetworkRequestManagerIndicator::sldRemoveNetworkManagerIndicatorDescription);
    return pNetworkManagerIndicator;
}

void UINetworkRequestManager::registerNetworkRequest(UINetworkRequest *pNetworkRequest)
{
    /* Add network-request widget to network-manager dialog: */
    m_pNetworkManagerDialog->addNetworkRequestWidget(pNetworkRequest);

    /* Add network-request description to network-manager state-indicators: */
    emit sigAddNetworkManagerIndicatorDescription(pNetworkRequest);
}

void UINetworkRequestManager::unregisterNetworkRequest(const QUuid &uuid)
{
    /* Remove network-request description from network-manager state-indicator: */
    emit sigRemoveNetworkManagerIndicatorDescription(uuid);

    /* Remove network-request widget from network-manager dialog: */
    m_pNetworkManagerDialog->removeNetworkRequestWidget(uuid);
}

void UINetworkRequestManager::show()
{
    /* Show network-manager dialog: */
    m_pNetworkManagerDialog->showNormal();
}

void UINetworkRequestManager::createNetworkRequest(UINetworkRequestType enmType, const QList<QUrl> &urls, const QString &strTarget,
                                            const UserDictionary &requestHeaders, UINetworkCustomer *pCustomer)
{
    /* Create network-request: */
    UINetworkRequest *pNetworkRequest = new UINetworkRequest(enmType, urls, strTarget, requestHeaders, pCustomer, this);
    /* Prepare created network-request: */
    prepareNetworkRequest(pNetworkRequest);
}

UINetworkRequestManager::UINetworkRequestManager()
    : m_pNetworkManagerDialog(0)
{
    /* Prepare instance: */
    s_pInstance = this;
}

UINetworkRequestManager::~UINetworkRequestManager()
{
    /* Cleanup instance: */
    s_pInstance = 0;
}

void UINetworkRequestManager::prepare()
{
    /* Prepare network-manager dialog: */
    m_pNetworkManagerDialog = new UINetworkRequestManagerWindow;
    connect(m_pNetworkManagerDialog, &UINetworkRequestManagerWindow::sigCancelNetworkRequests, this, &UINetworkRequestManager::sigCancelNetworkRequests);
}

void UINetworkRequestManager::cleanup()
{
    /* Cleanup network-requests first: */
    cleanupNetworkRequests();

    /* Cleanup network-manager dialog: */
    delete m_pNetworkManagerDialog;
}

void UINetworkRequestManager::prepareNetworkRequest(UINetworkRequest *pNetworkRequest)
{
    /* Prepare listeners for network-request: */
    connect(pNetworkRequest, static_cast<void(UINetworkRequest::*)(const QUuid&, qint64, qint64)>(&UINetworkRequest::sigProgress),
            this, &UINetworkRequestManager::sltHandleNetworkRequestProgress);
    connect(pNetworkRequest, &UINetworkRequest::sigCanceled,
            this, &UINetworkRequestManager::sltHandleNetworkRequestCancel);
    connect(pNetworkRequest, static_cast<void(UINetworkRequest::*)(const QUuid&)>(&UINetworkRequest::sigFinished),
            this, &UINetworkRequestManager::sltHandleNetworkRequestFinish);
    connect(pNetworkRequest, static_cast<void(UINetworkRequest::*)(const QUuid &uuid, const QString &strError)>(&UINetworkRequest::sigFailed),
            this, &UINetworkRequestManager::sltHandleNetworkRequestFailure);

    /* Add network-request into map: */
    m_requests.insert(pNetworkRequest->uuid(), pNetworkRequest);
}

void UINetworkRequestManager::cleanupNetworkRequest(QUuid uuid)
{
    /* Delete network-request from map: */
    delete m_requests[uuid];
    m_requests.remove(uuid);
}

void UINetworkRequestManager::cleanupNetworkRequests()
{
    /* Get all the request IDs: */
    const QList<QUuid> &uuids = m_requests.keys();
    /* Cleanup corresponding requests: */
    for (int i = 0; i < uuids.size(); ++i)
        cleanupNetworkRequest(uuids[i]);
}

void UINetworkRequestManager::sltHandleNetworkRequestProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal)
{
    /* Make sure corresponding map contains received ID: */
    AssertMsg(m_requests.contains(uuid), ("Network-request NOT found!\n"));

    /* Get corresponding network-request: */
    UINetworkRequest *pNetworkRequest = m_requests.value(uuid);

    /* Get corresponding customer: */
    UINetworkCustomer *pNetworkCustomer = pNetworkRequest->customer();

    /* Send to customer to process: */
    pNetworkCustomer->processNetworkReplyProgress(iReceived, iTotal);
}

void UINetworkRequestManager::sltHandleNetworkRequestCancel(const QUuid &uuid)
{
    /* Make sure corresponding map contains received ID: */
    AssertMsg(m_requests.contains(uuid), ("Network-request NOT found!\n"));

    /* Get corresponding network-request: */
    UINetworkRequest *pNetworkRequest = m_requests.value(uuid);

    /* Get corresponding customer: */
    UINetworkCustomer *pNetworkCustomer = pNetworkRequest->customer();

    /* Send to customer to process: */
    pNetworkCustomer->processNetworkReplyCanceled(pNetworkRequest->reply());

    /* Cleanup network-request: */
    cleanupNetworkRequest(uuid);
}

void UINetworkRequestManager::sltHandleNetworkRequestFinish(const QUuid &uuid)
{
    /* Make sure corresponding map contains received ID: */
    AssertMsg(m_requests.contains(uuid), ("Network-request NOT found!\n"));

    /* Get corresponding network-request: */
    UINetworkRequest *pNetworkRequest = m_requests.value(uuid);

    /* Get corresponding customer: */
    UINetworkCustomer *pNetworkCustomer = pNetworkRequest->customer();

    /* Send to customer to process: */
    pNetworkCustomer->processNetworkReplyFinished(pNetworkRequest->reply());

    /* Cleanup network-request: */
    cleanupNetworkRequest(uuid);
}

void UINetworkRequestManager::sltHandleNetworkRequestFailure(const QUuid &uuid, const QString &)
{
    /* Make sure corresponding map contains received ID: */
    AssertMsg(m_requests.contains(uuid), ("Network-request NOT found!\n"));

    /* Get corresponding network-request: */
    UINetworkRequest *pNetworkRequest = m_requests.value(uuid);

    /* Get corresponding customer: */
    UINetworkCustomer *pNetworkCustomer = pNetworkRequest->customer();

    /* If customer made a force-call: */
    if (pNetworkCustomer->isItForceCall())
    {
        /* Just show the dialog: */
        show();
    }
}

