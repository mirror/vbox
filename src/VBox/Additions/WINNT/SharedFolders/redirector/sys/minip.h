/*++

 Copyright (c) 1989 - 1999 Microsoft Corporation

 Module Name:

 minip.h

 Abstract:

 Macros and definitions private to the null mini driver.

 Notes:

 This module has been built and tested only in UNICODE environment

 --*/

#ifndef _NULLMINIP_H_
#define _NULLMINIP_H_

NTHALAPI
VOID
KeStallExecutionProcessor (
        IN ULONG MicroSeconds
);

#ifndef min
#define min(a, b)       ((a) > (b) ? (b) : (a))
#endif

#define RX_VERIFY( f )  if( (f) ) ; else ASSERT( 1==0 )

//
//  Set or Validate equal
//
#define SetOrValidate(x,y,f)                                \
        if( f ) (x) = (y); else ASSERT( (x) == (y) )

//
//  RXCONTEXT data - mini-rdr context stored for async completions
//  NOTE: sizeof this struct should be == MRX_CONTEXT_SIZE !!
//

typedef struct _MRX_VBOX_COMPLETION_CONTEXT
{
    //
    //  IoStatus.Information
    //
    ULONG Information;

    //
    //  IoStatus.Status
    //
    NTSTATUS Status;

    //
    //  Outstanding I/Os
    //
    ULONG OutstandingIOs;

    //
    //  I/O type
    //
    ULONG IoType;

} MRX_VBOX_COMPLETION_CONTEXT, *PMRX_VBOX_COMPLETION_CONTEXT;

#define IO_TYPE_SYNCHRONOUS     0x00000001
#define IO_TYPE_ASYNC           0x00000010

#define VBoxMRxGetMinirdrContext(pRxContext)     \
        ((PMRX_VBOX_COMPLETION_CONTEXT)(&(pRxContext)->MRxContext[0]))

#endif // _NULLMINIP_H_

