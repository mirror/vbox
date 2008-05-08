/** @file
 * VBoxGuestLib - Support library header for VirtualBox
 * Additions: Public header.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_VBoxGuestLib_h
#define ___VBox_VBoxGuestLib_h

#include <VBox/VBoxGuest.h>

#include <VBox/err.h>

#ifndef IN_RING0
#error VBoxGuestLib is only suitable for ring-0!
#endif

/** @defgroup grp_guest_lib     VirtualBox Guest Library
 * @{
 */

/** @page pg_guest_lib  VirtualBox Guest Library
 *
 * The library has 2 versions for each platform:
 *  1) for VBoxGuest main driver, who is responsible for managing the VMMDev virtual hardware;
 *  2) for other guest drivers.
 *
 * Library therefore consists of:
 *  common code to be used by both VBoxGuest and other drivers;
 *  VBoxGuest specific code;
 *  code for other drivers which communicate with VBoxGuest via an IOCTL.
 *
 * The library sources produce 2 libs VBoxGuestLib and VBoxGuestLibBase,
 * the latter one is for VBoxGuest.
 *
 * Drivers must choose right library in their makefiles.
 *
 * Library source code and the header have a define VBGL_VBOXGUEST,
 * which is defined for VBoxGuest and undefined for other drivers.
 *
 */

#define DECLVBGL(type) type VBOXCALL

typedef uint32_t VBGLIOPORT;

__BEGIN_DECLS

#ifdef VBGL_VBOXGUEST

/**
 * The library initialization function to be used by the main
 * VBoxGuest system driver.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglInit (VBGLIOPORT portVMMDev, VMMDevMemory *pVMMDevMemory);

#else

/**
 * The library initialization function to be used by all drivers
 * other than the main VBoxGuest system driver.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglInit (void);

#endif

/**
 * The library termination function.
 */
DECLVBGL(void) VbglTerminate (void);

/** @name Generic request functions.
 * @{
 */

/**
 * Allocate memory for generic request and initialize the request header.
 *
 * @param ppReq    pointer to resulting memory address.
 * @param cbSize   size of memory block required for the request.
 * @param reqType  the generic request type.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglGRAlloc (VMMDevRequestHeader **ppReq, uint32_t cbSize, VMMDevRequestType reqType);

/**
 * Perform the generic request.
 *
 * @param pReq     pointer the request structure.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglGRPerform (VMMDevRequestHeader *pReq);

/**
 * Free the generic request memory.
 *
 * @param pReq     pointer the request structure.
 *
 * @return VBox status code.
 */
DECLVBGL(void) VbglGRFree (VMMDevRequestHeader *pReq);
/** @} */

#ifdef VBOX_HGCM

#ifdef VBGL_VBOXGUEST

/**
 * Callback function called from HGCM helpers when a wait for request
 * completion IRQ is required.
 *
 * @param pvData     VBoxGuest pointer to be passed to callback.
 * @param u32Data    VBoxGuest 32 bit value to be passed to callback.
 */

typedef DECLVBGL(void) VBGLHGCMCALLBACK(VMMDevHGCMRequestHeader *pHeader, void *pvData, uint32_t u32Data);

/**
 * Perform a connect request. That is locate required service and
 * obtain a client identifier for future access.
 *
 * @note This function can NOT handle cancelled requests!
 *
 * @param pConnectInfo    The request data.
 * @param pAsyncCallback  Required pointer to function that is called when
 *                        host returns VINF_HGCM_ASYNC_EXECUTE. VBoxGuest
 *                        implements waiting for an IRQ in this function.
 * @param pvAsyncData     An arbitrary VBoxGuest pointer to be passed to callback.
 * @param u32AsyncData    An arbitrary VBoxGuest 32 bit value to be passed to callback.
 *
 * @return VBox status code.
 */

DECLVBGL(int) VbglHGCMConnect (VBoxGuestHGCMConnectInfo *pConnectInfo,
                               VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData);


