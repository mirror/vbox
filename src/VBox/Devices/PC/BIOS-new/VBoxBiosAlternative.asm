
section _DATA progbits vstart=0x0 align=1 ; size=0x18 class=DATA group=DGROUP
_dskacc:                                     ; 0xf0000 LB 0x18
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 02bh, 028h, 0a1h, 028h, 000h, 000h, 000h, 000h
    db  044h, 077h, 0fch, 077h, 0e3h, 082h, 079h, 083h

section CONST progbits vstart=0x18 align=1 ; size=0xbde class=DATA group=DGROUP
    db   'NMI Handler called', 00ah, 000h
    db   'INT18: BOOT FAILURE', 00ah, 000h
    db   '%s', 00ah, 000h, 000h
    db   'FATAL: ', 000h
    db   'bios_printf: unknown format', 00ah, 000h, 000h
    db   'ata-detect: Failed to detect ATA device', 00ah, 000h
    db   'ata%d-%d: PCHS=%u/%d/%d LCHS=%u/%u/%u', 00ah, 000h
    db   'ata-detect: Failed to detect ATAPI device', 00ah, 000h
    db   ' slave', 000h
    db   'master', 000h
    db   'ata%d %s: ', 000h
    db   '%c', 000h
    db   ' ATA-%d Hard-Disk (%lu MBytes)', 00ah, 000h
    db   ' ATAPI-%d CD-ROM/DVD-ROM', 00ah, 000h
    db   ' ATAPI-%d Device', 00ah, 000h
    db   'ata%d %s: Unknown device', 00ah, 000h
    db   'ata_cmd_packet', 000h
    db   '%s: DATA_OUT not supported yet', 00ah, 000h
    db   'set_diskette_current_cyl: drive > 1', 00ah, 000h
    db   'int13_diskette_function', 000h
    db   '%s: drive>1 || head>1 ...', 00ah, 000h
    db   '%s: ctrl not ready', 00ah, 000h
    db   '%s: read error', 00ah, 000h
    db   '%s: write error', 00ah, 000h
    db   '%s: bad floppy type', 00ah, 000h
    db   '%s: unsupported AH=%02x', 00ah, 000h, 000h
    db   'int13_eltorito', 000h
    db   '%s: call with AX=%04x. Please report', 00ah, 000h
    db   '%s: unsupported AH=%02x', 00ah, 000h
    db   'int13_cdemu', 000h
    db   '%s: function %02x, emulation not active for DL= %02x', 00ah, 000h
    db   '%s: function %02x, error %02x !', 00ah, 000h
    db   '%s: function AH=%02x unsupported, returns fail', 00ah, 000h
    db   'int13_cdrom', 000h
    db   '%s: function %02x, ELDL out of range %02x', 00ah, 000h
    db   '%s: function %02x, unmapped device for ELDL=%02x', 00ah, 000h
    db   '%s: function %02x. Can', 027h, 't use 64bits lba', 00ah, 000h
    db   '%s: function %02x, status %02x !', 00ah, 000h, 000h
    db   'Booting from %s...', 00ah, 000h
    db   'Boot from %s failed', 00ah, 000h
    db   'Boot from %s %d failed', 00ah, 000h
    db   'No bootable medium found! System halted.', 00ah, 000h
    db   'Could not read from the boot medium! System halted.', 00ah, 000h
    db   'CDROM boot failure code : %04x', 00ah, 000h
    db   'Boot : bseqnr=%d, bootseq=%x', 00dh, 00ah, 000h, 000h
    db   'Keyboard error:%u', 00ah, 000h
    db   'KBD: int09 handler: AL=0', 00ah, 000h
    db   'KBD: int09h_handler(): unknown scancode read: 0x%02x!', 00ah, 000h
    db   'KBD: int09h_handler(): scancode & asciicode are zero?', 00ah, 000h
    db   'KBD: int16h: out of keyboard input', 00ah, 000h
    db   'KBD: unsupported int 16h function %02x', 00ah, 000h
    db   'AX=%04x BX=%04x CX=%04x DX=%04x ', 00ah, 000h, 000h
    db   'int13_harddisk', 000h
    db   '%s: function %02x, ELDL out of range %02x', 00ah, 000h
    db   '%s: function %02x, unmapped device for ELDL=%02x', 00ah, 000h
    db   '%s: function %02x, count out of range!', 00ah, 000h
    db   '%s: function %02x, disk %02x, parameters out of range %04x/%04x/%04x!', 00ah
    db   000h
    db   '%s: function %02x, error %02x !', 00ah, 000h
    db   'format disk track called', 00ah, 000h
    db   '%s: function %02xh unimplemented, returns success', 00ah, 000h
    db   '%s: function %02xh unsupported, returns fail', 00ah, 000h
    db   'int13_harddisk_ext', 000h
    db   '%s: function %02x. Can', 027h, 't use 64bits lba', 00ah, 000h
    db   '%s: function %02x. LBA out of range', 00ah, 000h
    db   'int15: Func 24h, subfunc %02xh, A20 gate control not supported', 00ah, 000h
    db   '*** int 15h function AH=bf not yet supported!', 00ah, 000h
    db   'EISA BIOS not present', 00ah, 000h
    db   '*** int 15h function AX=%04x, BX=%04x not yet supported!', 00ah, 000h
    db   'sendmouse', 000h
    db   'setkbdcomm', 000h
    db   'Mouse reset returned %02x (should be ack)', 00ah, 000h
    db   'Mouse status returned %02x (should be ack)', 00ah, 000h
    db   'INT 15h C2 AL=6, BH=%02x', 00ah, 000h
    db   'INT 15h C2 default case entered', 00ah, 000h, 000h
    db   'Key pressed: %x', 00ah, 000h
    db   00ah, 00ah, 'AHCI controller:', 00ah, 000h
    db   00ah, '    %d) Hard disk', 000h
    db   00ah, 00ah, 'SCSI controller:', 00ah, 000h
    db   'IDE controller:', 00ah, 000h
    db   00ah, '    %d) ', 000h
    db   'Secondary ', 000h
    db   'Primary ', 000h
    db   'Slave', 000h
    db   'Master', 000h
    db   'No hard disks found', 000h
    db   00ah, 000h
    db   'Press F12 to select boot device.', 00ah, 000h
    db   00ah, 'VirtualBox temporary boot device selection', 00ah, 00ah, 'Detected H'
    db   'ard disks:', 00ah, 00ah, 000h
    db   00ah, 'Other boot devices:', 00ah, ' f) Floppy', 00ah, ' c) CD-ROM', 00ah
    db   ' l) LAN', 00ah, 00ah, ' b) Continue booting', 00ah, 000h
    db   'Delaying boot for %d seconds:', 000h
    db   ' %d', 000h
    db   'scsi_read_sectors: device_id out of range %d', 00ah, 000h
    db   'scsi_write_sectors: device_id out of range %d', 00ah, 000h
    db   'scsi_enumerate_attached_devices: SCSI_INQUIRY failed', 00ah, 000h
    db   'scsi_enumerate_attached_devices: SCSI_READ_CAPACITY failed', 00ah, 000h
    db   'Disk %d has an unsupported sector size of %u', 00ah, 000h, 000h
    db   'ahci_read_sectors', 000h
    db   '%s: device_id out of range %d', 00ah, 000h
    db   'ahci_write_sectors', 000h
    db   'ahci_cmd_packet', 000h
    db   '%s: DATA_OUT not supported yet', 00ah, 000h

section CONST2 progbits vstart=0xbf6 align=1 ; size=0x408 class=DATA group=DGROUP
_bios_cvs_version_string:                    ; 0xf0bf6 LB 0x20
    db  'VirtualBox VBOX_VERSION_STRING', 000h, 000h
_bios_prefix_string:                         ; 0xf0c16 LB 0x8
    db  'BIOS: ', 000h, 000h
_isotag:                                     ; 0xf0c1e LB 0x6
    db  'CD001', 000h
_eltorito:                                   ; 0xf0c24 LB 0x18
    db  'EL TORITO SPECIFICATION', 000h
_drivetypes:                                 ; 0xf0c3c LB 0x28
    db  046h, 06ch, 06fh, 070h, 070h, 079h, 000h, 000h, 000h, 000h, 048h, 061h, 072h, 064h, 020h, 044h
    db  069h, 073h, 06bh, 000h, 043h, 044h, 02dh, 052h, 04fh, 04dh, 000h, 000h, 000h, 000h, 04ch, 041h
    db  04eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
_scan_to_scanascii:                          ; 0xf0c64 LB 0x37a
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 01bh, 001h, 01bh, 001h, 01bh, 001h
    db  000h, 001h, 000h, 000h, 031h, 002h, 021h, 002h, 000h, 000h, 000h, 078h, 000h, 000h, 032h, 003h
    db  040h, 003h, 000h, 003h, 000h, 079h, 000h, 000h, 033h, 004h, 023h, 004h, 000h, 000h, 000h, 07ah
    db  000h, 000h, 034h, 005h, 024h, 005h, 000h, 000h, 000h, 07bh, 000h, 000h, 035h, 006h, 025h, 006h
    db  000h, 000h, 000h, 07ch, 000h, 000h, 036h, 007h, 05eh, 007h, 01eh, 007h, 000h, 07dh, 000h, 000h
    db  037h, 008h, 026h, 008h, 000h, 000h, 000h, 07eh, 000h, 000h, 038h, 009h, 02ah, 009h, 000h, 000h
    db  000h, 07fh, 000h, 000h, 039h, 00ah, 028h, 00ah, 000h, 000h, 000h, 080h, 000h, 000h, 030h, 00bh
    db  029h, 00bh, 000h, 000h, 000h, 081h, 000h, 000h, 02dh, 00ch, 05fh, 00ch, 01fh, 00ch, 000h, 082h
    db  000h, 000h, 03dh, 00dh, 02bh, 00dh, 000h, 000h, 000h, 083h, 000h, 000h, 008h, 00eh, 008h, 00eh
    db  07fh, 00eh, 000h, 000h, 000h, 000h, 009h, 00fh, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h
    db  071h, 010h, 051h, 010h, 011h, 010h, 000h, 010h, 040h, 000h, 077h, 011h, 057h, 011h, 017h, 011h
    db  000h, 011h, 040h, 000h, 065h, 012h, 045h, 012h, 005h, 012h, 000h, 012h, 040h, 000h, 072h, 013h
    db  052h, 013h, 012h, 013h, 000h, 013h, 040h, 000h, 074h, 014h, 054h, 014h, 014h, 014h, 000h, 014h
    db  040h, 000h, 079h, 015h, 059h, 015h, 019h, 015h, 000h, 015h, 040h, 000h, 075h, 016h, 055h, 016h
    db  015h, 016h, 000h, 016h, 040h, 000h, 069h, 017h, 049h, 017h, 009h, 017h, 000h, 017h, 040h, 000h
    db  06fh, 018h, 04fh, 018h, 00fh, 018h, 000h, 018h, 040h, 000h, 070h, 019h, 050h, 019h, 010h, 019h
    db  000h, 019h, 040h, 000h, 05bh, 01ah, 07bh, 01ah, 01bh, 01ah, 000h, 000h, 000h, 000h, 05dh, 01bh
    db  07dh, 01bh, 01dh, 01bh, 000h, 000h, 000h, 000h, 00dh, 01ch, 00dh, 01ch, 00ah, 01ch, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 061h, 01eh, 041h, 01eh
    db  001h, 01eh, 000h, 01eh, 040h, 000h, 073h, 01fh, 053h, 01fh, 013h, 01fh, 000h, 01fh, 040h, 000h
    db  064h, 020h, 044h, 020h, 004h, 020h, 000h, 020h, 040h, 000h, 066h, 021h, 046h, 021h, 006h, 021h
    db  000h, 021h, 040h, 000h, 067h, 022h, 047h, 022h, 007h, 022h, 000h, 022h, 040h, 000h, 068h, 023h
    db  048h, 023h, 008h, 023h, 000h, 023h, 040h, 000h, 06ah, 024h, 04ah, 024h, 00ah, 024h, 000h, 024h
    db  040h, 000h, 06bh, 025h, 04bh, 025h, 00bh, 025h, 000h, 025h, 040h, 000h, 06ch, 026h, 04ch, 026h
    db  00ch, 026h, 000h, 026h, 040h, 000h, 03bh, 027h, 03ah, 027h, 000h, 000h, 000h, 000h, 000h, 000h
    db  027h, 028h, 022h, 028h, 000h, 000h, 000h, 000h, 000h, 000h, 060h, 029h, 07eh, 029h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 05ch, 02bh
    db  07ch, 02bh, 01ch, 02bh, 000h, 000h, 000h, 000h, 07ah, 02ch, 05ah, 02ch, 01ah, 02ch, 000h, 02ch
    db  040h, 000h, 078h, 02dh, 058h, 02dh, 018h, 02dh, 000h, 02dh, 040h, 000h, 063h, 02eh, 043h, 02eh
    db  003h, 02eh, 000h, 02eh, 040h, 000h, 076h, 02fh, 056h, 02fh, 016h, 02fh, 000h, 02fh, 040h, 000h
    db  062h, 030h, 042h, 030h, 002h, 030h, 000h, 030h, 040h, 000h, 06eh, 031h, 04eh, 031h, 00eh, 031h
    db  000h, 031h, 040h, 000h, 06dh, 032h, 04dh, 032h, 00dh, 032h, 000h, 032h, 040h, 000h, 02ch, 033h
    db  03ch, 033h, 000h, 000h, 000h, 000h, 000h, 000h, 02eh, 034h, 03eh, 034h, 000h, 000h, 000h, 000h
    db  000h, 000h, 02fh, 035h, 03fh, 035h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 02ah, 037h, 02ah, 037h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 020h, 039h, 020h, 039h, 020h, 039h
    db  020h, 039h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 03bh
    db  000h, 054h, 000h, 05eh, 000h, 068h, 000h, 000h, 000h, 03ch, 000h, 055h, 000h, 05fh, 000h, 069h
    db  000h, 000h, 000h, 03dh, 000h, 056h, 000h, 060h, 000h, 06ah, 000h, 000h, 000h, 03eh, 000h, 057h
    db  000h, 061h, 000h, 06bh, 000h, 000h, 000h, 03fh, 000h, 058h, 000h, 062h, 000h, 06ch, 000h, 000h
    db  000h, 040h, 000h, 059h, 000h, 063h, 000h, 06dh, 000h, 000h, 000h, 041h, 000h, 05ah, 000h, 064h
    db  000h, 06eh, 000h, 000h, 000h, 042h, 000h, 05bh, 000h, 065h, 000h, 06fh, 000h, 000h, 000h, 043h
    db  000h, 05ch, 000h, 066h, 000h, 070h, 000h, 000h, 000h, 044h, 000h, 05dh, 000h, 067h, 000h, 071h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 047h, 037h, 047h, 000h, 077h, 000h, 000h, 020h, 000h
    db  000h, 048h, 038h, 048h, 000h, 000h, 000h, 000h, 020h, 000h, 000h, 049h, 039h, 049h, 000h, 084h
    db  000h, 000h, 020h, 000h, 02dh, 04ah, 02dh, 04ah, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 04bh
    db  034h, 04bh, 000h, 073h, 000h, 000h, 020h, 000h, 000h, 04ch, 035h, 04ch, 000h, 000h, 000h, 000h
    db  020h, 000h, 000h, 04dh, 036h, 04dh, 000h, 074h, 000h, 000h, 020h, 000h, 02bh, 04eh, 02bh, 04eh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 04fh, 031h, 04fh, 000h, 075h, 000h, 000h, 020h, 000h
    db  000h, 050h, 032h, 050h, 000h, 000h, 000h, 000h, 020h, 000h, 000h, 051h, 033h, 051h, 000h, 076h
    db  000h, 000h, 020h, 000h, 000h, 052h, 030h, 052h, 000h, 000h, 000h, 000h, 020h, 000h, 000h, 053h
    db  02eh, 053h, 000h, 000h, 000h, 000h, 020h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 05ch, 056h, 07ch, 056h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 085h, 000h, 087h, 000h, 089h, 000h, 08bh, 000h, 000h
    db  000h, 086h, 000h, 088h, 000h, 08ah, 000h, 08ch, 000h, 000h
_panic_msg_keyb_buffer_full:                 ; 0xf0fde LB 0x20
    db  '%s: keyboard input buffer full', 00ah, 000h

  ; Padding 0x602 bytes at 0xf0ffe
  times 1538 db 0

section _TEXT progbits vstart=0x1600 align=1 ; size=0x780f class=CODE group=AUTO
read_byte_:                                  ; 0xf1600 LB 0xe
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov al, byte [es:bx]                      ; 26 8a 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_byte_:                                 ; 0xf160e LB 0xe
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov byte [es:si], bl                      ; 26 88 1c
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
read_word_:                                  ; 0xf161c LB 0xe
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_word_:                                 ; 0xf162a LB 0xe
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov word [es:si], bx                      ; 26 89 1c
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
read_dword_:                                 ; 0xf1638 LB 0x12
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, dx                                ; 89 d3
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
write_dword_:                                ; 0xf164a LB 0x12
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov word [es:si], bx                      ; 26 89 1c
    mov word [es:si+002h], cx                 ; 26 89 4c 02
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
inb_cmos_:                                   ; 0xf165c LB 0x11
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 00070h                ; ba 70 00
    out DX, AL                                ; ee
    mov dx, strict word 00071h                ; ba 71 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
outb_cmos_:                                  ; 0xf166d LB 0x11
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov ah, dl                                ; 88 d4
    mov dx, strict word 00070h                ; ba 70 00
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    mov dx, strict word 00071h                ; ba 71 00
    out DX, AL                                ; ee
    pop bp                                    ; 5d
    retn                                      ; c3
_dummy_isr_function:                         ; 0xf167e LB 0x65
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    mov CL, strict byte 0ffh                  ; b1 ff
    mov AL, strict byte 00bh                  ; b0 0b
    mov dx, strict word 00020h                ; ba 20 00
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    mov byte [bp-002h], al                    ; 88 46 fe
    test al, al                               ; 84 c0
    je short 016d2h                           ; 74 3c
    mov AL, strict byte 00bh                  ; b0 0b
    mov dx, 000a0h                            ; ba a0 00
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    test al, al                               ; 84 c0
    je short 016bah                           ; 74 15
    mov dx, 000a1h                            ; ba a1 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bl, al                                ; 88 c3
    mov al, cl                                ; 88 c8
    or al, bl                                 ; 08 d8
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    mov dx, 000a0h                            ; ba a0 00
    out DX, AL                                ; ee
    jmp short 016c9h                          ; eb 0f
    mov dx, strict word 00021h                ; ba 21 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and bl, 0fbh                              ; 80 e3 fb
    mov byte [bp-002h], bl                    ; 88 5e fe
    or al, bl                                 ; 08 d8
    out DX, AL                                ; ee
    mov AL, strict byte 020h                  ; b0 20
    mov dx, strict word 00020h                ; ba 20 00
    out DX, AL                                ; ee
    mov cl, byte [bp-002h]                    ; 8a 4e fe
    mov bl, cl                                ; 88 cb
    xor bh, bh                                ; 30 ff
    mov dx, strict word 0006bh                ; ba 6b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 2f ff
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
_nmi_handler_msg:                            ; 0xf16e3 LB 0x13
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov ax, strict word 00018h                ; b8 18 00
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 09 02
    add sp, strict byte 00004h                ; 83 c4 04
    pop bp                                    ; 5d
    retn                                      ; c3
_int18_panic_msg:                            ; 0xf16f6 LB 0x13
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov ax, strict word 0002ch                ; b8 2c 00
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 f6 01
    add sp, strict byte 00004h                ; 83 c4 04
    pop bp                                    ; 5d
    retn                                      ; c3
_log_bios_start:                             ; 0xf1709 LB 0x22
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 a6 01
    mov ax, 00bf6h                            ; b8 f6 0b
    push ax                                   ; 50
    mov ax, strict word 00041h                ; b8 41 00
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 d4 01
    add sp, strict byte 00006h                ; 83 c4 06
    pop bp                                    ; 5d
    retn                                      ; c3
_print_bios_banner:                          ; 0xf172b LB 0x2c
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 00072h                ; ba 72 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 e5 fe
    mov cx, ax                                ; 89 c1
    xor bx, bx                                ; 31 db
    mov dx, strict word 00072h                ; ba 72 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 e6 fe
    cmp cx, 01234h                            ; 81 f9 34 12
    jne short 01752h                          ; 75 08
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 000h                  ; b4 00
    int 010h                                  ; cd 10
    pop bp                                    ; 5d
    retn                                      ; c3
    call 073e3h                               ; e8 8e 5c
    pop bp                                    ; 5d
    retn                                      ; c3
send_:                                       ; 0xf1757 LB 0x38
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov cl, dl                                ; 88 d1
    test AL, strict byte 008h                 ; a8 08
    je short 0176ah                           ; 74 06
    mov al, dl                                ; 88 d0
    mov dx, 00403h                            ; ba 03 04
    out DX, AL                                ; ee
    test bl, 004h                             ; f6 c3 04
    je short 01775h                           ; 74 06
    mov al, cl                                ; 88 c8
    mov dx, 00504h                            ; ba 04 05
    out DX, AL                                ; ee
    test bl, 002h                             ; f6 c3 02
    je short 0178bh                           ; 74 11
    cmp cl, 00ah                              ; 80 f9 0a
    jne short 01785h                          ; 75 06
    mov AL, strict byte 00dh                  ; b0 0d
    mov AH, strict byte 00eh                  ; b4 0e
    int 010h                                  ; cd 10
    mov al, cl                                ; 88 c8
    mov AH, strict byte 00eh                  ; b4 0e
    int 010h                                  ; cd 10
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
put_int_:                                    ; 0xf178f LB 0x62
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov word [bp-004h], dx                    ; 89 56 fc
    mov di, bx                                ; 89 df
    mov bx, strict word 0000ah                ; bb 0a 00
    mov ax, dx                                ; 89 d0
    cwd                                       ; 99
    idiv bx                                   ; f7 fb
    mov word [bp-002h], ax                    ; 89 46 fe
    test ax, ax                               ; 85 c0
    je short 017b8h                           ; 74 0c
    lea bx, [di-001h]                         ; 8d 5d ff
    mov dx, ax                                ; 89 c2
    mov ax, si                                ; 89 f0
    call 0178fh                               ; e8 d9 ff
    jmp short 017d3h                          ; eb 1b
    dec di                                    ; 4f
    test di, di                               ; 85 ff
    jle short 017c7h                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 92 ff
    jmp short 017b8h                          ; eb f1
    test cx, cx                               ; 85 c9
    je short 017d3h                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 84 ff
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov BL, strict byte 00ah                  ; b3 0a
    mul bl                                    ; f6 e3
    mov bl, byte [bp-004h]                    ; 8a 5e fc
    sub bl, al                                ; 28 c3
    add bl, 030h                              ; 80 c3 30
    xor bh, bh                                ; 30 ff
    mov dx, bx                                ; 89 da
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 6c ff
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
put_uint_:                                   ; 0xf17f1 LB 0x5d
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov word [bp-004h], dx                    ; 89 56 fc
    mov ax, dx                                ; 89 d0
    xor dx, dx                                ; 31 d2
    mov di, strict word 0000ah                ; bf 0a 00
    div di                                    ; f7 f7
    mov word [bp-002h], ax                    ; 89 46 fe
    test ax, ax                               ; 85 c0
    je short 01817h                           ; 74 0a
    dec bx                                    ; 4b
    mov dx, ax                                ; 89 c2
    mov ax, si                                ; 89 f0
    call 017f1h                               ; e8 dc ff
    jmp short 01832h                          ; eb 1b
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jle short 01826h                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 33 ff
    jmp short 01817h                          ; eb f1
    test cx, cx                               ; 85 c9
    je short 01832h                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 25 ff
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov DL, strict byte 00ah                  ; b2 0a
    mul dl                                    ; f6 e2
    mov dl, byte [bp-004h]                    ; 8a 56 fc
    sub dl, al                                ; 28 c2
    add dl, 030h                              ; 80 c2 30
    xor dh, dh                                ; 30 f6
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 0f ff
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
put_luint_:                                  ; 0xf184e LB 0x6f
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov word [bp-002h], bx                    ; 89 5e fe
    mov di, dx                                ; 89 d7
    mov ax, bx                                ; 89 d8
    mov dx, cx                                ; 89 ca
    mov bx, strict word 0000ah                ; bb 0a 00
    xor cx, cx                                ; 31 c9
    call 08d37h                               ; e8 cf 74
    mov word [bp-004h], ax                    ; 89 46 fc
    mov cx, dx                                ; 89 d1
    mov dx, ax                                ; 89 c2
    or dx, cx                                 ; 09 ca
    je short 01882h                           ; 74 0f
    push word [bp+008h]                       ; ff 76 08
    lea dx, [di-001h]                         ; 8d 55 ff
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 0184eh                               ; e8 ce ff
    jmp short 0189fh                          ; eb 1d
    dec di                                    ; 4f
    test di, di                               ; 85 ff
    jle short 01891h                          ; 7e 0a
    mov dx, strict word 00020h                ; ba 20 00
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 c8 fe
    jmp short 01882h                          ; eb f1
    cmp word [bp+008h], strict byte 00000h    ; 83 7e 08 00
    je short 0189fh                           ; 74 08
    mov dx, strict word 0002dh                ; ba 2d 00
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 b8 fe
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov DL, strict byte 00ah                  ; b2 0a
    mul dl                                    ; f6 e2
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    sub dl, al                                ; 28 c2
    add dl, 030h                              ; 80 c2 30
    xor dh, dh                                ; 30 f6
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 a2 fe
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
put_str_:                                    ; 0xf18bd LB 0x1e
    push dx                                   ; 52
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, ax                                ; 89 c6
    mov es, cx                                ; 8e c1
    mov dl, byte [es:bx]                      ; 26 8a 17
    test dl, dl                               ; 84 d2
    je short 018d7h                           ; 74 0a
    xor dh, dh                                ; 30 f6
    mov ax, si                                ; 89 f0
    call 01757h                               ; e8 83 fe
    inc bx                                    ; 43
    jmp short 018c4h                          ; eb ed
    pop bp                                    ; 5d
    pop si                                    ; 5e
    pop dx                                    ; 5a
    retn                                      ; c3
put_str_near_:                               ; 0xf18db LB 0x1f
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov cx, ax                                ; 89 c1
    mov bx, dx                                ; 89 d3
    mov al, byte [bx]                         ; 8a 07
    test al, al                               ; 84 c0
    je short 018f6h                           ; 74 0c
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    mov ax, cx                                ; 89 c8
    call 01757h                               ; e8 64 fe
    inc bx                                    ; 43
    jmp short 018e4h                          ; eb ee
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
bios_printf_:                                ; 0xf18fa LB 0x24f
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00008h                ; 83 ec 08
    lea bx, [bp+012h]                         ; 8d 5e 12
    mov word [bp-008h], bx                    ; 89 5e f8
    mov [bp-006h], ss                         ; 8c 56 fa
    xor cx, cx                                ; 31 c9
    xor si, si                                ; 31 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    and ax, strict word 00007h                ; 25 07 00
    cmp ax, strict word 00007h                ; 3d 07 00
    jne short 01931h                          ; 75 14
    xor al, al                                ; 30 c0
    mov dx, 00401h                            ; ba 01 04
    out DX, AL                                ; ee
    mov ax, strict word 00046h                ; b8 46 00
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 cc ff
    add sp, strict byte 00004h                ; 83 c4 04
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov dl, byte [bx]                         ; 8a 17
    test dl, dl                               ; 84 d2
    je short 01999h                           ; 74 5f
    cmp dl, 025h                              ; 80 fa 25
    jne short 01947h                          ; 75 08
    mov cx, strict word 00001h                ; b9 01 00
    xor si, si                                ; 31 f6
    jmp near 01b28h                           ; e9 e1 01
    test cx, cx                               ; 85 c9
    je short 0199ch                           ; 74 51
    cmp dl, 030h                              ; 80 fa 30
    jc short 0196ah                           ; 72 1a
    cmp dl, 039h                              ; 80 fa 39
    jnbe short 0196ah                         ; 77 15
    mov bl, dl                                ; 88 d3
    xor bh, bh                                ; 30 ff
    mov ax, si                                ; 89 f0
    mov dx, strict word 0000ah                ; ba 0a 00
    mul dx                                    ; f7 e2
    sub bx, strict byte 00030h                ; 83 eb 30
    mov si, ax                                ; 89 c6
    add si, bx                                ; 01 de
    jmp near 01b28h                           ; e9 be 01
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [bp-006h], ax                    ; 89 46 fa
    add word [bp-008h], strict byte 00002h    ; 83 46 f8 02
    les bx, [bp-008h]                         ; c4 5e f8
    mov ax, word [es:bx-002h]                 ; 26 8b 47 fe
    mov word [bp-002h], ax                    ; 89 46 fe
    cmp dl, 078h                              ; 80 fa 78
    je short 01988h                           ; 74 05
    cmp dl, 058h                              ; 80 fa 58
    jne short 019d6h                          ; 75 4e
    test si, si                               ; 85 f6
    jne short 0198fh                          ; 75 03
    mov si, strict word 00004h                ; be 04 00
    cmp dl, 078h                              ; 80 fa 78
    jne short 0199fh                          ; 75 0b
    mov di, strict word 00061h                ; bf 61 00
    jmp short 019a2h                          ; eb 09
    jmp near 01b2eh                           ; e9 92 01
    jmp near 01b20h                           ; e9 81 01
    mov di, strict word 00041h                ; bf 41 00
    lea bx, [si-001h]                         ; 8d 5c ff
    test bx, bx                               ; 85 db
    jl short 019e7h                           ; 7c 3e
    mov cx, bx                                ; 89 d9
    sal cx, 1                                 ; d1 e1
    sal cx, 1                                 ; d1 e1
    mov ax, word [bp-002h]                    ; 8b 46 fe
    shr ax, CL                                ; d3 e8
    xor ah, ah                                ; 30 e4
    and AL, strict byte 00fh                  ; 24 0f
    cmp ax, strict word 00009h                ; 3d 09 00
    jnbe short 019c4h                         ; 77 07
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00030h                ; 83 c2 30
    jmp short 019cbh                          ; eb 07
    mov dx, ax                                ; 89 c2
    sub dx, strict byte 0000ah                ; 83 ea 0a
    add dx, di                                ; 01 fa
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01757h                               ; e8 84 fd
    dec bx                                    ; 4b
    jmp short 019a5h                          ; eb cf
    cmp dl, 075h                              ; 80 fa 75
    jne short 019eah                          ; 75 0f
    xor cx, cx                                ; 31 c9
    mov bx, si                                ; 89 f3
    mov dx, ax                                ; 89 c2
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 017f1h                               ; e8 0a fe
    jmp near 01b1ch                           ; e9 32 01
    lea bx, [si-001h]                         ; 8d 5c ff
    cmp dl, 06ch                              ; 80 fa 6c
    jne short 01a4ah                          ; 75 58
    inc word [bp+010h]                        ; ff 46 10
    mov di, word [bp+010h]                    ; 8b 7e 10
    mov dl, byte [di]                         ; 8a 15
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [bp-006h], ax                    ; 89 46 fa
    add word [bp-008h], strict byte 00002h    ; 83 46 f8 02
    les di, [bp-008h]                         ; c4 7e f8
    mov ax, word [es:di-002h]                 ; 26 8b 45 fe
    mov word [bp-004h], ax                    ; 89 46 fc
    cmp dl, 064h                              ; 80 fa 64
    jne short 01a43h                          ; 75 30
    test byte [bp-003h], 080h                 ; f6 46 fd 80
    je short 01a30h                           ; 74 17
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    mov ax, word [bp-002h]                    ; 8b 46 fe
    mov cx, word [bp-004h]                    ; 8b 4e fc
    neg cx                                    ; f7 d9
    neg ax                                    ; f7 d8
    sbb cx, strict byte 00000h                ; 83 d9 00
    mov dx, bx                                ; 89 da
    mov bx, ax                                ; 89 c3
    jmp short 01a3bh                          ; eb 0b
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov bx, word [bp-002h]                    ; 8b 5e fe
    mov dx, si                                ; 89 f2
    mov cx, word [bp-004h]                    ; 8b 4e fc
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 0184eh                               ; e8 0d fe
    jmp short 019e7h                          ; eb a4
    cmp dl, 075h                              ; 80 fa 75
    jne short 01a4ch                          ; 75 04
    jmp short 01a30h                          ; eb e6
    jmp short 01aa6h                          ; eb 5a
    cmp dl, 078h                              ; 80 fa 78
    je short 01a56h                           ; 74 05
    cmp dl, 058h                              ; 80 fa 58
    jne short 019e7h                          ; 75 91
    test si, si                               ; 85 f6
    jne short 01a5dh                          ; 75 03
    mov si, strict word 00008h                ; be 08 00
    cmp dl, 078h                              ; 80 fa 78
    jne short 01a67h                          ; 75 05
    mov di, strict word 00061h                ; bf 61 00
    jmp short 01a6ah                          ; eb 03
    mov di, strict word 00041h                ; bf 41 00
    lea bx, [si-001h]                         ; 8d 5c ff
    test bx, bx                               ; 85 db
    jl short 01ad7h                           ; 7c 66
    mov ax, word [bp-002h]                    ; 8b 46 fe
    mov cx, bx                                ; 89 d9
    sal cx, 1                                 ; d1 e1
    sal cx, 1                                 ; d1 e1
    mov dx, word [bp-004h]                    ; 8b 56 fc
    jcxz 01a85h                               ; e3 06
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 01a7fh                               ; e2 fa
    and ax, strict word 0000fh                ; 25 0f 00
    cmp ax, strict word 00009h                ; 3d 09 00
    jnbe short 01a94h                         ; 77 07
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00030h                ; 83 c2 30
    jmp short 01a9bh                          ; eb 07
    mov dx, ax                                ; 89 c2
    sub dx, strict byte 0000ah                ; 83 ea 0a
    add dx, di                                ; 01 fa
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01757h                               ; e8 b4 fc
    dec bx                                    ; 4b
    jmp short 01a6dh                          ; eb c7
    cmp dl, 064h                              ; 80 fa 64
    jne short 01ac8h                          ; 75 1d
    test byte [bp-001h], 080h                 ; f6 46 ff 80
    je short 01abah                           ; 74 09
    mov dx, ax                                ; 89 c2
    neg dx                                    ; f7 da
    mov cx, strict word 00001h                ; b9 01 00
    jmp short 01ac0h                          ; eb 06
    xor cx, cx                                ; 31 c9
    mov bx, si                                ; 89 f3
    mov dx, ax                                ; 89 c2
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 0178fh                               ; e8 c9 fc
    jmp short 01b1ch                          ; eb 54
    cmp dl, 073h                              ; 80 fa 73
    jne short 01ad9h                          ; 75 0c
    mov cx, ds                                ; 8c d9
    mov bx, ax                                ; 89 c3
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 018bdh                               ; e8 e6 fd
    jmp short 01b1ch                          ; eb 43
    cmp dl, 053h                              ; 80 fa 53
    jne short 01afch                          ; 75 1e
    mov word [bp-004h], ax                    ; 89 46 fc
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [bp-006h], ax                    ; 89 46 fa
    add word [bp-008h], strict byte 00002h    ; 83 46 f8 02
    les bx, [bp-008h]                         ; c4 5e f8
    mov ax, word [es:bx-002h]                 ; 26 8b 47 fe
    mov word [bp-002h], ax                    ; 89 46 fe
    mov bx, ax                                ; 89 c3
    mov cx, word [bp-004h]                    ; 8b 4e fc
    jmp short 01ad1h                          ; eb d5
    cmp dl, 063h                              ; 80 fa 63
    jne short 01b0eh                          ; 75 0d
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01757h                               ; e8 4b fc
    jmp short 01b1ch                          ; eb 0e
    mov ax, strict word 0004eh                ; b8 4e 00
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 e1 fd
    add sp, strict byte 00004h                ; 83 c4 04
    xor cx, cx                                ; 31 c9
    jmp short 01b28h                          ; eb 08
    xor dh, dh                                ; 30 f6
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    call 01757h                               ; e8 2f fc
    inc word [bp+010h]                        ; ff 46 10
    jmp near 01931h                           ; e9 03 fe
    xor ax, ax                                ; 31 c0
    mov word [bp-008h], ax                    ; 89 46 f8
    mov word [bp-006h], ax                    ; 89 46 fa
    test byte [bp+00eh], 001h                 ; f6 46 0e 01
    je short 01b40h                           ; 74 04
    cli                                       ; fa
    hlt                                       ; f4
    jmp short 01b3dh                          ; eb fd
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
_ata_init:                                   ; 0xf1b49 LB 0xe4
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 c5 fa
    mov si, 00122h                            ; be 22 01
    mov di, ax                                ; 89 c7
    xor cl, cl                                ; 30 c9
    jmp short 01b65h                          ; eb 05
    cmp cl, 004h                              ; 80 f9 04
    jnc short 01b92h                          ; 73 2d
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    mov bx, strict word 00006h                ; bb 06 00
    imul bx                                   ; f7 eb
    mov es, di                                ; 8e c7
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov byte [es:bx+001c0h], 000h             ; 26 c6 87 c0 01 00
    mov word [es:bx+001c2h], strict word 00000h ; 26 c7 87 c2 01 00 00
    mov word [es:bx+001c4h], strict word 00000h ; 26 c7 87 c4 01 00 00
    mov byte [es:bx+001c1h], 000h             ; 26 c6 87 c1 01 00
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    jmp short 01b60h                          ; eb ce
    xor cl, cl                                ; 30 c9
    jmp short 01b9bh                          ; eb 05
    cmp cl, 008h                              ; 80 f9 08
    jnc short 01bfah                          ; 73 5f
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    mov bx, strict word 00018h                ; bb 18 00
    imul bx                                   ; f7 eb
    mov es, di                                ; 8e c7
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov word [es:bx+01eh], strict word 00000h ; 26 c7 47 1e 00 00
    mov word [es:bx+020h], strict word 00000h ; 26 c7 47 20 00 00
    mov byte [es:bx+022h], 000h               ; 26 c6 47 22 00
    mov word [es:bx+024h], 00200h             ; 26 c7 47 24 00 02
    mov byte [es:bx+023h], 000h               ; 26 c6 47 23 00
    mov word [es:bx+026h], strict word 00000h ; 26 c7 47 26 00 00
    mov word [es:bx+028h], strict word 00000h ; 26 c7 47 28 00 00
    mov word [es:bx+02ah], strict word 00000h ; 26 c7 47 2a 00 00
    mov word [es:bx+02ch], strict word 00000h ; 26 c7 47 2c 00 00
    mov word [es:bx+02eh], strict word 00000h ; 26 c7 47 2e 00 00
    mov word [es:bx+030h], strict word 00000h ; 26 c7 47 30 00 00
    mov word [es:bx+032h], strict word 00000h ; 26 c7 47 32 00 00
    mov word [es:bx+034h], strict word 00000h ; 26 c7 47 34 00 00
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    jmp short 01b96h                          ; eb 9c
    xor cl, cl                                ; 30 c9
    jmp short 01c03h                          ; eb 05
    cmp cl, 010h                              ; 80 f9 10
    jnc short 01c1bh                          ; 73 18
    mov bl, cl                                ; 88 cb
    xor bh, bh                                ; 30 ff
    mov es, di                                ; 8e c7
    add bx, si                                ; 01 f3
    mov byte [es:bx+0019fh], 010h             ; 26 c6 87 9f 01 10
    mov byte [es:bx+001b0h], 010h             ; 26 c6 87 b0 01 10
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    jmp short 01bfeh                          ; eb e3
    mov es, di                                ; 8e c7
    mov byte [es:si+0019eh], 000h             ; 26 c6 84 9e 01 00
    mov byte [es:si+001afh], 000h             ; 26 c6 84 af 01 00
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ata_reset_:                                  ; 0xf1c2d LB 0xe4
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push ax                                   ; 50
    push ax                                   ; 50
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 db f9
    mov es, ax                                ; 8e c0
    mov di, 00122h                            ; bf 22 01
    mov word [bp-004h], ax                    ; 89 46 fc
    mov ax, word [bp-006h]                    ; 8b 46 fa
    shr ax, 1                                 ; d1 e8
    mov ah, byte [bp-006h]                    ; 8a 66 fa
    and ah, 001h                              ; 80 e4 01
    mov byte [bp-002h], ah                    ; 88 66 fe
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    mov bx, ax                                ; 89 c3
    add bx, di                                ; 01 fb
    mov cx, word [es:bx+001c2h]               ; 26 8b 8f c2 01
    mov si, word [es:bx+001c4h]               ; 26 8b b7 c4 01
    lea dx, [si+006h]                         ; 8d 54 06
    mov AL, strict byte 00eh                  ; b0 0e
    out DX, AL                                ; ee
    mov bx, 000ffh                            ; bb ff 00
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 01c86h                          ; 76 0c
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01c75h                           ; 74 ef
    lea dx, [si+006h]                         ; 8d 54 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, di                                ; 89 fb
    add bx, ax                                ; 01 c3
    cmp byte [es:bx+01eh], 000h               ; 26 80 7f 1e 00
    je short 01ceeh                           ; 74 4c
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 01cadh                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 01cb0h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00006h                ; 83 c2 06
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    inc dx                                    ; 42
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00003h                ; 83 c2 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp bl, 001h                              ; 80 fb 01
    jne short 01ceeh                          ; 75 22
    cmp al, bl                                ; 38 d8
    jne short 01ceeh                          ; 75 1e
    mov bx, strict word 0ffffh                ; bb ff ff
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 01ceeh                          ; 76 16
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01ceeh                           ; 74 0a
    mov ax, strict word 0ffffh                ; b8 ff ff
    dec ax                                    ; 48
    test ax, ax                               ; 85 c0
    jnbe short 01ce7h                         ; 77 fb
    jmp short 01cd3h                          ; eb e5
    mov bx, strict word 00010h                ; bb 10 00
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 01d02h                          ; 76 0c
    mov dx, cx                                ; 89 ca
    add dx, strict byte 00007h                ; 83 c2 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 040h                 ; a8 40
    je short 01cf1h                           ; 74 ef
    lea dx, [si+006h]                         ; 8d 54 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ata_cmd_data_in_:                            ; 0xf1d11 LB 0x268
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0001ch                ; 83 ec 1c
    mov si, ax                                ; 89 c6
    mov word [bp-016h], dx                    ; 89 56 ea
    mov word [bp-00eh], bx                    ; 89 5e f2
    mov word [bp-018h], cx                    ; 89 4e e8
    mov es, dx                                ; 8e c2
    mov cl, byte [es:si+008h]                 ; 26 8a 4c 08
    xor ch, ch                                ; 30 ed
    mov ax, cx                                ; 89 c8
    mov dx, cx                                ; 89 ca
    shr dx, 1                                 ; d1 ea
    and AL, strict byte 001h                  ; 24 01
    mov byte [bp-004h], al                    ; 88 46 fc
    mov al, dl                                ; 88 d0
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov bx, word [es:di+001c2h]               ; 26 8b 9d c2 01
    mov ax, word [es:di+001c4h]               ; 26 8b 85 c4 01
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, cx                                ; 89 c8
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov al, byte [es:di+022h]                 ; 26 8a 45 22
    mov byte [bp-002h], al                    ; 88 46 fe
    mov ax, word [es:di+024h]                 ; 26 8b 45 24
    mov word [bp-006h], ax                    ; 89 46 fa
    test ax, ax                               ; 85 c0
    jne short 01d80h                          ; 75 14
    cmp byte [bp-002h], 001h                  ; 80 7e fe 01
    jne short 01d79h                          ; 75 07
    mov word [bp-006h], 04000h                ; c7 46 fa 00 40
    jmp short 01d8ch                          ; eb 13
    mov word [bp-006h], 08000h                ; c7 46 fa 00 80
    jmp short 01d8ch                          ; eb 0c
    cmp byte [bp-002h], 001h                  ; 80 7e fe 01
    jne short 01d89h                          ; 75 03
    shr word [bp-006h], 1                     ; d1 6e fa
    shr word [bp-006h], 1                     ; d1 6e fa
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 01da5h                           ; 74 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 01f61h                           ; e9 bc 01
    mov es, [bp-016h]                         ; 8e 46 ea
    mov ax, word [es:si]                      ; 26 8b 04
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [es:si+002h]                 ; 26 8b 44 02
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov di, word [es:si+004h]                 ; 26 8b 7c 04
    mov ax, word [es:si+006h]                 ; 26 8b 44 06
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, word [es:si+012h]                 ; 26 8b 44 12
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [es:si+00eh]                 ; 26 8b 44 0e
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [es:si+010h]                 ; 26 8b 44 10
    mov word [bp-014h], ax                    ; 89 46 ec
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    test ax, ax                               ; 85 c0
    jne short 01e3fh                          ; 75 63
    mov dx, word [bp-01ch]                    ; 8b 56 e4
    add dx, word [bp-018h]                    ; 03 56 e8
    adc ax, word [bp-01ah]                    ; 13 46 e6
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 01dech                         ; 77 02
    jne short 01e13h                          ; 75 27
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov al, ah                                ; 88 e0
    xor ah, ah                                ; 30 e4
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov al, ah                                ; 88 e0
    lea dx, [bx+002h]                         ; 8d 57 02
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    mov byte [bp-019h], al                    ; 88 46 e7
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    xor ah, ah                                ; 30 e4
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov cx, strict word 00008h                ; b9 08 00
    shr word [bp-01ah], 1                     ; d1 6e e6
    rcr word [bp-01ch], 1                     ; d1 5e e4
    loop 01e1eh                               ; e2 f8
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov word [bp-01ah], strict word 00000h    ; c7 46 e6 00 00
    and ax, strict word 0000fh                ; 25 0f 00
    or AL, strict byte 040h                   ; 0c 40
    mov word [bp-014h], ax                    ; 89 46 ec
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    mov al, byte [bp-018h]                    ; 8a 46 e8
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    out DX, AL                                ; ee
    mov ax, word [bp-012h]                    ; 8b 46 ee
    lea dx, [bx+004h]                         ; 8d 57 04
    out DX, AL                                ; ee
    mov al, byte [bp-011h]                    ; 8a 46 ef
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 01e75h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 01e78h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    mov dl, byte [bp-014h]                    ; 8a 56 ec
    xor dh, dh                                ; 30 f6
    or ax, dx                                 ; 09 d0
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    out DX, AL                                ; ee
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    cmp ax, 000c4h                            ; 3d c4 00
    je short 01e97h                           ; 74 05
    cmp ax, strict word 00029h                ; 3d 29 00
    jne short 01ea4h                          ; 75 0d
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov word [bp-010h], ax                    ; 89 46 f0
    mov word [bp-018h], strict word 00001h    ; c7 46 e8 01 00
    jmp short 01ea9h                          ; eb 05
    mov word [bp-010h], strict word 00001h    ; c7 46 f0 01 00
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 01ea9h                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 01ec8h                           ; 74 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 01f61h                           ; e9 99 00
    test dl, 008h                             ; f6 c2 08
    jne short 01edch                          ; 75 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 01f61h                           ; e9 85 00
    sti                                       ; fb
    cmp di, 0f800h                            ; 81 ff 00 f8
    jc short 01ef0h                           ; 72 0d
    sub di, 00800h                            ; 81 ef 00 08
    mov ax, word [bp-008h]                    ; 8b 46 f8
    add ax, 00080h                            ; 05 80 00
    mov word [bp-008h], ax                    ; 89 46 f8
    cmp byte [bp-002h], 001h                  ; 80 7e fe 01
    jne short 01f03h                          ; 75 0d
    mov dx, bx                                ; 89 da
    mov cx, word [bp-006h]                    ; 8b 4e fa
    mov es, [bp-008h]                         ; 8e 46 f8
    db  0f3h, 066h, 06dh
    ; rep insd                                  ; f3 66 6d
    jmp short 01f0dh                          ; eb 0a
    mov dx, bx                                ; 89 da
    mov cx, word [bp-006h]                    ; 8b 4e fa
    mov es, [bp-008h]                         ; 8e 46 f8
    rep insw                                  ; f3 6d
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov es, [bp-016h]                         ; 8e 46 ea
    add word [es:si+014h], ax                 ; 26 01 44 14
    dec word [bp-018h]                        ; ff 4e e8
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 01f1ah                          ; 75 f4
    cmp word [bp-018h], strict byte 00000h    ; 83 7e e8 00
    jne short 01f40h                          ; 75 14
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 01f56h                           ; 74 24
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00004h                ; ba 04 00
    jmp short 01f61h                          ; eb 21
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 01eddh                           ; 74 95
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00005h                ; ba 05 00
    jmp short 01f61h                          ; eb 0b
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    sal word [bx+di], 0f7h                    ; c1 21 f7
    and bx, di                                ; 21 fb
    and di, di                                ; 21 ff
    and word [bp+di], ax                      ; 21 03
    and al, byte [bx]                         ; 22 07
    and cl, byte [bp+di]                      ; 22 0b
    and cl, byte [bx]                         ; 22 0f
    db  022h
_ata_detect:                                 ; 0xf1f79 LB 0x695
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, 00262h                            ; 81 ec 62 02
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 91 f6
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov bx, 00122h                            ; bb 22 01
    mov es, ax                                ; 8e c0
    mov si, bx                                ; 89 de
    mov word [bp-022h], ax                    ; 89 46 de
    mov byte [es:bx+001c0h], 000h             ; 26 c6 87 c0 01 00
    mov word [es:bx+001c2h], 001f0h           ; 26 c7 87 c2 01 f0 01
    mov word [es:bx+001c4h], 003f0h           ; 26 c7 87 c4 01 f0 03
    mov byte [es:bx+001c1h], 00eh             ; 26 c6 87 c1 01 0e
    mov byte [es:bx+001c6h], 000h             ; 26 c6 87 c6 01 00
    mov word [es:bx+001c8h], 00170h           ; 26 c7 87 c8 01 70 01
    mov word [es:bx+001cah], 00370h           ; 26 c7 87 ca 01 70 03
    mov byte [es:bx+001c7h], 00fh             ; 26 c6 87 c7 01 0f
    xor al, al                                ; 30 c0
    mov byte [bp-008h], al                    ; 88 46 f8
    mov byte [bp-014h], al                    ; 88 46 ec
    mov byte [bp-00ch], al                    ; 88 46 f4
    jmp near 02591h                           ; e9 b7 05
    mov ax, 000a0h                            ; b8 a0 00
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea cx, [bx+002h]                         ; 8d 4f 02
    mov AL, strict byte 055h                  ; b0 55
    mov dx, cx                                ; 89 ca
    out DX, AL                                ; ee
    lea di, [bx+003h]                         ; 8d 7f 03
    mov AL, strict byte 0aah                  ; b0 aa
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    out DX, AL                                ; ee
    mov AL, strict byte 055h                  ; b0 55
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    out DX, AL                                ; ee
    mov AL, strict byte 0aah                  ; b0 aa
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    mov dx, cx                                ; 89 ca
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp cl, 055h                              ; 80 f9 55
    jne short 0205dh                          ; 75 4b
    cmp AL, strict byte 0aah                  ; 3c aa
    jne short 0205dh                          ; 75 47
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov byte [es:di+01eh], 001h               ; 26 c6 45 1e 01
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    call 01c2dh                               ; e8 f9 fb
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    je short 0203fh                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 02042h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    lea dx, [bx+003h]                         ; 8d 57 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp cl, 001h                              ; 80 f9 01
    jne short 0209fh                          ; 75 46
    cmp al, cl                                ; 38 c8
    je short 0205fh                           ; 74 02
    jmp short 0209fh                          ; eb 40
    lea dx, [bx+004h]                         ; 8d 57 04
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cx, ax                                ; 89 c1
    mov ch, al                                ; 88 c5
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov al, byte [bp-01ah]                    ; 8a 46 e6
    mov byte [bp-002h], al                    ; 88 46 fe
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp cl, 014h                              ; 80 f9 14
    jne short 020a1h                          ; 75 1e
    cmp byte [bp-01ah], 0ebh                  ; 80 7e e6 eb
    jne short 020a1h                          ; 75 18
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 003h               ; 26 c6 47 1e 03
    jmp short 020e7h                          ; eb 46
    test ch, ch                               ; 84 ed
    jne short 020c7h                          ; 75 22
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 020c7h                          ; 75 1c
    test al, al                               ; 84 c0
    je short 020c7h                           ; 74 18
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 002h               ; 26 c6 47 1e 02
    jmp short 020e7h                          ; eb 20
    cmp ch, 0ffh                              ; 80 fd ff
    jne short 020e7h                          ; 75 1b
    cmp ch, byte [bp-002h]                    ; 3a 6e fe
    jne short 020e7h                          ; 75 16
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01eh], 000h               ; 26 c6 47 1e 00
    mov dx, word [bp-030h]                    ; 8b 56 d0
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov al, byte [es:bx+01eh]                 ; 26 8a 47 1e
    mov byte [bp-00eh], al                    ; 88 46 f2
    cmp AL, strict byte 002h                  ; 3c 02
    jne short 02166h                          ; 75 5a
    mov byte [es:bx+01fh], 0ffh               ; 26 c6 47 1f ff
    mov byte [es:bx+022h], 000h               ; 26 c6 47 22 00
    lea dx, [bp-00262h]                       ; 8d 96 9e fd
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov [es:si+006h], ss                      ; 26 8c 54 06
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov byte [es:si+008h], al                 ; 26 88 44 08
    mov cx, strict word 00001h                ; b9 01 00
    mov bx, 000ech                            ; bb ec 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 01d11h                               ; e8 db fb
    test ax, ax                               ; 85 c0
    je short 02148h                           ; 74 0e
    mov ax, strict word 0006ch                ; b8 6c 00
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 b5 f7
    add sp, strict byte 00004h                ; 83 c4 04
    test byte [bp-00262h], 080h               ; f6 86 9e fd 80
    je short 02154h                           ; 74 05
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 02156h                          ; eb 02
    xor ax, ax                                ; 31 c0
    mov byte [bp-018h], al                    ; 88 46 e8
    mov al, byte [bp-00202h]                  ; 8a 86 fe fd
    test al, al                               ; 84 c0
    je short 02169h                           ; 74 08
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 0216bh                          ; eb 05
    jmp near 0232fh                           ; e9 c6 01
    xor ah, ah                                ; 30 e4
    mov byte [bp-012h], al                    ; 88 46 ee
    mov word [bp-032h], 00200h                ; c7 46 ce 00 02
    mov ax, word [bp-00260h]                  ; 8b 86 a0 fd
    mov word [bp-024h], ax                    ; 89 46 dc
    mov ax, word [bp-0025ch]                  ; 8b 86 a4 fd
    mov word [bp-02eh], ax                    ; 89 46 d2
    mov ax, word [bp-00256h]                  ; 8b 86 aa fd
    mov word [bp-028h], ax                    ; 89 46 d8
    mov ax, word [bp-001eah]                  ; 8b 86 16 fe
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [bp-001e8h]                  ; 8b 86 18 fe
    mov word [bp-02ch], ax                    ; 89 46 d4
    cmp ax, 00fffh                            ; 3d ff 0f
    jne short 021afh                          ; 75 14
    cmp word [bp-01ch], strict byte 0ffffh    ; 83 7e e4 ff
    jne short 021afh                          ; 75 0e
    mov ax, word [bp-0019ah]                  ; 8b 86 66 fe
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [bp-00198h]                  ; 8b 86 68 fe
    mov word [bp-02ch], ax                    ; 89 46 d4
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    cmp AL, strict byte 007h                  ; 3c 07
    jnbe short 02213h                         ; 77 5d
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    sal bx, 1                                 ; d1 e3
    jmp word [cs:bx+01f69h]                   ; 2e ff a7 69 1f
    mov BL, strict byte 01eh                  ; b3 1e
    mov al, bl                                ; 88 d8
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 90 f4
    mov dh, al                                ; 88 c6
    xor dl, dl                                ; 30 d2
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 85 f4
    xor ah, ah                                ; 30 e4
    mov di, ax                                ; 89 c7
    add di, dx                                ; 01 d7
    mov al, bl                                ; 88 d8
    add AL, strict byte 002h                  ; 04 02
    call 0165ch                               ; e8 78 f4
    xor ah, ah                                ; 30 e4
    mov word [bp-02ah], ax                    ; 89 46 d6
    mov al, bl                                ; 88 d8
    add AL, strict byte 007h                  ; 04 07
    call 0165ch                               ; e8 6c f4
    xor ah, ah                                ; 30 e4
    mov word [bp-020h], ax                    ; 89 46 e0
    jmp short 0221bh                          ; eb 24
    mov BL, strict byte 026h                  ; b3 26
    jmp short 021c3h                          ; eb c8
    mov BL, strict byte 067h                  ; b3 67
    jmp short 021c3h                          ; eb c4
    mov BL, strict byte 070h                  ; b3 70
    jmp short 021c3h                          ; eb c0
    mov BL, strict byte 040h                  ; b3 40
    jmp short 021c3h                          ; eb bc
    mov BL, strict byte 048h                  ; b3 48
    jmp short 021c3h                          ; eb b8
    mov BL, strict byte 050h                  ; b3 50
    jmp short 021c3h                          ; eb b4
    mov BL, strict byte 058h                  ; b3 58
    jmp short 021c3h                          ; eb b0
    xor di, di                                ; 31 ff
    mov word [bp-02ah], di                    ; 89 7e d6
    mov word [bp-020h], di                    ; 89 7e e0
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 97 f6
    push word [bp-020h]                       ; ff 76 e0
    push word [bp-02ah]                       ; ff 76 d6
    push di                                   ; 57
    push word [bp-028h]                       ; ff 76 d8
    push word [bp-02eh]                       ; ff 76 d2
    push word [bp-024h]                       ; ff 76 dc
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp-010h]                    ; 8a 46 f0
    push ax                                   ; 50
    mov ax, 00095h                            ; b8 95 00
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 af f6
    add sp, strict byte 00014h                ; 83 c4 14
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01fh], 0ffh               ; 26 c6 47 1f ff
    mov al, byte [bp-018h]                    ; 8a 46 e8
    mov byte [es:bx+020h], al                 ; 26 88 47 20
    mov al, byte [bp-012h]                    ; 8a 46 ee
    mov byte [es:bx+022h], al                 ; 26 88 47 22
    mov ax, word [bp-032h]                    ; 8b 46 ce
    mov word [es:bx+024h], ax                 ; 26 89 47 24
    mov ax, word [bp-02eh]                    ; 8b 46 d2
    mov word [es:bx+02ch], ax                 ; 26 89 47 2c
    mov ax, word [bp-024h]                    ; 8b 46 dc
    mov word [es:bx+02eh], ax                 ; 26 89 47 2e
    mov ax, word [bp-028h]                    ; 8b 46 d8
    mov word [es:bx+030h], ax                 ; 26 89 47 30
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov word [es:bx+032h], ax                 ; 26 89 47 32
    mov ax, word [bp-02ch]                    ; 8b 46 d4
    mov word [es:bx+034h], ax                 ; 26 89 47 34
    mov ax, word [bp-02ah]                    ; 8b 46 d6
    mov word [es:bx+026h], ax                 ; 26 89 47 26
    mov word [es:bx+028h], di                 ; 26 89 7f 28
    mov ax, word [bp-020h]                    ; 8b 46 e0
    mov word [es:bx+02ah], ax                 ; 26 89 47 2a
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    cmp AL, strict byte 002h                  ; 3c 02
    jnc short 0231ah                          ; 73 65
    test al, al                               ; 84 c0
    jne short 022beh                          ; 75 05
    mov bx, strict word 0003dh                ; bb 3d 00
    jmp short 022c1h                          ; eb 03
    mov bx, strict word 0004dh                ; bb 4d 00
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    mov word [bp-026h], ax                    ; 89 46 da
    mov es, [bp-026h]                         ; 8e 46 da
    mov word [es:bx], di                      ; 26 89 3f
    mov al, byte [bp-02ah]                    ; 8a 46 d6
    mov byte [es:bx+002h], al                 ; 26 88 47 02
    mov byte [es:bx+003h], 0a0h               ; 26 c6 47 03 a0
    mov al, byte [bp-028h]                    ; 8a 46 d8
    mov byte [es:bx+004h], al                 ; 26 88 47 04
    mov ax, word [bp-024h]                    ; 8b 46 dc
    mov word [es:bx+009h], ax                 ; 26 89 47 09
    mov al, byte [bp-02eh]                    ; 8a 46 d2
    mov byte [es:bx+00bh], al                 ; 26 88 47 0b
    mov al, byte [bp-028h]                    ; 8a 46 d8
    mov byte [es:bx+00eh], al                 ; 26 88 47 0e
    xor cl, cl                                ; 30 c9
    xor al, al                                ; 30 c0
    jmp short 022ffh                          ; eb 04
    cmp AL, strict byte 00fh                  ; 3c 0f
    jnc short 02311h                          ; 73 12
    mov dl, al                                ; 88 c2
    xor dh, dh                                ; 30 f6
    mov es, [bp-026h]                         ; 8e 46 da
    mov di, bx                                ; 89 df
    add di, dx                                ; 01 d7
    add cl, byte [es:di]                      ; 26 02 0d
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    jmp short 022fbh                          ; eb ea
    neg cl                                    ; f6 d9
    mov es, [bp-026h]                         ; 8e 46 da
    mov byte [es:bx+00fh], cl                 ; 26 88 4f 0f
    mov bl, byte [bp-014h]                    ; 8a 5e ec
    xor bh, bh                                ; 30 ff
    mov es, [bp-022h]                         ; 8e 46 de
    add bx, si                                ; 01 f3
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov byte [es:bx+0019fh], al               ; 26 88 87 9f 01
    inc byte [bp-014h]                        ; fe 46 ec
    cmp byte [bp-00eh], 003h                  ; 80 7e f2 03
    jne short 02397h                          ; 75 62
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov byte [es:bx+01fh], 005h               ; 26 c6 47 1f 05
    mov byte [es:bx+022h], 000h               ; 26 c6 47 22 00
    lea dx, [bp-00262h]                       ; 8d 96 9e fd
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov [es:si+006h], ss                      ; 26 8c 54 06
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov byte [es:si+008h], al                 ; 26 88 44 08
    mov cx, strict word 00001h                ; b9 01 00
    mov bx, 000a1h                            ; bb a1 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 01d11h                               ; e8 a1 f9
    test ax, ax                               ; 85 c0
    je short 02382h                           ; 74 0e
    mov ax, 000bch                            ; b8 bc 00
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 7b f5
    add sp, strict byte 00004h                ; 83 c4 04
    mov al, byte [bp-00261h]                  ; 8a 86 9f fd
    and AL, strict byte 01fh                  ; 24 1f
    mov byte [bp-016h], al                    ; 88 46 ea
    test byte [bp-00262h], 080h               ; f6 86 9e fd 80
    je short 02399h                           ; 74 07
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 0239bh                          ; eb 04
    jmp short 023e7h                          ; eb 4e
    xor ax, ax                                ; 31 c0
    mov byte [bp-004h], al                    ; 88 46 fc
    cmp byte [bp-00202h], 000h                ; 80 be fe fd 00
    je short 023aah                           ; 74 05
    mov cx, strict word 00001h                ; b9 01 00
    jmp short 023ach                          ; eb 02
    xor cx, cx                                ; 31 c9
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov al, byte [bp-016h]                    ; 8a 46 ea
    mov byte [es:bx+01fh], al                 ; 26 88 47 1f
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [es:bx+020h], al                 ; 26 88 47 20
    mov byte [es:bx+022h], cl                 ; 26 88 4f 22
    mov word [es:bx+024h], 00800h             ; 26 c7 47 24 00 08
    mov bl, byte [bp-008h]                    ; 8a 5e f8
    xor bh, bh                                ; 30 ff
    add bx, si                                ; 01 f3
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov byte [es:bx+001b0h], al               ; 26 88 87 b0 01
    inc byte [bp-008h]                        ; fe 46 f8
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    cmp AL, strict byte 003h                  ; 3c 03
    je short 0241ch                           ; 74 2e
    cmp AL, strict byte 002h                  ; 3c 02
    jne short 0245dh                          ; 75 6b
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [es:bx+032h]                 ; 26 8b 47 32
    mov word [bp-038h], ax                    ; 89 46 c8
    mov ax, word [es:bx+034h]                 ; 26 8b 47 34
    mov word [bp-036h], ax                    ; 89 46 ca
    mov cx, strict word 0000bh                ; b9 0b 00
    shr word [bp-036h], 1                     ; d1 6e ca
    rcr word [bp-038h], 1                     ; d1 5e c8
    loop 02414h                               ; e2 f8
    mov ah, byte [bp-001c1h]                  ; 8a a6 3f fe
    mov al, byte [bp-001c2h]                  ; 8a 86 3e fe
    mov byte [bp-006h], 00fh                  ; c6 46 fa 0f
    jmp short 02433h                          ; eb 09
    dec byte [bp-006h]                        ; fe 4e fa
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jbe short 0243fh                          ; 76 0c
    mov cl, byte [bp-006h]                    ; 8a 4e fa
    mov dx, strict word 00001h                ; ba 01 00
    sal dx, CL                                ; d3 e2
    test ax, dx                               ; 85 d0
    je short 0242ah                           ; 74 eb
    xor bx, bx                                ; 31 db
    jmp short 02448h                          ; eb 05
    cmp bx, strict byte 00014h                ; 83 fb 14
    jnl short 0245fh                          ; 7d 17
    mov di, bx                                ; 89 df
    sal di, 1                                 ; d1 e7
    mov al, byte [bp+di-0022bh]               ; 8a 83 d5 fd
    mov byte [bp+di-062h], al                 ; 88 43 9e
    mov al, byte [bp+di-0022ch]               ; 8a 83 d4 fd
    mov byte [bp+di-061h], al                 ; 88 43 9f
    inc bx                                    ; 43
    jmp short 02443h                          ; eb e6
    jmp short 0247bh                          ; eb 1c
    mov byte [bp-03ah], 000h                  ; c6 46 c6 00
    mov bx, strict word 00027h                ; bb 27 00
    jmp short 0246dh                          ; eb 05
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jle short 0247bh                          ; 7e 0e
    mov di, bx                                ; 89 df
    cmp byte [bp+di-062h], 020h               ; 80 7b 9e 20
    jne short 0247bh                          ; 75 06
    mov byte [bp+di-062h], 000h               ; c6 43 9e 00
    jmp short 02468h                          ; eb ed
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    cmp AL, strict byte 003h                  ; 3c 03
    je short 024eah                           ; 74 68
    cmp AL, strict byte 002h                  ; 3c 02
    je short 0248dh                           ; 74 07
    cmp AL, strict byte 001h                  ; 3c 01
    je short 024f5h                           ; 74 6b
    jmp near 02588h                           ; e9 fb 00
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    je short 02498h                           ; 74 05
    mov ax, 000e7h                            ; b8 e7 00
    jmp short 0249bh                          ; eb 03
    mov ax, 000eeh                            ; b8 ee 00
    push ax                                   ; 50
    mov al, byte [bp-010h]                    ; 8a 46 f0
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 000f5h                            ; b8 f5 00
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 4d f4
    add sp, strict byte 00008h                ; 83 c4 08
    xor bx, bx                                ; 31 db
    mov di, bx                                ; 89 df
    mov al, byte [bp+di-062h]                 ; 8a 43 9e
    xor ah, ah                                ; 30 e4
    inc bx                                    ; 43
    test ax, ax                               ; 85 c0
    je short 024cfh                           ; 74 11
    push ax                                   ; 50
    mov ax, 00100h                            ; b8 00 01
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 30 f4
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 024b2h                          ; eb e3
    push word [bp-036h]                       ; ff 76 ca
    push word [bp-038h]                       ; ff 76 c8
    mov al, byte [bp-006h]                    ; 8a 46 fa
    push ax                                   ; 50
    mov ax, 00103h                            ; b8 03 01
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 16 f4
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 02588h                           ; e9 9e 00
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    je short 024f7h                           ; 74 07
    mov ax, 000e7h                            ; b8 e7 00
    jmp short 024fah                          ; eb 05
    jmp short 02565h                          ; eb 6e
    mov ax, 000eeh                            ; b8 ee 00
    push ax                                   ; 50
    mov al, byte [bp-010h]                    ; 8a 46 f0
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 000f5h                            ; b8 f5 00
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 ee f3
    add sp, strict byte 00008h                ; 83 c4 08
    xor bx, bx                                ; 31 db
    mov di, bx                                ; 89 df
    mov al, byte [bp+di-062h]                 ; 8a 43 9e
    xor ah, ah                                ; 30 e4
    inc bx                                    ; 43
    test ax, ax                               ; 85 c0
    je short 0252eh                           ; 74 11
    push ax                                   ; 50
    mov ax, 00100h                            ; b8 00 01
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 d1 f3
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 02511h                          ; eb e3
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    cmp byte [es:bx+01fh], 005h               ; 26 80 7f 1f 05
    jne short 0254fh                          ; 75 0b
    mov al, byte [bp-006h]                    ; 8a 46 fa
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 00123h                            ; b8 23 01
    jmp short 02558h                          ; eb 09
    mov al, byte [bp-006h]                    ; 8a 46 fa
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 0013dh                            ; b8 3d 01
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 9a f3
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 02588h                          ; eb 23
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    je short 02570h                           ; 74 05
    mov ax, 000e7h                            ; b8 e7 00
    jmp short 02573h                          ; eb 03
    mov ax, 000eeh                            ; b8 ee 00
    push ax                                   ; 50
    mov al, byte [bp-010h]                    ; 8a 46 f0
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 0014fh                            ; b8 4f 01
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 75 f3
    add sp, strict byte 00008h                ; 83 c4 08
    inc byte [bp-00ch]                        ; fe 46 f4
    cmp byte [bp-00ch], 008h                  ; 80 7e f4 08
    jnc short 025e7h                          ; 73 56
    mov bl, byte [bp-00ch]                    ; 8a 5e f4
    xor bh, bh                                ; 30 ff
    mov ax, bx                                ; 89 d8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov cx, ax                                ; 89 c1
    mov byte [bp-010h], al                    ; 88 46 f0
    mov ax, bx                                ; 89 d8
    cwd                                       ; 99
    mov bx, strict word 00002h                ; bb 02 00
    idiv bx                                   ; f7 fb
    mov word [bp-034h], dx                    ; 89 56 cc
    mov al, byte [bp-034h]                    ; 8a 46 cc
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    mov es, [bp-022h]                         ; 8e 46 de
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov bx, word [es:di+001c2h]               ; 26 8b 9d c2 01
    mov ax, word [es:di+001c4h]               ; 26 8b 85 c4 01
    mov word [bp-030h], ax                    ; 89 46 d0
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    cmp byte [bp-034h], 000h                  ; 80 7e cc 00
    jne short 025e1h                          ; 75 03
    jmp near 01fdah                           ; e9 f9 f9
    mov ax, 000b0h                            ; b8 b0 00
    jmp near 01fddh                           ; e9 f6 f9
    mov al, byte [bp-014h]                    ; 8a 46 ec
    mov es, [bp-022h]                         ; 8e 46 de
    mov byte [es:si+0019eh], al               ; 26 88 84 9e 01
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov byte [es:si+001afh], al               ; 26 88 84 af 01
    mov bl, byte [bp-014h]                    ; 8a 5e ec
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 06 f0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ata_cmd_data_out_:                           ; 0xf260e LB 0x21d
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0001ah                ; 83 ec 1a
    mov di, ax                                ; 89 c7
    mov word [bp-016h], dx                    ; 89 56 ea
    mov word [bp-012h], bx                    ; 89 5e ee
    mov word [bp-014h], cx                    ; 89 4e ec
    mov es, dx                                ; 8e c2
    mov cl, byte [es:di+008h]                 ; 26 8a 4d 08
    xor ch, ch                                ; 30 ed
    mov ax, cx                                ; 89 c8
    mov dx, cx                                ; 89 ca
    shr dx, 1                                 ; d1 ea
    and AL, strict byte 001h                  ; 24 01
    mov byte [bp-004h], al                    ; 88 46 fc
    mov al, dl                                ; 88 d0
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    mov si, di                                ; 89 fe
    add si, ax                                ; 01 c6
    mov bx, word [es:si+001c2h]               ; 26 8b 9c c2 01
    mov ax, word [es:si+001c4h]               ; 26 8b 84 c4 01
    mov word [bp-006h], ax                    ; 89 46 fa
    mov ax, cx                                ; 89 c8
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov si, di                                ; 89 fe
    add si, ax                                ; 01 c6
    mov al, byte [es:si+022h]                 ; 26 8a 44 22
    mov byte [bp-002h], al                    ; 88 46 fe
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02669h                          ; 75 07
    mov word [bp-00ch], 00080h                ; c7 46 f4 80 00
    jmp short 0266eh                          ; eb 05
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 02687h                           ; 74 0f
    mov dx, word [bp-006h]                    ; 8b 56 fa
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 02823h                           ; e9 9c 01
    mov es, [bp-016h]                         ; 8e 46 ea
    mov ax, word [es:di]                      ; 26 8b 05
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    mov word [bp-018h], ax                    ; 89 46 e8
    mov si, word [es:di+004h]                 ; 26 8b 75 04
    mov ax, word [es:di+006h]                 ; 26 8b 45 06
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, word [es:di+012h]                 ; 26 8b 45 12
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [es:di+00eh]                 ; 26 8b 45 0e
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [es:di+010h]                 ; 26 8b 45 10
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    test ax, ax                               ; 85 c0
    jne short 02721h                          ; 75 63
    mov dx, word [bp-01ah]                    ; 8b 56 e6
    add dx, word [bp-014h]                    ; 03 56 ec
    adc ax, word [bp-018h]                    ; 13 46 e8
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 026ceh                         ; 77 02
    jne short 026f5h                          ; 75 27
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov al, ah                                ; 88 e0
    xor ah, ah                                ; 30 e4
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov al, ah                                ; 88 e0
    lea dx, [bx+002h]                         ; 8d 57 02
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    mov byte [bp-017h], al                    ; 88 46 e9
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    xor ah, ah                                ; 30 e4
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov cx, strict word 00008h                ; b9 08 00
    shr word [bp-018h], 1                     ; d1 6e e8
    rcr word [bp-01ah], 1                     ; d1 5e e6
    loop 02700h                               ; e2 f8
    mov ax, word [bp-01ah]                    ; 8b 46 e6
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-018h], strict word 00000h    ; c7 46 e8 00 00
    and ax, strict word 0000fh                ; 25 0f 00
    or AL, strict byte 040h                   ; 0c 40
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov dx, word [bp-006h]                    ; 8b 56 fa
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    mov al, byte [bp-014h]                    ; 8a 46 ec
    out DX, AL                                ; ee
    lea dx, [bx+003h]                         ; 8d 57 03
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    out DX, AL                                ; ee
    mov ax, word [bp-010h]                    ; 8b 46 f0
    lea dx, [bx+004h]                         ; 8d 57 04
    out DX, AL                                ; ee
    mov al, byte [bp-00fh]                    ; 8a 46 f1
    lea dx, [bx+005h]                         ; 8d 57 05
    out DX, AL                                ; ee
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 02757h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 0275ah                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    mov dl, byte [bp-00eh]                    ; 8a 56 f2
    xor dh, dh                                ; 30 f6
    or ax, dx                                 ; 09 d0
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    mov al, byte [bp-012h]                    ; 8a 46 ee
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 0276ch                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 0278bh                           ; 74 0f
    mov dx, word [bp-006h]                    ; 8b 56 fa
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 02823h                           ; e9 98 00
    test dl, 008h                             ; f6 c2 08
    jne short 0279fh                          ; 75 0f
    mov dx, word [bp-006h]                    ; 8b 56 fa
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 02823h                           ; e9 84 00
    sti                                       ; fb
    cmp si, 0f800h                            ; 81 fe 00 f8
    jc short 027b3h                           ; 72 0d
    sub si, 00800h                            ; 81 ee 00 08
    mov ax, word [bp-008h]                    ; 8b 46 f8
    add ax, 00080h                            ; 05 80 00
    mov word [bp-008h], ax                    ; 89 46 f8
    cmp byte [bp-002h], 001h                  ; 80 7e fe 01
    jne short 027c7h                          ; 75 0e
    mov dx, bx                                ; 89 da
    mov cx, word [bp-00ch]                    ; 8b 4e f4
    mov es, [bp-008h]                         ; 8e 46 f8
    db  0f3h, 066h, 026h, 06fh
    ; rep es outsd                              ; f3 66 26 6f
    jmp short 027d2h                          ; eb 0b
    mov dx, bx                                ; 89 da
    mov cx, word [bp-00ch]                    ; 8b 4e f4
    mov es, [bp-008h]                         ; 8e 46 f8
    db  0f3h, 026h, 06fh
    ; rep es outsw                              ; f3 26 6f
    mov es, [bp-016h]                         ; 8e 46 ea
    inc word [es:di+014h]                     ; 26 ff 45 14
    dec word [bp-014h]                        ; ff 4e ec
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 027dch                          ; 75 f4
    cmp word [bp-014h], strict byte 00000h    ; 83 7e ec 00
    jne short 02802h                          ; 75 14
    and AL, strict byte 0e9h                  ; 24 e9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02818h                           ; 74 24
    mov dx, word [bp-006h]                    ; 8b 56 fa
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00006h                ; ba 06 00
    jmp short 02823h                          ; eb 21
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 027a0h                           ; 74 96
    mov dx, word [bp-006h]                    ; 8b 56 fa
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00007h                ; ba 07 00
    jmp short 02823h                          ; eb 0b
    mov dx, word [bp-006h]                    ; 8b 56 fa
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
@ata_read_sectors:                           ; 0xf282b LB 0x76
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    mov si, word [bp+008h]                    ; 8b 76 08
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov al, byte [es:si+008h]                 ; 26 8a 44 08
    mov bx, word [es:si+00ah]                 ; 26 8b 5c 0a
    mov CL, strict byte 009h                  ; b1 09
    mov dx, bx                                ; 89 da
    sal dx, CL                                ; d3 e2
    mov cx, dx                                ; 89 d1
    cmp word [es:si+012h], strict byte 00000h ; 26 83 7c 12 00
    jne short 02872h                          ; 75 24
    xor di, di                                ; 31 ff
    mov dx, word [es:si]                      ; 26 8b 14
    add dx, bx                                ; 01 da
    mov word [bp-002h], dx                    ; 89 56 fe
    adc di, word [es:si+002h]                 ; 26 13 7c 02
    cmp di, 01000h                            ; 81 ff 00 10
    jnbe short 02864h                         ; 77 02
    jne short 02872h                          ; 75 0e
    mov cx, bx                                ; 89 d9
    mov bx, strict word 00024h                ; bb 24 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 01d11h                               ; e8 a1 f4
    jmp short 02899h                          ; eb 27
    xor ah, ah                                ; 30 e4
    mov di, strict word 00018h                ; bf 18 00
    imul di                                   ; f7 ef
    mov dx, es                                ; 8c c2
    mov [bp-002h], es                         ; 8c 46 fe
    mov di, si                                ; 89 f7
    add di, ax                                ; 01 c7
    mov word [es:di+024h], cx                 ; 26 89 4d 24
    mov cx, bx                                ; 89 d9
    mov bx, 000c4h                            ; bb c4 00
    mov ax, si                                ; 89 f0
    call 01d11h                               ; e8 81 f4
    mov es, [bp-002h]                         ; 8e 46 fe
    mov word [es:di+024h], 00200h             ; 26 c7 45 24 00 02
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
@ata_write_sectors:                          ; 0xf28a1 LB 0x3a
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    les si, [bp+006h]                         ; c4 76 06
    mov cx, word [es:si+00ah]                 ; 26 8b 4c 0a
    cmp word [es:si+012h], strict byte 00000h ; 26 83 7c 12 00
    je short 028bfh                           ; 74 0c
    mov bx, strict word 00030h                ; bb 30 00
    mov ax, si                                ; 89 f0
    mov dx, es                                ; 8c c2
    call 0260eh                               ; e8 51 fd
    jmp short 028d6h                          ; eb 17
    xor ax, ax                                ; 31 c0
    mov dx, word [es:si]                      ; 26 8b 14
    add dx, cx                                ; 01 ca
    adc ax, word [es:si+002h]                 ; 26 13 44 02
    cmp ax, 01000h                            ; 3d 00 10
    jnbe short 028d1h                         ; 77 02
    jne short 028b3h                          ; 75 e2
    mov bx, strict word 00034h                ; bb 34 00
    jmp short 028b6h                          ; eb e0
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
ata_cmd_packet_:                             ; 0xf28db LB 0x2ef
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00014h                ; 83 ec 14
    push ax                                   ; 50
    mov byte [bp-004h], dl                    ; 88 56 fc
    mov di, bx                                ; 89 df
    mov word [bp-010h], cx                    ; 89 4e f0
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 27 ed
    mov word [bp-00ah], 00122h                ; c7 46 f6 22 01
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, word [bp-016h]                    ; 8b 46 ea
    shr ax, 1                                 ; d1 e8
    mov cl, byte [bp-016h]                    ; 8a 4e ea
    and cl, 001h                              ; 80 e1 01
    cmp byte [bp+00eh], 002h                  ; 80 7e 0e 02
    jne short 02931h                          ; 75 23
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 a4 ef
    mov ax, 00169h                            ; b8 69 01
    push ax                                   ; 50
    mov ax, 00178h                            ; b8 78 01
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 d2 ef
    add sp, strict byte 00006h                ; 83 c4 06
    mov dx, strict word 00001h                ; ba 01 00
    jmp near 02bc0h                           ; e9 8f 02
    test byte [bp+008h], 001h                 ; f6 46 08 01
    jne short 0292bh                          ; 75 f4
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    les si, [bp-00ah]                         ; c4 76 f6
    add si, ax                                ; 01 c6
    mov bx, word [es:si+001c2h]               ; 26 8b 9c c2 01
    mov ax, word [es:si+001c4h]               ; 26 8b 84 c4 01
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov si, word [bp-00ah]                    ; 8b 76 f6
    add si, ax                                ; 01 c6
    mov al, byte [es:si+022h]                 ; 26 8a 44 22
    mov byte [bp-002h], al                    ; 88 46 fe
    xor ax, ax                                ; 31 c0
    mov word [bp-012h], ax                    ; 89 46 ee
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov al, byte [bp-004h]                    ; 8a 46 fc
    cmp AL, strict byte 00ch                  ; 3c 0c
    jnc short 02979h                          ; 73 06
    mov byte [bp-004h], 00ch                  ; c6 46 fc 0c
    jmp short 0297fh                          ; eb 06
    jbe short 0297fh                          ; 76 04
    mov byte [bp-004h], 010h                  ; c6 46 fc 10
    shr byte [bp-004h], 1                     ; d0 6e fc
    les si, [bp-00ah]                         ; c4 76 f6
    mov word [es:si+014h], strict word 00000h ; 26 c7 44 14 00 00
    mov word [es:si+016h], strict word 00000h ; 26 c7 44 16 00 00
    mov word [es:si+018h], strict word 00000h ; 26 c7 44 18 00 00
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 080h                 ; a8 80
    je short 029a7h                           ; 74 06
    mov dx, strict word 00002h                ; ba 02 00
    jmp near 02bc0h                           ; e9 19 02
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 00ah                  ; b0 0a
    out DX, AL                                ; ee
    lea dx, [bx+004h]                         ; 8d 57 04
    mov AL, strict byte 0f0h                  ; b0 f0
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    test cl, cl                               ; 84 c9
    je short 029c5h                           ; 74 05
    mov ax, 000b0h                            ; b8 b0 00
    jmp short 029c8h                          ; eb 03
    mov ax, 000a0h                            ; b8 a0 00
    lea dx, [bx+006h]                         ; 8d 57 06
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    mov AL, strict byte 0a0h                  ; b0 a0
    out DX, AL                                ; ee
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 029d2h                          ; 75 f4
    test AL, strict byte 001h                 ; a8 01
    je short 029f1h                           ; 74 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00003h                ; ba 03 00
    jmp near 02bc0h                           ; e9 cf 01
    test dl, 008h                             ; f6 c2 08
    jne short 02a05h                          ; 75 0f
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, strict word 00004h                ; ba 04 00
    jmp near 02bc0h                           ; e9 bb 01
    sti                                       ; fb
    mov CL, strict byte 004h                  ; b1 04
    mov ax, di                                ; 89 f8
    shr ax, CL                                ; d3 e8
    add ax, word [bp-010h]                    ; 03 46 f0
    mov si, di                                ; 89 fe
    and si, strict byte 0000fh                ; 83 e6 0f
    mov cl, byte [bp-004h]                    ; 8a 4e fc
    xor ch, ch                                ; 30 ed
    mov dx, bx                                ; 89 da
    mov es, ax                                ; 8e c0
    db  0f3h, 026h, 06fh
    ; rep es outsw                              ; f3 26 6f
    cmp byte [bp+00eh], 000h                  ; 80 7e 0e 00
    jne short 02a31h                          ; 75 0b
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    jmp near 02ba1h                           ; e9 70 01
    lea dx, [bx+007h]                         ; 8d 57 07
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov dl, al                                ; 88 c2
    test AL, strict byte 080h                 ; a8 80
    jne short 02a31h                          ; 75 f4
    test AL, strict byte 088h                 ; a8 88
    je short 02aa1h                           ; 74 60
    test AL, strict byte 001h                 ; a8 01
    je short 02a50h                           ; 74 0b
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp short 029ebh                          ; eb 9b
    mov al, dl                                ; 88 d0
    and AL, strict byte 0c9h                  ; 24 c9
    cmp AL, strict byte 048h                  ; 3c 48
    je short 02a63h                           ; 74 0b
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp short 029ffh                          ; eb 9c
    mov CL, strict byte 004h                  ; b1 04
    mov ax, word [bp+010h]                    ; 8b 46 10
    shr ax, CL                                ; d3 e8
    add ax, word [bp+012h]                    ; 03 46 12
    mov dx, word [bp+010h]                    ; 8b 56 10
    and dx, strict byte 0000fh                ; 83 e2 0f
    mov word [bp+010h], dx                    ; 89 56 10
    mov word [bp+012h], ax                    ; 89 46 12
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov ch, al                                ; 88 c5
    xor cl, cl                                ; 30 c9
    lea dx, [bx+004h]                         ; 8d 57 04
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    add cx, ax                                ; 01 c1
    mov word [bp-006h], cx                    ; 89 4e fa
    mov ax, word [bp+008h]                    ; 8b 46 08
    cmp ax, cx                                ; 39 c8
    jbe short 02aa4h                          ; 76 0f
    mov ax, cx                                ; 89 c8
    sub word [bp+008h], cx                    ; 29 4e 08
    xor ax, cx                                ; 31 c8
    mov word [bp-006h], ax                    ; 89 46 fa
    jmp short 02aaeh                          ; eb 0d
    jmp near 02ba1h                           ; e9 fd 00
    mov cx, ax                                ; 89 c1
    mov word [bp+008h], strict word 00000h    ; c7 46 08 00 00
    sub word [bp-006h], ax                    ; 29 46 fa
    xor ax, ax                                ; 31 c0
    cmp word [bp+00ch], strict byte 00000h    ; 83 7e 0c 00
    jne short 02ad7h                          ; 75 21
    mov dx, word [bp-006h]                    ; 8b 56 fa
    cmp dx, word [bp+00ah]                    ; 3b 56 0a
    jbe short 02ad7h                          ; 76 19
    mov ax, word [bp-006h]                    ; 8b 46 fa
    sub ax, word [bp+00ah]                    ; 2b 46 0a
    mov word [bp-014h], ax                    ; 89 46 ec
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    mov word [bp-006h], ax                    ; 89 46 fa
    xor ax, ax                                ; 31 c0
    mov word [bp+00ah], ax                    ; 89 46 0a
    mov word [bp+00ch], ax                    ; 89 46 0c
    jmp short 02ae3h                          ; eb 0c
    mov word [bp-014h], ax                    ; 89 46 ec
    mov dx, word [bp-006h]                    ; 8b 56 fa
    sub word [bp+00ah], dx                    ; 29 56 0a
    sbb word [bp+00ch], ax                    ; 19 46 0c
    mov si, word [bp-006h]                    ; 8b 76 fa
    mov al, byte [bp-002h]                    ; 8a 46 fe
    test cl, 003h                             ; f6 c1 03
    je short 02af0h                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-006h], 003h                 ; f6 46 fa 03
    je short 02af8h                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-014h], 003h                 ; f6 46 ec 03
    je short 02b00h                           ; 74 02
    xor al, al                                ; 30 c0
    test byte [bp-006h], 001h                 ; f6 46 fa 01
    je short 02b18h                           ; 74 12
    inc word [bp-006h]                        ; ff 46 fa
    cmp word [bp-014h], strict byte 00000h    ; 83 7e ec 00
    jbe short 02b18h                          ; 76 09
    test byte [bp-014h], 001h                 ; f6 46 ec 01
    je short 02b18h                           ; 74 03
    dec word [bp-014h]                        ; ff 4e ec
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02b2bh                          ; 75 0f
    shr word [bp-006h], 1                     ; d1 6e fa
    shr word [bp-006h], 1                     ; d1 6e fa
    shr cx, 1                                 ; d1 e9
    shr cx, 1                                 ; d1 e9
    shr word [bp-014h], 1                     ; d1 6e ec
    jmp short 02b30h                          ; eb 05
    shr word [bp-006h], 1                     ; d1 6e fa
    shr cx, 1                                 ; d1 e9
    shr word [bp-014h], 1                     ; d1 6e ec
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 02b63h                          ; 75 2c
    test cx, cx                               ; 85 c9
    je short 02b45h                           ; 74 0a
    mov dx, bx                                ; 89 da
    push eax                                  ; 66 50
    in eax, DX                                ; 66 ed
    loop 02b3fh                               ; e2 fc
    pop eax                                   ; 66 58
    mov dx, bx                                ; 89 da
    mov cx, word [bp-006h]                    ; 8b 4e fa
    les di, [bp+010h]                         ; c4 7e 10
    db  0f3h, 066h, 06dh
    ; rep insd                                  ; f3 66 6d
    mov ax, word [bp-014h]                    ; 8b 46 ec
    test ax, ax                               ; 85 c0
    je short 02b82h                           ; 74 2b
    mov cx, ax                                ; 89 c1
    push eax                                  ; 66 50
    in eax, DX                                ; 66 ed
    loop 02b5bh                               ; e2 fc
    pop eax                                   ; 66 58
    jmp short 02b82h                          ; eb 1f
    test cx, cx                               ; 85 c9
    je short 02b6ch                           ; 74 05
    mov dx, bx                                ; 89 da
    in ax, DX                                 ; ed
    loop 02b69h                               ; e2 fd
    mov dx, bx                                ; 89 da
    mov cx, word [bp-006h]                    ; 8b 4e fa
    les di, [bp+010h]                         ; c4 7e 10
    rep insw                                  ; f3 6d
    mov ax, word [bp-014h]                    ; 8b 46 ec
    test ax, ax                               ; 85 c0
    je short 02b82h                           ; 74 05
    mov cx, ax                                ; 89 c1
    in ax, DX                                 ; ed
    loop 02b7fh                               ; e2 fd
    add word [bp+010h], si                    ; 01 76 10
    xor ax, ax                                ; 31 c0
    add word [bp-012h], si                    ; 01 76 ee
    adc word [bp-00eh], ax                    ; 11 46 f2
    mov ax, word [bp-012h]                    ; 8b 46 ee
    les si, [bp-00ah]                         ; c4 76 f6
    mov word [es:si+016h], ax                 ; 26 89 44 16
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:si+018h], ax                 ; 26 89 44 18
    jmp near 02a31h                           ; e9 90 fe
    mov al, dl                                ; 88 d0
    and AL, strict byte 0e9h                  ; 24 e9
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02bb5h                           ; 74 0c
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    jmp near 029ffh                           ; e9 4a fe
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    add dx, strict byte 00006h                ; 83 c2 06
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    xor dx, dx                                ; 31 d2
    mov ax, dx                                ; 89 d0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 0000ch                               ; c2 0c 00
set_diskette_ret_status_:                    ; 0xf2bca LB 0x16
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00041h                ; ba 41 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 32 ea
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
set_diskette_current_cyl_:                   ; 0xf2be0 LB 0x33
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    cmp AL, strict byte 001h                  ; 3c 01
    jbe short 02bf9h                          ; 76 0e
    mov ax, 00198h                            ; b8 98 01
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 04 ed
    add sp, strict byte 00004h                ; 83 c4 04
    mov al, dl                                ; 88 d0
    xor ah, ah                                ; 30 e4
    mov cx, ax                                ; 89 c1
    mov al, bl                                ; 88 d8
    mov dx, ax                                ; 89 c2
    add dx, 00094h                            ; 81 c2 94 00
    mov bx, cx                                ; 89 cb
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ff e9
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_reset_controller_:                    ; 0xf2c13 LB 0x25
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, 003f2h                            ; ba f2 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bx, ax                                ; 89 c3
    and AL, strict byte 0fbh                  ; 24 fb
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    or AL, strict byte 004h                   ; 0c 04
    out DX, AL                                ; ee
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 080h                  ; 3c 80
    jne short 02c28h                          ; 75 f4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_prepare_controller_:                  ; 0xf2c38 LB 0x93
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    mov cx, ax                                ; 89 c1
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 b6 e9
    and AL, strict byte 07fh                  ; 24 7f
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 b5 e9
    mov dx, 003f2h                            ; ba f2 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 004h                  ; 24 04
    mov byte [bp-002h], al                    ; 88 46 fe
    test cx, cx                               ; 85 c9
    je short 02c6ch                           ; 74 04
    mov AL, strict byte 020h                  ; b0 20
    jmp short 02c6eh                          ; eb 02
    mov AL, strict byte 010h                  ; b0 10
    or AL, strict byte 00ch                   ; 0c 0c
    or al, cl                                 ; 08 c8
    mov dx, 003f2h                            ; ba f2 03
    out DX, AL                                ; ee
    mov bx, strict word 00025h                ; bb 25 00
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 0160eh                               ; e8 8d e9
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 76 e9
    mov CL, strict byte 006h                  ; b1 06
    shr al, CL                                ; d2 e8
    mov dx, 003f7h                            ; ba f7 03
    out DX, AL                                ; ee
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 080h                  ; 3c 80
    jne short 02c92h                          ; 75 f4
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 02cc4h                          ; 75 20
    sti                                       ; fb
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 52 e9
    test AL, strict byte 080h                 ; a8 80
    je short 02ca5h                           ; 74 f3
    and AL, strict byte 07fh                  ; 24 7f
    cli                                       ; fa
    mov cl, al                                ; 88 c1
    xor ch, ch                                ; 30 ed
    mov bx, cx                                ; 89 cb
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 4a e9
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_media_known_:                         ; 0xf2ccb LB 0x43
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 24 e9
    mov ah, al                                ; 88 c4
    test bx, bx                               ; 85 db
    je short 02ce4h                           ; 74 02
    shr al, 1                                 ; d0 e8
    and AL, strict byte 001h                  ; 24 01
    jne short 02cech                          ; 75 04
    xor ah, ah                                ; 30 e4
    jmp short 02d09h                          ; eb 1d
    mov dx, 00090h                            ; ba 90 00
    test bx, bx                               ; 85 db
    je short 02cf6h                           ; 74 03
    mov dx, 00091h                            ; ba 91 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 04 e9
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 004h                  ; b1 04
    sar ax, CL                                ; d3 f8
    and AL, strict byte 001h                  ; 24 01
    je short 02ce8h                           ; 74 e2
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_read_id_:                             ; 0xf2d0e LB 0x51
    push bx                                   ; 53
    push dx                                   ; 52
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00008h                ; 83 ec 08
    mov bx, ax                                ; 89 c3
    call 02c38h                               ; e8 1c ff
    mov AL, strict byte 04ah                  ; b0 4a
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 d1 e8
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 02d26h                           ; 74 f1
    cli                                       ; fa
    xor si, si                                ; 31 f6
    jmp short 02d3fh                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 02d4bh                          ; 7d 0c
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-008h], al                 ; 88 42 f8
    inc si                                    ; 46
    jmp short 02d3ah                          ; eb ef
    test byte [bp-008h], 0c0h                 ; f6 46 f8 c0
    je short 02d55h                           ; 74 04
    xor ax, ax                                ; 31 c0
    jmp short 02d58h                          ; eb 03
    mov ax, strict word 00001h                ; b8 01 00
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_drive_recal_:                         ; 0xf2d5f LB 0x5f
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    call 02c38h                               ; e8 ce fe
    mov AL, strict byte 007h                  ; b0 07
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 83 e8
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 02d74h                           ; 74 f1
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 73 e8
    and AL, strict byte 07fh                  ; 24 7f
    test bx, bx                               ; 85 db
    je short 02d9ah                           ; 74 07
    or AL, strict byte 002h                   ; 0c 02
    mov cx, 00095h                            ; b9 95 00
    jmp short 02d9fh                          ; eb 05
    or AL, strict byte 001h                   ; 0c 01
    mov cx, 00094h                            ; b9 94 00
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 62 e8
    xor bx, bx                                ; 31 db
    mov dx, cx                                ; 89 ca
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 58 e8
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_media_sense_:                         ; 0xf2dbe LB 0x104
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov di, ax                                ; 89 c7
    call 02d5fh                               ; e8 94 ff
    test ax, ax                               ; 85 c0
    jne short 02dd4h                          ; 75 05
    xor cx, cx                                ; 31 c9
    jmp near 02eb9h                           ; e9 e5 00
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 82 e8
    test di, di                               ; 85 ff
    jne short 02de6h                          ; 75 08
    mov CL, strict byte 004h                  ; b1 04
    shr al, CL                                ; d2 e8
    mov cl, al                                ; 88 c1
    jmp short 02debh                          ; eb 05
    mov cl, al                                ; 88 c1
    and cl, 00fh                              ; 80 e1 0f
    cmp cl, 001h                              ; 80 f9 01
    jne short 02df9h                          ; 75 09
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 015h                  ; b5 15
    mov si, strict word 00001h                ; be 01 00
    jmp short 02e44h                          ; eb 4b
    cmp cl, 002h                              ; 80 f9 02
    jne short 02e04h                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 035h                  ; b5 35
    jmp short 02df4h                          ; eb f0
    cmp cl, 003h                              ; 80 f9 03
    jne short 02e0fh                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 017h                  ; b5 17
    jmp short 02df4h                          ; eb e5
    cmp cl, 004h                              ; 80 f9 04
    jne short 02e1ah                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 017h                  ; b5 17
    jmp short 02df4h                          ; eb da
    cmp cl, 005h                              ; 80 f9 05
    jne short 02e25h                          ; 75 06
    mov CL, strict byte 0cch                  ; b1 cc
    mov CH, strict byte 0d7h                  ; b5 d7
    jmp short 02df4h                          ; eb cf
    cmp cl, 006h                              ; 80 f9 06
    jne short 02e30h                          ; 75 06
    xor cl, cl                                ; 30 c9
    mov CH, strict byte 027h                  ; b5 27
    jmp short 02df4h                          ; eb c4
    cmp cl, 007h                              ; 80 f9 07
    jne short 02e37h                          ; 75 02
    jmp short 02e2ah                          ; eb f3
    cmp cl, 008h                              ; 80 f9 08
    jne short 02e3eh                          ; 75 02
    jmp short 02e2ah                          ; eb ec
    xor cl, cl                                ; 30 c9
    xor ch, ch                                ; 30 ed
    xor si, si                                ; 31 f6
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 bb e7
    mov ax, di                                ; 89 f8
    call 02d0eh                               ; e8 b6 fe
    test ax, ax                               ; 85 c0
    jne short 02e8eh                          ; 75 32
    mov al, cl                                ; 88 c8
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 080h                  ; 3c 80
    je short 02e8eh                           ; 74 2a
    mov al, cl                                ; 88 c8
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 02e7bh                           ; 74 0f
    mov ah, cl                                ; 88 cc
    and ah, 03fh                              ; 80 e4 3f
    cmp AL, strict byte 040h                  ; 3c 40
    je short 02e87h                           ; 74 12
    test al, al                               ; 84 c0
    je short 02e80h                           ; 74 07
    jmp short 02e44h                          ; eb c9
    and cl, 03fh                              ; 80 e1 3f
    jmp short 02e44h                          ; eb c4
    mov cl, ah                                ; 88 e1
    or cl, 040h                               ; 80 c9 40
    jmp short 02e44h                          ; eb bd
    mov cl, ah                                ; 88 e1
    or cl, 080h                               ; 80 c9 80
    jmp short 02e44h                          ; eb b6
    test di, di                               ; 85 ff
    jne short 02e97h                          ; 75 05
    mov di, 00090h                            ; bf 90 00
    jmp short 02e9ah                          ; eb 03
    mov di, 00091h                            ; bf 91 00
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, 0008bh                            ; ba 8b 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 65 e7
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, di                                ; 89 fa
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 57 e7
    mov cx, si                                ; 89 f1
    mov ax, cx                                ; 89 c8
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
floppy_drive_exists_:                        ; 0xf2ec2 LB 0x3a
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, ax                                ; 89 c2
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 8d e7
    test dx, dx                               ; 85 d2
    jne short 02ed9h                          ; 75 06
    mov CL, strict byte 004h                  ; b1 04
    shr al, CL                                ; d2 e8
    jmp short 02edbh                          ; eb 02
    and AL, strict byte 00fh                  ; 24 0f
    test al, al                               ; 84 c0
    je short 02ee4h                           ; 74 05
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 02ee6h                          ; eb 02
    xor ah, ah                                ; 30 e4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    retn                                      ; c3
    or AL, strict byte 036h                   ; 0c 36
    pop SS                                    ; 17
    and word [ss:03628h], si                  ; 36 21 36 28 36
    das                                       ; 2f
    db  036h, 036h, 036h, 03dh, 036h, 047h
    ; ss cmp ax, 04736h                         ; 36 36 36 3d 36 47
    db  036h, 04eh
    ; ss dec si                                 ; 36 4e
    db  036h
_int13_diskette_function:                    ; 0xf2efc LB 0x7fd
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00018h                ; 83 ec 18
    mov bl, byte [bp+01bh]                    ; 8a 5e 1b
    xor bh, bh                                ; 30 ff
    mov cl, bl                                ; 88 d9
    mov si, word [bp+01ah]                    ; 8b 76 1a
    and si, 000ffh                            ; 81 e6 ff 00
    mov ah, byte [bp+012h]                    ; 8a 66 12
    cmp bl, 008h                              ; 80 fb 08
    jc short 02f4ah                           ; 72 30
    mov dx, word [bp+020h]                    ; 8b 56 20
    or dl, 001h                               ; 80 ca 01
    cmp bl, 008h                              ; 80 fb 08
    jbe short 02f65h                          ; 76 40
    cmp bl, 016h                              ; 80 fb 16
    jc short 02f42h                           ; 72 18
    or si, 00100h                             ; 81 ce 00 01
    mov cx, si                                ; 89 f1
    cmp bl, 016h                              ; 80 fb 16
    jbe short 02f86h                          ; 76 51
    cmp bl, 018h                              ; 80 fb 18
    je short 02f89h                           ; 74 4f
    cmp bl, 017h                              ; 80 fb 17
    je short 02f89h                           ; 74 4a
    jmp near 036d3h                           ; e9 91 07
    cmp bl, 015h                              ; 80 fb 15
    je short 02fa0h                           ; 74 59
    jmp near 036d3h                           ; e9 89 07
    cmp bl, 001h                              ; 80 fb 01
    jc short 02f5eh                           ; 72 0f
    jbe short 02fa3h                          ; 76 52
    cmp bl, 005h                              ; 80 fb 05
    je short 02fbeh                           ; 74 68
    cmp bl, 004h                              ; 80 fb 04
    jbe short 02fc1h                          ; 76 66
    jmp near 036d3h                           ; e9 75 07
    test bl, bl                               ; 84 db
    je short 02f68h                           ; 74 06
    jmp near 036d3h                           ; e9 6e 07
    jmp near 03585h                           ; e9 1d 06
    mov al, byte [bp+012h]                    ; 8a 46 12
    mov byte [bp-002h], al                    ; 88 46 fe
    cmp AL, strict byte 001h                  ; 3c 01
    jbe short 02f8ch                          ; 76 1a
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00001h                ; b8 01 00
    call 02bcah                               ; e8 47 fc
    jmp near 03391h                           ; e9 0b 04
    jmp near 036b1h                           ; e9 28 07
    jmp near 036b6h                           ; e9 2a 07
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 ca e6
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 02fa5h                          ; 75 0d
    mov CL, strict byte 004h                  ; b1 04
    mov bl, al                                ; 88 c3
    shr bl, CL                                ; d2 eb
    jmp short 02faah                          ; eb 0a
    jmp near 03674h                           ; e9 d1 06
    jmp short 02fe9h                          ; eb 44
    mov bl, al                                ; 88 c3
    and bl, 00fh                              ; 80 e3 0f
    test bl, bl                               ; 84 db
    jne short 02fc3h                          ; 75 15
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, 00080h                            ; b8 80 00
    jmp short 02f80h                          ; eb c2
    jmp near 033c4h                           ; e9 03 04
    jmp short 03005h                          ; eb 42
    xor bx, bx                                ; 31 db
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 40 e6
    xor al, al                                ; 30 c0
    mov byte [bp+01bh], al                    ; 88 46 1b
    xor ah, ah                                ; 30 e4
    call 02bcah                               ; e8 f2 fb
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    xor dx, dx                                ; 31 d2
    call 02be0h                               ; e8 fa fb
    jmp near 03243h                           ; e9 5a 02
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    mov dx, 00441h                            ; ba 41 04
    xor ax, ax                                ; 31 c0
    call 01600h                               ; e8 0b e6
    mov dh, al                                ; 88 c6
    xor dl, dl                                ; 30 d2
    or si, dx                                 ; 09 d6
    mov word [bp+01ah], si                    ; 89 76 1a
    test al, al                               ; 84 c0
    je short 03060h                           ; 74 5e
    jmp near 03391h                           ; e9 8c 03
    mov ch, byte [bp+01ah]                    ; 8a 6e 1a
    mov dl, byte [bp+019h]                    ; 8a 56 19
    mov byte [bp-008h], dl                    ; 88 56 f8
    mov al, byte [bp+018h]                    ; 8a 46 18
    mov byte [bp-004h], al                    ; 88 46 fc
    mov dl, byte [bp+017h]                    ; 8a 56 17
    xor dh, dh                                ; 30 f6
    mov byte [bp-006h], dl                    ; 88 56 fa
    mov byte [bp-002h], ah                    ; 88 66 fe
    cmp ah, 001h                              ; 80 fc 01
    jnbe short 03032h                         ; 77 0e
    cmp dl, 001h                              ; 80 fa 01
    jnbe short 03032h                         ; 77 09
    test ch, ch                               ; 84 ed
    je short 03032h                           ; 74 05
    cmp ch, 048h                              ; 80 fd 48
    jbe short 03063h                          ; 76 31
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 80 e8
    mov ax, 001bdh                            ; b8 bd 01
    push ax                                   ; 50
    mov ax, 001d5h                            ; b8 d5 01
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 ae e8
    add sp, strict byte 00006h                ; 83 c4 06
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00001h                ; b8 01 00
    jmp near 030e1h                           ; e9 81 00
    jmp near 03243h                           ; e9 e0 01
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 02ec2h                               ; e8 57 fe
    test ax, ax                               ; 85 c0
    je short 0309dh                           ; 74 2e
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    xor dh, dh                                ; 30 f6
    mov ax, dx                                ; 89 d0
    call 02ccbh                               ; e8 52 fc
    test ax, ax                               ; 85 c0
    jne short 030a0h                          ; 75 23
    mov ax, dx                                ; 89 d0
    call 02dbeh                               ; e8 3c fd
    test ax, ax                               ; 85 c0
    jne short 030a0h                          ; 75 1a
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 0000ch                ; b8 0c 00
    call 02bcah                               ; e8 33 fb
    mov byte [bp+01ah], dh                    ; 88 76 1a
    jmp near 03391h                           ; e9 f4 02
    jmp near 0317eh                           ; e9 de 00
    cmp cl, 002h                              ; 80 f9 02
    jne short 030ebh                          ; 75 46
    mov CL, strict byte 00ch                  ; b1 0c
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    shr ax, CL                                ; d3 e8
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ah, byte [bp-00eh]                    ; 8a 66 f2
    mov CL, strict byte 004h                  ; b1 04
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    sal dx, CL                                ; d3 e2
    mov bx, word [bp+014h]                    ; 8b 5e 14
    add bx, dx                                ; 01 d3
    cmp bx, dx                                ; 39 d3
    jnc short 030c4h                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    mov dl, ch                                ; 88 ea
    xor dh, dh                                ; 30 f6
    mov CL, strict byte 009h                  ; b1 09
    sal dx, CL                                ; d3 e2
    dec dx                                    ; 4a
    mov word [bp-00ch], dx                    ; 89 56 f4
    add dx, bx                                ; 01 da
    cmp dx, bx                                ; 39 da
    jnc short 030eeh                          ; 73 18
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    mov ah, cl                                ; 88 cc
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00009h                ; b8 09 00
    call 02bcah                               ; e8 e6 fa
    mov byte [bp+01ah], 000h                  ; c6 46 1a 00
    jmp near 03391h                           ; e9 a6 02
    jmp near 03249h                           ; e9 5b 01
    mov AL, strict byte 006h                  ; b0 06
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00004h                ; ba 04 00
    out DX, AL                                ; ee
    mov al, bh                                ; 88 f8
    out DX, AL                                ; ee
    xor al, bh                                ; 30 f8
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov dx, strict word 00005h                ; ba 05 00
    out DX, AL                                ; ee
    mov al, byte [bp-00bh]                    ; 8a 46 f5
    out DX, AL                                ; ee
    mov AL, strict byte 046h                  ; b0 46
    mov dx, strict word 0000bh                ; ba 0b 00
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    mov dx, 00081h                            ; ba 81 00
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    out DX, AL                                ; ee
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 02c38h                               ; e8 09 fb
    mov AL, strict byte 0e6h                  ; b0 e6
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 1                                 ; d1 e2
    sal dx, 1                                 ; d1 e2
    mov al, byte [bp-002h]                    ; 8a 46 fe
    or ax, dx                                 ; 09 d0
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-008h]                    ; 8a 46 f8
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    out DX, AL                                ; ee
    mov al, byte [bp-004h]                    ; 8a 46 fc
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    mov dl, byte [bp-004h]                    ; 8a 56 fc
    xor dh, dh                                ; 30 f6
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    add ax, dx                                ; 01 d0
    dec ax                                    ; 48
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 01600h                               ; e8 89 e4
    test al, al                               ; 84 c0
    jne short 0318fh                          ; 75 14
    call 02c13h                               ; e8 95 fa
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, 00080h                            ; b8 80 00
    jmp near 030e1h                           ; e9 52 ff
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 68 e4
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 0316fh                           ; 74 d1
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 58 e4
    mov bl, al                                ; 88 c3
    and bl, 07fh                              ; 80 e3 7f
    xor bh, bh                                ; 30 ff
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 56 e4
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 031d6h                           ; 74 12
    mov ax, 001bdh                            ; b8 bd 01
    push ax                                   ; 50
    mov ax, 001f0h                            ; b8 f0 01
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 27 e7
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 031dfh                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 031f8h                          ; 7d 19
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-018h], al                 ; 88 42 e8
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 19 e4
    inc si                                    ; 46
    jmp short 031dah                          ; eb e2
    test byte [bp-018h], 0c0h                 ; f6 46 e8 c0
    je short 0320fh                           ; 74 11
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 020h                               ; 80 cc 20
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00020h                ; b8 20 00
    jmp near 030e1h                           ; e9 d2 fe
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 009h                  ; b1 09
    sal ax, CL                                ; d3 e0
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov si, word [bp+014h]                    ; 8b 76 14
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    mov di, si                                ; 89 f7
    mov es, dx                                ; 8e c2
    mov cx, ax                                ; 89 c1
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    mov dl, byte [bp-008h]                    ; 8a 56 f8
    xor dh, dh                                ; 30 f6
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 02be0h                               ; e8 a5 f9
    mov byte [bp+01bh], 000h                  ; c6 46 1b 00
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    cmp cl, 003h                              ; 80 f9 03
    je short 03251h                           ; 74 03
    jmp near 033ach                           ; e9 5b 01
    mov CL, strict byte 00ch                  ; b1 0c
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    shr ax, CL                                ; d3 e8
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ah, byte [bp-00ah]                    ; 8a 66 f6
    mov CL, strict byte 004h                  ; b1 04
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    sal dx, CL                                ; d3 e2
    mov bx, word [bp+014h]                    ; 8b 5e 14
    add bx, dx                                ; 01 d3
    cmp bx, dx                                ; 39 d3
    jnc short 03270h                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    mov dl, ch                                ; 88 ea
    xor dh, dh                                ; 30 f6
    mov CL, strict byte 009h                  ; b1 09
    sal dx, CL                                ; d3 e2
    dec dx                                    ; 4a
    mov word [bp-00ch], dx                    ; 89 56 f4
    add dx, bx                                ; 01 da
    cmp dx, bx                                ; 39 da
    jnc short 03285h                          ; 73 03
    jmp near 030d6h                           ; e9 51 fe
    mov AL, strict byte 006h                  ; b0 06
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00004h                ; ba 04 00
    out DX, AL                                ; ee
    mov al, bh                                ; 88 f8
    out DX, AL                                ; ee
    xor al, bh                                ; 30 f8
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov dx, strict word 00005h                ; ba 05 00
    out DX, AL                                ; ee
    mov al, byte [bp-00bh]                    ; 8a 46 f5
    out DX, AL                                ; ee
    mov AL, strict byte 04ah                  ; b0 4a
    mov dx, strict word 0000bh                ; ba 0b 00
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    mov dx, 00081h                            ; ba 81 00
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 02c38h                               ; e8 73 f9
    mov AL, strict byte 0c5h                  ; b0 c5
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 1                                 ; d1 e2
    sal dx, 1                                 ; d1 e2
    mov al, byte [bp-002h]                    ; 8a 46 fe
    or ax, dx                                 ; 09 d0
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-008h]                    ; 8a 46 f8
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    out DX, AL                                ; ee
    mov al, byte [bp-004h]                    ; 8a 46 fc
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    mov dl, ch                                ; 88 ea
    xor dh, dh                                ; 30 f6
    add ax, dx                                ; 01 d0
    dec ax                                    ; 48
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov AL, strict byte 0ffh                  ; b0 ff
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 01600h                               ; e8 f3 e2
    test al, al                               ; 84 c0
    jne short 03314h                          ; 75 03
    jmp near 0317bh                           ; e9 67 fe
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 e3 e2
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 03305h                           ; 74 e2
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 d3 e2
    mov bl, al                                ; 88 c3
    and bl, 07fh                              ; 80 e3 7f
    xor bh, bh                                ; 30 ff
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 d1 e2
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 0335bh                           ; 74 12
    mov ax, 001bdh                            ; b8 bd 01
    push ax                                   ; 50
    mov ax, 001f0h                            ; b8 f0 01
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 a2 e5
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 03364h                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 0337dh                          ; 7d 19
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-018h], al                 ; 88 42 e8
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 94 e2
    inc si                                    ; 46
    jmp short 0335fh                          ; eb e2
    test byte [bp-018h], 0c0h                 ; f6 46 e8 c0
    jne short 03386h                          ; 75 03
    jmp near 0322eh                           ; e9 a8 fe
    test byte [bp-017h], 002h                 ; f6 46 e9 02
    je short 03398h                           ; 74 0c
    mov word [bp+01ah], 00300h                ; c7 46 1a 00 03
    or byte [bp+020h], 001h                   ; 80 4e 20 01
    jmp near 03243h                           ; e9 ab fe
    mov ax, 001bdh                            ; b8 bd 01
    push ax                                   ; 50
    mov ax, 00204h                            ; b8 04 02
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 53 e5
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 03383h                          ; eb d7
    mov dl, byte [bp-008h]                    ; 8a 56 f8
    xor dh, dh                                ; 30 f6
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 02be0h                               ; e8 27 f8
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    mov byte [bp+01bh], 000h                  ; c6 46 1b 00
    jmp near 03243h                           ; e9 7f fe
    mov ch, byte [bp+01ah]                    ; 8a 6e 1a
    mov al, byte [bp+019h]                    ; 8a 46 19
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    mov al, byte [bp+017h]                    ; 8a 46 17
    mov byte [bp-006h], al                    ; 88 46 fa
    mov bl, byte [bp+012h]                    ; 8a 5e 12
    mov byte [bp-002h], bl                    ; 88 5e fe
    cmp bl, 001h                              ; 80 fb 01
    jnbe short 033f1h                         ; 77 12
    cmp AL, strict byte 001h                  ; 3c 01
    jnbe short 033f1h                         ; 77 0e
    cmp dl, 04fh                              ; 80 fa 4f
    jnbe short 033f1h                         ; 77 09
    test ch, ch                               ; 84 ed
    je short 033f1h                           ; 74 05
    cmp ch, 012h                              ; 80 fd 12
    jbe short 03406h                          ; 76 15
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00001h                ; b8 01 00
    call 02bcah                               ; e8 c8 f7
    or byte [bp+020h], 001h                   ; 80 4e 20 01
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 02ec2h                               ; e8 b4 fa
    test ax, ax                               ; 85 c0
    jne short 03415h                          ; 75 03
    jmp near 02faeh                           ; e9 99 fb
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    xor dh, dh                                ; 30 f6
    mov ax, dx                                ; 89 d0
    call 02ccbh                               ; e8 ac f8
    test ax, ax                               ; 85 c0
    jne short 0342fh                          ; 75 0c
    mov ax, dx                                ; 89 d0
    call 02dbeh                               ; e8 96 f9
    test ax, ax                               ; 85 c0
    jne short 0342fh                          ; 75 03
    jmp near 03086h                           ; e9 57 fc
    mov CL, strict byte 00ch                  ; b1 0c
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    shr ax, CL                                ; d3 e8
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ah, byte [bp-010h]                    ; 8a 66 f0
    mov CL, strict byte 004h                  ; b1 04
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    sal dx, CL                                ; d3 e2
    mov bx, word [bp+014h]                    ; 8b 5e 14
    add bx, dx                                ; 01 d3
    cmp bx, dx                                ; 39 d3
    jnc short 0344eh                          ; 73 02
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    mov dl, ch                                ; 88 ea
    xor dh, dh                                ; 30 f6
    sal dx, 1                                 ; d1 e2
    sal dx, 1                                 ; d1 e2
    dec dx                                    ; 4a
    mov word [bp-00ch], dx                    ; 89 56 f4
    add dx, bx                                ; 01 da
    cmp dx, bx                                ; 39 da
    jnc short 0346bh                          ; 73 0b
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 009h                               ; 80 cc 09
    jmp near 030dbh                           ; e9 70 fc
    mov AL, strict byte 006h                  ; b0 06
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00004h                ; ba 04 00
    out DX, AL                                ; ee
    mov al, bh                                ; 88 f8
    out DX, AL                                ; ee
    xor al, bh                                ; 30 f8
    mov dx, strict word 0000ch                ; ba 0c 00
    out DX, AL                                ; ee
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    mov dx, strict word 00005h                ; ba 05 00
    out DX, AL                                ; ee
    mov al, byte [bp-00bh]                    ; 8a 46 f5
    out DX, AL                                ; ee
    mov AL, strict byte 04ah                  ; b0 4a
    mov dx, strict word 0000bh                ; ba 0b 00
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    mov dx, 00081h                            ; ba 81 00
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, strict word 0000ah                ; ba 0a 00
    out DX, AL                                ; ee
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 02c38h                               ; e8 8d f7
    mov AL, strict byte 04dh                  ; b0 4d
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov al, byte [bp-006h]                    ; 8a 46 fa
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    sal dx, 1                                 ; d1 e2
    sal dx, 1                                 ; d1 e2
    mov al, byte [bp-002h]                    ; 8a 46 fe
    or ax, dx                                 ; 09 d0
    mov dx, 003f5h                            ; ba f5 03
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    mov al, ch                                ; 88 e8
    out DX, AL                                ; ee
    xor al, ch                                ; 30 e8
    out DX, AL                                ; ee
    mov AL, strict byte 0f6h                  ; b0 f6
    out DX, AL                                ; ee
    sti                                       ; fb
    mov dx, strict word 00040h                ; ba 40 00
    mov ax, dx                                ; 89 d0
    call 01600h                               ; e8 26 e1
    test al, al                               ; 84 c0
    jne short 034e4h                          ; 75 06
    call 02c13h                               ; e8 32 f7
    jmp near 02faeh                           ; e9 ca fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 13 e1
    and AL, strict byte 080h                  ; 24 80
    test al, al                               ; 84 c0
    je short 034d2h                           ; 74 df
    cli                                       ; fa
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 03 e1
    mov bl, al                                ; 88 c3
    and bl, 07fh                              ; 80 e3 7f
    xor bh, bh                                ; 30 ff
    mov dx, strict word 0003eh                ; ba 3e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 01 e1
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 0352bh                           ; 74 12
    mov ax, 001bdh                            ; b8 bd 01
    push ax                                   ; 50
    mov ax, 001f0h                            ; b8 f0 01
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 d2 e3
    add sp, strict byte 00006h                ; 83 c4 06
    xor si, si                                ; 31 f6
    jmp short 03534h                          ; eb 05
    cmp si, strict byte 00007h                ; 83 fe 07
    jnl short 0354dh                          ; 7d 19
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+si-018h], al                 ; 88 42 e8
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    lea dx, [si+042h]                         ; 8d 54 42
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 c4 e0
    inc si                                    ; 46
    jmp short 0352fh                          ; eb e2
    test byte [bp-018h], 0c0h                 ; f6 46 e8 c0
    je short 0356eh                           ; 74 1b
    test byte [bp-017h], 002h                 ; f6 46 e9 02
    je short 0355ch                           ; 74 03
    jmp near 0338ch                           ; e9 30 fe
    mov ax, 001bdh                            ; b8 bd 01
    push ax                                   ; 50
    mov ax, 00214h                            ; b8 14 02
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 8f e3
    add sp, strict byte 00006h                ; 83 c4 06
    xor al, al                                ; 30 c0
    mov byte [bp+01bh], al                    ; 88 46 1b
    xor ah, ah                                ; 30 e4
    call 02bcah                               ; e8 52 f6
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    xor dx, dx                                ; 31 d2
    call 02be0h                               ; e8 5e f6
    jmp near 0323fh                           ; e9 ba fc
    mov byte [bp-002h], ah                    ; 88 66 fe
    cmp ah, 001h                              ; 80 fc 01
    jbe short 035aeh                          ; 76 21
    xor ax, ax                                ; 31 c0
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+018h], ax                    ; 89 46 18
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+00ah], ax                    ; 89 46 0a
    mov word [bp+00ch], ax                    ; 89 46 0c
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+020h], dx                    ; 89 56 20
    jmp near 03243h                           ; e9 95 fc
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 a8 e0
    mov bl, al                                ; 88 c3
    xor ch, ch                                ; 30 ed
    test AL, strict byte 0f0h                 ; a8 f0
    je short 035beh                           ; 74 02
    mov CH, strict byte 001h                  ; b5 01
    test bl, 00fh                             ; f6 c3 0f
    je short 035c5h                           ; 74 02
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 035d1h                          ; 75 06
    mov CL, strict byte 004h                  ; b1 04
    shr bl, CL                                ; d2 eb
    jmp short 035d4h                          ; eb 03
    and bl, 00fh                              ; 80 e3 0f
    mov byte [bp+015h], 000h                  ; c6 46 15 00
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov word [bp+014h], ax                    ; 89 46 14
    mov word [bp+01ah], strict word 00000h    ; c7 46 1a 00 00
    mov si, word [bp+016h]                    ; 8b 76 16
    and si, 0ff00h                            ; 81 e6 00 ff
    mov dl, ch                                ; 88 ea
    xor dh, dh                                ; 30 f6
    or si, dx                                 ; 09 d6
    mov word [bp+016h], si                    ; 89 76 16
    cmp bl, 008h                              ; 80 fb 08
    jnbe short 03655h                         ; 77 5c
    mov si, ax                                ; 89 c6
    sal si, 1                                 ; d1 e6
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    or bh, 001h                               ; 80 cf 01
    jmp word [cs:si+02eeah]                   ; 2e ff a4 ea 2e
    mov word [bp+018h], strict word 00000h    ; c7 46 18 00 00
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    jmp short 03667h                          ; eb 50
    mov word [bp+018h], 02709h                ; c7 46 18 09 27
    mov word [bp+016h], bx                    ; 89 5e 16
    jmp short 03667h                          ; eb 46
    mov word [bp+018h], 04f0fh                ; c7 46 18 0f 4f
    jmp short 0361ch                          ; eb f4
    mov word [bp+018h], 04f09h                ; c7 46 18 09 4f
    jmp short 0361ch                          ; eb ed
    mov word [bp+018h], 04f12h                ; c7 46 18 12 4f
    jmp short 0361ch                          ; eb e6
    mov word [bp+018h], 04f24h                ; c7 46 18 24 4f
    jmp short 0361ch                          ; eb df
    mov word [bp+018h], 02708h                ; c7 46 18 08 27
    mov word [bp+016h], ax                    ; 89 46 16
    jmp short 03667h                          ; eb 20
    mov word [bp+018h], 02709h                ; c7 46 18 09 27
    jmp short 03642h                          ; eb f4
    mov word [bp+018h], 02708h                ; c7 46 18 08 27
    jmp short 0361ch                          ; eb c7
    mov ax, 001bdh                            ; b8 bd 01
    push ax                                   ; 50
    mov ax, 00225h                            ; b8 25 02
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 96 e2
    add sp, strict byte 00006h                ; 83 c4 06
    mov word [bp+00ah], 0f000h                ; c7 46 0a 00 f0
    mov word [bp+00ch], 0efc7h                ; c7 46 0c c7 ef
    jmp near 0323fh                           ; e9 cb fb
    mov byte [bp-002h], ah                    ; 88 66 fe
    cmp ah, 001h                              ; 80 fc 01
    jbe short 03682h                          ; 76 06
    mov word [bp+01ah], si                    ; 89 76 1a
    jmp near 035a8h                           ; e9 26 ff
    mov ax, strict word 00010h                ; b8 10 00
    call 0165ch                               ; e8 d4 df
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 03696h                          ; 75 08
    mov CL, strict byte 004h                  ; b1 04
    mov bl, al                                ; 88 c3
    shr bl, CL                                ; d2 eb
    jmp short 0369bh                          ; eb 05
    mov bl, al                                ; 88 c3
    and bl, 00fh                              ; 80 e3 0f
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    test bl, bl                               ; 84 db
    je short 036abh                           ; 74 03
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    jmp near 03243h                           ; e9 92 fb
    cmp ah, 001h                              ; 80 fc 01
    jbe short 036c2h                          ; 76 0c
    mov word [bp+01ah], si                    ; 89 76 1a
    mov ax, strict word 00001h                ; b8 01 00
    call 02bcah                               ; e8 0b f5
    jmp near 035a8h                           ; e9 e6 fe
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 006h                               ; 80 cc 06
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov ax, strict word 00006h                ; b8 06 00
    jmp near 02f80h                           ; e9 ad f8
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 df e1
    mov al, byte [bp+01bh]                    ; 8a 46 1b
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 001bdh                            ; b8 bd 01
    push ax                                   ; 50
    mov ax, 0023ah                            ; b8 3a 02
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 07 e2
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 02f72h                           ; e9 79 f8
_cdemu_init:                                 ; 0xf36f9 LB 0x16
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 17 df
    xor bx, bx                                ; 31 db
    mov dx, 00322h                            ; ba 22 03
    call 0160eh                               ; e8 01 df
    pop bp                                    ; 5d
    retn                                      ; c3
_cdemu_isactive:                             ; 0xf370f LB 0x14
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 01 df
    mov dx, 00322h                            ; ba 22 03
    call 01600h                               ; e8 df de
    pop bp                                    ; 5d
    retn                                      ; c3
_cdemu_emulated_drive:                       ; 0xf3723 LB 0x14
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 ed de
    mov dx, 00324h                            ; ba 24 03
    call 01600h                               ; e8 cb de
    pop bp                                    ; 5d
    retn                                      ; c3
_int13_eltorito:                             ; 0xf3737 LB 0x187
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 d7 de
    mov si, 00322h                            ; be 22 03
    mov di, ax                                ; 89 c7
    mov al, byte [bp+01bh]                    ; 8a 46 1b
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 0004bh                ; 3d 4b 00
    jc short 0375eh                           ; 72 0a
    jbe short 0377eh                          ; 76 28
    cmp ax, strict word 0004dh                ; 3d 4d 00
    jbe short 03763h                          ; 76 08
    jmp near 03880h                           ; e9 22 01
    cmp ax, strict word 0004ah                ; 3d 4a 00
    jne short 0377bh                          ; 75 18
    push word [bp+01ah]                       ; ff 76 1a
    mov ax, 00254h                            ; b8 54 02
    push ax                                   ; 50
    mov ax, 00263h                            ; b8 63 02
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 85 e1
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 0389fh                           ; e9 24 01
    jmp near 03880h                           ; e9 02 01
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    mov ax, word [bp+008h]                    ; 8b 46 08
    mov bx, strict word 00013h                ; bb 13 00
    call 0160eh                               ; e8 84 de
    mov es, di                                ; 8e c7
    mov bl, byte [es:si+001h]                 ; 26 8a 5c 01
    xor bh, bh                                ; 30 ff
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    inc dx                                    ; 42
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 72 de
    mov es, di                                ; 8e c7
    mov bl, byte [es:si+002h]                 ; 26 8a 5c 02
    xor bh, bh                                ; 30 ff
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    inc dx                                    ; 42
    inc dx                                    ; 42
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 5f de
    mov es, di                                ; 8e c7
    mov bl, byte [es:si+003h]                 ; 26 8a 5c 03
    xor bh, bh                                ; 30 ff
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00003h                ; 83 c2 03
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 4b de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+008h]                 ; 26 8b 5c 08
    mov cx, word [es:si+00ah]                 ; 26 8b 4c 0a
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0164ah                               ; e8 71 de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+004h]                 ; 26 8b 5c 04
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0162ah                               ; e8 3f de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+006h]                 ; 26 8b 5c 06
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 0000ah                ; 83 c2 0a
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0162ah                               ; e8 2d de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+00ch]                 ; 26 8b 5c 0c
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 0000ch                ; 83 c2 0c
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0162ah                               ; e8 1b de
    mov es, di                                ; 8e c7
    mov bx, word [es:si+00eh]                 ; 26 8b 5c 0e
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 0000eh                ; 83 c2 0e
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0162ah                               ; e8 09 de
    mov es, di                                ; 8e c7
    mov bl, byte [es:si+012h]                 ; 26 8a 5c 12
    xor bh, bh                                ; 30 ff
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00010h                ; 83 c2 10
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 d9 dd
    mov es, di                                ; 8e c7
    mov bl, byte [es:si+014h]                 ; 26 8a 5c 14
    xor bh, bh                                ; 30 ff
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00011h                ; 83 c2 11
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 c5 dd
    mov es, di                                ; 8e c7
    mov bl, byte [es:si+010h]                 ; 26 8a 5c 10
    xor bh, bh                                ; 30 ff
    mov dx, word [bp+00eh]                    ; 8b 56 0e
    add dx, strict byte 00012h                ; 83 c2 12
    mov ax, word [bp+008h]                    ; 8b 46 08
    call 0160eh                               ; e8 b1 dd
    test byte [bp+01ah], 0ffh                 ; f6 46 1a ff
    jne short 03869h                          ; 75 06
    mov es, di                                ; 8e c7
    mov byte [es:si], 000h                    ; 26 c6 04 00
    mov byte [bp+01bh], 000h                  ; c6 46 1b 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 96 dd
    and byte [bp+020h], 0feh                  ; 80 66 20 fe
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 32 e0
    mov al, byte [bp+01bh]                    ; 8a 46 1b
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 00254h                            ; b8 54 02
    push ax                                   ; 50
    mov ax, 00289h                            ; b8 89 02
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    jmp near 03771h                           ; e9 d2 fe
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ah], ax                    ; 89 46 1a
    mov bl, byte [bp+01bh]                    ; 8a 5e 1b
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 56 dd
    or byte [bp+020h], 001h                   ; 80 4e 20 01
    jmp short 0387ch                          ; eb be
device_is_cdrom_:                            ; 0xf38be LB 0x3c
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 4d dd
    mov cx, ax                                ; 89 c1
    cmp bl, 010h                              ; 80 fb 10
    jc short 038dah                           ; 72 04
    xor ax, ax                                ; 31 c0
    jmp short 038f5h                          ; eb 1b
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov bx, strict word 00018h                ; bb 18 00
    imul bx                                   ; f7 eb
    mov es, cx                                ; 8e c1
    mov bx, ax                                ; 89 c3
    add bx, 00122h                            ; 81 c3 22 01
    cmp byte [es:bx+01fh], 005h               ; 26 80 7f 1f 05
    jne short 038d6h                          ; 75 e4
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
cdrom_boot_:                                 ; 0xf38fa LB 0x468
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, 0081ch                            ; 81 ec 1c 08
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 0d dd
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov si, 00322h                            ; be 22 03
    mov word [bp-008h], ax                    ; 89 46 f8
    mov word [bp-00ah], 00122h                ; c7 46 f6 22 01
    mov word [bp-006h], ax                    ; 89 46 fa
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    jmp short 0392fh                          ; eb 09
    inc byte [bp-004h]                        ; fe 46 fc
    cmp byte [bp-004h], 010h                  ; 80 7e fc 10
    jnc short 0393bh                          ; 73 0c
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    call 038beh                               ; e8 87 ff
    test ax, ax                               ; 85 c0
    je short 03926h                           ; 74 eb
    cmp byte [bp-004h], 010h                  ; 80 7e fc 10
    jc short 03947h                           ; 72 06
    mov ax, strict word 00002h                ; b8 02 00
    jmp near 03d00h                           ; e9 b9 03
    mov cx, strict word 0000ch                ; b9 0c 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-01ch]                         ; 8d 46 e4
    call 08dbbh                               ; e8 67 54
    mov word [bp-01ch], strict word 00028h    ; c7 46 e4 28 00
    mov ax, strict word 00011h                ; b8 11 00
    xor dx, dx                                ; 31 d2
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-018h], dx                    ; 89 56 e8
    mov ax, strict word 00001h                ; b8 01 00
    xchg ah, al                               ; 86 c4
    mov word [bp-015h], ax                    ; 89 46 eb
    mov es, [bp-006h]                         ; 8e 46 fa
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00
    mov word [es:bx+00ch], 00800h             ; 26 c7 47 0c 00 08
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    jmp short 039bah                          ; eb 31
    lea dx, [bp-0081ch]                       ; 8d 96 e4 f7
    push SS                                   ; 16
    push dx                                   ; 52
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov ax, 00800h                            ; b8 00 08
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    call 028dbh                               ; e8 2e ef
    test ax, ax                               ; 85 c0
    je short 039e6h                           ; 74 35
    inc byte [bp-002h]                        ; fe 46 fe
    cmp byte [bp-002h], 004h                  ; 80 7e fe 04
    jnbe short 039e6h                         ; 77 2c
    cmp byte [bp-004h], 008h                  ; 80 7e fc 08
    jbe short 03989h                          ; 76 c9
    lea dx, [bp-0081ch]                       ; 8d 96 e4 f7
    push SS                                   ; 16
    push dx                                   ; 52
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov ax, 00800h                            ; b8 00 08
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    call 083e4h                               ; e8 00 4a
    jmp short 039adh                          ; eb c7
    test ax, ax                               ; 85 c0
    je short 039f0h                           ; 74 06
    mov ax, strict word 00003h                ; b8 03 00
    jmp near 03d00h                           ; e9 10 03
    cmp byte [bp-0081ch], 000h                ; 80 be e4 f7 00
    je short 039fdh                           ; 74 06
    mov ax, strict word 00004h                ; b8 04 00
    jmp near 03d00h                           ; e9 03 03
    xor di, di                                ; 31 ff
    jmp short 03a07h                          ; eb 06
    inc di                                    ; 47
    cmp di, strict byte 00005h                ; 83 ff 05
    jnc short 03a17h                          ; 73 10
    mov al, byte [bp+di-0081bh]               ; 8a 83 e5 f7
    cmp al, byte [di+00c1eh]                  ; 3a 85 1e 0c
    je short 03a01h                           ; 74 f0
    mov ax, strict word 00005h                ; b8 05 00
    jmp near 03d00h                           ; e9 e9 02
    xor di, di                                ; 31 ff
    jmp short 03a21h                          ; eb 06
    inc di                                    ; 47
    cmp di, strict byte 00017h                ; 83 ff 17
    jnc short 03a31h                          ; 73 10
    mov al, byte [bp+di-00815h]               ; 8a 83 eb f7
    cmp al, byte [di+00c24h]                  ; 3a 85 24 0c
    je short 03a1bh                           ; 74 f0
    mov ax, strict word 00006h                ; b8 06 00
    jmp near 03d00h                           ; e9 cf 02
    mov ax, word [bp-007d5h]                  ; 8b 86 2b f8
    mov dx, word [bp-007d3h]                  ; 8b 96 2d f8
    mov word [bp-01ch], strict word 00028h    ; c7 46 e4 28 00
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-018h], dx                    ; 89 56 e8
    mov ax, strict word 00001h                ; b8 01 00
    xchg ah, al                               ; 86 c4
    mov word [bp-015h], ax                    ; 89 46 eb
    cmp byte [bp-004h], 008h                  ; 80 7e fc 08
    jbe short 03a7dh                          ; 76 26
    lea dx, [bp-0081ch]                       ; 8d 96 e4 f7
    push SS                                   ; 16
    push dx                                   ; 52
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov ax, 00800h                            ; b8 00 08
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    call 083e4h                               ; e8 69 49
    jmp short 03aa1h                          ; eb 24
    lea dx, [bp-0081ch]                       ; 8d 96 e4 f7
    push SS                                   ; 16
    push dx                                   ; 52
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov ax, 00800h                            ; b8 00 08
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    call 028dbh                               ; e8 3a ee
    test ax, ax                               ; 85 c0
    je short 03aabh                           ; 74 06
    mov ax, strict word 00007h                ; b8 07 00
    jmp near 03d00h                           ; e9 55 02
    cmp byte [bp-0081ch], 001h                ; 80 be e4 f7 01
    je short 03ab8h                           ; 74 06
    mov ax, strict word 00008h                ; b8 08 00
    jmp near 03d00h                           ; e9 48 02
    cmp byte [bp-0081bh], 000h                ; 80 be e5 f7 00
    je short 03ac5h                           ; 74 06
    mov ax, strict word 00009h                ; b8 09 00
    jmp near 03d00h                           ; e9 3b 02
    cmp byte [bp-007feh], 055h                ; 80 be 02 f8 55
    je short 03ad2h                           ; 74 06
    mov ax, strict word 0000ah                ; b8 0a 00
    jmp near 03d00h                           ; e9 2e 02
    cmp byte [bp-007fdh], 0aah                ; 80 be 03 f8 aa
    jne short 03acch                          ; 75 f3
    cmp byte [bp-007fch], 088h                ; 80 be 04 f8 88
    je short 03ae6h                           ; 74 06
    mov ax, strict word 0000bh                ; b8 0b 00
    jmp near 03d00h                           ; e9 1a 02
    mov al, byte [bp-007fbh]                  ; 8a 86 05 f8
    mov es, [bp-008h]                         ; 8e 46 f8
    mov byte [es:si+001h], al                 ; 26 88 44 01
    cmp byte [bp-007fbh], 000h                ; 80 be 05 f8 00
    jne short 03affh                          ; 75 07
    mov byte [es:si+002h], 0e0h               ; 26 c6 44 02 e0
    jmp short 03b12h                          ; eb 13
    cmp byte [bp-007fbh], 004h                ; 80 be 05 f8 04
    jnc short 03b0dh                          ; 73 07
    mov byte [es:si+002h], 000h               ; 26 c6 44 02 00
    jmp short 03b12h                          ; eb 05
    mov byte [es:si+002h], 080h               ; 26 c6 44 02 80
    mov bl, byte [bp-004h]                    ; 8a 5e fc
    xor bh, bh                                ; 30 ff
    mov ax, bx                                ; 89 d8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    mov es, [bp-008h]                         ; 8e 46 f8
    mov byte [es:si+003h], al                 ; 26 88 44 03
    mov ax, bx                                ; 89 d8
    cwd                                       ; 99
    mov bx, strict word 00002h                ; bb 02 00
    idiv bx                                   ; f7 fb
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov di, word [bp-007fah]                  ; 8b be 06 f8
    test di, di                               ; 85 ff
    jne short 03b3ch                          ; 75 03
    mov di, 007c0h                            ; bf c0 07
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+00ch], di                 ; 26 89 7c 0c
    mov word [es:si+006h], strict word 00000h ; 26 c7 44 06 00 00
    mov cx, word [bp-007f6h]                  ; 8b 8e 0a f8
    mov word [es:si+00eh], cx                 ; 26 89 4c 0e
    mov ax, word [bp-007f4h]                  ; 8b 86 0c f8
    mov dx, word [bp-007f2h]                  ; 8b 96 0e f8
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov word [es:si+00ah], dx                 ; 26 89 54 0a
    mov word [bp-01ch], strict word 00028h    ; c7 46 e4 28 00
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov word [bp-018h], dx                    ; 89 56 e8
    mov dx, cx                                ; 89 ca
    dec dx                                    ; 4a
    shr dx, 1                                 ; d1 ea
    shr dx, 1                                 ; d1 ea
    inc dx                                    ; 42
    mov ax, dx                                ; 89 d0
    xchg ah, al                               ; 86 c4
    mov word [bp-015h], ax                    ; 89 46 eb
    mov es, [bp-006h]                         ; 8e 46 fa
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov word [es:bx+00ah], dx                 ; 26 89 57 0a
    mov word [es:bx+00ch], 00200h             ; 26 c7 47 0c 00 02
    mov word [bp-010h], cx                    ; 89 4e f0
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00
    mov cx, strict word 00009h                ; b9 09 00
    sal word [bp-010h], 1                     ; d1 66 f0
    rcl word [bp-00eh], 1                     ; d1 56 f2
    loop 03b9bh                               ; e2 f8
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov dx, word [bp-00eh]                    ; 8b 56 f2
    mov bx, 00800h                            ; bb 00 08
    xor cx, cx                                ; 31 c9
    call 08ceeh                               ; e8 3d 51
    mov ax, 00800h                            ; b8 00 08
    sub ax, bx                                ; 29 d8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov word [es:bx+01ch], ax                 ; 26 89 47 1c
    cmp byte [bp-004h], 008h                  ; 80 7e fc 08
    jbe short 03be6h                          ; 76 23
    push di                                   ; 57
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    push word [bp-00eh]                       ; ff 76 f2
    push word [bp-010h]                       ; ff 76 f0
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    call 083e4h                               ; e8 00 48
    jmp short 03c07h                          ; eb 21
    push di                                   ; 57
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    push word [bp-00eh]                       ; ff 76 f2
    push word [bp-010h]                       ; ff 76 f0
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01ch]                         ; 8d 5e e4
    mov dx, strict word 0000ch                ; ba 0c 00
    call 028dbh                               ; e8 d4 ec
    mov es, [bp-006h]                         ; 8e 46 fa
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov word [es:bx+01ch], strict word 00000h ; 26 c7 47 1c 00 00
    test ax, ax                               ; 85 c0
    je short 03c1dh                           ; 74 06
    mov ax, strict word 0000ch                ; b8 0c 00
    jmp near 03d00h                           ; e9 e3 00
    mov es, [bp-008h]                         ; 8e 46 f8
    mov al, byte [es:si+001h]                 ; 26 8a 44 01
    cmp AL, strict byte 002h                  ; 3c 02
    jc short 03c34h                           ; 72 0c
    jbe short 03c4fh                          ; 76 25
    cmp AL, strict byte 004h                  ; 3c 04
    je short 03c5fh                           ; 74 31
    cmp AL, strict byte 003h                  ; 3c 03
    je short 03c57h                           ; 74 25
    jmp short 03ca8h                          ; eb 74
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 03ca8h                          ; 75 70
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+014h], strict word 0000fh ; 26 c7 44 14 0f 00
    mov word [es:si+012h], strict word 00050h ; 26 c7 44 12 50 00
    mov word [es:si+010h], strict word 00002h ; 26 c7 44 10 02 00
    jmp short 03ca8h                          ; eb 59
    mov word [es:si+014h], strict word 00012h ; 26 c7 44 14 12 00
    jmp short 03c41h                          ; eb ea
    mov word [es:si+014h], strict word 00024h ; 26 c7 44 14 24 00
    jmp short 03c41h                          ; eb e2
    mov dx, 001c4h                            ; ba c4 01
    mov ax, di                                ; 89 f8
    call 01600h                               ; e8 99 d9
    and AL, strict byte 03fh                  ; 24 3f
    xor ah, ah                                ; 30 e4
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+014h], ax                 ; 26 89 44 14
    mov dx, 001c4h                            ; ba c4 01
    mov ax, di                                ; 89 f8
    call 01600h                               ; e8 86 d9
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    sal bx, 1                                 ; d1 e3
    sal bx, 1                                 ; d1 e3
    mov dx, 001c5h                            ; ba c5 01
    mov ax, di                                ; 89 f8
    call 01600h                               ; e8 76 d9
    xor ah, ah                                ; 30 e4
    add ax, bx                                ; 01 d8
    inc ax                                    ; 40
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+012h], ax                 ; 26 89 44 12
    mov dx, 001c3h                            ; ba c3 01
    mov ax, di                                ; 89 f8
    call 01600h                               ; e8 62 d9
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+010h], ax                 ; 26 89 44 10
    mov es, [bp-008h]                         ; 8e 46 f8
    cmp byte [es:si+001h], 000h               ; 26 80 7c 01 00
    je short 03ce9h                           ; 74 37
    cmp byte [es:si+002h], 000h               ; 26 80 7c 02 00
    jne short 03cd1h                          ; 75 18
    mov dx, strict word 00010h                ; ba 10 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 3e d9
    mov bl, al                                ; 88 c3
    or bl, 041h                               ; 80 cb 41
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00010h                ; ba 10 00
    mov ax, strict word 00040h                ; b8 40 00
    jmp short 03ce6h                          ; eb 15
    mov dx, 002c0h                            ; ba c0 02
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    call 01600h                               ; e8 26 d9
    mov bl, al                                ; 88 c3
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    xor bh, bh                                ; 30 ff
    mov dx, 002c0h                            ; ba c0 02
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    call 0160eh                               ; e8 25 d9
    mov es, [bp-008h]                         ; 8e 46 f8
    cmp byte [es:si+001h], 000h               ; 26 80 7c 01 00
    je short 03cf7h                           ; 74 04
    mov byte [es:si], 001h                    ; 26 c6 04 01
    mov es, [bp-008h]                         ; 8e 46 f8
    mov ah, byte [es:si+002h]                 ; 26 8a 64 02
    xor al, al                                ; 30 c0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
    push ax                                   ; 50
    dec si                                    ; 4e
    dec cx                                    ; 49
    dec ax                                    ; 48
    inc di                                    ; 47
    inc si                                    ; 46
    inc bp                                    ; 45
    inc sp                                    ; 44
    inc bx                                    ; 43
    inc dx                                    ; 42
    inc cx                                    ; 41
    sbb byte [01415h], dl                     ; 18 16 15 14
    adc word [bx+si], dx                      ; 11 10
    or ax, 00b0ch                             ; 0d 0c 0b
    or cl, byte [bx+di]                       ; 0a 09
    or byte [di], al                          ; 08 05
    add AL, strict byte 003h                  ; 04 03
    add al, byte [bx+di]                      ; 02 01
    add dh, ah                                ; 00 e6
    inc ax                                    ; 40
    das                                       ; 2f
    inc ax                                    ; 40
    push SS                                   ; 16
    db  03eh, 03fh
    ; ds aas                                    ; 3e 3f
    or di, word [ds:03e3fh]                   ; 3e 0b 3e 3f 3e
    or di, word [04048h]                      ; 0b 3e 48 40
    das                                       ; 2f
    inc ax                                    ; 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    das                                       ; 2f
    inc ax                                    ; 40
    das                                       ; 2f
    inc ax                                    ; 40
    das                                       ; 2f
    inc ax                                    ; 40
    das                                       ; 2f
    inc ax                                    ; 40
    das                                       ; 2f
    inc ax                                    ; 40
    fadd qword [bx+si+02fh]                   ; dc 40 2f
    inc ax                                    ; 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
_int13_cdemu:                                ; 0xf3d62 LB 0x420
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0002ch                ; 83 ec 2c
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 a9 d8
    mov di, 00322h                            ; bf 22 03
    mov cx, ax                                ; 89 c1
    mov si, di                                ; 89 fe
    mov word [bp-012h], ax                    ; 89 46 ee
    mov word [bp-016h], 00122h                ; c7 46 ea 22 01
    mov word [bp-014h], ax                    ; 89 46 ec
    mov es, ax                                ; 8e c0
    mov al, byte [es:di+003h]                 ; 26 8a 45 03
    sal al, 1                                 ; d0 e0
    mov byte [bp-002h], al                    ; 88 46 fe
    mov al, byte [es:di+004h]                 ; 26 8a 45 04
    add byte [bp-002h], al                    ; 00 46 fe
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 6c d8
    mov es, cx                                ; 8e c1
    cmp byte [es:di], 000h                    ; 26 80 3d 00
    je short 03db9h                           ; 74 0f
    mov al, byte [es:di+002h]                 ; 26 8a 45 02
    xor ah, ah                                ; 30 e4
    mov dx, word [bp+016h]                    ; 8b 56 16
    xor dh, dh                                ; 30 f6
    cmp ax, dx                                ; 39 d0
    je short 03de3h                           ; 74 2a
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 f9 da
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+01bh]                    ; 8a 46 1b
    push ax                                   ; 50
    mov ax, 002a2h                            ; b8 a2 02
    push ax                                   ; 50
    mov ax, 002aeh                            ; b8 ae 02
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 1d db
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 04109h                           ; e9 26 03
    mov al, byte [bp+01bh]                    ; 8a 46 1b
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    cmp ax, strict word 00050h                ; 3d 50 00
    jnbe short 03e3ch                         ; 77 4d
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0001eh                ; b9 1e 00
    mov di, 03d09h                            ; bf 09 3d
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+03d26h]               ; 2e 8b 85 26 3d
    mov di, word [bp+01ah]                    ; 8b 7e 1a
    and di, 000ffh                            ; 81 e7 ff 00
    jmp ax                                    ; ff e0
    mov ax, word [bp+01ah]                    ; 8b 46 1a
    xor ah, ah                                ; 30 e4
    or ah, 003h                               ; 80 cc 03
    jmp near 04111h                           ; e9 fb 02
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 e1 d7
    mov cl, al                                ; 88 c1
    mov ah, al                                ; 88 c4
    xor al, al                                ; 30 c0
    or di, ax                                 ; 09 c7
    mov word [bp+01ah], di                    ; 89 7e 1a
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 d9 d7
    test cl, cl                               ; 84 c9
    je short 03e9ch                           ; 74 63
    jmp near 04122h                           ; e9 e6 02
    jmp near 040e6h                           ; e9 a7 02
    mov es, [bp-012h]                         ; 8e 46 ee
    mov di, word [es:si+014h]                 ; 26 8b 7c 14
    mov dx, word [es:si+012h]                 ; 26 8b 54 12
    mov ax, word [es:si+010h]                 ; 26 8b 44 10
    mov word [bp-004h], ax                    ; 89 46 fc
    mov ax, word [es:si+008h]                 ; 26 8b 44 08
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [es:si+00ah]                 ; 26 8b 44 0a
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [bp+018h]                    ; 8b 46 18
    and ax, strict word 0003fh                ; 25 3f 00
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov bx, word [bp+018h]                    ; 8b 5e 18
    and bx, 000c0h                            ; 81 e3 c0 00
    sal bx, 1                                 ; d1 e3
    sal bx, 1                                 ; d1 e3
    mov al, byte [bp+019h]                    ; 8a 46 19
    or ax, bx                                 ; 09 d8
    mov bl, byte [bp+017h]                    ; 8a 5e 17
    xor bh, bh                                ; 30 ff
    mov si, bx                                ; 89 de
    mov cx, word [bp+01ah]                    ; 8b 4e 1a
    xor ch, ch                                ; 30 ed
    mov word [bp-018h], cx                    ; 89 4e e8
    test cx, cx                               ; 85 c9
    je short 03ea9h                           ; 74 1e
    cmp di, word [bp-00ah]                    ; 3b 7e f6
    jc short 03e99h                           ; 72 09
    cmp ax, dx                                ; 39 d0
    jnc short 03e99h                          ; 73 05
    cmp bx, word [bp-004h]                    ; 3b 5e fc
    jc short 03e9fh                           ; 72 06
    jmp near 04109h                           ; e9 6d 02
    jmp near 04033h                           ; e9 94 01
    mov dl, byte [bp+01bh]                    ; 8a 56 1b
    xor dh, dh                                ; 30 f6
    cmp dx, strict byte 00004h                ; 83 fa 04
    jne short 03each                          ; 75 03
    jmp near 0402fh                           ; e9 83 01
    mov CL, strict byte 004h                  ; b1 04
    mov dx, word [bp+014h]                    ; 8b 56 14
    shr dx, CL                                ; d3 ea
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    add bx, dx                                ; 01 d3
    mov word [bp-006h], bx                    ; 89 5e fa
    mov dx, word [bp+014h]                    ; 8b 56 14
    and dx, strict byte 0000fh                ; 83 e2 0f
    mov word [bp-008h], dx                    ; 89 56 f8
    xor dl, dl                                ; 30 d2
    mov bx, word [bp-004h]                    ; 8b 5e fc
    xor cx, cx                                ; 31 c9
    call 08dd2h                               ; e8 04 4f
    xor bx, bx                                ; 31 db
    add ax, si                                ; 01 f0
    adc dx, bx                                ; 11 da
    mov bx, di                                ; 89 fb
    xor cx, cx                                ; 31 c9
    call 08dd2h                               ; e8 f7 4e
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    dec bx                                    ; 4b
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    mov bx, word [bp+01ah]                    ; 8b 5e 1a
    xor bl, bl                                ; 30 db
    mov cx, word [bp-018h]                    ; 8b 4e e8
    or cx, bx                                 ; 09 d9
    mov word [bp+01ah], cx                    ; 89 4e 1a
    mov si, ax                                ; 89 c6
    mov di, dx                                ; 89 d7
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    mov word [bp-01ch], di                    ; 89 7e e4
    mov di, ax                                ; 89 c7
    and di, strict byte 00003h                ; 83 e7 03
    xor bh, bh                                ; 30 ff
    add ax, word [bp-018h]                    ; 03 46 e8
    db  011h, 0dah, 005h, 0ffh, 0ffh, 083h, 0d2h, 0ffh, 089h, 046h, 0e0h, 089h, 056h, 0e2h, 0d1h, 06eh
    db  0e2h, 0d1h, 05eh, 0e0h, 0d1h, 06eh, 0e2h, 0d1h, 05eh, 0e0h, 0b9h, 00ch, 000h, 08ch, 0d2h, 08dh
    db  046h, 0d4h, 0e8h, 08bh, 04eh, 0c7h, 046h, 0d4h, 028h, 000h, 08bh, 046h, 0f2h, 001h, 0f0h, 08bh
    db  056h, 0f4h, 013h, 056h, 0e4h, 086h, 0c4h, 086h, 0d6h, 092h, 089h, 046h, 0d6h, 089h, 056h, 0d8h
    db  08bh, 056h, 0e0h, 029h, 0f2h, 042h, 089h, 0d0h, 086h, 0c4h, 089h, 046h, 0dbh, 0c4h, 05eh, 0eah
    db  026h, 089h, 057h, 00ah, 026h, 0c7h, 047h, 00ch, 000h, 002h, 0b1h, 009h, 0d3h, 0e7h, 089h, 07eh
    db  0f0h, 026h, 089h, 07fh, 01ah, 08bh, 076h, 0e8h, 031h, 0ffh, 0b9h, 009h, 000h, 0d1h, 0e6h, 0d1h
    db  0d7h, 0e2h, 0fah, 089h, 0f0h, 089h, 0fah, 0bbh, 000h, 008h, 031h, 0c9h, 0e8h, 064h, 04dh, 0bah
    db  000h, 008h, 029h, 0dah, 08bh, 05eh, 0eah, 026h, 08bh, 047h, 01ah, 029h, 0c2h, 026h, 089h, 057h
    db  01ch, 080h, 07eh, 0feh, 008h, 076h, 021h, 0ffh, 076h, 0fah, 0ffh, 076h, 0f8h, 0b8h, 001h, 000h
    db  050h, 057h, 056h, 0ffh, 076h, 0f0h, 08ah, 046h, 0feh, 030h, 0e4h, 08ch, 0d1h, 08dh, 05eh, 0d4h
    db  0bah, 00ch, 000h, 0e8h, 023h, 044h, 0ebh, 01fh, 0ffh, 076h, 0fah, 0ffh, 076h, 0f8h, 0b8h, 001h
    db  000h, 050h, 057h, 056h, 0ffh, 076h, 0f0h, 08ah, 046h, 0feh, 030h, 0e4h, 08ch, 0d1h, 08dh, 05eh
    db  0d4h, 0bah, 00ch, 000h, 0e8h, 0f9h, 0e8h, 088h, 0c2h, 0c4h, 05eh, 0eah, 026h, 0c7h, 047h, 01ah
    db  000h, 000h, 026h, 0c7h, 047h, 01ch, 000h, 000h, 084h, 0d2h, 074h, 038h, 0bbh, 016h, 00ch, 08ch
    db  0d9h, 0b8h, 004h, 000h, 0e8h, 0bbh, 0d8h, 088h, 0d0h, 030h, 0e4h, 050h, 08ah, 046h, 01bh, 050h
    db  0b8h, 0a2h, 002h, 050h, 0b8h, 0e4h, 002h, 050h, 0b8h, 004h, 000h, 050h, 0e8h, 0e0h, 0d8h, 083h
    db  0c4h, 00ah, 08bh, 046h, 01ah, 030h, 0e4h, 080h, 0cch, 002h, 089h, 046h, 01ah, 0c6h, 046h, 01ah
    db  000h, 0e9h, 0e5h, 000h, 0c6h, 046h, 01bh, 000h, 031h, 0dbh, 0bah, 074h, 000h, 0b8h, 040h, 000h
    db  0e8h, 0d0h, 0d5h, 080h, 066h, 020h, 0feh, 089h, 0ech, 05dh, 05fh, 05eh, 0c3h, 08eh, 046h, 0eeh
    db  026h, 08bh, 07ch, 014h, 026h, 08bh, 054h, 012h, 04ah, 026h, 08bh, 044h, 010h, 048h, 089h, 046h
    db  0fch, 0c6h, 046h, 01ah, 000h, 08bh, 046h, 014h, 030h, 0c0h, 08bh, 05eh, 018h, 030h, 0ffh, 089h
    db  05eh, 0e6h, 089h, 0d3h, 088h, 0d7h, 030h, 0d3h, 08bh, 04eh, 0e6h, 009h, 0d9h, 089h, 04eh, 018h
    db  0d1h, 0eah, 0d1h, 0eah, 030h, 0f6h, 080h, 0e2h, 0c0h, 083h, 0e7h, 03fh, 009h, 0d7h, 089h, 0cah
    db  030h, 0cah, 009h, 0fah, 089h, 056h, 018h, 08bh, 05eh, 016h, 08ah, 07eh, 0fch, 089h, 05eh, 016h
    db  089h, 0dah, 030h, 0dah, 080h, 0cah, 002h, 089h, 056h, 016h, 026h, 08ah, 054h, 001h, 089h, 046h
    db  014h, 080h, 0fah, 003h, 074h, 01ah, 080h, 0fah, 002h, 074h, 011h, 080h, 0fah, 001h, 075h, 014h
    db  08bh, 046h, 014h, 030h, 0c0h, 00ch, 002h, 089h, 046h, 014h, 0ebh, 008h, 00ch, 004h, 0ebh, 0f7h
    db  00ch, 006h, 0ebh, 0f3h, 0c7h, 046h, 00ch, 0c7h, 0efh, 0c7h, 046h, 00ah, 000h, 0f0h, 0e9h, 053h
    db  0ffh, 081h, 0cfh, 000h, 003h, 089h, 07eh, 01ah, 0e9h, 04dh, 0ffh, 0bbh, 016h, 00ch, 08ch, 0d9h
    db  0b8h, 004h, 000h, 0e8h, 0cch, 0d7h, 08ah, 046h, 01bh, 030h, 0e4h, 050h, 0b8h, 0a2h, 002h, 050h
    db  0b8h, 005h, 003h, 050h, 0b8h, 004h, 000h, 050h, 0e8h, 0f4h, 0d7h, 083h, 0c4h, 008h, 08bh, 046h
    db  01ah, 030h, 0e4h, 080h, 0cch, 001h, 089h, 046h, 01ah, 08ah, 05eh, 01bh, 030h, 0ffh, 0bah, 074h
    db  000h, 0b8h, 040h, 000h, 0e8h, 0ech, 0d4h, 080h, 04eh, 020h, 001h, 0e9h, 019h, 0ffh, 050h, 04eh
    db  049h, 048h, 047h, 046h, 045h, 044h, 043h, 042h, 041h, 018h, 016h, 015h, 014h, 011h, 010h, 00dh
    db  00ch, 00bh, 00ah, 009h, 008h, 005h, 004h, 003h, 002h, 001h, 000h, 06eh, 042h, 017h, 047h, 030h
    db  042h, 06eh, 042h, 025h, 042h, 06eh, 042h, 025h, 042h, 06eh, 042h, 017h, 047h, 06eh, 042h, 06eh
    db  042h, 017h, 047h, 017h, 047h, 017h, 047h, 017h, 047h, 017h, 047h, 052h, 042h, 017h, 047h, 06eh
    db  042h, 05bh, 042h, 08ch, 042h, 025h, 042h, 08ch, 042h, 0edh, 043h, 098h, 044h, 08ch, 042h, 0c9h
    db  044h, 030h, 047h, 038h, 047h, 06eh, 042h
_int13_cdrom:                                ; 0xf4182 LB 0x5eb
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0002ah                ; 83 ec 2a
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 89 d4
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov word [bp-00ah], 00122h                ; c7 46 f6 22 01
    mov word [bp-018h], ax                    ; 89 46 e8
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 65 d4
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    cmp ax, 000e0h                            ; 3d e0 00
    jc short 041b8h                           ; 72 05
    cmp ax, 000f0h                            ; 3d f0 00
    jc short 041d7h                           ; 72 1f
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+01dh]                    ; 8a 46 1d
    push ax                                   ; 50
    mov ax, 00335h                            ; b8 35 03
    push ax                                   ; 50
    mov ax, 00341h                            ; b8 41 03
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 29 d7
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 0474eh                           ; e9 77 05
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    mov es, [bp-018h]                         ; 8e 46 e8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    add bx, ax                                ; 01 c3
    mov dl, byte [es:bx+000d0h]               ; 26 8a 97 d0 00
    mov byte [bp-002h], dl                    ; 88 56 fe
    cmp dl, 010h                              ; 80 fa 10
    jc short 041ffh                           ; 72 0e
    push ax                                   ; 50
    mov al, byte [bp+01dh]                    ; 8a 46 1d
    push ax                                   ; 50
    mov ax, 00335h                            ; b8 35 03
    push ax                                   ; 50
    mov ax, 0036ch                            ; b8 6c 03
    jmp short 041c9h                          ; eb ca
    mov al, byte [bp+01dh]                    ; 8a 46 1d
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    cmp ax, strict word 00050h                ; 3d 50 00
    jnbe short 0426eh                         ; 77 63
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0001eh                ; b9 1e 00
    mov di, 04129h                            ; bf 29 41
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+04146h]               ; 2e 8b 85 46 41
    mov bx, word [bp+01ch]                    ; 8b 5e 1c
    xor bh, bh                                ; 30 ff
    jmp ax                                    ; ff e0
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 003h                               ; 80 cc 03
    jmp near 04756h                           ; e9 26 05
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 c7 d3
    mov cl, al                                ; 88 c1
    mov bh, al                                ; 88 c7
    mov word [bp+01ch], bx                    ; 89 5e 1c
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 c3 d3
    test cl, cl                               ; 84 c9
    je short 0426bh                           ; 74 1c
    jmp near 04767h                           ; e9 15 05
    or bh, 002h                               ; 80 cf 02
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp near 04759h                           ; e9 fe 04
    mov word [bp+016h], 0aa55h                ; c7 46 16 55 aa
    or bh, 030h                               ; 80 cf 30
    mov word [bp+01ch], bx                    ; 89 5e 1c
    mov word [bp+01ah], strict word 00007h    ; c7 46 1a 07 00
    jmp near 0471bh                           ; e9 ad 04
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 44 d6
    mov al, byte [bp+01dh]                    ; 8a 46 1d
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 00335h                            ; b8 35 03
    push ax                                   ; 50
    mov ax, 00289h                            ; b8 89 02
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    jmp short 042cdh                          ; eb 41
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov di, bx                                ; 89 df
    mov [bp-01ah], es                         ; 8c 46 e6
    mov si, word [es:bx+002h]                 ; 26 8b 77 02
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [es:bx+004h]                 ; 26 8b 47 04
    mov word [bp-012h], ax                    ; 89 46 ee
    mov ax, word [es:bx+00ch]                 ; 26 8b 47 0c
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [es:bx+00eh]                 ; 26 8b 47 0e
    mov word [bp-00eh], ax                    ; 89 46 f2
    or ax, word [bp-010h]                     ; 0b 46 f0
    je short 042d7h                           ; 74 1b
    mov al, byte [bp+01dh]                    ; 8a 46 1d
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 00335h                            ; b8 35 03
    push ax                                   ; 50
    mov ax, 0039eh                            ; b8 9e 03
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 29 d6
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 0474eh                           ; e9 77 04
    mov es, [bp-01ah]                         ; 8e 46 e6
    mov ax, word [es:di+008h]                 ; 26 8b 45 08
    mov word [bp-010h], ax                    ; 89 46 f0
    mov ax, word [es:di+00ah]                 ; 26 8b 45 0a
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov al, byte [bp+01dh]                    ; 8a 46 1d
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00044h                ; 3d 44 00
    je short 042f7h                           ; 74 05
    cmp ax, strict word 00047h                ; 3d 47 00
    jne short 042fah                          ; 75 03
    jmp near 04717h                           ; e9 1d 04
    mov cx, strict word 0000ch                ; b9 0c 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-02ah]                         ; 8d 46 d6
    call 08dbbh                               ; e8 b4 4a
    mov word [bp-02ah], strict word 00028h    ; c7 46 d6 28 00
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov dx, word [bp-00eh]                    ; 8b 56 f2
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-028h], ax                    ; 89 46 d8
    mov word [bp-026h], dx                    ; 89 56 da
    mov ax, si                                ; 89 f0
    xchg ah, al                               ; 86 c4
    mov word [bp-023h], ax                    ; 89 46 dd
    mov es, [bp-018h]                         ; 8e 46 e8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov word [es:bx+00ah], si                 ; 26 89 77 0a
    mov word [es:bx+00ch], 00800h             ; 26 c7 47 0c 00 08
    cmp byte [bp-002h], 008h                  ; 80 7e fe 08
    jbe short 04368h                          ; 76 2e
    push word [bp-01ch]                       ; ff 76 e4
    push word [bp-012h]                       ; ff 76 ee
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    mov bx, si                                ; 89 f3
    xor si, si                                ; 31 f6
    mov cx, strict word 0000bh                ; b9 0b 00
    sal bx, 1                                 ; d1 e3
    rcl si, 1                                 ; d1 d6
    loop 0434bh                               ; e2 fa
    push si                                   ; 56
    push bx                                   ; 53
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-02ah]                         ; 8d 5e d6
    mov dx, strict word 0000ch                ; ba 0c 00
    call 083e4h                               ; e8 7e 40
    jmp short 04394h                          ; eb 2c
    push word [bp-01ch]                       ; ff 76 e4
    push word [bp-012h]                       ; ff 76 ee
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    mov bx, si                                ; 89 f3
    xor si, si                                ; 31 f6
    mov cx, strict word 0000bh                ; b9 0b 00
    sal bx, 1                                 ; d1 e3
    rcl si, 1                                 ; d1 d6
    loop 04379h                               ; e2 fa
    push si                                   ; 56
    push bx                                   ; 53
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov cx, ss                                ; 8c d1
    lea bx, [bp-02ah]                         ; 8d 5e d6
    mov dx, strict word 0000ch                ; ba 0c 00
    call 028dbh                               ; e8 47 e5
    mov byte [bp-004h], al                    ; 88 46 fc
    mov es, [bp-018h]                         ; 8e 46 e8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov ax, word [es:bx+016h]                 ; 26 8b 47 16
    mov dx, word [es:bx+018h]                 ; 26 8b 57 18
    mov cx, strict word 0000bh                ; b9 0b 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 043a8h                               ; e2 fa
    mov es, [bp-01ah]                         ; 8e 46 e6
    mov word [es:di+002h], ax                 ; 26 89 45 02
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 04416h                           ; 74 5b
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 f7 d4
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+01dh]                    ; 8a 46 1d
    push ax                                   ; 50
    mov ax, 00335h                            ; b8 35 03
    push ax                                   ; 50
    mov ax, 003c7h                            ; b8 c7 03
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 1b d5
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    jmp near 04756h                           ; e9 69 03
    cmp bx, strict byte 00002h                ; 83 fb 02
    jnbe short 04454h                         ; 77 62
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-018h]                         ; 8e 46 e8
    mov si, word [bp-00ah]                    ; 8b 76 f6
    add si, ax                                ; 01 c6
    mov cl, byte [es:si+021h]                 ; 26 8a 4c 21
    cmp bx, strict byte 00002h                ; 83 fb 02
    je short 04467h                           ; 74 5a
    cmp bx, strict byte 00001h                ; 83 fb 01
    je short 04457h                           ; 74 45
    test bx, bx                               ; 85 db
    je short 04419h                           ; 74 03
    jmp near 04717h                           ; e9 fe 02
    cmp cl, 0ffh                              ; 80 f9 ff
    jne short 04430h                          ; 75 12
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 0b4h                               ; 80 cc b4
    mov word [bp+01ch], ax                    ; 89 46 1c
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    jmp near 04756h                           ; e9 26 03
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    mov es, [bp-018h]                         ; 8e 46 e8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    add bx, ax                                ; 01 c3
    mov byte [es:bx+021h], cl                 ; 26 88 4f 21
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    mov word [bp+01ch], ax                    ; 89 46 1c
    jmp short 04416h                          ; eb c2
    jmp near 0474eh                           ; e9 f7 02
    test cl, cl                               ; 84 c9
    jne short 04469h                          ; 75 0e
    or bh, 0b0h                               ; 80 cf b0
    mov word [bp+01ch], bx                    ; 89 5e 1c
    mov byte [bp+01ch], cl                    ; 88 4e 1c
    jmp near 04759h                           ; e9 f2 02
    jmp short 04481h                          ; eb 18
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9
    mov es, [bp-018h]                         ; 8e 46 e8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    add bx, ax                                ; 01 c3
    mov byte [es:bx+021h], cl                 ; 26 88 4f 21
    test cl, cl                               ; 84 c9
    jne short 04493h                          ; 75 0e
    xor ax, ax                                ; 31 c0
    mov dx, word [bp+01ch]                    ; 8b 56 1c
    xor dl, dl                                ; 30 d2
    or dx, ax                                 ; 09 c2
    mov word [bp+01ch], dx                    ; 89 56 1c
    jmp short 04416h                          ; eb 83
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 04487h                          ; eb ef
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-018h]                         ; 8e 46 e8
    mov si, word [bp-00ah]                    ; 8b 76 f6
    add si, ax                                ; 01 c6
    mov cl, byte [es:si+021h]                 ; 26 8a 4c 21
    test cl, cl                               ; 84 c9
    je short 044b8h                           ; 74 06
    or bh, 0b1h                               ; 80 cf b1
    jmp near 04255h                           ; e9 9d fd
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 044e4h                           ; 74 26
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 0b1h                               ; 80 cc b1
    jmp near 04756h                           ; e9 8d 02
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov cx, word [bp+00ah]                    ; 8b 4e 0a
    mov si, bx                                ; 89 de
    mov word [bp-008h], cx                    ; 89 4e f8
    mov es, cx                                ; 8e c1
    mov ax, word [es:bx]                      ; 26 8b 07
    mov word [bp-00ch], ax                    ; 89 46 f4
    cmp ax, strict word 0001ah                ; 3d 1a 00
    jnc short 044e7h                          ; 73 06
    jmp near 0474eh                           ; e9 6a 02
    jmp near 04717h                           ; e9 30 02
    jc short 0454ch                           ; 72 63
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-018h]                         ; 8e 46 e8
    mov di, word [bp-00ah]                    ; 8b 7e f6
    add di, ax                                ; 01 c7
    mov ax, word [es:di+024h]                 ; 26 8b 45 24
    mov es, cx                                ; 8e c1
    mov word [es:bx], strict word 0001ah      ; 26 c7 07 1a 00
    mov word [es:bx+002h], strict word 00074h ; 26 c7 47 02 74 00
    mov word [es:bx+004h], strict word 0ffffh ; 26 c7 47 04 ff ff
    mov word [es:bx+006h], strict word 0ffffh ; 26 c7 47 06 ff ff
    mov word [es:bx+008h], strict word 0ffffh ; 26 c7 47 08 ff ff
    mov word [es:bx+00ah], strict word 0ffffh ; 26 c7 47 0a ff ff
    mov word [es:bx+00ch], strict word 0ffffh ; 26 c7 47 0c ff ff
    mov word [es:bx+00eh], strict word 0ffffh ; 26 c7 47 0e ff ff
    mov word [es:bx+018h], ax                 ; 26 89 47 18
    mov word [es:bx+010h], strict word 0ffffh ; 26 c7 47 10 ff ff
    mov word [es:bx+012h], strict word 0ffffh ; 26 c7 47 12 ff ff
    mov word [es:bx+014h], strict word 0ffffh ; 26 c7 47 14 ff ff
    mov word [es:bx+016h], strict word 0ffffh ; 26 c7 47 16 ff ff
    cmp word [bp-00ch], strict byte 0001eh    ; 83 7e f4 1e
    jc short 045b6h                           ; 72 64
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si], strict word 0001eh      ; 26 c7 04 1e 00
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    mov word [es:si+01ch], ax                 ; 26 89 44 1c
    mov word [es:si+01ah], 00312h             ; 26 c7 44 1a 12 03
    mov cl, byte [bp-002h]                    ; 8a 4e fe
    xor ch, ch                                ; 30 ed
    mov ax, cx                                ; 89 c8
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    mov es, [bp-018h]                         ; 8e 46 e8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    add bx, ax                                ; 01 c3
    mov ax, word [es:bx+001c2h]               ; 26 8b 87 c2 01
    mov word [bp-016h], ax                    ; 89 46 ea
    mov ax, word [es:bx+001c4h]               ; 26 8b 87 c4 01
    mov word [bp-014h], ax                    ; 89 46 ec
    mov al, byte [es:bx+001c1h]               ; 26 8a 87 c1 01
    mov byte [bp-006h], al                    ; 88 46 fa
    mov ax, cx                                ; 89 c8
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    add bx, ax                                ; 01 c3
    mov al, byte [es:bx+022h]                 ; 26 8a 47 22
    mov di, strict word 00070h                ; bf 70 00
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 045b9h                          ; 75 08
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 045bbh                          ; eb 05
    jmp near 04648h                           ; e9 8f 00
    xor ax, ax                                ; 31 c0
    or di, ax                                 ; 09 c7
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov es, [bp-018h]                         ; 8e 46 e8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov word [es:bx+001f0h], ax               ; 26 89 87 f0 01
    mov ax, word [bp-014h]                    ; 8b 46 ec
    mov word [es:bx+001f2h], ax               ; 26 89 87 f2 01
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    cwd                                       ; 99
    mov bx, strict word 00002h                ; bb 02 00
    idiv bx                                   ; f7 fb
    or dl, 00eh                               ; 80 ca 0e
    mov ax, dx                                ; 89 d0
    mov CL, strict byte 004h                  ; b1 04
    sal ax, CL                                ; d3 e0
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    mov byte [es:bx+001f4h], al               ; 26 88 87 f4 01
    mov byte [es:bx+001f5h], 0cbh             ; 26 c6 87 f5 01 cb
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [es:bx+001f6h], al               ; 26 88 87 f6 01
    mov word [es:bx+001f7h], strict word 00001h ; 26 c7 87 f7 01 01 00
    mov byte [es:bx+001f9h], 000h             ; 26 c6 87 f9 01 00
    mov word [es:bx+001fah], di               ; 26 89 bf fa 01
    mov word [es:bx+001fch], strict word 00000h ; 26 c7 87 fc 01 00 00
    mov byte [es:bx+001feh], 011h             ; 26 c6 87 fe 01 11
    xor bl, bl                                ; 30 db
    xor bh, bh                                ; 30 ff
    jmp short 04627h                          ; eb 05
    cmp bh, 00fh                              ; 80 ff 0f
    jnc short 0463bh                          ; 73 14
    mov dl, bh                                ; 88 fa
    xor dh, dh                                ; 30 f6
    add dx, 00312h                            ; 81 c2 12 03
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    call 01600h                               ; e8 cb cf
    add bl, al                                ; 00 c3
    db  0feh, 0c7h
    ; inc bh                                    ; fe c7
    jmp short 04622h                          ; eb e7
    neg bl                                    ; f6 db
    mov es, [bp-018h]                         ; 8e 46 e8
    mov di, word [bp-00ah]                    ; 8b 7e f6
    mov byte [es:di+001ffh], bl               ; 26 88 9d ff 01
    cmp word [bp-00ch], strict byte 00042h    ; 83 7e f4 42
    jnc short 04651h                          ; 73 03
    jmp near 04717h                           ; e9 c6 00
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    mov es, [bp-018h]                         ; 8e 46 e8
    mov bx, word [bp-00ah]                    ; 8b 5e f6
    add bx, ax                                ; 01 c3
    mov al, byte [es:bx+001c0h]               ; 26 8a 87 c0 01
    mov dx, word [es:bx+001c2h]               ; 26 8b 97 c2 01
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si], strict word 00042h      ; 26 c7 04 42 00
    mov word [es:si+01eh], 0beddh             ; 26 c7 44 1e dd be
    mov word [es:si+020h], strict word 00024h ; 26 c7 44 20 24 00
    mov word [es:si+022h], strict word 00000h ; 26 c7 44 22 00 00
    test al, al                               ; 84 c0
    jne short 0469eh                          ; 75 0c
    mov word [es:si+024h], 05349h             ; 26 c7 44 24 49 53
    mov word [es:si+026h], 02041h             ; 26 c7 44 26 41 20
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+028h], 05441h             ; 26 c7 44 28 41 54
    mov word [es:si+02ah], 02041h             ; 26 c7 44 2a 41 20
    mov word [es:si+02ch], 02020h             ; 26 c7 44 2c 20 20
    mov word [es:si+02eh], 02020h             ; 26 c7 44 2e 20 20
    test al, al                               ; 84 c0
    jne short 046d3h                          ; 75 16
    mov word [es:si+030h], dx                 ; 26 89 54 30
    mov word [es:si+032h], strict word 00000h ; 26 c7 44 32 00 00
    mov word [es:si+034h], strict word 00000h ; 26 c7 44 34 00 00
    mov word [es:si+036h], strict word 00000h ; 26 c7 44 36 00 00
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 001h                  ; 24 01
    xor ah, ah                                ; 30 e4
    mov es, [bp-008h]                         ; 8e 46 f8
    mov word [es:si+038h], ax                 ; 26 89 44 38
    mov word [es:si+03ah], strict word 00000h ; 26 c7 44 3a 00 00
    mov word [es:si+03ch], strict word 00000h ; 26 c7 44 3c 00 00
    mov word [es:si+03eh], strict word 00000h ; 26 c7 44 3e 00 00
    xor al, al                                ; 30 c0
    mov AH, strict byte 01eh                  ; b4 1e
    jmp short 046feh                          ; eb 05
    cmp ah, 040h                              ; 80 fc 40
    jnc short 0470eh                          ; 73 10
    mov bl, ah                                ; 88 e3
    xor bh, bh                                ; 30 ff
    mov es, [bp-008h]                         ; 8e 46 f8
    add bx, si                                ; 01 f3
    add al, byte [es:bx]                      ; 26 02 07
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    jmp short 046f9h                          ; eb eb
    neg al                                    ; f6 d8
    mov es, [bp-008h]                         ; 8e 46 f8
    mov byte [es:si+041h], al                 ; 26 88 44 41
    mov byte [bp+01dh], 000h                  ; c6 46 1d 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 e8 ce
    and byte [bp+022h], 0feh                  ; 80 66 22 fe
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    or bh, 006h                               ; 80 cf 06
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp short 04767h                          ; eb 2f
    cmp bx, strict byte 00006h                ; 83 fb 06
    je short 04717h                           ; 74 da
    cmp bx, strict byte 00001h                ; 83 fb 01
    jc short 0474eh                           ; 72 0c
    jbe short 04717h                          ; 76 d3
    cmp bx, strict byte 00003h                ; 83 fb 03
    jc short 0474eh                           ; 72 05
    cmp bx, strict byte 00004h                ; 83 fb 04
    jbe short 04717h                          ; 76 c9
    mov ax, word [bp+01ch]                    ; 8b 46 1c
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+01ch], ax                    ; 89 46 1c
    mov bl, byte [bp+01dh]                    ; 8a 5e 1d
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 a7 ce
    or byte [bp+022h], 001h                   ; 80 4e 22 01
    jmp short 0472ah                          ; eb bd
print_boot_device_:                          ; 0xf476d LB 0x4e
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    test al, al                               ; 84 c0
    je short 0477ah                           ; 74 05
    mov dx, strict word 00002h                ; ba 02 00
    jmp short 04794h                          ; eb 1a
    test dl, dl                               ; 84 d2
    je short 04783h                           ; 74 05
    mov dx, strict word 00003h                ; ba 03 00
    jmp short 04794h                          ; eb 11
    test bl, 080h                             ; f6 c3 80
    jne short 0478ch                          ; 75 04
    xor dh, dh                                ; 30 f6
    jmp short 04794h                          ; eb 08
    test bl, 080h                             ; f6 c3 80
    je short 047b8h                           ; 74 27
    mov dx, strict word 00001h                ; ba 01 00
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 1e d1
    mov ax, dx                                ; 89 d0
    mov dx, strict word 0000ah                ; ba 0a 00
    imul dx                                   ; f7 ea
    add ax, 00c3ch                            ; 05 3c 0c
    push ax                                   ; 50
    mov ax, 003eah                            ; b8 ea 03
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 45 d1
    add sp, strict byte 00006h                ; 83 c4 06
    pop bp                                    ; 5d
    pop cx                                    ; 59
    retn                                      ; c3
print_boot_failure_:                         ; 0xf47bb LB 0x9c
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dh, cl                                ; 88 ce
    mov cl, bl                                ; 88 d9
    and cl, 07fh                              ; 80 e1 7f
    xor ch, ch                                ; 30 ed
    mov si, cx                                ; 89 ce
    test al, al                               ; 84 c0
    je short 047edh                           ; 74 1f
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 e4 d0
    mov cx, 00c50h                            ; b9 50 0c
    push cx                                   ; 51
    mov cx, 003feh                            ; b9 fe 03
    push cx                                   ; 51
    mov cx, strict word 00004h                ; b9 04 00
    push cx                                   ; 51
    call 018fah                               ; e8 12 d1
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 04835h                          ; eb 48
    test dl, dl                               ; 84 d2
    je short 04801h                           ; 74 10
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 c1 d0
    mov cx, 00c5ah                            ; b9 5a 0c
    jmp short 047dch                          ; eb db
    test bl, 080h                             ; f6 c3 80
    je short 04817h                           ; 74 11
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 ac d0
    push si                                   ; 56
    mov cx, 00c46h                            ; b9 46 0c
    jmp short 04826h                          ; eb 0f
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 9b d0
    push si                                   ; 56
    mov cx, 00c3ch                            ; b9 3c 0c
    push cx                                   ; 51
    mov cx, 00413h                            ; b9 13 04
    push cx                                   ; 51
    mov cx, strict word 00004h                ; b9 04 00
    push cx                                   ; 51
    call 018fah                               ; e8 c8 d0
    add sp, strict byte 00008h                ; 83 c4 08
    cmp byte [bp+006h], 001h                  ; 80 7e 06 01
    jne short 04852h                          ; 75 17
    test dh, dh                               ; 84 f6
    jne short 04844h                          ; 75 05
    mov dx, 0042bh                            ; ba 2b 04
    jmp short 04847h                          ; eb 03
    mov dx, 00455h                            ; ba 55 04
    push dx                                   ; 52
    mov dx, strict word 00007h                ; ba 07 00
    push dx                                   ; 52
    call 018fah                               ; e8 ab d0
    add sp, strict byte 00004h                ; 83 c4 04
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
print_cdromboot_failure_:                    ; 0xf4857 LB 0x27
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, ax                                ; 89 c2
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 53 d0
    push dx                                   ; 52
    mov dx, 0048ah                            ; ba 8a 04
    push dx                                   ; 52
    mov dx, strict word 00004h                ; ba 04 00
    push dx                                   ; 52
    call 018fah                               ; e8 84 d0
    add sp, strict byte 00006h                ; 83 c4 06
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
_int19_function:                             ; 0xf487e LB 0x296
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00010h                ; 83 ec 10
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 8d cd
    mov bx, ax                                ; 89 c3
    mov di, ax                                ; 89 c7
    mov byte [bp-008h], 000h                  ; c6 46 f8 00
    mov ax, strict word 0003dh                ; b8 3d 00
    call 0165ch                               ; e8 bf cd
    mov dl, al                                ; 88 c2
    xor dh, dh                                ; 30 f6
    mov word [bp-00ch], dx                    ; 89 56 f4
    mov ax, strict word 00038h                ; b8 38 00
    call 0165ch                               ; e8 b2 cd
    and AL, strict byte 0f0h                  ; 24 f0
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov byte [bp-009h], dh                    ; 88 76 f7
    mov CL, strict byte 004h                  ; b1 04
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    sal ax, CL                                ; d3 e0
    or dx, ax                                 ; 09 c2
    mov word [bp-00ch], dx                    ; 89 56 f4
    mov ax, strict word 0003ch                ; b8 3c 00
    call 0165ch                               ; e8 98 cd
    and AL, strict byte 00fh                  ; 24 0f
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 00ch                  ; b1 0c
    sal ax, CL                                ; d3 e0
    or word [bp-00ch], ax                     ; 09 46 f4
    mov dx, 00339h                            ; ba 39 03
    mov ax, bx                                ; 89 d8
    call 01600h                               ; e8 29 cd
    test al, al                               ; 84 c0
    je short 048e8h                           ; 74 0d
    mov dx, 00339h                            ; ba 39 03
    mov ax, bx                                ; 89 d8
    call 01600h                               ; e8 1d cd
    xor ah, ah                                ; 30 e4
    mov word [bp-00ch], ax                    ; 89 46 f4
    cmp byte [bp+008h], 001h                  ; 80 7e 08 01
    jne short 048ffh                          ; 75 11
    mov ax, strict word 0003ch                ; b8 3c 00
    call 0165ch                               ; e8 68 cd
    and AL, strict byte 0f0h                  ; 24 f0
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 004h                  ; b1 04
    sar ax, CL                                ; d3 f8
    call 07610h                               ; e8 11 2d
    cmp byte [bp+008h], 002h                  ; 80 7e 08 02
    jne short 0490ah                          ; 75 05
    mov CL, strict byte 004h                  ; b1 04
    shr word [bp-00ch], CL                    ; d3 6e f4
    cmp byte [bp+008h], 003h                  ; 80 7e 08 03
    jne short 04918h                          ; 75 08
    mov al, byte [bp-00bh]                    ; 8a 46 f5
    xor ah, ah                                ; 30 e4
    mov word [bp-00ch], ax                    ; 89 46 f4
    cmp byte [bp+008h], 004h                  ; 80 7e 08 04
    jne short 04923h                          ; 75 05
    mov CL, strict byte 00ch                  ; b1 0c
    shr word [bp-00ch], CL                    ; d3 6e f4
    cmp word [bp-00ch], strict byte 00010h    ; 83 7e f4 10
    jnc short 0492dh                          ; 73 04
    mov byte [bp-008h], 001h                  ; c6 46 f8 01
    xor al, al                                ; 30 c0
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-004h], al                    ; 88 46 fc
    mov byte [bp-006h], al                    ; 88 46 fa
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 7a cf
    push word [bp-00ch]                       ; ff 76 f4
    mov al, byte [bp+008h]                    ; 8a 46 08
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 004aah                            ; b8 aa 04
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 a3 cf
    add sp, strict byte 00008h                ; 83 c4 08
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    and ax, strict word 0000fh                ; 25 0f 00
    cmp ax, strict word 00002h                ; 3d 02 00
    jc short 04973h                           ; 72 0e
    jbe short 04982h                          ; 76 1b
    cmp ax, strict word 00004h                ; 3d 04 00
    je short 0499fh                           ; 74 33
    cmp ax, strict word 00003h                ; 3d 03 00
    je short 04995h                           ; 74 24
    jmp short 049ceh                          ; eb 5b
    cmp ax, strict word 00001h                ; 3d 01 00
    jne short 049ceh                          ; 75 56
    xor al, al                                ; 30 c0
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-004h], al                    ; 88 46 fc
    jmp short 049e2h                          ; eb 60
    mov dx, 00338h                            ; ba 38 03
    mov ax, di                                ; 89 f8
    call 01600h                               ; e8 76 cc
    add AL, strict byte 080h                  ; 04 80
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    jmp short 049e2h                          ; eb 4d
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    mov byte [bp-004h], 001h                  ; c6 46 fc 01
    jmp short 049a9h                          ; eb 0a
    mov byte [bp-006h], 001h                  ; c6 46 fa 01
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 049e2h                           ; 74 39
    call 038fah                               ; e8 4e ef
    mov bx, ax                                ; 89 c3
    test AL, strict byte 0ffh                 ; a8 ff
    je short 049d5h                           ; 74 23
    call 04857h                               ; e8 a2 fe
    mov al, byte [bp-008h]                    ; 8a 46 f8
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    xor dh, dh                                ; 30 f6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov cx, strict word 00001h                ; b9 01 00
    call 047bbh                               ; e8 ed fd
    xor ax, ax                                ; 31 c0
    xor dx, dx                                ; 31 d2
    jmp near 04b0eh                           ; e9 39 01
    mov dx, 0032eh                            ; ba 2e 03
    mov ax, di                                ; 89 f8
    call 0161ch                               ; e8 3f cc
    mov si, ax                                ; 89 c6
    mov byte [bp-002h], bh                    ; 88 7e fe
    cmp byte [bp-006h], 001h                  ; 80 7e fa 01
    jne short 04a3dh                          ; 75 55
    xor si, si                                ; 31 f6
    mov ax, 0e200h                            ; b8 00 e2
    mov es, ax                                ; 8e c0
    cmp word [es:si], 0aa55h                  ; 26 81 3c 55 aa
    jne short 049b5h                          ; 75 bf
    mov cx, ax                                ; 89 c1
    mov si, word [es:si+01ah]                 ; 26 8b 74 1a
    cmp word [es:si+002h], 0506eh             ; 26 81 7c 02 6e 50
    jne short 049b5h                          ; 75 b1
    cmp word [es:si], 05024h                  ; 26 81 3c 24 50
    jne short 049b5h                          ; 75 aa
    mov bx, word [es:si+00eh]                 ; 26 8b 5c 0e
    mov dx, word [es:bx]                      ; 26 8b 17
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02
    cmp ax, 06568h                            ; 3d 68 65
    jne short 04a3fh                          ; 75 24
    cmp dx, 07445h                            ; 81 fa 45 74
    jne short 04a3fh                          ; 75 1e
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    xor dh, dh                                ; 30 f6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    call 0476dh                               ; e8 3a fd
    mov word [bp-010h], strict word 00006h    ; c7 46 f0 06 00
    mov word [bp-00eh], cx                    ; 89 4e f2
    jmp short 04a6ch                          ; eb 2f
    jmp short 04a71h                          ; eb 32
    cmp ax, 06574h                            ; 3d 74 65
    je short 04a47h                           ; 74 03
    jmp near 049b5h                           ; e9 6e ff
    cmp dx, 06e49h                            ; 81 fa 49 6e
    jne short 04a44h                          ; 75 f7
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    xor dh, dh                                ; 30 f6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    call 0476dh                               ; e8 0e fd
    sti                                       ; fb
    mov word [bp-00eh], cx                    ; 89 4e f2
    mov es, cx                                ; 8e c1
    mov ax, word [es:si+01ah]                 ; 26 8b 44 1a
    mov word [bp-010h], ax                    ; 89 46 f0
    call far [bp-010h]                        ; ff 5e f0
    jmp short 04a44h                          ; eb d3
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 04a9bh                          ; 75 24
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 04a9bh                          ; 75 1e
    mov si, 007c0h                            ; be c0 07
    mov es, si                                ; 8e c6
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    mov ax, 00201h                            ; b8 01 02
    mov DH, strict byte 000h                  ; b6 00
    mov cx, strict word 00001h                ; b9 01 00
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    int 013h                                  ; cd 13
    mov ax, strict word 00000h                ; b8 00 00
    sbb ax, strict byte 00000h                ; 83 d8 00
    test ax, ax                               ; 85 c0
    jne short 04a44h                          ; 75 a9
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 04aa5h                           ; 74 04
    xor bl, bl                                ; 30 db
    jmp short 04aa7h                          ; eb 02
    mov BL, strict byte 001h                  ; b3 01
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 04aafh                           ; 74 02
    mov BL, strict byte 001h                  ; b3 01
    xor dx, dx                                ; 31 d2
    mov ax, si                                ; 89 f0
    call 0161ch                               ; e8 66 cb
    mov di, ax                                ; 89 c7
    mov dx, strict word 00002h                ; ba 02 00
    mov ax, si                                ; 89 f0
    call 0161ch                               ; e8 5c cb
    cmp di, ax                                ; 39 c7
    je short 04ad5h                           ; 74 11
    test bl, bl                               ; 84 db
    jne short 04aedh                          ; 75 25
    mov dx, 001feh                            ; ba fe 01
    mov ax, si                                ; 89 f0
    call 0161ch                               ; e8 4c cb
    cmp ax, 0aa55h                            ; 3d 55 aa
    je short 04aedh                           ; 74 18
    mov al, byte [bp-008h]                    ; 8a 46 f8
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    xor dh, dh                                ; 30 f6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor cx, cx                                ; 31 c9
    jmp near 049cbh                           ; e9 de fe
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    xor dh, dh                                ; 30 f6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    call 0476dh                               ; e8 6e fc
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    xor dx, dx                                ; 31 d2
    xor al, al                                ; 30 c0
    add ax, si                                ; 01 f0
    adc dx, bx                                ; 11 da
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
keyboard_panic_:                             ; 0xf4b14 LB 0x14
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    mov ax, 004cah                            ; b8 ca 04
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 d7 cd
    add sp, strict byte 00006h                ; 83 c4 06
    pop bp                                    ; 5d
    retn                                      ; c3
_keyboard_init:                              ; 0xf4b28 LB 0x27a
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov AL, strict byte 0aah                  ; b0 aa
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04b4bh                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04b4bh                          ; 76 08
    xor al, al                                ; 30 c0
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04b34h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04b54h                          ; 75 05
    xor ax, ax                                ; 31 c0
    call 04b14h                               ; e8 c0 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04b6eh                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04b6eh                          ; 76 08
    mov AL, strict byte 001h                  ; b0 01
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04b57h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04b78h                          ; 75 06
    mov ax, strict word 00001h                ; b8 01 00
    call 04b14h                               ; e8 9c ff
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, strict word 00055h                ; 3d 55 00
    je short 04b89h                           ; 74 06
    mov ax, 003dfh                            ; b8 df 03
    call 04b14h                               ; e8 8b ff
    mov AL, strict byte 0abh                  ; b0 ab
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04ba9h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04ba9h                          ; 76 08
    mov AL, strict byte 010h                  ; b0 10
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04b92h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04bb3h                          ; 75 06
    mov ax, strict word 0000ah                ; b8 0a 00
    call 04b14h                               ; e8 61 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04bcdh                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04bcdh                          ; 76 08
    mov AL, strict byte 011h                  ; b0 11
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04bb6h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04bd7h                          ; 75 06
    mov ax, strict word 0000bh                ; b8 0b 00
    call 04b14h                               ; e8 3d ff
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test ax, ax                               ; 85 c0
    je short 04be7h                           ; 74 06
    mov ax, 003e0h                            ; b8 e0 03
    call 04b14h                               ; e8 2d ff
    mov AL, strict byte 0ffh                  ; b0 ff
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04c07h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04c07h                          ; 76 08
    mov AL, strict byte 020h                  ; b0 20
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04bf0h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04c11h                          ; 75 06
    mov ax, strict word 00014h                ; b8 14 00
    call 04b14h                               ; e8 03 ff
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04c2bh                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04c2bh                          ; 76 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04c14h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04c35h                          ; 75 06
    mov ax, strict word 00015h                ; b8 15 00
    call 04b14h                               ; e8 df fe
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 04c46h                           ; 74 06
    mov ax, 003e1h                            ; b8 e1 03
    call 04b14h                               ; e8 ce fe
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04c60h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04c60h                          ; 76 08
    mov AL, strict byte 031h                  ; b0 31
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04c49h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04c6ah                          ; 75 06
    mov ax, strict word 0001fh                ; b8 1f 00
    call 04b14h                               ; e8 aa fe
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000aah                            ; 3d aa 00
    je short 04c83h                           ; 74 0e
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000aah                            ; 3d aa 00
    je short 04c83h                           ; 74 06
    mov ax, 003e2h                            ; b8 e2 03
    call 04b14h                               ; e8 91 fe
    mov AL, strict byte 0f5h                  ; b0 f5
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04ca3h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04ca3h                          ; 76 08
    mov AL, strict byte 040h                  ; b0 40
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04c8ch                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04cadh                          ; 75 06
    mov ax, strict word 00028h                ; b8 28 00
    call 04b14h                               ; e8 67 fe
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04cc7h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04cc7h                          ; 76 08
    mov AL, strict byte 041h                  ; b0 41
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04cb0h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04cd1h                          ; 75 06
    mov ax, strict word 00029h                ; b8 29 00
    call 04b14h                               ; e8 43 fe
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 04ce2h                           ; 74 06
    mov ax, 003e3h                            ; b8 e3 03
    call 04b14h                               ; e8 32 fe
    mov AL, strict byte 060h                  ; b0 60
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04d02h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04d02h                          ; 76 08
    mov AL, strict byte 050h                  ; b0 50
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04cebh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04d0ch                          ; 75 06
    mov ax, strict word 00032h                ; b8 32 00
    call 04b14h                               ; e8 08 fe
    mov AL, strict byte 065h                  ; b0 65
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04d2ch                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04d2ch                          ; 76 08
    mov AL, strict byte 060h                  ; b0 60
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04d15h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04d36h                          ; 75 06
    mov ax, strict word 0003ch                ; b8 3c 00
    call 04b14h                               ; e8 de fd
    mov AL, strict byte 0f4h                  ; b0 f4
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 04d56h                           ; 74 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04d56h                          ; 76 08
    mov AL, strict byte 070h                  ; b0 70
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04d3fh                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04d60h                          ; 75 06
    mov ax, strict word 00046h                ; b8 46 00
    call 04b14h                               ; e8 b4 fd
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 04d7ah                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 04d7ah                          ; 76 08
    mov AL, strict byte 071h                  ; b0 71
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 04d63h                          ; eb e9
    test bx, bx                               ; 85 db
    jne short 04d84h                          ; 75 06
    mov ax, strict word 00046h                ; b8 46 00
    call 04b14h                               ; e8 90 fd
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    je short 04d95h                           ; 74 06
    mov ax, 003e4h                            ; b8 e4 03
    call 04b14h                               ; e8 7f fd
    mov AL, strict byte 0a8h                  ; b0 a8
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    xor ax, ax                                ; 31 c0
    call 06040h                               ; e8 a0 12
    pop bp                                    ; 5d
    retn                                      ; c3
enqueue_key_:                                ; 0xf4da2 LB 0x97
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    mov byte [bp-002h], al                    ; 88 46 fe
    mov bl, dl                                ; 88 d3
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 64 c8
    mov di, ax                                ; 89 c7
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 59 c8
    mov si, ax                                ; 89 c6
    lea cx, [si+002h]                         ; 8d 4c 02
    cmp cx, strict byte 0003eh                ; 83 f9 3e
    jc short 04dd0h                           ; 72 03
    mov cx, strict word 0001eh                ; b9 1e 00
    cmp cx, di                                ; 39 f9
    jne short 04dd8h                          ; 75 04
    xor ax, ax                                ; 31 c0
    jmp short 04e02h                          ; eb 2a
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, si                                ; 89 f2
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 28 c8
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    lea dx, [si+001h]                         ; 8d 54 01
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 1a c8
    mov bx, cx                                ; 89 cb
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 2b c8
    mov ax, strict word 00001h                ; b8 01 00
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
    db  0c6h, 0c5h, 0bah
    ; mov ch, 0bah                              ; c6 c5 ba
    mov ax, 0aab6h                            ; b8 b6 aa
    popfw                                     ; 9d
    push bx                                   ; 53
    inc si                                    ; 46
    inc bp                                    ; 45
    cmp bh, byte [bx+si]                      ; 3a 38
    sub bl, byte [ss:di]                      ; 36 2a 1d
    jnle short 04e6bh                         ; 7f 50
    pop ES                                    ; 07
    dec di                                    ; 4f
    aam 04eh                                  ; d4 4e
    aam 04eh                                  ; d4 4e
    db  08fh, 04fh, 0abh
    ; pop word [bx-055h]                        ; 8f 4f ab
    dec si                                    ; 4e
    add dx, word [bx+si+04fh]                 ; 03 50 4f
    push ax                                   ; 50
    jc short 04e7bh                           ; 72 50
    dec bx                                    ; 4b
    dec di                                    ; 4f
    aam 04eh                                  ; d4 4e
    aam 04eh                                  ; d4 4e
    leave                                     ; c9
    dec di                                    ; 4f
    lds cx, [bp+032h]                         ; c5 4e 32
    push ax                                   ; 50
    db  06bh
    push ax                                   ; 50
_int09_function:                             ; 0xf4e39 LB 0x3a2
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0000eh                ; 83 ec 0e
    mov al, byte [bp+018h]                    ; 8a 46 18
    mov byte [bp-00ah], al                    ; 88 46 f6
    test al, al                               ; 84 c0
    jne short 04e67h                          ; 75 1c
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 67 ca
    mov ax, 004ddh                            ; b8 dd 04
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 99 ca
    add sp, strict byte 00004h                ; 83 c4 04
    jmp near 051d5h                           ; e9 6e 03
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 90 c7
    mov byte [bp-004h], al                    ; 88 46 fc
    mov bl, al                                ; 88 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 82 c7
    mov byte [bp-008h], al                    ; 88 46 f8
    mov byte [bp-006h], al                    ; 88 46 fa
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 73 c7
    mov byte [bp-00eh], al                    ; 88 46 f2
    mov byte [bp-002h], al                    ; 88 46 fe
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00010h                ; b9 10 00
    mov di, 04e0ah                            ; bf 0a 4e
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+04e19h]               ; 2e 8b 85 19 4e
    jmp ax                                    ; ff e0
    xor bl, 040h                              ; 80 f3 40
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 55 c7
    or byte [bp-006h], 040h                   ; 80 4e fa 40
    mov bl, byte [bp-006h]                    ; 8a 5e fa
    xor bh, bh                                ; 30 ff
    jmp near 05044h                           ; e9 7f 01
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 0bfh                  ; 24 bf
    mov byte [bp-006h], al                    ; 88 46 fa
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    jmp near 05044h                           ; e9 70 01
    test byte [bp-002h], 002h                 ; f6 46 fe 02
    jne short 04f04h                          ; 75 2a
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    and AL, strict byte 07fh                  ; 24 7f
    cmp AL, strict byte 02ah                  ; 3c 2a
    jne short 04ee8h                          ; 75 05
    mov ax, strict word 00002h                ; b8 02 00
    jmp short 04eebh                          ; eb 03
    mov ax, strict word 00001h                ; b8 01 00
    test byte [bp-00ah], 080h                 ; f6 46 f6 80
    je short 04ef7h                           ; 74 06
    not al                                    ; f6 d0
    and bl, al                                ; 20 c3
    jmp short 04ef9h                          ; eb 02
    or bl, al                                 ; 08 c3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 0a c7
    jmp near 051b4h                           ; e9 ad 02
    test byte [bp-00eh], 001h                 ; f6 46 f2 01
    jne short 04f48h                          ; 75 3b
    mov al, bl                                ; 88 d8
    or AL, strict byte 004h                   ; 0c 04
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 f0 c6
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    test AL, strict byte 002h                 ; a8 02
    je short 04f33h                           ; 74 0e
    or AL, strict byte 004h                   ; 0c 04
    mov byte [bp-002h], al                    ; 88 46 fe
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    mov dx, 00096h                            ; ba 96 00
    jmp short 04f42h                          ; eb 0f
    mov al, byte [bp-008h]                    ; 8a 46 f8
    or AL, strict byte 001h                   ; 0c 01
    mov byte [bp-006h], al                    ; 88 46 fa
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 c6 c6
    jmp near 051b4h                           ; e9 69 02
    test byte [bp-00eh], 001h                 ; f6 46 f2 01
    jne short 04f8ch                          ; 75 3b
    mov al, bl                                ; 88 d8
    and AL, strict byte 0fbh                  ; 24 fb
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 ac c6
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    test AL, strict byte 002h                 ; a8 02
    je short 04f77h                           ; 74 0e
    and AL, strict byte 0fbh                  ; 24 fb
    mov byte [bp-002h], al                    ; 88 46 fe
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, 00096h                            ; ba 96 00
    jmp short 04f86h                          ; eb 0f
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 0feh                  ; 24 fe
    mov byte [bp-006h], al                    ; 88 46 fa
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 82 c6
    jmp near 051b4h                           ; e9 25 02
    or bl, 008h                               ; 80 cb 08
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 71 c6
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    test AL, strict byte 002h                 ; a8 02
    je short 04fb2h                           ; 74 0e
    or AL, strict byte 008h                   ; 0c 08
    mov byte [bp-002h], al                    ; 88 46 fe
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, 00096h                            ; ba 96 00
    jmp short 04fc1h                          ; eb 0f
    mov al, byte [bp-008h]                    ; 8a 46 f8
    or AL, strict byte 002h                   ; 0c 02
    mov byte [bp-006h], al                    ; 88 46 fa
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 47 c6
    jmp short 04f8ch                          ; eb c3
    and bl, 0f7h                              ; 80 e3 f7
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 37 c6
    mov al, byte [bp-00eh]                    ; 8a 46 f2
    test AL, strict byte 002h                 ; a8 02
    je short 04fech                           ; 74 0e
    and AL, strict byte 0f7h                  ; 24 f7
    mov byte [bp-002h], al                    ; 88 46 fe
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, 00096h                            ; ba 96 00
    jmp short 04ffbh                          ; eb 0f
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 0fdh                  ; 24 fd
    mov byte [bp-006h], al                    ; 88 46 fa
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 0d c6
    jmp short 04f8ch                          ; eb 89
    test byte [bp-00eh], 003h                 ; f6 46 f2 03
    jne short 0504dh                          ; 75 44
    mov al, byte [bp-008h]                    ; 8a 46 f8
    or AL, strict byte 020h                   ; 0c 20
    mov byte [bp-006h], al                    ; 88 46 fa
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 f0 c5
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor AL, strict byte 020h                  ; 34 20
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 de c5
    jmp short 0504dh                          ; eb 1b
    test byte [bp-00eh], 003h                 ; f6 46 f2 03
    jne short 05085h                          ; 75 4d
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 0dfh                  ; 24 df
    mov byte [bp-006h], al                    ; 88 46 fa
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 c1 c5
    jmp short 05085h                          ; eb 36
    mov al, byte [bp-008h]                    ; 8a 46 f8
    or AL, strict byte 010h                   ; 0c 10
    mov byte [bp-006h], al                    ; 88 46 fa
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 aa c5
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor AL, strict byte 010h                  ; 34 10
    jmp short 05023h                          ; eb b8
    mov al, byte [bp-008h]                    ; 8a 46 f8
    and AL, strict byte 0efh                  ; 24 ef
    jmp short 0503dh                          ; eb cb
    mov al, bl                                ; 88 d8
    and AL, strict byte 00ch                  ; 24 0c
    cmp AL, strict byte 00ch                  ; 3c 0c
    jne short 0507fh                          ; 75 05
    jmp far 0f000h:0e05bh                     ; ea 5b e0 00 f0
    test byte [bp-00ah], 080h                 ; f6 46 f6 80
    je short 05088h                           ; 74 03
    jmp near 051b4h                           ; e9 2c 01
    cmp byte [bp-00ah], 058h                  ; 80 7e f6 58
    jbe short 050b0h                          ; 76 22
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 24 c8
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 004f7h                            ; b8 f7 04
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 50 c8
    add sp, strict byte 00006h                ; 83 c4 06
    jmp near 051d5h                           ; e9 25 01
    test bl, 008h                             ; f6 c3 08
    je short 050cfh                           ; 74 1a
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    xor ah, ah                                ; 30 e4
    mov dx, strict word 0000ah                ; ba 0a 00
    imul dx                                   ; f7 ea
    mov si, ax                                ; 89 c6
    mov al, byte [si+00c6ah]                  ; 8a 84 6a 0c
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov al, byte [si+00c6bh]                  ; 8a 84 6b 0c
    jmp near 0517fh                           ; e9 b0 00
    test bl, 004h                             ; f6 c3 04
    je short 050eeh                           ; 74 1a
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    xor ah, ah                                ; 30 e4
    mov dx, strict word 0000ah                ; ba 0a 00
    imul dx                                   ; f7 ea
    mov si, ax                                ; 89 c6
    mov al, byte [si+00c68h]                  ; 8a 84 68 0c
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov al, byte [si+00c69h]                  ; 8a 84 69 0c
    jmp near 0517fh                           ; e9 91 00
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 002h                  ; 24 02
    test al, al                               ; 84 c0
    jbe short 05111h                          ; 76 1a
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    cmp AL, strict byte 047h                  ; 3c 47
    jc short 05111h                           ; 72 13
    cmp AL, strict byte 053h                  ; 3c 53
    jnbe short 05111h                         ; 77 0f
    mov byte [bp-00ch], 0e0h                  ; c6 46 f4 e0
    xor ah, ah                                ; 30 e4
    mov dx, strict word 0000ah                ; ba 0a 00
    imul dx                                   ; f7 ea
    mov si, ax                                ; 89 c6
    jmp short 0517bh                          ; eb 6a
    test bl, 003h                             ; f6 c3 03
    je short 0514dh                           ; 74 37
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    xor ah, ah                                ; 30 e4
    mov dx, strict word 0000ah                ; ba 0a 00
    imul dx                                   ; f7 ea
    mov si, ax                                ; 89 c6
    mov al, byte [si+00c6ch]                  ; 8a 84 6c 0c
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    mov al, bl                                ; 88 d8
    test ax, dx                               ; 85 d0
    je short 0513dh                           ; 74 0d
    mov al, byte [si+00c64h]                  ; 8a 84 64 0c
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov al, byte [si+00c65h]                  ; 8a 84 65 0c
    jmp short 05148h                          ; eb 0b
    mov al, byte [si+00c66h]                  ; 8a 84 66 0c
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov al, byte [si+00c67h]                  ; 8a 84 67 0c
    mov byte [bp-00ah], al                    ; 88 46 f6
    jmp short 05182h                          ; eb 35
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    xor ah, ah                                ; 30 e4
    mov dx, strict word 0000ah                ; ba 0a 00
    imul dx                                   ; f7 ea
    mov si, ax                                ; 89 c6
    mov al, byte [si+00c6ch]                  ; 8a 84 6c 0c
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    mov al, bl                                ; 88 d8
    test ax, dx                               ; 85 d0
    je short 05174h                           ; 74 0d
    mov al, byte [si+00c66h]                  ; 8a 84 66 0c
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov al, byte [si+00c67h]                  ; 8a 84 67 0c
    jmp short 0517fh                          ; eb 0b
    mov al, byte [si+00c64h]                  ; 8a 84 64 0c
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov al, byte [si+00c65h]                  ; 8a 84 65 0c
    mov byte [bp-00ah], al                    ; 88 46 f6
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    jne short 051a7h                          ; 75 1f
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00
    jne short 051a7h                          ; 75 19
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 24 c7
    mov ax, 0052eh                            ; b8 2e 05
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 56 c7
    add sp, strict byte 00004h                ; 83 c4 04
    mov al, byte [bp-00ch]                    ; 8a 46 f4
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    call 04da2h                               ; e8 ee fb
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    and AL, strict byte 07fh                  ; 24 7f
    cmp AL, strict byte 01dh                  ; 3c 1d
    je short 051c1h                           ; 74 04
    and byte [bp-002h], 0feh                  ; 80 66 fe fe
    and byte [bp-002h], 0fdh                  ; 80 66 fe fd
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 39 c4
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
dequeue_key_:                                ; 0xf51db LB 0x93
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push ax                                   ; 50
    mov di, ax                                ; 89 c7
    mov word [bp-002h], dx                    ; 89 56 fe
    mov si, bx                                ; 89 de
    mov word [bp-004h], cx                    ; 89 4e fc
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 27 c4
    mov bx, ax                                ; 89 c3
    mov dx, strict word 0001ch                ; ba 1c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 1c c4
    cmp bx, ax                                ; 39 c3
    je short 05241h                           ; 74 3d
    mov dx, bx                                ; 89 da
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 f4 c3
    mov cl, al                                ; 88 c1
    lea dx, [bx+001h]                         ; 8d 57 01
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 e9 c3
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si], cl                      ; 26 88 0c
    mov es, [bp-002h]                         ; 8e 46 fe
    mov byte [es:di], al                      ; 26 88 05
    cmp word [bp+008h], strict byte 00000h    ; 83 7e 08 00
    je short 0523ch                           ; 74 13
    inc bx                                    ; 43
    inc bx                                    ; 43
    cmp bx, strict byte 0003eh                ; 83 fb 3e
    jc short 05233h                           ; 72 03
    mov bx, strict word 0001eh                ; bb 1e 00
    mov dx, strict word 0001ah                ; ba 1a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 ee c3
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 05243h                          ; eb 02
    xor ax, ax                                ; 31 c0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
    mov byte [01292h], AL                     ; a2 92 12
    adc word [bx+si], dx                      ; 11 10
    or cl, byte [bx+di]                       ; 0a 09
    add ax, 00102h                            ; 05 02 01
    add byte [si+053h], dh                    ; 00 74 53
    das                                       ; 2f
    push bx                                   ; 53
    retn                                      ; c3
    push bx                                   ; 53
    pop ES                                    ; 07
    push sp                                   ; 54
    sbb dl, byte [si+041h]                    ; 1a 54 41
    push sp                                   ; 54
    dec bx                                    ; 4b
    push sp                                   ; 54
    mov dx, 0f154h                            ; ba 54 f1
    push sp                                   ; 54
    and word [di+058h], dx                    ; 21 55 58
    push bp                                   ; 55
    db  0beh
    push bx                                   ; 53
_int16_function:                             ; 0xf526e LB 0x327
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00006h                ; 83 ec 06
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 82 c3
    mov cl, al                                ; 88 c1
    mov bh, al                                ; 88 c7
    mov dx, 00097h                            ; ba 97 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 75 c3
    mov bl, al                                ; 88 c3
    mov dl, cl                                ; 88 ca
    xor dh, dh                                ; 30 f6
    mov CL, strict byte 004h                  ; b1 04
    sar dx, CL                                ; d3 fa
    and dl, 007h                              ; 80 e2 07
    and AL, strict byte 007h                  ; 24 07
    xor ah, ah                                ; 30 e4
    xor al, dl                                ; 30 d0
    test ax, ax                               ; 85 c0
    je short 0530eh                           ; 74 6c
    cli                                       ; fa
    mov AL, strict byte 0edh                  ; b0 ed
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 052bbh                          ; 75 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 052a9h                          ; eb ee
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    jne short 0530dh                          ; 75 47
    and bl, 0f8h                              ; 80 e3 f8
    mov al, bh                                ; 88 f8
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 004h                  ; b1 04
    sar ax, CL                                ; d3 f8
    mov cx, ax                                ; 89 c1
    xor ch, ah                                ; 30 e5
    and cl, 007h                              ; 80 e1 07
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    or dx, cx                                 ; 09 ca
    mov bl, dl                                ; 88 d3
    mov al, dl                                ; 88 d0
    and AL, strict byte 007h                  ; 24 07
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 052fch                          ; 75 08
    mov AL, strict byte 021h                  ; b0 21
    mov dx, 00080h                            ; ba 80 00
    out DX, AL                                ; ee
    jmp short 052eah                          ; eb ee
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor bh, bh                                ; 30 ff
    mov dx, 00097h                            ; ba 97 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 01 c3
    sti                                       ; fb
    mov CL, strict byte 008h                  ; b1 08
    mov ax, word [bp+014h]                    ; 8b 46 14
    shr ax, CL                                ; d3 e8
    cmp ax, 000a2h                            ; 3d a2 00
    jnbe short 05374h                         ; 77 5a
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 0000ch                ; b9 0c 00
    mov di, 0524bh                            ; bf 4b 52
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di+05256h]               ; 2e 8b 85 56 52
    jmp ax                                    ; ff e0
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    mov cx, ss                                ; 8c d1
    lea bx, [bp-004h]                         ; 8d 5e fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 051dbh                               ; e8 9b fe
    test ax, ax                               ; 85 c0
    jne short 05352h                          ; 75 0e
    mov ax, 00565h                            ; b8 65 05
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 ab c5
    add sp, strict byte 00004h                ; 83 c4 04
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 0535eh                           ; 74 06
    cmp byte [bp-004h], 0f0h                  ; 80 7e fc f0
    je short 05364h                           ; 74 06
    cmp byte [bp-004h], 0e0h                  ; 80 7e fc e0
    jne short 05368h                          ; 75 04
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    mov ah, byte [bp-006h]                    ; 8a 66 fa
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov word [bp+014h], ax                    ; 89 46 14
    jmp near 053beh                           ; e9 4a 00
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 3e c5
    mov CL, strict byte 008h                  ; b1 08
    mov ax, word [bp+014h]                    ; 8b 46 14
    shr ax, CL                                ; d3 e8
    push ax                                   ; 50
    mov ax, 00589h                            ; b8 89 05
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 68 c5
    add sp, strict byte 00006h                ; 83 c4 06
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 1d c5
    mov ax, word [bp+010h]                    ; 8b 46 10
    push ax                                   ; 50
    mov ax, word [bp+012h]                    ; 8b 46 12
    push ax                                   ; 50
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    push ax                                   ; 50
    mov ax, word [bp+014h]                    ; 8b 46 14
    push ax                                   ; 50
    mov ax, 005b1h                            ; b8 b1 05
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 3f c5
    add sp, strict byte 0000ch                ; 83 c4 0c
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    retn                                      ; c3
    or word [bp+01eh], 00200h                 ; 81 4e 1e 00 02
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov cx, ss                                ; 8c d1
    lea bx, [bp-004h]                         ; 8d 5e fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 051dbh                               ; e8 03 fe
    test ax, ax                               ; 85 c0
    jne short 053e2h                          ; 75 06
    or word [bp+01eh], strict byte 00040h     ; 83 4e 1e 40
    jmp short 053beh                          ; eb dc
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 053eeh                           ; 74 06
    cmp byte [bp-004h], 0f0h                  ; 80 7e fc f0
    je short 053f4h                           ; 74 06
    cmp byte [bp-004h], 0e0h                  ; 80 7e fc e0
    jne short 053f8h                          ; 75 04
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    mov dh, byte [bp-006h]                    ; 8a 76 fa
    mov dl, byte [bp-004h]                    ; 8a 56 fc
    mov word [bp+014h], dx                    ; 89 56 14
    and word [bp+01eh], strict byte 0ffbfh    ; 83 66 1e bf
    jmp short 053beh                          ; eb b7
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 f0 c1
    mov dx, word [bp+014h]                    ; 8b 56 14
    mov dl, al                                ; 88 c2
    mov word [bp+014h], dx                    ; 89 56 14
    jmp short 053beh                          ; eb a4
    mov dl, byte [bp+012h]                    ; 8a 56 12
    xor dh, dh                                ; 30 f6
    mov CL, strict byte 008h                  ; b1 08
    mov ax, word [bp+012h]                    ; 8b 46 12
    shr ax, CL                                ; d3 e8
    xor ah, ah                                ; 30 e4
    call 04da2h                               ; e8 77 f9
    test ax, ax                               ; 85 c0
    jne short 05439h                          ; 75 0a
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor al, al                                ; 30 c0
    or AL, strict byte 001h                   ; 0c 01
    jmp near 0536eh                           ; e9 35 ff
    and word [bp+014h], 0ff00h                ; 81 66 14 00 ff
    jmp near 053beh                           ; e9 7d ff
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor al, al                                ; 30 c0
    or AL, strict byte 030h                   ; 0c 30
    jmp near 0536eh                           ; e9 23 ff
    mov byte [bp-002h], 002h                  ; c6 46 fe 02
    xor cx, cx                                ; 31 c9
    cli                                       ; fa
    mov AL, strict byte 0f2h                  ; b0 f2
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 05472h                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 05472h                          ; 76 08
    mov dx, 00080h                            ; ba 80 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 0545bh                          ; eb e9
    test bx, bx                               ; 85 db
    jbe short 054b4h                          ; 76 3e
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp ax, 000fah                            ; 3d fa 00
    jne short 054b4h                          ; 75 33
    mov bx, strict word 0ffffh                ; bb ff ff
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 0549bh                          ; 75 0d
    dec bx                                    ; 4b
    test bx, bx                               ; 85 db
    jbe short 0549bh                          ; 76 08
    mov dx, 00080h                            ; ba 80 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 05484h                          ; eb e9
    test bx, bx                               ; 85 db
    jbe short 054abh                          ; 76 0c
    mov bl, ch                                ; 88 eb
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov ch, al                                ; 88 c5
    mov cl, bl                                ; 88 d9
    dec byte [bp-002h]                        ; fe 4e fe
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jnbe short 05481h                         ; 77 cd
    mov word [bp+00eh], cx                    ; 89 4e 0e
    jmp near 053beh                           ; e9 04 ff
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    mov cx, ss                                ; 8c d1
    lea bx, [bp-004h]                         ; 8d 5e fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 051dbh                               ; e8 10 fd
    test ax, ax                               ; 85 c0
    jne short 054ddh                          ; 75 0e
    mov ax, 00565h                            ; b8 65 05
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 20 c4
    add sp, strict byte 00004h                ; 83 c4 04
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 054e6h                          ; 75 03
    jmp near 05368h                           ; e9 82 fe
    cmp byte [bp-004h], 0f0h                  ; 80 7e fc f0
    jne short 054efh                          ; 75 03
    jmp near 05364h                           ; e9 75 fe
    jmp short 054e3h                          ; eb f2
    or word [bp+01eh], 00200h                 ; 81 4e 1e 00 02
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov cx, ss                                ; 8c d1
    lea bx, [bp-004h]                         ; 8d 5e fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 051dbh                               ; e8 d5 fc
    test ax, ax                               ; 85 c0
    jne short 0550dh                          ; 75 03
    jmp near 053dch                           ; e9 cf fe
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 05516h                          ; 75 03
    jmp near 053f8h                           ; e9 e2 fe
    cmp byte [bp-004h], 0f0h                  ; 80 7e fc f0
    jne short 0551fh                          ; 75 03
    jmp near 053f4h                           ; e9 d5 fe
    jmp short 05513h                          ; eb f2
    mov dx, strict word 00017h                ; ba 17 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 d6 c0
    mov dx, word [bp+014h]                    ; 8b 56 14
    mov dl, al                                ; 88 c2
    mov word [bp+014h], dx                    ; 89 56 14
    mov dx, strict word 00018h                ; ba 18 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 c5 c0
    mov bh, al                                ; 88 c7
    and bh, 073h                              ; 80 e7 73
    mov dx, 00096h                            ; ba 96 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 b7 c0
    mov ah, al                                ; 88 c4
    and ah, 00ch                              ; 80 e4 0c
    or ah, bh                                 ; 08 fc
    mov dx, word [bp+014h]                    ; 8b 56 14
    mov dh, ah                                ; 88 e6
    jmp near 05415h                           ; e9 bd fe
    mov ax, word [bp+014h]                    ; 8b 46 14
    xor ah, ah                                ; 30 e4
    or ah, 080h                               ; 80 cc 80
    jmp near 0536eh                           ; e9 0b fe
    sbb ax, 02e56h                            ; 1d 56 2e
    push si                                   ; 56
    push si                                   ; 56
    push si                                   ; 56
    push si                                   ; 56
    push si                                   ; 56
    push si                                   ; 56
    push si                                   ; 56
    cmp byte [bx+si+074h], bl                 ; 38 58 74
    pop cx                                    ; 59
    je short 055cch                           ; 74 59
    push 04e58h                               ; 68 58 4e
    pop cx                                    ; 59
    je short 055d2h                           ; 74 59
    je short 055d4h                           ; 74 59
    dec si                                    ; 4e
    pop cx                                    ; 59
    dec si                                    ; 4e
    pop cx                                    ; 59
    je short 055dah                           ; 74 59
    je short 055dch                           ; 74 59
    into                                      ; ce
    pop ax                                    ; 58
    dec si                                    ; 4e
    pop cx                                    ; 59
    je short 055e2h                           ; 74 59
    je short 055e4h                           ; 74 59
    dec si                                    ; 4e
    pop cx                                    ; 59
    call far [bx+si+074h]                     ; ff 58 74
    pop cx                                    ; 59
    je short 055ech                           ; 74 59
    je short 055eeh                           ; 74 59
_int13_harddisk:                             ; 0xf5595 LB 0x43a
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00010h                ; 83 ec 10
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 78 c0
    mov si, 00122h                            ; be 22 01
    mov word [bp-004h], ax                    ; 89 46 fc
    xor bx, bx                                ; 31 db
    mov dx, 0008eh                            ; ba 8e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 59 c0
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    cmp ax, 00080h                            ; 3d 80 00
    jc short 055c4h                           ; 72 05
    cmp ax, 00090h                            ; 3d 90 00
    jc short 055e3h                           ; 72 1f
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+017h]                    ; 8a 46 17
    push ax                                   ; 50
    mov ax, 005d4h                            ; b8 d4 05
    push ax                                   ; 50
    mov ax, 005e3h                            ; b8 e3 05
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 1d c3
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 0598fh                           ; e9 ac 03
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov dl, byte [es:bx+0011fh]               ; 26 8a 97 1f 01
    mov byte [bp-002h], dl                    ; 88 56 fe
    cmp dl, 010h                              ; 80 fa 10
    jc short 0560ah                           ; 72 0e
    push ax                                   ; 50
    mov al, byte [bp+017h]                    ; 8a 46 17
    push ax                                   ; 50
    mov ax, 005d4h                            ; b8 d4 05
    push ax                                   ; 50
    mov ax, 0060eh                            ; b8 0e 06
    jmp short 055d5h                          ; eb cb
    mov al, byte [bp+017h]                    ; 8a 46 17
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00018h                ; 3d 18 00
    jnbe short 05653h                         ; 77 3f
    mov bx, ax                                ; 89 c3
    sal bx, 1                                 ; d1 e3
    jmp word [cs:bx+05563h]                   ; 2e ff a7 63 55
    cmp byte [bp-002h], 008h                  ; 80 7e fe 08
    jnc short 0562bh                          ; 73 08
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    call 01c2dh                               ; e8 02 c6
    jmp near 05851h                           ; e9 23 02
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 c9 bf
    mov cl, al                                ; 88 c1
    mov dx, word [bp+016h]                    ; 8b 56 16
    mov dh, al                                ; 88 c6
    mov word [bp+016h], dx                    ; 89 56 16
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 c2 bf
    test cl, cl                               ; 84 c9
    je short 056b2h                           ; 74 62
    jmp near 059a8h                           ; e9 55 03
    jmp near 05974h                           ; e9 1e 03
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov al, byte [bp+015h]                    ; 8a 46 15
    mov dx, word [bp+014h]                    ; 8b 56 14
    xor dh, dh                                ; 30 f6
    sal dx, 1                                 ; d1 e2
    sal dx, 1                                 ; d1 e2
    and dh, 003h                              ; 80 e6 03
    mov ah, dh                                ; 88 f4
    mov word [bp-006h], ax                    ; 89 46 fa
    mov di, word [bp+014h]                    ; 8b 7e 14
    and di, strict byte 0003fh                ; 83 e7 3f
    mov al, byte [bp+013h]                    ; 8a 46 13
    xor ah, dh                                ; 30 f4
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    cmp ax, 00080h                            ; 3d 80 00
    jnbe short 0568ch                         ; 77 04
    test ax, ax                               ; 85 c0
    jne short 056b5h                          ; 75 29
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 26 c2
    mov al, byte [bp+017h]                    ; 8a 46 17
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 005d4h                            ; b8 d4 05
    push ax                                   ; 50
    mov ax, 00640h                            ; b8 40 06
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 4e c2
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 0598fh                           ; e9 dd 02
    jmp near 05855h                           ; e9 a0 01
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [es:bx+028h]                 ; 26 8b 47 28
    mov cx, word [es:bx+026h]                 ; 26 8b 4f 26
    mov dx, word [es:bx+02ah]                 ; 26 8b 57 2a
    mov word [bp-00eh], dx                    ; 89 56 f2
    cmp ax, word [bp-006h]                    ; 3b 46 fa
    jbe short 056e3h                          ; 76 09
    cmp cx, word [bp-008h]                    ; 3b 4e f8
    jbe short 056e3h                          ; 76 04
    cmp di, dx                                ; 39 d7
    jbe short 05714h                          ; 76 31
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 cf c1
    push di                                   ; 57
    push word [bp-008h]                       ; ff 76 f8
    push word [bp-006h]                       ; ff 76 fa
    mov ax, word [bp+012h]                    ; 8b 46 12
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+017h]                    ; 8a 46 17
    push ax                                   ; 50
    mov ax, 005d4h                            ; b8 d4 05
    push ax                                   ; 50
    mov ax, 00668h                            ; b8 68 06
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 ec c1
    add sp, strict byte 00010h                ; 83 c4 10
    jmp near 0598fh                           ; e9 7b 02
    mov al, byte [bp+017h]                    ; 8a 46 17
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00004h                ; 3d 04 00
    je short 0573eh                           ; 74 20
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    cmp cx, word [es:bx+02ch]                 ; 26 3b 4f 2c
    jne short 05747h                          ; 75 14
    mov ax, word [es:bx+030h]                 ; 26 8b 47 30
    cmp ax, word [bp-00eh]                    ; 3b 46 f2
    je short 05741h                           ; 74 05
    jmp short 05747h                          ; eb 09
    jmp near 05851h                           ; e9 10 01
    cmp byte [bp-002h], 008h                  ; 80 7e fe 08
    jc short 05776h                           ; 72 2f
    mov ax, word [bp-006h]                    ; 8b 46 fa
    xor dx, dx                                ; 31 d2
    mov bx, cx                                ; 89 cb
    xor cx, cx                                ; 31 c9
    call 08dd2h                               ; e8 7f 36
    xor bx, bx                                ; 31 db
    add ax, word [bp-008h]                    ; 03 46 f8
    adc dx, bx                                ; 11 da
    mov bx, word [bp-00eh]                    ; 8b 5e f2
    xor cx, cx                                ; 31 c9
    call 08dd2h                               ; e8 70 36
    xor bx, bx                                ; 31 db
    add ax, di                                ; 01 f8
    db  011h, 0dah, 005h, 0ffh, 0ffh, 089h, 046h, 0f4h, 083h, 0d2h, 0ffh, 089h, 056h, 0f0h, 031h, 0ffh
    db  08eh, 046h, 0fch, 026h, 0c7h, 044h, 014h, 000h, 000h, 026h, 0c7h, 044h, 016h, 000h, 000h, 026h
    db  0c7h, 044h, 018h, 000h, 000h, 08bh, 046h, 0f4h, 026h, 089h, 004h, 08bh, 046h, 0f0h, 026h, 089h
    db  044h, 002h, 08bh, 056h, 010h, 08bh, 046h, 006h, 026h, 089h, 054h, 004h, 026h, 089h, 044h, 006h
    db  08bh, 046h, 0f6h, 026h, 089h, 044h, 00ah, 026h, 0c7h, 044h, 00ch, 000h, 002h, 08bh, 046h, 0fah
    db  026h, 089h, 044h, 00eh, 08bh, 046h, 0f8h, 026h, 089h, 044h, 010h, 026h, 089h, 07ch, 012h, 08ah
    db  046h, 0feh, 026h, 088h, 044h, 008h, 030h, 0e4h, 0bah, 018h, 000h, 0f7h, 0eah, 089h, 0f3h, 001h
    db  0c3h, 026h, 08ah, 047h, 01eh, 030h, 0e4h, 089h, 0c3h, 0d1h, 0e3h, 0d1h, 0e3h, 08ah, 046h, 017h
    db  0d1h, 0e0h, 001h, 0c3h, 006h, 056h, 0ffh, 097h, 0fch, 0ffh, 089h, 0c2h, 08bh, 046h, 016h, 030h
    db  0c0h, 08eh, 046h, 0fch, 026h, 08bh, 05ch, 014h, 009h, 0c3h, 089h, 05eh, 016h, 084h, 0d2h, 074h
    db  04ah, 0bbh, 016h, 00ch, 08ch, 0d9h, 0b8h, 004h, 000h, 0e8h, 0abh, 0c0h, 088h, 0d0h, 030h, 0e4h
    db  050h, 08ah, 046h, 017h, 050h, 0b8h, 0d4h, 005h, 050h, 0b8h, 0afh, 006h, 050h, 0b8h, 004h, 000h
    db  050h, 0e8h, 0d0h, 0c0h, 083h, 0c4h, 00ah, 08bh, 046h, 016h, 030h, 0e4h, 080h, 0cch, 00ch, 0e9h
    db  05fh, 001h, 0bbh, 016h, 00ch, 08ch, 0d9h, 0b8h, 004h, 000h, 0e8h, 07ah, 0c0h, 0b8h, 0d0h, 006h
    db  050h, 0b8h, 004h, 000h, 050h, 0e8h, 0ach, 0c0h, 083h, 0c4h, 004h, 0c6h, 046h, 017h, 000h, 031h
    db  0dbh, 0bah, 074h, 000h, 0b8h, 040h, 000h, 0e8h, 0aeh, 0bdh, 080h, 066h, 01ch, 0feh, 089h, 0ech
    db  05dh, 0c3h, 08ah, 046h, 0feh, 0bah, 018h, 000h, 0f7h, 0eah, 08eh, 046h, 0fch, 089h, 0f3h, 001h
    db  0c3h, 026h, 08bh, 07fh, 028h, 026h, 08bh, 04fh, 026h, 026h, 08bh, 047h, 02ah, 089h, 046h, 0f2h
    db  026h, 08ah, 094h, 09eh, 001h, 030h, 0f6h, 088h, 076h, 016h, 08bh, 05eh, 014h, 04fh, 089h, 0f8h
    db  088h, 0c7h, 089h, 05eh, 014h, 0d1h, 0efh, 0d1h, 0efh, 081h, 0e7h, 0c0h, 000h, 08bh, 046h, 0f2h
    db  025h, 03fh, 000h, 009h, 0f8h, 030h, 0dbh, 009h, 0c3h, 089h, 05eh, 014h, 08bh, 05eh, 012h, 030h
    db  0ffh, 088h, 0cch, 030h, 0c0h, 02dh, 000h, 001h, 009h, 0c3h, 089h, 05eh, 012h, 089h, 0d8h, 030h
    db  0d8h, 009h, 0d0h, 089h, 046h, 012h, 0ebh, 083h, 08ah, 046h, 0feh, 099h, 02bh, 0c2h, 0d1h, 0f8h
    db  0bah, 006h, 000h, 0f7h, 0eah, 08eh, 046h, 0fch, 001h, 0c6h, 026h, 08bh, 094h, 0c2h, 001h, 083h
    db  0c2h, 007h, 0ech, 02ah, 0e4h, 024h, 0c0h, 03ch, 040h, 075h, 003h, 0e9h, 05dh, 0ffh, 08bh, 046h
    db  016h, 030h, 0e4h, 080h, 0cch, 0aah, 0e9h, 098h, 000h, 08ah, 046h, 0feh, 030h, 0e4h, 0bah, 018h
    db  000h, 0f7h, 0eah, 08eh, 046h, 0fch, 001h, 0c6h, 026h, 08bh, 044h, 02eh, 089h, 046h, 0fah, 026h
    db  08bh, 044h, 02ch, 089h, 046h, 0f8h, 026h, 08bh, 07ch, 030h, 08bh, 046h, 0fah, 031h, 0d2h, 08bh
    db  05eh, 0f8h, 031h, 0c9h, 0e8h, 0a5h, 034h, 089h, 0fbh, 031h, 0c9h, 0e8h, 09eh, 034h, 089h, 046h
    db  0f4h, 089h, 056h, 0f0h, 089h, 056h, 014h, 089h, 046h, 012h, 08bh, 046h, 016h, 030h, 0e4h, 080h
    db  0cch, 003h, 089h, 046h, 016h, 0e9h, 007h, 0ffh, 0bbh, 016h, 00ch, 08ch, 0d9h, 0b8h, 004h, 000h
    db  0e8h, 064h, 0bfh, 08ah, 046h, 017h, 030h, 0e4h, 050h, 0b8h, 0d4h, 005h, 050h, 0b8h, 0eah, 006h
    db  050h, 0b8h, 004h, 000h, 050h, 0e8h, 08ch, 0bfh, 083h, 0c4h, 008h, 0e9h, 0ddh, 0feh, 0bbh, 016h
    db  00ch, 08ch, 0d9h, 0b8h, 004h, 000h, 0e8h, 03eh, 0bfh, 08ah, 046h, 017h, 030h, 0e4h, 050h, 0b8h
    db  0d4h, 005h, 050h, 0b8h, 01dh, 007h, 0e9h, 015h, 0fdh, 08bh, 046h, 016h, 030h, 0e4h, 080h, 0cch
    db  001h, 089h, 046h, 016h, 08ah, 05eh, 017h, 030h, 0ffh, 0bah, 074h, 000h, 0b8h, 040h, 000h, 0e8h
    db  066h, 0bch, 080h, 04eh, 01ch, 001h, 0e9h, 0b5h, 0feh, 066h, 05ah, 07eh, 05ah, 07eh, 05ah, 07eh
    db  05ah, 083h, 05eh, 0e0h, 05bh, 07eh, 05ah, 0e6h, 05bh, 083h, 05eh, 0cfh, 05eh, 0cfh, 05eh, 0cfh
    db  05eh, 0cfh, 05eh, 09ah, 05eh, 0cfh, 05eh, 0cfh, 05eh
_int13_harddisk_ext:                         ; 0xf59cf LB 0x51b
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00026h                ; 83 ec 26
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 3e bc
    mov word [bp-018h], ax                    ; 89 46 e8
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 32 bc
    mov di, 00122h                            ; bf 22 01
    mov word [bp-004h], ax                    ; 89 46 fc
    xor bx, bx                                ; 31 db
    mov dx, 0008eh                            ; ba 8e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 13 bc
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    cmp ax, 00080h                            ; 3d 80 00
    jc short 05a0ah                           ; 72 05
    cmp ax, 00090h                            ; 3d 90 00
    jc short 05a29h                           ; 72 1f
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov al, byte [bp+017h]                    ; 8a 46 17
    push ax                                   ; 50
    mov ax, 0074bh                            ; b8 4b 07
    push ax                                   ; 50
    mov ax, 005e3h                            ; b8 e3 05
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 d7 be
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 05eb0h                           ; e9 87 04
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    xor ah, ah                                ; 30 e4
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, di                                ; 89 fb
    add bx, ax                                ; 01 c3
    mov ch, byte [es:bx+0011fh]               ; 26 8a af 1f 01
    cmp ch, 010h                              ; 80 fd 10
    jc short 05a4dh                           ; 72 0e
    push ax                                   ; 50
    mov al, byte [bp+017h]                    ; 8a 46 17
    push ax                                   ; 50
    mov ax, 0074bh                            ; b8 4b 07
    push ax                                   ; 50
    mov ax, 0060eh                            ; b8 0e 06
    jmp short 05a1bh                          ; eb ce
    mov bl, byte [bp+017h]                    ; 8a 5e 17
    xor bh, bh                                ; 30 ff
    sub bx, strict byte 00041h                ; 83 eb 41
    cmp bx, strict byte 0000fh                ; 83 fb 0f
    jnbe short 05ac4h                         ; 77 6a
    sal bx, 1                                 ; d1 e3
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    jmp word [cs:bx+059afh]                   ; 2e ff a7 af 59
    mov word [bp+010h], 0aa55h                ; c7 46 10 55 aa
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 030h                               ; 80 cc 30
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+014h], strict word 00007h    ; c7 46 14 07 00
    jmp near 05e87h                           ; e9 09 04
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov es, [bp+004h]                         ; 8e 46 04
    mov si, bx                                ; 89 de
    mov [bp-014h], es                         ; 8c 46 ec
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02
    mov word [bp-016h], ax                    ; 89 46 ea
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    mov word [bp-01ah], ax                    ; 89 46 e6
    mov ax, word [es:bx+004h]                 ; 26 8b 47 04
    mov word [bp-024h], ax                    ; 89 46 dc
    mov ax, word [es:bx+00ch]                 ; 26 8b 47 0c
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [es:bx+00eh]                 ; 26 8b 47 0e
    mov word [bp-00ah], ax                    ; 89 46 f6
    or ax, word [bp-00eh]                     ; 0b 46 f2
    je short 05ac7h                           ; 74 16
    mov al, byte [bp+017h]                    ; 8a 46 17
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 0074bh                            ; b8 4b 07
    push ax                                   ; 50
    mov ax, 0075eh                            ; b8 5e 07
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    jmp short 05b1ch                          ; eb 58
    jmp near 05ecfh                           ; e9 08 04
    mov es, [bp-014h]                         ; 8e 46 ec
    mov ax, word [es:si+008h]                 ; 26 8b 44 08
    mov word [bp-00eh], ax                    ; 89 46 f2
    mov ax, word [es:si+00ah]                 ; 26 8b 44 0a
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, di                                ; 89 fb
    add bx, ax                                ; 01 c3
    mov al, byte [es:bx+01eh]                 ; 26 8a 47 1e
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    cmp dx, word [es:bx+034h]                 ; 26 3b 57 34
    jnbe short 05b00h                         ; 77 0b
    jne short 05b26h                          ; 75 2f
    mov dx, word [bp-00eh]                    ; 8b 56 f2
    cmp dx, word [es:bx+032h]                 ; 26 3b 57 32
    jc short 05b26h                           ; 72 26
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 b2 bd
    mov al, byte [bp+017h]                    ; 8a 46 17
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 0074bh                            ; b8 4b 07
    push ax                                   ; 50
    mov ax, 00787h                            ; b8 87 07
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 da bd
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 05eb0h                           ; e9 8a 03
    mov ah, byte [bp+017h]                    ; 8a 66 17
    mov byte [bp-010h], ah                    ; 88 66 f0
    mov byte [bp-00fh], 000h                  ; c6 46 f1 00
    cmp word [bp-010h], strict byte 00044h    ; 83 7e f0 44
    je short 05b3ch                           ; 74 06
    cmp word [bp-010h], strict byte 00047h    ; 83 7e f0 47
    jne short 05b3fh                          ; 75 03
    jmp near 05e83h                           ; e9 44 03
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:di+014h], strict word 00000h ; 26 c7 45 14 00 00
    mov word [es:di+016h], strict word 00000h ; 26 c7 45 16 00 00
    mov word [es:di+018h], strict word 00000h ; 26 c7 45 18 00 00
    mov dx, word [bp-00eh]                    ; 8b 56 f2
    mov word [es:di], dx                      ; 26 89 15
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    mov word [es:di+002h], dx                 ; 26 89 55 02
    mov dx, word [bp-024h]                    ; 8b 56 dc
    mov word [es:di+004h], dx                 ; 26 89 55 04
    mov dx, word [bp-01ah]                    ; 8b 56 e6
    mov word [es:di+006h], dx                 ; 26 89 55 06
    mov dx, word [bp-016h]                    ; 8b 56 ea
    mov word [es:di+00ah], dx                 ; 26 89 55 0a
    mov word [es:di+00ch], 00200h             ; 26 c7 45 0c 00 02
    mov word [es:di+012h], strict word 00000h ; 26 c7 45 12 00 00
    mov byte [es:di+008h], ch                 ; 26 88 6d 08
    mov bx, word [bp-010h]                    ; 8b 5e f0
    sal bx, 1                                 ; d1 e3
    xor ah, ah                                ; 30 e4
    sal ax, 1                                 ; d1 e0
    sal ax, 1                                 ; d1 e0
    add bx, ax                                ; 01 c3
    push ES                                   ; 06
    push di                                   ; 57
    call word [bx-00084h]                     ; ff 97 7c ff
    mov dx, ax                                ; 89 c2
    mov es, [bp-004h]                         ; 8e 46 fc
    mov ax, word [es:di+014h]                 ; 26 8b 45 14
    mov word [bp-016h], ax                    ; 89 46 ea
    mov es, [bp-014h]                         ; 8e 46 ec
    mov word [es:si+002h], ax                 ; 26 89 44 02
    test dl, dl                               ; 84 d2
    je short 05b3ch                           ; 74 8c
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 02 bd
    mov al, dl                                ; 88 d0
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    push word [bp-010h]                       ; ff 76 f0
    mov ax, 0074bh                            ; b8 4b 07
    push ax                                   ; 50
    mov ax, 006afh                            ; b8 af 06
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 28 bd
    add sp, strict byte 0000ah                ; 83 c4 0a
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 00ch                               ; 80 cc 0c
    jmp near 05eb8h                           ; e9 d8 02
    or ah, 0b2h                               ; 80 cc b2
    jmp near 05eb8h                           ; e9 d2 02
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov ax, word [bp+004h]                    ; 8b 46 04
    mov word [bp-008h], ax                    ; 89 46 f8
    mov si, bx                                ; 89 de
    mov word [bp-006h], ax                    ; 89 46 fa
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    mov word [bp-012h], ax                    ; 89 46 ee
    cmp ax, strict word 0001ah                ; 3d 1a 00
    jnc short 05c04h                          ; 73 03
    jmp near 05eb0h                           ; e9 ac 02
    jnc short 05c09h                          ; 73 03
    jmp near 05c8fh                           ; e9 86 00
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, di                                ; 89 fb
    add bx, ax                                ; 01 c3
    mov ax, word [es:bx+02eh]                 ; 26 8b 47 2e
    mov dx, word [es:bx+02ch]                 ; 26 8b 57 2c
    mov word [bp-022h], dx                    ; 89 56 de
    mov dx, word [es:bx+030h]                 ; 26 8b 57 30
    mov word [bp-026h], dx                    ; 89 56 da
    mov dx, word [es:bx+032h]                 ; 26 8b 57 32
    mov word [bp-00eh], dx                    ; 89 56 f2
    mov dx, word [es:bx+034h]                 ; 26 8b 57 34
    mov word [bp-00ah], dx                    ; 89 56 f6
    mov dx, word [es:bx+024h]                 ; 26 8b 57 24
    mov es, [bp-008h]                         ; 8e 46 f8
    mov bx, si                                ; 89 f3
    mov word [es:bx], strict word 0001ah      ; 26 c7 07 1a 00
    mov word [es:bx+002h], strict word 00002h ; 26 c7 47 02 02 00
    mov word [es:bx+004h], ax                 ; 26 89 47 04
    mov word [es:bx+006h], strict word 00000h ; 26 c7 47 06 00 00
    mov ax, word [bp-022h]                    ; 8b 46 de
    mov word [es:bx+008h], ax                 ; 26 89 47 08
    mov word [es:bx+00ah], strict word 00000h ; 26 c7 47 0a 00 00
    mov ax, word [bp-026h]                    ; 8b 46 da
    mov word [es:bx+00ch], ax                 ; 26 89 47 0c
    mov word [es:bx+00eh], strict word 00000h ; 26 c7 47 0e 00 00
    mov word [es:bx+018h], dx                 ; 26 89 57 18
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:bx+010h], ax                 ; 26 89 47 10
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    mov word [es:bx+012h], ax                 ; 26 89 47 12
    mov word [es:bx+014h], strict word 00000h ; 26 c7 47 14 00 00
    mov word [es:bx+016h], strict word 00000h ; 26 c7 47 16 00 00
    cmp word [bp-012h], strict byte 0001eh    ; 83 7e ee 1e
    jc short 05cfbh                           ; 72 66
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:si], strict word 0001eh      ; 26 c7 04 1e 00
    mov ax, word [bp-018h]                    ; 8b 46 e8
    mov word [es:si+01ch], ax                 ; 26 89 44 1c
    mov word [es:si+01ah], 00312h             ; 26 c7 44 1a 12 03
    mov byte [bp-020h], ch                    ; 88 6e e0
    mov byte [bp-01fh], 000h                  ; c6 46 e1 00
    mov ax, word [bp-020h]                    ; 8b 46 e0
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, di                                ; 89 fb
    add bx, ax                                ; 01 c3
    mov ax, word [es:bx+001c2h]               ; 26 8b 87 c2 01
    mov word [bp-01ch], ax                    ; 89 46 e4
    mov ax, word [es:bx+001c4h]               ; 26 8b 87 c4 01
    mov word [bp-01eh], ax                    ; 89 46 e2
    mov al, byte [es:bx+001c1h]               ; 26 8a 87 c1 01
    mov byte [bp-002h], al                    ; 88 46 fe
    mov ax, word [bp-020h]                    ; 8b 46 e0
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov bx, di                                ; 89 fb
    add bx, ax                                ; 01 c3
    mov ah, byte [es:bx+022h]                 ; 26 8a 67 22
    mov al, byte [es:bx+023h]                 ; 26 8a 47 23
    test al, al                               ; 84 c0
    jne short 05cfeh                          ; 75 07
    xor dx, dx                                ; 31 d2
    jmp short 05d01h                          ; eb 06
    jmp near 05db4h                           ; e9 b6 00
    mov dx, strict word 00008h                ; ba 08 00
    or dl, 010h                               ; 80 ca 10
    mov word [bp-00ch], dx                    ; 89 56 f4
    cmp ah, 001h                              ; 80 fc 01
    jne short 05d11h                          ; 75 05
    mov dx, strict word 00001h                ; ba 01 00
    jmp short 05d13h                          ; eb 02
    xor dx, dx                                ; 31 d2
    or word [bp-00ch], dx                     ; 09 56 f4
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 05d1fh                          ; 75 05
    mov dx, strict word 00001h                ; ba 01 00
    jmp short 05d21h                          ; eb 02
    xor dx, dx                                ; 31 d2
    or word [bp-00ch], dx                     ; 09 56 f4
    cmp AL, strict byte 003h                  ; 3c 03
    jne short 05d2dh                          ; 75 05
    mov ax, strict word 00003h                ; b8 03 00
    jmp short 05d2fh                          ; eb 02
    xor ax, ax                                ; 31 c0
    or word [bp-00ch], ax                     ; 09 46 f4
    mov ax, word [bp-01ch]                    ; 8b 46 e4
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:di+001f0h], ax               ; 26 89 85 f0 01
    mov ax, word [bp-01eh]                    ; 8b 46 e2
    mov word [es:di+001f2h], ax               ; 26 89 85 f2 01
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    cwd                                       ; 99
    mov bx, strict word 00002h                ; bb 02 00
    idiv bx                                   ; f7 fb
    or dl, 00eh                               ; 80 ca 0e
    mov CL, strict byte 004h                  ; b1 04
    sal dx, CL                                ; d3 e2
    mov byte [es:di+001f4h], dl               ; 26 88 95 f4 01
    mov byte [es:di+001f5h], 0cbh             ; 26 c6 85 f5 01 cb
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [es:di+001f6h], al               ; 26 88 85 f6 01
    mov word [es:di+001f7h], strict word 00001h ; 26 c7 85 f7 01 01 00
    mov byte [es:di+001f9h], 000h             ; 26 c6 85 f9 01 00
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov word [es:di+001fah], ax               ; 26 89 85 fa 01
    mov word [es:di+001fch], strict word 00000h ; 26 c7 85 fc 01 00 00
    mov byte [es:di+001feh], 011h             ; 26 c6 85 fe 01 11
    xor bl, bl                                ; 30 db
    xor bh, bh                                ; 30 ff
    jmp short 05d96h                          ; eb 05
    cmp bh, 00fh                              ; 80 ff 0f
    jnc short 05daah                          ; 73 14
    mov dl, bh                                ; 88 fa
    xor dh, dh                                ; 30 f6
    add dx, 00312h                            ; 81 c2 12 03
    mov ax, word [bp-018h]                    ; 8b 46 e8
    call 01600h                               ; e8 5c b8
    add bl, al                                ; 00 c3
    db  0feh, 0c7h
    ; inc bh                                    ; fe c7
    jmp short 05d91h                          ; eb e7
    neg bl                                    ; f6 db
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:di+001ffh], bl               ; 26 88 9d ff 01
    cmp word [bp-012h], strict byte 00042h    ; 83 7e ee 42
    jnc short 05dbdh                          ; 73 03
    jmp near 05e83h                           ; e9 c6 00
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00006h                ; ba 06 00
    imul dx                                   ; f7 ea
    mov es, [bp-004h]                         ; 8e 46 fc
    add di, ax                                ; 01 c7
    mov al, byte [es:di+001c0h]               ; 26 8a 85 c0 01
    mov dx, word [es:di+001c2h]               ; 26 8b 95 c2 01
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:si], strict word 00042h      ; 26 c7 04 42 00
    mov word [es:si+01eh], 0beddh             ; 26 c7 44 1e dd be
    mov word [es:si+020h], strict word 00024h ; 26 c7 44 20 24 00
    mov word [es:si+022h], strict word 00000h ; 26 c7 44 22 00 00
    test al, al                               ; 84 c0
    jne short 05e06h                          ; 75 0c
    mov word [es:si+024h], 05349h             ; 26 c7 44 24 49 53
    mov word [es:si+026h], 02041h             ; 26 c7 44 26 41 20
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:si+028h], 05441h             ; 26 c7 44 28 41 54
    mov word [es:si+02ah], 02041h             ; 26 c7 44 2a 41 20
    mov word [es:si+02ch], 02020h             ; 26 c7 44 2c 20 20
    mov word [es:si+02eh], 02020h             ; 26 c7 44 2e 20 20
    test al, al                               ; 84 c0
    jne short 05e3bh                          ; 75 16
    mov word [es:si+030h], dx                 ; 26 89 54 30
    mov word [es:si+032h], strict word 00000h ; 26 c7 44 32 00 00
    mov word [es:si+034h], strict word 00000h ; 26 c7 44 34 00 00
    mov word [es:si+036h], strict word 00000h ; 26 c7 44 36 00 00
    mov al, ch                                ; 88 e8
    and AL, strict byte 001h                  ; 24 01
    xor ah, ah                                ; 30 e4
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:si+038h], ax                 ; 26 89 44 38
    mov word [es:si+03ah], strict word 00000h ; 26 c7 44 3a 00 00
    mov word [es:si+03ch], strict word 00000h ; 26 c7 44 3c 00 00
    mov word [es:si+03eh], strict word 00000h ; 26 c7 44 3e 00 00
    xor bl, bl                                ; 30 db
    mov BH, strict byte 01eh                  ; b7 1e
    jmp short 05e65h                          ; eb 05
    cmp bh, 040h                              ; 80 ff 40
    jnc short 05e7ah                          ; 73 15
    mov al, bh                                ; 88 f8
    xor ah, ah                                ; 30 e4
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, ax                                ; 01 c2
    mov ax, word [bp+004h]                    ; 8b 46 04
    call 01600h                               ; e8 8c b7
    add bl, al                                ; 00 c3
    db  0feh, 0c7h
    ; inc bh                                    ; fe c7
    jmp short 05e60h                          ; eb e6
    neg bl                                    ; f6 db
    mov es, [bp-006h]                         ; 8e 46 fa
    mov byte [es:si+041h], bl                 ; 26 88 5c 41
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    xor bx, bx                                ; 31 db
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 7c b7
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
    cmp ax, strict word 00006h                ; 3d 06 00
    je short 05e83h                           ; 74 e4
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 05eb0h                           ; 72 0c
    jbe short 05e83h                          ; 76 dd
    cmp ax, strict word 00003h                ; 3d 03 00
    jc short 05eb0h                           ; 72 05
    cmp ax, strict word 00004h                ; 3d 04 00
    jbe short 05e83h                          ; 76 d3
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov word [bp+016h], ax                    ; 89 46 16
    mov bl, byte [bp+017h]                    ; 8a 5e 17
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00074h                ; ba 74 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 45 b7
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp short 05e96h                          ; eb c7
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 e3 b9
    mov al, byte [bp+017h]                    ; 8a 46 17
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 0074bh                            ; b8 4b 07
    push ax                                   ; 50
    mov ax, 0071dh                            ; b8 1d 07
    jmp near 05b18h                           ; e9 2e fc
_int14_function:                             ; 0xf5eea LB 0x156
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sti                                       ; fb
    mov dx, word [bp+010h]                    ; 8b 56 10
    sal dx, 1                                 ; d1 e2
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 22 b7
    mov si, ax                                ; 89 c6
    mov bx, ax                                ; 89 c3
    mov dx, word [bp+010h]                    ; 8b 56 10
    add dx, strict byte 0007ch                ; 83 c2 7c
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 f6 b6
    mov cl, al                                ; 88 c1
    cmp word [bp+010h], strict byte 00004h    ; 83 7e 10 04
    jnc short 05f16h                          ; 73 04
    test si, si                               ; 85 f6
    jnbe short 05f19h                         ; 77 03
    jmp near 06039h                           ; e9 20 01
    mov al, byte [bp+015h]                    ; 8a 46 15
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 05f2dh                           ; 72 0d
    jbe short 05f88h                          ; 76 66
    cmp AL, strict byte 003h                  ; 3c 03
    je short 05f80h                           ; 74 5a
    cmp AL, strict byte 002h                  ; 3c 02
    je short 05f83h                           ; 74 59
    jmp near 06032h                           ; e9 05 01
    test al, al                               ; 84 c0
    jne short 05f85h                          ; 75 54
    lea dx, [bx+003h]                         ; 8d 57 03
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    or AL, strict byte 080h                   ; 0c 80
    out DX, AL                                ; ee
    mov al, byte [bp+014h]                    ; 8a 46 14
    and AL, strict byte 0e0h                  ; 24 e0
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 005h                  ; b1 05
    sar ax, CL                                ; d3 f8
    mov cx, ax                                ; 89 c1
    mov ax, 00600h                            ; b8 00 06
    sar ax, CL                                ; d3 f8
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    mov al, ah                                ; 88 e0
    lea dx, [bx+001h]                         ; 8d 57 01
    out DX, AL                                ; ee
    mov al, byte [bp+014h]                    ; 8a 46 14
    and AL, strict byte 01fh                  ; 24 1f
    lea dx, [bx+003h]                         ; 8d 57 03
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+015h], al                    ; 88 46 15
    lea dx, [bx+006h]                         ; 8d 57 06
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+014h], al                    ; 88 46 14
    jmp near 06012h                           ; e9 9f 00
    mov AL, strict byte 017h                  ; b0 17
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+001h]                         ; 8d 57 01
    mov AL, strict byte 004h                  ; b0 04
    out DX, AL                                ; ee
    jmp short 05f55h                          ; eb d5
    jmp near 06021h                           ; e9 9e 00
    jmp short 05fd6h                          ; eb 51
    jmp near 06032h                           ; e9 aa 00
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 8b b6
    mov si, ax                                ; 89 c6
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and ax, strict word 00060h                ; 25 60 00
    cmp ax, strict word 00060h                ; 3d 60 00
    je short 05fb8h                           ; 74 17
    test cl, cl                               ; 84 c9
    je short 05fb8h                           ; 74 13
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 6e b6
    cmp ax, si                                ; 39 f0
    je short 05f93h                           ; 74 e1
    mov si, ax                                ; 89 c6
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9
    jmp short 05f93h                          ; eb db
    test cl, cl                               ; 84 c9
    je short 05fc2h                           ; 74 06
    mov al, byte [bp+014h]                    ; 8a 46 14
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+015h], al                    ; 88 46 15
    test cl, cl                               ; 84 c9
    jne short 06012h                          ; 75 43
    or AL, strict byte 080h                   ; 0c 80
    mov byte [bp+015h], al                    ; 88 46 15
    jmp short 06012h                          ; eb 3c
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 3d b6
    mov si, ax                                ; 89 c6
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 06002h                          ; 75 17
    test cl, cl                               ; 84 c9
    je short 06002h                           ; 74 13
    mov dx, strict word 0006ch                ; ba 6c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 24 b6
    cmp ax, si                                ; 39 f0
    je short 05fe1h                           ; 74 e5
    mov si, ax                                ; 89 c6
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9
    jmp short 05fe1h                          ; eb df
    test cl, cl                               ; 84 c9
    je short 06019h                           ; 74 13
    mov byte [bp+015h], 000h                  ; c6 46 15 00
    mov dx, bx                                ; 89 da
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+014h], al                    ; 88 46 14
    and byte [bp+01eh], 0feh                  ; 80 66 1e fe
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    lea dx, [bx+005h]                         ; 8d 57 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 05fd1h                          ; eb b0
    lea dx, [si+005h]                         ; 8d 54 05
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [bp+015h], al                    ; 88 46 15
    lea dx, [si+006h]                         ; 8d 54 06
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    jmp short 0600fh                          ; eb dd
    or byte [bp+01eh], 001h                   ; 80 4e 1e 01
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    or byte [bp+01eh], 001h                   ; 80 4e 1e 01
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
set_enable_a20_:                             ; 0xf6040 LB 0x2d
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov dx, 00092h                            ; ba 92 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov cl, al                                ; 88 c1
    test bx, bx                               ; 85 db
    je short 06059h                           ; 74 05
    or AL, strict byte 002h                   ; 0c 02
    out DX, AL                                ; ee
    jmp short 0605ch                          ; eb 03
    and AL, strict byte 0fdh                  ; 24 fd
    out DX, AL                                ; ee
    test cl, 002h                             ; f6 c1 02
    je short 06066h                           ; 74 05
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 06068h                          ; eb 02
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
set_e820_range_:                             ; 0xf606d LB 0x8b
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, dx                                ; 89 d6
    mov es, ax                                ; 8e c0
    mov word [es:si], bx                      ; 26 89 1c
    mov word [es:si+002h], cx                 ; 26 89 4c 02
    mov al, byte [bp+00ah]                    ; 8a 46 0a
    xor ah, ah                                ; 30 e4
    mov word [es:si+004h], ax                 ; 26 89 44 04
    mov word [es:si+006h], strict word 00000h ; 26 c7 44 06 00 00
    sub word [bp+006h], bx                    ; 29 5e 06
    sbb word [bp+008h], cx                    ; 19 4e 08
    sub byte [bp+00ch], al                    ; 28 46 0c
    mov ax, word [bp+006h]                    ; 8b 46 06
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov ax, word [bp+008h]                    ; 8b 46 08
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    mov al, byte [bp+00ch]                    ; 8a 46 0c
    xor ah, ah                                ; 30 e4
    mov word [es:si+00ch], ax                 ; 26 89 44 0c
    mov word [es:si+00eh], strict word 00000h ; 26 c7 44 0e 00 00
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov word [es:si+010h], ax                 ; 26 89 44 10
    mov word [es:si+012h], strict word 00000h ; 26 c7 44 12 00 00
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 0000ah                               ; c2 0a 00
    in AL, DX                                 ; ec
    jmp near 0229fh                           ; e9 d8 c1
    sar byte [bx-06f6fh], 089h                ; c0 bf 91 90 89
    mov byte [bx+05283h], al                  ; 88 87 83 52
    dec di                                    ; 4f
    inc cx                                    ; 41
    and AL, strict byte 000h                  ; 24 00
    xchg bx, ax                               ; 93
    db  065h, 034h, 061h
    ; gs xor AL, strict byte 061h               ; 65 34 61
    dec ax                                    ; 48
    popaw                                     ; 61
    frstor [bx+di-01dh]                       ; dd 61 e3
    popaw                                     ; 61
    call 04e42h                               ; e8 61 ed
    popaw                                     ; 61
    xchg bp, ax                               ; 95
    bound bp, [bx]                            ; 62 2f
    db  064h, 04eh
    ; fs dec si                                 ; 64 4e
    fldenv [fs:bx+di-027h]                    ; 64 d9 61 d9
    popaw                                     ; 61
    sbb byte [di+043h], ah                    ; 18 65 43
    db  065h, 056h
    ; gs push si                                ; 65 56
    db  065h, 065h, 065h, 0ddh, 061h, 06eh
    ; frstor [gs:bx+di+06eh]                    ; 65 65 65 dd 61 6e
    db  065h
_int15_function:                             ; 0xf60f8 LB 0x4ce
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov al, byte [bp+017h]                    ; 8a 46 17
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    cmp ax, 000ech                            ; 3d ec 00
    jnbe short 0613eh                         ; 77 35
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00012h                ; b9 12 00
    mov di, 060c3h                            ; bf c3 60
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov si, word [cs:di+060d4h]               ; 2e 8b b5 d4 60
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    mov cx, word [bp+01ch]                    ; 8b 4e 1c
    and cl, 0feh                              ; 80 e1 fe
    mov bx, word [bp+01ch]                    ; 8b 5e 1c
    or bl, 001h                               ; 80 cb 01
    mov dx, ax                                ; 89 c2
    or dh, 086h                               ; 80 ce 86
    jmp si                                    ; ff e6
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    cmp ax, 000c0h                            ; 3d c0 00
    je short 06141h                           ; 74 03
    jmp near 06593h                           ; e9 52 04
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp near 0653ah                           ; e9 f2 03
    mov dx, ax                                ; 89 c2
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 0615dh                           ; 72 0e
    jbe short 06171h                          ; 76 20
    cmp ax, strict word 00003h                ; 3d 03 00
    je short 0619eh                           ; 74 48
    cmp ax, strict word 00002h                ; 3d 02 00
    je short 06181h                           ; 74 26
    jmp short 061abh                          ; eb 4e
    test ax, ax                               ; 85 c0
    jne short 061abh                          ; 75 4a
    xor ax, ax                                ; 31 c0
    call 06040h                               ; e8 da fe
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    jmp near 061d9h                           ; e9 68 00
    mov ax, strict word 00001h                ; b8 01 00
    call 06040h                               ; e8 c9 fe
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], dh                    ; 88 76 17
    jmp near 061d9h                           ; e9 58 00
    mov dx, 00092h                            ; ba 92 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    shr ax, 1                                 ; d1 e8
    and ax, strict word 00001h                ; 25 01 00
    mov dx, word [bp+016h]                    ; 8b 56 16
    mov dl, al                                ; 88 c2
    mov word [bp+016h], dx                    ; 89 56 16
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], ah                    ; 88 66 17
    jmp near 061d9h                           ; e9 3b 00
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov byte [bp+017h], ah                    ; 88 66 17
    mov word [bp+010h], ax                    ; 89 46 10
    jmp near 061d9h                           ; e9 2e 00
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 07 b7
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 007ach                            ; b8 ac 07
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 33 b7
    add sp, strict byte 00006h                ; 83 c4 06
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    mov word [bp+016h], ax                    ; 89 46 16
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp near 0628fh                           ; e9 ac 00
    mov word [bp+01ch], bx                    ; 89 5e 1c
    jmp short 061d9h                          ; eb f1
    mov word [bp+01ch], cx                    ; 89 4e 1c
    jmp short 061d6h                          ; eb e9
    test byte [bp+016h], 0ffh                 ; f6 46 16 ff
    je short 06262h                           ; 74 6f
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 04 b4
    test AL, strict byte 001h                 ; a8 01
    jne short 0625fh                          ; 75 5f
    mov bx, strict word 00001h                ; bb 01 00
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 02 b4
    mov bx, word [bp+018h]                    ; 8b 5e 18
    mov dx, 00098h                            ; ba 98 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 12 b4
    mov bx, word [bp+010h]                    ; 8b 5e 10
    mov dx, 0009ah                            ; ba 9a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 06 b4
    mov bx, word [bp+012h]                    ; 8b 5e 12
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 fa b3
    mov bx, word [bp+014h]                    ; 8b 5e 14
    mov dx, 0009eh                            ; ba 9e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0162ah                               ; e8 ee b3
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov dx, 000a1h                            ; ba a1 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0feh                  ; 24 fe
    out DX, AL                                ; ee
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 0d b4
    mov dl, al                                ; 88 c2
    or dl, 040h                               ; 80 ca 40
    xor dh, dh                                ; 30 f6
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 11 b4
    jmp near 061d9h                           ; e9 7a ff
    jmp near 06531h                           ; e9 cf 02
    cmp ax, strict word 00001h                ; 3d 01 00
    jne short 06283h                          ; 75 1c
    xor bx, bx                                ; 31 db
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 9c b3
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 e0 b3
    mov dl, al                                ; 88 c2
    and dl, 0bfh                              ; 80 e2 bf
    jmp short 06254h                          ; eb d1
    mov word [bp+01ch], bx                    ; 89 5e 1c
    mov ax, dx                                ; 89 d0
    xor ah, dh                                ; 30 f4
    xor dl, dl                                ; 30 d2
    dec ax                                    ; 48
    or dx, ax                                 ; 09 c2
    mov word [bp+016h], dx                    ; 89 56 16
    jmp near 061d9h                           ; e9 44 ff
    cli                                       ; fa
    mov ax, strict word 00001h                ; b8 01 00
    call 06040h                               ; e8 a4 fd
    mov di, ax                                ; 89 c7
    mov CL, strict byte 004h                  ; b1 04
    mov ax, word [bp+018h]                    ; 8b 46 18
    sal ax, CL                                ; d3 e0
    mov si, word [bp+00ah]                    ; 8b 76 0a
    add si, ax                                ; 01 c6
    mov CL, strict byte 00ch                  ; b1 0c
    mov dx, word [bp+018h]                    ; 8b 56 18
    shr dx, CL                                ; d3 ea
    mov cl, dl                                ; 88 d1
    cmp si, ax                                ; 39 c6
    jnc short 062b9h                          ; 73 02
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0002fh                ; bb 2f 00
    call 0162ah                               ; e8 62 b3
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000ah                ; 83 c2 0a
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, si                                ; 89 f3
    call 0162ah                               ; e8 54 b3
    mov bl, cl                                ; 88 cb
    xor bh, bh                                ; 30 ff
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000ch                ; 83 c2 0c
    mov ax, word [bp+018h]                    ; 8b 46 18
    call 0160eh                               ; e8 28 b3
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000dh                ; 83 c2 0d
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, 00093h                            ; bb 93 00
    call 0160eh                               ; e8 19 b3
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0000eh                ; 83 c2 0e
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 27 b3
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00020h                ; 83 c2 20
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0162ah                               ; e8 18 b3
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00022h                ; 83 c2 22
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 0a b3
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00024h                ; 83 c2 24
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0000fh                ; bb 0f 00
    call 0160eh                               ; e8 df b2
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00025h                ; 83 c2 25
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, 0009bh                            ; bb 9b 00
    call 0160eh                               ; e8 d0 b2
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00026h                ; 83 c2 26
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 de b2
    mov ax, ss                                ; 8c d0
    mov CL, strict byte 004h                  ; b1 04
    mov si, ax                                ; 89 c6
    sal si, CL                                ; d3 e6
    mov CL, strict byte 00ch                  ; b1 0c
    shr ax, CL                                ; d3 e8
    mov cx, ax                                ; 89 c1
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00028h                ; 83 c2 28
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0162ah                               ; e8 c1 b2
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0002ah                ; 83 c2 2a
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, si                                ; 89 f3
    call 0162ah                               ; e8 b3 b2
    mov bl, cl                                ; 88 cb
    xor bh, bh                                ; 30 ff
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0002ch                ; 83 c2 2c
    mov ax, word [bp+018h]                    ; 8b 46 18
    call 0160eh                               ; e8 87 b2
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0002dh                ; 83 c2 2d
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, 00093h                            ; bb 93 00
    call 0160eh                               ; e8 78 b2
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0002eh                ; 83 c2 2e
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 86 b2
    mov si, word [bp+00ah]                    ; 8b 76 0a
    mov es, [bp+018h]                         ; 8e 46 18
    mov cx, word [bp+014h]                    ; 8b 4e 14
    push DS                                   ; 1e
    push eax                                  ; 66 50
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov ds, ax                                ; 8e d8
    mov word [00467h], sp                     ; 89 26 67 04
    mov [00469h], ss                          ; 8c 16 69 04
    call 063c0h                               ; e8 00 00
    pop di                                    ; 5f
    add di, strict byte 0001bh                ; 83 c7 1b
    push strict byte 00020h                   ; 6a 20
    push di                                   ; 57
    lgdt [es:si+008h]                         ; 26 0f 01 54 08
    lidt [cs:0efdfh]                          ; 2e 0f 01 1e df ef
    mov eax, cr0                              ; 0f 20 c0
    or AL, strict byte 001h                   ; 0c 01
    mov cr0, eax                              ; 0f 22 c0
    retf                                      ; cb
    mov ax, strict word 00028h                ; b8 28 00
    mov ss, ax                                ; 8e d0
    mov ax, strict word 00010h                ; b8 10 00
    mov ds, ax                                ; 8e d8
    mov ax, strict word 00018h                ; b8 18 00
    mov es, ax                                ; 8e c0
    db  033h, 0f6h
    ; xor si, si                                ; 33 f6
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    cld                                       ; fc
    rep movsw                                 ; f3 a5
    call 063f4h                               ; e8 00 00
    pop ax                                    ; 58
    push 0f000h                               ; 68 00 f0
    add ax, strict byte 00018h                ; 83 c0 18
    push ax                                   ; 50
    mov ax, strict word 00028h                ; b8 28 00
    mov ds, ax                                ; 8e d8
    mov es, ax                                ; 8e c0
    mov eax, cr0                              ; 0f 20 c0
    and AL, strict byte 0feh                  ; 24 fe
    mov cr0, eax                              ; 0f 22 c0
    retf                                      ; cb
    lidt [cs:0efe5h]                          ; 2e 0f 01 1e e5 ef
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    lss sp, [00467h]                          ; 0f b2 26 67 04
    pop eax                                   ; 66 58
    pop DS                                    ; 1f
    mov ax, di                                ; 89 f8
    call 06040h                               ; e8 1d fc
    sti                                       ; fb
    mov byte [bp+017h], 000h                  ; c6 46 17 00
    and byte [bp+01ch], 0feh                  ; 80 66 1c fe
    jmp near 061d9h                           ; e9 aa fd
    mov ax, strict word 00031h                ; b8 31 00
    call 0165ch                               ; e8 27 b2
    mov dh, al                                ; 88 c6
    mov ax, strict word 00030h                ; b8 30 00
    call 0165ch                               ; e8 1f b2
    mov dl, al                                ; 88 c2
    mov word [bp+016h], dx                    ; 89 56 16
    cmp dx, strict byte 0ffc0h                ; 83 fa c0
    jbe short 06428h                          ; 76 e1
    mov word [bp+016h], strict word 0ffc0h    ; c7 46 16 c0 ff
    jmp short 06428h                          ; eb da
    cli                                       ; fa
    mov ax, strict word 00001h                ; b8 01 00
    call 06040h                               ; e8 eb fb
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 00038h                ; 83 c2 38
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0ffffh                ; bb ff ff
    call 0162ah                               ; e8 c6 b1
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0003ah                ; 83 c2 3a
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 b8 b1
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0003ch                ; 83 c2 3c
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, strict word 0000fh                ; bb 0f 00
    call 0160eh                               ; e8 8d b1
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0003dh                ; 83 c2 3d
    mov ax, word [bp+018h]                    ; 8b 46 18
    mov bx, 0009bh                            ; bb 9b 00
    call 0160eh                               ; e8 7e b1
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    add dx, strict byte 0003eh                ; 83 c2 3e
    mov ax, word [bp+018h]                    ; 8b 46 18
    xor bx, bx                                ; 31 db
    call 0162ah                               ; e8 8c b1
    mov AL, strict byte 011h                  ; b0 11
    mov dx, strict word 00020h                ; ba 20 00
    out DX, AL                                ; ee
    mov dx, 000a0h                            ; ba a0 00
    out DX, AL                                ; ee
    mov al, byte [bp+011h]                    ; 8a 46 11
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov ax, word [bp+010h]                    ; 8b 46 10
    mov dx, 000a1h                            ; ba a1 00
    out DX, AL                                ; ee
    mov AL, strict byte 004h                  ; b0 04
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov AL, strict byte 002h                  ; b0 02
    mov dx, 000a1h                            ; ba a1 00
    out DX, AL                                ; ee
    mov AL, strict byte 001h                  ; b0 01
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov dx, 000a1h                            ; ba a1 00
    out DX, AL                                ; ee
    mov AL, strict byte 0ffh                  ; b0 ff
    mov dx, strict word 00021h                ; ba 21 00
    out DX, AL                                ; ee
    mov dx, 000a1h                            ; ba a1 00
    out DX, AL                                ; ee
    mov si, word [bp+00ah]                    ; 8b 76 0a
    call 064dch                               ; e8 00 00
    pop di                                    ; 5f
    add di, strict byte 00018h                ; 83 c7 18
    push strict byte 00038h                   ; 6a 38
    push di                                   ; 57
    lgdt [es:si+008h]                         ; 26 0f 01 54 08
    lidt [es:si+010h]                         ; 26 0f 01 5c 10
    mov ax, strict word 00001h                ; b8 01 00
    lmsw ax                                   ; 0f 01 f0
    retf                                      ; cb
    mov ax, strict word 00028h                ; b8 28 00
    mov ss, ax                                ; 8e d0
    mov ax, strict word 00018h                ; b8 18 00
    mov ds, ax                                ; 8e d8
    mov ax, strict word 00020h                ; b8 20 00
    mov es, ax                                ; 8e c0
    lea ax, [bp+008h]                         ; 8d 46 08
    db  08bh, 0e0h
    ; mov sp, ax                                ; 8b e0
    popaw                                     ; 61
    add sp, strict byte 00006h                ; 83 c4 06
    pop cx                                    ; 59
    pop ax                                    ; 58
    pop ax                                    ; 58
    mov ax, strict word 00030h                ; b8 30 00
    push ax                                   ; 50
    push cx                                   ; 51
    retf                                      ; cb
    jmp near 061d9h                           ; e9 c1 fc
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 9a b3
    mov ax, 007ech                            ; b8 ec 07
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 cc b3
    add sp, strict byte 00004h                ; 83 c4 04
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    mov ax, word [bp+016h]                    ; 8b 46 16
    xor ah, ah                                ; 30 e4
    or ah, 086h                               ; 80 cc 86
    mov word [bp+016h], ax                    ; 89 46 16
    jmp near 061d9h                           ; e9 96 fc
    mov word [bp+01ch], cx                    ; 89 4e 1c
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+010h], 0e6f5h                ; c7 46 10 f5 e6
    mov word [bp+018h], 0f000h                ; c7 46 18 00 f0
    jmp near 061d9h                           ; e9 83 fc
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 bd b0
    mov word [bp+018h], ax                    ; 89 46 18
    jmp near 06428h                           ; e9 c3 fe
    mov ax, 0081bh                            ; b8 1b 08
    push ax                                   ; 50
    mov ax, strict word 00008h                ; b8 08 00
    jmp short 0652ah                          ; eb bc
    test byte [bp+016h], 0ffh                 ; f6 46 16 ff
    jne short 06593h                          ; 75 1f
    mov word [bp+016h], ax                    ; 89 46 16
    mov ax, word [bp+010h]                    ; 8b 46 10
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00001h                ; 3d 01 00
    jc short 0658ch                           ; 72 0b
    cmp ax, strict word 00003h                ; 3d 03 00
    jnbe short 0658ch                         ; 77 06
    mov word [bp+01ch], cx                    ; 89 4e 1c
    jmp near 061d9h                           ; e9 4d fc
    or byte [bp+01ch], 001h                   ; 80 4e 1c 01
    jmp near 061d9h                           ; e9 46 fc
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 1f b3
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    push word [bp+016h]                       ; ff 76 16
    mov ax, 00832h                            ; b8 32 08
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 4a b3
    add sp, strict byte 00008h                ; 83 c4 08
    jmp near 06531h                           ; e9 7b ff
    hlt                                       ; f4
    db  066h, 01eh
    ; push DS                                   ; 66 1e
    db  067h, 075h, 067h
    ; jne short 06623h                          ; 67 75 67
    call far 0d967h:0ba67h                    ; 9a 67 ba 67 d9
    db  067h, 016h
    ; push SS                                   ; 67 16
    push 0684ah                               ; 68 4a 68
_int15_function32:                           ; 0xf65c6 LB 0x317
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00008h                ; 83 ec 08
    mov al, byte [bp+023h]                    ; 8a 46 23
    xor ah, ah                                ; 30 e4
    cmp ax, 000e8h                            ; 3d e8 00
    je short 06616h                           ; 74 3f
    cmp ax, 00086h                            ; 3d 86 00
    jne short 06625h                          ; 75 49
    sti                                       ; fb
    mov ax, word [bp+01eh]                    ; 8b 46 1e
    mov dx, word [bp+01ah]                    ; 8b 56 1a
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    mov ebx, strict dword 00000000fh          ; 66 bb 0f 00 00 00
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    div ebx                                   ; 66 f7 f3
    db  066h, 08bh, 0c8h
    ; mov ecx, eax                              ; 66 8b c8
    in AL, strict byte 061h                   ; e4 61
    and AL, strict byte 010h                  ; 24 10
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    db  066h, 00bh, 0c9h
    ; or ecx, ecx                               ; 66 0b c9
    je near 06613h                            ; 0f 84 0e 00
    in AL, strict byte 061h                   ; e4 61
    and AL, strict byte 010h                  ; 24 10
    db  03ah, 0c4h
    ; cmp al, ah                                ; 3a c4
    je short 06605h                           ; 74 f8
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    dec ecx                                   ; 66 49
    jne short 06605h                          ; 75 f2
    jmp near 06770h                           ; e9 5a 01
    mov ax, word [bp+022h]                    ; 8b 46 22
    xor ah, ah                                ; 30 e4
    cmp ax, strict word 00020h                ; 3d 20 00
    je short 0662bh                           ; 74 0b
    cmp ax, strict word 00001h                ; 3d 01 00
    je short 06628h                           ; 74 03
    jmp near 06743h                           ; e9 1b 01
    jmp near 0689ch                           ; e9 71 02
    cmp word [bp+01ch], 0534dh                ; 81 7e 1c 4d 53
    jne short 06625h                          ; 75 f3
    cmp word [bp+01ah], 04150h                ; 81 7e 1a 50 41
    jne short 06625h                          ; 75 ec
    mov ax, strict word 00035h                ; b8 35 00
    call 0165ch                               ; e8 1d b0
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 06648h                               ; e2 fa
    mov ax, strict word 00034h                ; b8 34 00
    call 0165ch                               ; e8 08 b0
    xor ah, ah                                ; 30 e4
    mov dx, bx                                ; 89 da
    or dx, ax                                 ; 09 c2
    xor bx, bx                                ; 31 db
    add bx, bx                                ; 01 db
    adc dx, 00100h                            ; 81 d2 00 01
    cmp dx, 00100h                            ; 81 fa 00 01
    jc short 0666eh                           ; 72 06
    jne short 0669ch                          ; 75 32
    test bx, bx                               ; 85 db
    jnbe short 0669ch                         ; 77 2e
    mov ax, strict word 00031h                ; b8 31 00
    call 0165ch                               ; e8 e8 af
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 0667dh                               ; e2 fa
    mov ax, strict word 00030h                ; b8 30 00
    call 0165ch                               ; e8 d3 af
    xor ah, ah                                ; 30 e4
    or bx, ax                                 ; 09 c3
    mov cx, strict word 0000ah                ; b9 0a 00
    sal bx, 1                                 ; d1 e3
    rcl dx, 1                                 ; d1 d2
    loop 06690h                               ; e2 fa
    add bx, strict byte 00000h                ; 83 c3 00
    adc dx, strict byte 00010h                ; 83 d2 10
    mov ax, strict word 00062h                ; b8 62 00
    call 0165ch                               ; e8 ba af
    xor ah, ah                                ; 30 e4
    mov word [bp-008h], ax                    ; 89 46 f8
    xor al, al                                ; 30 c0
    mov word [bp-006h], ax                    ; 89 46 fa
    mov cx, strict word 00008h                ; b9 08 00
    sal word [bp-008h], 1                     ; d1 66 f8
    rcl word [bp-006h], 1                     ; d1 56 fa
    loop 066afh                               ; e2 f8
    mov ax, strict word 00061h                ; b8 61 00
    call 0165ch                               ; e8 9f af
    xor ah, ah                                ; 30 e4
    or word [bp-008h], ax                     ; 09 46 f8
    mov ax, word [bp-008h]                    ; 8b 46 f8
    mov word [bp-006h], ax                    ; 89 46 fa
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00
    mov ax, strict word 00063h                ; b8 63 00
    call 0165ch                               ; e8 89 af
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-004h], al                    ; 88 46 fc
    mov ax, word [bp+016h]                    ; 8b 46 16
    cmp ax, strict word 00007h                ; 3d 07 00
    jnbe short 06743h                         ; 77 62
    mov si, ax                                ; 89 c6
    sal si, 1                                 ; d1 e6
    mov ax, bx                                ; 89 d8
    add ax, strict word 00000h                ; 05 00 00
    mov cx, dx                                ; 89 d1
    adc cx, strict byte 0ffffh                ; 83 d1 ff
    jmp word [cs:si+065b6h]                   ; 2e ff a4 b6 65
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    push ax                                   ; 50
    mov ax, strict word 00009h                ; b8 09 00
    push ax                                   ; 50
    mov ax, 0fc00h                            ; b8 00 fc
    push ax                                   ; 50
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 0606dh                               ; e8 5c f9
    mov word [bp+016h], strict word 00001h    ; c7 46 16 01 00
    mov word [bp+018h], strict word 00000h    ; c7 46 18 00 00
    jmp near 06881h                           ; e9 63 01
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    push ax                                   ; 50
    mov ax, strict word 0000ah                ; b8 0a 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    mov bx, 0fc00h                            ; bb 00 fc
    mov cx, strict word 00009h                ; b9 09 00
    call 0606dh                               ; e8 31 f9
    mov word [bp+016h], strict word 00002h    ; c7 46 16 02 00
    jmp short 06716h                          ; eb d3
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 6f b1
    push word [bp+016h]                       ; ff 76 16
    push word [bp+022h]                       ; ff 76 22
    mov ax, 00832h                            ; b8 32 08
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 9b b1
    add sp, strict byte 00008h                ; 83 c4 08
    or byte [bp+02ah], 001h                   ; 80 4e 2a 01
    mov ax, word [bp+022h]                    ; 8b 46 22
    xor al, al                                ; 30 c0
    or AL, strict byte 086h                   ; 0c 86
    mov word [bp+022h], ax                    ; 89 46 22
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    push ax                                   ; 50
    mov ax, strict word 00010h                ; b8 10 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    mov cx, strict word 0000fh                ; b9 0f 00
    call 0606dh                               ; e8 db f8
    mov word [bp+016h], strict word 00003h    ; c7 46 16 03 00
    jmp near 06716h                           ; e9 7c ff
    mov dx, strict word 00001h                ; ba 01 00
    push dx                                   ; 52
    xor dx, dx                                ; 31 d2
    push dx                                   ; 52
    push dx                                   ; 52
    push cx                                   ; 51
    push ax                                   ; 50
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    mov cx, strict word 00010h                ; b9 10 00
    call 0606dh                               ; e8 bb f8
    mov word [bp+016h], strict word 00004h    ; c7 46 16 04 00
    jmp near 06716h                           ; e9 5c ff
    mov si, strict word 00003h                ; be 03 00
    push si                                   ; 56
    xor si, si                                ; 31 f6
    push si                                   ; 56
    push si                                   ; 56
    push dx                                   ; 52
    push bx                                   ; 53
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov si, word [bp+026h]                    ; 8b 76 26
    mov bx, ax                                ; 89 c3
    mov ax, si                                ; 89 f0
    call 0606dh                               ; e8 9c f8
    mov word [bp+016h], strict word 00005h    ; c7 46 16 05 00
    jmp near 06716h                           ; e9 3d ff
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    push ax                                   ; 50
    push ax                                   ; 50
    push ax                                   ; 50
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    mov cx, strict word 0fffch                ; b9 fc ff
    call 0606dh                               ; e8 7c f8
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 067feh                          ; 75 07
    mov ax, word [bp-006h]                    ; 8b 46 fa
    test ax, ax                               ; 85 c0
    je short 0680eh                           ; 74 10
    mov word [bp+016h], strict word 00007h    ; c7 46 16 07 00
    jmp near 06716h                           ; e9 10 ff
    mov word [bp+016h], strict word 00006h    ; c7 46 16 06 00
    jmp near 06716h                           ; e9 08 ff
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+018h], ax                    ; 89 46 18
    jmp short 06881h                          ; eb 6b
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    xor ax, ax                                ; 31 c0
    push ax                                   ; 50
    push ax                                   ; 50
    push ax                                   ; 50
    push ax                                   ; 50
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 0606dh                               ; e8 40 f8
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 0683ah                          ; 75 07
    mov ax, word [bp-006h]                    ; 8b 46 fa
    test ax, ax                               ; 85 c0
    je short 06842h                           ; 74 08
    mov word [bp+016h], strict word 00007h    ; c7 46 16 07 00
    jmp near 06716h                           ; e9 d4 fe
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+018h], ax                    ; 89 46 18
    jmp short 06881h                          ; eb 37
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 06856h                          ; 75 06
    cmp word [bp-006h], strict byte 00000h    ; 83 7e fa 00
    je short 06881h                           ; 74 2b
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    mov al, byte [bp-004h]                    ; 8a 46 fc
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    push word [bp-006h]                       ; ff 76 fa
    push word [bp-008h]                       ; ff 76 f8
    mov dx, word [bp+006h]                    ; 8b 56 06
    mov ax, word [bp+026h]                    ; 8b 46 26
    xor bx, bx                                ; 31 db
    xor cx, cx                                ; 31 c9
    call 0606dh                               ; e8 f4 f7
    xor ax, ax                                ; 31 c0
    mov word [bp+016h], ax                    ; 89 46 16
    mov word [bp+018h], ax                    ; 89 46 18
    mov word [bp+022h], 04150h                ; c7 46 22 50 41
    mov word [bp+024h], 0534dh                ; c7 46 24 4d 53
    mov word [bp+01eh], strict word 00014h    ; c7 46 1e 14 00
    mov word [bp+020h], strict word 00000h    ; c7 46 20 00 00
    and byte [bp+02ah], 0feh                  ; 80 66 2a fe
    jmp near 06770h                           ; e9 d4 fe
    and byte [bp+02ah], 0feh                  ; 80 66 2a fe
    mov ax, strict word 00031h                ; b8 31 00
    call 0165ch                               ; e8 b6 ad
    mov dh, al                                ; 88 c6
    mov ax, strict word 00030h                ; b8 30 00
    call 0165ch                               ; e8 ae ad
    mov dl, al                                ; 88 c2
    mov word [bp+01eh], dx                    ; 89 56 1e
    cmp dx, 03c00h                            ; 81 fa 00 3c
    jbe short 068beh                          ; 76 05
    mov word [bp+01eh], 03c00h                ; c7 46 1e 00 3c
    mov ax, strict word 00035h                ; b8 35 00
    call 0165ch                               ; e8 98 ad
    mov dh, al                                ; 88 c6
    mov ax, strict word 00034h                ; b8 34 00
    call 0165ch                               ; e8 90 ad
    mov dl, al                                ; 88 c2
    mov word [bp+01ah], dx                    ; 89 56 1a
    mov ax, word [bp+01eh]                    ; 8b 46 1e
    mov word [bp+022h], ax                    ; 89 46 22
    mov word [bp+016h], dx                    ; 89 56 16
    jmp near 06770h                           ; e9 93 fe
init_rtc_:                                   ; 0xf68dd LB 0x25
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, strict word 0000ah                ; b8 0a 00
    call 0166dh                               ; e8 83 ad
    mov dx, strict word 00002h                ; ba 02 00
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 7a ad
    mov ax, strict word 0000ch                ; b8 0c 00
    call 0165ch                               ; e8 63 ad
    mov ax, strict word 0000dh                ; b8 0d 00
    call 0165ch                               ; e8 5d ad
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
rtc_updating_:                               ; 0xf6902 LB 0x1f
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, 061a8h                            ; ba a8 61
    dec dx                                    ; 4a
    je short 0691bh                           ; 74 0f
    mov ax, strict word 0000ah                ; b8 0a 00
    call 0165ch                               ; e8 4a ad
    test AL, strict byte 080h                 ; a8 80
    jne short 06909h                          ; 75 f3
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
_int70_function:                             ; 0xf6921 LB 0xae
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 30 ad
    mov bl, al                                ; 88 c3
    mov byte [bp-002h], al                    ; 88 46 fe
    mov ax, strict word 0000ch                ; b8 0c 00
    call 0165ch                               ; e8 25 ad
    mov dl, al                                ; 88 c2
    test bl, 060h                             ; f6 c3 60
    jne short 06941h                          ; 75 03
    jmp near 069c7h                           ; e9 86 00
    test AL, strict byte 020h                 ; a8 20
    je short 06949h                           ; 74 04
    sti                                       ; fb
    int 04ah                                  ; cd 4a
    cli                                       ; fa
    test dl, 040h                             ; f6 c2 40
    je short 069b1h                           ; 74 63
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 a9 ac
    test al, al                               ; 84 c0
    je short 069c7h                           ; 74 6c
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01638h                               ; e8 d4 ac
    test dx, dx                               ; 85 d2
    jne short 069b3h                          ; 75 4b
    cmp ax, 003d1h                            ; 3d d1 03
    jnc short 069b3h                          ; 73 46
    mov dx, 00098h                            ; ba 98 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 a6 ac
    mov si, ax                                ; 89 c6
    mov dx, 0009ah                            ; ba 9a 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 9b ac
    mov cx, ax                                ; 89 c1
    xor bx, bx                                ; 31 db
    mov dx, 000a0h                            ; ba a0 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 80 ac
    mov dl, byte [bp-002h]                    ; 8a 56 fe
    and dl, 037h                              ; 80 e2 37
    xor dh, dh                                ; 30 f6
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 d1 ac
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 01600h                               ; e8 5d ac
    mov bl, al                                ; 88 c3
    or bl, 080h                               ; 80 cb 80
    xor bh, bh                                ; 30 ff
    mov dx, cx                                ; 89 ca
    mov ax, si                                ; 89 f0
    call 0160eh                               ; e8 5d ac
    jmp short 069c7h                          ; eb 14
    mov bx, ax                                ; 89 c3
    add bx, 0fc2fh                            ; 81 c3 2f fc
    mov cx, dx                                ; 89 d1
    adc cx, strict byte 0ffffh                ; 83 d1 ff
    mov dx, 0009ch                            ; ba 9c 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0164ah                               ; e8 83 ac
    call 0e03bh                               ; e8 71 76
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
_int1a_function_pci:                         ; 0xf69cf LB 0x31
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 00021h                ; ba 21 00
    mov ax, strict word 00004h                ; b8 04 00
    call 01757h                               ; e8 7c ad
    mov dx, strict word 0000ah                ; ba 0a 00
    mov ax, strict word 00004h                ; b8 04 00
    call 01757h                               ; e8 73 ad
    test byte [bp+018h], 001h                 ; f6 46 18 01
    jne short 069eeh                          ; 75 04
    or byte [bp+018h], 001h                   ; 80 4e 18 01
    pop bp                                    ; 5d
    retn                                      ; c3
    push SS                                   ; 16
    push strict byte 0003dh                   ; 6a 3d
    push strict byte 00062h                   ; 6a 62
    push strict byte 0ff9bh                   ; 6a 9b
    push strict byte 0ffedh                   ; 6a ed
    push strict byte 00023h                   ; 6a 23
    imul bp, word [bx+di+06bh], strict byte 0ffc2h ; 6b 69 6b c2
    db  06bh
_int1a_function:                             ; 0xf6a00 LB 0x1d4
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sti                                       ; fb
    mov al, byte [bp+013h]                    ; 8a 46 13
    cmp AL, strict byte 007h                  ; 3c 07
    jnbe short 06a69h                         ; 77 5e
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    sal bx, 1                                 ; d1 e3
    jmp word [cs:bx+069f0h]                   ; 2e ff a7 f0 69
    cli                                       ; fa
    mov bx, 0046eh                            ; bb 6e 04
    xor ax, ax                                ; 31 c0
    mov es, ax                                ; 8e c0
    mov ax, word [es:bx]                      ; 26 8b 07
    mov word [bp+010h], ax                    ; 89 46 10
    mov bx, 0046ch                            ; bb 6c 04
    mov ax, word [es:bx]                      ; 26 8b 07
    mov word [bp+00eh], ax                    ; 89 46 0e
    mov bx, 00470h                            ; bb 70 04
    mov al, byte [es:bx]                      ; 26 8a 07
    mov byte [bp+012h], al                    ; 88 46 12
    mov byte [es:bx], 000h                    ; 26 c6 07 00
    sti                                       ; fb
    pop bp                                    ; 5d
    retn                                      ; c3
    cli                                       ; fa
    mov bx, 0046eh                            ; bb 6e 04
    xor ax, ax                                ; 31 c0
    mov es, ax                                ; 8e c0
    mov ax, word [bp+010h]                    ; 8b 46 10
    mov word [es:bx], ax                      ; 26 89 07
    mov bx, 0046ch                            ; bb 6c 04
    mov ax, word [bp+00eh]                    ; 8b 46 0e
    mov word [es:bx], ax                      ; 26 89 07
    mov bx, 00470h                            ; bb 70 04
    mov byte [es:bx], 000h                    ; 26 c6 07 00
    sti                                       ; fb
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    pop bp                                    ; 5d
    retn                                      ; c3
    call 06902h                               ; e8 9d fe
    test ax, ax                               ; 85 c0
    je short 06a6bh                           ; 74 02
    pop bp                                    ; 5d
    retn                                      ; c3
    xor ax, ax                                ; 31 c0
    call 0165ch                               ; e8 ec ab
    mov byte [bp+00fh], al                    ; 88 46 0f
    mov ax, strict word 00002h                ; b8 02 00
    call 0165ch                               ; e8 e3 ab
    mov byte [bp+010h], al                    ; 88 46 10
    mov ax, strict word 00004h                ; b8 04 00
    call 0165ch                               ; e8 da ab
    mov dl, al                                ; 88 c2
    mov byte [bp+011h], al                    ; 88 46 11
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 cf ab
    and AL, strict byte 001h                  ; 24 01
    mov byte [bp+00eh], al                    ; 88 46 0e
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    mov byte [bp+012h], dl                    ; 88 56 12
    pop bp                                    ; 5d
    retn                                      ; c3
    call 06902h                               ; e8 64 fe
    test ax, ax                               ; 85 c0
    je short 06aa5h                           ; 74 03
    call 068ddh                               ; e8 38 fe
    mov dl, byte [bp+00fh]                    ; 8a 56 0f
    xor dh, dh                                ; 30 f6
    xor ax, ax                                ; 31 c0
    call 0166dh                               ; e8 be ab
    mov dl, byte [bp+010h]                    ; 8a 56 10
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00002h                ; b8 02 00
    call 0166dh                               ; e8 b3 ab
    mov dl, byte [bp+011h]                    ; 8a 56 11
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00004h                ; b8 04 00
    call 0166dh                               ; e8 a8 ab
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 91 ab
    mov bl, al                                ; 88 c3
    and bl, 060h                              ; 80 e3 60
    or bl, 002h                               ; 80 cb 02
    mov al, byte [bp+00eh]                    ; 8a 46 0e
    and AL, strict byte 001h                  ; 24 01
    or bl, al                                 ; 08 c3
    mov dl, bl                                ; 88 da
    xor dh, dh                                ; 30 f6
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 89 ab
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    mov byte [bp+012h], bl                    ; 88 5e 12
    pop bp                                    ; 5d
    retn                                      ; c3
    mov byte [bp+013h], 000h                  ; c6 46 13 00
    call 06902h                               ; e8 0e fe
    test ax, ax                               ; 85 c0
    je short 06afah                           ; 74 02
    pop bp                                    ; 5d
    retn                                      ; c3
    mov ax, strict word 00009h                ; b8 09 00
    call 0165ch                               ; e8 5c ab
    mov byte [bp+010h], al                    ; 88 46 10
    mov ax, strict word 00008h                ; b8 08 00
    call 0165ch                               ; e8 53 ab
    mov byte [bp+00fh], al                    ; 88 46 0f
    mov ax, strict word 00007h                ; b8 07 00
    call 0165ch                               ; e8 4a ab
    mov byte [bp+00eh], al                    ; 88 46 0e
    mov ax, strict word 00032h                ; b8 32 00
    call 0165ch                               ; e8 41 ab
    mov byte [bp+011h], al                    ; 88 46 11
    mov byte [bp+012h], al                    ; 88 46 12
    pop bp                                    ; 5d
    retn                                      ; c3
    call 06902h                               ; e8 dc fd
    test ax, ax                               ; 85 c0
    je short 06b2fh                           ; 74 05
    call 068ddh                               ; e8 b0 fd
    pop bp                                    ; 5d
    retn                                      ; c3
    mov dl, byte [bp+010h]                    ; 8a 56 10
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00009h                ; b8 09 00
    call 0166dh                               ; e8 33 ab
    mov dl, byte [bp+00fh]                    ; 8a 56 0f
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00008h                ; b8 08 00
    call 0166dh                               ; e8 28 ab
    mov dl, byte [bp+00eh]                    ; 8a 56 0e
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00007h                ; b8 07 00
    call 0166dh                               ; e8 1d ab
    mov dl, byte [bp+011h]                    ; 8a 56 11
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00032h                ; b8 32 00
    call 0166dh                               ; e8 12 ab
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 fb aa
    mov bl, al                                ; 88 c3
    and bl, 07fh                              ; 80 e3 7f
    jmp near 06adah                           ; e9 71 ff
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 ed aa
    mov bl, al                                ; 88 c3
    mov word [bp+012h], strict word 00000h    ; c7 46 12 00 00
    test AL, strict byte 020h                 ; a8 20
    je short 06b7ch                           ; 74 02
    pop bp                                    ; 5d
    retn                                      ; c3
    call 06902h                               ; e8 83 fd
    test ax, ax                               ; 85 c0
    je short 06b86h                           ; 74 03
    call 068ddh                               ; e8 57 fd
    mov dl, byte [bp+00fh]                    ; 8a 56 0f
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00001h                ; b8 01 00
    call 0166dh                               ; e8 dc aa
    mov dl, byte [bp+010h]                    ; 8a 56 10
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00003h                ; b8 03 00
    call 0166dh                               ; e8 d1 aa
    mov dl, byte [bp+011h]                    ; 8a 56 11
    xor dh, dh                                ; 30 f6
    mov ax, strict word 00005h                ; b8 05 00
    call 0166dh                               ; e8 c6 aa
    mov dx, 000a1h                            ; ba a1 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 0feh                  ; 24 fe
    out DX, AL                                ; ee
    mov dl, bl                                ; 88 da
    and dl, 05fh                              ; 80 e2 5f
    or dl, 020h                               ; 80 ca 20
    xor dh, dh                                ; 30 f6
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0166dh                               ; e8 ad aa
    pop bp                                    ; 5d
    retn                                      ; c3
    mov ax, strict word 0000bh                ; b8 0b 00
    call 0165ch                               ; e8 94 aa
    mov bl, al                                ; 88 c3
    mov dl, al                                ; 88 c2
    and dl, 057h                              ; 80 e2 57
    jmp near 06adch                           ; e9 0a ff
    pop bp                                    ; 5d
    retn                                      ; c3
send_to_mouse_ctrl_:                         ; 0xf6bd4 LB 0x35
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 06bf7h                           ; 74 12
    mov ax, 0086ch                            ; b8 6c 08
    push ax                                   ; 50
    mov ax, 00fdeh                            ; b8 de 0f
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 06 ad
    add sp, strict byte 00006h                ; 83 c4 06
    mov AL, strict byte 0d4h                  ; b0 d4
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    xor al, bl                                ; 30 d8
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
get_mouse_data_:                             ; 0xf6c09 LB 0x38
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov es, dx                                ; 8e c2
    mov cx, 02710h                            ; b9 10 27
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and ax, strict word 00021h                ; 25 21 00
    cmp ax, strict word 00021h                ; 3d 21 00
    je short 06c2ah                           ; 74 07
    test cx, cx                               ; 85 c9
    je short 06c2ah                           ; 74 03
    dec cx                                    ; 49
    jmp short 06c15h                          ; eb eb
    test cx, cx                               ; 85 c9
    jne short 06c32h                          ; 75 04
    mov AL, strict byte 001h                  ; b0 01
    jmp short 06c3dh                          ; eb 0b
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov byte [es:bx], al                      ; 26 88 07
    xor al, al                                ; 30 c0
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
set_kbd_command_byte_:                       ; 0xf6c41 LB 0x33
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 002h                 ; a8 02
    je short 06c64h                           ; 74 12
    mov ax, 00876h                            ; b8 76 08
    push ax                                   ; 50
    mov ax, 00fdeh                            ; b8 de 0f
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 99 ac
    add sp, strict byte 00006h                ; 83 c4 06
    mov AL, strict byte 060h                  ; b0 60
    mov dx, strict word 00064h                ; ba 64 00
    out DX, AL                                ; ee
    mov al, bl                                ; 88 d8
    mov dx, strict word 00060h                ; ba 60 00
    out DX, AL                                ; ee
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
_int74_function:                             ; 0xf6c74 LB 0xd2
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00008h                ; 83 ec 08
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 99 a9
    mov cx, ax                                ; 89 c1
    mov word [bp+004h], strict word 00000h    ; c7 46 04 00 00
    mov dx, strict word 00064h                ; ba 64 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    and AL, strict byte 021h                  ; 24 21
    cmp AL, strict byte 021h                  ; 3c 21
    jne short 06cb8h                          ; 75 22
    mov dx, strict word 00060h                ; ba 60 00
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov bl, al                                ; 88 c3
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 5a a9
    mov byte [bp-002h], al                    ; 88 46 fe
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 4f a9
    mov byte [bp-006h], al                    ; 88 46 fa
    test AL, strict byte 080h                 ; a8 80
    jne short 06cbbh                          ; 75 03
    jmp near 06d32h                           ; e9 77 00
    mov al, byte [bp-006h]                    ; 8a 46 fa
    and AL, strict byte 007h                  ; 24 07
    mov byte [bp-004h], al                    ; 88 46 fc
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 007h                  ; 24 07
    mov byte [bp-008h], al                    ; 88 46 f8
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov al, byte [bp-008h]                    ; 8a 46 f8
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00028h                ; 83 c2 28
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 30 a9
    mov al, byte [bp-008h]                    ; 8a 46 f8
    cmp al, byte [bp-004h]                    ; 3a 46 fc
    jc short 06d22h                           ; 72 3c
    mov dx, strict word 00028h                ; ba 28 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 12 a9
    xor ah, ah                                ; 30 e4
    mov word [bp+00ch], ax                    ; 89 46 0c
    mov dx, strict word 00029h                ; ba 29 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 05 a9
    xor ah, ah                                ; 30 e4
    mov word [bp+00ah], ax                    ; 89 46 0a
    mov dx, strict word 0002ah                ; ba 2a 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 f8 a8
    xor ah, ah                                ; 30 e4
    mov word [bp+008h], ax                    ; 89 46 08
    xor al, al                                ; 30 c0
    mov word [bp+006h], ax                    ; 89 46 06
    mov byte [bp-002h], ah                    ; 88 66 fe
    test byte [bp-006h], 080h                 ; f6 46 fa 80
    je short 06d25h                           ; 74 0a
    mov word [bp+004h], strict word 00001h    ; c7 46 04 01 00
    jmp short 06d25h                          ; eb 03
    inc byte [bp-002h]                        ; fe 46 fe
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 dc a8
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    retn                                      ; c3
    mov byte [di+004h], ch                    ; 88 6d 04
    outsb                                     ; 6e
    test byte [bp+019h], ch                   ; 84 6e 19
    outsw                                     ; 6f
    mov bp, word [bx-031h]                    ; 8b 6f cf
    insw                                      ; 6d
    mov BL, strict byte 06fh                  ; b3 6f
    db  080h
    db  070h
_int15_function_mouse:                       ; 0xf6d46 LB 0x39f
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00006h                ; 83 ec 06
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 c6 a8
    mov cx, ax                                ; 89 c1
    cmp byte [bp+014h], 007h                  ; 80 7e 14 07
    jbe short 06d69h                          ; 76 0b
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 001h                  ; c6 46 15 01
    jmp near 070e0h                           ; e9 77 03
    mov ax, strict word 00065h                ; b8 65 00
    call 06c41h                               ; e8 d2 fe
    and word [bp+01ah], strict byte 0fffeh    ; 83 66 1a fe
    mov byte [bp+015h], 000h                  ; c6 46 15 00
    mov bl, byte [bp+014h]                    ; 8a 5e 14
    cmp bl, 007h                              ; 80 fb 07
    jnbe short 06dddh                         ; 77 5e
    xor bh, bh                                ; 30 ff
    sal bx, 1                                 ; d1 e3
    jmp word [cs:bx+06d36h]                   ; 2e ff a7 36 6d
    cmp byte [bp+00fh], 001h                  ; 80 7e 0f 01
    jnbe short 06de0h                         ; 77 52
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 6a a8
    test AL, strict byte 080h                 ; a8 80
    jne short 06da5h                          ; 75 0b
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 005h                  ; c6 46 15 05
    jmp near 070dah                           ; e9 35 03
    cmp byte [bp+00fh], 000h                  ; 80 7e 0f 00
    jne short 06dafh                          ; 75 04
    mov AL, strict byte 0f5h                  ; b0 f5
    jmp short 06db1h                          ; eb 02
    mov AL, strict byte 0f4h                  ; b0 f4
    xor ah, ah                                ; 30 e4
    call 06bd4h                               ; e8 1e fe
    test al, al                               ; 84 c0
    jne short 06de3h                          ; 75 29
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c09h                               ; e8 47 fe
    test al, al                               ; 84 c0
    je short 06dcch                           ; 74 06
    cmp byte [bp-004h], 0fah                  ; 80 7e fc fa
    jne short 06de3h                          ; 75 17
    jmp near 070dah                           ; e9 0b 03
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 06ddah                           ; 72 04
    cmp AL, strict byte 008h                  ; 3c 08
    jbe short 06de6h                          ; 76 0c
    jmp near 06f81h                           ; e9 a4 01
    jmp near 070c4h                           ; e9 e4 02
    jmp near 070d2h                           ; e9 ef 02
    jmp near 07058h                           ; e9 72 02
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 12 a8
    mov ah, byte [bp+00fh]                    ; 8a 66 0f
    db  0feh, 0cch
    ; dec ah                                    ; fe cc
    mov bl, al                                ; 88 c3
    and bl, 0f8h                              ; 80 e3 f8
    or bl, ah                                 ; 08 e3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 0a a8
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 f4 a7
    mov bl, al                                ; 88 c3
    and bl, 0f8h                              ; 80 e3 f8
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00026h                ; ba 26 00
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 f3 a7
    mov ax, 000ffh                            ; b8 ff 00
    call 06bd4h                               ; e8 b3 fd
    test al, al                               ; 84 c0
    jne short 06de3h                          ; 75 be
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06c09h                               ; e8 dc fd
    mov cl, al                                ; 88 c1
    cmp byte [bp-006h], 0feh                  ; 80 7e fa fe
    jne short 06e3fh                          ; 75 0a
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 004h                  ; c6 46 15 04
    jmp short 06dcch                          ; eb 8d
    cmp byte [bp-006h], 0fah                  ; 80 7e fa fa
    je short 06e59h                           ; 74 14
    mov al, byte [bp-006h]                    ; 8a 46 fa
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 00881h                            ; b8 81 08
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 a4 aa
    add sp, strict byte 00006h                ; 83 c4 06
    test cl, cl                               ; 84 c9
    jne short 06de3h                          ; 75 86
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c09h                               ; e8 a4 fd
    test al, al                               ; 84 c0
    jne short 06ebfh                          ; 75 56
    mov dx, ss                                ; 8c d2
    lea ax, [bp-002h]                         ; 8d 46 fe
    call 06c09h                               ; e8 98 fd
    test al, al                               ; 84 c0
    jne short 06ebfh                          ; 75 4a
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [bp+00eh], al                    ; 88 46 0e
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [bp+00fh], al                    ; 88 46 0f
    jmp near 070dah                           ; e9 56 02
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    cmp AL, strict byte 003h                  ; 3c 03
    jc short 06e9bh                           ; 72 10
    jbe short 06eb9h                          ; 76 2c
    cmp AL, strict byte 006h                  ; 3c 06
    je short 06eceh                           ; 74 3d
    cmp AL, strict byte 005h                  ; 3c 05
    je short 06ec8h                           ; 74 33
    cmp AL, strict byte 004h                  ; 3c 04
    je short 06ec2h                           ; 74 29
    jmp short 06ed4h                          ; eb 39
    cmp AL, strict byte 002h                  ; 3c 02
    je short 06eb3h                           ; 74 14
    cmp AL, strict byte 001h                  ; 3c 01
    je short 06eadh                           ; 74 0a
    test al, al                               ; 84 c0
    jne short 06ed4h                          ; 75 2d
    mov byte [bp-004h], 00ah                  ; c6 46 fc 0a
    jmp short 06ed8h                          ; eb 2b
    mov byte [bp-004h], 014h                  ; c6 46 fc 14
    jmp short 06ed8h                          ; eb 25
    mov byte [bp-004h], 028h                  ; c6 46 fc 28
    jmp short 06ed8h                          ; eb 1f
    mov byte [bp-004h], 03ch                  ; c6 46 fc 3c
    jmp short 06ed8h                          ; eb 19
    jmp near 07058h                           ; e9 96 01
    mov byte [bp-004h], 050h                  ; c6 46 fc 50
    jmp short 06ed8h                          ; eb 10
    mov byte [bp-004h], 064h                  ; c6 46 fc 64
    jmp short 06ed8h                          ; eb 0a
    mov byte [bp-004h], 0c8h                  ; c6 46 fc c8
    jmp short 06ed8h                          ; eb 04
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jbe short 06f0eh                          ; 76 30
    mov ax, 000f3h                            ; b8 f3 00
    call 06bd4h                               ; e8 f0 fc
    test al, al                               ; 84 c0
    jne short 06f03h                          ; 75 1b
    mov dx, ss                                ; 8c d2
    lea ax, [bp-002h]                         ; 8d 46 fe
    call 06c09h                               ; e8 19 fd
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    call 06bd4h                               ; e8 dc fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-002h]                         ; 8d 46 fe
    call 06c09h                               ; e8 09 fd
    jmp near 070dah                           ; e9 d7 01
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 003h                  ; c6 46 15 03
    jmp near 070dah                           ; e9 cc 01
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 002h                  ; c6 46 15 02
    jmp near 070dah                           ; e9 c1 01
    cmp byte [bp+00fh], 004h                  ; 80 7e 0f 04
    jnc short 06f81h                          ; 73 62
    mov ax, 000e8h                            ; b8 e8 00
    call 06bd4h                               ; e8 af fc
    test al, al                               ; 84 c0
    jne short 06f77h                          ; 75 4e
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c09h                               ; e8 d8 fc
    cmp byte [bp-004h], 0fah                  ; 80 7e fc fa
    je short 06f4bh                           ; 74 14
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 008ach                            ; b8 ac 08
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 b2 a9
    add sp, strict byte 00006h                ; 83 c4 06
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    xor ah, ah                                ; 30 e4
    call 06bd4h                               ; e8 81 fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c09h                               ; e8 ae fc
    cmp byte [bp-004h], 0fah                  ; 80 7e fc fa
    je short 06fb0h                           ; 74 4f
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 008ach                            ; b8 ac 08
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 88 a9
    add sp, strict byte 00006h                ; 83 c4 06
    jmp short 06fb0h                          ; eb 39
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 003h                  ; c6 46 15 03
    jmp short 06fb0h                          ; eb 2f
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 002h                  ; c6 46 15 02
    jmp short 06fb0h                          ; eb 25
    mov ax, 000f2h                            ; b8 f2 00
    call 06bd4h                               ; e8 43 fc
    test al, al                               ; 84 c0
    jne short 06fa8h                          ; 75 13
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c09h                               ; e8 6c fc
    mov dx, ss                                ; 8c d2
    lea ax, [bp-002h]                         ; 8d 46 fe
    call 06c09h                               ; e8 64 fc
    jmp near 06e7bh                           ; e9 d3 fe
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 003h                  ; c6 46 15 03
    jmp near 070dah                           ; e9 27 01
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    test al, al                               ; 84 c0
    jbe short 06fc3h                          ; 76 09
    cmp AL, strict byte 002h                  ; 3c 02
    jbe short 06fc1h                          ; 76 03
    jmp near 07062h                           ; e9 a1 00
    jmp short 0702dh                          ; eb 6a
    mov ax, 000e9h                            ; b8 e9 00
    call 06bd4h                               ; e8 0b fc
    test al, al                               ; 84 c0
    jne short 07036h                          ; 75 69
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c09h                               ; e8 34 fc
    mov cl, al                                ; 88 c1
    cmp byte [bp-004h], 0fah                  ; 80 7e fc fa
    je short 06ff1h                           ; 74 14
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 008ach                            ; b8 ac 08
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 0c a9
    add sp, strict byte 00006h                ; 83 c4 06
    test cl, cl                               ; 84 c9
    jne short 07058h                          ; 75 63
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c09h                               ; e8 0c fc
    test al, al                               ; 84 c0
    jne short 07058h                          ; 75 57
    mov dx, ss                                ; 8c d2
    lea ax, [bp-002h]                         ; 8d 46 fe
    call 06c09h                               ; e8 00 fc
    test al, al                               ; 84 c0
    jne short 07058h                          ; 75 4b
    mov dx, ss                                ; 8c d2
    lea ax, [bp-006h]                         ; 8d 46 fa
    call 06c09h                               ; e8 f4 fb
    test al, al                               ; 84 c0
    jne short 07058h                          ; 75 3f
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [bp+00eh], al                    ; 88 46 0e
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [bp+012h], al                    ; 88 46 12
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [bp+010h], al                    ; 88 46 10
    jmp short 06fb0h                          ; eb 83
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 07038h                          ; 75 07
    mov ax, 000e6h                            ; b8 e6 00
    jmp short 0703bh                          ; eb 05
    jmp short 07058h                          ; eb 20
    mov ax, 000e7h                            ; b8 e7 00
    call 06bd4h                               ; e8 96 fb
    mov cl, al                                ; 88 c1
    test cl, cl                               ; 84 c9
    jne short 07054h                          ; 75 10
    mov dx, ss                                ; 8c d2
    lea ax, [bp-004h]                         ; 8d 46 fc
    call 06c09h                               ; e8 bd fb
    cmp byte [bp-004h], 0fah                  ; 80 7e fc fa
    je short 07054h                           ; 74 02
    mov CL, strict byte 001h                  ; b1 01
    test cl, cl                               ; 84 c9
    je short 070c2h                           ; 74 6a
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 003h                  ; c6 46 15 03
    jmp short 070c2h                          ; eb 60
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 008d8h                            ; b8 d8 08
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 87 a8
    add sp, strict byte 00006h                ; 83 c4 06
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 001h                  ; c6 46 15 01
    jmp short 070dah                          ; eb 5a
    mov si, word [bp+00eh]                    ; 8b 76 0e
    mov bx, si                                ; 89 f3
    mov dx, strict word 00022h                ; ba 22 00
    mov ax, cx                                ; 89 c8
    call 0162ah                               ; e8 9d a5
    mov bx, word [bp+016h]                    ; 8b 5e 16
    mov dx, strict word 00024h                ; ba 24 00
    mov ax, cx                                ; 89 c8
    call 0162ah                               ; e8 92 a5
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 01600h                               ; e8 60 a5
    mov ah, al                                ; 88 c4
    test si, si                               ; 85 f6
    jne short 070b4h                          ; 75 0e
    cmp word [bp+016h], strict byte 00000h    ; 83 7e 16 00
    jne short 070b4h                          ; 75 08
    test AL, strict byte 080h                 ; a8 80
    je short 070b6h                           ; 74 06
    and AL, strict byte 07fh                  ; 24 7f
    jmp short 070b6h                          ; eb 02
    or AL, strict byte 080h                   ; 0c 80
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00027h                ; ba 27 00
    mov ax, cx                                ; 89 c8
    call 0160eh                               ; e8 4c a5
    jmp short 070dah                          ; eb 16
    mov ax, 008f2h                            ; b8 f2 08
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 2b a8
    add sp, strict byte 00004h                ; 83 c4 04
    or word [bp+01ah], strict byte 00001h     ; 83 4e 1a 01
    mov byte [bp+015h], 001h                  ; c6 46 15 01
    mov ax, strict word 00047h                ; b8 47 00
    call 06c41h                               ; e8 61 fb
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
_int17_function:                             ; 0xf70e5 LB 0xab
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    sti                                       ; fb
    mov dx, word [bp+010h]                    ; 8b 56 10
    sal dx, 1                                 ; d1 e2
    add dx, strict byte 00008h                ; 83 c2 08
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 23 a5
    mov bx, ax                                ; 89 c3
    mov si, ax                                ; 89 c6
    cmp byte [bp+015h], 003h                  ; 80 7e 15 03
    jnc short 0710fh                          ; 73 0c
    mov ax, word [bp+010h]                    ; 8b 46 10
    cmp ax, strict word 00003h                ; 3d 03 00
    jnc short 0710fh                          ; 73 04
    test bx, bx                               ; 85 db
    jnbe short 07112h                         ; 77 03
    jmp near 07187h                           ; e9 75 00
    mov dx, ax                                ; 89 c2
    add dx, strict byte 00078h                ; 83 c2 78
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 e3 a4
    mov ch, al                                ; 88 c5
    xor cl, cl                                ; 30 c9
    cmp byte [bp+015h], 000h                  ; 80 7e 15 00
    jne short 07153h                          ; 75 2c
    mov al, byte [bp+014h]                    ; 8a 46 14
    mov dx, bx                                ; 89 da
    out DX, AL                                ; ee
    lea dx, [bx+002h]                         ; 8d 57 02
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-002h], ax                    ; 89 46 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    or AL, strict byte 001h                   ; 0c 01
    out DX, AL                                ; ee
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 0feh                  ; 24 fe
    out DX, AL                                ; ee
    lea dx, [si+001h]                         ; 8d 54 01
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 040h                 ; a8 40
    je short 07153h                           ; 74 07
    test cx, cx                               ; 85 c9
    je short 07153h                           ; 74 03
    dec cx                                    ; 49
    jmp short 07142h                          ; eb ef
    cmp byte [bp+015h], 001h                  ; 80 7e 15 01
    jne short 0716eh                          ; 75 15
    lea dx, [si+002h]                         ; 8d 54 02
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    mov word [bp-002h], ax                    ; 89 46 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    and AL, strict byte 0fbh                  ; 24 fb
    out DX, AL                                ; ee
    mov al, byte [bp-002h]                    ; 8a 46 fe
    or AL, strict byte 004h                   ; 0c 04
    out DX, AL                                ; ee
    lea dx, [si+001h]                         ; 8d 54 01
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    xor AL, strict byte 048h                  ; 34 48
    mov byte [bp+015h], al                    ; 88 46 15
    test cx, cx                               ; 85 c9
    jne short 07181h                          ; 75 04
    or byte [bp+015h], 001h                   ; 80 4e 15 01
    and byte [bp+01eh], 0feh                  ; 80 66 1e fe
    jmp short 0718bh                          ; eb 04
    or byte [bp+01eh], 001h                   ; 80 4e 1e 01
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn                                      ; c3
wait_:                                       ; 0xf7190 LB 0xb1
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0000ah                ; 83 ec 0a
    mov si, ax                                ; 89 c6
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    pushfw                                    ; 9c
    pop ax                                    ; 58
    mov word [bp-008h], ax                    ; 89 46 f8
    sti                                       ; fb
    xor cx, cx                                ; 31 c9
    mov dx, 0046ch                            ; ba 6c 04
    xor ax, ax                                ; 31 c0
    call 01638h                               ; e8 85 a4
    mov word [bp-006h], ax                    ; 89 46 fa
    mov bx, dx                                ; 89 d3
    hlt                                       ; f4
    mov dx, 0046ch                            ; ba 6c 04
    xor ax, ax                                ; 31 c0
    call 01638h                               ; e8 77 a4
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov di, dx                                ; 89 d7
    cmp dx, bx                                ; 39 da
    jnbe short 071d1h                         ; 77 07
    jne short 071d8h                          ; 75 0c
    cmp ax, word [bp-006h]                    ; 3b 46 fa
    jbe short 071d8h                          ; 76 07
    sub ax, word [bp-006h]                    ; 2b 46 fa
    sbb dx, bx                                ; 19 da
    jmp short 071e3h                          ; eb 0b
    cmp dx, bx                                ; 39 da
    jc short 071e3h                           ; 72 07
    jne short 071e7h                          ; 75 09
    cmp ax, word [bp-006h]                    ; 3b 46 fa
    jnc short 071e7h                          ; 73 04
    sub si, ax                                ; 29 c6
    sbb cx, dx                                ; 19 d1
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    mov word [bp-006h], ax                    ; 89 46 fa
    mov bx, di                                ; 89 fb
    mov ax, 00100h                            ; b8 00 01
    int 016h                                  ; cd 16
    je short 071fbh                           ; 74 05
    mov AL, strict byte 001h                  ; b0 01
    jmp near 071fdh                           ; e9 02 00
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    test al, al                               ; 84 c0
    je short 07227h                           ; 74 26
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    int 016h                                  ; cd 16
    xchg ah, al                               ; 86 c4
    mov dl, al                                ; 88 c2
    mov byte [bp-004h], al                    ; 88 46 fc
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 00914h                            ; b8 14 09
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 e0 a6
    add sp, strict byte 00006h                ; 83 c4 06
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 07227h                           ; 74 04
    mov al, dl                                ; 88 d0
    jmp short 07239h                          ; eb 12
    test cx, cx                               ; 85 c9
    jnle short 071b8h                         ; 7f 8d
    jne short 07231h                          ; 75 04
    test si, si                               ; 85 f6
    jnbe short 071b8h                         ; 77 87
    mov ax, word [bp-008h]                    ; 8b 46 f8
    push ax                                   ; 50
    popfw                                     ; 9d
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
read_logo_byte_:                             ; 0xf7241 LB 0x13
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
read_logo_word_:                             ; 0xf7254 LB 0x11
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    xor ah, ah                                ; 30 e4
    or ah, 001h                               ; 80 cc 01
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    in ax, DX                                 ; ed
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    retn                                      ; c3
print_detected_harddisks_:                   ; 0xf7265 LB 0x159
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00006h                ; 83 ec 06
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 a4 a3
    mov si, ax                                ; 89 c6
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    xor ch, ch                                ; 30 ed
    mov byte [bp-004h], ch                    ; 88 6e fc
    mov dx, 002c0h                            ; ba c0 02
    call 01600h                               ; e8 77 a3
    mov byte [bp-002h], al                    ; 88 46 fe
    xor cl, cl                                ; 30 c9
    cmp cl, byte [bp-002h]                    ; 3a 4e fe
    jnc short 072f1h                          ; 73 5e
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    mov dx, ax                                ; 89 c2
    add dx, 002c1h                            ; 81 c2 c1 02
    mov ax, si                                ; 89 f0
    call 01600h                               ; e8 5e a3
    mov bl, al                                ; 88 c3
    cmp AL, strict byte 00ch                  ; 3c 0c
    jc short 072d3h                           ; 72 2b
    test ch, ch                               ; 84 ed
    jne short 072bch                          ; 75 10
    mov ax, 00925h                            ; b8 25 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 43 a6
    add sp, strict byte 00004h                ; 83 c4 04
    mov CH, strict byte 001h                  ; b5 01
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    push ax                                   ; 50
    mov ax, 00939h                            ; b8 39 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 2d a6
    add sp, strict byte 00006h                ; 83 c4 06
    jmp near 07385h                           ; e9 b2 00
    cmp AL, strict byte 008h                  ; 3c 08
    jc short 072f4h                           ; 72 1d
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 072efh                          ; 75 12
    mov ax, 0094ch                            ; b8 4c 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 12 a6
    add sp, strict byte 00004h                ; 83 c4 04
    mov byte [bp-004h], 001h                  ; c6 46 fc 01
    jmp short 072bch                          ; eb cb
    jmp near 0738ah                           ; e9 96 00
    cmp AL, strict byte 004h                  ; 3c 04
    jnc short 07312h                          ; 73 1a
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 07312h                          ; 75 14
    mov ax, 00960h                            ; b8 60 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 f1 a5
    add sp, strict byte 00004h                ; 83 c4 04
    mov byte [bp-006h], 001h                  ; c6 46 fa 01
    jmp short 0732bh                          ; eb 19
    cmp bl, 004h                              ; 80 fb 04
    jc short 0732bh                           ; 72 14
    test ch, ch                               ; 84 ed
    jne short 0732bh                          ; 75 10
    mov ax, 00925h                            ; b8 25 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 d4 a5
    add sp, strict byte 00004h                ; 83 c4 04
    mov CH, strict byte 001h                  ; b5 01
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    inc ax                                    ; 40
    push ax                                   ; 50
    mov ax, 00971h                            ; b8 71 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 be a5
    add sp, strict byte 00006h                ; 83 c4 06
    cmp bl, 004h                              ; 80 fb 04
    jc short 07347h                           ; 72 03
    sub bl, 004h                              ; 80 eb 04
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    cwd                                       ; 99
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2
    sar ax, 1                                 ; d1 f8
    test ax, ax                               ; 85 c0
    je short 07359h                           ; 74 05
    mov ax, 0097bh                            ; b8 7b 09
    jmp short 0735ch                          ; eb 03
    mov ax, 00986h                            ; b8 86 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 96 a5
    add sp, strict byte 00004h                ; 83 c4 04
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov bx, strict word 00002h                ; bb 02 00
    cwd                                       ; 99
    idiv bx                                   ; f7 fb
    test dx, dx                               ; 85 d2
    je short 0737ah                           ; 74 05
    mov ax, 0098fh                            ; b8 8f 09
    jmp short 0737dh                          ; eb 03
    mov ax, 00995h                            ; b8 95 09
    push ax                                   ; 50
    push bx                                   ; 53
    call 018fah                               ; e8 78 a5
    add sp, strict byte 00004h                ; 83 c4 04
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    jmp near 0728eh                           ; e9 04 ff
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 073a8h                          ; 75 18
    test ch, ch                               ; 84 ed
    jne short 073a8h                          ; 75 14
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    jne short 073a8h                          ; 75 0e
    mov ax, 0099ch                            ; b8 9c 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 55 a5
    add sp, strict byte 00004h                ; 83 c4 04
    mov ax, 009b0h                            ; b8 b0 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 47 a5
    add sp, strict byte 00004h                ; 83 c4 04
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
get_boot_drive_:                             ; 0xf73be LB 0x25
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bl, al                                ; 88 c3
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 4e a2
    mov dx, 002c0h                            ; ba c0 02
    call 01600h                               ; e8 2c a2
    sub bl, 002h                              ; 80 eb 02
    cmp bl, al                                ; 38 c3
    jc short 073ddh                           ; 72 02
    mov BL, strict byte 0ffh                  ; b3 ff
    mov al, bl                                ; 88 d8
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
show_logo_:                                  ; 0xf73e3 LB 0x22d
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0000ah                ; 83 ec 0a
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 25 a2
    mov si, ax                                ; 89 c6
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    xor cx, cx                                ; 31 c9
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    mov AL, strict byte 0d3h                  ; b0 d3
    out strict byte 040h, AL                  ; e6 40
    mov AL, strict byte 048h                  ; b0 48
    out strict byte 040h, AL                  ; e6 40
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    call 07254h                               ; e8 42 fe
    cmp ax, 066bbh                            ; 3d bb 66
    jne short 0746ah                          ; 75 53
    mov al, cl                                ; 88 c8
    add AL, strict byte 004h                  ; 04 04
    xor ah, ah                                ; 30 e4
    call 07241h                               ; e8 21 fe
    mov ch, al                                ; 88 c5
    mov byte [bp-006h], al                    ; 88 46 fa
    mov al, cl                                ; 88 c8
    add AL, strict byte 005h                  ; 04 05
    xor ah, ah                                ; 30 e4
    call 07241h                               ; e8 13 fe
    mov bl, al                                ; 88 c3
    mov byte [bp-008h], al                    ; 88 46 f8
    mov al, cl                                ; 88 c8
    add AL, strict byte 002h                  ; 04 02
    xor ah, ah                                ; 30 e4
    call 07254h                               ; e8 18 fe
    mov dx, ax                                ; 89 c2
    mov di, ax                                ; 89 c7
    mov al, cl                                ; 88 c8
    add AL, strict byte 006h                  ; 04 06
    xor ah, ah                                ; 30 e4
    call 07241h                               ; e8 f8 fd
    mov byte [bp-00ah], al                    ; 88 46 f6
    test ch, ch                               ; 84 ed
    jne short 07458h                          ; 75 08
    test bl, bl                               ; 84 db
    jne short 07458h                          ; 75 04
    test dx, dx                               ; 85 d2
    je short 0746ah                           ; 74 12
    mov bx, 00142h                            ; bb 42 01
    mov ax, 04f02h                            ; b8 02 4f
    int 010h                                  ; cd 10
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    je short 07490h                           ; 74 2a
    xor cx, cx                                ; 31 c9
    jmp short 07472h                          ; eb 08
    jmp near 074e0h                           ; e9 73 00
    cmp cx, strict byte 00010h                ; 83 f9 10
    jnbe short 07497h                         ; 77 25
    mov ax, cx                                ; 89 c8
    or ah, 002h                               ; 80 cc 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00001h                ; b8 01 00
    call 07190h                               ; e8 0d fd
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 0748dh                          ; 75 06
    mov byte [bp-002h], 001h                  ; c6 46 fe 01
    jmp short 07497h                          ; eb 0a
    inc cx                                    ; 41
    jmp short 0746dh                          ; eb dd
    mov ax, 00210h                            ; b8 10 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 074b1h                          ; 75 14
    mov CL, strict byte 004h                  ; b1 04
    mov ax, di                                ; 89 f8
    shr ax, CL                                ; d3 e8
    mov dx, strict word 00001h                ; ba 01 00
    call 07190h                               ; e8 e7 fc
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 074b1h                          ; 75 04
    mov byte [bp-002h], 001h                  ; c6 46 fe 01
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    je short 074e0h                           ; 74 29
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 074e0h                          ; 75 23
    mov cx, strict word 00010h                ; b9 10 00
    jmp short 074c7h                          ; eb 05
    dec cx                                    ; 49
    test cx, cx                               ; 85 c9
    jbe short 074e0h                          ; 76 19
    mov ax, cx                                ; 89 c8
    or ah, 002h                               ; 80 cc 02
    mov dx, 003b8h                            ; ba b8 03
    out DX, ax                                ; ef
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00001h                ; b8 01 00
    call 07190h                               ; e8 b8 fc
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 074c2h                          ; 75 e6
    mov byte [bp-002h], 001h                  ; c6 46 fe 01
    xor bx, bx                                ; 31 db
    mov dx, 00339h                            ; ba 39 03
    mov ax, si                                ; 89 f0
    call 0160eh                               ; e8 24 a1
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 000h                  ; b4 00
    int 010h                                  ; cd 10
    cmp byte [bp-00ah], 000h                  ; 80 7e f6 00
    je short 07508h                           ; 74 12
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00
    jne short 07536h                          ; 75 3a
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00
    jne short 07536h                          ; 75 34
    test di, di                               ; 85 ff
    je short 0750bh                           ; 74 05
    jmp short 07536h                          ; eb 2e
    jmp near 075f2h                           ; e9 e7 00
    cmp byte [bp-00ah], 002h                  ; 80 7e f6 02
    jne short 0751fh                          ; 75 0e
    mov ax, 009b2h                            ; b8 b2 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 de a3
    add sp, strict byte 00004h                ; 83 c4 04
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    jne short 07536h                          ; 75 11
    mov dx, strict word 00001h                ; ba 01 00
    mov ax, 000c0h                            ; b8 c0 00
    call 07190h                               ; e8 62 fc
    cmp AL, strict byte 086h                  ; 3c 86
    jne short 07536h                          ; 75 04
    mov byte [bp-002h], 001h                  ; c6 46 fe 01
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00
    je short 07508h                           ; 74 cc
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    mov ax, 00100h                            ; b8 00 01
    mov cx, 01000h                            ; b9 00 10
    int 010h                                  ; cd 10
    mov ax, 00700h                            ; b8 00 07
    mov BH, strict byte 007h                  ; b7 07
    db  033h, 0c9h
    ; xor cx, cx                                ; 33 c9
    mov dx, 0184fh                            ; ba 4f 18
    int 010h                                  ; cd 10
    mov ax, 00200h                            ; b8 00 02
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2
    int 010h                                  ; cd 10
    mov ax, 009d4h                            ; b8 d4 09
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 92 a3
    add sp, strict byte 00004h                ; 83 c4 04
    call 07265h                               ; e8 f7 fc
    mov ax, 00a18h                            ; b8 18 0a
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 81 a3
    add sp, strict byte 00004h                ; 83 c4 04
    mov dx, strict word 00001h                ; ba 01 00
    mov ax, strict word 00040h                ; b8 40 00
    call 07190h                               ; e8 0b fc
    mov cl, al                                ; 88 c1
    test al, al                               ; 84 c0
    je short 0757ch                           ; 74 f1
    cmp AL, strict byte 030h                  ; 3c 30
    je short 075dfh                           ; 74 50
    cmp cl, 002h                              ; 80 f9 02
    jc short 075b8h                           ; 72 24
    cmp cl, 009h                              ; 80 f9 09
    jnbe short 075b8h                         ; 77 1f
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    call 073beh                               ; e8 1e fe
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 075a6h                          ; 75 02
    jmp short 0757ch                          ; eb d6
    xor ah, ah                                ; 30 e4
    mov bx, ax                                ; 89 c3
    mov dx, 00338h                            ; ba 38 03
    mov ax, si                                ; 89 f0
    call 0160eh                               ; e8 5c a0
    mov byte [bp-004h], 002h                  ; c6 46 fc 02
    jmp short 075dfh                          ; eb 27
    cmp cl, 02eh                              ; 80 f9 2e
    je short 075cdh                           ; 74 10
    cmp cl, 026h                              ; 80 f9 26
    je short 075d3h                           ; 74 11
    cmp cl, 021h                              ; 80 f9 21
    jne short 075d9h                          ; 75 12
    mov byte [bp-004h], 001h                  ; c6 46 fc 01
    jmp short 075dfh                          ; eb 12
    mov byte [bp-004h], 003h                  ; c6 46 fc 03
    jmp short 075dfh                          ; eb 0c
    mov byte [bp-004h], 004h                  ; c6 46 fc 04
    jmp short 075dfh                          ; eb 06
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00
    je short 0757ch                           ; 74 9d
    mov bl, byte [bp-004h]                    ; 8a 5e fc
    xor bh, bh                                ; 30 ff
    mov dx, 00339h                            ; ba 39 03
    mov ax, si                                ; 89 f0
    call 0160eh                               ; e8 22 a0
    mov AL, strict byte 003h                  ; b0 03
    mov AH, strict byte 000h                  ; b4 00
    int 010h                                  ; cd 10
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    pushad                                    ; 66 60
    push DS                                   ; 1e
    mov ds, ax                                ; 8e d8
    call 0edbfh                               ; e8 bb 77
    pop DS                                    ; 1f
    popad                                     ; 66 61
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
delay_boot_:                                 ; 0xf7610 LB 0x6b
    push bx                                   ; 53
    push dx                                   ; 52
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    test ax, ax                               ; 85 c0
    je short 07677h                           ; 74 5c
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    mov AL, strict byte 0d3h                  ; b0 d3
    out strict byte 040h, AL                  ; e6 40
    mov AL, strict byte 048h                  ; b0 48
    out strict byte 040h, AL                  ; e6 40
    push bx                                   ; 53
    mov ax, 00a62h                            ; b8 62 0a
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 c7 a2
    add sp, strict byte 00006h                ; 83 c4 06
    test bx, bx                               ; 85 db
    jbe short 07654h                          ; 76 1a
    push bx                                   ; 53
    mov ax, 00a80h                            ; b8 80 0a
    push ax                                   ; 50
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 018fah                               ; e8 b4 a2
    add sp, strict byte 00006h                ; 83 c4 06
    xor dx, dx                                ; 31 d2
    mov ax, strict word 00040h                ; b8 40 00
    call 07190h                               ; e8 3f fb
    dec bx                                    ; 4b
    jmp short 07636h                          ; eb e2
    mov bx, 009b0h                            ; bb b0 09
    push bx                                   ; 53
    mov bx, strict word 00002h                ; bb 02 00
    push bx                                   ; 53
    call 018fah                               ; e8 9b a2
    add sp, strict byte 00004h                ; 83 c4 04
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    pushad                                    ; 66 60
    push DS                                   ; 1e
    mov ds, ax                                ; 8e d8
    call 0edbfh                               ; e8 4b 77
    pop DS                                    ; 1f
    popad                                     ; 66 61
    pop bp                                    ; 5d
    pop dx                                    ; 5a
    pop bx                                    ; 5b
    retn                                      ; c3
scsi_cmd_data_in_:                           ; 0xf767b LB 0x64
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov word [bp-004h], bx                    ; 89 5e fc
    mov dx, si                                ; 89 f2
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 0768ah                          ; 75 f7
    mov al, byte [bp-002h]                    ; 8a 46 fe
    out DX, AL                                ; ee
    xor al, al                                ; 30 c0
    out DX, AL                                ; ee
    mov al, byte [bp+008h]                    ; 8a 46 08
    out DX, AL                                ; ee
    mov al, byte [bp+00eh]                    ; 8a 46 0e
    out DX, AL                                ; ee
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    out DX, AL                                ; ee
    xor bx, bx                                ; 31 db
    mov al, byte [bp+008h]                    ; 8a 46 08
    xor ah, ah                                ; 30 e4
    cmp bx, ax                                ; 39 c3
    jnc short 076c1h                          ; 73 10
    mov es, cx                                ; 8e c1
    mov di, word [bp-004h]                    ; 8b 7e fc
    add di, bx                                ; 01 df
    mov al, byte [es:di]                      ; 26 8a 05
    mov dx, si                                ; 89 f2
    out DX, AL                                ; ee
    inc bx                                    ; 43
    jmp short 076a8h                          ; eb e7
    mov dx, si                                ; 89 f2
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 076c1h                          ; 75 f7
    lea dx, [si+001h]                         ; 8d 54 01
    mov cx, word [bp+00eh]                    ; 8b 4e 0e
    les di, [bp+00ah]                         ; c4 7e 0a
    rep insb                                  ; f3 6c
    xor ax, ax                                ; 31 c0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00008h                               ; c2 08 00
scsi_cmd_data_out_:                          ; 0xf76df LB 0x65
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push ax                                   ; 50
    mov di, ax                                ; 89 c7
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov word [bp-004h], bx                    ; 89 5e fc
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 076eeh                          ; 75 f7
    mov al, byte [bp-002h]                    ; 8a 46 fe
    out DX, AL                                ; ee
    mov AL, strict byte 001h                  ; b0 01
    out DX, AL                                ; ee
    mov al, byte [bp+008h]                    ; 8a 46 08
    out DX, AL                                ; ee
    mov al, byte [bp+00eh]                    ; 8a 46 0e
    out DX, AL                                ; ee
    mov al, byte [bp+00fh]                    ; 8a 46 0f
    out DX, AL                                ; ee
    xor bx, bx                                ; 31 db
    mov al, byte [bp+008h]                    ; 8a 46 08
    xor ah, ah                                ; 30 e4
    cmp bx, ax                                ; 39 c3
    jnc short 07725h                          ; 73 10
    mov es, cx                                ; 8e c1
    mov si, word [bp-004h]                    ; 8b 76 fc
    add si, bx                                ; 01 de
    mov al, byte [es:si]                      ; 26 8a 04
    mov dx, di                                ; 89 fa
    out DX, AL                                ; ee
    inc bx                                    ; 43
    jmp short 0770ch                          ; eb e7
    lea dx, [di+001h]                         ; 8d 55 01
    mov cx, word [bp+00eh]                    ; 8b 4e 0e
    les si, [bp+00ah]                         ; c4 76 0a
    db  0f3h, 026h, 06eh
    ; rep es outsb                              ; f3 26 6e
    mov dx, di                                ; 89 fa
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    test AL, strict byte 001h                 ; a8 01
    jne short 07731h                          ; 75 f7
    xor ax, ax                                ; 31 c0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00008h                               ; c2 08 00
@scsi_read_sectors:                          ; 0xf7744 LB 0xb8
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0000ch                ; 83 ec 0c
    mov si, word [bp+008h]                    ; 8b 76 08
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    sub bl, 008h                              ; 80 eb 08
    cmp bl, 004h                              ; 80 fb 04
    jbe short 07771h                          ; 76 13
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 00a84h                            ; b8 84 0a
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 8c a1
    add sp, strict byte 00006h                ; 83 c4 06
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov di, word [es:si+00ah]                 ; 26 8b 7c 0a
    mov word [bp-00ch], strict word 00028h    ; c7 46 f4 28 00
    mov ax, word [es:si]                      ; 26 8b 04
    mov dx, word [es:si+002h]                 ; 26 8b 54 02
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov word [bp-008h], dx                    ; 89 56 f8
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    mov ax, di                                ; 89 f8
    xchg ah, al                               ; 86 c4
    mov word [bp-005h], ax                    ; 89 46 fb
    mov byte [bp-003h], 000h                  ; c6 46 fd 00
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    sal ax, 1                                 ; d1 e0
    sal ax, 1                                 ; d1 e0
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [es:bx+001d8h]               ; 26 8b 87 d8 01
    mov bl, byte [es:bx+001dah]               ; 26 8a 9f da 01
    mov CL, strict byte 009h                  ; b1 09
    mov dx, di                                ; 89 fa
    sal dx, CL                                ; d3 e2
    mov word [bp-002h], dx                    ; 89 56 fe
    push dx                                   ; 52
    push word [es:si+006h]                    ; 26 ff 74 06
    push word [es:si+004h]                    ; 26 ff 74 04
    mov dx, strict word 0000ah                ; ba 0a 00
    push dx                                   ; 52
    mov dl, bl                                ; 88 da
    xor dh, dh                                ; 30 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-00ch]                         ; 8d 5e f4
    call 0767bh                               ; e8 a5 fe
    mov ah, al                                ; 88 c4
    test al, al                               ; 84 c0
    jne short 077f0h                          ; 75 14
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov dx, word [bp-002h]                    ; 8b 56 fe
    mov word [es:si+016h], dx                 ; 26 89 54 16
    mov word [es:si+018h], strict word 00000h ; 26 c7 44 18 00 00
    mov al, ah                                ; 88 e0
    xor ah, ah                                ; 30 e4
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
@scsi_write_sectors:                         ; 0xf77fc LB 0xb8
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0000ch                ; 83 ec 0c
    mov si, word [bp+008h]                    ; 8b 76 08
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    sub bl, 008h                              ; 80 eb 08
    cmp bl, 004h                              ; 80 fb 04
    jbe short 07829h                          ; 76 13
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov ax, 00ab2h                            ; b8 b2 0a
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 d4 a0
    add sp, strict byte 00006h                ; 83 c4 06
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov di, word [es:si+00ah]                 ; 26 8b 7c 0a
    mov word [bp-00ch], strict word 0002ah    ; c7 46 f4 2a 00
    mov ax, word [es:si]                      ; 26 8b 04
    mov dx, word [es:si+002h]                 ; 26 8b 54 02
    xchg ah, al                               ; 86 c4
    xchg dh, dl                               ; 86 d6
    xchg dx, ax                               ; 92
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov word [bp-008h], dx                    ; 89 56 f8
    mov byte [bp-006h], 000h                  ; c6 46 fa 00
    mov ax, di                                ; 89 f8
    xchg ah, al                               ; 86 c4
    mov word [bp-005h], ax                    ; 89 46 fb
    mov byte [bp-003h], 000h                  ; c6 46 fd 00
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    sal ax, 1                                 ; d1 e0
    sal ax, 1                                 ; d1 e0
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [es:bx+001d8h]               ; 26 8b 87 d8 01
    mov bl, byte [es:bx+001dah]               ; 26 8a 9f da 01
    mov CL, strict byte 009h                  ; b1 09
    mov dx, di                                ; 89 fa
    sal dx, CL                                ; d3 e2
    mov word [bp-002h], dx                    ; 89 56 fe
    push dx                                   ; 52
    push word [es:si+006h]                    ; 26 ff 74 06
    push word [es:si+004h]                    ; 26 ff 74 04
    mov dx, strict word 0000ah                ; ba 0a 00
    push dx                                   ; 52
    mov dl, bl                                ; 88 da
    xor dh, dh                                ; 30 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-00ch]                         ; 8d 5e f4
    call 076dfh                               ; e8 51 fe
    mov ah, al                                ; 88 c4
    test al, al                               ; 84 c0
    jne short 078a8h                          ; 75 14
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov dx, word [bp-002h]                    ; 8b 56 fe
    mov word [es:si+016h], dx                 ; 26 89 54 16
    mov word [es:si+018h], strict word 00000h ; 26 c7 44 18 00 00
    mov al, ah                                ; 88 e0
    xor ah, ah                                ; 30 e4
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
scsi_enumerate_attached_devices_:            ; 0xf78b4 LB 0x2aa
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, 0021eh                            ; 81 ec 1e 02
    mov di, ax                                ; 89 c7
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 51 9d
    mov si, 00122h                            ; be 22 01
    mov word [bp-014h], ax                    ; 89 46 ec
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00
    jmp near 07afdh                           ; e9 24 02
    mov es, [bp-014h]                         ; 8e 46 ec
    cmp byte [es:si+001e8h], 004h             ; 26 80 bc e8 01 04
    jc short 078e7h                           ; 72 03
    jmp near 07b55h                           ; e9 6e 02
    mov cx, strict word 0000ah                ; b9 0a 00
    xor bx, bx                                ; 31 db
    mov dx, ss                                ; 8c d2
    lea ax, [bp-01eh]                         ; 8d 46 e2
    call 08dbbh                               ; e8 c7 14
    mov byte [bp-01eh], 025h                  ; c6 46 e2 25
    mov ax, strict word 00008h                ; b8 08 00
    push ax                                   ; 50
    lea dx, [bp-0021eh]                       ; 8d 96 e2 fd
    push SS                                   ; 16
    push dx                                   ; 52
    mov ax, strict word 0000ah                ; b8 0a 00
    push ax                                   ; 50
    mov dl, byte [bp-00ah]                    ; 8a 56 f6
    xor dh, dh                                ; 30 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01eh]                         ; 8d 5e e2
    mov ax, di                                ; 89 f8
    call 0767bh                               ; e8 66 fd
    test al, al                               ; 84 c0
    je short 07927h                           ; 74 0e
    mov ax, 00b17h                            ; b8 17 0b
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 d6 9f
    add sp, strict byte 00004h                ; 83 c4 04
    mov al, byte [bp-0021dh]                  ; 8a 86 e3 fd
    xor dh, dh                                ; 30 f6
    mov ah, byte [bp-0021eh]                  ; 8a a6 e2 fd
    xor bx, bx                                ; 31 db
    mov word [bp-008h], ax                    ; 89 46 f8
    mov al, byte [bp-0021ch]                  ; 8a 86 e4 fd
    xor ah, ah                                ; 30 e4
    xor dl, dl                                ; 30 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 07941h                               ; e2 fa
    or bx, ax                                 ; 09 c3
    or dx, word [bp-008h]                     ; 0b 56 f8
    mov al, byte [bp-0021bh]                  ; 8a 86 e5 fd
    xor ah, ah                                ; 30 e4
    or bx, ax                                 ; 09 c3
    mov word [bp-012h], bx                    ; 89 5e ee
    mov word [bp-010h], dx                    ; 89 56 f0
    mov dh, byte [bp-0021ah]                  ; 8a b6 e6 fd
    mov dl, byte [bp-00219h]                  ; 8a 96 e7 fd
    xor bx, bx                                ; 31 db
    mov word [bp-008h], dx                    ; 89 56 f8
    mov al, byte [bp-00218h]                  ; 8a 86 e8 fd
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00008h                ; b9 08 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 07970h                               ; e2 fa
    or bx, ax                                 ; 09 c3
    or dx, word [bp-008h]                     ; 0b 56 f8
    mov al, byte [bp-00217h]                  ; 8a 86 e9 fd
    xor ah, ah                                ; 30 e4
    or bx, ax                                 ; 09 c3
    mov word [bp-00eh], bx                    ; 89 5e f2
    test dx, dx                               ; 85 d2
    jne short 07990h                          ; 75 06
    cmp bx, 00200h                            ; 81 fb 00 02
    je short 079b3h                           ; 74 23
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 22 9f
    push dx                                   ; 52
    push word [bp-00eh]                       ; ff 76 f2
    push word [bp-00ah]                       ; ff 76 f6
    mov ax, 00b53h                            ; b8 53 0b
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 4d 9f
    add sp, strict byte 0000ah                ; 83 c4 0a
    jmp near 07af4h                           ; e9 41 01
    mov word [bp-004h], 000ffh                ; c7 46 fc ff 00
    mov word [bp-002h], strict word 0003fh    ; c7 46 fe 3f 00
    jmp short 079d0h                          ; eb 11
    mov word [bp-004h], 00080h                ; c7 46 fc 80 00
    jmp short 079cbh                          ; eb 05
    mov word [bp-004h], strict word 00040h    ; c7 46 fc 40 00
    mov word [bp-002h], strict word 00020h    ; c7 46 fe 20 00
    mov ax, word [bp-004h]                    ; 8b 46 fc
    mul word [bp-002h]                        ; f7 66 fe
    mov bx, ax                                ; 89 c3
    mov ax, word [bp-012h]                    ; 8b 46 ee
    mov dx, word [bp-010h]                    ; 8b 56 f0
    xor cx, cx                                ; 31 c9
    call 08d37h                               ; e8 54 13
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov word [bp-006h], dx                    ; 89 56 fa
    mov es, [bp-014h]                         ; 8e 46 ec
    mov cl, byte [es:si+001e8h]               ; 26 8a 8c e8 01
    mov ch, cl                                ; 88 cd
    add ch, 008h                              ; 80 c5 08
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    sal ax, 1                                 ; d1 e0
    sal ax, 1                                 ; d1 e0
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov word [es:bx+001d8h], di               ; 26 89 bf d8 01
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:bx+001dah], al               ; 26 88 87 da 01
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov word [es:bx+01eh], 0ff04h             ; 26 c7 47 1e 04 ff
    mov word [es:bx+020h], strict word 00000h ; 26 c7 47 20 00 00
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:bx+024h], ax                 ; 26 89 47 24
    mov byte [es:bx+023h], 001h               ; 26 c6 47 23 01
    mov ax, word [bp-004h]                    ; 8b 46 fc
    mov word [es:bx+026h], ax                 ; 26 89 47 26
    mov ax, word [bp-002h]                    ; 8b 46 fe
    mov word [es:bx+02ah], ax                 ; 26 89 47 2a
    cmp word [bp-006h], strict byte 00000h    ; 83 7e fa 00
    jne short 07a4fh                          ; 75 07
    cmp word [bp-00ch], 00400h                ; 81 7e f4 00 04
    jbe short 07a57h                          ; 76 08
    mov word [es:bx+028h], 00400h             ; 26 c7 47 28 00 04
    jmp short 07a5eh                          ; eb 07
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov word [es:bx+028h], ax                 ; 26 89 47 28
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-014h]                         ; 8e 46 ec
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [bp-004h]                    ; 8b 46 fc
    mov word [es:bx+02ch], ax                 ; 26 89 47 2c
    mov ax, word [bp-002h]                    ; 8b 46 fe
    mov word [es:bx+030h], ax                 ; 26 89 47 30
    cmp word [bp-006h], strict byte 00000h    ; 83 7e fa 00
    jne short 07a89h                          ; 75 07
    cmp word [bp-00ch], 00400h                ; 81 7e f4 00 04
    jbe short 07a91h                          ; 76 08
    mov word [es:bx+02eh], 00400h             ; 26 c7 47 2e 00 04
    jmp short 07a98h                          ; eb 07
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov word [es:bx+02eh], ax                 ; 26 89 47 2e
    mov al, ch                                ; 88 e8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov es, [bp-014h]                         ; 8e 46 ec
    mov bx, si                                ; 89 f3
    add bx, ax                                ; 01 c3
    mov ax, word [bp-012h]                    ; 8b 46 ee
    mov word [es:bx+032h], ax                 ; 26 89 47 32
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov word [es:bx+034h], ax                 ; 26 89 47 34
    mov al, byte [es:si+0019eh]               ; 26 8a 84 9e 01
    mov ah, cl                                ; 88 cc
    add ah, 008h                              ; 80 c4 08
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    add bx, si                                ; 01 f3
    mov byte [es:bx+0019fh], ah               ; 26 88 a7 9f 01
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [es:si+0019eh], al               ; 26 88 84 9e 01
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 25 9b
    mov bl, al                                ; 88 c3
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 24 9b
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1
    mov es, [bp-014h]                         ; 8e 46 ec
    mov byte [es:si+001e8h], cl               ; 26 88 8c e8 01
    inc word [bp-00ah]                        ; ff 46 f6
    cmp word [bp-00ah], strict byte 00010h    ; 83 7e f6 10
    jnl short 07b55h                          ; 7d 58
    mov byte [bp-01eh], 012h                  ; c6 46 e2 12
    xor al, al                                ; 30 c0
    mov byte [bp-01dh], al                    ; 88 46 e3
    mov byte [bp-01ch], al                    ; 88 46 e4
    mov byte [bp-01bh], al                    ; 88 46 e5
    mov byte [bp-01ah], 005h                  ; c6 46 e6 05
    mov byte [bp-019h], al                    ; 88 46 e7
    mov ax, strict word 00005h                ; b8 05 00
    push ax                                   ; 50
    lea dx, [bp-0021eh]                       ; 8d 96 e2 fd
    push SS                                   ; 16
    push dx                                   ; 52
    mov ax, strict word 00006h                ; b8 06 00
    push ax                                   ; 50
    mov dl, byte [bp-00ah]                    ; 8a 56 f6
    xor dh, dh                                ; 30 f6
    mov cx, ss                                ; 8c d1
    lea bx, [bp-01eh]                         ; 8d 5e e2
    mov ax, di                                ; 89 f8
    call 0767bh                               ; e8 4b fb
    test al, al                               ; 84 c0
    je short 07b42h                           ; 74 0e
    mov ax, 00ae1h                            ; b8 e1 0a
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 bb 9d
    add sp, strict byte 00004h                ; 83 c4 04
    test byte [bp-0021eh], 0e0h               ; f6 86 e2 fd e0
    jne short 07af4h                          ; 75 ab
    test byte [bp-0021eh], 01fh               ; f6 86 e2 fd 1f
    jne short 07b53h                          ; 75 03
    jmp near 078d9h                           ; e9 86 fd
    jmp short 07af4h                          ; eb 9f
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
_scsi_init:                                  ; 0xf7b5e LB 0x64
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 b2 9a
    mov bx, 00122h                            ; bb 22 01
    mov es, ax                                ; 8e c0
    mov byte [es:bx+001e8h], 000h             ; 26 c6 87 e8 01 00
    mov AL, strict byte 055h                  ; b0 55
    mov dx, 00332h                            ; ba 32 03
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp AL, strict byte 055h                  ; 3c 55
    jne short 07b8eh                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 00333h                            ; ba 33 03
    out DX, AL                                ; ee
    mov ax, 00330h                            ; b8 30 03
    call 078b4h                               ; e8 26 fd
    mov AL, strict byte 055h                  ; b0 55
    mov dx, 00342h                            ; ba 42 03
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp AL, strict byte 055h                  ; 3c 55
    jne short 07ba7h                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 00343h                            ; ba 43 03
    out DX, AL                                ; ee
    mov ax, 00340h                            ; b8 40 03
    call 078b4h                               ; e8 0d fd
    mov AL, strict byte 055h                  ; b0 55
    mov dx, 00352h                            ; ba 52 03
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4
    cmp AL, strict byte 055h                  ; 3c 55
    jne short 07bc0h                          ; 75 0c
    xor al, al                                ; 30 c0
    mov dx, 00353h                            ; ba 53 03
    out DX, AL                                ; ee
    mov ax, 00350h                            ; b8 50 03
    call 078b4h                               ; e8 f4 fc
    pop bp                                    ; 5d
    retn                                      ; c3
high_bits_save_:                             ; 0xf7bc2 LB 0x14
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    shr eax, 010h                             ; 66 c1 e8 10
    mov es, dx                                ; 8e c2
    mov word [es:bx+00268h], ax               ; 26 89 87 68 02
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
high_bits_restore_:                          ; 0xf7bd6 LB 0x14
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov es, dx                                ; 8e c2
    mov ax, word [es:bx+00268h]               ; 26 8b 87 68 02
    sal eax, 010h                             ; 66 c1 e0 10
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_ctrl_set_bits_:                         ; 0xf7bea LB 0x42
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov ax, dx                                ; 89 d0
    mov word [bp-002h], bx                    ; 89 5e fe
    mov di, cx                                ; 89 cf
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea bx, [si+004h]                         ; 8d 5c 04
    mov dx, bx                                ; 89 da
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    or ax, word [bp-002h]                     ; 0b 46 fe
    mov cx, dx                                ; 89 d1
    or cx, di                                 ; 09 f9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ahci_ctrl_clear_bits_:                       ; 0xf7c2c LB 0x46
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov ax, dx                                ; 89 d0
    mov di, bx                                ; 89 df
    mov word [bp-002h], cx                    ; 89 4e fe
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea bx, [si+004h]                         ; 8d 5c 04
    mov dx, bx                                ; 89 da
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    not di                                    ; f7 d7
    mov cx, word [bp-002h]                    ; 8b 4e fe
    not cx                                    ; f7 d1
    and ax, di                                ; 21 f8
    and cx, dx                                ; 21 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ahci_ctrl_is_bit_set_:                       ; 0xf7c72 LB 0x36
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, ax                                ; 89 c6
    mov ax, dx                                ; 89 d0
    mov di, cx                                ; 89 cf
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea dx, [si+004h]                         ; 8d 54 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    test dx, di                               ; 85 fa
    jne short 07c9eh                          ; 75 04
    test ax, bx                               ; 85 d8
    je short 07ca2h                           ; 74 04
    mov AL, strict byte 001h                  ; b0 01
    jmp short 07ca4h                          ; eb 02
    xor al, al                                ; 30 c0
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn                                      ; c3
ahci_ctrl_extract_bits_:                     ; 0xf7ca8 LB 0x1c
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, ax                                ; 89 c6
    and ax, bx                                ; 21 d8
    and dx, cx                                ; 21 ca
    mov cl, byte [bp+006h]                    ; 8a 4e 06
    xor ch, ch                                ; 30 ed
    jcxz 07cbfh                               ; e3 06
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07cb9h                               ; e2 fa
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00
ahci_addr_to_phys_:                          ; 0xf7cc4 LB 0x1e
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, dx                                ; 89 d0
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00004h                ; b9 04 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 07cd2h                               ; e2 fa
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_port_cmd_sync_:                         ; 0xf7ce2 LB 0xdd
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    push ax                                   ; 50
    push ax                                   ; 50
    mov si, ax                                ; 89 c6
    mov cx, dx                                ; 89 d1
    mov dl, bl                                ; 88 da
    mov es, cx                                ; 8e c1
    mov al, byte [es:si+00262h]               ; 26 8a 84 62 02
    mov byte [bp-002h], al                    ; 88 46 fe
    mov di, word [es:si+00260h]               ; 26 8b bc 60 02
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 07d06h                          ; 75 03
    jmp near 07db8h                           ; e9 b2 00
    mov al, byte [es:si+00263h]               ; 26 8a 84 63 02
    xor ah, ah                                ; 30 e4
    xor bx, bx                                ; 31 db
    or bl, 080h                               ; 80 cb 80
    xor dh, dh                                ; 30 f6
    or bx, dx                                 ; 09 d3
    mov word [es:si], bx                      ; 26 89 1c
    mov word [es:si+002h], ax                 ; 26 89 44 02
    mov word [es:si+004h], strict word 00000h ; 26 c7 44 04 00 00
    mov word [es:si+006h], strict word 00000h ; 26 c7 44 06 00 00
    lea ax, [si+00080h]                       ; 8d 84 80 00
    mov dx, cx                                ; 89 ca
    call 07cc4h                               ; e8 92 ff
    mov es, cx                                ; 8e c1
    mov word [es:si+008h], ax                 ; 26 89 44 08
    mov word [es:si+00ah], dx                 ; 26 89 54 0a
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 007h                  ; b1 07
    mov si, ax                                ; 89 c6
    sal si, CL                                ; d3 e6
    lea dx, [si+00118h]                       ; 8d 94 18 01
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    mov ax, di                                ; 89 f8
    call 07beah                               ; e8 95 fe
    lea ax, [si+00138h]                       ; 8d 84 38 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, di                                ; 89 fa
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea dx, [di+004h]                         ; 8d 55 04
    mov ax, strict word 00001h                ; b8 01 00
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 007h                  ; b1 07
    sal ax, CL                                ; d3 e0
    mov word [bp-004h], ax                    ; 89 46 fc
    mov si, ax                                ; 89 c6
    add si, 00110h                            ; 81 c6 10 01
    mov bx, strict word 00001h                ; bb 01 00
    mov cx, 04000h                            ; b9 00 40
    mov dx, si                                ; 89 f2
    mov ax, di                                ; 89 f8
    call 07c72h                               ; e8 db fe
    test al, al                               ; 84 c0
    je short 07d78h                           ; 74 dd
    mov bx, strict word 00001h                ; bb 01 00
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    mov ax, di                                ; 89 f8
    call 07beah                               ; e8 43 fe
    mov dx, word [bp-004h]                    ; 8b 56 fc
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, strict word 00001h                ; bb 01 00
    xor cx, cx                                ; 31 c9
    mov ax, di                                ; 89 f8
    call 07c2ch                               ; e8 74 fe
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
ahci_cmd_data_:                              ; 0xf7dbf LB 0x1cf
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00010h                ; 83 ec 10
    mov di, ax                                ; 89 c7
    mov word [bp-006h], dx                    ; 89 56 fa
    mov byte [bp-002h], bl                    ; 88 5e fe
    xor si, si                                ; 31 f6
    mov es, dx                                ; 8e c2
    mov ax, word [es:di+001eeh]               ; 26 8b 85 ee 01
    mov word [bp-004h], ax                    ; 89 46 fc
    mov word [bp-010h], si                    ; 89 76 f0
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, word [es:di+00ah]                 ; 26 8b 45 0a
    mov word [bp-00ah], ax                    ; 89 46 f6
    mov ax, word [es:di+00ch]                 ; 26 8b 45 0c
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov ax, 00080h                            ; b8 80 00
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08dbbh                               ; e8 bd 0f
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si+00080h], 08027h           ; 26 c7 84 80 00 27 80
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov byte [es:si+00082h], al               ; 26 88 84 82 00
    mov byte [es:si+00083h], 000h             ; 26 c6 84 83 00 00
    mov es, [bp-006h]                         ; 8e 46 fa
    mov al, byte [es:di]                      ; 26 8a 05
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00084h], al               ; 26 88 84 84 00
    mov es, [bp-006h]                         ; 8e 46 fa
    mov ax, word [es:di]                      ; 26 8b 05
    mov dx, word [es:di+002h]                 ; 26 8b 55 02
    mov cx, strict word 00008h                ; b9 08 00
    shr dx, 1                                 ; d1 ea
    rcr ax, 1                                 ; d1 d8
    loop 07e31h                               ; e2 fa
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00085h], al               ; 26 88 84 85 00
    mov es, [bp-006h]                         ; 8e 46 fa
    mov ax, word [es:di+002h]                 ; 26 8b 45 02
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00086h], al               ; 26 88 84 86 00
    mov byte [es:si+00087h], 040h             ; 26 c6 84 87 00 40
    mov es, [bp-006h]                         ; 8e 46 fa
    mov al, byte [es:di+003h]                 ; 26 8a 45 03
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00088h], al               ; 26 88 84 88 00
    mov word [es:si+00089h], strict word 00000h ; 26 c7 84 89 00 00 00
    mov byte [es:si+0008bh], 000h             ; 26 c6 84 8b 00 00
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:si+0008ch], al               ; 26 88 84 8c 00
    mov al, byte [bp-009h]                    ; 8a 46 f7
    mov byte [es:si+0008dh], al               ; 26 88 84 8d 00
    mov word [es:si+00276h], strict word 00010h ; 26 c7 84 76 02 10 00
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    xor dx, dx                                ; 31 d2
    mov bx, word [bp-00ch]                    ; 8b 5e f4
    xor cx, cx                                ; 31 c9
    call 08dd2h                               ; e8 3e 0f
    push dx                                   ; 52
    push ax                                   ; 50
    mov es, [bp-006h]                         ; 8e 46 fa
    mov bx, word [es:di+004h]                 ; 26 8b 5d 04
    mov cx, word [es:di+006h]                 ; 26 8b 4d 06
    mov ax, 0026ah                            ; b8 6a 02
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08c4eh                               ; e8 a4 0d
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dl, byte [es:si+00263h]               ; 26 8a 94 63 02
    xor dh, dh                                ; 30 f6
    mov ax, word [es:si+0027eh]               ; 26 8b 84 7e 02
    add ax, strict word 0ffffh                ; 05 ff ff
    mov bx, word [es:si+00280h]               ; 26 8b 9c 80 02
    adc bx, strict byte 0ffffh                ; 83 d3 ff
    mov word [bp-00eh], bx                    ; 89 5e f2
    mov CL, strict byte 004h                  ; b1 04
    mov bx, dx                                ; 89 d3
    sal bx, CL                                ; d3 e3
    mov word [es:bx+0010ch], ax               ; 26 89 87 0c 01
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    mov word [es:bx+0010eh], ax               ; 26 89 87 0e 01
    mov ax, word [es:si+0027ah]               ; 26 8b 84 7a 02
    mov cx, word [es:si+0027ch]               ; 26 8b 8c 7c 02
    mov word [es:bx+00100h], ax               ; 26 89 87 00 01
    mov word [es:bx+00102h], cx               ; 26 89 8f 02 01
    mov ax, dx                                ; 89 d0
    inc ax                                    ; 40
    mov es, [bp-006h]                         ; 8e 46 fa
    cmp word [es:di+01ch], strict byte 00000h ; 26 83 7d 1c 00
    je short 07f28h                           ; 74 2d
    mov dx, word [es:di+01ch]                 ; 26 8b 55 1c
    dec dx                                    ; 4a
    mov CL, strict byte 004h                  ; b1 04
    mov di, ax                                ; 89 c7
    sal di, CL                                ; d3 e7
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:di+0010ch], dx               ; 26 89 95 0c 01
    mov word [es:di+0010eh], si               ; 26 89 b5 0e 01
    mov bx, word [es:si+00264h]               ; 26 8b 9c 64 02
    mov dx, word [es:si+00266h]               ; 26 8b 94 66 02
    mov word [es:di+00100h], bx               ; 26 89 9d 00 01
    mov word [es:di+00102h], dx               ; 26 89 95 02 01
    inc ax                                    ; 40
    mov es, [bp-008h]                         ; 8e 46 f8
    mov bx, word [bp-010h]                    ; 8b 5e f0
    mov byte [es:bx+00263h], al               ; 26 88 87 63 02
    xor ax, ax                                ; 31 c0
    mov es, [bp-008h]                         ; 8e 46 f8
    mov bx, word [bp-010h]                    ; 8b 5e f0
    mov dl, byte [es:bx+00263h]               ; 26 8a 97 63 02
    xor dh, dh                                ; 30 f6
    cmp ax, dx                                ; 39 d0
    jnc short 07f49h                          ; 73 03
    inc ax                                    ; 40
    jmp short 07f35h                          ; eb ec
    mov al, byte [bp-002h]                    ; 8a 46 fe
    cmp AL, strict byte 035h                  ; 3c 35
    jne short 07f56h                          ; 75 06
    mov byte [bp-002h], 040h                  ; c6 46 fe 40
    jmp short 07f69h                          ; eb 13
    cmp AL, strict byte 0a0h                  ; 3c a0
    jne short 07f66h                          ; 75 0c
    or byte [bp-002h], 020h                   ; 80 4e fe 20
    or byte [es:bx+00083h], 001h              ; 26 80 8f 83 00 01
    jmp short 07f69h                          ; eb 03
    mov byte [bp-002h], dh                    ; 88 76 fe
    or byte [bp-002h], 005h                   ; 80 4e fe 05
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    mov ax, word [bp-010h]                    ; 8b 46 f0
    mov dx, word [bp-008h]                    ; 8b 56 f8
    call 07ce2h                               ; e8 67 fd
    mov ax, word [bp-010h]                    ; 8b 46 f0
    add ax, 0026ah                            ; 05 6a 02
    mov dx, word [bp-008h]                    ; 8b 56 f8
    call 08cc2h                               ; e8 3b 0d
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
ahci_port_deinit_current_:                   ; 0xf7f8e LB 0x148
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00006h                ; 83 ec 06
    mov di, ax                                ; 89 c7
    mov word [bp-004h], dx                    ; 89 56 fc
    mov es, dx                                ; 8e c2
    mov si, word [es:di+00260h]               ; 26 8b b5 60 02
    mov al, byte [es:di+00262h]               ; 26 8a 85 62 02
    mov byte [bp-002h], al                    ; 88 46 fe
    cmp AL, strict byte 0ffh                  ; 3c ff
    jne short 07fb3h                          ; 75 03
    jmp near 080ceh                           ; e9 1b 01
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 007h                  ; b1 07
    mov dx, ax                                ; 89 c2
    sal dx, CL                                ; d3 e2
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    mov ax, si                                ; 89 f0
    call 07c2ch                               ; e8 63 fc
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 007h                  ; b1 07
    sal ax, CL                                ; d3 e0
    mov word [bp-006h], ax                    ; 89 46 fa
    mov dx, ax                                ; 89 c2
    add dx, 00118h                            ; 81 c2 18 01
    mov bx, 0c011h                            ; bb 11 c0
    xor cx, cx                                ; 31 c9
    mov ax, si                                ; 89 f0
    call 07c72h                               ; e8 8d fc
    cmp AL, strict byte 001h                  ; 3c 01
    je short 07fc9h                           ; 74 e0
    mov cx, strict word 00020h                ; b9 20 00
    xor bx, bx                                ; 31 db
    mov ax, di                                ; 89 f8
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08dbbh                               ; e8 c5 0d
    lea ax, [di+00080h]                       ; 8d 85 80 00
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08dbbh                               ; e8 b6 0d
    lea ax, [di+00200h]                       ; 8d 85 00 02
    mov cx, strict word 00060h                ; b9 60 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08dbbh                               ; e8 a7 0d
    mov ax, word [bp-006h]                    ; 8b 46 fa
    add ax, 00108h                            ; 05 08 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea bx, [si+004h]                         ; 8d 5c 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    add ax, 0010ch                            ; 05 0c 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    add ax, 00104h                            ; 05 04 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    add ax, 00114h                            ; 05 14 01
    cwd                                       ; 99
    mov cx, dx                                ; 89 d1
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:di+00262h], 0ffh             ; 26 c6 85 62 02 ff
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_port_init_:                             ; 0xf80d6 LB 0x20d
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00006h                ; 83 ec 06
    mov si, ax                                ; 89 c6
    mov word [bp-004h], dx                    ; 89 56 fc
    mov byte [bp-002h], bl                    ; 88 5e fe
    call 07f8eh                               ; e8 a4 fe
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 007h                  ; b1 07
    mov dx, ax                                ; 89 c2
    sal dx, CL                                ; d3 e2
    add dx, 00118h                            ; 81 c2 18 01
    mov es, [bp-004h]                         ; 8e 46 fc
    mov ax, word [es:si+00260h]               ; 26 8b 84 60 02
    mov bx, strict word 00011h                ; bb 11 00
    xor cx, cx                                ; 31 c9
    call 07c2ch                               ; e8 24 fb
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    mov CL, strict byte 007h                  ; b1 07
    mov di, ax                                ; 89 c7
    sal di, CL                                ; d3 e7
    lea dx, [di+00118h]                       ; 8d 95 18 01
    mov es, [bp-004h]                         ; 8e 46 fc
    mov ax, word [es:si+00260h]               ; 26 8b 84 60 02
    mov bx, 0c011h                            ; bb 11 c0
    xor cx, cx                                ; 31 c9
    call 07c72h                               ; e8 4b fb
    cmp AL, strict byte 001h                  ; 3c 01
    je short 08108h                           ; 74 dd
    mov cx, strict word 00020h                ; b9 20 00
    xor bx, bx                                ; 31 db
    mov ax, si                                ; 89 f0
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08dbbh                               ; e8 83 0c
    lea ax, [si+00080h]                       ; 8d 84 80 00
    mov cx, strict word 00040h                ; b9 40 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08dbbh                               ; e8 74 0c
    mov ax, si                                ; 89 f0
    add ah, 002h                              ; 80 c4 02
    mov word [bp-006h], ax                    ; 89 46 fa
    mov cx, strict word 00060h                ; b9 60 00
    xor bx, bx                                ; 31 db
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 08dbbh                               ; e8 61 0c
    lea ax, [di+00108h]                       ; 8d 85 08 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 07cc4h                               ; e8 47 fb
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    add bx, strict byte 00004h                ; 83 c3 04
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+0010ch]                       ; 8d 85 0c 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00100h]                       ; 8d 85 00 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, si                                ; 89 f0
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 07cc4h                               ; e8 db fa
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    add bx, strict byte 00004h                ; 83 c3 04
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00104h]                       ; 8d 85 04 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00114h]                       ; 8d 85 14 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00110h]                       ; 8d 85 10 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 0ffffh                ; b8 ff ff
    mov cx, ax                                ; 89 c1
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea ax, [di+00130h]                       ; 8d 85 30 01
    cwd                                       ; 99
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [es:si+00260h]               ; 26 8b 9c 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-004h]                         ; 8e 46 fc
    mov dx, word [es:si+00260h]               ; 26 8b 94 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 0ffffh                ; b8 ff ff
    mov cx, ax                                ; 89 c1
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov al, byte [bp-002h]                    ; 8a 46 fe
    mov es, [bp-004h]                         ; 8e 46 fc
    mov byte [es:si+00262h], al               ; 26 88 84 62 02
    mov byte [es:si+00263h], 000h             ; 26 c6 84 63 02 00
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
@ahci_read_sectors:                          ; 0xf82e3 LB 0x96
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    les bx, [bp+008h]                         ; c4 5e 08
    mov al, byte [es:bx+008h]                 ; 26 8a 47 08
    xor ah, ah                                ; 30 e4
    mov di, ax                                ; 89 c7
    sub di, strict byte 0000ch                ; 83 ef 0c
    cmp di, strict byte 00004h                ; 83 ff 04
    jbe short 0830eh                          ; 76 13
    push di                                   ; 57
    mov ax, 00b82h                            ; b8 82 0b
    push ax                                   ; 50
    mov ax, 00b94h                            ; b8 94 0b
    push ax                                   ; 50
    mov ax, strict word 00007h                ; b8 07 00
    push ax                                   ; 50
    call 018fah                               ; e8 ef 95
    add sp, strict byte 00008h                ; 83 c4 08
    les bx, [bp+008h]                         ; c4 5e 08
    mov dx, word [es:bx+001eeh]               ; 26 8b 97 ee 01
    xor ax, ax                                ; 31 c0
    call 07bc2h                               ; e8 a7 f8
    mov es, [bp+00ah]                         ; 8e 46 0a
    add bx, di                                ; 01 fb
    mov bl, byte [es:bx+001e9h]               ; 26 8a 9f e9 01
    xor bh, bh                                ; 30 ff
    mov di, word [bp+008h]                    ; 8b 7e 08
    mov dx, word [es:di+001eeh]               ; 26 8b 95 ee 01
    xor ax, ax                                ; 31 c0
    call 080d6h                               ; e8 a2 fd
    mov bx, strict word 00025h                ; bb 25 00
    mov ax, di                                ; 89 f8
    mov dx, word [bp+00ah]                    ; 8b 56 0a
    call 07dbfh                               ; e8 80 fa
    mov CL, strict byte 009h                  ; b1 09
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov bx, di                                ; 89 fb
    mov ax, word [es:bx+00ah]                 ; 26 8b 47 0a
    sal ax, CL                                ; d3 e0
    mov cx, ax                                ; 89 c1
    shr cx, 1                                 ; d1 e9
    mov di, word [es:di+004h]                 ; 26 8b 7d 04
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    mov si, di                                ; 89 fe
    mov dx, ax                                ; 89 c2
    mov es, ax                                ; 8e c0
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    mov es, [bp+00ah]                         ; 8e 46 0a
    mov dx, word [es:bx+001eeh]               ; 26 8b 97 ee 01
    xor ax, ax                                ; 31 c0
    call 07bd6h                               ; e8 65 f8
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
@ahci_write_sectors:                         ; 0xf8379 LB 0x6b
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov si, word [bp+006h]                    ; 8b 76 06
    mov cx, word [bp+008h]                    ; 8b 4e 08
    mov es, cx                                ; 8e c1
    mov bl, byte [es:si+008h]                 ; 26 8a 5c 08
    xor bh, bh                                ; 30 ff
    sub bx, strict byte 0000ch                ; 83 eb 0c
    cmp bx, strict byte 00004h                ; 83 fb 04
    jbe short 083a6h                          ; 76 13
    push bx                                   ; 53
    mov dx, 00bb3h                            ; ba b3 0b
    push dx                                   ; 52
    mov dx, 00b94h                            ; ba 94 0b
    push dx                                   ; 52
    mov dx, strict word 00007h                ; ba 07 00
    push dx                                   ; 52
    call 018fah                               ; e8 57 95
    add sp, strict byte 00008h                ; 83 c4 08
    mov es, cx                                ; 8e c1
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 07bc2h                               ; e8 10 f8
    mov es, cx                                ; 8e c1
    add bx, si                                ; 01 f3
    mov bl, byte [es:bx+001e9h]               ; 26 8a 9f e9 01
    xor bh, bh                                ; 30 ff
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 080d6h                               ; e8 0f fd
    mov bx, strict word 00035h                ; bb 35 00
    mov ax, si                                ; 89 f0
    mov dx, cx                                ; 89 ca
    call 07dbfh                               ; e8 ee f9
    mov es, cx                                ; 8e c1
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 07bd6h                               ; e8 f9 f7
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
ahci_cmd_packet_:                            ; 0xf83e4 LB 0x178
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0000eh                ; 83 ec 0e
    push ax                                   ; 50
    mov byte [bp-002h], dl                    ; 88 56 fe
    mov word [bp-00eh], bx                    ; 89 5e f2
    mov word [bp-00ch], cx                    ; 89 4e f4
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 1d 92
    mov si, 00122h                            ; be 22 01
    mov word [bp-004h], ax                    ; 89 46 fc
    cmp byte [bp+00eh], 002h                  ; 80 7e 0e 02
    jne short 0842eh                          ; 75 23
    mov bx, 00c16h                            ; bb 16 0c
    mov cx, ds                                ; 8c d9
    mov ax, strict word 00004h                ; b8 04 00
    call 018bdh                               ; e8 a7 94
    mov ax, 00bc6h                            ; b8 c6 0b
    push ax                                   ; 50
    mov ax, 00bd6h                            ; b8 d6 0b
    push ax                                   ; 50
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 018fah                               ; e8 d5 94
    add sp, strict byte 00006h                ; 83 c4 06
    mov ax, strict word 00001h                ; b8 01 00
    jmp near 08554h                           ; e9 26 01
    test byte [bp+008h], 001h                 ; f6 46 08 01
    jne short 08428h                          ; 75 f4
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov di, word [bp+00ch]                    ; 8b 7e 0c
    mov cx, strict word 00008h                ; b9 08 00
    sal bx, 1                                 ; d1 e3
    rcl di, 1                                 ; d1 d7
    loop 0843dh                               ; e2 fa
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si], bx                      ; 26 89 1c
    mov word [es:si+002h], di                 ; 26 89 7c 02
    mov ax, word [bp+010h]                    ; 8b 46 10
    mov word [es:si+004h], ax                 ; 26 89 44 04
    mov ax, word [bp+012h]                    ; 8b 46 12
    mov word [es:si+006h], ax                 ; 26 89 44 06
    mov bx, word [es:si+00ch]                 ; 26 8b 5c 0c
    mov ax, word [bp+00ah]                    ; 8b 46 0a
    mov dx, word [bp+00ch]                    ; 8b 56 0c
    xor cx, cx                                ; 31 c9
    call 08d37h                               ; e8 cd 08
    mov word [es:si+00ah], ax                 ; 26 89 44 0a
    xor di, di                                ; 31 ff
    mov ax, word [es:si+001eeh]               ; 26 8b 84 ee 01
    mov word [bp-006h], ax                    ; 89 46 fa
    mov word [bp-00ah], di                    ; 89 7e f6
    mov word [bp-008h], ax                    ; 89 46 f8
    add word [bp-010h], strict byte 0fff4h    ; 83 46 f0 f4
    xor ax, ax                                ; 31 c0
    mov dx, word [bp-006h]                    ; 8b 56 fa
    call 07bc2h                               ; e8 38 f7
    mov es, [bp-004h]                         ; 8e 46 fc
    mov bx, word [bp-010h]                    ; 8b 5e f0
    add bx, si                                ; 01 f3
    mov bl, byte [es:bx+001e9h]               ; 26 8a 9f e9 01
    xor bh, bh                                ; 30 ff
    mov dx, word [es:si+001eeh]               ; 26 8b 94 ee 01
    xor ax, ax                                ; 31 c0
    call 080d6h                               ; e8 33 fc
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    push ax                                   ; 50
    mov bx, word [bp-00eh]                    ; 8b 5e f2
    mov cx, word [bp-00ch]                    ; 8b 4e f4
    mov ax, 000c0h                            ; b8 c0 00
    mov dx, word [bp-006h]                    ; 8b 56 fa
    call 08deah                               ; e8 32 09
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si+014h], di                 ; 26 89 7c 14
    mov word [es:si+016h], di                 ; 26 89 7c 16
    mov word [es:si+018h], di                 ; 26 89 7c 18
    mov ax, word [es:si+01ah]                 ; 26 8b 44 1a
    test ax, ax                               ; 85 c0
    je short 084f6h                           ; 74 27
    dec ax                                    ; 48
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:di+0010ch], ax               ; 26 89 85 0c 01
    mov word [es:di+0010eh], di               ; 26 89 bd 0e 01
    mov ax, word [es:di+00264h]               ; 26 8b 85 64 02
    mov dx, word [es:di+00266h]               ; 26 8b 95 66 02
    mov word [es:di+00100h], ax               ; 26 89 85 00 01
    mov word [es:di+00102h], dx               ; 26 89 95 02 01
    inc byte [es:di+00263h]                   ; 26 fe 85 63 02
    mov bx, 000a0h                            ; bb a0 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-004h]                    ; 8b 56 fc
    call 07dbfh                               ; e8 be f8
    les bx, [bp-00ah]                         ; c4 5e f6
    mov ax, word [es:bx+004h]                 ; 26 8b 47 04
    mov dx, word [es:bx+006h]                 ; 26 8b 57 06
    mov es, [bp-004h]                         ; 8e 46 fc
    mov word [es:si+016h], ax                 ; 26 89 44 16
    mov word [es:si+018h], dx                 ; 26 89 54 18
    mov bx, word [es:si+016h]                 ; 26 8b 5c 16
    mov cx, dx                                ; 89 d1
    shr cx, 1                                 ; d1 e9
    rcr bx, 1                                 ; d1 db
    mov di, word [es:si+004h]                 ; 26 8b 7c 04
    mov ax, word [es:si+006h]                 ; 26 8b 44 06
    mov cx, bx                                ; 89 d9
    mov si, di                                ; 89 fe
    mov dx, ax                                ; 89 c2
    mov es, ax                                ; 8e c0
    push DS                                   ; 1e
    mov ds, dx                                ; 8e da
    rep movsw                                 ; f3 a5
    pop DS                                    ; 1f
    mov ax, word [bp-00ah]                    ; 8b 46 f6
    mov dx, word [bp-008h]                    ; 8b 56 f8
    call 07bd6h                               ; e8 96 f6
    les bx, [bp-00ah]                         ; c4 5e f6
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06
    or ax, word [es:bx+004h]                  ; 26 0b 47 04
    jne short 08552h                          ; 75 05
    mov ax, strict word 00004h                ; b8 04 00
    jmp short 08554h                          ; eb 02
    xor ax, ax                                ; 31 c0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 0000ch                               ; c2 0c 00
ahci_port_detect_device_:                    ; 0xf855c LB 0x3a0
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, 0021ah                            ; 81 ec 1a 02
    mov di, ax                                ; 89 c7
    mov word [bp-014h], dx                    ; 89 56 ec
    mov byte [bp-004h], bl                    ; 88 5e fc
    mov al, bl                                ; 88 d8
    mov byte [bp-016h], bl                    ; 88 5e ea
    xor al, bl                                ; 30 d8
    mov byte [bp-015h], al                    ; 88 46 eb
    mov bx, word [bp-016h]                    ; 8b 5e ea
    mov ax, di                                ; 89 f8
    call 080d6h                               ; e8 56 fb
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 93 90
    mov word [bp-010h], ax                    ; 89 46 f0
    mov si, 00122h                            ; be 22 01
    mov word [bp-012h], ax                    ; 89 46 ee
    mov word [bp-01ah], si                    ; 89 76 e6
    mov word [bp-018h], ax                    ; 89 46 e8
    mov CL, strict byte 007h                  ; b1 07
    mov ax, word [bp-016h]                    ; 8b 46 ea
    sal ax, CL                                ; d3 e0
    mov word [bp-00eh], ax                    ; 89 46 f2
    add ax, 0012ch                            ; 05 2c 01
    cwd                                       ; 99
    mov word [bp-016h], ax                    ; 89 46 ea
    mov bx, dx                                ; 89 d3
    mov es, [bp-014h]                         ; 8e 46 ec
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    mov cx, bx                                ; 89 d9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-014h]                         ; 8e 46 ec
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    mov ax, strict word 00001h                ; b8 01 00
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-014h]                         ; 8e 46 ec
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    mov ax, word [bp-016h]                    ; 8b 46 ea
    mov cx, bx                                ; 89 d9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-014h]                         ; 8e 46 ec
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    add ax, 00128h                            ; 05 28 01
    cwd                                       ; 99
    mov es, [bp-014h]                         ; 8e 46 ec
    mov bx, word [es:di+00260h]               ; 26 8b 9d 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-014h]                         ; 8e 46 ec
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    xor bx, bx                                ; 31 db
    push bx                                   ; 53
    mov bx, strict word 0000fh                ; bb 0f 00
    xor cx, cx                                ; 31 c9
    call 07ca8h                               ; e8 68 f6
    cmp ax, strict word 00003h                ; 3d 03 00
    je short 08648h                           ; 74 03
    jmp near 088f5h                           ; e9 ad 02
    mov es, [bp-012h]                         ; 8e 46 ee
    mov al, byte [es:si+001edh]               ; 26 8a 84 ed 01
    mov byte [bp-002h], al                    ; 88 46 fe
    cmp AL, strict byte 004h                  ; 3c 04
    jnc short 08645h                          ; 73 ee
    mov dx, word [bp-00eh]                    ; 8b 56 f2
    add dx, 00118h                            ; 81 c2 18 01
    mov es, [bp-014h]                         ; 8e 46 ec
    mov ax, word [es:di+00260h]               ; 26 8b 85 60 02
    mov bx, strict word 00010h                ; bb 10 00
    xor cx, cx                                ; 31 c9
    call 07beah                               ; e8 7c f5
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    add ax, 00124h                            ; 05 24 01
    cwd                                       ; 99
    mov es, [bp-014h]                         ; 8e 46 ec
    mov bx, word [es:di+00260h]               ; 26 8b 9d 60 02
    mov cx, dx                                ; 89 d1
    mov dx, bx                                ; 89 da
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov es, [bp-014h]                         ; 8e 46 ec
    mov dx, word [es:di+00260h]               ; 26 8b 95 60 02
    add dx, strict byte 00004h                ; 83 c2 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    mov cl, byte [bp-002h]                    ; 8a 4e fe
    add cl, 00ch                              ; 80 c1 0c
    test dx, dx                               ; 85 d2
    jne short 086eah                          ; 75 42
    cmp ax, 00101h                            ; 3d 01 01
    jne short 086eah                          ; 75 3d
    mov es, [bp-012h]                         ; 8e 46 ee
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00
    mov word [es:si+002h], strict word 00000h ; 26 c7 44 02 00 00
    lea dx, [bp-0021ah]                       ; 8d 96 e6 fd
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov [es:si+006h], ss                      ; 26 8c 54 06
    mov word [es:si+00ah], strict word 00001h ; 26 c7 44 0a 01 00
    mov word [es:si+00ch], 00200h             ; 26 c7 44 0c 00 02
    mov bx, 000ech                            ; bb ec 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-010h]                    ; 8b 56 f0
    call 07dbfh                               ; e8 e1 f6
    test byte [bp-0021ah], 080h               ; f6 86 e6 fd 80
    je short 086edh                           ; 74 08
    mov ax, strict word 00001h                ; b8 01 00
    jmp short 086efh                          ; eb 05
    jmp near 0883fh                           ; e9 52 01
    xor ax, ax                                ; 31 c0
    mov ch, al                                ; 88 c5
    mov ax, word [bp-00218h]                  ; 8b 86 e8 fd
    mov word [bp-00ch], ax                    ; 89 46 f4
    mov ax, word [bp-00214h]                  ; 8b 86 ec fd
    mov word [bp-008h], ax                    ; 89 46 f8
    mov di, word [bp-0020eh]                  ; 8b be f2 fd
    mov ax, word [bp-001a2h]                  ; 8b 86 5e fe
    mov word [bp-006h], ax                    ; 89 46 fa
    mov bx, word [bp-001a0h]                  ; 8b 9e 60 fe
    cmp bx, 00fffh                            ; 81 fb ff 0f
    jne short 08722h                          ; 75 0e
    cmp ax, strict word 0ffffh                ; 3d ff ff
    jne short 08722h                          ; 75 09
    mov ax, word [bp-00152h]                  ; 8b 86 ae fe
    mov word [bp-006h], ax                    ; 89 46 fa
    xor bx, bx                                ; 31 db
    mov al, byte [bp-002h]                    ; 8a 46 fe
    xor ah, ah                                ; 30 e4
    les si, [bp-01ah]                         ; c4 76 e6
    add si, ax                                ; 01 c6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [es:si+001e9h], al               ; 26 88 84 e9 01
    mov al, cl                                ; 88 c8
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov si, word [bp-01ah]                    ; 8b 76 e6
    add si, ax                                ; 01 c6
    mov word [es:si+01eh], 0ff05h             ; 26 c7 44 1e 05 ff
    mov byte [es:si+020h], ch                 ; 26 88 6c 20
    mov byte [es:si+021h], 000h               ; 26 c6 44 21 00
    mov word [es:si+024h], 00200h             ; 26 c7 44 24 00 02
    mov byte [es:si+023h], 001h               ; 26 c6 44 23 01
    mov ax, word [bp-006h]                    ; 8b 46 fa
    mov word [es:si+032h], ax                 ; 26 89 44 32
    mov word [es:si+034h], bx                 ; 26 89 5c 34
    mov ax, word [bp-008h]                    ; 8b 46 f8
    mov word [es:si+02ch], ax                 ; 26 89 44 2c
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov word [es:si+02eh], ax                 ; 26 89 44 2e
    mov word [es:si+030h], di                 ; 26 89 7c 30
    mov al, byte [bp-002h]                    ; 8a 46 fe
    cmp AL, strict byte 001h                  ; 3c 01
    jc short 0878ah                           ; 72 0c
    jbe short 08792h                          ; 76 12
    cmp AL, strict byte 003h                  ; 3c 03
    je short 0879ah                           ; 74 16
    cmp AL, strict byte 002h                  ; 3c 02
    je short 08796h                           ; 74 0e
    jmp short 087e1h                          ; eb 57
    test al, al                               ; 84 c0
    jne short 087e1h                          ; 75 53
    mov DL, strict byte 040h                  ; b2 40
    jmp short 0879ch                          ; eb 0a
    mov DL, strict byte 048h                  ; b2 48
    jmp short 0879ch                          ; eb 06
    mov DL, strict byte 050h                  ; b2 50
    jmp short 0879ch                          ; eb 02
    mov DL, strict byte 058h                  ; b2 58
    mov bl, dl                                ; 88 d3
    add bl, 007h                              ; 80 c3 07
    xor bh, bh                                ; 30 ff
    mov ax, bx                                ; 89 d8
    call 0165ch                               ; e8 b4 8e
    test al, al                               ; 84 c0
    je short 087e1h                           ; 74 35
    mov al, dl                                ; 88 d0
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 a7 8e
    mov byte [bp-015h], al                    ; 88 46 eb
    mov byte [bp-016h], bh                    ; 88 7e ea
    mov al, dl                                ; 88 d0
    xor ah, ah                                ; 30 e4
    call 0165ch                               ; e8 9a 8e
    xor ah, ah                                ; 30 e4
    mov si, word [bp-016h]                    ; 8b 76 ea
    add si, ax                                ; 01 c6
    mov word [bp-00ch], si                    ; 89 76 f4
    mov al, dl                                ; 88 d0
    add AL, strict byte 002h                  ; 04 02
    call 0165ch                               ; e8 89 8e
    xor ah, ah                                ; 30 e4
    mov word [bp-008h], ax                    ; 89 46 f8
    mov ax, bx                                ; 89 d8
    call 0165ch                               ; e8 7f 8e
    xor ah, ah                                ; 30 e4
    mov di, ax                                ; 89 c7
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    les si, [bp-01ah]                         ; c4 76 e6
    add si, ax                                ; 01 c6
    mov ax, word [bp-008h]                    ; 8b 46 f8
    mov word [es:si+026h], ax                 ; 26 89 44 26
    mov ax, word [bp-00ch]                    ; 8b 46 f4
    mov word [es:si+028h], ax                 ; 26 89 44 28
    mov word [es:si+02ah], di                 ; 26 89 7c 2a
    mov bx, word [bp-01ah]                    ; 8b 5e e6
    mov dl, byte [es:bx+0019eh]               ; 26 8a 97 9e 01
    mov al, byte [bp-002h]                    ; 8a 46 fe
    add AL, strict byte 00ch                  ; 04 0c
    mov bl, dl                                ; 88 d3
    xor bh, bh                                ; 30 ff
    add bx, word [bp-01ah]                    ; 03 5e e6
    mov byte [es:bx+0019fh], al               ; 26 88 87 9f 01
    db  0feh, 0c2h
    ; inc dl                                    ; fe c2
    mov bx, word [bp-01ah]                    ; 8b 5e e6
    mov byte [es:bx+0019eh], dl               ; 26 88 97 9e 01
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 01600h                               ; e8 d3 8d
    mov bl, al                                ; 88 c3
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    xor bh, bh                                ; 30 ff
    mov dx, strict word 00075h                ; ba 75 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0160eh                               ; e8 d2 8d
    jmp near 088e7h                           ; e9 a8 00
    cmp dx, 0eb14h                            ; 81 fa 14 eb
    jne short 08889h                          ; 75 44
    cmp ax, 00101h                            ; 3d 01 01
    jne short 08889h                          ; 75 3f
    mov es, [bp-012h]                         ; 8e 46 ee
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00
    mov word [es:si+002h], strict word 00000h ; 26 c7 44 02 00 00
    lea dx, [bp-0021ah]                       ; 8d 96 e6 fd
    mov word [es:si+004h], dx                 ; 26 89 54 04
    mov [es:si+006h], ss                      ; 26 8c 54 06
    mov word [es:si+00ah], strict word 00001h ; 26 c7 44 0a 01 00
    mov word [es:si+00ch], 00200h             ; 26 c7 44 0c 00 02
    mov bx, 000a1h                            ; bb a1 00
    mov ax, si                                ; 89 f0
    mov dx, word [bp-010h]                    ; 8b 56 f0
    call 07dbfh                               ; e8 44 f5
    test byte [bp-0021ah], 080h               ; f6 86 e6 fd 80
    je short 0888bh                           ; 74 09
    mov word [bp-00ah], strict word 00001h    ; c7 46 f6 01 00
    jmp short 08890h                          ; eb 07
    jmp short 088e7h                          ; eb 5c
    mov word [bp-00ah], strict word 00000h    ; c7 46 f6 00 00
    mov bl, byte [bp-002h]                    ; 8a 5e fe
    xor bh, bh                                ; 30 ff
    mov es, [bp-018h]                         ; 8e 46 e8
    add bx, word [bp-01ah]                    ; 03 5e e6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [es:bx+001e9h], al               ; 26 88 87 e9 01
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    mov dx, strict word 00018h                ; ba 18 00
    imul dx                                   ; f7 ea
    mov si, word [bp-01ah]                    ; 8b 76 e6
    add si, ax                                ; 01 c6
    mov word [es:si+01eh], 00505h             ; 26 c7 44 1e 05 05
    mov al, byte [bp-00ah]                    ; 8a 46 f6
    mov byte [es:si+020h], al                 ; 26 88 44 20
    mov word [es:si+024h], 00800h             ; 26 c7 44 24 00 08
    mov bx, word [bp-01ah]                    ; 8b 5e e6
    mov dl, byte [es:bx+001afh]               ; 26 8a 97 af 01
    mov al, byte [bp-002h]                    ; 8a 46 fe
    add AL, strict byte 00ch                  ; 04 0c
    mov bl, dl                                ; 88 d3
    xor bh, bh                                ; 30 ff
    add bx, word [bp-01ah]                    ; 03 5e e6
    mov byte [es:bx+001b0h], al               ; 26 88 87 b0 01
    db  0feh, 0c2h
    ; inc dl                                    ; fe c2
    mov bx, word [bp-01ah]                    ; 8b 5e e6
    mov byte [es:bx+001afh], dl               ; 26 88 97 af 01
    inc byte [bp-002h]                        ; fe 46 fe
    mov al, byte [bp-002h]                    ; 8a 46 fe
    les bx, [bp-01ah]                         ; c4 5e e6
    mov byte [es:bx+001edh], al               ; 26 88 87 ed 01
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    retn                                      ; c3
ahci_mem_alloc_:                             ; 0xf88fc LB 0x40
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dx, 00413h                            ; ba 13 04
    xor ax, ax                                ; 31 c0
    call 0161ch                               ; e8 10 8d
    test ax, ax                               ; 85 c0
    je short 08935h                           ; 74 25
    dec ax                                    ; 48
    mov bx, ax                                ; 89 c3
    xor dx, dx                                ; 31 d2
    mov cx, strict word 0000ah                ; b9 0a 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08918h                               ; e2 fa
    mov si, ax                                ; 89 c6
    mov di, dx                                ; 89 d7
    mov cx, strict word 00004h                ; b9 04 00
    shr di, 1                                 ; d1 ef
    rcr si, 1                                 ; d1 de
    loop 08925h                               ; e2 fa
    mov dx, 00413h                            ; ba 13 04
    xor ax, ax                                ; 31 c0
    call 0162ah                               ; e8 f7 8c
    mov ax, si                                ; 89 f0
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
ahci_hba_init_:                              ; 0xf893c LB 0x12c
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 00006h                ; 83 ec 06
    mov si, ax                                ; 89 c6
    mov dx, strict word 0000eh                ; ba 0e 00
    mov ax, strict word 00040h                ; b8 40 00
    call 0161ch                               ; e8 ca 8c
    mov bx, 00122h                            ; bb 22 01
    mov word [bp-006h], ax                    ; 89 46 fa
    mov ax, strict word 00010h                ; b8 10 00
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea dx, [si+004h]                         ; 8d 54 04
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    call 088fch                               ; e8 85 ff
    mov di, ax                                ; 89 c7
    test ax, ax                               ; 85 c0
    jne short 08980h                          ; 75 03
    jmp near 08a48h                           ; e9 c8 00
    mov es, [bp-006h]                         ; 8e 46 fa
    mov word [es:bx+001eeh], di               ; 26 89 bf ee 01
    mov byte [es:bx+001edh], 000h             ; 26 c6 87 ed 01 00
    xor bx, bx                                ; 31 db
    mov es, di                                ; 8e c7
    mov byte [es:bx+00262h], 0ffh             ; 26 c6 87 62 02 ff
    mov word [es:bx+00260h], si               ; 26 89 b7 60 02
    mov word [es:bx+00264h], 0c000h           ; 26 c7 87 64 02 00 c0
    mov word [es:bx+00266h], strict word 0000ch ; 26 c7 87 66 02 0c 00
    mov bx, strict word 00001h                ; bb 01 00
    xor cx, cx                                ; 31 c9
    mov dx, strict word 00004h                ; ba 04 00
    mov ax, si                                ; 89 f0
    call 07beah                               ; e8 32 f2
    mov ax, strict word 00004h                ; b8 04 00
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    lea bx, [si+004h]                         ; 8d 5c 04
    mov dx, bx                                ; 89 da
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    test AL, strict byte 001h                 ; a8 01
    jne short 089b8h                          ; 75 de
    xor ax, ax                                ; 31 c0
    xor cx, cx                                ; 31 c9
    mov dx, si                                ; 89 f2
    xchg cx, ax                               ; 91
    sal eax, 010h                             ; 66 c1 e0 10
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, eax                               ; 66 ef
    mov dx, bx                                ; 89 da
    in eax, DX                                ; 66 ed
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0
    shr eax, 010h                             ; 66 c1 e8 10
    xchg dx, ax                               ; 92
    xor bx, bx                                ; 31 db
    push bx                                   ; 53
    mov bx, strict word 0001fh                ; bb 1f 00
    xor cx, cx                                ; 31 c9
    call 07ca8h                               ; e8 a9 f2
    db  0feh, 0c0h
    ; inc al                                    ; fe c0
    mov byte [bp-002h], al                    ; 88 46 fe
    mov byte [bp-004h], 000h                  ; c6 46 fc 00
    jmp short 08a13h                          ; eb 09
    inc byte [bp-004h]                        ; fe 46 fc
    cmp byte [bp-004h], 020h                  ; 80 7e fc 20
    jnc short 08a46h                          ; 73 33
    mov cl, byte [bp-004h]                    ; 8a 4e fc
    xor ch, ch                                ; 30 ed
    mov ax, strict word 00001h                ; b8 01 00
    xor dx, dx                                ; 31 d2
    jcxz 08a25h                               ; e3 06
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08a1fh                               ; e2 fa
    mov bx, ax                                ; 89 c3
    mov cx, dx                                ; 89 d1
    mov dx, strict word 0000ch                ; ba 0c 00
    mov ax, si                                ; 89 f0
    call 07c72h                               ; e8 41 f2
    test al, al                               ; 84 c0
    je short 08a0ah                           ; 74 d5
    mov bl, byte [bp-004h]                    ; 8a 5e fc
    xor bh, bh                                ; 30 ff
    xor ax, ax                                ; 31 c0
    mov dx, di                                ; 89 fa
    call 0855ch                               ; e8 1b fb
    dec byte [bp-002h]                        ; fe 4e fe
    jne short 08a0ah                          ; 75 c4
    xor ax, ax                                ; 31 c0
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
    or ax, word [di]                          ; 0b 05
    add AL, strict byte 003h                  ; 04 03
    add al, byte [bx+di]                      ; 02 01
    add byte [di-075h], cl                    ; 00 4d 8b
    sub cx, word [bp+di-074cfh]               ; 2b 8b 31 8b
    aaa                                       ; 37
    mov di, word [di]                         ; 8b 3d
    mov ax, word [bp+di-075h]                 ; 8b 43 8b
    dec cx                                    ; 49
    mov cx, word [di-075h]                    ; 8b 4d 8b
_ahci_init:                                  ; 0xf8a68 LB 0x119
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    sub sp, strict byte 0000eh                ; 83 ec 0e
    mov ax, 00601h                            ; b8 01 06
    mov dx, strict word 00001h                ; ba 01 00
    call 08b81h                               ; e8 09 01
    mov dx, ax                                ; 89 c2
    cmp ax, strict word 0ffffh                ; 3d ff ff
    je short 08ac8h                           ; 74 49
    mov al, ah                                ; 88 e0
    mov byte [bp-004h], ah                    ; 88 66 fc
    mov byte [bp-006h], dl                    ; 88 56 fa
    xor dh, ah                                ; 30 e6
    xor ah, ah                                ; 30 e4
    mov bx, strict word 00034h                ; bb 34 00
    call 08ba9h                               ; e8 18 01
    mov cl, al                                ; 88 c1
    test cl, cl                               ; 84 c9
    je short 08acbh                           ; 74 34
    mov bl, cl                                ; 88 cb
    xor bh, bh                                ; 30 ff
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [bp-00ah], al                    ; 88 46 f6
    mov byte [bp-009h], bh                    ; 88 7e f7
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [bp-00eh], al                    ; 88 46 f2
    mov byte [bp-00dh], bh                    ; 88 7e f3
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    call 08ba9h                               ; e8 f3 00
    cmp AL, strict byte 012h                  ; 3c 12
    je short 08acbh                           ; 74 11
    mov bl, cl                                ; 88 cb
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    xor bh, bh                                ; 30 ff
    mov dx, word [bp-00ah]                    ; 8b 56 f6
    mov ax, word [bp-00eh]                    ; 8b 46 f2
    jmp short 08a8eh                          ; eb c6
    jmp near 08b7ch                           ; e9 b1 00
    test cl, cl                               ; 84 c9
    je short 08ac8h                           ; 74 f9
    add cl, 002h                              ; 80 c1 02
    mov bl, cl                                ; 88 cb
    xor bh, bh                                ; 30 ff
    mov al, byte [bp-006h]                    ; 8a 46 fa
    mov byte [bp-00ch], al                    ; 88 46 f4
    mov byte [bp-00bh], bh                    ; 88 7e f5
    mov al, byte [bp-004h]                    ; 8a 46 fc
    mov byte [bp-008h], al                    ; 88 46 f8
    mov byte [bp-007h], bh                    ; 88 7e f9
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    mov ax, word [bp-008h]                    ; 8b 46 f8
    call 08ba9h                               ; e8 b8 00
    cmp AL, strict byte 010h                  ; 3c 10
    jne short 08ac8h                          ; 75 d3
    mov byte [bp-002h], 000h                  ; c6 46 fe 00
    mov bl, cl                                ; 88 cb
    add bl, 002h                              ; 80 c3 02
    xor bh, bh                                ; 30 ff
    mov dx, word [bp-00ch]                    ; 8b 56 f4
    mov ax, word [bp-008h]                    ; 8b 46 f8
    call 08bcdh                               ; e8 c4 00
    mov dx, ax                                ; 89 c2
    and ax, strict word 0000fh                ; 25 0f 00
    sub ax, strict word 00004h                ; 2d 04 00
    cmp ax, strict word 0000bh                ; 3d 0b 00
    jnbe short 08b4dh                         ; 77 37
    push CS                                   ; 0e
    pop ES                                    ; 07
    mov cx, strict word 00008h                ; b9 08 00
    mov di, 08a51h                            ; bf 51 8a
    repne scasb                               ; f2 ae
    sal cx, 1                                 ; d1 e1
    mov di, cx                                ; 89 cf
    mov ax, word [cs:di-075a8h]               ; 2e 8b 85 58 8a
    jmp ax                                    ; ff e0
    mov byte [bp-002h], 010h                  ; c6 46 fe 10
    jmp short 08b4dh                          ; eb 1c
    mov byte [bp-002h], 014h                  ; c6 46 fe 14
    jmp short 08b4dh                          ; eb 16
    mov byte [bp-002h], 018h                  ; c6 46 fe 18
    jmp short 08b4dh                          ; eb 10
    mov byte [bp-002h], 01ch                  ; c6 46 fe 1c
    jmp short 08b4dh                          ; eb 0a
    mov byte [bp-002h], 020h                  ; c6 46 fe 20
    jmp short 08b4dh                          ; eb 04
    mov byte [bp-002h], 024h                  ; c6 46 fe 24
    mov CL, strict byte 004h                  ; b1 04
    mov ax, dx                                ; 89 d0
    shr ax, CL                                ; d3 e8
    mov cx, ax                                ; 89 c1
    sal cx, 1                                 ; d1 e1
    sal cx, 1                                 ; d1 e1
    mov al, byte [bp-002h]                    ; 8a 46 fe
    test al, al                               ; 84 c0
    je short 08b7ch                           ; 74 1c
    mov bl, al                                ; 88 c3
    xor bh, bh                                ; 30 ff
    mov dl, byte [bp-006h]                    ; 8a 56 fa
    xor dh, dh                                ; 30 f6
    mov al, byte [bp-004h]                    ; 8a 46 fc
    xor ah, ah                                ; 30 e4
    call 08befh                               ; e8 7e 00
    test AL, strict byte 001h                 ; a8 01
    je short 08b7ch                           ; 74 07
    and AL, strict byte 0f0h                  ; 24 f0
    add ax, cx                                ; 01 c8
    call 0893ch                               ; e8 c0 fd
    mov sp, bp                                ; 89 ec
    pop bp                                    ; 5d
    pop di                                    ; 5f
    retn                                      ; c3
pci_find_classcode_:                         ; 0xf8b81 LB 0x28
    push bx                                   ; 53
    push cx                                   ; 51
    push si                                   ; 56
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov cx, dx                                ; 89 d1
    xor si, si                                ; 31 f6
    mov dx, ax                                ; 89 c2
    mov ax, 0b103h                            ; b8 03 b1
    sal ecx, 010h                             ; 66 c1 e1 10
    db  08bh, 0cah
    ; mov cx, dx                                ; 8b ca
    int 01ah                                  ; cd 1a
    cmp ah, 000h                              ; 80 fc 00
    je near 08ba2h                            ; 0f 84 03 00
    mov bx, strict word 0ffffh                ; bb ff ff
    mov ax, bx                                ; 89 d8
    pop bp                                    ; 5d
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
pci_read_config_byte_:                       ; 0xf8ba9 LB 0x24
    push cx                                   ; 51
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dh, al                                ; 88 c6
    mov bh, dl                                ; 88 d7
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    xor dl, dl                                ; 30 d2
    mov bl, bh                                ; 88 fb
    mov bh, dh                                ; 88 f7
    mov di, ax                                ; 89 c7
    mov ax, 0b108h                            ; b8 08 b1
    int 01ah                                  ; cd 1a
    mov al, cl                                ; 88 c8
    xor ah, ah                                ; 30 e4
    xor dh, dh                                ; 30 f6
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop cx                                    ; 59
    retn                                      ; c3
pci_read_config_word_:                       ; 0xf8bcd LB 0x22
    push cx                                   ; 51
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dh, al                                ; 88 c6
    mov cl, dl                                ; 88 d1
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov bh, dh                                ; 88 f7
    xor dh, dh                                ; 30 f6
    mov bl, dl                                ; 88 d3
    mov di, ax                                ; 89 c7
    mov ax, 0b109h                            ; b8 09 b1
    int 01ah                                  ; cd 1a
    mov ax, cx                                ; 89 c8
    xor dl, dl                                ; 30 d2
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop cx                                    ; 59
    retn                                      ; c3
pci_read_config_dword_:                      ; 0xf8bef LB 0x24
    push cx                                   ; 51
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov dh, al                                ; 88 c6
    mov bh, dl                                ; 88 d7
    mov al, bl                                ; 88 d8
    xor ah, ah                                ; 30 e4
    mov bh, dh                                ; 88 f7
    mov bl, dl                                ; 88 d3
    mov di, ax                                ; 89 c7
    mov ax, 0b10ah                            ; b8 0a b1
    int 01ah                                  ; cd 1a
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    shr ecx, 010h                             ; 66 c1 e9 10
    mov dx, cx                                ; 89 ca
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop cx                                    ; 59
    retn                                      ; c3
vds_is_present_:                             ; 0xf8c13 LB 0x1d
    push bx                                   ; 53
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, strict word 0007bh                ; bb 7b 00
    mov ax, strict word 00040h                ; b8 40 00
    mov es, ax                                ; 8e c0
    test byte [es:bx], 020h                   ; 26 f6 07 20
    je short 08c2bh                           ; 74 06
    mov ax, strict word 00001h                ; b8 01 00
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
    xor ax, ax                                ; 31 c0
    pop bp                                    ; 5d
    pop bx                                    ; 5b
    retn                                      ; c3
vds_real_to_lin_:                            ; 0xf8c30 LB 0x1e
    push bx                                   ; 53
    push cx                                   ; 51
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    mov ax, dx                                ; 89 d0
    xor dx, dx                                ; 31 d2
    mov cx, strict word 00004h                ; b9 04 00
    sal ax, 1                                 ; d1 e0
    rcl dx, 1                                 ; d1 d2
    loop 08c3eh                               ; e2 fa
    xor cx, cx                                ; 31 c9
    add ax, bx                                ; 01 d8
    adc dx, cx                                ; 11 ca
    pop bp                                    ; 5d
    pop cx                                    ; 59
    pop bx                                    ; 5b
    retn                                      ; c3
vds_build_sg_list_:                          ; 0xf8c4e LB 0x74
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov di, ax                                ; 89 c7
    mov si, dx                                ; 89 d6
    mov ax, bx                                ; 89 d8
    mov dx, cx                                ; 89 ca
    mov bx, word [bp+008h]                    ; 8b 5e 08
    mov es, si                                ; 8e c6
    mov word [es:di], bx                      ; 26 89 1d
    mov bx, word [bp+00ah]                    ; 8b 5e 0a
    mov word [es:di+002h], bx                 ; 26 89 5d 02
    call 08c30h                               ; e8 c3 ff
    mov es, si                                ; 8e c6
    mov word [es:di+004h], ax                 ; 26 89 45 04
    mov word [es:di+006h], dx                 ; 26 89 55 06
    mov word [es:di+008h], strict word 00000h ; 26 c7 45 08 00 00
    call 08c13h                               ; e8 93 ff
    test ax, ax                               ; 85 c0
    je short 08c95h                           ; 74 11
    mov es, si                                ; 8e c6
    mov ax, 08105h                            ; b8 05 81
    mov dx, strict word 00000h                ; ba 00 00
    int 04bh                                  ; cd 4b
    jc short 08c92h                           ; 72 02
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    cbw                                       ; 98
    jmp short 08cbch                          ; eb 27
    mov es, si                                ; 8e c6
    mov word [es:di+00eh], strict word 00001h ; 26 c7 45 0e 01 00
    mov dx, word [es:di+004h]                 ; 26 8b 55 04
    mov ax, word [es:di+006h]                 ; 26 8b 45 06
    mov word [es:di+010h], dx                 ; 26 89 55 10
    mov word [es:di+012h], ax                 ; 26 89 45 12
    mov ax, word [bp+008h]                    ; 8b 46 08
    mov word [es:di+014h], ax                 ; 26 89 45 14
    mov ax, bx                                ; 89 d8
    mov word [es:di+016h], bx                 ; 26 89 5d 16
    xor ax, bx                                ; 31 d8
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00004h                               ; c2 04 00
vds_free_sg_list_:                           ; 0xf8cc2 LB 0x2c
    push bx                                   ; 53
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov bx, ax                                ; 89 c3
    call 08c13h                               ; e8 47 ff
    test ax, ax                               ; 85 c0
    je short 08ce1h                           ; 74 11
    mov di, bx                                ; 89 df
    mov es, dx                                ; 8e c2
    mov ax, 08106h                            ; b8 06 81
    mov dx, strict word 00000h                ; ba 00 00
    int 04bh                                  ; cd 4b
    jc short 08ce0h                           ; 72 02
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    cbw                                       ; 98
    mov es, dx                                ; 8e c2
    mov word [es:bx+00eh], strict word 00000h ; 26 c7 47 0e 00 00
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop bx                                    ; 5b
    retn                                      ; c3
    times 0x1 db 0
_small_code_:                                ; 0xf8cee LB 0x49
    db  00bh, 0d2h
    ; or dx, dx                                 ; 0b d2
    js short 08d08h                           ; 78 16
    db  00bh, 0c9h
    ; or cx, cx                                 ; 0b c9
    jns short 08d37h                          ; 79 41
    neg cx                                    ; f7 d9
    neg bx                                    ; f7 db
    sbb cx, strict byte 00000h                ; 83 d9 00
    call 08d37h                               ; e8 37 00
    neg dx                                    ; f7 da
    neg ax                                    ; f7 d8
    sbb dx, strict byte 00000h                ; 83 da 00
    retn                                      ; c3
    neg dx                                    ; f7 da
    neg ax                                    ; f7 d8
    sbb dx, strict byte 00000h                ; 83 da 00
    db  00bh, 0c9h
    ; or cx, cx                                 ; 0b c9
    jns short 08d25h                          ; 79 12
    neg cx                                    ; f7 d9
    neg bx                                    ; f7 db
    sbb cx, strict byte 00000h                ; 83 d9 00
    call 08d37h                               ; e8 1a 00
    neg cx                                    ; f7 d9
    neg bx                                    ; f7 db
    sbb cx, strict byte 00000h                ; 83 d9 00
    retn                                      ; c3
    call 08d37h                               ; e8 0f 00
    neg cx                                    ; f7 d9
    neg bx                                    ; f7 db
    sbb cx, strict byte 00000h                ; 83 d9 00
    neg dx                                    ; f7 da
    neg ax                                    ; f7 d8
    sbb dx, strict byte 00000h                ; 83 da 00
    retn                                      ; c3
__U4D:                                       ; 0xf8d37 LB 0x84
    db  00bh, 0c9h
    ; or cx, cx                                 ; 0b c9
    jne short 08d55h                          ; 75 1a
    dec bx                                    ; 4b
    je short 08d54h                           ; 74 16
    inc bx                                    ; 43
    db  03bh, 0dah
    ; cmp bx, dx                                ; 3b da
    jnbe short 08d4ch                         ; 77 09
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2
    db  02bh, 0d2h
    ; sub dx, dx                                ; 2b d2
    div bx                                    ; f7 f3
    xchg cx, ax                               ; 91
    div bx                                    ; f7 f3
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da
    db  08bh, 0d1h
    ; mov dx, cx                                ; 8b d1
    db  02bh, 0c9h
    ; sub cx, cx                                ; 2b c9
    retn                                      ; c3
    db  03bh, 0cah
    ; cmp cx, dx                                ; 3b ca
    jc short 08d73h                           ; 72 1a
    jne short 08d6bh                          ; 75 10
    db  03bh, 0d8h
    ; cmp bx, ax                                ; 3b d8
    jnbe short 08d6bh                         ; 77 0c
    db  02bh, 0c3h
    ; sub ax, bx                                ; 2b c3
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    db  02bh, 0c9h
    ; sub cx, cx                                ; 2b c9
    db  02bh, 0d2h
    ; sub dx, dx                                ; 2b d2
    mov ax, strict word 00001h                ; b8 01 00
    retn                                      ; c3
    db  02bh, 0c9h
    ; sub cx, cx                                ; 2b c9
    db  02bh, 0dbh
    ; sub bx, bx                                ; 2b db
    xchg bx, ax                               ; 93
    xchg dx, cx                               ; 87 ca
    retn                                      ; c3
    push bp                                   ; 55
    push si                                   ; 56
    db  02bh, 0f6h
    ; sub si, si                                ; 2b f6
    db  08bh, 0eeh
    ; mov bp, si                                ; 8b ee
    db  003h, 0dbh
    ; add bx, bx                                ; 03 db
    db  013h, 0c9h
    ; adc cx, cx                                ; 13 c9
    jc short 08d90h                           ; 72 11
    inc bp                                    ; 45
    db  03bh, 0cah
    ; cmp cx, dx                                ; 3b ca
    jc short 08d79h                           ; 72 f5
    jnbe short 08d8ah                         ; 77 04
    db  03bh, 0d8h
    ; cmp bx, ax                                ; 3b d8
    jbe short 08d79h                          ; 76 ef
    clc                                       ; f8
    db  013h, 0f6h
    ; adc si, si                                ; 13 f6
    dec bp                                    ; 4d
    js short 08db0h                           ; 78 20
    rcr cx, 1                                 ; d1 d9
    rcr bx, 1                                 ; d1 db
    db  02bh, 0c3h
    ; sub ax, bx                                ; 2b c3
    sbb dx, cx                                ; 19 ca
    cmc                                       ; f5
    jc short 08d8bh                           ; 72 f0
    db  003h, 0f6h
    ; add si, si                                ; 03 f6
    dec bp                                    ; 4d
    js short 08dach                           ; 78 0c
    shr cx, 1                                 ; d1 e9
    rcr bx, 1                                 ; d1 db
    db  003h, 0c3h
    ; add ax, bx                                ; 03 c3
    db  013h, 0d1h
    ; adc dx, cx                                ; 13 d1
    jnc short 08d9bh                          ; 73 f1
    jmp short 08d8bh                          ; eb df
    db  003h, 0c3h
    ; add ax, bx                                ; 03 c3
    db  013h, 0d1h
    ; adc dx, cx                                ; 13 d1
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    db  08bh, 0cah
    ; mov cx, dx                                ; 8b ca
    db  08bh, 0c6h
    ; mov ax, si                                ; 8b c6
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2
    pop si                                    ; 5e
    pop bp                                    ; 5d
    retn                                      ; c3
_fmemset_:                                   ; 0xf8dbb LB 0x17
    push di                                   ; 57
    mov di, ax                                ; 89 c7
    mov al, bl                                ; 88 d8
    mov es, dx                                ; 8e c2
    push di                                   ; 57
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    shr cx, 1                                 ; d1 e9
    rep stosw                                 ; f3 ab
    db  013h, 0c9h
    ; adc cx, cx                                ; 13 c9
    rep stosb                                 ; f3 aa
    pop di                                    ; 5f
    mov ax, di                                ; 89 f8
    pop di                                    ; 5f
    retn                                      ; c3
__I4M:                                       ; 0xf8dd2 LB 0x18
    xchg bx, ax                               ; 93
    push ax                                   ; 50
    xchg dx, ax                               ; 92
    db  00bh, 0c0h
    ; or ax, ax                                 ; 0b c0
    je short 08ddbh                           ; 74 02
    mul dx                                    ; f7 e2
    xchg cx, ax                               ; 91
    db  00bh, 0c0h
    ; or ax, ax                                 ; 0b c0
    je short 08de4h                           ; 74 04
    mul bx                                    ; f7 e3
    db  003h, 0c8h
    ; add cx, ax                                ; 03 c8
    pop ax                                    ; 58
    mul bx                                    ; f7 e3
    db  003h, 0d1h
    ; add dx, cx                                ; 03 d1
    retn                                      ; c3
_fmemcpy_:                                   ; 0xf8dea LB 0x25
    push si                                   ; 56
    push di                                   ; 57
    push bp                                   ; 55
    mov bp, sp                                ; 89 e5
    mov di, ax                                ; 89 c7
    mov ax, word [bp+008h]                    ; 8b 46 08
    mov si, bx                                ; 89 de
    mov es, dx                                ; 8e c2
    push DS                                   ; 1e
    push di                                   ; 57
    xchg cx, ax                               ; 91
    mov ds, ax                                ; 8e d8
    shr cx, 1                                 ; d1 e9
    rep movsw                                 ; f3 a5
    db  013h, 0c9h
    ; adc cx, cx                                ; 13 c9
    rep movsb                                 ; f3 a4
    pop di                                    ; 5f
    pop DS                                    ; 1f
    mov ax, di                                ; 89 f8
    pop bp                                    ; 5d
    pop di                                    ; 5f
    pop si                                    ; 5e
    retn 00002h                               ; c2 02 00

  ; Padding 0x4ff1 bytes at 0xf8e0f
  times 20465 db 0

section BIOS32 progbits vstart=0xde00 align=1 ; size=0x1b7 class=CODE group=AUTO
    db  05fh, 033h, 032h, 05fh, 010h, 0deh, 00fh, 000h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h
    db  09ch, 03dh, 024h, 050h, 043h, 049h, 075h, 041h, 0b8h, 000h, 000h, 000h, 080h, 066h, 0bah, 0f8h
    db  00ch, 0efh, 066h, 0bah, 0fch, 00ch, 0edh, 03dh, 086h, 080h, 037h, 012h, 074h, 018h, 0b8h, 000h
    db  0f0h, 000h, 080h, 066h, 0bah, 0f8h, 00ch, 0efh, 066h, 0bah, 0fch, 00ch, 0edh, 03dh, 086h, 080h
    db  04eh, 024h, 074h, 002h, 0ebh, 013h, 0bbh, 000h, 000h, 00fh, 000h, 0b9h, 000h, 000h, 000h, 000h
    db  0bah, 060h, 0deh, 000h, 000h, 032h, 0c0h, 0ebh, 002h, 0b0h, 080h, 09dh, 0cbh, 08dh, 040h, 000h
pcibios_protected:                           ; 0xfde60 LB 0x157
    pushfw                                    ; 9c
    cli                                       ; fa
    push si                                   ; 56
    push di                                   ; 57
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 0de7ch                          ; 75 14
    mov ebx, strict dword 0b9660210h          ; 66 bb 10 02 66 b9
    add byte [bx+si], al                      ; 00 00
    mov dx, 04350h                            ; ba 50 43
    dec cx                                    ; 49
    and byte [bx+si-016ffh], dh               ; 20 b0 01 e9
    pop SS                                    ; 17
    add word [bx+si], ax                      ; 01 00
    add byte [si], bh                         ; 00 3c
    add dh, byte [di+036h]                    ; 02 75 36
    sal cx, 010h                              ; c1 e1 10
    db  066h, 08bh, 0cah
    ; mov ecx, edx                              ; 66 8b ca
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov edi, strict dword 009e80000h          ; 66 bf 00 00 e8 09
    add word [bx+si], ax                      ; 01 00
    add byte [bp-046h], ah                    ; 00 66 ba
    cld                                       ; fc
    or AL, strict byte 0edh                   ; 0c ed
    db  03bh, 0c1h
    ; cmp ax, cx                                ; 3b c1
    jne short 0dea6h                          ; 75 0c
    cmp esi, strict byte 000000000h           ; 66 83 fe 00
    je near 0df91h                            ; 0f 84 ef 00
    add byte [bx+si], al                      ; 00 00
    dec esi                                   ; 66 4e
    inc bx                                    ; 43
    cmp bx, strict word 00000h                ; 81 fb 00 00
    add word [bx+si], ax                      ; 01 00
    jne short 0de8ch                          ; 75 dd
    mov AH, strict byte 086h                  ; b4 86
    jmp near 0df8ch                           ; e9 d8 00
    add byte [bx+si], al                      ; 00 00
    cmp AL, strict byte 003h                  ; 3c 03
    jne short 0deedh                          ; 75 33
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov edi, strict dword 0d5e80008h          ; 66 bf 08 00 e8 d5
    add byte [bx+si], al                      ; 00 00
    add byte [bp-046h], ah                    ; 00 66 ba
    cld                                       ; fc
    or AL, strict byte 0edh                   ; 0c ed
    shr ax, 008h                              ; c1 e8 08
    db  03bh, 0c1h
    ; cmp ax, cx                                ; 3b c1
    jne short 0deddh                          ; 75 0c
    cmp esi, strict byte 000000000h           ; 66 83 fe 00
    je near 0df91h                            ; 0f 84 b8 00
    add byte [bx+si], al                      ; 00 00
    dec esi                                   ; 66 4e
    inc bx                                    ; 43
    cmp bx, strict word 00000h                ; 81 fb 00 00
    add word [bx+si], ax                      ; 01 00
    jne short 0dec0h                          ; 75 da
    mov AH, strict byte 086h                  ; b4 86
    jmp near 0df8ch                           ; e9 a1 00
    add byte [bx+si], al                      ; 00 00
    cmp AL, strict byte 008h                  ; 3c 08
    jne short 0df0ch                          ; 75 1b
    call 0df98h                               ; e8 a4 00
    add byte [bx+si], al                      ; 00 00
    push dx                                   ; 52
    db  066h, 08bh, 0d7h
    ; mov edx, edi                              ; 66 8b d7
    and edx, strict byte 000000003h           ; 66 83 e2 03
    add edx, strict dword 05aec0cfch          ; 66 81 c2 fc 0c ec 5a
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8
    jmp near 0df91h                           ; e9 87 00
    add byte [bx+si], al                      ; 00 00
    cmp AL, strict byte 009h                  ; 3c 09
    jne short 0df2ah                          ; 75 1a
    call 0df98h                               ; e8 85 00
    add byte [bx+si], al                      ; 00 00
    push dx                                   ; 52
    db  066h, 08bh, 0d7h
    ; mov edx, edi                              ; 66 8b d7
    and edx, strict byte 000000002h           ; 66 83 e2 02
    add edx, strict dword 0ed660cfch          ; 66 81 c2 fc 0c 66 ed
    pop dx                                    ; 5a
    db  066h, 08bh, 0c8h
    ; mov ecx, eax                              ; 66 8b c8
    jmp short 0df93h                          ; eb 69
    cmp AL, strict byte 00ah                  ; 3c 0a
    jne short 0df3eh                          ; 75 10
    call 0df98h                               ; e8 67 00
    add byte [bx+si], al                      ; 00 00
    push dx                                   ; 52
    mov edx, strict dword 05aed0cfch          ; 66 ba fc 0c ed 5a
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    jmp short 0df93h                          ; eb 55
    cmp AL, strict byte 00bh                  ; 3c 0b
    jne short 0df5ah                          ; 75 18
    call 0df98h                               ; e8 53 00
    add byte [bx+si], al                      ; 00 00
    push dx                                   ; 52
    db  066h, 08bh, 0d7h
    ; mov edx, edi                              ; 66 8b d7
    and edx, strict byte 000000003h           ; 66 83 e2 03
    add edx, strict dword 0c18a0cfch          ; 66 81 c2 fc 0c 8a c1
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    jmp short 0df93h                          ; eb 39
    cmp AL, strict byte 00ch                  ; 3c 0c
    jne short 0df78h                          ; 75 1a
    call 0df98h                               ; e8 37 00
    add byte [bx+si], al                      ; 00 00
    push dx                                   ; 52
    db  066h, 08bh, 0d7h
    ; mov edx, edi                              ; 66 8b d7
    and edx, strict byte 000000002h           ; 66 83 e2 02
    add edx, strict dword 08b660cfch          ; 66 81 c2 fc 0c 66 8b
    sal word [bp-011h], 05ah                  ; c1 66 ef 5a
    jmp short 0df93h                          ; eb 1b
    cmp AL, strict byte 00dh                  ; 3c 0d
    jne short 0df8ch                          ; 75 10
    call 0df98h                               ; e8 19 00
    add byte [bx+si], al                      ; 00 00
    push dx                                   ; 52
    mov edx, strict dword 0c18b0cfch          ; 66 ba fc 0c 8b c1
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    jmp short 0df93h                          ; eb 07
    mov AH, strict byte 081h                  ; b4 81
    pop di                                    ; 5f
    pop si                                    ; 5e
    popfw                                     ; 9d
    stc                                       ; f9
    retf                                      ; cb
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4
    pop di                                    ; 5f
    pop si                                    ; 5e
    popfw                                     ; 9d
    clc                                       ; f8
    retf                                      ; cb
    push dx                                   ; 52
    mov ax, strict word 00000h                ; b8 00 00
    add byte [bx+si], 066h                    ; 80 00 66
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3
    sal ax, 008h                              ; c1 e0 08
    and edi, strict dword 00b6600ffh          ; 66 81 e7 ff 00 66 0b
    db  0c7h, 024h, 0fch, 066h
    ; mov word [si], 066fch                     ; c7 24 fc 66
    mov dx, 00cf8h                            ; ba f8 0c
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    retn                                      ; c3

  ; Padding 0x49 bytes at 0xfdfb7
  times 73 db 0

section BIOSSEG progbits vstart=0xe000 align=1 ; size=0x2000 class=CODE group=AUTO
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 058h, 04dh
eoi_jmp_post:                                ; 0xfe030 LB 0xb
    call 0e03bh                               ; e8 08 00
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    jmp far [00467h]                          ; ff 2e 67 04
eoi_both_pics:                               ; 0xfe03b LB 0x4
    mov AL, strict byte 020h                  ; b0 20
    out strict byte 0a0h, AL                  ; e6 a0
eoi_master_pic:                              ; 0xfe03f LB 0x1c
    mov AL, strict byte 020h                  ; b0 20
    out strict byte 020h, AL                  ; e6 20
    retn                                      ; c3
    times 0x15 db 0
    db  'XM'
post:                                        ; 0xfe05b LB 0x30
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    out strict byte 00dh, AL                  ; e6 0d
    out strict byte 0dah, AL                  ; e6 da
    mov AL, strict byte 0c0h                  ; b0 c0
    out strict byte 0d6h, AL                  ; e6 d6
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 0d4h, AL                  ; e6 d4
    mov AL, strict byte 00fh                  ; b0 0f
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8
    mov AL, strict byte 00fh                  ; b0 0f
    out strict byte 070h, AL                  ; e6 70
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 071h, AL                  ; e6 71
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3
    cmp AL, strict byte 000h                  ; 3c 00
    je short 0e08bh                           ; 74 0c
    cmp AL, strict byte 00dh                  ; 3c 0d
    jnc short 0e08bh                          ; 73 08
    cmp AL, strict byte 009h                  ; 3c 09
    je short 0e08bh                           ; 74 04
    cmp AL, strict byte 005h                  ; 3c 05
    je short 0e030h                           ; 74 a5
normal_post:                                 ; 0xfe08b LB 0x238
    cli                                       ; fa
    mov ax, 07800h                            ; b8 00 78
    db  08bh, 0e0h
    ; mov sp, ax                                ; 8b e0
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov ss, ax                                ; 8e d0
    mov es, ax                                ; 8e c0
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    cld                                       ; fc
    mov cx, 00239h                            ; b9 39 02
    rep stosw                                 ; f3 ab
    inc di                                    ; 47
    inc di                                    ; 47
    mov cx, 005c6h                            ; b9 c6 05
    rep stosw                                 ; f3 ab
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    add bx, 01000h                            ; 81 c3 00 10
    cmp bx, 09000h                            ; 81 fb 00 90
    jnc short 0e0bfh                          ; 73 0b
    mov es, bx                                ; 8e c3
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    mov cx, 08000h                            ; b9 00 80
    rep stosw                                 ; f3 ab
    jmp short 0e0aah                          ; eb eb
    mov es, bx                                ; 8e c3
    db  033h, 0ffh
    ; xor di, di                                ; 33 ff
    mov cx, 07e00h                            ; b9 00 7e
    rep stosw                                 ; f3 ab
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 01709h                               ; e8 39 36
    call 0e8c8h                               ; e8 f5 07
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov ds, bx                                ; 8e db
    mov cx, strict word 00078h                ; b9 78 00
    mov ax, 0ff53h                            ; b8 53 ff
    mov dx, 0f000h                            ; ba 00 f0
    mov word [bx], ax                         ; 89 07
    mov word [bx+002h], dx                    ; 89 57 02
    add bx, strict byte 00004h                ; 83 c3 04
    loop 0e0e0h                               ; e2 f6
    mov ax, 0027fh                            ; b8 7f 02
    mov word [00413h], ax                     ; a3 13 04
    mov ax, 0f84dh                            ; b8 4d f8
    mov word [00044h], ax                     ; a3 44 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00046h], ax                     ; a3 46 00
    mov ax, 0f841h                            ; b8 41 f8
    mov word [00048h], ax                     ; a3 48 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0004ah], ax                     ; a3 4a 00
    mov ax, 0f859h                            ; b8 59 f8
    mov word [00054h], ax                     ; a3 54 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00056h], ax                     ; a3 56 00
    mov ax, 0efd2h                            ; b8 d2 ef
    mov word [0005ch], ax                     ; a3 5c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0005eh], ax                     ; a3 5e 00
    mov ax, 0f0a4h                            ; b8 a4 f0
    mov word [00060h], ax                     ; a3 60 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00062h], ax                     ; a3 62 00
    mov ax, 0e6f2h                            ; b8 f2 e6
    mov word [00064h], ax                     ; a3 64 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00066h], ax                     ; a3 66 00
    mov ax, 0efebh                            ; b8 eb ef
    mov word [00070h], ax                     ; a3 70 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00072h], ax                     ; a3 72 00
    call 0e7c0h                               ; e8 79 06
    mov ax, 0fea5h                            ; b8 a5 fe
    mov word [00020h], ax                     ; a3 20 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00022h], ax                     ; a3 22 00
    mov AL, strict byte 034h                  ; b0 34
    out strict byte 043h, AL                  ; e6 43
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 040h, AL                  ; e6 40
    out strict byte 040h, AL                  ; e6 40
    mov ax, 0e987h                            ; b8 87 e9
    mov word [00024h], ax                     ; a3 24 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00026h], ax                     ; a3 26 00
    mov ax, 0e82eh                            ; b8 2e e8
    mov word [00058h], ax                     ; a3 58 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0005ah], ax                     ; a3 5a 00
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov byte [00417h], AL                     ; a2 17 04
    mov byte [00418h], AL                     ; a2 18 04
    mov byte [00419h], AL                     ; a2 19 04
    mov byte [00471h], AL                     ; a2 71 04
    mov byte [00497h], AL                     ; a2 97 04
    mov AL, strict byte 010h                  ; b0 10
    mov byte [00496h], AL                     ; a2 96 04
    mov bx, strict word 0001eh                ; bb 1e 00
    mov word [0041ah], bx                     ; 89 1e 1a 04
    mov word [0041ch], bx                     ; 89 1e 1c 04
    mov word [00480h], bx                     ; 89 1e 80 04
    mov bx, strict word 0003eh                ; bb 3e 00
    mov word [00482h], bx                     ; 89 1e 82 04
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 04b28h                               ; e8 7e 69
    pop DS                                    ; 1f
    mov AL, strict byte 014h                  ; b0 14
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    mov byte [00410h], AL                     ; a2 10 04
    mov ax, 0ff53h                            ; b8 53 ff
    mov word [0003ch], ax                     ; a3 3c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0003eh], ax                     ; a3 3e 00
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov CL, strict byte 014h                  ; b1 14
    mov dx, 00378h                            ; ba 78 03
    call 0ecedh                               ; e8 1f 0b
    mov dx, 00278h                            ; ba 78 02
    call 0ecedh                               ; e8 19 0b
    sal bx, 00eh                              ; c1 e3 0e
    mov ax, word [00410h]                     ; a1 10 04
    and ax, 03fffh                            ; 25 ff 3f
    db  00bh, 0c3h
    ; or ax, bx                                 ; 0b c3
    mov word [00410h], ax                     ; a3 10 04
    mov ax, 0e746h                            ; b8 46 e7
    mov word [0002ch], ax                     ; a3 2c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0002eh], ax                     ; a3 2e 00
    mov ax, 0e746h                            ; b8 46 e7
    mov word [00030h], ax                     ; a3 30 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00032h], ax                     ; a3 32 00
    mov ax, 0e739h                            ; b8 39 e7
    mov word [00050h], ax                     ; a3 50 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00052h], ax                     ; a3 52 00
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov CL, strict byte 00ah                  ; b1 0a
    mov dx, 003f8h                            ; ba f8 03
    call 0ed0bh                               ; e8 fb 0a
    mov dx, 002f8h                            ; ba f8 02
    call 0ed0bh                               ; e8 f5 0a
    mov dx, 003e8h                            ; ba e8 03
    call 0ed0bh                               ; e8 ef 0a
    mov dx, 002e8h                            ; ba e8 02
    call 0ed0bh                               ; e8 e9 0a
    sal bx, 009h                              ; c1 e3 09
    mov ax, word [00410h]                     ; a1 10 04
    and ax, 0f1ffh                            ; 25 ff f1
    db  00bh, 0c3h
    ; or ax, bx                                 ; 0b c3
    mov word [00410h], ax                     ; a3 10 04
    mov ax, 0fe6eh                            ; b8 6e fe
    mov word [00068h], ax                     ; a3 68 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0006ah], ax                     ; a3 6a 00
    mov ax, 0ff53h                            ; b8 53 ff
    mov word [00128h], ax                     ; a3 28 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0012ah], ax                     ; a3 2a 01
    mov ax, 0fe8eh                            ; b8 8e fe
    mov word [001c0h], ax                     ; a3 c0 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001c2h], ax                     ; a3 c2 01
    call 0edbfh                               ; e8 68 0b
    mov ax, 0f8a3h                            ; b8 a3 f8
    mov word [001d0h], ax                     ; a3 d0 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001d2h], ax                     ; a3 d2 01
    mov ax, 0e2cah                            ; b8 ca e2
    mov word [001d4h], ax                     ; a3 d4 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001d6h], ax                     ; a3 d6 01
    mov ax, 0f065h                            ; b8 65 f0
    mov word [00040h], ax                     ; a3 40 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00042h], ax                     ; a3 42 00
    call 0e79bh                               ; e8 1d 05
    call 0f2feh                               ; e8 7d 10
    call 0f383h                               ; e8 ff 10
    call 0e758h                               ; e8 d1 04
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 01b49h                               ; e8 bc 38
    call 01f79h                               ; e8 e9 3c
    call 07b5eh                               ; e8 cb 98
    call 08a68h                               ; e8 d2 a7
    call 0ed2fh                               ; e8 96 0a
    call 0e2d2h                               ; e8 36 00
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 0172bh                               ; e8 89 34
    call 036f9h                               ; e8 54 54
    sti                                       ; fb
    int 019h                                  ; cd 19
    sti                                       ; fb
    hlt                                       ; f4
    jmp short 0e2a9h                          ; eb fd
    cli                                       ; fa
    hlt                                       ; f4
    times 0x13 db 0
    db  'XM'
nmi:                                         ; 0xfe2c3 LB 0x7
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 016e3h                               ; e8 1a 34
    iret                                      ; cf
int75_handler:                               ; 0xfe2ca LB 0x8
    out strict byte 0f0h, AL                  ; e6 f0
    call 0e03bh                               ; e8 6c fd
    int 002h                                  ; cd 02
    iret                                      ; cf
hard_drive_post:                             ; 0xfe2d2 LB 0x12c
    mov AL, strict byte 00ah                  ; b0 0a
    mov dx, 003f6h                            ; ba f6 03
    out DX, AL                                ; ee
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov byte [00474h], AL                     ; a2 74 04
    mov byte [00477h], AL                     ; a2 77 04
    mov byte [0048ch], AL                     ; a2 8c 04
    mov byte [0048dh], AL                     ; a2 8d 04
    mov byte [0048eh], AL                     ; a2 8e 04
    mov AL, strict byte 0c0h                  ; b0 c0
    mov byte [00476h], AL                     ; a2 76 04
    mov ax, 0e3feh                            ; b8 fe e3
    mov word [0004ch], ax                     ; a3 4c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0004eh], ax                     ; a3 4e 00
    mov ax, 0f8d1h                            ; b8 d1 f8
    mov word [001d8h], ax                     ; a3 d8 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001dah], ax                     ; a3 da 01
    mov ax, strict word 0003dh                ; b8 3d 00
    mov word [00104h], ax                     ; a3 04 01
    mov ax, 09fc0h                            ; b8 c0 9f
    mov word [00106h], ax                     ; a3 06 01
    mov ax, strict word 0004dh                ; b8 4d 00
    mov word [00118h], ax                     ; a3 18 01
    mov ax, 09fc0h                            ; b8 c0 9f
    mov word [0011ah], ax                     ; a3 1a 01
    retn                                      ; c3
    times 0xdb db 0
    db  'XM'
int13_handler:                               ; 0xfe3fe LB 0x3
    jmp near 0ec5bh                           ; e9 5a 08
rom_fdpt:                                    ; 0xfe401 LB 0x2f1
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 058h
    db  04dh
int19_handler:                               ; 0xfe6f2 LB 0x61
    jmp near 0f0ach                           ; e9 b7 09
    or word [bx+si], ax                       ; 09 00
    cld                                       ; fc
    add byte [bx+di], al                      ; 00 01
    je short 0e73ch                           ; 74 40
    times 0x2b db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    times 0xe db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    push DS                                   ; 1e
    push ES                                   ; 06
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 05eeah                               ; e8 a8 77
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    iret                                      ; cf
    push DS                                   ; 1e
    push ES                                   ; 06
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 0167eh                               ; e8 2f 2f
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    iret                                      ; cf
rom_checksum:                                ; 0xfe753 LB 0x5
    push ax                                   ; 50
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    pop ax                                    ; 58
    retn                                      ; c3
rom_scan:                                    ; 0xfe758 LB 0x43
    mov cx, 0c000h                            ; b9 00 c0
    mov ds, cx                                ; 8e d9
    mov ax, strict word 00004h                ; b8 04 00
    cmp word [word 00000h], 0aa55h            ; 81 3e 00 00 55 aa
    jne short 0e78bh                          ; 75 23
    call 0e753h                               ; e8 e8 ff
    jne short 0e78bh                          ; 75 1e
    mov AL, byte [00002h]                     ; a0 02 00
    test AL, strict byte 003h                 ; a8 03
    je short 0e778h                           ; 74 04
    and AL, strict byte 0fch                  ; 24 fc
    add AL, strict byte 004h                  ; 04 04
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db
    mov ds, bx                                ; 8e db
    push ax                                   ; 50
    push cx                                   ; 51
    push strict byte 00003h                   ; 6a 03
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    call far [byte bp+000h]                   ; ff 5e 00
    cli                                       ; fa
    add sp, strict byte 00002h                ; 83 c4 02
    pop cx                                    ; 59
    pop ax                                    ; 58
    sal ax, 005h                              ; c1 e0 05
    db  003h, 0c8h
    ; add cx, ax                                ; 03 c8
    cmp cx, 0e800h                            ; 81 f9 00 e8
    jbe short 0e75bh                          ; 76 c5
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    retn                                      ; c3
init_pic:                                    ; 0xfe79b LB 0x25
    mov AL, strict byte 011h                  ; b0 11
    out strict byte 020h, AL                  ; e6 20
    out strict byte 0a0h, AL                  ; e6 a0
    mov AL, strict byte 008h                  ; b0 08
    out strict byte 021h, AL                  ; e6 21
    mov AL, strict byte 070h                  ; b0 70
    out strict byte 0a1h, AL                  ; e6 a1
    mov AL, strict byte 004h                  ; b0 04
    out strict byte 021h, AL                  ; e6 21
    mov AL, strict byte 002h                  ; b0 02
    out strict byte 0a1h, AL                  ; e6 a1
    mov AL, strict byte 001h                  ; b0 01
    out strict byte 021h, AL                  ; e6 21
    out strict byte 0a1h, AL                  ; e6 a1
    mov AL, strict byte 0b8h                  ; b0 b8
    out strict byte 021h, AL                  ; e6 21
    mov AL, strict byte 08fh                  ; b0 8f
    out strict byte 0a1h, AL                  ; e6 a1
    retn                                      ; c3
ebda_post:                                   ; 0xfe7c0 LB 0xa4
    mov ax, 0e746h                            ; b8 46 e7
    mov word [00034h], ax                     ; a3 34 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00036h], ax                     ; a3 36 00
    mov ax, 0e746h                            ; b8 46 e7
    mov word [0003ch], ax                     ; a3 3c 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0003eh], ax                     ; a3 3e 00
    mov ax, 0e746h                            ; b8 46 e7
    mov word [001c8h], ax                     ; a3 c8 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001cah], ax                     ; a3 ca 01
    mov ax, 0e746h                            ; b8 46 e7
    mov word [001dch], ax                     ; a3 dc 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [001deh], ax                     ; a3 de 01
    mov ax, 09fc0h                            ; b8 c0 9f
    mov ds, ax                                ; 8e d8
    mov byte [word 00000h], 001h              ; c6 06 00 00 01
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov word [0040eh], 09fc0h                 ; c7 06 0e 04 c0 9f
    retn                                      ; c3
    times 0x27 db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    sti                                       ; fb
    push ES                                   ; 06
    push DS                                   ; 1e
    pushaw                                    ; 60
    cmp ah, 000h                              ; 80 fc 00
    je short 0e846h                           ; 74 0f
    cmp ah, 010h                              ; 80 fc 10
    je short 0e846h                           ; 74 0a
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 0526eh                               ; e8 2c 6a
    popaw                                     ; 61
    pop DS                                    ; 1f
    pop ES                                    ; 07
    iret                                      ; cf
    mov bx, strict word 00040h                ; bb 40 00
    mov ds, bx                                ; 8e db
    cli                                       ; fa
    mov bx, word [word 0001ah]                ; 8b 1e 1a 00
    cmp bx, word [word 0001ch]                ; 3b 1e 1c 00
    jne short 0e85ah                          ; 75 04
    sti                                       ; fb
    nop                                       ; 90
    jmp short 0e84bh                          ; eb f1
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 0526eh                               ; e8 0e 6a
    popaw                                     ; 61
    pop DS                                    ; 1f
    pop ES                                    ; 07
    iret                                      ; cf
pmode_enter:                                 ; 0xfe864 LB 0x1b
    push CS                                   ; 0e
    pop DS                                    ; 1f
    lgdt [cs:0e892h]                          ; 2e 0f 01 16 92 e8
    mov eax, cr0                              ; 0f 20 c0
    or AL, strict byte 001h                   ; 0c 01
    mov cr0, eax                              ; 0f 22 c0
    jmp far 00020h:0e879h                     ; ea 79 e8 20 00
    mov ax, strict word 00018h                ; b8 18 00
    mov ds, ax                                ; 8e d8
    retn                                      ; c3
pmode_exit:                                  ; 0xfe87f LB 0x13
    mov ax, strict word 00028h                ; b8 28 00
    mov ds, ax                                ; 8e d8
    mov eax, cr0                              ; 0f 20 c0
    and AL, strict byte 0feh                  ; 24 fe
    mov cr0, eax                              ; 0f 22 c0
    jmp far 0f000h:0e891h                     ; ea 91 e8 00 f0
    retn                                      ; c3
pmbios_gdt_desc:                             ; 0xfe892 LB 0x6
    db  030h, 000h, 098h, 0e8h, 00fh, 000h
pmbios_gdt:                                  ; 0xfe898 LB 0x30
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 000h, 000h, 000h, 09bh, 0cfh, 000h, 0ffh, 0ffh, 000h, 000h, 000h, 093h, 0cfh, 000h
    db  0ffh, 0ffh, 000h, 000h, 00fh, 09bh, 000h, 000h, 0ffh, 0ffh, 000h, 000h, 000h, 093h, 000h, 000h
pmode_setup:                                 ; 0xfe8c8 LB 0x393
    push eax                                  ; 66 50
    push esi                                  ; 66 56
    pushfw                                    ; 9c
    cli                                       ; fa
    call 0e864h                               ; e8 93 ff
    mov eax, cr0                              ; 0f 20 c0
    and eax, strict dword 09fffffffh          ; 66 25 ff ff ff 9f
    mov cr0, eax                              ; 0f 22 c0
    mov esi, strict dword 0fee00350h          ; 66 be 50 03 e0 fe
    mov eax, dword [esi]                      ; 67 66 8b 06
    and eax, strict dword 0fffe00ffh          ; 66 25 ff 00 fe ff
    or ah, 007h                               ; 80 cc 07
    mov dword [esi], eax                      ; 67 66 89 06
    mov esi, strict dword 0fee00360h          ; 66 be 60 03 e0 fe
    mov eax, dword [esi]                      ; 67 66 8b 06
    and eax, strict dword 0fffe00ffh          ; 66 25 ff 00 fe ff
    or ah, 004h                               ; 80 cc 04
    mov dword [esi], eax                      ; 67 66 89 06
    call 0e87fh                               ; e8 71 ff
    popfw                                     ; 9d
    pop esi                                   ; 66 5e
    pop eax                                   ; 66 58
    retn                                      ; c3
    times 0x71 db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    cli                                       ; fa
    push ax                                   ; 50
    mov AL, strict byte 0adh                  ; b0 ad
    out strict byte 064h, AL                  ; e6 64
    mov AL, strict byte 00bh                  ; b0 0b
    out strict byte 020h, AL                  ; e6 20
    in AL, strict byte 020h                   ; e4 20
    and AL, strict byte 002h                  ; 24 02
    je short 0e9d6h                           ; 74 3f
    in AL, strict byte 060h                   ; e4 60
    push DS                                   ; 1e
    pushaw                                    ; 60
    cld                                       ; fc
    mov AH, strict byte 04fh                  ; b4 4f
    stc                                       ; f9
    int 015h                                  ; cd 15
    jnc short 0e9d0h                          ; 73 2d
    sti                                       ; fb
    cmp AL, strict byte 0e0h                  ; 3c e0
    jne short 0e9b6h                          ; 75 0e
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov AL, byte [00496h]                     ; a0 96 04
    or AL, strict byte 002h                   ; 0c 02
    mov byte [00496h], AL                     ; a2 96 04
    jmp short 0e9d0h                          ; eb 1a
    cmp AL, strict byte 0e1h                  ; 3c e1
    jne short 0e9c8h                          ; 75 0e
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov AL, byte [00496h]                     ; a0 96 04
    or AL, strict byte 001h                   ; 0c 01
    mov byte [00496h], AL                     ; a2 96 04
    jmp short 0e9d0h                          ; eb 08
    push ES                                   ; 06
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 04e39h                               ; e8 6a 64
    pop ES                                    ; 07
    popaw                                     ; 61
    pop DS                                    ; 1f
    cli                                       ; fa
    call 0e03fh                               ; e8 69 f6
    mov AL, strict byte 0aeh                  ; b0 ae
    out strict byte 064h, AL                  ; e6 64
    pop ax                                    ; 58
    iret                                      ; cf
    times 0x27b db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    jmp short 0ecb0h                          ; eb 55
int13_relocated:                             ; 0xfec5b LB 0x55
    cmp ah, 04ah                              ; 80 fc 4a
    jc short 0ec71h                           ; 72 11
    cmp ah, 04dh                              ; 80 fc 4d
    jnbe short 0ec71h                         ; 77 0c
    pushaw                                    ; 60
    push ES                                   ; 06
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    push 0ece9h                               ; 68 e9 ec
    jmp near 03737h                           ; e9 c6 4a
    push ES                                   ; 06
    push ax                                   ; 50
    push bx                                   ; 53
    push cx                                   ; 51
    push dx                                   ; 52
    call 0370fh                               ; e8 96 4a
    cmp AL, strict byte 000h                  ; 3c 00
    je short 0ecabh                           ; 74 2e
    call 03723h                               ; e8 a3 4a
    pop dx                                    ; 5a
    push dx                                   ; 52
    db  03ah, 0c2h
    ; cmp al, dl                                ; 3a c2
    jne short 0ec97h                          ; 75 11
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    pop ES                                    ; 07
    pushaw                                    ; 60
    push ES                                   ; 06
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    push 0ece9h                               ; 68 e9 ec
    jmp near 03d62h                           ; e9 cb 50
    and dl, 0e0h                              ; 80 e2 e0
    db  03ah, 0c2h
    ; cmp al, dl                                ; 3a c2
    jne short 0ecabh                          ; 75 0d
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    pop ES                                    ; 07
    push ax                                   ; 50
    push cx                                   ; 51
    push dx                                   ; 52
    push bx                                   ; 53
    db  0feh, 0cah
    ; dec dl                                    ; fe ca
    jmp short 0ecb4h                          ; eb 09
    pop dx                                    ; 5a
    pop cx                                    ; 59
    pop bx                                    ; 5b
    pop ax                                    ; 58
    pop ES                                    ; 07
int13_noeltorito:                            ; 0xfecb0 LB 0x4
    push ax                                   ; 50
    push cx                                   ; 51
    push dx                                   ; 52
    push bx                                   ; 53
int13_legacy:                                ; 0xfecb4 LB 0x14
    push dx                                   ; 52
    push bp                                   ; 55
    push si                                   ; 56
    push di                                   ; 57
    push ES                                   ; 06
    push DS                                   ; 1e
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    test dl, 080h                             ; f6 c2 80
    jne short 0ecc8h                          ; 75 06
    push 0ece9h                               ; 68 e9 ec
    jmp near 02efch                           ; e9 34 42
int13_notfloppy:                             ; 0xfecc8 LB 0x14
    cmp dl, 0e0h                              ; 80 fa e0
    jc short 0ecdch                           ; 72 0f
    shr ebx, 010h                             ; 66 c1 eb 10
    push bx                                   ; 53
    call 04182h                               ; e8 ad 54
    pop bx                                    ; 5b
    sal ebx, 010h                             ; 66 c1 e3 10
    jmp short 0ece9h                          ; eb 0d
int13_disk:                                  ; 0xfecdc LB 0xd
    cmp ah, 040h                              ; 80 fc 40
    jnbe short 0ece6h                         ; 77 05
    call 05595h                               ; e8 b1 68
    jmp short 0ece9h                          ; eb 03
    call 059cfh                               ; e8 e6 6c
int13_out:                                   ; 0xfece9 LB 0x4
    pop DS                                    ; 1f
    pop ES                                    ; 07
    popaw                                     ; 61
    iret                                      ; cf
detect_parport:                              ; 0xfeced LB 0x1e
    push dx                                   ; 52
    inc dx                                    ; 42
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    and AL, strict byte 0dfh                  ; 24 df
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    mov AL, strict byte 0aah                  ; b0 aa
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    cmp AL, strict byte 0aah                  ; 3c aa
    jne short 0ed0ah                          ; 75 0d
    push bx                                   ; 53
    sal bx, 1                                 ; d1 e3
    mov word [bx+00408h], dx                  ; 89 97 08 04
    pop bx                                    ; 5b
    mov byte [bx+00478h], cl                  ; 88 8f 78 04
    inc bx                                    ; 43
    retn                                      ; c3
detect_serial:                               ; 0xfed0b LB 0x24
    push dx                                   ; 52
    inc dx                                    ; 42
    mov AL, strict byte 002h                  ; b0 02
    out DX, AL                                ; ee
    in AL, DX                                 ; ec
    cmp AL, strict byte 002h                  ; 3c 02
    jne short 0ed2dh                          ; 75 18
    inc dx                                    ; 42
    in AL, DX                                 ; ec
    cmp AL, strict byte 002h                  ; 3c 02
    jne short 0ed2dh                          ; 75 12
    dec dx                                    ; 4a
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    pop dx                                    ; 5a
    push bx                                   ; 53
    sal bx, 1                                 ; d1 e3
    mov word [bx+00400h], dx                  ; 89 97 00 04
    pop bx                                    ; 5b
    mov byte [bx+0047ch], cl                  ; 88 8f 7c 04
    inc bx                                    ; 43
    retn                                      ; c3
    pop dx                                    ; 5a
    retn                                      ; c3
floppy_post:                                 ; 0xfed2f LB 0x87
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov AL, strict byte 000h                  ; b0 00
    mov byte [0043eh], AL                     ; a2 3e 04
    mov byte [0043fh], AL                     ; a2 3f 04
    mov byte [00440h], AL                     ; a2 40 04
    mov byte [00441h], AL                     ; a2 41 04
    mov byte [00442h], AL                     ; a2 42 04
    mov byte [00443h], AL                     ; a2 43 04
    mov byte [00444h], AL                     ; a2 44 04
    mov byte [00445h], AL                     ; a2 45 04
    mov byte [00446h], AL                     ; a2 46 04
    mov byte [00447h], AL                     ; a2 47 04
    mov byte [00448h], AL                     ; a2 48 04
    mov byte [0048bh], AL                     ; a2 8b 04
    mov AL, strict byte 010h                  ; b0 10
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0
    shr al, 004h                              ; c0 e8 04
    je short 0ed6ah                           ; 74 04
    mov BL, strict byte 007h                  ; b3 07
    jmp short 0ed6ch                          ; eb 02
    mov BL, strict byte 000h                  ; b3 00
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4
    and AL, strict byte 00fh                  ; 24 0f
    je short 0ed75h                           ; 74 03
    or bl, 070h                               ; 80 cb 70
    mov byte [0048fh], bl                     ; 88 1e 8f 04
    mov AL, strict byte 000h                  ; b0 00
    mov byte [00490h], AL                     ; a2 90 04
    mov byte [00491h], AL                     ; a2 91 04
    mov byte [00492h], AL                     ; a2 92 04
    mov byte [00493h], AL                     ; a2 93 04
    mov byte [00494h], AL                     ; a2 94 04
    mov byte [00495h], AL                     ; a2 95 04
    mov AL, strict byte 002h                  ; b0 02
    out strict byte 00ah, AL                  ; e6 0a
    mov ax, 0efc7h                            ; b8 c7 ef
    mov word [00078h], ax                     ; a3 78 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0007ah], ax                     ; a3 7a 00
    mov ax, 0ec59h                            ; b8 59 ec
    mov word [00100h], ax                     ; a3 00 01
    mov ax, 0f000h                            ; b8 00 f0
    mov word [00102h], ax                     ; a3 02 01
    mov ax, 0ef57h                            ; b8 57 ef
    mov word [00038h], ax                     ; a3 38 00
    mov ax, 0f000h                            ; b8 00 f0
    mov word [0003ah], ax                     ; a3 3a 00
    retn                                      ; c3
bcd_to_bin:                                  ; 0xfedb6 LB 0x9
    sal ax, 004h                              ; c1 e0 04
    shr al, 004h                              ; c0 e8 04
    aad 00ah                                  ; d5 0a
    retn                                      ; c3
rtc_post:                                    ; 0xfedbf LB 0x198
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov AL, strict byte 000h                  ; b0 00
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    call 0edb6h                               ; e8 eb ff
    mov edx, strict dword 00115cf2bh          ; 66 ba 2b cf 15 01
    mul edx                                   ; 66 f7 e2
    mov ebx, strict dword 0000f4240h          ; 66 bb 40 42 0f 00
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    div ebx                                   ; 66 f7 f3
    db  066h, 08bh, 0c8h
    ; mov ecx, eax                              ; 66 8b c8
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov AL, strict byte 002h                  ; b0 02
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    call 0edb6h                               ; e8 c7 ff
    mov edx, strict dword 000a6af80h          ; 66 ba 80 af a6 00
    mul edx                                   ; 66 f7 e2
    mov ebx, strict dword 000002710h          ; 66 bb 10 27 00 00
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    div ebx                                   ; 66 f7 f3
    db  066h, 003h, 0c8h
    ; add ecx, eax                              ; 66 03 c8
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    mov AL, strict byte 004h                  ; b0 04
    out strict byte 070h, AL                  ; e6 70
    in AL, strict byte 071h                   ; e4 71
    call 0edb6h                               ; e8 a3 ff
    mov edx, strict dword 003e81d03h          ; 66 ba 03 1d e8 03
    mul edx                                   ; 66 f7 e2
    mov ebx, strict dword 0000003e8h          ; 66 bb e8 03 00 00
    db  066h, 033h, 0d2h
    ; xor edx, edx                              ; 66 33 d2
    div ebx                                   ; 66 f7 f3
    db  066h, 003h, 0c8h
    ; add ecx, eax                              ; 66 03 c8
    mov dword [0046ch], ecx                   ; 66 89 0e 6c 04
    db  032h, 0c0h
    ; xor al, al                                ; 32 c0
    mov byte [00470h], AL                     ; a2 70 04
    retn                                      ; c3
    times 0x11f db 0
    db  'XM'
int0e_handler:                               ; 0xfef57 LB 0x70
    push ax                                   ; 50
    push dx                                   ; 52
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 0ef81h                           ; 74 1e
    mov dx, 003f5h                            ; ba f5 03
    mov AL, strict byte 008h                  ; b0 08
    out DX, AL                                ; ee
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    jne short 0ef69h                          ; 75 f6
    mov dx, 003f5h                            ; ba f5 03
    in AL, DX                                 ; ec
    mov dx, 003f4h                            ; ba f4 03
    in AL, DX                                 ; ec
    and AL, strict byte 0c0h                  ; 24 c0
    cmp AL, strict byte 0c0h                  ; 3c c0
    je short 0ef73h                           ; 74 f2
    push DS                                   ; 1e
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    call 0e03fh                               ; e8 b6 f0
    or byte [0043eh], 080h                    ; 80 0e 3e 04 80
    pop DS                                    ; 1f
    pop dx                                    ; 5a
    pop ax                                    ; 58
    iret                                      ; cf
    times 0x33 db 0
    db  'XM'
_diskette_param_table:                       ; 0xfefc7 LB 0xb
    scasw                                     ; af
    add ah, byte [di]                         ; 02 25
    add dl, byte [bp+si]                      ; 02 12
    db  01bh, 0ffh
    ; sbb di, di                                ; 1b ff
    insb                                      ; 6c
    db  0f6h, 00fh
    ;                                           ; f6 0f
    db  008h
int17_handler:                               ; 0xfefd2 LB 0xd
    push DS                                   ; 1e
    push ES                                   ; 06
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 070e5h                               ; e8 0a 81
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    iret                                      ; cf
_pmode_IDT:                                  ; 0xfefdf LB 0x6
    db  000h, 000h, 000h, 000h, 00fh, 000h
_rmode_IDT:                                  ; 0xfefe5 LB 0x6
    db  0ffh, 003h, 000h, 000h, 000h, 000h
int1c_handler:                               ; 0xfefeb LB 0x7a
    iret                                      ; cf
    times 0x57 db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    iret                                      ; cf
    times 0x1d db 0
    db  'XM'
int10_handler:                               ; 0xff065 LB 0x47
    iret                                      ; cf
    times 0x3c db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 016f6h                               ; e8 4c 26
    hlt                                       ; f4
    iret                                      ; cf
int19_relocated:                             ; 0xff0ac LB 0x71
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    mov ax, word [bp+002h]                    ; 8b 46 02
    cmp ax, 0f000h                            ; 3d 00 f0
    je short 0f0c3h                           ; 74 0d
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov ax, 01234h                            ; b8 34 12
    mov word [001d8h], ax                     ; a3 d8 01
    jmp near 0e05bh                           ; e9 98 ef
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    mov ax, strict word 00001h                ; b8 01 00
    push ax                                   ; 50
    call 0487eh                               ; e8 ae 57
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 28
    mov ax, strict word 00002h                ; b8 02 00
    push ax                                   ; 50
    call 0487eh                               ; e8 a1 57
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 1b
    mov ax, strict word 00003h                ; b8 03 00
    push strict byte 00003h                   ; 6a 03
    call 0487eh                               ; e8 93 57
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    jne short 0f0feh                          ; 75 0d
    mov ax, strict word 00004h                ; b8 04 00
    push ax                                   ; 50
    call 0487eh                               ; e8 86 57
    inc sp                                    ; 44
    inc sp                                    ; 44
    test ax, ax                               ; 85 c0
    je short 0f0a4h                           ; 74 a6
    sal eax, 004h                             ; 66 c1 e0 04
    mov word [bp+002h], ax                    ; 89 46 02
    shr eax, 004h                             ; 66 c1 e8 04
    and ax, 0f000h                            ; 25 00 f0
    mov word [bp+004h], ax                    ; 89 46 04
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov es, ax                                ; 8e c0
    mov word [byte bp+000h], ax               ; 89 46 00
    mov ax, 0aa55h                            ; b8 55 aa
    pop bp                                    ; 5d
    iret                                      ; cf
pcibios_real:                                ; 0xff11d LB 0x3a
    push eax                                  ; 66 50
    push dx                                   ; 52
    mov eax, strict dword 080000000h          ; 66 b8 00 00 00 80
    mov dx, 00cf8h                            ; ba f8 0c
    out DX, eax                               ; 66 ef
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    cmp eax, strict dword 012378086h          ; 66 3d 86 80 37 12
    je short 0f157h                           ; 74 1f
    mov eax, strict dword 08000f000h          ; 66 b8 00 f0 00 80
    mov dx, 00cf8h                            ; ba f8 0c
    out DX, eax                               ; 66 ef
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    cmp eax, strict dword 0244e8086h          ; 66 3d 86 80 4e 24
    je short 0f157h                           ; 74 07
    pop dx                                    ; 5a
    pop eax                                   ; 66 58
    mov AH, strict byte 0ffh                  ; b4 ff
    stc                                       ; f9
    retn                                      ; c3
pci_present:                                 ; 0xff157 LB 0x1e
    pop dx                                    ; 5a
    pop eax                                   ; 66 58
    cmp AL, strict byte 001h                  ; 3c 01
    jne short 0f175h                          ; 75 17
    mov ax, strict word 00001h                ; b8 01 00
    mov bx, 00210h                            ; bb 10 02
    mov cx, strict word 00000h                ; b9 00 00
    mov edx, strict dword 020494350h          ; 66 ba 50 43 49 20
    mov edi, strict dword 00000de60h          ; 66 bf 60 de 00 00
    clc                                       ; f8
    retn                                      ; c3
pci_real_f02:                                ; 0xff175 LB 0x16
    push esi                                  ; 66 56
    push edi                                  ; 66 57
    push edx                                  ; 66 52
    cmp AL, strict byte 002h                  ; 3c 02
    jne short 0f1b7h                          ; 75 38
    sal ecx, 010h                             ; 66 c1 e1 10
    db  08bh, 0cah
    ; mov cx, dx                                ; 8b ca
    db  066h, 033h, 0dbh
    ; xor ebx, ebx                              ; 66 33 db
    mov di, strict word 00000h                ; bf 00 00
pci_real_devloop:                            ; 0xff18b LB 0x15
    call 0f2c3h                               ; e8 35 01
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    db  066h, 03bh, 0c1h
    ; cmp eax, ecx                              ; 66 3b c1
    jne short 0f1a0h                          ; 75 08
    cmp si, strict byte 00000h                ; 83 fe 00
    je near 0f2b9h                            ; 0f 84 1a 01
    dec si                                    ; 4e
pci_real_nextdev:                            ; 0xff1a0 LB 0x17
    inc ebx                                   ; 66 43
    cmp ebx, strict dword 000010000h          ; 66 81 fb 00 00 01 00
    jne short 0f18bh                          ; 75 e0
    db  08bh, 0d1h
    ; mov dx, cx                                ; 8b d1
    shr ecx, 010h                             ; 66 c1 e9 10
    mov ax, 08602h                            ; b8 02 86
    jmp near 0f2b1h                           ; e9 fa 00
pci_real_f03:                                ; 0xff1b7 LB 0xa
    cmp AL, strict byte 003h                  ; 3c 03
    jne short 0f1ebh                          ; 75 30
    db  066h, 033h, 0dbh
    ; xor ebx, ebx                              ; 66 33 db
    mov di, strict word 00008h                ; bf 08 00
pci_real_devloop2:                           ; 0xff1c1 LB 0x19
    call 0f2c3h                               ; e8 ff 00
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    shr eax, 008h                             ; 66 c1 e8 08
    db  066h, 03bh, 0c1h
    ; cmp eax, ecx                              ; 66 3b c1
    jne short 0f1dah                          ; 75 08
    cmp si, strict byte 00000h                ; 83 fe 00
    je near 0f2b9h                            ; 0f 84 e0 00
    dec si                                    ; 4e
pci_real_nextdev2:                           ; 0xff1da LB 0xd7
    inc ebx                                   ; 66 43
    cmp ebx, strict dword 000010000h          ; 66 81 fb 00 00 01 00
    jne short 0f1c1h                          ; 75 dc
    mov ax, 08603h                            ; b8 03 86
    jmp near 0f2b1h                           ; e9 c6 00
    cmp AL, strict byte 008h                  ; 3c 08
    jne short 0f203h                          ; 75 14
    call 0f2c3h                               ; e8 d1 00
    push dx                                   ; 52
    db  08bh, 0d7h
    ; mov dx, di                                ; 8b d7
    and dx, strict byte 00003h                ; 83 e2 03
    add dx, 00cfch                            ; 81 c2 fc 0c
    in AL, DX                                 ; ec
    pop dx                                    ; 5a
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8
    jmp near 0f2b9h                           ; e9 b6 00
    cmp AL, strict byte 009h                  ; 3c 09
    jne short 0f21bh                          ; 75 14
    call 0f2c3h                               ; e8 b9 00
    push dx                                   ; 52
    db  08bh, 0d7h
    ; mov dx, di                                ; 8b d7
    and dx, strict byte 00002h                ; 83 e2 02
    add dx, 00cfch                            ; 81 c2 fc 0c
    in ax, DX                                 ; ed
    pop dx                                    ; 5a
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    jmp near 0f2b9h                           ; e9 9e 00
    cmp AL, strict byte 00ah                  ; 3c 0a
    jne short 0f22fh                          ; 75 10
    call 0f2c3h                               ; e8 a1 00
    push dx                                   ; 52
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    pop dx                                    ; 5a
    db  066h, 08bh, 0c8h
    ; mov ecx, eax                              ; 66 8b c8
    jmp near 0f2b9h                           ; e9 8a 00
    cmp AL, strict byte 00bh                  ; 3c 0b
    jne short 0f246h                          ; 75 13
    call 0f2c3h                               ; e8 8d 00
    push dx                                   ; 52
    db  08bh, 0d7h
    ; mov dx, di                                ; 8b d7
    and dx, strict byte 00003h                ; 83 e2 03
    add dx, 00cfch                            ; 81 c2 fc 0c
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    jmp short 0f2b9h                          ; eb 73
    cmp AL, strict byte 00ch                  ; 3c 0c
    jne short 0f25dh                          ; 75 13
    call 0f2c3h                               ; e8 76 00
    push dx                                   ; 52
    db  08bh, 0d7h
    ; mov dx, di                                ; 8b d7
    and dx, strict byte 00002h                ; 83 e2 02
    add dx, 00cfch                            ; 81 c2 fc 0c
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1
    out DX, ax                                ; ef
    pop dx                                    ; 5a
    jmp short 0f2b9h                          ; eb 5c
    cmp AL, strict byte 00dh                  ; 3c 0d
    jne short 0f270h                          ; 75 0f
    call 0f2c3h                               ; e8 5f 00
    push dx                                   ; 52
    mov dx, 00cfch                            ; ba fc 0c
    db  066h, 08bh, 0c1h
    ; mov eax, ecx                              ; 66 8b c1
    out DX, eax                               ; 66 ef
    pop dx                                    ; 5a
    jmp short 0f2b9h                          ; eb 49
    cmp AL, strict byte 00eh                  ; 3c 0e
    jne short 0f2afh                          ; 75 3b
    cmp word [es:di], 001e0h                  ; 26 81 3d e0 01
    jc short 0f2a6h                           ; 72 2b
    mov word [es:di], 001e0h                  ; 26 c7 05 e0 01
    pushfw                                    ; 9c
    push DS                                   ; 1e
    push ES                                   ; 06
    push cx                                   ; 51
    push si                                   ; 56
    push di                                   ; 57
    cld                                       ; fc
    mov si, 0f480h                            ; be 80 f4
    push CS                                   ; 0e
    pop DS                                    ; 1f
    mov cx, word [es:di+002h]                 ; 26 8b 4d 02
    mov es, [es:di+004h]                      ; 26 8e 45 04
    db  08bh, 0f9h
    ; mov di, cx                                ; 8b f9
    mov cx, 001e0h                            ; b9 e0 01
    rep movsb                                 ; f3 a4
    pop di                                    ; 5f
    pop si                                    ; 5e
    pop cx                                    ; 59
    pop ES                                    ; 07
    pop DS                                    ; 1f
    popfw                                     ; 9d
    mov bx, 00a00h                            ; bb 00 0a
    jmp short 0f2b9h                          ; eb 13
    mov word [es:di], 001e0h                  ; 26 c7 05 e0 01
    mov AH, strict byte 089h                  ; b4 89
    jmp short 0f2b1h                          ; eb 02
    mov AH, strict byte 081h                  ; b4 81
pci_real_fail:                               ; 0xff2b1 LB 0x8
    pop edx                                   ; 66 5a
    pop edi                                   ; 66 5f
    pop esi                                   ; 66 5e
    stc                                       ; f9
    retn                                      ; c3
pci_real_ok:                                 ; 0xff2b9 LB 0xa
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4
    pop edx                                   ; 66 5a
    pop edi                                   ; 66 5f
    pop esi                                   ; 66 5e
    clc                                       ; f8
    retn                                      ; c3
pci_real_select_reg:                         ; 0xff2c3 LB 0x3b
    push dx                                   ; 52
    mov eax, strict dword 000800000h          ; 66 b8 00 00 80 00
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3
    sal eax, 008h                             ; 66 c1 e0 08
    and di, 000ffh                            ; 81 e7 ff 00
    db  00bh, 0c7h
    ; or ax, di                                 ; 0b c7
    and AL, strict byte 0fch                  ; 24 fc
    mov dx, 00cf8h                            ; ba f8 0c
    out DX, eax                               ; 66 ef
    pop dx                                    ; 5a
    retn                                      ; c3
    or cx, word [bp+si]                       ; 0b 0a
    or word [di], ax                          ; 09 05
    push eax                                  ; 66 50
    mov eax, strict dword 000800000h          ; 66 b8 00 00 80 00
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3
    sal eax, 008h                             ; 66 c1 e0 08
    and dl, 0fch                              ; 80 e2 fc
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2
    mov dx, 00cf8h                            ; ba f8 0c
    out DX, eax                               ; 66 ef
    pop eax                                   ; 66 58
    retn                                      ; c3
pcibios_init_iomem_bases:                    ; 0xff2fe LB 0x16
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    mov eax, strict dword 0e0000000h          ; 66 b8 00 00 00 e0
    push eax                                  ; 66 50
    mov ax, 0d000h                            ; b8 00 d0
    push ax                                   ; 50
    mov ax, strict word 00010h                ; b8 10 00
    push ax                                   ; 50
    mov bx, strict word 00008h                ; bb 08 00
pci_init_io_loop1:                           ; 0xff314 LB 0xe
    mov DL, strict byte 000h                  ; b2 00
    call 0f2e3h                               ; e8 ca ff
    mov dx, 00cfch                            ; ba fc 0c
    in ax, DX                                 ; ed
    cmp ax, strict byte 0ffffh                ; 83 f8 ff
    je short 0f35bh                           ; 74 39
enable_iomem_space:                          ; 0xff322 LB 0x39
    mov DL, strict byte 004h                  ; b2 04
    call 0f2e3h                               ; e8 bc ff
    mov dx, 00cfch                            ; ba fc 0c
    in AL, DX                                 ; ec
    or AL, strict byte 007h                   ; 0c 07
    out DX, AL                                ; ee
    mov DL, strict byte 000h                  ; b2 00
    call 0f2e3h                               ; e8 b0 ff
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    cmp eax, strict dword 020001022h          ; 66 3d 22 10 00 20
    jne short 0f35bh                          ; 75 1b
    mov DL, strict byte 010h                  ; b2 10
    call 0f2e3h                               ; e8 9e ff
    mov dx, 00cfch                            ; ba fc 0c
    in ax, DX                                 ; ed
    and ax, strict byte 0fffch                ; 83 e0 fc
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    db  08bh, 0d1h
    ; mov dx, cx                                ; 8b d1
    add dx, strict byte 00014h                ; 83 c2 14
    in ax, DX                                 ; ed
    db  08bh, 0d1h
    ; mov dx, cx                                ; 8b d1
    add dx, strict byte 00018h                ; 83 c2 18
    in eax, DX                                ; 66 ed
next_pci_dev:                                ; 0xff35b LB 0xf
    mov byte [bp-008h], 010h                  ; c6 46 f8 10
    inc bx                                    ; 43
    cmp bx, 00100h                            ; 81 fb 00 01
    jne short 0f314h                          ; 75 ae
    db  08bh, 0e5h
    ; mov sp, bp                                ; 8b e5
    pop bp                                    ; 5d
    retn                                      ; c3
pcibios_init_set_elcr:                       ; 0xff36a LB 0xc
    push ax                                   ; 50
    push cx                                   ; 51
    mov dx, 004d0h                            ; ba d0 04
    test AL, strict byte 008h                 ; a8 08
    je short 0f376h                           ; 74 03
    inc dx                                    ; 42
    and AL, strict byte 007h                  ; 24 07
is_master_pic:                               ; 0xff376 LB 0xd
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8
    mov BL, strict byte 001h                  ; b3 01
    sal bl, CL                                ; d2 e3
    in AL, DX                                 ; ec
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3
    out DX, AL                                ; ee
    pop cx                                    ; 59
    pop ax                                    ; 58
    retn                                      ; c3
pcibios_init_irqs:                           ; 0xff383 LB 0x53
    push DS                                   ; 1e
    push bp                                   ; 55
    mov ax, 0f000h                            ; b8 00 f0
    mov ds, ax                                ; 8e d8
    mov dx, 004d0h                            ; ba d0 04
    mov AL, strict byte 000h                  ; b0 00
    out DX, AL                                ; ee
    inc dx                                    ; 42
    out DX, AL                                ; ee
    mov si, 0f460h                            ; be 60 f4
    mov bh, byte [si+008h]                    ; 8a 7c 08
    mov bl, byte [si+009h]                    ; 8a 5c 09
    mov DL, strict byte 000h                  ; b2 00
    call 0f2e3h                               ; e8 43 ff
    mov dx, 00cfch                            ; ba fc 0c
    in eax, DX                                ; 66 ed
    cmp eax, dword [si+00ch]                  ; 66 3b 44 0c
    jne near 0f453h                           ; 0f 85 a6 00
    mov dl, byte [si+022h]                    ; 8a 54 22
    call 0f2e3h                               ; e8 30 ff
    push bx                                   ; 53
    mov dx, 00cfch                            ; ba fc 0c
    mov ax, 08080h                            ; b8 80 80
    out DX, ax                                ; ef
    add dx, strict byte 00002h                ; 83 c2 02
    out DX, ax                                ; ef
    mov ax, word [si+006h]                    ; 8b 44 06
    sub ax, strict byte 00020h                ; 83 e8 20
    shr ax, 004h                              ; c1 e8 04
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8
    add si, strict byte 00020h                ; 83 c6 20
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    mov ax, 0f2dfh                            ; b8 df f2
    push ax                                   ; 50
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    push ax                                   ; 50
pci_init_irq_loop1:                          ; 0xff3d6 LB 0x5
    mov bh, byte [si]                         ; 8a 3c
    mov bl, byte [si+001h]                    ; 8a 5c 01
pci_init_irq_loop2:                          ; 0xff3db LB 0x15
    mov DL, strict byte 000h                  ; b2 00
    call 0f2e3h                               ; e8 03 ff
    mov dx, 00cfch                            ; ba fc 0c
    in ax, DX                                 ; ed
    cmp ax, strict byte 0ffffh                ; 83 f8 ff
    jne short 0f3f0h                          ; 75 07
    test bl, 007h                             ; f6 c3 07
    je short 0f447h                           ; 74 59
    jmp short 0f43dh                          ; eb 4d
pci_test_int_pin:                            ; 0xff3f0 LB 0x3c
    mov DL, strict byte 03ch                  ; b2 3c
    call 0f2e3h                               ; e8 ee fe
    mov dx, 00cfdh                            ; ba fd 0c
    in AL, DX                                 ; ec
    and AL, strict byte 007h                  ; 24 07
    je short 0f43dh                           ; 74 40
    db  0feh, 0c8h
    ; dec al                                    ; fe c8
    mov DL, strict byte 003h                  ; b2 03
    mul dl                                    ; f6 e2
    add AL, strict byte 002h                  ; 04 02
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8
    mov al, byte [bx+si]                      ; 8a 00
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0
    mov bx, word [byte bp+000h]               ; 8b 5e 00
    call 0f2e3h                               ; e8 d0 fe
    mov dx, 00cfch                            ; ba fc 0c
    and AL, strict byte 003h                  ; 24 03
    db  002h, 0d0h
    ; add dl, al                                ; 02 d0
    in AL, DX                                 ; ec
    cmp AL, strict byte 080h                  ; 3c 80
    jc short 0f42ch                           ; 72 0d
    mov bx, word [bp-002h]                    ; 8b 5e fe
    mov al, byte [bx]                         ; 8a 07
    out DX, AL                                ; ee
    inc bx                                    ; 43
    mov word [bp-002h], bx                    ; 89 5e fe
    call 0f36ah                               ; e8 3e ff
pirq_found:                                  ; 0xff42c LB 0x11
    mov bh, byte [si]                         ; 8a 3c
    mov bl, byte [si+001h]                    ; 8a 5c 01
    add bl, byte [bp-003h]                    ; 02 5e fd
    mov DL, strict byte 03ch                  ; b2 3c
    call 0f2e3h                               ; e8 aa fe
    mov dx, 00cfch                            ; ba fc 0c
    out DX, AL                                ; ee
next_pci_func:                               ; 0xff43d LB 0xa
    inc byte [bp-003h]                        ; fe 46 fd
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3
    test bl, 007h                             ; f6 c3 07
    jne short 0f3dbh                          ; 75 94
next_pir_entry:                              ; 0xff447 LB 0xc
    add si, strict byte 00010h                ; 83 c6 10
    mov byte [bp-003h], 000h                  ; c6 46 fd 00
    loop 0f3d6h                               ; e2 86
    db  08bh, 0e5h
    ; mov sp, bp                                ; 8b e5
    pop bx                                    ; 5b
pci_init_end:                                ; 0xff453 LB 0xd
    pop bp                                    ; 5d
    pop DS                                    ; 1f
    retn                                      ; c3
    db  089h, 0c0h, 089h, 0c0h, 089h, 0c0h, 089h, 0c0h, 089h, 0c0h
pci_routing_table_structure:                 ; 0xff460 LB 0x3e1
    db  024h, 050h, 049h, 052h, 000h, 001h, 000h, 002h, 000h, 008h, 000h, 000h, 086h, 080h, 000h, 070h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 008h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 000h, 000h
    db  000h, 010h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 001h, 000h
    db  000h, 018h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 002h, 000h
    db  000h, 020h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 003h, 000h
    db  000h, 028h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 004h, 000h
    db  000h, 030h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 005h, 000h
    db  000h, 038h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 006h, 000h
    db  000h, 040h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 007h, 000h
    db  000h, 048h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 008h, 000h
    db  000h, 050h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 009h, 000h
    db  000h, 058h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 00ah, 000h
    db  000h, 060h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 00bh, 000h
    db  000h, 068h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 00ch, 000h
    db  000h, 070h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 00dh, 000h
    db  000h, 078h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 00eh, 000h
    db  000h, 080h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 00fh, 000h
    db  000h, 088h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 010h, 000h
    db  000h, 090h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 011h, 000h
    db  000h, 098h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 012h, 000h
    db  000h, 0a0h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 013h, 000h
    db  000h, 0a8h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 014h, 000h
    db  000h, 0b0h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 015h, 000h
    db  000h, 0b8h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 016h, 000h
    db  000h, 0c0h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 017h, 000h
    db  000h, 0c8h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 018h, 000h
    db  000h, 0d0h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 019h, 000h
    db  000h, 0d8h, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 01ah, 000h
    db  000h, 0e0h, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 01bh, 000h
    db  000h, 0e8h, 060h, 0f8h, 0deh, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 01ch, 000h
    db  000h, 0f0h, 061h, 0f8h, 0deh, 062h, 0f8h, 0deh, 063h, 0f8h, 0deh, 060h, 0f8h, 0deh, 01dh, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 058h
    db  04dh
int12_handler:                               ; 0xff841 LB 0xc
    sti                                       ; fb
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov ax, word [00013h]                     ; a1 13 00
    pop DS                                    ; 1f
    iret                                      ; cf
int11_handler:                               ; 0xff84d LB 0xc
    sti                                       ; fb
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov ax, word [00010h]                     ; a1 10 00
    pop DS                                    ; 1f
    iret                                      ; cf
int15_handler:                               ; 0xff859 LB 0x28
    pushfw                                    ; 9c
    cmp ah, 053h                              ; 80 fc 53
    je short 0f87dh                           ; 74 1e
    push DS                                   ; 1e
    push ES                                   ; 06
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    cmp ah, 086h                              ; 80 fc 86
    je short 0f886h                           ; 74 1d
    cmp ah, 0e8h                              ; 80 fc e8
    je short 0f886h                           ; 74 18
    pushaw                                    ; 60
    cmp ah, 0c2h                              ; 80 fc c2
    je short 0f881h                           ; 74 0d
    call 060f8h                               ; e8 81 68
    popaw                                     ; 61
    pop ES                                    ; 07
    pop DS                                    ; 1f
    popfw                                     ; 9d
    jmp short 0f88fh                          ; eb 12
    popfw                                     ; 9d
    stc                                       ; f9
    jmp short 0f88fh                          ; eb 0e
int15_handler_mouse:                         ; 0xff881 LB 0x5
    call 06d46h                               ; e8 c2 74
    jmp short 0f877h                          ; eb f1
int15_handler32:                             ; 0xff886 LB 0x9
    pushad                                    ; 66 60
    call 065c6h                               ; e8 3b 6d
    popad                                     ; 66 61
    jmp short 0f878h                          ; eb e9
iret_modify_cf:                              ; 0xff88f LB 0x14
    jc short 0f89ah                           ; 72 09
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    and byte [bp+006h], 0feh                  ; 80 66 06 fe
    pop bp                                    ; 5d
    iret                                      ; cf
    push bp                                   ; 55
    db  08bh, 0ech
    ; mov bp, sp                                ; 8b ec
    or byte [bp+006h], 001h                   ; 80 4e 06 01
    pop bp                                    ; 5d
    iret                                      ; cf
int74_handler:                               ; 0xff8a3 LB 0x2e
    sti                                       ; fb
    pushaw                                    ; 60
    push ES                                   ; 06
    push DS                                   ; 1e
    push strict byte 00000h                   ; 6a 00
    push strict byte 00000h                   ; 6a 00
    push strict byte 00000h                   ; 6a 00
    push strict byte 00000h                   ; 6a 00
    push strict byte 00000h                   ; 6a 00
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 06c74h                               ; e8 bd 73
    pop cx                                    ; 59
    jcxz 0f8c6h                               ; e3 0c
    push strict byte 00000h                   ; 6a 00
    pop DS                                    ; 1f
    push word [0040eh]                        ; ff 36 0e 04
    pop DS                                    ; 1f
    call far [word 00022h]                    ; ff 1e 22 00
    cli                                       ; fa
    call 0e03bh                               ; e8 71 e7
    add sp, strict byte 00008h                ; 83 c4 08
    pop DS                                    ; 1f
    pop ES                                    ; 07
    popaw                                     ; 61
    iret                                      ; cf
int76_handler:                               ; 0xff8d1 LB 0x19d
    push ax                                   ; 50
    push DS                                   ; 1e
    mov ax, strict word 00040h                ; b8 40 00
    mov ds, ax                                ; 8e d8
    mov byte [0008eh], 0ffh                   ; c6 06 8e 00 ff
    call 0e03bh                               ; e8 5b e7
    pop DS                                    ; 1f
    pop ax                                    ; 58
    iret                                      ; cf
    times 0x189 db 0
    db  'XM'
font8x8:                                     ; 0xffa6e LB 0x420
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 081h, 0a5h, 081h, 0bdh, 099h, 081h, 07eh
    db  07eh, 0ffh, 0dbh, 0ffh, 0c3h, 0e7h, 0ffh, 07eh, 06ch, 0feh, 0feh, 0feh, 07ch, 038h, 010h, 000h
    db  010h, 038h, 07ch, 0feh, 07ch, 038h, 010h, 000h, 038h, 07ch, 038h, 0feh, 0feh, 07ch, 038h, 07ch
    db  010h, 010h, 038h, 07ch, 0feh, 07ch, 038h, 07ch, 000h, 000h, 018h, 03ch, 03ch, 018h, 000h, 000h
    db  0ffh, 0ffh, 0e7h, 0c3h, 0c3h, 0e7h, 0ffh, 0ffh, 000h, 03ch, 066h, 042h, 042h, 066h, 03ch, 000h
    db  0ffh, 0c3h, 099h, 0bdh, 0bdh, 099h, 0c3h, 0ffh, 00fh, 007h, 00fh, 07dh, 0cch, 0cch, 0cch, 078h
    db  03ch, 066h, 066h, 066h, 03ch, 018h, 07eh, 018h, 03fh, 033h, 03fh, 030h, 030h, 070h, 0f0h, 0e0h
    db  07fh, 063h, 07fh, 063h, 063h, 067h, 0e6h, 0c0h, 099h, 05ah, 03ch, 0e7h, 0e7h, 03ch, 05ah, 099h
    db  080h, 0e0h, 0f8h, 0feh, 0f8h, 0e0h, 080h, 000h, 002h, 00eh, 03eh, 0feh, 03eh, 00eh, 002h, 000h
    db  018h, 03ch, 07eh, 018h, 018h, 07eh, 03ch, 018h, 066h, 066h, 066h, 066h, 066h, 000h, 066h, 000h
    db  07fh, 0dbh, 0dbh, 07bh, 01bh, 01bh, 01bh, 000h, 03eh, 063h, 038h, 06ch, 06ch, 038h, 0cch, 078h
    db  000h, 000h, 000h, 000h, 07eh, 07eh, 07eh, 000h, 018h, 03ch, 07eh, 018h, 07eh, 03ch, 018h, 0ffh
    db  018h, 03ch, 07eh, 018h, 018h, 018h, 018h, 000h, 018h, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h
    db  000h, 018h, 00ch, 0feh, 00ch, 018h, 000h, 000h, 000h, 030h, 060h, 0feh, 060h, 030h, 000h, 000h
    db  000h, 000h, 0c0h, 0c0h, 0c0h, 0feh, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h
    db  000h, 018h, 03ch, 07eh, 0ffh, 0ffh, 000h, 000h, 000h, 0ffh, 0ffh, 07eh, 03ch, 018h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 078h, 078h, 030h, 030h, 000h, 030h, 000h
    db  06ch, 06ch, 06ch, 000h, 000h, 000h, 000h, 000h, 06ch, 06ch, 0feh, 06ch, 0feh, 06ch, 06ch, 000h
    db  030h, 07ch, 0c0h, 078h, 00ch, 0f8h, 030h, 000h, 000h, 0c6h, 0cch, 018h, 030h, 066h, 0c6h, 000h
    db  038h, 06ch, 038h, 076h, 0dch, 0cch, 076h, 000h, 060h, 060h, 0c0h, 000h, 000h, 000h, 000h, 000h
    db  018h, 030h, 060h, 060h, 060h, 030h, 018h, 000h, 060h, 030h, 018h, 018h, 018h, 030h, 060h, 000h
    db  000h, 066h, 03ch, 0ffh, 03ch, 066h, 000h, 000h, 000h, 030h, 030h, 0fch, 030h, 030h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 030h, 030h, 060h, 000h, 000h, 000h, 0fch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 030h, 030h, 000h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 080h, 000h
    db  07ch, 0c6h, 0ceh, 0deh, 0f6h, 0e6h, 07ch, 000h, 030h, 070h, 030h, 030h, 030h, 030h, 0fch, 000h
    db  078h, 0cch, 00ch, 038h, 060h, 0cch, 0fch, 000h, 078h, 0cch, 00ch, 038h, 00ch, 0cch, 078h, 000h
    db  01ch, 03ch, 06ch, 0cch, 0feh, 00ch, 01eh, 000h, 0fch, 0c0h, 0f8h, 00ch, 00ch, 0cch, 078h, 000h
    db  038h, 060h, 0c0h, 0f8h, 0cch, 0cch, 078h, 000h, 0fch, 0cch, 00ch, 018h, 030h, 030h, 030h, 000h
    db  078h, 0cch, 0cch, 078h, 0cch, 0cch, 078h, 000h, 078h, 0cch, 0cch, 07ch, 00ch, 018h, 070h, 000h
    db  000h, 030h, 030h, 000h, 000h, 030h, 030h, 000h, 000h, 030h, 030h, 000h, 000h, 030h, 030h, 060h
    db  018h, 030h, 060h, 0c0h, 060h, 030h, 018h, 000h, 000h, 000h, 0fch, 000h, 000h, 0fch, 000h, 000h
    db  060h, 030h, 018h, 00ch, 018h, 030h, 060h, 000h, 078h, 0cch, 00ch, 018h, 030h, 000h, 030h, 000h
    db  07ch, 0c6h, 0deh, 0deh, 0deh, 0c0h, 078h, 000h, 030h, 078h, 0cch, 0cch, 0fch, 0cch, 0cch, 000h
    db  0fch, 066h, 066h, 07ch, 066h, 066h, 0fch, 000h, 03ch, 066h, 0c0h, 0c0h, 0c0h, 066h, 03ch, 000h
    db  0f8h, 06ch, 066h, 066h, 066h, 06ch, 0f8h, 000h, 0feh, 062h, 068h, 078h, 068h, 062h, 0feh, 000h
    db  0feh, 062h, 068h, 078h, 068h, 060h, 0f0h, 000h, 03ch, 066h, 0c0h, 0c0h, 0ceh, 066h, 03eh, 000h
    db  0cch, 0cch, 0cch, 0fch, 0cch, 0cch, 0cch, 000h, 078h, 030h, 030h, 030h, 030h, 030h, 078h, 000h
    db  01eh, 00ch, 00ch, 00ch, 0cch, 0cch, 078h, 000h, 0e6h, 066h, 06ch, 078h, 06ch, 066h, 0e6h, 000h
    db  0f0h, 060h, 060h, 060h, 062h, 066h, 0feh, 000h, 0c6h, 0eeh, 0feh, 0feh, 0d6h, 0c6h, 0c6h, 000h
    db  0c6h, 0e6h, 0f6h, 0deh, 0ceh, 0c6h, 0c6h, 000h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 06ch, 038h, 000h
    db  0fch, 066h, 066h, 07ch, 060h, 060h, 0f0h, 000h, 078h, 0cch, 0cch, 0cch, 0dch, 078h, 01ch, 000h
    db  0fch, 066h, 066h, 07ch, 06ch, 066h, 0e6h, 000h, 078h, 0cch, 0e0h, 070h, 01ch, 0cch, 078h, 000h
    db  0fch, 0b4h, 030h, 030h, 030h, 030h, 078h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 0fch, 000h
    db  0cch, 0cch, 0cch, 0cch, 0cch, 078h, 030h, 000h, 0c6h, 0c6h, 0c6h, 0d6h, 0feh, 0eeh, 0c6h, 000h
    db  0c6h, 0c6h, 06ch, 038h, 038h, 06ch, 0c6h, 000h, 0cch, 0cch, 0cch, 078h, 030h, 030h, 078h, 000h
    db  0feh, 0c6h, 08ch, 018h, 032h, 066h, 0feh, 000h, 078h, 060h, 060h, 060h, 060h, 060h, 078h, 000h
    db  0c0h, 060h, 030h, 018h, 00ch, 006h, 002h, 000h, 078h, 018h, 018h, 018h, 018h, 018h, 078h, 000h
    db  010h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  030h, 030h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 078h, 00ch, 07ch, 0cch, 076h, 000h
    db  0e0h, 060h, 060h, 07ch, 066h, 066h, 0dch, 000h, 000h, 000h, 078h, 0cch, 0c0h, 0cch, 078h, 000h
    db  01ch, 00ch, 00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h
    db  038h, 06ch, 060h, 0f0h, 060h, 060h, 0f0h, 000h, 000h, 000h, 076h, 0cch, 0cch, 07ch, 00ch, 0f8h
    db  0e0h, 060h, 06ch, 076h, 066h, 066h, 0e6h, 000h, 030h, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  00ch, 000h, 00ch, 00ch, 00ch, 0cch, 0cch, 078h, 0e0h, 060h, 066h, 06ch, 078h, 06ch, 0e6h, 000h
    db  070h, 030h, 030h, 030h, 030h, 030h, 078h, 000h, 000h, 000h, 0cch, 0feh, 0feh, 0d6h, 0c6h, 000h
    db  000h, 000h, 0f8h, 0cch, 0cch, 0cch, 0cch, 000h, 000h, 000h, 078h, 0cch, 0cch, 0cch, 078h, 000h
    db  000h, 000h, 0dch, 066h, 066h, 07ch, 060h, 0f0h, 000h, 000h, 076h, 0cch, 0cch, 07ch, 00ch, 01eh
    db  000h, 000h, 0dch, 076h, 066h, 060h, 0f0h, 000h, 000h, 000h, 07ch, 0c0h, 078h, 00ch, 0f8h, 000h
    db  010h, 030h, 07ch, 030h, 030h, 034h, 018h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 0cch, 0cch, 0cch, 078h, 030h, 000h, 000h, 000h, 0c6h, 0d6h, 0feh, 0feh, 06ch, 000h
    db  000h, 000h, 0c6h, 06ch, 038h, 06ch, 0c6h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 07ch, 00ch, 0f8h
    db  000h, 000h, 0fch, 098h, 030h, 064h, 0fch, 000h, 01ch, 030h, 030h, 0e0h, 030h, 030h, 01ch, 000h
    db  018h, 018h, 018h, 000h, 018h, 018h, 018h, 000h, 0e0h, 030h, 030h, 01ch, 030h, 030h, 0e0h, 000h
    db  076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 000h
    db  080h, 0fch, 0b1h, 075h, 00eh, 0e8h, 0a7h, 0f2h, 072h, 003h, 0e9h, 014h, 0fah, 060h, 0e8h, 050h
    db  06bh, 061h, 0cfh, 006h, 01eh, 060h, 00eh, 01fh, 0fch, 0e8h, 076h, 06bh, 061h, 01fh, 007h, 0cfh
int70_handler:                               ; 0xffe8e LB 0x17
    push ES                                   ; 06
    push DS                                   ; 1e
    pushaw                                    ; 60
    push CS                                   ; 0e
    pop DS                                    ; 1f
    cld                                       ; fc
    call 06921h                               ; e8 8a 6a
    popaw                                     ; 61
    pop DS                                    ; 1f
    pop ES                                    ; 07
    iret                                      ; cf
    times 0x8 db 0
    db  'XM'
int08_handler:                               ; 0xffea5 LB 0xae
    sti                                       ; fb
    push eax                                  ; 66 50
    push DS                                   ; 1e
    db  033h, 0c0h
    ; xor ax, ax                                ; 33 c0
    mov ds, ax                                ; 8e d8
    mov AL, byte [00440h]                     ; a0 40 04
    db  00ah, 0c0h
    ; or al, al                                 ; 0a c0
    je short 0febdh                           ; 74 09
    push dx                                   ; 52
    mov dx, 003f2h                            ; ba f2 03
    in AL, DX                                 ; ec
    and AL, strict byte 0cfh                  ; 24 cf
    out DX, AL                                ; ee
    pop dx                                    ; 5a
    mov eax, dword [0046ch]                   ; 66 a1 6c 04
    inc eax                                   ; 66 40
    cmp eax, strict dword 0001800b0h          ; 66 3d b0 00 18 00
    jc short 0fed2h                           ; 72 07
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0
    inc byte [00470h]                         ; fe 06 70 04
    mov dword [0046ch], eax                   ; 66 a3 6c 04
    int 01ch                                  ; cd 1c
    cli                                       ; fa
    call 0e03fh                               ; e8 63 e1
    pop DS                                    ; 1f
    pop eax                                   ; 66 58
    iret                                      ; cf
    times 0x11 db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    times 0xb db 0
    pop ax                                    ; 58
    dec bp                                    ; 4d
    dec di                                    ; 4f
    jc short 0ff64h                           ; 72 61
    arpl word [si+065h], bp                   ; 63 6c 65
    and byte [bp+04dh], dl                    ; 20 56 4d
    and byte [bp+069h], dl                    ; 20 56 69
    jc short 0ff82h                           ; 72 74
    jne short 0ff71h                          ; 75 61
    insb                                      ; 6c
    inc dx                                    ; 42
    outsw                                     ; 6f
    js short 0ff35h                           ; 78 20
    inc dx                                    ; 42
    dec cx                                    ; 49
    dec di                                    ; 4f
    push bx                                   ; 53
    times 0x38 db 0
    db  'XM'
dummy_iret:                                  ; 0xfff53 LB 0x9d
    iret                                      ; cf
    iret                                      ; cf
    mov ax, ax                                ; 89 c0
    mov ax, ax                                ; 89 c0
    mov ax, ax                                ; 89 c0
    mov ax, ax                                ; 89 c0
    mov ax, ax                                ; 89 c0
    cld                                       ; fc
    pop di                                    ; 5f
    push bx                                   ; 53
    dec bp                                    ; 4d
    pop di                                    ; 5f
    add byte [bx], bl                         ; 00 1f
    add al, byte [di]                         ; 02 05
    inc word [bx+si]                          ; ff 00
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si], al                      ; 00 00
    add byte [bx+si], al                      ; 00 00
    pop di                                    ; 5f
    inc sp                                    ; 44
    dec bp                                    ; 4d
    dec cx                                    ; 49
    pop di                                    ; 5f
    add byte [bx+si+001h], ah                 ; 00 60 01
    add byte [bx+si], dl                      ; 00 10
    push CS                                   ; 0e
    add byte [di], al                         ; 00 05
    add byte [di], ah                         ; 00 25
    times 0x6f db 0
    db  'XM'
cpu_reset:                                   ; 0xffff0 LB 0x10
    jmp far 0f000h:0e05bh                     ; ea 5b e0 00 f0
    db  030h, 036h, 02fh, 032h, 033h, 02fh, 039h, 039h, 000h, 0fch, 0ffh
