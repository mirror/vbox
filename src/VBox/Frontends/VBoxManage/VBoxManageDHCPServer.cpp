/* $Id$ */
/** @file
 * VBoxManage - Implementation of dhcpserver command.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/cidr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/net.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>

#include <VBox/log.h>

#include "VBoxManage.h"

#include <string>
#include <vector>
#include <map>

using namespace com;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define DHCPD_CMD_COMMON_OPT_NETWORK          999 /**< The --network / --netname option number. */
#define DHCPD_CMD_COMMON_OPT_INTERFACE        998 /**< The --interface / --ifname option number. */
/** Common option definitions. */
#define DHCPD_CMD_COMMON_OPTION_DEFS() \
        { "--network",              DHCPD_CMD_COMMON_OPT_NETWORK,       RTGETOPT_REQ_STRING  }, \
        { "--netname",              DHCPD_CMD_COMMON_OPT_NETWORK,       RTGETOPT_REQ_STRING  }, /* legacy */ \
        { "--interface",            DHCPD_CMD_COMMON_OPT_INTERFACE,     RTGETOPT_REQ_STRING  }, \
        { "--ifname",               DHCPD_CMD_COMMON_OPT_INTERFACE,     RTGETOPT_REQ_STRING  }  /* legacy */

/** Handles common options in the typical option parsing switch. */
#define DHCPD_CMD_COMMON_OPTION_CASES(a_pCtx, a_ch, a_pValueUnion) \
        case DHCPD_CMD_COMMON_OPT_NETWORK: \
            if ((a_pCtx)->pszInterface != NULL) \
                return errorSyntax("Either --network or --interface, not both"); \
            (a_pCtx)->pszNetwork = ValueUnion.psz; \
            break; \
        case DHCPD_CMD_COMMON_OPT_INTERFACE: \
            if ((a_pCtx)->pszNetwork != NULL) \
                return errorSyntax("Either --interface or --network, not both"); \
            (a_pCtx)->pszInterface = ValueUnion.psz; \
            break


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Definition of a dhcpserver command, with handler and various flags.
 */
typedef struct DHCPDCMDDEF
{
    /** The command name. */
    const char *pszName;

    /**
     * Actual command handler callback.
     *
     * @param   pCtx            Pointer to command context to use.
     */
    DECLR3CALLBACKMEMBER(RTEXITCODE, pfnHandler, (struct DHCPDCMDCTX *pCtx, int argc, char **argv));

    /** The sub-command scope flags. */
    uint64_t    fSubcommandScope;
} DHCPDCMDDEF;
/** Pointer to a const dhcpserver command definition. */
typedef DHCPDCMDDEF const *PCDHCPDCMDDEF;

/**
 * dhcpserver command context (mainly for carrying common options and such).
 */
typedef struct DHCPDCMDCTX
{
    /** The handler arguments from the main() function. */
    HandlerArg     *pArg;
    /** Pointer to the command definition. */
    PCDHCPDCMDDEF   pCmdDef;
    /** The network name. */
    const char     *pszNetwork;
    /** The (trunk) interface name. */
    const char     *pszInterface;
} DHCPDCMDCTX;
/** Pointer to a dhcpserver command context. */
typedef DHCPDCMDCTX *PDHCPDCMDCTX;



typedef enum enMainOpCodes
{
    OP_ADD = 1000,
    OP_REMOVE,
    OP_MODIFY,
    OP_RESTART
} OPCODE;

typedef std::pair<DhcpOpt_T, std::string> DhcpOptSpec;
typedef std::vector<DhcpOptSpec> DhcpOpts;
typedef DhcpOpts::iterator DhcpOptIterator;

typedef std::vector<DhcpOpt_T> DhcpOptIds;
typedef DhcpOptIds::iterator DhcpOptIdIterator;

struct VmNameSlotKey
{
    const std::string VmName;
    uint8_t u8Slot;

    VmNameSlotKey(const std::string &aVmName, uint8_t aSlot)
      : VmName(aVmName)
      , u8Slot(aSlot)
    {}

    bool operator< (const VmNameSlotKey& that) const
    {
        if (VmName == that.VmName)
            return u8Slot < that.u8Slot;
        else
            return VmName < that.VmName;
    }
};

typedef std::map<VmNameSlotKey, DhcpOpts> VmSlot2OptionsM;
typedef VmSlot2OptionsM::iterator VmSlot2OptionsIterator;
typedef VmSlot2OptionsM::value_type VmSlot2OptionsPair;

