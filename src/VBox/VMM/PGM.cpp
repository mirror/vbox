/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor. (Mixing stuff here, not good?)
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


/** @page pg_pgm PGM - The Page Manager and Monitor
 *
 * @see grp_pgm,
 * @ref pg_pgm_pool,
 * @ref pg_pgm_phys.
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
 * Because of guest context mappings requires PDPT and PML4 entries to allow
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
 * hypervisor location. The identity mapped location is for when the world
 * switches that involves disabling paging.
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
 *
 *
 * @section         sec_pgm_handlers        Access Handlers
 *
 * Placeholder.
 *
 *
 * @subsection      sec_pgm_handlers_virt   Virtual Access Handlers
 *
 * Placeholder.
 *
 *
 * @subsection      sec_pgm_handlers_virt   Virtual Access Handlers
 *
 * We currently implement three types of virtual access handlers:  ALL, WRITE
 * and HYPERVISOR (WRITE). See PGMVIRTHANDLERTYPE for some more details.
 *
 * The HYPERVISOR access handlers is kept in a separate tree since it doesn't apply
 * to physical pages (PGMTREES::HyperVirtHandlers) and only needs to be consulted in
 * a special \#PF case. The ALL and WRITE are in the PGMTREES::VirtHandlers tree, the
 * rest of this section is going to be about these handlers.
 *
 * We'll go thru the life cycle of a handler and try make sense of it all, don't know
 * how successfull this is gonna be...
 *
 * 1. A handler is registered thru the PGMR3HandlerVirtualRegister and
 * PGMHandlerVirtualRegisterEx APIs. We check for conflicting virtual handlers
 * and create a new node that is inserted into the AVL tree (range key). Then
 * a full PGM resync is flagged (clear pool, sync cr3, update virtual bit of PGMPAGE).
 *
 * 2. The following PGMSyncCR3/SyncCR3 operation will first make invoke HandlerVirtualUpdate.
 *
 * 2a. HandlerVirtualUpdate will will lookup all the pages covered by virtual handlers
 * via the current guest CR3 and update the physical page -> virtual handler
 * translation. Needless to say, this doesn't exactly scale very well. If any changes
 * are detected, it will flag a virtual bit update just like we did on registration.
 * PGMPHYS pages with changes will have their virtual handler state reset to NONE.
 *
 * 2b. The virtual bit update process will iterate all the pages covered by all the
 * virtual handlers and update the PGMPAGE virtual handler state to the max of all
 * virtual handlers on that page.
 *
 * 2c. Back in SyncCR3 we will now flush the entire shadow page cache to make sure
 * we don't miss any alias mappings of the monitored pages.
 *
 * 2d. SyncCR3 will then proceed with syncing the CR3 table.
 *
 * 3. \#PF(np,read) on a page in the range. This will cause it to be synced
 * read-only and resumed if it's a WRITE handler. If it's an ALL handler we
 * will call the handlers like in the next step. If the physical mapping has
 * changed we will - some time in the future - perform a handler callback
 * (optional) and update the physical -> virtual handler cache.
 *
 * 4. \#PF(,write) on a page in the range. This will cause the handler to
 * be invoked.
 *
 * 5. The guest invalidates the page and changes the physical backing or
 * unmaps it. This should cause the invalidation callback to be invoked
 * (it might not yet be 100% perfect). Exactly what happens next... is
 * this where we mess up and end up out of sync for a while?
 *
 * 6. The handler is deregistered by the client via PGMHandlerVirtualDeregister.
 * We will then set all PGMPAGEs in the physical -> virtual handler cache for
 * this handler to NONE and trigger a full PGM resync (basically the same
 * as int step 1). Which means 2 is executed again.
 *
 *
 * @subsubsection   sub_sec_pgm_handler_virt_todo   TODOs
 *
 * There is a bunch of things that needs to be done to make the virtual handlers
 * work 100% correctly and work more efficiently.
 *
 * The first bit hasn't been implemented yet because it's going to slow the
 * whole mess down even more, and besides it seems to be working reliably for
 * our current uses. OTOH, some of the optimizations might end up more or less
 * implementing the missing bits, so we'll see.
 *
 * On the optimization side, the first thing to do is to try avoid unnecessary
 * cache flushing. Then try team up with the shadowing code to track changes
 * in mappings by means of access to them (shadow in), updates to shadows pages,
 * invlpg, and shadow PT discarding (perhaps).
 *
 * Some idea that have popped up for optimization for current and new features:
 *    - bitmap indicating where there are virtual handlers installed.
 *      (4KB => 2**20 pages, page 2**12 => covers 32-bit address space 1:1!)
 *    - Further optimize this by min/max (needs min/max avl getters).
 *    - Shadow page table entry bit (if any left)?
 *
 */


/** @page pg_pgm_phys   PGM Physical Guest Memory Management
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
 *      - PGMPhysSimpleWriteGCPhys and PGMPhysWrite.
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
#ifdef DEBUG_bird
# include <iprt/env.h>
#endif
#include <VBox/param.h>
#include <VBox/err.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Saved state data unit version for 2.5.x and later. */
#define PGM_SAVED_STATE_VERSION                 9
/** Saved state data unit version for 2.2.2 and later. */
#define PGM_SAVED_STATE_VERSION_2_2_2           8
/** Saved state data unit version for 2.2.0. */
#define PGM_SAVED_STATE_VERSION_RR_DESC         7
/** Saved state data unit version. */
#define PGM_SAVED_STATE_VERSION_OLD_PHYS_CODE   6


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int                pgmR3InitPaging(PVM pVM);
static void               pgmR3InitStats(PVM pVM);
static DECLCALLBACK(void) pgmR3PhysInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) pgmR3InfoMode(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) pgmR3InfoCr3(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(int)  pgmR3RelocatePhysHandler(PAVLROGCPHYSNODECORE pNode, void *pvUser);
static DECLCALLBACK(int)  pgmR3RelocateVirtHandler(PAVLROGCPTRNODECORE pNode, void *pvUser);
static DECLCALLBACK(int)  pgmR3RelocateHyperVirtHandler(PAVLROGCPTRNODECORE pNode, void *pvUser);
#ifdef VBOX_STRICT
static DECLCALLBACK(void) pgmR3ResetNoMorePhysWritesFlag(PVM pVM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser);
#endif
static DECLCALLBACK(int)  pgmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  pgmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static int                pgmR3ModeDataInit(PVM pVM, bool fResolveGCAndR0);
static void               pgmR3ModeDataSwitch(PVM pVM, PVMCPU pVCpu, PGMMODE enmShw, PGMMODE enmGst);
static PGMMODE            pgmR3CalcShadowMode(PVM pVM, PGMMODE enmGuestMode, SUPPAGINGMODE enmHostMode, PGMMODE enmShadowMode, VMMSWITCHER *penmSwitcher);

#ifdef VBOX_WITH_DEBUGGER
/** @todo Convert the first two commands to 'info' items. */
static DECLCALLBACK(int)  pgmR3CmdRam(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int)  pgmR3CmdMap(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int)  pgmR3CmdError(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int)  pgmR3CmdSync(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
static DECLCALLBACK(int)  pgmR3CmdSyncAlways(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
# ifdef VBOX_STRICT
static DECLCALLBACK(int)  pgmR3CmdAssertCR3(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult);
# endif
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef VBOX_WITH_DEBUGGER
/** Argument descriptors for '.pgmerror' and '.pgmerroroff'. */
static const DBGCVARDESC g_aPgmErrorArgs[] =
{
    /* cTimesMin,   cTimesMax,  enmCategory,            fFlags,                         pszName,        pszDescription */
    {  0,           1,          DBGCVAR_CAT_STRING,     0,                              "where",        "Error injection location." },
};

/** Command descriptors. */
static const DBGCCMD    g_aCmds[] =
{
    /* pszCmd,  cArgsMin, cArgsMax, paArgDesc,          cArgDescs,                  pResultDesc,        fFlags,     pfnHandler          pszSyntax,          ....pszDescription */
    { "pgmram",        0, 0,        NULL,               0,                          NULL,               0,          pgmR3CmdRam,        "",                     "Display the ram ranges." },
    { "pgmmap",        0, 0,        NULL,               0,                          NULL,               0,          pgmR3CmdMap,        "",                     "Display the mapping ranges." },
    { "pgmsync",       0, 0,        NULL,               0,                          NULL,               0,          pgmR3CmdSync,       "",                     "Sync the CR3 page." },
    { "pgmerror",      0, 1,        &g_aPgmErrorArgs[0],1,                          NULL,               0,          pgmR3CmdError,      "",                     "Enables inject runtime of errors into parts of PGM." },
    { "pgmerroroff",   0, 1,        &g_aPgmErrorArgs[0],1,                          NULL,               0,          pgmR3CmdError,      "",                     "Disables inject runtime errors into parts of PGM." },
#ifdef VBOX_STRICT
    { "pgmassertcr3",  0, 0,        NULL,               0,                          NULL,               0,          pgmR3CmdAssertCR3,  "",                     "Check the shadow CR3 mapping." },
#endif
    { "pgmsyncalways", 0, 0,        NULL,               0,                          NULL,               0,          pgmR3CmdSyncAlways, "",                     "Toggle permanent CR3 syncing." },
};
#endif




/*
 * Shadow - 32-bit mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_32BIT
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_32BIT(name)
#define PGM_SHW_NAME_RC_STR(name)   PGM_SHW_NAME_RC_32BIT_STR(name)
#define PGM_SHW_NAME_R0_STR(name)   PGM_SHW_NAME_R0_32BIT_STR(name)
#include "PGMShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_REAL_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_REAL_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_REAL(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_32BIT_REAL_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_32BIT_REAL_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_PHYS
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_32BIT_PD_PHYS
#include "PGMBth.h"
#include "PGMGstDefs.h"
#include "PGMGst.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_PROT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PROT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_PROT(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_32BIT_PROT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_32BIT_PROT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_PHYS
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_32BIT_PD_PHYS
#include "PGMBth.h"
#include "PGMGstDefs.h"
#include "PGMGst.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_32BIT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_32BIT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_32BIT(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_32BIT_32BIT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_32BIT_32BIT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_32BIT_PD
#include "PGMBth.h"
#include "PGMGstDefs.h"
#include "PGMGst.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#undef PGM_SHW_NAME_RC_STR
#undef PGM_SHW_NAME_R0_STR


/*
 * Shadow - PAE mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_PAE
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_PAE(name)
#define PGM_SHW_NAME_RC_STR(name)   PGM_SHW_NAME_RC_PAE_STR(name)
#define PGM_SHW_NAME_R0_STR(name)   PGM_SHW_NAME_R0_PAE_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#include "PGMShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_REAL_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_REAL_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_PAE_REAL_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_PAE_REAL_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_PAE_PDPT_PHYS
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_PROT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PROT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PROT(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_PAE_PROT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_PAE_PROT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_PAE_PDPT_PHYS
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_32BIT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_32BIT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_32BIT(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_PAE_32BIT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_PAE_32BIT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_32BIT_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_PAE_PDPT_FOR_32BIT
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - PAE mode */
#define PGM_GST_TYPE                PGM_TYPE_PAE
#define PGM_GST_NAME(name)          PGM_GST_NAME_PAE(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_PAE_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PAE_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PAE(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_PAE_PAE_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_PAE_PAE_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PAE_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_PAE_PDPT
#include "PGMBth.h"
#include "PGMGstDefs.h"
#include "PGMGst.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#undef PGM_SHW_NAME_RC_STR
#undef PGM_SHW_NAME_R0_STR


/*
 * Shadow - AMD64 mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_AMD64
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_AMD64(name)
#define PGM_SHW_NAME_RC_STR(name)   PGM_SHW_NAME_RC_AMD64_STR(name)
#define PGM_SHW_NAME_R0_STR(name)   PGM_SHW_NAME_R0_AMD64_STR(name)
#include "PGMShw.h"

#ifdef VBOX_WITH_64_BITS_GUESTS
/* Guest - AMD64 mode */
# define PGM_GST_TYPE               PGM_TYPE_AMD64
# define PGM_GST_NAME(name)         PGM_GST_NAME_AMD64(name)
# define PGM_GST_NAME_RC_STR(name)  PGM_GST_NAME_RC_AMD64_STR(name)
# define PGM_GST_NAME_R0_STR(name)  PGM_GST_NAME_R0_AMD64_STR(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_AMD64_AMD64(name)
# define PGM_BTH_NAME_RC_STR(name)  PGM_BTH_NAME_RC_AMD64_AMD64_STR(name)
# define PGM_BTH_NAME_R0_STR(name)  PGM_BTH_NAME_R0_AMD64_AMD64_STR(name)
# define BTH_PGMPOOLKIND_PT_FOR_PT  PGMPOOLKIND_PAE_PT_FOR_PAE_PT
# define BTH_PGMPOOLKIND_PT_FOR_BIG PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
# define BTH_PGMPOOLKIND_ROOT       PGMPOOLKIND_64BIT_PML4
# include "PGMBth.h"
# include "PGMGstDefs.h"
# include "PGMGst.h"
# undef BTH_PGMPOOLKIND_PT_FOR_BIG
# undef BTH_PGMPOOLKIND_PT_FOR_PT
# undef BTH_PGMPOOLKIND_ROOT
# undef PGM_BTH_NAME
# undef PGM_BTH_NAME_RC_STR
# undef PGM_BTH_NAME_R0_STR
# undef PGM_GST_TYPE
# undef PGM_GST_NAME
# undef PGM_GST_NAME_RC_STR
# undef PGM_GST_NAME_R0_STR
#endif /* VBOX_WITH_64_BITS_GUESTS */

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#undef PGM_SHW_NAME_RC_STR
#undef PGM_SHW_NAME_R0_STR


/*
 * Shadow - Nested paging mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_NESTED
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_NESTED(name)
#define PGM_SHW_NAME_RC_STR(name)   PGM_SHW_NAME_RC_NESTED_STR(name)
#define PGM_SHW_NAME_R0_STR(name)   PGM_SHW_NAME_R0_NESTED_STR(name)
#include "PGMShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_REAL_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_REAL_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_NESTED_REAL(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_NESTED_REAL_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_NESTED_REAL_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_PROT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PROT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_NESTED_PROT(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_NESTED_PROT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_NESTED_PROT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_32BIT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_32BIT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_NESTED_32BIT(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_NESTED_32BIT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_NESTED_32BIT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_32BIT_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - PAE mode */
#define PGM_GST_TYPE                PGM_TYPE_PAE
#define PGM_GST_NAME(name)          PGM_GST_NAME_PAE(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_PAE_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PAE_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_NESTED_PAE(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_NESTED_PAE_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_NESTED_PAE_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PAE_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

#ifdef VBOX_WITH_64_BITS_GUESTS
/* Guest - AMD64 mode */
# define PGM_GST_TYPE               PGM_TYPE_AMD64
# define PGM_GST_NAME(name)         PGM_GST_NAME_AMD64(name)
# define PGM_GST_NAME_RC_STR(name)  PGM_GST_NAME_RC_AMD64_STR(name)
# define PGM_GST_NAME_R0_STR(name)  PGM_GST_NAME_R0_AMD64_STR(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_NESTED_AMD64(name)
# define PGM_BTH_NAME_RC_STR(name)  PGM_BTH_NAME_RC_NESTED_AMD64_STR(name)
# define PGM_BTH_NAME_R0_STR(name)  PGM_BTH_NAME_R0_NESTED_AMD64_STR(name)
# define BTH_PGMPOOLKIND_PT_FOR_PT  PGMPOOLKIND_PAE_PT_FOR_PAE_PT
# define BTH_PGMPOOLKIND_PT_FOR_BIG PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
# include "PGMGstDefs.h"
# include "PGMBth.h"
# undef BTH_PGMPOOLKIND_PT_FOR_BIG
# undef BTH_PGMPOOLKIND_PT_FOR_PT
# undef PGM_BTH_NAME
# undef PGM_BTH_NAME_RC_STR
# undef PGM_BTH_NAME_R0_STR
# undef PGM_GST_TYPE
# undef PGM_GST_NAME
# undef PGM_GST_NAME_RC_STR
# undef PGM_GST_NAME_R0_STR
#endif /* VBOX_WITH_64_BITS_GUESTS */

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#undef PGM_SHW_NAME_RC_STR
#undef PGM_SHW_NAME_R0_STR


/*
 * Shadow - EPT
 */
#define PGM_SHW_TYPE                PGM_TYPE_EPT
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_EPT(name)
#define PGM_SHW_NAME_RC_STR(name)   PGM_SHW_NAME_RC_EPT_STR(name)
#define PGM_SHW_NAME_R0_STR(name)   PGM_SHW_NAME_R0_EPT_STR(name)
#include "PGMShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_REAL_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_REAL_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_EPT_REAL(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_EPT_REAL_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_EPT_REAL_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_PROT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PROT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_EPT_PROT(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_EPT_PROT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_EPT_PROT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_32BIT_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_32BIT_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_EPT_32BIT(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_EPT_32BIT_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_EPT_32BIT_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_32BIT_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

/* Guest - PAE mode */
#define PGM_GST_TYPE                PGM_TYPE_PAE
#define PGM_GST_NAME(name)          PGM_GST_NAME_PAE(name)
#define PGM_GST_NAME_RC_STR(name)   PGM_GST_NAME_RC_PAE_STR(name)
#define PGM_GST_NAME_R0_STR(name)   PGM_GST_NAME_R0_PAE_STR(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_EPT_PAE(name)
#define PGM_BTH_NAME_RC_STR(name)   PGM_BTH_NAME_RC_EPT_PAE_STR(name)
#define PGM_BTH_NAME_R0_STR(name)   PGM_BTH_NAME_R0_EPT_PAE_STR(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PAE_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
#include "PGMGstDefs.h"
#include "PGMBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef PGM_BTH_NAME
#undef PGM_BTH_NAME_RC_STR
#undef PGM_BTH_NAME_R0_STR
#undef PGM_GST_TYPE
#undef PGM_GST_NAME
#undef PGM_GST_NAME_RC_STR
#undef PGM_GST_NAME_R0_STR

#ifdef VBOX_WITH_64_BITS_GUESTS
/* Guest - AMD64 mode */
# define PGM_GST_TYPE               PGM_TYPE_AMD64
# define PGM_GST_NAME(name)         PGM_GST_NAME_AMD64(name)
# define PGM_GST_NAME_RC_STR(name)  PGM_GST_NAME_RC_AMD64_STR(name)
# define PGM_GST_NAME_R0_STR(name)  PGM_GST_NAME_R0_AMD64_STR(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_EPT_AMD64(name)
# define PGM_BTH_NAME_RC_STR(name)  PGM_BTH_NAME_RC_EPT_AMD64_STR(name)
# define PGM_BTH_NAME_R0_STR(name)  PGM_BTH_NAME_R0_EPT_AMD64_STR(name)
# define BTH_PGMPOOLKIND_PT_FOR_PT  PGMPOOLKIND_PAE_PT_FOR_PAE_PT
# define BTH_PGMPOOLKIND_PT_FOR_BIG PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
# include "PGMGstDefs.h"
# include "PGMBth.h"
# undef BTH_PGMPOOLKIND_PT_FOR_BIG
# undef BTH_PGMPOOLKIND_PT_FOR_PT
# undef PGM_BTH_NAME
# undef PGM_BTH_NAME_RC_STR
# undef PGM_BTH_NAME_R0_STR
# undef PGM_GST_TYPE
# undef PGM_GST_NAME
# undef PGM_GST_NAME_RC_STR
# undef PGM_GST_NAME_R0_STR
#endif /* VBOX_WITH_64_BITS_GUESTS */

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME
#undef PGM_SHW_NAME_RC_STR
#undef PGM_SHW_NAME_R0_STR



/**
 * Initiates the paging of VM.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to VM structure.
 */
VMMR3DECL(int) PGMR3Init(PVM pVM)
{
    LogFlow(("PGMR3Init:\n"));
    PCFGMNODE pCfgPGM = CFGMR3GetChild(CFGMR3GetRoot(pVM), "/PGM");
    int rc;

    /*
     * Assert alignment and sizes.
     */
    AssertCompile(sizeof(pVM->pgm.s) <= sizeof(pVM->pgm.padding));

    /*
     * Init the structure.
     */
    pVM->pgm.s.offVM       = RT_OFFSETOF(VM, pgm.s);
    pVM->pgm.s.offVCpuPGM  = RT_OFFSETOF(VMCPU, pgm.s);

    /* Init the per-CPU part. */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        PPGMCPU pPGM = &pVCpu->pgm.s;

        pPGM->offVM      = (uintptr_t)&pVCpu->pgm.s - (uintptr_t)pVM;
        pPGM->offVCpu    = RT_OFFSETOF(VMCPU, pgm.s);
        pPGM->offPGM     = (uintptr_t)&pVCpu->pgm.s - (uintptr_t)&pVM->pgm.s;

        pPGM->enmShadowMode    = PGMMODE_INVALID;
        pPGM->enmGuestMode     = PGMMODE_INVALID;

        pPGM->GCPhysCR3        = NIL_RTGCPHYS;

        pPGM->pGstPaePdptR3    = NULL;
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
        pPGM->pGstPaePdptR0    = NIL_RTR0PTR;
#endif
        pPGM->pGstPaePdptRC    = NIL_RTRCPTR;
        for (unsigned i = 0; i < RT_ELEMENTS(pVCpu->pgm.s.apGstPaePDsR3); i++)
        {
            pPGM->apGstPaePDsR3[i]             = NULL;
#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
            pPGM->apGstPaePDsR0[i]             = NIL_RTR0PTR;
#endif
            pPGM->apGstPaePDsRC[i]             = NIL_RTRCPTR;
            pPGM->aGCPhysGstPaePDs[i]          = NIL_RTGCPHYS;
            pPGM->aGCPhysGstPaePDsMonitored[i] = NIL_RTGCPHYS;
        }

        pPGM->fA20Enabled      = true;
    }

    pVM->pgm.s.enmHostMode      = SUPPAGINGMODE_INVALID;
    pVM->pgm.s.GCPhys4MBPSEMask = RT_BIT_64(32) - 1; /* default; checked later */
    pVM->pgm.s.GCPtrPrevRamRangeMapping = MM_HYPER_AREA_ADDRESS;

    rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "RamPreAlloc", &pVM->pgm.s.fRamPreAlloc,
#ifdef VBOX_WITH_PREALLOC_RAM_BY_DEFAULT
                            true
#else
                            false
#endif
                           );
    AssertLogRelRCReturn(rc, rc);

#if HC_ARCH_BITS == 64 || 1 /** @todo 4GB/32-bit: remove || 1 later and adjust the limit. */
    rc = CFGMR3QueryU32Def(pCfgPGM, "MaxRing3Chunks", &pVM->pgm.s.ChunkR3Map.cMax, UINT32_MAX);
#else
    rc = CFGMR3QueryU32Def(pCfgPGM, "MaxRing3Chunks", &pVM->pgm.s.ChunkR3Map.cMax, _1G / GMM_CHUNK_SIZE);
#endif
    AssertLogRelRCReturn(rc, rc);
    for (uint32_t i = 0; i < RT_ELEMENTS(pVM->pgm.s.ChunkR3Map.Tlb.aEntries); i++)
        pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].idChunk = NIL_GMM_CHUNKID;

    /*
     * Get the configured RAM size - to estimate saved state size.
     */
    uint64_t    cbRam;
    rc = CFGMR3QueryU64(CFGMR3GetRoot(pVM), "RamSize", &cbRam);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cbRam = 0;
    else if (RT_SUCCESS(rc))
    {
        if (cbRam < PAGE_SIZE)
            cbRam = 0;
        cbRam = RT_ALIGN_64(cbRam, PAGE_SIZE);
    }
    else
    {
        AssertMsgFailed(("Configuration error: Failed to query integer \"RamSize\", rc=%Rrc.\n", rc));
        return rc;
    }

    /*
     * Register callbacks, string formatters and the saved state data unit.
     */
#ifdef VBOX_STRICT
    VMR3AtStateRegister(pVM, pgmR3ResetNoMorePhysWritesFlag, NULL);
#endif
    PGMRegisterStringFormatTypes();

    rc = SSMR3RegisterInternal(pVM, "pgm", 1, PGM_SAVED_STATE_VERSION, (size_t)cbRam + sizeof(PGM),
                               NULL, pgmR3Save, NULL,
                               NULL, pgmR3Load, NULL);
    if (RT_FAILURE(rc))
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
     * For the time being we sport a full set of handy pages in addition to the base
     * memory to simplify things.
     */
    rc = MMR3ReserveHandyPages(pVM, RT_ELEMENTS(pVM->pgm.s.aHandyPages)); /** @todo this should be changed to PGM_HANDY_PAGES_MIN but this needs proper testing... */
    AssertRCReturn(rc, rc);

    /*
     * Trees
     */
    rc = MMHyperAlloc(pVM, sizeof(PGMTREES), 0, MM_TAG_PGM, (void **)&pVM->pgm.s.pTreesR3);
    if (RT_SUCCESS(rc))
    {
        pVM->pgm.s.pTreesR0 = MMHyperR3ToR0(pVM, pVM->pgm.s.pTreesR3);
        pVM->pgm.s.pTreesRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pTreesR3);

        /*
         * Alocate the zero page.
         */
        rc = MMHyperAlloc(pVM, PAGE_SIZE, PAGE_SIZE, MM_TAG_PGM, &pVM->pgm.s.pvZeroPgR3);
    }
    if (RT_SUCCESS(rc))
    {
        pVM->pgm.s.pvZeroPgRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pvZeroPgR3);
        pVM->pgm.s.pvZeroPgR0 = MMHyperR3ToR0(pVM, pVM->pgm.s.pvZeroPgR3);
        pVM->pgm.s.HCPhysZeroPg = MMR3HyperHCVirt2HCPhys(pVM, pVM->pgm.s.pvZeroPgR3);
        AssertRelease(pVM->pgm.s.HCPhysZeroPg != NIL_RTHCPHYS);

        /*
         * Init the paging.
         */
        rc = pgmR3InitPaging(pVM);
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Init the page pool.
         */
        rc = pgmR3PoolInit(pVM);
    }
    if (RT_SUCCESS(rc))
    {
        for (unsigned i=0;i<pVM->cCPUs;i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            rc = PGMR3ChangeMode(pVM, pVCpu, PGMMODE_REAL);
            if (RT_FAILURE(rc))
                break;
        }
    }

    if (RT_SUCCESS(rc))
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
                                   "Dumps physical, virtual and hyper virtual handlers. "
                                   "Pass 'phys', 'virt', 'hyper' as argument if only one kind is wanted."
                                   "Add 'nost' if the statistics are unwanted, use together with 'all' or explicit selection.",
                                   pgmR3InfoHandlers);
        DBGFR3InfoRegisterInternal(pVM, "mappings",
                                   "Dumps guest mappings.",
                                   pgmR3MapInfo);

        pgmR3InitStats(pVM);

