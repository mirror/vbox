/* $Id$ */
/** @file
 * VBox Qt GUI - Qt patching related stuff tucked away here to not clutter the rest of the code.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>

# define FN_CALL_REG_PARM1 DISGREG_RCX
# define FN_CALL_REG_PARM2 DISGREG_RDX
# define FN_CALL_REG_PARM3 DISGREG_R8

# define QT_QTEXTBROWSER_LIB            "Qt5Widgets.dll"
# define QT_QTEXTBROWSER_PAINTEVENT_SYM "?paintEvent@QTextBrowser@@MEAAXPEAVQPaintEvent@@@Z"

#else
# include <dlfcn.h>

# define FN_CALL_REG_PARM1 DISGREG_RDI
# define FN_CALL_REG_PARM2 DISGREG_RSI
# define FN_CALL_REG_PARM3 DISGREG_RDX

# ifdef RT_OS_DARWIN
#  define QT_QTEXTBROWSER_LIB            "libQt5Widgets.dylib"
# else
#  define QT_QTEXTBROWSER_LIB            "libQt5Widgets.so"
# endif
# define QT_QTEXTBROWSER_PAINTEVENT_SYM "_ZN12QTextBrowser10paintEventEP11QPaintEvent"
#endif

/* Qt includes: */
#include <QPainter>

/* VBox includes: */
#include <VBox/dis.h>
#include <VBox/log.h>
#include <iprt/once.h>
#include <iprt/mem.h>


static RTONCE g_TextBrowserPatchingOnce = RTONCE_INITIALIZER;
/** Memory allocated for the patch. */
static uint8_t *g_pbExecMemory          = NULL;


/**
 * Our hook for QTextBrowser::paintEvent to set the proper render hints.
 *
 * @returns nothing.
 * @param   uArg1DoNotUse       The first argument passed to d->paint... (this).
 * @param   pPainter            The Qt painter object to set the render hints for.
 * @param   uArg3DoNotUse       The third argument passed to d->paint().
 */
static void uiCommonQtTextBrowserPaintHook(uintptr_t uArg1DoNotUse, QPainter *pPainter, uintptr_t uArg3DoNotUse)
{
    RT_NOREF(uArg1DoNotUse, uArg3DoNotUse, pPainter);
    pPainter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
}


/**
 * Checks whether the given instruction is a supported mov instruction.
 *
 * @returns Flag whether the instruction is a supported mov.
 * @param   pbCur               Pointer to the instruction to check.
 * @param   pcbInsnMov          Where to store the size of the mov instruction on success.
 */
static bool uiCommonQtRuntimePatchIsMovParam(const uint8_t * const pbCur, uint32_t *pcbInsnMov)
{
    DISSTATE Dis;
    uint32_t cbInsn = 1;

    int rc = DISInstr(pbCur, DISCPUMODE_64BIT, &Dis, &cbInsn);
    if (RT_FAILURE(rc))
        return false;

    *pcbInsnMov = cbInsn;

    if (   Dis.pCurInstr->uOpcode == OP_MOV
        && Dis.Param1.fUse & (DISUSE_BASE | DISUSE_REG_GEN64)
        && Dis.Param2.fUse & (DISUSE_BASE | DISUSE_REG_GEN64)
        && (   Dis.Param1.Base.idxGenReg == FN_CALL_REG_PARM1
            || Dis.Param1.Base.idxGenReg == FN_CALL_REG_PARM2
            || Dis.Param1.Base.idxGenReg == FN_CALL_REG_PARM3))
        return true;

    if (   Dis.pCurInstr->uOpcode == OP_LEA
        && Dis.Param1.fUse & (DISUSE_BASE | DISUSE_REG_GEN64)
        && (   Dis.Param1.Base.idxGenReg == FN_CALL_REG_PARM1
            || Dis.Param1.Base.idxGenReg == FN_CALL_REG_PARM2
            || Dis.Param1.Base.idxGenReg == FN_CALL_REG_PARM3))
        return true;

    return false;
}


/**
 * Checks whether the given instruction is a supported call instruction.
 *
 * @returns Flag whether the instruction is a supported call.
 * @param   pbCur               Pointer to the instruction to check.
 * @param   pcbInsnCall         Where to store the size of the call instruction on success.
 * @param   pPtrDst             Where to store the destination address of the call on success.
 */
