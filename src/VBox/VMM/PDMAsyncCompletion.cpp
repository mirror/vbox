/* $Id$ */
/** @file
 * PDM Async I/O - Transport data asynchronous in R3 using EMT.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_ASYNC_COMPLETION
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/rem.h>
#include <VBox/vm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/mem.h>
#include <iprt/critsect.h>
#include <iprt/tcp.h>
#include <iprt/path.h>

#include <VBox/pdmasynccompletion.h>
#include "PDMAsyncCompletionInternal.h"

/**
 * Async I/O type.
 */
typedef enum PDMASYNCCOMPLETIONTEMPLATETYPE
{
    /** Device . */
    PDMASYNCCOMPLETIONTEMPLATETYPE_DEV = 1,
    /** Driver consumer. */
    PDMASYNCCOMPLETIONTEMPLATETYPE_DRV,
    /** Internal consumer. */
    PDMASYNCCOMPLETIONTEMPLATETYPE_INTERNAL,
    /** Usb consumer. */
    PDMASYNCCOMPLETIONTEMPLATETYPE_USB
} PDMASYNCTEMPLATETYPE;

/**
 * PDM Async I/O template.
 */
typedef struct PDMASYNCCOMPLETIONTEMPLATE
{
    /** Pointer to the next template in the list. */
    R3PTRTYPE(PPDMASYNCCOMPLETIONTEMPLATE)    pNext;
    /** Pointer to the previous template in the list. */
    R3PTRTYPE(PPDMASYNCCOMPLETIONTEMPLATE)    pPrev;
    /** Type specific data. */
    union
    {
        /** PDMASYNCCOMPLETIONTEMPLATETYPE_DEV */
        struct
        {
            /** Pointer to consumer function. */
            R3PTRTYPE(PFNPDMASYNCCOMPLETEDEV)   pfnCompleted;
            /** Pointer to the device instance owning the template. */
            R3PTRTYPE(PPDMDEVINS)               pDevIns;
        } Dev;
        /** PDMASYNCCOMPLETIONTEMPLATETYPE_DRV */
        struct
        {
            /** Pointer to consumer function. */
            R3PTRTYPE(PFNPDMASYNCCOMPLETEDRV)   pfnCompleted;
            /** Pointer to the driver instance owning the template. */
            R3PTRTYPE(PPDMDRVINS)               pDrvIns;
            /** User agument given during template creation.
             *  This is only here to make things much easier
             *  for DrVVD. */
            void                               *pvTemplateUser;
        } Drv;
        /** PDMASYNCCOMPLETIONTEMPLATETYPE_INTERNAL */
        struct
        {
            /** Pointer to consumer function. */
            R3PTRTYPE(PFNPDMASYNCCOMPLETEINT)   pfnCompleted;
            /** Pointer to user data. */
            R3PTRTYPE(void *)                   pvUser;
        } Int;
        /** PDMASYNCCOMPLETIONTEMPLATETYPE_USB */
        struct
        {
            /** Pointer to consumer function. */
            R3PTRTYPE(PFNPDMASYNCCOMPLETEUSB)   pfnCompleted;
            /** Pointer to the usb instance owning the template. */
            R3PTRTYPE(PPDMUSBINS)               pUsbIns;
        } Usb;
    } u;
    /** Template type. */
    PDMASYNCCOMPLETIONTEMPLATETYPE          enmType;
    /** Pointer to the VM. */
    R3PTRTYPE(PVM)                          pVM;
    /** Use count of the template. */
    volatile uint32_t                       cUsed;
} PDMASYNCCOMPLETIONTEMPLATE;

static void pdmR3AsyncCompletionPutTask(PPDMASYNCCOMPLETIONENDPOINT pEndpoint, PPDMASYNCCOMPLETIONTASK pTask);

/**
 * Internal worker for the creation apis
 *
 * @returns VBox status.
 * @param   pVM           VM handle.
 * @param   ppTemplate    Where to store the template handle.
 */
static int pdmR3AsyncCompletionTemplateCreate(PVM pVM, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PDMASYNCCOMPLETIONTEMPLATETYPE enmType)
{
    int rc = VINF_SUCCESS;

    if (ppTemplate == NULL)
    {
        AssertMsgFailed(("ppTemplate is NULL\n"));
        return VERR_INVALID_PARAMETER;
    }

    PPDMASYNCCOMPLETIONTEMPLATE pTemplate;
    rc = MMR3HeapAllocZEx(pVM, MM_TAG_PDM_ASYNC_COMPLETION, sizeof(PDMASYNCCOMPLETIONTEMPLATE), (void **)&pTemplate);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Initialize fields.
     */
    pTemplate->pVM = pVM;
    pTemplate->cUsed = 0;
    pTemplate->enmType = enmType;

    /*
     * Add template to the global VM template list.
     */
    pTemplate->pNext = pVM->pdm.s.pAsyncCompletionTemplates;
    if (pVM->pdm.s.pAsyncCompletionTemplates)
        pVM->pdm.s.pAsyncCompletionTemplates->pPrev = pTemplate;
    pVM->pdm.s.pAsyncCompletionTemplates = pTemplate;

    *ppTemplate = pTemplate;
    return VINF_SUCCESS;
}