#ifdef VBOX_WITH_DEBUGGER
        /*
         * Debugger commands.
         */
        static bool s_fRegisteredCmds = false;
        if (!s_fRegisteredCmds)
        {
            int rc = DBGCRegisterCommands(&g_aCmds[0], RT_ELEMENTS(g_aCmds));
            if (RT_SUCCESS(rc))
                s_fRegisteredCmds = true;
        }
#endif
        return VINF_SUCCESS;
    }

    /* Almost no cleanup necessary, MM frees all memory. */
    PDMR3CritSectDelete(&pVM->pgm.s.CritSect);

    return rc;
}


/**
 * Initializes the per-VCPU PGM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) PGMR3InitCPU(PVM pVM)
{
    LogFlow(("PGMR3InitCPU\n"));
    return VINF_SUCCESS;
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
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->pgm.s.enmShadowMode = PGMMODE_INVALID;
        pVCpu->pgm.s.enmGuestMode  = PGMMODE_INVALID;
    }

    pVM->pgm.s.enmHostMode   = SUPPAGINGMODE_INVALID;

    /*
     * Allocate static mapping space for whatever the cr3 register
     * points to and in the case of PAE mode to the 4 PDs.
     */
    int rc = MMR3HyperReserve(pVM, PAGE_SIZE * 5, "CR3 mapping", &pVM->pgm.s.GCPtrCR3Mapping);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to reserve two pages for cr mapping in HMA, rc=%Rrc\n", rc));
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
    pVM->pgm.s.pInterPaePDPT    = (PX86PDPT)MMR3PageAllocLow(pVM);
    pVM->pgm.s.pInterPaePDPT64  = (PX86PDPT)MMR3PageAllocLow(pVM);
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
        ||  !pVM->pgm.s.pInterPaePDPT
        ||  !pVM->pgm.s.pInterPaePDPT64
        ||  !pVM->pgm.s.pInterPaePML4)
    {
        AssertMsgFailed(("Failed to allocate pages for the intermediate context!\n"));
        return VERR_NO_PAGE_MEMORY;
    }

    pVM->pgm.s.HCPhysInterPD = MMPage2Phys(pVM, pVM->pgm.s.pInterPD);
    AssertRelease(pVM->pgm.s.HCPhysInterPD != NIL_RTHCPHYS && !(pVM->pgm.s.HCPhysInterPD & PAGE_OFFSET_MASK));
    pVM->pgm.s.HCPhysInterPaePDPT = MMPage2Phys(pVM, pVM->pgm.s.pInterPaePDPT);
    AssertRelease(pVM->pgm.s.HCPhysInterPaePDPT != NIL_RTHCPHYS && !(pVM->pgm.s.HCPhysInterPaePDPT & PAGE_OFFSET_MASK));
    pVM->pgm.s.HCPhysInterPaePML4 = MMPage2Phys(pVM, pVM->pgm.s.pInterPaePML4);
    AssertRelease(pVM->pgm.s.HCPhysInterPaePML4 != NIL_RTHCPHYS && !(pVM->pgm.s.HCPhysInterPaePML4 & PAGE_OFFSET_MASK) && pVM->pgm.s.HCPhysInterPaePML4 < 0xffffffff);

    /*
     * Initialize the pages, setting up the PML4 and PDPT for repetitive 4GB action.
     */
    ASMMemZeroPage(pVM->pgm.s.pInterPD);
    ASMMemZeroPage(pVM->pgm.s.apInterPTs[0]);
    ASMMemZeroPage(pVM->pgm.s.apInterPTs[1]);

    ASMMemZeroPage(pVM->pgm.s.apInterPaePTs[0]);
    ASMMemZeroPage(pVM->pgm.s.apInterPaePTs[1]);

    ASMMemZeroPage(pVM->pgm.s.pInterPaePDPT);
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.apInterPaePDs); i++)
    {
        ASMMemZeroPage(pVM->pgm.s.apInterPaePDs[i]);
        pVM->pgm.s.pInterPaePDPT->a[i].u = X86_PDPE_P | PGM_PLXFLAGS_PERMANENT
                                          | MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[i]);
    }

    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.pInterPaePDPT64->a); i++)
    {
        const unsigned iPD = i % RT_ELEMENTS(pVM->pgm.s.apInterPaePDs);
        pVM->pgm.s.pInterPaePDPT64->a[i].u = X86_PDPE_P | X86_PDPE_RW | X86_PDPE_US | X86_PDPE_A | PGM_PLXFLAGS_PERMANENT
                                            | MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[iPD]);
    }

    RTHCPHYS HCPhysInterPaePDPT64 = MMPage2Phys(pVM, pVM->pgm.s.pInterPaePDPT64);
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.pInterPaePML4->a); i++)
        pVM->pgm.s.pInterPaePML4->a[i].u = X86_PML4E_P | X86_PML4E_RW | X86_PML4E_US | X86_PML4E_A | PGM_PLXFLAGS_PERMANENT
                                         | HCPhysInterPaePDPT64;

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
#ifndef VBOX_WITH_HYBRID_32BIT_KERNEL
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
    if (RT_SUCCESS(rc))
    {
        LogFlow(("pgmR3InitPaging: returns successfully\n"));
#if HC_ARCH_BITS == 64
        LogRel(("Debug: HCPhysInterPD=%RHp HCPhysInterPaePDPT=%RHp HCPhysInterPaePML4=%RHp\n",
                pVM->pgm.s.HCPhysInterPD, pVM->pgm.s.HCPhysInterPaePDPT, pVM->pgm.s.HCPhysInterPaePML4));
        LogRel(("Debug: apInterPTs={%RHp,%RHp} apInterPaePTs={%RHp,%RHp} apInterPaePDs={%RHp,%RHp,%RHp,%RHp} pInterPaePDPT64=%RHp\n",
                MMPage2Phys(pVM, pVM->pgm.s.apInterPTs[0]),    MMPage2Phys(pVM, pVM->pgm.s.apInterPTs[1]),
                MMPage2Phys(pVM, pVM->pgm.s.apInterPaePTs[0]), MMPage2Phys(pVM, pVM->pgm.s.apInterPaePTs[1]),
                MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[0]), MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[1]), MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[2]), MMPage2Phys(pVM, pVM->pgm.s.apInterPaePDs[3]),
                MMPage2Phys(pVM, pVM->pgm.s.pInterPaePDPT64)));
#endif

        return VINF_SUCCESS;
    }

    LogFlow(("pgmR3InitPaging: returns %Rrc\n", rc));
    return rc;
}


/**
 * Init statistics
 */
