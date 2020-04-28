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
#include "CForm.h"
#include "CProgress.h"
#include "CStringArray.h"
#include "CVirtualBox.h"
#include "CVirtualBoxErrorInfo.h"
#include "CVirtualSystemDescription.h"


CCloudProviderManager UICloudNetworkingStuff::cloudProviderManager(QWidget *pParent /* = 0 */)
{
    /* Acquire VBox: */
    const CVirtualBox comVBox = uiCommon().virtualBox();
    if (comVBox.isNotNull())
    {
        /* Acquire cloud provider manager: */
        CCloudProviderManager comProviderManager = comVBox.GetCloudProviderManager();
        if (!comVBox.isOk())
            msgCenter().cannotAcquireCloudProviderManager(comVBox, pParent);
        else
            return comProviderManager;
    }
    /* Null by default: */
    return CCloudProviderManager();
}

CCloudProviderManager UICloudNetworkingStuff::cloudProviderManager(QString &strErrorMessage)
{
    /* Acquire VBox: */
    const CVirtualBox comVBox = uiCommon().virtualBox();
    if (comVBox.isNotNull())
    {
        /* Acquire cloud provider manager: */
        CCloudProviderManager comProviderManager = comVBox.GetCloudProviderManager();
        if (!comVBox.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comVBox);
        else
            return comProviderManager;
    }
    /* Null by default: */
    return CCloudProviderManager();
}

CCloudProvider UICloudNetworkingStuff::cloudProviderByShortName(const QString &strProviderShortName,
                                                                QWidget *pParent /* = 0 */)
{
    /* Acquire cloud provider manager: */
    CCloudProviderManager comProviderManager = cloudProviderManager(pParent);
    if (comProviderManager.isNotNull())
    {
        /* Acquire cloud provider: */
        CCloudProvider comProvider = comProviderManager.GetProviderByShortName(strProviderShortName);
        if (!comProviderManager.isOk())
            msgCenter().cannotAcquireCloudProviderManagerParameter(comProviderManager, pParent);
        else
            return comProvider;
    }
    /* Null by default: */
    return CCloudProvider();
}

CCloudProvider UICloudNetworkingStuff::cloudProviderByShortName(const QString &strProviderShortName,
                                                                QString &strErrorMessage)
{
    /* Acquire cloud provider manager: */
    CCloudProviderManager comProviderManager = cloudProviderManager(strErrorMessage);
    if (comProviderManager.isNotNull())
    {
        /* Acquire cloud provider: */
        CCloudProvider comProvider = comProviderManager.GetProviderByShortName(strProviderShortName);
        if (!comProviderManager.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comProviderManager);
        else
            return comProvider;
    }
    /* Null by default: */
    return CCloudProvider();
}

CCloudProfile UICloudNetworkingStuff::cloudProfileByName(const QString &strProviderShortName,
                                                         const QString &strProfileName,
                                                         QWidget *pParent /* = 0 */)
{
    /* Acquire cloud provider: */
    CCloudProvider comProvider = cloudProviderByShortName(strProviderShortName, pParent);
    if (comProvider.isNotNull())
    {
        /* Acquire cloud profile: */
        CCloudProfile comProfile = comProvider.GetProfileByName(strProfileName);
        if (!comProvider.isOk())
            msgCenter().cannotAcquireCloudProviderParameter(comProvider, pParent);
        else
            return comProfile;
    }
    /* Null by default: */
    return CCloudProfile();
}

CCloudProfile UICloudNetworkingStuff::cloudProfileByName(const QString &strProviderShortName,
                                                         const QString &strProfileName,
                                                         QString &strErrorMessage)
{
    /* Acquire cloud provider: */
    CCloudProvider comProvider = cloudProviderByShortName(strProviderShortName, strErrorMessage);
    if (comProvider.isNotNull())
    {
        /* Acquire cloud profile: */
        CCloudProfile comProfile = comProvider.GetProfileByName(strProfileName);
        if (!comProvider.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comProvider);
        else
            return comProfile;
    }
    /* Null by default: */
    return CCloudProfile();
}

