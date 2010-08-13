/*++

Copyright (c) 1996-1998  Microsoft Corporation

Module Name:

    cldskwmi.c

Abstract:

    km wmi tracing code.

    Will be shared between our drivers.


Environment:

    kernel mode only

Notes:



Comments:

        This code is a quick hack to enable WMI tracing in cluster drivers.
        It should eventually go away.

        WmlTinySystemControl will be replaced with WmilibSystemControl from wmilib.sys .

        WmlTrace or equivalent will be added to the kernel in addition to IoWMIWriteEvent(&TraceBuffer);

--*/
#include "precomp.h"
#pragma hdrstop

//#include <ntos.h>
//#include <ntrtl.h>
//#include <nturtl.h>


// #include <wmistr.h>
// #include <evntrace.h>

// #include "wmlkm.h"

BOOLEAN
WmlpFindGuid(
    __in PVOID GuidList,
    __in ULONG GuidCount,
    __in LPVOID Guid,
    __out PULONG GuidIndex
    )
/*++

Routine Description:

    This routine will search the list of guids registered and return
    the index for the one that was registered.

Arguments:

    GuidList is the list of guids to search

    GuidCount is the count of guids in the list

    Guid is the guid being searched for

    *GuidIndex returns the index to the guid

Return Value:

    TRUE if guid is found else FALSE

--*/
{

    return(FALSE);
}


NTSTATUS
WmlTinySystemControl(
    __inout PVOID WmiLibInfo,
    __in PVOID DeviceObject,
    __in PVOID Irp
    )
/*++

Routine Description:

    Dispatch routine for IRP_MJ_SYSTEM_CONTROL. This routine will process
    all wmi requests received, forwarding them if they are not for this
    driver or determining if the guid is valid and if so passing it to
    the driver specific function for handing wmi requests.

Arguments:

    WmiLibInfo has the WMI information control block

    DeviceObject - Supplies a pointer to the device object for this request.

    Irp - Supplies the Irp making the request.

Return Value:

    status

--*/

{
    return(STATUS_WMI_GUID_NOT_FOUND);
}

ULONG
WmlTrace(
    __in ULONG Type,
    __in LPVOID TraceGuid,
    __in ULONG64 LoggerHandle,
    ... // Pairs: Address, Length
    )
{
    return STATUS_SUCCESS;
}


ULONG
WmlPrintf(
    __in ULONG Type,
    __in LPCGUID TraceGuid,
    __in ULONG64 LoggerHandle,
    __in PCWSTR FormatString,
    ... // printf var args
    )
{
    return STATUS_SUCCESS;
}

