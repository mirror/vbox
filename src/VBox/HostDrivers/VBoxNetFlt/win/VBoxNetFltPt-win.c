/* $Id$ */
/** @file
 * VBoxNetFlt - Network Filter Driver (Host), Windows Specific Code. Protocol edge of ndis filter driver
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
/*
 * Based in part on Microsoft DDK sample code for Ndis Intermediate Miniport passthru driver sample.
 * Copyright (c) 1993-1999, Microsoft Corporation
 */

#include "VBoxNetFltCommon-win.h"

#ifdef VBOXNETADP
# error "No protocol edge"
#endif

/** protocol handle */
static NDIS_HANDLE         g_hProtHandle = NULL;
/** medium array used while opening underlying adaprot
 * we are actually binding to NdisMedium802_3 and NdisMediumWan
 * as specified in VBoxNetFlt.inf:
 * HKR, Ndi\Interfaces, FilterMediaTypes,    , "ethernet, wan" */
static NDIS_MEDIUM         g_aMediumArray[] =
                    {
                        /* Ethernet */
                        NdisMedium802_3,
                        /* Wan */
                        NdisMediumWan
                    };

/**
 * performs binding to the given adapter
 */
#ifdef VBOX_NETFLT_ONDEMAND_BIND
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDoBinding(IN PADAPT pAdapt)
#else
DECLHIDDEN(NDIS_STATUS) vboxNetFltWinPtDoBinding(IN PADAPT pAdapt, IN PNDIS_STRING pOurDeviceName, IN PNDIS_STRING pBindToDeviceName)
#endif
{
    UINT                            MediumIndex;
    NDIS_STATUS Status, Sts;

    Assert(pAdapt->PTState.PowerState == NdisDeviceStateD3);
    Assert(pAdapt->PTState.OpState == kVBoxNetDevOpState_Deinitialized);
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Initializing);

    do
    {
//        NDIS_STATUS TmpStatus;
        /* copy the bind to dev name to our buffer */
#ifdef VBOX_NETFLT_ONDEMAND_BIND
        NDIS_STRING BindToDeviceName;
        PNDIS_STRING pBindToDeviceName;
        PVBOXNETFLTINS pThis = PADAPT_2_PVBOXNETFLTINS(pAdapt);
        PWSTR pUnicode;
        ULONG cbUnicode;
        ANSI_STRING AnsiStr;

        /* most Rtlxx functions we are using here require this */
        Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

        RtlInitAnsiString(&AnsiStr, pThis->szName);
        cbUnicode = RtlAnsiStringToUnicodeSize(&AnsiStr);

        pUnicode = alloca(cbUnicode);
        BindToDeviceName.Buffer = pUnicode;
        BindToDeviceName.MaximumLength = (USHORT)cbUnicode;

        Status = RtlAnsiStringToUnicodeString(&BindToDeviceName, &AnsiStr, FALSE);
        if(!NT_SUCCESS(Status))
        {
            Assert(0);
            break;
        }

        pBindToDeviceName = &BindToDeviceName;
#else
        Status = vboxNetFltWinCopyString(&pAdapt->DeviceName, pOurDeviceName);
        if(Status != NDIS_STATUS_SUCCESS)
        {
            Assert(0);
            break;
        }
#endif

        vboxNetFltWinSetPowerState(&pAdapt->PTState, NdisDeviceStateD0);
        pAdapt->Status = NDIS_STATUS_SUCCESS;

        NdisResetEvent(&pAdapt->hEvent);

        /*
         * Now open the adapter below and complete the initialization
         */
        NdisOpenAdapter(&Status,
                          &Sts,
                          &pAdapt->hBindingHandle,
                          &MediumIndex,
                          g_aMediumArray,
                          sizeof(g_aMediumArray)/sizeof(NDIS_MEDIUM),
                          g_hProtHandle,
                          pAdapt,
                          pBindToDeviceName,
                          0,
                          NULL);

        if (Status == NDIS_STATUS_PENDING)
        {
            NdisWaitEvent(&pAdapt->hEvent, 0);
            Status = pAdapt->Status;
        }

        Assert(Status == NDIS_STATUS_SUCCESS);
        if(Status != NDIS_STATUS_SUCCESS)
        {
            vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);
            pAdapt->hBindingHandle = NULL;
            LogRel(("NdisOpenAdapter failed, Status (0c%x)", Status));
            break;
        }

        Assert(pAdapt->hBindingHandle);

        pAdapt->Medium = g_aMediumArray[MediumIndex];
        vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Initialized);

#ifndef VBOX_NETFLT_ONDEMAND_BIND
        Status = vboxNetFltWinMpInitializeDevideInstance(pAdapt);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            Log(("BindAdapter: Adapt %p, IMInitializeDeviceInstance error %x\n",
                pAdapt, Status));

            vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitializing);
            vboxNetFltWinPtCloseAdapter(pAdapt, &Sts);
            vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);
            break;
        }
#endif
    } while(0);

    return Status;
}

/**
 * Called by NDIS to bind to a miniport below.
 * @param Status            - Return status of bind here.
 * @param BindContext        - Can be passed to NdisCompleteBindAdapter if this call is pended.
 * @param DeviceName         - Device name to bind to. This is passed to NdisOpenAdapter.
 * @param SystemSpecific1    - Can be passed to NdisOpenProtocolConfiguration to read per-binding information
 * @paran SystemSpecific2    - Unused
 * @return NDIS_STATUS_PENDING    if this call is pended. In this case call NdisCompleteBindAdapter to complete.
 *                                Anything else          Completes this call synchronously */
static VOID
vboxNetFltWinPtBindAdapter(
    OUT PNDIS_STATUS            pStatus,
    IN  NDIS_HANDLE             BindContext,
    IN  PNDIS_STRING            pDeviceName,
    IN  PVOID                   SystemSpecific1,
    IN  PVOID                   SystemSpecific2
    )
{
#ifdef VBOX_NETFLT_ONDEMAND_BIND
    /* we initiate the binding ourselves by calling NdisOpenAdapter */
    LogFlow(("==> Protocol BindAdapter\n"));
    Assert(0);
    *pStatus = NDIS_STATUS_OPEN_FAILED;
    LogFlow(("<== Protocol BindAdapter\n"));
    return;
#else
    NDIS_HANDLE                     ConfigHandle = NULL;
    PNDIS_CONFIGURATION_PARAMETER   Param;
    NDIS_STRING                     DeviceStr = NDIS_STRING_CONST("UpperBindings");
    PADAPT                          pAdapt = NULL;

    UNREFERENCED_PARAMETER(BindContext);
    UNREFERENCED_PARAMETER(SystemSpecific2);

    LogFlow(("==> Protocol BindAdapter\n"));

    do
    {
        /* Access the configuration section for our binding-specific
         * parameters. */

        NdisOpenProtocolConfiguration(pStatus,
                                       &ConfigHandle,
                                       (PNDIS_STRING)SystemSpecific1);

        if (*pStatus != NDIS_STATUS_SUCCESS)
        {
            break;
        }

        /* Read the "UpperBindings" reserved key that contains a list
         * of device names representing our miniport instances corresponding
         * to this lower binding. Since this is a 1:1 IM driver, this key
         * contains exactly one name.
         *
         * If we want to implement a N:1 mux driver (N adapter instances
         * over a single lower binding), then UpperBindings will be a
         * MULTI_SZ containing a list of device names - we would loop through
         * this list, calling NdisIMInitializeDeviceInstanceEx once for
         * each name in it. */

         NdisReadConfiguration(pStatus,
                              &Param,
                              ConfigHandle,
                              &DeviceStr,
                              NdisParameterString);
        if (*pStatus != NDIS_STATUS_SUCCESS)
        {
            break;
        }

        *pStatus = vboxNetFltWinPtInitBind(&pAdapt, &Param->ParameterData.StringData, pDeviceName);
        if (*pStatus != NDIS_STATUS_SUCCESS)
        {
            break;
        }
    } while(FALSE);

    /*
     * Close the configuration handle now - see comments above with
     * the call to NdisIMInitializeDeviceInstanceEx.
     */
    if (ConfigHandle != NULL)
    {
        NdisCloseConfiguration(ConfigHandle);
    }

    LogFlow(("<== Protocol BindAdapter: pAdapt %p, Status %x\n", pAdapt, *pStatus));
#endif
}

/**
 * Completion routine for NdisOpenAdapter issued from within the vboxNetFltWinPtBindAdapter. Simply
 * unblock the caller.
 *
 * @param ProtocolBindingContext    Pointer to the adapter
 * @param Status                    Status of the NdisOpenAdapter call
 * @param OpenErrorStatus            Secondary status(ignored by us).
 * @return    None
 * */
static VOID
vboxNetFltWinPtOpenAdapterComplete(
    IN  NDIS_HANDLE             ProtocolBindingContext,
    IN  NDIS_STATUS             Status,
    IN  NDIS_STATUS             OpenErrorStatus
    )
{
    PADAPT      pAdapt =(PADAPT)ProtocolBindingContext;

    UNREFERENCED_PARAMETER(OpenErrorStatus);

    LogFlow(("==> vboxNetFltWinPtOpenAdapterComplete: Adapt %p, Status %x\n", pAdapt, Status));
    if(pAdapt->Status == NDIS_STATUS_SUCCESS)
    {
        pAdapt->Status = Status;
    }
    NdisSetEvent(&pAdapt->hEvent);
}

DECLHIDDEN(NDIS_STATUS)
vboxNetFltWinPtDoUnbinding(PADAPT pAdapt, bool bOnUnbind)
{
    NDIS_STATUS    Status;
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    PNDIS_PACKET   PacketArray[MAX_RECEIVE_PACKET_ARRAY_SIZE];
    ULONG          NumberOfPackets = 0, i;
    BOOLEAN        CompleteRequest = FALSE;
    BOOLEAN        ReturnPackets = FALSE;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    uint64_t NanoTS = RTTimeSystemNanoTS();
#endif
    int cPPUsage;

    LogFlow(("==> vboxNetFltWinPtDoUnbinding: Adapt %p\n", pAdapt));

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

#ifndef VBOX_NETFLT_ONDEMAND_BIND
    Assert(vboxNetFltWinGetOpState(&pAdapt->PTState) == kVBoxNetDevOpState_Initialized);
    /*
     * Set the flag that the miniport below is unbinding, so the request handlers will
     * fail any request comming later
     */
    RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);

    ASMAtomicUoWriteBool(&pNetFlt->fDisconnectedFromHost, true);
    ASMAtomicUoWriteBool(&pNetFlt->fRediscoveryPending, false);
    ASMAtomicUoWriteU64(&pNetFlt->NanoTSLastRediscovery, NanoTS);

