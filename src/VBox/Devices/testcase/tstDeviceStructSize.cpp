/* $Id$ */
/** @file
 * tstDeviceStructSize - testcase for check structure sizes/alignment
 *                       and to verify that HC and GC uses the same
 *                       representation of the structures.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/types.h>
#include <VBox/x86.h>

#define VBOX_DEVICE_STRUCT_TESTCASE
#undef LOG_GROUP
#include "Bus/DevPCI.cpp"
#undef LOG_GROUP
#include "Graphics/DevVGA.cpp"
#undef LOG_GROUP
#include "Input/DevPS2.cpp"
#undef LOG_GROUP
#include "Network/DevPCNet.cpp"
//#undef LOG_GROUP
//#include "Network/ne2000.c"
#undef LOG_GROUP
#include "PC/DevACPI.cpp"
#undef LOG_GROUP
#include "PC/DevPIC.cpp"
#undef LOG_GROUP
#include "PC/DevPit-i8254.cpp"
#undef LOG_GROUP
#include "PC/DevRTC.cpp"
#undef LOG_GROUP
#include "PC/DevAPIC.cpp"
#undef LOG_GROUP
#include "Storage/DevATA.cpp"
#ifdef VBOX_WITH_USB
# undef LOG_GROUP
# include "USB/DevOHCI.cpp"
# include "USB/DevEHCI.cpp"
#endif
#undef LOG_GROUP
#include "VMMDev/VBoxDev.cpp"
#undef LOG_GROUP
#include "Serial/DevSerial.cpp"
#ifdef VBOX_WITH_AHCI
#undef LOG_GROUP
#include "Storage/DevAHCI.cpp"
#endif

#include <stdio.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/**
 * Checks the offset of a data member.
 * @param   type    Type.
 * @param   off     Correct offset.
 * @param   m       Member name.
 */
