/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkRequestManagerIndicator stuff implementation.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
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
#include "UICommon.h"
#include "UIIconPool.h"
#include "UINetworkRequestManagerIndicator.h"
#include "UINetworkRequest.h"


UINetworkRequestManagerIndicator::UINetworkRequestManagerIndicator()
{
    /* Assign state icons: */
    setStateIcon(UINetworkRequestManagerIndicatorState_Idle, UIIconPool::iconSet(":/download_manager_16px.png"));
    setStateIcon(UINetworkRequestManagerIndicatorState_Loading, UIIconPool::iconSet(":/download_manager_loading_16px.png"));
    setStateIcon(UINetworkRequestManagerIndicatorState_Error, UIIconPool::iconSet(":/download_manager_error_16px.png"));

    /* Translate content: */
    retranslateUi();
}

void UINetworkRequestManagerIndicator::updateAppearance()
{
    /* First of all, we are hiding LED in case of 'idle' state: */
    if (state() == UINetworkRequestManagerIndicatorState_Idle && !isHidden())
        hide();

    /* Prepare description: */
    QString strDecription;
    /* Check if there are any network-requests: */
    if (!m_ids.isEmpty())
    {
        /* Prepare table: */
        QString strTable("<table>%1</table>");
        QString strBodyItem("<tr><td>%1</td><td>&nbsp;</td><td>%2</td></tr>");
        QString strParagraph("<p>%1</p>");
        QString strBoldNobreak("<nobr><b>%1</b></nobr>");
        QString strNobreak("<nobr>%1</nobr>");
        QString strItalic("<i>%1</i>");
        /* Prepare header: */
        QString strHeader(strBoldNobreak.arg(tr("Current network operations:")));
        /* Prepare table body: */
        QString strBody;
        for (int i = 0; i < m_data.size(); ++i)
        {
            const UINetworkRequestData &data = m_data[i];
            const QString &strDescription = data.description;
            QString strStatus(data.failed ? tr("failed", "network operation") :
                                            tr("(%1 of %2)")
                                               .arg(uiCommon().formatSize(data.bytesReceived))
                                               .arg(uiCommon().formatSize(data.bytesTotal)));
            QString strBodyLine(strBodyItem.arg(strNobreak.arg(strDescription)).arg(strNobreak.arg(strStatus)));
            strBody += strBodyLine;
        }
        /* Compose description: */
        strDecription = strParagraph.arg(strHeader + strTable.arg(strBody)) +
                        strParagraph.arg(strNobreak.arg(strItalic.arg(tr("Double-click for more information."))));
    }
    else
        strDecription = QString();
    /* Set description: */
    setToolTip(strDecription);

    /* Finally, we are showing LED in case of state is not 'idle': */
    if (state() != UINetworkRequestManagerIndicatorState_Idle && isHidden())
        show();
}

void UINetworkRequestManagerIndicator::sltAddNetworkManagerIndicatorDescription(UINetworkRequest *pNetworkRequest)
{
    /* Make sure network-request is really exists: */
    AssertMsg(pNetworkRequest, ("Invalid network-request passed!"));
    /* Make sure network-request was NOT registered yet: */
    AssertMsg(!m_ids.contains(pNetworkRequest->uuid()), ("Network-request already registered!"));

    /* Append network-request data: */
    m_ids.append(pNetworkRequest->uuid());
    m_data.append(UINetworkRequestData(pNetworkRequest->description(), 0, 0));

    /* Prepare network-request listeners: */
    connect(pNetworkRequest, static_cast<void(UINetworkRequest::*)(const QUuid&)>(&UINetworkRequest::sigStarted),
            this, &UINetworkRequestManagerIndicator::sltSetProgressToStarted);
    connect(pNetworkRequest, &UINetworkRequest::sigCanceled,
            this, &UINetworkRequestManagerIndicator::sltSetProgressToCanceled);
    connect(pNetworkRequest, static_cast<void(UINetworkRequest::*)(const QUuid&)>(&UINetworkRequest::sigFinished),
            this, &UINetworkRequestManagerIndicator::sltSetProgressToFinished);
    connect(pNetworkRequest, static_cast<void(UINetworkRequest::*)(const QUuid&, const QString &)>(&UINetworkRequest::sigFailed),
            this, &UINetworkRequestManagerIndicator::sltSetProgressToFailed);
    connect(pNetworkRequest, static_cast<void(UINetworkRequest::*)(const QUuid&, qint64, qint64)>(&UINetworkRequest::sigProgress),
            this, &UINetworkRequestManagerIndicator::sltSetProgress);

    /* Update appearance: */
    recalculateIndicatorState();
}

