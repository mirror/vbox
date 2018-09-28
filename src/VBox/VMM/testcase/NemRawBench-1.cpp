
/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
# include <WinHvPlatform.h>
typedef unsigned long long uint64_t;

#elif defined(RT_OS_LINUX)
# include <linux/kvm.h>
# include <errno.h>
# include <stdint.h>
# include <sys/fcntl.h>
# include <sys/ioctl.h>
# include <sys/mman.h>
# include <unistd.h>
# include <time.h>

#else
# error "port me"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The base mapping address of the g_pbMem. */
#define MY_MEM_BASE             0x1000
/** No-op MMIO access address.   */
#define MY_NOP_MMIO             0x0808
/** The RIP which the testcode starts. */
#define MY_TEST_RIP             0x2000

/** The test termination port number. */
#define MY_TERM_PORT            0x01
/** The no-op test port number. */
#define MY_NOP_PORT             0x7f



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Chunk of memory mapped at address 0x1000 (MY_MEM_BASE). */
static unsigned char           *g_pbMem;
/** Amount of RAM at address 0x1000 (MY_MEM_BASE). */
static size_t                   g_cbMem;
#ifdef RT_OS_WINDOWS
static WHV_PARTITION_HANDLE     g_hPartition = NULL;

/** @name APIs imported from WinHvPlatform.dll
 * @{ */
static decltype(WHvCreatePartition)                *g_pfnWHvCreatePartition;
static decltype(WHvSetupPartition)                 *g_pfnWHvSetupPartition;
static decltype(WHvGetPartitionProperty)           *g_pfnWHvGetPartitionProperty;
static decltype(WHvSetPartitionProperty)           *g_pfnWHvSetPartitionProperty;
static decltype(WHvMapGpaRange)                    *g_pfnWHvMapGpaRange;
static decltype(WHvCreateVirtualProcessor)         *g_pfnWHvCreateVirtualProcessor;
static decltype(WHvRunVirtualProcessor)            *g_pfnWHvRunVirtualProcessor;
static decltype(WHvGetVirtualProcessorRegisters)   *g_pfnWHvGetVirtualProcessorRegisters;
static decltype(WHvSetVirtualProcessorRegisters)   *g_pfnWHvSetVirtualProcessorRegisters;
/** @} */
static uint64_t (WINAPI                            *g_pfnRtlGetSystemTimePrecise)(void);

#elif defined(RT_OS_LINUX)
/** The VM handle.   */
static int                      g_fdVm;
/** The VCPU handle.   */
static int                      g_fdVCpu;
/** The kvm_run structure for the VCpu. */
static struct kvm_run          *g_pVCpuRun;
/** The size of the g_pVCpuRun mapping. */
static ssize_t                  g_cbVCpuRun;
#endif


static int error(const char *pszFormat, ...)
{
    fprintf(stderr, "error: ");
    va_list va;
    va_start(va, pszFormat);
    vfprintf(stderr, pszFormat, va);
    va_end(va);
    return 1;
}


static uint64_t getNanoTS(void)
{
#ifdef RT_OS_WINDOWS
    return g_pfnRtlGetSystemTimePrecise() * 100;
#elif defined(RT_OS_LINUX)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + ts.tv_nsec;
#else
# error "port me"
#endif
}


#ifdef RT_OS_WINDOWS

/*
 * Windows - Hyper-V Platform API.
 */

