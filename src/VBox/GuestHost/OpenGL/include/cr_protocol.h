/* Copyright (c) 2001, Stanford University
 * All rights reserved.
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#ifndef CR_PROTOCOL_H
#define CR_PROTOCOL_H

#include <iprt/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*For now guest is allowed to connect host opengl service if protocol version matches exactly*/
/*Note: that after any change to this file, or glapi_parser\apispec.txt version should be changed*/
#define CR_PROTOCOL_VERSION_MAJOR 9
#define CR_PROTOCOL_VERSION_MINOR 1

typedef enum {
    /* first message types is 'wGL\001', so we can immediately
         recognize bad message types */
    CR_MESSAGE_OPCODES = 0x77474c01,
    CR_MESSAGE_WRITEBACK,
    CR_MESSAGE_READBACK,
    CR_MESSAGE_READ_PIXELS,
    CR_MESSAGE_MULTI_BODY,
    CR_MESSAGE_MULTI_TAIL,
    CR_MESSAGE_FLOW_CONTROL,
    CR_MESSAGE_OOB,
    CR_MESSAGE_NEWCLIENT,
    CR_MESSAGE_GATHER,
    CR_MESSAGE_ERROR,
    CR_MESSAGE_CRUT,
    CR_MESSAGE_REDIR_PTR
} CRMessageType;

typedef union {
    /* pointers are usually 4 bytes, but on 64-bit machines (Alpha,
     * SGI-n64, etc.) they are 8 bytes.  Pass network pointers around
     * as an opaque array of bytes, since the interpretation & size of
     * the pointer only matter to the machine which emitted the
     * pointer (and will eventually receive the pointer back again) */
    unsigned int  ptrAlign[2];
    unsigned char ptrSize[8];
    /* hack to make this packet big */
    /* unsigned int  junk[512]; */
} CRNetworkPointer;

#ifdef DEBUG_misha
#define CRDBGPTR_SETZ(_p) crMemset((_p), 0, sizeof (CRNetworkPointer))
#define CRDBGPTR_CHECKZ(_p) do { \
        CRNetworkPointer _ptr = {0}; \
        Assert(!crMemcmp((_p), &_ptr, sizeof (CRNetworkPointer))); \
    } while (0)
#define CRDBGPTR_CHECKNZ(_p) do { \
        CRNetworkPointer _ptr = {0}; \
        Assert(crMemcmp((_p), &_ptr, sizeof (CRNetworkPointer))); \
    } while (0)
# if 0
#  define _CRDBGPTR_PRINT(_tStr, _id, _p) do { \
        crDebug(_tStr "%d:0x%08x%08x", (_id), (_p)->ptrAlign[1], (_p)->ptrAlign[0]); \
    } while (0)
# else
#  define _CRDBGPTR_PRINT(_tStr, _id, _p) do { } while (0)
# endif
#define CRDBGPTR_PRINTWB(_id, _p) _CRDBGPTR_PRINT("wbptr:", _id, _p)
#define CRDBGPTR_PRINTRB(_id, _p) _CRDBGPTR_PRINT("rbptr:", _id, _p)
#else
#define CRDBGPTR_SETZ(_p) do { } while (0)
#define CRDBGPTR_CHECKZ(_p) do { } while (0)
#define CRDBGPTR_CHECKNZ(_p) do { } while (0)
#define CRDBGPTR_PRINTWB(_id, _p) do { } while (0)
#define CRDBGPTR_PRINTRB(_id, _p) do { } while (0)
#endif

#ifdef VBOX_WITH_CRHGSMI
typedef struct CRVBOXHGSMI_CMDDATA {
    struct VBOXVDMACMD_CHROMIUM_CMD *pCmd;
    int          *pCmdRc;
    char         *pWriteback;
    unsigned int *pcbWriteback;
    unsigned int cbWriteback;
} CRVBOXHGSMI_CMDDATA, *PCRVBOXHGSMI_CMDDATA;

#ifdef DEBUG
# define CRVBOXHGSMI_CMDDATA_ASSERT_CONSISTENT(_pData)  do { \
        CRASSERT(!(_pData)->pCmd == !(_pData)->pCmdRc); \
        CRASSERT(!(_pData)->pWriteback == !(_pData)->pcbWriteback); \
        CRASSERT(!(_pData)->pWriteback == !(_pData)->cbWriteback); \
        if ((_pData)->pWriteback) \
        { \
            CRASSERT((_pData)->pCmd); \
        } \
    } while (0)

# define CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(_pData)  do { \
        CRASSERT(!(_pData)->pCmd); \
        CRASSERT(!(_pData)->pCmdRc); \
        CRASSERT(!(_pData)->pWriteback); \
        CRASSERT(!(_pData)->pcbWriteback); \
        CRASSERT(!(_pData)->cbWriteback); \
    } while (0)

# define CRVBOXHGSMI_CMDDATA_ASSERT_ISSET(_pData)  do { \
        CRVBOXHGSMI_CMDDATA_ASSERT_CONSISTENT(_pData); \
        CRASSERT(CRVBOXHGSMI_CMDDATA_IS_SET(_pData)); \
    } while (0)