static bool uiCommonQtRuntimePatchIsCall(const uint8_t * const pbCur, uint32_t *pcbInsnCall, uintptr_t *pPtrDst)
{
    DISSTATE Dis;
    uint32_t cbInsn = 1;

    int rc = DISInstr(pbCur, DISCPUMODE_64BIT, &Dis, &cbInsn);
    if (RT_FAILURE(rc))
        return false;

    *pcbInsnCall = cbInsn;

    if (   Dis.pCurInstr->uOpcode == OP_CALL
        && (Dis.Param1.fUse & DISUSE_IMMEDIATE64_REL))
    {
        /* Deduce destination. */
        *pPtrDst = (uintptr_t)pbCur + cbInsn + (int64_t)Dis.Param1.uValue;
        return true;
    }

    return false;
}


/**
 * Hooks the QTextBrowser::paintEvent routine to call our hook.
 *
 * @returns VBox status code.
 * @param   pbDst               The destination to hook.
 * @param   cbDst               Number of bytes of the region of instructions to replace.
 * @param   uPtrTgt             The target address to jump to.
 */
static int uiCommonQtRuntimeQTextBrowserPatchHook(const uint8_t *pbDst, size_t cbDst, uintptr_t uPtrTgt)
{
    /*
     * Make the target memory writeable to be able to insert the patch.
     * Unprotect two pages in case the code crosses a page boundary.
     */
    void *pvTgtBase = (void *)(((uintptr_t)pbDst) & ~(uintptr_t)(_4K - 1));
    int rc = RTMemProtect(pvTgtBase, 2 * _4K, RTMEM_PROT_WRITE | RTMEM_PROT_READ | RTMEM_PROT_EXEC);
    if (RT_FAILURE(rc))
        return rc;

    uint8_t *pbTarget = (uint8_t *)pbDst;

    *pbTarget++ = 0x48; /* mov rax, qword */
    *pbTarget++ = 0xb8;
    *(uintptr_t *)pbTarget = uPtrTgt;
    pbTarget += sizeof(uintptr_t);
    *pbTarget++ = 0xff; /* jmp rax */
    *pbTarget++ = 0xe0;

    /* Fill the rest with nops. */
    while (pbTarget < pbDst + cbDst)
        *pbTarget++ = 0x90; /* nop */

    return RTMemProtect(pvTgtBase, 2 * _4K, RTMEM_PROT_READ | RTMEM_PROT_EXEC);
}


/**
 * Patches QTextBrowser::paintEvent to be able to modify the painter's render hints.
 *
 * @returns VBox status code.
 * @param   pvUser              Opaque user data (unused).
 *
 * @remarks This whole stuff is required as the painter used by QTextBrowser::paintEvent()
 *          is not using QPainter::SmoothPixmapTransform making pixmaps look like garbage in the
 *          help viewer.
 *          Patching the sources and rebuilding Qt is not possible on Linux either as some packages
 *          use the package provided by the distribution instead of our own self built one. Rewriting
 *          QTextBrowser internally is too complex and using the WebEngine is too heavy for a simple help
 *          viewer, so we are left with this mess unfortunately...
 */
