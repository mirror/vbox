/* $Id$ */
/** @file
 * DBGC - Debugger Console, Windows Kd Remote Stub.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/dbg.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/vmapi.h> /* VMR3GetVM() */
#include <VBox/vmm/hm.h>    /* HMR3IsEnabled */
#include <VBox/vmm/nem.h>   /* NEMR3IsEnabled */
#include <iprt/assertcompile.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/sg.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/x86.h>
#include <iprt/formats/pecoff.h>
#include <iprt/formats/mz.h>

#include <stdlib.h>

#include "DBGCInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** Number of milliseconds we wait for new data to arrive when a new packet was detected. */
#define DBGC_KD_RECV_TIMEOUT_MS                     UINT32_C(1000)

/** NT status code - Success. */
#define NTSTATUS_SUCCESS                            0
/** NT status code - operation unsuccesful. */
#define NTSTATUS_UNSUCCESSFUL                       UINT32_C(0xc0000001)
/** NT status code - operation not implemented. */
#define NTSTATUS_NOT_IMPLEMENTED                    UINT32_C(0xc0000002)

/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * KD packet header as sent over the wire.
 */
typedef struct KDPACKETHDR
{
    /** Packet signature (leader) - defines the type of packet. */
    uint32_t                    u32Signature;
    /** Packet (sub) type. */
    uint16_t                    u16SubType;
    /** Size of the packet body in bytes.*/
    uint16_t                    cbBody;
    /** Packet ID. */
    uint32_t                    idPacket;
    /** Checksum of the packet body. */
    uint32_t                    u32ChkSum;
} KDPACKETHDR;
AssertCompileSize(KDPACKETHDR, 16);
/** Pointer to a packet header. */
typedef KDPACKETHDR *PKDPACKETHDR;
/** Pointer to a const packet header. */
typedef const KDPACKETHDR *PCKDPACKETHDR;

/** Signature for a data packet. */
#define KD_PACKET_HDR_SIGNATURE_DATA                UINT32_C(0x30303030)
/** First byte for a data packet header. */
#define KD_PACKET_HDR_SIGNATURE_DATA_BYTE           0x30
/** Signature for a control packet. */
#define KD_PACKET_HDR_SIGNATURE_CONTROL             UINT32_C(0x69696969)
/** First byte for a control packet header. */
#define KD_PACKET_HDR_SIGNATURE_CONTROL_BYTE        0x69
/** Signature for a breakin packet. */
#define KD_PACKET_HDR_SIGNATURE_BREAKIN             UINT32_C(0x62626262)
/** First byte for a breakin packet header. */
#define KD_PACKET_HDR_SIGNATURE_BREAKIN_BYTE        0x62

/** @name Packet sub types.
 * @{ */
#define KD_PACKET_HDR_SUB_TYPE_STATE_CHANGE32       UINT16_C(1)
#define KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE     UINT16_C(2)
#define KD_PACKET_HDR_SUB_TYPE_DEBUG_IO             UINT16_C(3)
#define KD_PACKET_HDR_SUB_TYPE_ACKNOWLEDGE          UINT16_C(4)
#define KD_PACKET_HDR_SUB_TYPE_RESEND               UINT16_C(5)
#define KD_PACKET_HDR_SUB_TYPE_RESET                UINT16_C(6)
#define KD_PACKET_HDR_SUB_TYPE_STATE_CHANGE64       UINT16_C(7)
#define KD_PACKET_HDR_SUB_TYPE_POLL_BREAKIN         UINT16_C(8)
#define KD_PACKET_HDR_SUB_TYPE_TRACE_IO             UINT16_C(9)
#define KD_PACKET_HDR_SUB_TYPE_CONTROL_REQUEST      UINT16_C(10)
#define KD_PACKET_HDR_SUB_TYPE_FILE_IO              UINT16_C(11)
#define KD_PACKET_HDR_SUB_TYPE_MAX                  UINT16_C(12)
/** @} */

/** Initial packet ID value. */
#define KD_PACKET_HDR_ID_INITIAL                    UINT32_C(0x80800800)
/** Packet ID value after a resync. */
#define KD_PACKET_HDR_ID_RESET                      UINT32_C(0x80800000)

/** Trailing byte of a packet. */
#define KD_PACKET_TRAILING_BYTE                     0xaa


/** Maximum number of parameters in the exception record. */
#define KDPACKETEXCP_PARMS_MAX                      15

/**
 * 64bit exception record.
 */
typedef struct KDPACKETEXCP64
{
    /** The exception code identifying the excpetion. */
    uint32_t                    u32ExcpCode;
    /** Flags associated with the exception. */
    uint32_t                    u32ExcpFlags;
    /** Pointer to a chained exception record. */
    uint64_t                    u64PtrExcpRecNested;
    /** Address where the exception occurred. */
    uint64_t                    u64PtrExcpAddr;
    /** Number of parameters in the exception information array. */
    uint32_t                    cExcpParms;
    /** Alignment. */
    uint32_t                    u32Alignment;
    /** Exception parameters array. */
    uint64_t                    au64ExcpParms[KDPACKETEXCP_PARMS_MAX];
} KDPACKETEXCP64;
AssertCompileSize(KDPACKETEXCP64, 152);
/** Pointer to an exception record. */
typedef KDPACKETEXCP64 *PKDPACKETEXCP64;
/** Pointer to a const exception record. */
typedef const KDPACKETEXCP64 *PCKDPACKETEXCP64;


/**
 * amd64 NT context structure.
 */
typedef struct NTCONTEXT64
{
    /** The P[1-6]Home members. */
    uint64_t                    au64PHome[6];
    /** Context flags indicating the valid bits, see NTCONTEXT_F_XXX. */
    uint32_t                    fContext;
    /** MXCSR register. */
    uint32_t                    u32RegMxCsr;
    /** CS selector. */
    uint16_t                    u16SegCs;
    /** DS selector. */
    uint16_t                    u16SegDs;
    /** ES selector. */
    uint16_t                    u16SegEs;
    /** FS selector. */
    uint16_t                    u16SegFs;
    /** GS selector. */
    uint16_t                    u16SegGs;
    /** SS selector. */
    uint16_t                    u16SegSs;
    /** EFlags register. */
    uint32_t                    u32RegEflags;
    /** DR0 register. */
    uint64_t                    u64RegDr0;
    /** DR1 register. */
    uint64_t                    u64RegDr1;
    /** DR2 register. */
    uint64_t                    u64RegDr2;
    /** DR3 register. */
    uint64_t                    u64RegDr3;
    /** DR6 register. */
    uint64_t                    u64RegDr6;
    /** DR7 register. */
    uint64_t                    u64RegDr7;
    /** RAX register. */
    uint64_t                    u64RegRax;
    /** RCX register. */
    uint64_t                    u64RegRcx;
    /** RDX register. */
    uint64_t                    u64RegRdx;
    /** RBX register. */
    uint64_t                    u64RegRbx;
    /** RSP register. */
    uint64_t                    u64RegRsp;
    /** RBP register. */
    uint64_t                    u64RegRbp;
    /** RSI register. */
    uint64_t                    u64RegRsi;
    /** RDI register. */
    uint64_t                    u64RegRdi;
    /** R8 register. */
    uint64_t                    u64RegR8;
    /** R9 register. */
    uint64_t                    u64RegR9;
    /** R10 register. */
    uint64_t                    u64RegR10;
    /** R11 register. */
    uint64_t                    u64RegR11;
    /** R12 register. */
    uint64_t                    u64RegR12;
    /** R13 register. */
    uint64_t                    u64RegR13;
    /** R14 register. */
    uint64_t                    u64RegR14;
    /** R15 register. */
    uint64_t                    u64RegR15;
    /** RIP register. */
    uint64_t                    u64RegRip;
    /** Extended floating point save area. */
    X86FXSTATE                  FxSave;
    /** AVX(?) vector registers. */
    RTUINT128U                  aRegsVec[26];
    /** Vector control register. */
    uint64_t                    u64RegVecCtrl;
    /** Debug control. */
    uint64_t                    u64DbgCtrl;
    /** @todo */
    uint64_t                    u64LastBrToRip;
    uint64_t                    u64LastBrFromRip;
    uint64_t                    u64LastExcpToRip;
    uint64_t                    u64LastExcpFromRip;
} NTCONTEXT64;
AssertCompileSize(NTCONTEXT64, 1232);
AssertCompileMemberOffset(NTCONTEXT64, FxSave, 0x100);
AssertCompileMemberOffset(NTCONTEXT64, aRegsVec[0], 0x300);
/** Pointer to an amd64 NT context. */
typedef NTCONTEXT64 *PNTCONTEXT64;
/** Pointer to a const amd64 NT context. */
typedef const NTCONTEXT64 *PCNTCONTEXT64;


/**
 * [GI]DT descriptor.
 */
typedef struct NTKCONTEXTDESC64
{
    /** Alignment. */
    uint16_t                    au16Alignment[3];
    /** Limit. */
    uint16_t                    u16Limit;
    /** Base address. */
    uint64_t                    u64PtrBase;
} NTKCONTEXTDESC64;
AssertCompileSize(NTKCONTEXTDESC64, 2 * 8);
/** Pointer to an amd64 NT context. */
typedef NTKCONTEXTDESC64 *PNTKCONTEXTDESC64;
/** Pointer to a const amd64 NT context. */
typedef const NTKCONTEXTDESC64 *PCNTKCONTEXTDESC64;


/**
 * Kernel context as queried by KD_PACKET_MANIPULATE_REQ_READ_CTRL_SPACE
 */
typedef struct NTKCONTEXT64
{
    /** CR0 register. */
    uint64_t                    u64RegCr0;
    /** CR2 register. */
    uint64_t                    u64RegCr2;
    /** CR3 register. */
    uint64_t                    u64RegCr3;
    /** CR4 register. */
    uint64_t                    u64RegCr4;
    /** DR0 register. */
    uint64_t                    u64RegDr0;
    /** DR1 register. */
    uint64_t                    u64RegDr1;
    /** DR2 register. */
    uint64_t                    u64RegDr2;
    /** DR3 register. */
    uint64_t                    u64RegDr3;
    /** DR6 register. */
    uint64_t                    u64RegDr6;
    /** DR7 register. */
    uint64_t                    u64RegDr7;
    /** GDTR. */
    NTKCONTEXTDESC64            Gdtr;
    /** IDTR. */
    NTKCONTEXTDESC64            Idtr;
    /** TR register. */
    uint16_t                    u16RegTr;
    /** LDTR register. */
    uint16_t                    u16RegLdtr;
    /** MXCSR register. */
    uint32_t                    u32RegMxCsr;
    /** Debug control. */
    uint64_t                    u64DbgCtrl;
    /** @todo */
    uint64_t                    u64LastBrToRip;
    uint64_t                    u64LastBrFromRip;
    uint64_t                    u64LastExcpToRip;
    uint64_t                    u64LastExcpFromRip;
    /** CR8 register. */
    uint64_t                    u64RegCr8;
    /** GS base MSR register. */
    uint64_t                    u64MsrGsBase;
    /** Kernel GS base MSR register. */
    uint64_t                    u64MsrKernelGsBase;
    /** STAR MSR register. */
    uint64_t                    u64MsrStar;
    /** LSTAR MSR register. */
    uint64_t                    u64MsrLstar;
    /** CSTAR MSR register. */
    uint64_t                    u64MsrCstar;
    /** SFMASK MSR register. */
    uint64_t                    u64MsrSfMask;
    /** XCR0 register. */
    uint64_t                    u64RegXcr0;
    /** Standard context. */
    NTCONTEXT64                 Ctx;
} NTKCONTEXT64;
/** Pointer to an amd64 NT context. */
typedef NTKCONTEXT64 *PNTKCONTEXT64;
/** Pointer to a const amd64 NT context. */
typedef const NTKCONTEXT64 *PCNTKCONTEXT64;


/** x86 context. */
#define NTCONTEXT_F_X86                             UINT32_C(0x00010000)
/** AMD64 context. */
#define NTCONTEXT_F_AMD64                           UINT32_C(0x00100000)
/** Control registers valid (CS, (R)SP, (R)IP, FLAGS and BP). */
#define NTCONTEXT_F_CONTROL                         RT_BIT_32(0)
/** Integer registers valid. */
#define NTCONTEXT_F_INTEGER                         RT_BIT_32(1)
/** Segment registers valid. */
#define NTCONTEXT_F_SEGMENTS                        RT_BIT_32(2)
/** Floating point registers valid. */
#define NTCONTEXT_F_FLOATING_POINT                  RT_BIT_32(3)
/** Debug registers valid. */
#define NTCONTEXT_F_DEBUG                           RT_BIT_32(4)
/** Extended registers valid (x86 only). */
#define NTCONTEXT_F_EXTENDED                        RT_BIT_32(5)
/** Full x86 context valid. */
#define NTCONTEXT32_F_FULL (NTCONTEXT_F_X86 | NTCONTEXT_F_CONTROL | NTCONTEXT_F_INTEGER | NTCONTEXT_F_SEGMENTS)
/** Full amd64 context valid. */
#define NTCONTEXT64_F_FULL (NTCONTEXT_F_AMD64 | NTCONTEXT_F_CONTROL | NTCONTEXT_F_INTEGER | NTCONTEXT_F_SEGMENTS)


/**
 * 32bit exception record.
 */
