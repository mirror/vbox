/* $Id$ */
/** @file
 * Implementation of local and cloud gateway management.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_MAIN_CONSOLE

/* Make sure all the stdint.h macros are included - must come first! */
#ifndef __STDC_LIMIT_MACROS
# define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif

#include "LoggingNew.h"
#include "ApplianceImpl.h"
#include "CloudNetworkImpl.h"
#include "CloudGateway.h"

#include <iprt/http.h>
#include <iprt/inifile.h>
#include <iprt/net.h>
#include <iprt/path.h>
#include <iprt/vfs.h>
#include <iprt/uri.h>

static HRESULT setMacAddress(const Utf8Str& str, RTMAC& mac)
{
    int rc = RTNetStrToMacAddr(str.c_str(), &mac);
    if (RT_FAILURE(rc))
    {
        LogRel(("CLOUD-NET: Invalid MAC address '%s'\n", str.c_str()));
        return E_INVALIDARG;
    }
    return S_OK;
}

HRESULT GatewayInfo::setCloudMacAddress(const Utf8Str& mac)
{
    return setMacAddress(mac, mCloudMacAddress);
}

HRESULT GatewayInfo::setLocalMacAddress(const Utf8Str& mac)
{
    return setMacAddress(mac, mLocalMacAddress);
}

Utf8Str GatewayInfo::getCloudMacAddressWithoutColons() const
{
    return Utf8StrFmt("%02X%02X%02X%02X%02X%02X",
                      mCloudMacAddress.au8[0], mCloudMacAddress.au8[1], mCloudMacAddress.au8[2],
                      mCloudMacAddress.au8[3], mCloudMacAddress.au8[4], mCloudMacAddress.au8[5]);
}

Utf8Str GatewayInfo::getLocalMacAddressWithoutColons() const
{
    return Utf8StrFmt("%02X%02X%02X%02X%02X%02X",
                      mLocalMacAddress.au8[0], mLocalMacAddress.au8[1], mLocalMacAddress.au8[2],
                      mLocalMacAddress.au8[3], mLocalMacAddress.au8[4], mLocalMacAddress.au8[5]);
}

Utf8Str GatewayInfo::getLocalMacAddressWithColons() const
{
    return Utf8StrFmt("%RTmac", &mLocalMacAddress);
}

class CloudError
{
public:
    CloudError(HRESULT hrc, const Utf8Str& strText) : mHrc(hrc), mText(strText) {};
    HRESULT getRc() { return mHrc; };
    Utf8Str getText() { return mText; };

private:
    HRESULT mHrc;
    Utf8Str mText;
};

static void handleErrors(HRESULT hrc, const char *pszFormat, ...)
{
    if (FAILED(hrc))
    {
        va_list va;
        va_start(va, pszFormat);
        Utf8Str strError(pszFormat, va);
        va_end(va);
        LogRel(("CLOUD-NET: %s (rc=%x)\n", strError.c_str(), hrc));
        throw CloudError(hrc, strError);
    }

}

class CloudClient
{
public:
    CloudClient(ComPtr<IVirtualBox> virtualBox, const Bstr& strProvider, const Bstr& strProfile);
    ~CloudClient() {};

    void startCloudGateway(const ComPtr<ICloudNetwork> &network, GatewayInfo& gateways);
    void stopCloudGateway(const GatewayInfo& gateways);

private:
    ComPtr<ICloudProviderManager> mManager;
    ComPtr<ICloudProvider>        mProvider;
    ComPtr<ICloudProfile>         mProfile;
    ComPtr<ICloudClient>          mClient;
};

CloudClient::CloudClient(ComPtr<IVirtualBox> virtualBox, const Bstr& strProvider, const Bstr& strProfile)
{
    HRESULT hrc = virtualBox->COMGETTER(CloudProviderManager)(mManager.asOutParam());
    handleErrors(hrc, "Failed to obtain cloud provider manager object");
    hrc = mManager->GetProviderByShortName(strProvider.raw(), mProvider.asOutParam());
    handleErrors(hrc, "Failed to obtain cloud provider '%ls'", strProvider.raw());
    hrc = mProvider->GetProfileByName(strProfile.raw(), mProfile.asOutParam());
    handleErrors(hrc, "Failed to obtain cloud profile '%ls'", strProfile.raw());
    hrc = mProfile->CreateCloudClient(mClient.asOutParam());
    handleErrors(hrc, "Failed to create cloud client");
}