typedef std::map<VmNameSlotKey, DhcpOptIds> VmSlot2OptionIdsM;
typedef VmSlot2OptionIdsM::iterator VmSlot2OptionIdsIterator;

typedef std::vector<VmNameSlotKey> VmConfigs;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/



static RTEXITCODE dhcpdHandleTooManyCmds(PDHCPDCMDCTX pCtx, int argc, char **argv)
{
    // glue - start
    HandlerArg *a       = pCtx->pArg;
    OPCODE      enmCode = pCtx->pCmdDef->fSubcommandScope == HELP_SCOPE_DHCPSERVER_ADD ? OP_ADD
                        : pCtx->pCmdDef->fSubcommandScope == HELP_SCOPE_DHCPSERVER_MODIFY ? OP_MODIFY
                        : pCtx->pCmdDef->fSubcommandScope == HELP_SCOPE_DHCPSERVER_REMOVE ? OP_REMOVE : OP_RESTART;
    RT_NOREF(argc, argv);
    // glue - end

    static const RTGETOPTDEF s_aDHCPIPOptions[] =
    {
        DHCPD_CMD_COMMON_OPTION_DEFS(),
        { "--ip",               'a', RTGETOPT_REQ_STRING  },
        { "-ip",                'a', RTGETOPT_REQ_STRING  },    // deprecated
        { "--netmask",          'm', RTGETOPT_REQ_STRING  },
        { "-netmask",           'm', RTGETOPT_REQ_STRING  },    // deprecated
        { "--lowerip",          'l', RTGETOPT_REQ_STRING  },
        { "-lowerip",           'l', RTGETOPT_REQ_STRING  },    // deprecated
        { "--upperip",          'u', RTGETOPT_REQ_STRING  },
        { "-upperip",           'u', RTGETOPT_REQ_STRING  },    // deprecated
        { "--enable",           'e', RTGETOPT_REQ_NOTHING },
        { "-enable",            'e', RTGETOPT_REQ_NOTHING },    // deprecated
        { "--disable",          'd', RTGETOPT_REQ_NOTHING },
        { "-disable",           'd', RTGETOPT_REQ_NOTHING },    // deprecated
        { "--global",           'g', RTGETOPT_REQ_NOTHING },
        { "--vm",               'M', RTGETOPT_REQ_STRING  },
        { "--nic",              'n', RTGETOPT_REQ_UINT8   },
        { "--add-opt",          'A', RTGETOPT_REQ_STRING  },
        { "--del-opt",          'D', RTGETOPT_REQ_STRING  },
        { "--id",               'i', RTGETOPT_REQ_UINT8   },    // obsolete, backwards compatibility only.
        { "--value",            'p', RTGETOPT_REQ_STRING  },    // obsolete, backwards compatibility only.
        { "--remove",           'r', RTGETOPT_REQ_NOTHING },    // obsolete, backwards compatibility only.
        { "--options",          'o', RTGETOPT_REQ_NOTHING },    // obsolete legacy, ignored

    };

    bool fNeedValueOrRemove = false; /* Only used with --id; remove in 6.1+ */

    const char *pszDhcpdIp = NULL;
    const char *pszNetmask = NULL;
    const char *pszLowerIp = NULL;
    const char *pszUpperIp = NULL;
    int         fEnabled   = -1;

    DhcpOpts          GlobalDhcpOptions;
    DhcpOptIds        GlobalDhcpOptions2Delete;
    VmSlot2OptionsM   VmSlot2Options;
    VmSlot2OptionIdsM VmSlot2Options2Delete;
    /// @todo what was  this for: VmConfigs         VmConfigs2Delete;

    const char       *pszVmName             = NULL;
    uint8_t           u8OptId               = 0;
    uint8_t           u8Slot                = 0;
    DhcpOpts         *pScopeOptions         = &GlobalDhcpOptions;
    DhcpOptIds       *pScopeOptions2Delete  = &GlobalDhcpOptions2Delete;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState,
                 a->argc,
                 a->argv,
                 s_aDHCPIPOptions,
                 enmCode != OP_REMOVE ? RT_ELEMENTS(s_aDHCPIPOptions) : 4, /* we use only --netname and --ifname for remove*/
                 1,
                 0);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            DHCPD_CMD_COMMON_OPTION_CASES(pCtx, c, &ValueUnion);
            case 'a':   // -ip
                pszDhcpdIp = ValueUnion.psz;
                break;
            case 'm':   // --netmask
                pszNetmask = ValueUnion.psz;
                break;
            case 'l':   // --lowerip
                pszLowerIp = ValueUnion.psz;
                break;
            case 'u':   // --upperip
                pszUpperIp = ValueUnion.psz;
                break;
            case 'e':   // --enable
                fEnabled = 1;
                break;
            case 'd':   // --disable
                fEnabled = 0;
                break;

            case 'g':   // --global     Sets the option scope to 'global'.
                if (fNeedValueOrRemove)
                    return errorSyntax("Incomplete option sequence preseeding '--global'");
                pScopeOptions         = &GlobalDhcpOptions;
                pScopeOptions2Delete  = &GlobalDhcpOptions2Delete;
                break;

            case 'M':   // --vm         Sets the option scope to ValueUnion.psz + 0.
                if (fNeedValueOrRemove)
                    return errorSyntax("Incomplete option sequence preseeding '--vm'");
                pszVmName = ValueUnion.psz;
                u8Slot    = 0;
                pScopeOptions        = &VmSlot2Options[VmNameSlotKey(pszVmName, u8Slot)];
                pScopeOptions2Delete = &VmSlot2Options2Delete[VmNameSlotKey(pszVmName, u8Slot)];
                break;

            case 'n':   // --nic        Sets the option scope to pszVmName + (ValueUnion.u8 - 1).
                if (!pszVmName)
                    return errorSyntax("--nic option requires a --vm preceeding selecting the VM it should apply to");
                if (fNeedValueOrRemove)
                    return errorSyntax("Incomplete option sequence preseeding '--nic=%u", ValueUnion.u8);
                u8Slot = ValueUnion.u8;
                if (u8Slot < 1)
                    return errorSyntax("invalid NIC number: %u", u8Slot);
                --u8Slot;
                pScopeOptions        = &VmSlot2Options[VmNameSlotKey(pszVmName, u8Slot)];
                pScopeOptions2Delete = &VmSlot2Options2Delete[VmNameSlotKey(pszVmName, u8Slot)];
                break;

            case 'A':   // --add-opt num hexvalue
            {
                uint8_t const idAddOpt = ValueUnion.u8;
                c = RTGetOptFetchValue(&GetState, &ValueUnion, RTGETOPT_REQ_STRING);
                if (RT_FAILURE(c))
                    return errorFetchValue(1, "--add-opt", c, &ValueUnion);
                pScopeOptions->push_back(DhcpOptSpec((DhcpOpt_T)idAddOpt, std::string(ValueUnion.psz)));
                break;
            }

            case 'D':   // --del-opt num
                if (enmCode == OP_ADD)
                    return errorSyntax("--del-opt does not apply to the 'add' subcommand");
                pScopeOptions2Delete->push_back((DhcpOpt_T)ValueUnion.u8);
                break;

            /*
             * For backwards compatibility. Remove in 6.1 or later.
             */

            case 'o':   // --options - obsolete, ignored.
                break;

            case 'i':   // --id
                if (fNeedValueOrRemove)
                    return errorSyntax("Incomplete option sequence preseeding '--id=%u", ValueUnion.u8);
                u8OptId = ValueUnion.u8;
                fNeedValueOrRemove = true;
                break;

            case 'p':   // --value
                if (!fNeedValueOrRemove)
                    return errorSyntax("--value without --id=dhcp-opt-no");
                pScopeOptions->push_back(DhcpOptSpec((DhcpOpt_T)u8OptId, std::string(ValueUnion.psz)));
                fNeedValueOrRemove = false;
                break;

            case 'r':   // --remove
                if (enmCode == OP_ADD)
                    return errorSyntax("--remove does not apply to the 'add' subcommand");
                if (!fNeedValueOrRemove)
                    return errorSyntax("--remove without --id=dhcp-opt-no");
                pScopeOptions2Delete->push_back((DhcpOpt_T)u8OptId);
                /** @todo remove from pScopeOptions */
                fNeedValueOrRemove = false;
                break;

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (!pCtx->pszNetwork && !pCtx->pszInterface)
        return errorSyntax("You need to specify either --network or --interface to identify the DHCP server");

    if (   enmCode != OP_REMOVE
        && enmCode != OP_RESTART
        && GlobalDhcpOptions.empty()
        && VmSlot2Options.empty()
        && GlobalDhcpOptions2Delete.empty()
        && VmSlot2Options2Delete.empty())
    {
        /** @todo For the 'modify' subcmd, missing configuration parameters could be
         * retrieved from the current config.  All are only required for 'add'!
         * The 'fEnabled' attribute does not come into play here.  */
        if (fEnabled < 0 || pszDhcpdIp || pszNetmask || pszLowerIp || pszUpperIp)
        {
            RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
            if (!pszDhcpdIp)
                rcExit = errorSyntax("Missing required option: --ip");
            if (!pszNetmask)
                rcExit = errorSyntax("Missing required option: --netmask");
            if (!pszLowerIp)
                rcExit = errorSyntax("Missing required option: --lowerip");
            if (!pszUpperIp)
                rcExit = errorSyntax("Missing required option: --upperip");
            if (rcExit != RTEXITCODE_SUCCESS)
                return rcExit;
        }
    }

    HRESULT rc;
    Bstr NetName;
    if (!pCtx->pszNetwork)
    {
        ComPtr<IHost> host;
        CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

        ComPtr<IHostNetworkInterface> hif;
        CHECK_ERROR(host, FindHostNetworkInterfaceByName(Bstr(pCtx->pszInterface).mutableRaw(), hif.asOutParam()));
        if (FAILED(rc))
            return errorArgument("Could not find interface '%s'", pCtx->pszInterface);

        CHECK_ERROR(hif, COMGETTER(NetworkName) (NetName.asOutParam()));
        if (FAILED(rc))
            return errorArgument("Could not get network name for the interface '%s'", pCtx->pszInterface);
    }
    else
    {
        NetName = Bstr(pCtx->pszNetwork);
    }

    ComPtr<IDHCPServer> svr;
    rc = a->virtualBox->FindDHCPServerByNetworkName(NetName.mutableRaw(), svr.asOutParam());
    if(enmCode == OP_ADD)
    {
        if (SUCCEEDED(rc))
            return errorArgument("DHCP server already exists");

        CHECK_ERROR(a->virtualBox, CreateDHCPServer(NetName.mutableRaw(), svr.asOutParam()));
        if (FAILED(rc))
            return errorArgument("Failed to create the DHCP server");
    }
    else if (FAILED(rc))
    {
        return errorArgument("DHCP server does not exist");
    }

    if (enmCode == OP_RESTART)
    {
        CHECK_ERROR(svr, Restart());
        if(FAILED(rc))
            return errorArgument("Failed to restart server");
    }
    else if (enmCode == OP_REMOVE)
    {
        CHECK_ERROR(a->virtualBox, RemoveDHCPServer(svr));
        if(FAILED(rc))
            return errorArgument("Failed to remove server");
    }
    else
    {
        if (pszDhcpdIp || pszNetmask || pszLowerIp || pszUpperIp)
        {
            CHECK_ERROR(svr, SetConfiguration(Bstr(pszDhcpdIp).mutableRaw(),
                                              Bstr(pszNetmask).mutableRaw(),
                                              Bstr(pszLowerIp).mutableRaw(),
                                              Bstr(pszUpperIp).mutableRaw()));
            if (FAILED(rc))
                return errorArgument("Failed to set configuration");
        }

        if (fEnabled >= 0)
            CHECK_ERROR(svr, COMSETTER(Enabled)((BOOL)fEnabled));

        /* Remove options: */
        for (DhcpOptIdIterator itOptId = GlobalDhcpOptions2Delete.begin(); itOptId != GlobalDhcpOptions2Delete.end(); ++itOptId)
            CHECK_ERROR(svr, RemoveGlobalOption(*itOptId));

        for (VmSlot2OptionIdsIterator itIdVector = VmSlot2Options2Delete.begin();
             itIdVector != VmSlot2Options2Delete.end(); ++itIdVector)
            for (DhcpOptIdIterator itOptId = itIdVector->second.begin(); itOptId != itIdVector->second.end(); ++itOptId)
                CHECK_ERROR(svr, RemoveVmSlotOption(Bstr(itIdVector->first.VmName.c_str()).raw(),
                                                    itIdVector->first.u8Slot, *itOptId));

        /* Global Options */
        for (DhcpOptIterator itOpt = GlobalDhcpOptions.begin(); itOpt != GlobalDhcpOptions.end(); ++itOpt)
            CHECK_ERROR(svr, AddGlobalOption(itOpt->first, com::Bstr(itOpt->second.c_str()).raw()));

        /* VM slot options. */
        for (VmSlot2OptionsIterator it = VmSlot2Options.begin(); it != VmSlot2Options.end(); ++it)
            for (DhcpOptIterator itOpt = it->second.begin(); itOpt != it->second.end(); ++itOpt)
                CHECK_ERROR(svr, AddVmSlotOption(Bstr(it->first.VmName.c_str()).raw(), it->first.u8Slot, itOpt->first,
                                                 com::Bstr(itOpt->second.c_str()).raw()));
    }

    return RTEXITCODE_SUCCESS;
}


