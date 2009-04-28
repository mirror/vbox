/* $Id$ */
/** @file
 * VirtualBox Main - Generic NetIf implementation.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/process.h>
#include <iprt/env.h>
#include <iprt/path.h>
#include <iprt/param.h>

#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN)
# include <cstdio>
#endif

#include "HostNetworkInterfaceImpl.h"
#include "ProgressImpl.h"
#include "VirtualBoxImpl.h"
#include "netif.h"

#define VBOXNETADPCTL_NAME "VBoxNetAdpCtl"

static int NetIfAdpCtl(const char * pcszIfName, const char *pszAddr, const char *pszOption, const char *pszMask)
{
    const char *args[] = { NULL, pcszIfName, pszAddr, pszOption, pszMask, NULL };

    char szAdpCtl[RTPATH_MAX];
    int rc = RTPathProgram(szAdpCtl, sizeof(szAdpCtl) - sizeof("/" VBOXNETADPCTL_NAME));
    if (RT_FAILURE(rc))
    {
        LogRel(("NetIfAdpCtl: failed to get program path, rc=%Vrc.\n", rc));
        return rc;
    }
    strcat(szAdpCtl, "/" VBOXNETADPCTL_NAME);
    args[0] = szAdpCtl;
    if (!RTPathExists(szAdpCtl))
    {
        LogRel(("NetIfAdpCtl: path %s does not exist. Failed to run " VBOXNETADPCTL_NAME " helper.\n",
                szAdpCtl));
        return VERR_FILE_NOT_FOUND;
    }

    RTPROCESS pid;
    rc = RTProcCreate(szAdpCtl, args, RTENV_DEFAULT, 0, &pid);
    if (RT_SUCCESS(rc))
    {
        RTPROCSTATUS Status;
        rc = RTProcWait(pid, 0, &Status);
        if (   RT_SUCCESS(rc)
            && Status.iStatus == 0
            && Status.enmReason == RTPROCEXITREASON_NORMAL)
            return VINF_SUCCESS;
    }
    else
        LogRel(("NetIfAdpCtl: failed to create process for %.\n",
                szAdpCtl));
    return rc;
}

static int NetIfAdpCtl(HostNetworkInterface * pIf, const char *pszAddr, const char *pszOption, const char *pszMask)
{
    Bstr interfaceName;
    pIf->COMGETTER(Name)(interfaceName.asOutParam());
    Utf8Str strName(interfaceName);
    return NetIfAdpCtl(strName, pszAddr, pszOption, pszMask);
}

int NetIfEnableStaticIpConfig(VirtualBox * /* vBox */, HostNetworkInterface * pIf, ULONG aOldIp, ULONG aNewIp, ULONG aMask)
{
    const char *pszOption, *pszMask;
    char szAddress[16]; /* 4*3 + 3*1 + 1 */
    char szNetMask[16]; /* 4*3 + 3*1 + 1 */
    uint8_t *pu8Addr = (uint8_t *)&aNewIp;
    uint8_t *pu8Mask = (uint8_t *)&aMask;
    if (aNewIp == 0)
    {
        pu8Addr = (uint8_t *)&aOldIp;
        pszOption = "remove";
        pszMask   = NULL;
    }
    else
    {
        pszOption = "netmask";
        pszMask  = szNetMask;
        RTStrPrintf(szNetMask, sizeof(szNetMask), "%d.%d.%d.%d",
                    pu8Mask[0], pu8Mask[1], pu8Mask[2], pu8Mask[3]);
    }
    RTStrPrintf(szAddress, sizeof(szAddress), "%d.%d.%d.%d",
                pu8Addr[0], pu8Addr[1], pu8Addr[2], pu8Addr[3]);
    return NetIfAdpCtl(pIf, szAddress, pszOption, pszMask);
}

int NetIfEnableStaticIpConfigV6(VirtualBox * /* vBox */, HostNetworkInterface * pIf, IN_BSTR aOldIPV6Address, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
    char szAddress[5*8 + 1 + 5 + 1];
    if (Bstr(aIPV6Address).length())
    {
        RTStrPrintf(szAddress, sizeof(szAddress), "%ls/%d",
                    aIPV6Address, aIPV6MaskPrefixLength);
        return NetIfAdpCtl(pIf, szAddress, NULL, NULL);
    }
    else
    {
        RTStrPrintf(szAddress, sizeof(szAddress), "%ls",
                    aOldIPV6Address);
        return NetIfAdpCtl(pIf, szAddress, "remove", NULL);
    }
}

int NetIfEnableDynamicIpConfig(VirtualBox * /* vBox */, HostNetworkInterface * /* pIf */)
{
    return VERR_NOT_IMPLEMENTED;
}


