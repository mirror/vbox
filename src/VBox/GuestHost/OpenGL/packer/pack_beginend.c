/* Copyright (c) 2001, Stanford University
 * All rights reserved
 *
 * See the file LICENSE.txt for information on redistributing this software.
 */

#include "packer.h"
#include "cr_protocol.h"

void PACK_APIENTRY crPackBegin( GLenum mode )
{
    CR_GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    if (pc->buffer.canBarf)
    {
        if (!pc->buffer.holds_BeginEnd)
            pc->Flush( pc->flush_arg );
        pc->buffer.in_BeginEnd = 1;
        pc->buffer.holds_BeginEnd = 1;
    }
    CR_GET_BUFFERED_POINTER( pc, 4 );
    CRASSERT(pc->enmBeginEndState == CRPackBeginEndStateNone);
    pc->enmBeginEndState = CRPackBeginEndStateStarted;
    pc->current.begin_data = data_ptr;
    pc->current.begin_op = pc->buffer.opcode_current;
    pc->current.attribsUsedMask = 0;
    WRITE_DATA( 0, GLenum, mode );
    WRITE_OPCODE( pc, CR_BEGIN_OPCODE );
    CR_UNLOCK_PACKER_CONTEXT(pc);
}

void PACK_APIENTRY crPackBeginSWAP( GLenum mode )
{
    CR_GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    if (pc->buffer.canBarf)
    {
        if (!pc->buffer.holds_BeginEnd)
            pc->Flush( pc->flush_arg );
        pc->buffer.in_BeginEnd = 1;
        pc->buffer.holds_BeginEnd = 1;
    }
    CR_GET_BUFFERED_POINTER( pc, 4 );
    CRASSERT(pc->enmBeginEndState == CRPackBeginEndStateNone);
    pc->enmBeginEndState = CRPackBeginEndStateStarted;
    pc->current.begin_data = data_ptr;
    pc->current.begin_op = pc->buffer.opcode_current;
    pc->current.attribsUsedMask = 0;
    WRITE_DATA( 0, GLenum, SWAP32(mode) );
    WRITE_OPCODE( pc, CR_BEGIN_OPCODE );
    CR_UNLOCK_PACKER_CONTEXT(pc);
}

void PACK_APIENTRY crPackEnd( void )
{
    CR_GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    CR_GET_BUFFERED_POINTER_NO_ARGS( pc );
    WRITE_OPCODE( pc, CR_END_OPCODE );
    pc->buffer.in_BeginEnd = 0;
    CRASSERT(pc->enmBeginEndState == CRPackBeginEndStateStarted
            || pc->enmBeginEndState == CRPackBeginEndStateFlushDone);
    if (pc->enmBeginEndState == CRPackBeginEndStateFlushDone)
    {
        pc->Flush( pc->flush_arg );
    }
    pc->enmBeginEndState = CRPackBeginEndStateNone;
    CR_UNLOCK_PACKER_CONTEXT(pc);
}

void PACK_APIENTRY crPackEndSWAP( void )
{
    CR_GET_PACKER_CONTEXT(pc);
    unsigned char *data_ptr;
    (void) pc;
    CR_GET_BUFFERED_POINTER_NO_ARGS( pc );
    WRITE_OPCODE( pc, CR_END_OPCODE );
    pc->buffer.in_BeginEnd = 0;
    CRASSERT(pc->enmBeginEndState == CRPackBeginEndStateStarted
            || pc->enmBeginEndState == CRPackBeginEndStateFlushDone);
    if (pc->enmBeginEndState == CRPackBeginEndStateFlushDone)
    {
        pc->Flush( pc->flush_arg );
    }
    CR_UNLOCK_PACKER_CONTEXT(pc);
}

