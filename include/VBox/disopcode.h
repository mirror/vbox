/** @file
 * Disassembler - opcode.h.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_opcode_h
#define ___VBox_opcode_h

#define MODRM_MOD(a)    (a>>6)
#define MODRM_REG(a)    ((a>>3)&0x7)
#define MODRM_RM(a)     (a&0x7)
#define MAKE_MODRM(mod, reg, rm) (((mod&3) << 6) | ((reg&7) << 3) | (rm&7))

#define SIB_SCALE(a)    (a>>6)
#define SIB_INDEX(a)    ((a>>3)&0x7)
#define SIB_BASE(a)     (a&0x7)


/** Full Intel X86 opcode list
 * @{
 */
#define OP_INVALID      0
#define OP_OPSIZE       1
#define OP_ADDRSIZE     2
#define OP_SEG          3
#define OP_REPNE        4
#define OP_REPE         5
#define OP_REX          6
#define OP_LOCK         7
#define OP_LAST_PREFIX  OP_LOCK   /* disassembler assumes this is the last prefix byte value!!!! */
#define OP_AND          8
#define OP_OR           9
#define OP_DAA          10
#define OP_SUB          11
#define OP_DAS          12
#define OP_XOR          13
#define OP_AAA          14
#define OP_CMP          15
#define OP_IMM_GRP1     16
#define OP_AAS          17
#define OP_INC          18
#define OP_DEC          19
#define OP_PUSHA        20
#define OP_POPA         21
#define OP_BOUND        22
#define OP_ARPL         23
#define OP_PUSH         24
#define OP_POP          25
#define OP_IMUL         26
#define OP_INSB         27
#define OP_INSWD        28
#define OP_OUTSB        29
#define OP_OUTSWD       30
#define OP_JO           31
#define OP_JNO          32
#define OP_JC           33
#define OP_JNC          34
#define OP_JE           35
#define OP_JNE          36
#define OP_JBE          37
#define OP_JNBE         38
#define OP_JS           39
#define OP_JNS          40
#define OP_JP           41
#define OP_JNP          42
#define OP_JL           43
#define OP_JNL          44
#define OP_JLE          45
#define OP_JNLE         46
#define OP_ADD          47
#define OP_TEST         48
#define OP_XCHG         49
#define OP_MOV          50
#define OP_LEA          51
#define OP_NOP          52
#define OP_CBW          53
#define OP_CWD          54
#define OP_CALL         55
#define OP_WAIT         56
#define OP_PUSHF        57
#define OP_POPF         58
#define OP_SAHF         59
#define OP_LAHF         60
#define OP_MOVSB        61
#define OP_MOVSWD       62
#define OP_CMPSB        63
#define OP_CMPWD        64
#define OP_STOSB        65
#define OP_STOSWD       66
#define OP_LODSB        67
#define OP_LODSWD       68
#define OP_SCASB        69
#define OP_SCASWD       70
#define OP_SHIFT_GRP2   71
#define OP_RETN         72
#define OP_LES          73
#define OP_LDS          74
#define OP_ENTER        75
#define OP_LEAVE        76
#define OP_RETF         77
#define OP_INT3         78
#define OP_INT          79
#define OP_INTO         80
#define OP_IRET         81
#define OP_AAM          82
#define OP_AAD          83
#define OP_XLAT         84
#define OP_ESCF0        85
#define OP_ESCF1        86
#define OP_ESCF2        87
#define OP_ESCF3        88
#define OP_ESCF4        89
#define OP_ESCF5        90
#define OP_ESCF6        91
#define OP_ESCF7        92
#define OP_LOOPNE       93
#define OP_LOOPE        94
#define OP_LOOP         95
#define OP_JECXZ        96
#define OP_IN           97
#define OP_OUT          98
#define OP_JMP          99
#define OP_2B_ESC       100
#define OP_ADC          101
#define OP_SBB          102
#define OP_HLT          103
#define OP_CMC          104
#define OP_UNARY_GRP3   105
#define OP_CLC          106
#define OP_STC          107
#define OP_CLI          108
#define OP_STI          109
#define OP_CLD          110
#define OP_STD          111
#define OP_INC_GRP4     112
#define OP_IND_GRP5     113
#define OP_GRP6         114
#define OP_GRP7         115
#define OP_LAR          116
#define OP_LSL          117
#define OP_SYSCALL      118
#define OP_CLTS         119
#define OP_SYSRET       120
#define OP_INVD         121
#define OP_WBINVD       122
#define OP_ILLUD2       123
#define OP_FEMMS        124
#define OP_3DNOW        125
#define OP_MOVUPS       126
#define OP_MOVLPS       127
#define OP_UNPCKLPS     128
#define OP_MOVHPS       129
#define OP_UNPCKHPS     130
#define OP_PREFETCH_GRP16   131
#define OP_MOV_CR       132
#define OP_MOVAPS       133
#define OP_CVTPI2PS     134
#define OP_MOVNTPS      135
#define OP_CVTTPS2PI    136
#define OP_CVTPS2PI     137
#define OP_UCOMISS      138
#define OP_COMISS       139
#define OP_WRMSR        140
#define OP_RDTSC        141
#define OP_RDMSR        142
#define OP_RPPMC        143
#define OP_SYSENTER     144
#define OP_SYSEXIT      145
#define OP_PAUSE        146
#define OP_CMOVO        147
#define OP_CMOVNO       148
#define OP_CMOVC        149
#define OP_CMOVNC       150
#define OP_CMOVZ        151
#define OP_CMOVNZ       152
#define OP_CMOVBE       153
#define OP_CMOVNBE      154
#define OP_CMOVS        155
#define OP_CMOVNS       156
#define OP_CMOVP        157
#define OP_CMOVNP       158
#define OP_CMOVL        159
#define OP_CMOVNL       160
#define OP_CMOVLE       161
#define OP_CMOVNLE      162
#define OP_MOVMSKPS     163
#define OP_SQRTPS       164
#define OP_RSQRTPS      165
#define OP_RCPPS        166
#define OP_ANDPS        167
#define OP_ANDNPS       168
#define OP_ORPS         169
#define OP_XORPS        170
#define OP_ADDPS        171
#define OP_MULPS        172
#define OP_CVTPS2PD     173
#define OP_CVTDQ2PS     174
#define OP_SUBPS        175
#define OP_MINPS        176
#define OP_DIVPS        177
#define OP_MAXPS        178
#define OP_PUNPCKLBW    179
#define OP_PUNPCKLWD    180
#define OP_PUNPCKLDQ    181
#define OP_PACKSSWB     182
#define OP_PCMPGTB      183
#define OP_PCMPGTW      184
#define OP_PCMPGTD      185
#define OP_PACKUSWB     186
#define OP_PUNPCKHBW    187
#define OP_PUNPCKHWD    188
#define OP_PUNPCKHDQ    189
#define OP_PACKSSDW     190
#define OP_MOVD         191
#define OP_MOVQ         192
#define OP_PSHUFW       193