/**
 * Helper that find the DHCP server instance.
 *
 * @returns The DHCP server instance. NULL if failed (complaining done).
 * @param   pCtx                The DHCP server command context.
 */
static ComPtr<IDHCPServer> dhcpdFindServer(PDHCPDCMDCTX pCtx)
{
    ComPtr<IDHCPServer> ptrRet;
    if (pCtx->pszNetwork || pCtx->pszInterface)
    {
        Assert(pCtx->pszNetwork == NULL || pCtx->pszInterface == NULL);

        /*
         * We need a network name to find the DHCP server.  So, if interface is
         * given we have to look it up.
         */
        HRESULT hrc;
        Bstr bstrNetName(pCtx->pszNetwork);
        if (!pCtx->pszNetwork)
        {
            ComPtr<IHost> ptrIHost;
            CHECK_ERROR2_RET(hrc, pCtx->pArg->virtualBox, COMGETTER(Host)(ptrIHost.asOutParam()), ptrRet);

            Bstr bstrInterface(pCtx->pszInterface);
            ComPtr<IHostNetworkInterface> ptrIHostIf;
            CHECK_ERROR2(hrc, ptrIHost, FindHostNetworkInterfaceByName(bstrInterface.raw(), ptrIHostIf.asOutParam()));
            if (FAILED(hrc))
            {
                errorArgument("Failed to locate host-only interface '%s'", pCtx->pszInterface);
                return ptrRet;
            }

            CHECK_ERROR2_RET(hrc, ptrIHostIf, COMGETTER(NetworkName)(bstrNetName.asOutParam()), ptrRet);
        }

        /*
         * Now, try locate the server
         */
        hrc = pCtx->pArg->virtualBox->FindDHCPServerByNetworkName(bstrNetName.raw(), ptrRet.asOutParam());
        if (SUCCEEDED(hrc))
            return ptrRet;
        if (pCtx->pszNetwork)
            errorArgument("Failed to find DHCP server for network '%s'", pCtx->pszNetwork);
        else
            errorArgument("Failed to find DHCP server for host-only interface '%s' (network '%ls')",
                          pCtx->pszInterface, bstrNetName.raw());
    }
    else
        errorSyntax("You need to specify either --network or --interface to identify the DHCP server");
    return ptrRet;
}


