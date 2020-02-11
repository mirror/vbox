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

/* GUI includes: */
#include "UICloudMachine.h"
#include "UICommon.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CAppliance.h"
#include "CVirtualBox.h"
#include "CVirtualSystemDescription.h"


/*********************************************************************************************************************************
*   Class UICloudMachineData implementation.                                                                                     *
*********************************************************************************************************************************/

UICloudMachineData::UICloudMachineData(const CCloudClient &comCloudClient,
                                       const QString &strId,
                                       const QString &strName)
    : m_comCloudClient(comCloudClient)
    , m_strId(strId)
    , m_strName(strName)
{
    //printf("Data for machine with id = {%s} is created\n", m_strId.toUtf8().constData());
}

UICloudMachineData::UICloudMachineData(const UICloudMachineData &other)
    : QSharedData(other)
    , m_comCloudClient(other.m_comCloudClient)
    , m_strId(other.m_strId)
    , m_strName(other.m_strName)
{
    //printf("Data for machine with id = {%s} is copied\n", m_strId.toUtf8().constData());
}

UICloudMachineData::~UICloudMachineData()
{
    //printf("Data for machine with id = {%s} is deleted\n", m_strId.toUtf8().constData());
}


/*********************************************************************************************************************************
*   Class UICloudMachine implementation.                                                                                         *
*********************************************************************************************************************************/

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
