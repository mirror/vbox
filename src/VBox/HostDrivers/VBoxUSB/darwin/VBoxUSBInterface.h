/** $Id$ */
/** @file
 * VirtualBox USB Driver User<->Kernel Interface.
 */

/*
 * Copyright (C) 2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef ___VBoxUSBInterface_h
#define ___VBoxUSBInterface_h

#include <VBox/usbfilter.h>

/**
 * org_virtualbox_VBoxUSBClient method indexes.
 */
typedef enum VBOXUSBMETHOD
{
    /** org_virtualbox_VBoxUSBClient::addFilter */
    VBOXUSBMETHOD_ADD_FILTER = 0,
    /** org_virtualbox_VBoxUSBClient::removeFilter */
    VBOXUSBMETHOD_REMOVE_FILTER,
    /** End/max. */
    VBOXUSBMETHOD_END
} VBOXUSBMETHOD;

/**
 * Output from a VBOXUSBMETHOD_ADD_FILTER call.
 */
typedef struct VBOXUSBADDFILTEROUT
{
    /** The ID. */
    uintptr_t       uId;
    /** The return code. */
    int             rc;
} VBOXUSBADDFILTEROUT;
/** Pointer to a VBOXUSBADDFILTEROUT. */
typedef VBOXUSBADDFILTEROUT *PVBOXUSBADDFILTEROUT;


#endif

