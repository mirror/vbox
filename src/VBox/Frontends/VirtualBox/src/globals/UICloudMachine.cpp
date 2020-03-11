/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudMachine class implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include <QMap>

/* GUI includes: */
#include "UICloudMachine.h"
#include "UICloudNetworkingStuff.h"


/*********************************************************************************************************************************
*   Class UICloudMachineData implementation.                                                                                     *
*********************************************************************************************************************************/

UICloudMachineData::UICloudMachineData(const CCloudClient &comCloudClient,
                                       const QString &strId,
                                       const QString &strName)
    : m_comCloudClient(comCloudClient)
    , m_strId(strId)
    , m_strName(strName)
    , m_fAccessible(true)
    , m_enmMachineState(KMachineState_PoweredOff)
    , m_strOsType("Other")
    , m_iMemorySize(0)
    , m_iCpuCount(0)
{
    //printf("Data for machine with id = {%s} is created\n", m_strId.toUtf8().constData());
}

UICloudMachineData::UICloudMachineData(const UICloudMachineData &other)
    : QSharedData(other)
    , m_comCloudClient(other.m_comCloudClient)
    , m_strId(other.m_strId)
    , m_strName(other.m_strName)
    , m_fAccessible(other.m_fAccessible)
    , m_strAccessError(other.m_strAccessError)
    , m_enmMachineState(other.m_enmMachineState)
    , m_strOsType(other.m_strOsType)
    , m_iMemorySize(other.m_iMemorySize)
    , m_iCpuCount(other.m_iCpuCount)
    , m_strShape(other.m_strShape)
    , m_strDomain(other.m_strDomain)
    , m_strBootingFirmware(other.m_strBootingFirmware)
    , m_strImageId(other.m_strImageId)
{
    //printf("Data for machine with id = {%s} is copied\n", m_strId.toUtf8().constData());
}

UICloudMachineData::~UICloudMachineData()
{
    //printf("Data for machine with id = {%s} is deleted\n", m_strId.toUtf8().constData());
}

void UICloudMachineData::refresh()
{
    /* Acquire instance info sync way, be aware, this is blocking stuff, it takes some time: */
    const QMap<KVirtualSystemDescriptionType, QString> instanceInfoMap = getInstanceInfo(m_comCloudClient, m_strId, m_strAccessError);
    /* Update accessibility state: */
    m_fAccessible = m_strAccessError.isNull();

    /* Refresh corresponding values if accessible: */
    if (m_fAccessible)
    {
        m_strOsType = fetchOsType(instanceInfoMap);
        m_iMemorySize = fetchMemorySize(instanceInfoMap);
        m_iCpuCount = fetchCpuCount(instanceInfoMap);
        m_enmMachineState = fetchMachineState(instanceInfoMap);
        m_strShape = fetchShape(instanceInfoMap);
        m_strDomain = fetchDomain(instanceInfoMap);
        m_strBootingFirmware = fetchBootingFirmware(instanceInfoMap);
        m_strImageId = fetchImageId(instanceInfoMap);

        /* Acquire image info sync way, be aware, this is blocking stuff, it takes some time: */
        const QMap<QString, QString> imageInfoMap = getImageInfo(m_comCloudClient, m_strImageId, m_strAccessError);
        /* Update accessibility state: */
        m_fAccessible = m_strAccessError.isNull();

        //printf("Image info:\n");
        //foreach (const QString &strKey, imageInfoMap.keys())
        //    printf("key = %s, value = %s\n", strKey.toUtf8().constData(), imageInfoMap.value(strKey).toUtf8().constData());
        //printf("\n");

        /* Refresh corresponding values if accessible: */
        if (m_fAccessible)
        {
            /* Sorry, but these are hardcoded in Main: */
            m_strImageName = imageInfoMap.value("display name");
            m_strImageSize = imageInfoMap.value("size");
        }
    }

    /* Reset everything if not accessible: */
    if (!m_fAccessible)
    {
        m_enmMachineState = KMachineState_PoweredOff;
        m_strOsType = "Other";
        m_iMemorySize = 0;
        m_iCpuCount = 0;
        m_strShape.clear();
        m_strDomain.clear();
        m_strBootingFirmware.clear();
        m_strImageId.clear();
        m_strImageName.clear();
        m_strImageSize.clear();
    }
}


/*********************************************************************************************************************************
*   Class UICloudMachine implementation.                                                                                         *
*********************************************************************************************************************************/

UICloudMachine::UICloudMachine()
{
}

UICloudMachine::UICloudMachine(const CCloudClient &comCloudClient,
                               const QString &strId,
                               const QString &strName)
    : d(new UICloudMachineData(comCloudClient, strId, strName))
{
}

UICloudMachine::UICloudMachine(const UICloudMachine &other)
    : d(other.d)
{
}
