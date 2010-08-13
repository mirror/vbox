/*++

 Copyright (c) 1989-1999  Microsoft Corporation

 Module Name:

 nulmrxnp.h

 Abstract:

 This module includes all network provider router interface related
 definitions for the sample

 Notes:

 This module has been built and tested only in UNICODE environment

 --*/

#ifndef _NULMRXNP_H_
#define _NULMRXNP_H_

#include "..\sys\nulmrx.h"

#include <iprt/alloc.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/log.h>
#include <VBox/version.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/Log.h>

#define MRX_VBOX_SERVER_NAME_U     L"VBOXSVR"
#define MRX_VBOX_SERVER_NAME_ALT_U L"VBOXSRV"

typedef struct _NULMRXNP_ENUMERATION_HANDLE_
{
    INT LastIndex;
} NULMRXNP_ENUMERATION_HANDLE, *PNULMRXNP_ENUMERATION_HANDLE;

#endif

