/* $Id$ */
/** @file
 * VBoxNetUDP - IntNet Client Library.
 */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxNetBaseService_h___
#define ___VBoxNetBaseService_h___

#include <iprt/critsect.h>
class VBoxNetBaseService
{
public:
    VBoxNetBaseService();
    virtual ~VBoxNetBaseService();
    int                 parseArgs(int argc, char **argv);
    int                 tryGoOnline(void);
    void                shutdown(void);
    int                 syncEnter() { return RTCritSectEnter(&this->m_csThis);}
    int                 syncLeave() { return RTCritSectLeave(&this->m_csThis);}
    int                 waitForIntNetEvent(int cMillis);
    int                 sendBufferOnWire(PCINTNETSEG pSg, int cSg, size_t cbBuffer);
    void                flushWire();

    virtual void        usage(void) = 0;
    virtual int         run(void) = 0;
    virtual int         parseOpt(int rc, const RTGETOPTUNION& getOptVal) = 0;

    virtual int         init(void);
    virtual bool        isMainNeeded() { return m_fNeedMain; }
    /* VirtualBox instance */
    ComPtr<IVirtualBox> virtualbox;

protected:
    /**
     * Print debug message depending on the m_cVerbosity level.
     *
     * @param   iMinLevel       The minimum m_cVerbosity level for this message.
     * @param   fMsg            Whether to dump parts for the current DHCP message.
     * @param   pszFmt          The message format string.
     * @param   ...             Optional arguments.
     */
    void debugPrint(int32_t iMinLevel, bool fMsg, const char *pszFmt, ...) const
    {
        if (iMinLevel <= m_cVerbosity)
        {
            va_list va;
            va_start(va, pszFmt);
            debugPrintV(iMinLevel, fMsg, pszFmt, va);
            va_end(va);
        }
    }

    virtual void debugPrintV(int32_t iMinLevel, bool fMsg, const char *pszFmt, va_list va) const;

    /** @name The server configuration data members.
     * @{ */
    std::string         m_Name;
    std::string         m_Network;
    std::string         m_TrunkName;
    INTNETTRUNKTYPE     m_enmTrunkType;
    RTMAC               m_MacAddress;
    RTNETADDRIPV4       m_Ipv4Address;
    RTNETADDRIPV4       m_Ipv4Netmask;
    /** @} */
    /** @name The network interface
     * @{ */
    PSUPDRVSESSION      m_pSession;
    uint32_t            m_cbSendBuf;
    uint32_t            m_cbRecvBuf;
    INTNETIFHANDLE      m_hIf;          /**< The handle to the network interface. */
    PINTNETBUF          m_pIfBuf;       /**< Interface buffer. */
    std::vector<PRTGETOPTDEF> m_vecOptionDefs;
    /** @} */
    /** @name Debug stuff
     * @{  */
    int32_t             m_cVerbosity;
private:
    PRTGETOPTDEF getOptionsPtr();

    /* cs for syncing */
    RTCRITSECT          m_csThis;
    /* Controls whether service will connect SVC for runtime needs */
    bool                m_fNeedMain;

    /** @} */
};
#endif
