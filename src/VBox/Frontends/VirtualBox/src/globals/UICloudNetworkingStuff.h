/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudNetworkingStuff namespace declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UICloudNetworkingStuff_h
#define FEQT_INCLUDED_SRC_globals_UICloudNetworkingStuff_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudProfile.h"
#include "CCloudProvider.h"
#include "CCloudProviderManager.h"
#include "CCloudClient.h"
#include "CCloudMachine.h"
#include "CForm.h"

/** Cloud networking stuff namespace. */
namespace UICloudNetworkingStuff
{
    /** Acquires cloud provider manager. */
    SHARED_LIBRARY_STUFF CCloudProviderManager cloudProviderManager();
    /** Acquires cloud provider specified by @a strProviderShortName. */
    SHARED_LIBRARY_STUFF CCloudProvider cloudProviderByShortName(const QString &strProviderShortName);
    /** Acquires cloud profile specified by @a strProviderShortName and @a strProfileName. */
    SHARED_LIBRARY_STUFF CCloudProfile cloudProfileByName(const QString &strProviderShortName,
                                                          const QString &strProfileName);
    /** Acquires cloud client specified by @a strProviderShortName and @a strProfileName. */
    SHARED_LIBRARY_STUFF CCloudClient cloudClientByName(const QString &strProviderShortName,
                                                        const QString &strProfileName);
    /** Acquires cloud machine specified by @a strProviderShortName, @a strProfileName and @a uMachineId. */
    SHARED_LIBRARY_STUFF CCloudMachine cloudMachineById(const QString &strProviderShortName,
                                                        const QString &strProfileName,
                                                        const QUuid &uMachineId);

    /** Acquires @a comCloudMachine ID as a @a uResult, using @a pParent to show messages according to. */
    SHARED_LIBRARY_STUFF bool cloudMachineId(const CCloudMachine &comCloudMachine,
                                             QUuid &uResult,
                                             QWidget *pParent = 0);
    /** Acquires @a comCloudMachine accessible state as a @a fResult, using @a pParent to show messages according to. */
    SHARED_LIBRARY_STUFF bool cloudMachineAccessible(const CCloudMachine &comCloudMachine,
                                                     bool &fResult,
                                                     QWidget *pParent = 0);
    /** Acquires @a comCloudMachine accessible error as a @a comResult, using @a pParent to show messages according to. */
    SHARED_LIBRARY_STUFF bool cloudMachineAccessError(const CCloudMachine &comCloudMachine,
                                                      CVirtualBoxErrorInfo &comResult,
                                                      QWidget *pParent = 0);
    /** Acquires @a comCloudMachine name as a @a strResult, using @a pParent to show messages according to. */
    SHARED_LIBRARY_STUFF bool cloudMachineName(const CCloudMachine &comCloudMachine,
                                               QString &strResult,
                                               QWidget *pParent = 0);
    /** Acquires @a comCloudMachine OS type ID as a @a strResult, using @a pParent to show messages according to. */
    SHARED_LIBRARY_STUFF bool cloudMachineOSTypeId(const CCloudMachine &comCloudMachine,
                                                   QString &strResult,
                                                   QWidget *pParent = 0);
    /** Acquires @a comCloudMachine state as a @a enmResult, using @a pParent to show messages according to. */
    SHARED_LIBRARY_STUFF bool cloudMachineState(const CCloudMachine &comCloudMachine,
                                                KMachineState &enmResult,
                                                QWidget *pParent = 0);

    /** Acquires @a comCloudMachine settings form as a @a comResult, using @a pParent to show messages according to.
      * @note  Be aware, this is a blocking function, corresponding progress dialog will be executed. */
    SHARED_LIBRARY_STUFF bool cloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                       CForm &comResult,
                                                       QWidget *pParent = 0);
    /** Acquires @a comCloudMachine settings form as a @a comResult, using @a strErrorMessage to store messages to.
      * @note  Be aware, this is a blocking function, it will hang for a time of progress being executed. */
    SHARED_LIBRARY_STUFF bool cloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                       CForm &comResult,
                                                       QString &strErrorMessage);

    /** Applies @a comCloudMachine @a comForm settings, using @a pParent to show messages according to.
      * @note  Be aware, this is a blocking function, corresponding progress dialog will be executed. */
    SHARED_LIBRARY_STUFF bool applyCloudMachineSettingsForm(CCloudMachine comCloudMachine,
                                                            CForm comForm,
                                                            QWidget *pParent = 0);

    /** Acquires instance map.
      * @param  comCloudClient   Brings cloud client object.
      * @param  strErrorMessage  Brings error message container.
      * @param  pWidget          Brings parent widget to show messages according to,
      *                          if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF QMap<QString, QString> listInstances(const CCloudClient &comCloudClient,
                                                              QString &strErrorMessage,
                                                              QWidget *pParent = 0);
    /** Acquires cloud machine vector.
      * @param  comCloudClient   Brings cloud client object.
      * @param  strErrorMessage  Brings error message container.
      * @param  pWidget          Brings parent widget to show messages according to,
      *                          if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF QVector<CCloudMachine> listCloudMachines(CCloudClient comCloudClient,
                                                                  QString &strErrorMessage,
                                                                  QWidget *pParent = 0);

    /** Acquires instance info as a map.
      * @param  comCloudClient   Brings cloud client object.
      * @param  strId            Brings cloud instance id.
      * @param  strErrorMessage  Brings error message container.
      * @param  pWidget          Brings parent widget to show messages according to,
      *                          if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF QMap<KVirtualSystemDescriptionType, QString> getInstanceInfo(const CCloudClient &comCloudClient,
                                                                                      const QString &strId,
                                                                                      QString &strErrorMessage,
                                                                                      QWidget *pParent = 0);
    /** Acquires instance info of certain @a enmType as a string.
      * @param  comCloudClient   Brings cloud client object.
      * @param  strId            Brings cloud instance id.
      * @param  strErrorMessage  Brings error message container.
      * @param  pWidget          Brings parent widget to show messages according to,
      *                          if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF QString getInstanceInfo(KVirtualSystemDescriptionType enmType,
                                                 const CCloudClient &comCloudClient,
                                                 const QString &strId,
                                                 QString &strErrorMessage,
                                                 QWidget *pParent = 0);
    /** Refreshes cloud machine information.
      * @param  comCloudMachine  Brings cloud machine object.
      * @param  strErrorMessage  Brings error message container.
      * @param  pWidget          Brings parent widget to show messages according to,
      *                          if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF void refreshCloudMachineInfo(CCloudMachine comCloudMachine,
                                                      QString &strErrorMessage,
                                                      QWidget *pParent = 0);

    /** Acquires image info as a map.
      * @param  comCloudClient   Brings cloud client object.
      * @param  strId            Brings cloud image id.
      * @param  strErrorMessage  Brings error message container.
      * @param  pWidget          Brings parent widget to show messages according to,
      *                          if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF QMap<QString, QString> getImageInfo(const CCloudClient &comCloudClient,
                                                             const QString &strId,
                                                             QString &strErrorMessage,
                                                             QWidget *pParent = 0);
}

/* Using across any module who included us: */
using namespace UICloudNetworkingStuff;

#endif /* !FEQT_INCLUDED_SRC_globals_UICloudNetworkingStuff_h */