/**
 * Creates a async completion template for a device instance.
 *
 * The template is used when creating new completion tasks.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDevIns         The device instance.
 * @param   ppTemplate      Where to store the template pointer on success.
 * @param   pfnCompleted    The completion callback routine.
 * @param   pszDesc         Description.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateCreateDevice(PVM pVM, PPDMDEVINS pDevIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEDEV pfnCompleted, const char *pszDesc)
{
    LogFlow(("%s: pDevIns=%p ppTemplate=%p pfnCompleted=%p pszDesc=%s\n",
              __FUNCTION__, pDevIns, ppTemplate, pfnCompleted, pszDesc));

    /*
     * Validate input.
     */
    VM_ASSERT_EMT(pVM);
    if (!pfnCompleted)
    {
        AssertMsgFailed(("No completion callback!\n"));
        return VERR_INVALID_PARAMETER;
    }

    if (!ppTemplate)
    {
        AssertMsgFailed(("Template pointer is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the template.
     */
    PPDMASYNCCOMPLETIONTEMPLATE pTemplate;
    int rc = pdmR3AsyncCompletionTemplateCreate(pVM, &pTemplate, PDMASYNCCOMPLETIONTEMPLATETYPE_DEV);
    if (RT_SUCCESS(rc))
    {
        pTemplate->u.Dev.pDevIns = pDevIns;
        pTemplate->u.Dev.pfnCompleted = pfnCompleted;

        *ppTemplate = pTemplate;
        Log(("PDM: Created device template %p: pfnCompleted=%p pDevIns=%p\n",
             pTemplate, pfnCompleted, pDevIns));
    }

    return rc;
}

/**
 * Creates a async completion template for a driver instance.
 *
 * The template is used when creating new completion tasks.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDrvIns         The driver instance.
 * @param   ppTemplate      Where to store the template pointer on success.
 * @param   pfnCompleted    The completion callback routine.
 * @param   pvTemplateUser  Template user argument
 * @param   pszDesc         Description.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEDRV pfnCompleted, void *pvTemplateUser, const char *pszDesc)
{
    LogFlow(("%s: pDrvIns=%p ppTemplate=%p pfnCompleted=%p pszDesc=%s\n",
              __FUNCTION__, pDrvIns, ppTemplate, pfnCompleted, pszDesc));

    /*
     * Validate input.
     */
    VM_ASSERT_EMT(pVM);
    if (!pfnCompleted)
    {
        AssertMsgFailed(("No completion callback!\n"));
        return VERR_INVALID_PARAMETER;
    }

    if (!ppTemplate)
    {
        AssertMsgFailed(("Template pointer is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the template.
     */
    PPDMASYNCCOMPLETIONTEMPLATE pTemplate;
    int rc = pdmR3AsyncCompletionTemplateCreate(pVM, &pTemplate, PDMASYNCCOMPLETIONTEMPLATETYPE_DRV);
    if (RT_SUCCESS(rc))
    {
        pTemplate->u.Drv.pDrvIns        = pDrvIns;
        pTemplate->u.Drv.pfnCompleted   = pfnCompleted;
        pTemplate->u.Drv.pvTemplateUser = pvTemplateUser;

        *ppTemplate = pTemplate;
        Log(("PDM: Created driver template %p: pfnCompleted=%p pDrvIns=%p\n",
             pTemplate, pfnCompleted, pDrvIns));
    }

    return rc;
}

/**
 * Creates a async completion template for a USB device instance.
 *
 * The template is used when creating new completion tasks.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pUsbIns         The USB device instance.
 * @param   ppTemplate      Where to store the template pointer on success.
 * @param   pfnCompleted    The completion callback routine.
 * @param   pszDesc         Description.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateCreateUsb(PVM pVM, PPDMUSBINS pUsbIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEUSB pfnCompleted, const char *pszDesc)
{
    LogFlow(("%s: pUsbIns=%p ppTemplate=%p pfnCompleted=%p pszDesc=%s\n",
              __FUNCTION__, pUsbIns, ppTemplate, pfnCompleted, pszDesc));

    /*
     * Validate input.
     */
    VM_ASSERT_EMT(pVM);
    if (!pfnCompleted)
    {
        AssertMsgFailed(("No completion callback!\n"));
        return VERR_INVALID_PARAMETER;
    }

    if (!ppTemplate)
    {
        AssertMsgFailed(("Template pointer is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the template.
     */
    PPDMASYNCCOMPLETIONTEMPLATE pTemplate;
    int rc = pdmR3AsyncCompletionTemplateCreate(pVM, &pTemplate, PDMASYNCCOMPLETIONTEMPLATETYPE_USB);
    if (RT_SUCCESS(rc))
    {
        pTemplate->u.Usb.pUsbIns = pUsbIns;
        pTemplate->u.Usb.pfnCompleted = pfnCompleted;

        *ppTemplate = pTemplate;
        Log(("PDM: Created usb template %p: pfnCompleted=%p pDevIns=%p\n",
             pTemplate, pfnCompleted, pUsbIns));
    }

    return rc;
}

/**
 * Creates a async completion template for internally by the VMM.
 *
 * The template is used when creating new completion tasks.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   ppTemplate      Where to store the template pointer on success.
 * @param   pfnCompleted    The completion callback routine.
 * @param   pvUser2         The 2nd user argument for the callback.
 * @param   pszDesc         Description.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateCreateInternal(PVM pVM, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEINT pfnCompleted, void *pvUser2, const char *pszDesc)
{
    LogFlow(("%s: ppTemplate=%p pfnCompleted=%p pvUser2=%p pszDesc=%s\n",
              __FUNCTION__, ppTemplate, pfnCompleted, pvUser2, pszDesc));

    /*
     * Validate input.
     */
    VM_ASSERT_EMT(pVM);
    if (!pfnCompleted)
    {
        AssertMsgFailed(("No completion callback!\n"));
        return VERR_INVALID_PARAMETER;
    }

    if (!ppTemplate)
    {
        AssertMsgFailed(("Template pointer is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the template.
     */
    PPDMASYNCCOMPLETIONTEMPLATE pTemplate;
    int rc = pdmR3AsyncCompletionTemplateCreate(pVM, &pTemplate, PDMASYNCCOMPLETIONTEMPLATETYPE_INTERNAL);
    if (RT_SUCCESS(rc))
    {
        pTemplate->u.Int.pvUser = pvUser2;
        pTemplate->u.Int.pfnCompleted = pfnCompleted;

        *ppTemplate = pTemplate;
        Log(("PDM: Created internal template %p: pfnCompleted=%p pvUser2=%p\n",
             pTemplate, pfnCompleted, pvUser2));
    }

    return rc;
}

/**
 * Destroys the specified async completion template.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PDM_ASYNC_TEMPLATE_BUSY if the template is still in use.
 *
 * @param   pTemplate       The template in question.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateDestroy(PPDMASYNCCOMPLETIONTEMPLATE pTemplate)
{
    LogFlow(("%s: pTemplate=%p\n", __FUNCTION__, pTemplate));

    if (!pTemplate)
    {
        AssertMsgFailed(("pTemplate is NULL!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Check if the template is still used.
     */
    if (pTemplate->cUsed > 0)
    {
        AssertMsgFailed(("Template is still in use\n"));
        return VERR_PDM_ASYNC_TEMPLATE_BUSY;
    }

    /*
     * Unlink the template from the list.
     */
    PVM pVM = pTemplate->pVM;
    PPDMASYNCCOMPLETIONTEMPLATE pPrev = pTemplate->pPrev;
    PPDMASYNCCOMPLETIONTEMPLATE pNext = pTemplate->pNext;

    if (pPrev)
        pPrev->pNext = pNext;
    else
        pVM->pdm.s.pAsyncCompletionTemplates = pNext;

    if (pNext)
        pNext->pPrev = pPrev;

    /*
     * Free the template.
     */
    MMR3HeapFree(pTemplate);

    return VINF_SUCCESS;
}

/**
 * Destroys all the specified async completion templates for the given device instance.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PDM_ASYNC_TEMPLATE_BUSY if one or more of the templates are still in use.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDevIns         The device instance.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyDevice(PVM pVM, PPDMDEVINS pDevIns)
{
    LogFlow(("%s: pDevIns=%p\n", __FUNCTION__, pDevIns));

    /*
     * Validate input.
     */
    if (!pDevIns)
        return VERR_INVALID_PARAMETER;
    VM_ASSERT_EMT(pVM);

    /*
     * Unlink it.
     */
    PPDMASYNCCOMPLETIONTEMPLATE pTemplate = pVM->pdm.s.pAsyncCompletionTemplates;
    while (pTemplate)
    {
        if (    pTemplate->enmType == PDMASYNCCOMPLETIONTEMPLATETYPE_DEV
            &&  pTemplate->u.Dev.pDevIns == pDevIns)
        {
            PPDMASYNCCOMPLETIONTEMPLATE pTemplateDestroy = pTemplate;
            pTemplate = pTemplate->pNext;
            int rc = PDMR3AsyncCompletionTemplateDestroy(pTemplateDestroy);
            if (RT_FAILURE(rc))
                return rc;
        }
        else
            pTemplate = pTemplate->pNext;
    }

    return VINF_SUCCESS;
}

/**
 * Destroys all the specified async completion templates for the given driver instance.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PDM_ASYNC_TEMPLATE_BUSY if one or more of the templates are still in use.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pDrvIns         The driver instance.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns)
{
    LogFlow(("%s: pDevIns=%p\n", __FUNCTION__, pDrvIns));

    /*
     * Validate input.
     */
    if (!pDrvIns)
        return VERR_INVALID_PARAMETER;
    VM_ASSERT_EMT(pVM);

    /*
     * Unlink it.
     */
    PPDMASYNCCOMPLETIONTEMPLATE pTemplate = pVM->pdm.s.pAsyncCompletionTemplates;
    while (pTemplate)
    {
        if (    pTemplate->enmType == PDMASYNCCOMPLETIONTEMPLATETYPE_DRV
            &&  pTemplate->u.Drv.pDrvIns == pDrvIns)
        {
            PPDMASYNCCOMPLETIONTEMPLATE pTemplateDestroy = pTemplate;
            pTemplate = pTemplate->pNext;
            int rc = PDMR3AsyncCompletionTemplateDestroy(pTemplateDestroy);
            if (RT_FAILURE(rc))
                return rc;
        }
        else
            pTemplate = pTemplate->pNext;
    }

    return VINF_SUCCESS;
}

/**
 * Destroys all the specified async completion templates for the given USB device instance.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PDM_ASYNC_TEMPLATE_BUSY if one or more of the templates are still in use.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pUsbIns         The USB device instance.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyUsb(PVM pVM, PPDMUSBINS pUsbIns)
{
    LogFlow(("%s: pUsbIns=%p\n", __FUNCTION__, pUsbIns));

    /*
     * Validate input.
     */
    if (!pUsbIns)
        return VERR_INVALID_PARAMETER;
    VM_ASSERT_EMT(pVM);

    /*
     * Unlink it.
     */
    PPDMASYNCCOMPLETIONTEMPLATE pTemplate = pVM->pdm.s.pAsyncCompletionTemplates;
    while (pTemplate)
    {
        if (    pTemplate->enmType == PDMASYNCCOMPLETIONTEMPLATETYPE_USB
            &&  pTemplate->u.Usb.pUsbIns == pUsbIns)
        {
            PPDMASYNCCOMPLETIONTEMPLATE pTemplateDestroy = pTemplate;
            pTemplate = pTemplate->pNext;
            int rc = PDMR3AsyncCompletionTemplateDestroy(pTemplateDestroy);
            if (RT_FAILURE(rc))
                return rc;
        }
        else
            pTemplate = pTemplate->pNext;
    }

    return VINF_SUCCESS;
}

void pdmR3AsyncCompletionCompleteTask(PPDMASYNCCOMPLETIONTASK pTask, bool fCallCompletionHandler)
{
    LogFlow(("%s: pTask=%#p fCallCompletionHandler=%RTbool\n", __FUNCTION__, pTask, fCallCompletionHandler));

    if (fCallCompletionHandler)
    {
        PPDMASYNCCOMPLETIONTEMPLATE pTemplate = pTask->pEndpoint->pTemplate;

        switch (pTemplate->enmType)
        {
            case PDMASYNCCOMPLETIONTEMPLATETYPE_DEV:
            {
                pTemplate->u.Dev.pfnCompleted(pTemplate->u.Dev.pDevIns, pTask->pvUser);
                break;
            }
            case PDMASYNCCOMPLETIONTEMPLATETYPE_DRV:
            {
                pTemplate->u.Drv.pfnCompleted(pTemplate->u.Drv.pDrvIns, pTemplate->u.Drv.pvTemplateUser, pTask->pvUser);
                break;
            }
            case PDMASYNCCOMPLETIONTEMPLATETYPE_USB:
            {
                pTemplate->u.Usb.pfnCompleted(pTemplate->u.Usb.pUsbIns, pTask->pvUser);
                break;
            }
            case PDMASYNCCOMPLETIONTEMPLATETYPE_INTERNAL:
            {
                pTemplate->u.Int.pfnCompleted(pTemplate->pVM, pTask->pvUser, pTemplate->u.Int.pvUser);
                break;
            }
            default:
                AssertMsgFailed(("Unknown template type!\n"));
        }
    }

    pdmR3AsyncCompletionPutTask(pTask->pEndpoint, pTask);
}

/**
 * Worker initializing a endpoint class.
 *
 * @returns VBox statis code.
 * @param   pVM        Pointer to the shared VM instance data.
 * @param   pEpClass   Pointer to the endpoint class structure.
 * @param   pCfgHandle Pointer to the the CFGM tree.
 */
int pdmR3AsyncCompletionEpClassInit(PVM pVM, PCPDMASYNCCOMPLETIONEPCLASSOPS pEpClassOps, PCFGMNODE pCfgHandle)
{
    int rc = VINF_SUCCESS;

    /* Validate input. */
    if (   !pEpClassOps
        || (pEpClassOps->u32Version != PDMAC_EPCLASS_OPS_VERSION)
        || (pEpClassOps->u32VersionEnd != PDMAC_EPCLASS_OPS_VERSION))
        AssertMsgFailedReturn(("Invalid endpoint class data\n"), VERR_VERSION_MISMATCH);

    LogFlowFunc((": pVM=%p pEpClassOps=%p{%s}\n", pVM, pEpClassOps, pEpClassOps->pcszName));

    /* Allocate global class data. */
    PPDMASYNCCOMPLETIONEPCLASS pEndpointClass = NULL;

    rc = MMR3HeapAllocZEx(pVM, MM_TAG_PDM_ASYNC_COMPLETION,
                          pEpClassOps->cbEndpointClassGlobal,
                          (void **)&pEndpointClass);
    if (RT_SUCCESS(rc))
    {
        /* Initialize common data. */
        pEndpointClass->pVM          = pVM;
        pEndpointClass->pEndpointOps = pEpClassOps;

        rc = RTCritSectInit(&pEndpointClass->CritSect);
        if (RT_SUCCESS(rc))
        {
            PCFGMNODE pCfgNodeClass = CFGMR3GetChild(pCfgHandle, pEpClassOps->pcszName);

            /* Create task cache */
            rc = RTMemCacheCreate(&pEndpointClass->hMemCacheTasks, pEpClassOps->cbTask,
                                  0, UINT32_MAX, NULL, NULL, NULL, 0);
            if (RT_SUCCESS(rc))
            {
                /* Call the specific endpoint class initializer. */
                rc = pEpClassOps->pfnInitialize(pEndpointClass, pCfgNodeClass);
                if (RT_SUCCESS(rc))
                {
                    AssertMsg(!pVM->pdm.s.papAsyncCompletionEndpointClass[pEpClassOps->enmClassType],
                                  ("Endpoint class was already initialized\n"));

                    pVM->pdm.s.papAsyncCompletionEndpointClass[pEpClassOps->enmClassType] = pEndpointClass;
                    LogFlowFunc((": Initialized endpoint class \"%s\" rc=%Rrc\n", pEpClassOps->pcszName, rc));
                    return VINF_SUCCESS;
                }
                RTMemCacheDestroy(pEndpointClass->hMemCacheTasks);
            }
            RTCritSectDelete(&pEndpointClass->CritSect);
        }
        MMR3HeapFree(pEndpointClass);
    }

    LogFlowFunc((": Failed to initialize endpoint class rc=%Rrc\n", rc));

    return rc;
}

/**
 * Worker terminating all endpoint classes.
 *
 * @returns nothing
 * @param   pEndpointClass    Pointer to the endpoint class to terminate.
 *
 * @remarks This method ensures that any still open endpoint is closed.
 */
static void pdmR3AsyncCompletionEpClassTerminate(PPDMASYNCCOMPLETIONEPCLASS pEndpointClass)
{
    int rc = VINF_SUCCESS;
    PVM pVM = pEndpointClass->pVM;

    /* Close all still open endpoints. */
    while (pEndpointClass->pEndpointsHead)
        PDMR3AsyncCompletionEpClose(pEndpointClass->pEndpointsHead);

    /* Call the termination callback of the class. */
    pEndpointClass->pEndpointOps->pfnTerminate(pEndpointClass);

    RTMemCacheDestroy(pEndpointClass->hMemCacheTasks);
    RTCritSectDelete(&pEndpointClass->CritSect);

    /* Free the memory of the class finally and clear the entry in the class array. */
    pVM->pdm.s.papAsyncCompletionEndpointClass[pEndpointClass->pEndpointOps->enmClassType] = NULL;
    MMR3HeapFree(pEndpointClass);
}

/**
 * Initialize the async completion manager.
 *
 * @returns VBox status code
 * @param   pVM Pointer to the shared VM structure.
 */
int pdmR3AsyncCompletionInit(PVM pVM)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc((": pVM=%p\n", pVM));

    VM_ASSERT_EMT(pVM);

    do
    {
        /* Allocate array for global class data. */
        rc = MMR3HeapAllocZEx(pVM, MM_TAG_PDM_ASYNC_COMPLETION,
                              sizeof(PPDMASYNCCOMPLETIONEPCLASS) * PDMASYNCCOMPLETIONEPCLASSTYPE_MAX,
                              (void **)&pVM->pdm.s.papAsyncCompletionEndpointClass);
        if (RT_FAILURE(rc))
            break;

        PCFGMNODE pCfgRoot = CFGMR3GetRoot(pVM);
        PCFGMNODE pCfgAsyncCompletion = CFGMR3GetChild(CFGMR3GetChild(pCfgRoot, "PDM"), "AsyncCompletion");

        rc = pdmR3AsyncCompletionEpClassInit(pVM, &g_PDMAsyncCompletionEndpointClassFile, pCfgAsyncCompletion);
        if (RT_FAILURE(rc))
            break;

        /* Put other classes here. */
    } while (0);

    LogFlowFunc((": pVM=%p rc=%Rrc\n", pVM, rc));

    return rc;
}

/**
 * Terminates the async completion manager.
 *
 * @returns VBox status code
 * @param   pVM Pointer to the shared VM structure.
 */
int pdmR3AsyncCompletionTerm(PVM pVM)
{
    LogFlowFunc((": pVM=%p\n", pVM));

    if (pVM->pdm.s.papAsyncCompletionEndpointClass)
    {
        pdmR3AsyncCompletionEpClassTerminate(pVM->pdm.s.papAsyncCompletionEndpointClass[PDMASYNCCOMPLETIONEPCLASSTYPE_FILE]);
        MMR3HeapFree(pVM->pdm.s.papAsyncCompletionEndpointClass);
    }
    return VINF_SUCCESS;
}

/**
 * Tries to get a free task from the endpoint or class cache
 * allocating the task if it fails.
 *
 * @returns Pointer to a new and initialized task or NULL
 * @param   pEndpoint    The endpoint the task is for.
 * @param   pvUser       Opaque user data for the task.
 */
static PPDMASYNCCOMPLETIONTASK pdmR3AsyncCompletionGetTask(PPDMASYNCCOMPLETIONENDPOINT pEndpoint, void *pvUser)
{
    PPDMASYNCCOMPLETIONEPCLASS pEndpointClass = pEndpoint->pEpClass;
    PPDMASYNCCOMPLETIONTASK pTask = NULL;

    pTask = (PPDMASYNCCOMPLETIONTASK)RTMemCacheAlloc(pEndpointClass->hMemCacheTasks);

    if (RT_LIKELY(pTask))
    {
        /* Get ID of the task. */
        pTask->uTaskId   = ASMAtomicIncU32(&pEndpoint->uTaskIdNext);

        /* Initialize common parts. */
        pTask->pvUser    = pvUser;
        pTask->pEndpoint = pEndpoint;
        /* Clear list pointers for safety. */
        pTask->pPrev     = NULL;
        pTask->pNext     = NULL;
#ifdef VBOX_WITH_STATISTICS
        pTask->tsNsStart = RTTimeNanoTS();
        STAM_COUNTER_INC(&pEndpoint->StatIoOpsStarted);
#endif
    }

    return pTask;
}

/**
 * Puts a task in one of the caches.
 *
 * @returns nothing.
 * @param   pEndpoint    The endpoint the task belongs to.
 * @param   pTask        The task to cache.
 */
static void pdmR3AsyncCompletionPutTask(PPDMASYNCCOMPLETIONENDPOINT pEndpoint, PPDMASYNCCOMPLETIONTASK pTask)
{
    PPDMASYNCCOMPLETIONEPCLASS pEndpointClass = pEndpoint->pEpClass;

#ifdef VBOX_WITH_STATISTICS
    uint64_t tsRun  = RTTimeNanoTS() - pTask->tsNsStart;
    uint64_t iStatIdx;

    if (tsRun < 1000)
    {
        /* Update nanoseconds statistics */
        iStatIdx = tsRun / 100;
        STAM_COUNTER_INC(&pEndpoint->StatTaskRunTimesNs[iStatIdx]);
    }
    else
    {
        tsRun /= 1000;

        if (tsRun < 1000)
        {
            /* Update microsecnds statistics */
            iStatIdx = tsRun / 100;
            STAM_COUNTER_INC(&pEndpoint->StatTaskRunTimesMicroSec[iStatIdx]);
        }
        else
        {
            tsRun /= 1000;

            if (tsRun < 1000)
            {
                /* Update milliseconds statistics */
                iStatIdx = tsRun / 100;
                STAM_COUNTER_INC(&pEndpoint->StatTaskRunTimesMs[iStatIdx]);
            }
            else
            {
                tsRun /= 1000;

                if (tsRun < 1000)
                {
                    /* Update seconds statistics */
                    iStatIdx = tsRun / 10;
                    STAM_COUNTER_INC(&pEndpoint->StatTaskRunTimesSec[iStatIdx]);
                }
                else
                    STAM_COUNTER_INC(&pEndpoint->StatTaskRunOver100Sec);
            }
        }
    }

    STAM_COUNTER_INC(&pEndpoint->StatIoOpsCompleted);
    pEndpoint->cIoOpsCompleted++;
    uint64_t tsMsCur = RTTimeMilliTS();
    uint64_t tsInterval = tsMsCur - pEndpoint->tsIntervalStartMs;

    if (tsInterval >= 1000)
    {
        pEndpoint->StatIoOpsPerSec.c = pEndpoint->cIoOpsCompleted / (tsInterval / 1000);
        pEndpoint->tsIntervalStartMs = tsMsCur;
        pEndpoint->cIoOpsCompleted = 0;
    }
#endif

    RTMemCacheFree(pEndpointClass->hMemCacheTasks, pTask);
}

static PPDMASYNCCOMPLETIONENDPOINT pdmR3AsyncCompletionFindEndpointWithUri(PPDMASYNCCOMPLETIONEPCLASS pEndpointClass,
                                                                           const char *pszUri)
{
    PPDMASYNCCOMPLETIONENDPOINT pEndpoint = pEndpointClass->pEndpointsHead;

    while (pEndpoint)
    {
        if (!RTStrCmp(pEndpoint->pszUri, pszUri))
            return pEndpoint;

        pEndpoint = pEndpoint->pNext;
    }

    return NULL;
}

VMMR3DECL(int) PDMR3AsyncCompletionEpCreateForFile(PPPDMASYNCCOMPLETIONENDPOINT ppEndpoint,
                                                   const char *pszFilename, uint32_t fFlags,
                                                   PPDMASYNCCOMPLETIONTEMPLATE pTemplate)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc((": ppEndpoint=%p pszFilename=%p{%s} fFlags=%u pTemplate=%p\n",
                 ppEndpoint, pszFilename, pszFilename, fFlags, pTemplate));

    /* Sanity checks. */
    AssertReturn(VALID_PTR(ppEndpoint),  VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pszFilename), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pTemplate),   VERR_INVALID_POINTER);

    /* Check that the flags are valid. */
    AssertReturn(((~(PDMACEP_FILE_FLAGS_READ_ONLY | PDMACEP_FILE_FLAGS_CACHING) & fFlags) == 0),
                 VERR_INVALID_PARAMETER);

    PVM pVM = pTemplate->pVM;
    PPDMASYNCCOMPLETIONEPCLASS  pEndpointClass = pVM->pdm.s.papAsyncCompletionEndpointClass[PDMASYNCCOMPLETIONEPCLASSTYPE_FILE];
    PPDMASYNCCOMPLETIONENDPOINT pEndpoint = NULL;

    AssertMsg(pEndpointClass, ("File endpoint class was not initialized\n"));

    /* Search for a already opened endpoint for this file. */
    pEndpoint = pdmR3AsyncCompletionFindEndpointWithUri(pEndpointClass, pszFilename);

    if(!pEndpoint)
    {
        rc = MMR3HeapAllocZEx(pVM, MM_TAG_PDM_ASYNC_COMPLETION,
                              pEndpointClass->pEndpointOps->cbEndpoint,
                              (void **)&pEndpoint);
        if (RT_SUCCESS(rc))
        {

            /* Initialize common parts. */
            pEndpoint->pNext             = NULL;
            pEndpoint->pPrev             = NULL;
            pEndpoint->pEpClass          = pEndpointClass;
            pEndpoint->uTaskIdNext       = 0;
            pEndpoint->fTaskIdWraparound = false;
            pEndpoint->pTemplate         = pTemplate;
            pEndpoint->pszUri            = RTStrDup(pszFilename);
            pEndpoint->cUsers            = 1;

#ifdef VBOX_WITH_STATISTICS
            /* Init the statistics part */
            for (unsigned i = 0; i < RT_ELEMENTS(pEndpoint->StatTaskRunTimesNs); i++)
            {
                rc = STAMR3RegisterF(pVM, &pEndpoint->StatTaskRunTimesNs[i], STAMTYPE_COUNTER,
                                     STAMVISIBILITY_USED,
                                     STAMUNIT_OCCURENCES,
                                     "Nanosecond resolution runtime statistics",
                                     "/PDM/AsyncCompletion/File/%s/TaskRun1Ns-%u-%u",
                                     RTPathFilename(pEndpoint->pszUri),
                                     i*100, i*100+100-1);
                if (RT_FAILURE(rc))
                    break;
            }

            if (RT_SUCCESS(rc))
            {
                for (unsigned i = 0; i < RT_ELEMENTS(pEndpoint->StatTaskRunTimesMicroSec); i++)
                {
                    rc = STAMR3RegisterF(pVM, &pEndpoint->StatTaskRunTimesMicroSec[i], STAMTYPE_COUNTER,
                                         STAMVISIBILITY_USED,
                                         STAMUNIT_OCCURENCES,
                                         "Microsecond resolution runtime statistics",
                                         "/PDM/AsyncCompletion/File/%s/TaskRun2MicroSec-%u-%u",
                                         RTPathFilename(pEndpoint->pszUri),
                                        i*100, i*100+100-1);
                    if (RT_FAILURE(rc))
                        break;
                }
            }

            if (RT_SUCCESS(rc))
            {
                for (unsigned i = 0; i < RT_ELEMENTS(pEndpoint->StatTaskRunTimesMs); i++)
                {
                    rc = STAMR3RegisterF(pVM, &pEndpoint->StatTaskRunTimesMs[i], STAMTYPE_COUNTER,
                                         STAMVISIBILITY_USED,
                                         STAMUNIT_OCCURENCES,
                                         "Milliseconds resolution runtime statistics",
                                         "/PDM/AsyncCompletion/File/%s/TaskRun3Ms-%u-%u",
                                         RTPathFilename(pEndpoint->pszUri),
                                        i*100, i*100+100-1);
                    if (RT_FAILURE(rc))
                        break;
                }
            }

            if (RT_SUCCESS(rc))
            {
                for (unsigned i = 0; i < RT_ELEMENTS(pEndpoint->StatTaskRunTimesMs); i++)
                {
                    rc = STAMR3RegisterF(pVM, &pEndpoint->StatTaskRunTimesSec[i], STAMTYPE_COUNTER,
                                         STAMVISIBILITY_USED,
                                         STAMUNIT_OCCURENCES,
                                         "Second resolution runtime statistics",
                                         "/PDM/AsyncCompletion/File/%s/TaskRun4Sec-%u-%u",
                                         RTPathFilename(pEndpoint->pszUri),
                                        i*10, i*10+10-1);
                    if (RT_FAILURE(rc))
                        break;
                }
            }

            if (RT_SUCCESS(rc))
            {
                rc = STAMR3RegisterF(pVM, &pEndpoint->StatTaskRunOver100Sec, STAMTYPE_COUNTER,
                                     STAMVISIBILITY_USED,
                                     STAMUNIT_OCCURENCES,
                                     "Tasks which ran more than 100sec",
                                     "/PDM/AsyncCompletion/File/%s/TaskRunSecGreater100Sec",
                                     RTPathFilename(pEndpoint->pszUri));
            }

            if (RT_SUCCESS(rc))
            {
                rc = STAMR3RegisterF(pVM, &pEndpoint->StatIoOpsPerSec, STAMTYPE_COUNTER,
                                     STAMVISIBILITY_ALWAYS,
                                     STAMUNIT_OCCURENCES,
                                     "Processed I/O operations per second",
                                     "/PDM/AsyncCompletion/File/%s/IoOpsPerSec",
                                     RTPathFilename(pEndpoint->pszUri));
            }

            if (RT_SUCCESS(rc))
            {
                rc = STAMR3RegisterF(pVM, &pEndpoint->StatIoOpsStarted, STAMTYPE_COUNTER,
                                     STAMVISIBILITY_ALWAYS,
                                     STAMUNIT_OCCURENCES,
                                     "Started I/O operations for this endpoint",
                                     "/PDM/AsyncCompletion/File/%s/IoOpsStarted",
                                     RTPathFilename(pEndpoint->pszUri));
            }

            if (RT_SUCCESS(rc))
            {
                rc = STAMR3RegisterF(pVM, &pEndpoint->StatIoOpsCompleted, STAMTYPE_COUNTER,
                                     STAMVISIBILITY_ALWAYS,
                                     STAMUNIT_OCCURENCES,
                                     "Completed I/O operations for this endpoint",
                                     "/PDM/AsyncCompletion/File/%s/IoOpsCompleted",
                                     RTPathFilename(pEndpoint->pszUri));
            }

            pEndpoint->tsIntervalStartMs = RTTimeMilliTS();
#endif

            if (   pEndpoint->pszUri
                && RT_SUCCESS(rc))
            {
                /* Call the initializer for the endpoint. */
                rc = pEndpointClass->pEndpointOps->pfnEpInitialize(pEndpoint, pszFilename, fFlags);
                if (RT_SUCCESS(rc))
                {
                    /* Link it into the list of endpoints. */
                    rc = RTCritSectEnter(&pEndpointClass->CritSect);
                    AssertMsg(RT_SUCCESS(rc), ("Failed to enter critical section rc=%Rrc\n", rc));

                    pEndpoint->pNext = pEndpointClass->pEndpointsHead;
                    if (pEndpointClass->pEndpointsHead)
                        pEndpointClass->pEndpointsHead->pPrev = pEndpoint;

                    pEndpointClass->pEndpointsHead = pEndpoint;
                    pEndpointClass->cEndpoints++;

                    rc = RTCritSectLeave(&pEndpointClass->CritSect);
                    AssertMsg(RT_SUCCESS(rc), ("Failed to enter critical section rc=%Rrc\n", rc));

                    /* Reference the template. */
                    ASMAtomicIncU32(&pTemplate->cUsed);

                    *ppEndpoint = pEndpoint;

                    LogFlowFunc((": Created endpoint for %s: rc=%Rrc\n", pszFilename, rc));
                    return VINF_SUCCESS;
                }
                RTStrFree(pEndpoint->pszUri);
            }
            MMR3HeapFree(pEndpoint);
        }
    }
    else
    {
        /* Endpoint found. */
        pEndpoint->cUsers++;

        *ppEndpoint = pEndpoint;
        return VINF_SUCCESS;
    }

    LogFlowFunc((": Creation of endpoint for %s failed: rc=%Rrc\n", pszFilename, rc));

    return rc;
}

