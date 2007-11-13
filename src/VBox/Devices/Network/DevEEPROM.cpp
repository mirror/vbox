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

#include <string.h>
#include "DevEEPROM.h"

/**
 * Construct EEPROM device.
 *
 * @param   initial     Initial EEPROM content (optional). The size of initial
 *                      content must be sizeof(uint16_t)*EEPROM93C46::SIZE
 *                      bytes.
 */
EEPROM93C46::EEPROM93C46(const uint16_t * initial)
{
    if ( initial ) {
        memcpy(this->data, initial, sizeof(this->data));
    } else {
        memset(this->data, 0, sizeof(this->data));
    }
    writeEnabled = false;
    internalWires = 0;
    state = STANDBY;
}

/**
 * Writes one word to specified location if write is enabled.
 *
 * @param   addr        Address to write at
 * @param   value       Value to store
 */
void EEPROM93C46::storeWord(uint32_t addr, uint16_t value)
{
    if (writeEnabled) {
        data[addr] = value;
    }
    mask = DATA_MSB;
}

/**
 * Fetch next word pointer by 'addr'.
 *
 * 'addr' is advanced and mask is reset to support sequential reads.
 *
 * @returns New state
 */
EEPROM93C46::State EEPROM93C46::opRead()
{
    word = data[addr++];
    mask = DATA_MSB;
    return WRITING_DO;
}

/**
 * Write the value of 'word' member to the location specified by 'addr' member.
 *
 * @returns New state
 *
 * @remarks Need to wait for CS lower/raise to show busy/ready indication.
 */
EEPROM93C46::State EEPROM93C46::opWrite()
{
    storeWord(addr, word);
    return WAITING_CS_FALL;
}

/**
 * Overwrite the entire contents of EEPROM with the value of 'word' member.
 *
 * @returns New state
 *
 * @remarks Need to wait for CS lower/raise to show busy/ready indication.
 */
EEPROM93C46::State EEPROM93C46::opWriteAll()
{
    for (int i=0; i<SIZE; i++)
        storeWord(i, word);
    return WAITING_CS_FALL;
}

/**
 * Decode opcode and address from 'opAddr' member.
 *
 * Decodes operation and executes it immediately if possible;
 * otherwise, stores the decoded operation and address.
 *
 * @returns New state
 */
EEPROM93C46::State EEPROM93C46::opDecode()
{
    switch (word>>6) {
    case 3: // ERASE
        storeWord(word & ADDR_MASK, 0xFFFF);
        return WAITING_CS_FALL;
    case 2: // READ
        op   = &EEPROM93C46::opRead;
        addr = word & ADDR_MASK;
        return opRead(); // Load first word
    case 1: // WRITE
        op   = &EEPROM93C46::opWrite;
        addr = word & ADDR_MASK;
        word = 0;
        mask = DATA_MSB;
        return READING_DI;
    case 0:
        switch (word>>4) {
        case 0: // ERASE/WRITE DISABLE
            writeEnabled = false;
            return STANDBY;
        case 1: // WRITE ALL
            op   = &EEPROM93C46::opWriteAll;
            word = 0;
            mask = DATA_MSB;
            return READING_DI;
        case 2: // ERASE ALL
            // Re-use opWriteAll
            word = 0xFFFF;
            return opWriteAll();
        case 3: // ERASE/WRITE ENABLE
            writeEnabled = true;
            return STANDBY;
        }
    }
    return state;
}

/**
 * Set bits in EEPROM 4-wire interface.
 *
 * @param   wires       Values of DI, CS, SK.
 * @remarks The value of DO bit in 'wires' is ignored.
 */
void EEPROM93C46::write(uint32_t wires)
{
    if (wires & CS) {
        if (!(internalWires & SK) && (wires & SK)) {
            // Positive edge of clock
            if (state == STANDBY) {
                if (wires & DI) {
                    state = READING_DI;
                    op    = &EEPROM93C46::opDecode;
                    mask  = OPADDR_MSB;
                    word  = 0;
                }
            }
            else {
                if (state == READING_DI) {
                    if (wires & DI) {
                        word |= mask;
                    }
                }
                else if (state == WRITING_DO) {
                    internalWires &= ~DO; 
                    if (word & mask) {
                        internalWires |= DO;
                    }
                }
                else return;
                // Next bit
                mask >>= 1;
                if (mask == 0)
                    state = (this->*op)();
            }
        }
        else if (state == WAITING_CS_RISE) {
            internalWires |= DO;    // ready
            state = STANDBY;
        }
    }
    else {
        switch(state) {
        case WAITING_CS_FALL: state = WAITING_CS_RISE; internalWires &= ~DO; break; // busy
        case WAITING_CS_RISE: break;
        case READING_DI:      internalWires &= ~DO; // Clear ready/busy status from DO. Fall through!
        default:              state = STANDBY; break;
        }
    }
    internalWires &= DO;
    internalWires |= wires & ~DO;    // Do not overwrite DO
}

/**
 * Read bits in EEPROM 4-wire interface.
 *
 * @returns Current values of DO, DI, CS, SK.
 *
 * @remarks Only DO is controlled by EEPROM, other bits are returned as they
 * were written by 'write'.
 */
uint32_t EEPROM93C46::read()
{
    return internalWires;
}
