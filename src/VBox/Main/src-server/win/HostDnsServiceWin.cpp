/* $Id$ */
/** @file
 * Host DNS listener for Windows.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/string.h>
#include <VBox/com/ptr.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/err.h>

#include <Windows.h>

#include <string>
#include <sstream>
#include <vector>
#include "../HostDnsService.h"

/* In order to monitor DNS setting updates we need to receive notification about
 * Computer\HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces\* keys changes.
 * Since it is not possible to use patterns when subscribing key changes, we need to find valid paths for all such
 * keys manually and subscribe to changes one by one (see enumerateSubTree()). */
const wchar_t HostDnsServiceWin::pwcKeyRoot[] = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces";


HostDnsServiceWin::HostDnsServiceWin()
    : HostDnsMonitor(true),
      m_aWarehouse(),
      m_hostInfoCache(),
      m_fInitialized(false)
{
    /* Add monitor destroy event.
     * This event should have index '0' at the events array in order to separate it from
     * DNS and tree change events. When this event occurs all resources should be released. */
    if (subscribeTo(NULL, NULL, 0))
    {
        /* Add registry tree change event and corresponding key.
         * This event should have index '1' at the events array in order to separate it from DNS events.
         * When this event occurs it means there are changes in the list of available network interfaces.
         * Network interfaces should be re-enumerated, all DNS events and keys data should re-initialized. */
        if (subscribeTo(pwcKeyRoot, NULL, REG_NOTIFY_CHANGE_NAME))
        {
            /* Enumerate all available network interfaces, create events and corresponding keys and perform subscription.  */
            if (enumerateSubTree())
            {
                updateInfo(NULL);
                m_fInitialized = true;
                return;
            }
            else
                LogRel(("WARNING: cannot set up monitor properly (3); monitor now disabled.\n"));
        }
        else
            /* Too bad we can't even subscribe to notifications about network interfaces changes. */
            LogRel(("WARNING: cannot set up monitor properly (2); monitor now disabled.\n"));
    }
    else
        /* Too bad we can't even subscribe to destroy event. */
        LogRel(("WARNING: cannot set up monitor properly (1); monitor now disabled.\n"));

    releaseResources();
}


HostDnsServiceWin::~HostDnsServiceWin()
{
    monitorThreadShutdown();
    releaseResources();
    m_fInitialized = false;
}


bool HostDnsServiceWin::releaseWarehouseItem(int idxItem)
{
    bool rc = true;
    /* We do not check if idxItem is in valid range of m_aWarehouse here
     * (a bit of performance optimization), so make sure you provided a valid value! */
    struct Item oTmpItem = m_aWarehouse[idxItem];

    /* Non-zero return code means ResetEvent() succeeded. */
    rc = ResetEvent(oTmpItem.hEvent) != 0;
    if (!rc) LogRel(("Failed to reset event (idxItem=%d); monitor unstable (rc=%d).\n", idxItem, GetLastError()));
    CloseHandle(oTmpItem.hEvent);
    oTmpItem.hEvent = NULL;

    RegCloseKey(oTmpItem.hKey);
    oTmpItem.hKey   = NULL;

    Log2(("Unsubscribed from %ls notifications\n", oTmpItem.wcsInterface));

    m_aWarehouse.erase(m_aWarehouse.begin() + idxItem);

    return rc;
}


bool HostDnsServiceWin::dropSubTreeNotifications()
{
    bool rc = true;
    /* Any sub-tree events we subscribed? */
    if (m_aWarehouse.size() > VBOX_OFFSET_SUBTREE_EVENTS)
        /* Going from the end to the beginning. */
        for (int idxItem = (int)m_aWarehouse.size() - 1; idxItem >= VBOX_OFFSET_SUBTREE_EVENTS; idxItem--)
            rc &= releaseWarehouseItem(idxItem);

    size_t cElementsLeft = m_aWarehouse.size();
    if (cElementsLeft != VBOX_OFFSET_SUBTREE_EVENTS)
    {
        LogRel(("DNS monitor unstable; %d events left after dropping.\n", (int)cElementsLeft - VBOX_OFFSET_SUBTREE_EVENTS));
        return false;
    }

    return rc;
}


