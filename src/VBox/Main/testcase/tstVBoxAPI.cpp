/* $Id$ */
/** @file
 * tstVBoxAPI - Checks VirtualBox API.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/sup.h>

#include <iprt/test.h>
#include <iprt/time.h>

using namespace com;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTTEST g_hTest;


/** Worker for TST_COM_EXPR(). */
static HRESULT tstComExpr(HRESULT hrc, const char *pszOperation, int iLine)
{
    if (FAILED(hrc))
        RTTestFailed(g_hTest, "%s failed on line %u with hrc=%Rhrc", pszOperation, iLine, hrc);
    return hrc;
}

/** Macro that executes the given expression and report any failure.
 *  The expression must return a HRESULT. */
#define TST_COM_EXPR(expr) tstComExpr(expr, #expr, __LINE__)



static void tstApiIVirtualBox(IVirtualBox *pVBox)
{
    HRESULT hrc;
    Bstr bstrTmp;
    ULONG ulTmp;

    RTTestSub(g_hTest, "IVirtualBox::version");
    hrc = pVBox->COMGETTER(Version)(bstrTmp.asOutParam());
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::version");
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::version failed with return value %Rhrc.", __LINE__, hrc);

    RTTestSub(g_hTest, "IVirtualBox::versionNormalized");
    hrc = pVBox->COMGETTER(VersionNormalized)(bstrTmp.asOutParam());
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::versionNormalized");
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::versionNormalized failed with return value %Rhrc.", __LINE__, hrc);

    RTTestSub(g_hTest, "IVirtualBox::revision");
    hrc = pVBox->COMGETTER(Revision)(&ulTmp);
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::revision");
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::revision failed with return value %Rhrc.", __LINE__, hrc);

    RTTestSub(g_hTest, "IVirtualBox::packageType");
    hrc = pVBox->COMGETTER(PackageType)(bstrTmp.asOutParam());
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::packageType");
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::packageType failed with return value %Rhrc.", __LINE__, hrc);

    RTTestSub(g_hTest, "IVirtualBox::APIVersion");
    hrc = pVBox->COMGETTER(APIVersion)(bstrTmp.asOutParam());
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::APIVersion");
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::APIVersion failed with return value %Rhrc.", __LINE__, hrc);

    RTTestSub(g_hTest, "IVirtualBox::homeFolder");
    hrc = pVBox->COMGETTER(HomeFolder)(bstrTmp.asOutParam());
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::homeFolder");
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::homeFolder failed with return value %Rhrc.", __LINE__, hrc);

    RTTestSub(g_hTest, "IVirtualBox::settingsFilePath");
    hrc = pVBox->COMGETTER(SettingsFilePath)(bstrTmp.asOutParam());
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::settingsFilePath");
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::settingsFilePath failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeIfaceArray<IGuestOSType> guestOSTypes;
    RTTestSub(g_hTest, "IVirtualBox::guestOSTypes");
    hrc = pVBox->COMGETTER(GuestOSTypes)(ComSafeArrayAsOutParam(guestOSTypes));
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::guestOSTypes");
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::guestOSTypes failed with return value %Rhrc.", __LINE__, hrc);

    /** Create VM */
    RTTestSub(g_hTest, "IVirtualBox::CreateMachine");
    ComPtr<IMachine> ptrMachine;
    Bstr tstMachineName = "TestMachine";
    com::SafeArray<BSTR> groups;
    /** Default VM settings */
    hrc = pVBox->CreateMachine(NULL,                          /** Settings */
                               tstMachineName.raw(),          /** Name */
                               ComSafeArrayAsInParam(groups), /** Groups */
                               NULL,                          /** OS Type */
                               NULL,                          /** Create flags */
                               ptrMachine.asOutParam());      /** Machine */
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::CreateMachine");
    else
    {
        RTTestFailed(g_hTest, "%d: IVirtualBox::CreateMachine failed with return value %Rhrc.", __LINE__, hrc);
        return;
    }

    RTTestSub(g_hTest, "IVirtualBox::RegisterMachine");
    hrc = pVBox->RegisterMachine(ptrMachine);
    if (SUCCEEDED(hrc))
        RTTestPassed(g_hTest, "IVirtualBox::RegisterMachine");
    else
    {
        RTTestFailed(g_hTest, "%d: IVirtualBox::RegisterMachine failed with return value %Rhrc.", __LINE__, hrc);
        return;
    }

    ComPtr<IHost> host;
    RTTestSub(g_hTest, "IVirtualBox::host");
    hrc = pVBox->COMGETTER(Host)(host.asOutParam());
    if (SUCCEEDED(hrc))
    {
        /** @todo Add IHost testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::host");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::host failed with return value %Rhrc.", __LINE__, hrc);

    ComPtr<ISystemProperties> sysprop;
    RTTestSub(g_hTest, "IVirtualBox::systemProperties");
    hrc = pVBox->COMGETTER(SystemProperties)(sysprop.asOutParam());
    if (SUCCEEDED(hrc))
    {
        /** @todo Add ISystemProperties testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::systemProperties");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::systemProperties failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeIfaceArray<IMachine> machines;
    RTTestSub(g_hTest, "IVirtualBox::machines");
    hrc = pVBox->COMGETTER(Machines)(ComSafeArrayAsOutParam(machines));
    if (SUCCEEDED(hrc))
    {
        bool bFound = FALSE;
        for (size_t i = 0; i < machines.size(); ++i)
        {
            if (machines[i])
            {
                Bstr tmpName;
                hrc = machines[i]->COMGETTER(Name)(tmpName.asOutParam());
                if (SUCCEEDED(hrc))
                {
                    if (tmpName == tstMachineName)
                    {
                        bFound = TRUE;
                        break;
                    }
                }
            }
        }

        if (bFound)
            RTTestPassed(g_hTest, "IVirtualBox::machines");
        else
            RTTestFailed(g_hTest, "%d: IVirtualBox::machines failed. No created machine found", __LINE__);
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::machines failed with return value %Rhrc.", __LINE__, hrc);

#if 0 /** Not yet implemented */
    com::SafeIfaceArray<ISharedFolder> sharedFolders;
    RTTestSub(g_hTest, "IVirtualBox::sharedFolders");
    hrc = pVBox->COMGETTER(SharedFolders)(ComSafeArrayAsOutParam(sharedFolders));
    if (SUCCEEDED(hrc))
    {
        /** @todo Add ISharedFolders testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::sharedFolders");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::sharedFolders failed with return value %Rhrc.", __LINE__, hrc);
#endif

    com::SafeIfaceArray<IMedium> hardDisks;
    RTTestSub(g_hTest, "IVirtualBox::hardDisks");
    hrc = pVBox->COMGETTER(HardDisks)(ComSafeArrayAsOutParam(hardDisks));
    if (SUCCEEDED(hrc))
    {
        /** @todo Add hardDisks testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::hardDisks");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::hardDisks failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeIfaceArray<IMedium> DVDImages;
    RTTestSub(g_hTest, "IVirtualBox::DVDImages");
    hrc = pVBox->COMGETTER(DVDImages)(ComSafeArrayAsOutParam(DVDImages));
    if (SUCCEEDED(hrc))
    {
        /** @todo Add DVDImages testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::DVDImages");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::DVDImages failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeIfaceArray<IMedium> floppyImages;
    RTTestSub(g_hTest, "IVirtualBox::floppyImages");
    hrc = pVBox->COMGETTER(FloppyImages)(ComSafeArrayAsOutParam(floppyImages));
    if (SUCCEEDED(hrc))
    {
        /** @todo Add floppyImages testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::floppyImages");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::floppyImages failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeIfaceArray<IProgress> progressOperations;
    RTTestSub(g_hTest, "IVirtualBox::progressOperations");
    hrc = pVBox->COMGETTER(ProgressOperations)(ComSafeArrayAsOutParam(progressOperations));
    if (SUCCEEDED(hrc))
    {
        /** @todo Add IProgress testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::progressOperations");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::progressOperations failed with return value %Rhrc.", __LINE__, hrc);

    ComPtr<IPerformanceCollector> performanceCollector;
    RTTestSub(g_hTest, "IVirtualBox::performanceCollector");
    hrc = pVBox->COMGETTER(PerformanceCollector)(performanceCollector.asOutParam());
    if (SUCCEEDED(hrc))
    {
        /** @todo Add IPerformanceCollector testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::performanceCollector");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::performanceCollector failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeIfaceArray<IDHCPServer> DHCPServers;
    RTTestSub(g_hTest, "IVirtualBox::DHCPServers");
    hrc = pVBox->COMGETTER(DHCPServers)(ComSafeArrayAsOutParam(DHCPServers));
    if (SUCCEEDED(hrc))
    {
        /** @todo Add IDHCPServers testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::DHCPServers");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::DHCPServers failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeIfaceArray<INATNetwork> NATNetworks;
    RTTestSub(g_hTest, "IVirtualBox::NATNetworks");
    hrc = pVBox->COMGETTER(NATNetworks)(ComSafeArrayAsOutParam(NATNetworks));
    if (SUCCEEDED(hrc))
    {
        /** @todo Add INATNetworks testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::NATNetworks");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::NATNetworks failed with return value %Rhrc.", __LINE__, hrc);

    ComPtr<IEventSource> eventSource;
    RTTestSub(g_hTest, "IVirtualBox::eventSource");
    hrc = pVBox->COMGETTER(EventSource)(eventSource.asOutParam());
    if (SUCCEEDED(hrc))
    {
        /** @todo Add IEventSource testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::eventSource");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::eventSource failed with return value %Rhrc.", __LINE__, hrc);

    ComPtr<IExtPackManager> extensionPackManager;
    RTTestSub(g_hTest, "IVirtualBox::extensionPackManager");
    hrc = pVBox->COMGETTER(ExtensionPackManager)(extensionPackManager.asOutParam());
    if (SUCCEEDED(hrc))
    {
        /** @todo Add IExtPackManager testing here. */
        RTTestPassed(g_hTest, "IVirtualBox::extensionPackManager");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::extensionPackManager failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeArray<BSTR> internalNetworks;
    RTTestSub(g_hTest, "IVirtualBox::internalNetworks");
    hrc = pVBox->COMGETTER(InternalNetworks)(ComSafeArrayAsOutParam(internalNetworks));
    if (SUCCEEDED(hrc))
    {
        RTTestPassed(g_hTest, "IVirtualBox::internalNetworks");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::internalNetworks failed with return value %Rhrc.", __LINE__, hrc);

    com::SafeArray<BSTR> genericNetworkDrivers;
    RTTestSub(g_hTest, "IVirtualBox::genericNetworkDrivers");
    hrc = pVBox->COMGETTER(GenericNetworkDrivers)(ComSafeArrayAsOutParam(genericNetworkDrivers));
    if (SUCCEEDED(hrc))
    {
        RTTestPassed(g_hTest, "IVirtualBox::genericNetworkDrivers");
    }
    else
        RTTestFailed(g_hTest, "%d: IVirtualBox::genericNetworkDrivers failed with return value %Rhrc.", __LINE__, hrc);
}



int main(int argc, char **argv)
{
    /*
     * Initialization.
     */
    RTEXITCODE rcExit = RTTestInitAndCreate("tstVBoxAPI", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    SUPR3Init(NULL); /* Better time support. */
    RTTestBanner(g_hTest);

    RTTestSub(g_hTest, "Initializing COM and singletons");
    HRESULT hrc = com::Initialize();
    if (SUCCEEDED(hrc))
    {
        ComPtr<IVirtualBox> ptrVBox;
        hrc = TST_COM_EXPR(ptrVBox.createLocalObject(CLSID_VirtualBox));
        if (SUCCEEDED(hrc))
        {
            ComPtr<ISession> ptrSession;
            hrc = TST_COM_EXPR(ptrSession.createInprocObject(CLSID_Session));
            if (SUCCEEDED(hrc))
            {
                RTTestSubDone(g_hTest);

                /*
                 * Call test functions.
                 */

                /** Test IVirtualBox interface */
                tstApiIVirtualBox(ptrVBox);
            }
        }

        ptrVBox.setNull();
        com::Shutdown();
    }
    else
        RTTestIFailed("com::Initialize failed with hrc=%Rhrc", hrc);
    return RTTestSummaryAndDestroy(g_hTest);
}
