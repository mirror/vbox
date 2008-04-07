/* $Id$ */
/** @file
 * BiosLogo - The Private BIOS Logo Interface.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
 */

#ifndef ___VBox_BiosLogo_h
#define ___VBox_BiosLogo_h

#ifndef VBOX_PC_BIOS
# include <iprt/types.h>
# include <iprt/assert.h>
#endif

/** @defgroup grp_bios_logo     The Private BIOS Logo Interface.
 * @remark All this is currently duplicated in logo.c.
 * @internal
 * @{
 */

/** The extra port which is used to show the BIOS logo. */
#define LOGO_IO_PORT         0x3b8

/** The BIOS logo fade in/fade out steps. */
#define LOGO_SHOW_STEPS      64

/** The BIOS boot menu text position, X. */
#define LOGO_F12TEXT_X       340
/** The BIOS boot menu text position, Y. */
#define LOGO_F12TEXT_Y       455

/** Width of the "Press F12 to select boot device." bitmap.
    Anything that exceeds the limit of F12BootText below is filled with
    background. */
#define LOGO_F12TEXT_WIDTH   286
/** Height of the boot device selection bitmap, see LOGO_F12TEXT_WIDTH. */
#define LOGO_F12TEXT_HEIGHT  12

#define LOGO_IMAGE_DEFAULT   0
#define LOGO_IMAGE_EXTERNAL  1

#define LOGO_MAX_WIDTH       640
#define LOGO_MAX_HEIGHT      480


/** @name The BIOS logo commands.
 * @{
 */
#define LOGO_CMD_NOP         0
#define LOGO_CMD_SET_OFFSET  0x100
#define LOGO_CMD_SET_X       0x200
#define LOGO_CMD_SET_Y       0x300
#define LOGO_CMD_SET_WIDTH   0x400
#define LOGO_CMD_SET_HEIGHT  0x500
#define LOGO_CMD_SET_DEPTH   0x600
#define LOGO_CMD_SET_PALSIZE 0x700
#define LOGO_CMD_SET_DEFAULT 0x800
#define LOGO_CMD_SET_PAL     0x900
#define LOGO_CMD_SHOW_BMP    0xA00
#define LOGO_CMD_SHOW_TEXT   0xB00
#define LOGO_CMD_CLS         0xC00
/** @} */

/**
 * PC Bios logo data structure.
 */
typedef struct LOGOHDR
{
    /** Signature (LOGO_HDR_MAGIC/0x66BB). */
    uint16_t        u16Signature;
    /** Logo time (msec). */
    uint16_t        u16LogoMillies;
    /** Fade in - boolean. */
    uint8_t         fu8FadeIn;
    /** Fade out - boolean. */
    uint8_t         fu8FadeOut;
    /** Show setup - boolean. */
    uint8_t         fu8ShowBootMenu;
    /** Reserved / padding. */
    uint8_t         u8Reserved;
    /** Logo file size. */
    uint32_t        cbLogo;
} LOGOHDR;
#ifndef VBOX_PC_BIOS
AssertCompileSize(LOGOHDR, 12);
#endif
/** Pointer to a PC Biso logo header. */
typedef LOGOHDR *PLOGOHDR;

/** The value of the LOGOHDR::u16Signature field. */
#define LOGO_HDR_MAGIC      0x66BB

/** The value which will switch you the default logo. */
#define LOGO_DEFAULT_LOGO   0xFFFF

/** @} */

#endif

