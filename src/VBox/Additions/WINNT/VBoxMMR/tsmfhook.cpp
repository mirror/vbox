/* $Id$ */
/** @file
 * VBoxMMR - Multimedia Redirection
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "stdafx.h"
#include "tsmfhook.h"
#include "tsmf.h"
#include "logging.h"

#include <DbgHelp.h>

#include <stdio.h>
#include <mfidl.h>
#include <WtsApi32.h>

#include <iprt/thread.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/VBoxGuest.h>
#include <VBox/HostServices/VBoxHostChannel.h>

#define CODECAVE_SIZE 0x64

/* Generic byte code for API calls redirection.
 * It corresponds to the following set of instructions:
 *     push  ebp
 *     mov   ebp, esp
 *     pop   ebp
 *     mov   eax, 0DEADBEEFh
 *     jmp   eax
 */
static const char g_szCodeCave[] = "\x55\x8B\xEC\x5D\xB8\xEF\xBE\xAD\xDE\xFF\xE0";

const WCHAR *g_pwszMMRFlags    = L"VBoxMMR";

const WCHAR *g_pwszMMRAdditions =
    L"SOFTWARE\\Oracle\\VirtualBox Guest Additions";

const char *g_pszVRDETSMF      = "/vrde/tsmf";

const DWORD g_dwMMRCodeCavingEnabled = 0x00000002;

BOOL g_bMMRCodeCavingIsEnabled = TRUE;

BOOL MMRCodeCavingIsEnabled()
{
    LONG lResult;
    HKEY hKey;
    DWORD dwType = 0;
    DWORD dwValue = 0;
    DWORD dwSize = sizeof(dwValue);

    BOOL fEnabled = TRUE;

    lResult = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE, g_pwszMMRAdditions, 0, KEY_QUERY_VALUE, &hKey);

    if (lResult == ERROR_SUCCESS)
    {
        lResult = RegQueryValueExW(
            hKey, g_pwszMMRFlags, NULL, &dwType, (BYTE *) &dwValue, &dwSize);

        RegCloseKey(hKey);

        if (lResult == ERROR_SUCCESS &&
            dwSize == sizeof(dwValue) &&
            dwType == REG_DWORD)
        {
            fEnabled = g_dwMMRCodeCavingEnabled & dwValue;
             VBoxMMRHookLog("VBoxMMR: Registry setting: %d\n", dwValue);
        }
    }

    return fEnabled;
}

/* Events, which are specific for the tsmf channel. */
#define VBOX_TSMF_HCH_CREATE_ACCEPTED (VBOX_HOST_CHANNEL_EVENT_USER + 0)
#define VBOX_TSMF_HCH_CREATE_DECLINED (VBOX_HOST_CHANNEL_EVENT_USER + 1)
#define VBOX_TSMF_HCH_DISCONNECTED    (VBOX_HOST_CHANNEL_EVENT_USER + 2)

RTTHREAD hDetachMonitor = NIL_RTTHREAD;
uint32_t nUserData = 1;

HANDLE ghVBoxDriver = NULL;

using std::map;
using std::list;
using std::queue;

extern bool isWMP;

CRITICAL_SECTION CreateLock;
HANDLE hCreateEvent;
uint32_t g_nCreateResult = VBOX_TSMF_HCH_CREATE_DECLINED;

TRACEHANDLE g_hTraceSession = NULL;

struct HookEntry
{
    LPCSTR DllName;
    LPCSTR FunctionName;
    PVOID  HookFuncAddr;
    PVOID *OrigFuncAddrPtr;
    PVOID *MMRFuncAddrPtr;
};


struct HostChannelCtx
{
    RTTHREAD thread;
    bool fShutdown;
    uint32_t u32HGCMClientId;
    uint8_t au8EventData[4];
};

HostChannelCtx g_HostChannelCtx = {0};

struct ReadReq
{
    PBYTE Buffer;
    LPOVERLAPPED_COMPLETION_ROUTINE Func;
    LPOVERLAPPED lpOverlapped;
    HANDLE hThread;
    DWORD cbRead;
    DWORD ErrorCode;
};


VOID CALLBACK APCProc(ULONG_PTR dwParam)
{
    ReadReq *pReq = (ReadReq *) dwParam;

    if (NULL != g_hTraceSession)
    {
        /*                                    Size       FieldTypeFlags     */
        TraceEventData Event; //  = {    { sizeof(Event), (USHORT) 0, }, h };

        ZeroMemory(&Event, sizeof(Event));
        Event.Header.Size = sizeof(Event.Header) + sizeof(Event.ReadEnd);
        Event.Header.Class.Type = EVENT_TRACE_TYPE_DC_END;
        Event.Header.Class.Version = 0;
        Event.Header.Class.Level = TRACE_LEVEL_INFORMATION;
        Event.Header.Flags = WNODE_FLAG_TRACED_GUID;
        Event.Header.Guid = ChannelReadCategoryId;
        Event.ReadEnd.ErrorCode = pReq->ErrorCode;

        if (0 == pReq->ErrorCode)
        {
            DWORD cbToTrace = min(((PCHANNEL_PDU_HEADER) pReq->Buffer)->length, sizeof(Event.ReadEnd.Data));
            CopyMemory(Event.ReadEnd.Data, pReq->Buffer, cbToTrace);
        }
        else
            Event.Header.Size -= sizeof(Event.ReadEnd.Data);


        TraceEvent(g_hTraceSession, &Event.Header);
    }

    pReq->Func(pReq->ErrorCode, pReq->cbRead, pReq->lpOverlapped);
    CloseHandle(pReq->hThread);
    delete pReq;
}

class VBOX_RDP_CHANNEL
{
    static list<HANDLE> ChannelList;
    static map<DWORD, HANDLE> ChannelMap;

    DWORD m_Id;
    CRITICAL_SECTION m_Lock;
    queue<PBYTE> m_BufferList;
    queue<ReadReq *> m_ReqList;

    // we need to ignore the first write operation(s), as they are handled by VRDP for us
    // we probably don't want this in the final solution.
    DWORD m_nWriteOps;

protected:

    void DoCompleteRead(PBYTE pBuffer, ReadReq *pReq)
    {
        uint32_t u32DataSize = *((uint32_t *) pBuffer);

        // FIXME : we really want the VBox RDP server to preserve these for us
        PCHANNEL_PDU_HEADER pPduHdr = (PCHANNEL_PDU_HEADER) pReq->Buffer;

        pPduHdr->length = u32DataSize;
        pPduHdr->flags = CHANNEL_FLAG_ONLY;

        CopyMemory(++pPduHdr, pBuffer + sizeof(u32DataSize), u32DataSize);

        pReq->cbRead = u32DataSize  + sizeof(CHANNEL_PDU_HEADER);
        pReq->ErrorCode = ERROR_SUCCESS;

        delete [] pBuffer;

        QueueUserAPC(APCProc, pReq->hThread, (ULONG_PTR) pReq);
    }

public:
    VBOX_RDP_CHANNEL(DWORD Id) : m_Id(Id), m_nWriteOps(0)
    {
        ChannelList.push_back(this);
        ChannelMap[Id] = this;

        InitializeCriticalSection(&m_Lock);
    };