#define CHECK_OFF(type, off, m) \
    do { \
        if (off != RT_OFFSETOF(type, m)) \
        { \
            printf("%#010x %s  Off by %d!! (off=%#x)\n", RT_OFFSETOF(type, m), #type "." #m, off - RT_OFFSETOF(type, m), off); \
            rc++; \
        } \
        /*else */ \
            /*printf("%#08x %s\n", RT_OFFSETOF(type, m), #m);*/ \
    } while (0)

/**
 * Checks the size of type.
 * @param   type    Type.
 * @param   size    Correct size.
 */
#define CHECK_SIZE(type, size) \
    do { \
        if (size != sizeof(type)) \
        { \
            printf("sizeof(%s): %#x (%d)  Off by %d!!\n", #type, (int)sizeof(type), (int)sizeof(type), (int)(sizeof(type) - size)); \
            rc++; \
        } \
        else \
            printf("sizeof(%s): %#x (%d)\n", #type, (int)sizeof(type), (int)sizeof(type)); \
    } while (0)

/**
 * Checks the alignment of a struct member.
 */
#define CHECK_MEMBER_ALIGNMENT(strct, member, align) \
    do \
    { \
        if ( RT_OFFSETOF(strct, member) & ((align) - 1) ) \
        { \
            printf("%s::%s offset=%d expected alignment %d, meaning %d off\n", #strct, #member, RT_OFFSETOF(strct, member), \
                   align, RT_OFFSETOF(strct, member) & (align - 1)); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that the size of a type is aligned correctly.
 */
#define CHECK_SIZE_ALIGNMENT(type, align) \
    do { \
        if (RT_ALIGN_Z(sizeof(type), (align)) != sizeof(type)) \
        { \
            printf("%s size=%#x, align=%#x %#x bytes off\n", #type, (int)sizeof(type), (align), (int)RT_ALIGN_Z(sizeof(type), align) - (int)sizeof(type)); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that a internal struct padding is big enough.
 */
#define CHECK_PADDING(strct, member) \
    do \
    { \
        strct *p; \
        if (sizeof(p->member.s) > sizeof(p->member.padding)) \
        { \
            printf("padding of %s::%s is too small, padding=%d struct=%d correct=%d\n", #strct, #member, \
                   (int)sizeof(p->member.padding), (int)sizeof(p->member.s), (int)RT_ALIGN_Z(sizeof(p->member.s), 32)); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that a internal struct padding is big enough.
 */
#define CHECK_PADDING2(strct) \
    do \
    { \
        strct *p; \
        if (sizeof(p->s) > sizeof(p->padding)) \
        { \
            printf("padding of %s is too small, padding=%d struct=%d correct=%d\n", #strct, \
                   (int)sizeof(p->padding), (int)sizeof(p->s), (int)RT_ALIGN_Z(sizeof(p->s), 32)); \
            rc++; \
        } \
    } while (0)

/**
 * Checks that a internal struct padding is big enough.
 */
#define CHECK_PADDING3(strct, member, pad_member) \
    do \
    { \
        strct *p; \
        if (sizeof(p->member) > sizeof(p->pad_member)) \
        { \
            printf("padding of %s::%s is too small, padding=%d struct=%d\n", #strct, #member, \
                   (int)sizeof(p->pad_member), (int)sizeof(p->member)); \
            rc++; \
        } \
    } while (0)

/**
 * Prints the offset of a struct member.
 */
#define PRINT_OFFSET(strct, member) \
    do \
    { \
        printf("%s::%s offset %d sizeof %d\n",  #strct, #member, (int)RT_OFFSETOF(strct, member), (int)RT_SIZEOFMEMB(strct, member)); \
    } while (0)


int main()
{
    int rc = 0;
    printf("tstDeviceStructSize: TESTING\n");

    /* Assert sanity */
    CHECK_SIZE(uint128_t, 128/8);
    CHECK_SIZE(int128_t, 128/8);
    CHECK_SIZE(uint64_t, 64/8);
    CHECK_SIZE(int64_t, 64/8);
    CHECK_SIZE(uint32_t, 32/8);
    CHECK_SIZE(int32_t, 32/8);
    CHECK_SIZE(uint16_t, 16/8);
    CHECK_SIZE(int16_t, 16/8);
    CHECK_SIZE(uint8_t, 8/8);
    CHECK_SIZE(int8_t, 8/8);

    /*
     * Misc alignment checks.
     */
    CHECK_MEMBER_ALIGNMENT(PDMDEVINS, achInstanceData, 32);
    CHECK_MEMBER_ALIGNMENT(PCIDEVICE, Int.s, 16);
    CHECK_MEMBER_ALIGNMENT(PCIDEVICE, Int.s.aIORegions, 16);
    CHECK_MEMBER_ALIGNMENT(PCIBUS, devices, 16);
    CHECK_MEMBER_ALIGNMENT(PCIGLOBALS, pci_irq_levels, 16);
    CHECK_MEMBER_ALIGNMENT(PCNetState, u64LastPoll, 8);
    CHECK_MEMBER_ALIGNMENT(VGASTATE, Dev, 8);
    CHECK_MEMBER_ALIGNMENT(VGASTATE, StatGCMemoryRead, 8);
#ifdef VBOX_WITH_STATISTICS
//    CHECK_MEMBER_ALIGNMENT(PCNetState, StatMMIOReadGC, 8);
    CHECK_MEMBER_ALIGNMENT(DEVPIC, StatSetIrqGC, 8);
    CHECK_MEMBER_ALIGNMENT(APICState, StatMMIOReadGC, 8);
    CHECK_MEMBER_ALIGNMENT(IOAPICState, StatMMIOReadGC, 8);
    CHECK_MEMBER_ALIGNMENT(IOAPICState, StatMMIOReadGC, 8);
#endif
    CHECK_MEMBER_ALIGNMENT(PITState, StatPITIrq, 8);
    CHECK_MEMBER_ALIGNMENT(ATADevState, cTotalSectors, 8);
    CHECK_MEMBER_ALIGNMENT(ATADevState, StatReads, 8);
    CHECK_MEMBER_ALIGNMENT(ATACONTROLLER, StatAsyncOps, 8);
#ifdef VBOX_WITH_USB
    CHECK_MEMBER_ALIGNMENT(OHCI, RootHub, 8);
# ifdef VBOX_WITH_STATISTICS
    CHECK_MEMBER_ALIGNMENT(OHCI, StatCanceledIsocUrbs, 8);
# endif
    CHECK_MEMBER_ALIGNMENT(EHCI, RootHub, 8);
# ifdef VBOX_WITH_STATISTICS
    CHECK_MEMBER_ALIGNMENT(EHCI, StatCanceledIsocUrbs, 8);
# endif
#endif


    /*
     * Compare HC and GC.
     */
    printf("tstDeviceStructSize: Comparing HC and GC...\n");
#include "tstDeviceStructSizeGC.h"

    /*
     * Report result.
     */
    if (rc)
        printf("tstDeviceStructSize: FAILURE - %d errors\n", rc);
    else
        printf("tstDeviceStructSize: SUCCESS\n");
    return rc;
}

