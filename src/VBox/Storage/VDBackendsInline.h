/* $Id$ */
/** @file
 * VD - backends inline helpers.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifndef ___VDBackendsInline_h
#define ___VDBackendsInline_h

#include <iprt/cdefs.h>

RT_C_DECLS_BEGIN

/**
 * Stub for VDIMAGEBACKEND::pfnGetComment when the backend doesn't suport comments.
 *
 * @returns VBox status code.
 * @param a_szPrefixBackend    The backend prefix.
 */
#define VD_BACKEND_CALLBACK_GET_COMMENT_DEF_NOT_SUPPORTED(a_szStubName) \
    static DECLCALLBACK(int) a_szStubName(void *pvBackend, char *pszComment, size_t cbComment) \
    { \
        RT_NOREF2(pszComment, cbComment); \
        LogFlowFunc(("pvBackend=%#p pszComment=%#p cbComment=%zu\n", pvBackend, pszComment, cbComment)); \
        AssertPtrReturn(pvBackend, VERR_VD_NOT_OPENED); \
        LogFlowFunc(("returns %Rrc comment='%s'\n", VERR_NOT_SUPPORTED, pszComment)); \
        return VERR_NOT_SUPPORTED; \
    } \
    typedef int ignore_semicolon


/**
 * Stub for VDIMAGEBACKEND::pfnSetComment when the backend doesn't suport comments.
 *
 * @returns VBox status code.
 * @param a_szPrefixBackend    The backend prefix.
 * @param a_ImageStateType     Type of the backend image state.
 */
#define VD_BACKEND_CALLBACK_SET_COMMENT_DEF_NOT_SUPPORTED(a_szStubName, a_ImageStateType) \
    static DECLCALLBACK(int) a_szStubName(void *pvBackend, const char *pszComment) \
    { \
        RT_NOREF1(pszComment); \
        LogFlowFunc(("pvBackend=%#p pszComment=\"%s\"\n", pvBackend, pszComment)); \
        a_ImageStateType pThis = (a_ImageStateType)pvBackend; \
        AssertPtrReturn(pThis, VERR_VD_NOT_OPENED); \
        int rc = VERR_NOT_SUPPORTED; \
        if (pThis->uOpenFlags & VD_OPEN_FLAGS_READONLY) \
            rc = VERR_VD_IMAGE_READ_ONLY; \
        LogFlowFunc(("returns %Rrc\n", rc)); \
        return rc; \
    } \
    typedef int ignore_semicolon


/**
 * Stub for VDIMAGEBACKEND::pfnGetUuid when the backend doesn't suport UUIDs.
 *
 * @returns VBox status code.
 * @param a_szPrefixBackend    The backend prefix.
 */
#define VD_BACKEND_CALLBACK_GET_UUID_DEF_NOT_SUPPORTED(a_szStubName) \
    static DECLCALLBACK(int) a_szStubName(void *pvBackend, PRTUUID pUuid) \
    { \
        RT_NOREF1(pUuid); \
        LogFlowFunc(("pvBackend=%#p pUuid=%#p\n", pvBackend, pUuid)); \
        AssertPtrReturn(pvBackend, VERR_VD_NOT_OPENED); \
        LogFlowFunc(("returns %Rrc (%RTuuid)\n", VERR_NOT_SUPPORTED, pUuid)); \
        return VERR_NOT_SUPPORTED; \
    } \
    typedef int ignore_semicolon


/**
 * Stub for VDIMAGEBACKEND::pfnSetComment when the backend doesn't suport UUIDs.
 *
 * @returns VBox status code.
 * @param a_szPrefixBackend    The backend prefix.
 * @param a_ImageStateType     Type of the backend image state.
 */
#define VD_BACKEND_CALLBACK_SET_UUID_DEF_NOT_SUPPORTED(a_szStubName, a_ImageStateType) \
    static DECLCALLBACK(int) a_szStubName(void *pvBackend, PCRTUUID pUuid) \
    { \
        RT_NOREF1(pUuid); \
        LogFlowFunc(("pvBackend=%#p Uuid=%RTuuid\n", pvBackend, pUuid)); \
        a_ImageStateType pThis = (a_ImageStateType)pvBackend; \
        AssertPtrReturn(pThis, VERR_VD_NOT_OPENED); \
        int rc = VERR_NOT_SUPPORTED; \
        if (pThis->uOpenFlags & VD_OPEN_FLAGS_READONLY) \
            rc = VERR_VD_IMAGE_READ_ONLY; \
        LogFlowFunc(("returns %Rrc\n", rc)); \
        return rc; \
    } \
    typedef int ignore_semicolon

RT_C_DECLS_END

#endif