void UINetworkRequestManagerIndicator::sldRemoveNetworkManagerIndicatorDescription(const QUuid &uuid)
{
    /* Make sure network-request still registered: */
    AssertMsg(m_ids.contains(uuid), ("Network-request already unregistered!"));

    /* Search for network-request index: */
    int iIndexOfRequiredElement = m_ids.indexOf(uuid);

    /* Delete corresponding network-request: */
    m_ids.remove(iIndexOfRequiredElement);
    m_data.remove(iIndexOfRequiredElement);

    /* Update appearance: */
    recalculateIndicatorState();
}

void UINetworkRequestManagerIndicator::retranslateUi()
{
    /* Update appearance: */
    updateAppearance();
}

void UINetworkRequestManagerIndicator::sltSetProgressToStarted(const QUuid &uuid)
{
    /* Make sure that network-request still registered: */
    AssertMsg(m_ids.contains(uuid), ("That network-request already unregistered!"));

    /* Search for network-request index: */
    int iIndexOfNetworkRequest = m_ids.indexOf(uuid);
    /* Update corresponding network-request data: */
    UINetworkRequestData &data = m_data[iIndexOfNetworkRequest];
    data.bytesReceived = 0;
    data.bytesTotal = 0;
    data.failed = false;

    /* Update appearance: */
    recalculateIndicatorState();
}

void UINetworkRequestManagerIndicator::sltSetProgressToCanceled(const QUuid &uuid)
{
    /* Make sure that network-request still registered: */
    AssertMsg(m_ids.contains(uuid), ("That network-request already unregistered!"));
    Q_UNUSED(uuid);

    /* Update appearance: */
    recalculateIndicatorState();
}

void UINetworkRequestManagerIndicator::sltSetProgressToFailed(const QUuid &uuid, const QString &)
{
    /* Make sure that network-request still registered: */
    AssertMsg(m_ids.contains(uuid), ("That network-request already unregistered!"));

    /* Search for network-request index: */
    int iIndexOfNetworkRequest = m_ids.indexOf(uuid);
    /* Update corresponding data: */
    UINetworkRequestData &data = m_data[iIndexOfNetworkRequest];
    data.failed = true;

    /* Update appearance: */
    recalculateIndicatorState();
}

void UINetworkRequestManagerIndicator::sltSetProgressToFinished(const QUuid &uuid)
{
    /* Make sure that network-request still registered: */
    AssertMsg(m_ids.contains(uuid), ("That network-request already unregistered!"));
    Q_UNUSED(uuid);

    /* Update appearance: */
    recalculateIndicatorState();
}

void UINetworkRequestManagerIndicator::sltSetProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal)
{
    /* Make sure that network-request still registered: */
    AssertMsg(m_ids.contains(uuid), ("That network-request already unregistered!"));

    /* Search for network-request index: */
    int iIndexOfNetworkRequest = m_ids.indexOf(uuid);
    /* Update corresponding network-request data: */
    UINetworkRequestData &data = m_data[iIndexOfNetworkRequest];
    data.bytesReceived = iReceived;
    data.bytesTotal = iTotal;

    /* Update appearance: */
    updateAppearance();
}

void UINetworkRequestManagerIndicator::recalculateIndicatorState()
{
    /* Check if there are network-requests at all: */
    if (m_ids.isEmpty())
    {
        /* Set state to 'idle': */
        setState(UINetworkRequestManagerIndicatorState_Idle);
    }
    else
    {
        /* Check if there is at least one failed network-request: */
        bool fIsThereAtLeastOneFailedNetworkRequest = false;
        for (int i = 0; i < m_data.size(); ++i)
        {
            if (m_data[i].failed)
            {
                fIsThereAtLeastOneFailedNetworkRequest = true;
                break;
            }
        }

        /* If there it least one failed network-request: */
        if (fIsThereAtLeastOneFailedNetworkRequest)
        {
            /* Set state to 'error': */
            setState(UINetworkRequestManagerIndicatorState_Error);
        }
        else
        {
            /* Set state to 'loading': */
            setState(UINetworkRequestManagerIndicatorState_Loading);
        }
    }

    /* Update appearance finally: */
    updateAppearance();
}