    ~VBOX_RDP_CHANNEL()
    {
        DeleteCriticalSection(&m_Lock);

        ChannelMap.erase(m_Id);
        ChannelList.remove(this);

        while (!m_ReqList.empty())
        {
            ReadReq *req = m_ReqList.front();

            req->cbRead = 0;
            req->ErrorCode = ERROR_BROKEN_PIPE;

            QueueUserAPC(APCProc, req->hThread, (ULONG_PTR) req);

            m_ReqList.pop();
        }
    }


    void PushRequest(PBYTE Buffer, LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionFunc)
    {
        ReadReq *req = new ReadReq;

        req->Buffer = Buffer;
        req->Func = lpCompletionFunc;
        req->lpOverlapped = lpOverlapped;

        // the pseudo handle returned by GetCurrentTHread() can not be used with QueueUserAPC()
        BOOL b = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                                 GetCurrentProcess(), &req->hThread,
                                 THREAD_SET_CONTEXT, FALSE, 0);

        EnterCriticalSection(&m_Lock);

        if (!m_BufferList.empty())
        {
            PBYTE pBuffer = m_BufferList.front();
            DoCompleteRead(pBuffer, req);
            m_BufferList.pop();
        }
        else
        {
            m_ReqList.push(req);
        }

        LeaveCriticalSection(&m_Lock);
    }

    void PushBuffer(PBYTE pBuffer)
    {
        EnterCriticalSection(&m_Lock);

        if (!m_ReqList.empty())
        {
            ReadReq *req = m_ReqList.front();
            DoCompleteRead(pBuffer, req);

            m_ReqList.pop();
        }
        else
        {
            m_BufferList.push(pBuffer);
        }

        LeaveCriticalSection(&m_Lock);
    }

    static bool IsInstance(HANDLE hObject)
    {
        for (list<HANDLE>::iterator iter = ChannelList.begin(); iter != ChannelList.end(); ++iter)
        {
            if (hObject == *iter)
                return true;
        }

        return false;
    }

    static uint32_t ChannelCount()
    {
        return ChannelList.size();
    }

    DWORD GetId() { return m_Id; }

    static VBOX_RDP_CHANNEL * GetFromId(DWORD Id)
    {
        map<DWORD, HANDLE>::iterator iter = ChannelMap.find(Id);
        if (iter != ChannelMap.end())
            return (VBOX_RDP_CHANNEL *) iter->second;

        return NULL;
    }

#ifdef TRACE

    void RequestWritten(const DWORD *pSharedHdr)
    {
        // avoid filling up the map too much
        if (pSharedHdr[2] != 0x100 && pSharedHdr[2] != 0x101 && pSharedHdr[2] != 0x106 && pSharedHdr[2] != 0x107 && pSharedHdr[2] != 0x108)
            return;

        LARGE_INTEGER li;
        li.HighPart = (pSharedHdr[0] & 0x3FFFFFFF);
        li.LowPart = pSharedHdr[1];

        m_ServerReqMap[li.QuadPart] = pSharedHdr[2];
    }

#endif
};

list<HANDLE> VBOX_RDP_CHANNEL::ChannelList;
map<DWORD, HANDLE> VBOX_RDP_CHANNEL::ChannelMap;

const IMAGE_IMPORT_DESCRIPTOR * GetImportDescriptor(HMODULE hMod)
{
    const PBYTE pBaseAddr = (PBYTE) hMod;

    const IMAGE_DOS_HEADER *pDosHdr = (IMAGE_DOS_HEADER *) pBaseAddr;
    const size_t nOptHdrOffset = pDosHdr->e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER);

    const IMAGE_OPTIONAL_HEADER *pOptHdr = (IMAGE_OPTIONAL_HEADER *) (pBaseAddr + nOptHdrOffset);
    const size_t nImportDescOffset = pOptHdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

    return (IMAGE_IMPORT_DESCRIPTOR *) (pBaseAddr + nImportDescOffset);
}

DWORD ReadLoadedImage(LPCVOID pBaseAddr)
{
    DWORD nCCAddress = 0;
    PIMAGE_SECTION_HEADER pTextSectionHdr = 0;

    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)pBaseAddr;

    PIMAGE_NT_HEADERS32 pNtFileHdr = (PIMAGE_NT_HEADERS32)((DWORD)pBaseAddr + pDosHdr->e_lfanew);

    DWORD nNumberOfSections = pNtFileHdr->FileHeader.NumberOfSections;

    PIMAGE_SECTION_HEADER pSectionsHdr = (PIMAGE_SECTION_HEADER)((DWORD)pBaseAddr + pDosHdr->e_lfanew + sizeof(IMAGE_NT_HEADERS32));


    for (DWORD i = 0; i < nNumberOfSections; i++)
    {
        PIMAGE_SECTION_HEADER pSectionHdr = &pSectionsHdr[i];

        if (!_stricmp((char*)pSectionHdr->Name, ".text"))
        {
            pTextSectionHdr = pSectionHdr;
            break;
        }
    }

    if(0 != pTextSectionHdr)
    {
       nCCAddress = (DWORD)pBaseAddr + pTextSectionHdr->VirtualAddress + pTextSectionHdr->SizeOfRawData;
    }

    return nCCAddress;
}


bool ReplaceStubAddr(BYTE* pBuffer,DWORD nBufferSize,DWORD nOld,DWORD nNew)
{
    if ( 0 != nNew )
    {
        for (DWORD i = 0; i < nBufferSize - sizeof(DWORD); i++)
        {
            if (*(PDWORD)(pBuffer + i) == nOld)
            {
                memcpy(pBuffer + i, &nNew, sizeof(DWORD));
                return true;
            }
        }
    }

    return false;
}

