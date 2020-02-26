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
      * @param  comCloudClient  Brings the cloud client object reference.
      * @param  strId           Brings the cloud VM id.
      * @param  strName         Brings the cloud VM name. */
    UICloudMachineData(const CCloudClient &comCloudClient,
                       const QString &strId,
                       const QString &strName);
    /** Constructs shared cloud VM data on the basis of @a other data. */
    UICloudMachineData(const UICloudMachineData &other);
    /** Destructs shared cloud VM data. */
    virtual ~UICloudMachineData();

    /** Returns cloud client object reference. */
    CCloudClient cloudClient() const;

    /** Returns cloud VM id. */
    QString id() const;
    /** Returns cloud VM name. */
    QString name() const;

    /** Returns whether cloud VM is accessible. */
    bool isAccessible() const;

    /** Returns cloud VM OS type. */
    QString osType();
    /** Returns cloud VM memory size. */
    int memorySize();
    /** Returns cloud VM CPU count. */
    int cpuCount();

private:

    /** Holds the cloud client object reference. */
    CCloudClient  m_comCloudClient;

    /** Holds the cloud VM id. */
    const QString  m_strId;
    /** Holds the cloud VM name. */
    const QString  m_strName;

    /** Holds whether cloud VM is accessible. */
    bool  m_fAccessible;

    /** Holds the cloud VM OS type. */
    QString  m_strOsType;
    /** Holds the cloud VM memory size. */
    int      m_iMemorySize;
    /** Holds the cloud VM CPU count. */
    int      m_iCpuCount;
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
      * @param  comCloudClient  Brings the cloud client object instance.
      * @param  strId           Brings the cloud VM id.
      * @param  strName         Brings the cloud VM name. */
    UICloudMachine(const CCloudClient &comCloudClient,
                   const QString &strId,
                   const QString &strName);
    /** Constructs cloud VM wrapper on the basis of @a other wrapper. */
    UICloudMachine(const UICloudMachine &other);

    /** Returns whether cloud VM wrapper is null. */
    bool isNull() const { return !d.constData(); }

    /** Returns cloud client object reference. */
    CCloudClient client() const { return d->cloudClient(); }

    /** Returns cloud VM id. */
    QString id() const { return d->id(); }
    /** Returns cloud VM name. */
    QString name() const { return d->name(); }

    /** Returns whether cloud VM is accessible. */
    bool isAccessible() const { return d->isAccessible(); }

    /** Returns cloud VM OS type. */
    QString osType() const { return d->osType(); }
    /** Returns cloud VM memory size. */
    int memorySize() const { return d->memorySize(); }
    /** Returns cloud VM CPU count. */
    int cpuCount() const { return d->cpuCount(); }

private:

    /** Holds the pointer to explicitly shared cloud VM data. */
    QExplicitlySharedDataPointer<UICloudMachineData>  d;
};

/* Make meta-object sub-system aware: */
Q_DECLARE_METATYPE(UICloudMachine);

#endif /* !FEQT_INCLUDED_SRC_globals_UICloudMachine_h */
