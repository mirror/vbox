/** @file
 * PDM - Pluggable Device Manager, Async I/O Completion.
 */

/*
 * Copyright (C) 2007-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vmm_pdmasynccompletion_h
#define ___VBox_vmm_pdmasynccompletion_h

#include <VBox/types.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/sg.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_pdm_async_completion  The PDM Async I/O Completion API
 * @ingroup grp_pdm
 * @{
 */

/** Pointer to a PDM async completion template handle. */
typedef struct PDMASYNCCOMPLETIONTEMPLATE *PPDMASYNCCOMPLETIONTEMPLATE;
/** Pointer to a PDM async completion template handle pointer. */
typedef PPDMASYNCCOMPLETIONTEMPLATE *PPPDMASYNCCOMPLETIONTEMPLATE;

/** Pointer to a PDM async completion task handle. */
typedef struct PDMASYNCCOMPLETIONTASK *PPDMASYNCCOMPLETIONTASK;
/** Pointer to a PDM async completion task handle pointer. */
typedef PPDMASYNCCOMPLETIONTASK *PPPDMASYNCCOMPLETIONTASK;

/** Pointer to a PDM async completion endpoint handle. */
typedef struct PDMASYNCCOMPLETIONENDPOINT *PPDMASYNCCOMPLETIONENDPOINT;
/** Pointer to a PDM async completion endpoint handle pointer. */
typedef PPDMASYNCCOMPLETIONENDPOINT *PPPDMASYNCCOMPLETIONENDPOINT;


/**
 * Completion callback for devices.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   rc          The status code of the completed request.
 */
typedef DECLCALLBACK(void) FNPDMASYNCCOMPLETEDEV(PPDMDEVINS pDevIns, void *pvUser, int rc);
/** Pointer to a FNPDMASYNCCOMPLETEDEV(). */
typedef FNPDMASYNCCOMPLETEDEV *PFNPDMASYNCCOMPLETEDEV;


/**
 * Completion callback for drivers.
 *
 * @param   pDrvIns        The driver instance.
 * @param   pvTemplateUser User argument given when creating the template.
 * @param   pvUser         User argument given during request initiation.
 * @param   rc          The status code of the completed request.
 */
typedef DECLCALLBACK(void) FNPDMASYNCCOMPLETEDRV(PPDMDRVINS pDrvIns, void *pvTemplateUser, void *pvUser, int rc);
/** Pointer to a FNPDMASYNCCOMPLETEDRV(). */
typedef FNPDMASYNCCOMPLETEDRV *PFNPDMASYNCCOMPLETEDRV;


/**
 * Completion callback for USB devices.
 *
 * @param   pUsbIns     The USB device instance.
 * @param   pvUser      User argument.
 * @param   rc          The status code of the completed request.
 */
typedef DECLCALLBACK(void) FNPDMASYNCCOMPLETEUSB(PPDMUSBINS pUsbIns, void *pvUser, int rc);
/** Pointer to a FNPDMASYNCCOMPLETEUSB(). */
typedef FNPDMASYNCCOMPLETEUSB *PFNPDMASYNCCOMPLETEUSB;


/**
 * Completion callback for internal.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pvUser      User argument for the task.
 * @param   pvUser2     User argument for the template.
 * @param   rc          The status code of the completed request.
 */
typedef DECLCALLBACK(void) FNPDMASYNCCOMPLETEINT(PVM pVM, void *pvUser, void *pvUser2, int rc);
/** Pointer to a FNPDMASYNCCOMPLETEINT(). */
typedef FNPDMASYNCCOMPLETEINT *PFNPDMASYNCCOMPLETEINT;


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
VMMR3DECL(int) PDMR3AsyncCompletionTemplateCreateDevice(PVM pVM, PPDMDEVINS pDevIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEDEV pfnCompleted, const char *pszDesc);

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
 * @param   pvTemplateUser  Template user argument.
 * @param   pszDesc         Description.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEDRV pfnCompleted, void *pvTemplateUser, const char *pszDesc);

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
VMMR3DECL(int) PDMR3AsyncCompletionTemplateCreateUsb(PVM pVM, PPDMUSBINS pUsbIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEUSB pfnCompleted, const char *pszDesc);

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
VMMR3DECL(int) PDMR3AsyncCompletionTemplateCreateInternal(PVM pVM, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEINT pfnCompleted, void *pvUser2, const char *pszDesc);