void InstallHook(IMAGE_THUNK_DATA *pIATEntry, HookEntry pEntry,const PBYTE pBaseAddr)
{
    SIZE_T cbWritten;
    static map<DWORD, DWORD> CCAddrHistoryMap;

    PVOID pfnHook   = pEntry.HookFuncAddr;
    PVOID *ppfnOrig = pEntry.OrigFuncAddrPtr;
    PVOID *ppfnMMR   = pEntry.MMRFuncAddrPtr;
    LPCSTR FunctionName = pEntry.FunctionName;

    unsigned int nCCOffSet = sizeof(g_szCodeCave);

     if (NULL == *ppfnOrig)
     {
         WriteProcessMemory(GetCurrentProcess(), ppfnOrig, &pIATEntry->u1.Function, 4, &cbWritten);
     }

    if(g_bMMRCodeCavingIsEnabled)
    {
        DWORD nCodeCaveAddr = ReadLoadedImage(pBaseAddr);

        if (!nCodeCaveAddr)
        {
            /* If fails perform the hooking without code caving */
            VBoxMMRHookLog("VBoxMMR: Retrieving code cave location had failed\n");
            WriteProcessMemory(GetCurrentProcess(), &pIATEntry->u1.Function, &pfnHook, 4, (SIZE_T *) &cbWritten);
            return;
        }

       nCodeCaveAddr = nCodeCaveAddr - CODECAVE_SIZE;

        BYTE pByteCodeBuffer[CODECAVE_SIZE] = {0};

        memcpy(pByteCodeBuffer, g_szCodeCave, CODECAVE_SIZE);

        map<DWORD, DWORD>::iterator iter = CCAddrHistoryMap.find(nCodeCaveAddr);
        if (iter != CCAddrHistoryMap.end())
        {
            nCodeCaveAddr +=  CCAddrHistoryMap[nCodeCaveAddr] ;
            iter->second += nCCOffSet;

        }else{

            CCAddrHistoryMap[nCodeCaveAddr] = nCCOffSet;
        }

        bool bResult = ReplaceStubAddr( pByteCodeBuffer,
                                          CODECAVE_SIZE,
                                          0xDEADBEEF,
                                          (DWORD)ppfnMMR);

        if (!bResult)
        {
            /* If fails perform the hooking without code caving */
            VBoxMMRHookLog("VBoxMMR: Replacing code cave stub address failed\n");
            WriteProcessMemory(GetCurrentProcess(), &pIATEntry->u1.Function, &pfnHook, 4, (SIZE_T *) &cbWritten);
            return ;
        }

        bResult = WriteProcessMemory( GetCurrentProcess(),
                                      (LPVOID)nCodeCaveAddr,
                                      pByteCodeBuffer,
                                      sizeof(g_szCodeCave),
                                      0 );

        if (!bResult)
        {
            /* If fails perform the hooking without code caving*/
            VBoxMMRHookLog("VBoxMMR: Writing  byte code to the text section failed\n");
            WriteProcessMemory(GetCurrentProcess(), &pIATEntry->u1.Function, &pfnHook, 4, (SIZE_T *) &cbWritten);
            return;
        }

        bResult = WriteProcessMemory(GetCurrentProcess(), &pIATEntry->u1.Function, &nCodeCaveAddr, 4, &cbWritten);

        if (!bResult)
        {
            VBoxMMRHookLog("VBoXMMR: Patching the IAT failed\n");
            WriteProcessMemory(GetCurrentProcess(), &pIATEntry->u1.Function, &pfnHook, 4, (SIZE_T *) &cbWritten);
            return;
        }
    }else
    {
        WriteProcessMemory(GetCurrentProcess(), &pIATEntry->u1.Function, &pfnHook, 4, (SIZE_T *) &cbWritten);
    }
}

void InstallHooks(const IMAGE_IMPORT_DESCRIPTOR *pDescriptor, const PBYTE pBaseAddr, HookEntry *pEntries)
{
    DWORD nCodeCaveOffSet= 0;
    while(pDescriptor->FirstThunk)
    {
        LPCSTR pszDllName = (LPCSTR) (pBaseAddr + pDescriptor->Name);

        const IMAGE_THUNK_DATA* pThunk = (IMAGE_THUNK_DATA*) (pBaseAddr + pDescriptor->OriginalFirstThunk);
        IMAGE_THUNK_DATA *pIATEntry = (IMAGE_THUNK_DATA *)(pBaseAddr + pDescriptor->FirstThunk);
        LPCSTR pszModuleName = (LPCSTR) (pBaseAddr + pDescriptor->Name);

        while (pThunk->u1.Function)
        {
            for (int nIndex = 0; NULL != pEntries[nIndex].DllName; ++nIndex)
            {
                if (0 == _stricmp(pszDllName, pEntries[nIndex].DllName))
                {
                    if (0 == (pThunk->u1.AddressOfData & IMAGE_ORDINAL_FLAG))
                    {
                        IMAGE_IMPORT_BY_NAME *pImport = (IMAGE_IMPORT_BY_NAME *) (pBaseAddr + pThunk->u1.AddressOfData);

                        if (0 == strcmp((LPCSTR) pImport->Name, pEntries[nIndex].FunctionName))
                        {
                            VBoxMMRHookLog("VBoxMMR: Install Hook for dll %s function %s \n",
                                           pEntries[nIndex].DllName, pEntries[nIndex].FunctionName);

                            InstallHook(pIATEntry,
                                        pEntries[nIndex],
                                        pBaseAddr);
                        }
                    }
                }
            }
            ++pThunk; ++pIATEntry;
        }
        ++pDescriptor;
    }
}

int StartMonitor(RTTHREAD *hMonitor, PFNRTTHREAD pMonitorFn,
    void *pData, size_t cbStack, RTTHREADTYPE enmType,
    uint32_t flags, const char *pszName)
{
    int rc;

    rc = RTThreadCreate(hMonitor, pMonitorFn, pData,
                        cbStack, enmType, flags, pszName);

    if (RT_FAILURE(rc))
    {
        VBoxMMRHookLog("VBoxMMR: Error starting monitor %s: %d\n", pszName, rc);
    }

    return rc;
}

int StopMonitor(RTTHREAD *hMonitor, const char* pszName)
{
    int rc;

    if (*hMonitor != NIL_RTTHREAD)
    {
        rc = RTThreadUserSignal(*hMonitor);

        if (RT_SUCCESS(rc))
        {
            // rc = RTThreadWait(*hMonitor, RT_INDEFINITE_WAIT, NULL);

            if (RT_FAILURE(rc))
            {
                VBoxMMRHookLog("VBoxMMR: Error waiting for monitor %s to stop: %d\n", pszName, rc);
            }
        }
        else
        {
            VBoxMMRHookLog("VBoxMMR: Error sending stop signal to monitor %s: %d\n", pszName, rc);
        }

        *hMonitor = NIL_RTTHREAD;
    }


    return rc;
}

