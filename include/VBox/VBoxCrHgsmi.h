#ifndef ___VBoxCrHgsmi_h__
#define ___VBoxCrHgsmi_h__

#include <iprt/cdefs.h>
#include <VBox/VBoxUhgsmi.h>

RT_C_DECLS_BEGIN

#if 0
/* enable this in case we include this in a dll*/
# ifdef IN_VBOXCRHGSMI
#  define VBOXCRHGSMI_DECL(_type) DECLEXPORT(_type)
# else
#  define VBOXCRHGSMI_DECL(_type) DECLIMPORT(_type)
# endif
#else
/*enable this in case we include this in a static lib*/
# define VBOXCRHGSMI_DECL(_type) _type
#endif

typedef void * HVBOXCRHGSMI_CLIENT;

typedef DECLCALLBACK(HVBOXCRHGSMI_CLIENT) FNVBOXCRHGSMI_CLIENT_CREATE(PVBOXUHGSMI pHgsmi);
typedef FNVBOXCRHGSMI_CLIENT_CREATE *PFNVBOXCRHGSMI_CLIENT_CREATE;

typedef DECLCALLBACK(void) FNVBOXCRHGSMI_CLIENT_DESTROY(HVBOXCRHGSMI_CLIENT hClient);
typedef FNVBOXCRHGSMI_CLIENT_DESTROY *PFNVBOXCRHGSMI_CLIENT_DESTROY;

typedef struct VBOXCRHGSMI_CALLBACKS
{
    PFNVBOXCRHGSMI_CLIENT_CREATE pfnClientCreate;
    PFNVBOXCRHGSMI_CLIENT_DESTROY pfnClientDestroy;
} VBOXCRHGSMI_CALLBACKS, *PVBOXCRHGSMI_CALLBACKS;

VBOXCRHGSMI_DECL(int) VBoxCrHgsmiInit(PVBOXCRHGSMI_CALLBACKS pCallbacks);
VBOXCRHGSMI_DECL(int) VBoxCrHgsmiTerm();
VBOXCRHGSMI_DECL(HVBOXCRHGSMI_CLIENT) VBoxCrHgsmiQueryClient();

RT_C_DECLS_END

#endif /* #ifndef ___VBoxCrHgsmi_h__ */