/**
 * Destroys the specified async completion template.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PDM_ASYNC_TEMPLATE_BUSY if the template is still in use.
 *
 * @param   pTemplate       The template in question.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTemplateDestroy(PPDMASYNCCOMPLETIONTEMPLATE pTemplate);

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
VMMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyDevice(PVM pVM, PPDMDEVINS pDevIns);

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
VMMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns);

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
VMMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyUsb(PVM pVM, PPDMUSBINS pUsbIns);


/**
 * Opens a file as a async completion endpoint.
 *
 * @returns VBox status code.
 * @param   ppEndpoint      Where to store the opaque endpoint handle on success.
 * @param   pszFilename     Path to the file which is to be opened. (UTF-8)
 * @param   fFlags          Open flags, see grp_pdmacep_file_flags.
 * @param   pTemplate       Handle to the completion callback template to use
 *                          for this end point.
 */
VMMR3DECL(int) PDMR3AsyncCompletionEpCreateForFile(PPPDMASYNCCOMPLETIONENDPOINT ppEndpoint,
                                                   const char *pszFilename, uint32_t fFlags,
                                                   PPDMASYNCCOMPLETIONTEMPLATE pTemplate);

/** @defgroup grp_pdmacep_file_flags Flags for PDMR3AsyncCompletionEpCreateForFile
 * @{ */
/** Open the file in read-only mode. */
#define PDMACEP_FILE_FLAGS_READ_ONLY             RT_BIT_32(0)
/** Whether the file should not be write protected.
 * The default is to protect the file against writes by other processes
 * when opened in read/write mode to prevent data corruption by
 * concurrent access which can occur if the local writeback cache is enabled.
 */
#define PDMACEP_FILE_FLAGS_DONT_LOCK             RT_BIT_32(2)
/** Open the endpoint with the host cache enabled. */
#define PDMACEP_FILE_FLAGS_HOST_CACHE_ENABLED    RT_BIT_32(3)
/** @} */

/**
 * Closes a endpoint waiting for any pending tasks to finish.
 *
 * @returns nothing.
 * @param   pEndpoint       Handle of the endpoint.
 */
VMMR3DECL(void) PDMR3AsyncCompletionEpClose(PPDMASYNCCOMPLETIONENDPOINT pEndpoint);

/**
 * Creates a read task on the given endpoint.
 *
 * @returns VBox status code.
 * @param   pEndpoint       The file endpoint to read from.
 * @param   off             Where to start reading from.
 * @param   paSegments      Scatter gather list to store the data in.
 * @param   cSegments       Number of segments in the list.
 * @param   cbRead          The overall number of bytes to read.
 * @param   pvUser          Opaque user data returned in the completion callback
 *                          upon completion of the task.
 * @param   ppTask          Where to store the task handle on success.
 */
VMMR3DECL(int) PDMR3AsyncCompletionEpRead(PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                                          PCRTSGSEG paSegments, unsigned cSegments,
                                          size_t cbRead, void *pvUser,
                                          PPPDMASYNCCOMPLETIONTASK ppTask);

/**
 * Creates a write task on the given endpoint.
 *
 * @returns VBox status code.
 * @param   pEndpoint       The file endpoint to write to.
 * @param   off             Where to start writing at.
 * @param   paSegments      Scatter gather list of the data to write.
 * @param   cSegments       Number of segments in the list.
 * @param   cbWrite         The overall number of bytes to write.
 * @param   pvUser          Opaque user data returned in the completion callback
 *                          upon completion of the task.
 * @param   ppTask          Where to store the task handle on success.
 */
