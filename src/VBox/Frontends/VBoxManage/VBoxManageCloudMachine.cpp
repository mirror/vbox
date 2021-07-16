/* $Id$ */
/** @file
 * VBoxManageCloudMachine - The cloud machine related commands.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxManage.h"

#include <VBox/log.h>

#include <VBox/com/ErrorInfo.h>
#include <VBox/com/Guid.h>
#include <VBox/com/errorprint.h>


static int selectCloudProvider(ComPtr<ICloudProvider> &pProvider,
                               const ComPtr<IVirtualBox> &pVirtualBox,
                               const char *pszProviderName);
static int selectCloudProfile(ComPtr<ICloudProfile> &pProfile,
                              const ComPtr<ICloudProvider> &pProvider,
                              const char *pszProviderName);

static RTEXITCODE handleCloudMachineImpl(HandlerArg *a, int iFirst,
                                         const ComPtr<ICloudProfile> &pProfile);
static RTEXITCODE listCloudMachinesImpl(HandlerArg *a, int iFirst,
                                        const ComPtr<ICloudProfile> &pProfile);



/*
 * This is a temporary hack as I don't want to refactor "cloud"
 * handling right now, as it's not yet clear to me what is the
 * direction that we want to take with it.
 *
 * The problem with the way "cloud" command handling is currently
 * written is that it's a bit schizophrenic about whether we have
 * multiple cloud providers or not.  OTOH it insists on --provider
 * being mandatory, on the other it hardcodes the list of available
 * subcommands, though in principle those can vary from provider to
 * provider.  If we do want to support multiple providers we might
 * need to come up with a way to allow an extpack provider to supply
 * its own VBoxManage command handler for "cloud" based on --provider
 * as the selector.
 *
 * Processing of --provider and --profile should not be postponed
 * until the leaf command handler, but rather happen immediately, so
 * do this here at our earliest opportunity (without actually doing it
 * in handleCloud).
 */
RTEXITCODE
handleCloudMachine(HandlerArg *a, int iFirst,
                   const char *pcszProviderName,
                   const char *pcszProfileName)
{
    int rc;

    ComPtr<ICloudProvider> pProvider;
    rc = selectCloudProvider(pProvider, a->virtualBox, pcszProviderName);
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    ComPtr<ICloudProfile> pProfile;
    rc = selectCloudProfile(pProfile, pProvider, pcszProfileName);
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    return handleCloudMachineImpl(a, iFirst, pProfile);
}


/*
 * Select cloud provider to use based on the --provider option to the
 * "cloud" command.  The option is not mandatory if only a single
 * provider is available.
 */
static int
selectCloudProvider(ComPtr<ICloudProvider> &pProvider,
                    const ComPtr<IVirtualBox> &pVirtualBox,
                    const char *pcszProviderName)
{
    HRESULT hrc;

    ComPtr<ICloudProviderManager> pCloudProviderManager;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
        COMGETTER(CloudProviderManager)(pCloudProviderManager.asOutParam()),
            VERR_GENERAL_FAILURE);


    /*
     * If the provider is explicitly specified, just look it up and
     * return.
     */
    if (pcszProviderName != NULL)
    {
        /*
         * Should we also provide a way to specify the provider also
         * by its id?  Is it even useful?  If so, should we use a
         * different option or check if the provider name looks like
         * an id and used a different getter?
         */
        CHECK_ERROR2_RET(hrc, pCloudProviderManager,
            GetProviderByShortName(com::Bstr(pcszProviderName).raw(),
                                   pProvider.asOutParam()),
                VERR_NOT_FOUND);

        return VINF_SUCCESS;
    }


    /*
     * We have only one provider and it's not clear if we will ever
     * have more than one.  Forcing the user to explicitly specify the
     * only provider available is not very nice.  So try to be
     * friendly.
     */
    com::SafeIfaceArray<ICloudProvider> aProviders;
    CHECK_ERROR2_RET(hrc, pCloudProviderManager,
        COMGETTER(Providers)(ComSafeArrayAsOutParam(aProviders)),
            VERR_GENERAL_FAILURE);

    if (aProviders.size() == 0)
    {
        RTMsgError("cloud: no providers available");
        return VERR_NOT_FOUND;
    }

    if (aProviders.size() > 1)
    {
        RTMsgError("cloud: multiple providers available,"
                   " '--provider' option is required");
        return VERR_MISSING;
    }

    /* Do RTMsgInfo telling the user which one was selected? */
    pProvider = aProviders[0];
    return VINF_SUCCESS;
}