DECLCALLBACK(int)
MonitorDetach(RTTHREAD hThreadSelf, void *pvUser)
{
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;
    bool bPrevious = FALSE;
    bool bCurrent = bPrevious;

    maskInfo.u32OrMask = VMMDEV_EVENT_VRDP;
    maskInfo.u32NotMask = 0;

    VBoxMMRHookLog("VBoxMMR: MonitorDetach starting\n");

    if (DeviceIoControl (ghVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        VBoxMMRHookLog("VBoxTray: VBoxVRDPThread: DeviceIOControl(CtlMask - or) succeeded\n");
    }
    else
    {
        VBoxMMRHookLog("VBoxTray: VBoxVRDPThread: DeviceIOControl(CtlMask) failed\n");
        return 0;
    }

    for(;;)
    {
        /* Call the host to get VRDP status and the experience level. */
        VMMDevVRDPChangeRequest vrdpChangeRequest = {0};

        vrdpChangeRequest.header.size            = sizeof(VMMDevVRDPChangeRequest);
        vrdpChangeRequest.header.version         = VMMDEV_REQUEST_HEADER_VERSION;
        vrdpChangeRequest.header.requestType     = VMMDevReq_GetVRDPChangeRequest;
        vrdpChangeRequest.u8VRDPActive           = 0;
        vrdpChangeRequest.u32VRDPExperienceLevel = 0;

        if (DeviceIoControl (ghVBoxDriver,
                             VBOXGUEST_IOCTL_VMMREQUEST(sizeof(VMMDevVRDPChangeRequest)),
                             &vrdpChangeRequest,
                             sizeof(VMMDevVRDPChangeRequest),
                             &vrdpChangeRequest,
                             sizeof(VMMDevVRDPChangeRequest),
                             &cbReturned, NULL))
        {
            bCurrent = ( vrdpChangeRequest.u8VRDPActive == 1) ? TRUE : FALSE;

            if (bCurrent != bPrevious)
            {
                VBoxMMRHookLog(
                    "VBoxMMR: VRDP active status changed: %d\n",
                    vrdpChangeRequest.u8VRDPActive);

                if (bCurrent == FALSE &&
                    VBOX_RDP_CHANNEL::ChannelCount() > 0)
                {
                    VBoxMMRHookLog("VBoxMMR: exiting ...\n");
                    ExitProcess(0);
                    break;
                }
            }

            bPrevious = bCurrent;
        }
        else
        {
           VBoxMMRHookLog("VBoxMMR: VBoxVRDPThread: Error from DeviceIoControl VBOXGUEST_IOCTL_VMMREQUEST\n");

        }

        if (RTThreadUserWait(hThreadSelf, 1000) == VINF_SUCCESS)
        {
            VBoxMMRHookLog("VBoxMMR: detach monitor received stop signal\n");
            break;
        }
    }

    VBoxMMRHookLog("VBoxMMR: MonitorDetach stopping\n");

    return VINF_SUCCESS;
}

/*
 * WTSQuerySessionInformationW
 */

BOOL (WINAPI * g_pfnWTSQuerySessionInformation)(HANDLE, DWORD, WTS_INFO_CLASS, LPWSTR *, DWORD *) = NULL;
BOOL WINAPI MMRWTSQuerySessionInformation(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR *ppBuffer, DWORD *pBytesReturned)
{
    BOOL b =  g_pfnWTSQuerySessionInformation(hServer, SessionId, WTSInfoClass, ppBuffer, pBytesReturned);

    if (WTSIsRemoteSession == WTSInfoClass)
    {
        PBYTE pb = (PBYTE) *ppBuffer;
        *pb = 1;
        return b;
    }
    else if (WTSClientProtocolType ==  WTSInfoClass)
    {
        PUSHORT pus = (PUSHORT) *ppBuffer;
        *pus = 2;
        return b;
    }

    return b;
}

BOOL WINAPI LocalWTSQuerySessionInformation(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR *ppBuffer, DWORD *pBytesReturned)
{
    return MMRWTSQuerySessionInformation( hServer, SessionId,  WTSInfoClass, ppBuffer, pBytesReturned);
}


/*
 * WriteFile
 */

BOOL (WINAPI *g_pfnWriteFile)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED) = NULL;

BOOL WINAPI MMRWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
    BOOL b = TRUE;

    if (NULL != g_hTraceSession)
    {
        /*                                    Size       FieldTypeFlags     */
        TraceEventData Event; //  = {    { sizeof(Event), (USHORT) 0, }, h };

        ZeroMemory(&Event, sizeof(Event));
        Event.Header.Size = sizeof(Event.Header) + sizeof(Event.WriteStart);
        Event.Header.Class.Type = EVENT_TRACE_TYPE_DC_START;
        Event.Header.Class.Version = 0;
        Event.Header.Class.Level = TRACE_LEVEL_INFORMATION;
        Event.Header.Flags = WNODE_FLAG_TRACED_GUID;
        Event.Header.Guid = ChannelWriteCategoryId;
        Event.WriteStart.Channel = hFile;

        DWORD cbToTrace = min(nNumberOfBytesToWrite, sizeof(Event.WriteStart.Data));
        CopyMemory(Event.WriteStart.Data, lpBuffer, cbToTrace);

        TraceEvent(g_hTraceSession, &Event.Header);
    }


    if (VBOX_RDP_CHANNEL::IsInstance(hFile))
    {
        VBOX_RDP_CHANNEL *pChannel = (VBOX_RDP_CHANNEL *) hFile;

        *lpNumberOfBytesWritten = 0;

        int rc = VbglR3HostChannelSend(
            pChannel->GetId(), g_HostChannelCtx.u32HGCMClientId,
            (void *) lpBuffer, nNumberOfBytesToWrite);

        if (RT_SUCCESS(rc))
        {
            *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
        }

        VBoxMMRHookLog(
            "VBoxMMR: TSMF send, channel: %d, sent: %d, result: %d\n",
            pChannel->GetId(), *lpNumberOfBytesWritten, rc);
    }
    else
    {
        b = g_pfnWriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
    }

    return b;
}

BOOL WINAPI LocalWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
    return MMRWriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

/*
/*
 * ReadFileEx
 */


BOOL (WINAPI *g_pfnReadFileEx)(HANDLE, LPVOID, DWORD, LPOVERLAPPED, LPOVERLAPPED_COMPLETION_ROUTINE) = 0;

BOOL WINAPI MMRReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    BOOL br = TRUE;

    if (NULL != g_hTraceSession)
    {
        /*                                    Size       FieldTypeFlags     */
        TraceEventData Event; //  = {    { sizeof(Event), (USHORT) 0, }, h };

        ZeroMemory(&Event, sizeof(Event));
        Event.Header.Size = sizeof(Event.Header) + sizeof(Event.ReadStart);
        Event.Header.Class.Type = EVENT_TRACE_TYPE_DC_START;
        Event.Header.Class.Version = 0;
        Event.Header.Class.Level = TRACE_LEVEL_INFORMATION;
        Event.Header.Flags = WNODE_FLAG_TRACED_GUID;
        Event.Header.Guid = ChannelReadCategoryId;
        Event.ReadStart.Channel = hFile;

        TraceEvent(g_hTraceSession, &Event.Header);
    }

    if (VBOX_RDP_CHANNEL::IsInstance(hFile))
    {
        VBOX_RDP_CHANNEL *pChannel = (VBOX_RDP_CHANNEL *) hFile;
        pChannel->PushRequest((PBYTE) lpBuffer, lpOverlapped, lpCompletionRoutine);
    }
    else
    {
        br = g_pfnReadFileEx(hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped, lpCompletionRoutine);
    }

    return br;
}