void CloudClient::startCloudGateway(const ComPtr<ICloudNetwork> &network, GatewayInfo& gateways)
{
    ComPtr<IProgress> progress;
    ComPtr<ICloudNetworkGatewayInfo> gatewayInfo;
    HRESULT hrc = mClient->StartCloudNetworkGateway(network, Bstr(gateways.mPublicSshKey).raw(),
                                                    gatewayInfo.asOutParam(), progress.asOutParam());
    handleErrors(hrc, "Failed to launch compute instance");
    hrc = progress->WaitForCompletion(-1);
    handleErrors(hrc, "Failed to launch compute instance (wait)");

    Bstr instanceId;
    hrc = gatewayInfo->COMGETTER(InstanceId)(instanceId.asOutParam());
    handleErrors(hrc, "Failed to get launched compute instance id");
    gateways.mGatewayInstanceId = instanceId;

    Bstr publicIP;
    hrc = gatewayInfo->COMGETTER(PublicIP)(publicIP.asOutParam());
    handleErrors(hrc, "Failed to get cloud gateway public IP address");
    gateways.mCloudPublicIp = publicIP;

    Bstr secondaryPublicIP;
    hrc = gatewayInfo->COMGETTER(SecondaryPublicIP)(secondaryPublicIP.asOutParam());
    handleErrors(hrc, "Failed to get cloud gateway secondary public IP address");
    gateways.mCloudSecondaryPublicIp = secondaryPublicIP;

    Bstr macAddress;
    hrc = gatewayInfo->COMGETTER(MacAddress)(macAddress.asOutParam());
    handleErrors(hrc, "Failed to get cloud gateway public IP address");
    gateways.setCloudMacAddress(macAddress);
}

void CloudClient::stopCloudGateway(const GatewayInfo& gateways)
{
    ComPtr<IProgress> progress;
    HRESULT hrc = mClient->TerminateInstance(Bstr(gateways.mGatewayInstanceId).raw(), progress.asOutParam());
    handleErrors(hrc, "Failed to terminate compute instance");
#if 0
    /* Someday we may want to wait until the cloud gateway has terminated. */
    hrc = progress->WaitForCompletion(-1);
    handleErrors(hrc, "Failed to terminate compute instance (wait)");
#endif
}


static HRESULT startCloudGateway(ComPtr<IVirtualBox> virtualBox, ComPtr<ICloudNetwork> network, GatewayInfo& gateways)
{
    HRESULT hrc = S_OK;

    try {
        hrc = network->COMGETTER(Provider)(gateways.mCloudProvider.asOutParam());
        hrc = network->COMGETTER(Profile)(gateways.mCloudProfile.asOutParam());
        CloudClient client(virtualBox, gateways.mCloudProvider, gateways.mCloudProfile);
        client.startCloudGateway(network, gateways);
    }
    catch (CloudError e)
    {
        hrc = e.getRc();
    }

    return hrc;
}


static HRESULT attachToLocalNetwork(ComPtr<ISession> aSession, const com::Utf8Str &aCloudNetwork)
{
    ComPtr<IMachine> sessionMachine;
    HRESULT hrc = aSession->COMGETTER(Machine)(sessionMachine.asOutParam());
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to obtain a mutable machine. hrc=%x\n", hrc));
        return hrc;
    }

    ComPtr<INetworkAdapter> networkAdapter;
    hrc = sessionMachine->GetNetworkAdapter(1, networkAdapter.asOutParam());
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to locate the second network adapter. hrc=%x\n", hrc));
        return hrc;
    }

    BstrFmt network("cloud-%s", aCloudNetwork.c_str());
    hrc = networkAdapter->COMSETTER(InternalNetwork)(network.raw());
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to set network name for the second network adapter. hrc=%x\n", hrc));
        return hrc;
    }

    hrc = sessionMachine->SaveSettings();
    if (FAILED(hrc))
        LogRel(("CLOUD-NET: Failed to save 'lgw' settings. hrc=%x\n", hrc));
    return hrc;
}

