/************************************************* -*- linux-c -*-
 * Myricom 10Gb Network Interface Card Software
 * Copyright 2006, Myricom, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ****************************************************************/

#ifndef _mcp_gen_header_h
#define _mcp_gen_header_h

/* this file define a standard header used as a first entry point to
   exchange information between firmware/driver and driver.  The
   header structure can be anywhere in the mcp. It will usually be in
   the .data section, because some fields needs to be initialized at
   compile time.
   The 32bit word at offset MX_HEADER_PTR_OFFSET in the mcp must
   contains the location of the header. 

   Typically a MCP will start with the following:
   .text
     .space 52    ! to help catch MEMORY_INT errors
     bt start     ! jump to real code
     nop
     .long _gen_mcp_header
   
   The source will have a definition like:

   mcp_gen_header_t gen_mcp_header = {
      .header_length = sizeof(mcp_gen_header_t),
      .mcp_type = MCP_TYPE_XXX,
      .version = "something $Id: mcp_gen_header.h,v 1.1 2006/03/25 02:53:47 glenn_brown Exp $",
      .mcp_globals = (unsigned)&Globals
   };
*/


#define MCP_HEADER_PTR_OFFSET  0x3c

#define MCP_TYPE_MX 0x4d582020 /* "MX  " */
#define MCP_TYPE_PCIE 0x70636965 /* "PCIE" pcie-only MCP */
#define MCP_TYPE_ETH 0x45544820 /* "ETH " */
#define MCP_TYPE_MCP0 0x4d435030 /* "MCP0" */


typedef struct mcp_gen_header {
  /* the first 4 fields are filled at compile time */
  unsigned header_length;
  unsigned mcp_type;
  char version[128];
  unsigned mcp_globals; /* pointer to mcp-type specific structure */

  /* filled by the MCP at run-time */
  unsigned sram_size;
  unsigned string_specs;  /* either the original STRING_SPECS or a superset */
  unsigned string_specs_len;

  /* Fields above this comment are guaranteed to be present.

     Fields below this comment are extensions added in later versions
     of this struct, drivers should compare the header_length against
     offsetof(field) to check wether a given MCP implements them.

     Never remove any field.  Keep everything naturally align.
  */
} mcp_gen_header_t;

/* Macro to create a simple mcp header */
#define MCP_GEN_HEADER_DECL(type, version_str, global_ptr)	\
  struct mcp_gen_header mcp_gen_header = {			\
    sizeof (struct mcp_gen_header),				\
    (type),							\
    version_str,						\
    (global_ptr),						\
    SRAM_SIZE,							\
    (unsigned int) STRING_SPECS,				\
    256								\
  }


#endif /* _mcp_gen_header_h */
