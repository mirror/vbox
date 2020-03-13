/* $Id$ */
/** @file
 * VBox Qt GUI - UICloudMachine class declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UICloudMachine_h
#define FEQT_INCLUDED_SRC_globals_UICloudMachine_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QSharedData>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CCloudClient.h"

/** QSharedData subclass to handle shared data for cloud VM wrapper below. */
class SHARED_LIBRARY_STUFF UICloudMachineData : public QSharedData
{
public:

    /** Constructs shared cloud VM data on the basis of arguments.
      * @param  comCloudClient   Brings the cloud client object reference.
      * @param  strInstanceId    Brings the cloud instance id.
      * @param  strInstanceName  Brings the cloud instance name. */
    UICloudMachineData(const CCloudClient &comCloudClient,
                       const QString &strInstanceId,
                       const QString &strInstanceName);
    /** Constructs shared cloud VM data on the basis of @a other data. */
    UICloudMachineData(const UICloudMachineData &other);
    /** Destructs shared cloud VM data. */
    virtual ~UICloudMachineData();

    /** Performs data refreshing. */
    void refresh();

    /** Returns cloud client object reference. */
    CCloudClient cloudClient() const { return m_comCloudClient; }

    /** Returns cloud instance id. */
    QString instanceId() const { return m_strInstanceId; }
    /** Returns cloud instance name. */
    QString instanceName() const { return m_strInstanceName; }

    /** Returns whether cloud VM is accessible. */
    bool accessible() const { return m_fAccessible; }
    /** Returns last access error message. */
    QString accessError() const { return m_strAccessError; }

    /** Returns cloud VM state. */
    KMachineState machineState() const { return m_enmMachineState; }

    /** Returns cloud VM OS type. */
    QString osType() const { return m_strOsType; }
    /** Returns cloud VM memory size. */
    int memorySize() const { return m_iMemorySize; }
    /** Returns cloud VM CPU count. */
    int cpuCount() const { return m_iCpuCount; }
    /** Returns cloud VM shape. */
    QString shape() const { return m_strShape; }
    /** Returns cloud VM domain. */
    QString domain() const { return m_strDomain; }
    /** Returns cloud VM booting firmware. */
    QString bootingFirmware() const { return m_strBootingFirmware; }
    /** Returns cloud VM image id. */
    QString imageId() const { return m_strImageId; }
    /** Returns cloud VM image name. */
    QString imageName() const { return m_strImageName; }
    /** Returns cloud VM image size. */
    QString imageSize() const { return m_strImageSize; }

private:

    /** Holds the cloud client object reference. */
    CCloudClient  m_comCloudClient;

    /** Holds the cloud instance id. */
    const QString  m_strInstanceId;
    /** Holds the cloud instance name. */
    const QString  m_strInstanceName;

    /** Holds whether cloud VM is accessible. */
    bool     m_fAccessible;
    /** Holds the last access error message. */
    QString  m_strAccessError;

    /** Holds the cloud VM state. */
    KMachineState  m_enmMachineState;

    /** Holds the cloud VM OS type. */
    QString  m_strOsType;
    /** Holds the cloud VM memory size. */
    int      m_iMemorySize;
    /** Holds the cloud VM CPU count. */
    int      m_iCpuCount;
    /** Holds the cloud VM shape. */
    QString  m_strShape;
    /** Holds the cloud VM domain. */
    QString  m_strDomain;
    /** Holds the cloud VM booting firmware. */
    QString  m_strBootingFirmware;
    /** Holds the cloud VM image id. */
    QString  m_strImageId;
    /** Holds the cloud VM image name. */
    QString  m_strImageName;
    /** Holds the cloud VM image size. */
    QString  m_strImageSize;
};

/** Class representing cloud VM wrapper.
  * This is temporary class before ICloudMachine interface is represented.
  * This class is based on explicitly-shared memory and thus optimized
  * for being passed across as copied object, not just reference. */
class SHARED_LIBRARY_STUFF UICloudMachine
{
public:

    /** Constructs null cloud VM wrapper. */
    UICloudMachine();
    /** Constructs cloud VM wrapper on the basis of arguments.
      * @param  comCloudClient   Brings the cloud client object instance.
      * @param  strInstanceId    Brings the cloud instance id.
      * @param  strInstanceName  Brings the cloud instance name. */
    UICloudMachine(const CCloudClient &comCloudClient,
                   const QString &strInstanceId,
                   const QString &strInstanceName);
    /** Constructs cloud VM wrapper on the basis of @a other wrapper. */
    UICloudMachine(const UICloudMachine &other);

    /** Performs data refreshing. */
    void refresh() { d->refresh(); }

    /** Returns whether cloud VM wrapper is null. */
    bool isNull() const { return !d.constData(); }

    /** Returns cloud client object reference. */
    CCloudClient client() const { return d->cloudClient(); }

    /** Returns cloud instance id. */
    QString instanceId() const { return d->instanceId(); }
    /** Returns cloud instance name. */
    QString instanceName() const { return d->instanceName(); }

    /** Returns whether cloud VM is accessible. */
    bool accessible() const { return d->accessible(); }
    /** Returns last access error message. */
    QString accessError() const { return d->accessError(); }

    /** Returns cloud VM state. */
    KMachineState machineState() { return d->machineState(); }

    /** Returns cloud VM OS type. */
    QString osType() const { return d->osType(); }
    /** Returns cloud VM memory size. */
    int memorySize() const { return d->memorySize(); }
    /** Returns cloud VM CPU count. */
    int cpuCount() const { return d->cpuCount(); }
    /** Returns cloud VM shape. */
    QString shape() const { return d->shape(); }
    /** Returns cloud VM domain. */
    QString domain() const { return d->domain(); }
    /** Returns cloud VM booting firmware. */
    QString bootingFirmware() const { return d->bootingFirmware(); }
    /** Returns cloud VM image id. */
    QString imageId() const { return d->imageId(); }
    /** Returns cloud VM image name. */
    QString imageName() const { return d->imageName(); }
    /** Returns cloud VM image size. */
    QString imageSize() const { return d->imageSize(); }

private:

    /** Holds the pointer to explicitly shared cloud VM data. */
    QExplicitlySharedDataPointer<UICloudMachineData>  d;
};

/* Make meta-object sub-system aware: */
Q_DECLARE_METATYPE(UICloudMachine);

#endif /* !FEQT_INCLUDED_SRC_globals_UICloudMachine_h */
