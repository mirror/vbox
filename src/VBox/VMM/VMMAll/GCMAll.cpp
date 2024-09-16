/** @file
 * GCM - Guest Compatibility Manager - All Contexts.
 */

/*
 * Copyright (C) 2022-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GCM
#include <VBox/vmm/gcm.h>
#include "GCMInternal.h"
#include <VBox/vmm/vmcc.h>

#include <VBox/dis.h>       /* For DISSTATE */
#include <VBox/err.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define VMWARE_HYPERVISOR_PORT                  UINT16_C(0x5658)
#define VMWARE_HYPERVISOR_PORT_HB               UINT16_C(0x5659)
#define VMWARE_HYPERVISOR_MAGIC                 UINT32_C(0x564d5868)    /**< eax value */

#define VMWARE_HYPERVISOR_CMD_MSG               0x001e
#define VMWARE_HYPERVISOR_CMD_HB_MSG            0x0000
#define VMWARE_HYPERVISOR_CMD_OPEN_CHANNEL      RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MSG, 0) /**< ecx */
#define VMWARE_OC_RPCI_PROTOCOL_NUM                 UINT32_C(0x49435052) /**< VMWARE_HYPERVISOR_CMD_OPEN_CHANNEL: ebx[30:0] */
#define VMWARE_OC_GUESTMSG_FLAG_COOKIE              UINT32_C(0x80000000) /**< VMWARE_HYPERVISOR_CMD_OPEN_CHANNEL: ebx bit 31 */
#define VMWARE_HYPERVISOR_CMD_SEND_SIZE         RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MSG, 1) /**< ecx */
#define VMWARE_HYPERVISOR_CMD_SEND_PAYLOAD      RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MSG, 2) /**< ecx */
#define VMWARE_HYPERVISOR_CMD_RECV_SIZE         RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MSG, 3) /**< ecx */
#define VMWARE_HYPERVISOR_CMD_RECV_PAYLOAD      RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MSG, 4) /**< ecx */
#define VMWARE_HYPERVISOR_CMD_RECV_STATUS       RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MSG, 5) /**< ecx */
#define VMWARE_HYPERVISOR_CMD_CLOSE_CHANNEL     RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MSG, 6) /**< ecx */

#define VMWARE_HYPERVISOR_CMD_MKS_GUEST_STATS   0x0055
#define VMWARE_HYPERVISOR_CMD_MKSGS_RESET       RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MKS_GUEST_STATS, 0) /**< ecx */
#define VMWARE_HYPERVISOR_CMD_MKSGS_ADD_PPN     RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MKS_GUEST_STATS, 1) /**< ecx */
#define VMWARE_HYPERVISOR_CMD_MKSGS_REMOVE_PPN  RT_MAKE_U32(VMWARE_HYPERVISOR_CMD_MKS_GUEST_STATS, 2) /**< ecx */

/** @name Message status return flags (ecx).
 * @{ */
#define VMWARE_MSG_STATUS_F_SUCCESS             UINT32_C(0x00010000)
#define VMWARE_MSG_STATUS_F_DO_RECV             UINT32_C(0x00020000)
#define VMWARE_MSG_STATUS_F_CPT                 UINT32_C(0x00100000)
#define VMWARE_MSG_STATUS_F_HB                  UINT32_C(0x00800000)
/** @} */



/**
 * Whether \#DE exceptions in the guest should be intercepted by GCM and
 * possibly fixed up.
 *
 * @returns true if needed, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMM_INT_DECL(bool) GCMIsInterceptingXcptDE(PVMCPUCC pVCpu)
{
    /* See if the enabled fixers need to intercept #DE. */
    PVM const  pVM  = pVCpu->CTX_SUFF(pVM);
    bool const fRet = (pVM->gcm.s.fFixerSet & (GCMFIXER_DBZ_DOS | GCMFIXER_DBZ_OS2 | GCMFIXER_DBZ_WIN9X)) != 0;
    LogFlow(("GCMIsInterceptingXcptDE: returns %d\n", fRet));
    return fRet;
}


/**
 * Exception handler for \#DE when registered by GCM.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS retry division and continue.
 * @retval  VERR_NOT_FOUND deliver exception to guest.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @thread  EMT(pVCpu).
 */
