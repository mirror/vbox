/* $Id$ */
/** @file
 * EbmlWriter.h - EBML writer + WebM container.
 */

/*
 * Copyright (C) 2013-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____EBMLWRITER
#define ____EBMLWRITER

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4668) /* vpx_codec.h(64) : warning C4668: '__GNUC__' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif' */
#include <vpx/vpx_encoder.h>
# pragma warning(pop)
#else
# include <vpx/vpx_encoder.h>
#endif

class WebMWriter_Impl;

class WebMWriter
{
public:
    WebMWriter();
    virtual ~WebMWriter();

    /** Creates output file
     *
     * @param   a_pszFilename   Name of the file to create.
     *
     * @returns VBox status code. */
    int create(const char *a_pszFilename);

    /* Closes output file. */
    void close();

    /** Writes WebM header to file.
     *
     * Should be called before any writeBlock call.
     *
     * @param a_pCfg Pointer to VPX Codec configuration structure.
     * @param a_pFps Framerate information (frames per second).
     *
     * @returns VBox status code.
     */
    int writeHeader(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_rational *a_pFps);

    /** Writes a block of compressed data
     *
     * @param a_pCfg Pointer to VPX Codec configuration structure.
     * @param a_pPkt VPX data packet.
     *
     * @returns VBox status code.
     */
    int writeBlock(const vpx_codec_enc_cfg_t *a_pCfg, const vpx_codec_cx_pkt_t *a_pPkt);

    /** Writes WebM footer.
     *
     * No other write functions should be called after this one.
     *
     * @param a_u64Hash Hash value for the data written.
     *
     * @returns VBox status code.
     */
    int writeFooter(uint32_t a_u64Hash);

    /** Gets current output file size.
     *
     * @returns File size in bytes.
     */
    uint64_t getFileSize();

    /** Gets current free storage space
     * available for the file.
     *
     * @returns Available storage free space.
     */
    uint64_t getAvailableSpace();

private:
    /** WebMWriter implementation.
     * To isolate some include files */
    WebMWriter_Impl *m_Impl;

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(WebMWriter);
};

#endif