static HRESULT startLocalGateway(ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> aSession, const com::Utf8Str &aCloudNetwork, GatewayInfo& gateways)
{
    /*
     * It would be really beneficial if we do not create a local gateway VM each time a target starts.
     * We probably just need to make sure its configuration matches the one required by the cloud network
     * attachment and update configuration if necessary.
     */
    Bstr strGatewayVM = BstrFmt("lgw-%ls", gateways.mTargetVM.raw());
    ComPtr<IMachine> machine;
    HRESULT hrc = virtualBox->FindMachine(strGatewayVM.raw(), machine.asOutParam());
    if (SUCCEEDED(hrc))
    {
        hrc = machine->LockMachine(aSession, LockType_Write);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to lock '%ls' for modifications. hrc=%x\n", strGatewayVM.raw(), hrc));
            return hrc;
        }

        hrc = attachToLocalNetwork(aSession, aCloudNetwork);
    }
    else
    {
        SafeArray<IN_BSTR> groups;
        groups.push_back(Bstr("/gateways").mutableRaw());
        hrc = virtualBox->CreateMachine(NULL, Bstr(strGatewayVM).raw(), ComSafeArrayAsInParam(groups), Bstr("Ubuntu_64").raw(), Bstr("").raw(), machine.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to create '%ls'. hrc=%x\n", strGatewayVM.raw(), hrc));
            return hrc;
        }
        /* Initial configuration */
        hrc = machine->ApplyDefaults(NULL);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to apply defaults to '%ls'. hrc=%x\n", strGatewayVM.raw(), hrc));
            return hrc;
        }

        /* Add second network adapter */
        ComPtr<INetworkAdapter> networkAdapter;
        hrc = machine->GetNetworkAdapter(1, networkAdapter.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to locate the second network adapter. hrc=%x\n", hrc));
            return hrc;
        }

        hrc = networkAdapter->COMSETTER(AttachmentType)(NetworkAttachmentType_Internal);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to set attachment type for the second network adapter. hrc=%x\n", hrc));
            return hrc;
        }

        BstrFmt network("cloud-%s", aCloudNetwork.c_str());
        hrc = networkAdapter->COMSETTER(InternalNetwork)(network.raw());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to set network name for the second network adapter. hrc=%x\n", hrc));
            return hrc;
        }

        hrc = networkAdapter->COMSETTER(PromiscModePolicy)(NetworkAdapterPromiscModePolicy_AllowAll);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to set promiscuous mode policy for the second network adapter. hrc=%x\n", hrc));
            return hrc;
        }

        hrc = networkAdapter->COMSETTER(Enabled)(TRUE);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to enable the second network adapter. hrc=%x\n", hrc));
            return hrc;
        }

        /* No need for audio -- disable it. */
        ComPtr<IAudioAdapter> audioAdapter;
        hrc = machine->GetNetworkAdapter(1, networkAdapter.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to locate the second network adapter. hrc=%x\n", hrc));
            return hrc;
        }

        hrc = machine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to set attachment type for the second network adapter. hrc=%x\n", hrc));
            return hrc;
        }

        hrc = audioAdapter->COMSETTER(Enabled)(FALSE);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to disable the audio adapter. hrc=%x\n", hrc));
            return hrc;
        }

        /** @todo Disable USB? */

        /* Register the local gateway VM */
        hrc = virtualBox->RegisterMachine(machine);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to register '%ls'. hrc=%x\n", strGatewayVM.raw(), hrc));
            return hrc;
        }

        /*
        * Storage can only be attached to registered VMs which means we need to use session
        * to lock VM in order to make it mutable again.
        */
        ComPtr<ISystemProperties> systemProperties;
        ComPtr<IMedium> hd;
        Bstr defaultMachineFolder;
        hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to obtain system properties. hrc=%x\n", hrc));
            return hrc;
        }
        hrc = systemProperties->COMGETTER(DefaultMachineFolder)(defaultMachineFolder.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to obtain default machine folder. hrc=%x\n", hrc));
            return hrc;
        }
        hrc = virtualBox->OpenMedium(BstrFmt("%ls\\gateways\\lgw.vdi", defaultMachineFolder.raw()).raw(), DeviceType_HardDisk, AccessMode_ReadWrite, FALSE, hd.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to open medium for '%ls'. hrc=%x\n", strGatewayVM.raw(), hrc));
            return hrc;
        }

        hrc = machine->LockMachine(aSession, LockType_Write);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to lock '%ls' for modifications. hrc=%x\n", strGatewayVM.raw(), hrc));
            return hrc;
        }

        ComPtr<IMachine> sessionMachine;
        hrc = aSession->COMGETTER(Machine)(sessionMachine.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to obtain a mutable machine. hrc=%x\n", hrc));
            return hrc;
        }

        hrc = sessionMachine->AttachDevice(Bstr("SATA").raw(), 0, 0, DeviceType_HardDisk, hd);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to attach HD to '%ls'. hrc=%x\n", strGatewayVM.raw(), hrc));
            return hrc;
        }

        /* Save settings */
        hrc = sessionMachine->SaveSettings();
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to save '%ls' settings. hrc=%x\n", strGatewayVM.raw(), hrc));
            return hrc;
        }
    }
    /* Unlock the machine before start, it will be re-locked by LaunchVMProcess */
    aSession->UnlockMachine();

