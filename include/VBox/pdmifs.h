/** @file
 * PDM - Pluggable Device Manager, Interfaces.
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

#ifndef ___VBox_pdmifs_h
#define ___VBox_pdmifs_h

#include <VBox/types.h>
#include <VBox/hgcmsvc.h>

__BEGIN_DECLS

/** @defgroup grp_pdm_interfaces    Interfaces
 * @ingroup grp_pdm
 * @{
 */

/**
 * Driver interface identficators.
 *
 * @remark  All interfaces have to be declared here. There is no such thing as
 *          private interface identifiers since they must be unique.
 *
 *          That said, interface structures and other stuff can be put elsewhere,
 *          actually, it is best if this file is not flooded with structures that
 *          could be put closer to home.
 */
typedef enum PDMINTERFACE
{
    /** PDMIBASE                - The interface everyone supports. */
    PDMINTERFACE_BASE = 1,
    /** PDMIMOUSEPORT           - The mouse port interface.             (Down)  Coupled with PDMINTERFACE_MOUSE_CONNECTOR. */
    PDMINTERFACE_MOUSE_PORT,
    /** PDMIMOUSECONNECTOR      - The mouse connector interface.        (Up)    Coupled with PDMINTERFACE_MOUSE_PORT. */
    PDMINTERFACE_MOUSE_CONNECTOR,
    /** PDMIKEYBOARDPORT        - The keyboard port interface.          (Down)  Coupled with PDMINTERFACE_KEYBOARD_CONNECTOR. */
    PDMINTERFACE_KEYBOARD_PORT,
    /** PDMIKEYBOARDCONNECTOR   - The keyboard connector interface.     (Up)    Coupled with PDMINTERFACE_KEYBOARD_PORT. */
    PDMINTERFACE_KEYBOARD_CONNECTOR,
    /** PDMIDISPLAYPORT         - The display port interface.           (Down)  Coupled with PDMINTERFACE_DISPLAY_CONNECTOR. */
    PDMINTERFACE_DISPLAY_PORT,
    /** PDMIDISPLAYCONNECTOR    - The display connector interface.      (Up)    Coupled with PDMINTERFACE_DISPLAY_PORT. */
    PDMINTERFACE_DISPLAY_CONNECTOR,
    /** PDMICHARPORT            - The char notify interface.            (Down)  Coupled with PDMINTERFACE_CHAR. */
    PDMINTERFACE_CHAR_PORT,
    /** PDMICHAR                - The char driver interface.            (Up)    Coupled with PDMINTERFACE_CHAR_PORT. */
    PDMINTERFACE_CHAR,
    /** PDMISTREAM              - The stream driver interface           (Up)    No coupling.
     * Used by a char driver to implement PDMINTERFACE_CHAR. */
    PDMINTERFACE_STREAM,
    /** PDMIBLOCKPORT           - The block notify interface            (Down)  Coupled with PDMINTERFACE_BLOCK. */
    PDMINTERFACE_BLOCK_PORT,
    /** PDMIBLOCK               - The block driver interface            (Up)    Coupled with PDMINTERFACE_BLOCK_PORT. */
    PDMINTERFACE_BLOCK,
    /** PDMIBLOCKBIOS           - The block bios interface.             (External) */
    PDMINTERFACE_BLOCK_BIOS,
    /** PDMIMOUNTNOTIFY         - The mountable notification interface. (Down)  Coupled with PDMINTERFACE_MOUNT. */
    PDMINTERFACE_MOUNT_NOTIFY,
    /** PDMIMOUNT               - The mountable interface.              (Up)    Coupled with PDMINTERFACE_MOUNT_NOTIFY. */
    PDMINTERFACE_MOUNT,
    /** PDMIMEDIA               - The media interface.                  (Up)    No coupling.
     * Used by a block unit driver to implement PDMINTERFACE_BLOCK and PDMINTERFACE_BLOCK_BIOS. */
    PDMINTERFACE_MEDIA,
    /** PDMIISCSITRANSPORT      - The iSCSI transport interface         (Up)    No coupling.
     * used by the iSCSI media driver.  */
    PDMINTERFACE_ISCSITRANSPORT,
    /** PDMIMEDIAASYNC          - Async version of the media interface  (Down)  Coupled with PDMINTERFACE_MEDIA_ASYNC_PORT. */
    PDMINTERFACE_MEDIA_ASYNC,
    /** PDMIMEDIAASYNCPORT      - Async version of the media interface  (Up)    Coupled with PDMINTERFACE_MEDIA_ASYNC. */
    PDMINTERFACE_MEDIA_ASYNC_PORT,
    /** PDMIBLOCKASYNC          - Async version of the block interface  (Down)  Coupled with PDMINTERFACE_BLOCK_ASYNC_PORT. */
    PDMINTERFACE_BLOCK_ASYNC,
    /** PDMIBLOCKASYNCPORT      - Async version of the block interface  (Up)    Coupled with PDMINTERFACE_BLOCK_ASYNC. */
    PDMINTERFACE_BLOCK_ASYNC_PORT,
    /** PDMITRANSPORTASYNC      - Transport data async to their target  (Down)  Coupled with PDMINTERFACE_TRANSPORT_ASYNC_PORT. */
    PDMINTERFACE_TRANSPORT_ASYNC,
    /** PDMITRANSPORTASYNCPORT  - Transport data async to their target  (Up)    Coupled with PDMINTERFACE_TRANSPORT_ASYNC. */
    PDMINTERFACE_TRANSPORT_ASYNC_PORT,


    /** PDMINETWORKPORT         - The network port interface.           (Down)  Coupled with PDMINTERFACE_NETWORK_CONNECTOR. */
    PDMINTERFACE_NETWORK_PORT,
    /** PDMINETWORKPORT         - The network connector interface.      (Up)    Coupled with PDMINTERFACE_NETWORK_PORT. */
    PDMINTERFACE_NETWORK_CONNECTOR,
    /** PDMINETWORKCONFIG       - The network configuartion interface.  (Main)  Used by the managment api. */
    PDMINTERFACE_NETWORK_CONFIG,

    /** PDMIAUDIOCONNECTOR      - The audio driver interface.           (Up)    No coupling. */
    PDMINTERFACE_AUDIO_CONNECTOR,

    /** PDMIAUDIOSNIFFERPORT    - The Audio Sniffer Device port interface. */
    PDMINTERFACE_AUDIO_SNIFFER_PORT,
    /** PDMIAUDIOSNIFFERCONNECTOR - The Audio Sniffer Driver connector interface. */
    PDMINTERFACE_AUDIO_SNIFFER_CONNECTOR,

    /** PDMIVMMDEVPORT          - The VMM Device port interface. */
    PDMINTERFACE_VMMDEV_PORT,
    /** PDMIVMMDEVCONNECTOR     - The VMM Device connector interface. */
    PDMINTERFACE_VMMDEV_CONNECTOR,

    /** PDMILEDPORTS            - The generic LED port interface.       (Down)  Coupled with PDMINTERFACE_LED_CONNECTORS. */
    PDMINTERFACE_LED_PORTS,
    /** PDMILEDCONNECTORS       - The generic LED connector interface.  (Up)    Coupled with PDMINTERFACE_LED_PORTS.  */
    PDMINTERFACE_LED_CONNECTORS,

    /** PDMIACPIPORT            - ACPI port interface.                  (Down)   Coupled with PDMINTERFACE_ACPI_CONNECTOR. */
    PDMINTERFACE_ACPI_PORT,
    /** PDMIACPICONNECTOR       - ACPI connector interface.             (Up)     Coupled with PDMINTERFACE_ACPI_PORT. */
    PDMINTERFACE_ACPI_CONNECTOR,

    /** PDMIHGCMPORT            - The Host-Guest communication manager port interface. Normally implemented by VMMDev. */
    PDMINTERFACE_HGCM_PORT,
    /** PDMIHGCMCONNECTOR       - The Host-Guest communication manager connector interface. Normally implemented by Main::VMMDevInterface. */
    PDMINTERFACE_HGCM_CONNECTOR,

    /** VUSBIROOTHUBPORT        - VUSB RootHub port interface.          (Down)   Coupled with PDMINTERFACE_USB_RH_CONNECTOR. */
    PDMINTERFACE_VUSB_RH_PORT,
    /** VUSBIROOTHUBCONNECTOR   - VUSB RootHub connector interface.     (Up)     Coupled with PDMINTERFACE_USB_RH_PORT. */
    PDMINTERFACE_VUSB_RH_CONNECTOR,
    /** VUSBIRHCONFIG           - VUSB RootHub configuration interface. (Main)   Used by the managment api. */
    PDMINTERFACE_VUSB_RH_CONFIG,

    /** VUSBROOTHUBCONNECTOR    - VUSB Device interface.                (Up)     No coupling. */
    PDMINTERFACE_VUSB_DEVICE,

    /** PDMIHOSTPARALLELPORT      - The Host Parallel port interface.     (Down)   Coupled with PDMINTERFACE_HOST_PARALLEL_CONNECTOR. */
    PDMINTERFACE_HOST_PARALLEL_PORT,
    /** PDMIHOSTPARALLELCONNECTOR - The Host Parallel connector interface (Up)     Coupled with PDMINTERFACE_HOST_PARALLEL_PORT. */
    PDMINTERFACE_HOST_PARALLEL_CONNECTOR,

    /** Maximum interface number. */
    PDMINTERFACE_MAX
} PDMINTERFACE;


