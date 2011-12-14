/** @file
 * PDM - Pluggable Device Manager, Interfaces.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

#ifndef ___VBox_vmm_pdmifs_h
#define ___VBox_vmm_pdmifs_h

#include <iprt/sg.h>
#include <VBox/types.h>
#include <VBox/hgcmsvc.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_pdm_interfaces    The PDM Interface Definitions
 * @ingroup grp_pdm
 *
 * For historical reasons (the PDMINTERFACE enum) a lot of interface was stuffed
 * together in this group instead, dragging stuff into global space that didn't
 * need to be there and making this file huge (>2500 lines).  Since we're using
 * UUIDs as interface identifiers (IIDs) now, no only generic PDM interface will
 * be added to this file.  Component specific interface should be defined in the
 * header file of that component.
 *
 * Interfaces consists of a method table (typedef'ed struct) and an interface
 * ID.  The typename of the method table should have an 'I' in it, be all
 * capitals and according to the rules, no underscores.  The interface ID is a
 * \#define constructed by appending '_IID' to the typename. The IID value is a
 * UUID string on the form "a2299c0d-b709-4551-aa5a-73f59ffbed74".  If you stick
 * to these rules, you can make use of the PDMIBASE_QUERY_INTERFACE and
 * PDMIBASE_RETURN_INTERFACE when querying interface and implementing
 * PDMIBASE::pfnQueryInterface respectively.
 *
 * In most interface descriptions the orientation of the interface is given as
 * 'down' or 'up'.  This refers to a model with the device on the top and the
 * drivers stacked below it.  Sometimes there is mention of 'main' or 'external'
 * which normally means the same, i.e. the Main or VBoxBFE API.  Picture the
 * orientation of 'main' as horizontal.
 *
 * @{
 */


/** @name PDMIBASE
 * @{
 */

/**
 * PDM Base Interface.
 *
 * Everyone implements this.
 */