#define OP_PCMPEQB      196
#define OP_PCMPEQW      197
#define OP_PCMPEQD      198
#define OP_SETO         199
#define OP_SETNO        200
#define OP_SETC         201
#define OP_SETNC        202
#define OP_SETE         203
#define OP_SETNE        204
#define OP_SETBE        205
#define OP_SETNBE       206
#define OP_SETS         207
#define OP_SETNS        208
#define OP_SETP         209
#define OP_SETNP        210
#define OP_SETL         211
#define OP_SETNL        212
#define OP_SETLE        213
#define OP_SETNLE       214
#define OP_CPUID        215
#define OP_BT           216
#define OP_SHLD         217
#define OP_RSM          218
#define OP_BTS          219
#define OP_SHRD         220
#define OP_GRP15        221
#define OP_CMPXCHG      222
#define OP_LSS          223
#define OP_BTR          224
#define OP_LFS          225
#define OP_LGS          226
#define OP_MOVZX        227
#define OP_GRP10_INV    228
#define OP_GRP8         229
#define OP_BTC          230
#define OP_BSF          231
#define OP_BSR          232
#define OP_MOVSX        233
#define OP_XADD         234
#define OP_CMPPS        235
#define OP_MOVNTI       236
#define OP_PINSRW       237
#define OP_PEXTRW       238
#define OP_SHUFPS       239
#define OP_GRP9         240
#define OP_BSWAP        241
#define OP_PSRLW        242
#define OP_PSRLD        243
#define OP_PSRLQ        244
#define OP_PADDQ        245
#define OP_PMULLW       246
#define OP_PMOVSKB      247
#define OP_PSUBUSB      248
#define OP_PSUBUSW      249
#define OP_PMINUB       250
#define OP_PAND         251
#define OP_PADDUSB      252
#define OP_PADDUSW      253
#define OP_PMAXUB       254
#define OP_PANDN        255
#define OP_PAVGN        256
#define OP_PSRAW        257
#define OP_PSRAD        258
#define OP_PAVGW        259
#define OP_PMULHUW      260
#define OP_PMULHW       261
#define OP_MOVNTQ       262
#define OP_PSUBSB       263
#define OP_PSUBSW       264
#define OP_PMINSW       265
#define OP_POR          266
#define OP_PADDSB       267
#define OP_PADDSW       268
#define OP_PMAXSW       269
#define OP_PXOR         270
#define OP_PSLLW        271
#define OP_PSLLD        272
#define OP_PSSQ         273
#define OP_PMULUDQ      274
#define OP_PADDWD       275
#define OP_PADBW        276
#define OP_PMASKMOVQ    277
#define OP_PSUBB        278
#define OP_PSUBW        279