# define CRVBOXHGSMI_CMDDATA_ASSERT_ISSETWB(_pData)  do { \
        CRVBOXHGSMI_CMDDATA_ASSERT_CONSISTENT(_pData); \
        CRASSERT(CRVBOXHGSMI_CMDDATA_IS_SETWB(_pData)); \
    } while (0)
#else
# define CRVBOXHGSMI_CMDDATA_ASSERT_CONSISTENT(_pData)  do { } while (0)
# define CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(_pData)  do { } while (0)
# define CRVBOXHGSMI_CMDDATA_ASSERT_ISSET(_pData)  do { } while (0)
# define CRVBOXHGSMI_CMDDATA_ASSERT_ISSETWB(_pData)  do { } while (0)
#endif

#define CRVBOXHGSMI_CMDDATA_IS_SET(_pData) (!!(_pData)->pCmd)
#define CRVBOXHGSMI_CMDDATA_IS_SETWB(_pData) (!!(_pData)->pWriteback)

#define CRVBOXHGSMI_CMDDATA_CLEANUP(_pData) do { \
        crMemset(_pData, 0, sizeof (CRVBOXHGSMI_CMDDATA)); \
        CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(_pData); \
        CRVBOXHGSMI_CMDDATA_ASSERT_CONSISTENT(_pData); \
    } while (0)

#define CRVBOXHGSMI_CMDDATA_SET(_pData, _pCmd, _pHdr) do { \
        CRVBOXHGSMI_CMDDATA_ASSERT_CLEANED(_pData); \
        (_pData)->pCmd = (_pCmd); \
        (_pData)->pCmdRc = &(_pHdr)->result; \
        CRVBOXHGSMI_CMDDATA_ASSERT_CONSISTENT(_pData); \
    } while (0)

#define CRVBOXHGSMI_CMDDATA_SETWB(_pData, _pCmd, _pHdr, _pWb, _cbWb, _pcbWb) do { \
        CRVBOXHGSMI_CMDDATA_SET(_pData, _pCmd, _pHdr); \
        (_pData)->pWriteback = (_pWb); \
        (_pData)->pcbWriteback = (_pcbWb); \
        (_pData)->cbWriteback = (_cbWb); \
        CRVBOXHGSMI_CMDDATA_ASSERT_CONSISTENT(_pData); \
    } while (0)

#define CRVBOXHGSMI_CMDDATA_RC(_pData, _rc) do { \
        *(_pData)->pCmdRc = (_rc); \
    } while (0)
#endif

typedef struct {
    CRMessageType          type;
    unsigned int           conn_id;
} CRMessageHeader;

typedef struct CRMessageOpcodes {
    CRMessageHeader        header;
    unsigned int           numOpcodes;
} CRMessageOpcodes;

typedef struct CRMessageRedirPtr {
    CRMessageHeader        header;
    CRMessageHeader*       pMessage;
#ifdef VBOX_WITH_CRHGSMI
    CRVBOXHGSMI_CMDDATA   CmdData;
#endif
} CRMessageRedirPtr;

typedef struct CRMessageWriteback {
    CRMessageHeader        header;
    CRNetworkPointer       writeback_ptr;
} CRMessageWriteback;

typedef struct CRMessageReadback {
    CRMessageHeader        header;
    CRNetworkPointer       writeback_ptr;
    CRNetworkPointer       readback_ptr;
} CRMessageReadback;

typedef struct CRMessageMulti {
    CRMessageHeader        header;
} CRMessageMulti;

typedef struct CRMessageFlowControl {
    CRMessageHeader        header;
    unsigned int           credits;
} CRMessageFlowControl;

typedef struct CRMessageReadPixels {
    CRMessageHeader        header;
    int                    width, height;
    unsigned int           bytes_per_row;
    unsigned int           stride;
    int                    alignment;
    int                    skipRows;
    int                    skipPixels;
    int                    rowLength;
    int                    format;
    int                    type;
    CRNetworkPointer       pixels;
} CRMessageReadPixels;

typedef struct CRMessageNewClient {
    CRMessageHeader        header;
} CRMessageNewClient;

typedef struct CRMessageGather {
    CRMessageHeader        header;
    unsigned long           offset;
    unsigned long           len;
} CRMessageGather;

typedef union {
    CRMessageHeader      header;
    CRMessageOpcodes     opcodes;
    CRMessageRedirPtr    redirptr;
    CRMessageWriteback   writeback;
    CRMessageReadback    readback;
    CRMessageReadPixels  readPixels;
    CRMessageMulti       multi;
    CRMessageFlowControl flowControl;
    CRMessageNewClient   newclient;
    CRMessageGather      gather;
} CRMessage;

DECLEXPORT(void) crNetworkPointerWrite( CRNetworkPointer *dst, void *src );

#ifdef __cplusplus
}
#endif

#endif /* CR_PROTOCOL_H */