static int createVM(void)
{
    /*
     * Resolve APIs.
     */
    HMODULE hmod = LoadLibraryW(L"WinHvPlatform.dll");
    if (hmod == NULL)
        return error("Error loading WinHvPlatform.dll: %u\n", GetLastError());
    static struct { const char *pszFunction; FARPROC *ppfn; } const s_aImports[] =
    {
# define IMPORT_ENTRY(a_Name) { #a_Name, (FARPROC *)&g_pfn##a_Name }
        IMPORT_ENTRY(WHvCreatePartition),
        IMPORT_ENTRY(WHvSetupPartition),
        IMPORT_ENTRY(WHvGetPartitionProperty),
        IMPORT_ENTRY(WHvSetPartitionProperty),
        IMPORT_ENTRY(WHvMapGpaRange),
        IMPORT_ENTRY(WHvCreateVirtualProcessor),
        IMPORT_ENTRY(WHvRunVirtualProcessor),
        IMPORT_ENTRY(WHvGetVirtualProcessorRegisters),
        IMPORT_ENTRY(WHvSetVirtualProcessorRegisters),
# undef IMPORT_ENTRY
    };
    FARPROC pfn;
    for (size_t i = 0; i < sizeof(s_aImports) / sizeof(s_aImports[0]); i++)
    {
        *s_aImports[i].ppfn = pfn = GetProcAddress(hmod, s_aImports[i].pszFunction);
        if (!pfn)
            return error("Error resolving WinHvPlatform.dll!%s: %u\n", s_aImports[i].pszFunction, GetLastError());
    }
# ifndef IN_SLICKEDIT
#  define WHvCreatePartition                         g_pfnWHvCreatePartition
#  define WHvSetupPartition                          g_pfnWHvSetupPartition
#  define WHvGetPartitionProperty                    g_pfnWHvGetPartitionProperty
#  define WHvSetPartitionProperty                    g_pfnWHvSetPartitionProperty
#  define WHvMapGpaRange                             g_pfnWHvMapGpaRange
#  define WHvCreateVirtualProcessor                  g_pfnWHvCreateVirtualProcessor
#  define WHvRunVirtualProcessor                     g_pfnWHvRunVirtualProcessor
#  define WHvGetVirtualProcessorRegisters            g_pfnWHvGetVirtualProcessorRegisters
#  define WHvSetVirtualProcessorRegisters            g_pfnWHvSetVirtualProcessorRegisters
# endif
    /* Need a precise time function. */
    *(FARPROC *)&g_pfnRtlGetSystemTimePrecise = pfn = GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetSystemTimePrecise");
    if (pfn == NULL)
        return error("Error resolving ntdll.dll!RtlGetSystemTimePrecise: %u\n", GetLastError());

    /*
     * Create the partition with 1 CPU and the specfied amount of memory.
     */
    WHV_PARTITION_HANDLE hPartition;
    HRESULT hrc = WHvCreatePartition(&hPartition);
    if (!SUCCEEDED(hrc))
        return error("WHvCreatePartition failed: %#x\n", hrc);
    g_hPartition = hPartition;

    WHV_PARTITION_PROPERTY Property;
    memset(&Property, 0, sizeof(Property));
    Property.ProcessorCount = 1;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeProcessorCount, &Property, sizeof(Property));
    if (!SUCCEEDED(hrc))
        return error("WHvSetPartitionProperty/WHvPartitionPropertyCodeProcessorCount failed: %#x\n", hrc);

    memset(&Property, 0, sizeof(Property));
    Property.ExtendedVmExits.X64CpuidExit  = 1;
    Property.ExtendedVmExits.X64MsrExit    = 1;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeExtendedVmExits, &Property, sizeof(Property));
    if (!SUCCEEDED(hrc))
        return error("WHvSetPartitionProperty/WHvPartitionPropertyCodeExtendedVmExits failed: %#x\n", hrc);

    hrc = WHvSetupPartition(hPartition);
    if (!SUCCEEDED(hrc))
        return error("WHvSetupPartition failed: %#x\n", hrc);

    hrc = WHvCreateVirtualProcessor(hPartition, 0 /*idVCpu*/, 0 /*fFlags*/);
    if (!SUCCEEDED(hrc))
        return error("WHvCreateVirtualProcessor failed: %#x\n", hrc);

    g_pbMem = (unsigned char *)VirtualAlloc(NULL, g_cbMem, MEM_COMMIT, PAGE_READWRITE);
    if (!g_pbMem)
        return error("VirtualAlloc failed: %u\n", GetLastError());
    memset(g_pbMem, 0xcc, g_cbMem);

    hrc = WHvMapGpaRange(hPartition, g_pbMem, MY_MEM_BASE /*GCPhys*/, g_cbMem,
                         WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute);
    if (!SUCCEEDED(hrc))
        return error("WHvMapGpaRange failed: %#x\n", hrc);

    WHV_RUN_VP_EXIT_CONTEXT ExitInfo;
    memset(&ExitInfo, 0, sizeof(ExitInfo));
    WHvRunVirtualProcessor(g_hPartition, 0 /*idCpu*/, &ExitInfo, sizeof(ExitInfo));

    return 0;
}


