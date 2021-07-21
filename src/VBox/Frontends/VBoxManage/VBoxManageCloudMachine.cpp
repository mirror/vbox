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

#include <algorithm>
#include <vector>


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
static RTEXITCODE handleCloudMachineInfo(HandlerArg *a, int iFirst,
                                          const ComPtr<ICloudProfile> &pProfile);

static HRESULT printMachineInfo(const ComPtr<ICloudMachine> &pMachine);
static HRESULT printFormValue(const ComPtr<IFormValue> &pValue);



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


static HRESULT
getMachineById(ComPtr<ICloudMachine> &pMachineOut,
               const ComPtr<ICloudProfile> &pProfile,
               const char *pcszStrId)
{
    HRESULT hrc;

    ComPtr<ICloudClient> pCloudClient;
    CHECK_ERROR2_RET(hrc, pProfile,
        CreateCloudClient(pCloudClient.asOutParam()), hrc);

    ComPtr<ICloudMachine> pMachine;
    CHECK_ERROR2_RET(hrc, pCloudClient,
        GetCloudMachine(com::Bstr(pcszStrId).raw(),
                        pMachine.asOutParam()), hrc);

    ComPtr<IProgress> pRefreshProgress;
    CHECK_ERROR2_RET(hrc, pMachine,
        Refresh(pRefreshProgress.asOutParam()), hrc);

    hrc = showProgress(pRefreshProgress, SHOW_PROGRESS_NONE);
    if (FAILED(hrc))
        return hrc;

    pMachineOut = pMachine;
    return S_OK;
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
                return handleCloudMachineInfo(a, OptState.iNext, pProfile);

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
        { "--sort",         's',                    RTGETOPT_REQ_NOTHING },
          CLOUD_MACHINE_RTGETOPTDEF_HELP
    };

    enum kFormatEnum { kFormat_Short, kFormat_Long };
    kFormatEnum enmFormat = kFormat_Short;

    enum kSortOrderEnum { kSortOrder_None, kSortOrder_Name, kSortOrder_Id };
    kSortOrderEnum enmSortOrder = kSortOrder_None;

    HRESULT hrc;
    int rc;


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
                /** @todo optional argument to select the sort key? */
                enmSortOrder = kSortOrder_Name;
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

    hrc = showProgress(pListProgress, SHOW_PROGRESS_NONE);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    com::SafeIfaceArray<ICloudMachine> aMachines;
    CHECK_ERROR2_RET(hrc, pClient,
        COMGETTER(CloudMachineList)(ComSafeArrayAsOutParam(aMachines)),
            RTEXITCODE_FAILURE);

    const size_t cMachines = aMachines.size();
    if (cMachines == 0)
        return RTEXITCODE_SUCCESS;


    /*
     * Get names/ids that we need for the short output and to sort the
     * list.
     */
    std::vector<ComPtr<ICloudMachine> > vMachines(cMachines);
    std::vector<com::Bstr> vBstrNames(cMachines);
    std::vector<com::Bstr> vBstrIds(cMachines);
    for (size_t i = 0; i < cMachines; ++i)
    {
        vMachines[i] = aMachines[i];

        CHECK_ERROR2_RET(hrc, vMachines[i],
            COMGETTER(Name)(vBstrNames[i].asOutParam()),
                RTEXITCODE_FAILURE);

        CHECK_ERROR2_RET(hrc, vMachines[i],
            COMGETTER(Id)(vBstrIds[i].asOutParam()),
                RTEXITCODE_FAILURE);
    }


    /*
     * Sort the list if necessary.  The sort is indirect via an
     * intermediate array of indexes.
     */
    std::vector<size_t> vIndexes(cMachines);
    for (size_t i = 0; i < cMachines; ++i)
        vIndexes[i] = i;

    if (enmSortOrder != kSortOrder_None)
    {
        struct SortBy {
            const std::vector<com::Bstr> &ks;
            SortBy(const std::vector<com::Bstr> &aKeys) : ks(aKeys) {}
            bool operator() (size_t l, size_t r) { return ks[l] < ks[r]; }
        };

        std::sort(vIndexes.begin(), vIndexes.end(),
                  SortBy(enmSortOrder == kSortOrder_Name
                         ? vBstrNames : vBstrIds));
    }


    if (enmFormat == kFormat_Short)
    {
        for (size_t i = 0; i < cMachines; ++i)
        {
            const size_t idx = vIndexes[i];
            const com::Bstr &bstrId = vBstrIds[idx];
            const com::Bstr &bstrName = vBstrNames[idx];

            RTPrintf("%ls %ls\n", bstrId.raw(), bstrName.raw());
        }
    }
    else // kFormat_Long
    {
        for (size_t i = 0; i < cMachines; ++i)
        {
            const size_t idx = vIndexes[i];
            const ComPtr<ICloudMachine> &pMachine = vMachines[idx];

            if (i != 0)
                RTPrintf("\n");
            printMachineInfo(pMachine);
        }
    }

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE
handleCloudMachineInfo(HandlerArg *a, int iFirst,
                       const ComPtr<ICloudProfile> &pProfile)
{
    HRESULT hrc;

    if (a->argc == iFirst)
    {
        return RTMsgErrorExit(RTEXITCODE_SYNTAX,
                   "cloud machine info: machine id required\n"
                   "Try '--help' for more information.");
    }

    for (int i = iFirst; i < a->argc; ++i)
    {
        ComPtr<ICloudMachine> pMachine;
        hrc = getMachineById(pMachine, pProfile, a->argv[i]);
        if (FAILED(hrc))
            return RTEXITCODE_FAILURE;

        hrc = printMachineInfo(pMachine);
        if (FAILED(hrc))
            return RTEXITCODE_FAILURE;
    }

    return RTEXITCODE_SUCCESS;
}


