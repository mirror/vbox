/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudNetworkingStuff namespace implementation.
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
#include "UICloudNetworkingStuff.h"
#include "UICommon.h"
#include "UIErrorString.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CAppliance.h"
#include "CProgress.h"
#include "CStringArray.h"
#include "CVirtualBox.h"
#include "CVirtualBoxErrorInfo.h"
#include "CVirtualSystemDescription.h"


CCloudProviderManager UICloudNetworkingStuff::cloudProviderManager()
{
    /* Acquire VBox: */
    const CVirtualBox comVBox = uiCommon().virtualBox();
    if (comVBox.isNotNull())
    {
        /* Acquire cloud provider manager: */
        CCloudProviderManager comProviderManager = comVBox.GetCloudProviderManager();
        if (!comVBox.isOk())
            msgCenter().cannotAcquireCloudProviderManager(comVBox);
        else
            return comProviderManager;
    }
    /* Null by default: */
    return CCloudProviderManager();
}

CCloudProvider UICloudNetworkingStuff::cloudProviderByShortName(const QString &strProviderShortName)
{
    /* Acquire cloud provider manager: */
    CCloudProviderManager comProviderManager = cloudProviderManager();
    if (comProviderManager.isNotNull())
    {
        /* Acquire cloud provider: */
        CCloudProvider comProvider = comProviderManager.GetProviderByShortName(strProviderShortName);
        if (!comProviderManager.isOk())
            msgCenter().cannotAcquireCloudProviderManagerParameter(comProviderManager);
        else
            return comProvider;
    }
    /* Null by default: */
    return CCloudProvider();
}

CCloudProfile UICloudNetworkingStuff::cloudProfileByName(const QString &strProviderShortName,
                                                         const QString &strProfileName)
{
    /* Acquire cloud provider: */
    CCloudProvider comProvider = cloudProviderByShortName(strProviderShortName);
    if (comProvider.isNotNull())
    {
        /* Acquire cloud profile: */
        CCloudProfile comProfile = comProvider.GetProfileByName(strProfileName);
        if (!comProvider.isOk())
            msgCenter().cannotAcquireCloudProviderParameter(comProvider);
        else
            return comProfile;
    }
    /* Null by default: */
    return CCloudProfile();
}

CCloudClient UICloudNetworkingStuff::cloudClientByName(const QString &strProviderShortName,
                                                       const QString &strProfileName)
{
    /* Acquire cloud profile: */
    CCloudProfile comProfile = cloudProfileByName(strProviderShortName, strProfileName);
    if (comProfile.isNotNull())
    {
        /* Create cloud client: */
        CCloudClient comClient = comProfile.CreateCloudClient();
        if (!comProfile.isOk())
            msgCenter().cannotAcquireCloudProfileParameter(comProfile);
        else
            return comClient;
    }
    /* Null by default: */
    return CCloudClient();
}

CCloudMachine UICloudNetworkingStuff::cloudMachineById(const QString &strProviderShortName,
                                                       const QString &strProfileName,
                                                       const QUuid &uMachineId)
{
    /* Acquire cloud client: */
    CCloudClient comClient = cloudClientByName(strProviderShortName, strProfileName);
    if (comClient.isNotNull())
    {
        /* Acquire cloud machine: */
        CCloudMachine comMachine = comClient.GetCloudMachine(uMachineId);
        if (!comClient.isOk())
            msgCenter().cannotAcquireCloudClientParameter(comClient);
        else
            return comMachine;
    }
    /* Null by default: */
    return CCloudMachine();
}

bool UICloudNetworkingStuff::cloudMachineId(const CCloudMachine &comCloudMachine,
                                            QUuid &uResult,
                                            QWidget *pParent /* = 0 */)
{
    const QUuid uId = comCloudMachine.GetId();
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }
    uResult = uId;
    return true;
}

bool UICloudNetworkingStuff::cloudMachineAccessible(const CCloudMachine &comCloudMachine,
                                                    bool &fResult,
                                                    QWidget *pParent /* = 0 */)
{
    const bool fAccessible = comCloudMachine.GetAccessible();
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }
    fResult = fAccessible;
    return true;
}