typedef struct KDPACKETEXCP32
{
    /** The exception code identifying the excpetion. */
    uint32_t                    u32ExcpCode;
    /** Flags associated with the exception. */
    uint32_t                    u32ExcpFlags;
    /** Pointer to a chained exception record. */
    uint32_t                    u32PtrExcpRecNested;
    /** Address where the exception occurred. */
    uint32_t                    u32PtrExcpAddr;
    /** Number of parameters in the exception information array. */
    uint32_t                    cExcpParms;
    /** Exception parameters array. */
    uint32_t                    au32ExcpParms[KDPACKETEXCP_PARMS_MAX];
} KDPACKETEXCP32;
AssertCompileSize(KDPACKETEXCP32, 80);
/** Pointer to an exception record. */
typedef KDPACKETEXCP32 *PKDPACKETEXCP32;
/** Pointer to a const exception record. */
typedef const KDPACKETEXCP32 *PCKDPACKETEXCP32;


/** @name Exception codes.
 * @{ */
/** A breakpoint was hit. */
#define KD_PACKET_EXCP_CODE_BKPT                    UINT32_C(0x80000003)
/** An instruction was single stepped. */
#define KD_PACKET_EXCP_CODE_SINGLE_STEP             UINT32_C(0x80000004)
/** @} */


/** Maximum number of bytes in the instruction stream. */
#define KD_PACKET_CTRL_REPORT_INSN_STREAM_MAX       16

/**
 * 64bit control report record.
 */
typedef struct KDPACKETCTRLREPORT64
{
    /** Value of DR6. */
    uint64_t                    u64RegDr6;
    /** Value of DR7. */
    uint64_t                    u64RegDr7;
    /** EFLAGS. */
    uint32_t                    u32RegEflags;
    /** Number of instruction bytes in the instruction stream. */
    uint16_t                    cbInsnStream;
    /** Report flags. */
    uint16_t                    fFlags;
    /** The instruction stream. */
    uint8_t                     abInsn[KD_PACKET_CTRL_REPORT_INSN_STREAM_MAX];
    /** CS selector. */
    uint16_t                    u16SegCs;
    /** DS selector. */
    uint16_t                    u16SegDs;
    /** ES selector. */
    uint16_t                    u16SegEs;
    /** FS selector. */
    uint16_t                    u16SegFs;
} KDPACKETCTRLREPORT64;
AssertCompileSize(KDPACKETCTRLREPORT64, 2 * 8 + 4 + 2 * 2 + 16 + 4 * 2);
/** Pointer to a control report record. */
typedef KDPACKETCTRLREPORT64 *PKDPACKETCTRLREPORT64;
/** Pointer to a const control report record. */
typedef const KDPACKETCTRLREPORT64 *PCKDPACKETCTRLREPORT64;


/**
 * 64bit state change packet body.
 */
typedef struct KDPACKETSTATECHANGE64
{
    /** The new state. */
    uint32_t                    u32StateNew;
    /** The processor level. */
    uint16_t                    u16CpuLvl;
    /** The processor ID generating the state change. */
    uint16_t                    idCpu;
    /** Number of processors in the system. */
    uint32_t                    cCpus;
    /** Alignment. */
    uint32_t                    u32Alignment;
    /** The thread ID currently executing when the state change occurred. */
    uint64_t                    idThread;
    /** Program counter of the thread. */
    uint64_t                    u64RipThread;
    /** Data based on the state. */
    union
    {
        /** Exception occurred data. */
        struct
        {
            /** The exception record. */
            KDPACKETEXCP64      ExcpRec;
            /** First chance(?). */
            uint32_t            u32FirstChance;
        } Exception;
    } u;
    /** The control report */
    union
    {
        /** AMD64 control report. */
        KDPACKETCTRLREPORT64    Amd64;
    } uCtrlReport;
} KDPACKETSTATECHANGE64;
//AssertCompileSize(KDPACKETSTATECHANGE64, 4 + 2 * 2 + 2 * 4 + 2 * 8 + sizeof(KDPACKETEXCP64) + 4 + sizeof(KDPACKETCTRLREPORT64));
/** Pointer to a 64bit state change packet body. */
typedef KDPACKETSTATECHANGE64 *PKDPACKETSTATECHANGE64;
/** Pointer to a const 64bit state change packet body. */
typedef const KDPACKETSTATECHANGE64 *PCKDPACKETSTATECHANGE64;


/** @name State change state types.
 * @{ */
/** Minimum state change type. */
#define KD_PACKET_STATE_CHANGE_MIN                  UINT32_C(0x00003030)
/** An exception occured. */
#define KD_PACKET_STATE_CHANGE_EXCEPTION            KD_PACKET_STATE_CHANGE_MIN
/** Symbols were loaded(?). */
#define KD_PACKET_STATE_CHANGE_LOAD_SYMBOLS         UINT32_C(0x00003031)
/** Command string (custom command was executed?). */
#define KD_PACKET_STATE_CHANGE_CMD_STRING           UINT32_C(0x00003032)
/** Maximum state change type (exclusive). */
#define KD_PACKET_STATE_CHANGE_MAX                  UINT32_C(0x00003033)
/** @} */


/**
 * 64bit get version manipulate payload.
 */
typedef struct KDPACKETMANIPULATE_GETVERSION64
{
    /** Major version. */
    uint16_t                    u16VersMaj;
    /** Minor version. */
    uint16_t                    u16VersMin;
    /** Protocol version. */
    uint8_t                     u8VersProtocol;
    /** KD secondary version. */
    uint8_t                     u8VersKdSecondary;
    /** Flags. */
    uint16_t                    fFlags;
    /** Machine type. */
    uint16_t                    u16MachineType;
    /** Maximum packet type. */
    uint8_t                     u8MaxPktType;
    /** Maximum state change */
    uint8_t                     u8MaxStateChange;
    /** Maximum manipulate request ID. */
    uint8_t                     u8MaxManipulate;
    /** Some simulation flag. */
    uint8_t                     u8Simulation;
    /** Padding. */
    uint16_t                    u16Padding;
    /** Kernel base. */
    uint64_t                    u64PtrKernBase;
    /** Pointer of the loaded module list head. */
    uint64_t                    u64PtrPsLoadedModuleList;
    /** Pointer of the debugger data list. */
    uint64_t                    u64PtrDebuggerDataList;
} KDPACKETMANIPULATE_GETVERSION64;
AssertCompileSize(KDPACKETMANIPULATE_GETVERSION64, 8 * 2 + 3 * 8);
/** Pointer to a 64bit get version manipulate payload. */
typedef KDPACKETMANIPULATE_GETVERSION64 *PKDPACKETMANIPULATE_GETVERSION64;
/** Pointer to a const 64bit get version manipulate payload. */
typedef const KDPACKETMANIPULATE_GETVERSION64 *PCKDPACKETMANIPULATE_GETVERSION64;


/**
 * 64bit memory transfer manipulate payload.
 */
typedef struct KDPACKETMANIPULATE_XFERMEM64
{
    /** Target base address. */
    uint64_t                    u64PtrTarget;
    /** Requested number of bytes to transfer*/
    uint32_t                    cbXferReq;
    /** Number of bytes actually transferred (response). */
    uint32_t                    cbXfered;
    /** Some padding?. */
    uint64_t                    au64Pad[3];
} KDPACKETMANIPULATE_XFERMEM64;
AssertCompileSize(KDPACKETMANIPULATE_XFERMEM64, 4 * 8 + 2 * 4);
/** Pointer to a 64bit memory transfer manipulate payload. */
typedef KDPACKETMANIPULATE_XFERMEM64 *PKDPACKETMANIPULATE_XFERMEM64;
/** Pointer to a const 64bit memory transfer manipulate payload. */
typedef const KDPACKETMANIPULATE_XFERMEM64 *PCKDPACKETMANIPULATE_XFERMEM64;


/**
 * 64bit control space transfer manipulate payload.
 *
 * @note Same layout as the memory transfer but the pointer has a different meaning so
 *       we moved it into a separate request structure.
 */
typedef struct KDPACKETMANIPULATE_XFERCTRLSPACE64
{
    /** Identifier of the item to transfer in the control space. */
    uint64_t                    u64IdXfer;
    /** Requested number of bytes to transfer*/
    uint32_t                    cbXferReq;
    /** Number of bytes actually transferred (response). */
    uint32_t                    cbXfered;
    /** Some padding?. */
    uint64_t                    au64Pad[3];
} KDPACKETMANIPULATE_XFERCTRLSPACE64;
AssertCompileSize(KDPACKETMANIPULATE_XFERCTRLSPACE64, 4 * 8 + 2 * 4);
/** Pointer to a 64bit memory transfer manipulate payload. */
typedef KDPACKETMANIPULATE_XFERCTRLSPACE64 *PKDPACKETMANIPULATE_XFERCTRLSPACE64;
/** Pointer to a const 64bit memory transfer manipulate payload. */
typedef const KDPACKETMANIPULATE_XFERCTRLSPACE64 *PCKDPACKETMANIPULATE_XFERCTRLSPACE64;


/** @name Known control space identifiers.
 * @{ */
/** Read/Write KPCR address. */
#define KD_PACKET_MANIPULATE64_CTRL_SPACE_ID_KPCR   UINT64_C(0)
/** Read/Write KPCRB address. */
#define KD_PACKET_MANIPULATE64_CTRL_SPACE_ID_KPCRB  UINT64_C(1)
/** Read/Write Kernel context. */
#define KD_PACKET_MANIPULATE64_CTRL_SPACE_ID_KCTX   UINT64_C(2)
/** Read/Write current kernel thread. */
#define KD_PACKET_MANIPULATE64_CTRL_SPACE_ID_KTHRD  UINT64_C(3)
/** @} */


/**
 * 64bit restore breakpoint manipulate payload.
 */
typedef struct KDPACKETMANIPULATE_RESTOREBKPT64
{
    /** The breakpoint handle to restore. */
    uint32_t                    u32HndBkpt;
} KDPACKETMANIPULATE_RESTOREBKPT64;
AssertCompileSize(KDPACKETMANIPULATE_RESTOREBKPT64, 4);
/** Pointer to a 64bit restore breakpoint manipulate payload. */
typedef KDPACKETMANIPULATE_RESTOREBKPT64 *PKDPACKETMANIPULATE_RESTOREBKPT64;
/** Pointer to a const 64bit restore breakpoint manipulate payload. */
typedef const KDPACKETMANIPULATE_RESTOREBKPT64 *PCKDPACKETMANIPULATE_RESTOREBKPT64;


/**
 * context extended manipulate payload.
 */
typedef struct KDPACKETMANIPULATE_CONTEXTEX
{
    /** Where to start copying the context. */
    uint32_t                    offStart;
    /** Number of bytes to transfer. */
    uint32_t                    cbXfer;
    /** Number of bytes actually transfered. */
    uint32_t                    cbXfered;
} KDPACKETMANIPULATE_CONTEXTEX;
AssertCompileSize(KDPACKETMANIPULATE_CONTEXTEX, 3 * 4);
/** Pointer to a context extended manipulate payload. */
typedef KDPACKETMANIPULATE_CONTEXTEX *PKDPACKETMANIPULATE_CONTEXTEX;
/** Pointer to a const context extended manipulate payload. */
typedef const KDPACKETMANIPULATE_CONTEXTEX *PCKDPACKETMANIPULATE_CONTEXTEX;


/**
 * Manipulate request packet header (Same for 32bit and 64bit).
 */
typedef struct KDPACKETMANIPULATEHDR
{
    /** The request to execute. */
    uint32_t                    idReq;
    /** The processor level to execute the request on. */
    uint16_t                    u16CpuLvl;
    /** The processor ID to execute the request on. */
    uint16_t                    idCpu;
    /** Return status code. */
    uint32_t                    u32NtStatus;
    /** Alignment. */
    uint32_t                    u32Alignment;
} KDPACKETMANIPULATEHDR;
AssertCompileSize(KDPACKETMANIPULATEHDR, 3 * 4 + 2 * 2);
/** Pointer to a manipulate request packet header. */
typedef KDPACKETMANIPULATEHDR *PKDPACKETMANIPULATEHDR;
/** Pointer to a const manipulate request packet header. */
typedef const KDPACKETMANIPULATEHDR *PCPKDPACKETMANIPULATEHDR;


/**
 * 64bit manipulate state request packet.
 */
typedef struct KDPACKETMANIPULATE64
{
    /** Header. */
    KDPACKETMANIPULATEHDR                  Hdr;
    /** Request payloads. */
    union
    {
        /** Get Version. */
        KDPACKETMANIPULATE_GETVERSION64    GetVersion;
        /** Read/Write memory. */
        KDPACKETMANIPULATE_XFERMEM64       XferMem;
        /** Read/Write control space. */
        KDPACKETMANIPULATE_XFERCTRLSPACE64 XferCtrlSpace;
        /** Restore breakpoint. */
        KDPACKETMANIPULATE_RESTOREBKPT64   RestoreBkpt;
        /** Context extended. */
        KDPACKETMANIPULATE_CONTEXTEX       ContextEx;
    } u;
} KDPACKETMANIPULATE64;
/** Pointer to a 64bit manipulate state request packet. */
typedef KDPACKETMANIPULATE64 *PKDPACKETMANIPULATE64;
/** Pointer to a const 64bit manipulate state request packet. */
typedef const KDPACKETMANIPULATE64 *PCKDPACKETMANIPULATE64;

/** @name Manipulate requests.
 * @{ */