VMMR3DECL(void) PDMR3AsyncCompletionEpClose(PPDMASYNCCOMPLETIONENDPOINT pEndpoint)
{
    LogFlowFunc((": pEndpoint=%p\n", pEndpoint));

    /* Sanity checks. */
    AssertReturnVoid(VALID_PTR(pEndpoint));

    pEndpoint->cUsers--;

    /* If the last user closed the endpoint we will free it. */
    if (!pEndpoint->cUsers)
    {
        PPDMASYNCCOMPLETIONEPCLASS pEndpointClass = pEndpoint->pEpClass;
        pEndpointClass->pEndpointOps->pfnEpClose(pEndpoint);

        /* Drop reference from the template. */
        ASMAtomicDecU32(&pEndpoint->pTemplate->cUsed);

        /* Unlink the endpoint from the list. */
        int rc = RTCritSectEnter(&pEndpointClass->CritSect);
        AssertMsg(RT_SUCCESS(rc), ("Failed to enter critical section rc=%Rrc\n", rc));

        PPDMASYNCCOMPLETIONENDPOINT pEndpointNext = pEndpoint->pNext;
        PPDMASYNCCOMPLETIONENDPOINT pEndpointPrev = pEndpoint->pPrev;

        if (pEndpointPrev)
            pEndpointPrev->pNext = pEndpointNext;
        else
            pEndpointClass->pEndpointsHead = pEndpointNext;
        if (pEndpointNext)
            pEndpointNext->pPrev = pEndpointPrev;

        pEndpointClass->cEndpoints--;

        rc = RTCritSectLeave(&pEndpointClass->CritSect);
        AssertMsg(RT_SUCCESS(rc), ("Failed to enter critical section rc=%Rrc\n", rc));

#ifdef VBOX_WITH_STATISTICS
        /* Deregister the statistics part */
        PVM pVM = pEndpointClass->pVM;

        for (unsigned i = 0; i < RT_ELEMENTS(pEndpoint->StatTaskRunTimesNs); i++)
            STAMR3Deregister(pVM, &pEndpoint->StatTaskRunTimesNs[i]);
        for (unsigned i = 0; i < RT_ELEMENTS(pEndpoint->StatTaskRunTimesMicroSec); i++)
            STAMR3Deregister(pVM, &pEndpoint->StatTaskRunTimesMicroSec[i]);
        for (unsigned i = 0; i < RT_ELEMENTS(pEndpoint->StatTaskRunTimesMs); i++)
            STAMR3Deregister(pVM, &pEndpoint->StatTaskRunTimesMs[i]);
        for (unsigned i = 0; i < RT_ELEMENTS(pEndpoint->StatTaskRunTimesMs); i++)
            STAMR3Deregister(pVM, &pEndpoint->StatTaskRunTimesSec[i]);

        STAMR3Deregister(pVM, &pEndpoint->StatTaskRunOver100Sec);
        STAMR3Deregister(pVM, &pEndpoint->StatIoOpsPerSec);
        STAMR3Deregister(pVM, &pEndpoint->StatIoOpsStarted);
        STAMR3Deregister(pVM, &pEndpoint->StatIoOpsCompleted);
#endif

        RTStrFree(pEndpoint->pszUri);
        MMR3HeapFree(pEndpoint);
    }
}