bool UICloudNetworkingStuff::cloudMachineAccessError(const CCloudMachine &comCloudMachine,
                                                     CVirtualBoxErrorInfo &comResult,
                                                     QWidget *pParent /* = 0 */)
{
    const CVirtualBoxErrorInfo comAccessError = comCloudMachine.GetAccessError();
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }
    comResult = comAccessError;
    return true;
}

bool UICloudNetworkingStuff::cloudMachineName(const CCloudMachine &comCloudMachine,
                                              QString &strResult,
                                              QWidget *pParent /* = 0 */)
{
    const QString strName = comCloudMachine.GetName();
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }
    strResult = strName;
    return true;
}

bool UICloudNetworkingStuff::cloudMachineOSTypeId(const CCloudMachine &comCloudMachine,
                                                  QString &strResult,
                                                  QWidget *pParent /* = 0 */)
{
    const QString strOSTypeId = comCloudMachine.GetOSTypeId();
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }
    strResult = strOSTypeId;
    return true;
}

bool UICloudNetworkingStuff::cloudMachineState(const CCloudMachine &comCloudMachine,
                                               KMachineState &enmResult,
                                               QWidget *pParent /* = 0 */)
{
    const KMachineState enmState = comCloudMachine.GetState();
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }
    enmResult = enmState;
    return true;
}

QMap<QString, QString> UICloudNetworkingStuff::listInstances(const CCloudClient &comCloudClient,
                                                             QString &strErrorMessage,
                                                             QWidget *pParent /* = 0 */)
{
    /* Prepare VM names, ids and states.
     * Currently we are interested in Running and Stopped VMs only. */
    CStringArray comNames;
    CStringArray comIDs;
    const QVector<KCloudMachineState> cloudMachineStates  = QVector<KCloudMachineState>()
                                                         << KCloudMachineState_Running
                                                         << KCloudMachineState_Stopped;

    /* Now execute ListInstances async method: */
    CProgress comProgress = comCloudClient.ListInstances(cloudMachineStates, comNames, comIDs);
    if (!comCloudClient.isOk())
    {
        if (pParent)
            msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
        else
            strErrorMessage = UIErrorString::formatErrorInfo(comCloudClient);
    }
    else
    {
        /* Show "Acquire cloud instances" progress: */
        if (pParent)
            msgCenter().showModalProgressDialog(comProgress,
                                                UICommon::tr("Acquire cloud instances ..."),
                                                ":/progress_reading_appliance_90px.png", pParent, 0);
        else
            comProgress.WaitForCompletion(-1);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            if (pParent)
                msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
            else
                strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        }
        else
        {
            /* Fetch acquired objects to map: */
            const QVector<QString> instanceIds = comIDs.GetValues();
            const QVector<QString> instanceNames = comNames.GetValues();
            QMap<QString, QString> resultMap;
            for (int i = 0; i < instanceIds.size(); ++i)
                resultMap[instanceIds.at(i)] = instanceNames.at(i);
            return resultMap;
        }
    }

    /* Return empty map by default: */
    return QMap<QString, QString>();
}

QVector<CCloudMachine> UICloudNetworkingStuff::listCloudMachines(CCloudClient &comCloudClient,
                                                                 QString &strErrorMessage,
                                                                 QWidget *pParent /* = 0 */)
{
    /* Execute ReadCloudMachineList async method: */
    CProgress comProgress = comCloudClient.ReadCloudMachineList();
    if (!comCloudClient.isOk())
    {
        if (pParent)
            msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
        else
            strErrorMessage = UIErrorString::formatErrorInfo(comCloudClient);
    }
    else
    {
        /* Show "Read cloud machines" progress: */
        if (pParent)
            msgCenter().showModalProgressDialog(comProgress,
                                                UICommon::tr("Read cloud machines ..."),
                                                ":/progress_reading_appliance_90px.png", pParent, 0);
        else
            comProgress.WaitForCompletion(-1);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            if (pParent)
                msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
            else
                strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        }
        else
        {
            /* Return acquired cloud machines: */
            return comCloudClient.GetCloudMachineList();
        }
    }

    /* Return empty list by default: */
    return QVector<CCloudMachine>();
}