//    pAdapt->PTState.DeviceState = NdisDeviceStateD3;
//    pAdapt->MPState.DeviceState = NdisDeviceStateD3;
    vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitializing);
    if(!bOnUnbind)
    {
        vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitializing);
    }

    if (pAdapt->bQueuedRequest == TRUE)
    {
        pAdapt->bQueuedRequest = FALSE;
        CompleteRequest = TRUE;
    }
    if (pAdapt->cReceivedPacketCount > 0)
    {

        NdisMoveMemory(PacketArray,
                      pAdapt->aReceivedPackets,
                      pAdapt->cReceivedPacketCount * sizeof(PNDIS_PACKET));

        NumberOfPackets = pAdapt->cReceivedPacketCount;

        pAdapt->cReceivedPacketCount = 0;
        ReturnPackets = TRUE;
    }


    RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);

    if (CompleteRequest == TRUE)
    {
        vboxNetFltWinPtRequestComplete(pAdapt,
                         &pAdapt->Request,
                         NDIS_STATUS_FAILURE );

    }
    if (ReturnPackets == TRUE)
    {
        for (i = 0; i < NumberOfPackets; i++)
        {
            vboxNetFltWinMpReturnPacket(pAdapt, PacketArray[i]);
        }
    }

    vboxNetFltWinWaitDereference(&pAdapt->MPState);

    vboxNetFltWinWaitDereference(&pAdapt->PTState);

    /* check packet pool is empty */
    cPPUsage = NdisPacketPoolUsage(pAdapt->hSendPacketPoolHandle);
    Assert(cPPUsage == 0);
    cPPUsage = NdisPacketPoolUsage(pAdapt->hRecvPacketPoolHandle);
    Assert(cPPUsage == 0);
    /* for debugging only, ignore the err in release */
    NOREF(cPPUsage);

    while (ASMAtomicUoReadBool((volatile bool *)&pAdapt->bOutstandingRequests))
    {
        /*
         * sleep till outstanding requests complete
         */
        vboxNetFltWinSleep(2);
    }

    if(!bOnUnbind || !vboxNetFltWinMpDeInitializeDevideInstance(pAdapt, &Status))
#endif /* #ifndef VBOX_NETFLT_ONDEMAND_BIND */
    {
        /*
         * We need to do some work here.
         * Close the binding below us
         * and release the memory allocated.
         */
        vboxNetFltWinPtCloseAdapter(pAdapt, &Status);
        vboxNetFltWinSetOpState(&pAdapt->PTState, kVBoxNetDevOpState_Deinitialized);

        if(!bOnUnbind)
        {
            Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitializing);
            vboxNetFltWinSetOpState(&pAdapt->MPState, kVBoxNetDevOpState_Deinitialized);
        }
        else
        {
            Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
        }
    }
    else
    {
        Assert(vboxNetFltWinGetOpState(&pAdapt->MPState) == kVBoxNetDevOpState_Deinitialized);
    }

    LogFlow(("<== vboxNetFltWinPtDoUnbinding: Adapt %p\n", pAdapt));

    return Status;
}

/**
 * Called by NDIS when we are required to unbind to the adapter below.
 * This functions shares functionality with the miniport's HaltHandler.
 * The code should ensure that NdisCloseAdapter and NdisFreeMemory is called
 * only once between the two functions
 *
 * @param Status                    Placeholder for return status
 * @param ProtocolBindingContext    Pointer to the adapter structure
 * @param UnbindContext            Context for NdisUnbindComplete() if this pends
 * @return NONE */
static VOID
vboxNetFltWinPtUnbindAdapter(
    OUT PNDIS_STATUS        pStatus,
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  NDIS_HANDLE            UnbindContext
    )
{
    PADAPT         pAdapt =(PADAPT)ProtocolBindingContext;
    PVBOXNETFLTINS pNetFltIf = PADAPT_2_PVBOXNETFLTINS(pAdapt);

    LogFlow(("==> vboxNetFltWinPtUnbindAdapter: Adapt %p\n", pAdapt));

    *pStatus = vboxNetFltWinDetachFromInterface(pAdapt, true);
    Assert(*pStatus == NDIS_STATUS_SUCCESS);

    LogFlow(("<== vboxNetFltWinPtUnbindAdapter: Adapt %p\n", pAdapt));
}

/**
 * protocol unload handler
 */
static VOID
vboxNetFltWinPtUnloadProtocol(
    VOID
)
{
    vboxNetFltWinPtDeregister();
    LogFlow(("vboxNetFltWinPtUnloadProtocol: done!\n"));
}


/**
 * Completion for the CloseAdapter call.
 *
 * @param ProtocolBindingContext    Pointer to the adapter structure
 * @param Status                    Completion status
 * @return None */
static VOID
vboxNetFltWinPtCloseAdapterComplete(
    IN    NDIS_HANDLE            ProtocolBindingContext,
    IN    NDIS_STATUS            Status
    )
{
    PADAPT      pAdapt =(PADAPT)ProtocolBindingContext;

    LogFlow(("CloseAdapterComplete: Adapt %p, Status %x\n", pAdapt, Status));
    if(pAdapt->Status == NDIS_STATUS_SUCCESS)
    {
        pAdapt->Status = Status;
    }
    NdisSetEvent(&pAdapt->hEvent);
}


/**
 * Completion for the reset.
 *
 * @param ProtocolBindingContext    Pointer to the adapter structure
 * @param Status                    Completion status
 * @return None */
static VOID
vboxNetFltWinPtResetComplete(
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  NDIS_STATUS            Status
    )
{

    UNREFERENCED_PARAMETER(ProtocolBindingContext);
    UNREFERENCED_PARAMETER(Status);
    /*
     * We never issue a reset, so we should not be here.
     */
    Assert(0);
}

/**
 * Completion handler for the previously posted request. All OIDS
 * are completed by and sent to the same miniport that they were requested for.
 * If Oid == OID_PNP_QUERY_POWER then the data structure needs to returned with all entries =
 * NdisDeviceStateUnspecified
 * @param ProtocolBindingContext    Pointer to the adapter structure
 * @param NdisRequest                The posted request
 * @param Status                    Completion status
 * @return None
 *
 */
DECLHIDDEN(VOID)
vboxNetFltWinPtRequestComplete(
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  PNDIS_REQUEST          NdisRequest,
    IN  NDIS_STATUS            Status
    )
{
    PADAPT        pAdapt = (PADAPT)ProtocolBindingContext;
    PNDIS_REQUEST pSynchRequest = pAdapt->pSynchRequest;
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    NDIS_OID      Oid = pAdapt->Request.DATA.SET_INFORMATION.Oid ;
#endif

    if(pSynchRequest == NdisRequest)
    {
        /* assynchronous completion of our synch request */

        /*1.set the status */
        pAdapt->fSynchCompletionStatus = Status;

        /* 2. set event */
        KeSetEvent(&pAdapt->hSynchCompletionEvent, 0, FALSE);

        /* 3. return; */
        return;
    }
#ifdef VBOX_NETFLT_ONDEMAND_BIND
    Assert(0);
    return;
#else

    /*
     * Since our request is not outstanding anymore
     */
    Assert(pAdapt->bOutstandingRequests == TRUE);

    pAdapt->bOutstandingRequests = FALSE;

    /*
     * Complete the Set or Query, and fill in the buffer for OID_PNP_CAPABILITIES, if need be.
     */
    switch (NdisRequest->RequestType)
    {
      case NdisRequestQueryInformation:

        /*
         * We never pass OID_PNP_QUERY_POWER down.
         */
        Assert(Oid != OID_PNP_QUERY_POWER);

        if ((Oid == OID_PNP_CAPABILITIES) && (Status == NDIS_STATUS_SUCCESS))
        {
            vboxNetFltWinMpQueryPNPCapabilities(pAdapt, &Status);
        }
        *pAdapt->BytesReadOrWritten = NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
        *pAdapt->BytesNeeded = NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded;

        if ((Oid == OID_GEN_MAC_OPTIONS) && (Status == NDIS_STATUS_SUCCESS))
        {
            /* save mac options for adaptor below us to use it with the NdisCopyLookaheadData when our ProtocolReceive is called */
            pAdapt->fMacOptions = *(PULONG)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
#ifndef VBOX_LOOPBACK_USEFLAGS
            /*
             * Remove the no-loopback bit from mac-options. In essence we are
             * telling NDIS that we can handle loopback. We don't, but the
             * interface below us does. If we do not do this, then loopback
             * processing happens both below us and above us. This is wasteful
             * at best and if Netmon is running, it will see multiple copies
             * of loopback packets when sniffing above us.
             *
             * Only the lowest miniport is a stack of layered miniports should
             * ever report this bit set to NDIS.
             */
            *(PULONG)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer &= ~NDIS_MAC_OPTION_NO_LOOPBACK;
#else
            /* we have to catch loopbacks from the underlying driver, so no duplications will occur,
             * just indicate NDIS to handle loopbacks for the packets coming from the protocol */
            *(PULONG)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer |= NDIS_MAC_OPTION_NO_LOOPBACK;
#endif
        }
        if(Oid == OID_GEN_CURRENT_PACKET_FILTER && VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt))
        {
        	/* we're here _ONLY_ in the passthru mode */
        	Assert(pAdapt->fProcessingPacketFilter == VBOXNETFLT_PFP_PASSTHRU);
        	if(pAdapt->fProcessingPacketFilter == VBOXNETFLT_PFP_PASSTHRU)
        	{
				PVBOXNETFLTINS pNetFltIf = PADAPT_2_PVBOXNETFLTINS(pAdapt);
				Assert(!pNetFltIf->fActive);
				vboxNetFltWinDereferenceModePassThru(pNetFltIf);
				vboxNetFltWinDereferenceAdapt(pAdapt);
        	}

            if(Status == NDIS_STATUS_SUCCESS)
            {
                /* the filter request is issued below only in case netflt is not active,
                 * simply update the cache here */
                /* cache the filter used by upper protocols */
                pAdapt->fUpperProtocolSetFilter = *(PULONG)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
                pAdapt->bUpperProtSetFilterInitialized = true;
            }
        }


        NdisMQueryInformationComplete(pAdapt->hMiniportHandle,
                                      Status);
        break;

      case NdisRequestSetInformation:

          Assert( Oid != OID_PNP_SET_POWER);

          if(Oid == OID_GEN_CURRENT_PACKET_FILTER && VBOXNETFLT_PROMISCUOUS_SUPPORTED(pAdapt))
          {
              PVBOXNETFLTINS pNetFltIf = PADAPT_2_PVBOXNETFLTINS(pAdapt);
              Assert(Status == NDIS_STATUS_SUCCESS);
              if(pAdapt->fProcessingPacketFilter == VBOXNETFLT_PFP_NETFLT)
              {
            	  Assert(pNetFltIf->fActive);
                  if(Status == NDIS_STATUS_SUCCESS)
                  {
                      pAdapt->fOurSetFilter = *((PULONG)pAdapt->Request.DATA.SET_INFORMATION.InformationBuffer);
                      Assert(pAdapt->fOurSetFilter == NDIS_PACKET_TYPE_PROMISCUOUS);
                  }
                  vboxNetFltWinDereferenceNetFlt(pNetFltIf);
                  vboxNetFltWinDereferenceAdapt(pAdapt);
                  pAdapt->fProcessingPacketFilter = 0;
              }
              else if(pAdapt->fProcessingPacketFilter == VBOXNETFLT_PFP_PASSTHRU)
              {
            	  Assert(!pNetFltIf->fActive);

                  if(Status == NDIS_STATUS_SUCCESS)
                  {
                      /* the request was issued when the netflt was not active, simply update the cache here */
                      pAdapt->fUpperProtocolSetFilter = *((PULONG)pAdapt->Request.DATA.SET_INFORMATION.InformationBuffer);
                      pAdapt->bUpperProtSetFilterInitialized = true;
                  }
                  vboxNetFltWinDereferenceModePassThru(pNetFltIf);
                  vboxNetFltWinDereferenceAdapt(pAdapt);
                  pAdapt->fProcessingPacketFilter = 0;
              }
          }


        *pAdapt->BytesReadOrWritten = NdisRequest->DATA.SET_INFORMATION.BytesRead;
        *pAdapt->BytesNeeded = NdisRequest->DATA.SET_INFORMATION.BytesNeeded;
        NdisMSetInformationComplete(pAdapt->hMiniportHandle,
                                    Status);
        break;

      default:
        Assert(0);
        break;
    }