/*
 * Select cloud profile to use based on the --profile option to the
 * "cloud" command.  The option is not mandatory if only a single
 * profile exists.
 */
static int
selectCloudProfile(ComPtr<ICloudProfile> &pProfile,
                   const ComPtr<ICloudProvider> &pProvider,
                   const char *pcszProfileName)
{
    HRESULT hrc;

    /*
     * If the profile is explicitly specified, just look it up and
     * return.
     */
    if (pcszProfileName != NULL)
    {
        CHECK_ERROR2_RET(hrc, pProvider,
            GetProfileByName(com::Bstr(pcszProfileName).raw(),
                             pProfile.asOutParam()),
                VERR_NOT_FOUND);

        return VINF_SUCCESS;
    }


    /*
     * If the user has just one profile for this provider, don't force
     * them to specify it.  I'm not entirely sure about this one,
     * actually.  It's nice for interactive use, but it might be not
     * forward compatible if used in a script and then when another
     * profile is created the script starts failing.  I'd say, give
     * them enough rope...
     */
    com::SafeIfaceArray<ICloudProfile> aProfiles;
    CHECK_ERROR2_RET(hrc, pProvider,
        COMGETTER(Profiles)(ComSafeArrayAsOutParam(aProfiles)),
            VERR_GENERAL_FAILURE);

    if (aProfiles.size() == 0)
    {
        RTMsgError("cloud: no profiles exist");
        return VERR_NOT_FOUND;
    }

    if (aProfiles.size() > 1)
    {
        RTMsgError("cloud: multiple profiles exist,"
                   " '--profile' option is required");
        return VERR_MISSING;
    }

    /* Do RTMsgInfo telling the user which one was selected? */
    pProfile = aProfiles[0];
    return VINF_SUCCESS;
}


/*
 * RTGETOPTINIT_FLAGS_NO_STD_OPTS recognizes both --help and --version
 * and we don't want the latter.  It's easier to add one line of this
 * macro to the s_aOptions initializers than to filter out --version.
 */
#define CLOUD_MACHINE_RTGETOPTDEF_HELP                                      \
        { "--help",         'h',                    RTGETOPT_REQ_NOTHING }, \
        { "-help",          'h',                    RTGETOPT_REQ_NOTHING }, \
        { "help",           'h',                    RTGETOPT_REQ_NOTHING }, \
        { "-?",             'h',                    RTGETOPT_REQ_NOTHING }


/*
 * cloud machine ...
 *
 * The "cloud" prefix handling is in VBoxManageCloud.cpp, so this
 * function is not static.
 */
static RTEXITCODE
handleCloudMachineImpl(HandlerArg *a, int iFirst,
                       const ComPtr<ICloudProfile> &pProfile)
{
    /*
     * cloud machine ...
     */
    enum
    {
        kMachineIota = 1000,
        kMachine_Info,
        kMachine_List,
    };

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "info",           kMachine_Info,          RTGETOPT_REQ_NOTHING },
        { "list",           kMachine_List,          RTGETOPT_REQ_NOTHING },
          CLOUD_MACHINE_RTGETOPTDEF_HELP
    };

    int rc;

    // setCurrentSubcommand(...);

    RTGETOPTSTATE OptState;
    rc = RTGetOptInit(&OptState, a->argc, a->argv,
                      s_aOptions, RT_ELEMENTS(s_aOptions),
                      iFirst, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    AssertRCStmt(rc,
        return RTMsgErrorExit(RTEXITCODE_INIT, /* internal error */
                   "cloud machine: RTGetOptInit: %Rra", rc));

    int ch;
    RTGETOPTUNION Val;
    while ((ch = RTGetOpt(&OptState, &Val)) != 0)
    {
        switch (ch)
        {
            case kMachine_Info:
                return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                      "cloud machine info: not yet implemented");

            case kMachine_List:
                return listCloudMachinesImpl(a, OptState.iNext, pProfile);


            case 'h':           /* --help */
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;


            case VINF_GETOPT_NOT_OPTION:
                return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                           "Invalid sub-command: %s", Val.psz);

            default:
                return RTGetOptPrintError(ch, &Val);
        }
    }

    return RTMsgErrorExit(RTEXITCODE_SYNTAX,
               "cloud machine: command required\n"
               "Try '--help' for more information.");
}


