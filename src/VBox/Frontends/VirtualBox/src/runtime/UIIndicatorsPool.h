/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIIndicatorsPool class declaration
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIIndicatorsPool_h__
#define __UIIndicatorsPool_h__

/* GUI includes: */
#include "QIStateIndicator.h"

/* Forward declarations: */
class CSession;

/* Indicator types: */
enum UIIndicatorIndex
{
    UIIndicatorIndex_HardDisks,
    UIIndicatorIndex_OpticalDisks,
    UIIndicatorIndex_FloppyDisks,
    UIIndicatorIndex_NetworkAdapters,
    UIIndicatorIndex_USBDevices,
    UIIndicatorIndex_SharedFolders,
    UIIndicatorIndex_VideoCapture,
    UIIndicatorIndex_Virtualization,
    UIIndicatorIndex_Mouse,
    UIIndicatorIndex_Hostkey,
    UIIndicatorIndex_End
};

/* Indicator pool interface/prototype: */
class UIIndicatorsPool : public QObject
{
    Q_OBJECT;

public:

    /* Constructor/destructor: */
    UIIndicatorsPool(CSession &session, QObject *pParent);
    ~UIIndicatorsPool();

    /* API indicator access stuff: */
    QIStateIndicator* indicator(UIIndicatorIndex index);

private:

    /* Helpers: Prepare/cleanup stuff: */
    void prepare();
    void cleanup();

    /* Variables: */
    CSession &m_session;
    QVector<QIStateIndicator*> m_pool;
};

#endif // __UIIndicatorsPool_h__