static HRESULT
printMachineInfo(const ComPtr<ICloudMachine> &pMachine)
{
    HRESULT hrc;

    /*
     * Check if the machine is accessible and print the error
     * message if not.
     */
    BOOL fAccessible = FALSE;
    CHECK_ERROR2_RET(hrc, pMachine,
        COMGETTER(Accessible)(&fAccessible), hrc);

    if (!fAccessible)
    {
        RTMsgError("machine is not accessible"); // XXX: Id?

        ComPtr<IVirtualBoxErrorInfo> pErrorInfo;
        CHECK_ERROR2_RET(hrc, pMachine,
            COMGETTER(AccessError)(pErrorInfo.asOutParam()), hrc);

        while (!pErrorInfo.isNull())
        {
            com::Bstr bstrText;
            CHECK_ERROR2_RET(hrc, pErrorInfo,
                COMGETTER(Text)(bstrText.asOutParam()), hrc);
            RTStrmPrintf(g_pStdErr, "%ls\n", bstrText.raw());

            CHECK_ERROR2_RET(hrc, pErrorInfo,
                COMGETTER(Next)(pErrorInfo.asOutParam()), hrc);
        }

        return E_FAIL;
    }


    /*
     * The machine seems to be ok, print its details.
     */
    ComPtr<IForm> pDetails;
    CHECK_ERROR2_RET(hrc, pMachine,
        GetDetailsForm(pDetails.asOutParam()), hrc);

    if (RT_UNLIKELY(pDetails.isNull()))
    {
        RTMsgError("null details"); /* better error message? */
        return E_FAIL;
    }

    com::SafeIfaceArray<IFormValue> aValues;
    CHECK_ERROR2_RET(hrc, pDetails,
        COMGETTER(Values)(ComSafeArrayAsOutParam(aValues)), hrc);
    for (size_t i = 0; i < aValues.size(); ++i)
    {
        hrc = printFormValue(aValues[i]);
        if (FAILED(hrc))
            return hrc;
    }

    return S_OK;
}