#define OP_PSUBD        281
#define OP_PADDB        282
#define OP_PADDW        283
#define OP_PADDD        284
#define OP_MOVUPD       285
#define OP_MOVLPD       286
#define OP_UNPCKLPD     287
#define OP_UNPCKHPD     288
#define OP_MOVHPD       289

#define OP_MOVAPD       291
#define OP_CVTPI2PD     292
#define OP_MOVNTPD      293
#define OP_CVTTPD2PI    294
#define OP_CVTPD2PI     295
#define OP_UCOMISD      296
#define OP_COMISD       297
#define OP_MOVMSKPD     298
#define OP_SQRTPD       299
#define OP_ANDPD        301
#define OP_ANDNPD       302
#define OP_ORPD         303
#define OP_XORPD        304
#define OP_ADDPD        305
#define OP_MULPD        306
#define OP_CVTPD2PS     307
#define OP_CVTPS2DQ     308
#define OP_SUBPD        309
#define OP_MINPD        310
#define OP_DIVPD        311
#define OP_MAXPD        312

#define OP_GRP12        313
#define OP_GRP13        314
#define OP_GRP14        315
#define OP_EMMS         316
#define OP_MMX_UD78     317
#define OP_MMX_UD79     318
#define OP_MMX_UD7A     319
#define OP_MMX_UD7B     320
#define OP_MMX_UD7C     321
#define OP_MMX_UD7D     322


#define OP_PUNPCKLQDQ   325
#define OP_PUNPCKHQD    326

#define OP_MOVDQA       328
#define OP_PSHUFD       329



#define OP_CMPPD        334
#define OP_SHUFPD       337


#define OP_CVTTPD2DQ    353
#define OP_MOVNTDQ      354

#define OP_PMASKMOVDQU  369




#define OP_MOVSD        377
#define OP_CVTSI2SD     378
#define OP_CVTTSD2SI    379
#define OP_CVTSD2SI     380
#define OP_SQRTSD       381
#define OP_ADDSD        382
#define OP_MULSD        383
#define OP_CVTSD2SS     384
#define OP_SUBSD        385
#define OP_MINSD        386
#define OP_DIVSD        387
#define OP_MAXSD        388
#define OP_PSHUFLW      389
#define OP_CMPSD        390
#define OP_MOVDQ2Q      391
#define OP_CVTPD2DQ     392
#define OP_MOVSS        393
#define OP_CVTSI2SS     394
#define OP_CVTTSS2SI    395
#define OP_CVTSS2SI     396
#define OP_SQRTSS       397
#define OP_RSQRTSS      398
#define OP_ADDSS        399
#define OP_MULSS        401
#define OP_CVTTPS2DQ    403
#define OP_SUBSS        404
#define OP_MINSS        405
#define OP_DIVSS        406
#define OP_MAXSS        407
#define OP_MOVDQU       408
#define OP_PSHUFHW      409
#define OP_CMPSS        410
#define OP_MOVQ2DQ      411
#define OP_CVTDQ2PD     412
/** @} */

