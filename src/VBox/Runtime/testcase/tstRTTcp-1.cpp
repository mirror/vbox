#include <iprt/tcp.h>

#include <iprt/string.h>
#include <iprt/test.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTTEST g_hTest;


/* * * * * * * *   Test 3    * * * * * * * */

static DECLCALLBACK(int) test3Server(RTSOCKET hSocket, void *pvUser)
{
    RTTestSetDefault(g_hTest, NULL);
    char szBuf[4096];

    /* say hello */
    RTTESTI_CHECK_RC_RET(RTTcpWrite(hSocket, "hello\n", sizeof("hello\n") - 1), VINF_SUCCESS, VERR_TCP_SERVER_STOP);
    RT_ZERO(szBuf);
    RTTESTI_CHECK_RC_RET(RTTcpRead(hSocket, szBuf, sizeof("dude!\n") - 1, NULL), VINF_SUCCESS, VERR_TCP_SERVER_STOP);
    szBuf[sizeof("dude!\n") - 1] = '\0';
    RTTESTI_CHECK_RET(strcmp(szBuf, "dude!\n") == 0, VERR_TCP_SERVER_STOP);

    /* Send ~20 MB of data that the client receives while trying to disconnect. */
    RT_ZERO(szBuf);
    size_t cbSent = 0;
    while (cbSent < 20 * _1M)
    {
        RTTESTI_CHECK_RC_RET(RTTcpWrite(hSocket, szBuf, sizeof(szBuf)), VINF_SUCCESS, VERR_TCP_SERVER_STOP);
        cbSent += sizeof(szBuf);
    }

    return VERR_TCP_SERVER_STOP;
}


void test3()
{
    RTTestSub(g_hTest, "Graceful disconnect");

    uint32_t cStartErrors = RTTestErrorCount(g_hTest);
    for (unsigned i = 0; i < 100 && cStartErrors == RTTestErrorCount(g_hTest); i++)
    {
        PRTTCPSERVER pServer;
        int rc = RTTcpServerCreate("localhost", 9999, RTTHREADTYPE_DEFAULT, "server-2", test3Server, NULL, &pServer);
#ifdef RT_OS_SOLARIS
        /** @todo testboxsh1 occationally hits this for some stupid reason. i=21 in
         *        one occurrence. Fudge a bit for now and see if it helps. */
        if (rc == VERR_NET_ADDRESS_IN_USE)
        {
            RTThreadSleep(500);
            rc = RTTcpServerCreate("localhost", 9999, RTTHREADTYPE_DEFAULT, "server-2", test3Server, NULL, &pServer);
        }
#endif
        if (rc != VINF_SUCCESS)
        {
            RTTestIFailed("RTTcpServerCreate -> %Rrc, i=%d", rc, i);
            return;
        }

        RTSOCKET hSocket;
        RTTESTI_CHECK_RC(rc = RTTcpClientConnect("localhost", 9999, &hSocket), VINF_SUCCESS);
        if (RT_SUCCESS(rc))
        {
            char szBuf[512];
            RT_ZERO(szBuf);
            do /* break non-loop */
            {
                RTTESTI_CHECK_RC_BREAK(RTTcpRead(hSocket, szBuf, sizeof("hello\n") - 1, NULL), VINF_SUCCESS);
                RTTESTI_CHECK_BREAK(strcmp(szBuf, "hello\n") == 0);
                RTTESTI_CHECK_RC_BREAK(RTTcpWrite(hSocket, "dude!\n", sizeof("dude!\n") - 1), VINF_SUCCESS);
            } while (0);

            RTTESTI_CHECK_RC(RTTcpClientClose(hSocket), VINF_SUCCESS);
        }

        RTTESTI_CHECK_RC(RTTcpServerDestroy(pServer), VINF_SUCCESS);
    }
}


/* * * * * * * *   Test 2    * * * * * * * */

static DECLCALLBACK(int) test2Server(RTSOCKET hSocket, void *pvUser)
{
    RTTestSetDefault(g_hTest, NULL);
    char szBuf[512];

    /* say hello */
    RTTESTI_CHECK_RC_RET(RTTcpWrite(hSocket, "hello\n", sizeof("hello\n") - 1), VINF_SUCCESS, VERR_TCP_SERVER_STOP);
    RT_ZERO(szBuf);
    RTTESTI_CHECK_RC_RET(RTTcpRead(hSocket, szBuf, sizeof("dude!\n") - 1, NULL), VINF_SUCCESS, VERR_TCP_SERVER_STOP);
    szBuf[sizeof("dude!\n") - 1] = '\0';
    RTTESTI_CHECK_RET(strcmp(szBuf, "dude!\n") == 0, VERR_TCP_SERVER_STOP);

    /* wait for a goodbye which doesn't arrive. */
    RT_ZERO(szBuf);
    RTTESTI_CHECK_RC_RET(RTTcpRead(hSocket, szBuf, sizeof("byebye\n") - 1, NULL), VERR_NET_SHUTDOWN, VERR_TCP_SERVER_STOP);

    return VERR_TCP_SERVER_STOP;
}