static int runtimeError(const char *pszFormat, ...)
{
    fprintf(stderr, "runtime error: ");
    va_list va;
    va_start(va, pszFormat);
    vfprintf(stderr, pszFormat, va);
    va_end(va);

    static struct { const char *pszName; WHV_REGISTER_NAME enmName; unsigned uType; } const s_aRegs[] =
    {
        { "rip",    WHvX64RegisterRip, 64 },
        { "cs",     WHvX64RegisterCs, 1 },
        { "rflags", WHvX64RegisterRflags, 32 },
        { "rax",    WHvX64RegisterRax, 64 },
        { "rcx",    WHvX64RegisterRcx, 64 },
        { "rdx",    WHvX64RegisterRdx, 64 },
        { "rbx",    WHvX64RegisterRbx, 64 },
        { "rsp",    WHvX64RegisterRsp, 64 },
        { "ss",     WHvX64RegisterSs, 1 },
        { "rbp",    WHvX64RegisterRbp, 64 },
        { "rsi",    WHvX64RegisterRsi, 64 },
        { "rdi",    WHvX64RegisterRdi, 64 },
        { "ds",     WHvX64RegisterDs, 1 },
        { "es",     WHvX64RegisterEs, 1 },
        { "fs",     WHvX64RegisterFs, 1 },
        { "gs",     WHvX64RegisterGs, 1 },
        { "cr0",    WHvX64RegisterCr0, 64 },
        { "cr2",    WHvX64RegisterCr2, 64 },
        { "cr3",    WHvX64RegisterCr3, 64 },
        { "cr4",    WHvX64RegisterCr4, 64 },
   };
    for (unsigned i = 0; i < sizeof(s_aRegs) / sizeof(s_aRegs[0]); i++)
    {
        WHV_REGISTER_VALUE Value;
        WHV_REGISTER_NAME  enmName = s_aRegs[i].enmName;
        HRESULT hrc = WHvGetVirtualProcessorRegisters(g_hPartition, 0 /*idCpu*/, &enmName, 1, &Value);
        if (SUCCEEDED(hrc))
        {
            if (s_aRegs[i].uType == 32)
                fprintf(stderr, "%8s=%08x\n", s_aRegs[i].pszName, Value.Reg32);
            else if (s_aRegs[i].uType == 64)
                fprintf(stderr, "%8s=%08x'%08x\n", s_aRegs[i].pszName, (unsigned)(Value.Reg64 >> 32), Value.Reg32);
            else if (s_aRegs[i].uType == 1)
                fprintf(stderr, "%8s=%04x  base=%08x'%08x  limit=%08x attr=%04x\n", s_aRegs[i].pszName,
                        Value.Segment.Selector, (unsigned)(Value.Segment.Base >> 32), (unsigned)Value.Segment.Base,
                        Value.Segment.Limit, Value.Segment.Attributes);
        }
        else
            fprintf(stderr, "%8s=<WHvGetVirtualProcessorRegisters failed %#x>\n", s_aRegs[i].pszName, hrc);
    }

    return 1;
}


static int runRealModeTest(unsigned cInstructions, const char *pszInstruction,
                           unsigned uEax, unsigned uEcx, unsigned uEdx, unsigned uEbx,
                           unsigned uEsp, unsigned uEbp, unsigned uEsi, unsigned uEdi)
{
    /*
     * Initialize the real mode context.
     */
# define ADD_REG64(a_enmName, a_uValue) do { \
            aenmNames[iReg]      = (a_enmName); \
            aValues[iReg].Reg128.High64 = 0; \
            aValues[iReg].Reg64  = (a_uValue); \
            iReg++; \
        } while (0)
# define ADD_SEG(a_enmName, a_Base, a_Limit, a_Sel, a_fCode) \
        do { \
            aenmNames[iReg]                  = a_enmName; \
            aValues[iReg].Segment.Base       = (a_Base); \
            aValues[iReg].Segment.Limit      = (a_Limit); \
            aValues[iReg].Segment.Selector   = (a_Sel); \
            aValues[iReg].Segment.Attributes = a_fCode ? 0x9b : 0x93; \
            iReg++; \
        } while (0)
    WHV_REGISTER_NAME  aenmNames[80];
    WHV_REGISTER_VALUE aValues[80];
    unsigned iReg = 0;
    ADD_REG64(WHvX64RegisterRax, uEax);
    ADD_REG64(WHvX64RegisterRcx, uEcx);
    ADD_REG64(WHvX64RegisterRdx, uEdx);
    ADD_REG64(WHvX64RegisterRbx, uEbx);
    ADD_REG64(WHvX64RegisterRsp, uEsp);
    ADD_REG64(WHvX64RegisterRbp, uEbp);
    ADD_REG64(WHvX64RegisterRsi, uEsi);
    ADD_REG64(WHvX64RegisterRdi, uEdi);
    ADD_REG64(WHvX64RegisterRip, MY_TEST_RIP);
    ADD_REG64(WHvX64RegisterRflags, 1);
    ADD_SEG(WHvX64RegisterEs, 0x00000, 0xffff, 0x0000, 0);
    ADD_SEG(WHvX64RegisterCs, 0x00000, 0xffff, 0x0000, 1);
    ADD_SEG(WHvX64RegisterSs, 0x00000, 0xffff, 0x0000, 0);
    ADD_SEG(WHvX64RegisterDs, 0x00000, 0xffff, 0x0000, 0);
    ADD_SEG(WHvX64RegisterFs, 0x00000, 0xffff, 0x0000, 0);
    ADD_SEG(WHvX64RegisterGs, 0x00000, 0xffff, 0x0000, 0);
    ADD_REG64(WHvX64RegisterCr0, 0x10010 /*WP+ET*/);
    ADD_REG64(WHvX64RegisterCr2, 0);
    ADD_REG64(WHvX64RegisterCr3, 0);
    ADD_REG64(WHvX64RegisterCr4, 0);
    HRESULT hrc = WHvSetVirtualProcessorRegisters(g_hPartition, 0 /*idCpu*/, aenmNames, iReg, aValues);
    if (!SUCCEEDED(hrc))
        return error("WHvSetVirtualProcessorRegisters failed (for %s): %#x\n", pszInstruction, hrc);
