/* $Id$ */
/** @file
 * DHCP server - DHCP options
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef _DHCP_OPTIONS_H_
#define _DHCP_OPTIONS_H_

#include "Defs.h"

#include <string.h>

#include <iprt/err.h>
#include <iprt/types.h>
#include <iprt/asm.h>
#include <iprt/stdint.h>
#include <iprt/net.h>

#include <string>

class DhcpClientMessage;


class DhcpOption
{
  protected:
    uint8_t m_OptCode;
    bool m_fPresent;

  public:
    explicit DhcpOption(uint8_t aOptCode)
      : m_OptCode(aOptCode), m_fPresent(true) {}

    DhcpOption(uint8_t aOptCode, bool fPresent)
      : m_OptCode(aOptCode), m_fPresent(fPresent) {}

    virtual DhcpOption *clone() const = 0;

    virtual ~DhcpOption() {}

  public:
    static DhcpOption *parse(uint8_t aOptCode, int aEnc, const char *pcszValue);

  public:
    uint8_t optcode() const { return m_OptCode; }
    bool present() const { return m_fPresent; }

  public:
    int encode(octets_t &dst) const;

    int decode(const rawopts_t &map);
    int decode(const DhcpClientMessage &req);

  protected:
    virtual ssize_t encodeValue(octets_t &dst) const = 0;
    virtual int decodeValue(const octets_t &src, size_t cb) = 0;

  protected:
    static const octets_t *findOption(const rawopts_t &aOptMap, uint8_t aOptCode);

  protected:
    /*
     * Serialization
     */
    static void append(octets_t &aDst, uint8_t aValue)
    {
        aDst.push_back(aValue);
    }

    static void append(octets_t &aDst, uint16_t aValue)
    {
        RTUINT16U u16 = { RT_H2N_U16(aValue) };
        aDst.insert(aDst.end(), u16.au8, u16.au8 + sizeof(aValue));
    }

    static void append(octets_t &aDst, uint32_t aValue)
    {
        RTUINT32U u32 = { RT_H2N_U32(aValue) };
        aDst.insert(aDst.end(), u32.au8, u32.au8 + sizeof(aValue));
    }

    static void append(octets_t &aDst, RTNETADDRIPV4 aIPv4)
    {
        aDst.insert(aDst.end(), aIPv4.au8, aIPv4.au8 + sizeof(aIPv4));
    }

    static void append(octets_t &aDst, const char *pszString, size_t cb)
    {
        aDst.insert(aDst.end(), pszString, pszString + cb);
    }

    static void append(octets_t &aDst, const std::string &str)
    {
        append(aDst, str.c_str(), str.size());
    }

    /* non-overloaded name to avoid ambiguity */
    static void appendLength(octets_t &aDst, size_t cb)
    {
        append(aDst, static_cast<uint8_t>(cb));
    }


    /*
     * Deserialization
     */
    static void extract(uint8_t &aValue, octets_t::const_iterator &pos)
    {
        aValue = *pos;
        pos += sizeof(uint8_t);
    }

    static void extract(uint16_t &aValue, octets_t::const_iterator &pos)
    {
        RTUINT16U u16;
        memcpy(u16.au8, &pos[0], sizeof(uint16_t));
        aValue = RT_N2H_U16(u16.u);
        pos += sizeof(uint16_t);
    }

    static void extract(uint32_t &aValue, octets_t::const_iterator &pos)
    {
        RTUINT32U u32;
        memcpy(u32.au8, &pos[0], sizeof(uint32_t));
        aValue = RT_N2H_U32(u32.u);
        pos += sizeof(uint32_t);
    }

    static void extract(RTNETADDRIPV4 &aValue, octets_t::const_iterator &pos)
    {
        memcpy(aValue.au8, &pos[0], sizeof(RTNETADDRIPV4));
        pos += sizeof(RTNETADDRIPV4);
    }

    static void extract(std::string &aString, octets_t::const_iterator &pos, size_t cb)
    {
        aString.replace(aString.begin(), aString.end(), &pos[0], &pos[cb]);
        pos += cb;
    }


    /*
     * Parse textual representation (e.g. in config file)
     */
    static int parse1(uint8_t &aValue, const char *pcszValue);
    static int parse1(uint16_t &aValue, const char *pcszValue);
    static int parse1(uint32_t &aValue, const char *pcszValue);
    static int parse1(RTNETADDRIPV4 &aValue, const char *pcszValue);

    static int parseList(std::vector<RTNETADDRIPV4> &aList, const char *pcszValue);

    static int parseHex(octets_t &aRawValue, const char *pcszValue);
};


