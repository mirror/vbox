/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINetworkManager stuff implementation
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QWidget>

/* Local includes: */
#include "UINetworkManager.h"
#include "UINetworkManagerDialog.h"
#include "UINetworkManagerIndicator.h"
#include "UINetworkRequest.h"
#include "UINetworkCustomer.h"
#include "VBoxGlobal.h"

UINetworkManager* UINetworkManager::m_pInstance = 0;

void UINetworkManager::create()
{
    /* Check that instance do NOT exist: */
    if (m_pInstance)
        return;

    /* Create instance: */
    new UINetworkManager;

    /* Prepare instance: */
    m_pInstance->prepare();
}

void UINetworkManager::destroy()
{
    /* Check that instance exists: */
    if (!m_pInstance)
        return;

    /* Cleanup instance: */
    m_pInstance->cleanup();

    /* Destroy instance: */
    delete m_pInstance;
}

UINetworkManagerDialog* UINetworkManager::window() const
{
    return m_pNetworkManagerDialog;
}

UINetworkManagerIndicator* UINetworkManager::indicator() const
{
    return m_pNetworkManagerIndicator;
}

void UINetworkManager::show()
{
    /* Show network-manager dialog: */
    m_pNetworkManagerDialog->showNormal();
}

void UINetworkManager::createNetworkRequest(const QNetworkRequest &request, UINetworkRequestType type, const QString &strDescription,
                                            UINetworkCustomer *pCustomer)
{
    /* Create network-request: */
    UINetworkRequest *pNetworkRequest = new UINetworkRequest(request, type, strDescription, pCustomer, this);
    /* Prepare created network-request: */
    prepareNetworkRequest(pNetworkRequest);
}

void UINetworkManager::createNetworkRequest(const QList<QNetworkRequest> &requests, UINetworkRequestType type, const QString &strDescription,
                                            UINetworkCustomer *pCustomer)
{
    /* Create network-request: */
    UINetworkRequest *pNetworkRequest = new UINetworkRequest(requests, type, strDescription, pCustomer, this);
    /* Prepare created network-request: */
    prepareNetworkRequest(pNetworkRequest);
}

UINetworkManager::UINetworkManager()
    : m_pNetworkManagerDialog(0)
    , m_pNetworkManagerIndicator(0)
{
    /* Prepare instance: */
    m_pInstance = this;
}

UINetworkManager::~UINetworkManager()
{
    /* Cleanup instance: */
    m_pInstance = 0;
}

void UINetworkManager::prepare()
{
    /* Prepare network-manager dialog: */
    m_pNetworkManagerDialog = new UINetworkManagerDialog;
    connect(m_pNetworkManagerDialog, SIGNAL(sigCancelNetworkRequests()), this, SIGNAL(sigCancelNetworkRequests()));

    /* Prepare network-manager state-indicator: */
    if (!vboxGlobal().isVMConsoleProcess())
    {
        m_pNetworkManagerIndicator = new UINetworkManagerIndicator;
        connect(m_pNetworkManagerIndicator, SIGNAL(mouseDoubleClicked(QIStateIndicator *, QMouseEvent *)), this, SLOT(show()));
    }
}

void UINetworkManager::cleanup()
{
    /* Cleanup network-requests first: */
    cleanupNetworkRequests();

    /* Cleanup network-manager state-indicator: */
    if (!vboxGlobal().isVMConsoleProcess())
    {
        delete m_pNetworkManagerIndicator;
        m_pNetworkManagerIndicator = 0;
    }

    /* Cleanup network-manager dialog: */
    delete m_pNetworkManagerDialog;
}

void UINetworkManager::prepareNetworkRequest(UINetworkRequest *pNetworkRequest)
{
    /* Prepare listeners for network-request: */
    connect(pNetworkRequest, SIGNAL(sigProgress(const QUuid&, qint64, qint64)),
            this, SLOT(sltHandleNetworkRequestProgress(const QUuid&, qint64, qint64)));
    connect(pNetworkRequest, SIGNAL(sigCanceled(const QUuid&)),
            this, SLOT(sltHandleNetworkRequestCancel(const QUuid&)));
    connect(pNetworkRequest, SIGNAL(sigFinished(const QUuid&)),
            this, SLOT(sltHandleNetworkRequestFinish(const QUuid&)));
    connect(pNetworkRequest, SIGNAL(sigFailed(const QUuid&, const QString&)),
            this, SLOT(sltHandleNetworkRequestFailure(const QUuid&, const QString&)));

    /* Add network-request into map: */
    m_requests.insert(pNetworkRequest->uuid(), pNetworkRequest);
}

void UINetworkManager::cleanupNetworkRequest(QUuid uuid)
{
    /* Delete network-request from map: */
    delete m_requests[uuid];
    m_requests.remove(uuid);
}

void UINetworkManager::cleanupNetworkRequests()
{
    /* Get all the request IDs: */
    const QList<QUuid> &uuids = m_requests.keys();
    /* Cleanup corresponding requests: */
    for (int i = 0; i < uuids.size(); ++i)
        cleanupNetworkRequest(uuids[i]);
}

void UINetworkManager::sltHandleNetworkRequestProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal)
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

void UINetworkManager::sltHandleNetworkRequestCancel(const QUuid &uuid)
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

void UINetworkManager::sltHandleNetworkRequestFinish(const QUuid &uuid)
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

void UINetworkManager::sltHandleNetworkRequestFailure(const QUuid &uuid, const QString &)
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