BOOL WINAPI LocalReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    return MMRReadFileEx(hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped, lpCompletionRoutine);
}


/*
 * CloseHandle
 */

BOOL (WINAPI * g_pfnCloseHandle)(HANDLE) = NULL;

BOOL WINAPI MMRCloseHandle(HANDLE hObject)
{
    if (NULL != g_hTraceSession)
    {
        /*                                    Size       FieldTypeFlags     */
        TraceEventData Event; //  = {    { sizeof(Event), (USHORT) 0, }, h };

        ZeroMemory(&Event, sizeof(Event));
        Event.Header.Size = sizeof(Event.Header) + sizeof(Event.Close);
        Event.Header.Class.Type = EVENT_TRACE_TYPE_DC_START;
        Event.Header.Class.Version = 0;
        Event.Header.Class.Level = TRACE_LEVEL_INFORMATION;
        Event.Header.Flags = WNODE_FLAG_TRACED_GUID;
        Event.Header.Guid = ChannelCloseCategoryId;
        Event.Close.Channel = hObject;

        TraceEvent(g_hTraceSession, &Event.Header);
    }

    if (VBOX_RDP_CHANNEL::IsInstance(hObject))
    {
        VBOX_RDP_CHANNEL *pChannel = (VBOX_RDP_CHANNEL *) hObject;

        VbglR3HostChannelDetach(
            pChannel->GetId(), g_HostChannelCtx.u32HGCMClientId);

        delete pChannel;

        return TRUE;
    }

    return g_pfnCloseHandle(hObject);
}

BOOL WINAPI LocalCloseHandle(HANDLE hObject)
{
    return MMRCloseHandle(hObject);
}

/*
 * CancelIo
 */

BOOL (WINAPI * g_pfnCancelIo)(HANDLE) = 0;

BOOL WINAPI MMRCancelIo(HANDLE hFile)
{
    if (VBOX_RDP_CHANNEL::IsInstance(hFile))
    {
        // FIXME
        return TRUE;
    }

    return g_pfnCancelIo(hFile);
}

BOOL WINAPI LocalCancelIo(HANDLE hFile)
{
    return MMRCancelIo(hFile);
}


/*
 * WinStationIsSessionRemoteable
 */

BOOL (WINAPI *g_pfnWinStationIsSessionRemoteable)(HANDLE, DWORD, PVOID) = NULL;

BOOL WINAPI MMRWinStationIsSessionRemoteable(HANDLE hServer, DWORD hSession, PVOID c)
{
    BOOL b = g_pfnWinStationIsSessionRemoteable(hServer, hSession, c);

    PBYTE pb = (PBYTE) c;

    VBoxMMRHookLog("VBoxMMR: LocalWinStationIsSessionRemoteable() = %d -> 1\n", (int) *pb);

    *pb = 1;

    return b;
}

BOOL WINAPI LocalWinStationIsSessionRemoteable(HANDLE hServer, DWORD hSession, PVOID c)
{
    return MMRWinStationIsSessionRemoteable(hServer, hSession, c);
}

/*
 * WinStationVirtualOpenEx
 */

HANDLE (WINAPI *g_pfnWinStationVirtualOpenEx)(HANDLE, DWORD, LPSTR, DWORD) = NULL;

HANDLE WINAPI MMRWinStationVirtualOpenEx(HANDLE hServer, DWORD hSession, LPSTR pVirtualName, DWORD flags)
{
    // assert(WTS_CURRENT_SERVER == hServer)
    // assert(WTS_CURRENT_SESSION == hSession)

    HANDLE h = g_pfnWinStationVirtualOpenEx(hServer, hSession, pVirtualName, flags);

    if (NULL == h && 0 != g_HostChannelCtx.u32HGCMClientId)
    {
        uint32_t u32ChannelHandle = 0;
        uint32_t u32Flags = 0x00; /* Not used currently. */

        EnterCriticalSection(&CreateLock);

        g_nCreateResult = VBOX_TSMF_HCH_CREATE_DECLINED;

        int rc = VbglR3HostChannelAttach(
            &u32ChannelHandle, g_HostChannelCtx.u32HGCMClientId, "/vrde/tsmf", u32Flags);

        if (RT_SUCCESS(rc))
        {
            WaitForSingleObject(hCreateEvent, 5000);

            if (g_nCreateResult == VBOX_TSMF_HCH_CREATE_ACCEPTED)
            {
                h = new VBOX_RDP_CHANNEL(u32ChannelHandle);

                if (hDetachMonitor == NIL_RTTHREAD)
                {
                    StartMonitor(&hDetachMonitor, MonitorDetach,
                        &nUserData, 0, RTTHREADTYPE_INFREQUENT_POLLER,
                        RTTHREADFLAGS_WAITABLE, "mmrpoll");
                }
            }
            else
            {
                VBoxMMRHookLog("VBoxMMR: Unable to open channel: %d\n", g_nCreateResult);
            }
        }
        else
        {
            VBoxMMRHookLog("VBoxMMR: Error attaching channel: %d\n", rc);
        }

        LeaveCriticalSection(&CreateLock);
    }

    if (NULL != g_hTraceSession)
    {
        /*                                    Size       FieldTypeFlags     */
        TraceEventData Event; //  = {    { sizeof(Event), (USHORT) 0, }, h };

        ZeroMemory(&Event, sizeof(Event));
        Event.Header.Size = sizeof(Event.Header) + sizeof(Event.Open);
        Event.Header.Class.Type = EVENT_TRACE_TYPE_DC_END;
        Event.Header.Class.Version = 0;
        Event.Header.Class.Level = TRACE_LEVEL_INFORMATION;
        Event.Header.Flags = WNODE_FLAG_TRACED_GUID;
        Event.Header.Guid = ChannelOpenCategoryId;
        Event.Open.Channel = h;

        TraceEvent(g_hTraceSession, &Event.Header);
    }

    return h;
}

HANDLE WINAPI LocalWinStationVirtualOpenEx(HANDLE hServer, DWORD hSession, LPSTR pVirtualName, DWORD flags)
{
    return MMRWinStationVirtualOpenEx(hServer, hSession, pVirtualName, flags);
}

/*
 * GetSystemMetrics
 */

int (WINAPI *g_pfnGetSystemMetrics)(int) = NULL;


int WINAPI MMRGetSystemMetrics(int nIndex)
{
    const char *format = (nIndex < 100) ? "VBoxMMR: GetSystemMetrics(%d)\n" : "VBoxMMR: GetSystemMetrics(0x%x)\n";

    VBoxMMRHookLog(format, nIndex);

    if (0x1000 == nIndex)
        return 1;

    return g_pfnGetSystemMetrics(nIndex);
}

int WINAPI LocalGetSystemMetrics(int nIndex)
{
    return MMRGetSystemMetrics(nIndex);
}

