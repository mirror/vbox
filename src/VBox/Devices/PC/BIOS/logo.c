/* $Id$ */
/** @file
 * Stuff for drawing the BIOS logo.
 */

/*
 * Copyright (C) 2004-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#define WAIT_HZ              64
#define WAIT_MS              16

#define F12_SCAN_CODE        0x86
#define F12_WAIT_TIME        (3 * WAIT_HZ)   /* 3 seconds. Used only if logo disabled. */

#define uint8_t    Bit8u
#define uint16_t   Bit16u
#define uint32_t   Bit32u
#include <VBox/bioslogo.h>

//static void        set_mode(mode);
//static void        vesa_set_mode(mode);
//static Bit8u       wait(ticks, stop_on_key);

/**
 * Set video mode (VGA).
 * @params    New video mode.
 */
void set_mode(mode)
  Bit8u mode;
  {
  ASM_START
    push bp
    mov  bp, sp

      push ax

      mov  ah, #0
      mov  al, 4[bp] ; mode
      int  #0x10

      pop  ax

    pop  bp
  ASM_END
  }

/**
 * Set VESA video mode.
 * @params    New video mode.
 */
Bit16u vesa_set_mode(mode)
  Bit16u mode;
  {
  ASM_START
    push bp
    mov  bp, sp

      push bx

      mov  ax, #0x4f02
      mov  bx, 4[bp] ; mode
      int  #0x10

      pop  bx

    pop  bp
  ASM_END
}

/**
 * Check for keystroke.
 * @returns    True if keystroke available, False if not.
 */
Bit8u check_for_keystroke()
  {
  ASM_START
    mov  ax, #0x100
    int  #0x16
    jz   no_key
    mov  al, #1
    jmp  done
no_key:
    xor  al, al
done:
  ASM_END
}

/**
 * Get keystroke.
 * @returns    BIOS scan code.
 */
Bit8u get_keystroke()
  {
  ASM_START
    mov  ax, #0x0
    int  #0x16
    xchg ah, al
  ASM_END
}

void wait_init()
{
    // The default is 18.2 ticks per second (~55ms tick interval).
    // Set the timer to 16ms ticks (64K / (Hz / (PIT_HZ / 64K)) = count).
    // 0x10000 / (1000 / (1193182 / 0x10000)) = 1193 (0x04a9)
    // 0x10000 / ( 128 / (1193182 / 0x10000)) = 9321 (0x2469)
    // 0x10000 / (  64 / (1193182 / 0x10000)) = 18643 (0x48d3)
ASM_START
    mov al, #0x34 ; timer0: binary count, 16bit count, mode 2
    out 0x43, al
    mov al, #0xd3 ; Low byte - 64Hz
    out 0x40, al
    mov al, #0x48 ; High byte - 64Hz
    out 0x40, al
ASM_END
}

void wait_uninit()
{
ASM_START
    pushf
    cli

    /* Restore the timer to the default 18.2Hz. */
    mov al, #0x34 ; timer0: binary count, 16bit count, mode 2
    out 0x43, al
    xor ax, ax    ; maximum count of 0000H = 18.2Hz
    out 0x40, al
    out 0x40, al

    /*
     * Reinitialize the tick and rollover counts since we've
     * screwed them up by running the timer at WAIT_HZ for a while.
     */
    pushad
    push ds
    mov  ds, ax   ; already 0
    call timer_tick_post
    pop  ds
    popad

    popf
ASM_END
}

/**
 * Waits (sleeps) for the given number of ticks.
 * Checks for keystroke.
 *
 * @returns BIOS scan code if available, 0 if not.
 * @param   ticks       Number of ticks to sleep.
 * @param   stop_on_key Whether to stop immediately upon keypress.
 */
Bit8u wait(ticks, stop_on_key)
  Bit16u ticks;
  Bit8u stop_on_key;
{
    long ticks_to_wait, delta;
    Bit32u prev_ticks, t;
    Bit8u scan_code = 0;

    /*
     * The 0:046c wraps around at 'midnight' according to a 18.2Hz clock.
     * We also have to be careful about interrupt storms.
     */
ASM_START
    pushf
    sti
ASM_END
    ticks_to_wait = ticks;
    prev_ticks = read_dword(0x0, 0x46c);
    do
    {
ASM_START
        hlt
ASM_END
        t = read_dword(0x0, 0x46c);
        if (t > prev_ticks)
        {
            delta = t - prev_ticks;     /* The temp var is required or bcc screws up. */
            ticks_to_wait -= delta;
        }
        else if (t < prev_ticks)
            ticks_to_wait -= t;         /* wrapped */
        prev_ticks = t;

        if (check_for_keystroke())
        {
            scan_code = get_keystroke();
            bios_printf(BIOS_PRINTF_INFO, "Key pressed: %x\n", scan_code);
            if (stop_on_key)
                return scan_code;
        }
    } while (ticks_to_wait > 0);
ASM_START
    popf
ASM_END
    return scan_code;
}

