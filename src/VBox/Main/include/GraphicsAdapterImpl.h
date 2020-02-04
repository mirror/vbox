/* $Id$ */
/** @file
 * Implementation of IGraphicsAdapter in VBoxSVC - Header.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_GraphicsAdapterImpl_h
#define MAIN_INCLUDED_GraphicsAdapterImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "GraphicsAdapterWrap.h"


namespace settings
{
    struct GraphicsAdapter;
};


class ATL_NO_VTABLE GraphicsAdapter :
    public GraphicsAdapterWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(GraphicsAdapter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, GraphicsAdapter *aThat);
    HRESULT initCopy(Machine *aParent, GraphicsAdapter *aThat);
    void uninit();


    // public methods only for internal purposes
    HRESULT i_loadSettings(const settings::GraphicsAdapter &data);
    HRESULT i_saveSettings(settings::GraphicsAdapter &data);

    void i_rollback();
    void i_commit();
    void i_copyFrom(GraphicsAdapter *aThat);

private:

    // wrapped IGraphicsAdapter properties
    HRESULT getGraphicsControllerType(GraphicsControllerType_T *aGraphicsControllerType);
    HRESULT setGraphicsControllerType(GraphicsControllerType_T aGraphicsControllerType);
    HRESULT getVRAMSize(ULONG *aVRAMSize);
    HRESULT setVRAMSize(ULONG aVRAMSize);
    HRESULT getAccelerate3DEnabled(BOOL *aAccelerate3DEnabled);
    HRESULT setAccelerate3DEnabled(BOOL aAccelerate3DEnabled);
    HRESULT getAccelerate2DVideoEnabled(BOOL *aAccelerate2DVideoEnabled);
    HRESULT setAccelerate2DVideoEnabled(BOOL aAccelerate2DVideoEnabled);
    HRESULT getMonitorCount(ULONG *aMonitorCount);
    HRESULT setMonitorCount(ULONG aMonitorCount);

    Machine * const     mParent;
    const ComObjPtr<GraphicsAdapter> mPeer;
    Backupable<settings::GraphicsAdapter> mData;
};

#endif /* !MAIN_INCLUDED_GraphicsAdapterImpl_h */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