QMap<KVirtualSystemDescriptionType, QString> UICloudNetworkingStuff::getInstanceInfo(const CCloudClient &comCloudClient,
                                                                                     const QString &strId,
                                                                                     QString &strErrorMessage,
                                                                                     QWidget *pParent /* = 0 */)
{
    /* Prepare result: */
    QMap<KVirtualSystemDescriptionType, QString> resultMap;

    /* Get VirtualBox object: */
    CVirtualBox comVBox = uiCommon().virtualBox();

    /* Create appliance: */
    CAppliance comAppliance = comVBox.CreateAppliance();
    if (!comVBox.isOk())
    {
        if (pParent)
            msgCenter().cannotCreateAppliance(comVBox, pParent);
        else
            strErrorMessage = UIErrorString::formatErrorInfo(comVBox);
    }
    else
    {
        /* Append it with one (1) description we need: */
        comAppliance.CreateVirtualSystemDescriptions(1);
        if (!comAppliance.isOk())
        {
            if (pParent)
                msgCenter().cannotCreateVirtualSystemDescription(comAppliance, pParent);
            else
                strErrorMessage = UIErrorString::formatErrorInfo(comAppliance);
        }
        else
        {
            /* Get received description: */
            QVector<CVirtualSystemDescription> descriptions = comAppliance.GetVirtualSystemDescriptions();
            AssertReturn(!descriptions.isEmpty(), resultMap);
            CVirtualSystemDescription comDescription = descriptions.at(0);

            /* Now execute GetInstanceInfo async method: */
            CProgress comProgress = comCloudClient.GetInstanceInfo(strId, comDescription);
            if (!comCloudClient.isOk())
            {
                if (pParent)
                    msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
                else
                    strErrorMessage = UIErrorString::formatErrorInfo(comCloudClient);
            }
            else
            {
                /* Show "Acquire instance info" progress: */
                if (pParent)
                    msgCenter().showModalProgressDialog(comProgress,
                                                        UICommon::tr("Acquire cloud instance info ..."),
                                                        ":/progress_reading_appliance_90px.png", pParent, 0);
                else
                    comProgress.WaitForCompletion(-1);
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                {
                    if (pParent)
                        msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
                    else
                        strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
                }
                else
                {
                    /* Now acquire description of certain type: */
                    QVector<KVirtualSystemDescriptionType> types;
                    QVector<QString> refs, origValues, configValues, extraConfigValues;
                    comDescription.GetDescription(types, refs, origValues, configValues, extraConfigValues);

                    /* Make sure key & value vectors have the same size: */
                    AssertReturn(!types.isEmpty() && types.size() == configValues.size(), resultMap);
                    /* Append resulting map with key/value pairs we have: */
                    for (int i = 0; i < types.size(); ++i)
                        resultMap[types.at(i)] = configValues.at(i);
                }
            }
        }
    }

    /* Return result: */
    return resultMap;
}

QString UICloudNetworkingStuff::getInstanceInfo(KVirtualSystemDescriptionType enmType,
                                                const CCloudClient &comCloudClient,
                                                const QString &strId,
                                                QString &strErrorMessage,
                                                QWidget *pParent /* = 0 */)
{
    return getInstanceInfo(comCloudClient, strId, strErrorMessage, pParent).value(enmType, QString());
}

void UICloudNetworkingStuff::refreshCloudMachineInfo(CCloudMachine &comCloudMachine,
                                                     QString &strErrorMessage,
                                                     QWidget *pParent /* = 0 */)
{
    /* Execute Refresh async method: */
    CProgress comProgress = comCloudMachine.Refresh();
    if (!comCloudMachine.isOk())
    {
        if (pParent)
            msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        else
            strErrorMessage = UIErrorString::formatErrorInfo(comCloudMachine);
    }
    else
    {
        /* Show "Refresh cloud machine information" progress: */
        if (pParent)
            msgCenter().showModalProgressDialog(comProgress,
                                                UICommon::tr("Refresh cloud machine information ..."),
                                                ":/progress_reading_appliance_90px.png", pParent, 0);
        else
            comProgress.WaitForCompletion(-1);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            if (pParent)
                msgCenter().cannotAcquireCloudMachineParameter(comProgress, pParent);
            else
                strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        }
    }
}

