/* $Id$ */
/** @file
 * VBoxNetUDP - IntNet Client Library.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
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
class VBoxNetBaseService
{
public:
    VBoxNetBaseService();
    virtual ~VBoxNetBaseService();
    int                 parseArgs(int argc, char **argv);
    int                 tryGoOnline(void);
    void                shutdown(void);
    virtual void        usage(void) = 0;
    virtual void        run(void) = 0;
    virtual int         init(void);
    virtual int         parseOpt(int rc, const RTGETOPTUNION& getOptVal) = 0;

    inline void         debugPrint( int32_t iMinLevel, bool fMsg,  const char *pszFmt, ...) const;
    void                debugPrintV(int32_t iMinLevel, bool fMsg,  const char *pszFmt, va_list va) const;
public:
    /** @name The server configuration data members.
     * @{ */
    std::string         m_Name;
    std::string         m_Network;
    std::string         m_TrunkName;
    INTNETTRUNKTYPE     m_enmTrunkType;
    RTMAC               m_MacAddress;
    RTNETADDRIPV4       m_Ipv4Address;
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
    /** @} */
};
#endif
