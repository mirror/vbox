/** @file
 * IPRT - C++ Representational State Transfer (REST) Binary String Class.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
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

#ifndef ___iprt_cpp_restbinarystring_h
#define ___iprt_cpp_restbinarystring_h

#include <iprt/cpp/restbase.h>
#include <iprt/http.h>


/** @defgroup grp_rt_cpp_restbinstr C++ Representational State Transfer (REST) Binary String Class.
 * @ingroup grp_rt_cpp
 * @{
 */

/**
 * Class for handling strings on the binary format.
 *
 * This can only be used for body parameters.
 */
class RT_DECL_CLASS RTCRestBinaryString : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestBinaryString();
    /** Destructor. */
    virtual ~RTCRestBinaryString();

    /** Safe copy assignment method. */
    int assignCopy(RTCRestBinaryString const &a_rThat);

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = kCollectionFormat_Unspecified) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

    /**
     * Gets the data size.
     *
     * This can be used from a consumer callback to get the Content-Length field
     * value if available.  Returns UINT64_MAX if not available.
     */
    uint64_t getDataSize() const { return m_cbData; }

    /**
     * Sets the data to upload.
     *
     * @returns IPRT status code.
     * @param   a_pvData            The data buffer.  NULL can be used to actively
     *                              deregister previous data.
     * @param   a_cbData            The amount of data to upload from that buffer.
     * @param   a_fCopy             Whether to make a copy (@a true) or use the
     *                              buffer directly (@a false).  In the latter case
     *                              the caller must make sure the data remains available
     *                              for the entire lifetime of this object (or until
     *                              setUploadData is called with NULL parameters).
     * @param   a_pszContentType    Specifies the content type to set.  Pass NULL (default)
     *                              to not explictly set the content type.
     */
    int setUploadData(void const *a_pvData, size_t a_cbData, bool a_fCopy, const char *a_pszContentType = NULL);

    /** @name Data callbacks.
     * @{ */
    /**
     * Callback for producing bytes to upload.
     *
     * @returns IPRT status code.
     * @param   a_pThis         The related string object.
     * @param   a_pvDst         Where to put the bytes.
     * @param   a_cbDst         Max number of bytes to produce.
     * @param   a_offContent    The byte offset corresponding to the start of @a a_pvDst.
     * @param   a_pcbActual     Where to return the number of bytes actually produced.
     * @remarks Use getCallbackData to get the user data.
     */
    typedef DECLCALLBACK(int) FNPRODUCER(RTCRestBinaryString *a_pThis, void *a_pvDst, size_t a_cbDst,
                                         uint64_t a_offContent, size_t *a_pcbActual);
    /** Pointer to a byte producer callback. */
    typedef FNPRODUCER *PFNPRODUCER;

    /**
     * Callback for consuming downloaded bytes.
     *
     * @returns IPRT status code.
     * @param   a_pThis     The related string object.
     * @param   a_pvSrc     Buffer containing the bytes.
     * @param   a_cbSrc     The number of bytes in the buffer.
     * @remarks Use getCallbackData to get the user data.
     */
    typedef DECLCALLBACK(int) FNCONSUMER(RTCRestBinaryString *a_pThis, const void *a_pvSrc, size_t a_cbSrc);
    /** Pointer to a byte consumer callback. */
    typedef FNCONSUMER *PFNCONSUMER;

    /**
     * Retrieves the callback data.
     */
    void *getCallbackData() const  { return m_pvCallbackData; }

    /**
     * Sets the consumer callback.
     *
     * @returns IPRT status code.
     * @param   a_pfnConsumer       The callback function for consuming downloaded data.
     *                              NULL if data should be stored in m_pbData/m_cbData (the default).
     * @param   a_pvCallbackData    Data the can be retrieved from the callback
     *                              using getCallbackData().
     */
    int setConsumerCallback(PFNCONSUMER a_pfnConsumer, void *a_pvCallbackData = NULL);

    /**
     * Sets the producer callback.
     *
     * @returns IPRT status code.
     * @param   a_pfnProducer       The callback function for producing data.
     * @param   a_pvCallbackData    Data the can be retrieved from the callback
     *                              using getCallbackData().
     * @param   a_cbData            The amount of data that will be uploaded,
     *                              UINT64_MAX if not unknown.
     *
     * @note    This will drop any buffer previously registered using
     *          setUploadData(), unless a_pfnProducer is NULL.
     */
    int setProducerCallback(PFNPRODUCER a_pfnProducer, void *a_pvCallbackData = NULL, uint64_t a_cbData = UINT64_MAX);
    /** @} */


    /**
     * Preprares transmission via @a a_hHttp.
     *
     * @returns IPRT status code.
     * @param   a_hHttp             The HTTP client instance.
     */
    virtual int  xmitPrepare(RTHTTP a_hHttp) const;

    /**
     * For completing and/or undoing setup from xmitPrepare.
     *
     * @param   a_hHttp             The HTTP client instance.
     */
    virtual void xmitComplete(RTHTTP a_hHttp) const;

    //virtual int receivePrepare(RTHTTP a_hHttp);
    //virtual int receiveComplete(int a_rcStatus, RTHTTP a_hHttp);

protected:
    /** Pointer to the bytes, if provided directly. */
    uint8_t    *m_pbData;
    /** Number of bytes.  UINT64_MAX if not known. */
    uint64_t    m_cbData;
    /** User argument for callbacks. */
    void       *m_pvCallbackData;
    /** Pointer to user-registered consumer callback function. */
    PFNCONSUMER m_pfnConsumer;
    /** Pointer to user-registered producer callback function. */
    PFNPRODUCER m_pfnProducer;
    /** Set if m_pbData must be freed. */
    bool        m_fFreeData;
    /** The content type (upload only). */
    RTCString   m_strContentType;


    static FNRTHTTPUPLOADCALLBACK xmitHttpCallback;

private:
    /* No copy constructor or copy assignment: */
    RTCRestBinaryString(RTCRestBinaryString const &a_rThat);
    RTCRestBinaryString &operator=(RTCRestBinaryString const &a_rThat);
};

#endif