/** Minimum available request. */
#define KD_PACKET_MANIPULATE_REQ_MIN                        UINT32_C(0x00003130)
/** Read virtual memory request. */
#define KD_PACKET_MANIPULATE_REQ_READ_VIRT_MEM              KD_PACKET_MANIPULATE_REQ_MIN
/** Write virtual memory request. */
#define KD_PACKET_MANIPULATE_REQ_WRITE_VIRT_MEM             UINT32_C(0x00003131)
/** Get context request. */
#define KD_PACKET_MANIPULATE_REQ_GET_CONTEXT                UINT32_C(0x00003132)
/** Set context request. */
#define KD_PACKET_MANIPULATE_REQ_SET_CONTEXT                UINT32_C(0x00003133)
/** Write breakpoint request. */
#define KD_PACKET_MANIPULATE_REQ_WRITE_BKPT                 UINT32_C(0x00003134)
/** Restore breakpoint request. */
#define KD_PACKET_MANIPULATE_REQ_RESTORE_BKPT               UINT32_C(0x00003135)
/** Continue request. */
#define KD_PACKET_MANIPULATE_REQ_CONTINUE                   UINT32_C(0x00003136)
/** Read control space request. */
#define KD_PACKET_MANIPULATE_REQ_READ_CTRL_SPACE            UINT32_C(0x00003137)
/** Write control space request. */
#define KD_PACKET_MANIPULATE_REQ_WRITE_CTRL_SPACE           UINT32_C(0x00003138)
/** Read I/O space request. */
#define KD_PACKET_MANIPULATE_REQ_READ_IO_SPACE              UINT32_C(0x00003139)
/** Write I/O space request. */
#define KD_PACKET_MANIPULATE_REQ_WRITE_IO_SPACE             UINT32_C(0x0000313a)
/** Reboot request. */
#define KD_PACKET_MANIPULATE_REQ_REBOOT                     UINT32_C(0x0000313b)
/** continue 2nd version request. */
#define KD_PACKET_MANIPULATE_REQ_CONTINUE2                  UINT32_C(0x0000313c)
/** Read physical memory request. */
#define KD_PACKET_MANIPULATE_REQ_READ_PHYS_MEM              UINT32_C(0x0000313d)
/** Write physical memory request. */
#define KD_PACKET_MANIPULATE_REQ_WRITE_PHYS_MEM             UINT32_C(0x0000313e)
/** Query special calls request. */
#define KD_PACKET_MANIPULATE_REQ_QUERY_SPEC_CALLS           UINT32_C(0x0000313f)
/** Set special calls request. */
#define KD_PACKET_MANIPULATE_REQ_SET_SPEC_CALLS             UINT32_C(0x00003140)
/** Clear special calls request. */
#define KD_PACKET_MANIPULATE_REQ_CLEAR_SPEC_CALLS           UINT32_C(0x00003141)
/** Set internal breakpoint request. */
#define KD_PACKET_MANIPULATE_REQ_SET_INTERNAL_BKPT          UINT32_C(0x00003142)
/** Get internal breakpoint request. */
#define KD_PACKET_MANIPULATE_REQ_GET_INTERNAL_BKPT          UINT32_C(0x00003143)
/** Read I/O space extended request. */
#define KD_PACKET_MANIPULATE_REQ_READ_IO_SPACE_EX           UINT32_C(0x00003144)
/** Write I/O space extended request. */
#define KD_PACKET_MANIPULATE_REQ_WRITE_IO_SPACE_EX          UINT32_C(0x00003145)
/** Get version request. */
#define KD_PACKET_MANIPULATE_REQ_GET_VERSION                UINT32_C(0x00003146)
/** @todo */
/** Clear all internal breakpoints request. */
#define KD_PACKET_MANIPULATE_REQ_CLEAR_ALL_INTERNAL_BKPT    UINT32_C(0x0000315a)
/** @todo */
/** Get context extended request. */
#define KD_PACKET_MANIPULATE_REQ_GET_CONTEXT_EX             UINT32_C(0x0000315f)
/** @todo */
/** Maximum available request (exclusive). */
#define KD_PACKET_MANIPULATE_REQ_MAX                        UINT32_C(0x00003161)
/** @} */

/**
 * KD stub receive state.
 */
typedef enum KDRECVSTATE
{
    /** Invalid state. */
    KDRECVSTATE_INVALID = 0,
    /** Receiving the first byte of the packet header. */
    KDRECVSTATE_PACKET_HDR_FIRST_BYTE,
    /** Receiving the second byte of the packet header. */
    KDRECVSTATE_PACKET_HDR_SECOND_BYTE,
    /** Receiving the header. */
    KDRECVSTATE_PACKET_HDR,
    /** Receiving the packet body. */
    KDRECVSTATE_PACKET_BODY,
    /** Receiving the trailing byte. */
    KDRECVSTATE_PACKET_TRAILER,
    /** Blow up the enum to 32bits for easier alignment of members in structs. */
    KDRECVSTATE_32BIT_HACK = 0x7fffffff
} KDRECVSTATE;


/**
 * KD context data.
 */
typedef struct KDCTX
{
    /** Internal debugger console data. */
    DBGC                        Dbgc;
    /** Number of bytes received left for the current state. */
    size_t                      cbRecvLeft;
    /** Pointer where to write the next received data. */
    uint8_t                     *pbRecv;
    /** The current state when receiving a new packet. */
    KDRECVSTATE                 enmState;
    /** The timeout waiting for new data. */
    RTMSINTERVAL                msRecvTimeout;
    /** Timestamp when we last received data from the remote end. */
    uint64_t                    tsRecvLast;
    /** Packet header being received. */
    union
    {
        KDPACKETHDR             Fields;
        uint8_t                 ab[16];
    } PktHdr;
    /** The next packet ID to send. */
    uint32_t                    idPktNext;
    /** Offset into the body receive buffer. */
    size_t                      offBodyRecv;
    /** Body data. */
    uint8_t                     abBody[_4K];
    /** The trailer byte storage. */
    uint8_t                     bTrailer;
    /** Flag whether a breakin packet was received since the last time it was reset. */
    bool                        fBreakinRecv;

    /** Pointer to the OS digger WinNt interface if a matching guest was detected. */
    PDBGFOSIWINNT               pIfWinNt;
} KDCTX;
/** Pointer to the KD context data. */
typedef KDCTX *PKDCTX;
/** Pointer to const KD context data. */
typedef const KDCTX *PCKDCTX;
/** Pointer to a KD context data pointer. */
typedef PKDCTX *PPKDCTX;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


#ifdef LOG_ENABLED
/**
 * Returns a human readable string of the given packet sub type.
 *
 * @returns Pointer to sub type string.
 * @param   u16SubType          The sub type to convert to a string.
 */
static const char *dbgcKdPktDumpSubTypeToStr(uint16_t u16SubType)
{
    switch (u16SubType)
    {
        case KD_PACKET_HDR_SUB_TYPE_STATE_CHANGE32:     return "StateChange32";
        case KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE:   return "Manipulate";
        case KD_PACKET_HDR_SUB_TYPE_DEBUG_IO:           return "DebugIo";
        case KD_PACKET_HDR_SUB_TYPE_ACKNOWLEDGE:        return "Ack";
        case KD_PACKET_HDR_SUB_TYPE_RESEND:             return "Resend";
        case KD_PACKET_HDR_SUB_TYPE_RESET:              return "Reset";
        case KD_PACKET_HDR_SUB_TYPE_STATE_CHANGE64:     return "StateChange64";
        case KD_PACKET_HDR_SUB_TYPE_POLL_BREAKIN:       return "PollBreakin";
        case KD_PACKET_HDR_SUB_TYPE_TRACE_IO:           return "TraceIo";
        case KD_PACKET_HDR_SUB_TYPE_CONTROL_REQUEST:    return "ControlRequest";
        case KD_PACKET_HDR_SUB_TYPE_FILE_IO:            return "FileIo";
        default:                                        break;
    }

    return "<UNKNOWN>";
}


/**
 * Returns a human readable string of the given manipulate request ID.
 *
 * @returns nothing.
 * @param   idReq               Request ID (API number in KD speak).
 */
static const char *dbgcKdPktDumpManipulateReqToStr(uint32_t idReq)
{
    switch (idReq)
    {
        case KD_PACKET_MANIPULATE_REQ_READ_VIRT_MEM:            return "ReadVirtMem";
        case KD_PACKET_MANIPULATE_REQ_WRITE_VIRT_MEM:           return "WriteVirtMem";
        case KD_PACKET_MANIPULATE_REQ_GET_CONTEXT:              return "GetContext";
        case KD_PACKET_MANIPULATE_REQ_SET_CONTEXT:              return "SetContext";
        case KD_PACKET_MANIPULATE_REQ_WRITE_BKPT:               return "WriteBkpt";
        case KD_PACKET_MANIPULATE_REQ_RESTORE_BKPT:             return "RestoreBkpt";
        case KD_PACKET_MANIPULATE_REQ_CONTINUE:                 return "Continue";
        case KD_PACKET_MANIPULATE_REQ_READ_CTRL_SPACE:          return "ReadCtrlSpace";
        case KD_PACKET_MANIPULATE_REQ_WRITE_CTRL_SPACE:         return "WriteCtrlSpace";
        case KD_PACKET_MANIPULATE_REQ_READ_IO_SPACE:            return "ReadIoSpace";
        case KD_PACKET_MANIPULATE_REQ_WRITE_IO_SPACE:           return "WriteIoSpace";
        case KD_PACKET_MANIPULATE_REQ_REBOOT:                   return "Reboot";
        case KD_PACKET_MANIPULATE_REQ_CONTINUE2:                return "Continue2";
        case KD_PACKET_MANIPULATE_REQ_READ_PHYS_MEM:            return "ReadPhysMem";
        case KD_PACKET_MANIPULATE_REQ_WRITE_PHYS_MEM:           return "WritePhysMem";
        case KD_PACKET_MANIPULATE_REQ_QUERY_SPEC_CALLS:         return "QuerySpecCalls";
        case KD_PACKET_MANIPULATE_REQ_SET_SPEC_CALLS:           return "SetSpecCalls";
        case KD_PACKET_MANIPULATE_REQ_CLEAR_SPEC_CALLS:         return "ClrSpecCalls";
        case KD_PACKET_MANIPULATE_REQ_SET_INTERNAL_BKPT:        return "SetIntBkpt";
        case KD_PACKET_MANIPULATE_REQ_GET_INTERNAL_BKPT:        return "GetIntBkpt";
        case KD_PACKET_MANIPULATE_REQ_READ_IO_SPACE_EX:         return "ReadIoSpaceEx";
        case KD_PACKET_MANIPULATE_REQ_WRITE_IO_SPACE_EX:        return "WriteIoSpaceEx";
        case KD_PACKET_MANIPULATE_REQ_GET_VERSION:              return "GetVersion";
        case KD_PACKET_MANIPULATE_REQ_CLEAR_ALL_INTERNAL_BKPT:  return "ClrAllIntBkpt";
        case KD_PACKET_MANIPULATE_REQ_GET_CONTEXT_EX:           return "GetContextEx";
        default:                                                break;
    }

    return "<UNKNOWN>";
}


/**
 * Dumps the content of a manipulate packet.
 *
 * @returns nothing.
 * @param   pSgBuf              S/G buffer containing the manipulate packet payload.
 */
static void dbgcKdPktDumpManipulate(PRTSGBUF pSgBuf)
{
    KDPACKETMANIPULATEHDR Hdr;
    size_t cbCopied = RTSgBufCopyToBuf(pSgBuf, &Hdr, sizeof(Hdr));

    if (cbCopied == sizeof(Hdr))
    {
        const char *pszReq = dbgcKdPktDumpManipulateReqToStr(Hdr.idReq);

        Log3(("    MANIPULATE(%#x (%s), %#x, %u, %#x)\n",
              Hdr.idReq, pszReq, Hdr.u16CpuLvl, Hdr.idCpu, Hdr.u32NtStatus));

        switch (Hdr.idReq)
        {
            case KD_PACKET_MANIPULATE_REQ_READ_VIRT_MEM:
            case KD_PACKET_MANIPULATE_REQ_WRITE_VIRT_MEM:
            case KD_PACKET_MANIPULATE_REQ_READ_PHYS_MEM:
            case KD_PACKET_MANIPULATE_REQ_WRITE_PHYS_MEM:
            {
                KDPACKETMANIPULATE_XFERMEM64 XferMem64;
                cbCopied = RTSgBufCopyToBuf(pSgBuf, &XferMem64, sizeof(XferMem64));
                if (cbCopied == sizeof(XferMem64))
                {
                    Log3(("        u64PtrTarget: %RX64\n"
                          "        cbXferReq:    %RX32\n"
                          "        cbXfered:     %RX32\n",
                          XferMem64.u64PtrTarget, XferMem64.cbXferReq, XferMem64.cbXfered));
                }
                else
                    Log3(("        Payload to small, expected %u, got %zu\n", sizeof(XferMem64), cbCopied));
                break;
            }
            case KD_PACKET_MANIPULATE_REQ_READ_CTRL_SPACE:
            case KD_PACKET_MANIPULATE_REQ_WRITE_CTRL_SPACE:
            {
                KDPACKETMANIPULATE_XFERCTRLSPACE64 XferCtrlSpace64;
                cbCopied = RTSgBufCopyToBuf(pSgBuf, &XferCtrlSpace64, sizeof(XferCtrlSpace64));
                if (cbCopied == sizeof(XferCtrlSpace64))
                {
                    Log3(("        u64IdXfer:    %RX64\n"
                          "        cbXferReq:    %RX32\n"
                          "        cbXfered:     %RX32\n",
                          XferCtrlSpace64.u64IdXfer, XferCtrlSpace64.cbXferReq, XferCtrlSpace64.cbXfered));
                }
                else
                    Log3(("        Payload to small, expected %u, got %zu\n", sizeof(XferCtrlSpace64), cbCopied));
                break;
            }
            default:
                break;
        }
    }
    else
        Log3(("    MANIPULATE(Header too small, expected %u, got %zu)\n", sizeof(Hdr), cbCopied));
}


