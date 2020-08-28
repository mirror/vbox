/* $Id$ */
/** @file
 * VirtualBox Main - IHostDrive implementation, VBoxSVC.
 */

/*
 * Copyright (C) 2013-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_HostDriveImpl_h
#define MAIN_INCLUDED_HostDriveImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HostDriveWrap.h"

class ATL_NO_VTABLE HostDrive
    : public HostDriveWrap
{
public:
    DECLARE_EMPTY_CTOR_DTOR(HostDrive)

    HRESULT FinalConstruct();
    void FinalRelease();

    /** @name Public initializer/uninitializer for internal purposes only.
     * @{ */
    HRESULT initFromPathAndModel(const com::Utf8Str &drivePath, const com::Utf8Str &driveModel);
    void uninit();
    /** @} */

    com::Utf8Str i_getDrivePath() { return m.drivePath; }

private:
    /** @name wrapped IHostDrive properties
     * @{ */
    virtual HRESULT getPartitioningType(PartitioningType_T *aPartitioningType) RT_OVERRIDE;
    virtual HRESULT getDrivePath(com::Utf8Str &aDrivePath) RT_OVERRIDE;
    virtual HRESULT getUuid(com::Guid &aUuid) RT_OVERRIDE;
    virtual HRESULT getSectorSize(ULONG *aSectorSize) RT_OVERRIDE;
    virtual HRESULT getSize(LONG64 *aSize) RT_OVERRIDE;
    virtual HRESULT getModel(com::Utf8Str &aModel) RT_OVERRIDE;
    virtual HRESULT getPartitions(std::vector<ComPtr<IHostDrivePartition> > &aPartitions) RT_OVERRIDE;
    /** @} */

    /** Data. */
    struct Data
    {
        Data() : cbSector(0), cbDisk(0)
        {
        }

        PartitioningType_T partitioningType;
        com::Utf8Str drivePath;
        com::Guid uuid;
        uint32_t cbSector;
        uint64_t cbDisk;
        com::Utf8Str model;
        std::vector<ComPtr<IHostDrivePartition> > partitions;
    };

    Data m;
};

#endif /* !MAIN_INCLUDED_HostDriveImpl_h */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