void HostDnsServiceWin::releaseResources()
{
    /* First, drop notifications subscription for sub-tree keys. */
    dropSubTreeNotifications();

    /* Then release notification about tree structure changes. */
    if (m_aWarehouse.size() > VBOX_OFFSET_TREE_EVENT)
        releaseWarehouseItem(VBOX_OFFSET_TREE_EVENT);

    /* Release shutdown event. */
    if (m_aWarehouse.size() > VBOX_OFFSET_SHUTDOWN_EVENT)
        releaseWarehouseItem(VBOX_OFFSET_SHUTDOWN_EVENT);

    AssertReturnVoid(m_aWarehouse.size() == 0);
}


bool HostDnsServiceWin::subscribeTo(const wchar_t *wcsPath, const wchar_t *wcsInterface, DWORD fFilter)
{
    HKEY   hTmpKey    = NULL;
    HANDLE hTmpEvent  = NULL;

    /* Do not add more than MAXIMUM_WAIT_OBJECTS items to the array due to WaitForMultipleObjects() limitation. */
    if ((m_aWarehouse.size() + 1 /* the array size if we would add an extra item */ ) > MAXIMUM_WAIT_OBJECTS)
    {
        LogRel(("Too many items to monitor.\n"));
        return false;
    }

    hTmpEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!hTmpEvent)
        return false;

    /* wcsPath might not be specified if we want to subscribe to the termination event. In this case
     * it is assumed that this is the first issued subscription request (i.e., m_aWarehouse.size() == 0). */
    if (wcsPath)
    {
        LONG rc;
        /* Open registry key itself. */
        rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, wcsPath, 0, KEY_READ | KEY_NOTIFY, &hTmpKey);
        if (rc == ERROR_SUCCESS)
        {
            /* Link registry key and notification event. */
            rc = RegNotifyChangeKeyValue(hTmpKey, TRUE, fFilter, hTmpEvent, TRUE);
            if (rc != ERROR_SUCCESS)
            {
                /* Don't leak! */
                RegCloseKey(hTmpKey);
                LogRel(("Unable to register key notification (rc=0x%X).\n", rc));
            }
        }
        else
            LogRel(("Unable to open key (rc=0x%X)\n", rc));

        /* All good so far? */
        if (rc != ERROR_SUCCESS)
        {
            LogRel(("WARNING: unable to set up %ls registry key notifications.\n", wcsPath));
            CloseHandle(hTmpEvent);
            return false;
        }
    }
    else if (m_aWarehouse.size() > 0)
    {
        LogRel(("Subscription to termination event already established.\n"));
        CloseHandle(hTmpEvent);
        return false;
    }

    /* Finally, construct array item and queue it. */
    struct Item oTmpItem = { hTmpKey, hTmpEvent, NULL };

    /* Sub-tree keys should provide interface name (UUID). This is needed in order to
     * collect all useful network settings to HostDnsInformation storage object to provide it to parent class. */
    if (wcsInterface)
        wcscpy(oTmpItem.wcsInterface, wcsInterface);

    if (wcsPath)
        Log2(("Subscription to %ls established.\n", wcsPath));

    m_aWarehouse.push_back(oTmpItem);

    return true;
}