VMM_INT_DECL(int) GCMXcptDE(PVMCPUCC pVCpu, PCPUMCTX pCtx)
{
    PVMCC pVM = pVCpu->CTX_SUFF(pVM);
    Assert(pVM->gcm.s.fFixerSet & (GCMFIXER_DBZ_DOS | GCMFIXER_DBZ_OS2 | GCMFIXER_DBZ_WIN9X));

    LogRel(("GCM: Intercepted #DE at CS:RIP=%04x:%RX64 (%RX64 linear) RDX:RAX=%RX64:%RX64 RCX=%RX64 RBX=%RX64\n",
            pCtx->cs.Sel, pCtx->rip, pCtx->cs.u64Base + pCtx->rip, pCtx->rdx, pCtx->rax, pCtx->rcx, pCtx->rbx));

    if (pVM->gcm.s.fFixerSet & GCMFIXER_DBZ_OS2)
    {
        if (pCtx->rcx == 0 && pCtx->rdx == 1 && pCtx->rax == 0x86a0)
        {
            /* OS/2 1.x drivers loaded during boot: DX:AX = 100,000, CX < 2 causes overflow. */
            /* Example: OS/2 1.0 KBD01.SYS, 16,945 bytes, dated 10/21/1987, div cx at offset 2:2ffeh */
            /* Code later merged into BASEDD01.SYS, crash fixed in OS/2 1.30.1; this should
             * fix all affected versions of OS/2 1.x.
             */
            pCtx->rcx = 2;
            return VINF_SUCCESS;
        }
        if ((uint16_t)pCtx->rbx == 0 && (uint16_t)pCtx->rdx == 0 && (uint16_t)pCtx->rax == 0x1000)
        {
            /* OS/2 2.1 and later boot loader: DX:AX = 0x1000, zero BX. May have junk in high words of all registers. */
            /* Example: OS/2 MCP2 OS2LDR, 44,544 bytes, dated 03/08/2002, idiv bx at offset 847ah */
            pCtx->rbx = (pCtx->rbx & ~0xffff) | 2;
            return VINF_SUCCESS;
        }
        if (pCtx->rbx == 0 && pCtx->rdx == 0 && pCtx->rax == 0x100)
        {
            /* OS/2 2.0 boot loader: DX:AX = 0x100, zero BX. May have junk in high words of registers. */
            /* Example: OS/2 2.0 OS2LDR, 32,256 bytes, dated 03/30/1992, idiv bx at offset 2298h */
            pCtx->rbx = 2;
            return VINF_SUCCESS;
        }
    }

    if (pVM->gcm.s.fFixerSet & GCMFIXER_DBZ_DOS)
    {
        /* NB: For 16-bit DOS software, we must generally only compare 16-bit registers.
         * The contents of the high words may be unpredictable depending on the environment.
         * For 32-bit Windows 3.x code that is not the case.
         */
        if (pCtx->rcx == 0 && pCtx->rdx == 0 && pCtx->rax == 0x100000)
        {
            /* NDIS.386 in WfW 3.11: CalibrateStall, EDX:EAX = 0x100000, zero ECX.
             * Occurs when NDIS.386 loads.
             */
            pCtx->rcx = 0x20000;    /* Want a large divisor to shorten stalls. */
            return VINF_SUCCESS;
        }
        if (pCtx->rcx == 0 && pCtx->rdx == 0 && pCtx->rax > 0x100000)
        {
            /* NDIS.386 in WfW 3.11: NdisStallExecution, EDX:EAX = 0xYY00000, zero ECX.
             * EDX:EAX is variable, but low 20 bits of EAX must be zero and EDX is likely
             * to be zero as well.
             * Only occurs if NdisStallExecution is called to do a longish stall.
             */
            pCtx->rcx = 22;
            return VINF_SUCCESS;
        }
        if ((uint16_t)pCtx->rbx == 0 && (uint16_t)pCtx->rdx == 0 && (uint16_t)pCtx->rax == 0x64)
        {
            /* Norton Sysinfo or Diagnostics 8.0 DX:AX = 0x64 (100 decimal), zero BX. */
            pCtx->rbx = (pCtx->rbx & 0xffff0000) | 1;   /* BX = 1 */
            return VINF_SUCCESS;
        }
        if ((uint16_t)pCtx->rbx == 0 && (uint16_t)pCtx->rdx == 0 && (uint16_t)pCtx->rax == 0xff)
        {
            /* IBM PC LAN Program 1.3: DX:AX=0xff (255 decimal), zero BX. */
            /* NETWORK1.CMD, 64,324 bytes, dated 06/06/1988, div bx at offset 0xa400 in file. */
            pCtx->rbx = (pCtx->rbx & 0xffff0000) | 1;   /* BX = 1 */
            return VINF_SUCCESS;
        }
        if ((uint16_t)pCtx->rdx == 0xffff && (uint16_t)pCtx->rax == 0xffff && (uint16_t)pCtx->rcx == 0xa8c0)
        {
            /* QNX 2.15C: DX:AX=0xffffffff (-1), constant CX = 0xa8c0 (43200). */
            /* div cx at e.g. 2220:fa5 and 2220:10a0 in memory. */
            pCtx->rdx = (pCtx->rdx & 0xffff0000) | 8;   /* DX = 8 */
            return VINF_SUCCESS;
        }
        if ((uint16_t)pCtx->rax > 0x1800 && ((uint16_t)pCtx->rax & 0x3f) == 0 && (uint16_t)pCtx->rbx == 0x19)
        {
            /* 3C501.COM ODI driver v1.21: AX > ~0x1900 (-1), BX = 0x19 (25). */
            /* AX was shifted left by 6 bits so low bits must be zero. */
            /* div bl at e.g. 06b3:2f80 and offset 0x2E80 in file. */
            pCtx->rax = (pCtx->rax & 0xffff0000) | 0x8c0;   /* AX = 0x8c0 */
            return VINF_SUCCESS;
        }
        if ((uint16_t)pCtx->rcx == 0x37 && ((uint16_t)pCtx->rdx > 0x34))
        {
            /* Turbo Pascal, classic Runtime Error 200: CX = 55, DX > ~54, AX/BX variable. */
            /* div cx at variable offset in file. */
            pCtx->rdx = (pCtx->rdx & 0xffff0000) | 0x30;    /* DX = 48 */
            return VINF_SUCCESS;
        }
    }

    if (pVM->gcm.s.fFixerSet & GCMFIXER_DBZ_WIN9X)
    {
        if (pCtx->rcx == 0 && pCtx->rdx == 0 && pCtx->rax == 0x100000)
        {
            /* NDIS.VXD in Win9x: EDX:EAX = 0x100000, zero ECX. */
            /* Example: Windows 95 NDIS.VXD, 99,084 bytes, dated 07/11/1994, div ecx at 28:Cxxxx80B */
            /* Crash fixed in Windows 98 SE. */
            pCtx->rcx = 0x20000;    /* Want a large divisor to shorten stalls. */
            return VINF_SUCCESS;
        }
        if (pCtx->rcx < 3 && pCtx->rdx == 2 && pCtx->rax == 0x540be400)
        {
            /* SCSI.PDR, ESDI506.PDR in Win95: EDX:EAX = 0x2540be400 (10,000,000,000 decimal), ECX < 3. */
            /* Example: Windows 95 SCSIPORT.PDR, 23,133 bytes, dated 07/11/1995, div ecx at 28:Cxxxx876  */
            /* Example: Win95 OSR2  ESDI506.PDR, 24,390 bytes, dated 04/24/1996, div ecx at 28:Cxxxx8E3 */
            /* Crash fixed in Windows 98. */
            pCtx->rcx = 1000;
            return VINF_SUCCESS;
        }
        if (pCtx->rcx == 0 && pCtx->rdx == 0x3d && pCtx->rax == 0x9000000)
        {
            /* Unknown source, Win9x shutdown, div ecx. */
            /* GCM: Intercepted #DE at CS:RIP=0028:c0050f8e RDX:RAX=3d:9000000 (250000*1024*1024) RCX=0 RBX=c19200e8 [RBX variable] */
            pCtx->rcx = 4096;
            return VINF_SUCCESS;
        }
    }

    /* If we got this far, deliver exception to guest. */
    return VERR_NOT_FOUND;
}


