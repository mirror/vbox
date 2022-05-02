/* $Id$ */
/** @file
 * Main - Cryptographic utility functions used by both VBoxSVC and VBoxC.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_CryptoUtils_h
#define MAIN_INCLUDED_CryptoUtils_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/vfs.h>

#include <VBox/VBoxCryptoIf.h>
#include <VBox/com/string.h>
#include <VBox/vmm/ssm.h>

#include "SecretKeyStore.h"
#ifdef VBOX_COM_INPROC
# include "ConsoleImpl.h"
#else
# include "MachineImpl.h"
# include "VirtualBoxImpl.h"
#endif


/**
 * Class handling encrypted and non encrypted SSM files.
 */
class SsmStream
{
    public:
#ifdef VBOX_COM_INPROC
        SsmStream(Console *pParent, SecretKeyStore *pKeyStore, const Utf8Str &strKeyId, const Utf8Str &strKeyStore);
#else
        SsmStream(VirtualBox *pParent, SecretKeyStore *pKeyStore, const Utf8Str &strKeyId, const Utf8Str &strKeyStore);
#endif
        ~SsmStream();

        /**
         * Actually opens the stream for either reading or writing.
         *
         * @returns VBox status code.
         * @param   strFilename The filename of the saved state to open or create.
         * @param   fWrite      Flag whether the stream should be opened for writing (true) or readonly (false).
         * @param   ppSsmHandle Where to store the SSM handle on success, don't call SSMR3Close() but the provided close() method.
         */
        int open(const Utf8Str &strFilename, bool fWrite, PSSMHANDLE *ppSsmHandle);

        /**
         * Closes an previously opened stream.
         *
         * @returns VBox status code.
         */
        int close(void);

    private:

        static DECLCALLBACK(int) i_ssmCryptoWrite(void *pvUser, uint64_t offStream, const void *pvBuf, size_t cbToWrite);
        static DECLCALLBACK(int) i_ssmCryptoRead(void *pvUser, uint64_t offStream, void *pvBuf, size_t cbToRead, size_t *pcbRead);
        static DECLCALLBACK(int) i_ssmCryptoSeek(void *pvUser, int64_t offSeek, unsigned uMethod, uint64_t *poffActual);
        static DECLCALLBACK(uint64_t) i_ssmCryptoTell(void *pvUser);
        static DECLCALLBACK(int) i_ssmCryptoSize(void *pvUser, uint64_t *pcb);
        static DECLCALLBACK(int) i_ssmCryptoIsOk(void *pvUser);
        static DECLCALLBACK(int) i_ssmCryptoClose(void *pvUser, bool fCancelled);

#ifdef VBOX_COM_INPROC
        Console        *m_pParent;
#else
        VirtualBox     *m_pParent;
#endif
        /** The key store for getting at passwords. */
        SecretKeyStore *m_pKeyStore;
        /** The key ID holding the password, empty if the saved state is not encrypted. */
        Utf8Str        m_strKeyId;
        /** The keystore holding the encrypted DEK. */
        Utf8Str        m_strKeyStore;
        /** The VFS file handle. */
        RTVFSFILE      m_hVfsFile;
        /** The SSM handle when opened. */
        PSSMHANDLE     m_pSsm;
        /** The SSM stream callbacks table. */
        SSMSTRMOPS     m_StrmOps;
        /** The cryptographic interfacer. */
        PCVBOXCRYPTOIF m_pCryptoIf;
};

#endif /* !MAIN_INCLUDED_CryptoUtils_h */