static void pgmR3InitStats(PVM pVM)
{
    PPGM pPGM = &pVM->pgm.s;
    int  rc;

    /* Common - misc variables */
    STAM_REL_REG(pVM, &pPGM->cAllPages,                     STAMTYPE_U32,     "/PGM/Page/cAllPages",                STAMUNIT_OCCURENCES,     "The total number of pages.");
    STAM_REL_REG(pVM, &pPGM->cPrivatePages,                 STAMTYPE_U32,     "/PGM/Page/cPrivatePages",            STAMUNIT_OCCURENCES,     "The number of private pages.");
    STAM_REL_REG(pVM, &pPGM->cSharedPages,                  STAMTYPE_U32,     "/PGM/Page/cSharedPages",             STAMUNIT_OCCURENCES,     "The number of shared pages.");
    STAM_REL_REG(pVM, &pPGM->cZeroPages,                    STAMTYPE_U32,     "/PGM/Page/cZeroPages",               STAMUNIT_OCCURENCES,     "The number of zero backed pages.");
    STAM_REL_REG(pVM, &pPGM->cHandyPages,                   STAMTYPE_U32,     "/PGM/Page/cHandyPages",              STAMUNIT_OCCURENCES,     "The number of handy pages (not included in cAllPages).");
    STAM_REL_REG(pVM, &pPGM->cRelocations,                  STAMTYPE_COUNTER, "/PGM/cRelocations",                  STAMUNIT_OCCURENCES,     "Number of hypervisor relocations.");
    STAM_REL_REG(pVM, &pPGM->ChunkR3Map.c,                  STAMTYPE_U32,     "/PGM/ChunkR3Map/c",                  STAMUNIT_OCCURENCES,     "Number of mapped chunks.");
    STAM_REL_REG(pVM, &pPGM->ChunkR3Map.cMax,               STAMTYPE_U32,     "/PGM/ChunkR3Map/cMax",               STAMUNIT_OCCURENCES,     "Maximum number of mapped chunks.");

#ifdef VBOX_WITH_STATISTICS

# define PGM_REG_COUNTER(a, b, c) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, c, b); \
        AssertRC(rc);

# define PGM_REG_PROFILE(a, b, c) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, c, b); \
        AssertRC(rc);

    PGM_REG_COUNTER(&pPGM->StatR3DetectedConflicts,           "/PGM/R3/DetectedConflicts",          "The number of times PGMR3CheckMappingConflicts() detected a conflict.");
    PGM_REG_PROFILE(&pPGM->StatR3ResolveConflict,             "/PGM/R3/ResolveConflict",            "pgmR3SyncPTResolveConflict() profiling (includes the entire relocation).");

    PGM_REG_COUNTER(&pPGM->StatRZChunkR3MapTlbHits,           "/PGM/ChunkR3Map/TlbHitsRZ",          "TLB hits.");
    PGM_REG_COUNTER(&pPGM->StatRZChunkR3MapTlbMisses,         "/PGM/ChunkR3Map/TlbMissesRZ",        "TLB misses.");
    PGM_REG_COUNTER(&pPGM->StatRZPageMapTlbHits,              "/PGM/RZ/Page/MapTlbHits",            "TLB hits.");
    PGM_REG_COUNTER(&pPGM->StatRZPageMapTlbMisses,            "/PGM/RZ/Page/MapTlbMisses",          "TLB misses.");
    PGM_REG_COUNTER(&pPGM->StatR3ChunkR3MapTlbHits,           "/PGM/ChunkR3Map/TlbHitsR3",          "TLB hits.");
    PGM_REG_COUNTER(&pPGM->StatR3ChunkR3MapTlbMisses,         "/PGM/ChunkR3Map/TlbMissesR3",        "TLB misses.");
    PGM_REG_COUNTER(&pPGM->StatR3PageMapTlbHits,              "/PGM/R3/Page/MapTlbHits",            "TLB hits.");
    PGM_REG_COUNTER(&pPGM->StatR3PageMapTlbMisses,            "/PGM/R3/Page/MapTlbMisses",          "TLB misses.");

    PGM_REG_PROFILE(&pPGM->StatRZSyncCR3HandlerVirtualUpdate, "/PGM/RZ/SyncCR3/Handlers/VirtualUpdate", "Profiling of the virtual handler updates.");
    PGM_REG_PROFILE(&pPGM->StatRZSyncCR3HandlerVirtualReset,  "/PGM/RZ/SyncCR3/Handlers/VirtualReset",  "Profiling of the virtual handler resets.");
    PGM_REG_PROFILE(&pPGM->StatR3SyncCR3HandlerVirtualUpdate, "/PGM/R3/SyncCR3/Handlers/VirtualUpdate", "Profiling of the virtual handler updates.");
    PGM_REG_PROFILE(&pPGM->StatR3SyncCR3HandlerVirtualReset,  "/PGM/R3/SyncCR3/Handlers/VirtualReset",  "Profiling of the virtual handler resets.");

    PGM_REG_COUNTER(&pPGM->StatRZPhysHandlerReset,            "/PGM/RZ/PhysHandlerReset",           "The number of times PGMHandlerPhysicalReset is called.");
    PGM_REG_COUNTER(&pPGM->StatR3PhysHandlerReset,            "/PGM/R3/PhysHandlerReset",           "The number of times PGMHandlerPhysicalReset is called.");
    PGM_REG_PROFILE(&pPGM->StatRZVirtHandlerSearchByPhys,     "/PGM/RZ/VirtHandlerSearchByPhys",    "Profiling of pgmHandlerVirtualFindByPhysAddr.");
    PGM_REG_PROFILE(&pPGM->StatR3VirtHandlerSearchByPhys,     "/PGM/R3/VirtHandlerSearchByPhys",    "Profiling of pgmHandlerVirtualFindByPhysAddr.");

    PGM_REG_COUNTER(&pPGM->StatRZPageReplaceShared,           "/PGM/RZ/Page/ReplacedShared",        "Times a shared page was replaced.");
    PGM_REG_COUNTER(&pPGM->StatRZPageReplaceZero,             "/PGM/RZ/Page/ReplacedZero",          "Times the zero page was replaced.");
/// @todo    PGM_REG_COUNTER(&pPGM->StatRZPageHandyAllocs,             "/PGM/RZ/Page/HandyAllocs",               "Number of times we've allocated more handy pages.");
    PGM_REG_COUNTER(&pPGM->StatR3PageReplaceShared,           "/PGM/R3/Page/ReplacedShared",        "Times a shared page was replaced.");
    PGM_REG_COUNTER(&pPGM->StatR3PageReplaceZero,             "/PGM/R3/Page/ReplacedZero",          "Times the zero page was replaced.");
/// @todo    PGM_REG_COUNTER(&pPGM->StatR3PageHandyAllocs,             "/PGM/R3/Page/HandyAllocs",               "Number of times we've allocated more handy pages.");

    /* GC only: */
    PGM_REG_COUNTER(&pPGM->StatRCDynMapCacheHits,             "/PGM/RC/DynMapCache/Hits" ,          "Number of dynamic page mapping cache hits.");
    PGM_REG_COUNTER(&pPGM->StatRCDynMapCacheMisses,           "/PGM/RC/DynMapCache/Misses" ,        "Number of dynamic page mapping cache misses.");
    PGM_REG_COUNTER(&pPGM->StatRCInvlPgConflict,              "/PGM/RC/InvlPgConflict",             "Number of times PGMInvalidatePage() detected a mapping conflict.");
    PGM_REG_COUNTER(&pPGM->StatRCInvlPgSyncMonCR3,            "/PGM/RC/InvlPgSyncMonitorCR3",       "Number of times PGMInvalidatePage() ran into PGM_SYNC_MONITOR_CR3.");

# ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    PGM_REG_COUNTER(&pPGM->StatTrackVirgin,                   "/PGM/Track/Virgin",                  "The number of first time shadowings");
    PGM_REG_COUNTER(&pPGM->StatTrackAliased,                  "/PGM/Track/Aliased",                 "The number of times switching to cRef2, i.e. the page is being shadowed by two PTs.");
    PGM_REG_COUNTER(&pPGM->StatTrackAliasedMany,              "/PGM/Track/AliasedMany",             "The number of times we're tracking using cRef2.");
    PGM_REG_COUNTER(&pPGM->StatTrackAliasedLots,              "/PGM/Track/AliasedLots",             "The number of times we're hitting pages which has overflowed cRef2");
    PGM_REG_COUNTER(&pPGM->StatTrackOverflows,                "/PGM/Track/Overflows",               "The number of times the extent list grows too long.");
    PGM_REG_PROFILE(&pPGM->StatTrackDeref,                    "/PGM/Track/Deref",                   "Profiling of SyncPageWorkerTrackDeref (expensive).");
# endif

# undef PGM_REG_COUNTER
# undef PGM_REG_PROFILE
#endif

    /*
     * Note! The layout below matches the member layout exactly!
     */

    /*
     * Common - stats
     */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU  pVCpu = &pVM->aCpus[i];
        PPGMCPU pPGM = &pVCpu->pgm.s;

#define PGM_REG_COUNTER(a, b, c) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, c, b, i); \
        AssertRC(rc);
#define PGM_REG_PROFILE(a, b, c) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, c, b, i); \
        AssertRC(rc);

        PGM_REG_COUNTER(&pPGM->cGuestModeChanges, "/PGM/CPU%d/cGuestModeChanges",  "Number of guest mode changes.");

#ifdef VBOX_WITH_STATISTICS
        for (unsigned j = 0; j < RT_ELEMENTS(pPGM->StatSyncPtPD); j++)
            STAMR3RegisterF(pVM, &pPGM->StatSyncPtPD[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "The number of SyncPT per PD n.", "/PGM/CPU%d/PDSyncPT/%04X", i, j);
        for (unsigned j = 0; j < RT_ELEMENTS(pPGM->StatSyncPagePD); j++)
            STAMR3RegisterF(pVM, &pPGM->StatSyncPagePD[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "The number of SyncPage per PD n.", "/PGM/CPU%d/PDSyncPage/%04X", i, j);

        /* R0 only: */
        PGM_REG_COUNTER(&pPGM->StatR0DynMapMigrateInvlPg,         "/PGM/CPU%d/R0/DynMapMigrateInvlPg",        "invlpg count in PGMDynMapMigrateAutoSet.");
        PGM_REG_PROFILE(&pPGM->StatR0DynMapGCPageInl,             "/PGM/CPU%d/R0/DynMapPageGCPageInl",        "Calls to pgmR0DynMapGCPageInlined.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapGCPageInlHits,         "/PGM/CPU%d/R0/DynMapPageGCPageInl/Hits",   "Hash table lookup hits.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapGCPageInlMisses,       "/PGM/CPU%d/R0/DynMapPageGCPageInl/Misses", "Misses that falls back to code common with PGMDynMapHCPage.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapGCPageInlRamHits,      "/PGM/CPU%d/R0/DynMapPageGCPageInl/RamHits",   "1st ram range hits.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapGCPageInlRamMisses,    "/PGM/CPU%d/R0/DynMapPageGCPageInl/RamMisses", "1st ram range misses, takes slow path.");
        PGM_REG_PROFILE(&pPGM->StatR0DynMapHCPageInl,             "/PGM/CPU%d/R0/DynMapPageHCPageInl",        "Calls to pgmR0DynMapHCPageInlined.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapHCPageInlHits,         "/PGM/CPU%d/R0/DynMapPageHCPageInl/Hits",   "Hash table lookup hits.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapHCPageInlMisses,       "/PGM/CPU%d/R0/DynMapPageHCPageInl/Misses", "Misses that falls back to code common with PGMDynMapHCPage.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPage,                  "/PGM/CPU%d/R0/DynMapPage",                 "Calls to pgmR0DynMapPage");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapSetOptimize,           "/PGM/CPU%d/R0/DynMapPage/SetOptimize",     "Calls to pgmDynMapOptimizeAutoSet.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapSetSearchFlushes,      "/PGM/CPU%d/R0/DynMapPage/SetSearchFlushes","Set search restorting to subset flushes.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapSetSearchHits,         "/PGM/CPU%d/R0/DynMapPage/SetSearchHits",   "Set search hits.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapSetSearchMisses,       "/PGM/CPU%d/R0/DynMapPage/SetSearchMisses", "Set search misses.");
        PGM_REG_PROFILE(&pPGM->StatR0DynMapHCPage,                "/PGM/CPU%d/R0/DynMapPage/HCPage",          "Calls to PGMDynMapHCPage (ring-0).");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPageHits0,             "/PGM/CPU%d/R0/DynMapPage/Hits0",           "Hits at iPage+0");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPageHits1,             "/PGM/CPU%d/R0/DynMapPage/Hits1",           "Hits at iPage+1");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPageHits2,             "/PGM/CPU%d/R0/DynMapPage/Hits2",           "Hits at iPage+2");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPageInvlPg,            "/PGM/CPU%d/R0/DynMapPage/InvlPg",          "invlpg count in pgmR0DynMapPageSlow.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPageSlow,              "/PGM/CPU%d/R0/DynMapPage/Slow",            "Calls to pgmR0DynMapPageSlow - subtract this from pgmR0DynMapPage to get 1st level hits.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPageSlowLoopHits,      "/PGM/CPU%d/R0/DynMapPage/SlowLoopHits" ,   "Hits in the loop path.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPageSlowLoopMisses,    "/PGM/CPU%d/R0/DynMapPage/SlowLoopMisses",  "Misses in the loop path. NonLoopMisses = Slow - SlowLoopHit - SlowLoopMisses");
        //PGM_REG_COUNTER(&pPGM->StatR0DynMapPageSlowLostHits,      "/PGM/CPU%d/R0/DynMapPage/SlowLostHits",    "Lost hits.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapSubsets,               "/PGM/CPU%d/R0/Subsets",                    "Times PGMDynMapPushAutoSubset was called.");
        PGM_REG_COUNTER(&pPGM->StatR0DynMapPopFlushes,            "/PGM/CPU%d/R0/SubsetPopFlushes",           "Times PGMDynMapPopAutoSubset flushes the subset.");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[0],           "/PGM/CPU%d/R0/SetSize000..09",              "00-09% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[1],           "/PGM/CPU%d/R0/SetSize010..19",              "10-19% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[2],           "/PGM/CPU%d/R0/SetSize020..29",              "20-29% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[3],           "/PGM/CPU%d/R0/SetSize030..39",              "30-39% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[4],           "/PGM/CPU%d/R0/SetSize040..49",              "40-49% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[5],           "/PGM/CPU%d/R0/SetSize050..59",              "50-59% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[6],           "/PGM/CPU%d/R0/SetSize060..69",              "60-69% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[7],           "/PGM/CPU%d/R0/SetSize070..79",              "70-79% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[8],           "/PGM/CPU%d/R0/SetSize080..89",              "80-89% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[9],           "/PGM/CPU%d/R0/SetSize090..99",              "90-99% filled");
        PGM_REG_COUNTER(&pPGM->aStatR0DynMapSetSize[10],          "/PGM/CPU%d/R0/SetSize100",                 "100% filled");

        /* RZ only: */
        PGM_REG_PROFILE(&pPGM->StatRZTrap0e,                      "/PGM/CPU%d/RZ/Trap0e",                     "Profiling of the PGMTrap0eHandler() body.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTimeCheckPageFault,    "/PGM/CPU%d/RZ/Trap0e/Time/CheckPageFault", "Profiling of checking for dirty/access emulation faults.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTimeSyncPT,            "/PGM/CPU%d/RZ/Trap0e/Time/SyncPT",         "Profiling of lazy page table syncing.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTimeMapping,           "/PGM/CPU%d/RZ/Trap0e/Time/Mapping",        "Profiling of checking virtual mappings.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTimeOutOfSync,         "/PGM/CPU%d/RZ/Trap0e/Time/OutOfSync",      "Profiling of out of sync page handling.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTimeHandlers,          "/PGM/CPU%d/RZ/Trap0e/Time/Handlers",       "Profiling of checking handlers.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2CSAM,             "/PGM/CPU%d/RZ/Trap0e/Time2/CSAM",              "Profiling of the Trap0eHandler body when the cause is CSAM.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2DirtyAndAccessed, "/PGM/CPU%d/RZ/Trap0e/Time2/DirtyAndAccessedBits", "Profiling of the Trap0eHandler body when the cause is dirty and/or accessed bit emulation.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2GuestTrap,        "/PGM/CPU%d/RZ/Trap0e/Time2/GuestTrap",         "Profiling of the Trap0eHandler body when the cause is a guest trap.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2HndPhys,          "/PGM/CPU%d/RZ/Trap0e/Time2/HandlerPhysical",   "Profiling of the Trap0eHandler body when the cause is a physical handler.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2HndVirt,          "/PGM/CPU%d/RZ/Trap0e/Time2/HandlerVirtual",    "Profiling of the Trap0eHandler body when the cause is a virtual handler.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2HndUnhandled,     "/PGM/CPU%d/RZ/Trap0e/Time2/HandlerUnhandled",  "Profiling of the Trap0eHandler body when the cause is access outside the monitored areas of a monitored page.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2Misc,             "/PGM/CPU%d/RZ/Trap0e/Time2/Misc",              "Profiling of the Trap0eHandler body when the cause is not known.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2OutOfSync,        "/PGM/CPU%d/RZ/Trap0e/Time2/OutOfSync",         "Profiling of the Trap0eHandler body when the cause is an out-of-sync page.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2OutOfSyncHndPhys, "/PGM/CPU%d/RZ/Trap0e/Time2/OutOfSyncHndPhys",  "Profiling of the Trap0eHandler body when the cause is an out-of-sync physical handler page.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2OutOfSyncHndVirt, "/PGM/CPU%d/RZ/Trap0e/Time2/OutOfSyncHndVirt",  "Profiling of the Trap0eHandler body when the cause is an out-of-sync virtual handler page.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2OutOfSyncHndObs,  "/PGM/CPU%d/RZ/Trap0e/Time2/OutOfSyncObsHnd",   "Profiling of the Trap0eHandler body when the cause is an obsolete handler page.");
        PGM_REG_PROFILE(&pPGM->StatRZTrap0eTime2SyncPT,           "/PGM/CPU%d/RZ/Trap0e/Time2/SyncPT",            "Profiling of the Trap0eHandler body when the cause is lazy syncing of a PT.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eConflicts,             "/PGM/CPU%d/RZ/Trap0e/Conflicts",               "The number of times #PF was caused by an undetected conflict.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eHandlersMapping,       "/PGM/CPU%d/RZ/Trap0e/Handlers/Mapping",        "Number of traps due to access handlers in mappings.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eHandlersOutOfSync,     "/PGM/CPU%d/RZ/Trap0e/Handlers/OutOfSync",      "Number of traps due to out-of-sync handled pages.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eHandlersPhysical,      "/PGM/CPU%d/RZ/Trap0e/Handlers/Physical",       "Number of traps due to physical access handlers.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eHandlersVirtual,       "/PGM/CPU%d/RZ/Trap0e/Handlers/Virtual",        "Number of traps due to virtual access handlers.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eHandlersVirtualByPhys, "/PGM/CPU%d/RZ/Trap0e/Handlers/VirtualByPhys",  "Number of traps due to virtual access handlers by physical address.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eHandlersVirtualUnmarked,"/PGM/CPU%d/RZ/Trap0e/Handlers/VirtualUnmarked","Number of traps due to virtual access handlers by virtual address (without proper physical flags).");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eHandlersUnhandled,     "/PGM/CPU%d/RZ/Trap0e/Handlers/Unhandled",      "Number of traps due to access outside range of monitored page(s).");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eHandlersInvalid,       "/PGM/CPU%d/RZ/Trap0e/Handlers/Invalid",        "Number of traps due to access to invalid physical memory.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eUSNotPresentRead,      "/PGM/CPU%d/RZ/Trap0e/Err/User/NPRead",         "Number of user mode not present read page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eUSNotPresentWrite,     "/PGM/CPU%d/RZ/Trap0e/Err/User/NPWrite",        "Number of user mode not present write page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eUSWrite,               "/PGM/CPU%d/RZ/Trap0e/Err/User/Write",          "Number of user mode write page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eUSReserved,            "/PGM/CPU%d/RZ/Trap0e/Err/User/Reserved",       "Number of user mode reserved bit page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eUSNXE,                 "/PGM/CPU%d/RZ/Trap0e/Err/User/NXE",            "Number of user mode NXE page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eUSRead,                "/PGM/CPU%d/RZ/Trap0e/Err/User/Read",           "Number of user mode read page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eSVNotPresentRead,      "/PGM/CPU%d/RZ/Trap0e/Err/Supervisor/NPRead",   "Number of supervisor mode not present read page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eSVNotPresentWrite,     "/PGM/CPU%d/RZ/Trap0e/Err/Supervisor/NPWrite",  "Number of supervisor mode not present write page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eSVWrite,               "/PGM/CPU%d/RZ/Trap0e/Err/Supervisor/Write",    "Number of supervisor mode write page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eSVReserved,            "/PGM/CPU%d/RZ/Trap0e/Err/Supervisor/Reserved", "Number of supervisor mode reserved bit page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eSNXE,                  "/PGM/CPU%d/RZ/Trap0e/Err/Supervisor/NXE",      "Number of supervisor mode NXE page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eGuestPF,               "/PGM/CPU%d/RZ/Trap0e/GuestPF",                 "Number of real guest page faults.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eGuestPFUnh,            "/PGM/CPU%d/RZ/Trap0e/GuestPF/Unhandled",       "Number of real guest page faults from the 'unhandled' case.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eGuestPFMapping,        "/PGM/CPU%d/RZ/Trap0e/GuestPF/InMapping",       "Number of real guest page faults in a mapping.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eWPEmulInRZ,            "/PGM/CPU%d/RZ/Trap0e/WP/InRZ",                 "Number of guest page faults due to X86_CR0_WP emulation.");
        PGM_REG_COUNTER(&pPGM->StatRZTrap0eWPEmulToR3,            "/PGM/CPU%d/RZ/Trap0e/WP/ToR3",                 "Number of guest page faults due to X86_CR0_WP emulation (forward to R3 for emulation).");
        for (unsigned j = 0; j < RT_ELEMENTS(pPGM->StatRZTrap0ePD); j++)
            STAMR3RegisterF(pVM, &pPGM->StatRZTrap0ePD[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "The number of traps in page directory n.", "/PGM/CPU%d/RZ/Trap0e/PD/%04X", i, j);

        PGM_REG_COUNTER(&pPGM->StatRZGuestCR3WriteHandled,        "/PGM/CPU%d/RZ/CR3WriteHandled",                "The number of times the Guest CR3 change was successfully handled.");
        PGM_REG_COUNTER(&pPGM->StatRZGuestCR3WriteUnhandled,      "/PGM/CPU%d/RZ/CR3WriteUnhandled",              "The number of times the Guest CR3 change was passed back to the recompiler.");
        PGM_REG_COUNTER(&pPGM->StatRZGuestCR3WriteConflict,       "/PGM/CPU%d/RZ/CR3WriteConflict",               "The number of times the Guest CR3 monitoring detected a conflict.");
        PGM_REG_COUNTER(&pPGM->StatRZGuestROMWriteHandled,        "/PGM/CPU%d/RZ/ROMWriteHandled",                "The number of times the Guest ROM change was successfully handled.");
        PGM_REG_COUNTER(&pPGM->StatRZGuestROMWriteUnhandled,      "/PGM/CPU%d/RZ/ROMWriteUnhandled",              "The number of times the Guest ROM change was passed back to the recompiler.");

        /* HC only: */

        /* RZ & R3: */
        PGM_REG_PROFILE(&pPGM->StatRZSyncCR3,                     "/PGM/CPU%d/RZ/SyncCR3",                        "Profiling of the PGMSyncCR3() body.");
        PGM_REG_PROFILE(&pPGM->StatRZSyncCR3Handlers,             "/PGM/CPU%d/RZ/SyncCR3/Handlers",               "Profiling of the PGMSyncCR3() update handler section.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncCR3Global,               "/PGM/CPU%d/RZ/SyncCR3/Global",                 "The number of global CR3 syncs.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncCR3NotGlobal,            "/PGM/CPU%d/RZ/SyncCR3/NotGlobal",              "The number of non-global CR3 syncs.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncCR3DstCacheHit,          "/PGM/CPU%d/RZ/SyncCR3/DstChacheHit",           "The number of times we got some kind of a cache hit.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncCR3DstFreed,             "/PGM/CPU%d/RZ/SyncCR3/DstFreed",               "The number of times we've had to free a shadow entry.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncCR3DstFreedSrcNP,        "/PGM/CPU%d/RZ/SyncCR3/DstFreedSrcNP",          "The number of times we've had to free a shadow entry for which the source entry was not present.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncCR3DstNotPresent,        "/PGM/CPU%d/RZ/SyncCR3/DstNotPresent",          "The number of times we've encountered a not present shadow entry for a present guest entry.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncCR3DstSkippedGlobalPD,   "/PGM/CPU%d/RZ/SyncCR3/DstSkippedGlobalPD",     "The number of times a global page directory wasn't flushed.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncCR3DstSkippedGlobalPT,   "/PGM/CPU%d/RZ/SyncCR3/DstSkippedGlobalPT",     "The number of times a page table with only global entries wasn't flushed.");
        PGM_REG_PROFILE(&pPGM->StatRZSyncPT,                      "/PGM/CPU%d/RZ/SyncPT",                         "Profiling of the pfnSyncPT() body.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncPTFailed,                "/PGM/CPU%d/RZ/SyncPT/Failed",                  "The number of times pfnSyncPT() failed.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncPT4K,                    "/PGM/CPU%d/RZ/SyncPT/4K",                      "Nr of 4K PT syncs");
        PGM_REG_COUNTER(&pPGM->StatRZSyncPT4M,                    "/PGM/CPU%d/RZ/SyncPT/4M",                      "Nr of 4M PT syncs");
        PGM_REG_COUNTER(&pPGM->StatRZSyncPagePDNAs,               "/PGM/CPU%d/RZ/SyncPagePDNAs",                  "The number of time we've marked a PD not present from SyncPage to virtualize the accessed bit.");
        PGM_REG_COUNTER(&pPGM->StatRZSyncPagePDOutOfSync,         "/PGM/CPU%d/RZ/SyncPagePDOutOfSync",            "The number of time we've encountered an out-of-sync PD in SyncPage.");
        PGM_REG_COUNTER(&pPGM->StatRZAccessedPage,                "/PGM/CPU%d/RZ/AccessedPage",               "The number of pages marked not present for accessed bit emulation.");
        PGM_REG_PROFILE(&pPGM->StatRZDirtyBitTracking,            "/PGM/CPU%d/RZ/DirtyPage",                  "Profiling the dirty bit tracking in CheckPageFault().");
        PGM_REG_COUNTER(&pPGM->StatRZDirtyPage,                   "/PGM/CPU%d/RZ/DirtyPage/Mark",             "The number of pages marked read-only for dirty bit tracking.");
        PGM_REG_COUNTER(&pPGM->StatRZDirtyPageBig,                "/PGM/CPU%d/RZ/DirtyPage/MarkBig",          "The number of 4MB pages marked read-only for dirty bit tracking.");
        PGM_REG_COUNTER(&pPGM->StatRZDirtyPageSkipped,            "/PGM/CPU%d/RZ/DirtyPage/Skipped",          "The number of pages already dirty or readonly.");
        PGM_REG_COUNTER(&pPGM->StatRZDirtyPageTrap,               "/PGM/CPU%d/RZ/DirtyPage/Trap",             "The number of traps generated for dirty bit tracking.");
        PGM_REG_COUNTER(&pPGM->StatRZDirtiedPage,                 "/PGM/CPU%d/RZ/DirtyPage/SetDirty",         "The number of pages marked dirty because of write accesses.");
        PGM_REG_COUNTER(&pPGM->StatRZDirtyTrackRealPF,            "/PGM/CPU%d/RZ/DirtyPage/RealPF",           "The number of real pages faults during dirty bit tracking.");
        PGM_REG_COUNTER(&pPGM->StatRZPageAlreadyDirty,            "/PGM/CPU%d/RZ/DirtyPage/AlreadySet",       "The number of pages already marked dirty because of write accesses.");
        PGM_REG_PROFILE(&pPGM->StatRZInvalidatePage,              "/PGM/CPU%d/RZ/InvalidatePage",             "PGMInvalidatePage() profiling.");
        PGM_REG_COUNTER(&pPGM->StatRZInvalidatePage4KBPages,      "/PGM/CPU%d/RZ/InvalidatePage/4KBPages",    "The number of times PGMInvalidatePage() was called for a 4KB page.");
        PGM_REG_COUNTER(&pPGM->StatRZInvalidatePage4MBPages,      "/PGM/CPU%d/RZ/InvalidatePage/4MBPages",    "The number of times PGMInvalidatePage() was called for a 4MB page.");
        PGM_REG_COUNTER(&pPGM->StatRZInvalidatePage4MBPagesSkip,  "/PGM/CPU%d/RZ/InvalidatePage/4MBPagesSkip","The number of times PGMInvalidatePage() skipped a 4MB page.");
        PGM_REG_COUNTER(&pPGM->StatRZInvalidatePagePDMappings,    "/PGM/CPU%d/RZ/InvalidatePage/PDMappings",  "The number of times PGMInvalidatePage() was called for a page directory containing mappings (no conflict).");
        PGM_REG_COUNTER(&pPGM->StatRZInvalidatePagePDNAs,         "/PGM/CPU%d/RZ/InvalidatePage/PDNAs",       "The number of times PGMInvalidatePage() was called for a not accessed page directory.");
        PGM_REG_COUNTER(&pPGM->StatRZInvalidatePagePDNPs,         "/PGM/CPU%d/RZ/InvalidatePage/PDNPs",       "The number of times PGMInvalidatePage() was called for a not present page directory.");
        PGM_REG_COUNTER(&pPGM->StatRZInvalidatePagePDOutOfSync,   "/PGM/CPU%d/RZ/InvalidatePage/PDOutOfSync", "The number of times PGMInvalidatePage() was called for an out of sync page directory.");
        PGM_REG_COUNTER(&pPGM->StatRZInvalidatePageSkipped,       "/PGM/CPU%d/RZ/InvalidatePage/Skipped",     "The number of times PGMInvalidatePage() was skipped due to not present shw or pending pending SyncCR3.");
        PGM_REG_COUNTER(&pPGM->StatRZPageOutOfSyncSupervisor,     "/PGM/CPU%d/RZ/OutOfSync/SuperVisor",       "Number of traps due to pages out of sync and times VerifyAccessSyncPage calls SyncPage.");
        PGM_REG_COUNTER(&pPGM->StatRZPageOutOfSyncUser,           "/PGM/CPU%d/RZ/OutOfSync/User",             "Number of traps due to pages out of sync and times VerifyAccessSyncPage calls SyncPage.");
        PGM_REG_PROFILE(&pPGM->StatRZPrefetch,                    "/PGM/CPU%d/RZ/Prefetch",                   "PGMPrefetchPage profiling.");
        PGM_REG_PROFILE(&pPGM->StatRZFlushTLB,                    "/PGM/CPU%d/RZ/FlushTLB",                   "Profiling of the PGMFlushTLB() body.");
        PGM_REG_COUNTER(&pPGM->StatRZFlushTLBNewCR3,              "/PGM/CPU%d/RZ/FlushTLB/NewCR3",            "The number of times PGMFlushTLB was called with a new CR3, non-global. (switch)");
        PGM_REG_COUNTER(&pPGM->StatRZFlushTLBNewCR3Global,        "/PGM/CPU%d/RZ/FlushTLB/NewCR3Global",      "The number of times PGMFlushTLB was called with a new CR3, global. (switch)");
        PGM_REG_COUNTER(&pPGM->StatRZFlushTLBSameCR3,             "/PGM/CPU%d/RZ/FlushTLB/SameCR3",           "The number of times PGMFlushTLB was called with the same CR3, non-global. (flush)");
        PGM_REG_COUNTER(&pPGM->StatRZFlushTLBSameCR3Global,       "/PGM/CPU%d/RZ/FlushTLB/SameCR3Global",     "The number of times PGMFlushTLB was called with the same CR3, global. (flush)");
        PGM_REG_PROFILE(&pPGM->StatRZGstModifyPage,               "/PGM/CPU%d/RZ/GstModifyPage",              "Profiling of the PGMGstModifyPage() body.");

        PGM_REG_PROFILE(&pPGM->StatR3SyncCR3,                     "/PGM/CPU%d/R3/SyncCR3",                        "Profiling of the PGMSyncCR3() body.");
        PGM_REG_PROFILE(&pPGM->StatR3SyncCR3Handlers,             "/PGM/CPU%d/R3/SyncCR3/Handlers",               "Profiling of the PGMSyncCR3() update handler section.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncCR3Global,               "/PGM/CPU%d/R3/SyncCR3/Global",                 "The number of global CR3 syncs.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncCR3NotGlobal,            "/PGM/CPU%d/R3/SyncCR3/NotGlobal",              "The number of non-global CR3 syncs.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncCR3DstCacheHit,          "/PGM/CPU%d/R3/SyncCR3/DstChacheHit",           "The number of times we got some kind of a cache hit.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncCR3DstFreed,             "/PGM/CPU%d/R3/SyncCR3/DstFreed",               "The number of times we've had to free a shadow entry.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncCR3DstFreedSrcNP,        "/PGM/CPU%d/R3/SyncCR3/DstFreedSrcNP",          "The number of times we've had to free a shadow entry for which the source entry was not present.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncCR3DstNotPresent,        "/PGM/CPU%d/R3/SyncCR3/DstNotPresent",          "The number of times we've encountered a not present shadow entry for a present guest entry.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncCR3DstSkippedGlobalPD,   "/PGM/CPU%d/R3/SyncCR3/DstSkippedGlobalPD",     "The number of times a global page directory wasn't flushed.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncCR3DstSkippedGlobalPT,   "/PGM/CPU%d/R3/SyncCR3/DstSkippedGlobalPT",     "The number of times a page table with only global entries wasn't flushed.");
        PGM_REG_PROFILE(&pPGM->StatR3SyncPT,                      "/PGM/CPU%d/R3/SyncPT",                         "Profiling of the pfnSyncPT() body.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncPTFailed,                "/PGM/CPU%d/R3/SyncPT/Failed",                  "The number of times pfnSyncPT() failed.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncPT4K,                    "/PGM/CPU%d/R3/SyncPT/4K",                      "Nr of 4K PT syncs");
        PGM_REG_COUNTER(&pPGM->StatR3SyncPT4M,                    "/PGM/CPU%d/R3/SyncPT/4M",                      "Nr of 4M PT syncs");
        PGM_REG_COUNTER(&pPGM->StatR3SyncPagePDNAs,               "/PGM/CPU%d/R3/SyncPagePDNAs",                  "The number of time we've marked a PD not present from SyncPage to virtualize the accessed bit.");
        PGM_REG_COUNTER(&pPGM->StatR3SyncPagePDOutOfSync,         "/PGM/CPU%d/R3/SyncPagePDOutOfSync",            "The number of time we've encountered an out-of-sync PD in SyncPage.");
        PGM_REG_COUNTER(&pPGM->StatR3AccessedPage,                "/PGM/CPU%d/R3/AccessedPage",               "The number of pages marked not present for accessed bit emulation.");
        PGM_REG_PROFILE(&pPGM->StatR3DirtyBitTracking,            "/PGM/CPU%d/R3/DirtyPage",                  "Profiling the dirty bit tracking in CheckPageFault().");
        PGM_REG_COUNTER(&pPGM->StatR3DirtyPage,                   "/PGM/CPU%d/R3/DirtyPage/Mark",             "The number of pages marked read-only for dirty bit tracking.");
        PGM_REG_COUNTER(&pPGM->StatR3DirtyPageBig,                "/PGM/CPU%d/R3/DirtyPage/MarkBig",          "The number of 4MB pages marked read-only for dirty bit tracking.");
        PGM_REG_COUNTER(&pPGM->StatR3DirtyPageSkipped,            "/PGM/CPU%d/R3/DirtyPage/Skipped",          "The number of pages already dirty or readonly.");
        PGM_REG_COUNTER(&pPGM->StatR3DirtyPageTrap,               "/PGM/CPU%d/R3/DirtyPage/Trap",             "The number of traps generated for dirty bit tracking.");
        PGM_REG_COUNTER(&pPGM->StatR3DirtiedPage,                 "/PGM/CPU%d/R3/DirtyPage/SetDirty",         "The number of pages marked dirty because of write accesses.");
        PGM_REG_COUNTER(&pPGM->StatR3DirtyTrackRealPF,            "/PGM/CPU%d/R3/DirtyPage/RealPF",           "The number of real pages faults during dirty bit tracking.");
        PGM_REG_COUNTER(&pPGM->StatR3PageAlreadyDirty,            "/PGM/CPU%d/R3/DirtyPage/AlreadySet",       "The number of pages already marked dirty because of write accesses.");
        PGM_REG_PROFILE(&pPGM->StatR3InvalidatePage,              "/PGM/CPU%d/R3/InvalidatePage",             "PGMInvalidatePage() profiling.");
        PGM_REG_COUNTER(&pPGM->StatR3InvalidatePage4KBPages,      "/PGM/CPU%d/R3/InvalidatePage/4KBPages",    "The number of times PGMInvalidatePage() was called for a 4KB page.");
        PGM_REG_COUNTER(&pPGM->StatR3InvalidatePage4MBPages,      "/PGM/CPU%d/R3/InvalidatePage/4MBPages",    "The number of times PGMInvalidatePage() was called for a 4MB page.");
        PGM_REG_COUNTER(&pPGM->StatR3InvalidatePage4MBPagesSkip,  "/PGM/CPU%d/R3/InvalidatePage/4MBPagesSkip","The number of times PGMInvalidatePage() skipped a 4MB page.");
        PGM_REG_COUNTER(&pPGM->StatR3InvalidatePagePDMappings,    "/PGM/CPU%d/R3/InvalidatePage/PDMappings",  "The number of times PGMInvalidatePage() was called for a page directory containing mappings (no conflict).");
        PGM_REG_COUNTER(&pPGM->StatR3InvalidatePagePDNAs,         "/PGM/CPU%d/R3/InvalidatePage/PDNAs",       "The number of times PGMInvalidatePage() was called for a not accessed page directory.");
        PGM_REG_COUNTER(&pPGM->StatR3InvalidatePagePDNPs,         "/PGM/CPU%d/R3/InvalidatePage/PDNPs",       "The number of times PGMInvalidatePage() was called for a not present page directory.");
        PGM_REG_COUNTER(&pPGM->StatR3InvalidatePagePDOutOfSync,   "/PGM/CPU%d/R3/InvalidatePage/PDOutOfSync", "The number of times PGMInvalidatePage() was called for an out of sync page directory.");
        PGM_REG_COUNTER(&pPGM->StatR3InvalidatePageSkipped,       "/PGM/CPU%d/R3/InvalidatePage/Skipped",     "The number of times PGMInvalidatePage() was skipped due to not present shw or pending pending SyncCR3.");
        PGM_REG_COUNTER(&pPGM->StatR3PageOutOfSyncSupervisor,     "/PGM/CPU%d/R3/OutOfSync/SuperVisor",       "Number of traps due to pages out of sync and times VerifyAccessSyncPage calls SyncPage.");
        PGM_REG_COUNTER(&pPGM->StatR3PageOutOfSyncUser,           "/PGM/CPU%d/R3/OutOfSync/User",             "Number of traps due to pages out of sync and times VerifyAccessSyncPage calls SyncPage.");
        PGM_REG_PROFILE(&pPGM->StatR3Prefetch,                    "/PGM/CPU%d/R3/Prefetch",                   "PGMPrefetchPage profiling.");
        PGM_REG_PROFILE(&pPGM->StatR3FlushTLB,                    "/PGM/CPU%d/R3/FlushTLB",                   "Profiling of the PGMFlushTLB() body.");
        PGM_REG_COUNTER(&pPGM->StatR3FlushTLBNewCR3,              "/PGM/CPU%d/R3/FlushTLB/NewCR3",            "The number of times PGMFlushTLB was called with a new CR3, non-global. (switch)");
        PGM_REG_COUNTER(&pPGM->StatR3FlushTLBNewCR3Global,        "/PGM/CPU%d/R3/FlushTLB/NewCR3Global",      "The number of times PGMFlushTLB was called with a new CR3, global. (switch)");
        PGM_REG_COUNTER(&pPGM->StatR3FlushTLBSameCR3,             "/PGM/CPU%d/R3/FlushTLB/SameCR3",           "The number of times PGMFlushTLB was called with the same CR3, non-global. (flush)");
        PGM_REG_COUNTER(&pPGM->StatR3FlushTLBSameCR3Global,       "/PGM/CPU%d/R3/FlushTLB/SameCR3Global",     "The number of times PGMFlushTLB was called with the same CR3, global. (flush)");
        PGM_REG_PROFILE(&pPGM->StatR3GstModifyPage,               "/PGM/CPU%d/R3/GstModifyPage",              "Profiling of the PGMGstModifyPage() body.");
#endif /* VBOX_WITH_STATISTICS */

#undef PGM_REG_PROFILE
#undef PGM_REG_COUNTER

    }
}


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
VMMR3DECL(int) PGMR3InitDynMap(PVM pVM)
{
    RTGCPTR GCPtr;
    int     rc;

    /*
     * Reserve space for the dynamic mappings.
     */
    rc = MMR3HyperReserve(pVM, MM_HYPER_DYNAMIC_SIZE, "Dynamic mapping", &GCPtr);
    if (RT_SUCCESS(rc))
        pVM->pgm.s.pbDynPageMapBaseGC = GCPtr;

    if (    RT_SUCCESS(rc)
        &&  (pVM->pgm.s.pbDynPageMapBaseGC >> X86_PD_PAE_SHIFT) != ((pVM->pgm.s.pbDynPageMapBaseGC + MM_HYPER_DYNAMIC_SIZE - 1) >> X86_PD_PAE_SHIFT))
    {
        rc = MMR3HyperReserve(pVM, MM_HYPER_DYNAMIC_SIZE, "Dynamic mapping not crossing", &GCPtr);
        if (RT_SUCCESS(rc))
            pVM->pgm.s.pbDynPageMapBaseGC = GCPtr;
    }
    if (RT_SUCCESS(rc))
    {
        AssertRelease((pVM->pgm.s.pbDynPageMapBaseGC >> X86_PD_PAE_SHIFT) == ((pVM->pgm.s.pbDynPageMapBaseGC + MM_HYPER_DYNAMIC_SIZE - 1) >> X86_PD_PAE_SHIFT));
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
VMMR3DECL(int) PGMR3InitFinalize(PVM pVM)
{
    int rc;

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
    pVM->pgm.s.paDynPageMap32BitPTEsGC = pMapping->aPTs[iPT].pPTRC      + iPG * sizeof(pMapping->aPTs[0].pPTR3->a[0]);
    pVM->pgm.s.paDynPageMapPaePTEsGC   = pMapping->aPTs[iPT].paPaePTsRC + iPG * sizeof(pMapping->aPTs[0].paPaePTsR3->a[0]);

    /* init cache */
    RTHCPHYS HCPhysDummy = MMR3PageDummyHCPhys(pVM);
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache); i++)
        pVM->pgm.s.aHCPhysDynPageMapCache[i] = HCPhysDummy;

    for (unsigned i = 0; i < MM_HYPER_DYNAMIC_SIZE; i += PAGE_SIZE)
    {
        rc = PGMMap(pVM, pVM->pgm.s.pbDynPageMapBaseGC + i, HCPhysDummy, PAGE_SIZE, 0);
        AssertRCReturn(rc, rc);
    }

    /*
     * Note that AMD uses all the 8 reserved bits for the address (so 40 bits in total);
     * Intel only goes up to 36 bits, so we stick to 36 as well.
     */
    /** @todo How to test for the 40 bits support? Long mode seems to be the test criterium. */
    uint32_t u32Dummy, u32Features;
    CPUMGetGuestCpuId(VMMGetCpu(pVM), 1, &u32Dummy, &u32Dummy, &u32Dummy, &u32Features);

    if (u32Features & X86_CPUID_FEATURE_EDX_PSE36)
        pVM->pgm.s.GCPhys4MBPSEMask = RT_BIT_64(36) - 1;
    else
        pVM->pgm.s.GCPhys4MBPSEMask = RT_BIT_64(32) - 1;

    /*
     * Allocate memory if we're supposed to do that.
     */
    if (pVM->pgm.s.fRamPreAlloc)
        rc = pgmR3PhysRamPreAllocate(pVM);

    LogRel(("PGMR3InitFinalize: 4 MB PSE mask %RGp\n", pVM->pgm.s.GCPhys4MBPSEMask));
    return rc;
}


/**
 * Applies relocations to data and code managed by this component.
 *
 * This function will be called at init and whenever the VMM need to relocate it
 * self inside the GC.
 *
 * @param   pVM     The VM.
 * @param   offDelta    Relocation delta relative to old location.
 */
VMMR3DECL(void) PGMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("PGMR3Relocate %RGv to %RGv\n", pVM->pgm.s.GCPtrCR3Mapping, pVM->pgm.s.GCPtrCR3Mapping + offDelta));

    /*
     * Paging stuff.
     */
    pVM->pgm.s.GCPtrCR3Mapping += offDelta;

    pgmR3ModeDataInit(pVM, true /* resolve GC/R0 symbols */);

    /* Shadow, guest and both mode switch & relocation for each VCPU. */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU  pVCpu = &pVM->aCpus[i];

        pgmR3ModeDataSwitch(pVM, pVCpu, pVCpu->pgm.s.enmShadowMode, pVCpu->pgm.s.enmGuestMode);

        PGM_SHW_PFN(Relocate, pVCpu)(pVCpu, offDelta);
        PGM_GST_PFN(Relocate, pVCpu)(pVCpu, offDelta);
        PGM_BTH_PFN(Relocate, pVCpu)(pVCpu, offDelta);
    }

    /*
     * Trees.
     */
    pVM->pgm.s.pTreesRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pTreesR3);

    /*
     * Ram ranges.
     */
    if (pVM->pgm.s.pRamRangesR3)
    {
        /* Update the pSelfRC pointers and relink them. */
        for (PPGMRAMRANGE pCur = pVM->pgm.s.pRamRangesR3; pCur; pCur = pCur->pNextR3)
            if (!(pCur->fFlags & PGM_RAM_RANGE_FLAGS_FLOATING))
                pCur->pSelfRC = MMHyperCCToRC(pVM, pCur);
        pgmR3PhysRelinkRamRanges(pVM);
    }

    /*
     * Update the two page directories with all page table mappings.
     * (One or more of them have changed, that's why we're here.)
     */
    pVM->pgm.s.pMappingsRC = MMHyperR3ToRC(pVM, pVM->pgm.s.pMappingsR3);
    for (PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3; pCur->pNextR3; pCur = pCur->pNextR3)
        pCur->pNextRC = MMHyperR3ToRC(pVM, pCur->pNextR3);

    /* Relocate GC addresses of Page Tables. */
    for (PPGMMAPPING pCur = pVM->pgm.s.pMappingsR3; pCur; pCur = pCur->pNextR3)
    {
        for (RTHCUINT i = 0; i < pCur->cPTs; i++)
        {
            pCur->aPTs[i].pPTRC = MMHyperR3ToRC(pVM, pCur->aPTs[i].pPTR3);
            pCur->aPTs[i].paPaePTsRC = MMHyperR3ToRC(pVM, pCur->aPTs[i].paPaePTsR3);
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
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    AssertRelease(pVM->pgm.s.pvZeroPgR0 != NIL_RTR0PTR || !VMMIsHwVirtExtForced(pVM));
#else
    AssertRelease(pVM->pgm.s.pvZeroPgR0 != NIL_RTR0PTR);
#endif

    /*
     * Physical and virtual handlers.
     */
    RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesR3->PhysHandlers,     true, pgmR3RelocatePhysHandler,      &offDelta);
    RTAvlroGCPtrDoWithAll(&pVM->pgm.s.pTreesR3->VirtHandlers,      true, pgmR3RelocateVirtHandler,      &offDelta);
    RTAvlroGCPtrDoWithAll(&pVM->pgm.s.pTreesR3->HyperVirtHandlers, true, pgmR3RelocateHyperVirtHandler, &offDelta);

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
    if (pHandler->pfnHandlerRC)
        pHandler->pfnHandlerRC += offDelta;
    if (pHandler->pvUserRC >= 0x10000)
        pHandler->pvUserRC += offDelta;
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
    Assert(     pHandler->enmType == PGMVIRTHANDLERTYPE_ALL
           ||   pHandler->enmType == PGMVIRTHANDLERTYPE_WRITE);
    Assert(pHandler->pfnHandlerRC);
    pHandler->pfnHandlerRC += offDelta;
    return 0;
}