int NetIfCreateHostOnlyNetworkInterface (VirtualBox *pVBox, IHostNetworkInterface **aHostNetworkInterface, IProgress **aProgress)
{
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN)
    /* create a progress object */
    ComObjPtr <Progress> progress;
    progress.createObject();

    ComPtr<IHost> host;
    HRESULT rc = pVBox->COMGETTER(Host)(host.asOutParam());
    if(SUCCEEDED(rc))
    {
        rc = progress->init (pVBox, host,
                             Bstr ("Creating host only network interface"),
                             FALSE /* aCancelable */);
        if(SUCCEEDED(rc))
        {
            CheckComRCReturnRC (rc);
            progress.queryInterfaceTo (aProgress);

            char szAdpCtl[RTPATH_MAX];
            int rc = RTPathProgram(szAdpCtl, sizeof(szAdpCtl) - sizeof("/" VBOXNETADPCTL_NAME " add"));
            if (RT_FAILURE(rc))
            {
                progress->notifyComplete(E_FAIL, COM_IIDOF(IHostNetworkInterface), HostNetworkInterface::getComponentName(),
                                         "Failed to get program path, rc=%Vrc.\n", rc);
                return rc;
            }
            strcat(szAdpCtl, "/" VBOXNETADPCTL_NAME " add");
            FILE *fp = popen(szAdpCtl, "r");

            if (fp)
            {
                char szBuf[VBOXNET_MAX_SHORT_NAME];
                if (fgets(szBuf, sizeof(szBuf), fp))
                {
                    char *pLast = szBuf + strlen(szBuf) - 1;
                    if (pLast >= szBuf && *pLast == '\n')
                        *pLast = 0;

                    NETIFINFO Info;
                    Bstr IfName(szBuf);
                    rc = NetIfGetConfigByName(IfName, &Info);
                    if (RT_FAILURE(rc))
                    {
                        progress->notifyComplete(E_FAIL, COM_IIDOF(IHostNetworkInterface), HostNetworkInterface::getComponentName(),
                                                 "Failed to get config info for %s (as reported by '" VBOXNETADPCTL_NAME " add').\n", szBuf);
                    }
                    else
                    {
                        /* create a new uninitialized host interface object */
                        ComObjPtr <HostNetworkInterface> iface;
                        iface.createObject();
                        iface->init(IfName, HostNetworkInterfaceType_HostOnly, &Info);
                        iface.queryInterfaceTo (aHostNetworkInterface);
                    }
                }
                if ((rc = pclose(fp)) != 0)
                {
                    progress->notifyComplete(E_FAIL, COM_IIDOF(IHostNetworkInterface), HostNetworkInterface::getComponentName(), "Failed to execute '"VBOXNETADPCTL_NAME " add' (exit status: %d).", rc);
                    rc = VERR_INTERNAL_ERROR;
                }
            }
            if (RT_SUCCESS(rc))
                progress->notifyComplete(rc);
        }
    }

    return rc;

#else
    return VERR_NOT_IMPLEMENTED;
#endif
}

int NetIfRemoveHostOnlyNetworkInterface (VirtualBox *pVBox, IN_GUID aId, IHostNetworkInterface **aHostNetworkInterface, IProgress **aProgress)
{
#if defined(RT_OS_LINUX) || defined(RT_OS_DARWIN)
    /* create a progress object */
    ComObjPtr <Progress> progress;
    progress.createObject();
    ComPtr<IHost> host;
    int rc = VINF_SUCCESS;
    HRESULT hr = pVBox->COMGETTER(Host)(host.asOutParam());
    if(SUCCEEDED(hr))
    {
        Bstr ifname;
        ComPtr <IHostNetworkInterface> iface;
        if (FAILED (host->FindHostNetworkInterfaceById (aId, iface.asOutParam())))
            return VERR_INVALID_PARAMETER;
        iface->COMGETTER (Name) (ifname.asOutParam());
        if (ifname.isNull())
            return VERR_INTERNAL_ERROR;

        rc = progress->init (pVBox, host,
                            Bstr ("Removing host network interface"),
                            FALSE /* aCancelable */);
        if(SUCCEEDED(rc))
        {
            CheckComRCReturnRC (rc);
            progress.queryInterfaceTo (aProgress);
            iface.queryInterfaceTo (aHostNetworkInterface);
            rc = NetIfAdpCtl(Utf8Str(ifname), "remove", NULL, NULL);
            if (RT_FAILURE(rc))
                progress->notifyComplete(E_FAIL, COM_IIDOF(IHostNetworkInterface), HostNetworkInterface::getComponentName(), "Failed to execute '"VBOXNETADPCTL_NAME "' (exit status: %d).", rc);
            else
                progress->notifyComplete(S_OK);
        }
    }
    else
    {
        progress->notifyComplete(hr);
        rc = VERR_INTERNAL_ERROR;
    }
    return rc;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}

int NetIfGetConfig(HostNetworkInterface * /* pIf */, NETIFINFO *)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfDhcpRediscover(VirtualBox * /* pVbox */, HostNetworkInterface * /* pIf */)
{
    return VERR_NOT_IMPLEMENTED;
}