#ifdef DEBUG
 #define LGW_FRONTEND "gui"
#else
 #define LGW_FRONTEND "headless"
#endif
    ComPtr<IProgress> progress;
    hrc = machine->LaunchVMProcess(aSession, Bstr(LGW_FRONTEND).raw(), ComSafeArrayNullInParam(), progress.asOutParam());
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to launch '%ls'. hrc=%x\n", strGatewayVM.raw(), hrc));
        return hrc;
    }

    hrc = progress->WaitForCompletion(-1);
    if (FAILED(hrc))
        LogRel(("CLOUD-NET: Failed to launch '%ls'. hrc=%x\n", strGatewayVM.raw(), hrc));

    gateways.mGatewayVM = strGatewayVM;

    ComPtr<IEventSource> es;
    hrc = virtualBox->COMGETTER(EventSource)(es.asOutParam());
    ComPtr<IEventListener> listener;
    hrc = es->CreateListener(listener.asOutParam());
    com::SafeArray <VBoxEventType_T> eventTypes(1);
    eventTypes.push_back(VBoxEventType_OnGuestPropertyChanged);
    hrc = es->RegisterListener(listener, ComSafeArrayAsInParam(eventTypes), false);

    Bstr publicKey;
    Bstr aMachStrGuid;
    machine->COMGETTER(Id)(aMachStrGuid.asOutParam());
    Guid aMachGuid(aMachStrGuid);

    uint64_t u64Started = RTTimeMilliTS();
    do
    {
        ComPtr<IEvent> ev;
        hrc = es->GetEvent(listener, 1000 /* seconds */, ev.asOutParam());
        if (ev)
        {
            VBoxEventType_T aType;
            hrc = ev->COMGETTER(Type)(&aType);
            if (aType == VBoxEventType_OnGuestPropertyChanged)
            {
                ComPtr<IGuestPropertyChangedEvent> gpcev = ev;
                Assert(gpcev);
                Bstr aNextStrGuid;
                gpcev->COMGETTER(MachineId)(aNextStrGuid.asOutParam());
                if (aMachGuid != Guid(aNextStrGuid))
                    continue;
                Bstr aNextName;
                gpcev->COMGETTER(Name)(aNextName.asOutParam());
                if (aNextName == "/VirtualBox/Gateway/PublicKey")
                {
                    gpcev->COMGETTER(Value)(publicKey.asOutParam());
                    LogRel(("CLOUD-NET: Got public key from local gateway '%ls'\n", publicKey.raw()));
                    break;
                }
            }

        }
    } while (RTTimeMilliTS() - u64Started < 300 * 1000); /** @todo reasonable timeout */

    if (publicKey.isEmpty())
    {
        LogRel(("CLOUD-NET: Failed to get ssh public key from '%ls'\n", strGatewayVM.raw()));
        return E_FAIL;
    }

    gateways.mPublicSshKey = publicKey;

    return hrc;
}

