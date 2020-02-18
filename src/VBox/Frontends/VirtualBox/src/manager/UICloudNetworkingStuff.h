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

#ifndef FEQT_INCLUDED_SRC_manager_UICloudNetworkingStuff_h
#define FEQT_INCLUDED_SRC_manager_UICloudNetworkingStuff_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UICloudMachine.h"

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
    QList<UICloudMachine> listInstances(const CCloudClient &comCloudClient,
                                        QWidget *pParent = 0);

    /** Acquires instance info of certain @a enmType.
      * @param  comCloudClient  Brings cloud client object.
      * @param  strId           Brings cloud VM id.
      * @param  pWidget         Brings parent widget to show messages according to,
      *                         if no parent set, progress will be executed in blocking way. */
    QString getInstanceInfo(KVirtualSystemDescriptionType enmType,
                            const CCloudClient &comCloudClient,
                            const QString &strId,
                            QWidget *pParent = 0);
}

/* Using across any module who included us: */
using namespace UICloudNetworkingStuff;

#endif /* !FEQT_INCLUDED_SRC_manager_UICloudNetworkingStuff_h */