/**
 * Dumps the received packet to the debug log.
 *
 * @returns VBox status code.
 * @param   pPktHdr             The packet header to dump.
 * @param   fRx                 Flag whether the packet was received (false indicates an outgoing packet).
 */
static void dbgcKdPktDump(PCKDPACKETHDR pPktHdr, PCRTSGSEG paSegs, uint32_t cSegs, bool fRx)
{
    RTSGBUF SgBuf;

    RTSgBufInit(&SgBuf, paSegs, cSegs);

    Log3(("%s KDPKTHDR(%#x, %#x (%s), %u, %#x, %#x)\n",
          fRx ? "=>" : "<=",
          pPktHdr->u32Signature, pPktHdr->u16SubType, dbgcKdPktDumpSubTypeToStr(pPktHdr->u16SubType),
          pPktHdr->cbBody, pPktHdr->idPacket, pPktHdr->u32ChkSum));
    switch (pPktHdr->u16SubType)
    {
        case KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE:
            dbgcKdPktDumpManipulate(&SgBuf);
            break;
        default:
            break;
    }
}
#endif


/**
 * Wrapper for the I/O interface write callback.
 *
 * @returns Status code.
 * @param   pThis               The KD context.
 * @param   pvPkt               The packet data to send.
 * @param   cbPkt               Size of the packet in bytes.
 */
DECLINLINE(int) dbgcKdCtxWrite(PKDCTX pThis, const void *pvPkt, size_t cbPkt)
{
    return pThis->Dbgc.pBack->pfnWrite(pThis->Dbgc.pBack, pvPkt, cbPkt, NULL /*pcbWritten*/);
}


/**
 * Fills in the given 64bit NT context structure with the requested values.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   idCpu               The CPU to query the context for.
 * @param   pNtCtx              The NT context structure to fill in.
 * @param   pCtxFlags           Combination of NTCONTEXT_F_XXX determining what to fill in.
 */
static int dbgcKdCtxQueryNtCtx64(PKDCTX pThis, VMCPUID idCpu, PNTCONTEXT64 pNtCtx, uint32_t fCtxFlags)
{
    RT_BZERO(pNtCtx, sizeof(*pNtCtx));

    pNtCtx->fContext = NTCONTEXT_F_AMD64;
    int rc = DBGFR3RegCpuQueryU32(pThis->Dbgc.pUVM, idCpu, DBGFREG_MXCSR, &pNtCtx->u32RegMxCsr);

    if (   RT_SUCCESS(rc)
        && fCtxFlags & NTCONTEXT_F_CONTROL)
    {
        rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_CS, &pNtCtx->u16SegCs);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_SS, &pNtCtx->u16SegSs);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RIP, &pNtCtx->u64RegRip);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RSP, &pNtCtx->u64RegRsp);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RBP, &pNtCtx->u64RegRbp);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU32(pThis->Dbgc.pUVM, idCpu, DBGFREG_EFLAGS, &pNtCtx->u32RegEflags);
        if (RT_SUCCESS(rc))
            pNtCtx->fContext |= NTCONTEXT_F_CONTROL;
    }

    if (   RT_SUCCESS(rc)
        && fCtxFlags & NTCONTEXT_F_INTEGER)
    {
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RAX, &pNtCtx->u64RegRax);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RCX, &pNtCtx->u64RegRcx);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RDX, &pNtCtx->u64RegRdx);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RBX, &pNtCtx->u64RegRbx);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RSI, &pNtCtx->u64RegRsi);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_RDI, &pNtCtx->u64RegRdi);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_R8, &pNtCtx->u64RegR8);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_R9, &pNtCtx->u64RegR9);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_R10, &pNtCtx->u64RegR10);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_R11, &pNtCtx->u64RegR11);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_R12, &pNtCtx->u64RegR12);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_R13, &pNtCtx->u64RegR13);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_R14, &pNtCtx->u64RegR14);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_R15, &pNtCtx->u64RegR15);
        if (RT_SUCCESS(rc))
            pNtCtx->fContext |= NTCONTEXT_F_INTEGER;
    }

    if (   RT_SUCCESS(rc)
        && fCtxFlags & NTCONTEXT_F_SEGMENTS)
    {
        rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_DS, &pNtCtx->u16SegDs);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_ES, &pNtCtx->u16SegEs);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_FS, &pNtCtx->u16SegFs);
        if (RT_SUCCESS(rc))
            rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_GS, &pNtCtx->u16SegGs);
        if (RT_SUCCESS(rc))
            pNtCtx->fContext |= NTCONTEXT_F_SEGMENTS;
    }

    if (   RT_SUCCESS(rc)
        && fCtxFlags & NTCONTEXT_F_FLOATING_POINT)
    {
        /** @todo. */
    }

    if (   RT_SUCCESS(rc)
        && fCtxFlags & NTCONTEXT_F_DEBUG)
    {
        /** @todo. */
    }

    return rc;
}


/**
 * Fills in the given 64bit NT kernel context structure with the requested values.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   idCpu               The CPU to query the context for.
 * @param   pNtCtx              The NT context structure to fill in.
 * @param   pCtxFlags           Combination of NTCONTEXT_F_XXX determining what to fill in.
 */
static int dbgcKdCtxQueryNtKCtx64(PKDCTX pThis, VMCPUID idCpu, PNTKCONTEXT64 pKNtCtx, uint32_t fCtxFlags)
{
    RT_BZERO(pKNtCtx, sizeof(*pKNtCtx));

    int rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_CR0, &pKNtCtx->u64RegCr0);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_CR2, &pKNtCtx->u64RegCr2);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_CR3, &pKNtCtx->u64RegCr3);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_CR4, &pKNtCtx->u64RegCr4);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_CR8, &pKNtCtx->u64RegCr8);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_DR0, &pKNtCtx->u64RegDr0);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_DR1, &pKNtCtx->u64RegDr1);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_DR2, &pKNtCtx->u64RegDr2);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_DR3, &pKNtCtx->u64RegDr3);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_DR6, &pKNtCtx->u64RegDr6);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_DR7, &pKNtCtx->u64RegDr7);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_GDTR_LIMIT, &pKNtCtx->Gdtr.u16Limit);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_GDTR_BASE, &pKNtCtx->Gdtr.u64PtrBase);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_IDTR_LIMIT, &pKNtCtx->Idtr.u16Limit);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_IDTR_BASE, &pKNtCtx->Idtr.u64PtrBase);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_TR, &pKNtCtx->u16RegTr);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, idCpu, DBGFREG_LDTR, &pKNtCtx->u16RegLdtr);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU32(pThis->Dbgc.pUVM, idCpu, DBGFREG_MXCSR, &pKNtCtx->u32RegMxCsr);
    /** @todo Debug control and stuff. */

    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_MSR_K8_GS_BASE, &pKNtCtx->u64MsrGsBase);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_MSR_K8_KERNEL_GS_BASE, &pKNtCtx->u64MsrKernelGsBase);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_MSR_K6_STAR, &pKNtCtx->u64MsrStar);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_MSR_K8_LSTAR, &pKNtCtx->u64MsrLstar);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_MSR_K8_CSTAR, &pKNtCtx->u64MsrCstar);
    if (RT_SUCCESS(rc))
        rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, idCpu, DBGFREG_MSR_K8_SF_MASK, &pKNtCtx->u64MsrSfMask);
    /** @todo XCR0 */

    if (RT_SUCCESS(rc))
        rc = dbgcKdCtxQueryNtCtx64(pThis, idCpu, &pKNtCtx->Ctx, fCtxFlags);

    return rc;
}


/**
 * Validates the given KD packet header.
 *
 * @returns Flag whether the packet header is valid, false if invalid.
 * @param   pPktHdr             The packet header to validate.
 */
static bool dbgcKdPktHdrValidate(PCKDPACKETHDR pPktHdr)
{
    if (   pPktHdr->u32Signature != KD_PACKET_HDR_SIGNATURE_DATA
        && pPktHdr->u32Signature != KD_PACKET_HDR_SIGNATURE_CONTROL
        && pPktHdr->u32Signature != KD_PACKET_HDR_SIGNATURE_BREAKIN)
        return false;

    if (pPktHdr->u16SubType >= KD_PACKET_HDR_SUB_TYPE_MAX)
        return false;

    uint32_t idPacket = pPktHdr->idPacket & UINT32_C(0xfffffffe);
    if (   idPacket != KD_PACKET_HDR_ID_INITIAL
        && idPacket != KD_PACKET_HDR_ID_RESET
        && idPacket != 0 /* Happens on the very first packet */)
        return false;

    return true;
}


/**
 * Generates a checksum from the given buffer.
 *
 * @returns Generated checksum.
 * @param   pv                  The data to generate a checksum from.
 * @param   cb                  Number of bytes to checksum.
 */
static uint32_t dbgcKdPktChkSumGen(const void *pv, size_t cb)
{
    const uint8_t *pb = (const uint8_t *)pv;
    uint32_t u32ChkSum = 0;

    while (cb--)
        u32ChkSum += *pb++;

    return u32ChkSum;
}


/**
 * Generates a checksum from the given segments.
 *
 * @returns Generated checksum.
 * @param   paSegs              Pointer to the array of segments containing the data.
 * @param   cSegs               Number of segments.
 * @param   pcbChkSum           Where to store the number of bytes checksummed, optional.
 */
static uint32_t dbgcKdPktChkSumGenSg(PCRTSGSEG paSegs, uint32_t cSegs, size_t *pcbChkSum)
{
    size_t cbChkSum = 0;
    uint32_t u32ChkSum = 0;

    for (uint32_t i = 0; i < cSegs; i++)
    {
        u32ChkSum += dbgcKdPktChkSumGen(paSegs[i].pvSeg, paSegs[i].cbSeg);
        cbChkSum += paSegs[i].cbSeg;
    }

    if (pcbChkSum)
        *pcbChkSum = cbChkSum;

    return u32ChkSum;
}


/**
 * Waits for an acknowledgment.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   msWait              Maximum number of milliseconds to wait for an acknowledge.
 * @param   pfResend            Where to store the resend requested flag on success.
 */