static HRESULT
printFormValue(const ComPtr<IFormValue> &pValue)
{
    HRESULT hrc;

    BOOL fVisible = FALSE;
    CHECK_ERROR2_RET(hrc, pValue,
        COMGETTER(Visible)(&fVisible), hrc);
    if (!fVisible)
        return S_OK;


    com::Bstr bstrLabel;
    CHECK_ERROR2_RET(hrc, pValue,
        COMGETTER(Label)(bstrLabel.asOutParam()), hrc);

    FormValueType_T enmType;
    CHECK_ERROR2_RET(hrc, pValue,
        COMGETTER(Type)(&enmType), hrc);

    switch (enmType)
    {
        case FormValueType_Boolean:
        {
            ComPtr<IBooleanFormValue> pBoolValue;
            hrc = pValue.queryInterfaceTo(pBoolValue.asOutParam());
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdErr,
                    "%ls: unable to convert to boolean value\n", bstrLabel.raw());
                break;
            }

            BOOL fSelected;
            hrc = pBoolValue->GetSelected(&fSelected);
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdOut,
                    "%ls: %Rhra", bstrLabel.raw(), hrc);
                break;
            }

            RTPrintf("%ls: %RTbool\n",
                bstrLabel.raw(), RT_BOOL(fSelected));
            break;
        }

        case FormValueType_String:
        {
            ComPtr<IStringFormValue> pStrValue;
            hrc = pValue.queryInterfaceTo(pStrValue.asOutParam());
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdErr,
                    "%ls: unable to convert to string value\n", bstrLabel.raw());
                break;
            }

            /*
             * GUI hack: if clipboard string is set, it contains
             * untruncated long value, usually full OCID, so check it
             * first.  Make this selectable with an option?
             */
            com::Bstr bstrValue;
            hrc = pStrValue->COMGETTER(ClipboardString)(bstrValue.asOutParam());
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdOut,
                    "%ls: %Rhra", bstrLabel.raw(), hrc);
                break;
            }

            if (bstrValue.isEmpty())
            {
                hrc = pStrValue->GetString(bstrValue.asOutParam());
                if (FAILED(hrc))
                {
                    RTStrmPrintf(g_pStdOut,
                        "%ls: %Rhra", bstrLabel.raw(), hrc);
                    break;
                }
            }

            RTPrintf("%ls: %ls\n",
                bstrLabel.raw(), bstrValue.raw());
            break;
        }

        case FormValueType_RangedInteger:
        {
            ComPtr<IRangedIntegerFormValue> pIntValue;
            hrc = pValue.queryInterfaceTo(pIntValue.asOutParam());
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdErr,
                    "%ls: unable to convert to integer value\n", bstrLabel.raw());
                break;
            }

            LONG lValue;
            hrc = pIntValue->GetInteger(&lValue);
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdOut,
                    "%ls: %Rhra", bstrLabel.raw(), hrc);
                break;
            }

            RTPrintf("%ls: %RI64\n",
                bstrLabel.raw(), (int64_t)lValue);
            break;
        }

        case FormValueType_Choice:
        {
            ComPtr<IChoiceFormValue> pChoiceValue;
            hrc = pValue.queryInterfaceTo(pChoiceValue.asOutParam());
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdErr,
                    "%ls: unable to convert to choice value\n", bstrLabel.raw());
                break;
            }

            com::SafeArray<BSTR> aValues;
            hrc = pChoiceValue->COMGETTER(Values)(ComSafeArrayAsOutParam(aValues));
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdOut,
                    "%ls: values: %Rhra", bstrLabel.raw(), hrc);
                break;
            }

            LONG idxSelected = -1;
            hrc = pChoiceValue->GetSelectedIndex(&idxSelected);
            if (FAILED(hrc))
            {
                RTStrmPrintf(g_pStdOut,
                    "%ls: selectedIndex: %Rhra", bstrLabel.raw(), hrc);
                break;
            }

            if (idxSelected < 0 || (size_t)idxSelected >  aValues.size())
            {
                RTStrmPrintf(g_pStdOut,
                    "%ls: selected index %RI64 out of range [0, %zu)\n",
                             bstrLabel.raw(), (int64_t)idxSelected, aValues.size());
                break;
            }

            RTPrintf("%ls: %ls\n",
                bstrLabel.raw(), aValues[idxSelected]);
            break;
        }

        default:
        {
            RTStrmPrintf(g_pStdOut, "unknown value type %RU32\n", enmType);
            break;
        }
    }

    return S_OK;
}
