/*
 * webtest.cpp:
 *      demo webservice client in C++. This mimics some of the
 *      functionality of VBoxManage for testing purposes.
 *
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

// gSOAP headers (must come after vbox includes because it checks for conflicting defs)
#include "soapStub.h"

// include generated namespaces table
#include "vboxwebsrv.nsmap"

#include <iostream>
#include <sstream>
#include <string>


/**
 *
 * @param argc
 * @param argv[]
 * @return
 */
int main(int argc, char* argv[])
{
    struct soap soap; // gSOAP runtime environment
    soap_init(&soap); // initialize runtime environment (only once)

    if (argc < 2)
    {
        std::cout <<
               "webtest: VirtualBox webservice testcase.\n"
               "Usage:\n"
               " - IWebsessionManager:\n"
               "   - webtest logon <user> <pass>: IWebsessionManage::logon().\n"
               "   - webtest getsession <vboxref>: IWebsessionManage::getSessionObject().\n"
               " - IVirtualBox:\n"
               "   - webtest version <vboxref>: IVirtualBox::getVersion().\n"
               "   - webtest gethost <vboxref>: IVirtualBox::getHost().\n"
               "   - webtest getmachines <vboxref>: IVirtualBox::getMachines().\n"
               "   - webtest createmachine <vboxref> <baseFolder> <name>: IVirtualBox::createMachine().\n"
               "   - webtest registermachine <vboxref> <machineref>: IVirtualBox::registerMachine().\n"
               " - IHost:\n"
               "   - webtest getdvddrives <hostref>: IHost::getDVDDrives.\n"
               " - IHostDVDDrive:\n"
               "   - webtest getdvdname <dvdref>: IHostDVDDrive::getname.\n"
               " - IMachine:\n"
               "   - webtest getname <machineref>: IMachine::getName().\n"
               "   - webtest getid <machineref>: IMachine::getId().\n"
               "   - webtest getostype <machineref>: IMachine::getGuestOSType().\n"
               "   - webtest savesettings <machineref>: IMachine::saveSettings().\n"
               " - All managed object references:\n"
               "   - webtest getif <ref>: report interface of object.\n"
               "   - webtest release <ref>: IUnknown::Release().\n";
        exit(1);
    }

    const char *pcszArgEndpoint = "localhost:18083";

    const char *pcszMode = argv[1];
    int soaprc = 2;

    if (!strcmp(pcszMode, "logon"))
    {
        if (argc < 4)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IWebsessionManager_USCORElogon req;
            req.username = argv[2];
            req.password = argv[3];
            std::cout << "logon: user = \"" << req.username << "\", pass = \"" << req.password << "\"\n";
            _vbox__IWebsessionManager_USCORElogonResponse resp;

            if (!(soaprc = soap_call___vbox__IWebsessionManager_USCORElogon(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
                std::cout << "VirtualBox objref: \"" << resp.returnval << "\"\n";
        }
    }
    else if (!strcmp(pcszMode, "getsession"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IWebsessionManager_USCOREgetSessionObject req;
            req.refIVirtualBox = argv[2];
            _vbox__IWebsessionManager_USCOREgetSessionObjectResponse resp;

            if (!(soaprc = soap_call___vbox__IWebsessionManager_USCOREgetSessionObject(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
                std::cout << "session: \"" << resp.returnval << "\"\n";
        }
    }
    else if (!strcmp(pcszMode, "version"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IVirtualBox_USCOREgetVersion req;
            req._USCOREthis = argv[2];
            _vbox__IVirtualBox_USCOREgetVersionResponse resp;

            if (!(soaprc = soap_call___vbox__IVirtualBox_USCOREgetVersion(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
                std::cout << "version: \"" << resp.returnval << "\"\n";
        }
    }
    else if (!strcmp(pcszMode, "gethost"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IVirtualBox_USCOREgetHost req;
            req._USCOREthis = argv[2];
            _vbox__IVirtualBox_USCOREgetHostResponse resp;

            if (!(soaprc = soap_call___vbox__IVirtualBox_USCOREgetHost(&soap,
                                                            pcszArgEndpoint,
                                                            NULL,
                                                            &req,
                                                            &resp)))
            {
                std::cout << "Host objref " << resp.returnval << "\n";
            }
        }
    }
    else if (!strcmp(pcszMode, "getmachines"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IVirtualBox_USCOREgetMachines req;
            req._USCOREthis = argv[2];
            _vbox__IVirtualBox_USCOREgetMachinesResponse resp;

            if (!(soaprc = soap_call___vbox__IVirtualBox_USCOREgetMachines(&soap,
                                                                pcszArgEndpoint,
                                                                NULL,
                                                                &req,
                                                                &resp)))
            {
                size_t c = resp.returnval.size();
                for (size_t i = 0;
                     i < c;
                     ++i)
                {
                    std::cout << "Machine " << i << ": objref " << resp.returnval[i] << "\n";
                }
            }
        }
    }
    else if (!strcmp(pcszMode, "createmachine"))
    {
        if (argc < 5)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IVirtualBox_USCOREcreateMachine req;
            req._USCOREthis = argv[2];
            req.baseFolder = argv[3];
            req.name = argv[4];
            std::cout << "createmachine: baseFolder = \"" << req.baseFolder << "\", name = \"" << req.name << "\"\n";
            _vbox__IVirtualBox_USCOREcreateMachineResponse resp;

            if (!(soaprc = soap_call___vbox__IVirtualBox_USCOREcreateMachine(&soap,
                                                                  pcszArgEndpoint,
                                                                  NULL,
                                                                  &req,
                                                                  &resp)))
                std::cout << "Machine created: managed object reference ID is " << resp.returnval << "\n";
        }
    }
    else if (!strcmp(pcszMode, "registermachine"))
    {
        if (argc < 4)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IVirtualBox_USCOREregisterMachine req;
            req._USCOREthis = argv[2];
            req.machine = argv[3];
            _vbox__IVirtualBox_USCOREregisterMachineResponse resp;
            if (!(soaprc = soap_call___vbox__IVirtualBox_USCOREregisterMachine(&soap,
                                                                    pcszArgEndpoint,
                                                                    NULL,
                                                                    &req,
                                                                    &resp)))
                std::cout << "Machine registered.\n";
        }
    }
    else if (!strcmp(pcszMode, "getdvddrives"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IHost_USCOREgetDVDDrives req;
            req._USCOREthis = argv[2];
            _vbox__IHost_USCOREgetDVDDrivesResponse resp;
            if (!(soaprc = soap_call___vbox__IHost_USCOREgetDVDDrives(&soap,
                                                           pcszArgEndpoint,
                                                           NULL,
                                                           &req,
                                                           &resp)))
        {
            size_t c = resp.returnval.size();
            for (size_t i = 0;
                 i < c;
                 ++i)
            {
                std::cout << "DVD drive " << i << ": objref " << resp.returnval[i] << "\n";
            }
        }
        }
    }
    else if (!strcmp(pcszMode, "getdvdname"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IHostDVDDrive_USCOREgetName req;
            req._USCOREthis = argv[2];
            _vbox__IHostDVDDrive_USCOREgetNameResponse resp;
            if (!(soaprc = soap_call___vbox__IHostDVDDrive_USCOREgetName(&soap,
                                                              pcszArgEndpoint,
                                                              NULL,
                                                              &req,
                                                              &resp)))
                std::cout << "Name is: \"" << resp.returnval << "\"\n";
        }
    }
    else if (!strcmp(pcszMode, "getname"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IMachine_USCOREgetName req;
            req._USCOREthis = argv[2];
            _vbox__IMachine_USCOREgetNameResponse resp;
            if (!(soaprc = soap_call___vbox__IMachine_USCOREgetName(&soap,
                                                         pcszArgEndpoint,
                                                         NULL,
                                                         &req,
                                                         &resp)))
                printf("Name is: %s\n", resp.returnval.c_str());
        }
    }
    else if (!strcmp(pcszMode, "getid"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IMachine_USCOREgetId req;
            req._USCOREthis = argv[2];
            _vbox__IMachine_USCOREgetIdResponse resp;
            if (!(soaprc = soap_call___vbox__IMachine_USCOREgetId(&soap,
                                                       pcszArgEndpoint,
                                                       NULL,
                                                       &req,
                                                       &resp)))
                std::cout << "UUID is: " << resp.returnval << "\n";;
        }
    }
    else if (!strcmp(pcszMode, "getostypeid"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IMachine_USCOREgetOSTypeId req;
            req._USCOREthis = argv[2];
            _vbox__IMachine_USCOREgetOSTypeIdResponse resp;
            if (!(soaprc = soap_call___vbox__IMachine_USCOREgetOSTypeId(&soap,
                                                             pcszArgEndpoint,
                                                             NULL,
                                                             &req,
                                                             &resp)))
                std::cout << "Guest OS type is: " << resp.returnval << "\n";
        }
    }
    else if (!strcmp(pcszMode, "savesettings"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IMachine_USCOREsaveSettings req;
            req._USCOREthis = argv[2];
            _vbox__IMachine_USCOREsaveSettingsResponse resp;
            if (!(soaprc = soap_call___vbox__IMachine_USCOREsaveSettings(&soap,
                                                              pcszArgEndpoint,
                                                              NULL,
                                                              &req,
                                                              &resp)))
                std::cout << "Settings saved\n";
        }
    }
    else if (!strcmp(pcszMode, "release"))
    {
        if (argc < 3)
            std::cout << "Not enough arguments for \"" << pcszMode << "\" mode.\n";
        else
        {
            _vbox__IManagedObjectRef_USCORErelease req;
            req._USCOREthis = argv[2];
            _vbox__IManagedObjectRef_USCOREreleaseResponse resp;
            if (!(soaprc = soap_call___vbox__IManagedObjectRef_USCORErelease(&soap,
                                                                  pcszArgEndpoint,
                                                                  NULL,
                                                                  &req,
                                                                  &resp)))
                std::cout << "Managed object reference " << req._USCOREthis << " released.\n";
        }
    }
    else
        std::cout << "Unknown mode parameter \"" << pcszMode << "\".\n";

    if (soaprc)
    {
        if (    (soap.fault)
             && (soap.fault->detail)
           )
        {
            if (soap.fault->detail->vbox__InvalidObjectFault)
            {
                std::cout << "Bad object ID: " << soap.fault->detail->vbox__InvalidObjectFault->badObjectID << "\n";
            }
            if (soap.fault->detail->vbox__RuntimeFault)
            {
                std::cout << "Result code:   0x" << std::hex << soap.fault->detail->vbox__RuntimeFault->resultCode << "\n";
                std::cout << "Text:          " << std::hex << soap.fault->detail->vbox__RuntimeFault->text << "\n";
                std::cout << "Component:     " << std::hex << soap.fault->detail->vbox__RuntimeFault->component << "\n";
                std::cout << "Interface ID:  " << std::hex << soap.fault->detail->vbox__RuntimeFault->interfaceID << "\n";
            }
        }
        else
        {
            std::cerr << "Invalid fault data, fault message:\n";
            soap_print_fault(&soap, stderr); // display the SOAP fault message on the stderr stream
        }
    }

    soap_destroy(&soap); // delete deserialized class instances (for C++ only)
    soap_end(&soap); // remove deserialized data and clean up
    soap_done(&soap); // detach the gSOAP environment

    return soaprc;
}