HookEntry g_TSMFHooks[] =
{
    { "winsta.dll", "WinStationIsSessionRemoteable", LocalWinStationIsSessionRemoteable, (PVOID *) &g_pfnWinStationIsSessionRemoteable, (PVOID *) &MMRWinStationIsSessionRemoteable },
    { "winsta.dll", "WinStationVirtualOpenEx", LocalWinStationVirtualOpenEx, (PVOID *) &g_pfnWinStationVirtualOpenEx, (PVOID *) &MMRWinStationVirtualOpenEx },
    { "user32.dll", "GetSystemMetrics", LocalGetSystemMetrics, (PVOID *) &g_pfnGetSystemMetrics, (PVOID *) &MMRGetSystemMetrics},
    { "kernel32.dll", "ReadFileEx", LocalReadFileEx, (PVOID *) &g_pfnReadFileEx, (PVOID *) &MMRReadFileEx },
    { "kernel32.dll", "WriteFile", LocalWriteFile, (PVOID *) &g_pfnWriteFile,(PVOID *) &MMRWriteFile },
    { "kernel32.dll", "CloseHandle", LocalCloseHandle, (PVOID *) &g_pfnCloseHandle,(PVOID *) &MMRCloseHandle },
    { "kernel32.dll", "CancelIo", LocalCancelIo, (PVOID *) &g_pfnCancelIo, (PVOID *) &MMRCancelIo },
    { "wtsapi32.dll", "WTSQuerySessionInformationW", LocalWTSQuerySessionInformation, (PVOID *) &g_pfnWTSQuerySessionInformation, (PVOID *) &MMRWTSQuerySessionInformation },
    { NULL, NULL, NULL, NULL, NULL }
};

/*
 * MFCreateRemoteDesktopPlugin
 */

HRESULT (STDAPICALLTYPE *g_pfnMFCreateRemoteDesktopPlugin)(IMFRemoteDesktopPlugin **ppPlugin) = NULL;

STDAPI MMRMFCreateRemoteDesktopPlugin(IMFRemoteDesktopPlugin **ppPlugin)
{
    HRESULT hr = g_pfnMFCreateRemoteDesktopPlugin(ppPlugin);

    VBoxMMRHookLog("VBoxMMR: LocalMFCreateRemoteDesktopPlugin: *ppPlugin = %p, result: %x\n", *ppPlugin, hr);

    static bool IsTSMFHooked = false;
    if (!IsTSMFHooked)
    {
        HMODULE hModule = GetModuleHandleA("tsmf");

        if (hModule)
        {
            VBoxMMRHookLog("VBoxMMR: Installing hooks for tsmf.dll\n");
            const IMAGE_IMPORT_DESCRIPTOR *pDescriptor = GetImportDescriptor(hModule);
            InstallHooks(pDescriptor, (PBYTE) hModule, g_TSMFHooks);
            IsTSMFHooked = true;
        }
    }

    return hr;
}

STDAPI LocalMFCreateRemoteDesktopPlugin(IMFRemoteDesktopPlugin **ppPlugin)
{
    return MMRMFCreateRemoteDesktopPlugin(ppPlugin);
}

/*
 * GetProcAddress
 */

FARPROC (WINAPI *g_pfnGetProcAddress)(HMODULE, LPCSTR) = NULL;
FARPROC WINAPI LocalGetProcAddress(HMODULE hModule, LPCSTR lpProcName);
FARPROC WINAPI MMRGetProcAddress(HMODULE hModule, LPCSTR lpProcName);

HookEntry g_MFHooks[] =
{
    { "kernel32.dll", "GetProcAddress", LocalGetProcAddress, (PVOID *) &g_pfnGetProcAddress,(PVOID *) &MMRGetProcAddress },
    { NULL, NULL, NULL, NULL, NULL }
};

HookEntry g_WinMMHooks[] =
{
    { "user32.dll", "GetSystemMetrics", LocalGetSystemMetrics, (PVOID *) &g_pfnGetSystemMetrics,(PVOID *) &MMRGetSystemMetrics},
    { "kernel32.dll", "GetProcAddress", LocalGetProcAddress, (PVOID *) &g_pfnGetProcAddress,(PVOID *) &MMRGetProcAddress },
    { NULL, NULL, NULL, NULL, NULL }
};

HookEntry g_DShowHooks[] =
{
    { "user32.dll", "GetSystemMetrics", LocalGetSystemMetrics, (PVOID *) &g_pfnGetSystemMetrics,(PVOID *) &MMRGetSystemMetrics },
    { "kernel32.dll", "GetProcAddress", LocalGetProcAddress, (PVOID *) &g_pfnGetProcAddress,(PVOID *) &MMRGetProcAddress },
    { NULL, NULL, NULL, NULL, NULL }
};


FARPROC WINAPI MMRGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    FARPROC ret = g_pfnGetProcAddress(hModule, lpProcName);

    static bool IsMFHooked = false;
    if (!IsMFHooked)
    {
        CHAR szBuf[512];
        GetModuleFileNameA(hModule, szBuf, sizeof(szBuf));
        PCHAR pc = strrchr(szBuf, '\\');
        if (0 == _stricmp(pc + 1, "mf.dll"))
        {
            VBoxMMRHookLog("VBoxMMR: Installing hooks for mf.dll\n");
            const IMAGE_IMPORT_DESCRIPTOR *pDescriptor = GetImportDescriptor(hModule);
            InstallHooks(pDescriptor, (PBYTE) hModule, g_MFHooks);
            IsMFHooked = true;
        }
    }

    static bool IsWinMMHooked = false;
    if (!IsWinMMHooked)
    {
        CHAR szBuf[512];
        GetModuleFileNameA(hModule, szBuf, sizeof(szBuf));
        PCHAR pc = strrchr(szBuf, '\\');
        if (0 == _stricmp(pc + 1, "winmm.dll"))
        {
            VBoxMMRHookLog("VBoxMMR: Installing hooks for winmm.dll\n");
            const IMAGE_IMPORT_DESCRIPTOR *pDescriptor = GetImportDescriptor(hModule);
            InstallHooks(pDescriptor, (PBYTE) hModule, g_WinMMHooks);
            IsWinMMHooked = true;
        }
    }

    // if an ordinal, all but the lower word must be 0
    // FIXME: should be pointer size
    if (0 == ((DWORD) lpProcName & ~0xFFFF))
    {
        CHAR szDllName[512];
        if (FALSE == GetModuleFileNameA(hModule, szDllName, sizeof(szDllName)))
            szDllName[0] = '\0';

        VBoxMMRHookLog("VBoxMMR: GetProcAddress: %u (%s)\n", (DWORD) lpProcName, szDllName);
    }
    else
    {
        if (0 == strncmp(lpProcName, "WTS", 3) || 0 == strncmp(lpProcName, "MF", 2) || 0 == strncmp(lpProcName, "GetTS", 5))
            VBoxMMRHookLog("VBoxMMR: GetProcAddress: %s\n", lpProcName);

        if (0 == strcmp(lpProcName, "MFCreateRemoteDesktopPlugin"))
        {
            g_pfnMFCreateRemoteDesktopPlugin = (HRESULT (STDAPICALLTYPE *)(IMFRemoteDesktopPlugin **ppPlugin)) ret;
            ret = (FARPROC) LocalMFCreateRemoteDesktopPlugin;

            CHAR szDllName[512];

            if (FALSE == GetModuleFileNameA(hModule, szDllName, sizeof(szDllName)))
            {
                szDllName[0] = '\0';
            }
        }
        else if (0 == strcmp(lpProcName, "WTSQuerySessionInformationW"))
        {
            g_pfnWTSQuerySessionInformation = (BOOL (__stdcall *)(HANDLE,DWORD,WTS_INFO_CLASS,LPWSTR *,DWORD *)) ret;
            ret = (FARPROC) LocalWTSQuerySessionInformation;

            CHAR szDllName[512];

            if (FALSE == GetModuleFileNameA(hModule, szDllName, sizeof(szDllName)))
            {
                szDllName[0] = '\0';
            }
        }
    }

    return ret;
}