CCloudClient UICloudNetworkingStuff::cloudClientByName(const QString &strProviderShortName,
                                                       const QString &strProfileName,
                                                       QWidget *pParent /* = 0 */)
{
    /* Acquire cloud profile: */
    CCloudProfile comProfile = cloudProfileByName(strProviderShortName, strProfileName, pParent);
    if (comProfile.isNotNull())
    {
        /* Create cloud client: */
        CCloudClient comClient = comProfile.CreateCloudClient();
        if (!comProfile.isOk())
            msgCenter().cannotAcquireCloudProfileParameter(comProfile, pParent);
        else
            return comClient;
    }
    /* Null by default: */
    return CCloudClient();
}

CCloudClient UICloudNetworkingStuff::cloudClientByName(const QString &strProviderShortName,
                                                       const QString &strProfileName,
                                                       QString &strErrorMessage)
{
    /* Acquire cloud profile: */
    CCloudProfile comProfile = cloudProfileByName(strProviderShortName, strProfileName, strErrorMessage);
    if (comProfile.isNotNull())
    {
        /* Create cloud client: */
        CCloudClient comClient = comProfile.CreateCloudClient();
        if (!comProfile.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comProfile);
        else
            return comClient;
    }
    /* Null by default: */
    return CCloudClient();
}

CCloudMachine UICloudNetworkingStuff::cloudMachineById(const QString &strProviderShortName,
                                                       const QString &strProfileName,
                                                       const QUuid &uMachineId,
                                                       QWidget *pParent /* = 0 */)
{
    /* Acquire cloud client: */
    CCloudClient comClient = cloudClientByName(strProviderShortName, strProfileName, pParent);
    if (comClient.isNotNull())
    {
        /* Acquire cloud machine: */
        CCloudMachine comMachine = comClient.GetCloudMachine(uMachineId);
        if (!comClient.isOk())
            msgCenter().cannotAcquireCloudClientParameter(comClient, pParent);
        else
            return comMachine;
    }
    /* Null by default: */
    return CCloudMachine();
}

CCloudMachine UICloudNetworkingStuff::cloudMachineById(const QString &strProviderShortName,
                                                       const QString &strProfileName,
                                                       const QUuid &uMachineId,
                                                       QString &strErrorMessage)
{
    /* Acquire cloud client: */
    CCloudClient comClient = cloudClientByName(strProviderShortName, strProfileName, strErrorMessage);
    if (comClient.isNotNull())
    {
        /* Acquire cloud machine: */
        CCloudMachine comMachine = comClient.GetCloudMachine(uMachineId);
        if (!comClient.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comClient);
        else
            return comMachine;
    }
    /* Null by default: */
    return CCloudMachine();
}

QVector<CCloudProvider> UICloudNetworkingStuff::listCloudProviders(QWidget *pParent /* = 0 */)
{
    /* Acquire cloud provider manager: */
    CCloudProviderManager comProviderManager = cloudProviderManager();
    if (comProviderManager.isNotNull())
    {
        /* Acquire cloud providers: */
        QVector<CCloudProvider> providers = comProviderManager.GetProviders();
        if (!comProviderManager.isOk())
            msgCenter().cannotAcquireCloudProviderManagerParameter(comProviderManager, pParent);
        else
            return providers;
    }
    /* Return empty list by default: */
    return QVector<CCloudProvider>();
}

QVector<CCloudProvider> UICloudNetworkingStuff::listCloudProviders(QString &strErrorMessage)
{
    /* Acquire cloud provider manager: */
    CCloudProviderManager comProviderManager = cloudProviderManager();
    if (comProviderManager.isNotNull())
    {
        /* Acquire cloud providers: */
        QVector<CCloudProvider> providers = comProviderManager.GetProviders();
        if (!comProviderManager.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comProviderManager);
        else
            return providers;
    }
    /* Return empty list by default: */
    return QVector<CCloudProvider>();
}

bool UICloudNetworkingStuff::cloudProviderShortName(const CCloudProvider &comCloudProvider,
                                                    QString &strResult,
                                                    QWidget *pParent /* = 0 */)
{
    const QString strShortName = comCloudProvider.GetShortName();
    if (!comCloudProvider.isOk())
        msgCenter().cannotAcquireCloudProviderParameter(comCloudProvider, pParent);
    else
    {
        strResult = strShortName;
        return true;
    }
    return false;
}

