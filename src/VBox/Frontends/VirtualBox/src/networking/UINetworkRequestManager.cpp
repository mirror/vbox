/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkRequestManager stuff implementation.
 */

/*
 * Copyright (C) 2011-2021 Oracle Corporation
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
#include <QUrl>

/* GUI includes: */
#include "UINetworkCustomer.h"
#include "UINetworkRequest.h"
#include "UINetworkRequestManager.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/* static */
UINetworkRequestManager *UINetworkRequestManager::s_pInstance = 0;

/* static */
void UINetworkRequestManager::create()
{
    AssertReturnVoid(!s_pInstance);
    new UINetworkRequestManager;
}

/* static */
void UINetworkRequestManager::destroy()
{
    AssertPtrReturnVoid(s_pInstance);
    delete s_pInstance;
}

/* static */
UINetworkRequestManager *UINetworkRequestManager::instance()
{
    return s_pInstance;
}

QUuid UINetworkRequestManager::createNetworkRequest(UINetworkRequestType enmType,
                                                    const QList<QUrl> &urls,
                                                    const QString &strTarget,
                                                    const UserDictionary &requestHeaders,
                                                    UINetworkCustomer *pCustomer)
{
    /* Create network-request: */
    UINetworkRequest *pNetworkRequest = new UINetworkRequest(enmType, urls, strTarget, requestHeaders);
    if (pNetworkRequest)
    {
        /* Configure request listeners: */
        connect(pNetworkRequest, &UINetworkRequest::sigProgress,
                this, &UINetworkRequestManager::sltHandleNetworkRequestProgress);
        connect(pNetworkRequest, &UINetworkRequest::sigCanceled,
                this, &UINetworkRequestManager::sltHandleNetworkRequestCancel);
        connect(pNetworkRequest, &UINetworkRequest::sigFinished,
                this, &UINetworkRequestManager::sltHandleNetworkRequestFinish);
        connect(pNetworkRequest, &UINetworkRequest::sigFailed,
                this, &UINetworkRequestManager::sltHandleNetworkRequestFailure);

        /* [Re]generate ID until unique: */
        QUuid uId = QUuid::createUuid();
        while (m_requests.contains(uId))
            uId = QUuid::createUuid();

        /* Add request&customer to map: */
        m_requests.insert(uId, pNetworkRequest);
        m_customers.insert(uId, pCustomer);

        /* Return ID: */
        return uId;
    }

    /* Null ID by default: */
    return QUuid();
}

UINetworkRequestManager::UINetworkRequestManager()
{
    s_pInstance = this;
    prepare();
}

UINetworkRequestManager::~UINetworkRequestManager()
{
    cleanup();
    s_pInstance = 0;
}

void UINetworkRequestManager::sltHandleNetworkRequestProgress(qint64 iReceived, qint64 iTotal)
{
    /* Make sure we have this request registered: */
    UINetworkRequest *pNetworkRequest = qobject_cast<UINetworkRequest*>(sender());
    AssertPtrReturnVoid(pNetworkRequest);
    const QUuid uId = m_requests.key(pNetworkRequest);
    AssertReturnVoid(!uId.isNull());

    /* Delegate request to customer: */
    UINetworkCustomer *pNetworkCustomer = m_customers.value(uId);
    AssertPtrReturnVoid(pNetworkCustomer);
    pNetworkCustomer->processNetworkReplyProgress(iReceived, iTotal);
}

void UINetworkRequestManager::sltHandleNetworkRequestFailure(const QString &strError)
{
    /* Make sure we have this request registered: */
    UINetworkRequest *pNetworkRequest = qobject_cast<UINetworkRequest*>(sender());
    AssertPtrReturnVoid(pNetworkRequest);
    const QUuid uId = m_requests.key(pNetworkRequest);
    AssertReturnVoid(!uId.isNull());

    /* Delegate request to customer: */
    UINetworkCustomer *pNetworkCustomer = m_customers.value(uId);
    AssertPtrReturnVoid(pNetworkCustomer);
    pNetworkCustomer->processNetworkReplyFailed(strError);

    /* Cleanup request: */
    cleanupNetworkRequest(uId);
}

void UINetworkRequestManager::sltHandleNetworkRequestCancel()
{
    /* Make sure we have this request registered: */
    UINetworkRequest *pNetworkRequest = qobject_cast<UINetworkRequest*>(sender());
    AssertPtrReturnVoid(pNetworkRequest);
    const QUuid uId = m_requests.key(pNetworkRequest);
    AssertReturnVoid(!uId.isNull());

    /* Delegate request to customer: */
    UINetworkCustomer *pNetworkCustomer = m_customers.value(uId);
    AssertPtrReturnVoid(pNetworkCustomer);
    pNetworkCustomer->processNetworkReplyCanceled(pNetworkRequest->reply());

    /* Cleanup request: */
    cleanupNetworkRequest(uId);
}

void UINetworkRequestManager::sltHandleNetworkRequestFinish()
{
    /* Make sure we have this request registered: */
    UINetworkRequest *pNetworkRequest = qobject_cast<UINetworkRequest*>(sender());
    AssertPtrReturnVoid(pNetworkRequest);
    const QUuid uId = m_requests.key(pNetworkRequest);
    AssertReturnVoid(!uId.isNull());

    /* Delegate request to customer: */
    UINetworkCustomer *pNetworkCustomer = m_customers.value(uId);
    AssertPtrReturnVoid(pNetworkCustomer);
    pNetworkCustomer->processNetworkReplyFinished(pNetworkRequest->reply());

    /* Cleanup request: */
    cleanupNetworkRequest(uId);
}

void UINetworkRequestManager::prepare()
{
    // nothing for now
}

void UINetworkRequestManager::cleanupNetworkRequest(const QUuid &uId)
{
    delete m_requests.value(uId);
    m_requests.remove(uId);
    m_customers.remove(uId);
}

void UINetworkRequestManager::cleanupNetworkRequests()
{
    foreach (const QUuid &uId, m_requests.keys())
        cleanupNetworkRequest(uId);
}

void UINetworkRequestManager::cleanup()
{
    cleanupNetworkRequests();
}
