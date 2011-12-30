/* $Id$ */
/** @file
 * VBoxNetDHCP - DHCP Service for connecting to IntNet.
 */
/** @todo r=bird: Cut&Past rules... Please fix DHCP refs! */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_NET_SERVICE

#include <iprt/alloca.h>
#include <iprt/buildconfig.h>
#include <iprt/err.h>
#include <iprt/net.h>                   /* must come before getopt.h. */
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/mem.h>

#include <VBox/sup.h>
#include <VBox/intnet.h>
#include <VBox/vmm/vmm.h>
#include <VBox/version.h>

#include <vector>
#include <string>

#include <VBox/log.h>

#include "VBoxNetLib.h"
#include "VBoxNetBaseService.h"

#ifdef RT_OS_WINDOWS /* WinMain */
# include <Windows.h>
# include <stdlib.h>
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
static RTGETOPTDEF g_aGetOptDef[] =
{
    { "--name",           'N',   RTGETOPT_REQ_STRING },
    { "--network",        'n',   RTGETOPT_REQ_STRING },
    { "--trunk-name",     't',   RTGETOPT_REQ_STRING },
    { "--trunk-type",     'T',   RTGETOPT_REQ_STRING },
    { "--mac-address",    'a',   RTGETOPT_REQ_MACADDR },
    { "--ip-address",     'i',   RTGETOPT_REQ_IPV4ADDR },
    { "--verbose",        'v',   RTGETOPT_REQ_NOTHING },
};
VBoxNetBaseService::VBoxNetBaseService()
{
}
VBoxNetBaseService::~VBoxNetBaseService()
{
    /*
     * Close the interface connection.
     */
    if (m_hIf != INTNET_HANDLE_INVALID)
    {
        INTNETIFCLOSEREQ CloseReq;
        CloseReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
        CloseReq.Hdr.cbReq = sizeof(CloseReq);
        CloseReq.pSession = m_pSession;
        CloseReq.hIf = m_hIf;
        m_hIf = INTNET_HANDLE_INVALID;
        int rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_RTCPUID, VMMR0_DO_INTNET_IF_CLOSE, 0, &CloseReq.Hdr);
        AssertRC(rc);
    }

    if (m_pSession)
    {
        SUPR3Term(false /*fForced*/);
        m_pSession = NIL_RTR0PTR;
    }
}

int VBoxNetBaseService::init()
{
    /* numbers from DrvIntNet */
    m_cbSendBuf             =  36 * _1K;
    m_cbRecvBuf             = 218 * _1K;
    m_hIf                   = INTNET_HANDLE_INVALID;
    m_pIfBuf                = NULL;

    m_cVerbosity            = 0;
    m_Name                  = "VBoxNetNAT";
    m_Network               = "intnet";
    for(unsigned int i = 0; i < RT_ELEMENTS(g_aGetOptDef); ++i)
        m_vecOptionDefs.push_back(&g_aGetOptDef[i]);
    return VINF_SUCCESS;
}
/**
 * Parse the arguments.
 *
 * @returns 0 on success, fully bitched exit code on failure.
 *
 * @param   argc    Argument count.
 * @param   argv    Argument vector.
 */