void test2()
{
    RTTestSub(g_hTest, "Rude client");

    PRTTCPSERVER pServer;
    RTTESTI_CHECK_RC_RETV(RTTcpServerCreate("localhost", 9999, RTTHREADTYPE_DEFAULT, "server-2",
                                            test2Server, NULL, &pServer), VINF_SUCCESS);

    int rc;
    RTSOCKET hSocket;
    RTTESTI_CHECK_RC(rc = RTTcpClientConnect("localhost", 9999, &hSocket), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        char szBuf[512];
        RT_ZERO(szBuf);
        do /* break non-loop */
        {
            RTTESTI_CHECK_RC_BREAK(RTTcpRead(hSocket, szBuf, sizeof("hello\n") - 1, NULL), VINF_SUCCESS);
            RTTESTI_CHECK_BREAK(strcmp(szBuf, "hello\n") == 0);
            RTTESTI_CHECK_RC_BREAK(RTTcpWrite(hSocket, "dude!\n", sizeof("dude!\n") - 1), VINF_SUCCESS);
        } while (0);

        RTTESTI_CHECK_RC(RTTcpClientClose(hSocket), VINF_SUCCESS);
    }

    RTTESTI_CHECK_RC(RTTcpServerDestroy(pServer), VINF_SUCCESS);
}


/* * * * * * * *   Test 1    * * * * * * * */

static DECLCALLBACK(int) test1Server(RTSOCKET hSocket, void *pvUser)
{
    RTTestSetDefault(g_hTest, NULL);

    char szBuf[512];
    RTTESTI_CHECK_RET(pvUser == NULL, VERR_TCP_SERVER_STOP);

    /* say hello */
    RTTESTI_CHECK_RC_RET(RTTcpWrite(hSocket, "hello\n", sizeof("hello\n") - 1), VINF_SUCCESS, VERR_TCP_SERVER_STOP);
    RTTESTI_CHECK_RC_RET(RTTcpRead(hSocket, szBuf, sizeof("dude!\n") - 1, NULL), VINF_SUCCESS, VERR_TCP_SERVER_STOP);
    szBuf[sizeof("dude!\n") - 1] = '\0';
    RTTESTI_CHECK_RET(strcmp(szBuf, "dude!\n") == 0, VERR_TCP_SERVER_STOP);

    /* say goodbye */
    RTTESTI_CHECK_RC_RET(RTTcpRead(hSocket, szBuf, sizeof("byebye\n") - 1, NULL), VINF_SUCCESS, VERR_TCP_SERVER_STOP);
    szBuf[sizeof("byebye\n") - 1] = '\0';
    RTTESTI_CHECK_RET(strcmp(szBuf, "byebye\n") == 0, VERR_TCP_SERVER_STOP);
    RTTESTI_CHECK_RC_RET(RTTcpWrite(hSocket, "bye\n", sizeof("bye\n") - 1), VINF_SUCCESS, VERR_TCP_SERVER_STOP);

    return VERR_TCP_SERVER_STOP;
}


void test1()
{
    RTTestSub(g_hTest, "Simple server-client setup");

    PRTTCPSERVER pServer;
    RTTESTI_CHECK_RC_RETV(RTTcpServerCreate("localhost", 9999, RTTHREADTYPE_DEFAULT, "server-1",
                                            test1Server, NULL, &pServer), VINF_SUCCESS);

    int rc;
    RTSOCKET hSocket;
    RTTESTI_CHECK_RC(rc = RTTcpClientConnect("localhost", 9999, &hSocket), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        do /* break non-loop */
        {
            char szBuf[512];
            RT_ZERO(szBuf);
            RTTESTI_CHECK_RC_BREAK(RTTcpRead(hSocket, szBuf, sizeof("hello\n") - 1, NULL), VINF_SUCCESS);
            RTTESTI_CHECK_BREAK(strcmp(szBuf, "hello\n") == 0);
            RTTESTI_CHECK_RC_BREAK(RTTcpWrite(hSocket, "dude!\n", sizeof("dude!\n") - 1), VINF_SUCCESS);

            RTTESTI_CHECK_RC_BREAK(RTTcpWrite(hSocket, "byebye\n", sizeof("byebye\n") - 1), VINF_SUCCESS);
            RT_ZERO(szBuf);
            RTTESTI_CHECK_RC_BREAK(RTTcpRead(hSocket, szBuf, sizeof("bye\n") - 1, NULL), VINF_SUCCESS);
            RTTESTI_CHECK_BREAK(strcmp(szBuf, "bye\n") == 0);
        } while (0);

        RTTESTI_CHECK_RC(RTTcpClientClose(hSocket), VINF_SUCCESS);
    }

    RTTESTI_CHECK_RC(RTTcpServerDestroy(pServer), VINF_SUCCESS);
}


int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTTcp-1", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(g_hTest);

    test1();
    test2();
    test3();

    /** @todo test the full RTTcp API. */

    return RTTestSummaryAndDestroy(g_hTest);
}

