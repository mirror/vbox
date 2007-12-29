/** @file
 * PDM - Pluggable Device Manager, Async I/O Completion.
 */

/*
 * Copyright (C) 2007 innotek GmbH
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

#ifndef ___VBox_pdmasynccompletion_h
#define ___VBox_pdmasynccompletion_h

#include <VBox/types.h>

__BEGIN_DECLS

/** @defgroup grp_pdm_async_completion  Async I/O Completion
 * @ingroup grp_pdm
 * @{
 */

/** Pointer to a PDM async completion template handle. */
typedef struct PDMASYNCCOMPLETIONTEMPLATE *PPDMASYNCCOMPLETIONTEMPLATE;
/** Pointer to a PDM async completion template handle pointer. */
typedef PPDMASYNCCOMPLETIONTEMPLATE *PPPDMASYNCCOMPLETIONTEMPLATE;

/** Pointer to a PDM async completion task handle. */
typedef struct PDMASYNCCOMPLETION *PPDMASYNCCOMPLETION;
/** Pointer to a PDM async completion task handle pointer. */
typedef PPDMASYNCCOMPLETION *PPPDMASYNCCOMPLETION;


/**
 * Completion callback for devices.
 *
 * @param   pDevIns     The device instance.
 * @param   pTask       Pointer to the completion task.
 *                      The task is at the time of the call setup to be resumed. So, the callback must
 *                      call PDMR3AsyncCompletionSuspend or PDMR3AsyncCompletionDestroy if any other
 *                      action is wanted upon return.
 * @param   pvCtx       Pointer to any additional, OS specific, completion context. TBD.
 * @param   pvUser      User argument.
 */
typedef DECLCALLBACK(void) FNPDMASYNCCOMPLETEDEV(PPDMDEVINS pDevIns, PPDMASYNCCOMPLETION pTask, void *pvCtx, void *pvUser);
/** Pointer to a FNPDMASYNCCOMPLETEDEV(). */
typedef FNPDMASYNCCOMPLETEDEV *PFNPDMASYNCCOMPLETEDEV;


/**
 * Completion callback for drivers.
 *
 * @param   pDrvIns     The driver instance.
 * @param   pTask       Pointer to the completion task.
 *                      The task is at the time of the call setup to be resumed. So, the callback must
 *                      call PDMR3AsyncCompletionSuspend or PDMR3AsyncCompletionDestroy if any other
 *                      action is wanted upon return.
 * @param   pvCtx       Pointer to any additional, OS specific, completion context. TBD.
 * @param   pvUser      User argument.
 */
typedef DECLCALLBACK(void) FNPDMASYNCCOMPLETEDRV(PPDMDRVINS pDrvIns, PPDMASYNCCOMPLETION pTask, void *pvCtx, void *pvUser);
/** Pointer to a FNPDMASYNCCOMPLETEDRV(). */
typedef FNPDMASYNCCOMPLETEDRV *PFNPDMASYNCCOMPLETEDRV;


/**
 * Completion callback for USB devices.
 *
 * @param   pUsbIns     The USB device instance.
 * @param   pTask       Pointer to the completion task.
 *                      The task is at the time of the call setup to be resumed. So, the callback must
 *                      call PDMR3AsyncCompletionSuspend or PDMR3AsyncCompletionDestroy if any other
 *                      action is wanted upon return.
 * @param   pvCtx       Pointer to any additional, OS specific, completion context. TBD.
 * @param   pvUser      User argument.
 */
typedef DECLCALLBACK(void) FNPDMASYNCCOMPLETEUSB(PPDMUSBINS pUsbIns, PPDMASYNCCOMPLETION pTask, void *pvCtx, void *pvUser);
/** Pointer to a FNPDMASYNCCOMPLETEUSB(). */
typedef FNPDMASYNCCOMPLETEUSB *PFNPDMASYNCCOMPLETEUSB;