static int dbgcKdCtxPktWaitForAck(PKDCTX pThis, RTMSINTERVAL msWait, bool *pfResend)
{
    KDPACKETHDR PktAck;
    uint8_t *pbCur = (uint8_t *)&PktAck;
    size_t cbLeft = sizeof(PktAck);
    uint64_t tsStartMs = RTTimeMilliTS();
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pThis=%p msWait=%u pfResend=%p\n", pThis, msWait, pfResend));

    RT_ZERO(PktAck);

    /* There might be breakin packets in the queue, read until we get something else. */
    while (   msWait
           && RT_SUCCESS(rc))
    {
        if (pThis->Dbgc.pBack->pfnInput(pThis->Dbgc.pBack, msWait))
        {
            size_t cbRead = 0;
            rc = pThis->Dbgc.pBack->pfnRead(pThis->Dbgc.pBack, pbCur, 1, &cbRead);
            if (   RT_SUCCESS(rc)
                && cbRead == 1)
            {
                uint64_t tsSpanMs = RTTimeMilliTS() - tsStartMs;
                msWait -= RT_MIN(msWait, tsSpanMs);
                tsStartMs = RTTimeMilliTS();

                if (*pbCur == KD_PACKET_HDR_SIGNATURE_BREAKIN_BYTE)
                    pThis->fBreakinRecv = true;
                else
                {
                    pbCur++;
                    cbLeft--;
                    break;
                }
            }
        }
        else
            rc = VERR_TIMEOUT;
    }

    if (   RT_SUCCESS(rc)
        && !msWait)
        rc = VERR_TIMEOUT;

    if (RT_SUCCESS(rc))
    {
        while (   msWait
               && RT_SUCCESS(rc)
               && cbLeft)
        {
            if (pThis->Dbgc.pBack->pfnInput(pThis->Dbgc.pBack, msWait))
            {
                size_t cbRead = 0;
                rc = pThis->Dbgc.pBack->pfnRead(pThis->Dbgc.pBack, pbCur, cbLeft, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    uint64_t tsSpanMs = RTTimeMilliTS() - tsStartMs;
                    msWait -= RT_MIN(msWait, tsSpanMs);
                    tsStartMs = RTTimeMilliTS();

                    cbLeft -= cbRead;
                    pbCur  += cbRead;
                }
            }
            else
                rc = VERR_TIMEOUT;
        }

        if (RT_SUCCESS(rc))
        {
            if (PktAck.u32Signature == KD_PACKET_HDR_SIGNATURE_CONTROL)
            {
                if (PktAck.u16SubType == KD_PACKET_HDR_SUB_TYPE_ACKNOWLEDGE)
                    rc = VINF_SUCCESS;
                else if (PktAck.u16SubType == KD_PACKET_HDR_SUB_TYPE_RESEND)
                {
                    *pfResend = true;
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_NET_PROTOCOL_ERROR;
            }
            else
                rc = VERR_NET_PROTOCOL_ERROR;
        }
    }

    LogFlowFunc(("returns rc=%Rrc *pfResend=%RTbool\n", rc, *pfResend));
    return rc;
}


/**
 * Sends the given packet header and optional segmented body (the trailing byte is sent automatically).
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   u32Signature        The signature to send.
 * @param   u16SubType          The sub type to send.
 * @param   idPacket            Packet ID to send.
 * @param   paSegs              Pointer to the array of segments to send in the body, optional.
 * @param   cSegs               Number of segments.
 * @param   fAck                Flag whether to wait for an acknowledge.
 */
static int dbgcKdCtxPktSendSg(PKDCTX pThis, uint32_t u32Signature, uint16_t u16SubType,
                              PCRTSGSEG paSegs, uint32_t cSegs, bool fAck)
{
    int rc = VINF_SUCCESS;
    uint32_t cRetriesLeft = 3;
    uint8_t bTrailer = KD_PACKET_TRAILING_BYTE;
    KDPACKETHDR Hdr;

    size_t cbChkSum = 0;
    uint32_t u32ChkSum = dbgcKdPktChkSumGenSg(paSegs, cSegs, &cbChkSum);

    Hdr.u32Signature = u32Signature;
    Hdr.u16SubType   = u16SubType;
    Hdr.cbBody       = (uint16_t)cbChkSum;
    Hdr.idPacket     = pThis->idPktNext;
    Hdr.u32ChkSum    = u32ChkSum;

#ifdef LOG_ENABLED
    dbgcKdPktDump(&Hdr, paSegs, cSegs, false /*fRx*/);
#endif

    while (cRetriesLeft--)
    {
        bool fResend = false;

        rc = dbgcKdCtxWrite(pThis, &Hdr, sizeof(Hdr));
        if (   RT_SUCCESS(rc)
            && paSegs
            && cSegs)
        {
            for (uint32_t i = 0; i < cSegs && RT_SUCCESS(rc); i++)
                rc = dbgcKdCtxWrite(pThis, paSegs[i].pvSeg, paSegs[i].cbSeg);

            if (RT_SUCCESS(rc))
                rc = dbgcKdCtxWrite(pThis, &bTrailer, sizeof(bTrailer));
        }

        if (RT_SUCCESS(rc))
        {
            if (fAck)
                rc = dbgcKdCtxPktWaitForAck(pThis, 10 * 1000, &fResend);

            if (   RT_SUCCESS(rc)
                && !fResend)
                break;
        }
    }

    return rc;
}


/**
 * Sends the given packet header and optional body (the trailing byte is sent automatically).
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   u32Signature        The signature to send.
 * @param   u16SubType          The sub type to send.
 * @param   pvBody              The body to send, optional.
 * @param   cbBody              Body size in bytes.
 * @param   fAck                Flag whether to wait for an acknowledge.
 */
DECLINLINE(int) dbgcKdCtxPktSend(PKDCTX pThis, uint32_t u32Signature, uint16_t u16SubType,
                                 const void *pvBody, size_t cbBody,
                                 bool fAck)
{
    RTSGSEG Seg;

    Seg.pvSeg = (void *)pvBody;
    Seg.cbSeg = cbBody;
    return dbgcKdCtxPktSendSg(pThis, u32Signature, u16SubType, cbBody ? &Seg : NULL, cbBody ? 1 : 0, fAck);
}


/**
 * Sends a resend packet answer.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 */
DECLINLINE(int) dbgcKdCtxPktSendResend(PKDCTX pThis)
{
    return dbgcKdCtxPktSend(pThis, KD_PACKET_HDR_SIGNATURE_CONTROL, KD_PACKET_HDR_SUB_TYPE_RESEND,
                            NULL /*pvBody*/, 0 /*cbBody*/, false /*fAck*/);
}


/**
 * Sends a resend packet answer.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 */
DECLINLINE(int) dbgcKdCtxPktSendReset(PKDCTX pThis)
{
    pThis->idPktNext = KD_PACKET_HDR_ID_INITIAL;
    return dbgcKdCtxPktSend(pThis, KD_PACKET_HDR_SIGNATURE_CONTROL, KD_PACKET_HDR_SUB_TYPE_RESET,
                            NULL /*pvBody*/, 0 /*cbBody*/, false /*fAck*/);
}


/**
 * Sends an acknowledge packet answer.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 */
DECLINLINE(int) dbgcKdCtxPktSendAck(PKDCTX pThis)
{
    return dbgcKdCtxPktSend(pThis, KD_PACKET_HDR_SIGNATURE_CONTROL, KD_PACKET_HDR_SUB_TYPE_ACKNOWLEDGE,
                            NULL /*pvBody*/, 0 /*cbBody*/, false /*fAck*/);
}


/**
 * Resets the packet receive state machine.
 *
 * @returns nothing.
 * @param   pThis               The KD context.
 */
static void dbgcKdCtxPktRecvReset(PKDCTX pThis)
{
    pThis->enmState       = KDRECVSTATE_PACKET_HDR_FIRST_BYTE;
    pThis->pbRecv         = &pThis->PktHdr.ab[0];
    pThis->cbRecvLeft     = sizeof(pThis->PktHdr.ab[0]);
    pThis->msRecvTimeout  = RT_INDEFINITE_WAIT;
    pThis->tsRecvLast     = RTTimeMilliTS();
}


/**
 * Sends a state change event packet.
 *
 * @returns VBox status code.
 * @param   pThis   The KD context data.
 * @param   enmType The event type.
 */
static int dbgcKdCtxStateChangeSend(PKDCTX pThis, DBGFEVENTTYPE enmType)
{
    LogFlowFunc(("pThis=%p enmType=%u\n", pThis, enmType));

    /* Select the record to send based on the CPU mode. */
    int rc = VINF_SUCCESS;
    CPUMMODE enmMode = DBGCCmdHlpGetCpuMode(&pThis->Dbgc.CmdHlp);
    switch (enmMode)
    {
        case CPUMMODE_PROTECTED:
        {
            break;
        }
        case CPUMMODE_LONG:
        {
            KDPACKETSTATECHANGE64 StateChange64;
            RT_ZERO(StateChange64);

            StateChange64.u32StateNew = KD_PACKET_STATE_CHANGE_EXCEPTION;
            StateChange64.u16CpuLvl   = 0x6; /** @todo Figure this one out. */
            StateChange64.idCpu       = pThis->Dbgc.idCpu;
            StateChange64.cCpus       = (uint16_t)DBGFR3CpuGetCount(pThis->Dbgc.pUVM);
            rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, DBGFREG_RIP, &StateChange64.u64RipThread);
            if (RT_SUCCESS(rc))
            {
                /** @todo Properly fill in the exception record. */
                switch (enmType)
                {
                    case DBGFEVENT_HALT_DONE:
                    case DBGFEVENT_BREAKPOINT:
                    case DBGFEVENT_BREAKPOINT_IO:
                    case DBGFEVENT_BREAKPOINT_MMIO:
                    case DBGFEVENT_BREAKPOINT_HYPER:
                        StateChange64.u.Exception.ExcpRec.u32ExcpCode = KD_PACKET_EXCP_CODE_BKPT;
                        break;
                    case DBGFEVENT_STEPPED:
                    case DBGFEVENT_STEPPED_HYPER:
                        StateChange64.u.Exception.ExcpRec.u32ExcpCode = KD_PACKET_EXCP_CODE_SINGLE_STEP;
                        break;
                    default:
                        AssertMsgFailed(("Invalid DBGF event type for state change %d!\n", enmType));
                }

                StateChange64.u.Exception.ExcpRec.cExcpParms = 3;
                StateChange64.u.Exception.u32FirstChance     = 0x1;

                /** @todo Properly fill in the control report. */
                rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, DBGFREG_DR6, &StateChange64.uCtrlReport.Amd64.u64RegDr6);
                if (RT_SUCCESS(rc))
                    rc = DBGFR3RegCpuQueryU64(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, DBGFREG_DR7, &StateChange64.uCtrlReport.Amd64.u64RegDr7);
                if (RT_SUCCESS(rc))
                    rc = DBGFR3RegCpuQueryU32(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, DBGFREG_RFLAGS, &StateChange64.uCtrlReport.Amd64.u32RegEflags);
                if (RT_SUCCESS(rc))
                    rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, DBGFREG_CS, &StateChange64.uCtrlReport.Amd64.u16SegCs);
                if (RT_SUCCESS(rc))
                    rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, DBGFREG_DS, &StateChange64.uCtrlReport.Amd64.u16SegDs);
                if (RT_SUCCESS(rc))
                    rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, DBGFREG_ES, &StateChange64.uCtrlReport.Amd64.u16SegEs);
                if (RT_SUCCESS(rc))
                    rc = DBGFR3RegCpuQueryU16(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, DBGFREG_FS, &StateChange64.uCtrlReport.Amd64.u16SegFs);

                /* Read instruction bytes. */
                StateChange64.uCtrlReport.Amd64.cbInsnStream = sizeof(StateChange64.uCtrlReport.Amd64.abInsn);
                DBGFADDRESS AddrRip;
                DBGFR3AddrFromFlat(pThis->Dbgc.pUVM, &AddrRip, StateChange64.u64RipThread);
                rc = DBGFR3MemRead(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, &AddrRip,
                                   &StateChange64.uCtrlReport.Amd64.abInsn[0], StateChange64.uCtrlReport.Amd64.cbInsnStream);
                if (RT_SUCCESS(rc))
                {
                    pThis->idPktNext = KD_PACKET_HDR_ID_INITIAL;
                    rc = dbgcKdCtxPktSend(pThis, KD_PACKET_HDR_SIGNATURE_DATA, KD_PACKET_HDR_SUB_TYPE_STATE_CHANGE64,
                                          &StateChange64, sizeof(StateChange64), false /*fAck*/);
                }
            }
            break;
        }
        case CPUMMODE_REAL:
        default:
            return DBGCCmdHlpPrintf(&pThis->Dbgc.CmdHlp, "error: Invalid CPU mode %d.\n", enmMode);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * Processes a get version 64 request.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   pPktManip           The manipulate packet request.
 */
static int dbgcKdCtxPktManipulate64GetVersion(PKDCTX pThis, PCKDPACKETMANIPULATE64 pPktManip)
{
    KDPACKETMANIPULATE64 Resp;
    RT_ZERO(Resp);

    /* Fill in the generic part. */
    Resp.Hdr.idReq       = KD_PACKET_MANIPULATE_REQ_GET_VERSION;
    Resp.Hdr.u16CpuLvl   = pPktManip->Hdr.u16CpuLvl;
    Resp.Hdr.idCpu       = pPktManip->Hdr.idCpu;
    Resp.Hdr.u32NtStatus = NTSTATUS_SUCCESS;

#if 0
    /* Build our own response in case there is no Windows interface available. */
    if (pThis->pIfWinNt)
    {
        RTGCUINTPTR GCPtrKpcr = 0;

        int rc = pThis->pIfWinNt->pfnQueryKpcrForVCpu(pThis->pIfWinNt, pThis->Dbgc.pUVM, Resp.Hdr.idCpu,
                                                      &GCPtrKpcr, NULL /*pKpcrb*/);
        if (RT_SUCCESS(rc))
        {
            DBGFADDRESS AddrKdVersionBlock;
            DBGFR3AddrFromFlat(pThis->Dbgc.pUVM, &AddrKdVersionBlock, GCPtrKpcr + 0x108);
            rc = DBGFR3MemRead(pThis->Dbgc.pUVM, Resp.Hdr.idCpu, &AddrKdVersionBlock, &Resp.u.GetVersion, sizeof(Resp.u.GetVersion));
        }
    }
    else
#endif
    {
        /* Fill in the request specific part, the static parts are from an amd64 Windows 10 guest. */
        Resp.u.GetVersion.u16VersMaj        = 0x0f;
        Resp.u.GetVersion.u16VersMin        = 0x2800;
        Resp.u.GetVersion.u8VersProtocol    = 0x6; /** From a Windows 10 guest. */
        Resp.u.GetVersion.u8VersKdSecondary = 0x2; /** From a Windows 10 guest. */
        Resp.u.GetVersion.fFlags            = 0x5; /** 64bit pointer. */
        Resp.u.GetVersion.u16MachineType    = IMAGE_FILE_MACHINE_AMD64;
        Resp.u.GetVersion.u8MaxPktType      = KD_PACKET_HDR_SUB_TYPE_MAX;
        Resp.u.GetVersion.u8MaxStateChange  = KD_PACKET_STATE_CHANGE_MAX - KD_PACKET_STATE_CHANGE_MIN;
        Resp.u.GetVersion.u8MaxManipulate   = KD_PACKET_MANIPULATE_REQ_CLEAR_ALL_INTERNAL_BKPT - KD_PACKET_MANIPULATE_REQ_MIN;
        Resp.u.GetVersion.u64PtrDebuggerDataList = 0 ;//0xfffff800deadc0de;
    }

    /* Try to fill in the rest using the OS digger interface if available. */
    int rc = VINF_SUCCESS;
    if (pThis->pIfWinNt)
        rc = pThis->pIfWinNt->pfnQueryKernelPtrs(pThis->pIfWinNt, pThis->Dbgc.pUVM, &Resp.u.GetVersion.u64PtrKernBase,
                                                 &Resp.u.GetVersion.u64PtrPsLoadedModuleList);
    else
    {
        /** @todo */
    }

    if (RT_SUCCESS(rc))
        rc = dbgcKdCtxPktSend(pThis, KD_PACKET_HDR_SIGNATURE_DATA, KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE,
                              &Resp, sizeof(Resp), true /*fAck*/);

    return rc;
}


