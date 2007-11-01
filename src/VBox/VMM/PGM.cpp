/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor. (Mixing stuff here, not good?)
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_pgm PGM - The Page Manager and Monitor
 *
 *
 *
 * @section         sec_pgm_modes           Paging Modes
 *
 * There are three memory contexts: Host Context (HC), Guest Context (GC)
 * and intermediate context. When talking about paging HC can also be refered to
 * as "host paging", and GC refered to as "shadow paging".
 *
 * We define three basic paging modes: 32-bit, PAE and AMD64. The host paging mode
 * is defined by the host operating system. The mode used in the shadow paging mode
 * depends on the host paging mode and what the mode the guest is currently in. The
 * following relation between the two is defined:
 *
 * @verbatim
     Host > 32-bit |  PAE   | AMD64  |
   Guest  |        |        |        |
   ==v================================
   32-bit   32-bit    PAE     PAE
   -------|--------|--------|--------|
   PAE       PAE      PAE     PAE
   -------|--------|--------|--------|
   AMD64    AMD64    AMD64    AMD64
   -------|--------|--------|--------| @endverbatim
 *
 * All configuration except those in the diagonal (upper left) are expected to
 * require special effort from the switcher (i.e. a bit slower).
 *
 *
 *
 *
 * @section         sec_pgm_shw             The Shadow Memory Context
 *
 *
 *  [..]
 *
 * Because of guest context mappings requires PDPTR and PML4 entries to allow
 * writing on AMD64, the two upper levels will have fixed flags whatever the
 * guest is thinking of using there. So, when shadowing the PD level we will
 * calculate the effective flags of PD and all the higher levels. In legacy
 * PAE mode this only applies to the PWT and PCD bits (the rest are
 * ignored/reserved/MBZ). We will ignore those bits for the present.
 *
 *
 *
 * @section         sec_pgm_int             The Intermediate Memory Context
 *
 * The world switch goes thru an intermediate memory context which purpose it is
 * to provide different mappings of the switcher code. All guest mappings are also
 * present in this context.
 *
 * The switcher code is mapped at the same location as on the host, at an
 * identity mapped location (physical equals virtual address), and at the
 * hypervisor location.
 *
 * PGM maintain page tables for 32-bit, PAE and AMD64 paging modes. This
 * simplifies switching guest CPU mode and consistency at the cost of more
 * code to do the work. All memory use for those page tables is located below
 * 4GB (this includes page tables for guest context mappings).
 *
 *
 * @subsection      subsec_pgm_int_gc       Guest Context Mappings
 *
 * During assignment and relocation of a guest context mapping the intermediate
 * memory context is used to verify the new location.
 *
 * Guest context mappings are currently restricted to below 4GB, for reasons
 * of simplicity. This may change when we implement AMD64 support.
 *
 *
 *
 *
 * @section         sec_pgm_misc            Misc
 *
 * @subsection      subsec_pgm_misc_diff    Differences Between Legacy PAE and Long Mode PAE
 *
 * The differences between legacy PAE and long mode PAE are:
 *      -# PDPE bits 1, 2, 5 and 6 are defined differently. In leagcy mode they are
 *         all marked down as must-be-zero, while in long mode 1, 2 and 5 have the
 *         usual meanings while 6 is ignored (AMD). This means that upon switching to
 *         legacy PAE mode we'll have to clear these bits and when going to long mode
 *         they must be set. This applies to both intermediate and shadow contexts,
 *         however we don't need to do it for the intermediate one since we're
 *         executing with CR0.WP at that time.
 *      -# CR3 allows a 32-byte aligned address in legacy mode, while in long mode
 *         a page aligned one is required.
 */


/** @page pg_pgmPhys PGMPhys - Physical Guest Memory Management.
 *
 *
 * Objectives:
 *      - Guest RAM over-commitment using memory ballooning,
 *        zero pages and general page sharing.
 *      - Moving or mirroring a VM onto a different physical machine.
 *
 *
 * @subsection subsec_pgmPhys_Definitions       Definitions
 *
 * Allocation chunk - A RTR0MemObjAllocPhysNC object and the tracking
 * machinery assoicated with it.
 *
 *
 *
 *
 * @subsection subsec_pgmPhys_AllocPage         Allocating a page.
 *
 * Initially we map *all* guest memory to the (per VM) zero page, which
 * means that none of the read functions will cause pages to be allocated.
 *
 * Exception, access bit in page tables that have been shared. This must
 * be handled, but we must also make sure PGMGst*Modify doesn't make
 * unnecessary modifications.
 *
 * Allocation points:
 *      - PGMPhysWriteGCPhys and PGMPhysWrite.
 *      - Replacing a zero page mapping at \#PF.
 *      - Replacing a shared page mapping at \#PF.
 *      - ROM registration (currently MMR3RomRegister).
 *      - VM restore (pgmR3Load).
 *
 * For the first three it would make sense to keep a few pages handy
 * until we've reached the max memory commitment for the VM.
 *
 * For the ROM registration, we know exactly how many pages we need
 * and will request these from ring-0. For restore, we will save
 * the number of non-zero pages in the saved state and allocate
 * them up front. This would allow the ring-0 component to refuse
 * the request if the isn't sufficient memory available for VM use.
 *
 * Btw. for both ROM and restore allocations we won't be requiring
 * zeroed pages as they are going to be filled instantly.
 *
 *
 * @subsection subsec_pgmPhys_FreePage          Freeing a page
 *
 * There are a few points where a page can be freed:
 *      - After being replaced by the zero page.
 *      - After being replaced by a shared page.
 *      - After being ballooned by the guest additions.
 *      - At reset.
 *      - At restore.
 *
 * When freeing one or more pages they will be returned to the ring-0
 * component and replaced by the zero page.
 *
 * The reasoning for clearing out all the pages on reset is that it will
 * return us to the exact same state as on power on, and may thereby help
 * us reduce the memory load on the system. Further it might have a
 * (temporary) positive influence on memory fragmentation (@see subsec_pgmPhys_Fragmentation).
 *
 * On restore, as mention under the allocation topic, pages should be
 * freed / allocated depending on how many is actually required by the
 * new VM state. The simplest approach is to do like on reset, and free
 * all non-ROM pages and then allocate what we need.
 *
 * A measure to prevent some fragmentation, would be to let each allocation
 * chunk have some affinity towards the VM having allocated the most pages
 * from it. Also, try make sure to allocate from allocation chunks that
 * are almost full. Admittedly, both these measures might work counter to
 * our intentions and its probably not worth putting a lot of effort,
 * cpu time or memory into this.
 *
 *
 * @subsection subsec_pgmPhys_SharePage         Sharing a page
 *
 * The basic idea is that there there will be a idle priority kernel
 * thread walking the non-shared VM pages hashing them and looking for
 * pages with the same checksum. If such pages are found, it will compare
 * them byte-by-byte to see if they actually are identical. If found to be
 * identical it will allocate a shared page, copy the content, check that
 * the page didn't change while doing this, and finally request both the
 * VMs to use the shared page instead. If the page is all zeros (special
 * checksum and byte-by-byte check) it will request the VM that owns it
 * to replace it with the zero page.
 *
 * To make this efficient, we will have to make sure not to try share a page
 * that will change its contents soon. This part requires the most work.
 * A simple idea would be to request the VM to write monitor the page for
 * a while to make sure it isn't modified any time soon. Also, it may
 * make sense to skip pages that are being write monitored since this
 * information is readily available to the thread if it works on the
 * per-VM guest memory structures (presently called PGMRAMRANGE).
 *
 *
 * @subsection subsec_pgmPhys_Fragmentation     Fragmentation Concerns and Counter Measures
 *
 * The pages are organized in allocation chunks in ring-0, this is a necessity
 * if we wish to have an OS agnostic approach to this whole thing. (On Linux we
 * could easily work on a page-by-page basis if we liked. Whether this is possible
 * or efficient on NT I don't quite know.) Fragmentation within these chunks may
 * become a problem as part of the idea here is that we wish to return memory to
 * the host system.
 *
 * For instance, starting two VMs at the same time, they will both allocate the
 * guest memory on-demand and if permitted their page allocations will be
 * intermixed. Shut down one of the two VMs and it will be difficult to return
 * any memory to the host system because the page allocation for the two VMs are
 * mixed up in the same allocation chunks.
 *
 * To further complicate matters, when pages are freed because they have been
 * ballooned or become shared/zero the whole idea is that the page is supposed
 * to be reused by another VM or returned to the host system. This will cause
 * allocation chunks to contain pages belonging to different VMs and prevent
 * returning memory to the host when one of those VM shuts down.
 *
 * The only way to really deal with this problem is to move pages. This can
 * either be done at VM shutdown and or by the idle priority worker thread
 * that will be responsible for finding sharable/zero pages. The mechanisms
 * involved for coercing a VM to move a page (or to do it for it) will be
 * the same as when telling it to share/zero a page.
 *
 *
 * @subsection subsec_pgmPhys_Tracking      Tracking Structures And Their Cost
 *
 * There's a difficult balance between keeping the per-page tracking structures
 * (global and guest page) easy to use and keeping them from eating too much
 * memory. We have limited virtual memory resources available when operating in
 * 32-bit kernel space (on 64-bit there'll it's quite a different story). The
 * tracking structures will be attemted designed such that we can deal with up
 * to 32GB of memory on a 32-bit system and essentially unlimited on 64-bit ones.
 *
 *
 * @subsubsection subsubsec_pgmPhys_Tracking_Kernel     Kernel Space
 *
 * @see pg_GMM
 *
 * @subsubsection subsubsec_pgmPhys_Tracking_PerVM      Per-VM
 *
 * Fixed info is the physical address of the page (HCPhys) and the page id
 * (described above). Theoretically we'll need 48(-12) bits for the HCPhys part.
 * Today we've restricting ourselves to 40(-12) bits because this is the current
 * restrictions of all AMD64 implementations (I think Barcelona will up this
 * to 48(-12) bits, not that it really matters) and I needed the bits for
 * tracking mappings of a page. 48-12 = 36. That leaves 28 bits, which means a
 * decent range for the page id: 2^(28+12) = 1024TB.
 *
 * In additions to these, we'll have to keep maintaining the page flags as we
 * currently do. Although it wouldn't harm to optimize these quite a bit, like
 * for instance the ROM shouldn't depend on having a write handler installed
 * in order for it to become read-only. A RO/RW bit should be considered so
 * that the page syncing code doesn't have to mess about checking multiple
 * flag combinations (ROM || RW handler || write monitored) in order to
 * figure out how to setup a shadow PTE. But this of course, is second
 * priority at present. Current this requires 12 bits, but could probably
 * be optimized to ~8.
 *
 * Then there's the 24 bits used to track which shadow page tables are
 * currently mapping a page for the purpose of speeding up physical
 * access handlers, and thereby the page pool cache. More bit for this
 * purpose wouldn't hurt IIRC.
 *
 * Then there is a new bit in which we need to record what kind of page
 * this is, shared, zero, normal or write-monitored-normal. This'll
 * require 2 bits. One bit might be needed for indicating whether a
 * write monitored page has been written to. And yet another one or
 * two for tracking migration status. 3-4 bits total then.
 *
 * Whatever is left will can be used to record the sharabilitiy of a
 * page. The page checksum will not be stored in the per-VM table as
 * the idle thread will not be permitted to do modifications to it.
 * It will instead have to keep its own working set of potentially
 * shareable pages and their check sums and stuff.
 *
 * For the present we'll keep the current packing of the
 * PGMRAMRANGE::aHCPhys to keep the changes simple, only of course,
 * we'll have to change it to a struct with a total of 128-bits at
 * our disposal.
 *
 * The initial layout will be like this:
 * @verbatim
    RTHCPHYS HCPhys;            The current stuff.
        63:40                   Current shadow PT tracking stuff.
        39:12                   The physical page frame number.
        11:0                    The current flags.
    uint32_t u28PageId : 28;    The page id.
    uint32_t u2State : 2;       The page state { zero, shared, normal, write monitored }.
    uint32_t fWrittenTo : 1;    Whether a write monitored page was written to.
    uint32_t u1Reserved : 1;    Reserved for later.
    uint32_t u32Reserved;       Reserved for later, mostly sharing stats.
 @endverbatim
 *
 * The final layout will be something like this:
 * @verbatim
    RTHCPHYS HCPhys;            The current stuff.
        63:48                   High page id (12+).
        47:12                   The physical page frame number.
        11:0                    Low page id.
    uint32_t fReadOnly : 1;     Whether it's readonly page (rom or monitored in some way).
    uint32_t u3Type : 3;        The page type {RESERVED, MMIO, MMIO2, ROM, shadowed ROM, RAM}.
    uint32_t u2PhysMon : 2;     Physical access handler type {none, read, write, all}.
    uint32_t u2VirtMon : 2;     Virtual access handler type {none, read, write, all}..
    uint32_t u2State : 2;       The page state { zero, shared, normal, write monitored }.
    uint32_t fWrittenTo : 1;    Whether a write monitored page was written to.
    uint32_t u20Reserved : 20;  Reserved for later, mostly sharing stats.
    uint32_t u32Tracking;       The shadow PT tracking stuff, roughly.
 @endverbatim
 *
 * Cost wise, this means we'll double the cost for guest memory. There isn't anyway
 * around that I'm afraid. It means that the cost of dealing out 32GB of memory
 * to one or more VMs is: (32GB >> PAGE_SHIFT) * 16 bytes, or 128MBs. Or another
 * example, the VM heap cost when assigning 1GB to a VM will be: 4MB.
 *
 * A couple of cost examples for the total cost per-VM + kernel.
 * 32-bit Windows and 32-bit linux:
 *      1GB guest ram, 256K pages:  4MB +  2MB(+) =   6MB
 *      4GB guest ram, 1M pages:   16MB +  8MB(+) =  24MB
 *     32GB guest ram, 8M pages:  128MB + 64MB(+) = 192MB
 * 64-bit Windows and 64-bit linux:
 *      1GB guest ram, 256K pages:  4MB +  3MB(+) =   7MB
 *      4GB guest ram, 1M pages:   16MB + 12MB(+) =  28MB
 *     32GB guest ram, 8M pages:  128MB + 96MB(+) = 224MB
 *
 * UPDATE - 2007-09-27:
 * Will need a ballooned flag/state too because we cannot
 * trust the guest 100% and reporting the same page as ballooned more
 * than once will put the GMM off balance.
 *
 *
 * @subsection subsec_pgmPhys_Serializing       Serializing Access
 *
 * Initially, we'll try a simple scheme:
 *
 *      - The per-VM RAM tracking structures (PGMRAMRANGE) is only modified
 *        by the EMT thread of that VM while in the pgm critsect.
 *      - Other threads in the VM process that needs to make reliable use of
 *        the per-VM RAM tracking structures will enter the critsect.
 *      - No process external thread or kernel thread will ever try enter
 *        the pgm critical section, as that just won't work.
 *      - The idle thread (and similar threads) doesn't not need 100% reliable
 *        data when performing it tasks as the EMT thread will be the one to
 *        do the actual changes later anyway. So, as long as it only accesses
 *        the main ram range, it can do so by somehow preventing the VM from
 *        being destroyed while it works on it...
 *
 *      - The over-commitment management, including the allocating/freeing
 *        chunks, is serialized by a ring-0 mutex lock (a fast one since the
 *        more mundane mutex implementation is broken on Linux).
 *      - A separeate mutex is protecting the set of allocation chunks so
 *        that pages can be shared or/and freed up while some other VM is
 *        allocating more chunks. This mutex can be take from under the other
 *        one, but not the otherway around.
 *
 *
 * @subsection subsec_pgmPhys_Request           VM Request interface
 *
 * When in ring-0 it will become necessary to send requests to a VM so it can
 * for instance move a page while defragmenting during VM destroy. The idle
 * thread will make use of this interface to request VMs to setup shared
 * pages and to perform write monitoring of pages.
 *
 * I would propose an interface similar to the current VMReq interface, similar
 * in that it doesn't require locking and that the one sending the request may
 * wait for completion if it wishes to. This shouldn't be very difficult to
 * realize.
 *
 * The requests themselves are also pretty simple. They are basically:
 *      -# Check that some precondition is still true.
 *      -# Do the update.
 *      -# Update all shadow page tables involved with the page.
 *
 * The 3rd step is identical to what we're already doing when updating a
 * physical handler, see pgmHandlerPhysicalSetRamFlagsAndFlushShadowPTs.
 *
 *
 *
 * @section sec_pgmPhys_MappingCaches   Mapping Caches
 *
 * In order to be able to map in and out memory and to be able to support
 * guest with more RAM than we've got virtual address space, we'll employing
 * a mapping cache. There is already a tiny one for GC (see PGMGCDynMapGCPageEx)
 * and we'll create a similar one for ring-0 unless we decide to setup a dedicate
 * memory context for the HWACCM execution.
 *
 *
 * @subsection subsec_pgmPhys_MappingCaches_R3  Ring-3
 *
 * We've considered implementing the ring-3 mapping cache page based but found
 * that this was bother some when one had to take into account TLBs+SMP and
 * portability (missing the necessary APIs on several platforms). There were
 * also some performance concerns with this approach which hadn't quite been
 * worked out.
 *
 * Instead, we'll be mapping allocation chunks into the VM process. This simplifies
 * matters greatly quite a bit since we don't need to invent any new ring-0 stuff,
 * only some minor RTR0MEMOBJ mapping stuff. The main concern here is that mapping
 * compared to the previous idea is that mapping or unmapping a 1MB chunk is more
 * costly than a single page, although how much more costly is uncertain. We'll
 * try address this by using a very big cache, preferably bigger than the actual
 * VM RAM size if possible. The current VM RAM sizes should give some idea for
 * 32-bit boxes, while on 64-bit we can probably get away with employing an
 * unlimited cache.
 *
 * The cache have to parts, as already indicated, the ring-3 side and the
 * ring-0 side.
 *
 * The ring-0 will be tied to the page allocator since it will operate on the
 * memory objects it contains. It will therefore require the first ring-0 mutex
 * discussed in @ref subsec_pgmPhys_Serializing. We
 * some double house keeping wrt to who has mapped what I think, since both
 * VMMR0.r0 and RTR0MemObj will keep track of mapping relataions
 *
 * The ring-3 part will be protected by the pgm critsect. For simplicity, we'll
 * require anyone that desires to do changes to the mapping cache to do that
 * from within this critsect. Alternatively, we could employ a separate critsect
 * for serializing changes to the mapping cache as this would reduce potential
 * contention with other threads accessing mappings unrelated to the changes
 * that are in process. We can see about this later, contention will show
 * up in the statistics anyway, so it'll be simple to tell.
 *
 * The organization of the ring-3 part will be very much like how the allocation
 * chunks are organized in ring-0, that is in an AVL tree by chunk id. To avoid
 * having to walk the tree all the time, we'll have a couple of lookaside entries
 * like in we do for I/O ports and MMIO in IOM.
 *
 * The simplified flow of a PGMPhysRead/Write function:
 *      -# Enter the PGM critsect.
 *      -# Lookup GCPhys in the ram ranges and get the Page ID.
 *      -# Calc the Allocation Chunk ID from the Page ID.
 *      -# Check the lookaside entries and then the AVL tree for the Chunk ID.
 *         If not found in cache:
 *              -# Call ring-0 and request it to be mapped and supply
 *                 a chunk to be unmapped if the cache is maxed out already.
 *              -# Insert the new mapping into the AVL tree (id + R3 address).
 *      -# Update the relevant lookaside entry and return the mapping address.
 *      -# Do the read/write according to monitoring flags and everything.
 *      -# Leave the critsect.
 *
 *
 * @section sec_pgmPhys_Fallback            Fallback
 *
 * Current all the "second tier" hosts will not support the RTR0MemObjAllocPhysNC
 * API and thus require a fallback.
 *
 * So, when RTR0MemObjAllocPhysNC returns VERR_NOT_SUPPORTED the page allocator
 * will return to the ring-3 caller (and later ring-0) and asking it to seed
 * the page allocator with some fresh pages (VERR_GMM_SEED_ME). Ring-3 will
 * then perform an SUPPageAlloc(cbChunk >> PAGE_SHIFT) call and make a
 * "SeededAllocPages" call to ring-0.
 *
 * The first time ring-0 sees the VERR_NOT_SUPPORTED failure it will disable
 * all page sharing (zero page detection will continue). It will also force
 * all allocations to come from the VM which seeded the page. Both these
 * measures are taken to make sure that there will never be any need for
 * mapping anything into ring-3 - everything will be mapped already.
 *
 * Whether we'll continue to use the current MM locked memory management
 * for this I don't quite know (I'd prefer not to and just ditch that all
 * togther), we'll see what's simplest to do.
 *
 *
 *
 * @section sec_pgmPhys_Changes             Changes
 *
 * Breakdown of the changes involved?
 */


/** Saved state data unit version. */
#define PGM_SAVED_STATE_VERSION     5

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/dbgf.h>
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/rem.h>
#include <VBox/dbgf.h>
#include <VBox/rem.h>
#include <VBox/selm.h>
#include <VBox/ssm.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/dbg.h>
#include <VBox/hwaccm.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <VBox/param.h>
#include <VBox/err.h>



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int pgmR3InitPaging(PVM pVM);
static DECLCALLBACK(void) pgmR3PhysInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) pgmR3InfoMode(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) pgmR3InfoCr3(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(int) pgmR3RelocatePhysHandler(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int) pgmR3RelocateVirtHandler(PAVLROGCPTRNODECORE pNode, void *pvUser);
#ifdef VBOX_STRICT
static DECLCALLBACK(void) pgmR3ResetNoMorePhysWritesFlag(PVM pVM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser);
#endif
static DECLCALLBACK(int) pgmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) pgmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static int               pgmR3ModeDataInit(PVM pVM, bool fResolveGCAndR0);
static void              pgmR3ModeDataSwitch(PVM pVM, PGMMODE enmShw, PGMMODE enmGst);
static PGMMODE           pgmR3CalcShadowMode(PGMMODE enmGuestMode, SUPPAGINGMODE enmHostMode, PGMMODE enmShadowMode, VMMSWITCHER *penmSwitcher);

#ifdef VBOX_WITH_STATISTICS
static void pgmR3InitStats(PVM pVM);
#endif