static RTEXITCODE dhcpdHandleFindLease(PDHCPDCMDCTX pCtx, int argc, char **argv)
{
    /*
     * Parse the command line.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        DHCPD_CMD_COMMON_OPTION_DEFS(),
        { "--mac-address",      'm', RTGETOPT_REQ_MACADDR  },

    };

    bool            fHaveMacAddress   = false;
    RTMAC           MacAddress        = { { 0, 0, 0,  0, 0, 0 } };

    RTGETOPTSTATE   GetState;
    int vrc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    RTGETOPTUNION   ValueUnion;
    while ((vrc = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (vrc)
        {
            DHCPD_CMD_COMMON_OPTION_CASES(pCtx, vrc, &ValueUnion);

            case 'm':   // --mac-address
                fHaveMacAddress = true;
                MacAddress = ValueUnion.MacAddr;
                break;

            default:
                return errorGetOpt(vrc, &ValueUnion);
        }
    }

    if (!fHaveMacAddress)
        return errorSyntax("You need to specify a MAC address too look for");

    /*
     * Locate the server and perform the requested operation.
     */
    ComPtr<IDHCPServer> ptrDHCPServer = dhcpdFindServer(pCtx);
    if (ptrDHCPServer.isNull())
        return RTEXITCODE_FAILURE;

    char    szMac[32];
    RTStrPrintf(szMac, sizeof(szMac), "%RTmac", &MacAddress);
    Bstr    bstrAddress;
    Bstr    bstrState;
    LONG64  secIssued = 0;
    LONG64  secExpire = 0;
    HRESULT hrc;
    CHECK_ERROR2(hrc, ptrDHCPServer, FindLeaseByMAC(Bstr(szMac).raw(), 0 /*type*/,
                                                    bstrAddress.asOutParam(), bstrState.asOutParam(), &secIssued, &secExpire));
    if (SUCCEEDED(hrc))
    {
        RTTIMESPEC  TimeSpec;
        int64_t     cSecLeftToLive = secExpire - RTTimeSpecGetSeconds(RTTimeNow(&TimeSpec));
        RTTIME      Time;
        char        szIssued[RTTIME_STR_LEN];
        RTTimeToStringEx(RTTimeExplode(&Time, RTTimeSpecSetSeconds(&TimeSpec, secIssued)), szIssued, sizeof(szIssued), 0);
        char        szExpire[RTTIME_STR_LEN];
        RTTimeToStringEx(RTTimeExplode(&Time, RTTimeSpecSetSeconds(&TimeSpec, secExpire)), szExpire, sizeof(szExpire), 0);

        RTPrintf("IP Address:  %ls\n"
                 "MAC Address: %RTmac\n"
                 "State:       %ls\n"
                 "Issued:      %s (%RU64)\n"
                 "Expire:      %s (%RU64)\n"
                 "TTL:         %RU64 sec, currently %RU64 sec left\n",
                 bstrAddress.raw(),
                 &MacAddress,
                 bstrState.raw(),
                 szIssued, secIssued,
                 szExpire, secExpire,
                 secExpire >= secIssued ? secExpire - secIssued : 0, cSecLeftToLive > 0  ? cSecLeftToLive : 0);
        return RTEXITCODE_SUCCESS;
    }
    return RTEXITCODE_FAILURE;
}


