/*
 *  i386 execution defines
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Sun LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */
#include "config.h"
#include "dyngen-exec.h"

/* XXX: factorize this mess */
#ifdef TARGET_X86_64
#define TARGET_LONG_BITS 64
#else
#define TARGET_LONG_BITS 32
#endif

#include "cpu-defs.h"

#ifndef VBOX
/* at least 4 register variables are defined */
register struct CPUX86State *env asm(AREG0);
#else
REGISTER_BOUND_GLOBAL(struct CPUX86State*, env, AREG0);
#endif /* VBOX */

#include "qemu-log.h"

#ifndef reg_EAX
#define EAX (env->regs[R_EAX])
#endif
#ifndef reg_ECX
#define ECX (env->regs[R_ECX])
#endif
#ifndef reg_EDX
#define EDX (env->regs[R_EDX])
#endif
#ifndef reg_EBX
#define EBX (env->regs[R_EBX])
#endif
#ifndef reg_ESP
#define ESP (env->regs[R_ESP])
#endif
#ifndef reg_EBP
#define EBP (env->regs[R_EBP])
#endif
#ifndef reg_ESI
#define ESI (env->regs[R_ESI])
#endif
#ifndef reg_EDI
#define EDI (env->regs[R_EDI])
#endif
#define EIP  (env->eip)
#define DF  (env->df)

#define CC_SRC (env->cc_src)
#define CC_DST (env->cc_dst)
#define CC_OP  (env->cc_op)

/* float macros */
#define FT0    (env->ft0)
#define ST0    (env->fpregs[env->fpstt].d)
#define ST(n)  (env->fpregs[(env->fpstt + (n)) & 7].d)
#define ST1    ST(1)

#include "cpu.h"
#include "exec-all.h"

void cpu_x86_update_cr3(CPUX86State *env, target_ulong new_cr3);
void cpu_x86_update_cr4(CPUX86State *env, uint32_t new_cr4);
int cpu_x86_handle_mmu_fault(CPUX86State *env, target_ulong addr,
                             int is_write, int mmu_idx, int is_softmmu);
void __hidden cpu_lock(void);
void __hidden cpu_unlock(void);
void do_interrupt(int intno, int is_int, int error_code,
                  target_ulong next_eip, int is_hw);
void do_interrupt_user(int intno, int is_int, int error_code,
                       target_ulong next_eip);
void raise_interrupt(int intno, int is_int, int error_code,
                     int next_eip_addend);
void raise_exception_err(int exception_index, int error_code);
void raise_exception(int exception_index);
void do_smm_enter(void);
void __hidden cpu_loop_exit(void);

void OPPROTO op_movl_eflags_T0(void);
void OPPROTO op_movl_T0_eflags(void);
#ifdef VBOX
void OPPROTO op_movl_T0_eflags_vme(void);
void OPPROTO op_movw_eflags_T0_vme(void);
void OPPROTO op_cli_vme(void);
void OPPROTO op_sti_vme(void);
#endif

/* n must be a constant to be efficient */
#ifndef VBOX
static inline target_long lshift(target_long x, int n)
#else
DECLINLINE(target_long) lshift(target_long x, int n)
#endif
{
    if (n >= 0)
        return x << n;
    else
        return x >> (-n);
}

#include "helper.h"

#ifndef VBOX
static inline void svm_check_intercept(uint32_t type)
#else
DECLINLINE(void) svm_check_intercept(uint32_t type)
#endif
{
    helper_svm_check_intercept_param(type, 0);
}

void check_iob_T0(void);
void check_iow_T0(void);
void check_iol_T0(void);
void check_iob_DX(void);
void check_iow_DX(void);
void check_iol_DX(void);

#if !defined(CONFIG_USER_ONLY)

#include "softmmu_exec.h"

#ifndef VBOX
static inline double ldfq(target_ulong ptr)
#else
DECLINLINE(double) ldfq(target_ulong ptr)
#endif
{
    union {
        double d;
        uint64_t i;
    } u;
    u.i = ldq(ptr);
    return u.d;
}

#ifndef VBOX 
static inline void stfq(target_ulong ptr, double v)
#else
DECLINLINE(void) stfq(target_ulong ptr, double v)
#endif
{
    union {
        double d;
        uint64_t i;
    } u;
    u.d = v;
    stq(ptr, u.i);
}