#ifdef VBOX_WITH_DEBUGGER
/** @todo all but the two last commands must be converted to 'info'. */
static DECLCALLBACK(int) pgmR3CmdRam(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) pgmR3CmdMap(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) pgmR3CmdSync(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int) pgmR3CmdSyncAlways(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef VBOX_WITH_DEBUGGER
/** Command descriptors. */
static const DBGCCMD    g_aCmds[] =
{
    /* pszCmd,      cArgsMin, cArgsMax, paArgDesc,          cArgDescs,                  pResultDesc,        fFlags,     pfnHandler          pszSyntax,          ....pszDescription */
    { "pgmram",     0,        0,        NULL,               0,                          NULL,               0,          pgmR3CmdRam,        "",                     "Display the ram ranges." },
    { "pgmmap",     0,        0,        NULL,               0,                          NULL,               0,          pgmR3CmdMap,        "",                     "Display the mapping ranges." },
    { "pgmsync",    0,        0,        NULL,               0,                          NULL,               0,          pgmR3CmdSync,       "",                     "Sync the CR3 page." },
    { "pgmsyncalways", 0,     0,        NULL,               0,                          NULL,               0,          pgmR3CmdSyncAlways, "",                     "Toggle permanent CR3 syncing." },
};
#endif




#if 1/// @todo ndef RT_ARCH_AMD64
/*
 * Shadow - 32-bit mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_32BIT
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_32BIT(name)
#define PGM_SHW_NAME_GC_STR(name)   PGM_SHW_NAME_GC_32BIT_STR(name)
#define PGM_SHW_NAME_R0_STR(name)   PGM_SHW_NAME_R0_32BIT_STR(name)
#include "PGMShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_REAL_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_REAL_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_REAL(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_32BIT_REAL_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_32BIT_REAL_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_PHYS
#include "PGMGst.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_PROT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PROT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_PROT(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_32BIT_PROT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_32BIT_PROT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_PHYS
#include "PGMGst.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_32BIT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_32BIT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_32BIT(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_32BIT_32BIT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_32BIT_32BIT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB
#include "PGMGst.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#undef PGM_SHW_NAME_GC_STR
#undef PGM_SHW_NAME_R0_STR
#endif /* !RT_ARCH_AMD64 */


/*
 * Shadow - PAE mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_PAE
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_PAE(name)
#define PGM_SHW_NAME_GC_STR(name)   PGM_SHW_NAME_GC_PAE_STR(name)
#define PGM_SHW_NAME_R0_STR(name)   PGM_SHW_NAME_R0_PAE_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#include "PGMShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_REAL_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_REAL_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_PAE_REAL_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_PAE_REAL_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_PROT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PROT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PROT(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_PAE_PROT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_PAE_PROT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_32BIT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_32BIT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_32BIT(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_PAE_32BIT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_PAE_32BIT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_32BIT_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - PAE mode */
#define PGM_GST_TYPE                PGM_TYPE_PAE
#define PGM_GST_NAME(name)          PGM_GST_NAME_PAE(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_PAE_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PAE_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PAE(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_PAE_PAE_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_PAE_PAE_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PAE_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
#include "PGMGst.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#undef PGM_SHW_NAME_GC_STR
#undef PGM_SHW_NAME_R0_STR


/*
 * Shadow - AMD64 mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_AMD64
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_AMD64(name)
#define PGM_SHW_NAME_GC_STR(name)   PGM_SHW_NAME_GC_AMD64_STR(name)
#define PGM_SHW_NAME_R0_STR(name)   PGM_SHW_NAME_R0_AMD64_STR(name)
#include "PGMShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_REAL_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_REAL_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_AMD64_REAL(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_AMD64_REAL_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_AMD64_REAL_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_PROT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PROT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_AMD64_PROT(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_AMD64_PROT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_AMD64_PROT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - AMD64 mode */
#define PGM_GST_TYPE                PGM_TYPE_AMD64
#define PGM_GST_NAME(name)          PGM_GST_NAME_AMD64(name)
#define PGM_GST_NAME_GC_STR(name)   PGM_GST_NAME_GC_AMD64_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_AMD64_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_AMD64_AMD64(name)
#define PGM_BTH_NAME_GC_STR(name)   PGM_BTH_NAME_GC_AMD64_AMD64_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_AMD64_AMD64_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PAE_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
#include "PGMGst.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_GC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_GC_STR
#undef PGM_GST_NAME_R0_STR

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#undef PGM_SHW_NAME_GC_STR
#undef PGM_SHW_NAME_R0_STR


/**
 * Initiates the paging of VM.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to VM structure.
 */
PGMR3DECL(int) PGMR3Init(PVM pVM)
{
    LogFlow(("PGMR3Init:\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertRelease(sizeof(pVM->pgm.s) <= sizeof(pVM->pgm.padding));

    /*
     * Init the structure.
     */
    pVM->pgm.s.offVM = RT_OFFSETOF(VM, pgm.s);
    pVM->pgm.s.enmShadowMode    = PGMMODE_INVALID;
    pVM->pgm.s.enmGuestMode     = PGMMODE_INVALID;
    pVM->pgm.s.enmHostMode      = SUPPAGINGMODE_INVALID;
    pVM->pgm.s.GCPhysCR3        = NIL_RTGCPHYS;
    pVM->pgm.s.GCPhysGstCR3Monitored = NIL_RTGCPHYS;
    pVM->pgm.s.fA20Enabled      = true;
    pVM->pgm.s.pGstPaePDPTRHC   = NULL;
    pVM->pgm.s.pGstPaePDPTRGC   = 0;
    for (unsigned i = 0; i < ELEMENTS(pVM->pgm.s.apGstPaePDsHC); i++)
    {
        pVM->pgm.s.apGstPaePDsHC[i]     = NULL;
        pVM->pgm.s.apGstPaePDsGC[i]     = 0;
        pVM->pgm.s.aGCPhysGstPaePDs[i]  = NIL_RTGCPHYS;
    }

#ifdef VBOX_STRICT
    VMR3AtStateRegister(pVM, pgmR3ResetNoMorePhysWritesFlag, NULL);
#endif

    /*
     * Get the configured RAM size - to estimate saved state size.
     */
    uint64_t    cbRam;
    int rc = CFGMR3QueryU64(CFGMR3GetRoot(pVM), "RamSize", &cbRam);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cbRam = pVM->pgm.s.cbRamSize = 0;
    else if (VBOX_SUCCESS(rc))
    {
        if (cbRam < PAGE_SIZE)
            cbRam = 0;
        cbRam = RT_ALIGN_64(cbRam, PAGE_SIZE);
        pVM->pgm.s.cbRamSize = (RTUINT)cbRam;
    }
    else
    {
        AssertMsgFailed(("Configuration error: Failed to query integer \"RamSize\", rc=%Vrc.\n", rc));
        return rc;
    }

    /*
     * Register saved state data unit.
     */
    rc = SSMR3RegisterInternal(pVM, "pgm", 1, PGM_SAVED_STATE_VERSION, (size_t)cbRam + sizeof(PGM),
                               NULL, pgmR3Save, NULL,
                               NULL, pgmR3Load, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Initialize the PGM critical section and flush the phys TLBs
     */
    rc = PDMR3CritSectInit(pVM, &pVM->pgm.s.CritSect, "PGM");
    AssertRCReturn(rc, rc);

    PGMR3PhysChunkInvalidateTLB(pVM);
    PGMPhysInvalidatePageR3MapTLB(pVM);
    PGMPhysInvalidatePageR0MapTLB(pVM);
    PGMPhysInvalidatePageGCMapTLB(pVM);

    /*
     * Trees
     */
    rc = MMHyperAlloc(pVM, sizeof(PGMTREES), 0, MM_TAG_PGM, (void **)&pVM->pgm.s.pTreesHC);
    if (VBOX_SUCCESS(rc))
    {
        pVM->pgm.s.pTreesGC = MMHyperHC2GC(pVM, pVM->pgm.s.pTreesHC);

        /*
         * Alocate the zero page.
         */
        rc = MMHyperAlloc(pVM, PAGE_SIZE, PAGE_SIZE, MM_TAG_PGM, &pVM->pgm.s.pvZeroPgR3);
    }
    if (VBOX_SUCCESS(rc))
    {
        pVM->pgm.s.pvZeroPgGC = MMHyperR3ToGC(pVM, pVM->pgm.s.pvZeroPgR3);
        pVM->pgm.s.pvZeroPgR0 = MMHyperR3ToR0(pVM, pVM->pgm.s.pvZeroPgR3);
        AssertRelease(pVM->pgm.s.pvZeroPgR0 != NIL_RTHCPHYS);
        pVM->pgm.s.HCPhysZeroPg = MMR3HyperHCVirt2HCPhys(pVM, pVM->pgm.s.pvZeroPgR3);
        AssertRelease(pVM->pgm.s.HCPhysZeroPg != NIL_RTHCPHYS);

        /*
         * Init the paging.
         */
        rc = pgmR3InitPaging(pVM);
    }
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Init the page pool.
         */
        rc = pgmR3PoolInit(pVM);
    }
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Info & statistics
         */
        DBGFR3InfoRegisterInternal(pVM, "mode",
                                   "Shows the current paging mode. "
                                   "Recognizes 'all', 'guest', 'shadow' and 'host' as arguments, defaulting to 'all' if nothing's given.",
                                   pgmR3InfoMode);
        DBGFR3InfoRegisterInternal(pVM, "pgmcr3",
                                   "Dumps all the entries in the top level paging table. No arguments.",
                                   pgmR3InfoCr3);
        DBGFR3InfoRegisterInternal(pVM, "phys",
                                   "Dumps all the physical address ranges. No arguments.",
                                   pgmR3PhysInfo);
        DBGFR3InfoRegisterInternal(pVM, "handlers",
                                   "Dumps physical and virtual handlers. "
                                   "Pass 'phys' or 'virt' as argument if only one kind is wanted.",
                                   pgmR3InfoHandlers);

        STAM_REL_REG(pVM, &pVM->pgm.s.cGuestModeChanges, STAMTYPE_COUNTER, "/PGM/cGuestModeChanges", STAMUNIT_OCCURENCES, "Number of guest mode changes.");
#ifdef VBOX_WITH_STATISTICS
        pgmR3InitStats(pVM);
#endif
#ifdef VBOX_WITH_DEBUGGER
        /*
         * Debugger commands.
         */
        static bool fRegisteredCmds = false;
        if (!fRegisteredCmds)
        {
            int rc = DBGCRegisterCommands(&g_aCmds[0], ELEMENTS(g_aCmds));
            if (VBOX_SUCCESS(rc))
                fRegisteredCmds = true;
        }
#endif
        return VINF_SUCCESS;
    }

    /* Almost no cleanup necessary, MM frees all memory. */
    PDMR3CritSectDelete(&pVM->pgm.s.CritSect);

    return rc;
}


/**
 * Init paging.
 *
 * Since we need to check what mode the host is operating in before we can choose
 * the right paging functions for the host we have to delay this until R0 has
 * been initialized.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 */
static int pgmR3InitPaging(PVM pVM)
{
    /*
     * Force a recalculation of modes and switcher so everyone gets notified.
     */
    pVM->pgm.s.enmShadowMode = PGMMODE_INVALID;
    pVM->pgm.s.enmGuestMode  = PGMMODE_INVALID;
    pVM->pgm.s.enmHostMode   = SUPPAGINGMODE_INVALID;

    /*
     * Allocate static mapping space for whatever the cr3 register
     * points to and in the case of PAE mode to the 4 PDs.
     */
    int rc = MMR3HyperReserve(pVM, PAGE_SIZE * 5, "CR3 mapping", &pVM->pgm.s.GCPtrCR3Mapping);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to reserve two pages for cr mapping in HMA, rc=%Vrc\n", rc));
        return rc;
    }
    MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);

    /*
     * Allocate pages for the three possible intermediate contexts
     * (AMD64, PAE and plain 32-Bit). We maintain all three contexts
     * for the sake of simplicity. The AMD64 uses the PAE for the
     * lower levels, making the total number of pages 11 (3 + 7 + 1).
     *
     * We assume that two page tables will be enought for the core code
     * mappings (HC virtual and identity).
     */
    pVM->pgm.s.pInterPD         = (PX86PD)MMR3PageAllocLow(pVM);
    pVM->pgm.s.apInterPTs[0]    = (PX86PT)MMR3PageAllocLow(pVM);
    pVM->pgm.s.apInterPTs[1]    = (PX86PT)MMR3PageAllocLow(pVM);
    pVM->pgm.s.apInterPaePTs[0] = (PX86PTPAE)MMR3PageAlloc(pVM);
    pVM->pgm.s.apInterPaePTs[1] = (PX86PTPAE)MMR3PageAlloc(pVM);
    pVM->pgm.s.apInterPaePDs[0] = (PX86PDPAE)MMR3PageAlloc(pVM);
    pVM->pgm.s.apInterPaePDs[1] = (PX86PDPAE)MMR3PageAlloc(pVM);
    pVM->pgm.s.apInterPaePDs[2] = (PX86PDPAE)MMR3PageAlloc(pVM);
    pVM->pgm.s.apInterPaePDs[3] = (PX86PDPAE)MMR3PageAlloc(pVM);
    pVM->pgm.s.pInterPaePDPTR   = (PX86PDPTR)MMR3PageAllocLow(pVM);
    pVM->pgm.s.pInterPaePDPTR64 = (PX86PDPTR)MMR3PageAllocLow(pVM);
    pVM->pgm.s.pInterPaePML4    = (PX86PML4)MMR3PageAllocLow(pVM);
    if (    !pVM->pgm.s.pInterPD
        ||  !pVM->pgm.s.apInterPTs[0]
        ||  !pVM->pgm.s.apInterPTs[1]
        ||  !pVM->pgm.s.apInterPaePTs[0]
        ||  !pVM->pgm.s.apInterPaePTs[1]
        ||  !pVM->pgm.s.apInterPaePDs[0]
        ||  !pVM->pgm.s.apInterPaePDs[1]
        ||  !pVM->pgm.s.apInterPaePDs[2]
        ||  !pVM->pgm.s.apInterPaePDs[3]
        ||  !pVM->pgm.s.pInterPaePDPTR
        ||  !pVM->pgm.s.pInterPaePDPTR64
        ||  !pVM->pgm.s.pInterPaePML4)
    {
        AssertMsgFailed(("Failed to allocate pages for the intermediate context!\n"));
        return VERR_NO_PAGE_MEMORY;
    }

    pVM->pgm.s.HCPhysInterPD = MMPage2Phys(pVM, pVM->pgm.s.pInterPD);
    AssertRelease(pVM->pgm.s.HCPhysInterPD != NIL_RTHCPHYS && !(pVM->pgm.s.HCPhysInterPD & PAGE_OFFSET_MASK));
    pVM->pgm.s.HCPhysInterPaePDPTR = MMPage2Phys(pVM, pVM->pgm.s.pInterPaePDPTR);
    AssertRelease(pVM->pgm.s.HCPhysInterPaePDPTR != NIL_RTHCPHYS && !(pVM->pgm.s.HCPhysInterPaePDPTR & PAGE_OFFSET_MASK));
    pVM->pgm.s.HCPhysInterPaePML4 = MMPage2Phys(pVM, pVM->pgm.s.pInterPaePML4);
    AssertRelease(pVM->pgm.s.HCPhysInterPaePML4 != NIL_RTHCPHYS && !(pVM->pgm.s.HCPhysInterPaePML4 & PAGE_OFFSET_MASK));

    /*
     * Initialize the pages, setting up the PML4 and PDPTR for repetitive 4GB action.
     */
    ASMMemZeroPage(pVM->pgm.s.pInterPD);
    ASMMemZeroPage(pVM->pgm.s.apInterPTs[0]);
    ASMMemZeroPage(pVM->pgm.s.apInterPTs[1]);

    ASMMemZeroPage(pVM->pgm.s.apInterPaePTs[0]);
    ASMMemZeroPage(pVM->pgm.s.apInterPaePTs[1]);

    ASMMemZeroPage(pVM->pgm.s.pInterPaePDPTR);
    for (unsigned i = 0; i < ELEMENTS(pVM->pgm.s.apInterPaePDs); i++)
    {
        ASMMemZeroPage(pVM->pgm.s.apInterPaePDs[i]);
        pVM->pgm.s.pInterPaePDPTR->a[i].u = X86_PDPE_P | PGM_PLXFLAGS_PERMANENT
                                          | MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[i]);
    }

    for (unsigned i = 0; i < ELEMENTS(pVM->pgm.s.pInterPaePDPTR64->a); i++)
    {
        const unsigned iPD = i % ELEMENTS(pVM->pgm.s.apInterPaePDs);
        pVM->pgm.s.pInterPaePDPTR64->a[i].u = X86_PDPE_P | X86_PDPE_RW | X86_PDPE_US | X86_PDPE_A | PGM_PLXFLAGS_PERMANENT
                                            | MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[iPD]);
    }

    RTHCPHYS HCPhysInterPaePDPTR64 = MMPage2Phys(pVM, pVM->pgm.s.pInterPaePDPTR64);
    for (unsigned i = 0; i < ELEMENTS(pVM->pgm.s.pInterPaePML4->a); i++)
        pVM->pgm.s.pInterPaePML4->a[i].u = X86_PML4E_P | X86_PML4E_RW | X86_PML4E_US | X86_PML4E_A | PGM_PLXFLAGS_PERMANENT
                                         | HCPhysInterPaePDPTR64;

    /*
     * Allocate pages for the three possible guest contexts (AMD64, PAE and plain 32-Bit).
     * We allocate pages for all three posibilities to in order to simplify mappings and
     * avoid resource failure during mode switches. So, we need to cover all levels of the
     * of the first 4GB down to PD level.
     * As with the intermediate context, AMD64 uses the PAE PDPTR and PDs.
     */
    pVM->pgm.s.pHC32BitPD    = (PX86PD)MMR3PageAllocLow(pVM);
    pVM->pgm.s.apHCPaePDs[0] = (PX86PDPAE)MMR3PageAlloc(pVM);
    pVM->pgm.s.apHCPaePDs[1] = (PX86PDPAE)MMR3PageAlloc(pVM);
    AssertRelease((uintptr_t)pVM->pgm.s.apHCPaePDs[0] + PAGE_SIZE == (uintptr_t)pVM->pgm.s.apHCPaePDs[1]);
    pVM->pgm.s.apHCPaePDs[2] = (PX86PDPAE)MMR3PageAlloc(pVM);
    AssertRelease((uintptr_t)pVM->pgm.s.apHCPaePDs[1] + PAGE_SIZE == (uintptr_t)pVM->pgm.s.apHCPaePDs[2]);
    pVM->pgm.s.apHCPaePDs[3] = (PX86PDPAE)MMR3PageAlloc(pVM);
    AssertRelease((uintptr_t)pVM->pgm.s.apHCPaePDs[2] + PAGE_SIZE == (uintptr_t)pVM->pgm.s.apHCPaePDs[3]);
    pVM->pgm.s.pHCPaePDPTR   = (PX86PDPTR)MMR3PageAllocLow(pVM);
    pVM->pgm.s.pHCPaePML4    = (PX86PML4)MMR3PageAllocLow(pVM);
    if (    !pVM->pgm.s.pHC32BitPD
        ||  !pVM->pgm.s.apHCPaePDs[0]
        ||  !pVM->pgm.s.apHCPaePDs[1]
        ||  !pVM->pgm.s.apHCPaePDs[2]
        ||  !pVM->pgm.s.apHCPaePDs[3]
        ||  !pVM->pgm.s.pHCPaePDPTR
        ||  !pVM->pgm.s.pHCPaePML4)
    {
        AssertMsgFailed(("Failed to allocate pages for the intermediate context!\n"));
        return VERR_NO_PAGE_MEMORY;
    }

    /* get physical addresses. */
    pVM->pgm.s.HCPhys32BitPD    = MMPage2Phys(pVM, pVM->pgm.s.pHC32BitPD);
    Assert(MMPagePhys2Page(pVM, pVM->pgm.s.HCPhys32BitPD) == pVM->pgm.s.pHC32BitPD);
    pVM->pgm.s.aHCPhysPaePDs[0] = MMPage2Phys(pVM, pVM->pgm.s.apHCPaePDs[0]);
    pVM->pgm.s.aHCPhysPaePDs[1] = MMPage2Phys(pVM, pVM->pgm.s.apHCPaePDs[1]);
    pVM->pgm.s.aHCPhysPaePDs[2] = MMPage2Phys(pVM, pVM->pgm.s.apHCPaePDs[2]);
    pVM->pgm.s.aHCPhysPaePDs[3] = MMPage2Phys(pVM, pVM->pgm.s.apHCPaePDs[3]);
    pVM->pgm.s.HCPhysPaePDPTR   = MMPage2Phys(pVM, pVM->pgm.s.pHCPaePDPTR);
    pVM->pgm.s.HCPhysPaePML4    = MMPage2Phys(pVM, pVM->pgm.s.pHCPaePML4);

    /*
     * Initialize the pages, setting up the PML4 and PDPTR for action below 4GB.
     */
    ASMMemZero32(pVM->pgm.s.pHC32BitPD, PAGE_SIZE);

    ASMMemZero32(pVM->pgm.s.pHCPaePDPTR, PAGE_SIZE);
    for (unsigned i = 0; i < ELEMENTS(pVM->pgm.s.apHCPaePDs); i++)
    {
        ASMMemZero32(pVM->pgm.s.apHCPaePDs[i], PAGE_SIZE);
        pVM->pgm.s.pHCPaePDPTR->a[i].u = X86_PDPE_P | PGM_PLXFLAGS_PERMANENT | pVM->pgm.s.aHCPhysPaePDs[i];
        /* The flags will be corrected when entering and leaving long mode. */
    }

    ASMMemZero32(pVM->pgm.s.pHCPaePML4, PAGE_SIZE);
    pVM->pgm.s.pHCPaePML4->a[0].u = X86_PML4E_P | X86_PML4E_RW | X86_PML4E_A
                                  | PGM_PLXFLAGS_PERMANENT | pVM->pgm.s.HCPhysPaePDPTR;

    CPUMSetHyperCR3(pVM, (uint32_t)pVM->pgm.s.HCPhys32BitPD);

    /*
     * Initialize paging workers and mode from current host mode
     * and the guest running in real mode.
     */
    pVM->pgm.s.enmHostMode = SUPGetPagingMode();
    switch (pVM->pgm.s.enmHostMode)
    {
        case SUPPAGINGMODE_32_BIT:
        case SUPPAGINGMODE_32_BIT_GLOBAL:
        case SUPPAGINGMODE_PAE:
        case SUPPAGINGMODE_PAE_GLOBAL:
        case SUPPAGINGMODE_PAE_NX:
        case SUPPAGINGMODE_PAE_GLOBAL_NX:
            break;

        case SUPPAGINGMODE_AMD64:
        case SUPPAGINGMODE_AMD64_GLOBAL:
        case SUPPAGINGMODE_AMD64_NX:
        case SUPPAGINGMODE_AMD64_GLOBAL_NX:
#ifndef VBOX_WITH_HYBIRD_32BIT_KERNEL
            if (ARCH_BITS != 64)
            {
                AssertMsgFailed(("Host mode %d (64-bit) is not supported by non-64bit builds\n", pVM->pgm.s.enmHostMode));
                LogRel(("Host mode %d (64-bit) is not supported by non-64bit builds\n", pVM->pgm.s.enmHostMode));
                return VERR_PGM_UNSUPPORTED_HOST_PAGING_MODE;
            }
#endif
            break;
        default:
            AssertMsgFailed(("Host mode %d is not supported\n", pVM->pgm.s.enmHostMode));
            return VERR_PGM_UNSUPPORTED_HOST_PAGING_MODE;
    }
    rc = pgmR3ModeDataInit(pVM, false /* don't resolve GC and R0 syms yet */);
    if (VBOX_SUCCESS(rc))
        rc = pgmR3ChangeMode(pVM, PGMMODE_REAL);
    if (VBOX_SUCCESS(rc))
    {
        LogFlow(("pgmR3InitPaging: returns successfully\n"));
#if HC_ARCH_BITS == 64
LogRel(("Debug: HCPhys32BitPD=%VHp aHCPhysPaePDs={%VHp,%VHp,%VHp,%VHp} HCPhysPaePDPTR=%VHp HCPhysPaePML4=%VHp\n",
        pVM->pgm.s.HCPhys32BitPD, pVM->pgm.s.aHCPhysPaePDs[0], pVM->pgm.s.aHCPhysPaePDs[1], pVM->pgm.s.aHCPhysPaePDs[2], pVM->pgm.s.aHCPhysPaePDs[3],
        pVM->pgm.s.HCPhysPaePDPTR, pVM->pgm.s.HCPhysPaePML4));
LogRel(("Debug: HCPhysInterPD=%VHp HCPhysInterPaePDPTR=%VHp HCPhysInterPaePML4=%VHp\n",
        pVM->pgm.s.HCPhysInterPD, pVM->pgm.s.HCPhysInterPaePDPTR, pVM->pgm.s.HCPhysInterPaePML4));
LogRel(("Debug: apInterPTs={%VHp,%VHp} apInterPaePTs={%VHp,%VHp} apInterPaePDs={%VHp,%VHp,%VHp,%VHp} pInterPaePDPTR64=%VHp\n",
        MMPage2Phys(pVM, pVM->pgm.s.apInterPTs[0]), MMPage2Phys(pVM, pVM->pgm.s.apInterPTs[1]),
        MMPage2Phys(pVM, pVM->pgm.s.apInterPaePTs[0]), MMPage2Phys(pVM, pVM->pgm.s.apInterPaePTs[1]),
        MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[0]), MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[1]), MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[2]), MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[3]),
        MMPage2Phys(pVM, pVM->pgm.s.pInterPaePDPTR64)));
