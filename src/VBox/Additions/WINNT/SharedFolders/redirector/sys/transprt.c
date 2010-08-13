/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 transport.c

 Abstract:

 This module implements all transport related functions in the SMB connection engine

 --*/

#include "precomp.h"
#pragma hdrstop

NTSTATUS VBoxMRxInitializeTransport ()
/*++

 Routine Description:

 This routine initializes the transport related data structures

 Returns:

 STATUS_SUCCESS if the transport data structures was successfully initialized

 Notes:

 --*/
{
    Log(("VBOXSF: VBoxMRxInitializeTransport: Called.\n"));
    return STATUS_SUCCESS;
}

NTSTATUS VBoxMRxUninitializeTransport ()
/*++

 Routine Description:

 This routine uninitializes the transport related data structures

 Notes:

 --*/
{
    Log(("VBOXSF: VBoxMRxUninitializeTransport: Called.\n"));
    return STATUS_SUCCESS;
}