inline octets_t &operator<<(octets_t &dst, const DhcpOption &option)
{
    option.encode(dst);
    return dst;
}


optmap_t &operator<<(optmap_t &optmap, DhcpOption *option);
optmap_t &operator<<(optmap_t &optmap, const std::shared_ptr<DhcpOption> &option);



/*
 * Only for << OptEnd() syntactic sugar...
 */
struct OptEnd {};
inline octets_t &operator<<(octets_t &dst, const OptEnd &end)
{
    RT_NOREF(end);

    dst.push_back(RTNET_DHCP_OPT_END);
    return dst;
}



/*
 * Option that has no value
 */
class OptNoValueBase
  : public DhcpOption
{
  public:
    explicit OptNoValueBase(uint8_t aOptCode)
      : DhcpOption(aOptCode, false) {}

    OptNoValueBase(uint8_t aOptCode, bool fPresent)
      : DhcpOption(aOptCode, fPresent) {}

    OptNoValueBase(uint8_t aOptCode, const DhcpClientMessage &req)
      : DhcpOption(aOptCode, false)
    {
        decode(req);
    }

    virtual OptNoValueBase *clone() const
    {
        return new OptNoValueBase(*this);
    }

  protected:
    virtual ssize_t encodeValue(octets_t &dst) const
    {
        RT_NOREF(dst);
        return 0;
    }

  public:
    static bool isLengthValid(size_t cb)
    {
        return cb == 0;
    }

    virtual int decodeValue(const octets_t &src, size_t cb)
    {
        RT_NOREF(src);

        if (!isLengthValid(cb))
            return VERR_INVALID_PARAMETER;

        m_fPresent = true;
        return VINF_SUCCESS;
    }
};

template <uint8_t _OptCode>
class OptNoValue
  : public OptNoValueBase
{
  public:
    static const uint8_t optcode = _OptCode;

    OptNoValue()
      : OptNoValueBase(optcode) {}

    explicit OptNoValue(bool fPresent) /* there's no overloaded ctor with value */
      : OptNoValueBase(optcode, fPresent) {}

    explicit OptNoValue(const DhcpClientMessage &req)
      : OptNoValueBase(optcode, req) {}
};



/*
 * Option that contains single value of fixed-size type T
 */
template <typename T>
class OptValueBase
  : public DhcpOption
{
  public:
    typedef T value_t;

  protected:
    T m_Value;

    explicit OptValueBase(uint8_t aOptCode)
      : DhcpOption(aOptCode, false), m_Value() {}

    OptValueBase(uint8_t aOptCode, const T &aOptValue)
      : DhcpOption(aOptCode), m_Value(aOptValue) {}

    OptValueBase(uint8_t aOptCode, const DhcpClientMessage &req)
      : DhcpOption(aOptCode, false), m_Value()
    {
        decode(req);
    }

  public:
    virtual OptValueBase *clone() const
    {
        return new OptValueBase(*this);
    }

  public:
    T &value() { return m_Value; }
    const T &value() const { return m_Value; }

  protected:
    virtual ssize_t encodeValue(octets_t &dst) const
    {
        append(dst, m_Value);
        return sizeof(T);
    }

  public:
    static bool isLengthValid(size_t cb)
    {
        return cb == sizeof(T);
    }

    virtual int decodeValue(const octets_t &src, size_t cb)
    {
        if (!isLengthValid(cb))
            return VERR_INVALID_PARAMETER;

        octets_t::const_iterator pos(src.begin());
        extract(m_Value, pos);

        m_fPresent = true;
        return VINF_SUCCESS;
    }
};

