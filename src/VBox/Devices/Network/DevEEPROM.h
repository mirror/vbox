/** @file
 * Microware-compatible 64x16-bit 93C46 EEPROM Emulation.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Interface */
#include <iprt/types.h>

/**
 * 93C46-compatible EEPROM device emulation.
 */
class EEPROM93C46 {
public:
    EEPROM93C46(const uint16_t * initial = 0);

    /** General definitions */
    enum {
        /** Size of EEPROM in words */
        SIZE        = 64,
        /** Number of bits per word */
        WORD_SIZE   = 16,
        /** Number of address bits */
        ADDR_SIZE   = 6,
        /** Number of bits in opcode */
        OPCODE_SIZE = 2,
        /** The most significant bit mask in data word */
        DATA_MSB    = 1<<(WORD_SIZE-1),
        /** Address mask */
        ADDR_MASK   = (1<<ADDR_SIZE)-1,
        /** The most significant bit mask in op+addr bit sequence */
        OPADDR_MSB  = 1<<(OPCODE_SIZE+ADDR_SIZE-1)
    };
    /**
     * Names of signal wires
     */
    enum Wires {
        SK=0x1,    ///< Clock
        CS=0x2,    ///< Chip Select
        DI=0x4,    ///< Data In
        DO=0x8     ///< Data Out
    };


    uint32_t read();
    void     write(uint32_t wires);

    /* @todo save and load methods */

protected:
    /** Actual content of EEPROM */
    uint16_t data[SIZE];

private:
    /** current state.
     *
     * EEPROM operates as a simple state machine. Events are primarily
     * triggered at positive edge of clock signal (SK). Refer to the
     * timing diagrams of 93C46 to get better understanding.
     */
    enum State {
        /** Initial state. Waiting for start condition (CS, SK, DI high). */
        STANDBY,
        /** Reading data in, shifting in the bits into 'word'. */
        READING_DI,
        /** Writing data out, shifting out the bits from 'word'. */
        WRITING_DO,
        /** Waiting for CS=0 to indicate we are busy (DO=0). */
        WAITING_CS_FALL,
        /** Waiting for CS=1 to indicate we are ready (DO=1). */
        WAITING_CS_RISE
    } state;
    /** setting writeEnable to false prevents write and erase operations */
    bool writeEnabled;
    /** intermediate storage */
    uint16_t word;
    /** currently processed bit in 'word' */
    uint16_t mask;
    /** decoded address */
    uint16_t addr;
    /** Data Out, Data In, Chip Select, Clock */
    uint32_t internalWires;

    /** Pointer to decoded operation. When no operation has been decoded yet
     *  it points to opDecode.
     */
    State (EEPROM93C46::*op)(void);

    // Operation handlers
    State opDecode();
    State opRead();
    State opWrite();
    State opWriteAll();

    /** Helper method to implement write protection */
    void storeWord(uint32_t addr, uint16_t value);
};