#endif
}

/**
 * Status handler for the lower-edge(protocol).
 *
 * @param ProtocolBindingContext    Pointer to the adapter structure
 * @param GeneralStatus             Status code
 * @param StatusBuffer              Status buffer
 * @param StatusBufferSize          Size of the status buffer
 * @return None
 */
static VOID
vboxNetFltWinPtStatus(
    IN  NDIS_HANDLE         ProtocolBindingContext,
    IN  NDIS_STATUS         GeneralStatus,
    IN  PVOID               StatusBuffer,
    IN  UINT                StatusBufferSize
    )
{
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    PADAPT      pAdapt = (PADAPT)ProtocolBindingContext;

    /*
     * Pass up this indication only if the upper edge miniport is initialized
     * and powered on. Also ignore indications that might be sent by the lower
     * miniport when it isn't at D0.
     */
    if (vboxNetFltWinReferenceAdapt(pAdapt))
    {
        Assert(pAdapt->hMiniportHandle);

        if ((GeneralStatus == NDIS_STATUS_MEDIA_CONNECT) ||
            (GeneralStatus == NDIS_STATUS_MEDIA_DISCONNECT))
        {

            pAdapt->LastIndicatedStatus = GeneralStatus;
        }
        NdisMIndicateStatus(pAdapt->hMiniportHandle,
                            GeneralStatus,
                            StatusBuffer,
                            StatusBufferSize);

        vboxNetFltWinDereferenceAdapt(pAdapt);
    }
    /*
     * Save the last indicated media status
     */
    else
    {
        if ((pAdapt->hMiniportHandle != NULL) &&
        ((GeneralStatus == NDIS_STATUS_MEDIA_CONNECT) ||
            (GeneralStatus == NDIS_STATUS_MEDIA_DISCONNECT)))
        {
            pAdapt->LatestUnIndicateStatus = GeneralStatus;
        }
    }
#endif
}

/**
 * status complete handler
 */
static VOID
vboxNetFltWinPtStatusComplete(
    IN NDIS_HANDLE            ProtocolBindingContext
    )
{
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    PADAPT      pAdapt = (PADAPT)ProtocolBindingContext;

    /*
     * Pass up this indication only if the upper edge miniport is initialized
     * and powered on. Also ignore indications that might be sent by the lower
     * miniport when it isn't at D0.
     */
    if (vboxNetFltWinReferenceAdapt(pAdapt))
    {
        NdisMIndicateStatusComplete(pAdapt->hMiniportHandle);

        vboxNetFltWinDereferenceAdapt(pAdapt);
    }
#endif
}

/**
 * Called by NDIS when the miniport below had completed a send. We should
 * complete the corresponding upper-edge send this represents.
 *
 * @param ProtocolBindingContext - Points to ADAPT structure
 * @param Packet - Low level packet being completed
 * @param Status - status of send
 * @return None
 */
static VOID
vboxNetFltWinPtSendComplete(
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  PNDIS_PACKET           Packet,
    IN  NDIS_STATUS            Status
    )
{
    PADAPT            pAdapt = (PADAPT)ProtocolBindingContext;
    PNDIS_PACKET      Pkt;

    {
        PSEND_RSVD        SendRsvd;

#if defined(DEBUG_NETFLT_PACKETS) || !defined(VBOX_LOOPBACK_USEFLAGS)
        /* @todo: for optimization we could check only for netflt-mode packets
         * do it for all for now */
        vboxNetFltWinLbRemoveSendPacket(pAdapt, Packet);
#endif
//        Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

        SendRsvd = (PSEND_RSVD)(Packet->ProtocolReserved);
        Pkt = SendRsvd->pOriginalPkt;

#ifndef VBOX_NETFLT_ONDEMAND_BIND
        if(Pkt)
        {
#ifndef WIN9X
            NdisIMCopySendCompletePerPacketInfo (Pkt, Packet);
#endif
            NdisFreePacket(Packet);

            /* the ptk was posted from the upperlying protocol */
            NdisMSendComplete(pAdapt->hMiniportHandle,
                                     Pkt,
                                     Status);
        }
        else
#else
            /* TODO: should change the PSEND_RSVD structure as we no nolnger need  to handle original packets
             * because all packets are originated by us */
            Assert(!Pkt);
#endif
        {
            /* if the ptk is zerro - the ptk was originated by netFlt send/receive
             * need to free packet buffers */
            PVOID pBufToFree = SendRsvd->pBufToFree;

            vboxNetFltWinFreeSGNdisPacket(Packet, !pBufToFree);
            if(pBufToFree)
            {
                vboxNetFltWinMemFree(pBufToFree);
            }
        }
    }

    vboxNetFltWinDereferenceAdapt(pAdapt);
}

#ifndef VBOX_NETFLT_ONDEMAND_BIND

/**
 * removes searches for the packet in the list and removes it if found
 * @return true if the packet was found and removed, false - otherwise
 */
static bool vboxNetFltWinRemovePacketFromList(PINTERLOCKED_SINGLE_LIST pList, PNDIS_PACKET pPacket)
{
    PTRANSFERDATA_RSVD pTDR = &((PPT_RSVD)pPacket->ProtocolReserved)->u.TransferDataRsvd;
    return vboxNetFltWinInterlockedSearchListEntry(pList, &pTDR->ListEntry,
            true /* remove*/);
}

/**
 * puts the packet to the tail of the list
 */
static void vboxNetFltWinPutPacketToList(PINTERLOCKED_SINGLE_LIST pList, PNDIS_PACKET pPacket, PNDIS_BUFFER pOrigBuffer)
{
    PTRANSFERDATA_RSVD pTDR = &((PPT_RSVD)pPacket->ProtocolReserved)->u.TransferDataRsvd;
    pTDR->pOriginalBuffer = pOrigBuffer;
    vboxNetFltWinInterlockedPutTail(pList, &pTDR->ListEntry);
}

/**
 * This is to queue the received packets and indicates them up if the given Packet
 * status is NDIS_STATUS_RESOURCES, or the array is full.
 *
 * @param pAdapt   -    Pointer to the adpater structure.
 * @param Packet   -    Pointer to the indicated packet.
 * @param Indicate -  Do the indication now.
 * @return NONE
 */
static VOID
vboxNetFltWinPtQueueReceivedPacket(
    IN PADAPT       pAdapt,
    IN PNDIS_PACKET Packet,
    IN BOOLEAN      DoIndicate
    )
{
    PNDIS_PACKET    PacketArray[MAX_RECEIVE_PACKET_ARRAY_SIZE];
    ULONG           NumberOfPackets = 0, i;
    bool bReturn = false;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
    do{
        RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);

        Assert(pAdapt->cReceivedPacketCount < MAX_RECEIVE_PACKET_ARRAY_SIZE);

        /*
         * pAdapt->ReceviePacketCount must be less than MAX_RECEIVE_PACKET_ARRAY_SIZE because
         * the thread which held the pVElan->Lock before should already indicate the packet(s)
         * up if pAdapt->ReceviePacketCount == MAX_RECEIVE_PACKET_ARRAY_SIZE.
         */
        pAdapt->aReceivedPackets[pAdapt->cReceivedPacketCount] = Packet;
        pAdapt->cReceivedPacketCount++;

        /* check the device state */
        if(vboxNetFltWinGetPowerState(&pAdapt->PTState) != NdisDeviceStateD0
                || vboxNetFltWinGetPowerState(&pAdapt->MPState) != NdisDeviceStateD0
                || vboxNetFltWinGetOpState(&pAdapt->PTState) > kVBoxNetDevOpState_Initialized
                || vboxNetFltWinGetOpState(&pAdapt->MPState) > kVBoxNetDevOpState_Initialized)
        {
            /* we need to return all packets */
            bReturn = true;
        }

        /*
         *  If our receive packet array is full, or the miniport below indicated the packets
         *  with resources, do the indicatin now.
         */

        if ((pAdapt->cReceivedPacketCount == MAX_RECEIVE_PACKET_ARRAY_SIZE) || DoIndicate || bReturn)
        {
            NdisMoveMemory(PacketArray,
                           pAdapt->aReceivedPackets,
                           pAdapt->cReceivedPacketCount * sizeof(PNDIS_PACKET));

            NumberOfPackets = pAdapt->cReceivedPacketCount;
            /*
             * So other thread can queue the received packets
             */
            pAdapt->cReceivedPacketCount = 0;

            if(!bReturn)
            {
                DoIndicate = TRUE;
            }
        }
        RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
    } while(0);

    if(!bReturn)
    {
        if(DoIndicate)
        {
            NdisMIndicateReceivePacket(pAdapt->hMiniportHandle, PacketArray, NumberOfPackets);
        }
    }
    else
    {
        if (DoIndicate)
        {
            NumberOfPackets  -= 1;
        }
        for (i = 0; i < NumberOfPackets; i++)
        {
            vboxNetFltWinMpReturnPacket(pAdapt, PacketArray[i]);
        }
    }
}