#ifndef VBOX
static inline float ldfl(target_ulong ptr)
#else
DECLINLINE(float) ldfl(target_ulong ptr)
#endif
{
    union {
        float f;
        uint32_t i;
    } u;
    u.i = ldl(ptr);
    return u.f;
}

#ifndef VBOX
static inline void stfl(target_ulong ptr, float v)
#else
DECLINLINE(void) stfl(target_ulong ptr, float v)
#endif
{
    union {
        float f;
        uint32_t i;
    } u;
    u.f = v;
    stl(ptr, u.i);
}

#endif /* !defined(CONFIG_USER_ONLY) */

#ifdef USE_X86LDOUBLE
/* use long double functions */
#define floatx_to_int32 floatx80_to_int32
#define floatx_to_int64 floatx80_to_int64
#define floatx_to_int32_round_to_zero floatx80_to_int32_round_to_zero
#define floatx_to_int64_round_to_zero floatx80_to_int64_round_to_zero
#define int32_to_floatx int32_to_floatx80
#define int64_to_floatx int64_to_floatx80
#define float32_to_floatx float32_to_floatx80
#define float64_to_floatx float64_to_floatx80
#define floatx_to_float32 floatx80_to_float32
#define floatx_to_float64 floatx80_to_float64
#define floatx_abs floatx80_abs
#define floatx_chs floatx80_chs
#define floatx_round_to_int floatx80_round_to_int
#define floatx_compare floatx80_compare
#define floatx_compare_quiet floatx80_compare_quiet
#ifdef VBOX
#undef sin
#undef cos
#undef sqrt
#undef pow
#undef log
#undef tan
#undef atan2
#undef floor
#undef ceil
#undef ldexp
#endif /* !VBOX */
#if !defined(VBOX) || !defined(_MSC_VER)
#define sin sinl
#define cos cosl
#define sqrt sqrtl
#define pow powl
#define log logl
#define tan tanl
#define atan2 atan2l
#define floor floorl
#define ceil ceill
#define ldexp ldexpl
#endif
#else
#define floatx_to_int32 float64_to_int32
#define floatx_to_int64 float64_to_int64
#define floatx_to_int32_round_to_zero float64_to_int32_round_to_zero
#define floatx_to_int64_round_to_zero float64_to_int64_round_to_zero
#define int32_to_floatx int32_to_float64
#define int64_to_floatx int64_to_float64
#define float32_to_floatx float32_to_float64
#define float64_to_floatx(x, e) (x)
#define floatx_to_float32 float64_to_float32
#define floatx_to_float64(x, e) (x)
#define floatx_abs float64_abs
#define floatx_chs float64_chs
#define floatx_round_to_int float64_round_to_int
#define floatx_compare float64_compare
#define floatx_compare_quiet float64_compare_quiet
#endif

#ifdef VBOX
#ifndef _MSC_VER
extern CPU86_LDouble sin(CPU86_LDouble x);
extern CPU86_LDouble cos(CPU86_LDouble x);
extern CPU86_LDouble sqrt(CPU86_LDouble x);
extern CPU86_LDouble pow(CPU86_LDouble, CPU86_LDouble);
extern CPU86_LDouble log(CPU86_LDouble x);
extern CPU86_LDouble tan(CPU86_LDouble x);
extern CPU86_LDouble atan2(CPU86_LDouble, CPU86_LDouble);
extern CPU86_LDouble floor(CPU86_LDouble x);
extern CPU86_LDouble ceil(CPU86_LDouble x);
#endif /* !_MSC_VER */
#endif /* VBOX */

#define RC_MASK         0xc00
#ifndef RC_NEAR
#define RC_NEAR		0x000
#endif
#ifndef RC_DOWN
#define RC_DOWN		0x400
#endif
#ifndef RC_UP
#define RC_UP		0x800
#endif
#ifndef RC_CHOP
#define RC_CHOP		0xc00
#endif

#define MAXTAN 9223372036854775808.0

#ifdef USE_X86LDOUBLE

/* only for x86 */
typedef union {
    long double d;
    struct {
        unsigned long long lower;
        unsigned short upper;
    } l;
} CPU86_LDoubleU;