static bool getProxyForIpAddr(ComPtr<IVirtualBox> virtualBox, const com::Utf8Str &strIpAddr, Bstr &strProxyType, Bstr &strProxyHost, Bstr &strProxyPort)
{
#ifndef VBOX_WITH_PROXY_INFO
    RT_NOREF(virtualBox, strIpAddr, strProxyType, strProxyHost, strProxyPort);
    LogRel(("CLOUD-NET: Proxy support is disabled. Using direct connection.\n"));
    return false;
#else /* VBOX_WITH_PROXY_INFO */
    ComPtr<ISystemProperties> systemProperties;
    ProxyMode_T enmProxyMode;
    HRESULT hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to obtain system properties. hrc=%x\n", hrc));
        return false;
    }
    hrc = systemProperties->COMGETTER(ProxyMode)(&enmProxyMode);
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to obtain default machine folder. hrc=%x\n", hrc));
        return false;
    }
    if (enmProxyMode == ProxyMode_NoProxy)
        return false;

    Bstr proxyUrl;
    if (enmProxyMode == ProxyMode_Manual)
    {
        hrc = systemProperties->COMGETTER(ProxyURL)(proxyUrl.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to obtain proxy URL. hrc=%x\n", hrc));
            return false;
        }
        Utf8Str strProxyUrl = proxyUrl;
        if (!strProxyUrl.contains("://"))
            strProxyUrl = "http://" + strProxyUrl;
        const char *pcszProxyUrl = strProxyUrl.c_str();
        RTURIPARSED Parsed;
        int rc = RTUriParse(pcszProxyUrl, &Parsed);
        if (RT_FAILURE(rc))
        {
            LogRel(("CLOUD-NET: Failed to parse proxy URL: %ls (rc=%d)\n", proxyUrl.raw(), rc));
            return false;
        }
        char *pszHost = RTUriParsedAuthorityHost(pcszProxyUrl, &Parsed);
        if (!pszHost)
        {
            LogRel(("CLOUD-NET: Failed to get proxy host name from proxy URL: %s\n", pcszProxyUrl));
            return false;
        }
        strProxyHost = pszHost;
        RTStrFree(pszHost);
        char *pszScheme = RTUriParsedScheme(pcszProxyUrl, &Parsed);
        if (!pszScheme)
        {
            LogRel(("CLOUD-NET: Failed to get proxy scheme from proxy URL: %s\n", pcszProxyUrl));
            return false;
        }
        strProxyType = Utf8Str(pszScheme).toUpper();
        RTStrFree(pszScheme);
        uint32_t uProxyPort  = RTUriParsedAuthorityPort(pcszProxyUrl, &Parsed);
        if (uProxyPort == UINT32_MAX)
        if (!pszScheme)
        {
            LogRel(("CLOUD-NET: Failed to get proxy port from proxy URL: %s\n", pcszProxyUrl));
            return false;
        }
        strProxyPort = BstrFmt("%d", uProxyPort);
    }
    else
    {
        /* Attempt to use system proxy settings (ProxyMode_System) */
        RTHTTP hHttp;
        int rc = RTHttpCreate(&hHttp);
        if (RT_FAILURE(rc))
        {
            LogRel(("CLOUD-NET: Failed to create HTTP context (rc=%d)\n", rc));
            return false;
        }
        rc = RTHttpUseSystemProxySettings(hHttp);
        if (RT_FAILURE(rc))
        {
            LogRel(("CLOUD-NET: Failed to use system proxy (rc=%d)\n", rc));
            RTHttpDestroy(hHttp);
            return false;
        }

        RTHTTPPROXYINFO proxy;
        RT_ZERO(proxy);
        rc = RTHttpGetProxyInfoForUrl(hHttp, ("http://" + strIpAddr).c_str(), &proxy);
        if (RT_FAILURE(rc))
        {
            LogRel(("CLOUD-NET: Failed to get proxy for %s (rc=%d)\n", strIpAddr.c_str(), rc));
            RTHttpDestroy(hHttp);
            return false;
        }
        switch (proxy.enmProxyType)
        {
            case RTHTTPPROXYTYPE_HTTP:
                strProxyType = "HTTP";
                break;
            case RTHTTPPROXYTYPE_HTTPS:
                strProxyType = "HTTPS";
                break;
            case RTHTTPPROXYTYPE_SOCKS4:
                strProxyType = "SOCKS4";
                break;
            case RTHTTPPROXYTYPE_SOCKS5:
                strProxyType = "SOCKS5";
                break;
            case RTHTTPPROXYTYPE_UNKNOWN:
                LogRel(("CLOUD-NET: Unknown proxy type."));
                break;
        }
        strProxyHost = proxy.pszProxyHost;
        strProxyPort = BstrFmt("%d", proxy.uProxyPort);
        RTHttpFreeProxyInfo(&proxy);
        RTHttpDestroy(hHttp);
    }
    return true;
#endif /* VBOX_WITH_PROXY_INFO */
}