/** Floating point ops
 * @{
 */
#define OP_FADD         413
#define OP_FMUL         414
#define OP_FCOM         415
#define OP_FCOMP        416
#define OP_FSUB         417
#define OP_FSUBR        418
#define OP_FDIV         419
#define OP_FDIVR        420
#define OP_FLD          421
#define OP_FST          422
#define OP_FSTP         423
#define OP_FLDENV       424

#define OP_FSTENV       426
#define OP_FSTCW        427
#define OP_FXCH         428
#define OP_FNOP         429
#define OP_FCHS         430
#define OP_FABS         431

#define OP_FLD1         433
#define OP_FLDL2T       434
#define OP_FLDL2E       435
#define OP_FLDPI        436
#define OP_FLDLG2       437
#define OP_FLDLN2       438
#define OP_FLDZ         439
#define OP_F2XM1        440
#define OP_FYL2X        441
#define OP_FPTAN        442
#define OP_FPATAN       443
#define OP_FXTRACT      444
#define OP_FREM1        445
#define OP_FDECSTP      446
#define OP_FINCSTP      447
#define OP_FPREM        448
#define OP_FYL2XP1      449
#define OP_FSQRT        450
#define OP_FSINCOS      451
#define OP_FRNDINT      452
#define OP_FSCALE       453
#define OP_FSIN         454
#define OP_FCOS         455
#define OP_FIADD        456
#define OP_FIMUL        457
#define OP_FISUB        460
#define OP_FISUBR       461
#define OP_FIDIV        462
#define OP_FIDIVR       463
#define OP_FCMOVB       464
#define OP_FCMOVE       465
#define OP_FCMOVBE      466
#define OP_FCMOVU       467
#define OP_FUCOMPP      468
#define OP_FILD         469
#define OP_FIST         470
#define OP_FISTP        471
#define OP_FCMOVNB      474
#define OP_FCMOVNE      475
#define OP_FCMOVNBE     476
#define OP_FCMOVNU      477
#define OP_FCLEX        478
#define OP_FINIT        479
#define OP_FUCOMI       480
#define OP_FCOMI        481
#define OP_FRSTOR       482
#define OP_FSAVE        483
#define OP_FNSTSW       484
#define OP_FFREE        485
#define OP_FUCOM        486
#define OP_FUCOMP       487
#define OP_FICOM        490
#define OP_FICOMP       491
#define OP_FADDP        496
#define OP_FMULP        497
#define OP_FCOMPP       498
#define OP_FSUBRP       499
#define OP_FSUBP        500
#define OP_FDIVRP       501
#define OP_FDIVP        502
#define OP_FBLD         503
#define OP_FBSTP        504
#define OP_FCOMIP       506
#define OP_FUCOMIP      507
/** @} */

/** 3DNow!
 * @{
 */
#define OP_PI2FW        508
#define OP_PI2FD        509
#define OP_PF2IW        510
#define OP_PF2ID        511
#define OP_PFPNACC      512
#define OP_PFCMPGE      513
#define OP_PFMIN        514
#define OP_PFRCP        515
#define OP_PFRSQRT      516
#define OP_PFSUB        517
#define OP_PFADD        518
#define OP_PFCMPGT      519
#define OP_PFMAX        520
#define OP_PFRCPIT1     521
#define OP_PFRSQRTIT1   522
#define OP_PFSUBR       523
#define OP_PFACC        524
#define OP_PFCMPEQ      525
#define OP_PFMUL        526
#define OP_PFRCPIT2     527
#define OP_PFMULHRW     528
#define OP_PFSWAPD      529
#define OP_PAVGUSB      530
#define OP_PFNACC       531
#define OP_ROL          532
#define OP_ROR          533
#define OP_RCL          534
#define OP_RCR          535
#define OP_SHL          536
#define OP_SHR          537
#define OP_SAR          538
#define OP_NOT          539
#define OP_NEG          540
#define OP_MUL          541
#define OP_DIV          542
#define OP_IDIV         543
#define OP_SLDT         544
#define OP_STR          545
#define OP_LLDT         546
#define OP_LTR          547
#define OP_VERR         548
#define OP_VERW         549
#define OP_SGDT         550
#define OP_LGDT         551
#define OP_SIDT         552
#define OP_LIDT         553
#define OP_SMSW         554
#define OP_LMSW         555
#define OP_INVLPG       556
#define OP_CMPXCHG8B    557
#define OP_PSLLQ        558
#define OP_PSRLDQ       559
#define OP_PSLLDQ       560
#define OP_FXSAVE       561
#define OP_FXRSTOR      562
#define OP_LDMXCSR      563
#define OP_STMXCSR      564
#define OP_LFENCE       565
#define OP_MFENCE       566
#define OP_SFENCE       567
#define OP_PREFETCH     568
#define OP_MONITOR      569
#define OP_MWAIT        570
#define OP_CLFLUSH      571