/* the following deal with x86 long double-precision numbers */
#define MAXEXPD 0x7fff
#define EXPBIAS 16383
#define EXPD(fp)	(fp.l.upper & 0x7fff)
#define SIGND(fp)	((fp.l.upper) & 0x8000)
#define MANTD(fp)       (fp.l.lower)
#define BIASEXPONENT(fp) fp.l.upper = (fp.l.upper & ~(0x7fff)) | EXPBIAS

#else

/* NOTE: arm is horrible as double 32 bit words are stored in big endian ! */
typedef union {
    double d;
#if !defined(WORDS_BIGENDIAN) && !defined(__arm__)
    struct {
        uint32_t lower;
        int32_t upper;
    } l;
#else
    struct {
        int32_t upper;
        uint32_t lower;
    } l;
#endif
#ifndef __arm__
    int64_t ll;
#endif
} CPU86_LDoubleU;

/* the following deal with IEEE double-precision numbers */
#define MAXEXPD 0x7ff
#define EXPBIAS 1023
#define EXPD(fp)	(((fp.l.upper) >> 20) & 0x7FF)
#define SIGND(fp)	((fp.l.upper) & 0x80000000)
#ifdef __arm__
#define MANTD(fp)	(fp.l.lower | ((uint64_t)(fp.l.upper & ((1 << 20) - 1)) << 32))
#else
#define MANTD(fp)	(fp.ll & ((1LL << 52) - 1))
#endif
#define BIASEXPONENT(fp) fp.l.upper = (fp.l.upper & ~(0x7ff << 20)) | (EXPBIAS << 20)
#endif

#ifndef VBOX
static inline void fpush(void)
#else
DECLINLINE(void) fpush(void)
#endif
{
    env->fpstt = (env->fpstt - 1) & 7;
    env->fptags[env->fpstt] = 0; /* validate stack entry */
}

#ifndef VBOX
static inline void fpop(void)
#else
DECLINLINE(void) fpop(void)
#endif
{
    env->fptags[env->fpstt] = 1; /* invvalidate stack entry */
    env->fpstt = (env->fpstt + 1) & 7;
}

#ifndef USE_X86LDOUBLE
static inline CPU86_LDouble helper_fldt(target_ulong ptr)
{
    CPU86_LDoubleU temp;
    int upper, e;
    uint64_t ll;

    /* mantissa */
    upper = lduw(ptr + 8);
    /* XXX: handle overflow ? */
    e = (upper & 0x7fff) - 16383 + EXPBIAS; /* exponent */
    e |= (upper >> 4) & 0x800; /* sign */
    ll = (ldq(ptr) >> 11) & ((1LL << 52) - 1);
#ifdef __arm__
    temp.l.upper = (e << 20) | (ll >> 32);
    temp.l.lower = ll;
#else
    temp.ll = ll | ((uint64_t)e << 52);
#endif
    return temp.d;
}

static inline void helper_fstt(CPU86_LDouble f, target_ulong ptr)
{
    CPU86_LDoubleU temp;
    int e;

    temp.d = f;
    /* mantissa */
    stq(ptr, (MANTD(temp) << 11) | (1LL << 63));
    /* exponent + sign */
    e = EXPD(temp) - EXPBIAS + 16383;
    e |= SIGND(temp) >> 16;
    stw(ptr + 8, e);
}
#else

/* XXX: same endianness assumed */

#ifdef CONFIG_USER_ONLY

static inline CPU86_LDouble helper_fldt(target_ulong ptr)
{
    return *(CPU86_LDouble *)ptr;
}

static inline void helper_fstt(CPU86_LDouble f, target_ulong ptr)
{
    *(CPU86_LDouble *)ptr = f;
}

#else

/* we use memory access macros */

#ifndef VBOX
static inline CPU86_LDouble helper_fldt(target_ulong ptr)
#else
DECLINLINE(CPU86_LDouble) helper_fldt(target_ulong ptr)
#endif
{
    CPU86_LDoubleU temp;

    temp.l.lower = ldq(ptr);
    temp.l.upper = lduw(ptr + 8);
    return temp.d;
}

#ifndef VBOX
static inline void helper_fstt(CPU86_LDouble f, target_ulong ptr)
#else
DECLINLINE(void) helper_fstt(CPU86_LDouble f, target_ulong ptr)
#endif
{
    CPU86_LDoubleU temp;
    
    temp.d = f;
    stq(ptr, temp.l.lower);
    stw(ptr + 8, temp.l.upper);
}