#endif

static bool vboxNetFltWinPtTransferDataCompleteActive(IN PADAPT pAdapt,
        IN PNDIS_PACKET pPacket,
        IN NDIS_STATUS Status)
{
    PVBOXNETFLTINS pNetFltIf = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    PNDIS_BUFFER pBuffer;
    PTRANSFERDATA_RSVD pTDR;

    if(!vboxNetFltWinRemovePacketFromList(&pAdapt->TransferDataList, pPacket))
        return false;

    pTDR = &((PPT_RSVD)pPacket->ProtocolReserved)->u.TransferDataRsvd;
    Assert(pTDR);
    Assert(pTDR->pOriginalBuffer);

    do
    {
        NdisUnchainBufferAtFront(pPacket, &pBuffer);

        Assert(pBuffer);

        NdisFreeBuffer(pBuffer);

        pBuffer = pTDR->pOriginalBuffer;

        NdisChainBufferAtBack(pPacket, pBuffer);

        /* data transfer was initiated when the netFlt was active
         * the netFlt is still retained by us
         * 1. check if loopback
         * 2. enqueue packet
         * 3. release netFlt */

        if(Status == NDIS_STATUS_SUCCESS)
        {

#ifdef VBOX_LOOPBACK_USEFLAGS
            if(vboxNetFltWinIsLoopedBackPacket(pPacket))
            {
                /* should not be here */
                Assert(0);
            }
#else
            PNDIS_PACKET pLb = vboxNetFltWinLbSearchLoopBack(pAdapt, pPacket, false);
            if(pLb)
            {
#ifndef DEBUG_NETFLT_RECV_TRANSFERDATA
                /* should not be here */
                Assert(0);
#endif
                if(!vboxNetFltWinLbIsFromIntNet(pLb))
                {
                    /* the packet is not from int net, need to pass it up to the host */
                    vboxNetFltWinPtQueueReceivedPacket(pAdapt, pPacket, FALSE);
                    /* dereference NetFlt, pAdapt will be dereferenced on Packet return */
                    vboxNetFltWinDereferenceNetFlt(pNetFltIf);
                    break;
                }
            }
#endif
            else
            {
                PRECV_RSVD            pRecvRsvd;
                /* 2. enqueue */
                /* use the same packet info to put the packet in the processing packet queue */
#ifdef VBOX_NETFLT_ONDEMAND_BIND
                PNDIS_BUFFER pBuffer;
                PVOID pVA;
                UINT cbLength;
                uint32_t fFlags;

                NdisQueryPacket(pPacket, NULL, NULL, &pBuffer, NULL);
                NdisQueryBufferSafe(pBuffer, &pVA, &cbLength, NormalPagePriority);

                fFlags = MACS_EQUAL(((PRTNETETHERHDR)pVA)->SrcMac, pNetFltIf->u.s.Mac) ?
                                                PACKET_MINE | PACKET_SRC_HOST : PACKET_MINE;
                SET_FLAGS_TO_INFO(pInfo, fFlags);

                pRecvRsvd = (PRECV_RSVD)(pPacket->MiniportReserved);
                pRecvRsvd->pOriginalPkt = NULL;
                pRecvRsvd->pBufToFree = NULL;

                NdisSetPacketFlags(pPacket, 0);

                Status = vboxNetFltWinQuEnqueuePacket(pNetFltIf, pPacket, fFlags);
#else
                VBOXNETFLT_LBVERIFY(pNetFltIf, pPacket);

                pRecvRsvd = (PRECV_RSVD)(pPacket->MiniportReserved);
                pRecvRsvd->pOriginalPkt = NULL;
                pRecvRsvd->pBufToFree = NULL;

                NdisSetPacketFlags(pPacket, 0);

                Status = vboxNetFltWinQuEnqueuePacket(pNetFltIf, pPacket, PACKET_MINE);
#endif
                if(Status == NDIS_STATUS_SUCCESS)
                {
                    break;
                }
                Assert(0);
            }
        }
        else
        {
            Assert(0);
        }
        /* we are here because of error either in data transfer or in enqueueing the packet */
        vboxNetFltWinFreeSGNdisPacket(pPacket, true);
        vboxNetFltWinDereferenceNetFlt(pNetFltIf);
        vboxNetFltWinDereferenceAdapt(pAdapt);
    } while(0);

    return true;
}

/**
 * Entry point called by NDIS to indicate completion of a call by us
 * to NdisTransferData.
 *
 * See notes under SendComplete.
 */
static VOID
vboxNetFltWinPtTransferDataComplete(
    IN  NDIS_HANDLE         ProtocolBindingContext,
    IN  PNDIS_PACKET        pPacket,
    IN  NDIS_STATUS         Status,
    IN  UINT                BytesTransferred
    )
{
    PADAPT      pAdapt =(PADAPT)ProtocolBindingContext;
    if(!vboxNetFltWinPtTransferDataCompleteActive(pAdapt, pPacket, Status))
    {
#ifndef VBOX_NETFLT_ONDEMAND_BIND
        if(pAdapt->hMiniportHandle)
        {
            NdisMTransferDataComplete(pAdapt->hMiniportHandle,
                                      pPacket,
                                      Status,
                                      BytesTransferred);
        }

        vboxNetFltWinDereferenceAdapt(pAdapt);
#else
        /* we are here because we've failed to allocate packet info */
        Assert(0);
#endif
    }
}
#ifndef VBOX_NETFLT_ONDEMAND_BIND
/**
 * This routine process the queued the packet, if anything is fine, indicate the packet
 * up, otherwise, return the packet to the underlying miniports.
 *
 * @param pAdapt   -    Pointer to the adpater structure.
 * @param bReturn - if true the packets should be returned without indication to the upper protocol
 * @return None. */
DECLHIDDEN(VOID)
vboxNetFltWinPtFlushReceiveQueue(
    IN PADAPT         pAdapt,
    IN bool bReturn
    )
{

    PNDIS_PACKET  PacketArray[MAX_RECEIVE_PACKET_ARRAY_SIZE];
    ULONG         NumberOfPackets = 0, i;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    do
    {
        RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);

        if (pAdapt->cReceivedPacketCount > 0)
        {
            NdisMoveMemory(PacketArray,
                            pAdapt->aReceivedPackets,
                            pAdapt->cReceivedPacketCount * sizeof(PNDIS_PACKET));

            NumberOfPackets = pAdapt->cReceivedPacketCount;
            /*
             * So other thread can queue the received packets
             */
            pAdapt->cReceivedPacketCount = 0;

            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);

            if(!bReturn)
            {
                if(NumberOfPackets > 0)
                {
                    Assert(pAdapt->hMiniportHandle);

                    /* we are here because the NetFlt is NOT active,
                     * so no need for packet queueing here, simply indicate */
                    NdisMIndicateReceivePacket(pAdapt->hMiniportHandle,
                                               PacketArray,
                                               NumberOfPackets);
                }
                break;
            }
            /*
             * We need return the packet here
             */
            for (i = 0; i < NumberOfPackets; i ++)
            {
                vboxNetFltWinMpReturnPacket(pAdapt, PacketArray[i]);
            }

            /* break to ensure we do not call RTSpinlockRelease extra time */
            break;
        }

        /* we are here only in case pAdapt->cReceivedPacketCount == 0 */
        RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
    } while (FALSE);
}

/**
 * ReceivePacket handler. Called by NDIS if the miniport below supports
 * NDIS 4.0 style receives. Re-package the buffer chain in a new packet
 * and indicate the new packet to protocols above us. Any context for
 * packets indicated up must be kept in the MiniportReserved field.
 *
 * @param ProtocolBindingContext - Pointer to our adapter structure.
 * @param Packet - Pointer to the packet
 * @return INT  == 0 -> We are done with the packet
 *  != 0 -> We will keep the packet and call NdisReturnPackets() this
 *  many times when done. */
static INT
vboxNetFltWinRecvPacketPassThru(
    IN PADAPT            pAdapt,
    IN PNDIS_PACKET           pPacket
    )
{
    NDIS_STATUS         fStatus;
    PNDIS_PACKET        pMyPacket;

    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    fStatus = vboxNetFltWinPrepareRecvPacket(pAdapt, pPacket, &pMyPacket, true);

    Assert(pMyPacket);

    if(pMyPacket != NULL)
    {
        if (fStatus == NDIS_STATUS_RESOURCES)
        {
            vboxNetFltWinPtQueueReceivedPacket(pAdapt, pMyPacket, TRUE);

            /*
             * Our ReturnPackets handler will not be called for this packet.
             * We should reclaim it right here.
             */
            NdisDprFreePacket(pMyPacket);

            return 0;
        }

        vboxNetFltWinPtQueueReceivedPacket(pAdapt, pMyPacket, FALSE);

        return 1;
    }

    return 0;
}

/**
 * process the packet receive in a "passthru" mode
 */