/**
 * Processes a read virtual memory 64 request.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   pPktManip           The manipulate packet request.
 */
static int dbgcKdCtxPktManipulate64ReadMem(PKDCTX pThis, PCKDPACKETMANIPULATE64 pPktManip)
{
    KDPACKETMANIPULATEHDR RespHdr;
    KDPACKETMANIPULATE_XFERMEM64 XferMem64;
    uint8_t abMem[_4K];
    RT_ZERO(RespHdr); RT_ZERO(XferMem64);

    DBGFADDRESS AddrRead;
    uint32_t cbRead = RT_MIN(sizeof(abMem), pPktManip->u.XferMem.cbXferReq);
    if (pPktManip->Hdr.idReq == KD_PACKET_MANIPULATE_REQ_READ_VIRT_MEM)
        DBGFR3AddrFromFlat(pThis->Dbgc.pUVM, &AddrRead, pPktManip->u.XferMem.u64PtrTarget);
    else
        DBGFR3AddrFromPhys(pThis->Dbgc.pUVM, &AddrRead, pPktManip->u.XferMem.u64PtrTarget);

    RTSGSEG aRespSegs[3];
    uint32_t cSegs = 2; /* Gets incremented when read is successful. */
    RespHdr.idReq       = pPktManip->Hdr.idReq;
    RespHdr.u16CpuLvl   = pPktManip->Hdr.u16CpuLvl;
    RespHdr.idCpu       = pPktManip->Hdr.idCpu;
    RespHdr.u32NtStatus = NTSTATUS_SUCCESS;

    XferMem64.u64PtrTarget = pPktManip->u.XferMem.u64PtrTarget;
    XferMem64.cbXferReq    = pPktManip->u.XferMem.cbXferReq;
    XferMem64.cbXfered     = (uint32_t)cbRead;

    aRespSegs[0].pvSeg = &RespHdr;
    aRespSegs[0].cbSeg = sizeof(RespHdr);
    aRespSegs[1].pvSeg = &XferMem64;
    aRespSegs[1].cbSeg = sizeof(XferMem64);

    int rc = DBGFR3MemRead(pThis->Dbgc.pUVM, pThis->Dbgc.idCpu, &AddrRead, &abMem[0], cbRead);
    if (RT_SUCCESS(rc))
    {
        cSegs++;
        aRespSegs[2].pvSeg = &abMem[0];
        aRespSegs[2].cbSeg = cbRead;
    }
    else
        RespHdr.u32NtStatus = NTSTATUS_UNSUCCESSFUL; /** @todo Convert to an appropriate NT status code. */

    return dbgcKdCtxPktSendSg(pThis, KD_PACKET_HDR_SIGNATURE_DATA, KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE,
                              &aRespSegs[0], cSegs, true /*fAck*/);
}


/**
 * Processes a read control space 64 request.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   pPktManip           The manipulate packet request.
 */
static int dbgcKdCtxPktManipulate64ReadCtrlSpace(PKDCTX pThis, PCKDPACKETMANIPULATE64 pPktManip)
{
    KDPACKETMANIPULATEHDR RespHdr;
    KDPACKETMANIPULATE_XFERCTRLSPACE64 XferCtrlSpace64;
    uint8_t abResp[sizeof(NTKCONTEXT64)];
    size_t cbData = 0;
    RT_ZERO(RespHdr); RT_ZERO(XferCtrlSpace64);
    RT_ZERO(abResp);

    RTSGSEG aRespSegs[3];
    uint32_t cSegs = 2; /* Gets incremented when read is successful. */
    RespHdr.idReq       = KD_PACKET_MANIPULATE_REQ_READ_CTRL_SPACE;
    RespHdr.u16CpuLvl   = pPktManip->Hdr.u16CpuLvl;
    RespHdr.idCpu       = pPktManip->Hdr.idCpu;
    RespHdr.u32NtStatus = NTSTATUS_SUCCESS;

    XferCtrlSpace64.u64IdXfer = pPktManip->u.XferCtrlSpace.u64IdXfer;
    XferCtrlSpace64.cbXferReq = pPktManip->u.XferCtrlSpace.cbXferReq;

    aRespSegs[0].pvSeg = &RespHdr;
    aRespSegs[0].cbSeg = sizeof(RespHdr);
    aRespSegs[1].pvSeg = &XferCtrlSpace64;
    aRespSegs[1].cbSeg = sizeof(XferCtrlSpace64);

    int rc = VINF_SUCCESS;
    switch (pPktManip->u.XferCtrlSpace.u64IdXfer)
    {
        case KD_PACKET_MANIPULATE64_CTRL_SPACE_ID_KPCR:
        {
            if (pThis->pIfWinNt)
            {
                RTGCUINTPTR GCPtrKpcr = 0;

                rc = pThis->pIfWinNt->pfnQueryKpcrForVCpu(pThis->pIfWinNt, pThis->Dbgc.pUVM, RespHdr.idCpu,
                                                          &GCPtrKpcr, NULL /*pKpcrb*/);
                if (RT_SUCCESS(rc))
                    memcpy(&abResp[0], &GCPtrKpcr, sizeof(GCPtrKpcr));
            }

            cbData = sizeof(RTGCUINTPTR);
            break;
        }
        case KD_PACKET_MANIPULATE64_CTRL_SPACE_ID_KPCRB:
        {
            if (pThis->pIfWinNt)
            {
                RTGCUINTPTR GCPtrKpcrb = 0;

                rc = pThis->pIfWinNt->pfnQueryKpcrForVCpu(pThis->pIfWinNt, pThis->Dbgc.pUVM, RespHdr.idCpu,
                                                          NULL /*pKpcr*/, &GCPtrKpcrb);
                if (RT_SUCCESS(rc))
                    memcpy(&abResp[0], &GCPtrKpcrb, sizeof(GCPtrKpcrb));
            }

            cbData = sizeof(RTGCUINTPTR);
            break;
        }
        case KD_PACKET_MANIPULATE64_CTRL_SPACE_ID_KCTX:
        {
            rc = dbgcKdCtxQueryNtKCtx64(pThis, RespHdr.idCpu, (PNTKCONTEXT64)&abResp[0], NTCONTEXT64_F_FULL);
            if (RT_SUCCESS(rc))
                cbData = sizeof(NTKCONTEXT64);
            break;
        }
        case KD_PACKET_MANIPULATE64_CTRL_SPACE_ID_KTHRD:
        {
            if (pThis->pIfWinNt)
            {
                RTGCUINTPTR GCPtrCurThrd = 0;

                rc = pThis->pIfWinNt->pfnQueryCurThrdForVCpu(pThis->pIfWinNt, pThis->Dbgc.pUVM, RespHdr.idCpu,
                                                             &GCPtrCurThrd);
                if (RT_SUCCESS(rc))
                    memcpy(&abResp[0], &GCPtrCurThrd, sizeof(GCPtrCurThrd));
            }

            cbData = sizeof(RTGCUINTPTR);
            break;
        }
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (   RT_SUCCESS(rc)
        && cbData)
    {
        XferCtrlSpace64.cbXfered = RT_MIN(cbData, XferCtrlSpace64.cbXferReq);

        cSegs++;
        aRespSegs[2].pvSeg = &abResp[0];
        aRespSegs[2].cbSeg = cbData;
    }
    else if (RT_FAILURE(rc))
        RespHdr.u32NtStatus = NTSTATUS_UNSUCCESSFUL; /** @todo Convert to an appropriate NT status code. */

    return dbgcKdCtxPktSendSg(pThis, KD_PACKET_HDR_SIGNATURE_DATA, KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE,
                              &aRespSegs[0], cSegs, true /*fAck*/);
}


/**
 * Processes a restore breakpoint 64 request.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   pPktManip           The manipulate packet request.
 */
static int dbgcKdCtxPktManipulate64RestoreBkpt(PKDCTX pThis, PCKDPACKETMANIPULATE64 pPktManip)
{
    KDPACKETMANIPULATEHDR RespHdr;
    KDPACKETMANIPULATE_RESTOREBKPT64 RestoreBkpt64;
    RT_ZERO(RespHdr); RT_ZERO(RestoreBkpt64);

    RTSGSEG aRespSegs[2];
    RespHdr.idReq       = KD_PACKET_MANIPULATE_REQ_RESTORE_BKPT;
    RespHdr.u16CpuLvl   = pPktManip->Hdr.u16CpuLvl;
    RespHdr.idCpu       = pPktManip->Hdr.idCpu;
    RespHdr.u32NtStatus = NTSTATUS_SUCCESS;

    RestoreBkpt64.u32HndBkpt = pPktManip->u.RestoreBkpt.u32HndBkpt;

    aRespSegs[0].pvSeg = &RespHdr;
    aRespSegs[0].cbSeg = sizeof(RespHdr);
    aRespSegs[1].pvSeg = &RestoreBkpt64;
    aRespSegs[1].cbSeg = sizeof(RestoreBkpt64);

    /** @todo */

    return dbgcKdCtxPktSendSg(pThis, KD_PACKET_HDR_SIGNATURE_DATA, KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE,
                              &aRespSegs[0], RT_ELEMENTS(aRespSegs), true /*fAck*/);
}


/**
 * Processes a get context extended 64 request.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 * @param   pPktManip           The manipulate packet request.
 */
static int dbgcKdCtxPktManipulate64GetContextEx(PKDCTX pThis, PCKDPACKETMANIPULATE64 pPktManip)
{
    KDPACKETMANIPULATEHDR RespHdr;
    KDPACKETMANIPULATE_CONTEXTEX ContextEx;
    NTCONTEXT64 NtCtx;
    RT_ZERO(RespHdr); RT_ZERO(ContextEx);

    RTSGSEG aRespSegs[3];
    uint32_t cSegs = 2;
    RespHdr.idReq       = KD_PACKET_MANIPULATE_REQ_GET_CONTEXT_EX;
    RespHdr.u16CpuLvl   = pPktManip->Hdr.u16CpuLvl;
    RespHdr.idCpu       = pPktManip->Hdr.idCpu;
    RespHdr.u32NtStatus = NTSTATUS_UNSUCCESSFUL;

    ContextEx.offStart = pPktManip->u.ContextEx.offStart;
    ContextEx.cbXfer   = pPktManip->u.ContextEx.cbXfer;
    ContextEx.cbXfered = 0;

    aRespSegs[0].pvSeg = &RespHdr;
    aRespSegs[0].cbSeg = sizeof(RespHdr);
    aRespSegs[1].pvSeg = &ContextEx;
    aRespSegs[1].cbSeg = sizeof(ContextEx);

    int rc = dbgcKdCtxQueryNtCtx64(pThis, pPktManip->Hdr.idCpu, &NtCtx, NTCONTEXT64_F_FULL);
    if (   RT_SUCCESS(rc)
        && pPktManip->u.ContextEx.offStart < sizeof(NtCtx))
    {
        RespHdr.u32NtStatus = NTSTATUS_SUCCESS;
        ContextEx.cbXfered = RT_MIN(sizeof(NtCtx) - ContextEx.offStart, ContextEx.cbXfer);

        aRespSegs[2].pvSeg = (uint8_t *)&NtCtx + ContextEx.offStart;
        aRespSegs[2].cbSeg = ContextEx.cbXfered;
        cSegs++;
    }

    return dbgcKdCtxPktSendSg(pThis, KD_PACKET_HDR_SIGNATURE_DATA, KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE,
                              &aRespSegs[0], cSegs, true /*fAck*/);
}


/**
 * Processes a manipulate packet.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 */
static int dbgcKdCtxPktManipulate64Process(PKDCTX pThis)
{
    int rc = VINF_SUCCESS;
    PCKDPACKETMANIPULATE64 pPktManip = (PCKDPACKETMANIPULATE64)&pThis->abBody[0];

    switch (pPktManip->Hdr.idReq)
    {
        case KD_PACKET_MANIPULATE_REQ_GET_VERSION:
        {
            rc = dbgcKdCtxPktManipulate64GetVersion(pThis, pPktManip);
            break;
        }
        case KD_PACKET_MANIPULATE_REQ_READ_VIRT_MEM:
        case KD_PACKET_MANIPULATE_REQ_READ_PHYS_MEM:
        {
            rc = dbgcKdCtxPktManipulate64ReadMem(pThis, pPktManip);
            break;
        }
        case KD_PACKET_MANIPULATE_REQ_READ_CTRL_SPACE:
        {
            rc = dbgcKdCtxPktManipulate64ReadCtrlSpace(pThis, pPktManip);
            break;
        }
        case KD_PACKET_MANIPULATE_REQ_RESTORE_BKPT:
        {
            rc = dbgcKdCtxPktManipulate64RestoreBkpt(pThis, pPktManip);
            break;
        }
        case KD_PACKET_MANIPULATE_REQ_CLEAR_ALL_INTERNAL_BKPT:
            /* WinDbg doesn't seem to expect an answer apart from the ACK here. */
            break;
        case KD_PACKET_MANIPULATE_REQ_GET_CONTEXT_EX:
        {
            rc = dbgcKdCtxPktManipulate64GetContextEx(pThis, pPktManip);
            break;
        }
        default:
            KDPACKETMANIPULATEHDR RespHdr;
            RT_ZERO(RespHdr);

            RespHdr.idReq       = pPktManip->Hdr.idReq;
            RespHdr.u16CpuLvl   = pPktManip->Hdr.u16CpuLvl;
            RespHdr.idCpu       = pPktManip->Hdr.idCpu;
            RespHdr.u32NtStatus = NTSTATUS_NOT_IMPLEMENTED;
            rc = dbgcKdCtxPktSend(pThis, KD_PACKET_HDR_SIGNATURE_DATA, KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE,
                                  &RespHdr, sizeof(RespHdr), true /*fAck*/);
            break;
    }

    return rc;
}


/**
 * Processes a fully received packet.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 */
static int dbgcKdCtxPktProcess(PKDCTX pThis)
{
    int rc = VINF_SUCCESS;

    pThis->fBreakinRecv = false;

    /* Verify checksum. */
    if (dbgcKdPktChkSumGen(&pThis->abBody[0], pThis->PktHdr.Fields.cbBody) == pThis->PktHdr.Fields.u32ChkSum)
    {
        /** @todo Check packet id. */
        if (pThis->PktHdr.Fields.u16SubType != KD_PACKET_HDR_SUB_TYPE_RESET)
        {
            pThis->idPktNext = pThis->PktHdr.Fields.idPacket;
            rc = dbgcKdCtxPktSendAck(pThis);
        }
        if (RT_SUCCESS(rc))
        {
#ifdef LOG_ENABLED
            RTSGSEG Seg;
            Seg.pvSeg = &pThis->abBody[0];
            Seg.cbSeg = pThis->PktHdr.Fields.cbBody;
            dbgcKdPktDump(&pThis->PktHdr.Fields, &Seg, 1 /*cSegs*/, true /*fRx*/);
#endif

            switch (pThis->PktHdr.Fields.u16SubType)
            {
                case KD_PACKET_HDR_SUB_TYPE_RESET:
                {
                    pThis->idPktNext = 0;
                    rc = dbgcKdCtxPktSendReset(pThis);
                    if (RT_SUCCESS(rc))
                    {
                        rc = DBGFR3Halt(pThis->Dbgc.pUVM, VMCPUID_ALL);
                        if (rc == VWRN_DBGF_ALREADY_HALTED)
                            rc = dbgcKdCtxStateChangeSend(pThis, DBGFEVENT_HALT_DONE);
                    }
                    pThis->idPktNext = KD_PACKET_HDR_ID_RESET;
                    break;
                }
                case KD_PACKET_HDR_SUB_TYPE_STATE_MANIPULATE:
                {
                    CPUMMODE enmMode = DBGCCmdHlpGetCpuMode(&pThis->Dbgc.CmdHlp);
                    switch (enmMode)
                    {
                        case CPUMMODE_PROTECTED:
                        {
                            rc = VERR_NOT_IMPLEMENTED;
                            break;
                        }
                        case CPUMMODE_LONG:
                        {
                            pThis->idPktNext = pThis->PktHdr.Fields.idPacket ^ 0x1;
                            rc = dbgcKdCtxPktManipulate64Process(pThis);
                            break;
                        }
                        case CPUMMODE_REAL:
                        default:
                            rc = VERR_NOT_SUPPORTED;
                            break;
                    }
                    break;
                }
                case KD_PACKET_HDR_SUB_TYPE_ACKNOWLEDGE:
                case KD_PACKET_HDR_SUB_TYPE_RESEND:
                {
                    /* Don't do anything. */
                    rc = VINF_SUCCESS;
                    break;
                }
                default:
                    rc = VERR_NOT_IMPLEMENTED;
            }
        }
    }
    else
    {
        pThis->idPktNext = pThis->PktHdr.Fields.idPacket;
        rc = dbgcKdCtxPktSendResend(pThis);
    }

    if (pThis->fBreakinRecv)
    {
        pThis->fBreakinRecv = false;
        rc = DBGFR3Halt(pThis->Dbgc.pUVM, VMCPUID_ALL);
        if (rc == VWRN_DBGF_ALREADY_HALTED)
            rc = dbgcKdCtxStateChangeSend(pThis, DBGFEVENT_HALT_DONE);
    }

    /* Next packet. */
    dbgcKdCtxPktRecvReset(pThis);
    return rc;
}


/**
 * Processes the received data based on the current state.
 *
 * @returns VBox status code.
 * @param   pThis               The KD context.
 */
static int dbgcKdCtxRecvDataProcess(PKDCTX pThis)
{
    int rc = VINF_SUCCESS;

    switch (pThis->enmState)
    {
        case KDRECVSTATE_PACKET_HDR_FIRST_BYTE:
        {
            /* Does it look like a valid packet start?. */
            if (   pThis->PktHdr.ab[0] == KD_PACKET_HDR_SIGNATURE_DATA_BYTE
                || pThis->PktHdr.ab[0] == KD_PACKET_HDR_SIGNATURE_CONTROL_BYTE)
            {
                pThis->pbRecv        = &pThis->PktHdr.ab[1];
                pThis->cbRecvLeft    = sizeof(pThis->PktHdr.ab[1]);
                pThis->enmState      = KDRECVSTATE_PACKET_HDR_SECOND_BYTE;
                pThis->msRecvTimeout = DBGC_KD_RECV_TIMEOUT_MS;
            }
            else if (pThis->PktHdr.ab[0] == KD_PACKET_HDR_SIGNATURE_BREAKIN_BYTE)
            {
                rc = DBGFR3Halt(pThis->Dbgc.pUVM, VMCPUID_ALL);
                if (rc == VWRN_DBGF_ALREADY_HALTED)
                    rc = dbgcKdCtxStateChangeSend(pThis, DBGFEVENT_HALT_DONE);
                dbgcKdCtxPktRecvReset(pThis);
            }
            /* else: Ignore and continue. */
            break;
        }
        case KDRECVSTATE_PACKET_HDR_SECOND_BYTE:
        {
            /*
             * If the first and second byte differ there might be a single breakin
             * packet byte received and this is actually the start of a new packet.
             */
            if (pThis->PktHdr.ab[0] != pThis->PktHdr.ab[1])
            {
                if (pThis->PktHdr.ab[0] == KD_PACKET_HDR_SIGNATURE_BREAKIN_BYTE)
                {
                    /* Halt the VM and rearrange the packet receiving state machine. */
                    LogFlow(("DbgKd: Halting VM!\n"));

                    rc = DBGFR3Halt(pThis->Dbgc.pUVM, VMCPUID_ALL);
                    pThis->PktHdr.ab[0] = pThis->PktHdr.ab[1]; /* Overwrite the first byte with the new start. */
                    pThis->pbRecv       = &pThis->PktHdr.ab[1];
                    pThis->cbRecvLeft   = sizeof(pThis->PktHdr.ab[1]);
                }
                else
                    rc = VERR_NET_PROTOCOL_ERROR; /* Refuse talking to the remote end any further. */
            }
            else
            {
                /* Normal packet receive continues with the rest of the header. */
                pThis->pbRecv     = &pThis->PktHdr.ab[2];
                pThis->cbRecvLeft = sizeof(pThis->PktHdr.Fields) - 2;
                pThis->enmState   = KDRECVSTATE_PACKET_HDR;
            }
            break;
        }
        case KDRECVSTATE_PACKET_HDR:
        {
            if (   dbgcKdPktHdrValidate(&pThis->PktHdr.Fields)
                && pThis->PktHdr.Fields.cbBody <= sizeof(pThis->abBody))
            {
                /* Start receiving the body. */
                if (pThis->PktHdr.Fields.cbBody)
                {
                    pThis->pbRecv     = &pThis->abBody[0];
                    pThis->cbRecvLeft = pThis->PktHdr.Fields.cbBody;
                    pThis->enmState   = KDRECVSTATE_PACKET_BODY;
                }
                else /* No body means no trailer byte it looks like. */
                    rc = dbgcKdCtxPktProcess(pThis);
            }
            else
                rc = VERR_NET_PROTOCOL_ERROR;
            break;
        }
        case KDRECVSTATE_PACKET_BODY:
        {
            pThis->enmState   = KDRECVSTATE_PACKET_TRAILER;
            pThis->bTrailer   = 0;
            pThis->pbRecv     = &pThis->bTrailer;
            pThis->cbRecvLeft = sizeof(pThis->bTrailer);
            break;
        }
        case KDRECVSTATE_PACKET_TRAILER:
        {
            if (pThis->bTrailer == KD_PACKET_TRAILING_BYTE)
                rc = dbgcKdCtxPktProcess(pThis);
            else
                rc = VERR_NET_PROTOCOL_ERROR;
            break;
        }
        default:
            AssertMsgFailed(("Invalid receive state %d\n", pThis->enmState));
    }

    return rc;
}


/**
 * Receive data and processes complete packets.
 *
 * @returns Status code.
 * @param   pThis               The KD context.
 */
static int dbgcKdCtxRecv(PKDCTX pThis)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pThis=%p{.cbRecvLeft=%zu}\n", pThis, pThis->cbRecvLeft));

    if (pThis->cbRecvLeft)
    {
        size_t cbRead = 0;
        rc = pThis->Dbgc.pBack->pfnRead(pThis->Dbgc.pBack, pThis->pbRecv, pThis->cbRecvLeft, &cbRead);
        if (RT_SUCCESS(rc))
        {
            pThis->tsRecvLast  = RTTimeMilliTS();
            pThis->cbRecvLeft -= cbRead;
            pThis->pbRecv     += cbRead;
            if (!pThis->cbRecvLeft)
                rc = dbgcKdCtxRecvDataProcess(pThis);
        }
    }

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}


