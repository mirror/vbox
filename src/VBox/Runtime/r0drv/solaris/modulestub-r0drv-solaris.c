


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <sys/modctl.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static struct modlmisc g_rtModuleStubMisc =
{
    &mod_miscops,           /* extern from kernel */
    "platform agnostic module"
};


static struct modlinkage g_rtModuleStubModLinkage =
{
    MODREV_1,   /* loadable module system revision */
    {
        &g_rtModuleStubMisc,
        NULL    /* terminate array of linkage structures */
    }
};



int _init(void);
int _init(void)
{
    /* Disable auto unloading. */
    modctl_t *pModCtl = mod_getctl(&g_rtModuleStubModLinkage);
    if (pModCtl)
        pModCtl->mod_loadflags |= MOD_NOAUTOUNLOAD;

    return mod_install(&g_rtModuleStubModLinkage);
}


int _fini(void);
int _fini(void)
{
    return mod_remove(&g_rtModuleStubModLinkage);
}


int _info(struct modinfo *pModInfo);
int _info(struct modinfo *pModInfo)
{
    return mod_info(&g_rtModuleStubModLinkage, pModInfo);
}