#if 0
/**
 * Whether \#GP exceptions in the guest should be intercepted by GCM and
 * possibly fixed up.
 *
 * @returns true if needed, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMM_INT_DECL(bool) GCMIsInterceptingXcptGP(PVMCPUCC pVCpu)
{
    /* See if the enabled fixers require #GP interception. */
    PVM const  pVM  = pVCpu->CTX_SUFF(pVM);
    bool const fRet = (pVM->gcm.s.fFixerSet & GCMFIXER_MESA_VMSVGA_DRV) != 0;
    LogFlow(("GCMIsInterceptingXcptDE: returns %d\n", fRet));
    return fRet;
}


/**
 * Exception handler for \#GP when registered by GCM.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS retry the instruction and continue.
 * @retval  VERR_NOT_FOUND deliver exception to guest.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @thread  EMT(pVCpu).
 */
VMM_INT_DECL(int) GCMXcptGP(PVMCPUCC pVCpu, PCPUMCTX pCtx)
{

}
#endif


/**
 * Checks if I/O port reads for the given port need GCM attention.
 *
 * @returns true if needed, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   u16Port     The port being accessed. UINT16_MAX and cbReg ==
 *                      UINT8_MAX for any port.
 * @param   cbReg       The access size. UINT8_MAX and u16Port == UINT16_MAX for
 *                      any size.
 */
