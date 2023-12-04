/* The usual story: drag stuff from the libraries into the link. */


#include <plstr.h>
#include <nsDeque.h>
#include <nsHashSets.h>
#include <xptcall.h>
#include <nsProxyRelease.h>
#include "xpcom/proxy/src/nsProxyEventPrivate.h"
#include "nsTraceRefcnt.h"
#include "nsDebug.h"
#include "nsString.h"

uintptr_t deps[] =
{
    (uintptr_t)PL_strcasecmp,
    (uintptr_t)PL_HashString,
    (uintptr_t)NS_ProxyRelease,
    (uintptr_t)nsTraceRefcnt::LogRelease,
    (uintptr_t)nsDebug::Assertion,
    0
};

class foobardep : public nsXPTCStubBase
{
public:
    NS_IMETHOD_(nsrefcnt) AddRef(void)
    {
        return 1;
    }

    NS_IMETHOD_(nsrefcnt) Release(void)
    {
        return 0;
    }

    NS_IMETHOD GetInterfaceInfo(nsIInterfaceInfo** info)
    {
        (void)info;
        return 0;
    }

    // call this method and return result
    NS_IMETHOD CallMethod(PRUint16 methodIndex, const nsXPTMethodInfo* info, nsXPTCMiniVariant* params)
    {
        (void)methodIndex;
        (void)info;
        (void)params;
        return 0;
    }

};



void foodep(void)
{
    nsVoidHashSetSuper *a = new nsVoidHashSetSuper();
    a->Init(123);
    nsDeque *b = new nsDeque();

    //nsXPTCStubBase
    nsProxyEventObject *c = new nsProxyEventObject();
    c->Release();

    foobardep *d = new foobardep();
    nsXPTCStubBase *e = d;
    e->Release();

    // Dragged in by TestCRT.
    nsAutoString t1; 
    t1.AssignWithConversion(NULL);
}