template<uint8_t _OptCode, typename T>
class OptValue
  : public OptValueBase<T>
{
  public:
    using typename OptValueBase<T>::value_t;

  public:
    static const uint8_t optcode = _OptCode;

    OptValue()
      : OptValueBase<T>(optcode) {}

    explicit OptValue(const T &aOptValue)
      : OptValueBase<T>(optcode, aOptValue) {}

    explicit OptValue(const DhcpClientMessage &req)
      : OptValueBase<T>(optcode, req) {}

    static OptValue *parse(const char *pcszValue)
    {
	value_t v;
	int rc = DhcpOption::parse1(v, pcszValue);
	if (RT_FAILURE(rc))
	    return NULL;
	return new OptValue(v);
    }
};



/*
 * Option that contains a string.
 */
class OptStringBase
  : public DhcpOption
{
  public:
    typedef std::string value_t;

  protected:
    std::string m_String;

    explicit OptStringBase(uint8_t aOptCode)
      : DhcpOption(aOptCode, false), m_String() {}

    OptStringBase(uint8_t aOptCode, const std::string &aOptString)
      : DhcpOption(aOptCode), m_String(aOptString) {}

    OptStringBase(uint8_t aOptCode, const DhcpClientMessage &req)
      : DhcpOption(aOptCode, false), m_String()
    {
        decode(req);
    }

  public:
    virtual OptStringBase *clone() const
    {
        return new OptStringBase(*this);
    }

  public:
    std::string &value() { return m_String; }
    const std::string &value() const { return m_String; }

  protected:
    virtual ssize_t encodeValue(octets_t &dst) const
    {
        if (!isLengthValid(m_String.size()))
            return -1;

        append(dst, m_String);
        return m_String.size();
    }

  public:
    static bool isLengthValid(size_t cb)
    {
        return cb <= UINT8_MAX;
    }

    virtual int decodeValue(const octets_t &src, size_t cb)
    {
        if (!isLengthValid(cb))
            return VERR_INVALID_PARAMETER;

        octets_t::const_iterator pos(src.begin());
        extract(m_String, pos, cb);
        m_fPresent = true;
        return VINF_SUCCESS;
    }
};

template<uint8_t _OptCode>
class OptString
  : public OptStringBase
{
  public:
    static const uint8_t optcode = _OptCode;

    OptString()
      : OptStringBase(optcode) {}

    explicit OptString(const std::string &aOptString)
      : OptStringBase(optcode, aOptString) {}

    explicit OptString(const DhcpClientMessage &req)
      : OptStringBase(optcode, req) {}

    static OptString *parse(const char *pcszValue)
    {
	return new OptString(pcszValue);
    }
};



/*
 * Option that contains a list of values of type T
 */
template <typename T>
class OptListBase
  : public DhcpOption
{
  public:
    typedef std::vector<T> value_t;

  protected:
    std::vector<T> m_List;

    explicit OptListBase(uint8_t aOptCode)
      : DhcpOption(aOptCode, false), m_List() {}

    OptListBase(uint8_t aOptCode, const T &aOptSingle)
      : DhcpOption(aOptCode), m_List(1, aOptSingle) {}

    OptListBase(uint8_t aOptCode, const std::vector<T> &aOptList)
      : DhcpOption(aOptCode), m_List(aOptList) {}

    OptListBase(uint8_t aOptCode, const DhcpClientMessage &req)
      : DhcpOption(aOptCode, false), m_List()
    {
        decode(req);
    }

  public:
    virtual OptListBase *clone() const
    {
        return new OptListBase(*this);
    }

  public:
    std::vector<T> &value() { return m_List; }
    const std::vector<T> &value() const { return m_List; }

  protected:
    virtual ssize_t encodeValue(octets_t &dst) const
    {
        const size_t cbItem = sizeof(T);
        size_t cbValue = 0;

        for (size_t i = 0; i < m_List.size(); ++i)
        {
            if (cbValue + cbItem > UINT8_MAX)
                break;

            append(dst, m_List[i]);
            cbValue += cbItem;
        }

        return cbValue;
    }

  public:
    static bool isLengthValid(size_t cb)
    {
        return cb % sizeof(T) == 0;
    }

    virtual int decodeValue(const octets_t &src, size_t cb)
    {
        if (!isLengthValid(cb))
            return VERR_INVALID_PARAMETER;

        m_List.erase(m_List.begin(), m_List.end());

        octets_t::const_iterator pos(src.begin());
        for (size_t i = 0; i < cb / sizeof(T); ++i)
        {
            T item;
            extract(item, pos);
            m_List.push_back(item);
        }
        m_fPresent = true;
        return VINF_SUCCESS;
    }
};