/**
 * Callback function for relocating a virtual access handler for the hypervisor mapping.
 *
 * @returns 0 (continue enum)
 * @param   pNode       Pointer to a PGMVIRTHANDLER node.
 * @param   pvUser      Pointer to the offDelta. This is a pointer to the delta since we're
 *                      not certain the delta will fit in a void pointer for all possible configs.
 */
static DECLCALLBACK(int) pgmR3RelocateHyperVirtHandler(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMVIRTHANDLER pHandler = (PPGMVIRTHANDLER)pNode;
    RTGCINTPTR      offDelta = *(PRTGCINTPTR)pvUser;
    Assert(pHandler->enmType == PGMVIRTHANDLERTYPE_HYPERVISOR);
    Assert(pHandler->pfnHandlerRC);
    pHandler->pfnHandlerRC  += offDelta;
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
VMMR3DECL(void) PGMR3Reset(PVM pVM)
{
    int rc;

    LogFlow(("PGMR3Reset:\n"));
    VM_ASSERT_EMT(pVM);

    pgmLock(pVM);

    /*
     * Unfix any fixed mappings and disable CR3 monitoring.
     */
    pVM->pgm.s.fMappingsFixed    = false;
    pVM->pgm.s.GCPtrMappingFixed = 0;
    pVM->pgm.s.cbMappingFixed    = 0;

    /* Exit the guest paging mode before the pgm pool gets reset.
     * Important to clean up the amd64 case.
     */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU  pVCpu = &pVM->aCpus[i];

        rc = PGM_GST_PFN(Exit, pVCpu)(pVCpu);
        AssertRC(rc);
    }

#ifdef DEBUG
    DBGFR3InfoLog(pVM, "mappings", NULL);
    DBGFR3InfoLog(pVM, "handlers", "all nostat");
#endif

    /*
     * Reset the shadow page pool.
     */
    pgmR3PoolReset(pVM);

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU  pVCpu = &pVM->aCpus[i];

        /*
         * Re-init other members.
         */
        pVCpu->pgm.s.fA20Enabled = true;

        /*
         * Clear the FFs PGM owns.
         */
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
    }

    /*
     * Reset (zero) RAM pages.
     */
    rc = pgmR3PhysRamReset(pVM);
    if (RT_SUCCESS(rc))
    {
        /*
         * Reset (zero) shadow ROM pages.
         */
        rc = pgmR3PhysRomReset(pVM);
        if (RT_SUCCESS(rc))
        {
            /*
             * Switch mode back to real mode.
             */
            for (unsigned i=0;i<pVM->cCPUs;i++)
            {
                PVMCPU  pVCpu = &pVM->aCpus[i];

                rc = PGMR3ChangeMode(pVM, pVCpu, PGMMODE_REAL);
                AssertRC(rc);

                STAM_REL_COUNTER_RESET(&pVCpu->pgm.s.cGuestModeChanges);
            }
        }
    }

    pgmUnlock(pVM);
    //return rc;
    AssertReleaseRC(rc);
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
 * Terminates the PGM.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to VM structure.
 */