VMM_INT_DECL(bool) GCMIsInterceptingIOPortReadSlow(PVMCPUCC pVCpu, uint16_t u16Port, uint8_t cbReg)
{
    PVM const  pVM  = pVCpu->CTX_SUFF(pVM);
    bool const fRet = (pVM->gcm.s.fFixerSet & GCMFIXER_MESA_VMSVGA_DRV) != 0
                   && (   (u16Port == VMWARE_HYPERVISOR_PORT && cbReg == 4)
                       || (u16Port == UINT16_MAX && cbReg == UINT8_MAX) );
    LogFlow(("GCMIsInterceptingIOPortReadSlow(,%#x,%#x): returns %d\n", u16Port, cbReg, fRet));
    return fRet;
}


/**
 * Processes an intercepted IO port read instruction.
 *
 * @returns Strict VBox status code. Only two informational status codes
 *          are returned VINF_GCM_HANDLED and VINF_GCM_HANDLED_ADVANCE_RIP.
 * @retval  VINF_GCM_HANDLED
 * @retval  VINF_GCM_HANDLED_ADVANCE_RIP
 * @retval  VERR_GCM_NOT_HANDLED
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCtx        The CPU context.
 * @param   u16Port     The port being accessed.
 * @param   cbReg       The size of the access.
 */
