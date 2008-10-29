/** @file
 *
 * VirtualBox interface to host's power notification service
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef ____H_HOSTPOWER
#define ____H_HOSTPOWER

#include "VirtualBoxBase.h"

class VirtualBox;

typedef enum
{
    HostPowerEvent_Suspend,
    HostPowerEvent_Resume,
    HostPowerEvent_BatteryLow
} HostPowerEvent;

class HostPowerService
{
public:
    HostPowerService(VirtualBox *aVirtualBox);
    virtual ~HostPowerService();

    void notify(HostPowerEvent event);

protected:
    ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;
};

# ifdef RT_OS_WINDOWS
/**
 * The Windows hosted Power Service.
 */
class HostPowerServiceWin : public HostPowerService
{
public:
    HostPowerServiceWin(VirtualBox *aVirtualBox);
    virtual ~HostPowerServiceWin();

private:
    static DECLCALLBACK(int) NotificationThread (RTTHREAD ThreadSelf, void *pInstance);
    static LRESULT CALLBACK  WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND        mHwnd;
    RTTHREAD    mThread;
};
# endif /* RT_OS_WINDOWS */

#endif /* ____H_HOSTPOWER */