bool HostDnsServiceWin::enumerateSubTree()
{
    LONG  rc = 0;
    HKEY  hTmpKey;
    DWORD cSubKeys = 0;
    DWORD cbSubKeyNameMax = 0;

    /* Enumerate all the available interfaces. */
    rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, pwcKeyRoot, 0, KEY_READ, &hTmpKey);
    if (rc == ERROR_SUCCESS)
    {
        /* Get info about amount of available network interfaces. */
        rc = RegQueryInfoKeyW(hTmpKey, NULL, NULL, NULL, &cSubKeys, &cbSubKeyNameMax, NULL, NULL, NULL, NULL, NULL, NULL);
        if (rc == ERROR_SUCCESS)
        {
            /* Now iterate over interfaces if:
             * 1) there are interfaces available and
             * 2) maximum length of an interface name conforms to our buffer allocation size. */
            if (cSubKeys > 0 && cbSubKeyNameMax <= VBOX_KEY_NAME_LEN_MAX)
            {
                wchar_t sSubKeyName[VBOX_KEY_NAME_LEN_MAX];
                for (DWORD idxSubKey = 0; idxSubKey < cSubKeys; idxSubKey++)
                {
                    rc = RegEnumKeyW(hTmpKey, idxSubKey, sSubKeyName, VBOX_KEY_NAME_LEN_MAX);
                    if (rc == ERROR_SUCCESS)
                    {
                        /* Since we already know interface name (actually UUID), construct full registry path here. */
                        wchar_t sSubKeyFullPath[VBOX_KEY_NAME_LEN_MAX];
                        RT_ZERO(sSubKeyFullPath);
                        wcscpy(sSubKeyFullPath, pwcKeyRoot);
                        rc  = wcscat_s(sSubKeyFullPath, VBOX_KEY_NAME_LEN_MAX, L"\\");
                        rc |= wcscat_s(sSubKeyFullPath, VBOX_KEY_NAME_LEN_MAX, sSubKeyName);
                        if (rc == 0)
                            subscribeTo(sSubKeyFullPath, sSubKeyName, REG_NOTIFY_CHANGE_LAST_SET);
                    }
                    else
                        LogRel(("Unable to open interfaces list (1).\n"));
                }
                RegCloseKey(hTmpKey);
                return true;
            }
            else
                LogRel(("Unable to open interfaces list (2).\n"));
        }
        else
            LogRel(("Unable to open interfaces list (3).\n"));
        RegCloseKey(hTmpKey);
    }
    else
        LogRel(("Unable to open interfaces list (4).\n"));
    return false;
}


HRESULT HostDnsServiceWin::init()
{
    HRESULT hrc = HostDnsMonitor::init();
    AssertComRCReturn(hrc, hrc);

    return updateInfo(NULL);
}


void HostDnsServiceWin::monitorThreadShutdown()
{
    AssertReturnVoid(m_aWarehouse.size() > VBOX_OFFSET_SHUTDOWN_EVENT);
    SetEvent(m_aWarehouse[VBOX_OFFSET_SHUTDOWN_EVENT].hEvent);
}


void HostDnsServiceWin::extendVectorWithStrings(std::vector<std::string> &pVectorToExtend, const std::wstring &wcsParameter)
{
    std::wstringstream   wcsStream(wcsParameter);
    std::wstring         wcsSubString;

    while (std::getline(wcsStream, wcsSubString, L' '))
    {
        std::string str = std::_Narrow_str(wcsSubString);
        pVectorToExtend.push_back(str);
    }
}

#ifdef DEBUG
static void hostDnsWinDumpList(const std::vector<std::string> &awcszValues)
{
    for (int idxItem = 0; idxItem < awcszValues.size(); idxItem++)
    {
        LogRel(("%s\n", awcszValues[idxItem].c_str()));
    }
}
#endif /* DEBUG */