# undef ADD_REG64
# undef ADD_SEG

    /*
     * Run the test.
     */
    uint64_t const nsStart = getNanoTS();
    for (;;)
    {
        WHV_RUN_VP_EXIT_CONTEXT ExitInfo;
        memset(&ExitInfo, 0, sizeof(ExitInfo));
        hrc = WHvRunVirtualProcessor(g_hPartition, 0 /*idCpu*/, &ExitInfo, sizeof(ExitInfo));
        if (SUCCEEDED(hrc))
        {
            if (ExitInfo.ExitReason == WHvRunVpExitReasonX64IoPortAccess)
            {
                if (ExitInfo.IoPortAccess.PortNumber == MY_NOP_PORT)
                { /* likely: nop instruction */ }
                else if (ExitInfo.IoPortAccess.PortNumber == MY_TERM_PORT)
                    break;
                else
                    return runtimeError("Unexpected I/O port access (for %s): %#x\n", pszInstruction, ExitInfo.IoPortAccess.PortNumber);

                /* Advance. */
                if (ExitInfo.VpContext.InstructionLength)
                {
                    aenmNames[0] = WHvX64RegisterRip;
                    aValues[0].Reg64 = ExitInfo.VpContext.Rip + ExitInfo.VpContext.InstructionLength;
                    hrc = WHvSetVirtualProcessorRegisters(g_hPartition, 0 /*idCpu*/, aenmNames, 1, aValues);
                    if (SUCCEEDED(hrc))
                    { /* likely */ }
                    else
                        return runtimeError("Error advancing RIP (for %s): %#x\n", pszInstruction, hrc);
                }
                else
                    return runtimeError("VpContext.InstructionLength is zero (for %s)\n", pszInstruction);
            }
            else if (ExitInfo.ExitReason == WHvRunVpExitReasonX64Cpuid)
            {
                /* Advance RIP and set default results. */
                if (ExitInfo.VpContext.InstructionLength)
                {
                    aenmNames[0] = WHvX64RegisterRip;
                    aValues[0].Reg64 = ExitInfo.VpContext.Rip + ExitInfo.VpContext.InstructionLength;
                    aenmNames[1] = WHvX64RegisterRax;
                    aValues[1].Reg64 = ExitInfo.CpuidAccess.DefaultResultRax;
                    aenmNames[2] = WHvX64RegisterRcx;
                    aValues[2].Reg64 = ExitInfo.CpuidAccess.DefaultResultRcx;
                    aenmNames[3] = WHvX64RegisterRdx;
                    aValues[3].Reg64 = ExitInfo.CpuidAccess.DefaultResultRdx;
                    aenmNames[4] = WHvX64RegisterRbx;
                    aValues[4].Reg64 = ExitInfo.CpuidAccess.DefaultResultRbx;
                    hrc = WHvSetVirtualProcessorRegisters(g_hPartition, 0 /*idCpu*/, aenmNames, 5, aValues);
                    if (SUCCEEDED(hrc))
                    { /* likely */ }
                    else
                        return runtimeError("Error advancing RIP (for %s): %#x\n", pszInstruction, hrc);
                }
                else
                    return runtimeError("VpContext.InstructionLength is zero (for %s)\n", pszInstruction);
            }
            else if (ExitInfo.ExitReason == WHvRunVpExitReasonMemoryAccess)
            {
                if (ExitInfo.MemoryAccess.Gpa == MY_NOP_MMIO)
                { /* likely: nop address */ }
                else
                    return runtimeError("Unexpected memory access (for %s): %#x\n", pszInstruction, ExitInfo.MemoryAccess.Gpa);

                /* Advance and set return register (assuming RAX and two byte instruction). */
                aenmNames[0] = WHvX64RegisterRip;
                if (ExitInfo.VpContext.InstructionLength)
                    aValues[0].Reg64 = ExitInfo.VpContext.Rip + ExitInfo.VpContext.InstructionLength;
                else
                    aValues[0].Reg64 = ExitInfo.VpContext.Rip + 2;
                aenmNames[1] = WHvX64RegisterRax;
                aValues[1].Reg64 = 42;
                hrc = WHvSetVirtualProcessorRegisters(g_hPartition, 0 /*idCpu*/, aenmNames, 2, aValues);
                if (SUCCEEDED(hrc))
                { /* likely */ }
                else
                    return runtimeError("Error advancing RIP (for %s): %#x\n", pszInstruction, hrc);
            }
            else
                return runtimeError("Unexpected exit (for %s): %#x\n", pszInstruction, ExitInfo.ExitReason);
        }
        else
            return runtimeError("WHvRunVirtualProcessor failed (for %s): %#x\n", pszInstruction, hrc);
    }
    uint64_t const nsElapsed = getNanoTS() - nsStart;

    /* Report the results. */
    uint64_t const cInstrPerSec = nsElapsed ? (uint64_t)cInstructions * 1000000000 / nsElapsed : 0;
    printf("%10u %s instructions per second\n", (unsigned)cInstrPerSec, pszInstruction);
    return 0;
}