VMMR3DECL(int) PDMR3AsyncCompletionEpWrite(PPDMASYNCCOMPLETIONENDPOINT pEndpoint, RTFOFF off,
                                           PCRTSGSEG paSegments, unsigned cSegments,
                                           size_t cbWrite, void *pvUser,
                                           PPPDMASYNCCOMPLETIONTASK ppTask);

/**
 * Creates a flush task on the given endpoint.
 *
 * Every read and write task initiated before the flush task is
 * finished upon completion of this task.
 *
 * @returns VBox status code.
 * @param   pEndpoint       The file endpoint to flush.
 * @param   pvUser          Opaque user data returned in the completion callback
 *                          upon completion of the task.
 * @param   ppTask          Where to store the task handle on success.
 */
VMMR3DECL(int) PDMR3AsyncCompletionEpFlush(PPDMASYNCCOMPLETIONENDPOINT pEndpoint,
                                           void *pvUser,
                                           PPPDMASYNCCOMPLETIONTASK ppTask);

/**
 * Queries the size of an endpoint.
 * Not that some endpoints may not support this and will return an error
 * (sockets for example).
 *
 * @returns VBox status code.
 * @retval  VERR_NOT_SUPPORTED if the endpoint does not support this operation.
 * @param   pEndpoint       The file endpoint.
 * @param   pcbSize         Where to store the size of the endpoint.
 */
VMMR3DECL(int) PDMR3AsyncCompletionEpGetSize(PPDMASYNCCOMPLETIONENDPOINT pEndpoint,
                                             uint64_t *pcbSize);

/**
 * Sets the size of an endpoint.
 * Not that some endpoints may not support this and will return an error
 * (sockets for example).
 *
 * @returns VBox status code.
 * @retval  VERR_NOT_SUPPORTED if the endpoint does not support this operation.
 * @param   pEndpoint       The file endpoint.
 * @param   cbSize          The size to set.
 *
 * @note PDMR3AsyncCompletionEpFlush should be called before this operation is executed.
 */
VMMR3DECL(int) PDMR3AsyncCompletionEpSetSize(PPDMASYNCCOMPLETIONENDPOINT pEndpoint,
                                             uint64_t cbSize);

/**
 * Assigns or removes a bandwidth control manager to/from the endpoint.
 *
 * @returns VBox status code.
 * @param   pEndpoint       The endpoint.
 * @param   pcszBwMgr       The identifer of the new bandwidth manager to assign
 *                          or NULL to remove the current one.
 */
VMMR3DECL(int) PDMR3AsyncCompletionEpSetBwMgr(PPDMASYNCCOMPLETIONENDPOINT pEndpoint,
                                              const char *pcszBwMgr);

/**
 * Cancels a async completion task.
 *
 * If you want to use this method, you have to take great create to make sure
 * you will never attempt cancel a task which has been completed. Since there is
 * no reference counting or anything on the task it self, you have to serialize
 * the cancelation and completion paths such that the aren't racing one another.
 *
 * @returns VBox status code
 * @param   pTask           The Task to cancel.
 */
VMMR3DECL(int) PDMR3AsyncCompletionTaskCancel(PPDMASYNCCOMPLETIONTASK pTask);

/**
 * Changes the limit of a bandwidth manager for file endpoints to the given value.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the shared VM structure.
 * @param   pcszBwMgr       The identifer of the bandwidth manager to change.
 * @param   cbMaxNew        The new maximum for the bandwidth manager in bytes/sec.
 */
VMMR3DECL(int) PDMR3AsyncCompletionBwMgrSetMaxForFile(PVM pVM, const char *pcszBwMgr, uint32_t cbMaxNew);

/** @} */

RT_C_DECLS_END

#endif