HRESULT HostDnsServiceWin::updateInfo(uint8_t *fWhatsChanged)
{
    HostDnsInformation pHostDnsInfo;
    RT_ZERO(pHostDnsInfo);

    /* Any interfaces available? */
    if (m_aWarehouse.size() > VBOX_OFFSET_SUBTREE_EVENTS)
    {
        /* Walk across all the available interfaces and collect network configuration data:
         * domain name, name servers and search list. */
        for (int idxKey = VBOX_OFFSET_SUBTREE_EVENTS; idxKey < m_aWarehouse.size(); idxKey++)
        {
            LONG rc;

            /* Get number of key values. */
            DWORD cValues = 0;
            rc = RegQueryInfoKeyW(m_aWarehouse[idxKey].hKey, NULL, NULL, NULL, NULL, NULL, NULL, &cValues, NULL, NULL, NULL, NULL);
            if (rc == ERROR_SUCCESS)
            {
                for (DWORD idxValue = 0; idxValue < cValues; idxValue++)
                {
                    wchar_t wcsValueName[VBOX_KEY_NAME_LEN_MAX];
                    DWORD cbValueName = VBOX_KEY_NAME_LEN_MAX;
                    wchar_t wcsData[VBOX_KEY_NAME_LEN_MAX];
                    DWORD cbData = VBOX_KEY_NAME_LEN_MAX;

                    /* Walk across all the properties of given interface. */
                    rc = RegEnumValueW(m_aWarehouse[idxKey].hKey, idxValue, wcsValueName, &cbValueName, 0, NULL, (LPBYTE)wcsData, &cbData);
                    if (rc == ERROR_SUCCESS)
                    {

                        if ((   wcscmp(wcsValueName, L"Domain") == 0
                             || wcscmp(wcsValueName, L"DhcpDomain") == 0)
                            && wcslen(wcsData) > 0)
                        {
                            /* We rely on that fact that Windows host cannot be a member of more than one domain in the same time! */
                            if (pHostDnsInfo.domain.empty())
                                pHostDnsInfo.domain = std::_Narrow_str(std::wstring(wcsData));
                        }
                        else if ((   wcscmp(wcsValueName, L"NameServer") == 0
                                  || wcscmp(wcsValueName, L"DhcpNameServer") == 0)
                                 && wcslen(wcsData) > 0)
                        {
                            extendVectorWithStrings(pHostDnsInfo.servers, std::wstring(wcsData));
                        }
                        else if (wcscmp(wcsValueName, L"SearchList") == 0
                                 && wcslen(wcsData) > 0)
                        {
                            extendVectorWithStrings(pHostDnsInfo.searchList, std::wstring(wcsData));
                        }
                    }
                }
            }
        }

        uint8_t fChanged = VBOX_EVENT_NO_CHANGES;
        /* Compare cached network settings and newly obtained ones. */
        if (pHostDnsInfo.servers != m_hostInfoCache.servers)
        {
#ifdef DEBUG
            LogRel(("Servers changed from:\n"));
            hostDnsWinDumpList(m_hostInfoCache.servers);
            LogRel(("to:\n"));
            hostDnsWinDumpList(pHostDnsInfo.servers);
#endif /* DEBUG */
            fChanged |= VBOX_EVENT_SERVERS_CHANGED;
        }

        if (pHostDnsInfo.domain != m_hostInfoCache.domain)
        {
#ifdef DEBUG
            LogRel(("Domain changed: [%s]->[%s].\n",
                    m_hostInfoCache.domain.empty() ? "NONE" : m_hostInfoCache.domain.c_str(),
                    pHostDnsInfo.domain.c_str()));
#endif /* DEBUG */
            fChanged |= VBOX_EVENT_DOMAIN_CHANGED;
        }

        if (pHostDnsInfo.searchList != m_hostInfoCache.searchList)
        {
#ifdef DEBUG
            LogRel(("SearchList changed from:\n"));
            hostDnsWinDumpList(m_hostInfoCache.searchList);
            LogRel(("to:\n"));
            hostDnsWinDumpList(pHostDnsInfo.searchList);
#endif /* DEBUG */
            fChanged |= VBOX_EVENT_SEARCHLIST_CHANGED;
        }

        /* Provide info about changes if requested. */
        if (fWhatsChanged)
            *fWhatsChanged = fChanged;

        /* Update host network configuration cache. */
        m_hostInfoCache.servers.clear();
        m_hostInfoCache.servers = pHostDnsInfo.servers;
        m_hostInfoCache.domain.clear();
        m_hostInfoCache.domain.assign(pHostDnsInfo.domain);
        m_hostInfoCache.searchList.clear();
        m_hostInfoCache.searchList = pHostDnsInfo.searchList;

        HostDnsMonitor::setInfo(pHostDnsInfo);
    }

    return S_OK;
}


void HostDnsServiceWin::getEventHandles(HANDLE *ahEvents)
{
    AssertReturnVoid(m_aWarehouse.size() > 0);
    for (int idxHandle = 0; idxHandle < m_aWarehouse.size(); idxHandle++)
        ahEvents[idxHandle] = m_aWarehouse[idxHandle].hEvent;
}