/**
 * Completion callback for internal.
 *
 * @param   pVM         Pointer to the shared VM structure.
 * @param   pTask       Pointer to the completion task.
 *                      The task is at the time of the call setup to be resumed. So, the callback must
 *                      call PDMR3AsyncCompletionSuspend or PDMR3AsyncCompletionDestroy if any other
 *                      action is wanted upon return.
 * @param   pvCtx       Pointer to any additional, OS specific, completion context. TBD.
 * @param   pvUser      User argument for the task.
 * @param   pvUser2     User argument for the template.
 */
typedef DECLCALLBACK(void) FNPDMASYNCCOMPLETEINT(PVM pVM, PPDMASYNCCOMPLETION pTask, void *pvCtx, void *pvUser, void *pvUser2);
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
PDMR3DECL(int) PDMR3AsyncCompletionTemplateCreateDevice(PVM pVM, PPDMDEVINS pDevIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEDEV pfnCompleted, const char *pszDesc);

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
 * @param   pszDesc         Description.
 */
PDMR3DECL(int) PDMR3AsyncCompletionTemplateCreateDriver(PVM pVM, PPDMDRVINS pDrvIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEDRV pfnCompleted, const char *pszDesc);

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
PDMR3DECL(int) PDMR3AsyncCompletionTemplateCreateUsb(PVM pVM, PPDMUSBINS pUsbIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEUSB pfnCompleted, const char *pszDesc);

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
PDMR3DECL(int) PDMR3AsyncCompletionTemplateCreateInternal(PVM pVM, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate, PFNPDMASYNCCOMPLETEINT pfnCompleted, void *pvUser2, const char *pszDesc);

/**
 * Destroys the specified async completion template.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PDM_ASYNC_TEMPLATE_BUSY if the template is still in use.
 *
 * @param   pTemplate       The template in question.
 */
PDMR3DECL(int) PDMR3AsyncCompletionTemplateDestroy(PPDMASYNCCOMPLETIONTEMPLATE pTemplate);

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
PDMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyDevice(PVM pVM, PPDMDEVINS pDevIns);

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
PDMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyDriver(PVM pVM, PPDMDRVINS pDrvIns);

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
PDMR3DECL(int) PDMR3AsyncCompletionTemplateDestroyUsb(PVM pVM, PPDMUSBINS pUsbIns);


/**
 * Socket completion context (pvCtx).
 */
typedef struct PDMASYNCCOMPLETIONSOCKET
{
    /** The socket. */
    RTSOCKET        Socket;
    /** Readable. */
    bool            fReadable;
    /** Writable. */
    bool            fWriteable;
    /** Exceptions. */
    bool            fXcpt;
} PDMASYNCCOMPLETIONSOCKET;
/** Pointer to a socket completion context. */
typedef PDMASYNCCOMPLETIONSOCKET *PPDMASYNCCOMPLETIONSOCKET;


/**
 * Creates a completion task for a socket.
 *
 * The pvCtx callback argument will be pointing to a PDMASYNCCOMPLETIONSOCKET structure.
 *
 * @returns VBox status code.
 * @param   ppTask          Where to store the task handle on success.
 * @param   pTemplate       The async completion template.
 * @param   Socket          The socket.
 * @param   fReadable       Whether to callback when the socket becomes readable.
 * @param   fWriteable      Whether to callback when the socket becomes writable.
 * @param   fXcpt           Whether to callback on exception.
 * @param   pvUser          The user argument for the callback.
 */
PDMR3DECL(int) PDMR3AsyncCompletionCreateSocket(PPPDMASYNCCOMPLETION ppTask, PPDMASYNCCOMPLETIONTEMPLATE pTemplate, RTSOCKET Socket, bool fReadable, bool fWriteable, bool fXcpt, void *pvUser);

/**
 * Modifies a socket completion task.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_SUPPORTED if the task isn't a socket task.
 * @param   pTemplate       The async completion template.
 * @param   fReadable       Whether to callback when the socket becomes readable.
 * @param   fWriteable      Whether to callback when the socket becomes writable.
 * @param   fXcpt           Whether to callback on exception.
 */
