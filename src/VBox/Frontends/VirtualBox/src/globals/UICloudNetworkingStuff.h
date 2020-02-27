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
#include "UICloudMachine.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"

/** Cloud networking stuff namespace. */
namespace UICloudNetworkingStuff
{
    /** Acquires instance list.
      * @param  comCloudClient  Brings cloud client object.
      * @param  pWidget         Brings parent widget to show messages according to,
      *                         if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF QList<UICloudMachine> listInstances(const CCloudClient &comCloudClient,
                                                             QWidget *pParent = 0);

    /** Acquires instance info as a map.
      * @param  comCloudClient  Brings cloud client object.
      * @param  strId           Brings cloud VM id.
      * @param  pWidget         Brings parent widget to show messages according to,
      *                         if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF QMap<KVirtualSystemDescriptionType, QString> getInstanceInfo(const CCloudClient &comCloudClient,
                                                                                      const QString &strId,
                                                                                      QWidget *pParent = 0);
    /** Acquires instance info of certain @a enmType as a string.
      * @param  comCloudClient  Brings cloud client object.
      * @param  strId           Brings cloud VM id.
      * @param  pWidget         Brings parent widget to show messages according to,
      *                         if no parent set, progress will be executed in blocking way. */
    SHARED_LIBRARY_STUFF QString getInstanceInfo(KVirtualSystemDescriptionType enmType,
                                                 const CCloudClient &comCloudClient,
                                                 const QString &strId,
                                                 QWidget *pParent = 0);

    /** Fetches cloud VM OS type from the passed @a info. */
    SHARED_LIBRARY_STUFF QString fetchOsType(const QMap<KVirtualSystemDescriptionType, QString> &infoMap);
    /** Fetches cloud VM memory size from the passed @a info. */
    SHARED_LIBRARY_STUFF int fetchMemorySize(const QMap<KVirtualSystemDescriptionType, QString> &infoMap);
    /** Fetches cloud VM CPU count from the passed @a info. */
    SHARED_LIBRARY_STUFF int fetchCpuCount(const QMap<KVirtualSystemDescriptionType, QString> &infoMap);
    /** Fetches cloud VM instance shape from the passed @a info. */
    SHARED_LIBRARY_STUFF QString fetchInstanceShape(const QMap<KVirtualSystemDescriptionType, QString> &infoMap);
    /** Fetches cloud VM domain from the passed @a info. */
    SHARED_LIBRARY_STUFF QString fetchDomain(const QMap<KVirtualSystemDescriptionType, QString> &infoMap);
    /** Fetches cloud VM state from the passed @a info. */
    SHARED_LIBRARY_STUFF KMachineState fetchMachineState(const QMap<KVirtualSystemDescriptionType, QString> &infoMap);
    /** Fetches cloud VM booting firmware from the passed @a info. */
    SHARED_LIBRARY_STUFF QString fetchBootingFirmware(const QMap<KVirtualSystemDescriptionType, QString> &infoMap);
}

/* Using across any module who included us: */
using namespace UICloudNetworkingStuff;

#endif /* !FEQT_INCLUDED_SRC_globals_UICloudNetworkingStuff_h */