int HostDnsServiceWin::monitorWorker()
{
    monitorThreadInitializationDone();

    uint8_t fWhatsChabged = VBOX_EVENT_NO_CHANGES;

    if (!m_fInitialized)
    {
        LogRel(("Host DNS monitor was not initialized properly.\n"));
        return VERR_INTERNAL_ERROR;
    }

    HANDLE  ahEvents[MAXIMUM_WAIT_OBJECTS];

    while (true)
    {
        /* Each new iteration we need to update event handles list we monitor. */
        RT_ZERO(ahEvents);
        getEventHandles(ahEvents);

        DWORD rc = WaitForMultipleObjects((DWORD)m_aWarehouse.size(), ahEvents, FALSE, INFINITE);

        AssertMsgReturn(rc != WAIT_FAILED,
                        ("WaitForMultipleObjects failed (%d) to wait! Please debug",
                         GetLastError()), VERR_INTERNAL_ERROR);

        /* Shutdown requested. */
        if      (rc == (WAIT_OBJECT_0 + VBOX_OFFSET_SHUTDOWN_EVENT)) break;
        /* Interfaces amount changed. */
        else if (rc == (WAIT_OBJECT_0 + VBOX_OFFSET_TREE_EVENT))
        {
            Log2(("Network interfaces amount changed.\n"));

            /* Drop interface events. */
            if (dropSubTreeNotifications())
            {
                /* Drop event which is corresponds to interfaces tree changes. */
                if (releaseWarehouseItem(VBOX_OFFSET_TREE_EVENT))
                {
                    /* Restart interface tree monitoring. */
                    if (subscribeTo(pwcKeyRoot, NULL, REG_NOTIFY_CHANGE_NAME))
                    {
                        /* Restart interface events. */
                        if (enumerateSubTree())
                        {
                            Log2(("Monitor restarted successfully.\n"));
                            fWhatsChabged = VBOX_EVENT_NO_CHANGES;
                            updateInfo(&fWhatsChabged);
                            if (fWhatsChabged & VBOX_EVENT_SERVERS_CHANGED)
                            {
                                LogRel(("Notification sent (1).\n"));
                                notifyAll();
                            }
                            continue;
                        }
                        else
                            LogRel(("Monitor unstable: failed to subscribe network configuration changes.\n"));
                    }
                    else
                        LogRel(("Monitor unstable: failed to subscribe interface changes.\n"));
                }
                else
                    LogRel(("Monitor unstable: failed to unsubscribe from interfaces amount changes.\n"));
            }
            else
                LogRel(("Monitor unstable: failed to unsubscribe from previous notifications.\n"));

            /* If something went wrong, we break monitoring. */
            break;

        }
        /* DNS update events range. */
        else if (rc >= (WAIT_OBJECT_0 + VBOX_OFFSET_SUBTREE_EVENTS) &&
                 rc <  (WAIT_OBJECT_0 + m_aWarehouse.size()))
        {
            Log2(("Network setting has changed at interface %ls.\n", m_aWarehouse[rc - WAIT_OBJECT_0].wcsInterface));

            /* Drop previous notifications first. */
            if (dropSubTreeNotifications())
            {
                /* Re-subscribe. */
                if (enumerateSubTree())
                {
                    Log2(("Restart monitoring.\n"));
                    fWhatsChabged = VBOX_EVENT_NO_CHANGES;
                    updateInfo(&fWhatsChabged);
                    if (fWhatsChabged & VBOX_EVENT_SERVERS_CHANGED)
                    {
                        LogRel(("Notification sent (2).\n"));
                        notifyAll();
                    }
                    continue;
                }
                else
                    LogRel(("WARNING: Monitor unstable: unable to re-subscribe to notifications.\n"));
            }
            else
                LogRel(("WARNING: Monitor unstable: failed to unsubscribe from previous notifications.\n"));

            /* If something went wrong, we stop monitoring. */
            break;
        }
        else
            AssertMsgFailedReturn(("WaitForMultipleObjects returns out of bound (%d) index %d. Please debug!\n", m_aWarehouse.size(), rc), VERR_INTERNAL_ERROR);
    }
    LogRel(("Monitor thread exited.\n"));
    return VINF_SUCCESS;
}
