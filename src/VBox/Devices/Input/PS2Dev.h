/** @file
 * PS/2 devices - Internal header file.
 */

/*
 * Copyright (C) 2007-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef PS2DEV_H
#define PS2DEV_H

/** The size of the PS2K structure filler.
 * @note Must be at least as big as the real struct. Compile time assert
 *       makes sure this is so. */
#define PS2K_STRUCT_FILLER  512

/* Hide the internal structure. */
#if !(defined(IN_PS2K) || defined(VBOX_DEVICE_STRUCT_TESTCASE))
typedef struct PS2K
{
    uint8_t     abFiller[PS2K_STRUCT_FILLER];
} PS2K;
#endif

typedef struct PS2K *PPS2K;

int  PS2KByteToKbd(PPS2K pThis, uint8_t cmd);
int  PS2KByteFromKbd(PPS2K pThis, uint8_t *pVal);

int  PS2KConstruct(PPS2K pThis, PPDMDEVINS pDevIns, void *pParent, int iInstance);
int  PS2KAttach(PPS2K pThis, PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags);
void PS2KReset(PPS2K pThis);
void PS2KRelocate(PPS2K pThis, RTGCINTPTR offDelta, PPDMDEVINS pDevIns);
void PS2KSaveState(PPS2K pThis, PSSMHANDLE pSSM);
int  PS2KLoadState(PPS2K pThis, PSSMHANDLE pSSM, uint32_t uVersion);

void KBCUpdateInterrupts(void *pKbc);

PS2K *KBDGetPS2KFromDevIns(PPDMDEVINS pDevIns);

///@todo: This should live with the KBC implementation.
/** AT to PC scancode translator state.  */
typedef enum
{
    XS_IDLE,    /**< Starting state. */
    XS_BREAK,   /**< F0 break byte was received. */
    XS_HIBIT    /**< Break code still active. */
} xlat_state_t;

int32_t XlateAT2PC(int32_t state, uint8_t scanIn, uint8_t *pScanOut);

#endif