#define OP_MOV_DR       600
#define OP_MOV_TR       601

/* VT-x instructions */
#define OP_VMREAD       650
#define OP_VMWRITE      651
#define OP_VMCALL       652
#define OP_VMXON        653
#define OP_VMXOFF       654
#define OP_VMCLEAR      655
#define OP_VMLAUNCH     656
#define OP_VMRESUME     657
#define OP_VMPTRLD      658
#define OP_VMPTRST      659

/* 64 bits instruction */
#define OP_MOVSXD       700

/** @} */


/** Opcode parameters
 * @{
 */
/* NOTE: Register order is important for translations!! */
#define OP_PARM_NONE            0
#define OP_PARM_REG_EAX         1
#define OP_PARM_REG_GEN32_START OP_PARM_REG_EAX
#define OP_PARM_REG_ECX         2
#define OP_PARM_REG_EDX         3
#define OP_PARM_REG_EBX         4
#define OP_PARM_REG_ESP         5
#define OP_PARM_REG_EBP         6
#define OP_PARM_REG_ESI         7
#define OP_PARM_REG_EDI         8
#define OP_PARM_REG_GEN32_END   OP_PARM_REG_EDI

#define OP_PARM_REG_ES          9
#define OP_PARM_REG_SEG_START   OP_PARM_REG_ES
#define OP_PARM_REG_CS          10
#define OP_PARM_REG_SS          11
#define OP_PARM_REG_DS          12
#define OP_PARM_REG_FS          13
#define OP_PARM_REG_GS          14
#define OP_PARM_REG_SEG_END     OP_PARM_REG_GS

#define OP_PARM_REG_AX          15
#define OP_PARM_REG_GEN16_START   OP_PARM_REG_AX
#define OP_PARM_REG_CX          16
#define OP_PARM_REG_DX          17
#define OP_PARM_REG_BX          18
#define OP_PARM_REG_SP          19
#define OP_PARM_REG_BP          20
#define OP_PARM_REG_SI          21
#define OP_PARM_REG_DI          22
#define OP_PARM_REG_GEN16_END   OP_PARM_REG_DI

#define OP_PARM_REG_AL          23
#define OP_PARM_REG_GEN8_START  OP_PARM_REG_AL
#define OP_PARM_REG_CL          24
#define OP_PARM_REG_DL          25
#define OP_PARM_REG_BL          26
#define OP_PARM_REG_AH          27
#define OP_PARM_REG_CH          28
#define OP_PARM_REG_DH          29
#define OP_PARM_REG_BH          30
#define OP_PARM_REG_GEN8_END    OP_PARM_REG_BH

#define OP_PARM_REGFP_0         31
#define OP_PARM_REG_FP_START    OP_PARM_REGFP_0
#define OP_PARM_REGFP_1         32
#define OP_PARM_REGFP_2         33
#define OP_PARM_REGFP_3         34
#define OP_PARM_REGFP_4         35
#define OP_PARM_REGFP_5         36
#define OP_PARM_REGFP_6         37
#define OP_PARM_REGFP_7         38
#define OP_PARM_REG_FP_END      OP_PARM_REGFP_7

#define OP_PARM_NTA             39
#define OP_PARM_T0              40
#define OP_PARM_T1              41
#define OP_PARM_T2              42

#define OP_PARM_1               43