FARPROC WINAPI LocalGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    return MMRGetProcAddress(hModule, lpProcName);
}

HookEntry g_WMPHooks[] =
{
    { "user32.dll", "GetSystemMetrics", LocalGetSystemMetrics, (PVOID *) &g_pfnGetSystemMetrics, (PVOID *)&MMRGetSystemMetrics },
    { "kernel32.dll", "GetProcAddress", LocalGetProcAddress, (PVOID *) &g_pfnGetProcAddress, (PVOID *)&MMRGetProcAddress },
    { "mf.dll", "MFCreateRemoteDesktopPlugin", LocalMFCreateRemoteDesktopPlugin, (PVOID *) &g_pfnMFCreateRemoteDesktopPlugin, (PVOID *) &MMRMFCreateRemoteDesktopPlugin },
    { "wtsapi32.dll", "WTSQuerySessionInformationW", LocalGetProcAddress, (PVOID *) &g_pfnGetProcAddress,(PVOID *) &MMRGetProcAddress },
    { NULL, NULL, NULL, NULL, NULL }
};

/*
 * Note that McAffee hooks into GetProcAddress as well (HIPIS0e011b5.dll), so there is no point
 * in patching "GetProcAddress" of ole32.dll
 */

static TRACEHANDLE g_hTraceRegistration = NULL;

static ULONG WINAPI ControlCallback(WMIDPREQUESTCODE RequestCode, PVOID Context, ULONG *Reserved, PVOID pBuffer)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(Reserved);

    ULONG status = ERROR_SUCCESS;
    TRACEHANDLE hTraceSession = GetTraceLoggerHandle(pBuffer);

    switch(RequestCode)
    {
    case WMI_ENABLE_EVENTS:  // Enable the provider
        if (NULL == g_hTraceSession)
            g_hTraceSession = hTraceSession;
        break;
    case WMI_DISABLE_EVENTS:  // Disable the provider
        //  igonore disable requests from other sessions
        if (hTraceSession == g_hTraceSession)
            g_hTraceSession = NULL;
        break;
    default:
        status = ERROR_INVALID_PARAMETER;
        break;
    }

    return status;
}


static int VBoxMMROpenBaseDriver(void)
{
    /* Open VBox guest driver. */
    DWORD dwErr = ERROR_SUCCESS;
    ghVBoxDriver = CreateFile(TEXT(VBOXGUEST_DEVICE_NAME),
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                              NULL);

    if (INVALID_HANDLE_VALUE == ghVBoxDriver )
    {
        dwErr = GetLastError();
        VBoxMMRHookLog("VBoxMMR: Could not open VirtualBox Guest Additions driver! Please install / start it first! Error = %08X\n", dwErr);
    }
    return RTErrConvertFromWin32(dwErr);
}

static void VBoxMMRCloseBaseDriver(void)
{
    if (ghVBoxDriver)
    {
        CloseHandle(ghVBoxDriver);
        ghVBoxDriver = NULL;
    }
}

void
ReadTSMF(uint32_t u32ChannelHandle, uint32_t u32HGCMClientId, uint32_t u32SizeAvailable)
{
    uint32_t u32SizeReceived = 0;
    uint32_t u32SizeRemaining = 0;

    PBYTE pBuffer = (PBYTE) malloc(u32SizeAvailable + sizeof(u32SizeAvailable));
    CopyMemory(pBuffer, &u32SizeAvailable, sizeof(u32SizeAvailable));

    int rc = VbglR3HostChannelRecv(u32ChannelHandle,
                                   u32HGCMClientId,
                                   pBuffer + sizeof(u32SizeAvailable),
                                   u32SizeAvailable,
                                   &u32SizeReceived,
                                   &u32SizeRemaining);

    VBoxMMRHookLog(
        "VBoxMMR: TSMF recv, channel: %d, available: %d, received %d, remaining %d, result: %d\n",
        u32ChannelHandle, u32SizeAvailable, u32SizeReceived, u32SizeRemaining, rc);

    VBOX_RDP_CHANNEL *pChannel =
        VBOX_RDP_CHANNEL::GetFromId(u32ChannelHandle);

    if (RT_SUCCESS(rc) && pChannel && u32SizeReceived != 0)
    {
        DWORD *pPtr = (DWORD *) (pBuffer + sizeof(u32SizeAvailable));

        if (0x01 != pPtr[2] && 0x02 != pPtr[2] &&
            0x80000000 != pPtr[0] && 0x40000001 != pPtr[0])
        {
            VBoxMMRHookLog(
                "VBoxMMR: Unknown TSMF Data: %X %X %X\n",
                pPtr[0], pPtr[1], pPtr[2]);
        }

        pChannel->PushBuffer(pBuffer);
    }
    else
    {
        delete [] pBuffer;
    }
}

