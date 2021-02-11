/* $Id$ */
/** @file
 * ComHostUtils.cpp
 */

/*
 * Copyright (C) 2013-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#if defined(RT_OS_WINDOWS) && !defined(VBOX_COM_OUTOFPROC_MODULE)
# define VBOX_COM_OUTOFPROC_MODULE
#endif
#include <VBox/com/com.h>
#include <VBox/com/listeners.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/alloca.h>
#include <iprt/buildconfig.h>
#include <iprt/errcore.h>
#include <iprt/net.h>                   /* must come before getopt */
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <iprt/string.h>


#include "../NetLib/VBoxNetLib.h"
#include "../NetLib/shared_ptr.h"

#include <vector>
#include <list>
#include <iprt/sanitized/string>
#include <map>

#include "../NetLib/VBoxNetBaseService.h"

#ifdef RT_OS_WINDOWS /* WinMain */
# include <iprt/win/windows.h>
# include <stdlib.h>
# ifdef INET_ADDRSTRLEN
/* On Windows INET_ADDRSTRLEN defined as 22 Ws2ipdef.h, because it include port number */
#  undef INET_ADDRSTRLEN
# endif
# define INET_ADDRSTRLEN 16
#else
# include <netinet/in.h>
#endif

#include "utils.h"


VBOX_LISTENER_DECLARE(NATNetworkListenerImpl)


int createNatListener(ComNatListenerPtr& listener, const ComVirtualBoxPtr& vboxptr,
                      NATNetworkEventAdapter *adapter, /* const */ ComEventTypeArray& events)
{
    ComObjPtr<NATNetworkListenerImpl> obj;
    HRESULT hrc = obj.createObject();
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    hrc = obj->init(new NATNetworkListener(), adapter);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    ComPtr<IEventSource> esVBox;
    hrc = vboxptr->COMGETTER(EventSource)(esVBox.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    listener = obj;

    hrc = esVBox->RegisterListener(listener, ComSafeArrayAsInParam(events), true);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    return VINF_SUCCESS;
}

int destroyNatListener(ComNatListenerPtr& listener, const ComVirtualBoxPtr& vboxptr)
{
    if (listener)
    {
        ComPtr<IEventSource> esVBox;
        HRESULT hrc = vboxptr->COMGETTER(EventSource)(esVBox.asOutParam());
        AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
        if (!esVBox.isNull())
        {
            hrc = esVBox->UnregisterListener(listener);
            AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
        }
        listener.setNull();
    }
    return VINF_SUCCESS;
}

int createClientListener(ComNatListenerPtr& listener, const ComVirtualBoxClientPtr& vboxclientptr,
                         NATNetworkEventAdapter *adapter, /* const */ ComEventTypeArray& events)
{
    ComObjPtr<NATNetworkListenerImpl> obj;
    HRESULT hrc = obj.createObject();
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    hrc = obj->init(new NATNetworkListener(), adapter);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    ComPtr<IEventSource> esVBox;
    hrc = vboxclientptr->COMGETTER(EventSource)(esVBox.asOutParam());
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    listener = obj;

    hrc = esVBox->RegisterListener(listener, ComSafeArrayAsInParam(events), true);
    AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);

    return VINF_SUCCESS;
}

int destroyClientListener(ComNatListenerPtr& listener, const ComVirtualBoxClientPtr& vboxclientptr)
{
    if (listener)
    {
        ComPtr<IEventSource> esVBox;
        HRESULT hrc = vboxclientptr->COMGETTER(EventSource)(esVBox.asOutParam());
        AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
        if (!esVBox.isNull())
        {
            hrc = esVBox->UnregisterListener(listener);
            AssertComRCReturn(hrc, VERR_INTERNAL_ERROR);
        }
        listener.setNull();
    }
    return VINF_SUCCESS;
}