static NDIS_STATUS
vboxNetFltWinRecvPassThru(
        IN PADAPT             pAdapt,
        IN PNDIS_PACKET           pPacket)
{

    NDIS_STATUS fStatus;
    PNDIS_PACKET pMyPacket;
    /*
     * The miniport below did indicate up a packet. Use information
     * from that packet to construct a new packet to indicate up.
     */

    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    /*
     * Get a packet off the pool and indicate that up
     */
    NdisDprAllocatePacket(&fStatus,
                            &pMyPacket,
                            pAdapt->hRecvPacketPoolHandle);
    Assert(fStatus == NDIS_STATUS_SUCCESS);
    if (fStatus == NDIS_STATUS_SUCCESS)
    {
            /*
             * Make our packet point to data from the original
             * packet. NOTE: this works only because we are
             * indicating a receive directly from the context of
             * our receive indication. If we need to queue this
             * packet and indicate it from another thread context,
             * we will also have to allocate a new buffer and copy
             * over the packet contents, OOB data and per-packet
             * information. This is because the packet data
             * is available only for the duration of this
             * receive indication call.
             */
        NDIS_PACKET_FIRST_NDIS_BUFFER(pMyPacket) = NDIS_PACKET_FIRST_NDIS_BUFFER(pPacket);
        NDIS_PACKET_LAST_NDIS_BUFFER(pMyPacket) = NDIS_PACKET_LAST_NDIS_BUFFER(pPacket);

            /*
             * Get the original packet (it could be the same packet as the
             * one received or a different one based on the number of layered
             * miniports below) and set it on the indicated packet so the OOB
             * data is visible correctly at protocols above.
             */
        NDIS_SET_ORIGINAL_PACKET(pMyPacket, NDIS_GET_ORIGINAL_PACKET(pPacket));
        NDIS_SET_PACKET_HEADER_SIZE(pMyPacket, NDIS_GET_PACKET_HEADER_SIZE(pPacket));

            /*
             * Copy packet flags.
             */
        NdisGetPacketFlags(pMyPacket) = NdisGetPacketFlags(pPacket);

            /*
             * Force protocols above to make a copy if they want to hang
             * on to data in this packet. This is because we are in our
             * Receive handler (not ReceivePacket) and we can't return a
             * ref count from here.
             */
        NDIS_SET_PACKET_STATUS(pMyPacket, NDIS_STATUS_RESOURCES);

            /*
             * By setting NDIS_STATUS_RESOURCES, we also know that we can reclaim
             * this packet as soon as the call to NdisMIndicateReceivePacket
             * returns.
             *
             * NOTE: we queue the packet and indicate this packet immediately with
             * the already queued packets together. We have to the queue the packet
             * first because some versions of NDIS might call protocols'
             * ReceiveHandler(not ReceivePacketHandler) if the packet indicate status
             * is NDIS_STATUS_RESOURCES. If the miniport below indicates an array of
             * packets, some of them with status NDIS_STATUS_SUCCESS, some of them
             * with status NDIS_STATUS_RESOURCES, vboxNetFltWinPtReceive might be called, by
             * doing this way, we preserve the receive order of packets.
             */
        vboxNetFltWinPtQueueReceivedPacket(pAdapt, pMyPacket, TRUE);
            /*
             * Reclaim the indicated packet. Since we had set its status
             * to NDIS_STATUS_RESOURCES, we are guaranteed that protocols
             * above are done with it.
             */
        NdisDprFreePacket(pMyPacket);

    }
    return fStatus;
}

#endif /* #ifndef VBOX_NETFLT_ONDEMAND_BIND */




/**
 * process the ProtocolReceive in an "active" mode
 *
 * @return NDIS_STATUS_SUCCESS - the packet is processed
 * NDIS_STATUS_PENDING - the packet is being processed, we are waiting for the ProtocolTransferDataComplete to be called
 * NDIS_STATUS_NOT_ACCEPTED - the packet is not needed - typically this is because this is a loopback packet
 * NDIS_STATUS_FAILURE - packet processing failed
 */
static NDIS_STATUS
vboxNetFltWinPtReceiveActive(
    IN PADAPT pAdapt,
    IN NDIS_HANDLE                  MacReceiveContext,
    IN PVOID                        pHeaderBuffer,
    IN UINT                         cbHeaderBuffer,
    IN PVOID                        pLookaheadBuffer,
    IN UINT                         cbLookaheadBuffer,
    IN UINT                         cbPacket
    )
{
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;

    do
    {
        if (cbHeaderBuffer != ETH_HEADER_SIZE)
        {
            Status = NDIS_STATUS_NOT_ACCEPTED;
            break;
        }

#ifndef DEBUG_NETFLT_RECV_TRANSFERDATA
        if (cbPacket == cbLookaheadBuffer)
        {
            PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
            PINTNETSG pSG;
            PUCHAR pRcvData;
#ifndef VBOX_LOOPBACK_USEFLAGS
            PNDIS_PACKET pLb;
#endif

            /* allocate SG buffer */
            Status = vboxNetFltWinAllocSG(cbPacket + cbHeaderBuffer, &pSG);
            if(Status != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                break;
            }

            pRcvData = (PUCHAR)pSG->aSegs[0].pv;

            NdisMoveMappedMemory(pRcvData, pHeaderBuffer, cbHeaderBuffer);

            NdisCopyLookaheadData(pRcvData+cbHeaderBuffer,
                                                  pLookaheadBuffer,
                                                  cbLookaheadBuffer,
                                                  pAdapt->fMacOptions);
#ifndef VBOX_LOOPBACK_USEFLAGS
            pLb = vboxNetFltWinLbSearchLoopBackBySG(pAdapt, pSG, false);
            if(pLb)
            {
                /* should not be here */
                Assert(0);

                if(!vboxNetFltWinLbIsFromIntNet(pLb))
                {
                    PNDIS_PACKET pMyPacket;
                    pMyPacket = vboxNetFltWinNdisPacketFromSG(pAdapt, /* PADAPT */
                        pSG, /* PINTNETSG */
                        pSG, /* PVOID pBufToFree */
                        false, /* bool bToWire */
                        false); /* bool bCopyMemory */
                    if(pMyPacket)
                    {
                        vboxNetFltWinPtQueueReceivedPacket(pAdapt, pMyPacket, FALSE);
                        /* dereference the NetFlt here & indicate SUCCESS, which would mean the caller would not do a dereference
                         * the pAdapt dereference will be done on packet return */
                        vboxNetFltWinDereferenceNetFlt(pNetFlt);
                        Status = NDIS_STATUS_SUCCESS;
                    }
                    else
                    {
                        vboxNetFltWinMemFree(pSG);
                        Status = NDIS_STATUS_FAILURE;
                    }
                }
                else
                {
                    vboxNetFltWinMemFree(pSG);
                    Status = NDIS_STATUS_NOT_ACCEPTED;
                }
                break;
            }
#endif
            VBOXNETFLT_LBVERIFYSG(pNetFlt, pSG);

                /* enqueue SG */
#ifdef VBOX_NETFLT_ONDEMAND_BIND
                {
                    uint32_t fFlags = MACS_EQUAL(((PRTNETETHERHDR)pRcvData)->SrcMac, pNetFlt->u.s.Mac) ?
                            PACKET_SG | PACKET_MINE | PACKET_SRC_HOST : PACKET_SG | PACKET_MINE;
                    Status = vboxNetFltWinQuEnqueuePacket(pNetFlt, pSG, fFlags);
                }
#else
            Status = vboxNetFltWinQuEnqueuePacket(pNetFlt, pSG, PACKET_SG | PACKET_MINE);
#endif
            if(Status != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                vboxNetFltWinMemFree(pSG);
                break;
            }
        }
        else
#endif /* #ifndef DEBUG_NETFLT_RECV_TRANSFERDATA */
        {
            PNDIS_PACKET            pPacket;
            PNDIS_BUFFER pTransferBuffer;
            PNDIS_BUFFER pOrigBuffer;
            PUCHAR pMemBuf;
            UINT cbBuf = cbPacket + cbHeaderBuffer;
            UINT                    BytesTransferred;

            /* allocate NDIS Packet buffer */
#ifdef VBOX_NETFLT_ONDEMAND_BIND
            /* use the Send packet pool for packet allocation */
            NdisAllocatePacket(&Status, &pPacket, pAdapt->hSendPacketPoolHandle);
#else
            NdisAllocatePacket(&Status, &pPacket, pAdapt->hRecvPacketPoolHandle);
#endif
            if(Status != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                break;
            }

            VBOXNETFLT_OOB_INIT(pPacket);

#ifdef VBOX_LOOPBACK_USEFLAGS
            /* set "don't loopback" flags */
            NdisSetPacketFlags(pPacket, g_fPacketDontLoopBack);
#else
            NdisSetPacketFlags(pPacket, 0);
#endif

            Status = vboxNetFltWinMemAlloc(&pMemBuf, cbBuf);
            if(Status != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                NdisFreePacket(pPacket);
                break;
            }
#ifdef VBOX_NETFLT_ONDEMAND_BIND
            /* use the Send buffer pool for buffer allocation */
            NdisAllocateBuffer(&Status, &pTransferBuffer, pAdapt->hSendBufferPoolHandle, pMemBuf + cbHeaderBuffer, cbPacket);
#else
            NdisAllocateBuffer(&Status, &pTransferBuffer, pAdapt->hRecvBufferPoolHandle, pMemBuf + cbHeaderBuffer, cbPacket);
#endif
            if(Status != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                Status = NDIS_STATUS_FAILURE;
                NdisFreePacket(pPacket);
                vboxNetFltWinMemFree(pMemBuf);
                break;
            }

#ifdef VBOX_NETFLT_ONDEMAND_BIND
            /* use the Send buffer pool for buffer allocation */
            NdisAllocateBuffer(&Status, &pOrigBuffer, pAdapt->hSendBufferPoolHandle, pMemBuf, cbBuf);
#else
            NdisAllocateBuffer(&Status, &pOrigBuffer, pAdapt->hRecvBufferPoolHandle, pMemBuf, cbBuf);
#endif
            if(Status != NDIS_STATUS_SUCCESS)
            {
                Assert(0);
                Status = NDIS_STATUS_FAILURE;
                NdisFreeBuffer(pTransferBuffer);
                NdisFreePacket(pPacket);
                vboxNetFltWinMemFree(pMemBuf);
                break;
            }

            NdisChainBufferAtBack(pPacket, pTransferBuffer);

            NdisMoveMappedMemory(pMemBuf, pHeaderBuffer, cbHeaderBuffer);

#ifndef VBOX_NETFLT_ONDEMAND_BIND
            vboxNetFltWinPutPacketToList(&pAdapt->TransferDataList, pPacket, pOrigBuffer);
#endif

#ifdef DEBUG_NETFLT_RECV_TRANSFERDATA
            if(cbPacket == cbLookaheadBuffer)
            {
                NdisCopyLookaheadData(pMemBuf+cbHeaderBuffer,
                                                  pLookaheadBuffer,
                                                  cbLookaheadBuffer,
                                                  pAdapt->fMacOptions);
            }
            else
#endif
            {
                Assert(cbPacket > cbLookaheadBuffer);

                NdisTransferData(
                        &Status,
                        pAdapt->hBindingHandle,
                        MacReceiveContext,
                        0,  /* ByteOffset */
                        cbPacket,
                        pPacket,
                        &BytesTransferred);
            }
            if(Status != NDIS_STATUS_PENDING)
            {
                vboxNetFltWinPtTransferDataComplete(pAdapt, pPacket, Status, BytesTransferred);
            }
        }
    } while(0);

    return Status;
}