QMap<QString, QString> UICloudNetworkingStuff::getImageInfo(const CCloudClient &comCloudClient,
                                                            const QString &strId,
                                                            QString &strErrorMessage,
                                                            QWidget *pParent /* = 0 */)
{
    /* Prepare result: */
    QMap<QString, QString> resultMap;

    /* Execute GetImageInfo async method: */
    CStringArray comStringArray;
    CProgress comProgress = comCloudClient.GetImageInfo(strId, comStringArray);
    if (!comCloudClient.isOk())
    {
        if (pParent)
            msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
        else
            strErrorMessage = UIErrorString::formatErrorInfo(comCloudClient);
    }
    else
    {
        /* Show "Acquire image info" progress: */
        if (pParent)
            msgCenter().showModalProgressDialog(comProgress,
                                                UICommon::tr("Acquire cloud image info ..."),
                                                ":/progress_reading_appliance_90px.png", pParent, 0);
        else
            comProgress.WaitForCompletion(-1);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            if (pParent)
                msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
            else
                strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        }
        else
        {
            /* Append resulting list values we have, split them by pairs: */
            foreach (const QString &strPair, comStringArray.GetValues())
            {
                const QList<QString> pair = strPair.split(" = ");
                AssertReturn(pair.size() == 2, resultMap);
                resultMap[pair.at(0)] = pair.at(1);
            }
        }
    }

    /* Return result: */
    return resultMap;
}

QString UICloudNetworkingStuff::fetchOsType(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Prepare a map of known OS types: */
    QMap<QString, QString> osTypes;
    osTypes["Custom"] = QString("Other");
    osTypes["Oracle Linux"] = QString("Oracle_64");
    osTypes["Canonical Ubuntu"] = QString("Ubuntu_64");

    /* Return OS type value: */
    return osTypes.value(infoMap.value(KVirtualSystemDescriptionType_OS), "Other");
}

int UICloudNetworkingStuff::fetchMemorySize(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Return memory size value: */
    return infoMap.value(KVirtualSystemDescriptionType_Memory).toInt();
}

int UICloudNetworkingStuff::fetchCpuCount(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Return CPU count value: */
    return infoMap.value(KVirtualSystemDescriptionType_CPU).toInt();
}

QString UICloudNetworkingStuff::fetchShape(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Return shape value: */
    return infoMap.value(KVirtualSystemDescriptionType_CloudInstanceShape);
}

QString UICloudNetworkingStuff::fetchDomain(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Return domain value: */
    return infoMap.value(KVirtualSystemDescriptionType_CloudDomain);
}

KMachineState UICloudNetworkingStuff::fetchMachineState(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Prepare a map of known machine states: */
    QMap<QString, KMachineState> machineStates;
    machineStates["RUNNING"] = KMachineState_Running;
    machineStates["STOPPED"] = KMachineState_Paused;
    machineStates["STOPPING"] = KMachineState_Stopping;
    machineStates["STARTING"] = KMachineState_Starting;

    /* Return machine state value: */
    return machineStates.value(infoMap.value(KVirtualSystemDescriptionType_CloudInstanceState), KMachineState_PoweredOff);
}

QString UICloudNetworkingStuff::fetchBootingFirmware(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Return booting firmware value: */
    return infoMap.value(KVirtualSystemDescriptionType_BootingFirmware);
}

QString UICloudNetworkingStuff::fetchImageId(const QMap<KVirtualSystemDescriptionType, QString> &infoMap)
{
    /* Return image id value: */
    return infoMap.value(KVirtualSystemDescriptionType_CloudImageId);
}