int VBoxNetBaseService::parseArgs(int argc, char **argv)
{

    RTGETOPTSTATE State;
    PRTGETOPTDEF paOptionArray = getOptionsPtr();
    int rc = RTGetOptInit(&State, argc, argv, paOptionArray, m_vecOptionDefs.size(), 0, 0 /*fFlags*/);
    AssertRCReturn(rc, 49);
    Log2(("BaseService: parseArgs enter\n"));

    for (;;)
    {
        RTGETOPTUNION Val;
        rc = RTGetOpt(&State, &Val);
        if (!rc)
            break;
        switch (rc)
        {
            case 'N':
                m_Name = Val.psz;
                break;
            case 'n':
                m_Network = Val.psz;
                break;
            case 't':
                m_TrunkName = Val.psz;
                break;
            case 'T':
                if (!strcmp(Val.psz, "none"))
                    m_enmTrunkType = kIntNetTrunkType_None;
                else if (!strcmp(Val.psz, "whatever"))
                    m_enmTrunkType = kIntNetTrunkType_WhateverNone;
                else if (!strcmp(Val.psz, "netflt"))
                    m_enmTrunkType = kIntNetTrunkType_NetFlt;
                else if (!strcmp(Val.psz, "netadp"))
                    m_enmTrunkType = kIntNetTrunkType_NetAdp;
                else if (!strcmp(Val.psz, "srvnat"))
                    m_enmTrunkType = kIntNetTrunkType_SrvNat;
                else
                {
                    RTStrmPrintf(g_pStdErr, "Invalid trunk type '%s'\n", Val.psz);
                    return 1;
                }
                break;
            case 'a':
                m_MacAddress = Val.MacAddr;
                break;
            case 'i':
                m_Ipv4Address = Val.IPv4Addr;
                break;

            case 'v':
                m_cVerbosity++;
                break;

            case 'V':
                RTPrintf("%sr%u\n", RTBldCfgVersion(), RTBldCfgRevision());
                return 1;

            case 'h':
                RTPrintf("VBoxNetDHCP Version %s\n"
                         "(C) 2009-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
                         "All rights reserved.\n"
                         "\n"
                         "Usage: VBoxNetDHCP <options>\n"
                         "\n"
                         "Options:\n",
                         RTBldCfgVersion());
                for (unsigned int i = 0; i < m_vecOptionDefs.size(); i++)
                    RTPrintf("    -%c, %s\n", m_vecOptionDefs[i]->iShort, m_vecOptionDefs[i]->pszLong);
                usage(); /* to print Service Specific usage */
                return 1;

            default:
                int rc1 = parseOpt(rc, Val);
                if (RT_FAILURE(rc1))
                {
                    rc = RTGetOptPrintError(rc, &Val);
                    RTPrintf("Use --help for more information.\n");
                    return rc;
                }
        }
    }

    RTMemFree(paOptionArray);
    return rc;
}

int VBoxNetBaseService::tryGoOnline(void)
{
    /*
     * Open the session, load ring-0 and issue the request.
     */
    int rc = SUPR3Init(&m_pSession);
    if (RT_FAILURE(rc))
    {
        m_pSession = NIL_RTR0PTR;
        LogRel(("VBoxNetBaseService: SUPR3Init -> %Rrc\n", rc));
        return 1;
    }

    char szPath[RTPATH_MAX];
    rc = RTPathExecDir(szPath, sizeof(szPath) - sizeof("/VMMR0.r0"));
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxNetBaseService: RTPathExecDir -> %Rrc\n", rc));
        return 1;
    }

    rc = SUPR3LoadVMM(strcat(szPath, "/VMMR0.r0"));
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxNetBaseService: SUPR3LoadVMM(\"%s\") -> %Rrc\n", szPath, rc));
        return 1;
    }

    /*
     * Create the open request.
     */
    PINTNETBUF pBuf;
    INTNETOPENREQ OpenReq;
    OpenReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    OpenReq.Hdr.cbReq = sizeof(OpenReq);
    OpenReq.pSession = m_pSession;
    strncpy(OpenReq.szNetwork, m_Network.c_str(), sizeof(OpenReq.szNetwork));
    OpenReq.szNetwork[sizeof(OpenReq.szNetwork) - 1] = '\0';
    strncpy(OpenReq.szTrunk, m_TrunkName.c_str(), sizeof(OpenReq.szTrunk));
    OpenReq.szTrunk[sizeof(OpenReq.szTrunk) - 1] = '\0';
    OpenReq.enmTrunkType = m_enmTrunkType;
    OpenReq.fFlags = 0; /** @todo check this */
    OpenReq.cbSend = m_cbSendBuf;
    OpenReq.cbRecv = m_cbRecvBuf;
    OpenReq.hIf = INTNET_HANDLE_INVALID;

    /*
     * Issue the request.
     */
    Log2(("attempting to open/create network \"%s\"...\n", OpenReq.szNetwork));
    rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_OPEN, 0, &OpenReq.Hdr);
    if (RT_FAILURE(rc))
    {
        Log2(("VBoxNetBaseService: SUPR3CallVMMR0Ex(,VMMR0_DO_INTNET_OPEN,) failed, rc=%Rrc\n", rc));
        goto bad;
    }
    m_hIf = OpenReq.hIf;
    Log2(("successfully opened/created \"%s\" - hIf=%#x\n", OpenReq.szNetwork, m_hIf));

    /*
     * Get the ring-3 address of the shared interface buffer.
     */
    INTNETIFGETBUFFERPTRSREQ GetBufferPtrsReq;
    GetBufferPtrsReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    GetBufferPtrsReq.Hdr.cbReq = sizeof(GetBufferPtrsReq);
    GetBufferPtrsReq.pSession = m_pSession;
    GetBufferPtrsReq.hIf = m_hIf;
    GetBufferPtrsReq.pRing3Buf = NULL;
    GetBufferPtrsReq.pRing0Buf = NIL_RTR0PTR;
    rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_GET_BUFFER_PTRS, 0, &GetBufferPtrsReq.Hdr);
    if (RT_FAILURE(rc))
    {
        Log2(("VBoxNetBaseService: SUPR3CallVMMR0Ex(,VMMR0_DO_INTNET_IF_GET_BUFFER_PTRS,) failed, rc=%Rrc\n", rc));
        goto bad;
    }
    pBuf = GetBufferPtrsReq.pRing3Buf;
    Log2(("pBuf=%p cbBuf=%d cbSend=%d cbRecv=%d\n",
               pBuf, pBuf->cbBuf, pBuf->cbSend, pBuf->cbRecv));
    m_pIfBuf = pBuf;

    /*
     * Activate the interface.
     */
    INTNETIFSETACTIVEREQ ActiveReq;
    ActiveReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    ActiveReq.Hdr.cbReq = sizeof(ActiveReq);
    ActiveReq.pSession = m_pSession;
    ActiveReq.hIf = m_hIf;
    ActiveReq.fActive = true;
    rc = SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_SET_ACTIVE, 0, &ActiveReq.Hdr);
    if (RT_SUCCESS(rc))
        return 0;

    /* bail out */
    Log2(("VBoxNetBaseService: SUPR3CallVMMR0Ex(,VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE,) failed, rc=%Rrc\n", rc));

    return 0;
    bad:
        return 1;
}