typedef struct PDMIBASE
{
    /**
     * Queries an interface to the driver.
     *
     * @returns Pointer to interface.
     * @returns NULL if the interface was not supported by the driver.
     * @param   pInterface          Pointer to this interface structure.
     * @param   pszIID              The interface ID, a UUID string.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(void *, pfnQueryInterface,(struct PDMIBASE *pInterface, const char *pszIID));
} PDMIBASE;
/** PDMIBASE interface ID. */
#define PDMIBASE_IID                            "a2299c0d-b709-4551-aa5a-73f59ffbed74"

/**
 * Helper macro for querying an interface from PDMIBASE.
 *
 * @returns Correctly typed PDMIBASE::pfnQueryInterface return value.
 *
 * @param   pIBase          Pointer to the base interface.
 * @param   InterfaceType   The interface type name.  The interface ID is
 *                          derived from this by appending _IID.
 */
#define PDMIBASE_QUERY_INTERFACE(pIBase, InterfaceType)  \
    ( (InterfaceType *)(pIBase)->pfnQueryInterface(pIBase, InterfaceType##_IID ) )

/**
 * Helper macro for implementing PDMIBASE::pfnQueryInterface.
 *
 * Return @a pInterface if @a pszIID matches the @a InterfaceType.  This will
 * perform basic type checking.
 *
 * @param   pszIID          The ID of the interface that is being queried.
 * @param   InterfaceType   The interface type name.  The interface ID is
 *                          derived from this by appending _IID.
 * @param   pInterface      The interface address expression.
 */
#define PDMIBASE_RETURN_INTERFACE(pszIID, InterfaceType, pInterface)  \
    do { \
        if (RTUuidCompare2Strs((pszIID), InterfaceType##_IID) == 0) \
        { \
            P##InterfaceType pReturnInterfaceTypeCheck = (pInterface); \
            return pReturnInterfaceTypeCheck; \
        } \
    } while (0)

/** @} */


/** @name PDMIBASERC
 * @{
 */

/**
 * PDM Base Interface for querying ring-mode context interfaces in
 * ring-3.
 *
 * This is mandatory for drivers present in raw-mode context.
 */
typedef struct PDMIBASERC
{
    /**
     * Queries an ring-mode context interface to the driver.
     *
     * @returns Pointer to interface.
     * @returns NULL if the interface was not supported by the driver.
     * @param   pInterface          Pointer to this interface structure.
     * @param   pszIID              The interface ID, a UUID string.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(RTRCPTR, pfnQueryInterface,(struct PDMIBASERC *pInterface, const char *pszIID));
} PDMIBASERC;
/** Pointer to a PDM Base Interface for query ring-mode context interfaces. */
typedef PDMIBASERC *PPDMIBASERC;
/** PDMIBASERC interface ID. */
#define PDMIBASERC_IID                          "f6a6c649-6cb3-493f-9737-4653f221aeca"

/**
 * Helper macro for querying an interface from PDMIBASERC.
 *
 * @returns PDMIBASERC::pfnQueryInterface return value.
 *
 * @param   pIBaseRC        Pointer to the base raw-mode context interface.  Can
 *                          be NULL.
 * @param   InterfaceType   The interface type base name, no trailing RC.  The
 *                          interface ID is derived from this by appending _IID.
 *
 * @remarks Unlike PDMIBASE_QUERY_INTERFACE, this macro is not able to do any
 *          implicit type checking for you.
 */
#define PDMIBASERC_QUERY_INTERFACE(pIBaseRC, InterfaceType)  \
    ( (P##InterfaceType##RC)((pIBaseRC) ? (pIBaseRC)->pfnQueryInterface(pIBaseRC, InterfaceType##_IID) : NIL_RTRCPTR) )

/**
 * Helper macro for implementing PDMIBASERC::pfnQueryInterface.
 *
 * Return @a pInterface if @a pszIID matches the @a InterfaceType.  This will
 * perform basic type checking.
 *
 * @param   pIns            Pointer to the instance data.
 * @param   pszIID          The ID of the interface that is being queried.
 * @param   InterfaceType   The interface type base name, no trailing RC.  The
 *                          interface ID is derived from this by appending _IID.
 * @param   pInterface      The interface address expression.  This must resolve
 *                          to some address within the instance data.
 * @remarks Don't use with PDMIBASE.
 */
#define PDMIBASERC_RETURN_INTERFACE(pIns, pszIID, InterfaceType, pInterface)  \
    do { \
        Assert((uintptr_t)pInterface - PDMINS_2_DATA(pIns, uintptr_t) < _4M); \
        if (RTUuidCompare2Strs((pszIID), InterfaceType##_IID) == 0) \
        { \
            InterfaceType##RC *pReturnInterfaceTypeCheck = (pInterface); \
            return (uintptr_t)pReturnInterfaceTypeCheck \
                 - PDMINS_2_DATA(pIns, uintptr_t) \
                 + PDMINS_2_DATA_RCPTR(pIns); \
        } \
    } while (0)

/** @} */


/** @name PDMIBASER0
 * @{
 */

/**
 * PDM Base Interface for querying ring-0 interfaces in ring-3.
 *
 * This is mandatory for drivers present in ring-0 context.
 */
typedef struct PDMIBASER0
{
    /**
     * Queries an ring-0 interface to the driver.
     *
     * @returns Pointer to interface.
     * @returns NULL if the interface was not supported by the driver.
     * @param   pInterface          Pointer to this interface structure.
     * @param   pszIID              The interface ID, a UUID string.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(RTR0PTR, pfnQueryInterface,(struct PDMIBASER0 *pInterface, const char *pszIID));
} PDMIBASER0;
/** Pointer to a PDM Base Interface for query ring-0 context interfaces. */
typedef PDMIBASER0 *PPDMIBASER0;
/** PDMIBASER0 interface ID. */
#define PDMIBASER0_IID                          "9c9b99b8-7f53-4f59-a3c2-5bc9659c7944"

/**
 * Helper macro for querying an interface from PDMIBASER0.
 *
 * @returns PDMIBASER0::pfnQueryInterface return value.
 *
 * @param   pIBaseR0        Pointer to the base ring-0 interface.  Can be NULL.
 * @param   InterfaceType   The interface type base name, no trailing R0.  The
 *                          interface ID is derived from this by appending _IID.
 *
 * @remarks Unlike PDMIBASE_QUERY_INTERFACE, this macro is not able to do any
 *          implicit type checking for you.
 */
#define PDMIBASER0_QUERY_INTERFACE(pIBaseR0, InterfaceType)  \
    ( (P##InterfaceType##R0)((pIBaseR0) ? (pIBaseR0)->pfnQueryInterface(pIBaseR0, InterfaceType##_IID) : NIL_RTR0PTR) )

/**
 * Helper macro for implementing PDMIBASER0::pfnQueryInterface.
 *
 * Return @a pInterface if @a pszIID matches the @a InterfaceType.  This will
 * perform basic type checking.
 *
 * @param   pIns            Pointer to the instance data.
 * @param   pszIID          The ID of the interface that is being queried.
 * @param   InterfaceType   The interface type base name, no trailing R0.  The
 *                          interface ID is derived from this by appending _IID.
 * @param   pInterface      The interface address expression.  This must resolve
 *                          to some address within the instance data.
 * @remarks Don't use with PDMIBASE.
 */
#define PDMIBASER0_RETURN_INTERFACE(pIns, pszIID, InterfaceType, pInterface)  \
    do { \
        Assert((uintptr_t)pInterface - PDMINS_2_DATA(pIns, uintptr_t) < _4M); \
        if (RTUuidCompare2Strs((pszIID), InterfaceType##_IID) == 0) \
        { \
            InterfaceType##R0 *pReturnInterfaceTypeCheck = (pInterface); \
            return (uintptr_t)pReturnInterfaceTypeCheck \
                 - PDMINS_2_DATA(pIns, uintptr_t) \
                 + PDMINS_2_DATA_R0PTR(pIns); \
        } \
    } while (0)

/** @} */


/**
 * Dummy interface.
 *
 * This is used to typedef other dummy interfaces. The purpose of a dummy
 * interface is to validate the logical function of a driver/device and
 * full a natural interface pair.
 */
typedef struct PDMIDUMMY
{
    RTHCPTR pvDummy;
} PDMIDUMMY;


/** Pointer to a mouse port interface. */
typedef struct PDMIMOUSEPORT *PPDMIMOUSEPORT;
/**
 * Mouse port interface (down).
 * Pair with PDMIMOUSECONNECTOR.
 */
typedef struct PDMIMOUSEPORT
{
    /**
     * Puts a mouse event.
     *
     * This is called by the source of mouse events.  The event will be passed up
     * until the topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.  Return VERR_TRY_AGAIN if you cannot process the
     *          event now and want it to be repeated at a later point.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   iDeltaX             The X delta.
     * @param   iDeltaY             The Y delta.
     * @param   iDeltaZ             The Z delta.
     * @param   iDeltaW             The W (horizontal scroll button) delta.
     * @param   fButtonStates       The button states, see the PDMIMOUSEPORT_BUTTON_* \#defines.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEvent,(PPDMIMOUSEPORT pInterface, int32_t iDeltaX, int32_t iDeltaY, int32_t iDeltaZ, int32_t iDeltaW, uint32_t fButtonStates));
    /**
     * Puts an absolute mouse event.
     *
     * This is called by the source of mouse events.  The event will be passed up
     * until the topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.  Return VERR_TRY_AGAIN if you cannot process the
     *          event now and want it to be repeated at a later point.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   uX                  The X value, in the range 0 to 0xffff.
     * @param   uY                  The Y value, in the range 0 to 0xffff.
     * @param   iDeltaZ             The Z delta.
     * @param   iDeltaW             The W (horizontal scroll button) delta.
     * @param   fButtonStates       The button states, see the PDMIMOUSEPORT_BUTTON_* \#defines.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEventAbs,(PPDMIMOUSEPORT pInterface, uint32_t uX, uint32_t uY, int32_t iDeltaZ, int32_t iDeltaW, uint32_t fButtonStates));
} PDMIMOUSEPORT;
/** PDMIMOUSEPORT interface ID. */
#define PDMIMOUSEPORT_IID                       "442136fe-6f3c-49ec-9964-259b378ffa64"

/** Mouse button defines for PDMIMOUSEPORT::pfnPutEvent.
 * @{ */
#define PDMIMOUSEPORT_BUTTON_LEFT   RT_BIT(0)
#define PDMIMOUSEPORT_BUTTON_RIGHT  RT_BIT(1)
#define PDMIMOUSEPORT_BUTTON_MIDDLE RT_BIT(2)
#define PDMIMOUSEPORT_BUTTON_X1     RT_BIT(3)
#define PDMIMOUSEPORT_BUTTON_X2     RT_BIT(4)
/** @} */


/** Pointer to a mouse connector interface. */
typedef struct PDMIMOUSECONNECTOR *PPDMIMOUSECONNECTOR;
/**
 * Mouse connector interface (up).
 * Pair with PDMIMOUSEPORT.
 */
typedef struct PDMIMOUSECONNECTOR
{
    /**
     * Notifies the the downstream driver of changes to the reporting modes
     * supported by the driver
     *
     * @param   pInterface      Pointer to the this interface.
     * @param   fRelative       Whether relative mode is currently supported.
     * @param   fAbsolute       Whether absolute mode is currently supported.
     */
    DECLR3CALLBACKMEMBER(void, pfnReportModes,(PPDMIMOUSECONNECTOR pInterface, bool fRelative, bool fAbsolute));

} PDMIMOUSECONNECTOR;
/** PDMIMOUSECONNECTOR interface ID.  */
#define PDMIMOUSECONNECTOR_IID                  "ce64d7bd-fa8f-41d1-a6fb-d102a2d6bffe"


/** Pointer to a keyboard port interface. */
typedef struct PDMIKEYBOARDPORT *PPDMIKEYBOARDPORT;
/**
 * Keyboard port interface (down).
 * Pair with PDMIKEYBOARDCONNECTOR.
 */
typedef struct PDMIKEYBOARDPORT
{
    /**
     * Puts a keyboard event.
     *
     * This is called by the source of keyboard events. The event will be passed up
     * until the topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.  Return VERR_TRY_AGAIN if you cannot process the
     *          event now and want it to be repeated at a later point.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   u8KeyCode           The keycode to queue.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEvent,(PPDMIKEYBOARDPORT pInterface, uint8_t u8KeyCode));
} PDMIKEYBOARDPORT;
/** PDMIKEYBOARDPORT interface ID. */
#define PDMIKEYBOARDPORT_IID                    "2a0844f0-410b-40ab-a6ed-6575f3aa3e29"


/**
 * Keyboard LEDs.
 */
typedef enum PDMKEYBLEDS
{
    /** No leds. */
    PDMKEYBLEDS_NONE             = 0x0000,
    /** Num Lock */
    PDMKEYBLEDS_NUMLOCK          = 0x0001,
    /** Caps Lock */
    PDMKEYBLEDS_CAPSLOCK         = 0x0002,
    /** Scroll Lock */
    PDMKEYBLEDS_SCROLLLOCK       = 0x0004
} PDMKEYBLEDS;

/** Pointer to keyboard connector interface. */
typedef struct PDMIKEYBOARDCONNECTOR *PPDMIKEYBOARDCONNECTOR;
/**
 * Keyboard connector interface (up).
 * Pair with PDMIKEYBOARDPORT
 */
typedef struct PDMIKEYBOARDCONNECTOR
{
    /**
     * Notifies the the downstream driver about an LED change initiated by the guest.
     *
     * @param   pInterface      Pointer to the this interface.
     * @param   enmLeds         The new led mask.
     */
    DECLR3CALLBACKMEMBER(void, pfnLedStatusChange,(PPDMIKEYBOARDCONNECTOR pInterface, PDMKEYBLEDS enmLeds));

    /**
     * Notifies the the downstream driver of changes in driver state.
     *
     * @param   pInterface      Pointer to the this interface.
     * @param   fActive         Whether interface wishes to get "focus".
     */
    DECLR3CALLBACKMEMBER(void, pfnSetActive,(PPDMIKEYBOARDCONNECTOR pInterface, bool fActive));

} PDMIKEYBOARDCONNECTOR;
/** PDMIKEYBOARDCONNECTOR interface ID. */
#define PDMIKEYBOARDCONNECTOR_IID               "db3f7bd5-953e-436f-9f8e-077905a92d82"



/** Pointer to a display port interface. */
typedef struct PDMIDISPLAYPORT *PPDMIDISPLAYPORT;
/**
 * Display port interface (down).
 * Pair with PDMIDISPLAYCONNECTOR.
 */
typedef struct PDMIDISPLAYPORT
{
    /**
     * Update the display with any changed regions.
     *
     * Flushes any display changes to the memory pointed to by the
     * PDMIDISPLAYCONNECTOR interface and calles PDMIDISPLAYCONNECTOR::pfnUpdateRect()
     * while doing so.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUpdateDisplay,(PPDMIDISPLAYPORT pInterface));

    /**
     * Update the entire display.
     *
     * Flushes the entire display content to the memory pointed to by the
     * PDMIDISPLAYCONNECTOR interface and calles PDMIDISPLAYCONNECTOR::pfnUpdateRect().
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUpdateDisplayAll,(PPDMIDISPLAYPORT pInterface));

    /**
     * Return the current guest color depth in bits per pixel (bpp).
     *
     * As the graphics card is able to provide display updates with the bpp
     * requested by the host, this method can be used to query the actual
     * guest color depth.
     *
     * @returns VBox status code.
     * @param   pInterface         Pointer to this interface.
     * @param   pcBits             Where to store the current guest color depth.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryColorDepth,(PPDMIDISPLAYPORT pInterface, uint32_t *pcBits));

    /**
     * Sets the refresh rate and restart the timer.
     * The rate is defined as the minimum interval between the return of
     * one PDMIDISPLAYPORT::pfnRefresh() call to the next one.
     *
     * The interval timer will be restarted by this call. So at VM startup
     * this function must be called to start the refresh cycle. The refresh
     * rate is not saved, but have to be when resuming a loaded VM state.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   cMilliesInterval    Number of millis between two refreshes.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetRefreshRate,(PPDMIDISPLAYPORT pInterface, uint32_t cMilliesInterval));

    /**
     * Create a 32-bbp screenshot of the display.
     *
     * This will allocate and return a 32-bbp bitmap. Size of the bitmap scanline in bytes is 4*width.
     *
     * The allocated bitmap buffer must be freed with pfnFreeScreenshot.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   ppu8Data            Where to store the pointer to the allocated buffer.
     * @param   pcbData             Where to store the actual size of the bitmap.
     * @param   pcx                 Where to store the width of the bitmap.
     * @param   pcy                 Where to store the height of the bitmap.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnTakeScreenshot,(PPDMIDISPLAYPORT pInterface, uint8_t **ppu8Data, size_t *pcbData, uint32_t *pcx, uint32_t *pcy));

    /**
     * Free screenshot buffer.
     *
     * This will free the memory buffer allocated by pfnTakeScreenshot.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   ppu8Data            Pointer to the buffer returned by pfnTakeScreenshot.
     * @thread  Any.
     */
    DECLR3CALLBACKMEMBER(void, pfnFreeScreenshot,(PPDMIDISPLAYPORT pInterface, uint8_t *pu8Data));

    /**
     * Copy bitmap to the display.
     *
     * This will convert and copy a 32-bbp bitmap (with dword aligned scanline length) to
     * the memory pointed to by the PDMIDISPLAYCONNECTOR interface.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvData              Pointer to the bitmap bits.
     * @param   x                   The upper left corner x coordinate of the destination rectangle.
     * @param   y                   The upper left corner y coordinate of the destination rectangle.
     * @param   cx                  The width of the source and destination rectangles.
     * @param   cy                  The height of the source and destination rectangles.
     * @thread  The emulation thread.
     * @remark  This is just a convenience for using the bitmap conversions of the
     *          graphics device.
     */
    DECLR3CALLBACKMEMBER(int, pfnDisplayBlt,(PPDMIDISPLAYPORT pInterface, const void *pvData, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy));

    /**
     * Render a rectangle from guest VRAM to Framebuffer.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   x                   The upper left corner x coordinate of the rectangle to be updated.
     * @param   y                   The upper left corner y coordinate of the rectangle to be updated.
     * @param   cx                  The width of the rectangle to be updated.
     * @param   cy                  The height of the rectangle to be updated.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateDisplayRect,(PPDMIDISPLAYPORT pInterface, int32_t x, int32_t y, uint32_t cx, uint32_t cy));

    /**
     * Inform the VGA device whether the Display is directly using the guest VRAM and there is no need
     * to render the VRAM to the framebuffer memory.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fRender             Whether the VRAM content must be rendered to the framebuffer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetRenderVRAM,(PPDMIDISPLAYPORT pInterface, bool fRender));

    /**
     * Render a bitmap rectangle from source to target buffer.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   cx                  The width of the rectangle to be copied.
     * @param   cy                  The height of the rectangle to be copied.
     * @param   pbSrc               Source frame buffer 0,0.
     * @param   xSrc                The upper left corner x coordinate of the source rectangle.
     * @param   ySrc                The upper left corner y coordinate of the source rectangle.
     * @param   cxSrc               The width of the source frame buffer.
     * @param   cySrc               The height of the source frame buffer.
     * @param   cbSrcLine           The line length of the source frame buffer.
     * @param   cSrcBitsPerPixel    The pixel depth of the source.
     * @param   pbDst               Destination frame buffer 0,0.
     * @param   xDst                The upper left corner x coordinate of the destination rectangle.
     * @param   yDst                The upper left corner y coordinate of the destination rectangle.
     * @param   cxDst               The width of the destination frame buffer.
     * @param   cyDst               The height of the destination frame buffer.
     * @param   cbDstLine           The line length of the destination frame buffer.
     * @param   cDstBitsPerPixel    The pixel depth of the destination.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnCopyRect,(PPDMIDISPLAYPORT pInterface, uint32_t cx, uint32_t cy,
        const uint8_t *pbSrc, int32_t xSrc, int32_t ySrc, uint32_t cxSrc, uint32_t cySrc, uint32_t cbSrcLine, uint32_t cSrcBitsPerPixel,
        uint8_t       *pbDst, int32_t xDst, int32_t yDst, uint32_t cxDst, uint32_t cyDst, uint32_t cbDstLine, uint32_t cDstBitsPerPixel));

} PDMIDISPLAYPORT;
/** PDMIDISPLAYPORT interface ID. */
#define PDMIDISPLAYPORT_IID                     "22d3d93d-3407-487a-8308-85367eae00bb"


typedef struct VBOXVHWACMD *PVBOXVHWACMD; /**< @todo r=bird: A line what it is to make doxygen happy. */
typedef struct VBVACMDHDR *PVBVACMDHDR;
typedef struct VBVAINFOSCREEN *PVBVAINFOSCREEN;
typedef struct VBVAINFOVIEW *PVBVAINFOVIEW;
typedef struct VBVAHOSTFLAGS *PVBVAHOSTFLAGS;
typedef struct VBOXVDMACMD_CHROMIUM_CMD *PVBOXVDMACMD_CHROMIUM_CMD; /* <- chromium [hgsmi] command */
typedef struct VBOXVDMACMD_CHROMIUM_CTL *PVBOXVDMACMD_CHROMIUM_CTL; /* <- chromium [hgsmi] command */

/** Pointer to a display connector interface. */
typedef struct PDMIDISPLAYCONNECTOR *PPDMIDISPLAYCONNECTOR;
/**
 * Display connector interface (up).
 * Pair with PDMIDISPLAYPORT.
 */
typedef struct PDMIDISPLAYCONNECTOR
{
    /**
     * Resize the display.
     * This is called when the resolution changes. This usually happens on
     * request from the guest os, but may also happen as the result of a reset.
     * If the callback returns VINF_VGA_RESIZE_IN_PROGRESS, the caller (VGA device)
     * must not access the connector and return.
     *
     * @returns VINF_SUCCESS if the framebuffer resize was completed,
     *          VINF_VGA_RESIZE_IN_PROGRESS if resize takes time and not yet finished.
     * @param   pInterface          Pointer to this interface.
     * @param   cBits               Color depth (bits per pixel) of the new video mode.
     * @param   pvVRAM              Address of the guest VRAM.
     * @param   cbLine              Size in bytes of a single scan line.
     * @param   cx                  New display width.
     * @param   cy                  New display height.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnResize,(PPDMIDISPLAYCONNECTOR pInterface, uint32_t cBits, void *pvVRAM, uint32_t cbLine, uint32_t cx, uint32_t cy));

    /**
     * Update a rectangle of the display.
     * PDMIDISPLAYPORT::pfnUpdateDisplay is the caller.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   x                   The upper left corner x coordinate of the rectangle.
     * @param   y                   The upper left corner y coordinate of the rectangle.
     * @param   cx                  The width of the rectangle.
     * @param   cy                  The height of the rectangle.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateRect,(PPDMIDISPLAYCONNECTOR pInterface, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy));

    /**
     * Refresh the display.
     *
     * The interval between these calls is set by
     * PDMIDISPLAYPORT::pfnSetRefreshRate(). The driver should call
     * PDMIDISPLAYPORT::pfnUpdateDisplay() if it wishes to refresh the
     * display. PDMIDISPLAYPORT::pfnUpdateDisplay calls pfnUpdateRect with
     * the changed rectangles.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnRefresh,(PPDMIDISPLAYCONNECTOR pInterface));

    /**
     * Reset the display.
     *
     * Notification message when the graphics card has been reset.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnReset,(PPDMIDISPLAYCONNECTOR pInterface));

    /**
     * LFB video mode enter/exit.
     *
     * Notification message when LinearFrameBuffer video mode is enabled/disabled.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fEnabled            false - LFB mode was disabled,
     *                              true -  an LFB mode was disabled
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnLFBModeChange, (PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled));

    /**
     * Process the guest graphics adapter information.
     *
     * Direct notification from guest to the display connector.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvVRAM              Address of the guest VRAM.
     * @param   u32VRAMSize         Size of the guest VRAM.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnProcessAdapterData, (PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, uint32_t u32VRAMSize));

    /**
     * Process the guest display information.
     *
     * Direct notification from guest to the display connector.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvVRAM              Address of the guest VRAM.
     * @param   uScreenId           The index of the guest display to be processed.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnProcessDisplayData, (PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, unsigned uScreenId));

    /**
     * Process the guest Video HW Acceleration command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVHWACommandProcess, (PPDMIDISPLAYCONNECTOR pInterface, PVBOXVHWACMD pCmd));

    /**
     * Process the guest chromium command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnCrHgsmiCommandProcess, (PPDMIDISPLAYCONNECTOR pInterface, PVBOXVDMACMD_CHROMIUM_CMD pCmd, uint32_t cbCmd));

    /**
     * Process the guest chromium control command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnCrHgsmiControlProcess, (PPDMIDISPLAYCONNECTOR pInterface, PVBOXVDMACMD_CHROMIUM_CTL pCtl, uint32_t cbCtl));


    /**
     * The specified screen enters VBVA mode.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVBVAEnable,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, PVBVAHOSTFLAGS pHostFlags));

    /**
     * The specified screen leaves VBVA mode.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVADisable,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId));

    /**
     * A sequence of pfnVBVAUpdateProcess calls begins.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAUpdateBegin,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId));

    /**
     * Process the guest VBVA command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAUpdateProcess,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, const PVBVACMDHDR pCmd, size_t cbCmd));

    /**
     * A sequence of pfnVBVAUpdateProcess calls ends.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @param   x                   The upper left corner x coordinate of the combined rectangle of all VBVA updates.
     * @param   y                   The upper left corner y coordinate of the rectangle.
     * @param   cx                  The width of the rectangle.
     * @param   cy                  The height of the rectangle.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAUpdateEnd,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, int32_t x, int32_t y, uint32_t cx, uint32_t cy));

    /**
     * Resize the display.
     * This is called when the resolution changes. This usually happens on
     * request from the guest os, but may also happen as the result of a reset.
     * If the callback returns VINF_VGA_RESIZE_IN_PROGRESS, the caller (VGA device)
     * must not access the connector and return.
     *
     * @todo Merge with pfnResize.
     *
     * @returns VINF_SUCCESS if the framebuffer resize was completed,
     *          VINF_VGA_RESIZE_IN_PROGRESS if resize takes time and not yet finished.
     * @param   pInterface          Pointer to this interface.
     * @param   pView               The description of VRAM block for this screen.
     * @param   pScreen             The data of screen being resized.
     * @param   pvVRAM              Address of the guest VRAM.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVBVAResize,(PPDMIDISPLAYCONNECTOR pInterface, const PVBVAINFOVIEW pView, const PVBVAINFOSCREEN pScreen, void *pvVRAM));

    /**
     * Update the pointer shape.
     * This is called when the mouse pointer shape changes. The new shape
     * is passed as a caller allocated buffer that will be freed after returning
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fVisible            Visibility indicator (if false, the other parameters are undefined).
     * @param   fAlpha              Flag whether alpha channel is being passed.
     * @param   xHot                Pointer hot spot x coordinate.
     * @param   yHot                Pointer hot spot y coordinate.
     * @param   x                   Pointer new x coordinate on screen.
     * @param   y                   Pointer new y coordinate on screen.
     * @param   cx                  Pointer width in pixels.
     * @param   cy                  Pointer height in pixels.
     * @param   cbScanline          Size of one scanline in bytes.
     * @param   pvShape             New shape buffer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVBVAMousePointerShape,(PPDMIDISPLAYCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                                        uint32_t xHot, uint32_t yHot,
                                                        uint32_t cx, uint32_t cy,
                                                        const void *pvShape));

    /** Read-only attributes.
     * For preformance reasons some readonly attributes are kept in the interface.
     * We trust the interface users to respect the readonlyness of these.
     * @{
     */
    /** Pointer to the display data buffer. */
    uint8_t        *pu8Data;
    /** Size of a scanline in the data buffer. */
    uint32_t        cbScanline;
    /** The color depth (in bits) the graphics card is supposed to provide. */
    uint32_t        cBits;
    /** The display width. */
    uint32_t        cx;
    /** The display height. */
    uint32_t        cy;
    /** @} */
} PDMIDISPLAYCONNECTOR;
/** PDMIDISPLAYCONNECTOR interface ID. */
#define PDMIDISPLAYCONNECTOR_IID                "c7a1b36d-8dfc-421d-b71f-3a0eeaf733e6"


/** Pointer to a block port interface. */
typedef struct PDMIBLOCKPORT *PPDMIBLOCKPORT;
/**
 * Block notify interface (down).
 * Pair with PDMIBLOCK.
 */
typedef struct PDMIBLOCKPORT
{
    /**
     * Returns the storage controller name, instance and LUN of the attached medium.
     *
     * @returns VBox status.
     * @param   pInterface      Pointer to this interface.
     * @param   ppcszController Where to store the name of the storage controller.
     * @param   piInstance      Where to store the instance number of the controller.
     * @param   piLUN           Where to store the LUN of the attached device.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryDeviceLocation, (PPDMIBLOCKPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN));

} PDMIBLOCKPORT;
/** PDMIBLOCKPORT interface ID. */
#define PDMIBLOCKPORT_IID                 "bbbed4cf-0862-4ffd-b60c-f7a65ef8e8ff"


/**
 * Callback which provides progress information.
 *
 * @return  VBox status code.
 * @param   pvUser          Opaque user data.
 * @param   uPercent        Completion percentage.
 */
typedef DECLCALLBACK(int) FNSIMPLEPROGRESS(void *pvUser, unsigned uPercentage);
/** Pointer to FNSIMPLEPROGRESS() */
typedef FNSIMPLEPROGRESS *PFNSIMPLEPROGRESS;


/**
 * Block drive type.
 */
typedef enum PDMBLOCKTYPE
{
    /** Error (for the query function). */
    PDMBLOCKTYPE_ERROR = 1,
    /** 360KB 5 1/4" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_360,
    /** 720KB 3 1/2" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_720,
    /** 1.2MB 5 1/4" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_1_20,
    /** 1.44MB 3 1/2" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_1_44,
    /** 2.88MB 3 1/2" floppy drive. */
    PDMBLOCKTYPE_FLOPPY_2_88,
    /** CDROM drive. */
    PDMBLOCKTYPE_CDROM,
    /** DVD drive. */
    PDMBLOCKTYPE_DVD,
    /** Hard disk drive. */
    PDMBLOCKTYPE_HARD_DISK
} PDMBLOCKTYPE;


/**
 * Block raw command data transfer direction.
 */
typedef enum PDMBLOCKTXDIR
{
    PDMBLOCKTXDIR_NONE = 0,
    PDMBLOCKTXDIR_FROM_DEVICE,
    PDMBLOCKTXDIR_TO_DEVICE
} PDMBLOCKTXDIR;


/** Pointer to a block interface. */
typedef struct PDMIBLOCK *PPDMIBLOCK;
/**
 * Block interface (up).
 * Pair with PDMIBLOCKPORT.
 */
typedef struct PDMIBLOCK
{
    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from. The offset must be aligned to a sector boundary.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read. Must be aligned to a sector boundary.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIBLOCK pInterface, uint64_t off, void *pvBuf, size_t cbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at. The offset must be aligned to a sector boundary.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write. Must be aligned to a sector boundary.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIBLOCK pInterface, uint64_t off, const void *pvBuf, size_t cbWrite));

    /**
     * Make sure that the bits written are actually on the storage medium.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush,(PPDMIBLOCK pInterface));

    /**
     * Send a raw command to the underlying device (CDROM).
     * This method is optional (i.e. the function pointer may be NULL).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pbCmd           Offset to start reading from.
     * @param   enmTxDir        Direction of transfer.
     * @param   pvBuf           Pointer tp the transfer buffer.
     * @param   cbBuf           Size of the transfer buffer.
     * @param   pbSenseKey      Status of the command (when return value is VERR_DEV_IO_ERROR).
     * @param   cTimeoutMillies Command timeout in milliseconds.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSendCmd,(PPDMIBLOCK pInterface, const uint8_t *pbCmd, PDMBLOCKTXDIR enmTxDir, void *pvBuf, uint32_t *pcbBuf, uint8_t *pabSense, size_t cbSense, uint32_t cTimeoutMillies));

    /**
     * Merge medium contents during a live snapshot deletion.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfnProgress     Function pointer for progress notification.
     * @param   pvUser          Opaque user data for progress notification.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnMerge,(PPDMIBLOCK pInterface, PFNSIMPLEPROGRESS pfnProgress, void *pvUser));

    /**
     * Check if the media is readonly or not.
     *
     * @returns true if readonly.
     * @returns false if read/write.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsReadOnly,(PPDMIBLOCK pInterface));

    /**
     * Gets the media size in bytes.
     *
     * @returns Media size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize,(PPDMIBLOCK pInterface));

    /**
     * Gets the block drive type.
     *
     * @returns block drive type.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(PDMBLOCKTYPE, pfnGetType,(PPDMIBLOCK pInterface));

    /**
     * Gets the UUID of the block drive.
     * Don't return the media UUID if it's removable.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pUuid           Where to store the UUID on success.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid,(PPDMIBLOCK pInterface, PRTUUID pUuid));

    /**
     * Discards the given range.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   paRanges        Array of ranges to discard.
     * @param   cRanges         Number of entries in the array.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnDiscard,(PPDMIBLOCK pInterface, PCRTRANGE paRanges, unsigned cRanges));
} PDMIBLOCK;
/** PDMIBLOCK interface ID. */
#define PDMIBLOCK_IID                           "5e7123dd-8cdf-4a6e-97a5-ab0c68d7e850"


/** Pointer to a mount interface. */
typedef struct PDMIMOUNTNOTIFY *PPDMIMOUNTNOTIFY;
/**
 * Block interface (up).
 * Pair with PDMIMOUNT.
 */
typedef struct PDMIMOUNTNOTIFY
{
    /**
     * Called when a media is mounted.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnMountNotify,(PPDMIMOUNTNOTIFY pInterface));

    /**
     * Called when a media is unmounted
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUnmountNotify,(PPDMIMOUNTNOTIFY pInterface));
} PDMIMOUNTNOTIFY;
/** PDMIMOUNTNOTIFY interface ID. */
#define PDMIMOUNTNOTIFY_IID                     "fa143ac9-9fc6-498e-997f-945380a558f9"


/** Pointer to mount interface. */
typedef struct PDMIMOUNT *PPDMIMOUNT;
/**
 * Mount interface (down).
 * Pair with PDMIMOUNTNOTIFY.
 */
typedef struct PDMIMOUNT
{
    /**
     * Mount a media.
     *
     * This will not unmount any currently mounted media!
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pszFilename     Pointer to filename. If this is NULL it assumed that the caller have
     *                          constructed a configuration which can be attached to the bottom driver.
     * @param   pszCoreDriver   Core driver name. NULL will cause autodetection. Ignored if pszFilanem is NULL.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnMount,(PPDMIMOUNT pInterface, const char *pszFilename, const char *pszCoreDriver));

    /**
     * Unmount the media.
     *
     * The driver will validate and pass it on. On the rebounce it will decide whether or not to detach it self.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     * @param   fForce          Force the unmount, even for locked media.
     * @param   fEject          Eject the medium. Only relevant for host drives.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnmount,(PPDMIMOUNT pInterface, bool fForce, bool fEject));

    /**
     * Checks if a media is mounted.
     *
     * @returns true if mounted.
     * @returns false if not mounted.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsMounted,(PPDMIMOUNT pInterface));

    /**
     * Locks the media, preventing any unmounting of it.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnLock,(PPDMIMOUNT pInterface));

    /**
     * Unlocks the media, canceling previous calls to pfnLock().
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnlock,(PPDMIMOUNT pInterface));

    /**
     * Checks if a media is locked.
     *
     * @returns true if locked.
     * @returns false if not locked.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsLocked,(PPDMIMOUNT pInterface));
} PDMIMOUNT;
/** PDMIMOUNT interface ID. */
#define PDMIMOUNT_IID                           "34fc7a4c-623a-4806-a6bf-5be1be33c99f"


/**
 * Media geometry structure.
 */
typedef struct PDMMEDIAGEOMETRY
{
    /** Number of cylinders. */
    uint32_t    cCylinders;
    /** Number of heads. */
    uint32_t    cHeads;
    /** Number of sectors. */
    uint32_t    cSectors;
} PDMMEDIAGEOMETRY;

/** Pointer to media geometry structure. */
typedef PDMMEDIAGEOMETRY *PPDMMEDIAGEOMETRY;
/** Pointer to constant media geometry structure. */
typedef const PDMMEDIAGEOMETRY *PCPDMMEDIAGEOMETRY;

/** Pointer to a media port interface. */
typedef struct PDMIMEDIAPORT *PPDMIMEDIAPORT;
/**
 * Media port interface (down).
 */
typedef struct PDMIMEDIAPORT
{
    /**
     * Returns the storage controller name, instance and LUN of the attached medium.
     *
     * @returns VBox status.
     * @param   pInterface      Pointer to this interface.
     * @param   ppcszController Where to store the name of the storage controller.
     * @param   piInstance      Where to store the instance number of the controller.
     * @param   piLUN           Where to store the LUN of the attached device.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryDeviceLocation, (PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN));

} PDMIMEDIAPORT;
/** PDMIMEDIAPORT interface ID. */
#define PDMIMEDIAPORT_IID                           "9f7e8c9e-6d35-4453-bbef-1f78033174d6"

/** Pointer to a media interface. */
typedef struct PDMIMEDIA *PPDMIMEDIA;
/**
 * Media interface (up).
 * Makes up the foundation for PDMIBLOCK and PDMIBLOCKBIOS.  No interface pair.
 */
typedef struct PDMIMEDIA
{
    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from. The offset must be aligned to a sector boundary.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read. Must be aligned to a sector boundary.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at. The offset must be aligned to a sector boundary.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write. Must be aligned to a sector boundary.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite));

    /**
     * Make sure that the bits written are actually on the storage medium.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush,(PPDMIMEDIA pInterface));

    /**
     * Merge medium contents during a live snapshot deletion. All details
     * must have been configured through CFGM or this will fail.
     * This method is optional (i.e. the function pointer may be NULL).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfnProgress     Function pointer for progress notification.
     * @param   pvUser          Opaque user data for progress notification.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnMerge,(PPDMIMEDIA pInterface, PFNSIMPLEPROGRESS pfnProgress, void *pvUser));

    /**
     * Get the media size in bytes.
     *
     * @returns Media size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize,(PPDMIMEDIA pInterface));

    /**
     * Check if the media is readonly or not.
     *
     * @returns true if readonly.
     * @returns false if read/write.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsReadOnly,(PPDMIMEDIA pInterface));

    /**
     * Get stored media geometry (physical CHS, PCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnBiosSetPCHSGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pPCHSGeometry   Pointer to PCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetPCHSGeometry,(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry));

    /**
     * Store the media geometry (physical CHS, PCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pPCHSGeometry   Pointer to PCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetPCHSGeometry,(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry));

    /**
     * Get stored media geometry (logical CHS, LCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnBiosSetLCHSGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pLCHSGeometry   Pointer to LCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetLCHSGeometry,(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry));

    /**
     * Store the media geometry (logical CHS, LCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pLCHSGeometry   Pointer to LCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetLCHSGeometry,(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry));

    /**
     * Gets the UUID of the media drive.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pUuid           Where to store the UUID on success.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid,(PPDMIMEDIA pInterface, PRTUUID pUuid));

    /**
     * Discards the given range.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   paRanges        Array of ranges to discard.
     * @param   cRanges         Number of entries in the array.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnDiscard,(PPDMIMEDIA pInterface, PCRTRANGE paRanges, unsigned cRanges));

} PDMIMEDIA;
/** PDMIMEDIA interface ID. */
#define PDMIMEDIA_IID                           "ec385d21-7aa9-42ca-8cfb-e1388297fa52"


/** Pointer to a block BIOS interface. */
typedef struct PDMIBLOCKBIOS *PPDMIBLOCKBIOS;
/**
 * Media BIOS interface (Up / External).
 * The interface the getting and setting properties which the BIOS/CMOS care about.
 */
typedef struct PDMIBLOCKBIOS
{
    /**
     * Get stored media geometry (physical CHS, PCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnSetPCHSGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pPCHSGeometry   Pointer to PCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetPCHSGeometry,(PPDMIBLOCKBIOS pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry));

    /**
     * Store the media geometry (physical CHS, PCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pPCHSGeometry   Pointer to PCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetPCHSGeometry,(PPDMIBLOCKBIOS pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry));

    /**
     * Get stored media geometry (logical CHS, LCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnSetLCHSGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pLCHSGeometry   Pointer to LCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetLCHSGeometry,(PPDMIBLOCKBIOS pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry));

    /**
     * Store the media geometry (logical CHS, LCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pLCHSGeometry   Pointer to LCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetLCHSGeometry,(PPDMIBLOCKBIOS pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry));

    /**
     * Checks if the device should be visible to the BIOS or not.
     *
     * @returns true if the device is visible to the BIOS.
     * @returns false if the device is not visible to the BIOS.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsVisible,(PPDMIBLOCKBIOS pInterface));

    /**
     * Gets the block drive type.
     *
     * @returns block drive type.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(PDMBLOCKTYPE, pfnGetType,(PPDMIBLOCKBIOS pInterface));

} PDMIBLOCKBIOS;
/** PDMIBLOCKBIOS interface ID. */
#define PDMIBLOCKBIOS_IID                       "477c3eee-a48d-48a9-82fd-2a54de16b2e9"


/** Pointer to a static block core driver interface. */
typedef struct PDMIMEDIASTATIC *PPDMIMEDIASTATIC;
/**
 * Static block core driver interface.
 */
typedef struct PDMIMEDIASTATIC
{
    /**
     * Check if the specified file is a format which the core driver can handle.
     *
     * @returns true / false accordingly.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pszFilename     Name of the file to probe.
     */
    DECLR3CALLBACKMEMBER(bool, pfnCanHandle,(PPDMIMEDIASTATIC pInterface, const char *pszFilename));
} PDMIMEDIASTATIC;





/** Pointer to an asynchronous block notify interface. */
typedef struct PDMIBLOCKASYNCPORT *PPDMIBLOCKASYNCPORT;
/**
 * Asynchronous block notify interface (up).
 * Pair with PDMIBLOCKASYNC.
 */
typedef struct PDMIBLOCKASYNCPORT
{
    /**
     * Notify completion of an asynchronous transfer.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvUser          The user argument given in pfnStartWrite/Read.
     * @param   rcReq           IPRT Status code of the completed request.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnTransferCompleteNotify, (PPDMIBLOCKASYNCPORT pInterface, void *pvUser, int rcReq));
} PDMIBLOCKASYNCPORT;
/** PDMIBLOCKASYNCPORT interface ID. */
#define PDMIBLOCKASYNCPORT_IID                  "e3bdc0cb-9d99-41dd-8eec-0dc8cf5b2a92"



/** Pointer to an asynchronous block interface. */
typedef struct PDMIBLOCKASYNC *PPDMIBLOCKASYNC;
/**
 * Asynchronous block interface (down).
 * Pair with PDMIBLOCKASYNCPORT.
 */
typedef struct PDMIBLOCKASYNC
{
    /**
     * Start reading task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from.c
     * @param   paSegs          Pointer to the S/G segment array.
     * @param   cSegs           Number of entries in the array.
     * @param   cbRead          Number of bytes to read. Must be aligned to a sector boundary.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartRead,(PPDMIBLOCKASYNC pInterface, uint64_t off, PCRTSGSEG paSegs, unsigned cSegs, size_t cbRead, void *pvUser));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at. The offset must be aligned to a sector boundary.
     * @param   paSegs          Pointer to the S/G segment array.
     * @param   cSegs           Number of entries in the array.
     * @param   cbWrite         Number of bytes to write. Must be aligned to a sector boundary.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartWrite,(PPDMIBLOCKASYNC pInterface, uint64_t off, PCRTSGSEG paSegs, unsigned cSegs, size_t cbWrite, void *pvUser));

    /**
     * Flush everything to disk.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartFlush,(PPDMIBLOCKASYNC pInterface, void *pvUser));

    /**
     * Discards the given range.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   paRanges        Array of ranges to discard.
     * @param   cRanges         Number of entries in the array.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartDiscard,(PPDMIBLOCKASYNC pInterface, PCRTRANGE paRanges, unsigned cRanges, void *pvUser));

} PDMIBLOCKASYNC;
/** PDMIBLOCKASYNC interface ID. */
#define PDMIBLOCKASYNC_IID                      "a921dd96-1748-4ecd-941e-d5f3cd4c8fe4"


/** Pointer to an asynchronous notification interface. */
typedef struct PDMIMEDIAASYNCPORT *PPDMIMEDIAASYNCPORT;
/**
 * Asynchronous version of the media interface (up).
 * Pair with PDMIMEDIAASYNC.
 */
typedef struct PDMIMEDIAASYNCPORT
{
    /**
     * Notify completion of a task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvUser          The user argument given in pfnStartWrite.
     * @param   rcReq           IPRT Status code of the completed request.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnTransferCompleteNotify, (PPDMIMEDIAASYNCPORT pInterface, void *pvUser, int rcReq));
} PDMIMEDIAASYNCPORT;
/** PDMIMEDIAASYNCPORT interface ID. */
#define PDMIMEDIAASYNCPORT_IID                  "22d38853-901f-4a71-9670-4d9da6e82317"


/** Pointer to an asynchronous media interface. */
typedef struct PDMIMEDIAASYNC *PPDMIMEDIAASYNC;
/**
 * Asynchronous version of PDMIMEDIA (down).
 * Pair with PDMIMEDIAASYNCPORT.
 */
typedef struct PDMIMEDIAASYNC
{
    /**
     * Start reading task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from. Must be aligned to a sector boundary.
     * @param   paSegs          Pointer to the S/G segment array.
     * @param   cSegs           Number of entries in the array.
     * @param   cbRead          Number of bytes to read. Must be aligned to a sector boundary.
     * @param   pvUser          User data.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartRead,(PPDMIMEDIAASYNC pInterface, uint64_t off, PCRTSGSEG paSegs, unsigned cSegs, size_t cbRead, void *pvUser));

    /**
     * Start writing task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at. Must be aligned to a sector boundary.
     * @param   paSegs          Pointer to the S/G segment array.
     * @param   cSegs           Number of entries in the array.
     * @param   cbWrite         Number of bytes to write. Must be aligned to a sector boundary.
     * @param   pvUser          User data.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartWrite,(PPDMIMEDIAASYNC pInterface, uint64_t off, PCRTSGSEG paSegs, unsigned cSegs, size_t cbWrite, void *pvUser));

    /**
     * Flush everything to disk.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartFlush,(PPDMIMEDIAASYNC pInterface, void *pvUser));

    /**
     * Discards the given range.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   paRanges        Array of ranges to discard.
     * @param   cRanges         Number of entries in the array.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartDiscard,(PPDMIMEDIAASYNC pInterface, PCRTRANGE paRanges, unsigned cRanges, void *pvUser));

} PDMIMEDIAASYNC;
/** PDMIMEDIAASYNC interface ID. */
#define PDMIMEDIAASYNC_IID                      "4be209d3-ccb5-4297-82fe-7d8018bc6ab4"


/** Pointer to a char port interface. */
typedef struct PDMICHARPORT *PPDMICHARPORT;
/**
 * Char port interface (down).
 * Pair with PDMICHARCONNECTOR.
 */
typedef struct PDMICHARPORT
{
    /**
     * Deliver data read to the device/driver.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where the read bits are stored.
     * @param   pcbRead         Number of bytes available for reading/having been read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyRead,(PPDMICHARPORT pInterface, const void *pvBuf, size_t *pcbRead));

    /**
     * Notify the device/driver when the status lines changed.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fNewStatusLine  New state of the status line pins.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyStatusLinesChanged,(PPDMICHARPORT pInterface, uint32_t fNewStatusLines));

    /**
     * Notify the device when the driver buffer is full.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fFull           Buffer full.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyBufferFull,(PPDMICHARPORT pInterface, bool fFull));

    /**
     * Notify the device/driver that a break occurred.
     *
     * @returns VBox statsus code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyBreak,(PPDMICHARPORT pInterface));
} PDMICHARPORT;
/** PDMICHARPORT interface ID. */
#define PDMICHARPORT_IID                        "22769834-ea8b-4a6d-ade1-213dcdbd1228"

/** @name Bit mask definitions for status line type.
 * @{ */
#define PDMICHARPORT_STATUS_LINES_DCD   RT_BIT(0)
#define PDMICHARPORT_STATUS_LINES_RI    RT_BIT(1)
#define PDMICHARPORT_STATUS_LINES_DSR   RT_BIT(2)
#define PDMICHARPORT_STATUS_LINES_CTS   RT_BIT(3)
/** @} */


/** Pointer to a char interface. */
typedef struct PDMICHARCONNECTOR *PPDMICHARCONNECTOR;
/**
 * Char connector interface (up).
 * Pair with PDMICHARPORT.
 */
typedef struct PDMICHARCONNECTOR
{
    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMICHARCONNECTOR pInterface, const void *pvBuf, size_t cbWrite));

    /**
     * Set device parameters.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   Bps             Speed of the serial connection. (bits per second)
     * @param   chParity        Parity method: 'E' - even, 'O' - odd, 'N' - none.
     * @param   cDataBits       Number of data bits.
     * @param   cStopBits       Number of stop bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetParameters,(PPDMICHARCONNECTOR pInterface, unsigned Bps, char chParity, unsigned cDataBits, unsigned cStopBits));

    /**
     * Set the state of the modem lines.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   fRequestToSend      Set to true to make the Request to Send line active otherwise to 0.
     * @param   fDataTerminalReady  Set to true to make the Data Terminal Ready line active otherwise 0.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetModemLines,(PPDMICHARCONNECTOR pInterface, bool fRequestToSend, bool fDataTerminalReady));

    /**
     * Sets the TD line into break condition.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to the interface structure containing the called function pointer.
     * @param   fBreak      Set to true to let the device send a break false to put into normal operation.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetBreak,(PPDMICHARCONNECTOR pInterface, bool fBreak));
} PDMICHARCONNECTOR;
/** PDMICHARCONNECTOR interface ID. */
#define PDMICHARCONNECTOR_IID                   "4ad5c190-b408-4cef-926f-fbffce0dc5cc"


/** Pointer to a stream interface. */
typedef struct PDMISTREAM *PPDMISTREAM;
/**
 * Stream interface (up).
 * Makes up the foundation for PDMICHARCONNECTOR.  No pair interface.
 */
typedef struct PDMISTREAM
{
    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read/bytes actually read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMISTREAM pInterface, void *pvBuf, size_t *cbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write/bytes actually written.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMISTREAM pInterface, const void *pvBuf, size_t *cbWrite));
} PDMISTREAM;
/** PDMISTREAM interface ID. */
#define PDMISTREAM_IID                          "d1a5bf5e-3d2c-449a-bde9-addd7920b71f"


/** Mode of the parallel port */
typedef enum PDMPARALLELPORTMODE
{
    PDM_PARALLEL_PORT_MODE_COMPAT,
    PDM_PARALLEL_PORT_MODE_EPP,
    PDM_PARALLEL_PORT_MODE_ECP
} PDMPARALLELPORTMODE;

/** Pointer to a host parallel port interface. */
typedef struct PDMIHOSTPARALLELPORT *PPDMIHOSTPARALLELPORT;
/**
 * Host parallel port interface (down).
 * Pair with PDMIHOSTPARALLELCONNECTOR.
 */
typedef struct PDMIHOSTPARALLELPORT
{
    /**
     * Deliver data read to the device/driver.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where the read bits are stored.
     * @param   pcbRead         Number of bytes available for reading/having been read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyRead,(PPDMIHOSTPARALLELPORT pInterface, const void *pvBuf, size_t *pcbRead));

    /**
     * Notify device/driver that an interrupt has occurred.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyInterrupt,(PPDMIHOSTPARALLELPORT pInterface));
} PDMIHOSTPARALLELPORT;
/** PDMIHOSTPARALLELPORT interface ID. */
#define PDMIHOSTPARALLELPORT_IID                "ac13e437-cd30-47ac-a271-6120571f3a22"



/** Pointer to a Host Parallel connector interface. */
typedef struct PDMIHOSTPARALLELCONNECTOR *PPDMIHOSTPARALLELCONNECTOR;
/**
 * Host parallel connector interface (up).
 * Pair with PDMIHOSTPARALLELPORT.
 */
typedef struct PDMIHOSTPARALLELCONNECTOR
{
    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the write bits.
     * @param   pcbWrite        Number of bytes to write/bytes actually written.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIHOSTPARALLELCONNECTOR pInterface, const void *pvBuf, size_t *pcbWrite));

    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the read bits.
     * @param   pcbRead         Number of bytes to read/bytes actually read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIHOSTPARALLELCONNECTOR pInterface, void *pvBuf, size_t *pcbRead));

    /**
     * Write control register bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fReg            The new control register value.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteControl,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t fReg));

    /**
     * Read control register bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfReg           Where to store the control register bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadControl,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg));

    /**
     * Read status register bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfReg           Where to store the status register bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadStatus,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg));

    /**
     * Set mode of the host parallel port.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmMode         The mode of the host parallel port.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetMode,(PPDMIHOSTPARALLELCONNECTOR pInterface, PDMPARALLELPORTMODE enmMode));
} PDMIHOSTPARALLELCONNECTOR;
/** PDMIHOSTPARALLELCONNECTOR interface ID. */
#define PDMIHOSTPARALLELCONNECTOR_IID           "a03567ca-b29e-4a1b-b2f3-a12435fa2982"


/** ACPI power source identifier */
typedef enum PDMACPIPOWERSOURCE
{
    PDM_ACPI_POWER_SOURCE_UNKNOWN  =   0,
    PDM_ACPI_POWER_SOURCE_OUTLET,
    PDM_ACPI_POWER_SOURCE_BATTERY
} PDMACPIPOWERSOURCE;
/** Pointer to ACPI battery state. */
typedef PDMACPIPOWERSOURCE *PPDMACPIPOWERSOURCE;

/** ACPI battey capacity */
typedef enum PDMACPIBATCAPACITY
{
    PDM_ACPI_BAT_CAPACITY_MIN      =   0,
    PDM_ACPI_BAT_CAPACITY_MAX      = 100,
    PDM_ACPI_BAT_CAPACITY_UNKNOWN  = 255
} PDMACPIBATCAPACITY;
/** Pointer to ACPI battery capacity. */
typedef PDMACPIBATCAPACITY *PPDMACPIBATCAPACITY;

/** ACPI battery state. See ACPI 3.0 spec '_BST (Battery Status)' */
typedef enum PDMACPIBATSTATE
{
    PDM_ACPI_BAT_STATE_CHARGED     = 0x00,
    PDM_ACPI_BAT_STATE_DISCHARGING = 0x01,
    PDM_ACPI_BAT_STATE_CHARGING    = 0x02,
    PDM_ACPI_BAT_STATE_CRITICAL    = 0x04
} PDMACPIBATSTATE;
/** Pointer to ACPI battery state. */
typedef PDMACPIBATSTATE *PPDMACPIBATSTATE;

/** Pointer to an ACPI port interface. */
typedef struct PDMIACPIPORT *PPDMIACPIPORT;
/**
 * ACPI port interface (down). Used by both the ACPI driver and (grumble) main.
 * Pair with PDMIACPICONNECTOR.
 */
typedef struct PDMIACPIPORT
{
    /**
     * Send an ACPI power off event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnPowerButtonPress,(PPDMIACPIPORT pInterface));

    /**
     * Send an ACPI sleep button event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnSleepButtonPress,(PPDMIACPIPORT pInterface));

    /**
     * Check if the last power button event was handled by the guest.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfHandled       Is set to true if the last power button event was handled, false otherwise.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetPowerButtonHandled,(PPDMIACPIPORT pInterface, bool *pfHandled));

    /**
     * Check if the guest entered the ACPI mode.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfEnabled       Is set to true if the guest entered the ACPI mode, false otherwise.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetGuestEnteredACPIMode,(PPDMIACPIPORT pInterface, bool *pfEntered));

    /**
     * Check if the given CPU is still locked by the guest.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   uCpu            The CPU to check for.
     * @param   pfLocked        Is set to true if the CPU is still locked by the guest, false otherwise.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetCpuStatus,(PPDMIACPIPORT pInterface, unsigned uCpu, bool *pfLocked));
} PDMIACPIPORT;
/** PDMIACPIPORT interface ID. */
#define PDMIACPIPORT_IID                        "30d3dc4c-6a73-40c8-80e9-34309deacbb3"


/** Pointer to an ACPI connector interface. */
typedef struct PDMIACPICONNECTOR *PPDMIACPICONNECTOR;
/**
 * ACPI connector interface (up).
 * Pair with PDMIACPIPORT.
 */
typedef struct PDMIACPICONNECTOR
{
    /**
     * Get the current power source of the host system.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   penmPowerSource Pointer to the power source result variable.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryPowerSource,(PPDMIACPICONNECTOR, PPDMACPIPOWERSOURCE penmPowerSource));

    /**
     * Query the current battery status of the host system.
     *
     * @returns VBox status code?
     * @param   pInterface              Pointer to the interface structure containing the called function pointer.
     * @param   pfPresent               Is set to true if battery is present, false otherwise.
     * @param   penmRemainingCapacity   Pointer to the battery remaining capacity (0 - 100 or 255 for unknown).
     * @param   penmBatteryState        Pointer to the battery status.
     * @param   pu32PresentRate         Pointer to the present rate (0..1000 of the total capacity).
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryBatteryStatus,(PPDMIACPICONNECTOR, bool *pfPresent, PPDMACPIBATCAPACITY penmRemainingCapacity,
                                                     PPDMACPIBATSTATE penmBatteryState, uint32_t *pu32PresentRate));
} PDMIACPICONNECTOR;
/** PDMIACPICONNECTOR interface ID. */
#define PDMIACPICONNECTOR_IID                   "5f14bf8d-1edf-4e3a-a1e1-cca9fd08e359"


/** Pointer to a VMMDevice port interface. */
typedef struct PDMIVMMDEVPORT *PPDMIVMMDEVPORT;
/**
 * VMMDevice port interface (down).
 * Pair with PDMIVMMDEVCONNECTOR.
 */
typedef struct PDMIVMMDEVPORT
{
    /**
     * Return the current absolute mouse position in pixels
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pxAbs           Pointer of result value, can be NULL
     * @param   pyAbs           Pointer of result value, can be NULL
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryAbsoluteMouse,(PPDMIVMMDEVPORT pInterface, int32_t *pxAbs, int32_t *pyAbs));

    /**
     * Set the new absolute mouse position in pixels
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   xabs            New absolute X position
     * @param   yAbs            New absolute Y position
     */
    DECLR3CALLBACKMEMBER(int, pfnSetAbsoluteMouse,(PPDMIVMMDEVPORT pInterface, int32_t xAbs, int32_t yAbs));

    /**
     * Return the current mouse capability flags
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfCapabilities  Pointer of result value
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryMouseCapabilities,(PPDMIVMMDEVPORT pInterface, uint32_t *pfCapabilities));

    /**
     * Set the current mouse capability flag (host side)
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fCapsAdded      Mask of capabilities to add to the flag
     * @param   fCapsRemoved    Mask of capabilities to remove from the flag
     */
    DECLR3CALLBACKMEMBER(int, pfnUpdateMouseCapabilities,(PPDMIVMMDEVPORT pInterface, uint32_t fCapsAdded, uint32_t fCapsRemoved));

    /**
     * Issue a display resolution change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cx              Horizontal pixel resolution (0 = do not change).
     * @param   cy              Vertical pixel resolution (0 = do not change).
     * @param   cBits           Bits per pixel (0 = do not change).
     * @param   idxDisplay      The display index.
     */
    DECLR3CALLBACKMEMBER(int, pfnRequestDisplayChange,(PPDMIVMMDEVPORT pInterface, uint32_t cx, uint32_t cy, uint32_t cBits, uint32_t idxDisplay));

    /**
     * Pass credentials to guest.
     *
     * Note that there can only be one set of credentials and the guest may or may not
     * query them and may do whatever it wants with them.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pszUsername     User name, may be empty (UTF-8).
     * @param   pszPassword     Password, may be empty (UTF-8).
     * @param   pszDomain       Domain name, may be empty (UTF-8).
     * @param   fFlags          VMMDEV_SETCREDENTIALS_*.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetCredentials,(PPDMIVMMDEVPORT pInterface, const char *pszUsername,
                                                 const char *pszPassword, const char *pszDomain,
                                                 uint32_t fFlags));

    /**
     * Notify the driver about a VBVA status change.
     *
     * @returns Nothing. Because it is informational callback.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fEnabled        Current VBVA status.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAChange, (PPDMIVMMDEVPORT pInterface, bool fEnabled));

    /**
     * Issue a seamless mode change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fEnabled        Seamless mode enabled or not
     */
    DECLR3CALLBACKMEMBER(int, pfnRequestSeamlessChange,(PPDMIVMMDEVPORT pInterface, bool fEnabled));

    /**
     * Issue a memory balloon change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cMbBalloon      Balloon size in megabytes
     */
    DECLR3CALLBACKMEMBER(int, pfnSetMemoryBalloon,(PPDMIVMMDEVPORT pInterface, uint32_t cMbBalloon));

    /**
     * Issue a statistcs interval change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   cSecsStatInterval   Statistics query interval in seconds
     *                              (0=disable).
     */
    DECLR3CALLBACKMEMBER(int, pfnSetStatisticsInterval,(PPDMIVMMDEVPORT pInterface, uint32_t cSecsStatInterval));

    /**
     * Notify the guest about a VRDP status change.
     *
     * @returns VBox status code
     * @param   pInterface              Pointer to the interface structure containing the called function pointer.
     * @param   fVRDPEnabled            Current VRDP status.
     * @param   uVRDPExperienceLevel    Which visual effects to be disabled in
     *                                  the guest.
     */
    DECLR3CALLBACKMEMBER(int, pfnVRDPChange, (PPDMIVMMDEVPORT pInterface, bool fVRDPEnabled, uint32_t uVRDPExperienceLevel));

    /**
     * Notify the guest of CPU hot-unplug event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   idCpuCore       The core id of the CPU to remove.
     * @param   idCpuPackage    The package id of the CPU to remove.
     */
    DECLR3CALLBACKMEMBER(int, pfnCpuHotUnplug, (PPDMIVMMDEVPORT pInterface, uint32_t idCpuCore, uint32_t idCpuPackage));

    /**
     * Notify the guest of CPU hot-plug event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   idCpuCore       The core id of the CPU to add.
     * @param   idCpuPackage    The package id of the CPU to add.
     */
    DECLR3CALLBACKMEMBER(int, pfnCpuHotPlug, (PPDMIVMMDEVPORT pInterface, uint32_t idCpuCore, uint32_t idCpuPackage));

} PDMIVMMDEVPORT;
/** PDMIVMMDEVPORT interface ID. */
#define PDMIVMMDEVPORT_IID                      "d7e52035-3b6c-422e-9215-2a75646a945d"


/** Pointer to a HPET legacy notification interface. */
typedef struct PDMIHPETLEGACYNOTIFY *PPDMIHPETLEGACYNOTIFY;
/**
 * HPET legacy notification interface.
 */
typedef struct PDMIHPETLEGACYNOTIFY
{
    /**
     * Notify about change of HPET legacy mode.
     *
     * @param   pInterface      Pointer to the interface structure containing the
     *                          called function pointer.
     * @param   fActivated      If HPET legacy mode is activated (@c true) or
     *                          deactivated (@c false).
     */
    DECLR3CALLBACKMEMBER(void, pfnModeChanged,(PPDMIHPETLEGACYNOTIFY pInterface, bool fActivated));
} PDMIHPETLEGACYNOTIFY;
/** PDMIHPETLEGACYNOTIFY interface ID. */
#define PDMIHPETLEGACYNOTIFY_IID                "c9ada595-4b65-4311-8b21-b10498997774"


/** @name Flags for PDMIVMMDEVPORT::pfnSetCredentials.
 * @{ */
/** The guest should perform a logon with the credentials. */
#define VMMDEV_SETCREDENTIALS_GUESTLOGON                    RT_BIT(0)
/** The guest should prevent local logons. */
#define VMMDEV_SETCREDENTIALS_NOLOCALLOGON                  RT_BIT(1)
/** The guest should verify the credentials. */
#define VMMDEV_SETCREDENTIALS_JUDGE                         RT_BIT(15)
/** @} */

/** Forward declaration of the guest information structure. */
struct VBoxGuestInfo;
/** Forward declaration of the guest information-2 structure. */
struct VBoxGuestInfo2;
/** Forward declaration of the guest statistics structure */
struct VBoxGuestStatistics;
/** Forward declaration of the guest status structure */
struct VBoxGuestStatus;

/** Forward declaration of the video accelerator command memory. */
struct VBVAMEMORY;
/** Pointer to video accelerator command memory. */
typedef struct VBVAMEMORY *PVBVAMEMORY;

/** Pointer to a VMMDev connector interface. */
typedef struct PDMIVMMDEVCONNECTOR *PPDMIVMMDEVCONNECTOR;
/**
 * VMMDev connector interface (up).
 * Pair with PDMIVMMDEVPORT.
 */
typedef struct PDMIVMMDEVCONNECTOR
{
    /**
     * Reports the guest status.
     * Called whenever the Additions issue a guest status report request.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pGuestStatus        Pointer to guest information structure
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestStatus,(PPDMIVMMDEVCONNECTOR pInterface, const struct VBoxGuestStatus *pGuestStatus));

    /**
     * Reports the guest API and OS version.
     * Called whenever the Additions issue a guest info report request.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pGuestInfo          Pointer to guest information structure
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestInfo,(PPDMIVMMDEVCONNECTOR pInterface, const struct VBoxGuestInfo *pGuestInfo));

    /**
     * Reports the detailed Guest Additions version.
     * Called whenever the Additions issue a guest version report request or the VM is reset.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   guestInfo           Pointer to Guest Additions information structure.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestInfo2,(PPDMIVMMDEVCONNECTOR pInterface, const struct VBoxGuestInfo2 *pGuestInfo));

    /**
     * Update the guest additions capabilities.
     * This is called when the guest additions capabilities change. The new capabilities
     * are given and the connector should update its internal state.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   newCapabilities     New capabilities.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestCapabilities,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities));

    /**
     * Update the mouse capabilities.
     * This is called when the mouse capabilities change. The new capabilities
     * are given and the connector should update its internal state.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   newCapabilities     New capabilities.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateMouseCapabilities,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities));

    /**
     * Update the pointer shape.
     * This is called when the mouse pointer shape changes. The new shape
     * is passed as a caller allocated buffer that will be freed after returning
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fVisible            Visibility indicator (if false, the other parameters are undefined).
     * @param   fAlpha              Flag whether alpha channel is being passed.
     * @param   xHot                Pointer hot spot x coordinate.
     * @param   yHot                Pointer hot spot y coordinate.
     * @param   x                   Pointer new x coordinate on screen.
     * @param   y                   Pointer new y coordinate on screen.
     * @param   cx                  Pointer width in pixels.
     * @param   cy                  Pointer height in pixels.
     * @param   cbScanline          Size of one scanline in bytes.
     * @param   pvShape             New shape buffer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdatePointerShape,(PPDMIVMMDEVCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                                      uint32_t xHot, uint32_t yHot,
                                                      uint32_t cx, uint32_t cy,
                                                      void *pvShape));

    /**
     * Enable or disable video acceleration on behalf of guest.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fEnable             Whether to enable acceleration.
     * @param   pVbvaMemory         Video accelerator memory.

     * @return  VBox rc. VINF_SUCCESS if VBVA was enabled.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVideoAccelEnable,(PPDMIVMMDEVCONNECTOR pInterface, bool fEnable, PVBVAMEMORY pVbvaMemory));

    /**
     * Force video queue processing.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVideoAccelFlush,(PPDMIVMMDEVCONNECTOR pInterface));

    /**
     * Return whether the given video mode is supported/wanted by the host.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to this interface.
     * @param   display         The guest monitor, 0 for primary.
     * @param   cy              Video mode horizontal resolution in pixels.
     * @param   cx              Video mode vertical resolution in pixels.
     * @param   cBits           Video mode bits per pixel.
     * @param   pfSupported     Where to put the indicator for whether this mode is supported. (output)
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVideoModeSupported,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t display, uint32_t cx, uint32_t cy, uint32_t cBits, bool *pfSupported));

    /**
     * Queries by how many pixels the height should be reduced when calculating video modes
     *
     * @returns VBox status code
     * @param   pInterface          Pointer to this interface.
     * @param   pcyReduction        Pointer to the result value.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetHeightReduction,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcyReduction));

    /**
     * Informs about a credentials judgement result from the guest.
     *
     * @returns VBox status code
     * @param   pInterface          Pointer to this interface.
     * @param   fFlags              Judgement result flags.
     * @thread  The emulation thread.
     */
     DECLR3CALLBACKMEMBER(int, pfnSetCredentialsJudgementResult,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t fFlags));

    /**
     * Set the visible region of the display
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   cRect               Number of rectangles in pRect
     * @param   pRect               Rectangle array
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetVisibleRegion,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t cRect, PRTRECT pRect));

    /**
     * Query the visible region of the display
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pcRect              Number of rectangles in pRect
     * @param   pRect               Rectangle array (set to NULL to query the number of rectangles)
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryVisibleRegion,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcRect, PRTRECT pRect));

    /**
     * Request the statistics interval
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pulInterval         Pointer to interval in seconds
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryStatisticsInterval,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pulInterval));

    /**
     * Report new guest statistics
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pGuestStats         Guest statistics
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReportStatistics,(PPDMIVMMDEVCONNECTOR pInterface, struct VBoxGuestStatistics *pGuestStats));

    /**
     * Query the current balloon size
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pcbBalloon          Balloon size
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryBalloonSize,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcbBalloon));

    /**
     * Query the current page fusion setting
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pfPageFusionEnabled Pointer to boolean
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIsPageFusionEnabled,(PPDMIVMMDEVCONNECTOR pInterface, bool *pfPageFusionEnabled));

} PDMIVMMDEVCONNECTOR;
/** PDMIVMMDEVCONNECTOR interface ID. */
#define PDMIVMMDEVCONNECTOR_IID                 "aff90240-a443-434e-9132-80c186ab97d4"


/** Pointer to a network connector interface */
typedef struct PDMIAUDIOCONNECTOR *PPDMIAUDIOCONNECTOR;
/**
 * Audio connector interface (up).
 * No interface pair yet.
 */
typedef struct PDMIAUDIOCONNECTOR
{
    DECLR3CALLBACKMEMBER(void, pfnRun,(PPDMIAUDIOCONNECTOR pInterface));

/*    DECLR3CALLBACKMEMBER(int,  pfnSetRecordSource,(PPDMIAUDIOINCONNECTOR pInterface, AUDIORECSOURCE)); */

} PDMIAUDIOCONNECTOR;
/** PDMIAUDIOCONNECTOR interface ID. */
#define PDMIAUDIOCONNECTOR_IID                  "85d52af5-b3aa-4b3e-b176-4b5ebfc52f47"


/** @todo r=bird: the two following interfaces are hacks to work around the missing audio driver
 * interface. This should be addressed rather than making more temporary hacks. */

/** Pointer to a Audio Sniffer Device port interface. */
typedef struct PDMIAUDIOSNIFFERPORT *PPDMIAUDIOSNIFFERPORT;
/**
 * Audio Sniffer port interface (down).
 * Pair with PDMIAUDIOSNIFFERCONNECTOR.
 */
typedef struct PDMIAUDIOSNIFFERPORT
{
    /**
     * Enables or disables sniffing.
     *
     * If sniffing is being enabled also sets a flag whether the audio must be also
     * left on the host.
     *
     * @returns VBox status code
     * @param pInterface      Pointer to this interface.
     * @param fEnable         'true' for enable sniffing, 'false' to disable.
     * @param fKeepHostAudio  Indicates whether host audio should also present
     *                        'true' means that sound should not be played
     *                        by the audio device.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetup,(PPDMIAUDIOSNIFFERPORT pInterface, bool fEnable, bool fKeepHostAudio));

    /**
     * Enables or disables audio input.
     *
     * @returns VBox status code
     * @param pInterface      Pointer to this interface.
     * @param fIntercept      'true' for interception of audio input,
     *                        'false' to let the host audio backend do audio input.
     */
    DECLR3CALLBACKMEMBER(int, pfnAudioInputIntercept,(PPDMIAUDIOSNIFFERPORT pInterface, bool fIntercept));

    /**
     * Audio input is about to start.
     *
     * @returns VBox status code.
     * @param   pvContext       The callback context, supplied in the
     *                          PDMIAUDIOSNIFFERCONNECTOR::pfnAudioInputBegin as pvContext.
     * @param   iSampleHz       The sample frequency in Hz.
     * @param   cChannels       Number of channels. 1 for mono, 2 for stereo.
     * @param   cBits           How many bits a sample for a single channel has. Normally 8 or 16.
     * @param   fUnsigned       Whether samples are unsigned values.
     */
    DECLR3CALLBACKMEMBER(int, pfnAudioInputEventBegin,(PPDMIAUDIOSNIFFERPORT pInterface,
                                                       void *pvContext,
                                                       int iSampleHz,
                                                       int cChannels,
                                                       int cBits,
                                                       bool fUnsigned));

    /**
     * Callback which delivers audio data to the audio device.
     *
     * @returns VBox status code.
     * @param   pvContext       The callback context, supplied in the
     *                          PDMIAUDIOSNIFFERCONNECTOR::pfnAudioInputBegin as pvContext.
     * @param   pvData          Event specific data.
     * @param   cbData          Size of the buffer pointed by pvData.
     */
    DECLR3CALLBACKMEMBER(int, pfnAudioInputEventData,(PPDMIAUDIOSNIFFERPORT pInterface,
                                                      void *pvContext,
                                                      const void *pvData,
                                                      uint32_t cbData));

    /**
     * Audio input ends.
     *
     * @param   pvContext       The callback context, supplied in the
     *                          PDMIAUDIOSNIFFERCONNECTOR::pfnAudioInputBegin as pvContext.
     */
    DECLR3CALLBACKMEMBER(void, pfnAudioInputEventEnd,(PPDMIAUDIOSNIFFERPORT pInterface,
                                                      void *pvContext));
} PDMIAUDIOSNIFFERPORT;
/** PDMIAUDIOSNIFFERPORT interface ID. */
#define PDMIAUDIOSNIFFERPORT_IID                "8ad25d78-46e9-479b-a363-bb0bc0fe022f"


/** Pointer to a Audio Sniffer connector interface. */
typedef struct PDMIAUDIOSNIFFERCONNECTOR *PPDMIAUDIOSNIFFERCONNECTOR;

/**
 * Audio Sniffer connector interface (up).
 * Pair with PDMIAUDIOSNIFFERPORT.
 */
typedef struct PDMIAUDIOSNIFFERCONNECTOR
{
    /**
     * AudioSniffer device calls this method when audio samples
     * are about to be played and sniffing is enabled.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvSamples           Audio samples buffer.
     * @param   cSamples            How many complete samples are in the buffer.
     * @param   iSampleHz           The sample frequency in Hz.
     * @param   cChannels           Number of channels. 1 for mono, 2 for stereo.
     * @param   cBits               How many bits a sample for a single channel has. Normally 8 or 16.
     * @param   fUnsigned           Whether samples are unsigned values.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnAudioSamplesOut,(PPDMIAUDIOSNIFFERCONNECTOR pInterface, void *pvSamples, uint32_t cSamples,
                                                   int iSampleHz, int cChannels, int cBits, bool fUnsigned));

    /**
     * AudioSniffer device calls this method when output volume is changed.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   u16LeftVolume       0..0xFFFF volume level for left channel.
     * @param   u16RightVolume      0..0xFFFF volume level for right channel.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnAudioVolumeOut,(PPDMIAUDIOSNIFFERCONNECTOR pInterface, uint16_t u16LeftVolume, uint16_t u16RightVolume));

    /**
     * Audio input has been requested by the virtual audio device.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   ppvUserCtx          The interface context for this audio input stream,
     *                              it will be used in the pfnAudioInputEnd call.
     * @param   pvContext           The context pointer to be used in PDMIAUDIOSNIFFERPORT::pfnAudioInputEvent.
     * @param   cSamples            How many samples in a block is preferred in
     *                              PDMIAUDIOSNIFFERPORT::pfnAudioInputEvent.
     * @param   iSampleHz           The sample frequency in Hz.
     * @param   cChannels           Number of channels. 1 for mono, 2 for stereo.
     * @param   cBits               How many bits a sample for a single channel has. Normally 8 or 16.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnAudioInputBegin,(PPDMIAUDIOSNIFFERCONNECTOR pInterface,
                                                  void **ppvUserCtx,
                                                  void *pvContext,
                                                  uint32_t cSamples,
                                                  uint32_t iSampleHz,
                                                  uint32_t cChannels,
                                                  uint32_t cBits));

    /**
     * Audio input has been requested by the virtual audio device.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvUserCtx           The interface context for this audio input stream,
     *                              which was returned by pfnAudioInputBegin call.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnAudioInputEnd,(PPDMIAUDIOSNIFFERCONNECTOR pInterface,
                                                 void *pvUserCtx));
} PDMIAUDIOSNIFFERCONNECTOR;
/** PDMIAUDIOSNIFFERCONNECTOR - The Audio Sniffer Driver connector interface. */
#define PDMIAUDIOSNIFFERCONNECTOR_IID           "9d37f543-27af-45f8-8002-8ef7abac71e4"


/**
 * Generic status LED core.
 * Note that a unit doesn't have to support all the indicators.
 */
typedef union PDMLEDCORE
{
    /** 32-bit view. */
    uint32_t volatile u32;
    /** Bit view. */
    struct
    {
        /** Reading/Receiving indicator. */
        uint32_t    fReading : 1;
        /** Writing/Sending indicator. */
        uint32_t    fWriting : 1;
        /** Busy indicator. */
        uint32_t    fBusy : 1;
        /** Error indicator. */
        uint32_t    fError : 1;
    }           s;
} PDMLEDCORE;

/** LED bit masks for the u32 view.
 * @{ */
/** Reading/Receiving indicator. */
#define PDMLED_READING  RT_BIT(0)
/** Writing/Sending indicator. */
#define PDMLED_WRITING  RT_BIT(1)
/** Busy indicator. */
#define PDMLED_BUSY     RT_BIT(2)
/** Error indicator. */
#define PDMLED_ERROR    RT_BIT(3)
/** @} */


/**
 * Generic status LED.
 * Note that a unit doesn't have to support all the indicators.
 */
typedef struct PDMLED
{
    /** Just a magic for sanity checking. */
    uint32_t    u32Magic;
    uint32_t    u32Alignment;           /**< structure size alignment. */
    /** The actual LED status.
     * Only the device is allowed to change this. */
    PDMLEDCORE  Actual;
    /** The asserted LED status which is cleared by the reader.
     * The device will assert the bits but never clear them.
     * The driver clears them as it sees fit. */
    PDMLEDCORE  Asserted;
} PDMLED;

/** Pointer to an LED. */
typedef PDMLED *PPDMLED;
/** Pointer to a const LED. */
typedef const PDMLED *PCPDMLED;

/** Magic value for PDMLED::u32Magic. */
#define PDMLED_MAGIC    UINT32_C(0x11335577)

/** Pointer to an LED ports interface. */
typedef struct PDMILEDPORTS      *PPDMILEDPORTS;
/**
 * Interface for exporting LEDs (down).
 * Pair with PDMILEDCONNECTORS.
 */
typedef struct PDMILEDPORTS
{
    /**
     * Gets the pointer to the status LED of a unit.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   iLUN            The unit which status LED we desire.
     * @param   ppLed           Where to store the LED pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryStatusLed,(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed));

} PDMILEDPORTS;
/** PDMILEDPORTS interface ID. */
#define PDMILEDPORTS_IID                        "435e0cec-8549-4ca0-8c0d-98e52f1dc038"


/** Pointer to an LED connectors interface. */
typedef struct PDMILEDCONNECTORS *PPDMILEDCONNECTORS;
/**
 * Interface for reading LEDs (up).
 * Pair with PDMILEDPORTS.
 */
typedef struct PDMILEDCONNECTORS
{
    /**
     * Notification about a unit which have been changed.
     *
     * The driver must discard any pointers to data owned by
     * the unit and requery it.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   iLUN            The unit number.
     */
    DECLR3CALLBACKMEMBER(void, pfnUnitChanged,(PPDMILEDCONNECTORS pInterface, unsigned iLUN));
} PDMILEDCONNECTORS;
/** PDMILEDCONNECTORS interface ID. */
#define PDMILEDCONNECTORS_IID                   "8ed63568-82a7-4193-b57b-db8085ac4495"


/** Pointer to a Media Notification interface. */
typedef struct PDMIMEDIANOTIFY  *PPDMIMEDIANOTIFY;
/**
 * Interface for exporting Medium eject information (up).  No interface pair.
 */
typedef struct PDMIMEDIANOTIFY
{
    /**
     * Signals that the medium was ejected.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   iLUN            The unit which had the medium ejected.
     */
    DECLR3CALLBACKMEMBER(int, pfnEjected,(PPDMIMEDIANOTIFY pInterface, unsigned iLUN));

} PDMIMEDIANOTIFY;
/** PDMIMEDIANOTIFY interface ID. */
#define PDMIMEDIANOTIFY_IID                     "fc22d53e-feb1-4a9c-b9fb-0a990a6ab288"


/** The special status unit number */
#define PDM_STATUS_LUN      999


#ifdef VBOX_WITH_HGCM

/** Abstract HGCM command structure. Used only to define a typed pointer. */
struct VBOXHGCMCMD;

/** Pointer to HGCM command structure. This pointer is unique and identifies
 *  the command being processed. The pointer is passed to HGCM connector methods,
 *  and must be passed back to HGCM port when command is completed.
 */
typedef struct VBOXHGCMCMD *PVBOXHGCMCMD;

/** Pointer to a HGCM port interface. */
typedef struct PDMIHGCMPORT *PPDMIHGCMPORT;
/**
 * Host-Guest communication manager port interface (down). Normally implemented
 * by VMMDev.
 * Pair with PDMIHGCMCONNECTOR.
 */
typedef struct PDMIHGCMPORT
{
    /**
     * Notify the guest on a command completion.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   rc                  The return code (VBox error code).
     * @param   pCmd                A pointer that identifies the completed command.
     *
     * @returns VBox status code
     */
    DECLR3CALLBACKMEMBER(void, pfnCompleted,(PPDMIHGCMPORT pInterface, int32_t rc, PVBOXHGCMCMD pCmd));

} PDMIHGCMPORT;
/** PDMIHGCMPORT interface ID. */
# define PDMIHGCMPORT_IID                       "e00a0cbf-b75a-45c3-87f4-41cddbc5ae0b"


/** Pointer to a HGCM service location structure. */
typedef struct HGCMSERVICELOCATION *PHGCMSERVICELOCATION;

/** Pointer to a HGCM connector interface. */
typedef struct PDMIHGCMCONNECTOR *PPDMIHGCMCONNECTOR;
/**
 * The Host-Guest communication manager connector interface (up). Normally
 * implemented by Main::VMMDevInterface.
 * Pair with PDMIHGCMPORT.
 */
typedef struct PDMIHGCMCONNECTOR
{
    /**
     * Locate a service and inform it about a client connection.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   pServiceLocation    Pointer to the service location structure.
     * @param   pu32ClientID        Where to store the client id for the connection.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnConnect,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, PHGCMSERVICELOCATION pServiceLocation, uint32_t *pu32ClientID));

    /**
     * Disconnect from service.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   u32ClientID         The client id returned by the pfnConnect call.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnDisconnect,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID));

    /**
     * Process a guest issued command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   u32ClientID         The client id returned by the pfnConnect call.
     * @param   u32Function         Function to be performed by the service.
     * @param   cParms              Number of parameters in the array pointed to by paParams.
     * @param   paParms             Pointer to an array of parameters.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnCall,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function,
                                       uint32_t cParms, PVBOXHGCMSVCPARM paParms));

} PDMIHGCMCONNECTOR;
/** PDMIHGCMCONNECTOR interface ID. */
# define PDMIHGCMCONNECTOR_IID                  "a1104758-c888-4437-8f2a-7bac17865b5c"

#endif /* VBOX_WITH_HGCM */

/**
 * Data direction.
 */
typedef enum PDMSCSIREQUESTTXDIR
{
    PDMSCSIREQUESTTXDIR_UNKNOWN     = 0x00,
    PDMSCSIREQUESTTXDIR_FROM_DEVICE = 0x01,
    PDMSCSIREQUESTTXDIR_TO_DEVICE   = 0x02,
    PDMSCSIREQUESTTXDIR_NONE        = 0x03,
    PDMSCSIREQUESTTXDIR_32BIT_HACK  = 0x7fffffff
} PDMSCSIREQUESTTXDIR;

/**
 * SCSI request structure.
 */
typedef struct PDMSCSIREQUEST
{
    /** The logical unit. */
    uint32_t               uLogicalUnit;
    /** Direction of the data flow. */
    PDMSCSIREQUESTTXDIR    uDataDirection;
    /** Size of the SCSI CDB. */
    uint32_t               cbCDB;
    /** Pointer to the SCSI CDB. */
    uint8_t               *pbCDB;
    /** Overall size of all scatter gather list elements
     *  for data transfer if any. */
    uint32_t               cbScatterGather;
    /** Number of elements in the scatter gather list. */
    uint32_t               cScatterGatherEntries;
    /** Pointer to the head of the scatter gather list. */
    PRTSGSEG               paScatterGatherHead;
    /** Size of the sense buffer. */
    uint32_t               cbSenseBuffer;
    /** Pointer to the sense buffer. *
     * Current assumption that the sense buffer is not scattered. */
    uint8_t               *pbSenseBuffer;
    /** Opaque user data for use by the device. Left untouched by everything else! */
    void                  *pvUser;
} PDMSCSIREQUEST, *PPDMSCSIREQUEST;
/** Pointer to a const SCSI request structure. */
typedef const PDMSCSIREQUEST *PCSCSIREQUEST;

/** Pointer to a SCSI port interface. */
typedef struct PDMISCSIPORT *PPDMISCSIPORT;
/**
 * SCSI command execution port interface (down).
 * Pair with PDMISCSICONNECTOR.
 */
typedef struct PDMISCSIPORT
{

    /**
     * Notify the device on request completion.
     *
     * @returns VBox status code.
     * @param   pInterface    Pointer to this interface.
     * @param   pSCSIRequest  Pointer to the finished SCSI request.
     * @param   rcCompletion  SCSI_STATUS_* code for the completed request.
     * @param   fRedo         Flag whether the request can to be redone
     *                        when it failed.
     * @param   rcReq         The status code the request completed with (VERR_*)
     *                        Should be only used to choose the correct error message
     *                        displayed to the user if the error can be fixed by him
     *                        (fRedo is true).
     */
     DECLR3CALLBACKMEMBER(int, pfnSCSIRequestCompleted, (PPDMISCSIPORT pInterface, PPDMSCSIREQUEST pSCSIRequest,
                                                         int rcCompletion, bool fRedo, int rcReq));

    /**
     * Returns the storage controller name, instance and LUN of the attached medium.
     *
     * @returns VBox status.
     * @param   pInterface      Pointer to this interface.
     * @param   ppcszController Where to store the name of the storage controller.
     * @param   piInstance      Where to store the instance number of the controller.
     * @param   piLUN           Where to store the LUN of the attached device.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryDeviceLocation, (PPDMISCSIPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN));

} PDMISCSIPORT;
/** PDMISCSIPORT interface ID. */
#define PDMISCSIPORT_IID                        "05d9fc3b-e38c-4b30-8344-a323feebcfe5"


/** Pointer to a SCSI connector interface. */
typedef struct PDMISCSICONNECTOR *PPDMISCSICONNECTOR;
/**
 * SCSI command execution connector interface (up).
 * Pair with PDMISCSIPORT.
 */
typedef struct PDMISCSICONNECTOR
{

    /**
     * Submits a SCSI request for execution.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     * @param   pSCSIRequest    Pointer to the SCSI request to execute.
     */
     DECLR3CALLBACKMEMBER(int, pfnSCSIRequestSend, (PPDMISCSICONNECTOR pInterface, PPDMSCSIREQUEST pSCSIRequest));

} PDMISCSICONNECTOR;
/** PDMISCSICONNECTOR interface ID. */
#define PDMISCSICONNECTOR_IID                   "94465fbd-a2f2-447e-88c9-7366421bfbfe"


/** Pointer to a display VBVA callbacks interface. */
typedef struct PDMIDISPLAYVBVACALLBACKS *PPDMIDISPLAYVBVACALLBACKS;
/**
 * Display VBVA callbacks interface (up).
 */
typedef struct PDMIDISPLAYVBVACALLBACKS
{

    /**
     * Informs guest about completion of processing the given Video HW Acceleration
     * command, does not wait for the guest to process the command.
     *
     * @returns ???
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                The Video HW Acceleration Command that was
     *                              completed.
     * @todo r=bird: if async means asynchronous; then
     *                   s/pfnVHWACommandCompleteAsynch/pfnVHWACommandCompleteAsync/;
     *               fi
     */
    DECLR3CALLBACKMEMBER(int, pfnVHWACommandCompleteAsynch, (PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                             PVBOXVHWACMD pCmd));

    DECLR3CALLBACKMEMBER(int, pfnCrHgsmiCommandCompleteAsync, (PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                               PVBOXVDMACMD_CHROMIUM_CMD pCmd, int rc));

    DECLR3CALLBACKMEMBER(int, pfnCrHgsmiControlCompleteAsync, (PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                               PVBOXVDMACMD_CHROMIUM_CTL pCmd, int rc));
} PDMIDISPLAYVBVACALLBACKS;
/** PDMIDISPLAYVBVACALLBACKS  */
#define PDMIDISPLAYVBVACALLBACKS_IID            "b78b81d2-c821-4e66-96ff-dbafa76343a5"

/** Pointer to a PCI raw connector interface. */
typedef struct PDMIPCIRAWCONNECTOR *PPDMIPCIRAWCONNECTOR;
/**
 * PCI raw connector interface (up).
 */
typedef struct PDMIPCIRAWCONNECTOR
{

    /**
     *
     */
    DECLR3CALLBACKMEMBER(int, pfnDeviceConstructComplete, (PPDMIPCIRAWCONNECTOR pInterface, const char *pcszName,
                                                           uint32_t uHostPciAddress, uint32_t uGuestPciAddress,
                                                           int rc));

} PDMIPCIRAWCONNECTOR;
/** PDMIPCIRAWCONNECTOR interface ID. */
#define PDMIPCIRAWCONNECTOR_IID                 "14aa9c6c-8869-4782-9dfc-910071a6aebf"

/** @} */

RT_C_DECLS_END

#endif