static HRESULT exchangeInfoBetweenGateways(ComPtr<IVirtualBox> virtualBox, ComPtr<ISession> aSession, GatewayInfo& gateways)
{
    RT_NOREF(virtualBox);
    HRESULT hrc = S_OK;
    do
    {
        ComPtr<IConsole> console;
        hrc = aSession->COMGETTER(Console)(console.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to obtain console for 'lgw'. hrc=%x\n", hrc));
            break;
        }

        ComPtr<IGuest> guest;
        hrc = console->COMGETTER(Guest)(guest.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to obtain guest for 'lgw'. hrc=%x\n", hrc));
            break;
        }

        ComPtr<IGuestSession> guestSession;

        GuestSessionWaitResult_T enmWaitResult = GuestSessionWaitResult_None;
        for (int cTriesLeft = 6; cTriesLeft > 0; cTriesLeft--)
        {
            RTThreadSleep(5000 /* ms */);
            hrc = guest->CreateSession(Bstr("vbox").raw(), Bstr("vbox").raw(), NULL, Bstr("Cloud Gateway Impersonation").raw(), guestSession.asOutParam());
            if (FAILED(hrc))
            {
                LogRel(("CLOUD-NET: Failed to create guest session for 'lgw'%s. hrc=%x\n", cTriesLeft > 1 ? ", will re-try" : "", hrc));
                continue;
            }
            hrc = guestSession->WaitFor(GuestSessionWaitForFlag_Start, 30 * 1000, &enmWaitResult);
            if (FAILED(hrc))
            {
                LogRel(("CLOUD-NET: WARNING! Failed to wait in guest session for 'lgw'%s. waitResult=%x hrc=%x\n",
                        cTriesLeft > 1 ? ", will re-try" : "", enmWaitResult, hrc));
                guestSession->Close();
                guestSession.setNull();
                continue;
            }
            if (enmWaitResult == GuestSessionWaitResult_Start)
                break;
            LogRel(("CLOUD-NET: WARNING! 'lgw' guest session waitResult=%x%s\n",
                    enmWaitResult, cTriesLeft > 1 ? ", will re-try" : ""));
            guestSession->Close();
            guestSession.setNull();
        }

        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to start guest session for 'lgw'. waitResult=%x hrc=%x\n", enmWaitResult, hrc));
            break;
        }

        GuestSessionStatus_T enmSessionStatus;
        hrc = guestSession->COMGETTER(Status)(&enmSessionStatus);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to get guest session status for 'lgw'. hrc=%x\n", hrc));
            break;
        }
        LogRel(("CLOUD-NET: Session status: %d\n", enmSessionStatus));

        Bstr strPrimaryProxyType;
        Bstr strPrimaryProxyHost;
        Bstr strPrimaryProxyPort;
        Bstr strSecondaryProxyType;
        Bstr strSecondaryProxyHost;
        Bstr strSecondaryProxyPort;

        ComPtr<IGuestProcess> guestProcess;
        com::SafeArray<IN_BSTR> aArgs;
        com::SafeArray<IN_BSTR> aEnv;
        com::SafeArray<ProcessCreateFlag_T> aCreateFlags;
        com::SafeArray<ProcessWaitForFlag_T> aWaitFlags;
        aCreateFlags.push_back(ProcessCreateFlag_WaitForStdOut);
        aCreateFlags.push_back(ProcessCreateFlag_WaitForStdErr);