PDMR3DECL(int) PDMR3AsyncCompletionModifySocket(PPDMASYNCCOMPLETION pTask, bool fReadable, bool fWriteable, bool fXcpt);


#if defined(RT_OS_LINUX) /*&& defined(_AIO_H)*/

struct aiocb;

/**
 * Creates a completion task for an AIO operation on Linux.
 *
 * The pvCtx callback argument will be pAioCB.
 *
 * @returns VBox status code.
 * @param   ppTask          Where to store the task handle on success.
 * @param   pTemplate       The async completion template.
 * @param   pAioCB          The asynchronous I/O control block to wait for.
 * @param   pvUser          The user argument for the callback.
 */
PDMR3DECL(int) PDMR3AsyncCompletionCreateLnxAIO(PPPDMASYNCCOMPLETION ppTask, PPDMASYNCCOMPLETIONTEMPLATE pTemplate, const struct aiocb *pAioCB, void *pvUser);
#endif /* RT_OS_LINUX */

#ifdef RT_OS_OS2
/**
 * Creates a completion task for an event semaphore on OS/2.
 *
 * The pvCtx callback argument will be hev.
 *
 * @returns VBox status code.
 * @param   ppTask          Where to store the task handle on success.
 * @param   pTemplate       The async completion template.
 * @param   hev             The handle of the event semaphore to wait on.
 * @param   pvUser          The user argument for the callback.
 */
PDMR3DECL(int) PDMR3AsyncCompletionCreateOs2Event(PPPDMASYNCCOMPLETION ppTask, PPDMASYNCCOMPLETIONTEMPLATE pTemplate, unsigned long hev, void *pvUser);
#endif /* RT_OS_OS2 */

#ifdef RT_OS_WINDOWS
/**
 * Creates a completion task for an object on Windows.
 *
 * The pvCtx callback argument will be hObject.
 *
 * @returns VBox status code.
 * @param   ppTask          Where to store the task handle on success.
 * @param   pTemplate       The async completion template.
 * @param   hObject         The object to wait for.
 * @param   pvUser          The user argument for the callback.
 */
PDMR3DECL(int) PDMR3AsyncCompletionCreateWinObject(PPPDMASYNCCOMPLETION ppTask, PPDMASYNCCOMPLETIONTEMPLATE pTemplate, void *hObject, void *pvUser);
#endif /* RT_OS_WINDOWS */



/**
 * Sets the user argument of a completion task.
 *
 * @returns VBox status code.
 * @param   pTask           The async completion task.
 * @param   pvUser          The user argument for the callback.
 */
PDMR3DECL(int) PDMR3AsyncCompletionSetUserArg(PPDMASYNCCOMPLETION pTask, void *pvUser);

/**
 * Suspends a async completion task.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PDM_ASYNC_COMPLETION_ALREADY_SUSPENDED if already suspended.
 * @retval  VERR_INVALID_HANDLE if pTask is invalid (asserts).
 * @param   pTask           The async completion task.
 */
PDMR3DECL(int) PDMR3AsyncCompletionSuspend(PPDMASYNCCOMPLETION pTask);

/**
 * Suspends a async completion task.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PDM_ASYNC_COMPLETION_NOT_SUSPENDED if not suspended.
 * @retval  VERR_INVALID_HANDLE if pTask is invalid (asserts).
 * @param   pTask           The async completion task.
 */
PDMR3DECL(int) PDMR3AsyncCompletionResume(PPDMASYNCCOMPLETION pTask);

/**
 * Destroys a async completion task.
 *
 * The task doesn't have to be suspended or anything.
 *
 * @returns VBox status codes:
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_HANDLE if pTask is invalid but not NIL (asserts).
 * @param   pTask           The async completion task.
 */
PDMR3DECL(int) PDMR3AsyncCompletionDestroy(PPDMASYNCCOMPLETION pTask);

/** @} */

__END_DECLS

#endif