template<uint8_t _OptCode, typename T>
class OptList
  : public OptListBase<T>

{
  public:
    using typename OptListBase<T>::value_t;

  public:
    static const uint8_t optcode = _OptCode;

    OptList()
      : OptListBase<T>(optcode) {}

    explicit OptList(const T &aOptSingle)
      : OptListBase<T>(optcode, aOptSingle) {}

    explicit OptList(const std::vector<T> &aOptList)
      : OptListBase<T>(optcode, aOptList) {}

    explicit OptList(const DhcpClientMessage &req)
      : OptListBase<T>(optcode, req) {}

    static OptList *parse(const char *pcszValue)
    {
	value_t v;
	int rc = DhcpOption::parseList(v, pcszValue);
	if (RT_FAILURE(rc) || v.empty())
	    return NULL;
	return new OptList(v);
    }
};


/*
 * Options specified by raw binary data that we don't know how to
 * interpret.
 */
class RawOption
  : public DhcpOption
{
  protected:
    octets_t m_Data;

  public:
    explicit RawOption(uint8_t aOptCode)
      : DhcpOption(aOptCode, false), m_Data() {}

    RawOption(uint8_t aOptCode, const octets_t &aSrc)
      : DhcpOption(aOptCode), m_Data(aSrc) {}

  public:
    virtual RawOption *clone() const
    {
        return new RawOption(*this);
    }


  protected:
    virtual ssize_t encodeValue(octets_t &dst) const
    {
        dst.insert(dst.end(), m_Data.begin(), m_Data.end());
        return m_Data.size();
    }

    virtual int decodeValue(const octets_t &src, size_t cb)
    {
        octets_t::const_iterator beg(src.begin());
	octets_t data(beg, beg + cb);
	m_Data.swap(data);

	m_fPresent = true;
	return VINF_SUCCESS;
    }

  public:
    static RawOption *parse(uint8_t aOptCode, const char *pcszValue)
    {
	octets_t data;
	int rc = DhcpOption::parseHex(data, pcszValue);
	if (RT_FAILURE(rc))
	    return NULL;
	return new RawOption(aOptCode, data);
    }
};



/*
 * Define the DHCP options we want to use.
 */
typedef OptValue<1, RTNETADDRIPV4>      OptSubnetMask;
typedef OptValue<2, uint32_t>           OptTimeOffset;
typedef OptList<3, RTNETADDRIPV4>       OptRouter;
typedef OptList<4, RTNETADDRIPV4>       OptTimeServer;
typedef OptList<6, RTNETADDRIPV4>       OptDNS;
typedef OptString<12>                   OptHostName;
typedef OptString<15>                   OptDomainName;
typedef OptString<17>                   OptRootPath;

/* DHCP related options */
typedef OptList<43, uint8_t>            OptVendorSpecificInfo;
typedef OptValue<50, RTNETADDRIPV4>     OptRequestedAddress;
typedef OptValue<51, uint32_t>          OptLeaseTime;
/* 52 - option overload is syntactic and handled internally */
typedef OptValue<53, uint8_t>           OptMessageType;
typedef OptValue<54, RTNETADDRIPV4>     OptServerId;
typedef OptList<55, uint8_t>            OptParameterRequest;
typedef OptString<56>                   OptMessage;
typedef OptValue<57, uint16_t>          OptMaxDHCPMessageSize;
typedef OptValue<58, uint32_t>          OptRenewalTime;
typedef OptValue<59, uint32_t>          OptRebindingTime;
typedef OptList<60, uint8_t>            OptVendorClassId;
typedef OptList<61, uint8_t>            OptClientId;
typedef OptString<66>                   OptTFTPServer;   /* when overloaded */
typedef OptString<67>                   OptBootFileName; /* when overloaded */
typedef OptNoValue<80>                  OptRapidCommit;  /* RFC4039 */

#endif /* _DHCP_OPTIONS_H_ */
