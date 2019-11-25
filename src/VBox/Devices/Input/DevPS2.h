/* $Id$ */
/** @file
 * PS/2 devices - Internal header file.
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Input_DevPS2_h
#define VBOX_INCLUDED_SRC_Input_DevPS2_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** @defgroup grp_devps2    PS/2 Device
 * @{
 */

/** Pointer to the shared keyboard (PS/2) controller / device state. */
typedef struct KBDSTATE *PKBDSTATE;

/** Define a simple PS/2 input device queue. */
#define DEF_PS2Q_TYPE(name, size)   \
     typedef struct {               \
        uint32_t    rpos;           \
        uint32_t    wpos;           \
        uint32_t    cUsed;          \
        uint32_t    cSize;          \
        uint8_t     abQueue[size];  \
     } name

DEF_PS2Q_TYPE(GeneriQ, 1);


/** @defgroup grp_devps2k   DevPS2K - Keyboard
 * @{
 */

/** @name HID modifier range.
 * @{ */
#define HID_MODIFIER_FIRST  0xE0
#define HID_MODIFIER_LAST   0xE8
/** @} */

/** @name USB HID additional constants
 * @{ */
/** The highest USB usage code reported by VirtualBox. */
#define VBOX_USB_MAX_USAGE_CODE     0xE7
/** The size of an array needed to store all USB usage codes */
#define VBOX_USB_USAGE_ARRAY_SIZE   (VBOX_USB_MAX_USAGE_CODE + 1)
/** USB HID Keyboard Usage Page. */
#define USB_HID_KB_PAGE             7
/** USB HID Consumer Control Usage Page. */
#define USB_HID_CC_PAGE             12
/** @} */

/* Internal keyboard queue sizes. The input queue doesn't need to be
 * extra huge and the command queue only needs to handle a few bytes.
 */
#define KBD_KEY_QUEUE_SIZE         64
#define KBD_CMD_QUEUE_SIZE          4

DEF_PS2Q_TYPE(KbdKeyQ, KBD_KEY_QUEUE_SIZE);
DEF_PS2Q_TYPE(KbdCmdQ, KBD_CMD_QUEUE_SIZE);

/** Typematic state. */
typedef enum {
    KBD_TMS_IDLE    = 0,    /* No typematic key active. */
    KBD_TMS_DELAY   = 1,    /* In the initial delay period. */
    KBD_TMS_REPEAT  = 2,    /* Key repeating at set rate. */
    KBD_TMS_32BIT_HACK = 0x7fffffff
} tmatic_state_t;


/**
 * The shared PS/2 keyboard instance data.
 */
typedef struct PS2K
{
    /** Pointer to parent device (keyboard controller). */
    R3PTRTYPE(PKBDSTATE) pParent;
    /** Set if keyboard is enabled ('scans' for input). */
    bool                fScanning;
    /** Set NumLock is on. */
    bool                fNumLockOn;
    /** Selected scan set. */
    uint8_t             u8ScanSet;
    /** Modifier key state. */
    uint8_t             u8Modifiers;
    /** Currently processed command (if any). */
    uint8_t             u8CurrCmd;
    /** Status indicator (LED) state. */
    uint8_t             u8LEDs;
    /** Selected typematic delay/rate. */
    uint8_t             u8TypematicCfg;
    /** Usage code of current typematic key, if any. */
    uint32_t            u32TypematicKey;
    /** Current typematic repeat state. */
    tmatic_state_t      enmTypematicState;
    /** Buffer holding scan codes to be sent to the host. */
    KbdKeyQ             keyQ;
    /** Command response queue (priority). */
    KbdCmdQ             cmdQ;
    /** Currently depressed keys. */
    uint8_t             abDepressedKeys[VBOX_USB_USAGE_ARRAY_SIZE];
    /** Typematic delay in milliseconds. */
    unsigned            uTypematicDelay;
    /** Typematic repeat period in milliseconds. */
    unsigned            uTypematicRepeat;
    /** Set if the throttle delay is currently active. */
    bool                fThrottleActive;
    /** Set if the input rate should be throttled. */
    bool                fThrottleEnabled;

    uint8_t             Alignment0[2];

    /** Command delay timer - RC Ptr. */
    PTMTIMERRC          pKbdDelayTimerRC;
    /** Typematic timer - RC Ptr. */
    PTMTIMERRC          pKbdTypematicTimerRC;
    /** Input throttle timer - RC Ptr. */
    PTMTIMERRC          pThrottleTimerRC;

    /** The device critical section protecting everything - R3 Ptr */
    R3PTRTYPE(PPDMCRITSECT) pCritSectR3;

    /** Command delay timer - R3 Ptr. */
    PTMTIMERR3          pKbdDelayTimerR3;
    /** Typematic timer - R3 Ptr. */
    PTMTIMERR3          pKbdTypematicTimerR3;
    /** Input throttle timer - R3 Ptr. */
    PTMTIMERR3          pThrottleTimerR3;

    /** Command delay timer - R0 Ptr. */
    PTMTIMERR0          pKbdDelayTimerR0;
    /** Typematic timer - R0 Ptr. */
    PTMTIMERR0          pKbdTypematicTimerR0;
    /** Input throttle timer - R0 Ptr. */
    PTMTIMERR0          pThrottleTimerR0;

    /**
     * Keyboard port - LUN#0.
     *
     * @implements  PDMIBASE
     * @implements  PDMIKEYBOARDPORT
     */
    struct
    {
        /** The base interface for the keyboard port. */
        PDMIBASE                            IBase;
        /** The keyboard port base interface. */
        PDMIKEYBOARDPORT                    IPort;

        /** The base interface of the attached keyboard driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The keyboard interface of the attached keyboard driver. */
        R3PTRTYPE(PPDMIKEYBOARDCONNECTOR)   pDrv;
    } Keyboard;
} PS2K;
/** Pointer to the PS/2 keyboard instance data. */
typedef PS2K *PPS2K;