QVector<CCloudProfile> UICloudNetworkingStuff::listCloudProfiles(CCloudProvider comCloudProvider,
                                                                 QWidget *pParent /* = 0 */)
{
    /* Check cloud provider: */
    if (comCloudProvider.isNotNull())
    {
        /* Acquire cloud providers: */
        QVector<CCloudProfile> profiles = comCloudProvider.GetProfiles();
        if (!comCloudProvider.isOk())
            msgCenter().cannotAcquireCloudProviderParameter(comCloudProvider, pParent);
        else
            return profiles;
    }
    /* Return empty list by default: */
    return QVector<CCloudProfile>();
}

QVector<CCloudProfile> UICloudNetworkingStuff::listCloudProfiles(CCloudProvider comCloudProvider,
                                                                 QString &strErrorMessage)
{
    /* Check cloud provider: */
    if (comCloudProvider.isNotNull())
    {
        /* Acquire cloud providers: */
        QVector<CCloudProfile> profiles = comCloudProvider.GetProfiles();
        if (!comCloudProvider.isOk())
            strErrorMessage = UIErrorString::formatErrorInfo(comCloudProvider);
        else
            return profiles;
    }
    /* Return empty list by default: */
    return QVector<CCloudProfile>();
}

bool UICloudNetworkingStuff::cloudProfileName(const CCloudProfile &comCloudProfile,
                                              QString &strResult,
                                              QWidget *pParent /* = 0 */)
{
    const QString strName = comCloudProfile.GetName();
    if (!comCloudProfile.isOk())
        msgCenter().cannotAcquireCloudProfileParameter(comCloudProfile, pParent);
    else
    {
        strResult = strName;
        return true;
    }
    return false;
}

QVector<CCloudMachine> UICloudNetworkingStuff::listCloudMachines(CCloudClient comCloudClient,
                                                                 QWidget *pParent /* = 0 */)
{
    /* Execute ReadCloudMachineList async method: */
    CProgress comProgress = comCloudClient.ReadCloudMachineList();
    if (!comCloudClient.isOk())
        msgCenter().cannotAcquireCloudClientParameter(comCloudClient, pParent);
    else
    {
        /* Show "Read cloud machines" progress: */
        msgCenter().showModalProgressDialog(comProgress,
                                            QString(),
                                            ":/progress_reading_appliance_90px.png", pParent, 0);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
        else
            return comCloudClient.GetCloudMachineList();
    }
    /* Return empty list by default: */
    return QVector<CCloudMachine>();
}

QVector<CCloudMachine> UICloudNetworkingStuff::listCloudMachines(CCloudClient comCloudClient,
                                                                 QString &strErrorMessage)
{
    /* Execute ReadCloudMachineList async method: */
    CProgress comProgress = comCloudClient.ReadCloudMachineList();
    if (!comCloudClient.isOk())
        strErrorMessage = UIErrorString::formatErrorInfo(comCloudClient);
    else
    {
        /* Show "Read cloud machines" progress: */
        comProgress.WaitForCompletion(-1);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        else
            return comCloudClient.GetCloudMachineList();
    }
    /* Return empty list by default: */
    return QVector<CCloudMachine>();
}