VMMR3DECL(int) PGMR3Term(PVM pVM)
{
    PGMDeregisterStringFormatTypes();
    return PDMR3CritSectDelete(&pVM->pgm.s.CritSect);
}


/**
 * Terminates the per-VCPU PGM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) PGMR3TermCPU(PVM pVM)
{
    return 0;
}


/**
 * Find the ROM tracking structure for the given page.
 *
 * @returns Pointer to the ROM page structure. NULL if the caller didn't check
 *          that it's a ROM page.
 * @param   pVM         The VM handle.
 * @param   GCPhys      The address of the ROM page.
 */
static PPGMROMPAGE pgmR3GetRomPage(PVM pVM, RTGCPHYS GCPhys)
{
    for (PPGMROMRANGE pRomRange = pVM->pgm.s.CTX_SUFF(pRomRanges);
         pRomRange;
         pRomRange = pRomRange->CTX_SUFF(pNext))
    {
        RTGCPHYS off = GCPhys - pRomRange->GCPhys;
        if (GCPhys - pRomRange->GCPhys < pRomRange->cb)
            return &pRomRange->aPages[off >> PAGE_SHIFT];
    }
    return NULL;
}


/**
 * Save zero indicator + bits for the specified page.
 *
 * @returns VBox status code, errors are logged/asserted before returning.
 * @param   pVM         The VM handle.
 * @param   pSSH        The saved state handle.
 * @param   pPage       The page to save.
 * @param   GCPhys      The address of the page.
 * @param   pRam        The ram range (for error logging).
 */
static int pgmR3SavePage(PVM pVM, PSSMHANDLE pSSM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    int rc;
    if (PGM_PAGE_IS_ZERO(pPage))
        rc = SSMR3PutU8(pSSM, 0);
    else
    {
        void const *pvPage;
        rc = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, pPage, GCPhys, &pvPage);
        AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] GCPhys=%#x %s\n", pPage, GCPhys, pRam->pszDesc), rc);

        SSMR3PutU8(pSSM, 1);
        rc = SSMR3PutMem(pSSM, pvPage, PAGE_SIZE);
    }
    return rc;
}


/**
 * Save a shadowed ROM page.
 *
 * Format: Type, protection, and two pages with zero indicators.
 *
 * @returns VBox status code, errors are logged/asserted before returning.
 * @param   pVM         The VM handle.
 * @param   pSSH        The saved state handle.
 * @param   pPage       The page to save.
 * @param   GCPhys      The address of the page.
 * @param   pRam        The ram range (for error logging).
 */
static int pgmR3SaveShadowedRomPage(PVM pVM, PSSMHANDLE pSSM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    /* Need to save both pages and the current state. */
    PPGMROMPAGE pRomPage = pgmR3GetRomPage(pVM, GCPhys);
    AssertLogRelMsgReturn(pRomPage, ("GCPhys=%RGp %s\n", GCPhys, pRam->pszDesc), VERR_INTERNAL_ERROR);

    SSMR3PutU8(pSSM, PGMPAGETYPE_ROM_SHADOW);
    SSMR3PutU8(pSSM, pRomPage->enmProt);

    int rc = pgmR3SavePage(pVM, pSSM, pPage, GCPhys, pRam);
    if (RT_SUCCESS(rc))
    {
        PPGMPAGE pPagePassive = PGMROMPROT_IS_ROM(pRomPage->enmProt) ? &pRomPage->Shadow : &pRomPage->Virgin;
        rc = pgmR3SavePage(pVM, pSSM, pPagePassive, GCPhys, pRam);
    }
    return rc;
}

/** PGM fields to save/load. */
static const SSMFIELD s_aPGMFields[] =
{
    SSMFIELD_ENTRY(         PGM, fMappingsFixed),
    SSMFIELD_ENTRY_GCPTR(   PGM, GCPtrMappingFixed),
    SSMFIELD_ENTRY(         PGM, cbMappingFixed),
    SSMFIELD_ENTRY_TERM()
};

static const SSMFIELD s_aPGMCpuFields[] =
{
    SSMFIELD_ENTRY(         PGMCPU, fA20Enabled),
    SSMFIELD_ENTRY_GCPHYS(  PGMCPU, GCPhysA20Mask),
    SSMFIELD_ENTRY(         PGMCPU, enmGuestMode),
    SSMFIELD_ENTRY_TERM()
};

/* For loading old saved states. (pre-smp) */
typedef struct
{
    /** If set no conflict checks are required.  (boolean) */
    bool                            fMappingsFixed;
    /** Size of fixed mapping */
    uint32_t                        cbMappingFixed;
    /** Base address (GC) of fixed mapping */
    RTGCPTR                         GCPtrMappingFixed;
    /** A20 gate mask.
     * Our current approach to A20 emulation is to let REM do it and don't bother
     * anywhere else. The interesting Guests will be operating with it enabled anyway.
     * But whould need arrise, we'll subject physical addresses to this mask. */
    RTGCPHYS                        GCPhysA20Mask;
    /** A20 gate state - boolean! */
    bool                            fA20Enabled;
    /** The guest paging mode. */
    PGMMODE                         enmGuestMode;
} PGMOLD;

static const SSMFIELD s_aPGMFields_Old[] =
{
    SSMFIELD_ENTRY(         PGMOLD, fMappingsFixed),
    SSMFIELD_ENTRY_GCPTR(   PGMOLD, GCPtrMappingFixed),
    SSMFIELD_ENTRY(         PGMOLD, cbMappingFixed),
    SSMFIELD_ENTRY(         PGMOLD, fA20Enabled),
    SSMFIELD_ENTRY_GCPHYS(  PGMOLD, GCPhysA20Mask),
    SSMFIELD_ENTRY(         PGMOLD, enmGuestMode),
    SSMFIELD_ENTRY_TERM()
};


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) pgmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    int         rc;
    unsigned    i;
    PPGM        pPGM = &pVM->pgm.s;

    /*
     * Lock PGM and set the no-more-writes indicator.
     */
    pgmLock(pVM);
    pVM->pgm.s.fNoMorePhysWrites = true;

    /*
     * Save basic data (required / unaffected by relocation).
     */
    SSMR3PutStruct(pSSM, pPGM, &s_aPGMFields[0]);

    for (i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        SSMR3PutStruct(pSSM, &pVCpu->pgm.s, &s_aPGMCpuFields[0]);
    }

    /*
     * The guest mappings.
     */
    i = 0;
    for (PPGMMAPPING pMapping = pPGM->pMappingsR3; pMapping; pMapping = pMapping->pNextR3, i++)
    {
        SSMR3PutU32(      pSSM, i);
        SSMR3PutStrZ(     pSSM, pMapping->pszDesc); /* This is the best unique id we have... */
        SSMR3PutGCPtr(    pSSM, pMapping->GCPtr);
        SSMR3PutGCUIntPtr(pSSM, pMapping->cPTs);
    }
    rc = SSMR3PutU32(pSSM, ~0); /* terminator. */

    /*
     * Ram ranges and the memory they describe.
     */
    i = 0;
    for (PPGMRAMRANGE pRam = pPGM->pRamRangesR3; pRam; pRam = pRam->pNextR3, i++)
    {
        /*
         * Save the ram range details.
         */
        SSMR3PutU32(pSSM,       i);
        SSMR3PutGCPhys(pSSM,    pRam->GCPhys);
        SSMR3PutGCPhys(pSSM,    pRam->GCPhysLast);
        SSMR3PutGCPhys(pSSM,    pRam->cb);
        SSMR3PutU8(pSSM,        !!pRam->pvR3);      /* Boolean indicating memory or not. */
        SSMR3PutStrZ(pSSM,      pRam->pszDesc);     /* This is the best unique id we have... */

        /*
         * Iterate the pages, only two special case.
         */
        uint32_t const cPages = pRam->cb >> PAGE_SHIFT;
        for (uint32_t iPage = 0; iPage < cPages; iPage++)
        {
            RTGCPHYS GCPhysPage = pRam->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
            PPGMPAGE pPage      = &pRam->aPages[iPage];
            uint8_t  uType      = PGM_PAGE_GET_TYPE(pPage);

            if (uType == PGMPAGETYPE_ROM_SHADOW)
                rc = pgmR3SaveShadowedRomPage(pVM, pSSM, pPage, GCPhysPage, pRam);
            else if (uType == PGMPAGETYPE_MMIO2_ALIAS_MMIO)
            {
                /* MMIO2 alias -> MMIO; the device will just have to deal with this. */
                SSMR3PutU8(pSSM, PGMPAGETYPE_MMIO);
                rc = SSMR3PutU8(pSSM, 0 /* ZERO */);
            }
            else
            {
                SSMR3PutU8(pSSM, uType);
                rc = pgmR3SavePage(pVM, pSSM, pPage, GCPhysPage, pRam);
            }
            if (RT_FAILURE(rc))
                break;
        }
        if (RT_FAILURE(rc))
            break;
    }

    pgmUnlock(pVM);
    return SSMR3PutU32(pSSM, ~0); /* terminator. */
}


/**
 * Load an ignored page.
 *
 * @returns VBox status code.
 * @param   pSSM            The saved state handle.
 */
static int pgmR3LoadPageToDevNull(PSSMHANDLE pSSM)
{
    uint8_t abPage[PAGE_SIZE];
    return SSMR3GetMem(pSSM, &abPage[0], sizeof(abPage));
}


/**
 * Loads a page without any bits in the saved state, i.e. making sure it's
 * really zero.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   uType           The page type or PGMPAGETYPE_INVALID (old saved
 *                          state).
 * @param   pPage           The guest page tracking structure.
 * @param   GCPhys          The page address.
 * @param   pRam            The ram range (logging).
 */
static int pgmR3LoadPageZero(PVM pVM, uint8_t uType, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    if (    PGM_PAGE_GET_TYPE(pPage) != uType
        &&  uType != PGMPAGETYPE_INVALID)
        return VERR_SSM_UNEXPECTED_DATA;

    /* I think this should be sufficient. */
    if (!PGM_PAGE_IS_ZERO(pPage))
        return VERR_SSM_UNEXPECTED_DATA;

    NOREF(pVM);
    NOREF(GCPhys);
    NOREF(pRam);
    return VINF_SUCCESS;
}


/**
 * Loads a page from the saved state.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   pSSM            The SSM handle.
 * @param   uType           The page type or PGMPAGETYEP_INVALID (old saved
 *                          state).
 * @param   pPage           The guest page tracking structure.
 * @param   GCPhys          The page address.
 * @param   pRam            The ram range (logging).
 */
static int pgmR3LoadPageBits(PVM pVM, PSSMHANDLE pSSM, uint8_t uType, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    int rc;

    /*
     * Match up the type, dealing with MMIO2 aliases (dropped).
     */
    AssertLogRelMsgReturn(   PGM_PAGE_GET_TYPE(pPage) == uType
                          || uType == PGMPAGETYPE_INVALID,
                          ("pPage=%R[pgmpage] GCPhys=%#x %s\n", pPage, GCPhys, pRam->pszDesc),
                          VERR_SSM_UNEXPECTED_DATA);

    /*
     * Load the page.
     */
    void *pvPage;
    rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvPage);
    if (RT_SUCCESS(rc))
        rc = SSMR3GetMem(pSSM, pvPage, PAGE_SIZE);

    return rc;
}


/**
 * Loads a page (counter part to pgmR3SavePage).
 *
 * @returns VBox status code, fully bitched errors.
 * @param   pVM             The VM handle.
 * @param   pSSM            The SSM handle.
 * @param   uType           The page type.
 * @param   pPage           The page.
 * @param   GCPhys          The page address.
 * @param   pRam            The RAM range (for error messages).
 */
static int pgmR3LoadPage(PVM pVM, PSSMHANDLE pSSM, uint8_t uType, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    uint8_t         uState;
    int rc = SSMR3GetU8(pSSM, &uState);
    AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] GCPhys=%#x %s rc=%Rrc\n", pPage, GCPhys, pRam->pszDesc, rc), rc);
    if (uState == 0 /* zero */)
        rc = pgmR3LoadPageZero(pVM, uType, pPage, GCPhys, pRam);
    else if (uState == 1)
        rc = pgmR3LoadPageBits(pVM, pSSM, uType, pPage, GCPhys, pRam);
    else
        rc = VERR_INTERNAL_ERROR;
    AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] uState=%d uType=%d GCPhys=%RGp %s rc=%Rrc\n",
                                 pPage, uState, uType, GCPhys, pRam->pszDesc, rc),
                            rc);
    return VINF_SUCCESS;
}


/**
 * Loads a shadowed ROM page.
 *
 * @returns VBox status code, errors are fully bitched.
 * @param   pVM             The VM handle.
 * @param   pSSM            The saved state handle.
 * @param   pPage           The page.
 * @param   GCPhys          The page address.
 * @param   pRam            The RAM range (for error messages).
 */
static int pgmR3LoadShadowedRomPage(PVM pVM, PSSMHANDLE pSSM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    /*
     * Load and set the protection first, then load the two pages, the first
     * one is the active the other is the passive.
     */
    PPGMROMPAGE pRomPage = pgmR3GetRomPage(pVM, GCPhys);
    AssertLogRelMsgReturn(pRomPage, ("GCPhys=%RGp %s\n", GCPhys, pRam->pszDesc), VERR_INTERNAL_ERROR);

    uint8_t     uProt;
    int rc = SSMR3GetU8(pSSM, &uProt);
    AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] GCPhys=%#x %s\n", pPage, GCPhys, pRam->pszDesc), rc);
    PGMROMPROT  enmProt = (PGMROMPROT)uProt;
    AssertLogRelMsgReturn(    enmProt >= PGMROMPROT_INVALID
                          &&  enmProt <  PGMROMPROT_END,
                          ("enmProt=%d pPage=%R[pgmpage] GCPhys=%#x %s\n", enmProt, pPage, GCPhys, pRam->pszDesc),
                          VERR_SSM_UNEXPECTED_DATA);

    if (pRomPage->enmProt != enmProt)
    {
        rc = PGMR3PhysRomProtect(pVM, GCPhys, PAGE_SIZE, enmProt);
        AssertLogRelRCReturn(rc, rc);
        AssertLogRelReturn(pRomPage->enmProt == enmProt, VERR_INTERNAL_ERROR);
    }

    PPGMPAGE pPageActive  = PGMROMPROT_IS_ROM(enmProt) ? &pRomPage->Virgin      : &pRomPage->Shadow;
    PPGMPAGE pPagePassive = PGMROMPROT_IS_ROM(enmProt) ? &pRomPage->Shadow      : &pRomPage->Virgin;
    uint8_t  u8ActiveType = PGMROMPROT_IS_ROM(enmProt) ? PGMPAGETYPE_ROM        : PGMPAGETYPE_ROM_SHADOW;
    uint8_t  u8PassiveType= PGMROMPROT_IS_ROM(enmProt) ? PGMPAGETYPE_ROM_SHADOW : PGMPAGETYPE_ROM;

    rc = pgmR3LoadPage(pVM, pSSM, u8ActiveType, pPage, GCPhys, pRam);
    if (RT_SUCCESS(rc))
    {
        *pPageActive = *pPage;
        rc = pgmR3LoadPage(pVM, pSSM, u8PassiveType, pPagePassive, GCPhys, pRam);
    }
    return rc;
}


/**
 * Worker for pgmR3Load.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The VM handle.
 * @param   pSSM                The SSM handle.
 * @param   u32Version          The saved state version.
 */
