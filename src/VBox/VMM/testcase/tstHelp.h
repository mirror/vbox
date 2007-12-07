/* $Id$ */
/** @file
 * VMM testcase - Helper stuff.
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
 */

#ifndef ___tstHelp_h
#define ___tstHelp_h

#include <VBox/cdefs.h>
#include <VBox/cpum.h>

__BEGIN_DECLS
void tstDumpCtx(PCPUMCTX pCtx, const char *pszComment);
__END_DECLS


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
            printf("%s::%s offset=%u expected alignment %x, meaning %u off\n", #strct, #member, (unsigned)RT_OFFSETOF(strct, member), \
                   (unsigned)(align), (unsigned)(RT_OFFSETOF(strct, member) & ((align) - 1))); \
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
            printf("%s size=%#x, align=%#x %#x bytes off\n", #type, (int)sizeof(type), \
                  (align), (int)RT_ALIGN_Z(sizeof(type), align) - (int)sizeof(type)); \
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


#endif