/**
 * PDM Driver Base Interface.
 */
typedef struct PDMIBASE
{
    /**
     * Queries an interface to the driver.
     *
     * @returns Pointer to interface.
     * @returns NULL if the interface was not supported by the driver.
     * @param   pInterface          Pointer to this interface structure.
     * @param   enmInterface        The requested interface identification.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(void *, pfnQueryInterface,(struct PDMIBASE *pInterface, PDMINTERFACE enmInterface));
} PDMIBASE;


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
 * Mouse port interface.
 * Pair with PDMIMOUSECONNECTOR.
 */
typedef struct PDMIMOUSEPORT
{
    /**
     * Puts a mouse event.
     * This is called by the source of mouse events. The event will be passed up until the
     * topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface structure.
     * @param   i32DeltaX       The X delta.
     * @param   i32DeltaY       The Y delta.
     * @param   i32DeltaZ       The Z delta.
     * @param   fButtonStates   The button states, see the PDMIMOUSEPORT_BUTTON_* \#defines.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEvent,(PPDMIMOUSEPORT pInterface, int32_t i32DeltaX, int32_t i32DeltaY, int32_t i32DeltaZ, uint32_t fButtonStates));
} PDMIMOUSEPORT;

/** Mouse button defines for PDMIMOUSEPORT::pfnPutEvent.
 * @{ */
#define PDMIMOUSEPORT_BUTTON_LEFT   RT_BIT(0)
#define PDMIMOUSEPORT_BUTTON_RIGHT  RT_BIT(1)
#define PDMIMOUSEPORT_BUTTON_MIDDLE RT_BIT(2)
/** @} */


/**
 * Mouse connector interface.
 * Pair with PDMIMOUSEPORT.
 */
typedef PDMIDUMMY PDMIMOUSECONNECTOR;
 /** Pointer to a mouse connector interface. */
typedef PDMIMOUSECONNECTOR *PPDMIMOUSECONNECTOR;


/** Pointer to a keyboard port interface. */
typedef struct PDMIKEYBOARDPORT *PPDMIKEYBOARDPORT;
/**
 * Keyboard port interface.
 * Pair with PDMIKEYBOARDCONNECTOR.
 */
typedef struct PDMIKEYBOARDPORT
{
    /**
     * Puts a keyboard event.
     * This is called by the source of keyboard events. The event will be passed up until the
     * topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface structure.
     * @param   u8KeyCode           The keycode to queue.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEvent,(PPDMIKEYBOARDPORT pInterface, uint8_t u8KeyCode));
} PDMIKEYBOARDPORT;

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
 * Keyboard connector interface.
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

} PDMIKEYBOARDCONNECTOR;



/** Pointer to a display port interface. */
typedef struct PDMIDISPLAYPORT *PPDMIDISPLAYPORT;
/**
 * Display port interface.
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
     * @param   cMilliesInterval    Number of millies between two refreshes.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetRefreshRate,(PPDMIDISPLAYPORT pInterface, uint32_t cMilliesInterval));

    /**
     * Create a 32-bbp snapshot of the display.
     *
     * This will create a 32-bbp bitmap with dword aligned scanline length. Because
     * of a wish for no locks in the graphics device, this must be called from the
     * emulation thread.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvData              Pointer the buffer to copy the bits to.
     * @param   cbData              Size of the buffer.
     * @param   pcx                 Where to store the width of the bitmap. (optional)
     * @param   pcy                 Where to store the height of the bitmap. (optional)
     * @param   pcbData             Where to store the actual size of the bitmap. (optional)
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSnapshot,(PPDMIDISPLAYPORT pInterface, void *pvData, size_t cbData, uint32_t *pcx, uint32_t *pcy, size_t *pcbData));

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

} PDMIDISPLAYPORT;


/** Pointer to a display connector interface. */
typedef struct PDMIDISPLAYCONNECTOR *PPDMIDISPLAYCONNECTOR;
/**
 * Display connector interface.
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

/**
 * Block notify interface.
 * Pair with PDMIBLOCK.
 */
typedef PDMIDUMMY PDMIBLOCKPORT;
/** Pointer to a block notify interface (dummy). */
typedef PDMIBLOCKPORT *PPDMIBLOCKPORT;

/** Pointer to a block interface. */
typedef struct PDMIBLOCK *PPDMIBLOCK;
/**
 * Block interface.
 * Pair with PDMIBLOCKPORT.
 */
typedef struct PDMIBLOCK
{
    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIBLOCK pInterface, uint64_t off, void *pvBuf, size_t cbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
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
    DECLR3CALLBACKMEMBER(int, pfnSendCmd,(PPDMIBLOCK pInterface, const uint8_t *pbCmd, PDMBLOCKTXDIR enmTxDir, void *pvBuf, size_t *pcbBuf, uint8_t *pbSenseKey, uint32_t cTimeoutMillies));

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
} PDMIBLOCK;


/** Pointer to a mount interface. */
typedef struct PDMIMOUNTNOTIFY *PPDMIMOUNTNOTIFY;
/**
 * Block interface.
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


/* Pointer to mount interface. */
typedef struct PDMIMOUNT *PPDMIMOUNT;
/**
 * Mount interface.
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
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnmount,(PPDMIMOUNT pInterface, bool fForce));

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
} PDMIBLOCKMOUNT;

/**
 * BIOS translation mode.
 */
typedef enum PDMBIOSTRANSLATION
{
    /** No translation. */
    PDMBIOSTRANSLATION_NONE = 1,
    /** LBA translation. */
    PDMBIOSTRANSLATION_LBA,
    /** Automatic select mode. */
    PDMBIOSTRANSLATION_AUTO
} PDMBIOSTRANSLATION;

/** Pointer to BIOS translation mode. */
typedef PDMBIOSTRANSLATION *PPDMBIOSTRANSLATION;

/** Pointer to a media interface. */
typedef struct PDMIMEDIA *PPDMIMEDIA;
/**
 * Media interface.
 * Makes up the fundation for PDMIBLOCK and PDMIBLOCKBIOS.
 */
typedef struct PDMIMEDIA
{
    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
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
     * Get stored media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnBiosSetGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pcCylinders     Number of cylinders.
     * @param   pcHeads         Number of heads.
     * @param   pcSectors       Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetGeometry,(PPDMIMEDIA pInterface, uint32_t *pcCylinders, uint32_t *pcHeads, uint32_t *pcSectors));

    /**
     * Store the media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cCylinders      Number of cylinders.
     * @param   cHeads          Number of heads.
     * @param   cSectors        Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetGeometry,(PPDMIMEDIA pInterface, uint32_t cCylinders, uint32_t cHeads, uint32_t cSectors));

    /**
     * Get stored geometry translation mode - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry translation mode.
     * @returns VERR_PDM_TRANSLATION_NOT_SET if the translation hasn't been set using pfnBiosSetTranslation() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   penmTranslation Where to store the translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetTranslation,(PPDMIMEDIA pInterface, PPDMBIOSTRANSLATION penmTranslation));

    /**
     * Store media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmTranslation  The translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetTranslation,(PPDMIMEDIA pInterface, PDMBIOSTRANSLATION enmTranslation));

    /**
     * Gets the UUID of the media drive.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pUuid           Where to store the UUID on success.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid,(PPDMIMEDIA pInterface, PRTUUID pUuid));

} PDMIMEDIA;


/** Pointer to a block BIOS interface. */
typedef struct PDMIBLOCKBIOS *PPDMIBLOCKBIOS;
/**
 * Media BIOS interface.
 * The interface the getting and setting properties which the BIOS/CMOS care about.
 */
typedef struct PDMIBLOCKBIOS
{
    /**
     * Get stored media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pcCylinders     Number of cylinders.
     * @param   pcHeads         Number of heads.
     * @param   pcSectors       Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetGeometry,(PPDMIBLOCKBIOS pInterface, uint32_t *pcCylinders, uint32_t *pcHeads, uint32_t *pcSectors));

    /**
     * Store the media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cCylinders      Number of cylinders.
     * @param   cHeads          Number of heads.
     * @param   cSectors        Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetGeometry,(PPDMIBLOCKBIOS pInterface, uint32_t cCylinders, uint32_t cHeads, uint32_t cSectors));

    /**
     * Get stored geometry translation mode - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry translation mode.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   penmTranslation Where to store the translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetTranslation,(PPDMIBLOCKBIOS pInterface, PPDMBIOSTRANSLATION penmTranslation));

    /**
     * Store media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmTranslation  The translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetTranslation,(PPDMIBLOCKBIOS pInterface, PDMBIOSTRANSLATION enmTranslation));

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


/** Pointer to an iSCSI Request PDU buffer. */
typedef struct ISCSIREQ *PISCSIREQ;
/**
 * iSCSI Request PDU buffer (gather).
 */
typedef struct ISCSIREQ
{
    /** Length of PDU segment in bytes. */
    size_t cbSeg;
    /** Pointer to PDU segment. */
    const void *pcvSeg;
} ISCSIREQ;

/** Pointer to an iSCSI Response PDU buffer. */
typedef struct ISCSIRES *PISCSIRES;
/**
 * iSCSI Response PDU buffer (scatter).
 */
typedef struct ISCSIRES
{
    /** Length of PDU segment. */
    size_t cbSeg;
    /** Pointer to PDU segment. */
    void *pvSeg;
} ISCSIRES;

/** Pointer to an iSCSI transport driver interface. */
typedef struct PDMIISCSITRANSPORT *PPDMIISCSITRANSPORT;
/**
 * iSCSI transport driver interface.
 */
typedef struct PDMIISCSITRANSPORT
{
    /**
     * Read bytes from an iSCSI transport stream. If the connection fails, it is automatically
     * reopened on the next call after the error is signalled. Error recovery in this case is
     * the duty of the caller.
     *
     * @returns VBox status code.
     * @param   pTransport      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbBuf           Number of bytes to read.
     * @param   pcbRead         Actual number of bytes read.
     * @thread  Any thread.
     * @todo    Correct the docs.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIISCSITRANSPORT pTransport, PISCSIRES prgResponse, unsigned int cnResponse));

    /**
     * Write bytes to an iSCSI transport stream. Padding is performed when necessary. If the connection
     * fails, it is automatically reopened on the next call after the error is signalled. Error recovery
     * in this case is the duty of the caller.
     *
     * @returns VBox status code.
     * @param   pTransport      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where the write bits are stored.
     * @param   cbWrite         Number of bytes to write.
     * @thread  Any thread.
     * @todo    Correct the docs.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIISCSITRANSPORT pTransport, PISCSIREQ prgRequest, unsigned int cnRequest));

    /**
     * Open the iSCSI transport stream.
     *
     * @returns VBox status code.
     * @param   pTransport       Pointer to the interface structure containing the called function pointer.
     * @param   pszTargetAddress Pointer to string of the format address:port.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpen,(PPDMIISCSITRANSPORT pTransport, const char *pszTargetAddress));

    /**
     * Close the iSCSI transport stream.
     *
     * @returns VBox status code.
     * @param   pTransport      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnClose,(PPDMIISCSITRANSPORT pTransport));
} PDMIISCSITRANSPORT;


/** Pointer to a asynchronous block notify interface. */
typedef struct PDMIBLOCKASYNCPORT *PPDMIBLOCKASYNCPORT;
/**
 * Asynchronous block notify interface.
 * Pair with PDMIBLOCKASYNC.
 */
typedef struct PDMIBLOCKASYNCPORT
{
    /**
     * Notify completion of a read task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset the task read from.
     * @param   pvBuf           The buffer containig the read data.
     * @param   cbRead          Number of bytes read.
     * @param   pvUser          The user argument given in pfnStartRead.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadCompleteNotify, (PPDMIBLOCKASYNCPORT pInterface, uint64_t off, void *pvBuf, size_t cbRead, void *pvUser));

    /**
     * Notify completion of a write task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset the task has written to.
     * @param   pvBuf           The buffer containig the written data.
     * @param   cbWrite         Number of bytes actually written.
     * @param   pvUser          The user argument given in pfnStartWrite.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteCompleteNotify, (PPDMIBLOCKASYNCPORT pInterface, uint64_t off, void *pvBuf, size_t cbWrite, void *pvUser));
} PDMIBLOCKASYNCPORT;


/** Pointer to a asynchronous block interface. */
typedef struct PDMIBLOCKASYNC *PPDMIBLOCKASYNC;
/**
 * Asynchronous block interface.
 * Pair with PDMIBLOCKASYNCPORT.
 */
typedef struct PDMIBLOCKASYNC
{
    /**
     * Start reading task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartRead,(PPDMIBLOCKASYNC pInterface, uint64_t off, void *pvBuf, size_t cbRead, void *pvUser));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartWrite,(PPDMIBLOCKASYNC pInterface, uint64_t off, const void *pvBuf, size_t cbWrite, void *pvUser));

    /**
     * Make sure that the bits written are actually on the storage medium.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush,(PPDMIBLOCKASYNC pInterface));

    /**
     * Check if the media is readonly or not.
     *
     * @returns true if readonly.
     * @returns false if read/write.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsReadOnly,(PPDMIBLOCKASYNC pInterface));

    /**
     * Gets the media size in bytes.
     *
     * @returns Media size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize,(PPDMIBLOCKASYNC pInterface));

    /**
     * Gets the block drive type.
     *
     * @returns block drive type.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(PDMBLOCKTYPE, pfnGetType,(PPDMIBLOCKASYNC pInterface));

    /**
     * Gets the UUID of the block drive.
     * Don't return the media UUID if it's removable.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pUuid           Where to store the UUID on success.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid,(PPDMIBLOCKASYNC pInterface, PRTUUID pUuid));
} PDMIBLOCKASYNC;


/** Pointer to a asynchronous notification interface. */
typedef struct PDMIMEDIAASYNCPORT *PPDMIMEDIAASYNCPORT;
/**
 * Asynchronous media interface.
 * Makes up the fundation for PDMIBLOCKASYNC and PDMIBLOCKBIOS.
 */
typedef struct PDMIMEDIAASYNCPORT
{
    /**
     * Notify completion of a read task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset the task read from.
     * @param   pvBuf           The buffer containig the read data.
     * @param   cbRead          Number of bytes read.
     * @param   pvUser          The user argument given in pfnStartRead.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadCompleteNotify, (PPDMIMEDIAASYNCPORT pInterface, uint64_t off, void *pvBuf, size_t cbRead, void *pvUser));

    /**
     * Notify completion of a write task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset the task has written to.
     * @param   pvBuf           The buffer containig the written data.
     * @param   cbWritten       Number of bytes actually written.
     * @param   pvUser          The user argument given in pfnStartWrite.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteCompleteNotify, (PPDMIMEDIAASYNCPORT pInterface, uint64_t off, void *pvBuf, size_t cbWritten, void *pvUser));
} PDMIMEDIAASYNCPORT;


/** Pointer to a asynchronous media interface. */
typedef struct PDMIMEDIAASYNC *PPDMIMEDIAASYNC;
/**
 * Asynchronous media interface.
 * Makes up the fundation for PDMIBLOCKASYNC and PDMIBLOCKBIOS.
 */
typedef struct PDMIMEDIAASYNC
{
    /**
     * Start reading task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @param   pvUser          User data.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartRead,(PPDMIMEDIAASYNC pInterface, uint64_t off, void *pvBuf, size_t cbRead, void *pvUser));

    /**
     * Start writing task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
     * @param   pvUser          User data.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartWrite,(PPDMIMEDIAASYNC pInterface, uint64_t off, const void *pvBuf, size_t cbWrite, void *pvUser));

    /**
     * Make sure that the bits written are actually on the storage medium.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush,(PPDMIMEDIAASYNC pInterface));

    /**
     * Get the media size in bytes.
     *
     * @returns Media size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize,(PPDMIMEDIAASYNC pInterface));

    /**
     * Check if the media is readonly or not.
     *
     * @returns true if readonly.
     * @returns false if read/write.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsReadOnly,(PPDMIMEDIAASYNC pInterface));

    /**
     * Get stored media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnBiosSetGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pcCylinders     Number of cylinders.
     * @param   pcHeads         Number of heads.
     * @param   pcSectors       Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetGeometry,(PPDMIMEDIAASYNC pInterface, uint32_t *pcCylinders, uint32_t *pcHeads, uint32_t *pcSectors));

    /**
     * Store the media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cCylinders      Number of cylinders.
     * @param   cHeads          Number of heads.
     * @param   cSectors        Number of sectors. This number is 1-based.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetGeometry,(PPDMIMEDIAASYNC pInterface, uint32_t cCylinders, uint32_t cHeads, uint32_t cSectors));

    /**
     * Get stored geometry translation mode - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry translation mode.
     * @returns VERR_PDM_TRANSLATION_NOT_SET if the translation hasn't been set using pfnBiosSetTranslation() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   penmTranslation Where to store the translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetTranslation,(PPDMIMEDIAASYNC pInterface, PPDMBIOSTRANSLATION penmTranslation));

    /**
     * Store media geometry - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmTranslation  The translation type.
     * @remark  This have no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetTranslation,(PPDMIMEDIAASYNC pInterface, PDMBIOSTRANSLATION enmTranslation));

    /**
     * Gets the UUID of the media drive.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pUuid           Where to store the UUID on success.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid,(PPDMIMEDIAASYNC pInterface, PRTUUID pUuid));

} PDMIMEDIAASYNC;


/** Pointer to a async media notify interface. */
typedef struct PDMITRANSPORTASYNCPORT *PPDMITRANSPORTASYNCPORT;
/**
 * Notification interface for completed I/O tasks.
 * Pair with PDMITRANSPORTASYNC.
 */
typedef struct PDMITRANSPORTASYNCPORT
{
    /**
     * Notify completion of a read task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset the task read from.
     * @param   pvBuf           The buffer containig the read data.
     * @param   cbRead          Number of bytes read.
     * @param   pvUser          The user argument given in pfnStartRead.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadCompleteNotify, (PPDMITRANSPORTASYNCPORT pInterface, uint64_t off, void *pvBuf, size_t cbRead, void *pvUser));

    /**
     * Notify completion of a write task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset the task has written to.
     * @param   pvBuf           The buffer containig the written data.
     * @param   cbWritten       Number of bytes actually written.
     * @param   pvUser          The user argument given in pfnStartWrite.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteCompleteNotify, (PPDMITRANSPORTASYNCPORT pInterface, uint64_t off, void *pvBuf, size_t cbWritten, void *pvUser));
} PDMITRANSPORTASYNCPORT;


/** Pointer to a async media interface. */
typedef struct PDMITRANSPORTASYNC *PPDMITRANSPORTASYNC;
/**
 * Asynchronous transport interface.
 * Makes up the fundation for PDMIMEDIAASYNC.
 */
typedef struct PDMITRANSPORTASYNC
{
    /**
     * Read bits synchronous.
     * Blocks until finished.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containint the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           here to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @param   pcbRead         Where to store the number of bytes actually read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadSynchronous, (PPDMITRANSPORTASYNC pInterface, uint64_t off, void *pvBuf, size_t cbRead, size_t *pcbRead));

    /**
     * Write bits synchronous.
     * Blocks until finished.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containint the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           here to store the read bits.
     * @param   cbWrite         Number of bytes to read.
     * @param   pcbWritten      Where to store the number of bytes actually read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteSynchronous, (PPDMITRANSPORTASYNC pInterface, uint64_t off, void *pvBuf, size_t cbWrite, size_t *pcbWritten));

    /**
     * Start asynchronous read.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @param   pvUser          User argument returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadStartAsynchronous,(PPDMITRANSPORTASYNC pInterface, uint64_t off, void *pvBuf, size_t cbRead, void *pvUser));

    /**
     * Start asynchronous write.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
     * @param   pvUser          User argument returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteStartAsynchronous,(PPDMITRANSPORTASYNC pInterface, uint64_t off, const void *pvBuf, size_t cbWrite, void *pvUser));

    /**
     * Make sure that the bits written are actually on the storage medium.
     * This is a synchronous task
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlushSynchronous,(PPDMITRANSPORTASYNC pInterface));

    /**
     * Get the media size in bytes.
     *
     * @returns Media size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize,(PPDMITRANSPORTASYNC pInterface));

    /**
     * Check if the media is readonly or not.
     *
     * @returns true if readonly.
     * @returns false if read/write.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsReadOnly,(PPDMITRANSPORTASYNC pInterface));

    /**
     * Opens the data source.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pszPath         The path to open.
     * @param   fReadonly       If the target shoudl opened readonly.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpen, (PPDMITRANSPORTASYNC pInterface, const char *pszTargetPath, bool fReadonly));

    /**
     * Close the data source.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnClose, (PPDMITRANSPORTASYNC pInterface));

} PDMITRANSPORTASYNC;


/** @name Bit mask definitions for status line type
 * @{ */
#define PDM_ICHAR_STATUS_LINES_DCD  RT_BIT(0)
#define PDM_ICHAR_STATUS_LINES_RI   RT_BIT(1)
#define PDM_ICHAR_STATUS_LINES_DSR  RT_BIT(2)
#define PDM_ICHAR_STATUS_LINES_CTS  RT_BIT(3)
/** @} */

/** Pointer to a char port interface. */
typedef struct PDMICHARPORT *PPDMICHARPORT;
/**
 * Char port interface.
 * Pair with PDMICHAR.
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
} PDMICHARPORT;


/** Pointer to a char interface. */
typedef struct PDMICHAR *PPDMICHAR;
/**
 * Char interface.
 * Pair with PDMICHARPORT.
 */
typedef struct PDMICHAR
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
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMICHAR pInterface, const void *pvBuf, size_t cbWrite));

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
    DECLR3CALLBACKMEMBER(int, pfnSetParameters,(PPDMICHAR pInterface, unsigned Bps, char chParity, unsigned cDataBits, unsigned cStopBits));

    /**
     * Set the state of the modem lines.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   fRequestToSend      Set to true to make the Request to Send line active otherwise to 0.
     * @param   fDataTerminalReady  Set to true to make the Data Terminal Ready line active otherwise 0.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetModemLines,(PPDMICHAR pInterface, bool fRequestToSend, bool fDataTerminalReady));

} PDMICHAR;


/** Pointer to a stream interface. */
typedef struct PDMISTREAM *PPDMISTREAM;
/**
 * Stream interface.
 * Makes up the fundation for PDMICHAR.
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
 * Host parallel port interface.
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
     * Notify device/driver that an interrupt has occured.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyInterrupt, (PPDMIHOSTPARALLELPORT pInterface));
} PDMIHOSTPARALLELPORT;


/** Pointer to a Host Parallel connector interface. */
typedef struct PDMIHOSTPARALLELCONNECTOR *PPDMIHOSTPARALLELCONNECTOR;

/**
 * Host parallel connector interface
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
     * @param   val             The new control register value.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteControl,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t val));
 
    /**
     * Read control register bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the control register bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadControl,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pvBuf));

    /**
     * Read status register bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the status register bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadStatus,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pvBuf));

    /**
     * Set mode of the host parallel port.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   mode            The mode of the host parallel port.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetMode,(PPDMIHOSTPARALLELCONNECTOR pInterface, PDMPARALLELPORTMODE mode));
} PDMIHOSTPARALLELCONNECTOR;


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
    PDM_ACPI_BAT_STATE_CHARGING    = 0x01,
    PDM_ACPI_BAT_STATE_DISCHARGING = 0x02,
    PDM_ACPI_BAT_STATE_CRITICAL    = 0x04
} PDMACPIBATSTATE;
/** Pointer to ACPI battery state. */
typedef PDMACPIBATSTATE *PPDMACPIBATSTATE;

/** Pointer to an ACPI port interface. */
typedef struct PDMIACPIPORT *PPDMIACPIPORT;
/**
 * ACPI port interface.
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
} PDMIACPIPORT;

/** Pointer to an ACPI connector interface. */
typedef struct PDMIACPICONNECTOR *PPDMIACPICONNECTOR;
/**
 * ACPI connector interface.
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


/** Pointer to a VMMDevice port interface. */
typedef struct PDMIVMMDEVPORT *PPDMIVMMDEVPORT;
/**
 * VMMDevice port interface.
 */
typedef struct PDMIVMMDEVPORT
{
    /**
     * Return the current absolute mouse position in pixels
     *
     * @returns VBox status code
     * @param   pAbsX   Pointer of result value, can be NULL
     * @param   pAbsY   Pointer of result value, can be NULL
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryAbsoluteMouse,(PPDMIVMMDEVPORT pInterface, uint32_t *pAbsX, uint32_t *pAbsY));

    /**
     * Set the new absolute mouse position in pixels
     *
     * @returns VBox status code
     * @param   absX   New absolute X position
     * @param   absY   New absolute Y position
     */
    DECLR3CALLBACKMEMBER(int, pfnSetAbsoluteMouse,(PPDMIVMMDEVPORT pInterface, uint32_t absX, uint32_t absY));

    /**
     * Return the current mouse capability flags
     *
     * @returns VBox status code
     * @param   pCapabilities  Pointer of result value
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryMouseCapabilities,(PPDMIVMMDEVPORT pInterface, uint32_t *pCapabilities));

    /**
     * Set the current mouse capability flag (host side)
     *
     * @returns VBox status code
     * @param   capabilities  Capability mask
     */
    DECLR3CALLBACKMEMBER(int, pfnSetMouseCapabilities,(PPDMIVMMDEVPORT pInterface, uint32_t capabilities));

    /**
     * Issue a display resolution change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   cx          Horizontal pixel resolution (0 = do not change).
     * @param   cy          Vertical pixel resolution (0 = do not change).
     * @param   cBits       Bits per pixel (0 = do not change).
     * @param   display     The display index.
     */
    DECLR3CALLBACKMEMBER(int, pfnRequestDisplayChange,(PPDMIVMMDEVPORT pInterface, uint32_t cx, uint32_t cy, uint32_t cBits, uint32_t display));

    /**
     * Pass credentials to guest.
     *
     * Note that there can only be one set of credentials and the guest may or may not
     * query them and may do whatever it wants with them.
     *
     * @returns VBox status code
     * @param   pszUsername            User name, may be empty (UTF-8)
     * @param   pszPassword            Password, may be empty (UTF-8)
     * @param   pszDomain              Domain name, may be empty (UTF-8)
     * @param   fFlags                 Bitflags
     */
    DECLR3CALLBACKMEMBER(int, pfnSetCredentials,(PPDMIVMMDEVPORT pInterface, const char *pszUsername,
                                                 const char *pszPassword, const char *pszDomain,
                                                 uint32_t fFlags));

    /**
     * Notify the driver about a VBVA status change.
     *
     * @returns Nothing. Because it is informational callback.
     * @param   fEnabled    Current VBVA status.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAChange, (PPDMIVMMDEVPORT pInterface, bool fEnabled));

    /**
     * Issue a seamless mode change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   fEnabled       Seamless mode enabled or not
     */
    DECLR3CALLBACKMEMBER(int, pfnRequestSeamlessChange,(PPDMIVMMDEVPORT pInterface, bool fEnabled));

    /**
     * Issue a memory balloon change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   ulBalloonSize   Balloon size in megabytes
     */
    DECLR3CALLBACKMEMBER(int, pfnSetMemoryBalloon,(PPDMIVMMDEVPORT pInterface, uint32_t ulBalloonSize));

    /**
     * Issue a statistcs interval change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   ulStatInterval  Statistics query interval in seconds (0=disable)
     */
    DECLR3CALLBACKMEMBER(int, pfnSetStatisticsInterval,(PPDMIVMMDEVPORT pInterface, uint32_t ulStatInterval));

    /**
     * Notify the guest about a VRDP status change.
     *
     * @returns VBox status code
     * @param   fVRDPEnabled           Current VRDP status.
     * @param   u32VRDPExperienceLevel Which visual effects to be disabled in the guest.
     */
    DECLR3CALLBACKMEMBER(int, pfnVRDPChange, (PPDMIVMMDEVPORT pInterface, bool fVRDPEnabled, uint32_t u32VRDPExperienceLevel));

} PDMIVMMDEVPORT;

/** Forward declaration of the video accelerator command memory. */
struct _VBVAMEMORY;
/** Forward declaration of the guest information structure. */
struct VBoxGuestInfo;
/** Forward declaration of the guest statistics structure */
struct VBoxGuestStatistics;
/** Pointer to video accelerator command memory. */
typedef struct _VBVAMEMORY *PVBVAMEMORY;

/** Pointer to a VMMDev connector interface. */
typedef struct PDMIVMMDEVCONNECTOR *PPDMIVMMDEVCONNECTOR;
/**
 * VMMDev connector interface.
 * Pair with PDMIVMMDEVPORT.
 */
typedef struct PDMIVMMDEVCONNECTOR
{
    /**
     * Report guest OS version.
     * Called whenever the Additions issue a guest version report request.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pGuestInfo          Pointer to guest information structure
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestVersion,(PPDMIVMMDEVCONNECTOR pInterface, struct VBoxGuestInfo *pGuestInfo));

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
     * @param   cy              Video mode horizontal resolution in pixels.
     * @param   cx              Video mode vertical resolution in pixels.
     * @param   cBits           Video mode bits per pixel.
     * @param   pfSupported     Where to put the indicator for whether this mode is supported. (output)
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVideoModeSupported,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t cx, uint32_t cy, uint32_t cBits, bool *pfSupported));

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
     * Inflate or deflate the memory balloon
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   fInflate            Inflate or deflate
     * @param   cPages              Number of physical pages (must be 256 as we allocate in 1 MB chunks)
     * @param   aPhysPage           Array of physical page addresses
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnChangeMemoryBalloon, (PPDMIVMMDEVCONNECTOR pInterface, bool fInflate, uint32_t cPages, RTGCPHYS *aPhysPage));

} PDMIVMMDEVCONNECTOR;


/**
 * MAC address.
 * (The first 24 bits are the 'company id', where the first bit seems to have a special meaning if set.)
 */
typedef union PDMMAC
{
    /** 8-bit view. */
    uint8_t     au8[6];
    /** 16-bit view. */
    uint16_t    au16[3];
} PDMMAC;
/** Pointer to a MAC address. */
typedef PDMMAC *PPDMMAC;
/** Pointer to a const MAC address. */
typedef const PDMMAC *PCPDMMAC;


/** Pointer to a network port interface */
typedef struct PDMINETWORKPORT *PPDMINETWORKPORT;
/**
 * Network port interface.
 */
typedef struct PDMINETWORKPORT
{
    /**
     * Check how much data the device/driver can receive data now.
     * This must be called before the pfnRecieve() method is called.
     *
     * @returns Number of bytes the device can receive now.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(size_t, pfnCanReceive,(PPDMINETWORKPORT pInterface));

    /**
     * Receive data from the network.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           The available data.
     * @param   cb              Number of bytes available in the buffer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnReceive,(PPDMINETWORKPORT pInterface, const void *pvBuf, size_t cb));

} PDMINETWORKPORT;


/**
 * Network link state.
 */
typedef enum PDMNETWORKLINKSTATE
{
    /** Invalid state. */
    PDMNETWORKLINKSTATE_INVALID = 0,
    /** The link is up. */
    PDMNETWORKLINKSTATE_UP,
    /** The link is down. */
    PDMNETWORKLINKSTATE_DOWN,
    /** The link is temporarily down while resuming. */
    PDMNETWORKLINKSTATE_DOWN_RESUME
} PDMNETWORKLINKSTATE;


/** Pointer to a network connector interface */
typedef struct PDMINETWORKCONNECTOR *PPDMINETWORKCONNECTOR;
/**
 * Network connector interface.
 */
typedef struct PDMINETWORKCONNECTOR
{
    /**
     * Send data to the network.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Data to send.
     * @param   cb              Number of bytes to send.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnSend,(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb));

    /**
     * Set promiscuous mode.
     *
     * This is called when the promiscuous mode is set. This means that there doesn't have
     * to be a mode change when it's called.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fPromiscuous    Set if the adaptor is now in promiscuous mode. Clear if it is not.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnSetPromiscuousMode,(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous));

    /**
     * Notification on link status changes.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmLinkState    The new link state.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyLinkChanged,(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState));

    /**
     * More receive buffer has become available.
     *
     * This is called when the NIC frees up receive buffers.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(void, pfnNotifyCanReceive,(PPDMINETWORKCONNECTOR pInterface));

} PDMINETWORKCONNECTOR;


/** Pointer to a network config port interface */
typedef struct PDMINETWORKCONFIG *PPDMINETWORKCONFIG;
/**
 * Network config port interface.
 */
typedef struct PDMINETWORKCONFIG
{
    /**
     * Gets the current Media Access Control (MAC) address.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pMac            Where to store the MAC address.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnGetMac,(PPDMINETWORKCONFIG pInterface, PPDMMAC *pMac));

    /**
     * Gets the new link state.
     *
     * @returns The current link state.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(PDMNETWORKLINKSTATE, pfnGetLinkState,(PPDMINETWORKCONFIG pInterface));

    /**
     * Sets the new link state.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmState        The new link state
     * @thread  EMT
     */
    DECLR3CALLBACKMEMBER(int, pfnSetLinkState,(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState));

} PDMINETWORKCONFIG;


/** Pointer to a network connector interface */
typedef struct PDMIAUDIOCONNECTOR *PPDMIAUDIOCONNECTOR;
/**
 * Audio connector interface.
 */
typedef struct PDMIAUDIOCONNECTOR
{
    DECLR3CALLBACKMEMBER(void, pfnRun,(PPDMIAUDIOCONNECTOR pInterface));

/*    DECLR3CALLBACKMEMBER(int,  pfnSetRecordSource,(PPDMIAUDIOINCONNECTOR pInterface, AUDIORECSOURCE)); */

} PDMIAUDIOCONNECTOR;


/** @todo r=bird: the two following interfaces are hacks to work around the missing audio driver
 * interface. This should be addressed rather than making more temporary hacks. */

/** Pointer to a Audio Sniffer Device port interface. */
typedef struct PDMIAUDIOSNIFFERPORT *PPDMIAUDIOSNIFFERPORT;

/**
 * Audio Sniffer port interface.
 */
typedef struct PDMIAUDIOSNIFFERPORT
{
    /**
     * Enables or disables sniffing. If sniffing is being enabled also sets a flag
     * whether the audio must be also left on the host.
     *
     * @returns VBox status code
     * @param pInterface      Pointer to this interface.
     * @param fEnable         'true' for enable sniffing, 'false' to disable.
     * @param fKeepHostAudio  Indicates whether host audio should also present
     *                        'true' means that sound should not be played
     *                        by the audio device.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetup,(PPDMIAUDIOSNIFFERPORT pInterface, bool fEnable, bool fKeepHostAudio));

} PDMIAUDIOSNIFFERPORT;

/** Pointer to a Audio Sniffer connector interface. */
typedef struct PDMIAUDIOSNIFFERCONNECTOR *PPDMIAUDIOSNIFFERCONNECTOR;

/**
 * Audio Sniffer connector interface.
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

} PDMIAUDIOSNIFFERCONNECTOR;


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

#define PDMLED_MAGIC ( 0x11335577 )

/** Pointer to an LED ports interface. */
typedef struct PDMILEDPORTS      *PPDMILEDPORTS;
/**
 * Interface for exporting LEDs.
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


/** Pointer to an LED connectors interface. */
typedef struct PDMILEDCONNECTORS *PPDMILEDCONNECTORS;
/**
 * Interface for reading LEDs.
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


/** The special status unit number */
#define PDM_STATUS_LUN      999


#ifdef VBOX_HGCM

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
 * HGCM port interface. Normally implemented by VMMDev.
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


/** Pointer to a HGCM connector interface. */
typedef struct PDMIHGCMCONNECTOR *PPDMIHGCMCONNECTOR;

/** Pointer to a HGCM service location structure. */
typedef struct HGCMSERVICELOCATION *PHGCMSERVICELOCATION;

/**
 * HGCM connector interface.
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

#endif

/** @} */

__END_DECLS

#endif