RTEXITCODE handleDHCPServer(HandlerArg *pArg)
{
    /*
     * Command definitions.
     */
    static const DHCPDCMDDEF s_aCmdDefs[] =
    {
        { "add",            dhcpdHandleTooManyCmds,     HELP_SCOPE_DHCPSERVER_ADD },
        { "modify",         dhcpdHandleTooManyCmds,     HELP_SCOPE_DHCPSERVER_MODIFY },
        { "remove",         dhcpdHandleTooManyCmds,     HELP_SCOPE_DHCPSERVER_REMOVE },
        { "restart",        dhcpdHandleTooManyCmds,     HELP_SCOPE_DHCPSERVER_RESTART },
        { "findlease",      dhcpdHandleFindLease,       HELP_SCOPE_DHCPSERVER_FINDLEASE },
    };

    /*
     * VBoxManage dhcpserver [common-options] subcommand ...
     */
    DHCPDCMDCTX CmdCtx;
    CmdCtx.pArg         = pArg;
    CmdCtx.pCmdDef      = NULL;
    CmdCtx.pszInterface = NULL;
    CmdCtx.pszNetwork   = NULL;

    static const RTGETOPTDEF s_CommonOptions[] = { DHCPD_CMD_COMMON_OPTION_DEFS() };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv, s_CommonOptions, RT_ELEMENTS(s_CommonOptions), 0, 0 /* No sorting! */);

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (ch)
        {
            DHCPD_CMD_COMMON_OPTION_CASES(&CmdCtx, ch, &ValueUnion);

            case VINF_GETOPT_NOT_OPTION:
            {
                const char *pszCmd = ValueUnion.psz;
                uint32_t    iCmd;
                for (iCmd = 0; iCmd < RT_ELEMENTS(s_aCmdDefs); iCmd++)
                    if (strcmp(s_aCmdDefs[iCmd].pszName, pszCmd) == 0)
                    {
                        CmdCtx.pCmdDef = &s_aCmdDefs[iCmd];
                        setCurrentSubcommand(s_aCmdDefs[iCmd].fSubcommandScope);
                        return s_aCmdDefs[iCmd].pfnHandler(&CmdCtx, pArg->argc - GetState.iNext + 1,
                                                           &pArg->argv[GetState.iNext - 1]);
                    }
                return errorUnknownSubcommand(pszCmd);
            }

            default:
                return errorGetOpt(ch, &ValueUnion);
        }
    }
    return errorNoSubcommand();
}