DECLCALLBACK(int)
MonitorTSMFChannel(RTTHREAD hThreadSelf, void *pvUser)
{
    HostChannelCtx *pCtx = (HostChannelCtx *) pvUser;

    VBoxMMRHookLog("VBoxMMR: MonitorTSMFChannel starting\n");

    while (!pCtx->fShutdown)
    {
        uint32_t u32ChannelHandle = 0;
        uint32_t u32EventId = 0;
        uint32_t u32SizeReturned = 0;
        void *pvParm = &pCtx->au8EventData[0];
        uint32_t cbParm = sizeof(pCtx->au8EventData);

        int rc = VbglR3HostChannelEventWait(
            &u32ChannelHandle, pCtx->u32HGCMClientId,
            &u32EventId, pvParm, cbParm, &u32SizeReturned);

        if (RT_SUCCESS(rc))
        {
            switch(u32EventId)
            {
                case VBOX_TSMF_HCH_CREATE_ACCEPTED:
                {
                    VBoxMMRHookLog("VBoxMMR: VBOX_TSMF_HCH_CREATE_ACCEPTED: channel: %d\n", u32ChannelHandle);
                    g_nCreateResult = u32EventId;
                    SetEvent(hCreateEvent);
                } break;

                case VBOX_TSMF_HCH_CREATE_DECLINED:
                {
                    VBoxMMRHookLog("VBoxMMR: VBOX_TSMF_HCH_CREATE_DECLINED: channel: %d\n", u32ChannelHandle);
                    g_nCreateResult = u32EventId;
                    SetEvent(hCreateEvent);
                } break;

                case VBOX_TSMF_HCH_DISCONNECTED:
                {
                    VBoxMMRHookLog("VBoxMMR: VBOX_TSMF_HCH_DISCONNECTED: channel: %d\n", u32ChannelHandle);
                } break;

                case VBOX_HOST_CHANNEL_EVENT_CANCELLED:
                {
                    VBoxMMRHookLog("VBoxMMR: VBOX_HOST_CHANNEL_EVENT_CANCELLED\n");
                } break;

                case VBOX_HOST_CHANNEL_EVENT_UNREGISTERED:
                {
                    VBoxMMRHookLog("VBoxMMR: VBOX_HOST_CHANNEL_EVENT_UNREGISTERED\n");
                } break;

                case VBOX_HOST_CHANNEL_EVENT_RECV:
                {
                    VBOXHOSTCHANNELEVENTRECV *p =
                        (VBOXHOSTCHANNELEVENTRECV *)pvParm;

                    if (p->u32SizeAvailable > 0)
                    {
                        ReadTSMF(u32ChannelHandle, pCtx->u32HGCMClientId, p->u32SizeAvailable);
                    }
                    else
                    {
                        VBoxMMRHookLog("VBoxMMR: TSMF recv, channel: %d, available: 0\n", u32ChannelHandle);
                    }
                } break;

                default:
                {
                    VBoxMMRHookLog("VBoxMMR: unknown event id %d\n", u32EventId);
                } break;
            }
        }
    }

    VBoxMMRHookLog("VBoxMMR: MonitorTSMFChannel stopping\n");

    return VINF_SUCCESS;
}

void InstallHooksForModule(const char *pszName, HookEntry hooks[])
{
    HMODULE hMod = LoadLibraryA(pszName);
    if (hMod != NULL)
    {
        VBoxMMRHookLog("VBoxMMR: Hooking %s -> %x \n", pszName, hMod);
        const IMAGE_IMPORT_DESCRIPTOR *pDescriptor = GetImportDescriptor(hMod);
        InstallHooks(pDescriptor, (PBYTE) hMod, hooks);
    }
    else
    {
        VBoxMMRHookLog("VBoxMMR: Error hooking %s -> not found\n", pszName);
    }
}

TSMFHOOK_API LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    static bool bInit = false;

    if (isWMP && !bInit)
    {
        bInit = true;

        g_bMMRCodeCavingIsEnabled = MMRCodeCavingIsEnabled();

        VBoxMMRHookLog("VBoxMMR: WMP APIs Hooking started ...\n");

        HMODULE hMod = GetModuleHandleA("wmp");
        if (hMod != NULL)
        {
            VBoxMMRHookLog("VBoxMMR: Hooking wmp -> %x \n",hMod);
            const IMAGE_IMPORT_DESCRIPTOR *pDescriptor = GetImportDescriptor(hMod);
            InstallHooks(pDescriptor, (PBYTE) hMod, g_WMPHooks);
        }
        else
        {
            VBoxMMRHookLog("VBoxMMR: Error hooking wmp -> not found\n");
        }

        InstallHooksForModule("winmm.dll", g_WinMMHooks);
        InstallHooksForModule("tsmf.dll", g_TSMFHooks);
        InstallHooksForModule("DSHOWRDPFILTER.dll", g_TSMFHooks);
        InstallHooksForModule("MSMPEG2VDEC.dll", g_DShowHooks);
        InstallHooksForModule("MFDS.dll", g_DShowHooks);
        InstallHooksForModule("mf.dll", g_MFHooks);

        ULONG ret = RegisterTraceGuids(
            ControlCallback, NULL, &ProviderId, 0,
            NULL, NULL, NULL, &g_hTraceRegistration);

        if (ERROR_SUCCESS != ret)
        {
            VBoxMMRHookLog("VBoxMMR: RegisterTraceGuids failed with error code: %u\n", GetLastError());
        }

        bool bInRDPSession = (1 == GetSystemMetrics(0x1000));

        if (!bInRDPSession)
        {
            uint32_t u32HGCMClientId = 0;

            int rc = VbglR3HostChannelInit(&u32HGCMClientId);

            if (RT_SUCCESS(rc))
            {
                uint32_t u32Size = 0;

                rc = VbglR3HostChannelQuery(g_pszVRDETSMF, u32HGCMClientId,
                    VBOX_HOST_CHANNEL_CTRL_EXISTS, NULL, 0, NULL, 0, &u32Size);

                if (RT_SUCCESS(rc))
                {
                    hCreateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
                    InitializeCriticalSection(&CreateLock);

                    g_HostChannelCtx.thread = NIL_RTTHREAD;
                    g_HostChannelCtx.u32HGCMClientId = u32HGCMClientId;

                    StartMonitor(
                        &g_HostChannelCtx.thread, MonitorTSMFChannel,
                        &g_HostChannelCtx, 64*_1K,
                        RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                        "tsmfio");

                    VBoxMMROpenBaseDriver();
                }
                else
                {
                    VBoxMMRHookLog(
                        "VBoxMMR: TSMF HGCM unavailable: hgcmid: %d, rc: %d\n",
                        u32HGCMClientId, rc);

                    VbglR3HostChannelTerm(u32HGCMClientId);
                }
            }
            else
            {
                VBoxMMRHookLog("VBoxMMR: Error connecting to HGCM: %d\n", rc);
            }
        }
    }

    return CallNextHookEx(NULL /*ignored */, nCode, wParam, lParam);
}

void Shutdown()
{
    if (isWMP)
    {
        VBoxMMRHookLog("VBoxMMR: Shutdown\n");

        StopMonitor(&hDetachMonitor, "mmrpoll");
        VBoxMMRCloseBaseDriver();

        if (g_HostChannelCtx.u32HGCMClientId != 0)
        {
            g_HostChannelCtx.fShutdown = true;
            VbglR3HostChannelEventCancel(0, g_HostChannelCtx.u32HGCMClientId);
            StopMonitor(&g_HostChannelCtx.thread, "tsmfio");
            VbglR3HostChannelTerm(g_HostChannelCtx.u32HGCMClientId);
        }

        if (hCreateEvent)
        {
            CloseHandle(hCreateEvent);
            DeleteCriticalSection(&CreateLock);
        }

        if (g_hTraceRegistration)
            UnregisterTraceGuids(g_hTraceRegistration);
    }
}