bool UICloudNetworkingStuff::cloudMachineId(const CCloudMachine &comCloudMachine,
                                            QUuid &uResult,
                                            QWidget *pParent /* = 0 */)
{
    const QUuid uId = comCloudMachine.GetId();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        uResult = uId;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineAccessible(const CCloudMachine &comCloudMachine,
                                                    bool &fResult,
                                                    QWidget *pParent /* = 0 */)
{
    const bool fAccessible = comCloudMachine.GetAccessible();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        fResult = fAccessible;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineAccessError(const CCloudMachine &comCloudMachine,
                                                     CVirtualBoxErrorInfo &comResult,
                                                     QWidget *pParent /* = 0 */)
{
    const CVirtualBoxErrorInfo comAccessError = comCloudMachine.GetAccessError();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        comResult = comAccessError;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineName(const CCloudMachine &comCloudMachine,
                                              QString &strResult,
                                              QWidget *pParent /* = 0 */)
{
    const QString strName = comCloudMachine.GetName();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        strResult = strName;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineOSTypeId(const CCloudMachine &comCloudMachine,
                                                  QString &strResult,
                                                  QWidget *pParent /* = 0 */)
{
    const QString strOSTypeId = comCloudMachine.GetOSTypeId();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        strResult = strOSTypeId;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::cloudMachineState(const CCloudMachine &comCloudMachine,
                                               KMachineState &enmResult,
                                               QWidget *pParent /* = 0 */)
{
    const KMachineState enmState = comCloudMachine.GetState();
    if (!comCloudMachine.isOk())
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
    else
    {
        enmResult = enmState;
        return true;
    }
    return false;
}

bool UICloudNetworkingStuff::refreshCloudMachineInfo(CCloudMachine comCloudMachine,
                                                     QWidget *pParent /* = 0 */)
{
    /* Acquire machine name first: */
    QString strMachineName;
    if (!cloudMachineName(comCloudMachine, strMachineName))
        return false;

    /* Now execute Refresh async method: */
    CProgress comProgress = comCloudMachine.Refresh();
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }

    /* Show "Refresh cloud machine information" progress: */
    msgCenter().showModalProgressDialog(comProgress,
                                        strMachineName,
                                        ":/progress_reading_appliance_90px.png", pParent, 0);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotAcquireCloudMachineParameter(comProgress, pParent);
        return false;
    }

    /* Return result: */
    return true;
}

bool UICloudNetworkingStuff::refreshCloudMachineInfo(CCloudMachine comCloudMachine,
                                                     QString &strErrorMessage)
{
    /* Execute Refresh async method: */
    CProgress comProgress = comCloudMachine.Refresh();
    if (!comCloudMachine.isOk())
    {
        strErrorMessage = UIErrorString::formatErrorInfo(comCloudMachine);
        return false;
    }

    /* Show "Refresh cloud machine information" progress: */
    comProgress.WaitForCompletion(-1);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        return false;
    }

    /* Return result: */
    return true;
}

bool UICloudNetworkingStuff::cloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                      CForm &comResult,
                                                      QWidget *pParent /* = 0 */)
{
    /* Acquire machine name first: */
    QString strMachineName;
    if (!cloudMachineName(comCloudMachine, strMachineName))
        return false;

    /* Prepare settings form: */
    CForm comForm;

    /* Now execute GetSettingsForm async method: */
    CProgress comProgress = comCloudMachine.GetSettingsForm(comForm);
    if (!comCloudMachine.isOk())
    {
        msgCenter().cannotAcquireCloudMachineParameter(comCloudMachine, pParent);
        return false;
    }

    /* Show "Get settings form" progress: */
    msgCenter().showModalProgressDialog(comProgress,
                                        strMachineName,
                                        ":/progress_settings_90px.png", pParent, 0);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotAcquireCloudClientParameter(comProgress, pParent);
        return false;
    }

    /* Return result: */
    comResult = comForm;
    return true;
}

bool UICloudNetworkingStuff::cloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                      CForm &comResult,
                                                      QString &strErrorMessage)
{
    /* Prepare settings form: */
    CForm comForm;

    /* Now execute GetSettingsForm async method: */
    CProgress comProgress = comCloudMachine.GetSettingsForm(comForm);
    if (!comCloudMachine.isOk())
    {
        strErrorMessage = UIErrorString::formatErrorInfo(comCloudMachine);
        return false;
    }

    /* Wait for "Get settings form" progress: */
    comProgress.WaitForCompletion(-1);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        strErrorMessage = UIErrorString::formatErrorInfo(comProgress);
        return false;
    }

    /* Return result: */
    comResult = comForm;
    return true;
}

bool UICloudNetworkingStuff::applyCloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                           CForm comForm,
                                                           QWidget *pParent /* = 0 */)
{
    /* Acquire machine name first: */
    QString strMachineName;
    if (!cloudMachineName(comCloudMachine, strMachineName))
        return false;

    /* Now execute Apply async method: */
    CProgress comProgress = comForm.Apply();
    if (!comForm.isOk())
    {
        msgCenter().cannotApplyCloudMachineFormSettings(comForm, strMachineName, pParent);
        return false;
    }

    /* Show "Apply" progress: */
    msgCenter().showModalProgressDialog(comProgress,
                                        strMachineName,
                                        ":/progress_settings_90px.png", pParent, 0);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotApplyCloudMachineFormSettings(comProgress, strMachineName, pParent);
        return false;
    }

    /* Return result: */
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
                                                QString(),
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
                                                        QString(),
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
                                                QString(),
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
