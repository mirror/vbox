/** @file
 *
 * VirtualBox Remote USB backend
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_REMOTEUSBBACKEND
#define ____H_REMOTEUSBBACKEND

#include "RemoteUSBDeviceImpl.h"

#include <VBox/vrdpapi.h>
#include <VBox/vrdpusb.h>

typedef enum
{
    RDLIdle = 0,
    RDLReqSent,
    RDLObtained
} RDLState;

class Console;
class ConsoleVRDPServer;

#ifdef VRDP_MC

/* How many remote devices can be attached to a remote client. 
 * Normally a client computer has 2-8 physical USB ports, so 16 devices
 * should be usually enough.
 */
#define VRDP_MAX_USB_DEVICES_PER_CLIENT (16)

class RemoteUSBBackendListable
{
    public:
        RemoteUSBBackendListable *pNext;
        RemoteUSBBackendListable *pPrev;
        
        RemoteUSBBackendListable() : pNext (NULL), pPrev (NULL) {};
};
#endif /* VRDP_MC */

#ifdef VRDP_MC
class RemoteUSBBackend: public RemoteUSBBackendListable
#else
class RemoteUSBBackend
#endif /* VRDP_MC */
{
    public:

#ifdef VRDP_MC
        RemoteUSBBackend(Console *console, ConsoleVRDPServer *server, uint32_t u32ClientId);
        ~RemoteUSBBackend();
        
        uint32_t ClientId (void) { return mu32ClientId; }
        
        void AddRef (void);
        void Release (void);
        
        void QueryVRDPCallbackPointer (PFNVRDPUSBCALLBACK *ppfn, void **ppv);
        
        REMOTEUSBCALLBACK *GetBackendCallbackPointer (void) { return &mCallback; }
        
        void NotifyDelete (void);
        
        void PollRemoteDevices (void);
#else
        RemoteUSBBackend(Console *console, ConsoleVRDPServer *server);
        ~RemoteUSBBackend();

        int InterceptUSB (PFNVRDPUSBCALLBACK *ppfn, void **ppv);
        void ReleaseUSB (void);

        REMOTEUSBCALLBACK *GetRemoteBackendCallback (void) { return &mCallback; };
#endif /* VRDP_MC */

    public: /* Functions for internal use. */

        ConsoleVRDPServer *VRDPServer (void) { return mServer; };

#ifdef VRDP_MC
        bool pollingEnabledURB (void) { return mfPollURB; }

        int saveDeviceList (const void *pvList, uint32_t cbList);
        
        int negotiateResponse (const VRDPUSBREQNEGOTIATERET *pret);

        int reapURB (const void *pvBody, uint32_t cbBody);

        void request (void);
        void release (void);

        PREMOTEUSBDEVICE deviceFromId (VRDPUSBDEVID id);

        void addDevice (PREMOTEUSBDEVICE pDevice);
        void removeDevice (PREMOTEUSBDEVICE pDevice);
        
        bool addUUID (const Guid *pUuid);
        bool findUUID (const Guid *pUuid);
        void removeUUID (const Guid *pUuid);
#else
        bool pollingEnabledURB (void) { return mfPollURB; };

        void notifyThreadStarted (RTTHREAD self);
        void notifyThreadFinished (void);
        bool threadEnabled (void);
        bool continueThread (void);

        bool needRDL (void);
        void notifyRDLSent (void);

        int negotiateResponse (VRDPUSBREQNEGOTIATERET *pret);

        int saveDeviceList (void *pvList, uint32_t cbList);
        bool processRDL (void);

        void request (void);
        void release (void);

        void waitEvent (unsigned cMillies);

        void addDevice (PREMOTEUSBDEVICE pDevice);
        void removeDevice (PREMOTEUSBDEVICE pDevice);

        PREMOTEUSBDEVICE deviceFromId (VRDPUSBDEVID id);

        int reapURB (void *pvBody, uint32_t cbBody);
#endif /* VRDP_MC */

    private:

#ifndef VRDP_MC
        void initMembers (void);
#endif /* !VRDP_MC */

        Console *mConsole;
        ConsoleVRDPServer *mServer;

#ifdef VRDP_MC
        int cRefs;
        
        uint32_t mu32ClientId;
        
        RTCRITSECT mCritsect;
        
        REMOTEUSBCALLBACK mCallback;
        
        bool mfHasDeviceList;
        
        void *mpvDeviceList;
        uint32_t mcbDeviceList;
        
        typedef enum {
            PollRemoteDevicesStatus_Negotiate,
            PollRemoteDevicesStatus_WaitNegotiateResponse,
            PollRemoteDevicesStatus_SendRequest,
            PollRemoteDevicesStatus_WaitResponse,
            PollRemoteDevicesStatus_Dereferenced
        } PollRemoteDevicesStatus;
        
        PollRemoteDevicesStatus menmPollRemoteDevicesStatus;

        bool mfPollURB;
        
        PREMOTEUSBDEVICE mpDevices;
        
        bool mfWillBeDeleted;
        
        Guid aGuids[VRDP_MAX_USB_DEVICES_PER_CLIENT];
#else
        RTCRITSECT mCritsect;
        RTSEMEVENT mEvent;

        REMOTEUSBCALLBACK mCallback;

        RTTHREAD mThread;

        bool mfThreadActive;
        bool mfThreadEnabled;
        bool mfTerminateThread;

        RDLState menmRDLState;

        void *mpvDeviceList;
        uint32_t mcbDeviceList;

        PREMOTEUSBDEVICE mpDevices;

        bool mfPollURB;
#endif /* VRDP_MC */
};

#endif /* ____H_REMOTEUSBBACKEND */