#elif defined(RT_OS_LINUX)

/*
 * GNU/linux - KVM
 */

static int createVM(void)
{
    int fd = open("/dev/kvm", O_RDWR);
    if (fd < 0)
        return error("Error opening /dev/kvm: %d\n", errno);

    g_fdVm = ioctl(fd, KVM_CREATE_VM, (uintptr_t)0);
    if (g_fdVm < 0)
        return error("KVM_CREATE_VM failed: %d\n", errno);

    /* Create the VCpu. */
    g_cbVCpuRun = ioctl(fd, KVM_GET_VCPU_MMAP_SIZE, (uintptr_t)0);
    if (g_cbVCpuRun <= 0x1000 || (g_cbVCpuRun & 0xfff))
        return error("Failed to get KVM_GET_VCPU_MMAP_SIZE: %#xz errno=%d\n", g_cbVCpuRun, errno);

    g_fdVCpu = ioctl(g_fdVm, KVM_CREATE_VCPU, (uintptr_t)0);
    if (g_fdVCpu < 0)
        return error("KVM_CREATE_VCPU failed: %d\n", errno);

    g_pVCpuRun = (struct kvm_run *)mmap(NULL, g_cbVCpuRun, PROT_READ | PROT_WRITE, MAP_PRIVATE, g_fdVCpu, 0);
    if ((void *)g_pVCpuRun == MAP_FAILED)
        return error("mmap kvm_run failed: %d\n", errno);

    /* Memory. */
    g_pbMem = (unsigned char *)mmap(NULL, g_cbMem, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((void *)g_pbMem == MAP_FAILED)
        return error("mmap RAM failed: %d\n", errno);

    struct kvm_userspace_memory_region MemReg;
    MemReg.slot            = 0;
    MemReg.flags           = 0;
    MemReg.guest_phys_addr = MY_MEM_BASE;
    MemReg.memory_size     = g_cbMem;
    MemReg.userspace_addr  = (uintptr_t)g_pbMem;
    int rc = ioctl(g_fdVm, KVM_SET_USER_MEMORY_REGION, &MemReg);
    if (rc != 0)
        return error("KVM_SET_USER_MEMORY_REGION failed: %d (%d)\n", errno, rc);

    close(fd);
    return 0;
}


static void printSReg(const char *pszName, struct kvm_segment const *pSReg)
{
    fprintf(stderr, "     %5s=%04x  base=%016llx  limit=%08x type=%#x p=%d dpl=%d db=%d s=%d l=%d g=%d avl=%d un=%d\n",
            pszName, pSReg->selector, pSReg->base, pSReg->limit, pSReg->type, pSReg->present, pSReg->dpl,
            pSReg->db, pSReg->s, pSReg->l, pSReg->g, pSReg->avl, pSReg->unusable);
}


static int runtimeError(const char *pszFormat, ...)
{
    fprintf(stderr, "runtime error: ");
    va_list va;
    va_start(va, pszFormat);
    vfprintf(stderr, pszFormat, va);
    va_end(va);

    fprintf(stderr, "                  exit_reason=%#010x\n", g_pVCpuRun->exit_reason);
    fprintf(stderr, "ready_for_interrupt_injection=%#x\n", g_pVCpuRun->ready_for_interrupt_injection);
    fprintf(stderr, "                      if_flag=%#x\n", g_pVCpuRun->if_flag);
    fprintf(stderr, "                        flags=%#x\n", g_pVCpuRun->flags);
    fprintf(stderr, "               kvm_valid_regs=%#018llx\n", g_pVCpuRun->kvm_valid_regs);
    fprintf(stderr, "               kvm_dirty_regs=%#018llx\n", g_pVCpuRun->kvm_dirty_regs);

    struct kvm_regs Regs;
    memset(&Regs, 0, sizeof(Regs));
    struct kvm_sregs SRegs;
    memset(&SRegs, 0, sizeof(SRegs));
    if (   ioctl(g_fdVCpu, KVM_GET_REGS, &Regs) != -1
        && ioctl(g_fdVCpu, KVM_GET_SREGS, &SRegs) != -1)
    {
        fprintf(stderr, "       rip=%016llx\n", Regs.rip);
        printSReg("cs", &SRegs.cs);
        fprintf(stderr, "    rflags=%08llx\n", Regs.rflags);
        fprintf(stderr, "       rax=%016llx\n", Regs.rax);
        fprintf(stderr, "       rbx=%016llx\n", Regs.rcx);
        fprintf(stderr, "       rdx=%016llx\n", Regs.rdx);
        fprintf(stderr, "       rcx=%016llx\n", Regs.rbx);
        fprintf(stderr, "       rsp=%016llx\n", Regs.rsp);
        fprintf(stderr, "       rbp=%016llx\n", Regs.rbp);
        fprintf(stderr, "       rsi=%016llx\n", Regs.rsi);
        fprintf(stderr, "       rdi=%016llx\n", Regs.rdi);
        printSReg("ss", &SRegs.ss);
        printSReg("ds", &SRegs.ds);
        printSReg("es", &SRegs.es);
        printSReg("fs", &SRegs.fs);
        printSReg("gs", &SRegs.gs);
        printSReg("tr", &SRegs.tr);
        printSReg("ldtr", &SRegs.ldt);

        uint64_t const offMem = Regs.rip + SRegs.cs.base - MY_MEM_BASE;
        if (offMem < g_cbMem - 10)
            fprintf(stderr, "  bytes at PC (%#zx): %02x %02x %02x %02x %02x %02x %02x %02x\n", (size_t)(offMem + MY_MEM_BASE),
                    g_pbMem[offMem    ], g_pbMem[offMem + 1], g_pbMem[offMem + 2], g_pbMem[offMem + 3],
                    g_pbMem[offMem + 4], g_pbMem[offMem + 5], g_pbMem[offMem + 6], g_pbMem[offMem + 7]);
    }

    return 1;
}

static int runRealModeTest(unsigned cInstructions, const char *pszInstruction,
                           unsigned uEax, unsigned uEcx, unsigned uEdx, unsigned uEbx,
                           unsigned uEsp, unsigned uEbp, unsigned uEsi, unsigned uEdi)
{
    /*
     * Setup real mode context.
     */
#define SET_SEG(a_SReg, a_Base, a_Limit, a_Sel, a_fCode) \
        do { \
            a_SReg.base     = (a_Base); \
            a_SReg.limit    = (a_Limit); \
            a_SReg.selector = (a_Sel); \
            a_SReg.type     = (a_fCode) ? 10 : 3; \
            a_SReg.present  = 1; \
            a_SReg.dpl      = 0; \
            a_SReg.db       = 0; \
            a_SReg.s        = 1; \
            a_SReg.l        = 0; \
            a_SReg.g        = 0; \
            a_SReg.avl      = 0; \
            a_SReg.unusable = 0; \
            a_SReg.padding  = 0; \
        } while (0)
    struct kvm_regs Regs;
    memset(&Regs, 0, sizeof(Regs));
    Regs.rax = uEax;
    Regs.rcx = uEcx;
    Regs.rdx = uEdx;
    Regs.rbx = uEbx;
    Regs.rsp = uEsp;
    Regs.rbp = uEbp;
    Regs.rsi = uEsi;
    Regs.rdi = uEdi;
    Regs.rip = MY_TEST_RIP;
    Regs.rflags = 1;
    int rc = ioctl(g_fdVCpu, KVM_SET_REGS, &Regs);
    if (rc != 0)
        return error("KVM_SET_REGS failed: %d (rc=%d)\n", errno, rc);

    struct kvm_sregs SRegs;
    memset(&SRegs, 0, sizeof(SRegs));
    rc = ioctl(g_fdVCpu, KVM_GET_SREGS, &SRegs);
    if (rc != 0)
        return error("KVM_GET_SREGS failed: %d (rc=%d)\n", errno, rc);
    SET_SEG(SRegs.es, 0x00000, 0xffff, 0x0000, 0);
    SET_SEG(SRegs.cs, 0x00000, 0xffff, 0x0000, 1);
    SET_SEG(SRegs.ss, 0x00000, 0xffff, 0x0000, 0);
    SET_SEG(SRegs.ds, 0x00000, 0xffff, 0x0000, 0);
    SET_SEG(SRegs.fs, 0x00000, 0xffff, 0x0000, 0);
    SET_SEG(SRegs.gs, 0x00000, 0xffff, 0x0000, 0);
    //SRegs.cr0 = 0x10010 /*WP+ET*/;
    SRegs.cr2 = 0;
    //SRegs.cr3 = 0;
    //SRegs.cr4 = 0;
    rc = ioctl(g_fdVCpu, KVM_SET_SREGS, &SRegs);
    if (rc != 0)
        return error("KVM_SET_SREGS failed: %d (rc=%d)\n", errno, rc);

    /*
     * Run the test.
     */
    uint64_t const nsStart = getNanoTS();
    for (;;)
    {
        rc = ioctl(g_fdVCpu, KVM_RUN, (uintptr_t)0);
        if (rc == 0)
        {
            if (g_pVCpuRun->exit_reason == KVM_EXIT_IO)
            {
                if (g_pVCpuRun->io.port == MY_NOP_PORT)
                { /* likely: nop instruction */ }
                else if (g_pVCpuRun->io.port == MY_TERM_PORT)
                    break;
                else
                    return runtimeError("Unexpected I/O port access (for %s): %#x\n", pszInstruction, g_pVCpuRun->io.port);
            }
            else if (g_pVCpuRun->exit_reason == KVM_EXIT_MMIO)
            {
                if (g_pVCpuRun->mmio.phys_addr == MY_NOP_MMIO)
                { /* likely: nop address */ }
                else
                    return runtimeError("Unexpected memory access (for %s): %#llx\n", pszInstruction, g_pVCpuRun->mmio.phys_addr);
            }
            else
                return runtimeError("Unexpected exit (for %s): %d\n", pszInstruction, g_pVCpuRun->exit_reason);
        }
        else
            return runtimeError("KVM_RUN failed (for %s): %#x (ret %d)\n", pszInstruction, errno, rc);
    }
    uint64_t const nsElapsed = getNanoTS() - nsStart;

    /* Report the results. */
    uint64_t const cInstrPerSec = nsElapsed ? (uint64_t)cInstructions * 1000000000 / nsElapsed : 0;
    printf("%10u %s instructions per second\n", (unsigned)cInstrPerSec, pszInstruction);
    return 0;
}


#elif defined(RT_OS_DARWIN)
# error "port me"
#else
# error "port me"
#endif


int ioportTest(unsigned cFactor)
{
    /*
     * Produce realmode code
     */
    unsigned char *pb = &g_pbMem[MY_TEST_RIP - MY_MEM_BASE];
    unsigned char * const pbStart = pb;
    /* OUT DX, AL - 10 times */
    for (unsigned i = 0; i < 10; i++)
        *pb++ = 0xee;
    /* DEC ECX */
    *pb++ = 0x66;
    *pb++ = 0x48 + 1;
    /* JNZ MY_TEST_RIP */
    *pb++ = 0x75;
    *pb   = (signed char)(pbStart - pb + 1);
    pb++;
    /* OUT 1, AL -  Temination port call. */
    *pb++ = 0xe6;
    *pb++ = MY_TERM_PORT;
    /* JMP to previous instruction */
    *pb++ = 0xeb;
    *pb++ = 0xfc;

    return runRealModeTest(100000 * cFactor, "OUT", 42 /*eax*/, 10000 * cFactor /*ecx*/, MY_NOP_PORT /*edx*/, 0 /*ebx*/,
                           0 /*esp*/, 0 /*ebp*/, 0 /*esi*/, 0 /*uEdi*/);
}


int cpuidTest(unsigned cFactor)
{
    /*
     * Produce realmode code
     */
    unsigned char *pb = &g_pbMem[MY_TEST_RIP - MY_MEM_BASE];
    unsigned char * const pbStart = pb;
    for (unsigned i = 0; i < 10; i++)
    {
        /* XOR EAX,EAX */
#ifndef RT_OS_LINUX /* Broken on 4.18.0, probably wrong rip advance (2 instead of 3 bytes). */
        *pb++ = 0x66;
#endif
        *pb++ = 0x33;
        *pb++ = 0xc0;

        /* CPUID */
        *pb++ = 0x0f;
        *pb++ = 0xa2;
    }
    /* DEC ESI */
    *pb++ = 0x66;
    *pb++ = 0x48 + 6;
    /* JNZ MY_TEST_RIP */
    *pb++ = 0x75;
    *pb   = (signed char)(pbStart - pb + 1);
    pb++;
    /* OUT 1, AL -  Temination port call. */
    *pb++ = 0xe6;
    *pb++ = MY_TERM_PORT;
    /* JMP to previous instruction */
    *pb++ = 0xeb;
    *pb++ = 0xfc;

    return runRealModeTest(100000 * cFactor, "CPUID", 0 /*eax*/, 0 /*ecx*/, 0 /*edx*/, 0 /*ebx*/,
                           0 /*esp*/, 0 /*ebp*/, 10000 * cFactor /*esi*/, 0 /*uEdi*/);
}


int mmioTest(unsigned cFactor)
{
    /*
     * Produce realmode code accessing MY_MMIO_NOP address assuming it's low.
     */
    unsigned char *pb = &g_pbMem[MY_TEST_RIP - MY_MEM_BASE];
    unsigned char * const pbStart = pb;
    for (unsigned i = 0; i < 10; i++)
    {
        /* MOV AL,DS:[BX] */
        *pb++ = 0x8a;
        *pb++ = 0x07;
    }
    /* DEC ESI */
    *pb++ = 0x66;
    *pb++ = 0x48 + 6;
    /* JNZ MY_TEST_RIP */
    *pb++ = 0x75;
    *pb   = (signed char)(pbStart - pb + 1);
    pb++;
    /* OUT 1, AL -  Temination port call. */
    *pb++ = 0xe6;
    *pb++ = MY_TERM_PORT;
    /* JMP to previous instruction */
    *pb++ = 0xeb;
    *pb++ = 0xfc;

    return runRealModeTest(100000 * cFactor, "MMIO/r1" , 0 /*eax*/, 0 /*ecx*/, 0 /*edx*/, MY_NOP_MMIO /*ebx*/,
                           0 /*esp*/, 0 /*ebp*/, 10000 * cFactor /*esi*/, 0 /*uEdi*/);
}



int main(int argc, char **argv)
{
    /*
     * Do some parameter parsing.
     */
#ifdef RT_OS_WINDOWS
    unsigned const  cFactorDefault = 4;
#else
    unsigned const  cFactorDefault = 10;
#endif
    unsigned        cFactor = cFactorDefault;
    for (int i = 1; i < argc; i++)
    {
        const char *pszArg = argv[i];
        if (   strcmp(pszArg, "--help") == 0
            || strcmp(pszArg, "/help") == 0
            || strcmp(pszArg, "-h") == 0
            || strcmp(pszArg, "-?") == 0
            || strcmp(pszArg, "/?") == 0)
        {
            printf("Does some benchmarking of the native NEM engine.\n"
                   "\n"
                   "Usage: NemRawBench-1 --factor <factor>\n"
                   "\n"
                   "Options\n"
                   "  --factor <factor>\n"
                   "        Iteration count factor.  Default is %u.\n"
                   "        Lower it if execution is slow, increase if quick.\n",
                   cFactorDefault);
            return 0;
        }
        if (strcmp(pszArg, "--factor") == 0)
        {
            i++;
            if (i < argc)
                cFactor = atoi(argv[i]);
            else
            {
                fprintf(stderr, "syntax error: Option %s is takes a value!\n", pszArg);
                return 2;
            }
        }
        else
        {
            fprintf(stderr, "syntax error: Unknown option: %s\n", pszArg);
            return 2;
        }
    }

    /*
     * Create the VM
     */
    g_cbMem = 128*1024 - MY_MEM_BASE;
    int rcExit = createVM();
    if (rcExit == 0)
    {
        printf("tstNemMini-1: Successfully created test VM...\n");

        /*
         * Do the benchmarking.
         */
        ioportTest(cFactor);
#ifndef RT_OS_LINUX
        cpuidTest(cFactor);
#endif
        mmioTest(cFactor);

        printf("tstNemMini-1: done\n");
    }
    return rcExit;
}

/*
 * Results:
 *
 * - Linux 4.18.0-1-amd64 (debian); AMD Threadripper:
 *     545108 OUT instructions per second
 *     373159 MMIO/r1 instructions per second
 *
 *
 */