#endif

        return VINF_SUCCESS;
    }

    LogFlow(("pgmR3InitPaging: returns %Vrc\n", rc));
    return rc;
}


#ifdef VBOX_WITH_STATISTICS
/**
 * Init statistics
 */
static void pgmR3InitStats(PVM pVM)
{
    PPGM pPGM = &pVM->pgm.s;
    STAM_REG(pVM, &pPGM->StatGCInvalidatePage,              STAMTYPE_PROFILE, "/PGM/GC/InvalidatePage",             STAMUNIT_TICKS_PER_CALL, "PGMGCInvalidatePage() profiling.");
    STAM_REG(pVM, &pPGM->StatGCInvalidatePage4KBPages,      STAMTYPE_COUNTER, "/PGM/GC/InvalidatePage/4KBPages",    STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() was called for a 4KB page.");
    STAM_REG(pVM, &pPGM->StatGCInvalidatePage4MBPages,      STAMTYPE_COUNTER, "/PGM/GC/InvalidatePage/4MBPages",    STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() was called for a 4MB page.");
    STAM_REG(pVM, &pPGM->StatGCInvalidatePage4MBPagesSkip,  STAMTYPE_COUNTER, "/PGM/GC/InvalidatePage/4MBPagesSkip",STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() skipped a 4MB page.");
    STAM_REG(pVM, &pPGM->StatGCInvalidatePagePDMappings,    STAMTYPE_COUNTER, "/PGM/GC/InvalidatePage/PDMappings",  STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() was called for a page directory containing mappings (no conflict).");
    STAM_REG(pVM, &pPGM->StatGCInvalidatePagePDNAs,         STAMTYPE_COUNTER, "/PGM/GC/InvalidatePage/PDNAs",       STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() was called for a not accessed page directory.");
    STAM_REG(pVM, &pPGM->StatGCInvalidatePagePDNPs,         STAMTYPE_COUNTER, "/PGM/GC/InvalidatePage/PDNPs",       STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() was called for a not present page directory.");
    STAM_REG(pVM, &pPGM->StatGCInvalidatePagePDOutOfSync,   STAMTYPE_COUNTER, "/PGM/GC/InvalidatePage/PDOutOfSync", STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() was called for an out of sync page directory.");
    STAM_REG(pVM, &pPGM->StatGCInvalidatePageSkipped,       STAMTYPE_COUNTER, "/PGM/GC/InvalidatePage/Skipped",     STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() was skipped due to not present shw or pending pending SyncCR3.");
    STAM_REG(pVM, &pPGM->StatGCSyncPT,                      STAMTYPE_PROFILE, "/PGM/GC/SyncPT",                     STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMGCSyncPT() body.");
    STAM_REG(pVM, &pPGM->StatGCAccessedPage,                STAMTYPE_COUNTER, "/PGM/GC/AccessedPage",               STAMUNIT_OCCURENCES,     "The number of pages marked not present for accessed bit emulation.");
    STAM_REG(pVM, &pPGM->StatGCDirtyPage,                   STAMTYPE_COUNTER, "/PGM/GC/DirtyPage/Mark",             STAMUNIT_OCCURENCES,     "The number of pages marked read-only for dirty bit tracking.");
    STAM_REG(pVM, &pPGM->StatGCDirtyPageBig,                STAMTYPE_COUNTER, "/PGM/GC/DirtyPage/MarkBig",          STAMUNIT_OCCURENCES,     "The number of 4MB pages marked read-only for dirty bit tracking.");
    STAM_REG(pVM, &pPGM->StatGCDirtyPageTrap,               STAMTYPE_COUNTER, "/PGM/GC/DirtyPage/Trap",             STAMUNIT_OCCURENCES,     "The number of traps generated for dirty bit tracking.");
    STAM_REG(pVM, &pPGM->StatGCDirtyPageSkipped,            STAMTYPE_COUNTER, "/PGM/GC/DirtyPage/Skipped",          STAMUNIT_OCCURENCES,     "The number of pages already dirty or readonly.");
    STAM_REG(pVM, &pPGM->StatGCDirtiedPage,                 STAMTYPE_COUNTER, "/PGM/GC/DirtyPage/SetDirty",         STAMUNIT_OCCURENCES,     "The number of pages marked dirty because of write accesses.");
    STAM_REG(pVM, &pPGM->StatGCDirtyTrackRealPF,            STAMTYPE_COUNTER, "/PGM/GC/DirtyPage/RealPF",           STAMUNIT_OCCURENCES,     "The number of real pages faults during dirty bit tracking.");
    STAM_REG(pVM, &pPGM->StatGCPageAlreadyDirty,            STAMTYPE_COUNTER, "/PGM/GC/DirtyPage/AlreadySet",       STAMUNIT_OCCURENCES,     "The number of pages already marked dirty because of write accesses.");
    STAM_REG(pVM, &pPGM->StatGCDirtyBitTracking,            STAMTYPE_PROFILE, "/PGM/GC/DirtyPage",                  STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMTrackDirtyBit() body.");
    STAM_REG(pVM, &pPGM->StatGCSyncPTAlloc,                 STAMTYPE_COUNTER, "/PGM/GC/SyncPT/Alloc",               STAMUNIT_OCCURENCES,     "The number of times PGMGCSyncPT() needed to allocate page tables.");
    STAM_REG(pVM, &pPGM->StatGCSyncPTConflict,              STAMTYPE_COUNTER, "/PGM/GC/SyncPT/Conflicts",           STAMUNIT_OCCURENCES,     "The number of times PGMGCSyncPT() detected conflicts.");
    STAM_REG(pVM, &pPGM->StatGCSyncPTFailed,                STAMTYPE_COUNTER, "/PGM/GC/SyncPT/Failed",              STAMUNIT_OCCURENCES,     "The number of times PGMGCSyncPT() failed.");

    STAM_REG(pVM, &pPGM->StatGCTrap0e,                      STAMTYPE_PROFILE, "/PGM/GC/Trap0e",                     STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMGCTrap0eHandler() body.");
    STAM_REG(pVM, &pPGM->StatCheckPageFault,                STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time/CheckPageFault", STAMUNIT_TICKS_PER_CALL, "Profiling of checking for dirty/access emulation faults.");
    STAM_REG(pVM, &pPGM->StatLazySyncPT,                    STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time/SyncPT",         STAMUNIT_TICKS_PER_CALL, "Profiling of lazy page table syncing.");
    STAM_REG(pVM, &pPGM->StatMapping,                       STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time/Mapping",        STAMUNIT_TICKS_PER_CALL, "Profiling of checking virtual mappings.");
    STAM_REG(pVM, &pPGM->StatOutOfSync,                     STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time/OutOfSync",      STAMUNIT_TICKS_PER_CALL, "Profiling of out of sync page handling.");
    STAM_REG(pVM, &pPGM->StatHandlers,                      STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time/Handlers",       STAMUNIT_TICKS_PER_CALL, "Profiling of checking handlers.");
    STAM_REG(pVM, &pPGM->StatEIPHandlers,                   STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time/EIPHandlers",    STAMUNIT_TICKS_PER_CALL, "Profiling of checking eip handlers.");
    STAM_REG(pVM, &pPGM->StatTrap0eCSAM,                    STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/CSAM",          STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is CSAM.");
    STAM_REG(pVM, &pPGM->StatTrap0eDirtyAndAccessedBits,    STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/DirtyAndAccessedBits", STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is dirty and/or accessed bit emulation.");
    STAM_REG(pVM, &pPGM->StatTrap0eGuestTrap,               STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/GuestTrap",     STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is a guest trap.");
    STAM_REG(pVM, &pPGM->StatTrap0eHndPhys,                 STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/HandlerPhysical", STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is a physical handler.");
    STAM_REG(pVM, &pPGM->StatTrap0eHndVirt,                 STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/HandlerVirtual",STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is a virtual handler.");
    STAM_REG(pVM, &pPGM->StatTrap0eHndUnhandled,            STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/HandlerUnhandled", STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is access outside the monitored areas of a monitored page.");
    STAM_REG(pVM, &pPGM->StatTrap0eMisc,                    STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/Misc",          STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is not known.");
    STAM_REG(pVM, &pPGM->StatTrap0eOutOfSync,               STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/OutOfSync",     STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is an out-of-sync page.");
    STAM_REG(pVM, &pPGM->StatTrap0eOutOfSyncHndPhys,        STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/OutOfSyncHndPhys", STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is an out-of-sync physical handler page.");
    STAM_REG(pVM, &pPGM->StatTrap0eOutOfSyncHndVirt,        STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/OutOfSyncHndVirt", STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is an out-of-sync virtual handler page.");
    STAM_REG(pVM, &pPGM->StatTrap0eOutOfSyncObsHnd,         STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/OutOfSyncObsHnd", STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is an obsolete handler page.");
    STAM_REG(pVM, &pPGM->StatTrap0eSyncPT,                  STAMTYPE_PROFILE, "/PGM/GC/Trap0e/Time2/SyncPT",        STAMUNIT_TICKS_PER_CALL, "Profiling of the Trap0eHandler body when the cause is lazy syncing of a PT.");

    STAM_REG(pVM, &pPGM->StatTrap0eMapHandler,              STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Handlers/Mapping",    STAMUNIT_OCCURENCES,     "Number of traps due to access handlers in mappings.");
    STAM_REG(pVM, &pPGM->StatHandlersOutOfSync,             STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Handlers/OutOfSync",  STAMUNIT_OCCURENCES,     "Number of traps due to out-of-sync handled pages.");
    STAM_REG(pVM, &pPGM->StatHandlersPhysical,              STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Handlers/Physical",   STAMUNIT_OCCURENCES,     "Number of traps due to physical access handlers.");
    STAM_REG(pVM, &pPGM->StatHandlersVirtual,               STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Handlers/Virtual",    STAMUNIT_OCCURENCES,     "Number of traps due to virtual access handlers.");
    STAM_REG(pVM, &pPGM->StatHandlersVirtualByPhys,         STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Handlers/VirtualByPhys", STAMUNIT_OCCURENCES,  "Number of traps due to virtual access handlers by physical address.");
    STAM_REG(pVM, &pPGM->StatHandlersVirtualUnmarked,       STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Handlers/VirtualUnmarked", STAMUNIT_OCCURENCES,"Number of traps due to virtual access handlers by virtual address (without proper physical flags).");
    STAM_REG(pVM, &pPGM->StatHandlersUnhandled,             STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Handlers/Unhandled",  STAMUNIT_OCCURENCES,     "Number of traps due to access outside range of monitored page(s).");

    STAM_REG(pVM, &pPGM->StatGCTrap0eConflicts,             STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Conflicts",           STAMUNIT_OCCURENCES,     "The number of times #PF was caused by an undetected conflict.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eUSNotPresentRead,      STAMTYPE_COUNTER, "/PGM/GC/Trap0e/User/NPRead",         STAMUNIT_OCCURENCES,     "Number of user mode not present read page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eUSNotPresentWrite,     STAMTYPE_COUNTER, "/PGM/GC/Trap0e/User/NPWrite",        STAMUNIT_OCCURENCES,     "Number of user mode not present write page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eUSWrite,               STAMTYPE_COUNTER, "/PGM/GC/Trap0e/User/Write",          STAMUNIT_OCCURENCES,     "Number of user mode write page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eUSReserved,            STAMTYPE_COUNTER, "/PGM/GC/Trap0e/User/Reserved",       STAMUNIT_OCCURENCES,     "Number of user mode reserved bit page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eUSRead,                STAMTYPE_COUNTER, "/PGM/GC/Trap0e/User/Read",           STAMUNIT_OCCURENCES,     "Number of user mode read page faults.");

    STAM_REG(pVM, &pPGM->StatGCTrap0eSVNotPresentRead,      STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Supervisor/NPRead",   STAMUNIT_OCCURENCES,     "Number of supervisor mode not present read page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eSVNotPresentWrite,     STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Supervisor/NPWrite",  STAMUNIT_OCCURENCES,     "Number of supervisor mode not present write page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eSVWrite,               STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Supervisor/Write",    STAMUNIT_OCCURENCES,     "Number of supervisor mode write page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eSVReserved,            STAMTYPE_COUNTER, "/PGM/GC/Trap0e/Supervisor/Reserved", STAMUNIT_OCCURENCES,     "Number of supervisor mode reserved bit page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eUnhandled,             STAMTYPE_COUNTER, "/PGM/GC/Trap0e/GuestPF/Unhandled",   STAMUNIT_OCCURENCES,     "Number of guest real page faults.");
    STAM_REG(pVM, &pPGM->StatGCTrap0eMap,                   STAMTYPE_COUNTER, "/PGM/GC/Trap0e/GuestPF/Map",         STAMUNIT_OCCURENCES,     "Number of guest page faults due to map accesses.");


    STAM_REG(pVM, &pPGM->StatGCGuestCR3WriteHandled,        STAMTYPE_COUNTER, "/PGM/GC/CR3WriteInt",                STAMUNIT_OCCURENCES,     "The number of times the Guest CR3 change was successfully handled.");
    STAM_REG(pVM, &pPGM->StatGCGuestCR3WriteUnhandled,      STAMTYPE_COUNTER, "/PGM/GC/CR3WriteEmu",                STAMUNIT_OCCURENCES,     "The number of times the Guest CR3 change was passed back to the recompiler.");
    STAM_REG(pVM, &pPGM->StatGCGuestCR3WriteConflict,       STAMTYPE_COUNTER, "/PGM/GC/CR3WriteConflict",           STAMUNIT_OCCURENCES,     "The number of times the Guest CR3 monitoring detected a conflict.");

    STAM_REG(pVM, &pPGM->StatGCPageOutOfSyncSupervisor,     STAMTYPE_COUNTER, "/PGM/GC/OutOfSync/SuperVisor",       STAMUNIT_OCCURENCES,     "Number of traps due to pages out of sync.");
    STAM_REG(pVM, &pPGM->StatGCPageOutOfSyncUser,           STAMTYPE_COUNTER, "/PGM/GC/OutOfSync/User",             STAMUNIT_OCCURENCES,     "Number of traps due to pages out of sync.");

    STAM_REG(pVM, &pPGM->StatGCGuestROMWriteHandled,        STAMTYPE_COUNTER, "/PGM/GC/ROMWriteInt",                STAMUNIT_OCCURENCES,     "The number of times the Guest ROM change was successfully handled.");
    STAM_REG(pVM, &pPGM->StatGCGuestROMWriteUnhandled,      STAMTYPE_COUNTER, "/PGM/GC/ROMWriteEmu",                STAMUNIT_OCCURENCES,     "The number of times the Guest ROM change was passed back to the recompiler.");

    STAM_REG(pVM, &pPGM->StatDynMapCacheHits,               STAMTYPE_COUNTER, "/PGM/GC/DynMapCache/Hits" ,          STAMUNIT_OCCURENCES,     "Number of dynamic page mapping cache hits.");
    STAM_REG(pVM, &pPGM->StatDynMapCacheMisses,             STAMTYPE_COUNTER, "/PGM/GC/DynMapCache/Misses" ,        STAMUNIT_OCCURENCES,     "Number of dynamic page mapping cache misses.");

    STAM_REG(pVM, &pPGM->StatHCDetectedConflicts,           STAMTYPE_COUNTER, "/PGM/HC/DetectedConflicts",          STAMUNIT_OCCURENCES,     "The number of times PGMR3CheckMappingConflicts() detected a conflict.");
    STAM_REG(pVM, &pPGM->StatHCGuestPDWrite,                STAMTYPE_COUNTER, "/PGM/HC/PDWrite",                    STAMUNIT_OCCURENCES,     "The total number of times pgmHCGuestPDWriteHandler() was called.");
    STAM_REG(pVM, &pPGM->StatHCGuestPDWriteConflict,        STAMTYPE_COUNTER, "/PGM/HC/PDWriteConflict",            STAMUNIT_OCCURENCES,     "The number of times pgmHCGuestPDWriteHandler() detected a conflict.");

    STAM_REG(pVM, &pPGM->StatHCInvalidatePage,              STAMTYPE_PROFILE, "/PGM/HC/InvalidatePage",             STAMUNIT_TICKS_PER_CALL, "PGMHCInvalidatePage() profiling.");
    STAM_REG(pVM, &pPGM->StatHCInvalidatePage4KBPages,      STAMTYPE_COUNTER, "/PGM/HC/InvalidatePage/4KBPages",    STAMUNIT_OCCURENCES,     "The number of times PGMHCInvalidatePage() was called for a 4KB page.");
    STAM_REG(pVM, &pPGM->StatHCInvalidatePage4MBPages,      STAMTYPE_COUNTER, "/PGM/HC/InvalidatePage/4MBPages",    STAMUNIT_OCCURENCES,     "The number of times PGMHCInvalidatePage() was called for a 4MB page.");
    STAM_REG(pVM, &pPGM->StatHCInvalidatePage4MBPagesSkip,  STAMTYPE_COUNTER, "/PGM/HC/InvalidatePage/4MBPagesSkip",STAMUNIT_OCCURENCES,     "The number of times PGMHCInvalidatePage() skipped a 4MB page.");
    STAM_REG(pVM, &pPGM->StatHCInvalidatePagePDMappings,    STAMTYPE_COUNTER, "/PGM/HC/InvalidatePage/PDMappings",  STAMUNIT_OCCURENCES,     "The number of times PGMHCInvalidatePage() was called for a page directory containing mappings (no conflict).");
    STAM_REG(pVM, &pPGM->StatHCInvalidatePagePDNAs,         STAMTYPE_COUNTER, "/PGM/HC/InvalidatePage/PDNAs",       STAMUNIT_OCCURENCES,     "The number of times PGMHCInvalidatePage() was called for a not accessed page directory.");
    STAM_REG(pVM, &pPGM->StatHCInvalidatePagePDNPs,         STAMTYPE_COUNTER, "/PGM/HC/InvalidatePage/PDNPs",       STAMUNIT_OCCURENCES,     "The number of times PGMHCInvalidatePage() was called for a not present page directory.");
    STAM_REG(pVM, &pPGM->StatHCInvalidatePagePDOutOfSync,   STAMTYPE_COUNTER, "/PGM/HC/InvalidatePage/PDOutOfSync", STAMUNIT_OCCURENCES,     "The number of times PGMGCInvalidatePage() was called for an out of sync page directory.");
    STAM_REG(pVM, &pPGM->StatHCInvalidatePageSkipped,       STAMTYPE_COUNTER, "/PGM/HC/InvalidatePage/Skipped",     STAMUNIT_OCCURENCES,     "The number of times PGMHCInvalidatePage() was skipped due to not present shw or pending pending SyncCR3.");
    STAM_REG(pVM, &pPGM->StatHCResolveConflict,             STAMTYPE_PROFILE, "/PGM/HC/ResolveConflict",            STAMUNIT_TICKS_PER_CALL, "pgmR3SyncPTResolveConflict() profiling (includes the entire relocation).");
    STAM_REG(pVM, &pPGM->StatHCPrefetch,                    STAMTYPE_PROFILE, "/PGM/HC/Prefetch",                   STAMUNIT_TICKS_PER_CALL, "PGMR3PrefetchPage profiling.");

    STAM_REG(pVM, &pPGM->StatHCSyncPT,                      STAMTYPE_PROFILE, "/PGM/HC/SyncPT",                     STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMR3SyncPT() body.");
    STAM_REG(pVM, &pPGM->StatHCAccessedPage,                STAMTYPE_COUNTER, "/PGM/HC/AccessedPage",               STAMUNIT_OCCURENCES,     "The number of pages marked not present for accessed bit emulation.");
    STAM_REG(pVM, &pPGM->StatHCDirtyPage,                   STAMTYPE_COUNTER, "/PGM/HC/DirtyPage/Mark",             STAMUNIT_OCCURENCES,     "The number of pages marked read-only for dirty bit tracking.");
    STAM_REG(pVM, &pPGM->StatHCDirtyPageBig,                STAMTYPE_COUNTER, "/PGM/HC/DirtyPage/MarkBig",          STAMUNIT_OCCURENCES,     "The number of 4MB pages marked read-only for dirty bit tracking.");
    STAM_REG(pVM, &pPGM->StatHCDirtyPageTrap,               STAMTYPE_COUNTER, "/PGM/HC/DirtyPage/Trap",             STAMUNIT_OCCURENCES,     "The number of traps generated for dirty bit tracking.");
    STAM_REG(pVM, &pPGM->StatHCDirtyPageSkipped,            STAMTYPE_COUNTER, "/PGM/HC/DirtyPage/Skipped",          STAMUNIT_OCCURENCES,     "The number of pages already dirty or readonly.");
    STAM_REG(pVM, &pPGM->StatHCDirtyBitTracking,            STAMTYPE_PROFILE, "/PGM/HC/DirtyPage",                  STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMTrackDirtyBit() body.");

    STAM_REG(pVM, &pPGM->StatGCSyncPagePDNAs,               STAMTYPE_COUNTER, "/PGM/GC/SyncPagePDNAs",              STAMUNIT_OCCURENCES,     "The number of time we've marked a PD not present from SyncPage to virtualize the accessed bit.");
    STAM_REG(pVM, &pPGM->StatGCSyncPagePDOutOfSync,         STAMTYPE_COUNTER, "/PGM/GC/SyncPagePDOutOfSync",        STAMUNIT_OCCURENCES,     "The number of time we've encountered an out-of-sync PD in SyncPage.");
    STAM_REG(pVM, &pPGM->StatHCSyncPagePDNAs,               STAMTYPE_COUNTER, "/PGM/HC/SyncPagePDNAs",              STAMUNIT_OCCURENCES,     "The number of time we've marked a PD not present from SyncPage to virtualize the accessed bit.");
    STAM_REG(pVM, &pPGM->StatHCSyncPagePDOutOfSync,         STAMTYPE_COUNTER, "/PGM/HC/SyncPagePDOutOfSync",        STAMUNIT_OCCURENCES,     "The number of time we've encountered an out-of-sync PD in SyncPage.");

    STAM_REG(pVM, &pPGM->StatFlushTLB,                      STAMTYPE_PROFILE, "/PGM/FlushTLB",                      STAMUNIT_OCCURENCES,     "Profiling of the PGMFlushTLB() body.");
    STAM_REG(pVM, &pPGM->StatFlushTLBNewCR3,                STAMTYPE_COUNTER, "/PGM/FlushTLB/NewCR3",               STAMUNIT_OCCURENCES,     "The number of times PGMFlushTLB was called with a new CR3, non-global. (switch)");
    STAM_REG(pVM, &pPGM->StatFlushTLBNewCR3Global,          STAMTYPE_COUNTER, "/PGM/FlushTLB/NewCR3Global",         STAMUNIT_OCCURENCES,     "The number of times PGMFlushTLB was called with a new CR3, global. (switch)");
    STAM_REG(pVM, &pPGM->StatFlushTLBSameCR3,               STAMTYPE_COUNTER, "/PGM/FlushTLB/SameCR3",              STAMUNIT_OCCURENCES,     "The number of times PGMFlushTLB was called with the same CR3, non-global. (flush)");
    STAM_REG(pVM, &pPGM->StatFlushTLBSameCR3Global,         STAMTYPE_COUNTER, "/PGM/FlushTLB/SameCR3Global",        STAMUNIT_OCCURENCES,     "The number of times PGMFlushTLB was called with the same CR3, global. (flush)");

    STAM_REG(pVM, &pPGM->StatGCSyncCR3,                     STAMTYPE_PROFILE, "/PGM/GC/SyncCR3",                       STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMSyncCR3() body.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3Handlers,             STAMTYPE_PROFILE, "/PGM/GC/SyncCR3/Handlers",              STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMSyncCR3() update handler section.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3HandlerVirtualUpdate, STAMTYPE_PROFILE, "/PGM/GC/SyncCR3/Handlers/VirtualUpdate",STAMUNIT_TICKS_PER_CALL, "Profiling of the virtual handler updates.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3HandlerVirtualReset,  STAMTYPE_PROFILE, "/PGM/GC/SyncCR3/Handlers/VirtualReset", STAMUNIT_TICKS_PER_CALL, "Profiling of the virtual handler resets.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3Global,               STAMTYPE_COUNTER, "/PGM/GC/SyncCR3/Global",                STAMUNIT_OCCURENCES,     "The number of global CR3 syncs.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3NotGlobal,            STAMTYPE_COUNTER, "/PGM/GC/SyncCR3/NotGlobal",             STAMUNIT_OCCURENCES,     "The number of non-global CR3 syncs.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3DstCacheHit,          STAMTYPE_COUNTER, "/PGM/GC/SyncCR3/DstChacheHit",          STAMUNIT_OCCURENCES,     "The number of times we got some kind of a cache hit.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3DstFreed,             STAMTYPE_COUNTER, "/PGM/GC/SyncCR3/DstFreed",              STAMUNIT_OCCURENCES,     "The number of times we've had to free a shadow entry.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3DstFreedSrcNP,        STAMTYPE_COUNTER, "/PGM/GC/SyncCR3/DstFreedSrcNP",         STAMUNIT_OCCURENCES,     "The number of times we've had to free a shadow entry for which the source entry was not present.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3DstNotPresent,        STAMTYPE_COUNTER, "/PGM/GC/SyncCR3/DstNotPresent",         STAMUNIT_OCCURENCES,     "The number of times we've encountered a not present shadow entry for a present guest entry.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3DstSkippedGlobalPD,   STAMTYPE_COUNTER, "/PGM/GC/SyncCR3/DstSkippedGlobalPD",    STAMUNIT_OCCURENCES,     "The number of times a global page directory wasn't flushed.");
    STAM_REG(pVM, &pPGM->StatGCSyncCR3DstSkippedGlobalPT,   STAMTYPE_COUNTER, "/PGM/GC/SyncCR3/DstSkippedGlobalPT",    STAMUNIT_OCCURENCES,     "The number of times a page table with only global entries wasn't flushed.");

    STAM_REG(pVM, &pPGM->StatHCSyncCR3,                     STAMTYPE_PROFILE, "/PGM/HC/SyncCR3",                       STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMSyncCR3() body.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3Handlers,             STAMTYPE_PROFILE, "/PGM/HC/SyncCR3/Handlers",              STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMSyncCR3() update handler section.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3HandlerVirtualUpdate, STAMTYPE_PROFILE, "/PGM/HC/SyncCR3/Handlers/VirtualUpdate",STAMUNIT_TICKS_PER_CALL, "Profiling of the virtual handler updates.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3HandlerVirtualReset,  STAMTYPE_PROFILE, "/PGM/HC/SyncCR3/Handlers/VirtualReset", STAMUNIT_TICKS_PER_CALL, "Profiling of the virtual handler resets.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3Global,               STAMTYPE_COUNTER, "/PGM/HC/SyncCR3/Global",                STAMUNIT_OCCURENCES,     "The number of global CR3 syncs.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3NotGlobal,            STAMTYPE_COUNTER, "/PGM/HC/SyncCR3/NotGlobal",             STAMUNIT_OCCURENCES,     "The number of non-global CR3 syncs.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3DstCacheHit,          STAMTYPE_COUNTER, "/PGM/HC/SyncCR3/DstChacheHit",          STAMUNIT_OCCURENCES,     "The number of times we got some kind of a cache hit.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3DstFreed,             STAMTYPE_COUNTER, "/PGM/HC/SyncCR3/DstFreed",              STAMUNIT_OCCURENCES,     "The number of times we've had to free a shadow entry.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3DstFreedSrcNP,        STAMTYPE_COUNTER, "/PGM/HC/SyncCR3/DstFreedSrcNP",         STAMUNIT_OCCURENCES,     "The number of times we've had to free a shadow entry for which the source entry was not present.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3DstNotPresent,        STAMTYPE_COUNTER, "/PGM/HC/SyncCR3/DstNotPresent",         STAMUNIT_OCCURENCES,     "The number of times we've encountered a not present shadow entry for a present guest entry.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3DstSkippedGlobalPD,   STAMTYPE_COUNTER, "/PGM/HC/SyncCR3/DstSkippedGlobalPD",    STAMUNIT_OCCURENCES,     "The number of times a global page directory wasn't flushed.");
    STAM_REG(pVM, &pPGM->StatHCSyncCR3DstSkippedGlobalPT,   STAMTYPE_COUNTER, "/PGM/HC/SyncCR3/DstSkippedGlobalPT",    STAMUNIT_OCCURENCES,     "The number of times a page table with only global entries wasn't flushed.");

    STAM_REG(pVM, &pPGM->StatVirtHandleSearchByPhysGC,      STAMTYPE_PROFILE, "/PGM/VirtHandler/SearchByPhys/GC",   STAMUNIT_TICKS_PER_CALL, "Profiling of pgmHandlerVirtualFindByPhysAddr in GC.");
    STAM_REG(pVM, &pPGM->StatVirtHandleSearchByPhysHC,      STAMTYPE_PROFILE, "/PGM/VirtHandler/SearchByPhys/HC",   STAMUNIT_TICKS_PER_CALL, "Profiling of pgmHandlerVirtualFindByPhysAddr in HC.");
    STAM_REG(pVM, &pPGM->StatHandlePhysicalReset,           STAMTYPE_COUNTER, "/PGM/HC/HandlerPhysicalReset",       STAMUNIT_OCCURENCES,     "The number of times PGMR3HandlerPhysicalReset is called.");

    STAM_REG(pVM, &pPGM->StatHCGstModifyPage,               STAMTYPE_PROFILE, "/PGM/HC/GstModifyPage",              STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMGstModifyPage() body.");
    STAM_REG(pVM, &pPGM->StatGCGstModifyPage,               STAMTYPE_PROFILE, "/PGM/GC/GstModifyPage",              STAMUNIT_TICKS_PER_CALL, "Profiling of the PGMGstModifyPage() body.");

    STAM_REG(pVM, &pPGM->StatSynPT4kGC,                     STAMTYPE_COUNTER, "/PGM/GC/SyncPT/4k",                  STAMUNIT_OCCURENCES,     "Nr of 4k PT syncs");
    STAM_REG(pVM, &pPGM->StatSynPT4kHC,                     STAMTYPE_COUNTER, "/PGM/HC/SyncPT/4k",                  STAMUNIT_OCCURENCES,     "Nr of 4k PT syncs");
    STAM_REG(pVM, &pPGM->StatSynPT4MGC,                     STAMTYPE_COUNTER, "/PGM/GC/SyncPT/4M",                  STAMUNIT_OCCURENCES,     "Nr of 4M PT syncs");
    STAM_REG(pVM, &pPGM->StatSynPT4MHC,                     STAMTYPE_COUNTER, "/PGM/HC/SyncPT/4M",                  STAMUNIT_OCCURENCES,     "Nr of 4M PT syncs");

    STAM_REG(pVM, &pPGM->StatDynRamTotal,                   STAMTYPE_COUNTER, "/PGM/RAM/TotalAlloc",                STAMUNIT_MEGABYTES,      "Allocated mbs of guest ram.");
    STAM_REG(pVM, &pPGM->StatDynRamGrow,                    STAMTYPE_COUNTER, "/PGM/RAM/Grow",                      STAMUNIT_OCCURENCES,     "Nr of pgmr3PhysGrowRange calls.");

    STAM_REG(pVM, &pPGM->StatPageHCMapTlbHits,              STAMTYPE_COUNTER, "/PGM/PageHCMap/TlbHits",                 STAMUNIT_OCCURENCES, "TLB hits.");
    STAM_REG(pVM, &pPGM->StatPageHCMapTlbMisses,            STAMTYPE_COUNTER, "/PGM/PageHCMap/TlbMisses",               STAMUNIT_OCCURENCES, "TLB misses.");
    STAM_REG(pVM, &pPGM->ChunkR3Map.c,                      STAMTYPE_U32,     "/PGM/ChunkR3Map/c",                      STAMUNIT_OCCURENCES, "Number of mapped chunks.");
    STAM_REG(pVM, &pPGM->ChunkR3Map.cMax,                   STAMTYPE_U32,     "/PGM/ChunkR3Map/cMax",                   STAMUNIT_OCCURENCES, "Maximum number of mapped chunks.");
    STAM_REG(pVM, &pPGM->StatChunkR3MapTlbHits,             STAMTYPE_COUNTER, "/PGM/ChunkR3Map/TlbHits",                STAMUNIT_OCCURENCES, "TLB hits.");
    STAM_REG(pVM, &pPGM->StatChunkR3MapTlbMisses,           STAMTYPE_COUNTER, "/PGM/ChunkR3Map/TlbMisses",              STAMUNIT_OCCURENCES, "TLB misses.");
    STAM_REG(pVM, &pPGM->StatPageReplaceShared,             STAMTYPE_COUNTER, "/PGM/Page/ReplacedShared",               STAMUNIT_OCCURENCES, "Times a shared page was replaced.");
    STAM_REG(pVM, &pPGM->StatPageReplaceZero,               STAMTYPE_COUNTER, "/PGM/Page/ReplacedZero",                 STAMUNIT_OCCURENCES, "Times the zero page was replaced.");
    STAM_REG(pVM, &pPGM->StatPageHandyAllocs,               STAMTYPE_COUNTER, "/PGM/Page/HandyAllocs",                  STAMUNIT_OCCURENCES, "Number of times we've allocated more handy pages.");
    STAM_REG(pVM, &pPGM->cAllPages,                         STAMTYPE_U32,     "/PGM/Page/cAllPages",                    STAMUNIT_OCCURENCES, "The total number of pages.");
    STAM_REG(pVM, &pPGM->cPrivatePages,                     STAMTYPE_U32,     "/PGM/Page/cPrivatePages",                STAMUNIT_OCCURENCES, "The number of private pages.");
    STAM_REG(pVM, &pPGM->cSharedPages,                      STAMTYPE_U32,     "/PGM/Page/cSharedPages",                 STAMUNIT_OCCURENCES, "The number of shared pages.");
    STAM_REG(pVM, &pPGM->cZeroPages,                        STAMTYPE_U32,     "/PGM/Page/cZeroPages",                   STAMUNIT_OCCURENCES, "The number of zero backed pages.");

#ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    STAM_REG(pVM, &pPGM->StatTrackVirgin,                   STAMTYPE_COUNTER, "/PGM/Track/Virgin",                  STAMUNIT_OCCURENCES,     "The number of first time shadowings");
    STAM_REG(pVM, &pPGM->StatTrackAliased,                  STAMTYPE_COUNTER, "/PGM/Track/Aliased",                 STAMUNIT_OCCURENCES,     "The number of times switching to cRef2, i.e. the page is being shadowed by two PTs.");
    STAM_REG(pVM, &pPGM->StatTrackAliasedMany,              STAMTYPE_COUNTER, "/PGM/Track/AliasedMany",             STAMUNIT_OCCURENCES,     "The number of times we're tracking using cRef2.");
    STAM_REG(pVM, &pPGM->StatTrackAliasedLots,              STAMTYPE_COUNTER, "/PGM/Track/AliasedLots",             STAMUNIT_OCCURENCES,     "The number of times we're hitting pages which has overflowed cRef2");
    STAM_REG(pVM, &pPGM->StatTrackOverflows,                STAMTYPE_COUNTER, "/PGM/Track/Overflows",               STAMUNIT_OCCURENCES,     "The number of times the extent list grows to long.");
    STAM_REG(pVM, &pPGM->StatTrackDeref,                    STAMTYPE_PROFILE, "/PGM/Track/Deref",                   STAMUNIT_OCCURENCES,     "Profiling of SyncPageWorkerTrackDeref (expensive).");
#endif

    for (unsigned i = 0; i < PAGE_ENTRIES; i++)
    {
        /** @todo r=bird: We need a STAMR3RegisterF()! */
        char szName[32];

        RTStrPrintf(szName, sizeof(szName), "/PGM/GC/PD/Trap0e/%04X", i);
        int rc = STAMR3Register(pVM, &pPGM->StatGCTrap0ePD[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, "The number of traps in page directory n.");
        AssertRC(rc);

        RTStrPrintf(szName, sizeof(szName), "/PGM/GC/PD/SyncPt/%04X", i);
        rc = STAMR3Register(pVM, &pPGM->StatGCSyncPtPD[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, "The number of syncs per PD n.");
        AssertRC(rc);

        RTStrPrintf(szName, sizeof(szName), "/PGM/GC/PD/SyncPage/%04X", i);
        rc = STAMR3Register(pVM, &pPGM->StatGCSyncPagePD[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, "The number of out of sync pages per page directory n.");
        AssertRC(rc);
    }
}
#endif /* VBOX_WITH_STATISTICS */

/**
 * Init the PGM bits that rely on VMMR0 and MM to be fully initialized.
 *
 * The dynamic mapping area will also be allocated and initialized at this
 * time. We could allocate it during PGMR3Init of course, but the mapping
 * wouldn't be allocated at that time preventing us from setting up the
 * page table entries with the dummy page.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 */
PGMR3DECL(int) PGMR3InitDynMap(PVM pVM)
{
    /*
     * Reserve space for mapping the paging pages into guest context.
     */
    int rc = MMR3HyperReserve(pVM, PAGE_SIZE * (2 + ELEMENTS(pVM->pgm.s.apHCPaePDs) + 1 + 2 + 2), "Paging", &pVM->pgm.s.pGC32BitPD);
    AssertRCReturn(rc, rc);
    MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);

    /*
     * Reserve space for the dynamic mappings.
     */
    /** @todo r=bird: Need to verify that the checks for crossing PTs are correct here. They seems to be assuming 4MB PTs.. */
    rc = MMR3HyperReserve(pVM, MM_HYPER_DYNAMIC_SIZE, "Dynamic mapping", &pVM->pgm.s.pbDynPageMapBaseGC);
    if (    VBOX_SUCCESS(rc)
        &&  (pVM->pgm.s.pbDynPageMapBaseGC >> PGDIR_SHIFT) != ((pVM->pgm.s.pbDynPageMapBaseGC + MM_HYPER_DYNAMIC_SIZE - 1) >> PGDIR_SHIFT))
        rc = MMR3HyperReserve(pVM, MM_HYPER_DYNAMIC_SIZE, "Dynamic mapping not crossing", &pVM->pgm.s.pbDynPageMapBaseGC);
    if (VBOX_SUCCESS(rc))
    {
        AssertRelease((pVM->pgm.s.pbDynPageMapBaseGC >> PGDIR_SHIFT) == ((pVM->pgm.s.pbDynPageMapBaseGC + MM_HYPER_DYNAMIC_SIZE - 1) >> PGDIR_SHIFT));
        MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);
    }
    return rc;
}


/**
 * Ring-3 init finalizing.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
PGMR3DECL(int) PGMR3InitFinalize(PVM pVM)
{
    /*
     * Map the paging pages into the guest context.
     */
    RTGCPTR GCPtr = pVM->pgm.s.pGC32BitPD;
    AssertReleaseReturn(GCPtr, VERR_INTERNAL_ERROR);

    int rc = PGMMap(pVM, GCPtr, pVM->pgm.s.HCPhys32BitPD, PAGE_SIZE, 0);
    AssertRCReturn(rc, rc);
    pVM->pgm.s.pGC32BitPD = GCPtr;
    GCPtr += PAGE_SIZE;
    GCPtr += PAGE_SIZE; /* reserved page */

    for (unsigned i = 0; i < ELEMENTS(pVM->pgm.s.apHCPaePDs); i++)
    {
        rc = PGMMap(pVM, GCPtr, pVM->pgm.s.aHCPhysPaePDs[i], PAGE_SIZE, 0);
        AssertRCReturn(rc, rc);
        pVM->pgm.s.apGCPaePDs[i] = GCPtr;
        GCPtr += PAGE_SIZE;
    }
    /* A bit of paranoia is justified. */
    AssertRelease((RTGCUINTPTR)pVM->pgm.s.apGCPaePDs[0] + PAGE_SIZE == (RTGCUINTPTR)pVM->pgm.s.apGCPaePDs[1]);
    AssertRelease((RTGCUINTPTR)pVM->pgm.s.apGCPaePDs[1] + PAGE_SIZE == (RTGCUINTPTR)pVM->pgm.s.apGCPaePDs[2]);
    AssertRelease((RTGCUINTPTR)pVM->pgm.s.apGCPaePDs[2] + PAGE_SIZE == (RTGCUINTPTR)pVM->pgm.s.apGCPaePDs[3]);
    GCPtr += PAGE_SIZE; /* reserved page */

    rc = PGMMap(pVM, GCPtr, pVM->pgm.s.HCPhysPaePDPTR, PAGE_SIZE, 0);
    AssertRCReturn(rc, rc);
    pVM->pgm.s.pGCPaePDPTR = GCPtr;
    GCPtr += PAGE_SIZE;
    GCPtr += PAGE_SIZE; /* reserved page */

    rc = PGMMap(pVM, GCPtr, pVM->pgm.s.HCPhysPaePML4, PAGE_SIZE, 0);
    AssertRCReturn(rc, rc);
    pVM->pgm.s.pGCPaePML4 = GCPtr;
    GCPtr += PAGE_SIZE;
    GCPtr += PAGE_SIZE; /* reserved page */


    /*
     * Reserve space for the dynamic mappings.
     * Initialize the dynamic mapping pages with dummy pages to simply the cache.
     */
    /* get the pointer to the page table entries. */
    PPGMMAPPING pMapping = pgmGetMapping(pVM, pVM->pgm.s.pbDynPageMapBaseGC);
    AssertRelease(pMapping);
    const uintptr_t off = pVM->pgm.s.pbDynPageMapBaseGC - pMapping->GCPtr;
    const unsigned iPT = off >> X86_PD_SHIFT;
    const unsigned iPG = (off >> X86_PT_SHIFT) & X86_PT_MASK;
    pVM->pgm.s.paDynPageMap32BitPTEsGC = pMapping->aPTs[iPT].pPTGC      + iPG * sizeof(pMapping->aPTs[0].pPTR3->a[0]);
    pVM->pgm.s.paDynPageMapPaePTEsGC   = pMapping->aPTs[iPT].paPaePTsGC + iPG * sizeof(pMapping->aPTs[0].paPaePTsR3->a[0]);

    /* init cache */
    RTHCPHYS HCPhysDummy = MMR3PageDummyHCPhys(pVM);
    for (unsigned i = 0; i < ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache); i++)
        pVM->pgm.s.aHCPhysDynPageMapCache[i] = HCPhysDummy;

    for (unsigned i = 0; i < MM_HYPER_DYNAMIC_SIZE; i += PAGE_SIZE)
    {
        rc = PGMMap(pVM, pVM->pgm.s.pbDynPageMapBaseGC + i, HCPhysDummy, PAGE_SIZE, 0);
        AssertRCReturn(rc, rc);
    }

    return rc;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
PGMR3DECL(void) PGMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("PGMR3Relocate\n"));

    /*
     * Paging stuff.
     */
    pVM->pgm.s.GCPtrCR3Mapping += offDelta;
    /** @todo move this into shadow and guest specific relocation functions. */
    AssertMsg(pVM->pgm.s.pGC32BitPD, ("Init order, no relocation before paging is initialized!\n"));
    pVM->pgm.s.pGC32BitPD += offDelta;
    pVM->pgm.s.pGuestPDGC += offDelta;
    for (unsigned i = 0; i < ELEMENTS(pVM->pgm.s.apGCPaePDs); i++)
        pVM->pgm.s.apGCPaePDs[i] += offDelta;
    pVM->pgm.s.pGCPaePDPTR += offDelta;
    pVM->pgm.s.pGCPaePML4 += offDelta;

    pgmR3ModeDataInit(pVM, true /* resolve GC/R0 symbols */);
    pgmR3ModeDataSwitch(pVM, pVM->pgm.s.enmShadowMode, pVM->pgm.s.enmGuestMode);

    PGM_SHW_PFN(Relocate, pVM)(pVM, offDelta);
    PGM_GST_PFN(Relocate, pVM)(pVM, offDelta);
    PGM_BTH_PFN(Relocate, pVM)(pVM, offDelta);

    /*
     * Trees.
     */
    pVM->pgm.s.pTreesGC = MMHyperHC2GC(pVM, pVM->pgm.s.pTreesHC);

    /*
     * Ram ranges.
     */
    if (pVM->pgm.s.pRamRangesHC)
    {
        pVM->pgm.s.pRamRangesGC = MMHyperHC2GC(pVM, pVM->pgm.s.pRamRangesHC);
        for (PPGMRAMRANGE pCur = pVM->pgm.s.pRamRangesHC; pCur->pNextHC; pCur = pCur->pNextHC)
        {
            pCur->pNextGC = MMHyperHC2GC(pVM, pCur->pNextHC);
            if (pCur->pavHCChunkGC)
                pCur->pavHCChunkGC = MMHyperHC2GC(pVM, pCur->pavHCChunkHC);
        }
    }

    /*
     * Update the two page directories with all page table mappings.
     * (One or more of them have changed, that's why we're here.)
     */
    pVM->pgm.s.pMappingsGC = MMHyperHC2GC(pVM, pVM->pgm.s.pMappingsR3);
    for (PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3; pCur->pNextR3; pCur = pCur->pNextR3)
        pCur->pNextGC = MMHyperHC2GC(pVM, pCur->pNextR3);

    /* Relocate GC addresses of Page Tables. */
    for (PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3; pCur; pCur = pCur->pNextR3)
    {
        for (RTHCUINT i = 0; i < pCur->cPTs; i++)
        {
            pCur->aPTs[i].pPTGC = MMHyperR3ToGC(pVM, pCur->aPTs[i].pPTR3);
            pCur->aPTs[i].paPaePTsGC = MMHyperR3ToGC(pVM, pCur->aPTs[i].paPaePTsR3);
        }
    }

    /*
     * Dynamic page mapping area.
     */
    pVM->pgm.s.paDynPageMap32BitPTEsGC += offDelta;
    pVM->pgm.s.paDynPageMapPaePTEsGC += offDelta;
    pVM->pgm.s.pbDynPageMapBaseGC += offDelta;

    /*
     * The Zero page.
     */
    pVM->pgm.s.pvZeroPgR0 = MMHyperR3ToR0(pVM, pVM->pgm.s.pvZeroPgR3);
    AssertRelease(pVM->pgm.s.pvZeroPgR0);

    /*
     * Physical and virtual handlers.
     */
    RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesHC->PhysHandlers, true, pgmR3RelocatePhysHandler, &offDelta);
    RTAvlroGCPtrDoWithAll(&pVM->pgm.s.pTreesHC->VirtHandlers, true, pgmR3RelocateVirtHandler, &offDelta);

    /*
     * The page pool.
     */
    pgmR3PoolRelocate(pVM);
}


/**
 * Callback function for relocating a physical access handler.
 *
 * @returns 0 (continue enum)
 * @param   pNode       Pointer to a PGMPHYSHANDLER node.
 * @param   pvUser      Pointer to the offDelta. This is a pointer to the delta since we're
 *                      not certain the delta will fit in a void pointer for all possible configs.
 */
static DECLCALLBACK(int) pgmR3RelocatePhysHandler(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PPGMPHYSHANDLER pHandler = (PPGMPHYSHANDLER)pNode;
    RTGCINTPTR      offDelta = *(PRTGCINTPTR)pvUser;
    if (pHandler->pfnHandlerGC)
        pHandler->pfnHandlerGC += offDelta;
    if ((RTGCUINTPTR)pHandler->pvUserGC >= 0x10000)
        pHandler->pvUserGC += offDelta;
    return 0;
}


/**
 * Callback function for relocating a virtual access handler.
 *
 * @returns 0 (continue enum)
 * @param   pNode       Pointer to a PGMVIRTHANDLER node.
 * @param   pvUser      Pointer to the offDelta. This is a pointer to the delta since we're
 *                      not certain the delta will fit in a void pointer for all possible configs.
 */
static DECLCALLBACK(int) pgmR3RelocateVirtHandler(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMVIRTHANDLER pHandler = (PPGMVIRTHANDLER)pNode;
    RTGCINTPTR      offDelta = *(PRTGCINTPTR)pvUser;
    Assert(pHandler->pfnHandlerGC);
    pHandler->pfnHandlerGC  += offDelta;
    return 0;
}


/**
 * The VM is being reset.
 *
 * For the PGM component this means that any PD write monitors
 * needs to be removed.
 *
 * @param   pVM     VM handle.
 */
PGMR3DECL(void) PGMR3Reset(PVM pVM)
{
    LogFlow(("PGMR3Reset:\n"));
    VM_ASSERT_EMT(pVM);

    /*
     * Unfix any fixed mappings and disable CR3 monitoring.
     */
    pVM->pgm.s.fMappingsFixed    = false;
    pVM->pgm.s.GCPtrMappingFixed = 0;
    pVM->pgm.s.cbMappingFixed    = 0;

    int rc = PGM_GST_PFN(UnmonitorCR3, pVM)(pVM);
    AssertRC(rc);
#ifdef DEBUG
    PGMR3DumpMappings(pVM);
#endif

    /*
     * Reset the shadow page pool.
     */
    pgmR3PoolReset(pVM);

    /*
     * Re-init other members.
     */
    pVM->pgm.s.fA20Enabled = true;

    /*
     * Clear the FFs PGM owns.
     */
    VM_FF_CLEAR(pVM, VM_FF_PGM_SYNC_CR3);
    VM_FF_CLEAR(pVM, VM_FF_PGM_SYNC_CR3_NON_GLOBAL);

    /*
     * Zero memory.
     */
    for (PPGMRAMRANGE pRam = pVM->pgm.s.pRamRangesHC; pRam; pRam = pRam->pNextHC)
    {
        unsigned iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
        {
            if (pRam->aPages[iPage].HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2)) /** @todo PAGE FLAGS */
            {
                /* shadow ram is reloaded elsewhere. */
                Log4(("PGMR3Reset: not clearing phys page %RGp due to flags %RHp\n", pRam->GCPhys + (iPage << PAGE_SHIFT), pRam->aPages[iPage].HCPhys & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO))); /** @todo PAGE FLAGS */
                continue;
            }
            if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
            {
                unsigned iChunk = iPage >> (PGM_DYNAMIC_CHUNK_SHIFT - PAGE_SHIFT);
                if (pRam->pavHCChunkHC[iChunk])
                    ASMMemZero32((char *)pRam->pavHCChunkHC[iChunk] + ((iPage << PAGE_SHIFT) & PGM_DYNAMIC_CHUNK_OFFSET_MASK), PAGE_SIZE);
            }
            else
                ASMMemZero32((char *)pRam->pvHC + (iPage << PAGE_SHIFT), PAGE_SIZE);
        }
    }

    /*
     * Switch mode back to real mode.
     */
    rc = pgmR3ChangeMode(pVM, PGMMODE_REAL);
    AssertReleaseRC(rc);
    STAM_REL_COUNTER_RESET(&pVM->pgm.s.cGuestModeChanges);
}


/**
 * Terminates the PGM.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to VM structure.
 */
PGMR3DECL(int) PGMR3Term(PVM pVM)
{
    return PDMR3CritSectDelete(&pVM->pgm.s.CritSect);
}


#ifdef VBOX_STRICT
/**
 * VM state change callback for clearing fNoMorePhysWrites after
 * a snapshot has been created.
 */
static DECLCALLBACK(void) pgmR3ResetNoMorePhysWritesFlag(PVM pVM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser)
{
    if (enmState == VMSTATE_RUNNING)
        pVM->pgm.s.fNoMorePhysWrites = false;
}
#endif


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) pgmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    PPGM pPGM = &pVM->pgm.s;

    /* No more writes to physical memory after this point! */
    pVM->pgm.s.fNoMorePhysWrites = true;

    /*
     * Save basic data (required / unaffected by relocation).
     */
#if 1
    SSMR3PutBool(pSSM, pPGM->fMappingsFixed);
#else
    SSMR3PutUInt(pSSM, pPGM->fMappingsFixed);
#endif
    SSMR3PutGCPtr(pSSM, pPGM->GCPtrMappingFixed);
    SSMR3PutU32(pSSM, pPGM->cbMappingFixed);
    SSMR3PutUInt(pSSM, pPGM->cbRamSize);
    SSMR3PutGCPhys(pSSM, pPGM->GCPhysA20Mask);
    SSMR3PutUInt(pSSM, pPGM->fA20Enabled);
    SSMR3PutUInt(pSSM, pPGM->fSyncFlags);
    SSMR3PutUInt(pSSM, pPGM->enmGuestMode);
    SSMR3PutU32(pSSM, ~0);      /* Separator. */

    /*
     * The guest mappings.
     */
    uint32_t i = 0;
    for (PPGMMAPPING pMapping = pPGM->pMappingsR3; pMapping; pMapping = pMapping->pNextR3, i++)
    {
        SSMR3PutU32(pSSM, i);
        SSMR3PutStrZ(pSSM, pMapping->pszDesc); /* This is the best unique id we have... */
        SSMR3PutGCPtr(pSSM, pMapping->GCPtr);
        SSMR3PutGCUIntPtr(pSSM, pMapping->cPTs);
        /* flags are done by the mapping owners! */
    }
    SSMR3PutU32(pSSM, ~0); /* terminator. */

    /*
     * Ram range flags and bits.
     */
    i = 0;
    for (PPGMRAMRANGE pRam = pPGM->pRamRangesHC; pRam; pRam = pRam->pNextHC, i++)
    {
        /** @todo MMIO ranges may move (PCI reconfig), we currently assume they don't. */

        SSMR3PutU32(pSSM, i);
        SSMR3PutGCPhys(pSSM, pRam->GCPhys);
        SSMR3PutGCPhys(pSSM, pRam->GCPhysLast);
        SSMR3PutGCPhys(pSSM, pRam->cb);
        SSMR3PutU8(pSSM, !!pRam->pvHC);             /* boolean indicating memory or not. */

        /* Flags. */
        const unsigned cPages = pRam->cb >> PAGE_SHIFT;
        for (unsigned iPage = 0; iPage < cPages; iPage++)
            SSMR3PutU16(pSSM, (uint16_t)(pRam->aPages[iPage].HCPhys & ~X86_PTE_PAE_PG_MASK)); /** @todo PAGE FLAGS */

        /* any memory associated with the range. */
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                if (pRam->pavHCChunkHC[iChunk])
                {
                    SSMR3PutU8(pSSM, 1);    /* chunk present */
                    SSMR3PutMem(pSSM, pRam->pavHCChunkHC[iChunk], PGM_DYNAMIC_CHUNK_SIZE);
                }
                else
                    SSMR3PutU8(pSSM, 0);    /* no chunk present */
            }
        }
        else if (pRam->pvHC)
        {
            int rc = SSMR3PutMem(pSSM, pRam->pvHC, pRam->cb);
            if (VBOX_FAILURE(rc))
            {
                Log(("pgmR3Save: SSMR3PutMem(, %p, %#x) -> %Vrc\n", pRam->pvHC, pRam->cb, rc));
                return rc;
            }
        }
    }
    return SSMR3PutU32(pSSM, ~0); /* terminator. */
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) pgmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    /*
     * Validate version.
     */
    if (u32Version != PGM_SAVED_STATE_VERSION)
    {
        Log(("pgmR3Load: Invalid version u32Version=%d (current %d)!\n", u32Version, PGM_SAVED_STATE_VERSION));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Call the reset function to make sure all the memory is cleared.
     */
    PGMR3Reset(pVM);

    /*
     * Load basic data (required / unaffected by relocation).
     */
    PPGM pPGM = &pVM->pgm.s;
#if 1
    SSMR3GetBool(pSSM,      &pPGM->fMappingsFixed);
#else
    uint32_t u;
    SSMR3GetU32(pSSM,       &u);
    pPGM->fMappingsFixed = u;
#endif
    SSMR3GetGCPtr(pSSM,     &pPGM->GCPtrMappingFixed);
    SSMR3GetU32(pSSM,       &pPGM->cbMappingFixed);

    RTUINT cbRamSize;
    int rc = SSMR3GetU32(pSSM, &cbRamSize);
    if (VBOX_FAILURE(rc))
        return rc;
    if (cbRamSize != pPGM->cbRamSize)
        return VERR_SSM_LOAD_MEMORY_SIZE_MISMATCH;
    SSMR3GetGCPhys(pSSM,    &pPGM->GCPhysA20Mask);
    SSMR3GetUInt(pSSM,      &pPGM->fA20Enabled);
    SSMR3GetUInt(pSSM,      &pPGM->fSyncFlags);
    RTUINT uGuestMode;
    SSMR3GetUInt(pSSM,      &uGuestMode);
    pPGM->enmGuestMode = (PGMMODE)uGuestMode;

    /* check separator. */
    uint32_t u32Sep;
    SSMR3GetU32(pSSM, &u32Sep);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32Sep != (uint32_t)~0)
    {
        AssertMsgFailed(("u32Sep=%#x (first)\n", u32Sep));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }

    /*
     * The guest mappings.
     */
    uint32_t i = 0;
    for (;; i++)
    {
        /* Check the seqence number / separator. */
        rc = SSMR3GetU32(pSSM, &u32Sep);
        if (VBOX_FAILURE(rc))
            return rc;
        if (u32Sep == ~0U)
            break;
        if (u32Sep != i)
        {
            AssertMsgFailed(("u32Sep=%#x (last)\n", u32Sep));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }

        /* get the mapping details. */
        char szDesc[256];
        szDesc[0] = '\0';
        rc = SSMR3GetStrZ(pSSM, szDesc, sizeof(szDesc));
        if (VBOX_FAILURE(rc))
            return rc;
        RTGCPTR GCPtr;
        SSMR3GetGCPtr(pSSM,     &GCPtr);
        RTGCUINTPTR cPTs;
        rc = SSMR3GetU32(pSSM,  &cPTs);
        if (VBOX_FAILURE(rc))
            return rc;

        /* find matching range. */
        PPGMMAPPING pMapping;
        for (pMapping = pPGM->pMappingsR3; pMapping; pMapping = pMapping->pNextR3)
            if (    pMapping->cPTs == cPTs
                &&  !strcmp(pMapping->pszDesc, szDesc))
                break;
        if (!pMapping)
        {
            LogRel(("Couldn't find mapping: cPTs=%#x szDesc=%s (GCPtr=%VGv)\n",
                    cPTs, szDesc, GCPtr));
            AssertFailed();
            return VERR_SSM_LOAD_CONFIG_MISMATCH;
        }

        /* relocate it. */
        if (pMapping->GCPtr != GCPtr)
        {
            AssertMsg((GCPtr >> PGDIR_SHIFT << PGDIR_SHIFT) == GCPtr, ("GCPtr=%VGv\n", GCPtr));
#if HC_ARCH_BITS == 64
LogRel(("Mapping: %VGv -> %VGv %s\n", pMapping->GCPtr, GCPtr, pMapping->pszDesc));
#endif
            pgmR3MapRelocate(pVM, pMapping, pMapping->GCPtr >> PGDIR_SHIFT, GCPtr >> PGDIR_SHIFT);
        }
        else
            Log(("pgmR3Load: '%s' needed no relocation (%VGv)\n", szDesc, GCPtr));
    }

    /*
     * Ram range flags and bits.
     */
    i = 0;
    for (PPGMRAMRANGE pRam = pPGM->pRamRangesHC; pRam; pRam = pRam->pNextHC, i++)
    {
        /** @todo MMIO ranges may move (PCI reconfig), we currently assume they don't. */
        /* Check the seqence number / separator. */
        rc = SSMR3GetU32(pSSM, &u32Sep);
        if (VBOX_FAILURE(rc))
            return rc;
        if (u32Sep == ~0U)
            break;
        if (u32Sep != i)
        {
            AssertMsgFailed(("u32Sep=%#x (last)\n", u32Sep));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }

        /* Get the range details. */
        RTGCPHYS GCPhys;
        SSMR3GetGCPhys(pSSM, &GCPhys);
        RTGCPHYS GCPhysLast;
        SSMR3GetGCPhys(pSSM, &GCPhysLast);
        RTGCPHYS cb;
        SSMR3GetGCPhys(pSSM, &cb);
        uint8_t     fHaveBits;
        rc = SSMR3GetU8(pSSM, &fHaveBits);
        if (VBOX_FAILURE(rc))
            return rc;
        if (fHaveBits & ~1)
        {
            AssertMsgFailed(("u32Sep=%#x (last)\n", u32Sep));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }

        /* Match it up with the current range. */
        if (    GCPhys != pRam->GCPhys
            ||  GCPhysLast != pRam->GCPhysLast
            ||  cb != pRam->cb
            ||  fHaveBits != !!pRam->pvHC)
        {
            LogRel(("Ram range: %VGp-%VGp %VGp bytes %s\n"
                    "State    : %VGp-%VGp %VGp bytes %s\n",
                    pRam->GCPhys, pRam->GCPhysLast, pRam->cb, pRam->pvHC ? "bits" : "nobits",
                    GCPhys, GCPhysLast, cb, fHaveBits ? "bits" : "nobits"));
            /*
             * If we're loading a state for debugging purpose, don't make a fuss if
             * the MMIO[2] and ROM stuff isn't 100% right, just skip the mismatches.
             */
            if (    SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT
                ||  GCPhys < 8 * _1M)
                AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);

            RTGCPHYS cPages = ((GCPhysLast - GCPhys) + 1) >> PAGE_SHIFT;
            while (cPages-- > 0)
            {
                uint16_t u16Ignore;
                SSMR3GetU16(pSSM, &u16Ignore);
            }
            continue;
        }

        /* Flags. */
        const unsigned cPages = pRam->cb >> PAGE_SHIFT;
        for (unsigned iPage = 0; iPage < cPages; iPage++)
        {
            uint16_t    u16 = 0;
            SSMR3GetU16(pSSM, &u16);
            u16 &= PAGE_OFFSET_MASK & ~( MM_RAM_FLAGS_VIRTUAL_HANDLER | MM_RAM_FLAGS_VIRTUAL_WRITE | MM_RAM_FLAGS_VIRTUAL_ALL
                                       | MM_RAM_FLAGS_PHYSICAL_HANDLER | MM_RAM_FLAGS_PHYSICAL_WRITE | MM_RAM_FLAGS_PHYSICAL_ALL
                                       | MM_RAM_FLAGS_PHYSICAL_TEMP_OFF );
            pRam->aPages[iPage].HCPhys = PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) | (RTHCPHYS)u16; /** @todo PAGE FLAGS */
        }

        /* any memory associated with the range. */
        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            for (unsigned iChunk = 0; iChunk < (pRam->cb >> PGM_DYNAMIC_CHUNK_SHIFT); iChunk++)
            {
                uint8_t fValidChunk;

                rc = SSMR3GetU8(pSSM, &fValidChunk);
                if (VBOX_FAILURE(rc))
                    return rc;
                if (fValidChunk > 1)
                    return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;

                if (fValidChunk)
                {
                    if (!pRam->pavHCChunkHC[iChunk])
                    {
                        rc = pgmr3PhysGrowRange(pVM, pRam->GCPhys + iChunk * PGM_DYNAMIC_CHUNK_SIZE);
                        if (VBOX_FAILURE(rc))
                            return rc;
                    }
                    Assert(pRam->pavHCChunkHC[iChunk]);

                    SSMR3GetMem(pSSM, pRam->pavHCChunkHC[iChunk], PGM_DYNAMIC_CHUNK_SIZE);
                }
                /* else nothing to do */
            }
        }
        else if (pRam->pvHC)
        {
            int rc = SSMR3GetMem(pSSM, pRam->pvHC, pRam->cb);
            if (VBOX_FAILURE(rc))
            {
                Log(("pgmR3Save: SSMR3GetMem(, %p, %#x) -> %Vrc\n", pRam->pvHC, pRam->cb, rc));
                return rc;
            }
        }
    }

    /*
     * We require a full resync now.
     */
    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3_NON_GLOBAL);
    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
    pPGM->fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
    pPGM->fPhysCacheFlushPending = true;
    pgmR3HandlerPhysicalUpdateAll(pVM);

    /*
     * Change the paging mode.
     */
    return pgmR3ChangeMode(pVM, pPGM->enmGuestMode);
}


/**
 * Show paging mode.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     "all" (default), "guest", "shadow" or "host".
 */
static DECLCALLBACK(void) pgmR3InfoMode(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /* digest argument. */
    bool fGuest, fShadow, fHost;
    if (pszArgs)
        pszArgs = RTStrStripL(pszArgs);
    if (!pszArgs || !*pszArgs || strstr(pszArgs, "all"))
        fShadow = fHost = fGuest = true;
    else
    {
        fShadow = fHost = fGuest = false;
        if (strstr(pszArgs, "guest"))
            fGuest = true;
        if (strstr(pszArgs, "shadow"))
            fShadow = true;
        if (strstr(pszArgs, "host"))
            fHost = true;
    }

    /* print info. */
    if (fGuest)
        pHlp->pfnPrintf(pHlp, "Guest paging mode:  %s, changed %RU64 times, A20 %s\n",
                        PGMGetModeName(pVM->pgm.s.enmGuestMode), pVM->pgm.s.cGuestModeChanges.c,
                        pVM->pgm.s.fA20Enabled ? "enabled" : "disabled");
    if (fShadow)
        pHlp->pfnPrintf(pHlp, "Shadow paging mode: %s\n", PGMGetModeName(pVM->pgm.s.enmShadowMode));
    if (fHost)
    {
        const char *psz;
        switch (pVM->pgm.s.enmHostMode)
        {
            case SUPPAGINGMODE_INVALID:             psz = "invalid"; break;
            case SUPPAGINGMODE_32_BIT:              psz = "32-bit"; break;
            case SUPPAGINGMODE_32_BIT_GLOBAL:       psz = "32-bit+G"; break;
            case SUPPAGINGMODE_PAE:                 psz = "PAE"; break;
            case SUPPAGINGMODE_PAE_GLOBAL:          psz = "PAE+G"; break;
            case SUPPAGINGMODE_PAE_NX:              psz = "PAE+NX"; break;
            case SUPPAGINGMODE_PAE_GLOBAL_NX:       psz = "PAE+G+NX"; break;
            case SUPPAGINGMODE_AMD64:               psz = "AMD64"; break;
            case SUPPAGINGMODE_AMD64_GLOBAL:        psz = "AMD64+G"; break;
            case SUPPAGINGMODE_AMD64_NX:            psz = "AMD64+NX"; break;
            case SUPPAGINGMODE_AMD64_GLOBAL_NX:     psz = "AMD64+G+NX"; break;
            default:                                psz = "unknown"; break;
        }
        pHlp->pfnPrintf(pHlp, "Host paging mode:   %s\n", psz);
    }
}


/**
 * Dump registered MMIO ranges to the log.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) pgmR3PhysInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    pHlp->pfnPrintf(pHlp,
                    "RAM ranges (pVM=%p)\n"
                    "%.*s %.*s\n",
                    pVM,
                    sizeof(RTGCPHYS) * 4 + 1, "GC Phys Range                    ",
                    sizeof(RTHCPTR) * 2,      "pvHC            ");

    for (PPGMRAMRANGE pCur = pVM->pgm.s.pRamRangesHC; pCur; pCur = pCur->pNextHC)
        pHlp->pfnPrintf(pHlp,
                        "%VGp-%VGp %VHv\n",
                        pCur->GCPhys,
                        pCur->GCPhysLast,
                        pCur->pvHC);
}

/**
 * Dump the page directory to the log.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) pgmR3InfoCr3(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
/** @todo fix this! Convert the PGMR3DumpHierarchyHC functions to do guest stuff. */
    /* Big pages supported? */
    const bool  fPSE = !!(CPUMGetGuestCR4(pVM) & X86_CR4_PSE);
    /* Global pages supported? */
    const bool  fPGE = !!(CPUMGetGuestCR4(pVM) & X86_CR4_PGE);

    NOREF(pszArgs);

    /*
     * Get page directory addresses.
     */
    PVBOXPD     pPDSrc = pVM->pgm.s.pGuestPDHC;
    Assert(pPDSrc);
    Assert(MMPhysGCPhys2HCVirt(pVM, (RTGCPHYS)(CPUMGetGuestCR3(pVM) & X86_CR3_PAGE_MASK), sizeof(*pPDSrc)) == pPDSrc);

    /*
     * Iterate the page directory.
     */
    for (unsigned iPD = 0; iPD < ELEMENTS(pPDSrc->a); iPD++)
    {
        VBOXPDE PdeSrc = pPDSrc->a[iPD];
        if (PdeSrc.n.u1Present)
        {
            if (PdeSrc.b.u1Size && fPSE)
            {
                pHlp->pfnPrintf(pHlp,
                                "%04X - %VGp P=%d U=%d RW=%d G=%d - BIG\n",
                                iPD,
                                PdeSrc.u & X86_PDE_PG_MASK,
                                PdeSrc.b.u1Present, PdeSrc.b.u1User, PdeSrc.b.u1Write, PdeSrc.b.u1Global && fPGE);
            }
            else
            {
                pHlp->pfnPrintf(pHlp,
                                "%04X - %VGp P=%d U=%d RW=%d [G=%d]\n",
                                iPD,
                                PdeSrc.u & X86_PDE4M_PG_MASK,
                                PdeSrc.n.u1Present, PdeSrc.n.u1User, PdeSrc.n.u1Write, PdeSrc.b.u1Global && fPGE);
            }
        }
    }
}


/**
 * Serivce a VMMCALLHOST_PGM_LOCK call.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
PDMR3DECL(int) PGMR3LockCall(PVM pVM)
{
    return pgmLock(pVM);
}


/**
 * Converts a PGMMODE value to a PGM_TYPE_* \#define.
 *
 * @returns PGM_TYPE_*.
 * @param   pgmMode     The mode value to convert.
 */
DECLINLINE(unsigned) pgmModeToType(PGMMODE pgmMode)
{
    switch (pgmMode)
    {
        case PGMMODE_REAL:      return PGM_TYPE_REAL;
        case PGMMODE_PROTECTED: return PGM_TYPE_PROT;
        case PGMMODE_32_BIT:    return PGM_TYPE_32BIT;
        case PGMMODE_PAE:
        case PGMMODE_PAE_NX:    return PGM_TYPE_PAE;
        case PGMMODE_AMD64:
        case PGMMODE_AMD64_NX:  return PGM_TYPE_AMD64;
        default:
            AssertFatalMsgFailed(("pgmMode=%d\n", pgmMode));
    }
}


/**
 * Gets the index into the paging mode data array of a SHW+GST mode.
 *
 * @returns PGM::paPagingData index.
 * @param   uShwType      The shadow paging mode type.
 * @param   uGstType      The guest paging mode type.
 */
DECLINLINE(unsigned) pgmModeDataIndex(unsigned uShwType, unsigned uGstType)
{
    Assert(uShwType >= PGM_TYPE_32BIT && uShwType <= PGM_TYPE_AMD64);
    Assert(uGstType >= PGM_TYPE_REAL  && uGstType <= PGM_TYPE_AMD64);
    return (uShwType - PGM_TYPE_32BIT) * (PGM_TYPE_AMD64 - PGM_TYPE_32BIT + 1)
         + (uGstType - PGM_TYPE_REAL);
}


/**
 * Gets the index into the paging mode data array of a SHW+GST mode.
 *
 * @returns PGM::paPagingData index.
 * @param   enmShw      The shadow paging mode.
 * @param   enmGst      The guest paging mode.
 */
DECLINLINE(unsigned) pgmModeDataIndexByMode(PGMMODE enmShw, PGMMODE enmGst)
{
    Assert(enmShw >= PGMMODE_32_BIT && enmShw <= PGMMODE_MAX);
    Assert(enmGst > PGMMODE_INVALID && enmGst < PGMMODE_MAX);
    return pgmModeDataIndex(pgmModeToType(enmShw), pgmModeToType(enmGst));
}


/**
 * Calculates the max data index.
 * @returns The number of entries in the pagaing data array.
 */
DECLINLINE(unsigned) pgmModeDataMaxIndex(void)
{
    return pgmModeDataIndex(PGM_TYPE_AMD64, PGM_TYPE_AMD64) + 1;
}


/**
 * Initializes the paging mode data kept in PGM::paModeData.
 *
 * @param   pVM             The VM handle.
 * @param   fResolveGCAndR0 Indicate whether or not GC and Ring-0 symbols can be resolved now.
 *                          This is used early in the init process to avoid trouble with PDM
 *                          not being initialized yet.
 */
static int pgmR3ModeDataInit(PVM pVM, bool fResolveGCAndR0)
{
    PPGMMODEDATA pModeData;
    int rc;

    /*
     * Allocate the array on the first call.
     */
    if (!pVM->pgm.s.paModeData)
    {
        pVM->pgm.s.paModeData = (PPGMMODEDATA)MMR3HeapAllocZ(pVM, MM_TAG_PGM, sizeof(PGMMODEDATA) * pgmModeDataMaxIndex());
        AssertReturn(pVM->pgm.s.paModeData, VERR_NO_MEMORY);
    }

    /*
     * Initialize the array entries.
     */
    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_32BIT, PGM_TYPE_REAL)];
    pModeData->uShwType = PGM_TYPE_32BIT;
    pModeData->uGstType = PGM_TYPE_REAL;
    rc = PGM_SHW_NAME_32BIT(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_REAL(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_32BIT_REAL(InitData)( pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_32BIT, PGMMODE_PROTECTED)];
    pModeData->uShwType = PGM_TYPE_32BIT;
    pModeData->uGstType = PGM_TYPE_PROT;
    rc = PGM_SHW_NAME_32BIT(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_PROT(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_32BIT_PROT(InitData)( pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_32BIT, PGM_TYPE_32BIT)];
    pModeData->uShwType = PGM_TYPE_32BIT;
    pModeData->uGstType = PGM_TYPE_32BIT;
    rc = PGM_SHW_NAME_32BIT(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_32BIT(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_32BIT_32BIT(InitData)(pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_PAE, PGM_TYPE_REAL)];
    pModeData->uShwType = PGM_TYPE_PAE;
    pModeData->uGstType = PGM_TYPE_REAL;
    rc = PGM_SHW_NAME_PAE(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_REAL(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_PAE_REAL(InitData)(   pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_PAE, PGM_TYPE_PROT)];
    pModeData->uShwType = PGM_TYPE_PAE;
    pModeData->uGstType = PGM_TYPE_PROT;
    rc = PGM_SHW_NAME_PAE(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_PROT(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_PAE_PROT(InitData)(   pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_PAE, PGM_TYPE_32BIT)];
    pModeData->uShwType = PGM_TYPE_PAE;
    pModeData->uGstType = PGM_TYPE_32BIT;
    rc = PGM_SHW_NAME_PAE(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_32BIT(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_PAE_32BIT(InitData)(  pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_PAE, PGM_TYPE_PAE)];
    pModeData->uShwType = PGM_TYPE_PAE;
    pModeData->uGstType = PGM_TYPE_PAE;
    rc = PGM_SHW_NAME_PAE(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_PAE(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_PAE_PAE(InitData)(    pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_AMD64, PGM_TYPE_REAL)];
    pModeData->uShwType = PGM_TYPE_AMD64;
    pModeData->uGstType = PGM_TYPE_REAL;
    rc = PGM_SHW_NAME_AMD64(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_REAL(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_AMD64_REAL(InitData)( pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_AMD64, PGM_TYPE_PROT)];
    pModeData->uShwType = PGM_TYPE_AMD64;
    pModeData->uGstType = PGM_TYPE_PROT;
    rc = PGM_SHW_NAME_AMD64(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_PROT(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_AMD64_PROT(InitData)( pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_AMD64, PGM_TYPE_AMD64)];
    pModeData->uShwType = PGM_TYPE_AMD64;
    pModeData->uGstType = PGM_TYPE_AMD64;
    rc = PGM_SHW_NAME_AMD64(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_AMD64(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_AMD64_AMD64(InitData)(pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Swtich to different (or relocated in the relocate case) mode data.
 *
 * @param   pVM         The VM handle.
 * @param   enmShw      The the shadow paging mode.
 * @param   enmGst      The the guest paging mode.
 */
static void pgmR3ModeDataSwitch(PVM pVM, PGMMODE enmShw, PGMMODE enmGst)
{
    PPGMMODEDATA pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(enmShw, enmGst)];

    Assert(pModeData->uGstType == pgmModeToType(enmGst));
    Assert(pModeData->uShwType == pgmModeToType(enmShw));

    /* shadow */
    pVM->pgm.s.pfnR3ShwRelocate             = pModeData->pfnR3ShwRelocate;
    pVM->pgm.s.pfnR3ShwExit                 = pModeData->pfnR3ShwExit;
    pVM->pgm.s.pfnR3ShwGetPage              = pModeData->pfnR3ShwGetPage;
    Assert(pVM->pgm.s.pfnR3ShwGetPage);
    pVM->pgm.s.pfnR3ShwModifyPage           = pModeData->pfnR3ShwModifyPage;
    pVM->pgm.s.pfnR3ShwGetPDEByIndex        = pModeData->pfnR3ShwGetPDEByIndex;
    pVM->pgm.s.pfnR3ShwSetPDEByIndex        = pModeData->pfnR3ShwSetPDEByIndex;
    pVM->pgm.s.pfnR3ShwModifyPDEByIndex     = pModeData->pfnR3ShwModifyPDEByIndex;

    pVM->pgm.s.pfnGCShwGetPage              = pModeData->pfnGCShwGetPage;
    pVM->pgm.s.pfnGCShwModifyPage           = pModeData->pfnGCShwModifyPage;
    pVM->pgm.s.pfnGCShwGetPDEByIndex        = pModeData->pfnGCShwGetPDEByIndex;
    pVM->pgm.s.pfnGCShwSetPDEByIndex        = pModeData->pfnGCShwSetPDEByIndex;
    pVM->pgm.s.pfnGCShwModifyPDEByIndex     = pModeData->pfnGCShwModifyPDEByIndex;

    pVM->pgm.s.pfnR0ShwGetPage              = pModeData->pfnR0ShwGetPage;
    pVM->pgm.s.pfnR0ShwModifyPage           = pModeData->pfnR0ShwModifyPage;
    pVM->pgm.s.pfnR0ShwGetPDEByIndex        = pModeData->pfnR0ShwGetPDEByIndex;
    pVM->pgm.s.pfnR0ShwSetPDEByIndex        = pModeData->pfnR0ShwSetPDEByIndex;
    pVM->pgm.s.pfnR0ShwModifyPDEByIndex     = pModeData->pfnR0ShwModifyPDEByIndex;


    /* guest */
    pVM->pgm.s.pfnR3GstRelocate             = pModeData->pfnR3GstRelocate;
    pVM->pgm.s.pfnR3GstExit                 = pModeData->pfnR3GstExit;
    pVM->pgm.s.pfnR3GstGetPage              = pModeData->pfnR3GstGetPage;
    Assert(pVM->pgm.s.pfnR3GstGetPage);
    pVM->pgm.s.pfnR3GstModifyPage           = pModeData->pfnR3GstModifyPage;
    pVM->pgm.s.pfnR3GstGetPDE               = pModeData->pfnR3GstGetPDE;
    pVM->pgm.s.pfnR3GstMonitorCR3           = pModeData->pfnR3GstMonitorCR3;
    pVM->pgm.s.pfnR3GstUnmonitorCR3         = pModeData->pfnR3GstUnmonitorCR3;
    pVM->pgm.s.pfnR3GstMapCR3               = pModeData->pfnR3GstMapCR3;
    pVM->pgm.s.pfnR3GstUnmapCR3             = pModeData->pfnR3GstUnmapCR3;
    pVM->pgm.s.pfnHCGstWriteHandlerCR3      = pModeData->pfnHCGstWriteHandlerCR3;
    pVM->pgm.s.pszHCGstWriteHandlerCR3      = pModeData->pszHCGstWriteHandlerCR3;

    pVM->pgm.s.pfnGCGstGetPage              = pModeData->pfnGCGstGetPage;
    pVM->pgm.s.pfnGCGstModifyPage           = pModeData->pfnGCGstModifyPage;
    pVM->pgm.s.pfnGCGstGetPDE               = pModeData->pfnGCGstGetPDE;
    pVM->pgm.s.pfnGCGstMonitorCR3           = pModeData->pfnGCGstMonitorCR3;
    pVM->pgm.s.pfnGCGstUnmonitorCR3         = pModeData->pfnGCGstUnmonitorCR3;
    pVM->pgm.s.pfnGCGstMapCR3               = pModeData->pfnGCGstMapCR3;
    pVM->pgm.s.pfnGCGstUnmapCR3             = pModeData->pfnGCGstUnmapCR3;
    pVM->pgm.s.pfnGCGstWriteHandlerCR3      = pModeData->pfnGCGstWriteHandlerCR3;

    pVM->pgm.s.pfnR0GstGetPage              = pModeData->pfnR0GstGetPage;
    pVM->pgm.s.pfnR0GstModifyPage           = pModeData->pfnR0GstModifyPage;
    pVM->pgm.s.pfnR0GstGetPDE               = pModeData->pfnR0GstGetPDE;
    pVM->pgm.s.pfnR0GstMonitorCR3           = pModeData->pfnR0GstMonitorCR3;
    pVM->pgm.s.pfnR0GstUnmonitorCR3         = pModeData->pfnR0GstUnmonitorCR3;
    pVM->pgm.s.pfnR0GstMapCR3               = pModeData->pfnR0GstMapCR3;
    pVM->pgm.s.pfnR0GstUnmapCR3             = pModeData->pfnR0GstUnmapCR3;
    pVM->pgm.s.pfnR0GstWriteHandlerCR3      = pModeData->pfnR0GstWriteHandlerCR3;


    /* both */
    pVM->pgm.s.pfnR3BthRelocate             = pModeData->pfnR3BthRelocate;
    pVM->pgm.s.pfnR3BthTrap0eHandler        = pModeData->pfnR3BthTrap0eHandler;
    pVM->pgm.s.pfnR3BthInvalidatePage       = pModeData->pfnR3BthInvalidatePage;
    pVM->pgm.s.pfnR3BthSyncCR3              = pModeData->pfnR3BthSyncCR3;
    Assert(pVM->pgm.s.pfnR3BthSyncCR3);
    pVM->pgm.s.pfnR3BthSyncPage             = pModeData->pfnR3BthSyncPage;
    pVM->pgm.s.pfnR3BthPrefetchPage         = pModeData->pfnR3BthPrefetchPage;
    pVM->pgm.s.pfnR3BthVerifyAccessSyncPage = pModeData->pfnR3BthVerifyAccessSyncPage;
#ifdef VBOX_STRICT
    pVM->pgm.s.pfnR3BthAssertCR3            = pModeData->pfnR3BthAssertCR3;
#endif

    pVM->pgm.s.pfnGCBthTrap0eHandler        = pModeData->pfnGCBthTrap0eHandler;
    pVM->pgm.s.pfnGCBthInvalidatePage       = pModeData->pfnGCBthInvalidatePage;
    pVM->pgm.s.pfnGCBthSyncCR3              = pModeData->pfnGCBthSyncCR3;
    pVM->pgm.s.pfnGCBthSyncPage             = pModeData->pfnGCBthSyncPage;
    pVM->pgm.s.pfnGCBthPrefetchPage         = pModeData->pfnGCBthPrefetchPage;
    pVM->pgm.s.pfnGCBthVerifyAccessSyncPage = pModeData->pfnGCBthVerifyAccessSyncPage;
#ifdef VBOX_STRICT
    pVM->pgm.s.pfnGCBthAssertCR3            = pModeData->pfnGCBthAssertCR3;
#endif

    pVM->pgm.s.pfnR0BthTrap0eHandler        = pModeData->pfnR0BthTrap0eHandler;
    pVM->pgm.s.pfnR0BthInvalidatePage       = pModeData->pfnR0BthInvalidatePage;
    pVM->pgm.s.pfnR0BthSyncCR3              = pModeData->pfnR0BthSyncCR3;
    pVM->pgm.s.pfnR0BthSyncPage             = pModeData->pfnR0BthSyncPage;
    pVM->pgm.s.pfnR0BthPrefetchPage         = pModeData->pfnR0BthPrefetchPage;
    pVM->pgm.s.pfnR0BthVerifyAccessSyncPage = pModeData->pfnR0BthVerifyAccessSyncPage;
#ifdef VBOX_STRICT
    pVM->pgm.s.pfnR0BthAssertCR3            = pModeData->pfnR0BthAssertCR3;
#endif
}


#ifdef DEBUG_bird
#include <stdlib.h> /* getenv() remove me! */
#endif

/**
 * Calculates the shadow paging mode.
 *
 * @returns The shadow paging mode.
 * @param   enmGuestMode    The guest mode.
 * @param   enmHostMode     The host mode.
 * @param   enmShadowMode   The current shadow mode.
 * @param   penmSwitcher    Where to store the switcher to use.
 *                          VMMSWITCHER_INVALID means no change.
 */
static PGMMODE pgmR3CalcShadowMode(PGMMODE enmGuestMode, SUPPAGINGMODE enmHostMode, PGMMODE enmShadowMode, VMMSWITCHER *penmSwitcher)
{
    VMMSWITCHER enmSwitcher = VMMSWITCHER_INVALID;
    switch (enmGuestMode)
    {
        /*
         * When switching to real or protected mode we don't change
         * anything since it's likely that we'll switch back pretty soon.
         *
         * During pgmR3InitPaging we'll end up here with PGMMODE_INVALID
         * and is supposed to determin which shadow paging and switcher to
         * use during init.
         */
        case PGMMODE_REAL:
        case PGMMODE_PROTECTED:
            if (enmShadowMode != PGMMODE_INVALID)
                break; /* (no change) */
            switch (enmHostMode)
            {
                case SUPPAGINGMODE_32_BIT:
                case SUPPAGINGMODE_32_BIT_GLOBAL:
                    enmShadowMode = PGMMODE_32_BIT;
                    enmSwitcher = VMMSWITCHER_32_TO_32;
                    break;

                case SUPPAGINGMODE_PAE:
                case SUPPAGINGMODE_PAE_NX:
                case SUPPAGINGMODE_PAE_GLOBAL:
                case SUPPAGINGMODE_PAE_GLOBAL_NX:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_PAE_TO_PAE;
#ifdef DEBUG_bird
if (getenv("VBOX_32BIT"))
{
                    enmShadowMode = PGMMODE_32_BIT;
                    enmSwitcher = VMMSWITCHER_PAE_TO_32;
}
#endif
                    break;

                case SUPPAGINGMODE_AMD64:
                case SUPPAGINGMODE_AMD64_GLOBAL:
                case SUPPAGINGMODE_AMD64_NX:
                case SUPPAGINGMODE_AMD64_GLOBAL_NX:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_AMD64_TO_PAE;
                    break;

                default: AssertMsgFailed(("enmHostMode=%d\n", enmHostMode)); break;
            }
            break;

        case PGMMODE_32_BIT:
            switch (enmHostMode)
            {
                case SUPPAGINGMODE_32_BIT:
                case SUPPAGINGMODE_32_BIT_GLOBAL:
                    enmShadowMode = PGMMODE_32_BIT;
                    enmSwitcher = VMMSWITCHER_32_TO_32;
                    break;

                case SUPPAGINGMODE_PAE:
                case SUPPAGINGMODE_PAE_NX:
                case SUPPAGINGMODE_PAE_GLOBAL:
                case SUPPAGINGMODE_PAE_GLOBAL_NX:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_PAE_TO_PAE;
#ifdef DEBUG_bird
if (getenv("VBOX_32BIT"))
{
                    enmShadowMode = PGMMODE_32_BIT;
                    enmSwitcher = VMMSWITCHER_PAE_TO_32;
}
#endif
                    break;

                case SUPPAGINGMODE_AMD64:
                case SUPPAGINGMODE_AMD64_GLOBAL:
                case SUPPAGINGMODE_AMD64_NX:
                case SUPPAGINGMODE_AMD64_GLOBAL_NX:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_AMD64_TO_PAE;
                    break;

                default: AssertMsgFailed(("enmHostMode=%d\n", enmHostMode)); break;
            }
            break;

        case PGMMODE_PAE:
        case PGMMODE_PAE_NX: /** @todo This might require more switchers and guest+both modes. */
            switch (enmHostMode)
            {
                case SUPPAGINGMODE_32_BIT:
                case SUPPAGINGMODE_32_BIT_GLOBAL:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_32_TO_PAE;
                    break;

                case SUPPAGINGMODE_PAE:
                case SUPPAGINGMODE_PAE_NX:
                case SUPPAGINGMODE_PAE_GLOBAL:
                case SUPPAGINGMODE_PAE_GLOBAL_NX:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_PAE_TO_PAE;
                    break;

                case SUPPAGINGMODE_AMD64:
                case SUPPAGINGMODE_AMD64_GLOBAL:
                case SUPPAGINGMODE_AMD64_NX:
                case SUPPAGINGMODE_AMD64_GLOBAL_NX:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_AMD64_TO_PAE;
                    break;

                default: AssertMsgFailed(("enmHostMode=%d\n", enmHostMode)); break;
            }
            break;

        case PGMMODE_AMD64:
        case PGMMODE_AMD64_NX:
            switch (enmHostMode)
            {
                case SUPPAGINGMODE_32_BIT:
                case SUPPAGINGMODE_32_BIT_GLOBAL:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_32_TO_AMD64;
                    break;

                case SUPPAGINGMODE_PAE:
                case SUPPAGINGMODE_PAE_NX:
                case SUPPAGINGMODE_PAE_GLOBAL:
                case SUPPAGINGMODE_PAE_GLOBAL_NX:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_PAE_TO_AMD64;
                    break;

                case SUPPAGINGMODE_AMD64:
                case SUPPAGINGMODE_AMD64_GLOBAL:
                case SUPPAGINGMODE_AMD64_NX:
                case SUPPAGINGMODE_AMD64_GLOBAL_NX:
                    enmShadowMode = PGMMODE_PAE;
                    enmSwitcher = VMMSWITCHER_AMD64_TO_AMD64;
                    break;

                default: AssertMsgFailed(("enmHostMode=%d\n", enmHostMode)); break;
            }
            break;


        default:
            AssertReleaseMsgFailed(("enmGuestMode=%d\n", enmGuestMode));
            return PGMMODE_INVALID;
    }

    *penmSwitcher = enmSwitcher;
    return enmShadowMode;
}


/**
 * Performs the actual mode change.
 * This is called by PGMChangeMode and pgmR3InitPaging().
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   enmGuestMode    The new guest mode. This is assumed to be different from
 *                          the current mode.
 */
int pgmR3ChangeMode(PVM pVM, PGMMODE enmGuestMode)
{
    LogFlow(("pgmR3ChangeMode: Guest mode: %d -> %d\n", pVM->pgm.s.enmGuestMode, enmGuestMode));
    STAM_REL_COUNTER_INC(&pVM->pgm.s.cGuestModeChanges);

    /*
     * Calc the shadow mode and switcher.
     */
    VMMSWITCHER enmSwitcher;
    PGMMODE     enmShadowMode = pgmR3CalcShadowMode(enmGuestMode, pVM->pgm.s.enmHostMode, pVM->pgm.s.enmShadowMode, &enmSwitcher);
    if (enmSwitcher != VMMSWITCHER_INVALID)
    {
        /*
         * Select new switcher.
         */
        int rc = VMMR3SelectSwitcher(pVM, enmSwitcher);
        if (VBOX_FAILURE(rc))
        {
            AssertReleaseMsgFailed(("VMMR3SelectSwitcher(%d) -> %Vrc\n", enmSwitcher, rc));
            return rc;
        }
    }

    /*
     * Exit old mode(s).
     */
    /* shadow */
    if (enmShadowMode != pVM->pgm.s.enmShadowMode)
    {
        LogFlow(("pgmR3ChangeMode: Shadow mode: %d -> %d\n",  pVM->pgm.s.enmShadowMode, enmShadowMode));
        if (PGM_SHW_PFN(Exit, pVM))
        {
            int rc = PGM_SHW_PFN(Exit, pVM)(pVM);
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("Exit failed for shadow mode %d: %Vrc\n", pVM->pgm.s.enmShadowMode, rc));
                return rc;
            }
        }

    }

    /* guest */
    if (PGM_GST_PFN(Exit, pVM))
    {
        int rc = PGM_GST_PFN(Exit, pVM)(pVM);
        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("Exit failed for guest mode %d: %Vrc\n", pVM->pgm.s.enmGuestMode, rc));
            return rc;
        }
    }

    /*
     * Load new paging mode data.
     */
    pgmR3ModeDataSwitch(pVM, enmShadowMode, enmGuestMode);

    /*
     * Enter new shadow mode (if changed).
     */
    if (enmShadowMode != pVM->pgm.s.enmShadowMode)
    {
        int rc;
        pVM->pgm.s.enmShadowMode = enmShadowMode;
        switch (enmShadowMode)
        {
            case PGMMODE_32_BIT:
                rc = PGM_SHW_NAME_32BIT(Enter)(pVM);
                break;
            case PGMMODE_PAE:
            case PGMMODE_PAE_NX:
                rc = PGM_SHW_NAME_PAE(Enter)(pVM);
                break;
            case PGMMODE_AMD64:
            case PGMMODE_AMD64_NX:
                rc = PGM_SHW_NAME_AMD64(Enter)(pVM);
                break;
            case PGMMODE_REAL:
            case PGMMODE_PROTECTED:
            default:
                AssertReleaseMsgFailed(("enmShadowMode=%d\n", enmShadowMode));
                return VERR_INTERNAL_ERROR;
        }
        if (VBOX_FAILURE(rc))
        {
            AssertReleaseMsgFailed(("Entering enmShadowMode=%d failed: %Vrc\n", enmShadowMode, rc));
            pVM->pgm.s.enmShadowMode = PGMMODE_INVALID;
            return rc;
        }
    }

    /*
     * Enter the new guest and shadow+guest modes.
     */
    int rc = -1;
    int rc2 = -1;
    RTGCPHYS GCPhysCR3 = NIL_RTGCPHYS;
    pVM->pgm.s.enmGuestMode = enmGuestMode;
    switch (enmGuestMode)
    {
        case PGMMODE_REAL:
            rc = PGM_GST_NAME_REAL(Enter)(pVM, NIL_RTGCPHYS);
            switch (pVM->pgm.s.enmShadowMode)
            {
                case PGMMODE_32_BIT:
                    rc2 = PGM_BTH_NAME_32BIT_REAL(Enter)(pVM, NIL_RTGCPHYS);
                    break;
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    rc2 = PGM_BTH_NAME_PAE_REAL(Enter)(pVM, NIL_RTGCPHYS);
                    break;
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    rc2 = PGM_BTH_NAME_AMD64_REAL(Enter)(pVM, NIL_RTGCPHYS);
                    break;
                default: AssertFailed(); break;
            }
            break;

        case PGMMODE_PROTECTED:
            rc = PGM_GST_NAME_PROT(Enter)(pVM, NIL_RTGCPHYS);
            switch (pVM->pgm.s.enmShadowMode)
            {
                case PGMMODE_32_BIT:
                    rc2 = PGM_BTH_NAME_32BIT_PROT(Enter)(pVM, NIL_RTGCPHYS);
                    break;
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    rc2 = PGM_BTH_NAME_PAE_PROT(Enter)(pVM, NIL_RTGCPHYS);
                    break;
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    rc2 = PGM_BTH_NAME_AMD64_PROT(Enter)(pVM, NIL_RTGCPHYS);
                    break;
                default: AssertFailed(); break;
            }
            break;

        case PGMMODE_32_BIT:
            GCPhysCR3 = CPUMGetGuestCR3(pVM) & X86_CR3_PAGE_MASK;
            rc = PGM_GST_NAME_32BIT(Enter)(pVM, GCPhysCR3);
            switch (pVM->pgm.s.enmShadowMode)
            {
                case PGMMODE_32_BIT:
                    rc2 = PGM_BTH_NAME_32BIT_32BIT(Enter)(pVM, GCPhysCR3);
                    break;
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    rc2 = PGM_BTH_NAME_PAE_32BIT(Enter)(pVM, GCPhysCR3);
                    break;
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    AssertMsgFailed(("Should use PAE shadow mode!\n"));
                default: AssertFailed(); break;
            }
            break;

        //case PGMMODE_PAE_NX:
        case PGMMODE_PAE:
            GCPhysCR3 = CPUMGetGuestCR3(pVM) & X86_CR3_PAE_PAGE_MASK;
            rc = PGM_GST_NAME_PAE(Enter)(pVM, GCPhysCR3);
            switch (pVM->pgm.s.enmShadowMode)
            {
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    rc2 = PGM_BTH_NAME_PAE_PAE(Enter)(pVM, GCPhysCR3);
                    break;
                case PGMMODE_32_BIT:
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    AssertMsgFailed(("Should use PAE shadow mode!\n"));
                default: AssertFailed(); break;
            }
            break;

        //case PGMMODE_AMD64_NX:
        case PGMMODE_AMD64:
            GCPhysCR3 = CPUMGetGuestCR3(pVM) & 0xfffffffffffff000ULL; /** @todo define this mask and make CR3 64-bit in this case! */
            rc = PGM_GST_NAME_AMD64(Enter)(pVM, GCPhysCR3);
            switch (pVM->pgm.s.enmShadowMode)
            {
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    rc2 = PGM_BTH_NAME_AMD64_AMD64(Enter)(pVM, GCPhysCR3);
                    break;
                case PGMMODE_32_BIT:
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    AssertMsgFailed(("Should use AMD64 shadow mode!\n"));
                default: AssertFailed(); break;
            }
            break;

        default:
            AssertReleaseMsgFailed(("enmGuestMode=%d\n", enmGuestMode));
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    /* status codes. */
    AssertRC(rc);
    AssertRC(rc2);
    if (VBOX_SUCCESS(rc))
    {
        rc = rc2;
        if (VBOX_SUCCESS(rc)) /* no informational status codes. */
            rc = VINF_SUCCESS;
    }

    /*
     * Notify SELM so it can update the TSSes with correct CR3s.
     */
    SELMR3PagingModeChanged(pVM);

    /* Notify HWACCM as well. */
    HWACCMR3PagingModeChanged(pVM, pVM->pgm.s.enmShadowMode);
    return rc;
}


/**
 * Dumps a PAE shadow page table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   pPT         Pointer to the page table.
 * @param   u64Address  The virtual address of the page table starts.
 * @param   fLongMode   Set if this a long mode table; clear if it's a legacy mode table.
 * @param   cMaxDepth   The maxium depth.
 * @param   pHlp        Pointer to the output functions.
 */
static int  pgmR3DumpHierarchyHCPaePT(PVM pVM, PX86PTPAE pPT, uint64_t u64Address, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
{
    for (unsigned i = 0; i < ELEMENTS(pPT->a); i++)
    {
        X86PTEPAE Pte = pPT->a[i];
        if (Pte.n.u1Present)
        {
            pHlp->pfnPrintf(pHlp,
                            fLongMode       /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                            ? "%016llx 3    | P %c %c %c %c %c %s %s %s %s 4K %c%c%c  %016llx\n"
                            :  "%08llx 2   |  P %c %c %c %c %c %s %s %s %s 4K %c%c%c  %016llx\n",
                            u64Address + ((uint64_t)i << X86_PT_PAE_SHIFT),
                            Pte.n.u1Write       ? 'W'  : 'R',
                            Pte.n.u1User        ? 'U'  : 'S',
                            Pte.n.u1Accessed    ? 'A'  : '-',
                            Pte.n.u1Dirty       ? 'D'  : '-',
                            Pte.n.u1Global      ? 'G'  : '-',
                            Pte.n.u1WriteThru   ? "WT" : "--",
                            Pte.n.u1CacheDisable? "CD" : "--",
                            Pte.n.u1PAT         ? "AT" : "--",
                            Pte.n.u1NoExecute   ? "NX" : "--",
                            Pte.u & PGM_PTFLAGS_TRACK_DIRTY   ? 'd' : '-',
                            Pte.u & RT_BIT(10)                   ? '1' : '0',
                            Pte.u & PGM_PTFLAGS_CSAM_VALIDATED? 'v' : '-',
                            Pte.u & X86_PTE_PAE_PG_MASK);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Dumps a PAE shadow page directory table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   HCPhys      The physical address of the page directory table.
 * @param   u64Address  The virtual address of the page table starts.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   fLongMode   Set if this a long mode table; clear if it's a legacy mode table.
 * @param   cMaxDepth   The maxium depth.
 * @param   pHlp        Pointer to the output functions.
 */
static int  pgmR3DumpHierarchyHCPaePD(PVM pVM, RTHCPHYS HCPhys, uint64_t u64Address, uint32_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
{
    PX86PDPAE pPD = (PX86PDPAE)MMPagePhys2Page(pVM, HCPhys);
    if (!pPD)
    {
        pHlp->pfnPrintf(pHlp, "%0*llx error! Page directory at HCPhys=%#VHp was not found in the page pool!\n",
                        fLongMode ? 16 : 8, u64Address, HCPhys);
        return VERR_INVALID_PARAMETER;
    }
    int rc = VINF_SUCCESS;
    for (unsigned i = 0; i < ELEMENTS(pPD->a); i++)
    {
        X86PDEPAE Pde = pPD->a[i];
        if (Pde.n.u1Present)
        {
            if ((cr4 & X86_CR4_PSE) && Pde.b.u1Size)
                pHlp->pfnPrintf(pHlp,
                                fLongMode       /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                                ? "%016llx 2   |  P %c %c %c %c %c %s %s %s %s 4M %c%c%c  %016llx\n"
                                :  "%08llx 1  |   P %c %c %c %c %c %s %s %s %s 4M %c%c%c  %016llx\n",
                                u64Address + ((uint64_t)i << X86_PD_PAE_SHIFT),
                                Pde.b.u1Write       ? 'W'  : 'R',
                                Pde.b.u1User        ? 'U'  : 'S',
                                Pde.b.u1Accessed    ? 'A'  : '-',
                                Pde.b.u1Dirty       ? 'D'  : '-',
                                Pde.b.u1Global      ? 'G'  : '-',
                                Pde.b.u1WriteThru   ? "WT" : "--",
                                Pde.b.u1CacheDisable? "CD" : "--",
                                Pde.b.u1PAT         ? "AT" : "--",
                                Pde.b.u1NoExecute   ? "NX" : "--",
                                Pde.u & RT_BIT_64(9)                ? '1' : '0',
                                Pde.u & PGM_PDFLAGS_MAPPING     ? 'm' : '-',
                                Pde.u & PGM_PDFLAGS_TRACK_DIRTY ? 'd' : '-',
                                Pde.u & X86_PDE_PAE_PG_MASK);
            else
            {
                pHlp->pfnPrintf(pHlp,
                                fLongMode       /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                                ? "%016llx 2   |  P %c %c %c %c %c %s %s .. %s 4K %c%c%c  %016llx\n"
                                :  "%08llx 1  |   P %c %c %c %c %c %s %s .. %s 4K %c%c%c  %016llx\n",
                                u64Address + ((uint64_t)i << X86_PD_PAE_SHIFT),
                                Pde.n.u1Write       ? 'W'  : 'R',
                                Pde.n.u1User        ? 'U'  : 'S',
                                Pde.n.u1Accessed    ? 'A'  : '-',
                                Pde.n.u1Reserved0   ? '?'  : '.', /* ignored */
                                Pde.n.u1Reserved1   ? '?'  : '.', /* ignored */
                                Pde.n.u1WriteThru   ? "WT" : "--",
                                Pde.n.u1CacheDisable? "CD" : "--",
                                Pde.n.u1NoExecute   ? "NX" : "--",
                                Pde.u & RT_BIT_64(9)                ? '1' : '0',
                                Pde.u & PGM_PDFLAGS_MAPPING     ? 'm' : '-',
                                Pde.u & PGM_PDFLAGS_TRACK_DIRTY ? 'd' : '-',
                                Pde.u & X86_PDE_PAE_PG_MASK);
                if (cMaxDepth >= 1)
                {
                    /** @todo what about using the page pool for mapping PTs? */
                    uint64_t    u64AddressPT = u64Address + ((uint64_t)i << X86_PD_PAE_SHIFT);
                    RTHCPHYS    HCPhysPT     = Pde.u & X86_PDE_PAE_PG_MASK;
                    PX86PTPAE   pPT          = NULL;
                    if (!(Pde.u & PGM_PDFLAGS_MAPPING))
                        pPT = (PX86PTPAE)MMPagePhys2Page(pVM, HCPhysPT);
                    else
                    {
                        for (PPGMMAPPING pMap = pVM->pgm.s.pMappingsR3; pMap; pMap = pMap->pNextR3)
                        {
                            uint64_t off = u64AddressPT - pMap->GCPtr;
                            if (off < pMap->cb)
                            {
                                const int iPDE = (uint32_t)(off >> X86_PD_SHIFT);
                                const int iSub = (int)((off >> X86_PD_PAE_SHIFT) & 1); /* MSC is a pain sometimes */
                                if ((iSub ? pMap->aPTs[iPDE].HCPhysPaePT1 : pMap->aPTs[iPDE].HCPhysPaePT0) != HCPhysPT)
                                    pHlp->pfnPrintf(pHlp, "%0*llx error! Mapping error! PT %d has HCPhysPT=%VHp not %VHp is in the PD.\n",
                                                    fLongMode ? 16 : 8, u64AddressPT, iPDE,
                                                    iSub ? pMap->aPTs[iPDE].HCPhysPaePT1 : pMap->aPTs[iPDE].HCPhysPaePT0, HCPhysPT);
                                pPT = &pMap->aPTs[iPDE].paPaePTsR3[iSub];
                            }
                        }
                    }
                    int rc2 = VERR_INVALID_PARAMETER;
                    if (pPT)
                        rc2 = pgmR3DumpHierarchyHCPaePT(pVM, pPT, u64AddressPT, fLongMode, cMaxDepth - 1, pHlp);
                    else
                        pHlp->pfnPrintf(pHlp, "%0*llx error! Page table at HCPhys=%#VHp was not found in the page pool!\n",
                                        fLongMode ? 16 : 8, u64AddressPT, HCPhysPT);
                    if (rc2 < rc && VBOX_SUCCESS(rc))
                        rc = rc2;
                }
            }
        }
    }
    return rc;
}


/**
 * Dumps a PAE shadow page directory pointer table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   HCPhys      The physical address of the page directory pointer table.
 * @param   u64Address  The virtual address of the page table starts.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   fLongMode   Set if this a long mode table; clear if it's a legacy mode table.
 * @param   cMaxDepth   The maxium depth.
 * @param   pHlp        Pointer to the output functions.
 */
static int  pgmR3DumpHierarchyHCPaePDPTR(PVM pVM, RTHCPHYS HCPhys, uint64_t u64Address, uint32_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
{
    PX86PDPTR pPDPTR = (PX86PDPTR)MMPagePhys2Page(pVM, HCPhys);
    if (!pPDPTR)
    {
        pHlp->pfnPrintf(pHlp, "%0*llx error! Page directory pointer table at HCPhys=%#VHp was not found in the page pool!\n",
                        fLongMode ? 16 : 8, u64Address, HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;
    const unsigned c = fLongMode ? ELEMENTS(pPDPTR->a) : 4;
    for (unsigned i = 0; i < c; i++)
    {
        X86PDPE Pdpe = pPDPTR->a[i];
        if (Pdpe.n.u1Present)
        {
            if (fLongMode)
                pHlp->pfnPrintf(pHlp,         /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                                "%016llx 1  |   P %c %c %c %c %c %s %s %s %s .. %c%c%c  %016llx\n",
                                u64Address + ((uint64_t)i << X86_PDPTR_SHIFT),
                                Pdpe.n.u1Write       ? 'W'  : 'R',
                                Pdpe.n.u1User        ? 'U'  : 'S',
                                Pdpe.n.u1Accessed    ? 'A'  : '-',
                                Pdpe.n.u3Reserved & 1? '?'  : '.', /* ignored */
                                Pdpe.n.u3Reserved & 4? '!'  : '.', /* mbz */
                                Pdpe.n.u1WriteThru   ? "WT" : "--",
                                Pdpe.n.u1CacheDisable? "CD" : "--",
                                Pdpe.n.u3Reserved & 2? "!"  : "..",/* mbz */
                                Pdpe.n.u1NoExecute   ? "NX" : "--",
                                Pdpe.u & RT_BIT(9)                   ? '1' : '0',
                                Pdpe.u & PGM_PLXFLAGS_PERMANENT   ? 'p' : '-',
                                Pdpe.u & RT_BIT(11)                  ? '1' : '0',
                                Pdpe.u & X86_PDPE_PG_MASK);
            else
                pHlp->pfnPrintf(pHlp,      /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                                "%08x 0 |    P %c %c %c %c %c %s %s %s %s .. %c%c%c  %016llx\n",
                                i << X86_PDPTR_SHIFT,
                                Pdpe.n.u1Write       ? '!'  : '.', /* mbz */
                                Pdpe.n.u1User        ? '!'  : '.', /* mbz */
                                Pdpe.n.u1Accessed    ? '!'  : '.', /* mbz */
                                Pdpe.n.u3Reserved & 1? '!'  : '.', /* mbz */
                                Pdpe.n.u3Reserved & 4? '!'  : '.', /* mbz */
                                Pdpe.n.u1WriteThru   ? "WT" : "--",
                                Pdpe.n.u1CacheDisable? "CD" : "--",
                                Pdpe.n.u3Reserved & 2? "!"  : "..",/* mbz */
                                Pdpe.n.u1NoExecute   ? "NX" : "--",
                                Pdpe.u & RT_BIT(9)                   ? '1' : '0',
                                Pdpe.u & PGM_PLXFLAGS_PERMANENT   ? 'p' : '-',
                                Pdpe.u & RT_BIT(11)                  ? '1' : '0',
                                Pdpe.u & X86_PDPE_PG_MASK);
            if (cMaxDepth >= 1)
            {
                int rc2 = pgmR3DumpHierarchyHCPaePD(pVM, Pdpe.u & X86_PDPE_PG_MASK, u64Address + ((uint64_t)i << X86_PDPTR_SHIFT),
                                                    cr4, fLongMode, cMaxDepth - 1, pHlp);
                if (rc2 < rc && VBOX_SUCCESS(rc))
                    rc = rc2;
            }
        }
    }
    return rc;
}


/**
 * Dumps a 32-bit shadow page table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   HCPhys      The physical address of the table.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   cMaxDepth   The maxium depth.
 * @param   pHlp        Pointer to the output functions.
 */
static int pgmR3DumpHierarchyHcPaePML4(PVM pVM, RTHCPHYS HCPhys, uint32_t cr4, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
{
    PX86PML4 pPML4 = (PX86PML4)MMPagePhys2Page(pVM, HCPhys);
    if (!pPML4)
    {
        pHlp->pfnPrintf(pHlp, "Page map level 4 at HCPhys=%#VHp was not found in the page pool!\n", HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;
    for (unsigned i = 0; i < ELEMENTS(pPML4->a); i++)
    {
        X86PML4E Pml4e = pPML4->a[i];
        if (Pml4e.n.u1Present)
        {
            uint64_t u64Address = ((uint64_t)i << X86_PML4_SHIFT) | (((uint64_t)i >> (X86_PML4_SHIFT - X86_PDPTR_SHIFT - 1)) * 0xffff000000000000ULL);
            pHlp->pfnPrintf(pHlp,         /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                            "%016llx 0 |    P %c %c %c %c %c %s %s %s %s .. %c%c%c  %016llx\n",
                            u64Address,
                            Pml4e.n.u1Write       ? 'W'  : 'R',
                            Pml4e.n.u1User        ? 'U'  : 'S',
                            Pml4e.n.u1Accessed    ? 'A'  : '-',
                            Pml4e.n.u3Reserved & 1? '?'  : '.', /* ignored */
                            Pml4e.n.u3Reserved & 4? '!'  : '.', /* mbz */
                            Pml4e.n.u1WriteThru   ? "WT" : "--",
                            Pml4e.n.u1CacheDisable? "CD" : "--",
                            Pml4e.n.u3Reserved & 2? "!"  : "..",/* mbz */
                            Pml4e.n.u1NoExecute   ? "NX" : "--",
                            Pml4e.u & RT_BIT(9)                   ? '1' : '0',
                            Pml4e.u & PGM_PLXFLAGS_PERMANENT   ? 'p' : '-',
                            Pml4e.u & RT_BIT(11)                  ? '1' : '0',
                            Pml4e.u & X86_PML4E_PG_MASK);

            if (cMaxDepth >= 1)
            {
                int rc2 = pgmR3DumpHierarchyHCPaePDPTR(pVM, Pml4e.u & X86_PML4E_PG_MASK, u64Address, cr4, true, cMaxDepth - 1, pHlp);
                if (rc2 < rc && VBOX_SUCCESS(rc))
                    rc = rc2;
            }
        }
    }
    return rc;
}


/**
 * Dumps a 32-bit shadow page table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   pPT         Pointer to the page table.
 * @param   u32Address  The virtual address this table starts at.
 * @param   pHlp        Pointer to the output functions.
 */
int  pgmR3DumpHierarchyHC32BitPT(PVM pVM, PX86PT pPT, uint32_t u32Address, PCDBGFINFOHLP pHlp)
{
    for (unsigned i = 0; i < ELEMENTS(pPT->a); i++)
    {
        X86PTE Pte = pPT->a[i];
        if (Pte.n.u1Present)
        {
            pHlp->pfnPrintf(pHlp,      /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                            "%08x 1  |   P %c %c %c %c %c %s %s %s .. 4K %c%c%c  %08x\n",
                            u32Address + (i << X86_PT_SHIFT),
                            Pte.n.u1Write       ? 'W'  : 'R',
                            Pte.n.u1User        ? 'U'  : 'S',
                            Pte.n.u1Accessed    ? 'A'  : '-',
                            Pte.n.u1Dirty       ? 'D'  : '-',
                            Pte.n.u1Global      ? 'G'  : '-',
                            Pte.n.u1WriteThru   ? "WT" : "--",
                            Pte.n.u1CacheDisable? "CD" : "--",
                            Pte.n.u1PAT         ? "AT" : "--",
                            Pte.u & PGM_PTFLAGS_TRACK_DIRTY     ? 'd' : '-',
                            Pte.u & RT_BIT(10)                     ? '1' : '0',
                            Pte.u & PGM_PTFLAGS_CSAM_VALIDATED  ? 'v' : '-',
                            Pte.u & X86_PDE_PG_MASK);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Dumps a 32-bit shadow page directory and page tables.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   cMaxDepth   How deep into the hierarchy the dumper should go.
 * @param   pHlp        Pointer to the output functions.
 */
int  pgmR3DumpHierarchyHC32BitPD(PVM pVM, uint32_t cr3, uint32_t cr4, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
{
    PX86PD pPD = (PX86PD)MMPagePhys2Page(pVM, cr3 & X86_CR3_PAGE_MASK);
    if (!pPD)
    {
        pHlp->pfnPrintf(pHlp, "Page directory at %#x was not found in the page pool!\n", cr3 & X86_CR3_PAGE_MASK);
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;
    for (unsigned i = 0; i < ELEMENTS(pPD->a); i++)
    {
        X86PDE Pde = pPD->a[i];
        if (Pde.n.u1Present)
        {
            const uint32_t u32Address = i << X86_PD_SHIFT;
            if ((cr4 & X86_CR4_PSE) && Pde.b.u1Size)
                pHlp->pfnPrintf(pHlp,      /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                                "%08x 0 |    P %c %c %c %c %c %s %s %s .. 4M %c%c%c  %08x\n",
                                u32Address,
                                Pde.b.u1Write       ? 'W'  : 'R',
                                Pde.b.u1User        ? 'U'  : 'S',
                                Pde.b.u1Accessed    ? 'A'  : '-',
                                Pde.b.u1Dirty       ? 'D'  : '-',
                                Pde.b.u1Global      ? 'G'  : '-',
                                Pde.b.u1WriteThru   ? "WT" : "--",
                                Pde.b.u1CacheDisable? "CD" : "--",
                                Pde.b.u1PAT         ? "AT" : "--",
                                Pde.u & RT_BIT_64(9)                ? '1' : '0',
                                Pde.u & PGM_PDFLAGS_MAPPING     ? 'm' : '-',
                                Pde.u & PGM_PDFLAGS_TRACK_DIRTY ? 'd' : '-',
                                Pde.u & X86_PDE4M_PG_MASK);
            else
            {
                pHlp->pfnPrintf(pHlp,      /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                                "%08x 0 |    P %c %c %c %c %c %s %s .. .. 4K %c%c%c  %08x\n",
                                u32Address,
                                Pde.n.u1Write       ? 'W'  : 'R',
                                Pde.n.u1User        ? 'U'  : 'S',
                                Pde.n.u1Accessed    ? 'A'  : '-',
                                Pde.n.u1Reserved0   ? '?'  : '.', /* ignored */
                                Pde.n.u1Reserved1   ? '?'  : '.', /* ignored */
                                Pde.n.u1WriteThru   ? "WT" : "--",
                                Pde.n.u1CacheDisable? "CD" : "--",
                                Pde.u & RT_BIT_64(9)                ? '1' : '0',
                                Pde.u & PGM_PDFLAGS_MAPPING     ? 'm' : '-',
                                Pde.u & PGM_PDFLAGS_TRACK_DIRTY ? 'd' : '-',
                                Pde.u & X86_PDE_PG_MASK);
                if (cMaxDepth >= 1)
                {
                    /** @todo what about using the page pool for mapping PTs? */
                    RTHCPHYS HCPhys = Pde.u & X86_PDE_PG_MASK;
                    PX86PT pPT = NULL;
                    if (!(Pde.u & PGM_PDFLAGS_MAPPING))
                        pPT = (PX86PT)MMPagePhys2Page(pVM, HCPhys);
                    else
                    {
                        for (PPGMMAPPING pMap = pVM->pgm.s.pMappingsR3; pMap; pMap = pMap->pNextR3)
                            if (u32Address - pMap->GCPtr < pMap->cb)
                            {
                                int iPDE = (u32Address - pMap->GCPtr) >> X86_PD_SHIFT;
                                if (pMap->aPTs[iPDE].HCPhysPT != HCPhys)
                                    pHlp->pfnPrintf(pHlp, "%08x error! Mapping error! PT %d has HCPhysPT=%VHp not %VHp is in the PD.\n",
                                                    u32Address, iPDE, pMap->aPTs[iPDE].HCPhysPT, HCPhys);
                                pPT = pMap->aPTs[iPDE].pPTR3;
                            }
                    }
                    int rc2 = VERR_INVALID_PARAMETER;
                    if (pPT)
                        rc2 = pgmR3DumpHierarchyHC32BitPT(pVM, pPT, u32Address, pHlp);
                    else
                        pHlp->pfnPrintf(pHlp, "%08x error! Page table at %#x was not found in the page pool!\n", u32Address, HCPhys);
                    if (rc2 < rc && VBOX_SUCCESS(rc))
                        rc = rc2;
                }
            }
        }
    }

    return rc;
}


/**
 * Dumps a 32-bit shadow page table.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   pPT         Pointer to the page table.
 * @param   u32Address  The virtual address this table starts at.
 * @param   PhysSearch  Address to search for.
 */
int pgmR3DumpHierarchyGC32BitPT(PVM pVM, PX86PT pPT, uint32_t u32Address, RTGCPHYS PhysSearch)
{
    for (unsigned i = 0; i < ELEMENTS(pPT->a); i++)
    {
        X86PTE Pte = pPT->a[i];
        if (Pte.n.u1Present)
        {
            Log((           /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                 "%08x 1  |   P %c %c %c %c %c %s %s %s .. 4K %c%c%c  %08x\n",
                 u32Address + (i << X86_PT_SHIFT),
                 Pte.n.u1Write       ? 'W'  : 'R',
                 Pte.n.u1User        ? 'U'  : 'S',
                 Pte.n.u1Accessed    ? 'A'  : '-',
                 Pte.n.u1Dirty       ? 'D'  : '-',
                 Pte.n.u1Global      ? 'G'  : '-',
                 Pte.n.u1WriteThru   ? "WT" : "--",
                 Pte.n.u1CacheDisable? "CD" : "--",
                 Pte.n.u1PAT         ? "AT" : "--",
                 Pte.u & PGM_PTFLAGS_TRACK_DIRTY     ? 'd' : '-',
                 Pte.u & RT_BIT(10)                     ? '1' : '0',
                 Pte.u & PGM_PTFLAGS_CSAM_VALIDATED  ? 'v' : '-',
                 Pte.u & X86_PDE_PG_MASK));

            if ((Pte.u & X86_PDE_PG_MASK) == PhysSearch)
            {
                uint64_t fPageShw = 0;
                RTHCPHYS pPhysHC = 0;

                PGMShwGetPage(pVM, (RTGCPTR)(u32Address + (i << X86_PT_SHIFT)), &fPageShw, &pPhysHC);
                Log(("Found %VGp at %VGv -> flags=%llx\n", PhysSearch, (RTGCPTR)(u32Address + (i << X86_PT_SHIFT)), fPageShw));
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * Dumps a 32-bit guest page directory and page tables.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   cr4         The CR4, PSE is currently used.
 * @param   PhysSearch  Address to search for.
 */
PGMR3DECL(int) PGMR3DumpHierarchyGC(PVM pVM, uint32_t cr3, uint32_t cr4, RTGCPHYS PhysSearch)
{
    bool fLongMode = false;
    const unsigned cch = fLongMode ? 16 : 8; NOREF(cch);
    PX86PD pPD = 0;

    int rc = PGM_GCPHYS_2_PTR(pVM, cr3 & X86_CR3_PAGE_MASK, &pPD);
    if (VBOX_FAILURE(rc) || !pPD)
    {
        Log(("Page directory at %#x was not found in the page pool!\n", cr3 & X86_CR3_PAGE_MASK));
        return VERR_INVALID_PARAMETER;
    }

    Log(("cr3=%08x cr4=%08x%s\n"
         "%-*s        P - Present\n"
         "%-*s        | R/W - Read (0) / Write (1)\n"
         "%-*s        | | U/S - User (1) / Supervisor (0)\n"
         "%-*s        | | | A - Accessed\n"
         "%-*s        | | | | D - Dirty\n"
         "%-*s        | | | | | G - Global\n"
         "%-*s        | | | | | | WT - Write thru\n"
         "%-*s        | | | | | | |  CD - Cache disable\n"
         "%-*s        | | | | | | |  |  AT - Attribute table (PAT)\n"
         "%-*s        | | | | | | |  |  |  NX - No execute (K8)\n"
         "%-*s        | | | | | | |  |  |  |  4K/4M/2M - Page size.\n"
         "%-*s        | | | | | | |  |  |  |  |  AVL - a=allocated; m=mapping; d=track dirty;\n"
         "%-*s        | | | | | | |  |  |  |  |  |     p=permanent; v=validated;\n"
         "%-*s Level  | | | | | | |  |  |  |  |  |    Page\n"
       /* xxxx n **** P R S A D G WT CD AT NX 4M AVL xxxxxxxxxxxxx
                      - W U - - - -- -- -- -- -- 010 */
         , cr3, cr4, fLongMode ? " Long Mode" : "",
         cch, "", cch, "", cch, "", cch, "", cch, "", cch, "", cch, "",
         cch, "", cch, "", cch, "", cch, "", cch, "", cch, "", cch, "Address"));

    for (unsigned i = 0; i < ELEMENTS(pPD->a); i++)
    {
        X86PDE Pde = pPD->a[i];
        if (Pde.n.u1Present)
        {
            const uint32_t u32Address = i << X86_PD_SHIFT;

            if ((cr4 & X86_CR4_PSE) && Pde.b.u1Size)
                Log((           /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                     "%08x 0 |    P %c %c %c %c %c %s %s %s .. 4M %c%c%c  %08x\n",
                     u32Address,
                     Pde.b.u1Write       ? 'W'  : 'R',
                     Pde.b.u1User        ? 'U'  : 'S',
                     Pde.b.u1Accessed    ? 'A'  : '-',
                     Pde.b.u1Dirty       ? 'D'  : '-',
                     Pde.b.u1Global      ? 'G'  : '-',
                     Pde.b.u1WriteThru   ? "WT" : "--",
                     Pde.b.u1CacheDisable? "CD" : "--",
                     Pde.b.u1PAT         ? "AT" : "--",
                     Pde.u & RT_BIT(9)      ? '1' : '0',
                     Pde.u & RT_BIT(10)     ? '1' : '0',
                     Pde.u & RT_BIT(11)     ? '1' : '0',
                     Pde.u & X86_PDE4M_PG_MASK));
            /** @todo PhysSearch */
            else
            {
                Log((           /*P R  S  A  D  G  WT CD AT NX 4M a m d  */
                     "%08x 0 |    P %c %c %c %c %c %s %s .. .. 4K %c%c%c  %08x\n",
                     u32Address,
                     Pde.n.u1Write       ? 'W'  : 'R',
                     Pde.n.u1User        ? 'U'  : 'S',
                     Pde.n.u1Accessed    ? 'A'  : '-',
                     Pde.n.u1Reserved0   ? '?'  : '.', /* ignored */
                     Pde.n.u1Reserved1   ? '?'  : '.', /* ignored */
                     Pde.n.u1WriteThru   ? "WT" : "--",
                     Pde.n.u1CacheDisable? "CD" : "--",
                     Pde.u & RT_BIT(9)      ? '1' : '0',
                     Pde.u & RT_BIT(10)     ? '1' : '0',
                     Pde.u & RT_BIT(11)     ? '1' : '0',
                     Pde.u & X86_PDE_PG_MASK));
                ////if (cMaxDepth >= 1)
                {
                    /** @todo what about using the page pool for mapping PTs? */
                    RTGCPHYS GCPhys = Pde.u & X86_PDE_PG_MASK;
                    PX86PT pPT = NULL;

                    rc = PGM_GCPHYS_2_PTR(pVM, GCPhys, &pPT);

                    int rc2 = VERR_INVALID_PARAMETER;
                    if (pPT)
                        rc2 = pgmR3DumpHierarchyGC32BitPT(pVM, pPT, u32Address, PhysSearch);
                    else
                        Log(("%08x error! Page table at %#x was not found in the page pool!\n", u32Address, GCPhys));
                    if (rc2 < rc && VBOX_SUCCESS(rc))
                        rc = rc2;
                }
            }
        }
    }

    return rc;
}


/**
 * Dumps a page table hierarchy use only physical addresses and cr4/lm flags.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   cr4         The cr4, only PAE and PSE is currently used.
 * @param   fLongMode   Set if long mode, false if not long mode.
 * @param   cMaxDepth   Number of levels to dump.
 * @param   pHlp        Pointer to the output functions.
 */
PGMR3DECL(int) PGMR3DumpHierarchyHC(PVM pVM, uint32_t cr3, uint32_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
{
    if (!pHlp)
        pHlp = DBGFR3InfoLogHlp();
    if (!cMaxDepth)
        return VINF_SUCCESS;
    const unsigned cch = fLongMode ? 16 : 8;
    pHlp->pfnPrintf(pHlp,
                    "cr3=%08x cr4=%08x%s\n"
                    "%-*s        P - Present\n"
                    "%-*s        | R/W - Read (0) / Write (1)\n"
                    "%-*s        | | U/S - User (1) / Supervisor (0)\n"
                    "%-*s        | | | A - Accessed\n"
                    "%-*s        | | | | D - Dirty\n"
                    "%-*s        | | | | | G - Global\n"
                    "%-*s        | | | | | | WT - Write thru\n"
                    "%-*s        | | | | | | |  CD - Cache disable\n"
                    "%-*s        | | | | | | |  |  AT - Attribute table (PAT)\n"
                    "%-*s        | | | | | | |  |  |  NX - No execute (K8)\n"
                    "%-*s        | | | | | | |  |  |  |  4K/4M/2M - Page size.\n"
                    "%-*s        | | | | | | |  |  |  |  |  AVL - a=allocated; m=mapping; d=track dirty;\n"
                    "%-*s        | | | | | | |  |  |  |  |  |     p=permanent; v=validated;\n"
                    "%-*s Level  | | | | | | |  |  |  |  |  |    Page\n"
                  /* xxxx n **** P R S A D G WT CD AT NX 4M AVL xxxxxxxxxxxxx
                                 - W U - - - -- -- -- -- -- 010 */
                    , cr3, cr4, fLongMode ? " Long Mode" : "",
                    cch, "", cch, "", cch, "", cch, "", cch, "", cch, "", cch, "",
                    cch, "", cch, "", cch, "", cch, "", cch, "", cch, "", cch, "Address");
    if (cr4 & X86_CR4_PAE)
    {
        if (fLongMode)
            return pgmR3DumpHierarchyHcPaePML4(pVM, cr3 & X86_CR3_PAGE_MASK, cr4, cMaxDepth, pHlp);
        return pgmR3DumpHierarchyHCPaePDPTR(pVM, cr3 & X86_CR3_PAE_PAGE_MASK, 0, cr4, false, cMaxDepth, pHlp);
    }
    return pgmR3DumpHierarchyHC32BitPD(pVM, cr3 & X86_CR3_PAGE_MASK, cr4, cMaxDepth, pHlp);
}



#ifdef VBOX_WITH_DEBUGGER
/**
 * The '.pgmram' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) pgmR3CmdRam(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");
    if (!pVM->pgm.s.pRamRangesGC)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Sorry, no Ram is registered.\n");

    /*
     * Dump the ranges.
     */
    int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "From     - To (incl) pvHC\n");
    PPGMRAMRANGE pRam;
    for (pRam = pVM->pgm.s.pRamRangesHC; pRam; pRam = pRam->pNextHC)
    {
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
            "%VGp - %VGp  %p\n",
            pRam->GCPhys, pRam->GCPhysLast, pRam->pvHC);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * The '.pgmmap' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) pgmR3CmdMap(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");
    if (!pVM->pgm.s.pMappingsR3)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Sorry, no mappings are registered.\n");

    /*
     * Print message about the fixedness of the mappings.
     */
    int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, pVM->pgm.s.fMappingsFixed ? "The mappings are FIXED.\n" : "The mappings are FLOATING.\n");
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Dump the ranges.
     */
    PPGMMAPPING pCur;
    for (pCur = pVM->pgm.s.pMappingsR3; pCur; pCur = pCur->pNextR3)
    {
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
            "%08x - %08x %s\n",
            pCur->GCPtr, pCur->GCPtrLast, pCur->pszDesc);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * The '.pgmsync' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) pgmR3CmdSync(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");

    /*
     * Force page directory sync.
     */
    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);

    int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Forcing page directory sync.\n");
    if (VBOX_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


/**
 * The '.pgmsyncalways' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) pgmR3CmdSyncAlways(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires VM to be selected.\n");

    /*
     * Force page directory sync.
     */
    if (pVM->pgm.s.fSyncFlags & PGM_SYNC_ALWAYS)
    {
        ASMAtomicAndU32(&pVM->pgm.s.fSyncFlags, ~PGM_SYNC_ALWAYS);
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Disabled permanent forced page directory syncing.\n");
    }
    else
    {
        ASMAtomicOrU32(&pVM->pgm.s.fSyncFlags, PGM_SYNC_ALWAYS);
        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Enabled permanent forced page directory syncing.\n");
    }
}

#endif

/**
 * pvUser argument of the pgmR3CheckIntegrity*Node callbacks.
 */
typedef struct PGMCHECKINTARGS
{
    bool                    fLeftToRight;    /**< true: left-to-right; false: right-to-left. */
    PPGMPHYSHANDLER         pPrevPhys;
    PPGMVIRTHANDLER         pPrevVirt;
    PPGMPHYS2VIRTHANDLER   pPrevPhys2Virt;
    PVM                     pVM;
} PGMCHECKINTARGS, *PPGMCHECKINTARGS;

/**
 * Validate a node in the physical handler tree.
 *
 * @returns 0 on if ok, other wise 1.
 * @param   pNode       The handler node.
 * @param   pvUser      pVM.
 */
static DECLCALLBACK(int) pgmR3CheckIntegrityPhysHandlerNode(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PPGMCHECKINTARGS pArgs = (PPGMCHECKINTARGS)pvUser;
    PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)pNode;
    AssertReleaseReturn(!((uintptr_t)pCur & 7), 1);
    AssertReleaseMsg(pCur->Core.Key <= pCur->Core.KeyLast,("pCur=%p %VGp-%VGp %s\n", pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
    AssertReleaseMsg(   !pArgs->pPrevPhys
                     || (pArgs->fLeftToRight ? pArgs->pPrevPhys->Core.KeyLast < pCur->Core.Key : pArgs->pPrevPhys->Core.KeyLast > pCur->Core.Key),
                     ("pPrevPhys=%p %VGp-%VGp %s\n"
                      "     pCur=%p %VGp-%VGp %s\n",
                      pArgs->pPrevPhys, pArgs->pPrevPhys->Core.Key, pArgs->pPrevPhys->Core.KeyLast, pArgs->pPrevPhys->pszDesc,
                      pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
    pArgs->pPrevPhys = pCur;
    return 0;
}


/**
 * Validate a node in the virtual handler tree.
 *
 * @returns 0 on if ok, other wise 1.
 * @param   pNode       The handler node.
 * @param   pvUser      pVM.
 */
static DECLCALLBACK(int) pgmR3CheckIntegrityVirtHandlerNode(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMCHECKINTARGS pArgs = (PPGMCHECKINTARGS)pvUser;
    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)pNode;
    AssertReleaseReturn(!((uintptr_t)pCur & 7), 1);
    AssertReleaseMsg(pCur->Core.Key <= pCur->Core.KeyLast,("pCur=%p %VGv-%VGv %s\n", pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
    AssertReleaseMsg(   !pArgs->pPrevVirt
                     || (pArgs->fLeftToRight ? pArgs->pPrevVirt->Core.KeyLast < pCur->Core.Key : pArgs->pPrevVirt->Core.KeyLast > pCur->Core.Key),
                     ("pPrevVirt=%p %VGv-%VGv %s\n"
                      "     pCur=%p %VGv-%VGv %s\n",
                      pArgs->pPrevVirt, pArgs->pPrevVirt->Core.Key, pArgs->pPrevVirt->Core.KeyLast, pArgs->pPrevVirt->pszDesc,
                      pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
    for (unsigned iPage = 0; iPage < pCur->cPages; iPage++)
    {
        AssertReleaseMsg(pCur->aPhysToVirt[iPage].offVirtHandler == -RT_OFFSETOF(PGMVIRTHANDLER, aPhysToVirt[iPage]),
                         ("pCur=%p %VGv-%VGv %s\n"
                          "iPage=%d offVirtHandle=%#x expected %#x\n",
                          pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc,
                          iPage, pCur->aPhysToVirt[iPage].offVirtHandler, -RT_OFFSETOF(PGMVIRTHANDLER, aPhysToVirt[iPage])));
    }
    pArgs->pPrevVirt = pCur;
    return 0;
}


/**
 * Validate a node in the virtual handler tree.
 *
 * @returns 0 on if ok, other wise 1.
 * @param   pNode       The handler node.
 * @param   pvUser      pVM.
 */
static DECLCALLBACK(int) pgmR3CheckIntegrityPhysToVirtHandlerNode(PAVLROGCPHYSNODECORE pNode, void *pvUser)
{
    PPGMCHECKINTARGS pArgs = (PPGMCHECKINTARGS)pvUser;
    PPGMPHYS2VIRTHANDLER pCur = (PPGMPHYS2VIRTHANDLER)pNode;
    AssertReleaseMsgReturn(!((uintptr_t)pCur & 3),      ("\n"), 1);
    AssertReleaseMsgReturn(!(pCur->offVirtHandler & 3), ("\n"), 1);
    AssertReleaseMsg(pCur->Core.Key <= pCur->Core.KeyLast,("pCur=%p %VGp-%VGp\n", pCur, pCur->Core.Key, pCur->Core.KeyLast));
    AssertReleaseMsg(   !pArgs->pPrevPhys2Virt
                     || (pArgs->fLeftToRight ? pArgs->pPrevPhys2Virt->Core.KeyLast < pCur->Core.Key : pArgs->pPrevPhys2Virt->Core.KeyLast > pCur->Core.Key),
                     ("pPrevPhys2Virt=%p %VGp-%VGp\n"
                      "          pCur=%p %VGp-%VGp\n",
                      pArgs->pPrevPhys2Virt, pArgs->pPrevPhys2Virt->Core.Key, pArgs->pPrevPhys2Virt->Core.KeyLast,
                      pCur, pCur->Core.Key, pCur->Core.KeyLast));
    AssertReleaseMsg(   !pArgs->pPrevPhys2Virt
                     || (pArgs->fLeftToRight ? pArgs->pPrevPhys2Virt->Core.KeyLast < pCur->Core.Key : pArgs->pPrevPhys2Virt->Core.KeyLast > pCur->Core.Key),
                     ("pPrevPhys2Virt=%p %VGp-%VGp\n"
                      "          pCur=%p %VGp-%VGp\n",
                      pArgs->pPrevPhys2Virt, pArgs->pPrevPhys2Virt->Core.Key, pArgs->pPrevPhys2Virt->Core.KeyLast,
                      pCur, pCur->Core.Key, pCur->Core.KeyLast));
    AssertReleaseMsg((pCur->offNextAlias & (PGMPHYS2VIRTHANDLER_IN_TREE | PGMPHYS2VIRTHANDLER_IS_HEAD)) == (PGMPHYS2VIRTHANDLER_IN_TREE | PGMPHYS2VIRTHANDLER_IS_HEAD),
                     ("pCur=%p:{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                      pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias));
    if (pCur->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK)
    {
        PPGMPHYS2VIRTHANDLER pCur2 = pCur;
        for (;;)
        {
            pCur2 = (PPGMPHYS2VIRTHANDLER)((intptr_t)pCur + (pCur->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
            AssertReleaseMsg(pCur2 != pCur,
                             (" pCur=%p:{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                              pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias));
            AssertReleaseMsg((pCur2->offNextAlias & (PGMPHYS2VIRTHANDLER_IN_TREE | PGMPHYS2VIRTHANDLER_IS_HEAD)) == PGMPHYS2VIRTHANDLER_IN_TREE,
                             (" pCur=%p:{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n"
                              "pCur2=%p:{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                              pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias,
                              pCur2, pCur2->Core.Key, pCur2->Core.KeyLast, pCur2->offVirtHandler, pCur2->offNextAlias));
            AssertReleaseMsg((pCur2->Core.Key ^ pCur->Core.Key) < PAGE_SIZE,
                             (" pCur=%p:{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n"
                              "pCur2=%p:{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                              pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias,
                              pCur2, pCur2->Core.Key, pCur2->Core.KeyLast, pCur2->offVirtHandler, pCur2->offNextAlias));
            AssertReleaseMsg((pCur2->Core.KeyLast ^ pCur->Core.KeyLast) < PAGE_SIZE,
                             (" pCur=%p:{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n"
                              "pCur2=%p:{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                              pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias,
                              pCur2, pCur2->Core.Key, pCur2->Core.KeyLast, pCur2->offVirtHandler, pCur2->offNextAlias));
            if (!(pCur2->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK))
                break;
        }
    }

    pArgs->pPrevPhys2Virt = pCur;
    return 0;
}


/**
 * Perform an integrity check on the PGM component.
 *
 * @returns VINF_SUCCESS if everything is fine.
 * @returns VBox error status after asserting on integrity breach.
 * @param   pVM     The VM handle.
 */
PDMR3DECL(int) PGMR3CheckIntegrity(PVM pVM)
{
    AssertReleaseReturn(pVM->pgm.s.offVM, VERR_INTERNAL_ERROR);

    /*
     * Check the trees.
     */
    int cErrors = 0;
    PGMCHECKINTARGS Args = { true, NULL, NULL, NULL, pVM };
    cErrors += RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesHC->PhysHandlers,       true,  pgmR3CheckIntegrityPhysHandlerNode, &Args);
    Args.fLeftToRight = false;
    cErrors += RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesHC->PhysHandlers,       false, pgmR3CheckIntegrityPhysHandlerNode, &Args);
    Args.fLeftToRight = true;
    cErrors += RTAvlroGCPtrDoWithAll( &pVM->pgm.s.pTreesHC->VirtHandlers,       true,  pgmR3CheckIntegrityVirtHandlerNode, &Args);
    Args.fLeftToRight = false;
    cErrors += RTAvlroGCPtrDoWithAll( &pVM->pgm.s.pTreesHC->VirtHandlers,       false, pgmR3CheckIntegrityVirtHandlerNode, &Args);
    Args.fLeftToRight = true;
    cErrors += RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesHC->PhysToVirtHandlers, true,  pgmR3CheckIntegrityPhysToVirtHandlerNode, &Args);
    Args.fLeftToRight = false;
    cErrors += RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesHC->PhysToVirtHandlers, false, pgmR3CheckIntegrityPhysToVirtHandlerNode, &Args);

    return !cErrors ? VINF_SUCCESS : VERR_INTERNAL_ERROR;
}


/**
 * Inform PGM if we want all mappings to be put into the shadow page table. (necessary for e.g. VMX)
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   fEnable     Enable or disable shadow mappings
 */
PGMR3DECL(int) PGMR3ChangeShwPDMappings(PVM pVM, bool fEnable)
{
    pVM->pgm.s.fDisableMappings = !fEnable;

    size_t cb;
    int rc = PGMR3MappingsSize(pVM, &cb);
    AssertRCReturn(rc, rc);

    /* Pretend the mappings are now fixed; to force a refresh of the reserved PDEs. */
    rc = PGMR3MappingsFix(pVM, MM_HYPER_AREA_ADDRESS, cb);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}