#define OP_PARM_REX             50
#define OP_PARM_REX_START       OP_PARM_REX
#define OP_PARM_REX_B           51
#define OP_PARM_REX_X           52
#define OP_PARM_REX_XB          53
#define OP_PARM_REX_R           54
#define OP_PARM_REX_RB          55
#define OP_PARM_REX_RX          56
#define OP_PARM_REX_RXB         57
#define OP_PARM_REX_W           58
#define OP_PARM_REX_WB          59
#define OP_PARM_REX_WX          60
#define OP_PARM_REX_WXB         61
#define OP_PARM_REX_WR          62
#define OP_PARM_REX_WRB         63
#define OP_PARM_REX_WRX         64
#define OP_PARM_REX_WRXB        65

#define OP_PARM_REG_RAX         100
#define OP_PARM_REG_GEN64_START OP_PARM_REG_RAX
#define OP_PARM_REG_RCX         101
#define OP_PARM_REG_RDX         102
#define OP_PARM_REG_RBX         103
#define OP_PARM_REG_RSP         104
#define OP_PARM_REG_RBP         105
#define OP_PARM_REG_RSI         106
#define OP_PARM_REG_RDI         107
#define OP_PARM_REG_R8          108
#define OP_PARM_REG_R9          109
#define OP_PARM_REG_R10         110
#define OP_PARM_REG_R11         111
#define OP_PARM_REG_R12         112
#define OP_PARM_REG_R13         113
#define OP_PARM_REG_R14         114
#define OP_PARM_REG_R15         115
#define OP_PARM_REG_GEN64_END   OP_PARM_REG_R15


#define OP_PARM_VTYPE(a)        ((int)a & 0xFE0)
#define OP_PARM_VSUBTYPE(a)     ((int)a & 0x01F)

#define OP_PARM_A               0x100
#define OP_PARM_VARIABLE        OP_PARM_A
#define OP_PARM_E               0x120
#define OP_PARM_F               0x140
#define OP_PARM_G               0x160
#define OP_PARM_I               0x180
#define OP_PARM_J               0x1A0
#define OP_PARM_M               0x1C0
#define OP_PARM_O               0x1E0
#define OP_PARM_Q               0x200
#define OP_PARM_R               0x220
#define OP_PARM_X               0x240
#define OP_PARM_Y               0x260

/* Grouped rare parameters for optimization purposes */
#define IS_OP_PARM_RARE(a)      ((a & 0xF00) == 0x300)
#define OP_PARM_C               0x300       /* control register */
#define OP_PARM_D               0x320       /* debug register */
#define OP_PARM_S               0x340       /* segment register */
#define OP_PARM_T               0x360       /* test register */
#define OP_PARM_P               0x380       /* mmx register */
#define OP_PARM_W               0x3A0       /* xmm register */
#define OP_PARM_V               0x3C0

#define OP_PARM_NONE            0
#define OP_PARM_a               0x1
#define OP_PARM_b               0x2
#define OP_PARM_d               0x3
#define OP_PARM_dq              0x4
#define OP_PARM_p               0x5
#define OP_PARM_pd              0x6
#define OP_PARM_pi              0x7
#define OP_PARM_ps              0x8
#define OP_PARM_pq              0x9
#define OP_PARM_q               0xA
#define OP_PARM_s               0xB
#define OP_PARM_sd              0xC
#define OP_PARM_ss              0xD
#define OP_PARM_v               0xE
#define OP_PARM_w               0xF
#define OP_PARM_z               0x10


