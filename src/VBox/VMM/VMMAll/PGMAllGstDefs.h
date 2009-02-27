/* $Id$ */
/** @file
 * VBox - Page Manager, Guest Paging Template - All context code.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#undef GSTPT
#undef PGSTPT
#undef GSTPTE
#undef PGSTPTE
#undef GSTPD
#undef PGSTPD
#undef GSTPDE
#undef PGSTPDE
#undef GST_BIG_PAGE_SIZE
#undef GST_BIG_PAGE_OFFSET_MASK
#undef GST_PDE_PG_MASK
#undef GST_PDE_BIG_PG_MASK
#undef GST_PD_SHIFT
#undef GST_PD_MASK
#undef GST_PTE_PG_MASK
#undef GST_PT_SHIFT
#undef GST_PT_MASK
#undef GST_TOTAL_PD_ENTRIES
#undef GST_CR3_PAGE_MASK
#undef GST_PDPE_ENTRIES
#undef GST_PDPT_SHIFT
#undef GST_PDPT_MASK
#undef GST_PDPE_PG_MASK
#undef GST_GET_PDE_BIG_PG_GCPHYS

#if PGM_GST_TYPE == PGM_TYPE_REAL \
 || PGM_GST_TYPE == PGM_TYPE_PROT
# define GSTPT                      SHWPT
# define PGSTPT                     PSHWPT
# define GSTPTE                     SHWPTE
# define PGSTPTE                    PSHWPTE
# define GSTPD                      SHWPD
# define PGSTPD                     PSHWPD
# define GSTPDE                     SHWPDE
# define PGSTPDE                    PSHWPDE
# define GST_PTE_PG_MASK            SHW_PTE_PG_MASK

#elif PGM_GST_TYPE == PGM_TYPE_32BIT
# define GSTPT                      X86PT
# define PGSTPT                     PX86PT
# define GSTPTE                     X86PTE
# define PGSTPTE                    PX86PTE
# define GSTPD                      X86PD
# define PGSTPD                     PX86PD
# define GSTPDE                     X86PDE
# define PGSTPDE                    PX86PDE
# define GST_BIG_PAGE_SIZE          X86_PAGE_4M_SIZE
# define GST_BIG_PAGE_OFFSET_MASK   X86_PAGE_4M_OFFSET_MASK
# define GST_PDE_PG_MASK            X86_PDE_PG_MASK
# define GST_PDE_BIG_PG_MASK        X86_PDE4M_PG_MASK
# define GST_GET_PDE_BIG_PG_GCPHYS(PdeGst)  pgmGstGet4MBPhysPage(&pVM->pgm.s, PdeGst)
# define GST_PD_SHIFT               X86_PD_SHIFT
# define GST_PD_MASK                X86_PD_MASK
# define GST_TOTAL_PD_ENTRIES       X86_PG_ENTRIES
# define GST_PTE_PG_MASK            X86_PTE_PG_MASK
# define GST_PT_SHIFT               X86_PT_SHIFT
# define GST_PT_MASK                X86_PT_MASK
# define GST_CR3_PAGE_MASK          X86_CR3_PAGE_MASK

#elif   PGM_GST_TYPE == PGM_TYPE_PAE \
     || PGM_GST_TYPE == PGM_TYPE_AMD64
# define GSTPT                      X86PTPAE
# define PGSTPT                     PX86PTPAE
# define GSTPTE                     X86PTEPAE
# define PGSTPTE                    PX86PTEPAE
# define GSTPD                      X86PDPAE
# define PGSTPD                     PX86PDPAE
# define GSTPDE                     X86PDEPAE
# define PGSTPDE                    PX86PDEPAE
# define GST_BIG_PAGE_SIZE          X86_PAGE_2M_SIZE
# define GST_BIG_PAGE_OFFSET_MASK   X86_PAGE_2M_OFFSET_MASK
# define GST_PDE_PG_MASK            X86_PDE_PAE_PG_MASK_FULL
# define GST_PDE_BIG_PG_MASK        X86_PDE2M_PAE_PG_MASK
# define GST_GET_PDE_BIG_PG_GCPHYS(PdeGst)  (PdeGst.u & GST_PDE_BIG_PG_MASK)
# define GST_PD_SHIFT               X86_PD_PAE_SHIFT
# define GST_PD_MASK                X86_PD_PAE_MASK
# if PGM_GST_TYPE == PGM_TYPE_PAE
#  define GST_TOTAL_PD_ENTRIES      (X86_PG_PAE_ENTRIES * X86_PG_PAE_PDPE_ENTRIES)
#  define GST_PDPE_ENTRIES          X86_PG_PAE_PDPE_ENTRIES
#  define GST_PDPE_PG_MASK          X86_PDPE_PG_MASK_FULL
#  define GST_PDPT_SHIFT            X86_PDPT_SHIFT
#  define GST_PDPT_MASK             X86_PDPT_MASK_PAE
#  define GST_PTE_PG_MASK           X86_PTE_PAE_PG_MASK
#  define GST_CR3_PAGE_MASK         X86_CR3_PAE_PAGE_MASK
# else
#  define GST_TOTAL_PD_ENTRIES      (X86_PG_AMD64_ENTRIES * X86_PG_AMD64_PDPE_ENTRIES)
#  define GST_PDPE_ENTRIES          X86_PG_AMD64_PDPE_ENTRIES
#  define GST_PDPT_SHIFT            X86_PDPT_SHIFT
#  define GST_PDPE_PG_MASK          X86_PDPE_PG_MASK_FULL
#  define GST_PDPT_MASK             X86_PDPT_MASK_AMD64
#  define GST_PTE_PG_MASK           X86_PTE_PAE_PG_MASK_FULL
#  define GST_CR3_PAGE_MASK         X86_CR3_AMD64_PAGE_MASK
# endif
# define GST_PT_SHIFT               X86_PT_PAE_SHIFT
# define GST_PT_MASK                X86_PT_PAE_MASK
#endif