/**
 * Handle receive data indicated up by the miniport below. We pass
 * it along to the protocol above us.
 *
 * If the miniport below indicates packets, NDIS would more
 * likely call us at our ReceivePacket handler. However we
 * might be called here in certain situations even though
 * the miniport below has indicated a receive packet, e.g.
 * if the miniport had set packet status to NDIS_STATUS_RESOURCES.
 *
 * @param ProtocolBindingContext
 * @param MacReceiveContext
 * @param pHeaderBuffer
 * @param cbHeaderBuffer
 * @param pLookAheadBuffer
 * @param cbLookAheadBuffer
 * @param cbPacket
 * @return NDIS_STATUS_SUCCESS if we processed the receive successfully,
 *      NDIS_STATUS_XXX error code if we discarded it. */
static NDIS_STATUS
vboxNetFltWinPtReceive(
    IN  NDIS_HANDLE         ProtocolBindingContext,
    IN  NDIS_HANDLE         MacReceiveContext,
    IN  PVOID               pHeaderBuffer,
    IN  UINT                cbHeaderBuffer,
    IN  PVOID               pLookAheadBuffer,
    IN  UINT                cbLookAheadBuffer,
    IN  UINT                cbPacket
    )
{
    PADAPT            pAdapt = (PADAPT)ProtocolBindingContext;
    NDIS_STATUS       Status = NDIS_STATUS_SUCCESS;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
#ifdef VBOX_NETFLT_ONDEMAND_BIND
#if 0
    uint32_t fFlags;
#endif

    pNetFltIf = vboxNetFltWinReferenceAdaptNetFltFromAdapt(pAdapt);
    if(pNetFltIf)
    {
        do
        {
#if 0
            pPacket = NdisGetReceivedPacket(pAdapt->hBindingHandle, MacReceiveContext);
            if(pPacket)
            {
# ifdef DEBUG_NETFLT_LOOPBACK
# error "implement (see comments in the sources below this #error:)"
        /* @todo FIXME no need for the PPACKET_INFO mechanism here;
        instead the the NDIS_PACKET.ProtocolReserved + INTERLOCKED_SINGLE_LIST mechanism \
        similar to that used in TrasferData handling should be used;
        */

//                if(vboxNetFltWinIsLoopedBackPacket(pAdapt, pPacket))
# else
                if(vboxNetFltWinIsLoopedBackPacket(pPacket) || cbHeaderBuffer != ETH_HEADER_SIZE)
# endif

                {
//                    Assert(0);
                    /* nothing else to do here, just return the packet */
//                    NdisReturnPackets(&pPacket, 1);
//                    break;
                }

                fFlags = MACS_EQUAL(((PRTNETETHERHDR)pHeaderBuffer)->SrcMac, pNetFltIf->u.s.Mac) ?
                        PACKET_COPY | PACKET_SRC_HOST : PACKET_COPY;
                Status = vboxNetFltWinQuEnqueuePacket(pNetFltIf, pPacket, fFlags);
                if(Status == NDIS_STATUS_SUCCESS)
                {
                    NdisReturnPackets(&pPacket, 1);
                    pAdapt = NULL;
                    pNetFltIf = NULL;
                    break;
                }
            }
#endif
            Status = vboxNetFltWinPtReceiveActive(pAdapt, MacReceiveContext, pHeaderBuffer, cbHeaderBuffer,
                    pLookAheadBuffer, cbLookAheadBuffer, cbPacket);
            if(NT_SUCCESS(Status))
            {
                if(Status != NDIS_STATUS_NOT_ACCEPTED)
                {
                    pAdapt = NULL;
                    pNetFltIf = NULL;
                }
                else
                {
                    /* this is a looopback packet, nothing to do here */
                }
                break;
            }
        } while(0);

        if(pNetFltIf)
            vboxNetFltWinDereferenceNetFlt(pNetFltIf);
        if(pAdapt)
            vboxNetFltWinDereferenceAdapt(pAdapt);


#if 0
        if(pPacket)
        {
            NdisReturnPackets(&pPacket, 1);
        }
#endif
        /* we are here because the vboxNetFltWinPtReceiveActive returned pending,
         * which means our ProtocolDataTransferComplete we will called,
         * so return SUCCESS instead of NOT_ACCEPTED ?? */
//        return NDIS_STATUS_SUCCESS;
    }
    return NDIS_STATUS_NOT_ACCEPTED;
#else /* if NOT defined VBOX_NETFLT_ONDEMAND_BIND */
    PNDIS_PACKET      pPacket = NULL;
    bool bNetFltActive;
    bool fAdaptActive = vboxNetFltWinReferenceAdaptNetFlt(pNetFlt, pAdapt, &bNetFltActive);
    const bool bPassThruActive = !bNetFltActive;
    if(fAdaptActive)
    {
        do
        {
#ifndef DEBUG_NETFLT_RECV_NOPACKET
            /*
             * Get at the packet, if any, indicated up by the miniport below.
             */
            pPacket = NdisGetReceivedPacket(pAdapt->hBindingHandle, MacReceiveContext);
            if (pPacket != NULL)
            {
#ifndef VBOX_LOOPBACK_USEFLAGS
                PNDIS_PACKET pLb = NULL;
#endif
                do
                {
#ifdef VBOX_LOOPBACK_USEFLAGS
                    if(vboxNetFltWinIsLoopedBackPacket(pPacket))
                    {
                        Assert(0);
                        /* nothing else to do here, just return the packet */
                        //NdisReturnPackets(&pPacket, 1);
                        Status = NDIS_STATUS_NOT_ACCEPTED;
                        break;
                    }

                    VBOXNETFLT_LBVERIFY(pNetFlt, pPacket);
#endif
                    if(bNetFltActive)
                    {
#ifndef VBOX_LOOPBACK_USEFLAGS
                        pLb = vboxNetFltWinLbSearchLoopBack(pAdapt, pPacket, true /* ??? no need to keep it, so remove */);
                        if(!pLb)
#endif
                        {
                            VBOXNETFLT_LBVERIFY(pNetFlt, pPacket);

                            Status = vboxNetFltWinQuEnqueuePacket(pNetFlt, pPacket, PACKET_COPY);
                            Assert(Status == NDIS_STATUS_SUCCESS);
                            if(Status == NDIS_STATUS_SUCCESS)
                            {
                                //NdisReturnPackets(&pPacket, 1);
                                fAdaptActive = false;
                                bNetFltActive = false;
                                break;
                            }
                        }
#ifndef VBOX_LOOPBACK_USEFLAGS
                        else if(vboxNetFltWinLbIsFromIntNet(pLb))
                        {
                            /* nothing else to do here, just return the packet */
                            //NdisReturnPackets(&pPacket, 1);
                            Status = NDIS_STATUS_NOT_ACCEPTED;
                            break;
                        }
                        /* we are here because this is a looped back packet set not from intnet
                         * we will post it to the upper protocol */
#endif
                    }

#ifndef VBOX_LOOPBACK_USEFLAGS
                    Assert(!pLb || !vboxNetFltWinLbIsFromIntNet(pLb));
#endif
                    Status = vboxNetFltWinRecvPassThru(pAdapt, pPacket);
                    /* we are done with packet processing, and we will
                     * not receive packet return event for this packet,
                     * fAdaptActive should be true to ensure we release adapt*/
                    Assert(fAdaptActive);
                } while(FALSE);

                if(Status == NDIS_STATUS_SUCCESS || Status == NDIS_STATUS_NOT_ACCEPTED
#ifndef VBOX_LOOPBACK_USEFLAGS
                        || pLb
#endif
                        )
                {
                    break;
                }
            }
#endif
            if(bNetFltActive)
            {
                Status = vboxNetFltWinPtReceiveActive(pAdapt, MacReceiveContext, pHeaderBuffer, cbHeaderBuffer,
                        pLookAheadBuffer, cbLookAheadBuffer, cbPacket);
                if(NT_SUCCESS(Status))
                {
                    if(Status != NDIS_STATUS_NOT_ACCEPTED)
                    {
                        fAdaptActive = false;
                        bNetFltActive = false;
                    }
                    else
                    {
#ifndef VBOX_LOOPBACK_USEFLAGS
                        /* this is a loopback packet, nothing to do here */
#else
                        Assert(0);
                        /* should not be here */
#endif
                    }
                    break;
                }
            }

            /* Fall through if the miniport below us has either not
             * indicated a packet or we could not allocate one */
            if(pPacket != NULL)
            {
                /*
                 * We are here because we failed to allocate packet
                 */
                vboxNetFltWinPtFlushReceiveQueue(pAdapt, false);
            }

            /* we are done with packet processing, and we will
             * not receive packet return event for this packet,
             * fAdaptActive should be true to ensure we release adapt*/
            Assert(fAdaptActive);

            pAdapt->bIndicateRcvComplete = TRUE;
            switch (pAdapt->Medium)
            {
                case NdisMedium802_3:
                case NdisMediumWan:
                    NdisMEthIndicateReceive(pAdapt->hMiniportHandle,
                                                 MacReceiveContext,
                                                 (PCHAR)pHeaderBuffer,
                                                 cbHeaderBuffer,
                                                 pLookAheadBuffer,
                                                 cbLookAheadBuffer,
                                                 cbPacket);
                    break;
                default:
                    Assert(FALSE);
                    break;
            }
        } while(0);

        if(bNetFltActive)
        {
            vboxNetFltWinDereferenceNetFlt(pNetFlt);
        }
        else if(bPassThruActive)
        {
            vboxNetFltWinDereferenceModePassThru(pNetFlt);
        }
        if(fAdaptActive)
        {
            vboxNetFltWinDereferenceAdapt(pAdapt);
        }
    }
    else
    {
        Status = NDIS_STATUS_FAILURE;
    }

    return Status;
#endif
}

/**
 * Called by the adapter below us when it is done indicating a batch of
 * received packets.
 *
 * @param ProtocolBindingContext    Pointer to our adapter structure.
 * @return None */