#endif /* !CONFIG_USER_ONLY */

#endif /* USE_X86LDOUBLE */

#define FPUS_IE (1 << 0)
#define FPUS_DE (1 << 1)
#define FPUS_ZE (1 << 2)
#define FPUS_OE (1 << 3)
#define FPUS_UE (1 << 4)
#define FPUS_PE (1 << 5)
#define FPUS_SF (1 << 6)
#define FPUS_SE (1 << 7)
#define FPUS_B  (1 << 15)

#define FPUC_EM 0x3f

extern const CPU86_LDouble f15rk[7];

void fpu_raise_exception(void);
void restore_native_fp_state(CPUState *env);
void save_native_fp_state(CPUState *env);

extern const uint8_t parity_table[256];
extern const uint8_t rclw_table[32];
extern const uint8_t rclb_table[32];

#ifndef VBOX
static inline uint32_t compute_eflags(void)
#else
DECLINLINE(uint32_t) compute_eflags(void)
#endif
{
    return env->eflags | cc_table[CC_OP].compute_all() | (DF & DF_MASK);
}

/* NOTE: CC_OP must be modified manually to CC_OP_EFLAGS */
#ifndef VBOX
static inline void load_eflags(int eflags, int update_mask)
#else
DECLINLINE(void) load_eflags(int eflags, int update_mask)
#endif
{
    CC_SRC = eflags & (CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
    DF = 1 - (2 * ((eflags >> 10) & 1));
    env->eflags = (env->eflags & ~update_mask) | 
        (eflags & update_mask);
}

#ifndef VBOX
static inline void env_to_regs(void)
#else
DECLINLINE(void) env_to_regs(void)
#endif
{
#ifdef reg_EAX
    EAX = env->regs[R_EAX];
#endif
#ifdef reg_ECX
    ECX = env->regs[R_ECX];
#endif
#ifdef reg_EDX
    EDX = env->regs[R_EDX];
#endif
#ifdef reg_EBX
    EBX = env->regs[R_EBX];
#endif
#ifdef reg_ESP
    ESP = env->regs[R_ESP];
#endif
#ifdef reg_EBP
    EBP = env->regs[R_EBP];
#endif
#ifdef reg_ESI
    ESI = env->regs[R_ESI];
#endif
#ifdef reg_EDI
    EDI = env->regs[R_EDI];
#endif
}

#ifndef VBOX
static inline void regs_to_env(void)
#else
DECLINLINE(void) regs_to_env(void)
#endif
{
#ifdef reg_EAX
    env->regs[R_EAX] = EAX;
#endif
#ifdef reg_ECX
    env->regs[R_ECX] = ECX;
#endif
#ifdef reg_EDX
    env->regs[R_EDX] = EDX;
#endif
#ifdef reg_EBX
    env->regs[R_EBX] = EBX;
#endif
#ifdef reg_ESP
    env->regs[R_ESP] = ESP;
#endif
#ifdef reg_EBP
    env->regs[R_EBP] = EBP;
#endif
#ifdef reg_ESI
    env->regs[R_ESI] = ESI;
#endif
#ifdef reg_EDI
    env->regs[R_EDI] = EDI;
#endif
}

#ifndef VBOX
static inline int cpu_halted(CPUState *env) {
#else
DECLINLINE(int) cpu_halted(CPUState *env) {
#endif
    /* handle exit of HALTED state */
    if (!env->halted)
        return 0;
    /* disable halt condition */
    if (((env->interrupt_request & CPU_INTERRUPT_HARD) &&
         (env->eflags & IF_MASK)) ||
        (env->interrupt_request & CPU_INTERRUPT_NMI)) {
        env->halted = 0;
        return 0;
    }
    return EXCP_HALTED;
}

/* load efer and update the corresponding hflags. XXX: do consistency
   checks with cpuid bits ? */
#ifndef VBOX
static inline void cpu_load_efer(CPUState *env, uint64_t val)
#else
DECLINLINE(void) cpu_load_efer(CPUState *env, uint64_t val)
#endif
{
    env->efer = val;
    env->hflags &= ~(HF_LMA_MASK | HF_SVME_MASK);
    if (env->efer & MSR_EFER_LMA)
        env->hflags |= HF_LMA_MASK;
    if (env->efer & MSR_EFER_SVME)
        env->hflags |= HF_SVME_MASK;
}
