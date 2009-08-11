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
#define CR_PROTOCOL_VERSION_MAJOR 2
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