/*
 * cloud list machines
 *
 * The "cloud list" prefix handling is in VBoxManageCloud.cpp, so this
 * function is not static.  See handleCloudMachine() for the
 * explanation early provider/profile lookup.
 */
RTEXITCODE
listCloudMachines(HandlerArg *a, int iFirst,
                  const char *pcszProviderName,
                  const char *pcszProfileName)
{
    int rc;

    ComPtr<ICloudProvider> pProvider;
    rc = selectCloudProvider(pProvider, a->virtualBox, pcszProviderName);
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    ComPtr<ICloudProfile> pProfile;
    rc = selectCloudProfile(pProfile, pProvider, pcszProfileName);
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    return listCloudMachinesImpl(a, iFirst, pProfile);
}


/*
 * cloud machine list   # convenience alias
 * cloud list machines  # see above
 */
static RTEXITCODE
listCloudMachinesImpl(HandlerArg *a, int iFirst,
                      const ComPtr<ICloudProfile> &pProfile)
{
    /*
     * cloud machine list
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--long",         'l',                    RTGETOPT_REQ_NOTHING },
        { "--short",        's',                    RTGETOPT_REQ_NOTHING },
          CLOUD_MACHINE_RTGETOPTDEF_HELP
    };

    enum kFormatEnum
    {
        kFormat_Default,
        kFormat_Short,
        kFormat_Long
    };

    HRESULT hrc;
    int rc;

    kFormatEnum enmFormat = kFormat_Default;


    RTGETOPTSTATE OptState;
    rc = RTGetOptInit(&OptState, a->argc, a->argv,
                      s_aOptions, RT_ELEMENTS(s_aOptions),
                      iFirst, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    AssertRCStmt(rc,
        return RTMsgErrorExit(RTEXITCODE_INIT, /* internal error */
                   "cloud machine list: RTGetOptInit: %Rra", rc));

    int ch;
    RTGETOPTUNION Val;
    while ((ch = RTGetOpt(&OptState, &Val)) != 0)
    {
        switch (ch)
        {
            case 'l':
                enmFormat = kFormat_Long;
                break;

            case 's':
                enmFormat = kFormat_Short;
                break;

            case 'h':           /* --help */
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;


            case VINF_GETOPT_NOT_OPTION:
                return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                           "Invalid sub-command: %s", Val.psz);

            default:
                return RTGetOptPrintError(ch, &Val);
        }
    }

    ComPtr<ICloudClient> pClient;
    CHECK_ERROR2_RET(hrc, pProfile,
        CreateCloudClient(pClient.asOutParam()),
            RTEXITCODE_FAILURE);

    ComPtr<IProgress> pListProgress;
    CHECK_ERROR2_RET(hrc, pClient,
        ReadCloudMachineList(pListProgress.asOutParam()),
            RTEXITCODE_FAILURE);

    hrc = showProgress(pListProgress); // XXX: don't show
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    com::SafeIfaceArray<ICloudMachine> aMachines;
    CHECK_ERROR2_RET(hrc, pClient,
        COMGETTER(CloudMachineList)(ComSafeArrayAsOutParam(aMachines)),
            RTEXITCODE_FAILURE);

    for (size_t i = 0; i < aMachines.size(); ++i)
    {
        const ComPtr<ICloudMachine> pMachine = aMachines[i];

        com::Bstr bstrId;
        CHECK_ERROR2_RET(hrc, pMachine,
            COMGETTER(Id)(bstrId.asOutParam()),
                RTEXITCODE_FAILURE);

        com::Bstr bstrName;
        CHECK_ERROR2_RET(hrc, pMachine,
            COMGETTER(Name)(bstrName.asOutParam()),
                RTEXITCODE_FAILURE);

        RTPrintf("%ls %ls\n", bstrId.raw(), bstrName.raw());
    }

    return RTEXITCODE_SUCCESS;
}