int  PS2KByteToKbd(PPS2K pThis, uint8_t cmd);
int  PS2KByteFromKbd(PPS2K pThis, uint8_t *pVal);

int  PS2KConstruct(PPS2K pThis, PPDMDEVINS pDevIns, PKBDSTATE pParent, unsigned iInstance, PCFGMNODE pCfg);
int  PS2KAttach(PPS2K pThis, PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags);
void PS2KReset(PPS2K pThis);
void PS2KRelocate(PPS2K pThis, RTGCINTPTR offDelta, PPDMDEVINS pDevIns);
void PS2KSaveState(PPS2K pThis, PSSMHANDLE pSSM);
int  PS2KLoadState(PPS2K pThis, PSSMHANDLE pSSM, uint32_t uVersion);
int  PS2KLoadDone(PPS2K pThis, PSSMHANDLE pSSM);

PS2K *KBDGetPS2KFromDevIns(PPDMDEVINS pDevIns);

/** @} */


/** @defgroup grp_devps2m DevPS2M - Auxiliary Device (Mouse)
 * @{
 */

/* Internal mouse queue sizes. The input queue is relatively large,
 * but the command queue only needs to handle a few bytes.
 */
#define AUX_EVT_QUEUE_SIZE        256
#define AUX_CMD_QUEUE_SIZE          8

DEF_PS2Q_TYPE(AuxEvtQ, AUX_EVT_QUEUE_SIZE);
DEF_PS2Q_TYPE(AuxCmdQ, AUX_CMD_QUEUE_SIZE);

/** Auxiliary device special modes of operation. */
typedef enum {
    AUX_MODE_STD,           /* Standard operation. */
    AUX_MODE_RESET,         /* Currently in reset. */
    AUX_MODE_WRAP           /* Wrap mode (echoing input). */
} PS2M_MODE;

/** Auxiliary device operational state. */
typedef enum {
    AUX_STATE_RATE_ERR  = RT_BIT(0),    /* Invalid rate received. */
    AUX_STATE_RES_ERR   = RT_BIT(1),    /* Invalid resolution received. */
    AUX_STATE_SCALING   = RT_BIT(4),    /* 2:1 scaling in effect. */
    AUX_STATE_ENABLED   = RT_BIT(5),    /* Reporting enabled in stream mode. */
    AUX_STATE_REMOTE    = RT_BIT(6)     /* Remote mode (reports on request). */
} PS2M_STATE;

/** Externally visible state bits. */
#define AUX_STATE_EXTERNAL  (AUX_STATE_SCALING | AUX_STATE_ENABLED | AUX_STATE_REMOTE)

/** Protocols supported by the PS/2 mouse. */
typedef enum {
    PS2M_PROTO_PS2STD      = 0,  /* Standard PS/2 mouse protocol. */
    PS2M_PROTO_IMPS2       = 3,  /* IntelliMouse PS/2 protocol. */
    PS2M_PROTO_IMEX        = 4,  /* IntelliMouse Explorer protocol. */
    PS2M_PROTO_IMEX_HORZ   = 5   /* IntelliMouse Explorer with horizontal reports. */
} PS2M_PROTO;

/** Protocol selection 'knock' states. */
typedef enum {
    PS2M_KNOCK_INITIAL,
    PS2M_KNOCK_1ST,
    PS2M_KNOCK_IMPS2_2ND,
    PS2M_KNOCK_IMEX_2ND,
    PS2M_KNOCK_IMEX_HORZ_2ND
} PS2M_KNOCK_STATE;

/**
 * The PS/2 auxiliary device instance data.
 */