VMM_INT_DECL(VBOXSTRICTRC) GCMInterceptedIOPortRead(PVMCPUCC pVCpu, PCPUMCTX pCtx, uint16_t u16Port, uint8_t cbReg)
{
    Assert((pVCpu->CTX_SUFF(pVM)->gcm.s.fFixerSet & GCMFIXER_MESA_VMSVGA_DRV) != 0);
    if (u16Port == VMWARE_HYPERVISOR_PORT && cbReg == 4)
    {
        CPUM_IMPORT_EXTRN_WITH_CTX_RET(pVCpu, pCtx,
                                         CPUMCTX_EXTRN_RAX | CPUMCTX_EXTRN_RCX | CPUMCTX_EXTRN_RDX
                                       | CPUMCTX_EXTRN_RBX | CPUMCTX_EXTRN_RSI | CPUMCTX_EXTRN_RDI);
        if (pCtx->rax == VMWARE_HYPERVISOR_MAGIC)
        {
            switch (pCtx->rcx)
            {
                case VMWARE_HYPERVISOR_CMD_OPEN_CHANNEL:
                    Log(("GCMInterceptedIOPortRead: vmware open channel: protocol=%#RX64\n", pCtx->rbx));
                    break;
                case VMWARE_HYPERVISOR_CMD_SEND_SIZE:
                    Log(("GCMInterceptedIOPortRead: vmware send size\n"));
                    break;
                case VMWARE_HYPERVISOR_CMD_SEND_PAYLOAD:
                    Log(("GCMInterceptedIOPortRead: vmware send payload\n"));
                    break;
                case VMWARE_HYPERVISOR_CMD_RECV_SIZE:
                    Log(("GCMInterceptedIOPortRead: vmware recv size\n"));
                    break;
                case VMWARE_HYPERVISOR_CMD_RECV_PAYLOAD:
                    Log(("GCMInterceptedIOPortRead: vmware recv payload\n"));
                    break;
                case VMWARE_HYPERVISOR_CMD_RECV_STATUS:
                    Log(("GCMInterceptedIOPortRead: vmware recv status\n"));
                    break;
                case VMWARE_HYPERVISOR_CMD_CLOSE_CHANNEL:
                    Log(("GCMInterceptedIOPortRead: vmware close channel\n"));
                    break;

                case VMWARE_HYPERVISOR_CMD_MKSGS_RESET:
                    Log(("GCMInterceptedIOPortRead: vmware mks guest stats reset\n"));
                    break;
                case VMWARE_HYPERVISOR_CMD_MKSGS_ADD_PPN:
                    Log(("GCMInterceptedIOPortRead: vmware mks guest stats add ppn\n"));
                    break;
                case VMWARE_HYPERVISOR_CMD_MKSGS_REMOVE_PPN:
                    Log(("GCMInterceptedIOPortRead: vmware mks guest stats remove ppn\n"));
                    break;

                default:
                    LogRelMax(64, ("GCMInterceptedIOPortRead: Unknown vmware hypervisor call: rcx=%#RX64 (cmd), rbx=%#RX64 (len/whatever), rsi=%#RX64 (input), rdi=%#RX64 (input), rdx=%#RX64 (flags + chid) at %04x:%08RX64\n",
                                   pCtx->rcx, pCtx->rbx, pCtx->rsi, pCtx->rdi, pCtx->rdx, pCtx->cs.Sel, pCtx->rip));
                    return VERR_GCM_NOT_HANDLED;
            }

            /* Just fail the command. */
            pCtx->ecx &= ~VMWARE_MSG_STATUS_F_SUCCESS;
            return VINF_GCM_HANDLED_ADVANCE_RIP;
        }
    }
    return VERR_GCM_NOT_HANDLED;
}


#if 0 /* If we need to deal with high speed vmware hypervisor calls */

/**
 * Checks if I/O port string reads for the given port need GCM attention.
 *
 * @returns true if needed, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   u16Port     The port being accessed. UINT16_MAX and cbReg ==
 *                      UINT8_MAX for any port.
 * @param   cbReg       The access size. UINT8_MAX and u16Port == UINT16_MAX for
 *                      any size.
 */
VMM_INT_DECL(bool) GCMIsInterceptingIOPortReadStringSlow(PVMCPUCC pVCpu, uint16_t u16Port, uint8_t cbReg)
{
    PVM const  pVM  = pVCpu->CTX_SUFF(pVM);
    bool const fRet = (pVM->gcm.s.fFixerSet & GCMFIXER_MESA_VMSVGA_DRV) != 0
                   && (   (u16Port == VMWARE_HYPERVISOR_PORT_HB && cbReg == 1)
                       || (u16Port == UINT16_MAX && cbReg == UINT8_MAX) );
    LogFlow(("GCMIsInterceptingIOPortReadStringSlow(,%#x,%#x): returns %d\n", u16Port, cbReg, fRet));
    return fRet;
}


/**
 * Checks if I/O port string writes for the given port need GCM attention.
 *
 * @returns true if needed, false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   u16Port     The port being accessed. UINT16_MAX and cbReg ==
 *                      UINT8_MAX for any port.
 * @param   cbReg       The access size. UINT8_MAX and u16Port == UINT16_MAX for
 *                      any size.
 */
VMM_INT_DECL(bool) GCMIsInterceptingIOPortWriteStringSlow(PVMCPUCC pVCpu, uint16_t u16Port, uint8_t cbReg)
{
    PVM const  pVM  = pVCpu->CTX_SUFF(pVM);
    bool const fRet = (pVM->gcm.s.fFixerSet & GCMFIXER_MESA_VMSVGA_DRV) != 0
                   && (   (u16Port == VMWARE_HYPERVISOR_PORT_HB && cbReg == 1)
                       || (u16Port == UINT16_MAX && cbReg == UINT8_MAX) );
    LogFlow(("GCMIsInterceptingIOPortWriteStringSlow(,%#x,%#x): returns %d\n", u16Port, cbReg, fRet));
    return fRet;
}

#endif

