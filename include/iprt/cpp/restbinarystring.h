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

    /** Frees the data held by the object.
     * Will set m_pbData to NULL and m_cbData to UINT64_MAX.  */
    void freeData();

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
     * Retrieves the callback data.
     */
    void *getCallbackData() const  { return m_pvCallbackData; }


    /** @name Upload methods
     * @{ */

    /**
     * Sets the content-type for an upload.
     *
     * @returns VINF_SUCCESS or VERR_NO_STR_MEMORY.
     * @param   a_pszContentType    The content type to set.
     *                              If NULL, no content type is set.
     */
    int setContentType(const char *a_pszContentType);

    /**
     * Gets the content type that was set.
     */
    RTCString const &getContentType() const { return m_strContentType; }

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
     *
     * @note    This will drop any previously registered producer callback and user data..
     */
    int setUploadData(void const *a_pvData, size_t a_cbData, bool a_fCopy = true);

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
     * Sets the producer callback.
     *
     * @param   a_pfnProducer       The callback function for producing data.
     * @param   a_pvCallbackData    Data the can be retrieved from the callback
     *                              using getCallbackData().
     * @param   a_cbContentLength   The amount of data that will be uploaded and
     *                              to be set as the value of the content-length
     *                              header field.  Pass UINT64_MAX if not known.
     *
     * @note    This will drop any buffer previously registered using setUploadData().
     */
    void setProducerCallback(PFNPRODUCER a_pfnProducer, void *a_pvCallbackData = NULL, uint64_t a_cbContentLength = UINT64_MAX);

    /**
     * Preprares transmission via the @a a_hHttp client instance.
     *
     * @returns IPRT status code.
     * @param   a_hHttp             The HTTP client instance.
     * @internal
     */
    virtual int xmitPrepare(RTHTTP a_hHttp) const;

    /**
     * For completing and/or undoing setup from xmitPrepare.
     *
     * @param   a_hHttp             The HTTP client instance.
     * @internal
     */
    virtual void xmitComplete(RTHTTP a_hHttp) const;
    /** @} */


    /** @name Download methods
     * @{ */

    /**
     * Sets the max size to download to memory.
     *
     * This also indicates the intention to download to a memory buffer, so it
     * will drop any previously registered consumer callback and its user data.
     *
     * @param   a_cbMax Maximum number of bytes to download to memory.
     *                  If 0, a default is selected (currently 32MiB for
     *                  32-bit hosts and 128MiB for 64-bit).
     */
    void setMaxDownloadSize(size_t a_cbMaxDownload);

    /**
     * Gets the content-length value (UINT64_MAX if not available).
     */
    uint64_t getContentLength() const { return m_cbContentLength; }

    /**
     * Gets the number of bytes that has actually been downloaded.
     */
    uint64_t getDownloadSize() const { return m_cbDownloaded; }

    /**
     * Returns the pointer to the download buffer.
     * @note returns NULL if setConsumerCallback was used or no data was downloaded.
     */
    uint8_t const *getDownloadData() const { return m_pbData; }

    /**
     * Callback for consuming downloaded bytes.
     *
     * @returns IPRT status code.
     * @param   a_pThis         The related string object.
     * @param   a_pvSrc         Buffer containing the bytes.
     * @param   a_cbSrc         The number of bytes in the buffer.
     * @param   a_uHttpStatus   The HTTP status code.
     * @param   a_offContent    The byte offset corresponding to the start of @a a_pvSrc.
     * @param   a_cbContent     The content length field value, UINT64_MAX if not available.
     * @remarks Use getCallbackData to get the user data.
     */
    typedef DECLCALLBACK(int) FNCONSUMER(RTCRestBinaryString *a_pThis, const void *a_pvSrc, size_t a_cbSrc,
                                         uint32_t a_uHttpStatus, uint64_t a_offContent, uint64_t a_cbContent);
    /** Pointer to a byte consumer callback. */
    typedef FNCONSUMER *PFNCONSUMER;

    /**
     * Sets the consumer callback.
     *
     * @param   a_pfnConsumer       The callback function for consuming downloaded data.
     *                              NULL if data should be stored in m_pbData (the default).
     * @param   a_pvCallbackData    Data the can be retrieved from the callback
     *                              using getCallbackData().
     */
    void setConsumerCallback(PFNCONSUMER a_pfnConsumer, void *a_pvCallbackData = NULL);

    /**
     * Preprares for receiving via the @a a_hHttp client instance.
     *
     * @returns IPRT status code.
     * @param   a_hHttp             The HTTP client instance.
     * @param   a_fCallbackFlags    The HTTP callback flags (status code spec).
     * @internal
     */
    virtual int receivePrepare(RTHTTP a_hHttp, uint32_t a_fCallbackFlags);

    /**
     * For completing and/or undoing setup from receivePrepare.
     *
     * @param   a_hHttp             The HTTP client instance.
     * @internal
     */
    virtual void receiveComplete(RTHTTP a_hHttp);
    /** @} */


protected:
    /** Pointer to the bytes, if provided directly. (both) */
    uint8_t    *m_pbData;
    /** Number of bytes allocated for the m_pbData buffer (both). */
    size_t      m_cbAllocated;
    /** Set if m_pbData must be freed (both). */
    bool        m_fFreeData;
    /** Number of bytes corresponding to content-length.
     * UINT64_MAX if not known.  Used both for unploads and downloads. */
    uint64_t    m_cbContentLength;
    /** User argument for both callbacks (both). */
    void       *m_pvCallbackData;

    /** Pointer to user-registered producer callback function (upload only). */
    PFNPRODUCER m_pfnProducer;
    /** The content type if set (upload only). */
    RTCString   m_strContentType;

    /** Pointer to user-registered consumer callback function (download only). */
    PFNCONSUMER m_pfnConsumer;
    /** Number of bytes downloaded thus far. */
    uint64_t    m_cbDownloaded;
    /** Maximum data to download to memory (download only). */
    size_t      m_cbMaxDownload;

    /** Callback for use with RTHttpSetUploadCallback. */
    static FNRTHTTPUPLOADCALLBACK   xmitHttpCallback;
    /** Callback for use with RTHttpSetDownloadCallback. */
    static FNRTHTTPDOWNLOADCALLBACK receiveHttpCallback;

private:
    /* No copy constructor or copy assignment: */
    RTCRestBinaryString(RTCRestBinaryString const &a_rThat);
    RTCRestBinaryString &operator=(RTCRestBinaryString const &a_rThat);
};

#endif