static int pgmR3LoadLocked(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    int         rc;
    PPGM        pPGM = &pVM->pgm.s;
    uint32_t    u32Sep;

    /*
     * Load basic data (required / unaffected by relocation).
     */
    if (u32Version >= PGM_SAVED_STATE_VERSION)
    {
        rc = SSMR3GetStruct(pSSM, pPGM, &s_aPGMFields[0]);
        AssertLogRelRCReturn(rc, rc);

        for (unsigned i=0;i<pVM->cCPUs;i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            rc = SSMR3GetStruct(pSSM, &pVCpu->pgm.s, &s_aPGMCpuFields[0]);
            AssertLogRelRCReturn(rc, rc);
        }
    }
    else
    if (u32Version >= PGM_SAVED_STATE_VERSION_RR_DESC)
    {
        PGMOLD pgmOld;

        AssertRelease(pVM->cCPUs == 1);

        rc = SSMR3GetStruct(pSSM, &pgmOld, &s_aPGMFields_Old[0]);
        AssertLogRelRCReturn(rc, rc);

        pPGM->fMappingsFixed    = pgmOld.fMappingsFixed;
        pPGM->GCPtrMappingFixed = pgmOld.GCPtrMappingFixed;
        pPGM->cbMappingFixed    = pgmOld.cbMappingFixed;

        pVM->aCpus[0].pgm.s.fA20Enabled   = pgmOld.fA20Enabled;
        pVM->aCpus[0].pgm.s.GCPhysA20Mask = pgmOld.GCPhysA20Mask;
        pVM->aCpus[0].pgm.s.enmGuestMode  = pgmOld.enmGuestMode;
    }
    else
    {
        AssertRelease(pVM->cCPUs == 1);

        SSMR3GetBool(pSSM,      &pPGM->fMappingsFixed);
        SSMR3GetGCPtr(pSSM,     &pPGM->GCPtrMappingFixed);
        SSMR3GetU32(pSSM,       &pPGM->cbMappingFixed);

        uint32_t cbRamSizeIgnored;
        rc = SSMR3GetU32(pSSM, &cbRamSizeIgnored);
        if (RT_FAILURE(rc))
            return rc;
        SSMR3GetGCPhys(pSSM,    &pVM->aCpus[0].pgm.s.GCPhysA20Mask);

        uint32_t u32 = 0;
        SSMR3GetUInt(pSSM,      &u32);
        pVM->aCpus[0].pgm.s.fA20Enabled = !!u32;
        SSMR3GetUInt(pSSM,      &pVM->aCpus[0].pgm.s.fSyncFlags);
        RTUINT uGuestMode;
        SSMR3GetUInt(pSSM,      &uGuestMode);
        pVM->aCpus[0].pgm.s.enmGuestMode = (PGMMODE)uGuestMode;

        /* check separator. */
        SSMR3GetU32(pSSM, &u32Sep);
        if (RT_FAILURE(rc))
            return rc;
        if (u32Sep != (uint32_t)~0)
        {
            AssertMsgFailed(("u32Sep=%#x (first)\n", u32Sep));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }
    }

    /*
     * The guest mappings.
     */
    uint32_t i = 0;
    for (;; i++)
    {
        /* Check the seqence number / separator. */
        rc = SSMR3GetU32(pSSM, &u32Sep);
        if (RT_FAILURE(rc))
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
        if (RT_FAILURE(rc))
            return rc;
        RTGCPTR GCPtr;
        SSMR3GetGCPtr(pSSM, &GCPtr);
        RTGCPTR cPTs;
        rc = SSMR3GetGCUIntPtr(pSSM, &cPTs);
        if (RT_FAILURE(rc))
            return rc;

        /* find matching range. */
        PPGMMAPPING pMapping;
        for (pMapping = pPGM->pMappingsR3; pMapping; pMapping = pMapping->pNextR3)
            if (    pMapping->cPTs == cPTs
                &&  !strcmp(pMapping->pszDesc, szDesc))
                break;
        AssertLogRelMsgReturn(pMapping, ("Couldn't find mapping: cPTs=%#x szDesc=%s (GCPtr=%RGv)\n",
                                         cPTs, szDesc, GCPtr),
                              VERR_SSM_LOAD_CONFIG_MISMATCH);

        /* relocate it. */
        if (pMapping->GCPtr != GCPtr)
        {
            AssertMsg((GCPtr >> X86_PD_SHIFT << X86_PD_SHIFT) == GCPtr, ("GCPtr=%RGv\n", GCPtr));
            pgmR3MapRelocate(pVM, pMapping, pMapping->GCPtr, GCPtr);
        }
        else
            Log(("pgmR3Load: '%s' needed no relocation (%RGv)\n", szDesc, GCPtr));
    }

    /*
     * Ram range flags and bits.
     */
    i = 0;
    for (PPGMRAMRANGE pRam = pPGM->pRamRangesR3; pRam; pRam = pRam->pNextR3, i++)
    {
        /** @todo MMIO ranges may move (PCI reconfig), we currently assume they don't. */

        /* Check the seqence number / separator. */
        rc = SSMR3GetU32(pSSM, &u32Sep);
        if (RT_FAILURE(rc))
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
        if (RT_FAILURE(rc))
            return rc;
        if (fHaveBits & ~1)
        {
            AssertMsgFailed(("u32Sep=%#x (last)\n", u32Sep));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }
        size_t  cchDesc = 0;
        char    szDesc[256];
        szDesc[0] = '\0';
        if (u32Version >= PGM_SAVED_STATE_VERSION_RR_DESC)
        {
            rc = SSMR3GetStrZ(pSSM, szDesc, sizeof(szDesc));
            if (RT_FAILURE(rc))
                return rc;
            /* Since we've modified the description strings in r45878, only compare
               them if the saved state is more recent. */
            if (u32Version != PGM_SAVED_STATE_VERSION_RR_DESC)
                cchDesc = strlen(szDesc);
        }

        /*
         * Match it up with the current range.
         *
         * Note there is a hack for dealing with the high BIOS mapping
         * in the old saved state format, this means we might not have
         * a 1:1 match on success.
         */
        if (    (   GCPhys     != pRam->GCPhys
                 || GCPhysLast != pRam->GCPhysLast
                 || cb         != pRam->cb
                 ||  (   cchDesc
                      && strcmp(szDesc, pRam->pszDesc)) )
                /* Hack for PDMDevHlpPhysReserve(pDevIns, 0xfff80000, 0x80000, "High ROM Region"); */
            &&  (   u32Version != PGM_SAVED_STATE_VERSION_OLD_PHYS_CODE
                 || GCPhys     != UINT32_C(0xfff80000)
                 || GCPhysLast != UINT32_C(0xffffffff)
                 || pRam->GCPhysLast != GCPhysLast
                 || pRam->GCPhys     <  GCPhys
                 || !fHaveBits)
           )
        {
            LogRel(("Ram range: %RGp-%RGp %RGp bytes %s %s\n"
                    "State    : %RGp-%RGp %RGp bytes %s %s\n",
                    pRam->GCPhys, pRam->GCPhysLast, pRam->cb, pRam->pvR3 ? "bits" : "nobits", pRam->pszDesc,
                    GCPhys, GCPhysLast, cb, fHaveBits ? "bits" : "nobits", szDesc));
            /*
             * If we're loading a state for debugging purpose, don't make a fuss if
             * the MMIO and ROM stuff isn't 100% right, just skip the mismatches.
             */
            if (    SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT
                ||  GCPhys < 8 * _1M)
                AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);

            AssertMsgFailed(("debug skipping not implemented, sorry\n"));
            continue;
        }

        uint32_t cPages = (GCPhysLast - GCPhys + 1) >> PAGE_SHIFT;
        if (u32Version >= PGM_SAVED_STATE_VERSION_RR_DESC)
        {
            /*
             * Load the pages one by one.
             */
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                RTGCPHYS const  GCPhysPage = ((RTGCPHYS)iPage << PAGE_SHIFT) + pRam->GCPhys;
                PPGMPAGE        pPage      = &pRam->aPages[iPage];
                uint8_t         uType;
                rc = SSMR3GetU8(pSSM, &uType);
                AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] iPage=%#x GCPhysPage=%#x %s\n", pPage, iPage, GCPhysPage, pRam->pszDesc), rc);
                if (uType == PGMPAGETYPE_ROM_SHADOW)
                    rc = pgmR3LoadShadowedRomPage(pVM, pSSM, pPage, GCPhysPage, pRam);
                else
                    rc = pgmR3LoadPage(pVM, pSSM, uType, pPage, GCPhysPage, pRam);
                AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhysPage=%#x %s\n", rc, iPage, GCPhysPage, pRam->pszDesc), rc);
            }
        }
        else
        {
            /*
             * Old format.
             */
            AssertLogRelReturn(!pVM->pgm.s.fRamPreAlloc, VERR_NOT_SUPPORTED); /* can't be detected. */

            /* Of the page flags, pick up MMIO2 and ROM/RESERVED for the !fHaveBits case.
               The rest is generally irrelevant and wrong since the stuff have to match registrations. */
            uint32_t fFlags = 0;
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                uint16_t u16Flags;
                rc = SSMR3GetU16(pSSM, &u16Flags);
                AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhys=%#x %s\n", rc, iPage, pRam->GCPhys, pRam->pszDesc), rc);
                fFlags |= u16Flags;
            }

            /* Load the bits */
            if (    !fHaveBits
                &&  GCPhysLast < UINT32_C(0xe0000000))
            {
                /*
                 * Dynamic chunks.
                 */
                const uint32_t cPagesInChunk = (1*1024*1024) >> PAGE_SHIFT;
                AssertLogRelMsgReturn(cPages % cPagesInChunk == 0,
                                      ("cPages=%#x cPagesInChunk=%#x\n", cPages, cPagesInChunk, pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                for (uint32_t iPage = 0; iPage < cPages; /* incremented by inner loop */ )
                {
                    uint8_t fPresent;
                    rc = SSMR3GetU8(pSSM, &fPresent);
                    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhys=%#x %s\n", rc, iPage, pRam->GCPhys, pRam->pszDesc), rc);
                    AssertLogRelMsgReturn(fPresent == (uint8_t)true || fPresent == (uint8_t)false,
                                          ("fPresent=%#x iPage=%#x GCPhys=%#x %s\n", fPresent, iPage, pRam->GCPhys, pRam->pszDesc),
                                          VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                    for (uint32_t iChunkPage = 0; iChunkPage < cPagesInChunk; iChunkPage++, iPage++)
                    {
                        RTGCPHYS const  GCPhysPage = ((RTGCPHYS)iPage << PAGE_SHIFT) + pRam->GCPhys;
                        PPGMPAGE        pPage      = &pRam->aPages[iPage];
                        if (fPresent)
                        {
                            if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO)
                                rc = pgmR3LoadPageToDevNull(pSSM);
                            else
                                rc = pgmR3LoadPageBits(pVM, pSSM, PGMPAGETYPE_INVALID, pPage, GCPhysPage, pRam);
                        }
                        else
                            rc = pgmR3LoadPageZero(pVM, PGMPAGETYPE_INVALID, pPage, GCPhysPage, pRam);
                        AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhysPage=%#x %s\n", rc, iPage, GCPhysPage, pRam->pszDesc), rc);
                    }
                }
            }
            else if (pRam->pvR3)
            {
                /*
                 * MMIO2.
                 */
                AssertLogRelMsgReturn((fFlags & 0x0f) == RT_BIT(3) /*MM_RAM_FLAGS_MMIO2*/,
                                      ("fFlags=%#x GCPhys=%#x %s\n", fFlags, pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
                AssertLogRelMsgReturn(pRam->pvR3,
                                      ("GCPhys=%#x %s\n", pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                rc = SSMR3GetMem(pSSM, pRam->pvR3, pRam->cb);
                AssertLogRelMsgRCReturn(rc, ("GCPhys=%#x %s\n", pRam->GCPhys, pRam->pszDesc), rc);
            }
            else if (GCPhysLast < UINT32_C(0xfff80000))
            {
                /*
                 * PCI MMIO, no pages saved.
                 */
            }
            else
            {
                /*
                 * Load the 0xfff80000..0xffffffff BIOS range.
                 * It starts with X reserved pages that we have to skip over since
                 * the RAMRANGE create by the new code won't include those.
                 */
                AssertLogRelMsgReturn(   !(fFlags & RT_BIT(3) /*MM_RAM_FLAGS_MMIO2*/)
                                      && (fFlags  & RT_BIT(0) /*MM_RAM_FLAGS_RESERVED*/),
                                      ("fFlags=%#x GCPhys=%#x %s\n", fFlags, pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
                AssertLogRelMsgReturn(GCPhys == UINT32_C(0xfff80000),
                                      ("GCPhys=%RGp pRamRange{GCPhys=%#x %s}\n", GCPhys, pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                /* Skip wasted reserved pages before the ROM. */
                while (GCPhys < pRam->GCPhys)
                {
                    rc = pgmR3LoadPageToDevNull(pSSM);
                    GCPhys += PAGE_SIZE;
                }

                /* Load the bios pages. */
                cPages = pRam->cb >> PAGE_SHIFT;
                for (uint32_t iPage = 0; iPage < cPages; iPage++)
                {
                    RTGCPHYS const  GCPhysPage = ((RTGCPHYS)iPage << PAGE_SHIFT) + pRam->GCPhys;
                    PPGMPAGE        pPage      = &pRam->aPages[iPage];

                    AssertLogRelMsgReturn(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_ROM,
                                          ("GCPhys=%RGp pPage=%R[pgmpage]\n", GCPhys, GCPhys),
                                          VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
                    rc = pgmR3LoadPageBits(pVM, pSSM, PGMPAGETYPE_ROM, pPage, GCPhysPage, pRam);
                    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhys=%#x %s\n", rc, iPage, pRam->GCPhys, pRam->pszDesc), rc);
                }
            }
        }
    }

    return rc;
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
    int     rc;
    PPGM    pPGM = &pVM->pgm.s;

    /*
     * Validate version.
     */
    if (    u32Version != PGM_SAVED_STATE_VERSION
        &&  u32Version != PGM_SAVED_STATE_VERSION_2_2_2
        &&  u32Version != PGM_SAVED_STATE_VERSION_RR_DESC
        &&  u32Version != PGM_SAVED_STATE_VERSION_OLD_PHYS_CODE)
    {
        AssertMsgFailed(("pgmR3Load: Invalid version u32Version=%d (current %d)!\n", u32Version, PGM_SAVED_STATE_VERSION));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Call the reset function to make sure all the memory is cleared.
     */
    PGMR3Reset(pVM);

    /*
     * Do the loading while owning the lock because a bunch of the functions
     * we're using requires this.
     */
    pgmLock(pVM);
    rc = pgmR3LoadLocked(pVM, pSSM, u32Version);
    pgmUnlock(pVM);
    if (RT_SUCCESS(rc))
    {
        /*
         * We require a full resync now.
         */
        for (unsigned i=0;i<pVM->cCPUs;i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];
            VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
            VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);

            pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
        }

        pPGM->fPhysCacheFlushPending = true;
        pgmR3HandlerPhysicalUpdateAll(pVM);

        for (unsigned i=0;i<pVM->cCPUs;i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            /*
             * Change the paging mode.
             */
            rc = PGMR3ChangeMode(pVM, pVCpu, pVCpu->pgm.s.enmGuestMode);

            /* Restore pVM->pgm.s.GCPhysCR3. */
            Assert(pVCpu->pgm.s.GCPhysCR3 == NIL_RTGCPHYS);
            RTGCPHYS GCPhysCR3 = CPUMGetGuestCR3(pVCpu);
            if (    pVCpu->pgm.s.enmGuestMode == PGMMODE_PAE
                ||  pVCpu->pgm.s.enmGuestMode == PGMMODE_PAE_NX
                ||  pVCpu->pgm.s.enmGuestMode == PGMMODE_AMD64
                ||  pVCpu->pgm.s.enmGuestMode == PGMMODE_AMD64_NX)
                GCPhysCR3 = (GCPhysCR3 & X86_CR3_PAE_PAGE_MASK);
            else
                GCPhysCR3 = (GCPhysCR3 & X86_CR3_PAGE_MASK);
            pVCpu->pgm.s.GCPhysCR3 = GCPhysCR3;
        }
    }

    return rc;
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

    /** @todo SMP support! */
    /* print info. */
    if (fGuest)
        pHlp->pfnPrintf(pHlp, "Guest paging mode:  %s, changed %RU64 times, A20 %s\n",
                        PGMGetModeName(pVM->aCpus[0].pgm.s.enmGuestMode), pVM->aCpus[0].pgm.s.cGuestModeChanges.c,
                        pVM->aCpus[0].pgm.s.fA20Enabled ? "enabled" : "disabled");
    if (fShadow)
        pHlp->pfnPrintf(pHlp, "Shadow paging mode: %s\n", PGMGetModeName(pVM->aCpus[0].pgm.s.enmShadowMode));
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

    for (PPGMRAMRANGE pCur = pVM->pgm.s.pRamRangesR3; pCur; pCur = pCur->pNextR3)
        pHlp->pfnPrintf(pHlp,
                        "%RGp-%RGp %RHv %s\n",
                        pCur->GCPhys,
                        pCur->GCPhysLast,
                        pCur->pvR3,
                        pCur->pszDesc);
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
    /** @todo SMP support!! */
    PVMCPU pVCpu = &pVM->aCpus[0];

/** @todo fix this! Convert the PGMR3DumpHierarchyHC functions to do guest stuff. */
    /* Big pages supported? */
    const bool fPSE  = !!(CPUMGetGuestCR4(pVCpu) & X86_CR4_PSE);

    /* Global pages supported? */
    const bool fPGE = !!(CPUMGetGuestCR4(pVCpu) & X86_CR4_PGE);

    NOREF(pszArgs);

    /*
     * Get page directory addresses.
     */
    PX86PD     pPDSrc = pgmGstGet32bitPDPtr(&pVCpu->pgm.s);
    Assert(pPDSrc);
    Assert(PGMPhysGCPhys2R3PtrAssert(pVM, (RTGCPHYS)(CPUMGetGuestCR3(pVCpu) & X86_CR3_PAGE_MASK), sizeof(*pPDSrc)) == pPDSrc);

    /*
     * Iterate the page directory.
     */
    for (unsigned iPD = 0; iPD < RT_ELEMENTS(pPDSrc->a); iPD++)
    {
        X86PDE PdeSrc = pPDSrc->a[iPD];
        if (PdeSrc.n.u1Present)
        {
            if (PdeSrc.b.u1Size && fPSE)
                pHlp->pfnPrintf(pHlp,
                                "%04X - %RGp P=%d U=%d RW=%d G=%d - BIG\n",
                                iPD,
                                pgmGstGet4MBPhysPage(&pVM->pgm.s, PdeSrc),
                                PdeSrc.b.u1Present, PdeSrc.b.u1User, PdeSrc.b.u1Write, PdeSrc.b.u1Global && fPGE);
            else
                pHlp->pfnPrintf(pHlp,
                                "%04X - %RGp P=%d U=%d RW=%d [G=%d]\n",
                                iPD,
                                (RTGCPHYS)(PdeSrc.u & X86_PDE_PG_MASK),
                                PdeSrc.n.u1Present, PdeSrc.n.u1User, PdeSrc.n.u1Write, PdeSrc.b.u1Global && fPGE);
        }
    }
}


/**
 * Service a VMMCALLHOST_PGM_LOCK call.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
VMMR3DECL(int) PGMR3LockCall(PVM pVM)
{
    int rc = PDMR3CritSectEnterEx(&pVM->pgm.s.CritSect, true /* fHostCall */);
    AssertRC(rc);
    return rc;
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
        case PGMMODE_NESTED:    return PGM_TYPE_NESTED;
        case PGMMODE_EPT:       return PGM_TYPE_EPT;
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
    Assert(uShwType >= PGM_TYPE_32BIT && uShwType <= PGM_TYPE_MAX);
    Assert(uGstType >= PGM_TYPE_REAL  && uGstType <= PGM_TYPE_AMD64);
    return (uShwType - PGM_TYPE_32BIT) * (PGM_TYPE_AMD64 - PGM_TYPE_REAL + 1)
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
 * @returns The number of entries in the paging data array.
 */
DECLINLINE(unsigned) pgmModeDataMaxIndex(void)
{
    return pgmModeDataIndex(PGM_TYPE_MAX, PGM_TYPE_AMD64) + 1;
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

#ifdef VBOX_WITH_64_BITS_GUESTS
    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_AMD64, PGM_TYPE_AMD64)];
    pModeData->uShwType = PGM_TYPE_AMD64;
    pModeData->uGstType = PGM_TYPE_AMD64;
    rc = PGM_SHW_NAME_AMD64(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_AMD64(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_AMD64_AMD64(InitData)( pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
#endif

    /* The nested paging mode. */
    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, PGM_TYPE_REAL)];
    pModeData->uShwType = PGM_TYPE_NESTED;
    pModeData->uGstType = PGM_TYPE_REAL;
    rc = PGM_GST_NAME_REAL(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_NESTED_REAL(InitData)( pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, PGMMODE_PROTECTED)];
    pModeData->uShwType = PGM_TYPE_NESTED;
    pModeData->uGstType = PGM_TYPE_PROT;
    rc = PGM_GST_NAME_PROT(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_NESTED_PROT(InitData)( pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, PGM_TYPE_32BIT)];
    pModeData->uShwType = PGM_TYPE_NESTED;
    pModeData->uGstType = PGM_TYPE_32BIT;
    rc = PGM_GST_NAME_32BIT(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_NESTED_32BIT(InitData)(pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, PGM_TYPE_PAE)];
    pModeData->uShwType = PGM_TYPE_NESTED;
    pModeData->uGstType = PGM_TYPE_PAE;
    rc = PGM_GST_NAME_PAE(InitData)(         pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_NESTED_PAE(InitData)(  pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_64_BITS_GUESTS
    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, PGM_TYPE_AMD64)];
    pModeData->uShwType = PGM_TYPE_NESTED;
    pModeData->uGstType = PGM_TYPE_AMD64;
    rc = PGM_GST_NAME_AMD64(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_NESTED_AMD64(InitData)( pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
#endif

    /* The shadow part of the nested callback mode depends on the host paging mode (AMD-V only). */
    switch (pVM->pgm.s.enmHostMode)
    {
#if HC_ARCH_BITS == 32
    case SUPPAGINGMODE_32_BIT:
    case SUPPAGINGMODE_32_BIT_GLOBAL:
        for (unsigned i = PGM_TYPE_REAL; i <= PGM_TYPE_PAE; i++)
        {
            pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, i)];
            rc = PGM_SHW_NAME_32BIT(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
        }
# ifdef VBOX_WITH_64_BITS_GUESTS
        pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, PGM_TYPE_AMD64)];
        rc = PGM_SHW_NAME_AMD64(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
# endif
        break;

    case SUPPAGINGMODE_PAE:
    case SUPPAGINGMODE_PAE_NX:
    case SUPPAGINGMODE_PAE_GLOBAL:
    case SUPPAGINGMODE_PAE_GLOBAL_NX:
        for (unsigned i = PGM_TYPE_REAL; i <= PGM_TYPE_PAE; i++)
        {
            pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, i)];
            rc = PGM_SHW_NAME_PAE(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
        }
# ifdef VBOX_WITH_64_BITS_GUESTS
        pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, PGM_TYPE_AMD64)];
        rc = PGM_SHW_NAME_AMD64(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
# endif
        break;
#endif /* HC_ARCH_BITS == 32 */

#if HC_ARCH_BITS == 64 || defined(RT_OS_DARWIN)
    case SUPPAGINGMODE_AMD64:
    case SUPPAGINGMODE_AMD64_GLOBAL:
    case SUPPAGINGMODE_AMD64_NX:
    case SUPPAGINGMODE_AMD64_GLOBAL_NX:
# ifdef VBOX_WITH_64_BITS_GUESTS
        for (unsigned i = PGM_TYPE_REAL; i <= PGM_TYPE_AMD64; i++)
# else
        for (unsigned i = PGM_TYPE_REAL; i <= PGM_TYPE_PAE; i++)
# endif
        {
            pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_NESTED, i)];
            rc = PGM_SHW_NAME_AMD64(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
        }
        break;
#endif /* HC_ARCH_BITS == 64 || RT_OS_DARWIN */

    default:
        AssertFailed();
        break;
    }

    /* Extended paging (EPT) / Intel VT-x */
    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_EPT, PGM_TYPE_REAL)];
    pModeData->uShwType = PGM_TYPE_EPT;
    pModeData->uGstType = PGM_TYPE_REAL;
    rc = PGM_SHW_NAME_EPT(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_REAL(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_EPT_REAL(InitData)(   pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_EPT, PGM_TYPE_PROT)];
    pModeData->uShwType = PGM_TYPE_EPT;
    pModeData->uGstType = PGM_TYPE_PROT;
    rc = PGM_SHW_NAME_EPT(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_PROT(InitData)(       pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_EPT_PROT(InitData)(   pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_EPT, PGM_TYPE_32BIT)];
    pModeData->uShwType = PGM_TYPE_EPT;
    pModeData->uGstType = PGM_TYPE_32BIT;
    rc = PGM_SHW_NAME_EPT(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_32BIT(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_EPT_32BIT(InitData)(  pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_EPT, PGM_TYPE_PAE)];
    pModeData->uShwType = PGM_TYPE_EPT;
    pModeData->uGstType = PGM_TYPE_PAE;
    rc = PGM_SHW_NAME_EPT(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_PAE(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_EPT_PAE(InitData)(    pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_64_BITS_GUESTS
    pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndex(PGM_TYPE_EPT, PGM_TYPE_AMD64)];
    pModeData->uShwType = PGM_TYPE_EPT;
    pModeData->uGstType = PGM_TYPE_AMD64;
    rc = PGM_SHW_NAME_EPT(InitData)(        pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_GST_NAME_AMD64(InitData)(      pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
    rc = PGM_BTH_NAME_EPT_AMD64(InitData)(  pVM, pModeData, fResolveGCAndR0); AssertRCReturn(rc, rc);
#endif
    return VINF_SUCCESS;
}


/**
 * Switch to different (or relocated in the relocate case) mode data.
 *
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   enmShw      The the shadow paging mode.
 * @param   enmGst      The the guest paging mode.
 */
static void pgmR3ModeDataSwitch(PVM pVM, PVMCPU pVCpu, PGMMODE enmShw, PGMMODE enmGst)
{
    PPGMMODEDATA pModeData = &pVM->pgm.s.paModeData[pgmModeDataIndexByMode(enmShw, enmGst)];

    Assert(pModeData->uGstType == pgmModeToType(enmGst));
    Assert(pModeData->uShwType == pgmModeToType(enmShw));

    /* shadow */
    pVCpu->pgm.s.pfnR3ShwRelocate             = pModeData->pfnR3ShwRelocate;
    pVCpu->pgm.s.pfnR3ShwExit                 = pModeData->pfnR3ShwExit;
    pVCpu->pgm.s.pfnR3ShwGetPage              = pModeData->pfnR3ShwGetPage;
    Assert(pVCpu->pgm.s.pfnR3ShwGetPage);
    pVCpu->pgm.s.pfnR3ShwModifyPage           = pModeData->pfnR3ShwModifyPage;

    pVCpu->pgm.s.pfnRCShwGetPage              = pModeData->pfnRCShwGetPage;
    pVCpu->pgm.s.pfnRCShwModifyPage           = pModeData->pfnRCShwModifyPage;

    pVCpu->pgm.s.pfnR0ShwGetPage              = pModeData->pfnR0ShwGetPage;
    pVCpu->pgm.s.pfnR0ShwModifyPage           = pModeData->pfnR0ShwModifyPage;


    /* guest */
    pVCpu->pgm.s.pfnR3GstRelocate             = pModeData->pfnR3GstRelocate;
    pVCpu->pgm.s.pfnR3GstExit                 = pModeData->pfnR3GstExit;
    pVCpu->pgm.s.pfnR3GstGetPage              = pModeData->pfnR3GstGetPage;
    Assert(pVCpu->pgm.s.pfnR3GstGetPage);
    pVCpu->pgm.s.pfnR3GstModifyPage           = pModeData->pfnR3GstModifyPage;
    pVCpu->pgm.s.pfnR3GstGetPDE               = pModeData->pfnR3GstGetPDE;
    pVCpu->pgm.s.pfnRCGstGetPage              = pModeData->pfnRCGstGetPage;
    pVCpu->pgm.s.pfnRCGstModifyPage           = pModeData->pfnRCGstModifyPage;
    pVCpu->pgm.s.pfnRCGstGetPDE               = pModeData->pfnRCGstGetPDE;
    pVCpu->pgm.s.pfnR0GstGetPage              = pModeData->pfnR0GstGetPage;
    pVCpu->pgm.s.pfnR0GstModifyPage           = pModeData->pfnR0GstModifyPage;
    pVCpu->pgm.s.pfnR0GstGetPDE               = pModeData->pfnR0GstGetPDE;

    /* both */
    pVCpu->pgm.s.pfnR3BthRelocate             = pModeData->pfnR3BthRelocate;
    pVCpu->pgm.s.pfnR3BthInvalidatePage       = pModeData->pfnR3BthInvalidatePage;
    pVCpu->pgm.s.pfnR3BthSyncCR3              = pModeData->pfnR3BthSyncCR3;
    Assert(pVCpu->pgm.s.pfnR3BthSyncCR3);
    pVCpu->pgm.s.pfnR3BthSyncPage             = pModeData->pfnR3BthSyncPage;
    pVCpu->pgm.s.pfnR3BthPrefetchPage         = pModeData->pfnR3BthPrefetchPage;
    pVCpu->pgm.s.pfnR3BthVerifyAccessSyncPage = pModeData->pfnR3BthVerifyAccessSyncPage;
#ifdef VBOX_STRICT
    pVCpu->pgm.s.pfnR3BthAssertCR3            = pModeData->pfnR3BthAssertCR3;
#endif
    pVCpu->pgm.s.pfnR3BthMapCR3               = pModeData->pfnR3BthMapCR3;
    pVCpu->pgm.s.pfnR3BthUnmapCR3             = pModeData->pfnR3BthUnmapCR3;

    pVCpu->pgm.s.pfnRCBthTrap0eHandler        = pModeData->pfnRCBthTrap0eHandler;
    pVCpu->pgm.s.pfnRCBthInvalidatePage       = pModeData->pfnRCBthInvalidatePage;
    pVCpu->pgm.s.pfnRCBthSyncCR3              = pModeData->pfnRCBthSyncCR3;
    pVCpu->pgm.s.pfnRCBthSyncPage             = pModeData->pfnRCBthSyncPage;
    pVCpu->pgm.s.pfnRCBthPrefetchPage         = pModeData->pfnRCBthPrefetchPage;
    pVCpu->pgm.s.pfnRCBthVerifyAccessSyncPage = pModeData->pfnRCBthVerifyAccessSyncPage;
#ifdef VBOX_STRICT
    pVCpu->pgm.s.pfnRCBthAssertCR3            = pModeData->pfnRCBthAssertCR3;
#endif
    pVCpu->pgm.s.pfnRCBthMapCR3               = pModeData->pfnRCBthMapCR3;
    pVCpu->pgm.s.pfnRCBthUnmapCR3             = pModeData->pfnRCBthUnmapCR3;

    pVCpu->pgm.s.pfnR0BthTrap0eHandler        = pModeData->pfnR0BthTrap0eHandler;
    pVCpu->pgm.s.pfnR0BthInvalidatePage       = pModeData->pfnR0BthInvalidatePage;
    pVCpu->pgm.s.pfnR0BthSyncCR3              = pModeData->pfnR0BthSyncCR3;
    pVCpu->pgm.s.pfnR0BthSyncPage             = pModeData->pfnR0BthSyncPage;
    pVCpu->pgm.s.pfnR0BthPrefetchPage         = pModeData->pfnR0BthPrefetchPage;
    pVCpu->pgm.s.pfnR0BthVerifyAccessSyncPage = pModeData->pfnR0BthVerifyAccessSyncPage;
#ifdef VBOX_STRICT
    pVCpu->pgm.s.pfnR0BthAssertCR3            = pModeData->pfnR0BthAssertCR3;
#endif
    pVCpu->pgm.s.pfnR0BthMapCR3               = pModeData->pfnR0BthMapCR3;
    pVCpu->pgm.s.pfnR0BthUnmapCR3             = pModeData->pfnR0BthUnmapCR3;
}


/**
 * Calculates the shadow paging mode.
 *
 * @returns The shadow paging mode.
 * @param   pVM             VM handle.
 * @param   enmGuestMode    The guest mode.
 * @param   enmHostMode     The host mode.
 * @param   enmShadowMode   The current shadow mode.
 * @param   penmSwitcher    Where to store the switcher to use.
 *                          VMMSWITCHER_INVALID means no change.
 */
static PGMMODE pgmR3CalcShadowMode(PVM pVM, PGMMODE enmGuestMode, SUPPAGINGMODE enmHostMode, PGMMODE enmShadowMode, VMMSWITCHER *penmSwitcher)
{
    VMMSWITCHER enmSwitcher = VMMSWITCHER_INVALID;
    switch (enmGuestMode)
    {
        /*
         * When switching to real or protected mode we don't change
         * anything since it's likely that we'll switch back pretty soon.
         *
         * During pgmR3InitPaging we'll end up here with PGMMODE_INVALID
         * and is supposed to determine which shadow paging and switcher to
         * use during init.
         */
        case PGMMODE_REAL:
        case PGMMODE_PROTECTED:
            if (    enmShadowMode != PGMMODE_INVALID
                && !HWACCMIsEnabled(pVM) /* always switch in hwaccm mode! */)
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
                    if (RTEnvExist("VBOX_32BIT"))
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
#ifdef DEBUG_bird
                    if (RTEnvExist("VBOX_32BIT"))
                    {
                        enmShadowMode = PGMMODE_32_BIT;
                        enmSwitcher = VMMSWITCHER_AMD64_TO_32;
                    }
#endif
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
                    if (RTEnvExist("VBOX_32BIT"))
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
#ifdef DEBUG_bird
                    if (RTEnvExist("VBOX_32BIT"))
                    {
                        enmShadowMode = PGMMODE_32_BIT;
                        enmSwitcher = VMMSWITCHER_AMD64_TO_32;
                    }
#endif
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
                    enmShadowMode = PGMMODE_AMD64;
                    enmSwitcher = VMMSWITCHER_32_TO_AMD64;
                    break;

                case SUPPAGINGMODE_PAE:
                case SUPPAGINGMODE_PAE_NX:
                case SUPPAGINGMODE_PAE_GLOBAL:
                case SUPPAGINGMODE_PAE_GLOBAL_NX:
                    enmShadowMode = PGMMODE_AMD64;
                    enmSwitcher = VMMSWITCHER_PAE_TO_AMD64;
                    break;

                case SUPPAGINGMODE_AMD64:
                case SUPPAGINGMODE_AMD64_GLOBAL:
                case SUPPAGINGMODE_AMD64_NX:
                case SUPPAGINGMODE_AMD64_GLOBAL_NX:
                    enmShadowMode = PGMMODE_AMD64;
                    enmSwitcher = VMMSWITCHER_AMD64_TO_AMD64;
                    break;

                default: AssertMsgFailed(("enmHostMode=%d\n", enmHostMode)); break;
            }
            break;


        default:
            AssertReleaseMsgFailed(("enmGuestMode=%d\n", enmGuestMode));
            return PGMMODE_INVALID;
    }
    /* Override the shadow mode is nested paging is active. */
    if (HWACCMIsNestedPagingActive(pVM))
        enmShadowMode = HWACCMGetShwPagingMode(pVM);

    *penmSwitcher = enmSwitcher;
    return enmShadowMode;
}


/**
 * Performs the actual mode change.
 * This is called by PGMChangeMode and pgmR3InitPaging().
 *
 * @returns VBox status code. May suspend or power off the VM on error, but this
 *          will trigger using FFs and not status codes.
 *
 * @param   pVM             VM handle.
 * @param   pVCpu           The VMCPU to operate on.
 * @param   enmGuestMode    The new guest mode. This is assumed to be different from
 *                          the current mode.
 */
VMMR3DECL(int) PGMR3ChangeMode(PVM pVM, PVMCPU pVCpu, PGMMODE enmGuestMode)
{
    Log(("PGMR3ChangeMode: Guest mode: %s -> %s\n", PGMGetModeName(pVCpu->pgm.s.enmGuestMode), PGMGetModeName(enmGuestMode)));
    STAM_REL_COUNTER_INC(&pVCpu->pgm.s.cGuestModeChanges);

    /*
     * Calc the shadow mode and switcher.
     */
    VMMSWITCHER enmSwitcher;
    PGMMODE     enmShadowMode = pgmR3CalcShadowMode(pVM, enmGuestMode, pVM->pgm.s.enmHostMode, pVCpu->pgm.s.enmShadowMode, &enmSwitcher);
    if (enmSwitcher != VMMSWITCHER_INVALID)
    {
        /*
         * Select new switcher.
         */
        int rc = VMMR3SelectSwitcher(pVM, enmSwitcher);
        if (RT_FAILURE(rc))
        {
            AssertReleaseMsgFailed(("VMMR3SelectSwitcher(%d) -> %Rrc\n", enmSwitcher, rc));
            return rc;
        }
    }

    /*
     * Exit old mode(s).
     */
    /* shadow */
    if (enmShadowMode != pVCpu->pgm.s.enmShadowMode)
    {
        LogFlow(("PGMR3ChangeMode: Shadow mode: %s -> %s\n",  PGMGetModeName(pVCpu->pgm.s.enmShadowMode), PGMGetModeName(enmShadowMode)));
        if (PGM_SHW_PFN(Exit, pVCpu))
        {
            int rc = PGM_SHW_PFN(Exit, pVCpu)(pVCpu);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("Exit failed for shadow mode %d: %Rrc\n", pVCpu->pgm.s.enmShadowMode, rc));
                return rc;
            }
        }

    }
    else
        LogFlow(("PGMR3ChangeMode: Shadow mode remains: %s\n",  PGMGetModeName(pVCpu->pgm.s.enmShadowMode)));

    /* guest */
    if (PGM_GST_PFN(Exit, pVCpu))
    {
        int rc = PGM_GST_PFN(Exit, pVCpu)(pVCpu);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("Exit failed for guest mode %d: %Rrc\n", pVCpu->pgm.s.enmGuestMode, rc));
            return rc;
        }
    }

    /*
     * Load new paging mode data.
     */
    pgmR3ModeDataSwitch(pVM, pVCpu, enmShadowMode, enmGuestMode);

    /*
     * Enter new shadow mode (if changed).
     */
    if (enmShadowMode != pVCpu->pgm.s.enmShadowMode)
    {
        int rc;
        pVCpu->pgm.s.enmShadowMode = enmShadowMode;
        switch (enmShadowMode)
        {
            case PGMMODE_32_BIT:
                rc = PGM_SHW_NAME_32BIT(Enter)(pVCpu);
                break;
            case PGMMODE_PAE:
            case PGMMODE_PAE_NX:
                rc = PGM_SHW_NAME_PAE(Enter)(pVCpu);
                break;
            case PGMMODE_AMD64:
            case PGMMODE_AMD64_NX:
                rc = PGM_SHW_NAME_AMD64(Enter)(pVCpu);
                break;
            case PGMMODE_NESTED:
                rc = PGM_SHW_NAME_NESTED(Enter)(pVCpu);
                break;
            case PGMMODE_EPT:
                rc = PGM_SHW_NAME_EPT(Enter)(pVCpu);
                break;
            case PGMMODE_REAL:
            case PGMMODE_PROTECTED:
            default:
                AssertReleaseMsgFailed(("enmShadowMode=%d\n", enmShadowMode));
                return VERR_INTERNAL_ERROR;
        }
        if (RT_FAILURE(rc))
        {
            AssertReleaseMsgFailed(("Entering enmShadowMode=%d failed: %Rrc\n", enmShadowMode, rc));
            pVCpu->pgm.s.enmShadowMode = PGMMODE_INVALID;
            return rc;
        }
    }

    /*
     * Always flag the necessary updates
     */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);

    /*
     * Enter the new guest and shadow+guest modes.
     */
    int rc = -1;
    int rc2 = -1;
    RTGCPHYS GCPhysCR3 = NIL_RTGCPHYS;
    pVCpu->pgm.s.enmGuestMode = enmGuestMode;
    switch (enmGuestMode)
    {
        case PGMMODE_REAL:
            rc = PGM_GST_NAME_REAL(Enter)(pVCpu, NIL_RTGCPHYS);
            switch (pVCpu->pgm.s.enmShadowMode)
            {
                case PGMMODE_32_BIT:
                    rc2 = PGM_BTH_NAME_32BIT_REAL(Enter)(pVCpu, NIL_RTGCPHYS);
                    break;
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    rc2 = PGM_BTH_NAME_PAE_REAL(Enter)(pVCpu, NIL_RTGCPHYS);
                    break;
                case PGMMODE_NESTED:
                    rc2 = PGM_BTH_NAME_NESTED_REAL(Enter)(pVCpu, NIL_RTGCPHYS);
                    break;
                case PGMMODE_EPT:
                    rc2 = PGM_BTH_NAME_EPT_REAL(Enter)(pVCpu, NIL_RTGCPHYS);
                    break;
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    AssertMsgFailed(("Should use PAE shadow mode!\n"));
                default: AssertFailed(); break;
            }
            break;

        case PGMMODE_PROTECTED:
            rc = PGM_GST_NAME_PROT(Enter)(pVCpu, NIL_RTGCPHYS);
            switch (pVCpu->pgm.s.enmShadowMode)
            {
                case PGMMODE_32_BIT:
                    rc2 = PGM_BTH_NAME_32BIT_PROT(Enter)(pVCpu, NIL_RTGCPHYS);
                    break;
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    rc2 = PGM_BTH_NAME_PAE_PROT(Enter)(pVCpu, NIL_RTGCPHYS);
                    break;
                case PGMMODE_NESTED:
                    rc2 = PGM_BTH_NAME_NESTED_PROT(Enter)(pVCpu, NIL_RTGCPHYS);
                    break;
                case PGMMODE_EPT:
                    rc2 = PGM_BTH_NAME_EPT_PROT(Enter)(pVCpu, NIL_RTGCPHYS);
                    break;
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    AssertMsgFailed(("Should use PAE shadow mode!\n"));
                default: AssertFailed(); break;
            }
            break;

        case PGMMODE_32_BIT:
            GCPhysCR3 = CPUMGetGuestCR3(pVCpu) & X86_CR3_PAGE_MASK;
            rc = PGM_GST_NAME_32BIT(Enter)(pVCpu, GCPhysCR3);
            switch (pVCpu->pgm.s.enmShadowMode)
            {
                case PGMMODE_32_BIT:
                    rc2 = PGM_BTH_NAME_32BIT_32BIT(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    rc2 = PGM_BTH_NAME_PAE_32BIT(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_NESTED:
                    rc2 = PGM_BTH_NAME_NESTED_32BIT(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_EPT:
                    rc2 = PGM_BTH_NAME_EPT_32BIT(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    AssertMsgFailed(("Should use PAE shadow mode!\n"));
                default: AssertFailed(); break;
            }
            break;

        case PGMMODE_PAE_NX:
        case PGMMODE_PAE:
        {
            uint32_t u32Dummy, u32Features;

            CPUMGetGuestCpuId(pVCpu, 1, &u32Dummy, &u32Dummy, &u32Dummy, &u32Features);
            if (!(u32Features & X86_CPUID_FEATURE_EDX_PAE))
                return VMSetRuntimeError(pVM, VMSETRTERR_FLAGS_FATAL, "PAEmode",
                                         N_("The guest is trying to switch to the PAE mode which is currently disabled by default in VirtualBox. PAE support can be enabled using the VM settings (General/Advanced)"));

            GCPhysCR3 = CPUMGetGuestCR3(pVCpu) & X86_CR3_PAE_PAGE_MASK;
            rc = PGM_GST_NAME_PAE(Enter)(pVCpu, GCPhysCR3);
            switch (pVCpu->pgm.s.enmShadowMode)
            {
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    rc2 = PGM_BTH_NAME_PAE_PAE(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_NESTED:
                    rc2 = PGM_BTH_NAME_NESTED_PAE(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_EPT:
                    rc2 = PGM_BTH_NAME_EPT_PAE(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_32_BIT:
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    AssertMsgFailed(("Should use PAE shadow mode!\n"));
                default: AssertFailed(); break;
            }
            break;
        }

#ifdef VBOX_WITH_64_BITS_GUESTS
        case PGMMODE_AMD64_NX:
        case PGMMODE_AMD64:
            GCPhysCR3 = CPUMGetGuestCR3(pVCpu) & UINT64_C(0xfffffffffffff000); /** @todo define this mask! */
            rc = PGM_GST_NAME_AMD64(Enter)(pVCpu, GCPhysCR3);
            switch (pVCpu->pgm.s.enmShadowMode)
            {
                case PGMMODE_AMD64:
                case PGMMODE_AMD64_NX:
                    rc2 = PGM_BTH_NAME_AMD64_AMD64(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_NESTED:
                    rc2 = PGM_BTH_NAME_NESTED_AMD64(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_EPT:
                    rc2 = PGM_BTH_NAME_EPT_AMD64(Enter)(pVCpu, GCPhysCR3);
                    break;
                case PGMMODE_32_BIT:
                case PGMMODE_PAE:
                case PGMMODE_PAE_NX:
                    AssertMsgFailed(("Should use AMD64 shadow mode!\n"));
                default: AssertFailed(); break;
            }
            break;
#endif

        default:
            AssertReleaseMsgFailed(("enmGuestMode=%d\n", enmGuestMode));
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    /* status codes. */
    AssertRC(rc);
    AssertRC(rc2);
    if (RT_SUCCESS(rc))
    {
        rc = rc2;
        if (RT_SUCCESS(rc)) /* no informational status codes. */
            rc = VINF_SUCCESS;
    }

    /* Notify HWACCM as well. */
    HWACCMR3PagingModeChanged(pVM, pVCpu, pVCpu->pgm.s.enmShadowMode, pVCpu->pgm.s.enmGuestMode);
    return rc;
}


/**
 * Called by pgmPoolFlushAllInt prior to flushing the pool.
 *
 * @returns VBox status code, fully asserted.
 * @param   pVM     The VM handle.
 * @param   pVCpu   The VMCPU to operate on.
 */
int pgmR3ExitShadowModeBeforePoolFlush(PVM pVM, PVMCPU pVCpu)
{
    /** @todo Need to synchronize this across all VCPUs! */

    /* Unmap the old CR3 value before flushing everything. */
    int rc = PGM_BTH_PFN(UnmapCR3, pVCpu)(pVCpu);
    AssertRC(rc);

    /* Exit the current shadow paging mode as well; nested paging and EPT use a root CR3 which will get flushed here. */
    rc = PGM_SHW_PFN(Exit, pVCpu)(pVCpu);
    AssertRC(rc);
    Assert(pVCpu->pgm.s.pShwPageCR3R3 == NULL);
    return rc;
}


/**
 * Called by pgmPoolFlushAllInt after flushing the pool.
 *
 * @returns VBox status code, fully asserted.
 * @param   pVM     The VM handle.
 * @param   pVCpu   The VMCPU to operate on.
 */
int pgmR3ReEnterShadowModeAfterPoolFlush(PVM pVM, PVMCPU pVCpu)
{
    pVCpu->pgm.s.enmShadowMode = PGMMODE_INVALID;
    int rc = PGMR3ChangeMode(pVM, pVCpu, PGMGetGuestMode(pVCpu));
    Assert(VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
    AssertRCReturn(rc, rc);
    AssertRCSuccessReturn(rc, VERR_IPE_UNEXPECTED_INFO_STATUS);

    Assert(pVCpu->pgm.s.pShwPageCR3R3 != NULL);
    AssertMsg(   pVCpu->pgm.s.enmShadowMode >= PGMMODE_NESTED
              || CPUMGetHyperCR3(pVCpu) == PGMGetHyperCR3(pVCpu),
              ("%RHp != %RHp %s\n", (RTHCPHYS)CPUMGetHyperCR3(pVCpu), PGMGetHyperCR3(pVCpu), PGMGetModeName(pVCpu->pgm.s.enmShadowMode)));
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
    for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
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
        pHlp->pfnPrintf(pHlp, "%0*llx error! Page directory at HCPhys=%RHp was not found in the page pool!\n",
                        fLongMode ? 16 : 8, u64Address, HCPhys);
        return VERR_INVALID_PARAMETER;
    }
    const bool fBigPagesSupported = fLongMode || !!(cr4 & X86_CR4_PSE);

    int rc = VINF_SUCCESS;
    for (unsigned i = 0; i < RT_ELEMENTS(pPD->a); i++)
    {
        X86PDEPAE Pde = pPD->a[i];
        if (Pde.n.u1Present)
        {
            if (fBigPagesSupported && Pde.b.u1Size)
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
                                    pHlp->pfnPrintf(pHlp, "%0*llx error! Mapping error! PT %d has HCPhysPT=%RHp not %RHp is in the PD.\n",
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
                        pHlp->pfnPrintf(pHlp, "%0*llx error! Page table at HCPhys=%RHp was not found in the page pool!\n",
                                        fLongMode ? 16 : 8, u64AddressPT, HCPhysPT);
                    if (rc2 < rc && RT_SUCCESS(rc))
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
static int  pgmR3DumpHierarchyHCPaePDPT(PVM pVM, RTHCPHYS HCPhys, uint64_t u64Address, uint32_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
{
    PX86PDPT pPDPT = (PX86PDPT)MMPagePhys2Page(pVM, HCPhys);
    if (!pPDPT)
    {
        pHlp->pfnPrintf(pHlp, "%0*llx error! Page directory pointer table at HCPhys=%RHp was not found in the page pool!\n",
                        fLongMode ? 16 : 8, u64Address, HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;
    const unsigned c = fLongMode ? RT_ELEMENTS(pPDPT->a) : X86_PG_PAE_PDPE_ENTRIES;
    for (unsigned i = 0; i < c; i++)
    {
        X86PDPE Pdpe = pPDPT->a[i];
        if (Pdpe.n.u1Present)
        {
            if (fLongMode)
                pHlp->pfnPrintf(pHlp,         /*P R  S  A  D  G  WT CD AT NX 4M a p ?  */
                                "%016llx 1  |   P %c %c %c %c %c %s %s %s %s .. %c%c%c  %016llx\n",
                                u64Address + ((uint64_t)i << X86_PDPT_SHIFT),
                                Pdpe.lm.u1Write       ? 'W'  : 'R',
                                Pdpe.lm.u1User        ? 'U'  : 'S',
                                Pdpe.lm.u1Accessed    ? 'A'  : '-',
                                Pdpe.lm.u3Reserved & 1? '?'  : '.', /* ignored */
                                Pdpe.lm.u3Reserved & 4? '!'  : '.', /* mbz */
                                Pdpe.lm.u1WriteThru   ? "WT" : "--",
                                Pdpe.lm.u1CacheDisable? "CD" : "--",
                                Pdpe.lm.u3Reserved & 2? "!"  : "..",/* mbz */
                                Pdpe.lm.u1NoExecute   ? "NX" : "--",
                                Pdpe.u & RT_BIT(9)                   ? '1' : '0',
                                Pdpe.u & PGM_PLXFLAGS_PERMANENT   ? 'p' : '-',
                                Pdpe.u & RT_BIT(11)                  ? '1' : '0',
                                Pdpe.u & X86_PDPE_PG_MASK);
            else
                pHlp->pfnPrintf(pHlp,      /*P             G  WT CD AT NX 4M a p ?  */
                                "%08x 0 |    P             %c %s %s %s %s .. %c%c%c  %016llx\n",
                                i << X86_PDPT_SHIFT,
                                Pdpe.n.u4Reserved & 1? '!'  : '.', /* mbz */
                                Pdpe.n.u4Reserved & 4? '!'  : '.', /* mbz */
                                Pdpe.n.u1WriteThru   ? "WT" : "--",
                                Pdpe.n.u1CacheDisable? "CD" : "--",
                                Pdpe.n.u4Reserved & 2? "!"  : "..",/* mbz */
                                Pdpe.u & RT_BIT(9)                   ? '1' : '0',
                                Pdpe.u & PGM_PLXFLAGS_PERMANENT   ? 'p' : '-',
                                Pdpe.u & RT_BIT(11)                  ? '1' : '0',
                                Pdpe.u & X86_PDPE_PG_MASK);
            if (cMaxDepth >= 1)
            {
                int rc2 = pgmR3DumpHierarchyHCPaePD(pVM, Pdpe.u & X86_PDPE_PG_MASK, u64Address + ((uint64_t)i << X86_PDPT_SHIFT),
                                                    cr4, fLongMode, cMaxDepth - 1, pHlp);
                if (rc2 < rc && RT_SUCCESS(rc))
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
        pHlp->pfnPrintf(pHlp, "Page map level 4 at HCPhys=%RHp was not found in the page pool!\n", HCPhys);
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;
    for (unsigned i = 0; i < RT_ELEMENTS(pPML4->a); i++)
    {
        X86PML4E Pml4e = pPML4->a[i];
        if (Pml4e.n.u1Present)
        {
            uint64_t u64Address = ((uint64_t)i << X86_PML4_SHIFT) | (((uint64_t)i >> (X86_PML4_SHIFT - X86_PDPT_SHIFT - 1)) * 0xffff000000000000ULL);
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
                int rc2 = pgmR3DumpHierarchyHCPaePDPT(pVM, Pml4e.u & X86_PML4E_PG_MASK, u64Address, cr4, true, cMaxDepth - 1, pHlp);
                if (rc2 < rc && RT_SUCCESS(rc))
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
    for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
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
    for (unsigned i = 0; i < RT_ELEMENTS(pPD->a); i++)
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
                                    pHlp->pfnPrintf(pHlp, "%08x error! Mapping error! PT %d has HCPhysPT=%RHp not %RHp is in the PD.\n",
                                                    u32Address, iPDE, pMap->aPTs[iPDE].HCPhysPT, HCPhys);
                                pPT = pMap->aPTs[iPDE].pPTR3;
                            }
                    }
                    int rc2 = VERR_INVALID_PARAMETER;
                    if (pPT)
                        rc2 = pgmR3DumpHierarchyHC32BitPT(pVM, pPT, u32Address, pHlp);
                    else
                        pHlp->pfnPrintf(pHlp, "%08x error! Page table at %#x was not found in the page pool!\n", u32Address, HCPhys);
                    if (rc2 < rc && RT_SUCCESS(rc))
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
    for (unsigned i = 0; i < RT_ELEMENTS(pPT->a); i++)
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

                /** @todo SMP support!! */
                PGMShwGetPage(&pVM->aCpus[0], (RTGCPTR)(u32Address + (i << X86_PT_SHIFT)), &fPageShw, &pPhysHC);
                Log(("Found %RGp at %RGv -> flags=%llx\n", PhysSearch, (RTGCPTR)(u32Address + (i << X86_PT_SHIFT)), fPageShw));
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
VMMR3DECL(int) PGMR3DumpHierarchyGC(PVM pVM, uint64_t cr3, uint64_t cr4, RTGCPHYS PhysSearch)
{
    bool fLongMode = false;
    const unsigned cch = fLongMode ? 16 : 8; NOREF(cch);
    PX86PD pPD = 0;

    int rc = PGM_GCPHYS_2_PTR(pVM, cr3 & X86_CR3_PAGE_MASK, &pPD);
    if (RT_FAILURE(rc) || !pPD)
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

    for (unsigned i = 0; i < RT_ELEMENTS(pPD->a); i++)
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
                     pgmGstGet4MBPhysPage(&pVM->pgm.s, Pde)));
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
                    if (rc2 < rc && RT_SUCCESS(rc))
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
VMMR3DECL(int) PGMR3DumpHierarchyHC(PVM pVM, uint64_t cr3, uint64_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp)
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
        return pgmR3DumpHierarchyHCPaePDPT(pVM, cr3 & X86_CR3_PAE_PAGE_MASK, 0, cr4, false, cMaxDepth, pHlp);
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
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires a VM to be selected.\n");
    if (!pVM->pgm.s.pRamRangesRC)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Sorry, no Ram is registered.\n");

    /*
     * Dump the ranges.
     */
    int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "From     - To (incl) pvHC\n");
    PPGMRAMRANGE pRam;
    for (pRam = pVM->pgm.s.pRamRangesR3; pRam; pRam = pRam->pNextR3)
    {
        rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL,
            "%RGp - %RGp  %p\n",
            pRam->GCPhys, pRam->GCPhysLast, pRam->pvR3);
        if (RT_FAILURE(rc))
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
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires a VM to be selected.\n");
    if (!pVM->pgm.s.pMappingsR3)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Sorry, no mappings are registered.\n");

    /*
     * Print message about the fixedness of the mappings.
     */
    int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, pVM->pgm.s.fMappingsFixed ? "The mappings are FIXED.\n" : "The mappings are FLOATING.\n");
    if (RT_FAILURE(rc))
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
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * The '.pgmerror' and '.pgmerroroff' commands.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int)  pgmR3CmdError(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires a VM to be selected.\n");
    AssertReturn(cArgs == 0 || (cArgs == 1 && paArgs[0].enmType == DBGCVAR_TYPE_STRING),
                 pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Hit bug in the parser.\n"));

    if (!cArgs)
    {
        /*
         * Print the list of error injection locations with status.
         */
        pCmdHlp->pfnPrintf(pCmdHlp, NULL, "PGM error inject locations:\n");
        pCmdHlp->pfnPrintf(pCmdHlp, NULL, "  handy - %RTbool\n", pVM->pgm.s.fErrInjHandyPages);
    }
    else
    {

        /*
         * String switch on where to inject the error.
         */
        bool const  fNewState = !strcmp(pCmd->pszCmd, "pgmerror");
        const char *pszWhere = paArgs[0].u.pszString;
        if (!strcmp(pszWhere, "handy"))
            ASMAtomicWriteBool(&pVM->pgm.s.fErrInjHandyPages, fNewState);
        else
            return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: Invalid 'where' value: %s.\n", pszWhere);
        pCmdHlp->pfnPrintf(pCmdHlp, NULL, "done\n");
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
    /** @todo SMP support */
    PVMCPU pVCpu = &pVM->aCpus[0];

    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires a VM to be selected.\n");

    /*
     * Force page directory sync.
     */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);

    int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Forcing page directory sync.\n");
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


#ifdef VBOX_STRICT
/**
 * The '.pgmassertcr3' command.
 *
 * @returns VBox status.
 * @param   pCmd        Pointer to the command descriptor (as registered).
 * @param   pCmdHlp     Pointer to command helper functions.
 * @param   pVM         Pointer to the current VM (if any).
 * @param   paArgs      Pointer to (readonly) array of arguments.
 * @param   cArgs       Number of arguments in the array.
 */
static DECLCALLBACK(int) pgmR3CmdAssertCR3(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PVM pVM, PCDBGCVAR paArgs, unsigned cArgs, PDBGCVAR pResult)
{
    /** @todo SMP support!! */
    PVMCPU pVCpu = &pVM->aCpus[0];

    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires a VM to be selected.\n");

    int rc = pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Checking shadow CR3 page tables for consistency.\n");
    if (RT_FAILURE(rc))
        return rc;

    PGMAssertCR3(pVM, pVCpu, CPUMGetGuestCR3(pVCpu), CPUMGetGuestCR4(pVCpu));

    return VINF_SUCCESS;
}
#endif /* VBOX_STRICT */


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
    /** @todo SMP support!! */
    PVMCPU pVCpu = &pVM->aCpus[0];

    /*
     * Validate input.
     */
    if (!pVM)
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "error: The command requires a VM to be selected.\n");

    /*
     * Force page directory sync.
     */
    if (pVCpu->pgm.s.fSyncFlags & PGM_SYNC_ALWAYS)
    {
        ASMAtomicAndU32(&pVCpu->pgm.s.fSyncFlags, ~PGM_SYNC_ALWAYS);
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Disabled permanent forced page directory syncing.\n");
    }
    else
    {
        ASMAtomicOrU32(&pVCpu->pgm.s.fSyncFlags, PGM_SYNC_ALWAYS);
        VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
        return pCmdHlp->pfnPrintf(pCmdHlp, NULL, "Enabled permanent forced page directory syncing.\n");
    }
}

#endif /* VBOX_WITH_DEBUGGER */

/**
 * pvUser argument of the pgmR3CheckIntegrity*Node callbacks.
 */
typedef struct PGMCHECKINTARGS
{
    bool                    fLeftToRight;    /**< true: left-to-right; false: right-to-left. */
    PPGMPHYSHANDLER         pPrevPhys;
    PPGMVIRTHANDLER         pPrevVirt;
    PPGMPHYS2VIRTHANDLER    pPrevPhys2Virt;
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
    AssertReleaseMsg(pCur->Core.Key <= pCur->Core.KeyLast,("pCur=%p %RGp-%RGp %s\n", pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
    AssertReleaseMsg(   !pArgs->pPrevPhys
                     || (pArgs->fLeftToRight ? pArgs->pPrevPhys->Core.KeyLast < pCur->Core.Key : pArgs->pPrevPhys->Core.KeyLast > pCur->Core.Key),
                     ("pPrevPhys=%p %RGp-%RGp %s\n"
                      "     pCur=%p %RGp-%RGp %s\n",
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
    AssertReleaseMsg(pCur->Core.Key <= pCur->Core.KeyLast,("pCur=%p %RGv-%RGv %s\n", pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
    AssertReleaseMsg(   !pArgs->pPrevVirt
                     || (pArgs->fLeftToRight ? pArgs->pPrevVirt->Core.KeyLast < pCur->Core.Key : pArgs->pPrevVirt->Core.KeyLast > pCur->Core.Key),
                     ("pPrevVirt=%p %RGv-%RGv %s\n"
                      "     pCur=%p %RGv-%RGv %s\n",
                      pArgs->pPrevVirt, pArgs->pPrevVirt->Core.Key, pArgs->pPrevVirt->Core.KeyLast, pArgs->pPrevVirt->pszDesc,
                      pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->pszDesc));
    for (unsigned iPage = 0; iPage < pCur->cPages; iPage++)
    {
        AssertReleaseMsg(pCur->aPhysToVirt[iPage].offVirtHandler == -RT_OFFSETOF(PGMVIRTHANDLER, aPhysToVirt[iPage]),
                         ("pCur=%p %RGv-%RGv %s\n"
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
    AssertReleaseMsg(pCur->Core.Key <= pCur->Core.KeyLast,("pCur=%p %RGp-%RGp\n", pCur, pCur->Core.Key, pCur->Core.KeyLast));
    AssertReleaseMsg(   !pArgs->pPrevPhys2Virt
                     || (pArgs->fLeftToRight ? pArgs->pPrevPhys2Virt->Core.KeyLast < pCur->Core.Key : pArgs->pPrevPhys2Virt->Core.KeyLast > pCur->Core.Key),
                     ("pPrevPhys2Virt=%p %RGp-%RGp\n"
                      "          pCur=%p %RGp-%RGp\n",
                      pArgs->pPrevPhys2Virt, pArgs->pPrevPhys2Virt->Core.Key, pArgs->pPrevPhys2Virt->Core.KeyLast,
                      pCur, pCur->Core.Key, pCur->Core.KeyLast));
    AssertReleaseMsg(   !pArgs->pPrevPhys2Virt
                     || (pArgs->fLeftToRight ? pArgs->pPrevPhys2Virt->Core.KeyLast < pCur->Core.Key : pArgs->pPrevPhys2Virt->Core.KeyLast > pCur->Core.Key),
                     ("pPrevPhys2Virt=%p %RGp-%RGp\n"
                      "          pCur=%p %RGp-%RGp\n",
                      pArgs->pPrevPhys2Virt, pArgs->pPrevPhys2Virt->Core.Key, pArgs->pPrevPhys2Virt->Core.KeyLast,
                      pCur, pCur->Core.Key, pCur->Core.KeyLast));
    AssertReleaseMsg((pCur->offNextAlias & (PGMPHYS2VIRTHANDLER_IN_TREE | PGMPHYS2VIRTHANDLER_IS_HEAD)) == (PGMPHYS2VIRTHANDLER_IN_TREE | PGMPHYS2VIRTHANDLER_IS_HEAD),
                     ("pCur=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                      pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias));
    if (pCur->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK)
    {
        PPGMPHYS2VIRTHANDLER pCur2 = pCur;
        for (;;)
        {
            pCur2 = (PPGMPHYS2VIRTHANDLER)((intptr_t)pCur + (pCur->offNextAlias & PGMPHYS2VIRTHANDLER_OFF_MASK));
            AssertReleaseMsg(pCur2 != pCur,
                             (" pCur=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                              pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias));
            AssertReleaseMsg((pCur2->offNextAlias & (PGMPHYS2VIRTHANDLER_IN_TREE | PGMPHYS2VIRTHANDLER_IS_HEAD)) == PGMPHYS2VIRTHANDLER_IN_TREE,
                             (" pCur=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n"
                              "pCur2=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                              pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias,
                              pCur2, pCur2->Core.Key, pCur2->Core.KeyLast, pCur2->offVirtHandler, pCur2->offNextAlias));
            AssertReleaseMsg((pCur2->Core.Key ^ pCur->Core.Key) < PAGE_SIZE,
                             (" pCur=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n"
                              "pCur2=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                              pCur, pCur->Core.Key, pCur->Core.KeyLast, pCur->offVirtHandler, pCur->offNextAlias,
                              pCur2, pCur2->Core.Key, pCur2->Core.KeyLast, pCur2->offVirtHandler, pCur2->offNextAlias));
            AssertReleaseMsg((pCur2->Core.KeyLast ^ pCur->Core.KeyLast) < PAGE_SIZE,
                             (" pCur=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n"
                              "pCur2=%p:{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
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
VMMR3DECL(int) PGMR3CheckIntegrity(PVM pVM)
{
    AssertReleaseReturn(pVM->pgm.s.offVM, VERR_INTERNAL_ERROR);

    /*
     * Check the trees.
     */
    int cErrors = 0;
    const static PGMCHECKINTARGS s_LeftToRight = { true, NULL, NULL, NULL, pVM };
    const static PGMCHECKINTARGS s_RightToLeft = { false, NULL, NULL, NULL, pVM };
    PGMCHECKINTARGS Args = s_LeftToRight;
    cErrors += RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesR3->PhysHandlers,       true,  pgmR3CheckIntegrityPhysHandlerNode, &Args);
    Args = s_RightToLeft;
    cErrors += RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesR3->PhysHandlers,       false, pgmR3CheckIntegrityPhysHandlerNode, &Args);
    Args = s_LeftToRight;
    cErrors += RTAvlroGCPtrDoWithAll( &pVM->pgm.s.pTreesR3->VirtHandlers,       true,  pgmR3CheckIntegrityVirtHandlerNode, &Args);
    Args = s_RightToLeft;
    cErrors += RTAvlroGCPtrDoWithAll( &pVM->pgm.s.pTreesR3->VirtHandlers,       false, pgmR3CheckIntegrityVirtHandlerNode, &Args);
    Args = s_LeftToRight;
    cErrors += RTAvlroGCPtrDoWithAll( &pVM->pgm.s.pTreesR3->HyperVirtHandlers,  true,  pgmR3CheckIntegrityVirtHandlerNode, &Args);
    Args = s_RightToLeft;
    cErrors += RTAvlroGCPtrDoWithAll( &pVM->pgm.s.pTreesR3->HyperVirtHandlers,  false, pgmR3CheckIntegrityVirtHandlerNode, &Args);
    Args = s_LeftToRight;
    cErrors += RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesR3->PhysToVirtHandlers, true,  pgmR3CheckIntegrityPhysToVirtHandlerNode, &Args);
    Args = s_RightToLeft;
    cErrors += RTAvlroGCPhysDoWithAll(&pVM->pgm.s.pTreesR3->PhysToVirtHandlers, false, pgmR3CheckIntegrityPhysToVirtHandlerNode, &Args);

    return !cErrors ? VINF_SUCCESS : VERR_INTERNAL_ERROR;
}


