/* -*- indent-tabs-mode: nil; -*- */
#include <VBox/com/string.h>
#include <VBox/com/ptr.h>

#include "HostDnsService.h"

#ifdef RT_OS_OS2
# include <sys/socket.h>
typedef int socklen_t;
#endif

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>

#include <VBox/log.h>


static int fileGets(RTFILE File, void *pvBuf, size_t cbBufSize, size_t *pcbRead)
{
    size_t cbRead;
    char bTest;
    int rc = VERR_NO_MEMORY;
    char *pu8Buf = (char *)pvBuf;
    *pcbRead = 0;

    while (   RT_SUCCESS(rc = RTFileRead(File, &bTest, 1, &cbRead))
           && (pu8Buf - (char *)pvBuf) >= 0   
           && (size_t)(pu8Buf - (char *)pvBuf) < cbBufSize)
    {
        if (cbRead == 0)
            return VERR_EOF;

        if (bTest == '\r' || bTest == '\n')
        {
            *pu8Buf = 0;
            return VINF_SUCCESS;
        }
        *pu8Buf = bTest;
         pu8Buf++;
        (*pcbRead)++;
    }
    return rc;
}


HostDnsServiceResolvConf::HostDnsServiceResolvConf(const char* aResolvConfFilename)
{
    m_ResolvConfFilename = com::Utf8Str(aResolvConfFilename);
}


HostDnsServiceResolvConf::~HostDnsServiceResolvConf()
{
    m_ResolvConfFilename.setNull();
    RTFileClose(m_ResolvConfFile);
}

HRESULT HostDnsServiceResolvConf::init(const VirtualBox *aParent)
{
    HRESULT hrc;

    int rc = RTFileOpen(&m_ResolvConfFile, m_ResolvConfFilename.c_str(), 
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    AssertRCReturn(rc, E_FAIL);


    hrc = HostDnsService::init(aParent);
    AssertComRCReturn(hrc, hrc);



    hrc = update();
    AssertComRCReturn(hrc, hrc);

    return S_OK;
}


HRESULT HostDnsServiceResolvConf::update()
{
    char buff[256];
    char buff2[256];
    int cNameserversFound = 0;
    bool fWarnTooManyDnsServers = false;
    struct in_addr tmp_addr;
    int rc;
    size_t bytes;

    while (    RT_SUCCESS(rc = fileGets(m_ResolvConfFile, buff, sizeof(buff), &bytes))
            && rc != VERR_EOF)
    {
        if (   cNameserversFound == 4
            && !fWarnTooManyDnsServers
            && sscanf(buff, "nameserver%*[ \t]%255s", buff2) == 1)
        {
            fWarnTooManyDnsServers = true;
            LogRel(("NAT: too many nameservers registered.\n"));
        }
        if (   sscanf(buff, "nameserver%*[ \t]%255s", buff2) == 1
            && cNameserversFound < 4) /* Unix doesn't accept more than 4 name servers*/
        {
            if (!inet_aton(buff2, &tmp_addr))
                continue;

	    m_llNameServers.push_back(com::Utf8Str(buff2));

            cNameserversFound++;
        }
        if ((!strncmp(buff, "domain", 6) || !strncmp(buff, "search", 6)))
        {
            char *tok;
            char *saveptr;

            tok = strtok_r(&buff[6], " \t\n", &saveptr);
	    
	    if (tok != NULL)
		m_DomainName = com::Utf8Str(tok);
        }
    }

    return S_OK;

}