static VOID
vboxNetFltWinPtReceiveComplete(
    IN NDIS_HANDLE        ProtocolBindingContext
    )
{
#ifndef VBOX_NETFLT_ONDEMAND_BIND
    PADAPT        pAdapt =(PADAPT)ProtocolBindingContext;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    ULONG         NumberOfPackets = 0;
    /* since the receive array queued packets do not hold the reference we need to
     * reference  the PassThru/NetFlt mode here to avoid packet reordering caused by
     * concurrently running vboxNetFltWinPtReceiveComplete and vboxNetFltPortOsSetActive
     * on netflt activation/deactivation */
    bool bNetFltActive;
    bool fAdaptActive = vboxNetFltWinReferenceAdaptNetFlt(pNetFlt, pAdapt, &bNetFltActive);

    vboxNetFltWinPtFlushReceiveQueue(pAdapt, false);

    if ((pAdapt->hMiniportHandle != NULL)
               /* && (pAdapt->MPDeviceState == NdisDeviceStateD0) */
                && (pAdapt->bIndicateRcvComplete == TRUE))
    {
        switch (pAdapt->Medium)
        {
            case NdisMedium802_3:
            case NdisMediumWan:
                NdisMEthIndicateReceiveComplete(pAdapt->hMiniportHandle);
                break;
            default:
                Assert(FALSE);
                break;
        }
    }

    pAdapt->bIndicateRcvComplete = FALSE;

    if(fAdaptActive)
    {
        if(bNetFltActive)
        {
            vboxNetFltWinDereferenceNetFlt(pNetFlt);
        }
        else
        {
            vboxNetFltWinDereferenceModePassThru(pNetFlt);
        }
        vboxNetFltWinDereferenceAdapt(pAdapt);
    }
#endif
}

/**
 * ReceivePacket handler. Called by NDIS if the miniport below supports
 * NDIS 4.0 style receives. Re-package the buffer chain in a new packet
 * and indicate the new packet to protocols above us. Any context for
 * packets indicated up must be kept in the MiniportReserved field.
 *
 * @param ProtocolBindingContext - Pointer to our adapter structure.
 * @param Packet - Pointer to the packet
 * @return == 0 -> We are done with the packet,
 *  != 0 -> We will keep the packet and call NdisReturnPackets() this many times when done.
 */
static INT
vboxNetFltWinPtReceivePacket(
    IN NDIS_HANDLE            ProtocolBindingContext,
    IN PNDIS_PACKET           pPacket
    )
{
    PADAPT              pAdapt =(PADAPT)ProtocolBindingContext;
    INT         cRefCount = 0;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
#ifdef VBOX_NETFLT_ONDEMAND_BIND
    PNDIS_BUFFER pBuffer;
    PVOID pVA;
    UINT cbLength;
    uint32_t fFlags;

    pNetFltIf = vboxNetFltWinReferenceAdaptNetFltFromAdapt(pAdapt);

    if(pNetFltIf)
    {
        NDIS_STATUS Status;
        bool bResources;
        do
        {
#ifdef DEBUG_NETFLT_LOOPBACK
# error "implement (see comments in the sources below this #error:)"
        /* @todo FIXME no need for the PPACKET_INFO mechanism here;
        instead the the NDIS_PACKET.ProtocolReserved + INTERLOCKED_SINGLE_LIST mechanism \
        similar to that used in TrasferData handling should be used;
        */

//            if(vboxNetFltWinIsLoopedBackPacket(pAdapt, pPacket))
#else
            if(vboxNetFltWinIsLoopedBackPacket(pPacket))
#endif

            {
                Assert(0);
                NdisReturnPackets(&pPacket, 1);
                break;
            }
            bResources = NDIS_GET_PACKET_STATUS(pPacket) == NDIS_STATUS_RESOURCES;

            NdisQueryPacket(pPacket, NULL, NULL, &pBuffer, NULL);
            if(!pBuffer)
            {
                Assert(0);
                NdisReturnPackets(&pPacket, 1);
                cRefCount = 0;
                break;
            }

            NdisQueryBufferSafe(pBuffer, &pVA, &cbLength, NormalPagePriority);
            if(!pVA || !cbLength)
            {
                Assert(0);
                NdisReturnPackets(&pPacket, 1);
                cRefCount = 0;
                break;
            }

            fFlags = MACS_EQUAL(((PRTNETETHERHDR)pVA)->SrcMac, pNetFltIf->u.s.Mac) ? PACKET_SRC_HOST : 0;

            Status = vboxNetFltWinQuEnqueuePacket(pNetFltIf, pPacket, bResources ? fFlags | PACKET_COPY : fFlags);
            if(Status == NDIS_STATUS_SUCCESS)
            {
                if(bResources)
                {
                    cRefCount = 0;
                    NdisReturnPackets(&pPacket, 1);
                }
                else
                {
                    cRefCount = 1;
                }
                pNetFltIf = NULL;
                pAdapt = NULL;
                break;
            }
            else
            {
                Assert(0);
                NdisReturnPackets(&pPacket, 1);
                cRefCount = 0;
                break;
            }
        } while (0);

        if(pNetFltIf)
            vboxNetFltWinDereferenceNetFlt(pNetFltIf);
        if(pAdapt)
            vboxNetFltWinDereferenceAdapt(pAdapt);
        return cRefCount;
    }
    /* we are here because we are inactive, simply return the packet */
    NdisReturnPackets(&pPacket, 1);
    return 0;
#else
    bool bNetFltActive;
    bool fAdaptActive = vboxNetFltWinReferenceAdaptNetFlt(pNetFlt, pAdapt, &bNetFltActive);
    const bool bPassThruActive = !bNetFltActive;
    if(fAdaptActive)
    {
        do
        {
#ifdef VBOX_LOOPBACK_USEFLAGS
            if(vboxNetFltWinIsLoopedBackPacket(pPacket))
            {
                Assert(0);
                Log(("lb_rp"));

                /* nothing else to do here, just return the packet */
                cRefCount = 0;
                //NdisReturnPackets(&pPacket, 1);
                break;
            }

            VBOXNETFLT_LBVERIFY(pNetFlt, pPacket);
#endif

            if(bNetFltActive)
            {
#ifndef VBOX_LOOPBACK_USEFLAGS
                PNDIS_PACKET pLb = vboxNetFltWinLbSearchLoopBack(pAdapt, pPacket, true /* ??? no need to keep it, so remove */);
                if(!pLb)
#endif
                {
                    NDIS_STATUS fStatus;
                    bool bResources = NDIS_GET_PACKET_STATUS(pPacket) == NDIS_STATUS_RESOURCES;

                    VBOXNETFLT_LBVERIFY(pNetFlt, pPacket);

                    /*TODO: remove this assert.
                     * this is a temporary assert for debugging purposes:
                     * we're probably doing something wrong with the packets if the miniport reports NDIS_STATUS_RESOURCES */
                    Assert(!bResources);

                    fStatus = vboxNetFltWinQuEnqueuePacket(pNetFlt, pPacket, bResources ? PACKET_COPY : 0);
                    if(fStatus == NDIS_STATUS_SUCCESS)
                    {
                        bNetFltActive = false;
                        fAdaptActive = false;
                        if(bResources)
                        {
                            cRefCount = 0;
                            //NdisReturnPackets(&pPacket, 1);
                        }
                        else
                        {
                            cRefCount = 1;
                        }
                        break;
                    }
                    else
                    {
                        Assert(0);
                    }
                }
#ifndef VBOX_LOOPBACK_USEFLAGS
                else if(vboxNetFltWinLbIsFromIntNet(pLb))
                {
                    /* the packet is from intnet, it has already been set to the host,
                     * no need for loopng it back to the host again */
                    /* nothing else to do here, just return the packet */
                    cRefCount = 0;
                    //NdisReturnPackets(&pPacket, 1);
                    break;
                }
#endif
            }

            cRefCount = vboxNetFltWinRecvPacketPassThru(pAdapt, pPacket);
            if(cRefCount)
            {
                Assert(cRefCount == 1);
                fAdaptActive = false;
            }

        } while(FALSE);

        if(bNetFltActive)
        {
            vboxNetFltWinDereferenceNetFlt(pNetFlt);
        }
        else if(bPassThruActive)
        {
            vboxNetFltWinDereferenceModePassThru(pNetFlt);
        }
        if(fAdaptActive)
        {
            vboxNetFltWinDereferenceAdapt(pAdapt);
        }
    }
    else
    {
        cRefCount = 0;
        //NdisReturnPackets(&pPacket, 1);
    }

    return cRefCount;
#endif
}

/**
 * This routine is called from NDIS to notify our protocol edge of a
 * reconfiguration of parameters for either a specific binding (pAdapt
 * is not NULL), or global parameters if any (pAdapt is NULL).
 *
 * @param pAdapt - Pointer to our adapter structure.
 * @param pNetPnPEvent - the reconfigure event
 * @return NDIS_STATUS_SUCCESS */
static NDIS_STATUS
vboxNetFltWinPtPnPNetEventReconfigure(
    IN PADAPT            pAdapt,
    IN PNET_PNP_EVENT    pNetPnPEvent
    )
{
    NDIS_STATUS    ReconfigStatus = NDIS_STATUS_SUCCESS;
    NDIS_STATUS    ReturnStatus = NDIS_STATUS_SUCCESS;

    do
    {
        /*
         * Is this is a global reconfiguration notification ?
         */
        if (pAdapt == NULL)
        {
            /*
             * An important event that causes this notification to us is if
             * one of our upper-edge miniport instances was enabled after being
             * disabled earlier, e.g. from Device Manager in Win2000. Note that
             * NDIS calls this because we had set up an association between our
             * miniport and protocol entities by calling NdisIMAssociateMiniport.
             *
             * Since we would have torn down the lower binding for that miniport,
             * we need NDIS' assistance to re-bind to the lower miniport. The
             * call to NdisReEnumerateProtocolBindings does exactly that.
             */
            NdisReEnumerateProtocolBindings (g_hProtHandle);
            break;
        }

        ReconfigStatus = NDIS_STATUS_SUCCESS;

    } while(FALSE);

    LogFlow(("<==PtPNPNetEventReconfigure: pAdapt %p\n", pAdapt));

    return ReconfigStatus;
}

static NDIS_STATUS
vboxNetFltWinPtPnPNetEventBindsComplete(
    IN PADAPT            pAdapt,
    IN PNET_PNP_EVENT    pNetPnPEvent
    )
{
    return NDIS_STATUS_SUCCESS;
}

DECLHIDDEN(bool) vboxNetFltWinPtCloseAdapter(PADAPT pAdapt, PNDIS_STATUS pStatus)
{
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;

    RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);

    if(pAdapt->bClosingAdapter)
    {
        RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
        Assert(0);
        return false;
    }
    if (pAdapt->hBindingHandle == NULL)
    {
        RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
        Assert(0);
        return false;
    }

    pAdapt->bClosingAdapter = true;
    RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);

    /*
     * Close the binding below. and wait for it to complete
     */
    NdisResetEvent(&pAdapt->hEvent);

    NdisCloseAdapter(pStatus, pAdapt->hBindingHandle);

    if (*pStatus == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pAdapt->hEvent, 0);
        *pStatus = pAdapt->Status;
    }

    Assert (*pStatus == NDIS_STATUS_SUCCESS);

    pAdapt->hBindingHandle = NULL;

    return true;
}

