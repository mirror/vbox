/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIIndicatorsPool class declaration
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __UIIndicatorsPool_h__
#define __UIIndicatorsPool_h__

/* Local includes */
#include "QIStateIndicator.h"

class CSession;

enum UIIndicatorIndex
{
    UIIndicatorIndex_HardDisks,
    UIIndicatorIndex_OpticalDisks,
    UIIndicatorIndex_FloppyDisks,
    UIIndicatorIndex_NetworkAdapters,
    UIIndicatorIndex_USBDevices,
    UIIndicatorIndex_SharedFolders,
    UIIndicatorIndex_VRDP,
    UIIndicatorIndex_Virtualization,
    UIIndicatorIndex_Mouse,
    UIIndicatorIndex_Hostkey,
    UIIndicatorIndex_End
};

class UIIndicatorsPool : public QObject
{
    Q_OBJECT;

public:

    UIIndicatorsPool(CSession &session, QObject *pParent);
   ~UIIndicatorsPool();

    QIStateIndicator* indicator(UIIndicatorIndex index);

private:

    CSession &m_session;
    QVector<QIStateIndicator*> m_IndicatorsPool;
};

#endif // __UIIndicatorsPool_h__