Bit8u read_logo_byte(offset)
  Bit8u offset;
{
    outw(LOGO_IO_PORT, LOGO_CMD_SET_OFFSET | offset);
    return inb(LOGO_IO_PORT);
}

Bit16u read_logo_word(offset)
  Bit8u offset;
{
    outw(LOGO_IO_PORT, LOGO_CMD_SET_OFFSET | offset);
    return inw(LOGO_IO_PORT);
}

void clear_screen()
{
// Hide cursor, clear screen and move cursor to starting position
ASM_START
    push bx
    push cx
    push dx

    mov  ax, #0x100
    mov  cx, #0x1000
    int  #0x10

    mov  ax, #0x700
    mov  bh, #7
    xor  cx, cx
    mov  dx, #0x184f
    int  #0x10

    mov  ax, #0x200
    xor  bx, bx
    xor  dx, dx
    int  #0x10

    pop  dx
    pop  cx
    pop  bx
ASM_END
}

void print_detected_harddisks()
{
    Bit16u ebda_seg=read_word(0x0040,0x000E);
    Bit8u hd_count;
    Bit8u hd_curr = 0;
    Bit8u ide_ctrl_printed = 0;
    Bit8u sata_ctrl_printed = 0;
    Bit8u scsi_ctrl_printed = 0;
    Bit8u device;

    hd_count = read_byte(ebda_seg, &EbdaData->ata.hdcount);

    for (hd_curr = 0; hd_curr < hd_count; hd_curr++)
    {
        device = read_byte(ebda_seg, &EbdaData->ata.hdidmap[hd_curr]);

        if (!VBOX_IS_SCSI_DEVICE(device))
        {

            if ((device < 4) && (ide_ctrl_printed == 0))
            {
                printf("IDE controller:\n");
                ide_ctrl_printed = 1;
            }
            else if ((device >= 4) && (sata_ctrl_printed == 0))
            {
                printf("\n\nAHCI controller:\n");
                sata_ctrl_printed = 1;
            }

            printf("\n    %d) ", hd_curr+1);

            /*
             * If actual_device is bigger than or equal 4
             * this is the next controller and
             * the positions start at the beginning.
             */
            if (device >= 4)
                device -= 4;

            if (device / 2)
                printf("Secondary ");
            else
                printf("Primary ");

            if (device % 2)
                printf("Slave");
            else
                printf("Master");
        }
        else
        {
            if (scsi_ctrl_printed == 0)
            {
                printf("\n\nSCSI controller:\n");
                scsi_ctrl_printed = 1;
            }

            printf("\n    %d) Hard disk", hd_curr+1);

        }
    }

    if (   (ide_ctrl_printed == 0)
        && (sata_ctrl_printed == 0)
        && (scsi_ctrl_printed == 0))
        printf("No hard disks found");

    printf("\n");
}

Bit8u get_boot_drive(scode)
   Bit8u scode;
{
    Bit16u ebda_seg=read_word(0x0040,0x000E);

    /* Check that the scan code is in the range of detected hard disks. */
    Bit8u hd_count = read_byte(ebda_seg, &EbdaData->ata.hdcount);

    /* The key '1' has scancode 0x02 which represents the first disk */
    scode -= 2;

    if (scode < hd_count)
        return scode;

    /* Scancode is higher than number of available devices */
    return 0xff;
}