/**
 * Perform a disconnect request. That is tell the host that
 * the client will not call the service anymore.
 *
 * @note This function can NOT handle cancelled requests!
 *
 * @param pDisconnectInfo The request data.
 * @param pAsyncCallback  Required pointer to function that is called when
 *                        host returns VINF_HGCM_ASYNC_EXECUTE. VBoxGuest
 *                        implements waiting for an IRQ in this function.
 * @param pvAsyncData     An arbitrary VBoxGuest pointer to be passed to callback.
 * @param u32AsyncData    An arbitrary VBoxGuest 32 bit value to be passed to callback.
 *
 * @return VBox status code.
 */

DECLVBGL(int) VbglHGCMDisconnect (VBoxGuestHGCMDisconnectInfo *pDisconnectInfo,
                                  VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData);

/** Call a HGCM service.
 *
 * @note This function can deal with cancelled requests.
 *
 * @param pCallInfo       The request data.
 * @param pAsyncCallback  Required pointer to function that is called when
 *                        host returns VINF_HGCM_ASYNC_EXECUTE. VBoxGuest
 *                        implements waiting for an IRQ in this function.
 * @param pvAsyncData     An arbitrary VBoxGuest pointer to be passed to callback.
 * @param u32AsyncData    An arbitrary VBoxGuest 32 bit value to be passed to callback.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglHGCMCall (VBoxGuestHGCMCallInfo *pCallInfo,
                            VBGLHGCMCALLBACK *pAsyncCallback, void *pvAsyncData, uint32_t u32AsyncData);

#else /* !VBGL_VBOXGUEST */

struct VBGLHGCMHANDLEDATA;
typedef struct VBGLHGCMHANDLEDATA *VBGLHGCMHANDLE;

/** @name HGCM functions
 * @{
 */

/**
 * Connect to a service.
 *
 * @param pHandle     Pointer to variable that will hold a handle to be used
 *                    further in VbglHGCMCall and VbglHGCMClose.
 * @param pData       Connection information structure.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglHGCMConnect (VBGLHGCMHANDLE *pHandle, VBoxGuestHGCMConnectInfo *pData);

/**
 * Connect to a service.
 *
 * @param handle      Handle of the connection.
 * @param pData       Disconnect request information structure.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglHGCMDisconnect (VBGLHGCMHANDLE handle, VBoxGuestHGCMDisconnectInfo *pData);

/**
 * Call to a service.
 *
 * @param handle      Handle of the connection.
 * @param pData       Call request information structure, including function parameters.
 * @param cbData      Length in bytes of data.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglHGCMCall (VBGLHGCMHANDLE handle, VBoxGuestHGCMCallInfo *pData, uint32_t cbData);
/** @} */

#endif /* !VBGL_VBOXGUEST */

#endif /* VBOX_HGCM */


/**
 * Initialize the heap.
 *
 * @return VBox error code.
 */
DECLVBGL(int) VbglPhysHeapInit (void);

/**
 * Shutdown the heap.
 */
DECLVBGL(void) VbglPhysHeapTerminate (void);


/**
 * Allocate a memory block.
 *
 * @param cbSize    Size of block to be allocated.
 * @return Virtual address of allocated memory block.
 */
DECLVBGL(void *) VbglPhysHeapAlloc (uint32_t cbSize);

/**
 * Get physical address of memory block pointed by
 * the virtual address.
 *
 * @note WARNING!
 *       The function does not acquire the Heap mutex!
 *       When calling the function make sure that
 *       the pointer is a valid one and is not being
 *       deallocated.
 *       This function can NOT be used for verifying
 *       if the given pointer is a valid one allocated
 *       from the heap.
 *
 *
 * @param p    Virtual address of memory block.
 * @return Physical memory block.
 */
DECLVBGL(RTCCPHYS) VbglPhysHeapGetPhysAddr (void *p);

/**
 * Free a memory block.
 *
 * @param p    Virtual address of memory block.
 */
DECLVBGL(void) VbglPhysHeapFree (void *p);

DECLVBGL(int) VbglQueryVMMDevMemory (VMMDevMemory **ppVMMDevMemory);

__END_DECLS

/** @} */

#endif