VMMR3DECL(int) PDMR3AsyncCompletionEpRead(PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                                          PCPDMDATASEG paSegments, size_t cSegments,
                                          size_t cbRead, void *pvUser,
                                          PPPDMASYNCCOMPLETIONTASK ppTask)
{
    int rc = VINF_SUCCESS;

    AssertReturn(VALID_PTR(pEndpoint), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(paSegments), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(ppTask), VERR_INVALID_POINTER);
    AssertReturn(cSegments > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cbRead > 0, VERR_INVALID_PARAMETER);
    AssertReturn(off >= 0, VERR_INVALID_PARAMETER);

    PPDMASYNCCOMPLETIONTASK pTask;

    pTask = pdmR3AsyncCompletionGetTask(pEndpoint, pvUser);
    if (!pTask)
        return VERR_NO_MEMORY;

    rc = pEndpoint->pEpClass->pEndpointOps->pfnEpRead(pTask, pEndpoint, off,
                                                      paSegments, cSegments, cbRead);
    if (RT_SUCCESS(rc))
    {
        *ppTask = pTask;
    }
    else
        pdmR3AsyncCompletionPutTask(pEndpoint, pTask);

    return rc;
}

VMMR3DECL(int) PDMR3AsyncCompletionEpWrite(PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                                           PCPDMDATASEG paSegments, size_t cSegments,
                                           size_t cbWrite, void *pvUser,
                                           PPPDMASYNCCOMPLETIONTASK ppTask)
{
    int rc = VINF_SUCCESS;

    AssertReturn(VALID_PTR(pEndpoint), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(paSegments), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(ppTask), VERR_INVALID_POINTER);
    AssertReturn(cSegments > 0, VERR_INVALID_PARAMETER);
    AssertReturn(cbWrite > 0, VERR_INVALID_PARAMETER);
    AssertReturn(off >= 0, VERR_INVALID_PARAMETER);

    PPDMASYNCCOMPLETIONTASK pTask;

    pTask = pdmR3AsyncCompletionGetTask(pEndpoint, pvUser);
    if (!pTask)
        return VERR_NO_MEMORY;

    rc = pEndpoint->pEpClass->pEndpointOps->pfnEpWrite(pTask, pEndpoint, off,
                                                       paSegments, cSegments, cbWrite);
    if (RT_SUCCESS(rc))
    {
        *ppTask = pTask;
    }
    else
        pdmR3AsyncCompletionPutTask(pEndpoint, pTask);

    return rc;
}