void show_logo()
{
    Bit16u      ebda_seg = read_word(0x0040,0x000E);
    Bit8u       f12_pressed = 0;
    Bit8u       scode;
    Bit16u      tmp, i;

    LOGOHDR    *logo_hdr = 0;
    Bit8u       is_fade_in, is_fade_out, uBootMenu;
    Bit16u      logo_time;


    // Set PIT to 1ms ticks
    wait_init();


    // Get main signature
    tmp = read_logo_word(&logo_hdr->u16Signature);
    if (tmp != 0x66BB)
        goto done;

    // Get options
    is_fade_in = read_logo_byte(&logo_hdr->fu8FadeIn);
    is_fade_out = read_logo_byte(&logo_hdr->fu8FadeOut);
    logo_time = read_logo_word(&logo_hdr->u16LogoMillies);
    uBootMenu = read_logo_byte(&logo_hdr->fu8ShowBootMenu);

    // Is Logo disabled?
    if (!is_fade_in && !is_fade_out && !logo_time)
        goto done;

    // Set video mode #0x142 640x480x32bpp
    vesa_set_mode(0x142);

    if (is_fade_in)
    {
        for (i = 0; i <= LOGO_SHOW_STEPS; i++)
        {
            outw(LOGO_IO_PORT, LOGO_CMD_SHOW_BMP | i);
            scode = wait(16 / WAIT_MS, 0);
            if (scode == F12_SCAN_CODE)
            {
                f12_pressed = 1;
                break;
            }
        }
    }
    else
        outw(LOGO_IO_PORT, LOGO_CMD_SHOW_BMP | LOGO_SHOW_STEPS);

    // Wait (interval in milliseconds)
    if (!f12_pressed)
    {
        scode = wait(logo_time / WAIT_MS, 1);
        if (scode == F12_SCAN_CODE)
            f12_pressed = 1;
    }

    // Fade out (only if F12 was not pressed)
    if (is_fade_out && !f12_pressed)
    {
        for (i = LOGO_SHOW_STEPS; i > 0 ; i--)
        {
            outw(LOGO_IO_PORT, LOGO_CMD_SHOW_BMP | i);
            scode = wait(16 / WAIT_MS, 0);
            if (scode == F12_SCAN_CODE)
            {
                f12_pressed = 1;
                break;
            }
        }
    }

done:
    // Clear forced boot drive setting.
    write_byte(ebda_seg, &EbdaData->uForceBootDevice, 0);

    // Don't restore previous video mode
    // The default text mode should be set up. (defect #1235)
    set_mode(0x0003);

    // If Setup menu enabled
    if (uBootMenu)
    {
        // If the graphics logo disabled
        if (!is_fade_in && !is_fade_out && !logo_time)
        {
            int i;

            if (uBootMenu == 2)
                printf("Press F12 to select boot device.\n");

            // if the user has pressed F12 don't wait here
            if (!f12_pressed)
            {
                // Wait for timeout or keystroke
                scode = wait(F12_WAIT_TIME, 1);
                if (scode == F12_SCAN_CODE)
                    f12_pressed = 1;
            }
        }

        // If F12 pressed, show boot menu
        if (f12_pressed)
        {
            Bit8u boot_device = 0;
            Bit8u boot_drive = 0;

            clear_screen();

            // Show menu. Note that some versions of bcc freak out if we split these strings.
            printf("\nVirtualBox temporary boot device selection\n\nDetected Hard disks:\n\n");
            print_detected_harddisks();
            printf("\nOther boot devices:\n f) Floppy\n c) CD-ROM\n l) LAN\n\n b) Continue booting\n");



            // Wait for keystroke
            for (;;)
            {
                do
                {
                    scode = wait(WAIT_HZ, 1);
                } while (scode == 0);

                if (scode == 0x30)
                {
                    // 'b' ... continue
                    break;
                }

                // Check if hard disk was selected
                if ((scode >= 0x02) && (scode <= 0x09))
                {
                    boot_drive = get_boot_drive(scode);

                    /*
                     * 0xff indicates that there is no mapping
                     * from the scan code to a hard drive.
                     * Wait for next keystroke.
                     */
                    if (boot_drive == 0xff)
                        continue;

                    write_byte(ebda_seg, &EbdaData->uForceBootDrive, boot_drive);
                    boot_device = 0x02;
                    break;
                }

                switch (scode)
                {
                    case 0x21:
                        // Floppy
                        boot_device = 0x01;
                        break;
                    case 0x2e:
                        // CD-ROM
                        boot_device = 0x03;
                        break;
                    case 0x26:
                        // LAN
                        boot_device = 0x04;
                        break;
                }

                if (boot_device != 0)
                    break;
            }

            write_byte(ebda_seg, &EbdaData->uForceBootDevice, boot_device);

            // Switch to text mode. Clears screen and enables cursor again.
            set_mode(0x0003);
        }
    }

    // Restore PIT ticks
    wait_uninit();

    return;
}


void delay_boot(secs)
  Bit16u secs;
{
    Bit16u i;

    if (!secs)
        return;

    // Set PIT to 1ms ticks
    wait_init();

    printf("Delaying boot for %d seconds:", secs);
    for (i = secs; i > 0; i--)
    {
        printf(" %d", i);
        wait(WAIT_HZ, 0);
    }
    printf("\n");
    // Restore PIT ticks
    wait_uninit();
}