#define GUEST_CMD "/bin/sh"
        aArgs.push_back(Bstr(GUEST_CMD).mutableRaw());
        aArgs.push_back(Bstr("-x").mutableRaw());
        aArgs.push_back(Bstr("/home/vbox/local-bridge.sh").mutableRaw());
        aArgs.push_back(Bstr(gateways.mCloudPublicIp).mutableRaw());
        aArgs.push_back(Bstr(gateways.mCloudSecondaryPublicIp).mutableRaw());
        aArgs.push_back(Bstr(gateways.getLocalMacAddressWithColons()).mutableRaw());
        if (getProxyForIpAddr(virtualBox, gateways.mCloudPublicIp, strPrimaryProxyType, strPrimaryProxyHost, strPrimaryProxyPort))
        {
            aArgs.push_back(strPrimaryProxyType.mutableRaw());
            aArgs.push_back(strPrimaryProxyHost.mutableRaw());
            aArgs.push_back(strPrimaryProxyPort.mutableRaw());
            if (getProxyForIpAddr(virtualBox, gateways.mCloudSecondaryPublicIp, strSecondaryProxyType, strSecondaryProxyHost, strSecondaryProxyPort))
            {
                aArgs.push_back(strSecondaryProxyType.mutableRaw());
                aArgs.push_back(strSecondaryProxyHost.mutableRaw());
                aArgs.push_back(strSecondaryProxyPort.mutableRaw());
            }
        }
        hrc = guestSession->ProcessCreate(Bstr(GUEST_CMD).raw(),
                                        ComSafeArrayAsInParam(aArgs),
                                        ComSafeArrayAsInParam(aEnv),
                                        ComSafeArrayAsInParam(aCreateFlags),
                                        180 * 1000 /* ms */,
                                        guestProcess.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to create guest process '/bin/sh' for 'lgw'. hrc=%x\n", hrc));
            break;
        }

        ProcessWaitResult_T waitResult;
        hrc = guestProcess->WaitFor(ProcessWaitForFlag_Start, 10 * 1000 /* ms */, &waitResult);
        if (FAILED(hrc) || (waitResult != ProcessWaitResult_Start))
        {
            LogRel(("CLOUD-NET: Failed to wait for guest process to start for 'lgw'. waitResult=%x hrc=%x\n",
                    waitResult, hrc));
            break;
        }
        LogRel(("CLOUD-NET: waitResult=%x\n", waitResult));

        uint32_t cNotSupported = 0;
        bool fRead, fDone = false;
        uint64_t u64Start = RTTimeMilliTS();
        do
        {
            /** @todo wait for stdout when it becomes supported! */
            hrc = guestProcess->WaitFor(ProcessWaitForFlag_Terminate | ProcessWaitForFlag_StdOut, 1000 /* ms */, &waitResult);
            if (FAILED(hrc))
            {
                LogRel(("CLOUD-NET: Failed to get output from guest process for 'lgw'. waitResult=%x hrc=%x\n",
                        waitResult, hrc));
                break;
            }
            if (waitResult == ProcessWaitResult_WaitFlagNotSupported)
                ++cNotSupported;
            else
            {
                if (cNotSupported)
                {
                    LogRel(("CLOUD-NET: waitResult=9, repeated %u times\n", cNotSupported));
                    cNotSupported = 0;
                }
                LogRel(("CLOUD-NET: waitResult=%x\n", waitResult));
            }

            fRead = false;
            switch (waitResult)
            {
                case ProcessWaitResult_WaitFlagNotSupported:
                    RTThreadYield();
                    /* Fall through */
                case ProcessWaitResult_StdOut:
                    fRead = true;
                    break;
                case ProcessWaitResult_Terminate:
                    fDone = true;
                    break;
                case ProcessWaitResult_Timeout:
                {
                    ProcessStatus_T enmProcStatus;
                    hrc = guestProcess->COMGETTER(Status)(&enmProcStatus);
                    if (FAILED(hrc))
                    {
                        LogRel(("CLOUD-NET: Failed to query guest process status for 'lgw'. hrc=%x\n", hrc));
                        fDone = true;
                    }
                    else
                    {
                        LogRel(("CLOUD-NET: Guest process timeout for 'lgw'. status=%d\n", enmProcStatus));
                        if (   enmProcStatus == ProcessStatus_TimedOutKilled
                            || enmProcStatus == ProcessStatus_TimedOutAbnormally)
                        fDone = true;
                    }
                    fRead = true;
                    break;
                }
                default:
                    LogRel(("CLOUD-NET: Unexpected waitResult=%x\n", waitResult));
                    break;
            }

            if (fRead)
            {
                SafeArray<BYTE> aStdOutData, aStdErrData;
                hrc = guestProcess->Read(1 /* StdOut */, _64K, 60 * 1000 /* ms */, ComSafeArrayAsOutParam(aStdOutData));
                if (FAILED(hrc))
                {
                    LogRel(("CLOUD-NET: Failed to read stdout from guest process for 'lgw'. hrc=%x\n", hrc));
                    break;
                }
                hrc = guestProcess->Read(2 /* StdErr */, _64K, 60 * 1000 /* ms */, ComSafeArrayAsOutParam(aStdErrData));
                if (FAILED(hrc))
                {
                    LogRel(("CLOUD-NET: Failed to read stderr from guest process for 'lgw'. hrc=%x\n", hrc));
                    break;
                }

                size_t cbStdOutData = aStdOutData.size();
                size_t cbStdErrData = aStdErrData.size();
                if (cbStdOutData == 0 && cbStdErrData == 0)
                {
                    //LogRel(("CLOUD-NET: Empty output from guest process for 'lgw'. hrc=%x\n", hrc));
                    continue;
                }

                if (cNotSupported)
                {
                    LogRel(("CLOUD-NET: waitResult=9, repeated %u times\n", cNotSupported));
                    cNotSupported = 0;
                }
                if (cbStdOutData)
                    LogRel(("CLOUD-NET: Got stdout from 'lgw':\n%.*s", aStdOutData.size(), aStdOutData.raw()));
                if (cbStdErrData)
                    LogRel(("CLOUD-NET: Got stderr from 'lgw':\n%.*s", aStdErrData.size(), aStdErrData.raw()));
            }
        }
        while (!fDone && RTTimeMilliTS() - u64Start < 180 * 1000 /* ms */);

    } while (false);

    return hrc;
}