VMMR3DECL(int) PDMR3AsyncCompletionEpFlush(PPDMASYNCCOMPLETIONENDPOINT pEndpoint,
                                           void *pvUser,
                                           PPPDMASYNCCOMPLETIONTASK ppTask)
{
    int rc = VINF_SUCCESS;

    AssertReturn(VALID_PTR(pEndpoint), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(ppTask), VERR_INVALID_POINTER);

    PPDMASYNCCOMPLETIONTASK pTask;

    pTask = pdmR3AsyncCompletionGetTask(pEndpoint, pvUser);
    if (!pTask)
        return VERR_NO_MEMORY;

    rc = pEndpoint->pEpClass->pEndpointOps->pfnEpFlush(pTask, pEndpoint);
    if (RT_SUCCESS(rc))
    {
        *ppTask = pTask;
    }
    else
        pdmR3AsyncCompletionPutTask(pEndpoint, pTask);

    return rc;
}

VMMR3DECL(int) PDMR3AsyncCompletionEpGetSize(PPDMASYNCCOMPLETIONENDPOINT pEndpoint,
                                             uint64_t *pcbSize)
{
    AssertReturn(VALID_PTR(pEndpoint), VERR_INVALID_POINTER);
    AssertReturn(VALID_PTR(pcbSize), VERR_INVALID_POINTER);

    if (pEndpoint->pEpClass->pEndpointOps->pfnEpGetSize)
        return pEndpoint->pEpClass->pEndpointOps->pfnEpGetSize(pEndpoint, pcbSize);
    else
        return VERR_NOT_SUPPORTED;
}

VMMR3DECL(int) PDMR3AsyncCompletionEpSetSize(PPDMASYNCCOMPLETIONENDPOINT pEndpoint,
                                             uint64_t cbSize)
{
    AssertReturn(VALID_PTR(pEndpoint), VERR_INVALID_POINTER);

    if (pEndpoint->pEpClass->pEndpointOps->pfnEpSetSize)
        return pEndpoint->pEpClass->pEndpointOps->pfnEpSetSize(pEndpoint, cbSize);
    else
        return VERR_NOT_SUPPORTED;
}

VMMR3DECL(int) PDMR3AsyncCompletionTaskCancel(PPDMASYNCCOMPLETIONTASK pTask)
{
    return VERR_NOT_IMPLEMENTED;
}