typedef struct PS2M
{
    /** Pointer to parent device (keyboard controller). */
    R3PTRTYPE(PKBDSTATE) pParent;
    /** Operational state. */
    uint8_t             u8State;
    /** Configured sampling rate. */
    uint8_t             u8SampleRate;
    /** Configured resolution. */
    uint8_t             u8Resolution;
    /** Currently processed command (if any). */
    uint8_t             u8CurrCmd;
    /** Set if the throttle delay is active. */
    bool                fThrottleActive;
    /** Set if the throttle delay is active. */
    bool                fDelayReset;
    /** Operational mode. */
    PS2M_MODE           enmMode;
    /** Currently used protocol. */
    PS2M_PROTO          enmProtocol;
    /** Currently used protocol. */
    PS2M_KNOCK_STATE    enmKnockState;
    /** Buffer holding mouse events to be sent to the host. */
    AuxEvtQ             evtQ;
    /** Command response queue (priority). */
    AuxCmdQ             cmdQ;
    /** Accumulated horizontal movement. */
    int32_t             iAccumX;
    /** Accumulated vertical movement. */
    int32_t             iAccumY;
    /** Accumulated Z axis (vertical scroll) movement. */
    int32_t             iAccumZ;
    /** Accumulated W axis (horizontal scroll) movement. */
    int32_t             iAccumW;
    /** Accumulated button presses. */
    uint32_t            fAccumB;
    /** Instantaneous button data. */
    uint32_t            fCurrB;
    /** Button state last sent to the guest. */
    uint32_t            fReportedB;
    /** Throttling delay in milliseconds. */
    uint32_t            uThrottleDelay;

    /** The device critical section protecting everything - R3 Ptr */
    R3PTRTYPE(PPDMCRITSECT) pCritSectR3;
    /** Command delay timer - R3 Ptr. */
    PTMTIMERR3          pDelayTimerR3;
    /** Interrupt throttling timer - R3 Ptr. */
    PTMTIMERR3          pThrottleTimerR3;
    RTR3PTR             Alignment1;

    /** Command delay timer - RC Ptr. */
    PTMTIMERRC          pDelayTimerRC;
    /** Interrupt throttling timer - RC Ptr. */
    PTMTIMERRC          pThrottleTimerRC;

    /** Command delay timer - R0 Ptr. */
    PTMTIMERR0          pDelayTimerR0;
    /** Interrupt throttling timer - R0 Ptr. */
    PTMTIMERR0          pThrottleTimerR0;

    /**
     * Mouse port - LUN#1.
     *
     * @implements  PDMIBASE
     * @implements  PDMIMOUSEPORT
     */
    struct
    {
        /** The base interface for the mouse port. */
        PDMIBASE                            IBase;
        /** The keyboard port base interface. */
        PDMIMOUSEPORT                       IPort;

        /** The base interface of the attached mouse driver. */
        R3PTRTYPE(PPDMIBASE)                pDrvBase;
        /** The keyboard interface of the attached mouse driver. */
        R3PTRTYPE(PPDMIMOUSECONNECTOR)      pDrv;
    } Mouse;
} PS2M;
/** Pointer to the PS/2 auxiliary device instance data. */
typedef PS2M *PPS2M;

int  PS2MByteToAux(PPS2M pThis, uint8_t cmd);
int  PS2MByteFromAux(PPS2M pThis, uint8_t *pVal);

int  PS2MConstruct(PPS2M pThis, PPDMDEVINS pDevIns, PKBDSTATE pParent, unsigned iInstance);
int  PS2MAttach(PPS2M pThis, PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags);
void PS2MReset(PPS2M pThis);
void PS2MRelocate(PPS2M pThis, RTGCINTPTR offDelta, PPDMDEVINS pDevIns);
void PS2MSaveState(PPS2M pThis, PSSMHANDLE pSSM);
int  PS2MLoadState(PPS2M pThis, PSSMHANDLE pSSM, uint32_t uVersion);
void PS2MFixupState(PPS2M pThis, uint8_t u8State, uint8_t u8Rate, uint8_t u8Proto);

PS2M *KBDGetPS2MFromDevIns(PPDMDEVINS pDevIns);

/** @} */


/**
 * The shared keyboard controller/device state.
 *
 * @note We use the default critical section for serialize data access.
 */
typedef struct KBDSTATE
{
    uint8_t write_cmd; /* if non zero, write data to port 60 is expected */
    uint8_t status;
    uint8_t mode;
    uint8_t dbbout;    /* data buffer byte */
    /* keyboard state */
    int32_t translate;
    int32_t xlat_state;

    /** Pointer to the device instance - RC. */
    PPDMDEVINSRC                pDevInsRC;
    /** Pointer to the device instance - R3 . */
    PPDMDEVINSR3                pDevInsR3;
    /** Pointer to the device instance. */
    PPDMDEVINSR0                pDevInsR0;

    /** Keyboard state (implemented in separate PS2K module). */
    PS2K                        Kbd;

    /** Mouse state (implemented in separate PS2M module). */
    PS2M                        Aux;
} KBDSTATE, KBDState;


/* Shared keyboard/aux internal interface. */
void KBCUpdateInterrupts(void *pKbc);


///@todo: This should live with the KBC implementation.
/** AT to PC scancode translator state.  */
typedef enum
{
    XS_IDLE,    /**< Starting state. */
    XS_BREAK,   /**< F0 break byte was received. */
    XS_HIBIT    /**< Break code still active. */
} xlat_state_t;

int32_t XlateAT2PC(int32_t state, uint8_t scanIn, uint8_t *pScanOut);

/** @}  */

#endif /* !VBOX_INCLUDED_SRC_Input_DevPS2_h */