/**
 * Processes debugger events.
 *
 * @returns VBox status code.
 * @param   pThis   The KD context data.
 * @param   pEvent  Pointer to event data.
 */
static int dbgcKdCtxProcessEvent(PKDCTX pThis, PCDBGFEVENT pEvent)
{
    /*
     * Process the event.
     */
    //PDBGC pDbgc = &pThis->Dbgc;
    pThis->Dbgc.pszScratch = &pThis->Dbgc.achInput[0];
    pThis->Dbgc.iArg       = 0;
    int rc = VINF_SUCCESS;
    switch (pEvent->enmType)
    {
        /*
         * The first part is events we have initiated with commands.
         */
        case DBGFEVENT_HALT_DONE:
        {
            rc = dbgcKdCtxStateChangeSend(pThis, pEvent->enmType);
            break;
        }

        /*
         * The second part is events which can occur at any time.
         */
#if 0
        case DBGFEVENT_FATAL_ERROR:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbf event: Fatal error! (%s)\n",
                                         dbgcGetEventCtx(pEvent->enmCtx));
            if (RT_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }
#endif

        case DBGFEVENT_BREAKPOINT:
        case DBGFEVENT_BREAKPOINT_IO:
        case DBGFEVENT_BREAKPOINT_MMIO:
        case DBGFEVENT_BREAKPOINT_HYPER:
        case DBGFEVENT_STEPPED:
        case DBGFEVENT_STEPPED_HYPER:
        {
            rc = dbgcKdCtxStateChangeSend(pThis, pEvent->enmType);
            break;
        }

#if 0
        case DBGFEVENT_ASSERTION_HYPER:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                         "\ndbgf event: Hypervisor Assertion! (%s)\n"
                                         "%s"
                                         "%s"
                                         "\n",
                                         dbgcGetEventCtx(pEvent->enmCtx),
                                         pEvent->u.Assert.pszMsg1,
                                         pEvent->u.Assert.pszMsg2);
            if (RT_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }

        case DBGFEVENT_DEV_STOP:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                         "\n"
                                         "dbgf event: DBGFSTOP (%s)\n"
                                         "File:     %s\n"
                                         "Line:     %d\n"
                                         "Function: %s\n",
                                         dbgcGetEventCtx(pEvent->enmCtx),
                                         pEvent->u.Src.pszFile,
                                         pEvent->u.Src.uLine,
                                         pEvent->u.Src.pszFunction);
            if (RT_SUCCESS(rc) && pEvent->u.Src.pszMessage && *pEvent->u.Src.pszMessage)
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL,
                                             "Message:  %s\n",
                                             pEvent->u.Src.pszMessage);
            if (RT_SUCCESS(rc))
                rc = pDbgc->CmdHlp.pfnExec(&pDbgc->CmdHlp, "r");
            break;
        }


        case DBGFEVENT_INVALID_COMMAND:
        {
            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf/dbgc error: Invalid command event!\n");
            break;
        }
#endif

        case DBGFEVENT_POWERING_OFF:
        {
            pThis->Dbgc.fReady = false;
            pThis->Dbgc.pBack->pfnSetReady(pThis->Dbgc.pBack, false);
            rc = VERR_GENERAL_FAILURE;
            break;
        }

