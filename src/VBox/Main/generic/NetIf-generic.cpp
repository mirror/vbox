#include "HostNetworkInterfaceImpl.h"
#include "netif.h"

int NetIfEnableStaticIpConfig(HostNetworkInterface * pIf, ULONG ip, ULONG mask)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfEnableStaticIpConfigV6(HostNetworkInterface * pIf, IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
    return VERR_NOT_IMPLEMENTED;
}

int NetIfEnableDynamicIpConfig(HostNetworkInterface * pIf)
{
    return VERR_NOT_IMPLEMENTED;
}