void VBoxNetBaseService::shutdown(void)
{
}

/**
 * Print debug message depending on the m_cVerbosity level.
 *
 * @param   iMinLevel       The minimum m_cVerbosity level for this message.
 * @param   fMsg            Whether to dump parts for the current DHCP message.
 * @param   pszFmt          The message format string.
 * @param   ...             Optional arguments.
 */
inline void VBoxNetBaseService::debugPrint(int32_t iMinLevel, bool fMsg, const char *pszFmt, ...) const
{
    if (iMinLevel <= m_cVerbosity)
    {
        va_list va;
        va_start(va, pszFmt);
        debugPrintV(iMinLevel, fMsg, pszFmt, va);
        va_end(va);
    }
}


/**
 * Print debug message depending on the m_cVerbosity level.
 *
 * @param   iMinLevel       The minimum m_cVerbosity level for this message.
 * @param   fMsg            Whether to dump parts for the current DHCP message.
 * @param   pszFmt          The message format string.
 * @param   va              Optional arguments.
 */
void VBoxNetBaseService::debugPrintV(int iMinLevel, bool fMsg, const char *pszFmt, va_list va) const
{
    if (iMinLevel <= m_cVerbosity)
    {
        va_list vaCopy;                 /* This dude is *very* special, thus the copy. */
        va_copy(vaCopy, va);
        RTStrmPrintf(g_pStdErr, "VBoxNetDHCP: %s: %N\n", iMinLevel >= 2 ? "debug" : "info", pszFmt, &vaCopy);
        va_end(vaCopy);
    }

}

PRTGETOPTDEF VBoxNetBaseService::getOptionsPtr()
{
    PRTGETOPTDEF pOptArray = NULL;
    pOptArray = (PRTGETOPTDEF)RTMemAlloc(sizeof(RTGETOPTDEF) * m_vecOptionDefs.size());
    if (!pOptArray)
        return NULL;
    for (unsigned int i = 0; i < m_vecOptionDefs.size(); ++i)
    {
        PRTGETOPTDEF pOpt = m_vecOptionDefs[i];
        memcpy(&pOptArray[i], m_vecOptionDefs[i], sizeof(RTGETOPTDEF));
    }
    return pOptArray;
}