/**
 * This is a notification to our protocol edge of the power state
 * of the lower miniport. If it is going to a low-power state, we must
 * wait here for all outstanding sends and requests to complete.
 *
 * @param pAdapt            -    Pointer to the adpater structure
 * @param pNetPnPEvent    -    The Net Pnp Event. this contains the new device state
 * @return  NDIS_STATUS_SUCCESS or the status returned by upper-layer protocols.
 * */
static NDIS_STATUS
vboxNetFltWinPtPnPNetEventSetPower(
    IN PADAPT            pAdapt,
    IN PNET_PNP_EVENT    pNetPnPEvent
    )
{
    PNDIS_DEVICE_POWER_STATE       pDeviceState  =(PNDIS_DEVICE_POWER_STATE)(pNetPnPEvent->Buffer);
    NDIS_DEVICE_POWER_STATE        PrevDeviceState = vboxNetFltWinGetPowerState(&pAdapt->PTState);
    NDIS_STATUS                    ReturnStatus;
    PVBOXNETFLTINS pNetFlt = PADAPT_2_PVBOXNETFLTINS(pAdapt);
    RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
    int cPPUsage;

    ReturnStatus = NDIS_STATUS_SUCCESS;

    /*
     * Set the Internal Device State, this blocks all new sends or receives
     */
    RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);

    vboxNetFltWinSetPowerState(&pAdapt->PTState, *pDeviceState);

    /*
     * Check if the miniport below is going to a low power state.
     */
    if (vboxNetFltWinGetPowerState(&pAdapt->PTState) > NdisDeviceStateD0)
    {
        /*
         * If the miniport below is going to standby, fail all incoming requests
         */
        if (PrevDeviceState == NdisDeviceStateD0)
        {
            pAdapt->bStandingBy = TRUE;
        }
        RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
#ifndef VBOX_NETFLT_ONDEMAND_BIND

        vboxNetFltWinPtFlushReceiveQueue(pAdapt, false);

        vboxNetFltWinWaitDereference(&pAdapt->MPState);
#endif

        /*
         * Wait for outstanding sends and requests to complete.
         */
        vboxNetFltWinWaitDereference(&pAdapt->PTState);

#ifndef VBOX_NETFLT_ONDEMAND_BIND
        while (ASMAtomicUoReadBool((volatile bool *)&pAdapt->bOutstandingRequests))
        {
            /*
             * sleep till outstanding requests complete
             */
            vboxNetFltWinSleep(2);
        }

        /*
         * If the below miniport is going to low power state, complete the queued request
         */
        RTSpinlockAcquire(pNetFlt->hSpinlock, &Tmp);
        if (pAdapt->bQueuedRequest)
        {
            pAdapt->bQueuedRequest = FALSE;
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
            vboxNetFltWinPtRequestComplete(pAdapt, &pAdapt->Request, NDIS_STATUS_FAILURE);
        }
        else
        {
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
        }
#endif

        /* check packet pool is empty */
        cPPUsage = NdisPacketPoolUsage(pAdapt->hSendPacketPoolHandle);
        Assert(cPPUsage == 0);
        cPPUsage = NdisPacketPoolUsage(pAdapt->hRecvPacketPoolHandle);
        Assert(cPPUsage == 0);
        /* for debugging only, ignore the err in release */
        NOREF(cPPUsage);

#ifndef VBOX_NETFLT_ONDEMAND_BIND
        Assert(pAdapt->bOutstandingRequests == FALSE);
#endif
    }
    else
    {
        /*
         * If the physical miniport is powering up (from Low power state to D0),
         * clear the flag
         */
        if (PrevDeviceState > NdisDeviceStateD0)
        {
            pAdapt->bStandingBy = FALSE;
        }

#ifdef VBOX_NETFLT_ONDEMAND_BIND
        RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
#else
        /*
         * The device below is being turned on. If we had a request
         * pending, send it down now.
         */
        if (pAdapt->bQueuedRequest == TRUE)
        {
            NDIS_STATUS Status;

            pAdapt->bQueuedRequest = FALSE;

            pAdapt->bOutstandingRequests = TRUE;
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);

            NdisRequest(&Status,
                        pAdapt->hBindingHandle,
                        &pAdapt->Request);

            if (Status != NDIS_STATUS_PENDING)
            {
                vboxNetFltWinPtRequestComplete(pAdapt,
                                  &pAdapt->Request,
                                  Status);

            }
        }
        else
        {
            RTSpinlockRelease(pNetFlt->hSpinlock, &Tmp);
        }

#endif /* #ifndef VBOX_NETFLT_ONDEMAND_BIND */

    }

    return ReturnStatus;
}

/**
 * This is called by NDIS to notify us of a PNP event related to a lower
 * binding. Based on the event, this dispatches to other helper routines.
 *
 * @param ProtocolBindingContext - Pointer to our adapter structure. Can be NULL
 *              for "global" notifications
 * @param pNetPnPEvent - Pointer to the PNP event to be processed.
 * @return NDIS_STATUS code indicating status of event processing.
 * */
static NDIS_STATUS
vboxNetFltWinPtPnPHandler(
    IN NDIS_HANDLE        ProtocolBindingContext,
    IN PNET_PNP_EVENT     pNetPnPEvent
    )
{
    PADAPT            pAdapt  =(PADAPT)ProtocolBindingContext;
    NDIS_STATUS       Status  = NDIS_STATUS_SUCCESS;

    LogFlow(("vboxNetFltWinPtPnPHandler: Adapt %p, Event %d\n", pAdapt, pNetPnPEvent->NetEvent));

    switch (pNetPnPEvent->NetEvent)
    {
        case NetEventSetPower:
            Status = vboxNetFltWinPtPnPNetEventSetPower(pAdapt, pNetPnPEvent);
            break;

         case NetEventReconfigure:
             DBGPRINT(("NetEventReconfigure, pAdapt(%p)", pAdapt));
            Status = vboxNetFltWinPtPnPNetEventReconfigure(pAdapt, pNetPnPEvent);
            break;
         case NetEventBindsComplete:
             DBGPRINT(("NetEventBindsComplete"));
             Status = vboxNetFltWinPtPnPNetEventBindsComplete(pAdapt, pNetPnPEvent);
            break;
         default:
            Status = NDIS_STATUS_SUCCESS;
            break;
    }

    return Status;
}
#ifdef __cplusplus
# define PTCHARS_40(_p) ((_p).Ndis40Chars)
#else
# define PTCHARS_40(_p) (_p)
#endif

/**
 * register the protocol edge
 */
DECLHIDDEN(NDIS_STATUS)
vboxNetFltWinPtRegister(
    IN PDRIVER_OBJECT        DriverObject,
    IN PUNICODE_STRING       RegistryPath
    )
{
    NDIS_STATUS Status;
    NDIS_PROTOCOL_CHARACTERISTICS      PChars;
    NDIS_STRING                        Name;

    /*
     * Now register the protocol.
     */
    NdisZeroMemory(&PChars, sizeof(NDIS_PROTOCOL_CHARACTERISTICS));
    PTCHARS_40(PChars).MajorNdisVersion = VBOXNETFLT_PROT_MAJOR_NDIS_VERSION;
    PTCHARS_40(PChars).MinorNdisVersion = VBOXNETFLT_PROT_MINOR_NDIS_VERSION;

    /*
     * Make sure the protocol-name matches the service-name
     * (from the INF) under which this protocol is installed.
     * This is needed to ensure that NDIS can correctly determine
     * the binding and call us to bind to miniports below.
     */
    NdisInitUnicodeString(&Name, VBOXNETFLT_PROTOCOL_NAME);    /* Protocol name */
    PTCHARS_40(PChars).Name = Name;
    PTCHARS_40(PChars).OpenAdapterCompleteHandler = vboxNetFltWinPtOpenAdapterComplete;
    PTCHARS_40(PChars).CloseAdapterCompleteHandler = vboxNetFltWinPtCloseAdapterComplete;
    PTCHARS_40(PChars).SendCompleteHandler = vboxNetFltWinPtSendComplete;
    PTCHARS_40(PChars).TransferDataCompleteHandler = vboxNetFltWinPtTransferDataComplete;

    PTCHARS_40(PChars).ResetCompleteHandler = vboxNetFltWinPtResetComplete;
    PTCHARS_40(PChars).RequestCompleteHandler = vboxNetFltWinPtRequestComplete;
    PTCHARS_40(PChars).ReceiveHandler = vboxNetFltWinPtReceive;
    PTCHARS_40(PChars).ReceiveCompleteHandler = vboxNetFltWinPtReceiveComplete;
    PTCHARS_40(PChars).StatusHandler = vboxNetFltWinPtStatus;
    PTCHARS_40(PChars).StatusCompleteHandler = vboxNetFltWinPtStatusComplete;
    PTCHARS_40(PChars).BindAdapterHandler = vboxNetFltWinPtBindAdapter;
    PTCHARS_40(PChars).UnbindAdapterHandler = vboxNetFltWinPtUnbindAdapter;
    PTCHARS_40(PChars).UnloadHandler = vboxNetFltWinPtUnloadProtocol;
#if !defined(VBOX_NETFLT_ONDEMAND_BIND) && !defined(DEBUG_NETFLT_RECV)
    PTCHARS_40(PChars).ReceivePacketHandler = vboxNetFltWinPtReceivePacket;
#else
    PTCHARS_40(PChars).ReceivePacketHandler = NULL;
#endif
    PTCHARS_40(PChars).PnPEventHandler= vboxNetFltWinPtPnPHandler;

    NdisRegisterProtocol(&Status,
                         &g_hProtHandle,
                         &PChars,
                         sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

    return Status;
}

/**
 * deregister the protocol edge
 */
DECLHIDDEN(NDIS_STATUS)
vboxNetFltWinPtDeregister()
{
    NDIS_STATUS Status;

    if (g_hProtHandle != NULL)
    {
        NdisDeregisterProtocol(&Status, g_hProtHandle);
        g_hProtHandle = NULL;
    }

    return Status;
}

#ifndef VBOX_NETFLT_ONDEMAND_BIND
/**
 * returns the protocol handle
 */
DECLHIDDEN(NDIS_HANDLE) vboxNetFltWinPtGetHandle()
{
    return g_hProtHandle;
}
#endif