#if 0
        default:
        {
            /*
             * Probably a generic event. Look it up to find its name.
             */
            PCDBGCSXEVT pEvtDesc = dbgcEventLookup(pEvent->enmType);
            if (pEvtDesc)
            {
                if (pEvtDesc->enmKind == kDbgcSxEventKind_Interrupt)
                {
                    Assert(pEvtDesc->pszDesc);
                    Assert(pEvent->u.Generic.cArgs == 1);
                    rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: %s no %#llx! (%s)\n",
                                                 pEvtDesc->pszDesc, pEvent->u.Generic.auArgs[0], pEvtDesc->pszName);
                }
                else if (pEvtDesc->fFlags & DBGCSXEVT_F_BUGCHECK)
                {
                    Assert(pEvent->u.Generic.cArgs >= 5);
                    char szDetails[512];
                    DBGFR3FormatBugCheck(pDbgc->pUVM, szDetails, sizeof(szDetails), pEvent->u.Generic.auArgs[0],
                                         pEvent->u.Generic.auArgs[1], pEvent->u.Generic.auArgs[2],
                                         pEvent->u.Generic.auArgs[3], pEvent->u.Generic.auArgs[4]);
                    rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: %s %s%s!\n%s", pEvtDesc->pszName,
                                                 pEvtDesc->pszDesc ? "- " : "", pEvtDesc->pszDesc ? pEvtDesc->pszDesc : "",
                                                 szDetails);
                }
                else if (   (pEvtDesc->fFlags & DBGCSXEVT_F_TAKE_ARG)
                         || pEvent->u.Generic.cArgs > 1
                         || (   pEvent->u.Generic.cArgs == 1
                             && pEvent->u.Generic.auArgs[0] != 0))
                {
                    if (pEvtDesc->pszDesc)
                        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: %s - %s!",
                                                     pEvtDesc->pszName, pEvtDesc->pszDesc);
                    else
                        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: %s!", pEvtDesc->pszName);
                    if (pEvent->u.Generic.cArgs <= 1)
                        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, " arg=%#llx\n", pEvent->u.Generic.auArgs[0]);
                    else
                    {
                        for (uint32_t i = 0; i < pEvent->u.Generic.cArgs; i++)
                            rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, " args[%u]=%#llx", i, pEvent->u.Generic.auArgs[i]);
                        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\n");
                    }
                }
                else
                {
                    if (pEvtDesc->pszDesc)
                        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: %s - %s!\n",
                                                     pEvtDesc->pszName, pEvtDesc->pszDesc);
                    else
                        rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf event: %s!\n", pEvtDesc->pszName);
                }
            }
            else
                rc = pDbgc->CmdHlp.pfnPrintf(&pDbgc->CmdHlp, NULL, "\ndbgf/dbgc error: Unknown event %d!\n", pEvent->enmType);
            break;
        }
#else
        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
#endif
    }

    return rc;
}


/**
 * Handle a receive timeout.
 *
 * @returns VBox status code.
 * @param   pThis   Pointer to the KD context.
 */
static int dbgcKdCtxRecvTimeout(PKDCTX pThis)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pThis=%p\n", pThis));

    /*
     * If a single breakin packet byte was received but the header is otherwise incomplete
     * the VM is halted and a state change will be sent in the event processing loop.
     */
    if (   pThis->enmState == KDRECVSTATE_PACKET_HDR_SECOND_BYTE
        && pThis->PktHdr.ab[0] == KD_PACKET_HDR_SIGNATURE_BREAKIN_BYTE)
    {
        LogFlow(("DbgKd: Halting VM!\n"));
        rc = DBGFR3Halt(pThis->Dbgc.pUVM, VMCPUID_ALL);
    }
    else /* Send a reset packet (@todo Figure out the semantics in this case exactly). */
        rc = dbgcKdCtxPktSendReset(pThis);

    dbgcKdCtxPktRecvReset(pThis);

    LogFlowFunc(("rc=%Rrc\n", rc));
    return rc;
}


/**
 * Run the debugger console.
 *
 * @returns VBox status code.
 * @param   pThis   Pointer to the KD context.
 */
int dbgcKdRun(PKDCTX pThis)
{
    /*
     * We're ready for commands now.
     */
    pThis->Dbgc.fReady = true;
    pThis->Dbgc.pBack->pfnSetReady(pThis->Dbgc.pBack, true);

    /*
     * Main Debugger Loop.
     *
     * This loop will either block on waiting for input or on waiting on
     * debug events. If we're forwarding the log we cannot wait for long
     * before we must flush the log.
     */
    int rc;
    for (;;)
    {
        rc = VERR_SEM_OUT_OF_TURN;
        if (pThis->Dbgc.pUVM)
            rc = DBGFR3QueryWaitable(pThis->Dbgc.pUVM);

        if (RT_SUCCESS(rc))
        {
            /*
             * Wait for a debug event.
             */
            DBGFEVENT Evt;
            rc = DBGFR3EventWait(pThis->Dbgc.pUVM, 32, &Evt);
            if (RT_SUCCESS(rc))
            {
                rc = dbgcKdCtxProcessEvent(pThis, &Evt);
                if (RT_FAILURE(rc))
                    break;
            }
            else if (rc != VERR_TIMEOUT)
                break;

            /*
             * Check for input.
             */
            if (pThis->Dbgc.pBack->pfnInput(pThis->Dbgc.pBack, 0))
            {
                rc = dbgcKdCtxRecv(pThis);
                if (RT_FAILURE(rc))
                    break;
            }
        }
        else if (rc == VERR_SEM_OUT_OF_TURN)
        {
            /*
             * Wait for input.
             */
            if (pThis->Dbgc.pBack->pfnInput(pThis->Dbgc.pBack, 1000))
            {
                rc = dbgcKdCtxRecv(pThis);
                if (RT_FAILURE(rc))
                    break;
            }
            else if (   pThis->msRecvTimeout != RT_INDEFINITE_WAIT
                     && (RTTimeMilliTS() - pThis->tsRecvLast >= pThis->msRecvTimeout))
                rc = dbgcKdCtxRecvTimeout(pThis);
        }
        else
            break;
    }

    return rc;
}


/**
 * Creates a KD context instance with the given backend.
 *
 * @returns VBox status code.
 * @param   ppKdCtx                 Where to store the pointer to the KD stub context instance on success.
 * @param   pBack                   The backend to use for I/O.
 * @param   fFlags                  Flags controlling the behavior.
 */
static int dbgcKdCtxCreate(PPKDCTX ppKdCtx, PDBGCBACK pBack, unsigned fFlags)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pBack, VERR_INVALID_POINTER);
    AssertMsgReturn(!fFlags, ("%#x", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Allocate and initialize.
     */
    PKDCTX pThis = (PKDCTX)RTMemAllocZ(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    dbgcInitCmdHlp(&pThis->Dbgc);
    /*
     * This is compied from the native debug console (will be used for monitor commands)
     * in DBGCConsole.cpp. Try to keep both functions in sync.
     */
    pThis->Dbgc.pBack            = pBack;
    /*pThis->Dbgc.pfnOutput        = dbgcOutputGdb;*/
    pThis->Dbgc.pvOutputUser     = pThis;
    pThis->Dbgc.pVM              = NULL;
    pThis->Dbgc.pUVM             = NULL;
    pThis->Dbgc.idCpu            = 0;
    pThis->Dbgc.hDbgAs           = DBGF_AS_GLOBAL;
    pThis->Dbgc.pszEmulation     = "CodeView/WinDbg";
    pThis->Dbgc.paEmulationCmds  = &g_aCmdsCodeView[0];
    pThis->Dbgc.cEmulationCmds   = g_cCmdsCodeView;
    pThis->Dbgc.paEmulationFuncs = &g_aFuncsCodeView[0];
    pThis->Dbgc.cEmulationFuncs  = g_cFuncsCodeView;
    //pThis->Dbgc.fLog             = false;
    pThis->Dbgc.fRegTerse        = true;
    pThis->Dbgc.fStepTraceRegs   = true;
    //pThis->Dbgc.cPagingHierarchyDumps = 0;
    //pThis->Dbgc.DisasmPos        = {0};
    //pThis->Dbgc.SourcePos        = {0};
    //pThis->Dbgc.DumpPos          = {0};
    pThis->Dbgc.pLastPos          = &pThis->Dbgc.DisasmPos;
    //pThis->Dbgc.cbDumpElement    = 0;
    //pThis->Dbgc.cVars            = 0;
    //pThis->Dbgc.paVars           = NULL;
    //pThis->Dbgc.pPlugInHead      = NULL;
    //pThis->Dbgc.pFirstBp         = NULL;
    //pThis->Dbgc.abSearch         = {0};
    //pThis->Dbgc.cbSearch         = 0;
    pThis->Dbgc.cbSearchUnit       = 1;
    pThis->Dbgc.cMaxSearchHits     = 1;
    //pThis->Dbgc.SearchAddr       = {0};
    //pThis->Dbgc.cbSearchRange    = 0;

    //pThis->Dbgc.uInputZero       = 0;
    //pThis->Dbgc.iRead            = 0;
    //pThis->Dbgc.iWrite           = 0;
    //pThis->Dbgc.cInputLines      = 0;
    //pThis->Dbgc.fInputOverflow   = false;
    pThis->Dbgc.fReady           = true;
    pThis->Dbgc.pszScratch       = &pThis->Dbgc.achScratch[0];
    //pThis->Dbgc.iArg             = 0;
    //pThis->Dbgc.rcOutput         = 0;
    //pThis->Dbgc.rcCmd            = 0;

    //pThis->Dbgc.pszHistoryFile       = NULL;
    //pThis->Dbgc.pszGlobalInitScript  = NULL;
    //pThis->Dbgc.pszLocalInitScript   = NULL;

    dbgcEvalInit();

    pThis->fBreakinRecv = false;
    pThis->idPktNext    = KD_PACKET_HDR_ID_INITIAL;
    pThis->pIfWinNt     = NULL;
    dbgcKdCtxPktRecvReset(pThis);

    *ppKdCtx = pThis;
    return VINF_SUCCESS;
}


/**
 * Destroys the given KD context.
 *
 * @returns nothing.
 * @param   pThis                   The KD context to destroy.
 */
static void dbgcKdCtxDestroy(PKDCTX pThis)
{
    AssertPtr(pThis);

    pThis->pIfWinNt = NULL;

    /* Detach from the VM. */
    if (pThis->Dbgc.pUVM)
        DBGFR3Detach(pThis->Dbgc.pUVM);

    /* Free config strings. */
    RTStrFree(pThis->Dbgc.pszGlobalInitScript);
    pThis->Dbgc.pszGlobalInitScript = NULL;
    RTStrFree(pThis->Dbgc.pszLocalInitScript);
    pThis->Dbgc.pszLocalInitScript = NULL;
    RTStrFree(pThis->Dbgc.pszHistoryFile);
    pThis->Dbgc.pszHistoryFile = NULL;

    /* Finally, free the instance memory. */
    RTMemFree(pThis);
}


DECLHIDDEN(int) dbgcKdStubCreate(PUVM pUVM, PDBGCBACK pBack, unsigned fFlags)
{
    /*
     * Validate input.
     */
    AssertPtrNullReturn(pUVM, VERR_INVALID_VM_HANDLE);
    PVM pVM = NULL;
    if (pUVM)
    {
        pVM = VMR3GetVM(pUVM);
        AssertPtrReturn(pVM, VERR_INVALID_VM_HANDLE);
    }

    /*
     * Allocate and initialize instance data
     */
    PKDCTX pThis;
    int rc = dbgcKdCtxCreate(&pThis, pBack, fFlags);
    if (RT_FAILURE(rc))
        return rc;
    if (!HMR3IsEnabled(pUVM) && !NEMR3IsEnabled(pUVM))
        pThis->Dbgc.hDbgAs = DBGF_AS_RC_AND_GC_GLOBAL;

    /*
     * Attach to the specified VM.
     */
    if (RT_SUCCESS(rc) && pUVM)
    {
        rc = DBGFR3Attach(pUVM);
        if (RT_SUCCESS(rc))
        {
            pThis->Dbgc.pVM   = pVM;
            pThis->Dbgc.pUVM  = pUVM;
            pThis->Dbgc.idCpu = 0;

            /* Try detecting a Windows NT guest. */
            char szName[64];
            rc = DBGFR3OSDetect(pUVM, szName, sizeof(szName));
            if (RT_SUCCESS(rc))
            {
                pThis->pIfWinNt = (PDBGFOSIWINNT)DBGFR3OSQueryInterface(pUVM, DBGFOSINTERFACE_WINNT);
                if (pThis->pIfWinNt)
                    LogRel(("DBGC/Kd: Detected Windows NT guest OS (%s)\n", &szName[0]));
                else
                    LogRel(("DBGC/Kd: Detected guest OS is not of the Windows NT kind (%s)\n", &szName[0]));
            }
            else
            {
                LogRel(("DBGC/Kd: Unable to detect any guest operating system type, rc=%Rrc\n", rc));
                rc = VINF_SUCCESS; /* Try to continue nevertheless. */
            }
        }
        else
            rc = pThis->Dbgc.CmdHlp.pfnVBoxError(&pThis->Dbgc.CmdHlp, rc, "When trying to attach to VM %p\n", pThis->Dbgc.pVM);
    }

    /*
     * Load plugins.
     */
    if (RT_SUCCESS(rc))
    {
        if (pVM)
            DBGFR3PlugInLoadAll(pThis->Dbgc.pUVM);
        dbgcEventInit(&pThis->Dbgc);

        /*
         * Run the debugger main loop.
         */
        rc = dbgcKdRun(pThis);
        dbgcEventTerm(&pThis->Dbgc);
    }

    /*
     * Cleanup console debugger session.
     */
    dbgcKdCtxDestroy(pThis);
    return rc == VERR_DBGC_QUIT ? VINF_SUCCESS : rc;
}