#define OP_PARM_Ap              (OP_PARM_A+OP_PARM_p)
#define OP_PARM_Cd              (OP_PARM_C+OP_PARM_d)
#define OP_PARM_Dd              (OP_PARM_D+OP_PARM_d)
#define OP_PARM_Eb              (OP_PARM_E+OP_PARM_b)
#define OP_PARM_Ed              (OP_PARM_E+OP_PARM_d)
#define OP_PARM_Ep              (OP_PARM_E+OP_PARM_p)
#define OP_PARM_Ev              (OP_PARM_E+OP_PARM_v)
#define OP_PARM_Ew              (OP_PARM_E+OP_PARM_w)
#define OP_PARM_Fv              (OP_PARM_F+OP_PARM_v)
#define OP_PARM_Gb              (OP_PARM_G+OP_PARM_b)
#define OP_PARM_Gd              (OP_PARM_G+OP_PARM_d)
#define OP_PARM_Gv              (OP_PARM_G+OP_PARM_v)
#define OP_PARM_Gw              (OP_PARM_G+OP_PARM_w)
#define OP_PARM_Ib              (OP_PARM_I+OP_PARM_b)
#define OP_PARM_Id              (OP_PARM_I+OP_PARM_d)
#define OP_PARM_Iq              (OP_PARM_I+OP_PARM_q)
#define OP_PARM_Iw              (OP_PARM_I+OP_PARM_w)
#define OP_PARM_Iv              (OP_PARM_I+OP_PARM_v)
#define OP_PARM_Iz              (OP_PARM_I+OP_PARM_z)
#define OP_PARM_Jb              (OP_PARM_J+OP_PARM_b)
#define OP_PARM_Jv              (OP_PARM_J+OP_PARM_v)
#define OP_PARM_Ma              (OP_PARM_M+OP_PARM_a)
#define OP_PARM_Mb              (OP_PARM_M+OP_PARM_b)
#define OP_PARM_Mw              (OP_PARM_M+OP_PARM_w)
#define OP_PARM_Md              (OP_PARM_M+OP_PARM_d)
#define OP_PARM_Mp              (OP_PARM_M+OP_PARM_p)
#define OP_PARM_Mq              (OP_PARM_M+OP_PARM_q)
#define OP_PARM_Ms              (OP_PARM_M+OP_PARM_s)
#define OP_PARM_Ob              (OP_PARM_O+OP_PARM_b)
#define OP_PARM_Ov              (OP_PARM_O+OP_PARM_v)
#define OP_PARM_Pq              (OP_PARM_P+OP_PARM_q)
#define OP_PARM_Pd              (OP_PARM_P+OP_PARM_d)
#define OP_PARM_Qd              (OP_PARM_Q+OP_PARM_d)
#define OP_PARM_Qq              (OP_PARM_Q+OP_PARM_q)
#define OP_PARM_Rd              (OP_PARM_R+OP_PARM_d)
#define OP_PARM_Rw              (OP_PARM_R+OP_PARM_w)
#define OP_PARM_Sw              (OP_PARM_S+OP_PARM_w)
#define OP_PARM_Td              (OP_PARM_T+OP_PARM_d)
#define OP_PARM_Vq              (OP_PARM_V+OP_PARM_q)
#define OP_PARM_Wq              (OP_PARM_W+OP_PARM_q)
#define OP_PARM_Ws              (OP_PARM_W+OP_PARM_s)
#define OP_PARM_Xb              (OP_PARM_X+OP_PARM_b)
#define OP_PARM_Xv              (OP_PARM_X+OP_PARM_v)
#define OP_PARM_Yb              (OP_PARM_Y+OP_PARM_b)
#define OP_PARM_Yv              (OP_PARM_Y+OP_PARM_v)

#define OP_PARM_Vps             (OP_PARM_V+OP_PARM_ps)
#define OP_PARM_Vss             (OP_PARM_V+OP_PARM_ss)
#define OP_PARM_Vpd             (OP_PARM_V+OP_PARM_pd)
#define OP_PARM_Vdq             (OP_PARM_V+OP_PARM_dq)
#define OP_PARM_Wps             (OP_PARM_W+OP_PARM_ps)
#define OP_PARM_Wpd             (OP_PARM_W+OP_PARM_pd)
#define OP_PARM_Wss             (OP_PARM_W+OP_PARM_ss)
#define OP_PARM_Wdq             (OP_PARM_W+OP_PARM_dq)
#define OP_PARM_Ppi             (OP_PARM_P+OP_PARM_pi)
#define OP_PARM_Qpi             (OP_PARM_Q+OP_PARM_pi)
#define OP_PARM_Qdq             (OP_PARM_Q+OP_PARM_dq)
#define OP_PARM_Vsd             (OP_PARM_V+OP_PARM_sd)
#define OP_PARM_Wsd             (OP_PARM_W+OP_PARM_sd)
#define OP_PARM_Vpq             (OP_PARM_V+OP_PARM_pq)
#define OP_PARM_Pdq             (OP_PARM_P+OP_PARM_dq)

/** @} */

#endif