static DECLCALLBACK(int32_t) uiCommonQtRuntimeQTextBrowserPatch(void *pvUser)
{
    RT_NOREF(pvUser);

    /*
     * (lldb) dis -n QTextBrowser::paintEvent -F intel -r -b
     * libQt5Widgets.so.5`QTextBrowser::paintEvent:
     *     0x7ffff38a1df0 <+0>:   41 55                       push   r13
     *     0x7ffff38a1df2 <+2>:   41 54                       push   r12
     *     0x7ffff38a1df4 <+4>:   49 89 f4                    mov    r12, rsi
     *     0x7ffff38a1df7 <+7>:   55                          push   rbp
     *     0x7ffff38a1df8 <+8>:   48 83 ec 10                 sub    rsp, 0x10
     *     0x7ffff38a1dfc <+12>:  4c 8b 6f 08                 mov    r13, qword ptr [rdi + 0x8]
     *     0x7ffff38a1e00 <+16>:  64 48 8b 04 25 28 00 00 00  mov    rax, qword ptr fs:[0x28]
     *     0x7ffff38a1e09 <+25>:  48 89 44 24 08              mov    qword ptr [rsp + 0x8], rax
     *     0x7ffff38a1e0e <+30>:  31 c0                       xor    eax, eax
     *     0x7ffff38a1e10 <+32>:  48 89 e5                    mov    rbp, rsp
     *     0x7ffff38a1e13 <+35>:  49 8b b5 18 02 00 00        mov    rsi, qword ptr [r13 + 0x218]
     *     0x7ffff38a1e1a <+42>:  48 89 ef                    mov    rdi, rbp
     *     0x7ffff38a1e1d <+45>:  48 8d 46 10                 lea    rax, [rsi + 0x10]
     *     0x7ffff38a1e21 <+49>:  48 85 f6                    test   rsi, rsi
     *     0x7ffff38a1e24 <+52>:  48 0f 45 f0                 cmovne rsi, rax
     *     0x7ffff38a1e28 <+56>:  ff 15 72 75 36 00           call   qword ptr [rip + 0x367572]
     *     0x7ffff38a1e2e <+62>:  4c 89 ef                    mov    rdi, r13
     *     0x7ffff38a1e31 <+65>:  4c 89 e2                    mov    rdx, r12
     *     0x7ffff38a1e34 <+68>:  48 89 ee                    mov    rsi, rbp
     *     0x7ffff38a1e37 <+71>:  67 e8 f3 b2 ff ff           call   0x7ffff389d130            ; ___lldb_unnamed_symbol3028$$libQt5Widgets.so.5
     *     0x7ffff38a1e3d <+77>:  48 89 ef                    mov    rdi, rbp
     *     0x7ffff38a1e40 <+80>:  ff 15 5a 77 36 00           call   qword ptr [rip + 0x36775a]
     *     0x7ffff38a1e46 <+86>:  48 8b 44 24 08              mov    rax, qword ptr [rsp + 0x8]
     *     0x7ffff38a1e4b <+91>:  64 48 2b 04 25 28 00 00 00  sub    rax, qword ptr fs:[0x28]
     *     0x7ffff38a1e54 <+100>: 75 0a                       jne    0x7ffff38a1e60            ; <+112>
     *     0x7ffff38a1e56 <+102>: 48 83 c4 10                 add    rsp, 0x10
     *     0x7ffff38a1e5a <+106>: 5d                          pop    rbp
     *     0x7ffff38a1e5b <+107>: 41 5c                       pop    r12
     *     0x7ffff38a1e5d <+109>: 41 5d                       pop    r13
     *     0x7ffff38a1e5f <+111>: c3                          ret
     *     0x7ffff38a1e60 <+112>: ff 15 6a 4e 36 00           call   qword ptr [rip + 0x364e6a]
     *
     * Hook ourselves after the first call instruction (which creates the QPainter object) and before the second
     * call which does the actual painting.
     */
#ifndef RT_OS_WINDOWS
    uint8_t * const pbTgtBase = (uint8_t *)(uintptr_t)dlsym(RTLD_DEFAULT, QT_QTEXTBROWSER_PAINTEVENT_SYM);
#else
    HMODULE hQtWidgets = GetModuleHandleA(QT_QTEXTBROWSER_LIB);
    if (hQtWidgets == NULL)
        return VINF_SUCCESS;
    uint8_t * const pbTgtBase = (uint8_t * const)GetProcAddress(hQtWidgets, QT_QTEXTBROWSER_PAINTEVENT_SYM);
#endif
    if (pbTgtBase)
    {
        size_t cbCheck = 128; /* Maximum amount of instruction bytes to check. */
        const uint8_t *pbCur = pbTgtBase;
        while (cbCheck)
        {
            DISSTATE Dis;
            uint32_t cbInsn = 1;

            int rc = DISInstr(pbCur, DISCPUMODE_64BIT, &Dis, &cbInsn);
            if (RT_FAILURE(rc))
                return VINF_SUCCESS;

            cbCheck -= cbInsn;
            pbCur   += cbInsn;

            if (   Dis.pCurInstr->fOpType & DISOPTYPE_CONTROLFLOW
                && Dis.pCurInstr->uOpcode == OP_CALL)
                break;
        }

        if (cbCheck)
        {
            LogFlow(("Found matching call instruction before %p\n", pbCur));

            /*
             * Now check that the next three instructions are simple mov instructions where the destination is one of rdi, rdx or rsi
             * denoting the parameters passed to the d->paint() method. Also checks that the instruction after that is a call instruction.
             */
            uint32_t cbInsnMov1 = 1;
            uint32_t cbInsnMov2 = 1;
            uint32_t cbInsnMov3 = 1;
            uint32_t cbInsnCall = 1;
            uintptr_t PtrCallDst = 0;
            bool fValid = uiCommonQtRuntimePatchIsMovParam(pbCur, &cbInsnMov1);
            if (fValid)
                fValid = uiCommonQtRuntimePatchIsMovParam(pbCur + cbInsnMov1, &cbInsnMov2);
            if (fValid)
                fValid = uiCommonQtRuntimePatchIsMovParam(pbCur + cbInsnMov1 + cbInsnMov2, &cbInsnMov3);
            if (fValid)
                fValid = uiCommonQtRuntimePatchIsCall(pbCur + cbInsnMov1 + cbInsnMov2 + cbInsnMov3, &cbInsnCall, &PtrCallDst);

            if (fValid)
            {
                /* Allocate executable memory, assemble the patches and last make the area to patch writable and insert the trampoline. */
                g_pbExecMemory = (uint8_t *)RTMemExecAlloc(2 * (cbInsnMov1 + cbInsnMov2 + cbInsnMov3) + 128 /** @todo */);
                if (g_pbExecMemory)
                {
                    uint8_t *pbPatch = g_pbExecMemory;

                    /* Set up the parameters for our hook. */
                    memcpy(pbPatch, pbCur, cbInsnMov1 + cbInsnMov2 + cbInsnMov3);
                    pbPatch += cbInsnMov1 + cbInsnMov2 + cbInsnMov3;

                    /* Assemble our hook. */
                    *pbPatch++ = 0x48; /* mov rax, qword */
                    *pbPatch++ = 0xb8;
                    *(uintptr_t *)pbPatch = (uintptr_t)uiCommonQtTextBrowserPaintHook;
                    pbPatch += sizeof(uintptr_t);
                    *pbPatch++ = 0xff; /* call rax */
                    *pbPatch++ = 0xd0;

                    /* Assemble the original function call to d->paint(). */
                    memcpy(pbPatch, pbCur, cbInsnMov1 + cbInsnMov2 + cbInsnMov3);
                    pbPatch += cbInsnMov1 + cbInsnMov2 + cbInsnMov3;
                    *pbPatch++ = 0x48; /* mov rax, qword */
                    *pbPatch++ = 0xb8;
                    *(uintptr_t *)pbPatch = (uintptr_t)PtrCallDst;
                    pbPatch += sizeof(uintptr_t);
                    *pbPatch++ = 0xff; /* call rax */
                    *pbPatch++ = 0xd0;

                    /* Assemble the jump back to the original code. */
                    *pbPatch++ = 0x48; /* mov rax, qword */
                    *pbPatch++ = 0xb8;
                    *(uintptr_t *)pbPatch = (uintptr_t)(pbCur + 1 + 10 + 2); /* push rax; mov rax, dst; jmp rax. */
                    pbPatch += sizeof(uintptr_t);
                    *pbPatch++ = 0xff; /* jmp rax */
                    *pbPatch++ = 0xe0;

                    /* Finally insert our hook into the original code. */
                    int rc = uiCommonQtRuntimeQTextBrowserPatchHook(pbCur, cbInsnMov1 + cbInsnMov2 + cbInsnMov3 + cbInsnCall, (uintptr_t)g_pbExecMemory);
                    if (RT_FAILURE(rc))
                        LogFlow(("Failed to hook QTextBrowser::paintEvent() -> %Rrc\n", rc));
                }
                else
                    LogFlow(("Out of memory allocating executable memory\n"));
            }
            else
                LogFlow(("Instruction sequence not valid, ignoring\n"));
        }
        /* else: Failed to find the proper place to insert our patch into -> ignore and live with suboptimal results. */
    }
    return VINF_SUCCESS;
}


DECLHIDDEN(void) uiCommonQtRuntimePatch(void)
{
    RTOnce(&g_TextBrowserPatchingOnce, uiCommonQtRuntimeQTextBrowserPatch, NULL);
}