HRESULT destroyLocalGateway(ComPtr<IVirtualBox> virtualBox, const GatewayInfo& gateways)
{
    if (gateways.mGatewayVM.isEmpty())
        return S_OK;

    LogRel(("CLOUD-NET: Shutting down local gateway '%s'...\n", gateways.mGatewayVM.c_str()));

    ComPtr<IMachine> localGateway;
    HRESULT hrc = virtualBox->FindMachine(Bstr(gateways.mGatewayVM).raw(), localGateway.asOutParam());
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to locate '%s'. hrc=%x\n", gateways.mGatewayVM.c_str(), hrc));
        return hrc;
    }

    MachineState_T tmp;
    hrc = localGateway->COMGETTER(State)(&tmp);
    if (tmp == MachineState_Running)
    {
        /* If the gateway VM is running we need to stop it */
        ComPtr<ISession> session;
        hrc = session.createInprocObject(CLSID_Session);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to create a session. hrc=%x\n", hrc));
            return hrc;
        }

        hrc = localGateway->LockMachine(session, LockType_Shared);
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to lock '%s' for control. hrc=%x\n", gateways.mGatewayVM.c_str(), hrc));
            return hrc;
        }

        ComPtr<IConsole> console;
        hrc = session->COMGETTER(Console)(console.asOutParam());
        if (FAILED(hrc))
        {
            LogRel(("CLOUD-NET: Failed to obtain console for '%s'. hrc=%x\n", gateways.mGatewayVM.c_str(), hrc));
            return hrc;
        }

        ComPtr<IProgress> progress;
        console->PowerDown(progress.asOutParam()); /* We assume the gateway disk to be immutable! */

#if 0
        hrc = progress->WaitForCompletion(-1);
        if (FAILED(hrc))
            LogRel(("CLOUD-NET: Failed to stop '%s'. hrc=%x\n", gateways.mGatewayVM.c_str(), hrc));
#endif
        session->UnlockMachine();
    }
#if 0
    /*
     * Unfortunately we cannot unregister a machine we've just powered down and unlocked.
     * It takes some time for the machine to unlock completely.
     */
    /** @todo Removal of VM should probably be optional in the future. */
    SafeIfaceArray<IMedium> media;
    hrc = pLocalGateway->Unregister(CleanupMode_DetachAllReturnHardDisksOnly, ComSafeArrayAsOutParam(media));
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to unregister '%s'. hrc=%x\n", gateways.mGatewayVM.c_str(), hrc));
        return hrc;
    }
    ComPtr<IProgress> removalProgress;
    hrc = pLocalGateway->DeleteConfig(ComSafeArrayAsInParam(media), removalProgress.asOutParam());
    if (FAILED(hrc))
    {
        LogRel(("CLOUD-NET: Failed to delete machine files for '%s'. hrc=%x\n", gateways.mGatewayVM.c_str(), hrc));
    }
#endif
    return hrc;
}

static HRESULT terminateCloudGateway(ComPtr<IVirtualBox> virtualBox, const GatewayInfo& gateways)
{
    if (gateways.mGatewayInstanceId.isEmpty())
        return S_OK;

    LogRel(("CLOUD-NET: Terminating cloud gateway instance '%s'...\n", gateways.mGatewayInstanceId.c_str()));

    HRESULT hrc = S_OK;
    try {
        CloudClient client(virtualBox, gateways.mCloudProvider, gateways.mCloudProfile);
        client.stopCloudGateway(gateways);
    }
    catch (CloudError e)
    {
        hrc = e.getRc();
        LogRel(("CLOUD-NET: Failed to terminate cloud gateway instance (rc=%x).\n", hrc));
    }
    return hrc;
}

HRESULT startGateways(ComPtr<IVirtualBox> virtualBox, ComPtr<ICloudNetwork> network, GatewayInfo& gateways)
{
    /* Session used to launch and control the local gateway VM */
    ComPtr<ISession> session;
    Bstr strNetwork;
    HRESULT hrc = session.createInprocObject(CLSID_Session);
    if (FAILED(hrc))
        LogRel(("CLOUD-NET: Failed to create a session. hrc=%x\n", hrc));
    else
        hrc = network->COMGETTER(NetworkName)(strNetwork.asOutParam());
    if (SUCCEEDED(hrc))
        hrc = startLocalGateway(virtualBox, session, strNetwork, gateways);
    if (SUCCEEDED(hrc))
        hrc = startCloudGateway(virtualBox, network, gateways);
    if (SUCCEEDED(hrc))
        hrc = exchangeInfoBetweenGateways(virtualBox, session, gateways);

    session->UnlockMachine();
    /** @todo Destroy gateways if unsuccessful (cleanup) */

    return hrc;
}

HRESULT stopGateways(ComPtr<IVirtualBox> virtualBox, const GatewayInfo& gateways)
{
    HRESULT hrc = destroyLocalGateway(virtualBox, gateways);
    AssertComRC(hrc);
    hrc = terminateCloudGateway(virtualBox, gateways);
    AssertComRC(hrc);
    return hrc;
}
