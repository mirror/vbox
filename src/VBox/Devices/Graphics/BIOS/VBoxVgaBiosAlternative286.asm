; $Id$ 
;; @file
; Auto Generated source file. Do not edit.
;

;
; Source file: vgarom.asm
;
;  ============================================================================================
;  
;   Copyright (C) 2001,2002 the LGPL VGABios developers Team
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;  
;  ============================================================================================
;  
;   This VGA Bios is specific to the plex86/bochs Emulated VGA card.
;   You can NOT drive any physical vga card with it.
;  
;  ============================================================================================
;  

;
; Source file: vberom.asm
;
;  ============================================================================================
;  
;   Copyright (C) 2002 Jeroen Janssen
;  
;   This library is free software; you can redistribute it and/or
;   modify it under the terms of the GNU Lesser General Public
;   License as published by the Free Software Foundation; either
;   version 2 of the License, or (at your option) any later version.
;  
;   This library is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;   Lesser General Public License for more details.
;  
;   You should have received a copy of the GNU Lesser General Public
;   License along with this library; if not, write to the Free Software
;   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;  
;  ============================================================================================
;  
;   This VBE is part of the VGA Bios specific to the plex86/bochs Emulated VGA card.
;   You can NOT drive any physical vga card with it.
;  
;  ============================================================================================
;  
;   This VBE Bios is based on information taken from :
;    - VESA BIOS EXTENSION (VBE) Core Functions Standard Version 3.0 located at www.vesa.org
;  
;  ============================================================================================

;
; Source file: vgabios.c
;
;  // ============================================================================================
;  
;  vgabios.c
;  
;  // ============================================================================================
;  //
;  //  Copyright (C) 2001,2002 the LGPL VGABios developers Team
;  //
;  //  This library is free software; you can redistribute it and/or
;  //  modify it under the terms of the GNU Lesser General Public
;  //  License as published by the Free Software Foundation; either
;  //  version 2 of the License, or (at your option) any later version.
;  //
;  //  This library is distributed in the hope that it will be useful,
;  //  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  //  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;  //  Lesser General Public License for more details.
;  //
;  //  You should have received a copy of the GNU Lesser General Public
;  //  License along with this library; if not, write to the Free Software
;  //  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;  //
;  // ============================================================================================
;  //
;  //  This VGA Bios is specific to the plex86/bochs Emulated VGA card.
;  //  You can NOT drive any physical vga card with it.
;  //
;  // ============================================================================================
;  //
;  //  This file contains code ripped from :
;  //   - rombios.c of plex86
;  //
;  //  This VGA Bios contains fonts from :
;  //   - fntcol16.zip (c) by Joseph Gil avalable at :
;  //      ftp://ftp.simtel.net/pub/simtelnet/msdos/screen/fntcol16.zip
;  //     These fonts are public domain
;  //
;  //  This VGA Bios is based on information taken from :
;  //   - Kevin Lawton's vga card emulation for bochs/plex86
;  //   - Ralf Brown's interrupts list available at http://www.cs.cmu.edu/afs/cs/user/ralf/pub/WWW/files.html
;  //   - Finn Thogersons' VGADOC4b available at http://home.worldonline.dk/~finth/
;  //   - Michael Abrash's Graphics Programming Black Book
;  //   - Francois Gervais' book "programmation des cartes graphiques cga-ega-vga" edited by sybex
;  //   - DOSEMU 1.0.1 source code for several tables values and formulas
;  //
;  // Thanks for patches, comments and ideas to :
;  //   - techt@pikeonline.net
;  //
;  // ============================================================================================

;
; Source file: vbe.c
;
;  // ============================================================================================
;  //
;  //  Copyright (C) 2002 Jeroen Janssen
;  //
;  //  This library is free software; you can redistribute it and/or
;  //  modify it under the terms of the GNU Lesser General Public
;  //  License as published by the Free Software Foundation; either
;  //  version 2 of the License, or (at your option) any later version.
;  //
;  //  This library is distributed in the hope that it will be useful,
;  //  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  //  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;  //  Lesser General Public License for more details.
;  //
;  //  You should have received a copy of the GNU Lesser General Public
;  //  License along with this library; if not, write to the Free Software
;  //  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;  //
;  // ============================================================================================
;  //
;  //  This VBE is part of the VGA Bios specific to the plex86/bochs Emulated VGA card.
;  //  You can NOT drive any physical vga card with it.
;  //
;  // ============================================================================================
;  //
;  //  This VBE Bios is based on information taken from :
;  //   - VESA BIOS EXTENSION (VBE) Core Functions Standard Version 3.0 located at www.vesa.org
;  //
;  // ============================================================================================

;
; Oracle LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
; other than GPL or LGPL is available it will apply instead, Oracle elects to use only
; the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
; a choice of LGPL license versions is made available with the language indicating
; that LGPLv2 or any later version may be used, or where a choice of which version
; of the LGPL is applied is otherwise unspecified.
;





section VGAROM progbits vstart=0x0 align=1 ; size=0x8fa class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x8fa -> off=0x28 cb=0000000000000548 uValue=00000000000c0028 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0ebh, 01dh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h, 00eh, 01fh, 0fch, 0e9h, 03dh, 00ah
vgabios_int10_handler:                       ; 0xc0028 LB 0x548
    pushfw                                    ; 9c                          ; 0xc0028 vgarom.asm:91
    cmp ah, 00fh                              ; 80 fc 0f                    ; 0xc0029 vgarom.asm:104
    jne short 00034h                          ; 75 06                       ; 0xc002c vgarom.asm:105
    call 0017dh                               ; e8 4c 01                    ; 0xc002e vgarom.asm:106
    jmp near 000edh                           ; e9 b9 00                    ; 0xc0031 vgarom.asm:107
    cmp ah, 01ah                              ; 80 fc 1a                    ; 0xc0034 vgarom.asm:109
    jne short 0003fh                          ; 75 06                       ; 0xc0037 vgarom.asm:110
    call 00532h                               ; e8 f6 04                    ; 0xc0039 vgarom.asm:111
    jmp near 000edh                           ; e9 ae 00                    ; 0xc003c vgarom.asm:112
    cmp ah, 00bh                              ; 80 fc 0b                    ; 0xc003f vgarom.asm:114
    jne short 0004ah                          ; 75 06                       ; 0xc0042 vgarom.asm:115
    call 000efh                               ; e8 a8 00                    ; 0xc0044 vgarom.asm:116
    jmp near 000edh                           ; e9 a3 00                    ; 0xc0047 vgarom.asm:117
    cmp ax, 01103h                            ; 3d 03 11                    ; 0xc004a vgarom.asm:119
    jne short 00055h                          ; 75 06                       ; 0xc004d vgarom.asm:120
    call 00429h                               ; e8 d7 03                    ; 0xc004f vgarom.asm:121
    jmp near 000edh                           ; e9 98 00                    ; 0xc0052 vgarom.asm:122
    cmp ah, 012h                              ; 80 fc 12                    ; 0xc0055 vgarom.asm:124
    jne short 00097h                          ; 75 3d                       ; 0xc0058 vgarom.asm:125
    cmp bl, 010h                              ; 80 fb 10                    ; 0xc005a vgarom.asm:126
    jne short 00065h                          ; 75 06                       ; 0xc005d vgarom.asm:127
    call 00436h                               ; e8 d4 03                    ; 0xc005f vgarom.asm:128
    jmp near 000edh                           ; e9 88 00                    ; 0xc0062 vgarom.asm:129
    cmp bl, 030h                              ; 80 fb 30                    ; 0xc0065 vgarom.asm:131
    jne short 0006fh                          ; 75 05                       ; 0xc0068 vgarom.asm:132
    call 00459h                               ; e8 ec 03                    ; 0xc006a vgarom.asm:133
    jmp short 000edh                          ; eb 7e                       ; 0xc006d vgarom.asm:134
    cmp bl, 031h                              ; 80 fb 31                    ; 0xc006f vgarom.asm:136
    jne short 00079h                          ; 75 05                       ; 0xc0072 vgarom.asm:137
    call 004ach                               ; e8 35 04                    ; 0xc0074 vgarom.asm:138
    jmp short 000edh                          ; eb 74                       ; 0xc0077 vgarom.asm:139
    cmp bl, 032h                              ; 80 fb 32                    ; 0xc0079 vgarom.asm:141
    jne short 00083h                          ; 75 05                       ; 0xc007c vgarom.asm:142
    call 004ceh                               ; e8 4d 04                    ; 0xc007e vgarom.asm:143
    jmp short 000edh                          ; eb 6a                       ; 0xc0081 vgarom.asm:144
    cmp bl, 033h                              ; 80 fb 33                    ; 0xc0083 vgarom.asm:146
    jne short 0008dh                          ; 75 05                       ; 0xc0086 vgarom.asm:147
    call 004ech                               ; e8 61 04                    ; 0xc0088 vgarom.asm:148
    jmp short 000edh                          ; eb 60                       ; 0xc008b vgarom.asm:149
    cmp bl, 034h                              ; 80 fb 34                    ; 0xc008d vgarom.asm:151
    jne short 000e1h                          ; 75 4f                       ; 0xc0090 vgarom.asm:152
    call 00510h                               ; e8 7b 04                    ; 0xc0092 vgarom.asm:153
    jmp short 000edh                          ; eb 56                       ; 0xc0095 vgarom.asm:154
    cmp ax, 0101bh                            ; 3d 1b 10                    ; 0xc0097 vgarom.asm:156
    je short 000e1h                           ; 74 45                       ; 0xc009a vgarom.asm:157
    cmp ah, 010h                              ; 80 fc 10                    ; 0xc009c vgarom.asm:158
    jne short 000a6h                          ; 75 05                       ; 0xc009f vgarom.asm:162
    call 001a4h                               ; e8 00 01                    ; 0xc00a1 vgarom.asm:164
    jmp short 000edh                          ; eb 47                       ; 0xc00a4 vgarom.asm:165
    cmp ah, 04fh                              ; 80 fc 4f                    ; 0xc00a6 vgarom.asm:168
    jne short 000e1h                          ; 75 36                       ; 0xc00a9 vgarom.asm:169
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc00ab vgarom.asm:170
    jne short 000b4h                          ; 75 05                       ; 0xc00ad vgarom.asm:171
    call 007d2h                               ; e8 20 07                    ; 0xc00af vgarom.asm:172
    jmp short 000edh                          ; eb 39                       ; 0xc00b2 vgarom.asm:173
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc00b4 vgarom.asm:175
    jne short 000bdh                          ; 75 05                       ; 0xc00b6 vgarom.asm:176
    call 007f7h                               ; e8 3c 07                    ; 0xc00b8 vgarom.asm:177
    jmp short 000edh                          ; eb 30                       ; 0xc00bb vgarom.asm:178
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc00bd vgarom.asm:180
    jne short 000c6h                          ; 75 05                       ; 0xc00bf vgarom.asm:181
    call 00824h                               ; e8 60 07                    ; 0xc00c1 vgarom.asm:182
    jmp short 000edh                          ; eb 27                       ; 0xc00c4 vgarom.asm:183
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc00c6 vgarom.asm:185
    jne short 000cfh                          ; 75 05                       ; 0xc00c8 vgarom.asm:186
    call 00858h                               ; e8 8b 07                    ; 0xc00ca vgarom.asm:187
    jmp short 000edh                          ; eb 1e                       ; 0xc00cd vgarom.asm:188
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc00cf vgarom.asm:190
    jne short 000d8h                          ; 75 05                       ; 0xc00d1 vgarom.asm:191
    call 0088fh                               ; e8 b9 07                    ; 0xc00d3 vgarom.asm:192
    jmp short 000edh                          ; eb 15                       ; 0xc00d6 vgarom.asm:193
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc00d8 vgarom.asm:195
    jne short 000e1h                          ; 75 05                       ; 0xc00da vgarom.asm:196
    call 008e6h                               ; e8 07 08                    ; 0xc00dc vgarom.asm:197
    jmp short 000edh                          ; eb 0c                       ; 0xc00df vgarom.asm:198
    push ES                                   ; 06                          ; 0xc00e1 vgarom.asm:202
    push DS                                   ; 1e                          ; 0xc00e2 vgarom.asm:203
    pushaw                                    ; 60                          ; 0xc00e3 vgarom.asm:107
    push CS                                   ; 0e                          ; 0xc00e4 vgarom.asm:207
    pop DS                                    ; 1f                          ; 0xc00e5 vgarom.asm:208
    cld                                       ; fc                          ; 0xc00e6 vgarom.asm:209
    call 03982h                               ; e8 98 38                    ; 0xc00e7 vgarom.asm:210
    popaw                                     ; 61                          ; 0xc00ea vgarom.asm:124
    pop DS                                    ; 1f                          ; 0xc00eb vgarom.asm:213
    pop ES                                    ; 07                          ; 0xc00ec vgarom.asm:214
    popfw                                     ; 9d                          ; 0xc00ed vgarom.asm:216
    iret                                      ; cf                          ; 0xc00ee vgarom.asm:217
    cmp bh, 000h                              ; 80 ff 00                    ; 0xc00ef vgarom.asm:222
    je short 000fah                           ; 74 06                       ; 0xc00f2 vgarom.asm:223
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc00f4 vgarom.asm:224
    je short 0014bh                           ; 74 52                       ; 0xc00f7 vgarom.asm:225
    retn                                      ; c3                          ; 0xc00f9 vgarom.asm:229
    push ax                                   ; 50                          ; 0xc00fa vgarom.asm:231
    push bx                                   ; 53                          ; 0xc00fb vgarom.asm:232
    push cx                                   ; 51                          ; 0xc00fc vgarom.asm:233
    push dx                                   ; 52                          ; 0xc00fd vgarom.asm:234
    push DS                                   ; 1e                          ; 0xc00fe vgarom.asm:235
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc00ff vgarom.asm:236
    mov ds, dx                                ; 8e da                       ; 0xc0102 vgarom.asm:237
    mov dx, 003dah                            ; ba da 03                    ; 0xc0104 vgarom.asm:238
    in AL, DX                                 ; ec                          ; 0xc0107 vgarom.asm:239
    cmp byte [word 00049h], 003h              ; 80 3e 49 00 03              ; 0xc0108 vgarom.asm:240
    jbe short 0013eh                          ; 76 2f                       ; 0xc010d vgarom.asm:241
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc010f vgarom.asm:242
    mov AL, strict byte 000h                  ; b0 00                       ; 0xc0112 vgarom.asm:243
    out DX, AL                                ; ee                          ; 0xc0114 vgarom.asm:244
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0115 vgarom.asm:245
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc0117 vgarom.asm:246
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0119 vgarom.asm:247
    je short 0011fh                           ; 74 02                       ; 0xc011b vgarom.asm:248
    add AL, strict byte 008h                  ; 04 08                       ; 0xc011d vgarom.asm:249
    out DX, AL                                ; ee                          ; 0xc011f vgarom.asm:251
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc0120 vgarom.asm:252
    and bl, 010h                              ; 80 e3 10                    ; 0xc0122 vgarom.asm:253
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0125 vgarom.asm:255
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0128 vgarom.asm:256
    out DX, AL                                ; ee                          ; 0xc012a vgarom.asm:257
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc012b vgarom.asm:258
    in AL, DX                                 ; ec                          ; 0xc012e vgarom.asm:259
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc012f vgarom.asm:260
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0131 vgarom.asm:261
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0133 vgarom.asm:262
    out DX, AL                                ; ee                          ; 0xc0136 vgarom.asm:263
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0137 vgarom.asm:264
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0139 vgarom.asm:265
    jne short 00125h                          ; 75 e7                       ; 0xc013c vgarom.asm:266
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc013e vgarom.asm:268
    out DX, AL                                ; ee                          ; 0xc0140 vgarom.asm:269
    mov dx, 003dah                            ; ba da 03                    ; 0xc0141 vgarom.asm:271
    in AL, DX                                 ; ec                          ; 0xc0144 vgarom.asm:272
    pop DS                                    ; 1f                          ; 0xc0145 vgarom.asm:274
    pop dx                                    ; 5a                          ; 0xc0146 vgarom.asm:275
    pop cx                                    ; 59                          ; 0xc0147 vgarom.asm:276
    pop bx                                    ; 5b                          ; 0xc0148 vgarom.asm:277
    pop ax                                    ; 58                          ; 0xc0149 vgarom.asm:278
    retn                                      ; c3                          ; 0xc014a vgarom.asm:279
    push ax                                   ; 50                          ; 0xc014b vgarom.asm:281
    push bx                                   ; 53                          ; 0xc014c vgarom.asm:282
    push cx                                   ; 51                          ; 0xc014d vgarom.asm:283
    push dx                                   ; 52                          ; 0xc014e vgarom.asm:284
    mov dx, 003dah                            ; ba da 03                    ; 0xc014f vgarom.asm:285
    in AL, DX                                 ; ec                          ; 0xc0152 vgarom.asm:286
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc0153 vgarom.asm:287
    and bl, 001h                              ; 80 e3 01                    ; 0xc0155 vgarom.asm:288
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0158 vgarom.asm:290
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc015b vgarom.asm:291
    out DX, AL                                ; ee                          ; 0xc015d vgarom.asm:292
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc015e vgarom.asm:293
    in AL, DX                                 ; ec                          ; 0xc0161 vgarom.asm:294
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc0162 vgarom.asm:295
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0164 vgarom.asm:296
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0166 vgarom.asm:297
    out DX, AL                                ; ee                          ; 0xc0169 vgarom.asm:298
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc016a vgarom.asm:299
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc016c vgarom.asm:300
    jne short 00158h                          ; 75 e7                       ; 0xc016f vgarom.asm:301
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0171 vgarom.asm:302
    out DX, AL                                ; ee                          ; 0xc0173 vgarom.asm:303
    mov dx, 003dah                            ; ba da 03                    ; 0xc0174 vgarom.asm:305
    in AL, DX                                 ; ec                          ; 0xc0177 vgarom.asm:306
    pop dx                                    ; 5a                          ; 0xc0178 vgarom.asm:308
    pop cx                                    ; 59                          ; 0xc0179 vgarom.asm:309
    pop bx                                    ; 5b                          ; 0xc017a vgarom.asm:310
    pop ax                                    ; 58                          ; 0xc017b vgarom.asm:311
    retn                                      ; c3                          ; 0xc017c vgarom.asm:312
    push DS                                   ; 1e                          ; 0xc017d vgarom.asm:317
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc017e vgarom.asm:318
    mov ds, ax                                ; 8e d8                       ; 0xc0181 vgarom.asm:319
    push bx                                   ; 53                          ; 0xc0183 vgarom.asm:320
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc0184 vgarom.asm:321
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0187 vgarom.asm:322
    pop bx                                    ; 5b                          ; 0xc0189 vgarom.asm:323
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc018a vgarom.asm:324
    push bx                                   ; 53                          ; 0xc018c vgarom.asm:325
    mov bx, 00087h                            ; bb 87 00                    ; 0xc018d vgarom.asm:326
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc0190 vgarom.asm:327
    and ah, 080h                              ; 80 e4 80                    ; 0xc0192 vgarom.asm:328
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0195 vgarom.asm:329
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0198 vgarom.asm:330
    db  00ah, 0c4h
    ; or al, ah                                 ; 0a c4                     ; 0xc019a vgarom.asm:331
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc019c vgarom.asm:332
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc019f vgarom.asm:333
    pop bx                                    ; 5b                          ; 0xc01a1 vgarom.asm:334
    pop DS                                    ; 1f                          ; 0xc01a2 vgarom.asm:335
    retn                                      ; c3                          ; 0xc01a3 vgarom.asm:336
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc01a4 vgarom.asm:341
    jne short 001aah                          ; 75 02                       ; 0xc01a6 vgarom.asm:342
    jmp short 0020bh                          ; eb 61                       ; 0xc01a8 vgarom.asm:343
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc01aa vgarom.asm:345
    jne short 001b0h                          ; 75 02                       ; 0xc01ac vgarom.asm:346
    jmp short 00229h                          ; eb 79                       ; 0xc01ae vgarom.asm:347
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc01b0 vgarom.asm:349
    jne short 001b6h                          ; 75 02                       ; 0xc01b2 vgarom.asm:350
    jmp short 00231h                          ; eb 7b                       ; 0xc01b4 vgarom.asm:351
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc01b6 vgarom.asm:353
    jne short 001bdh                          ; 75 03                       ; 0xc01b8 vgarom.asm:354
    jmp near 00262h                           ; e9 a5 00                    ; 0xc01ba vgarom.asm:355
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc01bd vgarom.asm:357
    jne short 001c4h                          ; 75 03                       ; 0xc01bf vgarom.asm:358
    jmp near 0028ch                           ; e9 c8 00                    ; 0xc01c1 vgarom.asm:359
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc01c4 vgarom.asm:361
    jne short 001cbh                          ; 75 03                       ; 0xc01c6 vgarom.asm:362
    jmp near 002b4h                           ; e9 e9 00                    ; 0xc01c8 vgarom.asm:363
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc01cb vgarom.asm:365
    jne short 001d2h                          ; 75 03                       ; 0xc01cd vgarom.asm:366
    jmp near 002c2h                           ; e9 f0 00                    ; 0xc01cf vgarom.asm:367
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc01d2 vgarom.asm:369
    jne short 001d9h                          ; 75 03                       ; 0xc01d4 vgarom.asm:370
    jmp near 00307h                           ; e9 2e 01                    ; 0xc01d6 vgarom.asm:371
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc01d9 vgarom.asm:373
    jne short 001e0h                          ; 75 03                       ; 0xc01db vgarom.asm:374
    jmp near 00320h                           ; e9 40 01                    ; 0xc01dd vgarom.asm:375
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc01e0 vgarom.asm:377
    jne short 001e7h                          ; 75 03                       ; 0xc01e2 vgarom.asm:378
    jmp near 00348h                           ; e9 61 01                    ; 0xc01e4 vgarom.asm:379
    cmp AL, strict byte 015h                  ; 3c 15                       ; 0xc01e7 vgarom.asm:381
    jne short 001eeh                          ; 75 03                       ; 0xc01e9 vgarom.asm:382
    jmp near 0038fh                           ; e9 a1 01                    ; 0xc01eb vgarom.asm:383
    cmp AL, strict byte 017h                  ; 3c 17                       ; 0xc01ee vgarom.asm:385
    jne short 001f5h                          ; 75 03                       ; 0xc01f0 vgarom.asm:386
    jmp near 003aah                           ; e9 b5 01                    ; 0xc01f2 vgarom.asm:387
    cmp AL, strict byte 018h                  ; 3c 18                       ; 0xc01f5 vgarom.asm:389
    jne short 001fch                          ; 75 03                       ; 0xc01f7 vgarom.asm:390
    jmp near 003d2h                           ; e9 d6 01                    ; 0xc01f9 vgarom.asm:391
    cmp AL, strict byte 019h                  ; 3c 19                       ; 0xc01fc vgarom.asm:393
    jne short 00203h                          ; 75 03                       ; 0xc01fe vgarom.asm:394
    jmp near 003ddh                           ; e9 da 01                    ; 0xc0200 vgarom.asm:395
    cmp AL, strict byte 01ah                  ; 3c 1a                       ; 0xc0203 vgarom.asm:397
    jne short 0020ah                          ; 75 03                       ; 0xc0205 vgarom.asm:398
    jmp near 003e8h                           ; e9 de 01                    ; 0xc0207 vgarom.asm:399
    retn                                      ; c3                          ; 0xc020a vgarom.asm:404
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc020b vgarom.asm:407
    jnbe short 00228h                         ; 77 18                       ; 0xc020e vgarom.asm:408
    push ax                                   ; 50                          ; 0xc0210 vgarom.asm:409
    push dx                                   ; 52                          ; 0xc0211 vgarom.asm:410
    mov dx, 003dah                            ; ba da 03                    ; 0xc0212 vgarom.asm:411
    in AL, DX                                 ; ec                          ; 0xc0215 vgarom.asm:412
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0216 vgarom.asm:413
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0219 vgarom.asm:414
    out DX, AL                                ; ee                          ; 0xc021b vgarom.asm:415
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc021c vgarom.asm:416
    out DX, AL                                ; ee                          ; 0xc021e vgarom.asm:417
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc021f vgarom.asm:418
    out DX, AL                                ; ee                          ; 0xc0221 vgarom.asm:419
    mov dx, 003dah                            ; ba da 03                    ; 0xc0222 vgarom.asm:421
    in AL, DX                                 ; ec                          ; 0xc0225 vgarom.asm:422
    pop dx                                    ; 5a                          ; 0xc0226 vgarom.asm:424
    pop ax                                    ; 58                          ; 0xc0227 vgarom.asm:425
    retn                                      ; c3                          ; 0xc0228 vgarom.asm:427
    push bx                                   ; 53                          ; 0xc0229 vgarom.asm:432
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc022a vgarom.asm:433
    call 0020bh                               ; e8 dc ff                    ; 0xc022c vgarom.asm:434
    pop bx                                    ; 5b                          ; 0xc022f vgarom.asm:435
    retn                                      ; c3                          ; 0xc0230 vgarom.asm:436
    push ax                                   ; 50                          ; 0xc0231 vgarom.asm:441
    push bx                                   ; 53                          ; 0xc0232 vgarom.asm:442
    push cx                                   ; 51                          ; 0xc0233 vgarom.asm:443
    push dx                                   ; 52                          ; 0xc0234 vgarom.asm:444
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0235 vgarom.asm:445
    mov dx, 003dah                            ; ba da 03                    ; 0xc0237 vgarom.asm:446
    in AL, DX                                 ; ec                          ; 0xc023a vgarom.asm:447
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc023b vgarom.asm:448
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc023d vgarom.asm:449
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0240 vgarom.asm:451
    out DX, AL                                ; ee                          ; 0xc0242 vgarom.asm:452
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0243 vgarom.asm:453
    out DX, AL                                ; ee                          ; 0xc0246 vgarom.asm:454
    inc bx                                    ; 43                          ; 0xc0247 vgarom.asm:455
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0248 vgarom.asm:456
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc024a vgarom.asm:457
    jne short 00240h                          ; 75 f1                       ; 0xc024d vgarom.asm:458
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc024f vgarom.asm:459
    out DX, AL                                ; ee                          ; 0xc0251 vgarom.asm:460
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0252 vgarom.asm:461
    out DX, AL                                ; ee                          ; 0xc0255 vgarom.asm:462
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0256 vgarom.asm:463
    out DX, AL                                ; ee                          ; 0xc0258 vgarom.asm:464
    mov dx, 003dah                            ; ba da 03                    ; 0xc0259 vgarom.asm:466
    in AL, DX                                 ; ec                          ; 0xc025c vgarom.asm:467
    pop dx                                    ; 5a                          ; 0xc025d vgarom.asm:469
    pop cx                                    ; 59                          ; 0xc025e vgarom.asm:470
    pop bx                                    ; 5b                          ; 0xc025f vgarom.asm:471
    pop ax                                    ; 58                          ; 0xc0260 vgarom.asm:472
    retn                                      ; c3                          ; 0xc0261 vgarom.asm:473
    push ax                                   ; 50                          ; 0xc0262 vgarom.asm:478
    push bx                                   ; 53                          ; 0xc0263 vgarom.asm:479
    push dx                                   ; 52                          ; 0xc0264 vgarom.asm:480
    mov dx, 003dah                            ; ba da 03                    ; 0xc0265 vgarom.asm:481
    in AL, DX                                 ; ec                          ; 0xc0268 vgarom.asm:482
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0269 vgarom.asm:483
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc026c vgarom.asm:484
    out DX, AL                                ; ee                          ; 0xc026e vgarom.asm:485
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc026f vgarom.asm:486
    in AL, DX                                 ; ec                          ; 0xc0272 vgarom.asm:487
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc0273 vgarom.asm:488
    and bl, 001h                              ; 80 e3 01                    ; 0xc0275 vgarom.asm:489
    sal bl, 003h                              ; c0 e3 03                    ; 0xc0278 vgarom.asm:491
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc027b vgarom.asm:497
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc027d vgarom.asm:498
    out DX, AL                                ; ee                          ; 0xc0280 vgarom.asm:499
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0281 vgarom.asm:500
    out DX, AL                                ; ee                          ; 0xc0283 vgarom.asm:501
    mov dx, 003dah                            ; ba da 03                    ; 0xc0284 vgarom.asm:503
    in AL, DX                                 ; ec                          ; 0xc0287 vgarom.asm:504
    pop dx                                    ; 5a                          ; 0xc0288 vgarom.asm:506
    pop bx                                    ; 5b                          ; 0xc0289 vgarom.asm:507
    pop ax                                    ; 58                          ; 0xc028a vgarom.asm:508
    retn                                      ; c3                          ; 0xc028b vgarom.asm:509
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc028c vgarom.asm:514
    jnbe short 002b3h                         ; 77 22                       ; 0xc028f vgarom.asm:515
    push ax                                   ; 50                          ; 0xc0291 vgarom.asm:516
    push dx                                   ; 52                          ; 0xc0292 vgarom.asm:517
    mov dx, 003dah                            ; ba da 03                    ; 0xc0293 vgarom.asm:518
    in AL, DX                                 ; ec                          ; 0xc0296 vgarom.asm:519
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0297 vgarom.asm:520
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc029a vgarom.asm:521
    out DX, AL                                ; ee                          ; 0xc029c vgarom.asm:522
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc029d vgarom.asm:523
    in AL, DX                                 ; ec                          ; 0xc02a0 vgarom.asm:524
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02a1 vgarom.asm:525
    mov dx, 003dah                            ; ba da 03                    ; 0xc02a3 vgarom.asm:526
    in AL, DX                                 ; ec                          ; 0xc02a6 vgarom.asm:527
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02a7 vgarom.asm:528
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02aa vgarom.asm:529
    out DX, AL                                ; ee                          ; 0xc02ac vgarom.asm:530
    mov dx, 003dah                            ; ba da 03                    ; 0xc02ad vgarom.asm:532
    in AL, DX                                 ; ec                          ; 0xc02b0 vgarom.asm:533
    pop dx                                    ; 5a                          ; 0xc02b1 vgarom.asm:535
    pop ax                                    ; 58                          ; 0xc02b2 vgarom.asm:536
    retn                                      ; c3                          ; 0xc02b3 vgarom.asm:538
    push ax                                   ; 50                          ; 0xc02b4 vgarom.asm:543
    push bx                                   ; 53                          ; 0xc02b5 vgarom.asm:544
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc02b6 vgarom.asm:545
    call 0028ch                               ; e8 d1 ff                    ; 0xc02b8 vgarom.asm:546
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc02bb vgarom.asm:547
    pop bx                                    ; 5b                          ; 0xc02bd vgarom.asm:548
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02be vgarom.asm:549
    pop ax                                    ; 58                          ; 0xc02c0 vgarom.asm:550
    retn                                      ; c3                          ; 0xc02c1 vgarom.asm:551
    push ax                                   ; 50                          ; 0xc02c2 vgarom.asm:556
    push bx                                   ; 53                          ; 0xc02c3 vgarom.asm:557
    push cx                                   ; 51                          ; 0xc02c4 vgarom.asm:558
    push dx                                   ; 52                          ; 0xc02c5 vgarom.asm:559
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc02c6 vgarom.asm:560
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc02c8 vgarom.asm:561
    mov dx, 003dah                            ; ba da 03                    ; 0xc02ca vgarom.asm:563
    in AL, DX                                 ; ec                          ; 0xc02cd vgarom.asm:564
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02ce vgarom.asm:565
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc02d1 vgarom.asm:566
    out DX, AL                                ; ee                          ; 0xc02d3 vgarom.asm:567
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02d4 vgarom.asm:568
    in AL, DX                                 ; ec                          ; 0xc02d7 vgarom.asm:569
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02d8 vgarom.asm:570
    inc bx                                    ; 43                          ; 0xc02db vgarom.asm:571
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc02dc vgarom.asm:572
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc02de vgarom.asm:573
    jne short 002cah                          ; 75 e7                       ; 0xc02e1 vgarom.asm:574
    mov dx, 003dah                            ; ba da 03                    ; 0xc02e3 vgarom.asm:575
    in AL, DX                                 ; ec                          ; 0xc02e6 vgarom.asm:576
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02e7 vgarom.asm:577
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc02ea vgarom.asm:578
    out DX, AL                                ; ee                          ; 0xc02ec vgarom.asm:579
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02ed vgarom.asm:580
    in AL, DX                                 ; ec                          ; 0xc02f0 vgarom.asm:581
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02f1 vgarom.asm:582
    mov dx, 003dah                            ; ba da 03                    ; 0xc02f4 vgarom.asm:583
    in AL, DX                                 ; ec                          ; 0xc02f7 vgarom.asm:584
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02f8 vgarom.asm:585
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02fb vgarom.asm:586
    out DX, AL                                ; ee                          ; 0xc02fd vgarom.asm:587
    mov dx, 003dah                            ; ba da 03                    ; 0xc02fe vgarom.asm:589
    in AL, DX                                 ; ec                          ; 0xc0301 vgarom.asm:590
    pop dx                                    ; 5a                          ; 0xc0302 vgarom.asm:592
    pop cx                                    ; 59                          ; 0xc0303 vgarom.asm:593
    pop bx                                    ; 5b                          ; 0xc0304 vgarom.asm:594
    pop ax                                    ; 58                          ; 0xc0305 vgarom.asm:595
    retn                                      ; c3                          ; 0xc0306 vgarom.asm:596
    push ax                                   ; 50                          ; 0xc0307 vgarom.asm:601
    push dx                                   ; 52                          ; 0xc0308 vgarom.asm:602
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0309 vgarom.asm:603
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc030c vgarom.asm:604
    out DX, AL                                ; ee                          ; 0xc030e vgarom.asm:605
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc030f vgarom.asm:606
    pop ax                                    ; 58                          ; 0xc0312 vgarom.asm:607
    push ax                                   ; 50                          ; 0xc0313 vgarom.asm:608
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0314 vgarom.asm:609
    out DX, AL                                ; ee                          ; 0xc0316 vgarom.asm:610
    db  08ah, 0c5h
    ; mov al, ch                                ; 8a c5                     ; 0xc0317 vgarom.asm:611
    out DX, AL                                ; ee                          ; 0xc0319 vgarom.asm:612
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc031a vgarom.asm:613
    out DX, AL                                ; ee                          ; 0xc031c vgarom.asm:614
    pop dx                                    ; 5a                          ; 0xc031d vgarom.asm:615
    pop ax                                    ; 58                          ; 0xc031e vgarom.asm:616
    retn                                      ; c3                          ; 0xc031f vgarom.asm:617
    push ax                                   ; 50                          ; 0xc0320 vgarom.asm:622
    push bx                                   ; 53                          ; 0xc0321 vgarom.asm:623
    push cx                                   ; 51                          ; 0xc0322 vgarom.asm:624
    push dx                                   ; 52                          ; 0xc0323 vgarom.asm:625
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0324 vgarom.asm:626
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0327 vgarom.asm:627
    out DX, AL                                ; ee                          ; 0xc0329 vgarom.asm:628
    pop dx                                    ; 5a                          ; 0xc032a vgarom.asm:629
    push dx                                   ; 52                          ; 0xc032b vgarom.asm:630
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc032c vgarom.asm:631
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc032e vgarom.asm:632
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0331 vgarom.asm:634
    out DX, AL                                ; ee                          ; 0xc0334 vgarom.asm:635
    inc bx                                    ; 43                          ; 0xc0335 vgarom.asm:636
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0336 vgarom.asm:637
    out DX, AL                                ; ee                          ; 0xc0339 vgarom.asm:638
    inc bx                                    ; 43                          ; 0xc033a vgarom.asm:639
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc033b vgarom.asm:640
    out DX, AL                                ; ee                          ; 0xc033e vgarom.asm:641
    inc bx                                    ; 43                          ; 0xc033f vgarom.asm:642
    dec cx                                    ; 49                          ; 0xc0340 vgarom.asm:643
    jne short 00331h                          ; 75 ee                       ; 0xc0341 vgarom.asm:644
    pop dx                                    ; 5a                          ; 0xc0343 vgarom.asm:645
    pop cx                                    ; 59                          ; 0xc0344 vgarom.asm:646
    pop bx                                    ; 5b                          ; 0xc0345 vgarom.asm:647
    pop ax                                    ; 58                          ; 0xc0346 vgarom.asm:648
    retn                                      ; c3                          ; 0xc0347 vgarom.asm:649
    push ax                                   ; 50                          ; 0xc0348 vgarom.asm:654
    push bx                                   ; 53                          ; 0xc0349 vgarom.asm:655
    push dx                                   ; 52                          ; 0xc034a vgarom.asm:656
    mov dx, 003dah                            ; ba da 03                    ; 0xc034b vgarom.asm:657
    in AL, DX                                 ; ec                          ; 0xc034e vgarom.asm:658
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc034f vgarom.asm:659
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0352 vgarom.asm:660
    out DX, AL                                ; ee                          ; 0xc0354 vgarom.asm:661
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0355 vgarom.asm:662
    in AL, DX                                 ; ec                          ; 0xc0358 vgarom.asm:663
    and bl, 001h                              ; 80 e3 01                    ; 0xc0359 vgarom.asm:664
    jne short 0036bh                          ; 75 0d                       ; 0xc035c vgarom.asm:665
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc035e vgarom.asm:666
    sal bh, 007h                              ; c0 e7 07                    ; 0xc0360 vgarom.asm:668
    db  00ah, 0c7h
    ; or al, bh                                 ; 0a c7                     ; 0xc0363 vgarom.asm:678
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0365 vgarom.asm:679
    out DX, AL                                ; ee                          ; 0xc0368 vgarom.asm:680
    jmp short 00384h                          ; eb 19                       ; 0xc0369 vgarom.asm:681
    push ax                                   ; 50                          ; 0xc036b vgarom.asm:683
    mov dx, 003dah                            ; ba da 03                    ; 0xc036c vgarom.asm:684
    in AL, DX                                 ; ec                          ; 0xc036f vgarom.asm:685
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0370 vgarom.asm:686
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0373 vgarom.asm:687
    out DX, AL                                ; ee                          ; 0xc0375 vgarom.asm:688
    pop ax                                    ; 58                          ; 0xc0376 vgarom.asm:689
    and AL, strict byte 080h                  ; 24 80                       ; 0xc0377 vgarom.asm:690
    jne short 0037eh                          ; 75 03                       ; 0xc0379 vgarom.asm:691
    sal bh, 002h                              ; c0 e7 02                    ; 0xc037b vgarom.asm:693
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc037e vgarom.asm:699
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc0381 vgarom.asm:700
    out DX, AL                                ; ee                          ; 0xc0383 vgarom.asm:701
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0384 vgarom.asm:703
    out DX, AL                                ; ee                          ; 0xc0386 vgarom.asm:704
    mov dx, 003dah                            ; ba da 03                    ; 0xc0387 vgarom.asm:706
    in AL, DX                                 ; ec                          ; 0xc038a vgarom.asm:707
    pop dx                                    ; 5a                          ; 0xc038b vgarom.asm:709
    pop bx                                    ; 5b                          ; 0xc038c vgarom.asm:710
    pop ax                                    ; 58                          ; 0xc038d vgarom.asm:711
    retn                                      ; c3                          ; 0xc038e vgarom.asm:712
    push ax                                   ; 50                          ; 0xc038f vgarom.asm:717
    push dx                                   ; 52                          ; 0xc0390 vgarom.asm:718
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc0391 vgarom.asm:719
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0394 vgarom.asm:720
    out DX, AL                                ; ee                          ; 0xc0396 vgarom.asm:721
    pop ax                                    ; 58                          ; 0xc0397 vgarom.asm:722
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc0398 vgarom.asm:723
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc039a vgarom.asm:724
    in AL, DX                                 ; ec                          ; 0xc039d vgarom.asm:725
    xchg al, ah                               ; 86 e0                       ; 0xc039e vgarom.asm:726
    push ax                                   ; 50                          ; 0xc03a0 vgarom.asm:727
    in AL, DX                                 ; ec                          ; 0xc03a1 vgarom.asm:728
    db  08ah, 0e8h
    ; mov ch, al                                ; 8a e8                     ; 0xc03a2 vgarom.asm:729
    in AL, DX                                 ; ec                          ; 0xc03a4 vgarom.asm:730
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8                     ; 0xc03a5 vgarom.asm:731
    pop dx                                    ; 5a                          ; 0xc03a7 vgarom.asm:732
    pop ax                                    ; 58                          ; 0xc03a8 vgarom.asm:733
    retn                                      ; c3                          ; 0xc03a9 vgarom.asm:734
    push ax                                   ; 50                          ; 0xc03aa vgarom.asm:739
    push bx                                   ; 53                          ; 0xc03ab vgarom.asm:740
    push cx                                   ; 51                          ; 0xc03ac vgarom.asm:741
    push dx                                   ; 52                          ; 0xc03ad vgarom.asm:742
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03ae vgarom.asm:743
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03b1 vgarom.asm:744
    out DX, AL                                ; ee                          ; 0xc03b3 vgarom.asm:745
    pop dx                                    ; 5a                          ; 0xc03b4 vgarom.asm:746
    push dx                                   ; 52                          ; 0xc03b5 vgarom.asm:747
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc03b6 vgarom.asm:748
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03b8 vgarom.asm:749
    in AL, DX                                 ; ec                          ; 0xc03bb vgarom.asm:751
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03bc vgarom.asm:752
    inc bx                                    ; 43                          ; 0xc03bf vgarom.asm:753
    in AL, DX                                 ; ec                          ; 0xc03c0 vgarom.asm:754
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03c1 vgarom.asm:755
    inc bx                                    ; 43                          ; 0xc03c4 vgarom.asm:756
    in AL, DX                                 ; ec                          ; 0xc03c5 vgarom.asm:757
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03c6 vgarom.asm:758
    inc bx                                    ; 43                          ; 0xc03c9 vgarom.asm:759
    dec cx                                    ; 49                          ; 0xc03ca vgarom.asm:760
    jne short 003bbh                          ; 75 ee                       ; 0xc03cb vgarom.asm:761
    pop dx                                    ; 5a                          ; 0xc03cd vgarom.asm:762
    pop cx                                    ; 59                          ; 0xc03ce vgarom.asm:763
    pop bx                                    ; 5b                          ; 0xc03cf vgarom.asm:764
    pop ax                                    ; 58                          ; 0xc03d0 vgarom.asm:765
    retn                                      ; c3                          ; 0xc03d1 vgarom.asm:766
    push ax                                   ; 50                          ; 0xc03d2 vgarom.asm:771
    push dx                                   ; 52                          ; 0xc03d3 vgarom.asm:772
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03d4 vgarom.asm:773
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03d7 vgarom.asm:774
    out DX, AL                                ; ee                          ; 0xc03d9 vgarom.asm:775
    pop dx                                    ; 5a                          ; 0xc03da vgarom.asm:776
    pop ax                                    ; 58                          ; 0xc03db vgarom.asm:777
    retn                                      ; c3                          ; 0xc03dc vgarom.asm:778
    push ax                                   ; 50                          ; 0xc03dd vgarom.asm:783
    push dx                                   ; 52                          ; 0xc03de vgarom.asm:784
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03df vgarom.asm:785
    in AL, DX                                 ; ec                          ; 0xc03e2 vgarom.asm:786
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03e3 vgarom.asm:787
    pop dx                                    ; 5a                          ; 0xc03e5 vgarom.asm:788
    pop ax                                    ; 58                          ; 0xc03e6 vgarom.asm:789
    retn                                      ; c3                          ; 0xc03e7 vgarom.asm:790
    push ax                                   ; 50                          ; 0xc03e8 vgarom.asm:795
    push dx                                   ; 52                          ; 0xc03e9 vgarom.asm:796
    mov dx, 003dah                            ; ba da 03                    ; 0xc03ea vgarom.asm:797
    in AL, DX                                 ; ec                          ; 0xc03ed vgarom.asm:798
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc03ee vgarom.asm:799
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc03f1 vgarom.asm:800
    out DX, AL                                ; ee                          ; 0xc03f3 vgarom.asm:801
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc03f4 vgarom.asm:802
    in AL, DX                                 ; ec                          ; 0xc03f7 vgarom.asm:803
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03f8 vgarom.asm:804
    shr bl, 007h                              ; c0 eb 07                    ; 0xc03fa vgarom.asm:806
    mov dx, 003dah                            ; ba da 03                    ; 0xc03fd vgarom.asm:816
    in AL, DX                                 ; ec                          ; 0xc0400 vgarom.asm:817
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0401 vgarom.asm:818
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0404 vgarom.asm:819
    out DX, AL                                ; ee                          ; 0xc0406 vgarom.asm:820
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0407 vgarom.asm:821
    in AL, DX                                 ; ec                          ; 0xc040a vgarom.asm:822
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc040b vgarom.asm:823
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc040d vgarom.asm:824
    test bl, 001h                             ; f6 c3 01                    ; 0xc0410 vgarom.asm:825
    jne short 00418h                          ; 75 03                       ; 0xc0413 vgarom.asm:826
    shr bh, 002h                              ; c0 ef 02                    ; 0xc0415 vgarom.asm:828
    mov dx, 003dah                            ; ba da 03                    ; 0xc0418 vgarom.asm:834
    in AL, DX                                 ; ec                          ; 0xc041b vgarom.asm:835
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc041c vgarom.asm:836
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc041f vgarom.asm:837
    out DX, AL                                ; ee                          ; 0xc0421 vgarom.asm:838
    mov dx, 003dah                            ; ba da 03                    ; 0xc0422 vgarom.asm:840
    in AL, DX                                 ; ec                          ; 0xc0425 vgarom.asm:841
    pop dx                                    ; 5a                          ; 0xc0426 vgarom.asm:843
    pop ax                                    ; 58                          ; 0xc0427 vgarom.asm:844
    retn                                      ; c3                          ; 0xc0428 vgarom.asm:845
    push ax                                   ; 50                          ; 0xc0429 vgarom.asm:850
    push dx                                   ; 52                          ; 0xc042a vgarom.asm:851
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc042b vgarom.asm:852
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc042e vgarom.asm:853
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc0430 vgarom.asm:854
    out DX, ax                                ; ef                          ; 0xc0432 vgarom.asm:855
    pop dx                                    ; 5a                          ; 0xc0433 vgarom.asm:856
    pop ax                                    ; 58                          ; 0xc0434 vgarom.asm:857
    retn                                      ; c3                          ; 0xc0435 vgarom.asm:858
    push DS                                   ; 1e                          ; 0xc0436 vgarom.asm:863
    push ax                                   ; 50                          ; 0xc0437 vgarom.asm:864
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0438 vgarom.asm:865
    mov ds, ax                                ; 8e d8                       ; 0xc043b vgarom.asm:866
    db  032h, 0edh
    ; xor ch, ch                                ; 32 ed                     ; 0xc043d vgarom.asm:867
    mov bx, 00088h                            ; bb 88 00                    ; 0xc043f vgarom.asm:868
    mov cl, byte [bx]                         ; 8a 0f                       ; 0xc0442 vgarom.asm:869
    and cl, 00fh                              ; 80 e1 0f                    ; 0xc0444 vgarom.asm:870
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc0447 vgarom.asm:871
    mov ax, word [bx]                         ; 8b 07                       ; 0xc044a vgarom.asm:872
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc044c vgarom.asm:873
    cmp ax, 003b4h                            ; 3d b4 03                    ; 0xc044f vgarom.asm:874
    jne short 00456h                          ; 75 02                       ; 0xc0452 vgarom.asm:875
    mov BH, strict byte 001h                  ; b7 01                       ; 0xc0454 vgarom.asm:876
    pop ax                                    ; 58                          ; 0xc0456 vgarom.asm:878
    pop DS                                    ; 1f                          ; 0xc0457 vgarom.asm:879
    retn                                      ; c3                          ; 0xc0458 vgarom.asm:880
    push DS                                   ; 1e                          ; 0xc0459 vgarom.asm:888
    push bx                                   ; 53                          ; 0xc045a vgarom.asm:889
    push dx                                   ; 52                          ; 0xc045b vgarom.asm:890
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc045c vgarom.asm:891
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc045e vgarom.asm:892
    mov ds, ax                                ; 8e d8                       ; 0xc0461 vgarom.asm:893
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0463 vgarom.asm:894
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0466 vgarom.asm:895
    mov bx, 00088h                            ; bb 88 00                    ; 0xc0468 vgarom.asm:896
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc046b vgarom.asm:897
    cmp dl, 001h                              ; 80 fa 01                    ; 0xc046d vgarom.asm:898
    je short 00487h                           ; 74 15                       ; 0xc0470 vgarom.asm:899
    jc short 00491h                           ; 72 1d                       ; 0xc0472 vgarom.asm:900
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc0474 vgarom.asm:901
    je short 0047bh                           ; 74 02                       ; 0xc0477 vgarom.asm:902
    jmp short 004a5h                          ; eb 2a                       ; 0xc0479 vgarom.asm:912
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc047b vgarom.asm:918
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc047d vgarom.asm:919
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc047f vgarom.asm:920
    or ah, 009h                               ; 80 cc 09                    ; 0xc0482 vgarom.asm:921
    jne short 0049bh                          ; 75 14                       ; 0xc0485 vgarom.asm:922
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc0487 vgarom.asm:928
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc0489 vgarom.asm:929
    or ah, 009h                               ; 80 cc 09                    ; 0xc048c vgarom.asm:930
    jne short 0049bh                          ; 75 0a                       ; 0xc048f vgarom.asm:931
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc0491 vgarom.asm:937
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc0493 vgarom.asm:938
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc0495 vgarom.asm:939
    or ah, 008h                               ; 80 cc 08                    ; 0xc0498 vgarom.asm:940
    mov bx, 00089h                            ; bb 89 00                    ; 0xc049b vgarom.asm:942
    mov byte [bx], al                         ; 88 07                       ; 0xc049e vgarom.asm:943
    mov bx, 00088h                            ; bb 88 00                    ; 0xc04a0 vgarom.asm:944
    mov byte [bx], ah                         ; 88 27                       ; 0xc04a3 vgarom.asm:945
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04a5 vgarom.asm:947
    pop dx                                    ; 5a                          ; 0xc04a8 vgarom.asm:948
    pop bx                                    ; 5b                          ; 0xc04a9 vgarom.asm:949
    pop DS                                    ; 1f                          ; 0xc04aa vgarom.asm:950
    retn                                      ; c3                          ; 0xc04ab vgarom.asm:951
    push DS                                   ; 1e                          ; 0xc04ac vgarom.asm:960
    push bx                                   ; 53                          ; 0xc04ad vgarom.asm:961
    push dx                                   ; 52                          ; 0xc04ae vgarom.asm:962
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04af vgarom.asm:963
    and dl, 001h                              ; 80 e2 01                    ; 0xc04b1 vgarom.asm:964
    sal dl, 003h                              ; c0 e2 03                    ; 0xc04b4 vgarom.asm:966
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04b7 vgarom.asm:972
    mov ds, ax                                ; 8e d8                       ; 0xc04ba vgarom.asm:973
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04bc vgarom.asm:974
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04bf vgarom.asm:975
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc04c1 vgarom.asm:976
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc04c3 vgarom.asm:977
    mov byte [bx], al                         ; 88 07                       ; 0xc04c5 vgarom.asm:978
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04c7 vgarom.asm:979
    pop dx                                    ; 5a                          ; 0xc04ca vgarom.asm:980
    pop bx                                    ; 5b                          ; 0xc04cb vgarom.asm:981
    pop DS                                    ; 1f                          ; 0xc04cc vgarom.asm:982
    retn                                      ; c3                          ; 0xc04cd vgarom.asm:983
    push bx                                   ; 53                          ; 0xc04ce vgarom.asm:987
    push dx                                   ; 52                          ; 0xc04cf vgarom.asm:988
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc04d0 vgarom.asm:989
    and bl, 001h                              ; 80 e3 01                    ; 0xc04d2 vgarom.asm:990
    xor bl, 001h                              ; 80 f3 01                    ; 0xc04d5 vgarom.asm:991
    sal bl, 1                                 ; d0 e3                       ; 0xc04d8 vgarom.asm:992
    mov dx, 003cch                            ; ba cc 03                    ; 0xc04da vgarom.asm:993
    in AL, DX                                 ; ec                          ; 0xc04dd vgarom.asm:994
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc04de vgarom.asm:995
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc04e0 vgarom.asm:996
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc04e2 vgarom.asm:997
    out DX, AL                                ; ee                          ; 0xc04e5 vgarom.asm:998
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04e6 vgarom.asm:999
    pop dx                                    ; 5a                          ; 0xc04e9 vgarom.asm:1000
    pop bx                                    ; 5b                          ; 0xc04ea vgarom.asm:1001
    retn                                      ; c3                          ; 0xc04eb vgarom.asm:1002
    push DS                                   ; 1e                          ; 0xc04ec vgarom.asm:1006
    push bx                                   ; 53                          ; 0xc04ed vgarom.asm:1007
    push dx                                   ; 52                          ; 0xc04ee vgarom.asm:1008
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04ef vgarom.asm:1009
    and dl, 001h                              ; 80 e2 01                    ; 0xc04f1 vgarom.asm:1010
    xor dl, 001h                              ; 80 f2 01                    ; 0xc04f4 vgarom.asm:1011
    sal dl, 1                                 ; d0 e2                       ; 0xc04f7 vgarom.asm:1012
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04f9 vgarom.asm:1013
    mov ds, ax                                ; 8e d8                       ; 0xc04fc vgarom.asm:1014
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04fe vgarom.asm:1015
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0501 vgarom.asm:1016
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc0503 vgarom.asm:1017
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0505 vgarom.asm:1018
    mov byte [bx], al                         ; 88 07                       ; 0xc0507 vgarom.asm:1019
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0509 vgarom.asm:1020
    pop dx                                    ; 5a                          ; 0xc050c vgarom.asm:1021
    pop bx                                    ; 5b                          ; 0xc050d vgarom.asm:1022
    pop DS                                    ; 1f                          ; 0xc050e vgarom.asm:1023
    retn                                      ; c3                          ; 0xc050f vgarom.asm:1024
    push DS                                   ; 1e                          ; 0xc0510 vgarom.asm:1028
    push bx                                   ; 53                          ; 0xc0511 vgarom.asm:1029
    push dx                                   ; 52                          ; 0xc0512 vgarom.asm:1030
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0513 vgarom.asm:1031
    and dl, 001h                              ; 80 e2 01                    ; 0xc0515 vgarom.asm:1032
    xor dl, 001h                              ; 80 f2 01                    ; 0xc0518 vgarom.asm:1033
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc051b vgarom.asm:1034
    mov ds, ax                                ; 8e d8                       ; 0xc051e vgarom.asm:1035
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0520 vgarom.asm:1036
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0523 vgarom.asm:1037
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc0525 vgarom.asm:1038
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0527 vgarom.asm:1039
    mov byte [bx], al                         ; 88 07                       ; 0xc0529 vgarom.asm:1040
    mov ax, 01212h                            ; b8 12 12                    ; 0xc052b vgarom.asm:1041
    pop dx                                    ; 5a                          ; 0xc052e vgarom.asm:1042
    pop bx                                    ; 5b                          ; 0xc052f vgarom.asm:1043
    pop DS                                    ; 1f                          ; 0xc0530 vgarom.asm:1044
    retn                                      ; c3                          ; 0xc0531 vgarom.asm:1045
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc0532 vgarom.asm:1050
    je short 0053bh                           ; 74 05                       ; 0xc0534 vgarom.asm:1051
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc0536 vgarom.asm:1052
    je short 00550h                           ; 74 16                       ; 0xc0538 vgarom.asm:1053
    retn                                      ; c3                          ; 0xc053a vgarom.asm:1057
    push DS                                   ; 1e                          ; 0xc053b vgarom.asm:1059
    push ax                                   ; 50                          ; 0xc053c vgarom.asm:1060
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc053d vgarom.asm:1061
    mov ds, ax                                ; 8e d8                       ; 0xc0540 vgarom.asm:1062
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc0542 vgarom.asm:1063
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0545 vgarom.asm:1064
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0547 vgarom.asm:1065
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0549 vgarom.asm:1066
    pop ax                                    ; 58                          ; 0xc054b vgarom.asm:1067
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc054c vgarom.asm:1068
    pop DS                                    ; 1f                          ; 0xc054e vgarom.asm:1069
    retn                                      ; c3                          ; 0xc054f vgarom.asm:1070
    push DS                                   ; 1e                          ; 0xc0550 vgarom.asm:1072
    push ax                                   ; 50                          ; 0xc0551 vgarom.asm:1073
    push bx                                   ; 53                          ; 0xc0552 vgarom.asm:1074
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0553 vgarom.asm:1075
    mov ds, ax                                ; 8e d8                       ; 0xc0556 vgarom.asm:1076
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0558 vgarom.asm:1077
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc055a vgarom.asm:1078
    mov byte [bx], al                         ; 88 07                       ; 0xc055d vgarom.asm:1079
    pop bx                                    ; 5b                          ; 0xc055f vgarom.asm:1089
    pop ax                                    ; 58                          ; 0xc0560 vgarom.asm:1090
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0561 vgarom.asm:1091
    pop DS                                    ; 1f                          ; 0xc0563 vgarom.asm:1092
    retn                                      ; c3                          ; 0xc0564 vgarom.asm:1093
    times 0xb db 0
  ; disGetNextSymbol 0xc0570 LB 0x38a -> off=0x0 cb=0000000000000007 uValue=00000000000c0570 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0570 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0570 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0572 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0573 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0575 vberom.asm:72
    retn                                      ; c3                          ; 0xc0576 vberom.asm:73
  ; disGetNextSymbol 0xc0577 LB 0x383 -> off=0x0 cb=0000000000000040 uValue=00000000000c0577 'do_in_ax_dx'
do_in_ax_dx:                                 ; 0xc0577 LB 0x40
    in AL, DX                                 ; ec                          ; 0xc0577 vberom.asm:76
    xchg ah, al                               ; 86 c4                       ; 0xc0578 vberom.asm:77
    in AL, DX                                 ; ec                          ; 0xc057a vberom.asm:78
    retn                                      ; c3                          ; 0xc057b vberom.asm:79
    push ax                                   ; 50                          ; 0xc057c vberom.asm:90
    push dx                                   ; 52                          ; 0xc057d vberom.asm:91
    mov dx, 003dah                            ; ba da 03                    ; 0xc057e vberom.asm:92
    in AL, DX                                 ; ec                          ; 0xc0581 vberom.asm:94
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0582 vberom.asm:95
    je short 00581h                           ; 74 fb                       ; 0xc0584 vberom.asm:96
    pop dx                                    ; 5a                          ; 0xc0586 vberom.asm:97
    pop ax                                    ; 58                          ; 0xc0587 vberom.asm:98
    retn                                      ; c3                          ; 0xc0588 vberom.asm:99
    push ax                                   ; 50                          ; 0xc0589 vberom.asm:102
    push dx                                   ; 52                          ; 0xc058a vberom.asm:103
    mov dx, 003dah                            ; ba da 03                    ; 0xc058b vberom.asm:104
    in AL, DX                                 ; ec                          ; 0xc058e vberom.asm:106
    test AL, strict byte 008h                 ; a8 08                       ; 0xc058f vberom.asm:107
    jne short 0058eh                          ; 75 fb                       ; 0xc0591 vberom.asm:108
    pop dx                                    ; 5a                          ; 0xc0593 vberom.asm:109
    pop ax                                    ; 58                          ; 0xc0594 vberom.asm:110
    retn                                      ; c3                          ; 0xc0595 vberom.asm:111
    push dx                                   ; 52                          ; 0xc0596 vberom.asm:116
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0597 vberom.asm:117
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc059a vberom.asm:118
    call 00570h                               ; e8 d0 ff                    ; 0xc059d vberom.asm:119
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05a0 vberom.asm:120
    call 00577h                               ; e8 d1 ff                    ; 0xc05a3 vberom.asm:121
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc05a6 vberom.asm:122
    jbe short 005b5h                          ; 76 0b                       ; 0xc05a8 vberom.asm:123
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc05aa vberom.asm:124
    shr ah, 003h                              ; c0 ec 03                    ; 0xc05ac vberom.asm:126
    test AL, strict byte 007h                 ; a8 07                       ; 0xc05af vberom.asm:132
    je short 005b5h                           ; 74 02                       ; 0xc05b1 vberom.asm:133
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc05b3 vberom.asm:134
    pop dx                                    ; 5a                          ; 0xc05b5 vberom.asm:136
    retn                                      ; c3                          ; 0xc05b6 vberom.asm:137
  ; disGetNextSymbol 0xc05b7 LB 0x343 -> off=0x0 cb=0000000000000026 uValue=00000000000c05b7 '_dispi_get_max_bpp'
_dispi_get_max_bpp:                          ; 0xc05b7 LB 0x26
    push dx                                   ; 52                          ; 0xc05b7 vberom.asm:142
    push bx                                   ; 53                          ; 0xc05b8 vberom.asm:143
    call 005f1h                               ; e8 35 00                    ; 0xc05b9 vberom.asm:144
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc05bc vberom.asm:145
    or ax, strict byte 00002h                 ; 83 c8 02                    ; 0xc05be vberom.asm:146
    call 005ddh                               ; e8 19 00                    ; 0xc05c1 vberom.asm:147
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05c4 vberom.asm:148
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc05c7 vberom.asm:149
    call 00570h                               ; e8 a3 ff                    ; 0xc05ca vberom.asm:150
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05cd vberom.asm:151
    call 00577h                               ; e8 a4 ff                    ; 0xc05d0 vberom.asm:152
    push ax                                   ; 50                          ; 0xc05d3 vberom.asm:153
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc05d4 vberom.asm:154
    call 005ddh                               ; e8 04 00                    ; 0xc05d6 vberom.asm:155
    pop ax                                    ; 58                          ; 0xc05d9 vberom.asm:156
    pop bx                                    ; 5b                          ; 0xc05da vberom.asm:157
    pop dx                                    ; 5a                          ; 0xc05db vberom.asm:158
    retn                                      ; c3                          ; 0xc05dc vberom.asm:159
  ; disGetNextSymbol 0xc05dd LB 0x31d -> off=0x0 cb=0000000000000026 uValue=00000000000c05dd 'dispi_set_enable_'
dispi_set_enable_:                           ; 0xc05dd LB 0x26
    push dx                                   ; 52                          ; 0xc05dd vberom.asm:162
    push ax                                   ; 50                          ; 0xc05de vberom.asm:163
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05df vberom.asm:164
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc05e2 vberom.asm:165
    call 00570h                               ; e8 88 ff                    ; 0xc05e5 vberom.asm:166
    pop ax                                    ; 58                          ; 0xc05e8 vberom.asm:167
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05e9 vberom.asm:168
    call 00570h                               ; e8 81 ff                    ; 0xc05ec vberom.asm:169
    pop dx                                    ; 5a                          ; 0xc05ef vberom.asm:170
    retn                                      ; c3                          ; 0xc05f0 vberom.asm:171
    push dx                                   ; 52                          ; 0xc05f1 vberom.asm:174
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05f2 vberom.asm:175
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc05f5 vberom.asm:176
    call 00570h                               ; e8 75 ff                    ; 0xc05f8 vberom.asm:177
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05fb vberom.asm:178
    call 00577h                               ; e8 76 ff                    ; 0xc05fe vberom.asm:179
    pop dx                                    ; 5a                          ; 0xc0601 vberom.asm:180
    retn                                      ; c3                          ; 0xc0602 vberom.asm:181
  ; disGetNextSymbol 0xc0603 LB 0x2f7 -> off=0x0 cb=0000000000000026 uValue=00000000000c0603 'dispi_set_bank_'
dispi_set_bank_:                             ; 0xc0603 LB 0x26
    push dx                                   ; 52                          ; 0xc0603 vberom.asm:184
    push ax                                   ; 50                          ; 0xc0604 vberom.asm:185
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0605 vberom.asm:186
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0608 vberom.asm:187
    call 00570h                               ; e8 62 ff                    ; 0xc060b vberom.asm:188
    pop ax                                    ; 58                          ; 0xc060e vberom.asm:189
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc060f vberom.asm:190
    call 00570h                               ; e8 5b ff                    ; 0xc0612 vberom.asm:191
    pop dx                                    ; 5a                          ; 0xc0615 vberom.asm:192
    retn                                      ; c3                          ; 0xc0616 vberom.asm:193
    push dx                                   ; 52                          ; 0xc0617 vberom.asm:196
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0618 vberom.asm:197
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc061b vberom.asm:198
    call 00570h                               ; e8 4f ff                    ; 0xc061e vberom.asm:199
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0621 vberom.asm:200
    call 00577h                               ; e8 50 ff                    ; 0xc0624 vberom.asm:201
    pop dx                                    ; 5a                          ; 0xc0627 vberom.asm:202
    retn                                      ; c3                          ; 0xc0628 vberom.asm:203
  ; disGetNextSymbol 0xc0629 LB 0x2d1 -> off=0x0 cb=00000000000000a9 uValue=00000000000c0629 '_dispi_set_bank_farcall'
_dispi_set_bank_farcall:                     ; 0xc0629 LB 0xa9
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc0629 vberom.asm:206
    je short 00653h                           ; 74 24                       ; 0xc062d vberom.asm:207
    db  00bh, 0dbh
    ; or bx, bx                                 ; 0b db                     ; 0xc062f vberom.asm:208
    jne short 00665h                          ; 75 32                       ; 0xc0631 vberom.asm:209
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0633 vberom.asm:210
    push dx                                   ; 52                          ; 0xc0635 vberom.asm:211
    push ax                                   ; 50                          ; 0xc0636 vberom.asm:212
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0637 vberom.asm:213
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc063a vberom.asm:214
    call 00570h                               ; e8 30 ff                    ; 0xc063d vberom.asm:215
    pop ax                                    ; 58                          ; 0xc0640 vberom.asm:216
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0641 vberom.asm:217
    call 00570h                               ; e8 29 ff                    ; 0xc0644 vberom.asm:218
    call 00577h                               ; e8 2d ff                    ; 0xc0647 vberom.asm:219
    pop dx                                    ; 5a                          ; 0xc064a vberom.asm:220
    db  03bh, 0d0h
    ; cmp dx, ax                                ; 3b d0                     ; 0xc064b vberom.asm:221
    jne short 00665h                          ; 75 16                       ; 0xc064d vberom.asm:222
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc064f vberom.asm:223
    retf                                      ; cb                          ; 0xc0652 vberom.asm:224
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0653 vberom.asm:226
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0656 vberom.asm:227
    call 00570h                               ; e8 14 ff                    ; 0xc0659 vberom.asm:228
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc065c vberom.asm:229
    call 00577h                               ; e8 15 ff                    ; 0xc065f vberom.asm:230
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0662 vberom.asm:231
    retf                                      ; cb                          ; 0xc0664 vberom.asm:232
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0665 vberom.asm:234
    retf                                      ; cb                          ; 0xc0668 vberom.asm:235
    push dx                                   ; 52                          ; 0xc0669 vberom.asm:238
    push ax                                   ; 50                          ; 0xc066a vberom.asm:239
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc066b vberom.asm:240
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc066e vberom.asm:241
    call 00570h                               ; e8 fc fe                    ; 0xc0671 vberom.asm:242
    pop ax                                    ; 58                          ; 0xc0674 vberom.asm:243
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0675 vberom.asm:244
    call 00570h                               ; e8 f5 fe                    ; 0xc0678 vberom.asm:245
    pop dx                                    ; 5a                          ; 0xc067b vberom.asm:246
    retn                                      ; c3                          ; 0xc067c vberom.asm:247
    push dx                                   ; 52                          ; 0xc067d vberom.asm:250
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc067e vberom.asm:251
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc0681 vberom.asm:252
    call 00570h                               ; e8 e9 fe                    ; 0xc0684 vberom.asm:253
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0687 vberom.asm:254
    call 00577h                               ; e8 ea fe                    ; 0xc068a vberom.asm:255
    pop dx                                    ; 5a                          ; 0xc068d vberom.asm:256
    retn                                      ; c3                          ; 0xc068e vberom.asm:257
    push dx                                   ; 52                          ; 0xc068f vberom.asm:260
    push ax                                   ; 50                          ; 0xc0690 vberom.asm:261
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0691 vberom.asm:262
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc0694 vberom.asm:263
    call 00570h                               ; e8 d6 fe                    ; 0xc0697 vberom.asm:264
    pop ax                                    ; 58                          ; 0xc069a vberom.asm:265
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc069b vberom.asm:266
    call 00570h                               ; e8 cf fe                    ; 0xc069e vberom.asm:267
    pop dx                                    ; 5a                          ; 0xc06a1 vberom.asm:268
    retn                                      ; c3                          ; 0xc06a2 vberom.asm:269
    push dx                                   ; 52                          ; 0xc06a3 vberom.asm:272
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06a4 vberom.asm:273
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc06a7 vberom.asm:274
    call 00570h                               ; e8 c3 fe                    ; 0xc06aa vberom.asm:275
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06ad vberom.asm:276
    call 00577h                               ; e8 c4 fe                    ; 0xc06b0 vberom.asm:277
    pop dx                                    ; 5a                          ; 0xc06b3 vberom.asm:278
    retn                                      ; c3                          ; 0xc06b4 vberom.asm:279
    push ax                                   ; 50                          ; 0xc06b5 vberom.asm:282
    push bx                                   ; 53                          ; 0xc06b6 vberom.asm:283
    push dx                                   ; 52                          ; 0xc06b7 vberom.asm:284
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc06b8 vberom.asm:285
    call 00596h                               ; e8 d9 fe                    ; 0xc06ba vberom.asm:286
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc06bd vberom.asm:287
    jnbe short 006c3h                         ; 77 02                       ; 0xc06bf vberom.asm:288
    shr bx, 1                                 ; d1 eb                       ; 0xc06c1 vberom.asm:289
    shr bx, 003h                              ; c1 eb 03                    ; 0xc06c3 vberom.asm:292
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06c6 vberom.asm:298
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc06c9 vberom.asm:299
    mov AL, strict byte 013h                  ; b0 13                       ; 0xc06cb vberom.asm:300
    out DX, ax                                ; ef                          ; 0xc06cd vberom.asm:301
    pop dx                                    ; 5a                          ; 0xc06ce vberom.asm:302
    pop bx                                    ; 5b                          ; 0xc06cf vberom.asm:303
    pop ax                                    ; 58                          ; 0xc06d0 vberom.asm:304
    retn                                      ; c3                          ; 0xc06d1 vberom.asm:305
  ; disGetNextSymbol 0xc06d2 LB 0x228 -> off=0x0 cb=00000000000000ed uValue=00000000000c06d2 '_vga_compat_setup'
_vga_compat_setup:                           ; 0xc06d2 LB 0xed
    push ax                                   ; 50                          ; 0xc06d2 vberom.asm:308
    push dx                                   ; 52                          ; 0xc06d3 vberom.asm:309
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06d4 vberom.asm:312
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc06d7 vberom.asm:313
    call 00570h                               ; e8 93 fe                    ; 0xc06da vberom.asm:314
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06dd vberom.asm:315
    call 00577h                               ; e8 94 fe                    ; 0xc06e0 vberom.asm:316
    push ax                                   ; 50                          ; 0xc06e3 vberom.asm:317
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06e4 vberom.asm:318
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc06e7 vberom.asm:319
    out DX, ax                                ; ef                          ; 0xc06ea vberom.asm:320
    pop ax                                    ; 58                          ; 0xc06eb vberom.asm:321
    push ax                                   ; 50                          ; 0xc06ec vberom.asm:322
    shr ax, 003h                              ; c1 e8 03                    ; 0xc06ed vberom.asm:324
    dec ax                                    ; 48                          ; 0xc06f0 vberom.asm:330
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc06f1 vberom.asm:331
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc06f3 vberom.asm:332
    out DX, ax                                ; ef                          ; 0xc06f5 vberom.asm:333
    pop ax                                    ; 58                          ; 0xc06f6 vberom.asm:334
    call 006b5h                               ; e8 bb ff                    ; 0xc06f7 vberom.asm:335
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06fa vberom.asm:338
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc06fd vberom.asm:339
    call 00570h                               ; e8 6d fe                    ; 0xc0700 vberom.asm:340
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0703 vberom.asm:341
    call 00577h                               ; e8 6e fe                    ; 0xc0706 vberom.asm:342
    dec ax                                    ; 48                          ; 0xc0709 vberom.asm:343
    push ax                                   ; 50                          ; 0xc070a vberom.asm:344
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc070b vberom.asm:345
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc070e vberom.asm:346
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc0710 vberom.asm:347
    out DX, ax                                ; ef                          ; 0xc0712 vberom.asm:348
    pop ax                                    ; 58                          ; 0xc0713 vberom.asm:349
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc0714 vberom.asm:350
    out DX, AL                                ; ee                          ; 0xc0716 vberom.asm:351
    inc dx                                    ; 42                          ; 0xc0717 vberom.asm:352
    in AL, DX                                 ; ec                          ; 0xc0718 vberom.asm:353
    and AL, strict byte 0bdh                  ; 24 bd                       ; 0xc0719 vberom.asm:354
    test ah, 001h                             ; f6 c4 01                    ; 0xc071b vberom.asm:355
    je short 00722h                           ; 74 02                       ; 0xc071e vberom.asm:356
    or AL, strict byte 002h                   ; 0c 02                       ; 0xc0720 vberom.asm:357
    test ah, 002h                             ; f6 c4 02                    ; 0xc0722 vberom.asm:359
    je short 00729h                           ; 74 02                       ; 0xc0725 vberom.asm:360
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0727 vberom.asm:361
    out DX, AL                                ; ee                          ; 0xc0729 vberom.asm:363
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc072a vberom.asm:366
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc072d vberom.asm:367
    out DX, AL                                ; ee                          ; 0xc0730 vberom.asm:368
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc0731 vberom.asm:369
    in AL, DX                                 ; ec                          ; 0xc0734 vberom.asm:370
    and AL, strict byte 060h                  ; 24 60                       ; 0xc0735 vberom.asm:371
    out DX, AL                                ; ee                          ; 0xc0737 vberom.asm:372
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0738 vberom.asm:373
    mov AL, strict byte 017h                  ; b0 17                       ; 0xc073b vberom.asm:374
    out DX, AL                                ; ee                          ; 0xc073d vberom.asm:375
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc073e vberom.asm:376
    in AL, DX                                 ; ec                          ; 0xc0741 vberom.asm:377
    or AL, strict byte 003h                   ; 0c 03                       ; 0xc0742 vberom.asm:378
    out DX, AL                                ; ee                          ; 0xc0744 vberom.asm:379
    mov dx, 003dah                            ; ba da 03                    ; 0xc0745 vberom.asm:380
    in AL, DX                                 ; ec                          ; 0xc0748 vberom.asm:381
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0749 vberom.asm:382
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc074c vberom.asm:383
    out DX, AL                                ; ee                          ; 0xc074e vberom.asm:384
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc074f vberom.asm:385
    in AL, DX                                 ; ec                          ; 0xc0752 vberom.asm:386
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc0753 vberom.asm:387
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0755 vberom.asm:388
    out DX, AL                                ; ee                          ; 0xc0758 vberom.asm:389
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0759 vberom.asm:390
    out DX, AL                                ; ee                          ; 0xc075b vberom.asm:391
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc075c vberom.asm:392
    mov ax, 00506h                            ; b8 06 05                    ; 0xc075f vberom.asm:393
    out DX, ax                                ; ef                          ; 0xc0762 vberom.asm:394
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0763 vberom.asm:395
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc0766 vberom.asm:396
    out DX, ax                                ; ef                          ; 0xc0769 vberom.asm:397
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc076a vberom.asm:400
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc076d vberom.asm:401
    call 00570h                               ; e8 fd fd                    ; 0xc0770 vberom.asm:402
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0773 vberom.asm:403
    call 00577h                               ; e8 fe fd                    ; 0xc0776 vberom.asm:404
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc0779 vberom.asm:405
    jc short 007bdh                           ; 72 40                       ; 0xc077b vberom.asm:406
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc077d vberom.asm:407
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0780 vberom.asm:408
    out DX, AL                                ; ee                          ; 0xc0782 vberom.asm:409
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc0783 vberom.asm:410
    in AL, DX                                 ; ec                          ; 0xc0786 vberom.asm:411
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0787 vberom.asm:412
    out DX, AL                                ; ee                          ; 0xc0789 vberom.asm:413
    mov dx, 003dah                            ; ba da 03                    ; 0xc078a vberom.asm:414
    in AL, DX                                 ; ec                          ; 0xc078d vberom.asm:415
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc078e vberom.asm:416
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0791 vberom.asm:417
    out DX, AL                                ; ee                          ; 0xc0793 vberom.asm:418
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0794 vberom.asm:419
    in AL, DX                                 ; ec                          ; 0xc0797 vberom.asm:420
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0798 vberom.asm:421
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc079a vberom.asm:422
    out DX, AL                                ; ee                          ; 0xc079d vberom.asm:423
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc079e vberom.asm:424
    out DX, AL                                ; ee                          ; 0xc07a0 vberom.asm:425
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc07a1 vberom.asm:426
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc07a4 vberom.asm:427
    out DX, AL                                ; ee                          ; 0xc07a6 vberom.asm:428
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc07a7 vberom.asm:429
    in AL, DX                                 ; ec                          ; 0xc07aa vberom.asm:430
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc07ab vberom.asm:431
    out DX, AL                                ; ee                          ; 0xc07ad vberom.asm:432
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc07ae vberom.asm:433
    mov AL, strict byte 005h                  ; b0 05                       ; 0xc07b1 vberom.asm:434
    out DX, AL                                ; ee                          ; 0xc07b3 vberom.asm:435
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc07b4 vberom.asm:436
    in AL, DX                                 ; ec                          ; 0xc07b7 vberom.asm:437
    and AL, strict byte 09fh                  ; 24 9f                       ; 0xc07b8 vberom.asm:438
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07ba vberom.asm:439
    out DX, AL                                ; ee                          ; 0xc07bc vberom.asm:440
    pop dx                                    ; 5a                          ; 0xc07bd vberom.asm:443
    pop ax                                    ; 58                          ; 0xc07be vberom.asm:444
  ; disGetNextSymbol 0xc07bf LB 0x13b -> off=0x0 cb=0000000000000013 uValue=00000000000c07bf '_vbe_has_vbe_display'
_vbe_has_vbe_display:                        ; 0xc07bf LB 0x13
    push DS                                   ; 1e                          ; 0xc07bf vberom.asm:450
    push bx                                   ; 53                          ; 0xc07c0 vberom.asm:451
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07c1 vberom.asm:452
    mov ds, ax                                ; 8e d8                       ; 0xc07c4 vberom.asm:453
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc07c6 vberom.asm:454
    mov al, byte [bx]                         ; 8a 07                       ; 0xc07c9 vberom.asm:455
    and AL, strict byte 001h                  ; 24 01                       ; 0xc07cb vberom.asm:456
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc07cd vberom.asm:457
    pop bx                                    ; 5b                          ; 0xc07cf vberom.asm:458
    pop DS                                    ; 1f                          ; 0xc07d0 vberom.asm:459
    retn                                      ; c3                          ; 0xc07d1 vberom.asm:460
  ; disGetNextSymbol 0xc07d2 LB 0x128 -> off=0x0 cb=0000000000000025 uValue=00000000000c07d2 'vbe_biosfn_return_current_mode'
vbe_biosfn_return_current_mode:              ; 0xc07d2 LB 0x25
    push DS                                   ; 1e                          ; 0xc07d2 vberom.asm:473
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07d3 vberom.asm:474
    mov ds, ax                                ; 8e d8                       ; 0xc07d6 vberom.asm:475
    call 005f1h                               ; e8 16 fe                    ; 0xc07d8 vberom.asm:476
    and ax, strict byte 00001h                ; 83 e0 01                    ; 0xc07db vberom.asm:477
    je short 007e9h                           ; 74 09                       ; 0xc07de vberom.asm:478
    mov bx, 000bah                            ; bb ba 00                    ; 0xc07e0 vberom.asm:479
    mov ax, word [bx]                         ; 8b 07                       ; 0xc07e3 vberom.asm:480
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc07e5 vberom.asm:481
    jne short 007f2h                          ; 75 09                       ; 0xc07e7 vberom.asm:482
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc07e9 vberom.asm:484
    mov al, byte [bx]                         ; 8a 07                       ; 0xc07ec vberom.asm:485
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc07ee vberom.asm:486
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc07f0 vberom.asm:487
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc07f2 vberom.asm:489
    pop DS                                    ; 1f                          ; 0xc07f5 vberom.asm:490
    retn                                      ; c3                          ; 0xc07f6 vberom.asm:491
  ; disGetNextSymbol 0xc07f7 LB 0x103 -> off=0x0 cb=000000000000002d uValue=00000000000c07f7 'vbe_biosfn_display_window_control'
vbe_biosfn_display_window_control:           ; 0xc07f7 LB 0x2d
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc07f7 vberom.asm:515
    jne short 00820h                          ; 75 24                       ; 0xc07fa vberom.asm:516
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc07fc vberom.asm:517
    je short 00817h                           ; 74 16                       ; 0xc07ff vberom.asm:518
    jc short 00807h                           ; 72 04                       ; 0xc0801 vberom.asm:519
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0803 vberom.asm:520
    retn                                      ; c3                          ; 0xc0806 vberom.asm:521
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0807 vberom.asm:523
    call 00603h                               ; e8 f7 fd                    ; 0xc0809 vberom.asm:524
    call 00617h                               ; e8 08 fe                    ; 0xc080c vberom.asm:525
    db  03bh, 0c2h
    ; cmp ax, dx                                ; 3b c2                     ; 0xc080f vberom.asm:526
    jne short 00820h                          ; 75 0d                       ; 0xc0811 vberom.asm:527
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0813 vberom.asm:528
    retn                                      ; c3                          ; 0xc0816 vberom.asm:529
    call 00617h                               ; e8 fd fd                    ; 0xc0817 vberom.asm:531
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc081a vberom.asm:532
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc081c vberom.asm:533
    retn                                      ; c3                          ; 0xc081f vberom.asm:534
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0820 vberom.asm:536
    retn                                      ; c3                          ; 0xc0823 vberom.asm:537
  ; disGetNextSymbol 0xc0824 LB 0xd6 -> off=0x0 cb=0000000000000034 uValue=00000000000c0824 'vbe_biosfn_set_get_display_start'
vbe_biosfn_set_get_display_start:            ; 0xc0824 LB 0x34
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc0824 vberom.asm:577
    je short 00834h                           ; 74 0b                       ; 0xc0827 vberom.asm:578
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0829 vberom.asm:579
    je short 00848h                           ; 74 1a                       ; 0xc082c vberom.asm:580
    jc short 0083ah                           ; 72 0a                       ; 0xc082e vberom.asm:581
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0830 vberom.asm:582
    retn                                      ; c3                          ; 0xc0833 vberom.asm:583
    call 00589h                               ; e8 52 fd                    ; 0xc0834 vberom.asm:585
    call 0057ch                               ; e8 42 fd                    ; 0xc0837 vberom.asm:586
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc083a vberom.asm:588
    call 00669h                               ; e8 2a fe                    ; 0xc083c vberom.asm:589
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc083f vberom.asm:590
    call 0068fh                               ; e8 4b fe                    ; 0xc0841 vberom.asm:591
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0844 vberom.asm:592
    retn                                      ; c3                          ; 0xc0847 vberom.asm:593
    call 0067dh                               ; e8 32 fe                    ; 0xc0848 vberom.asm:595
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8                     ; 0xc084b vberom.asm:596
    call 006a3h                               ; e8 53 fe                    ; 0xc084d vberom.asm:597
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0850 vberom.asm:598
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0852 vberom.asm:599
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0854 vberom.asm:600
    retn                                      ; c3                          ; 0xc0857 vberom.asm:601
  ; disGetNextSymbol 0xc0858 LB 0xa2 -> off=0x0 cb=0000000000000037 uValue=00000000000c0858 'vbe_biosfn_set_get_dac_palette_format'
vbe_biosfn_set_get_dac_palette_format:       ; 0xc0858 LB 0x37
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0858 vberom.asm:616
    je short 0087bh                           ; 74 1e                       ; 0xc085b vberom.asm:617
    jc short 00863h                           ; 72 04                       ; 0xc085d vberom.asm:618
    mov ax, 00100h                            ; b8 00 01                    ; 0xc085f vberom.asm:619
    retn                                      ; c3                          ; 0xc0862 vberom.asm:620
    call 005f1h                               ; e8 8b fd                    ; 0xc0863 vberom.asm:622
    cmp bh, 006h                              ; 80 ff 06                    ; 0xc0866 vberom.asm:623
    je short 00875h                           ; 74 0a                       ; 0xc0869 vberom.asm:624
    cmp bh, 008h                              ; 80 ff 08                    ; 0xc086b vberom.asm:625
    jne short 0088bh                          ; 75 1b                       ; 0xc086e vberom.asm:626
    or ax, strict byte 00020h                 ; 83 c8 20                    ; 0xc0870 vberom.asm:627
    jne short 00878h                          ; 75 03                       ; 0xc0873 vberom.asm:628
    and ax, strict byte 0ffdfh                ; 83 e0 df                    ; 0xc0875 vberom.asm:630
    call 005ddh                               ; e8 62 fd                    ; 0xc0878 vberom.asm:632
    mov BH, strict byte 006h                  ; b7 06                       ; 0xc087b vberom.asm:634
    call 005f1h                               ; e8 71 fd                    ; 0xc087d vberom.asm:635
    and ax, strict byte 00020h                ; 83 e0 20                    ; 0xc0880 vberom.asm:636
    je short 00887h                           ; 74 02                       ; 0xc0883 vberom.asm:637
    mov BH, strict byte 008h                  ; b7 08                       ; 0xc0885 vberom.asm:638
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0887 vberom.asm:640
    retn                                      ; c3                          ; 0xc088a vberom.asm:641
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc088b vberom.asm:643
    retn                                      ; c3                          ; 0xc088e vberom.asm:644
  ; disGetNextSymbol 0xc088f LB 0x6b -> off=0x0 cb=0000000000000057 uValue=00000000000c088f 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc088f LB 0x57
    test bl, bl                               ; 84 db                       ; 0xc088f vberom.asm:683
    je short 008a2h                           ; 74 0f                       ; 0xc0891 vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0893 vberom.asm:685
    je short 008c2h                           ; 74 2a                       ; 0xc0896 vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0898 vberom.asm:687
    jbe short 008e2h                          ; 76 45                       ; 0xc089b vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc089d vberom.asm:689
    jne short 008deh                          ; 75 3c                       ; 0xc08a0 vberom.asm:690
    pushaw                                    ; 60                          ; 0xc08a2 vberom.asm:143
    push DS                                   ; 1e                          ; 0xc08a3 vberom.asm:696
    push ES                                   ; 06                          ; 0xc08a4 vberom.asm:697
    pop DS                                    ; 1f                          ; 0xc08a5 vberom.asm:698
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08a6 vberom.asm:699
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc08a8 vberom.asm:700
    out DX, AL                                ; ee                          ; 0xc08ab vberom.asm:701
    inc dx                                    ; 42                          ; 0xc08ac vberom.asm:702
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7                     ; 0xc08ad vberom.asm:703
    lodsw                                     ; ad                          ; 0xc08af vberom.asm:714
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc08b0 vberom.asm:715
    lodsw                                     ; ad                          ; 0xc08b2 vberom.asm:716
    out DX, AL                                ; ee                          ; 0xc08b3 vberom.asm:717
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc08b4 vberom.asm:718
    out DX, AL                                ; ee                          ; 0xc08b6 vberom.asm:719
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc08b7 vberom.asm:720
    out DX, AL                                ; ee                          ; 0xc08b9 vberom.asm:721
    loop 008afh                               ; e2 f3                       ; 0xc08ba vberom.asm:723
    pop DS                                    ; 1f                          ; 0xc08bc vberom.asm:724
    popaw                                     ; 61                          ; 0xc08bd vberom.asm:162
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08be vberom.asm:727
    retn                                      ; c3                          ; 0xc08c1 vberom.asm:728
    pushaw                                    ; 60                          ; 0xc08c2 vberom.asm:143
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08c3 vberom.asm:732
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc08c5 vberom.asm:733
    out DX, AL                                ; ee                          ; 0xc08c8 vberom.asm:734
    add dl, 002h                              ; 80 c2 02                    ; 0xc08c9 vberom.asm:735
    db  033h, 0dbh
    ; xor bx, bx                                ; 33 db                     ; 0xc08cc vberom.asm:746
    in AL, DX                                 ; ec                          ; 0xc08ce vberom.asm:748
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc08cf vberom.asm:749
    in AL, DX                                 ; ec                          ; 0xc08d1 vberom.asm:750
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc08d2 vberom.asm:751
    in AL, DX                                 ; ec                          ; 0xc08d4 vberom.asm:752
    stosw                                     ; ab                          ; 0xc08d5 vberom.asm:753
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc08d6 vberom.asm:754
    stosw                                     ; ab                          ; 0xc08d8 vberom.asm:755
    loop 008ceh                               ; e2 f3                       ; 0xc08d9 vberom.asm:757
    popaw                                     ; 61                          ; 0xc08db vberom.asm:162
    jmp short 008beh                          ; eb e0                       ; 0xc08dc vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08de vberom.asm:762
    retn                                      ; c3                          ; 0xc08e1 vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc08e2 vberom.asm:765
    retn                                      ; c3                          ; 0xc08e5 vberom.asm:766
  ; disGetNextSymbol 0xc08e6 LB 0x14 -> off=0x0 cb=0000000000000014 uValue=00000000000c08e6 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc08e6 LB 0x14
    test bl, bl                               ; 84 db                       ; 0xc08e6 vberom.asm:780
    jne short 008f6h                          ; 75 0c                       ; 0xc08e8 vberom.asm:781
    push CS                                   ; 0e                          ; 0xc08ea vberom.asm:782
    pop ES                                    ; 07                          ; 0xc08eb vberom.asm:783
    mov di, 04640h                            ; bf 40 46                    ; 0xc08ec vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc08ef vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08f2 vberom.asm:786
    retn                                      ; c3                          ; 0xc08f5 vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08f6 vberom.asm:789
    retn                                      ; c3                          ; 0xc08f9 vberom.asm:790

  ; Padding 0xf6 bytes at 0xc08fa
  times 246 db 0

section _TEXT progbits vstart=0x9f0 align=1 ; size=0x3bd5 class=CODE group=AUTO
  ; disGetNextSymbol 0xc09f0 LB 0x3bd5 -> off=0x0 cb=000000000000001b uValue=00000000000c09f0 'set_int_vector'
set_int_vector:                              ; 0xc09f0 LB 0x1b
    push dx                                   ; 52                          ; 0xc09f0 vgabios.c:88
    push bp                                   ; 55                          ; 0xc09f1
    mov bp, sp                                ; 89 e5                       ; 0xc09f2
    mov dx, bx                                ; 89 da                       ; 0xc09f4
    mov bl, al                                ; 88 c3                       ; 0xc09f6 vgabios.c:92
    xor bh, bh                                ; 30 ff                       ; 0xc09f8
    sal bx, 002h                              ; c1 e3 02                    ; 0xc09fa
    xor ax, ax                                ; 31 c0                       ; 0xc09fd
    mov es, ax                                ; 8e c0                       ; 0xc09ff
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0a01
    mov word [es:bx+002h], cx                 ; 26 89 4f 02                 ; 0xc0a04
    pop bp                                    ; 5d                          ; 0xc0a08 vgabios.c:93
    pop dx                                    ; 5a                          ; 0xc0a09
    retn                                      ; c3                          ; 0xc0a0a
  ; disGetNextSymbol 0xc0a0b LB 0x3bba -> off=0x0 cb=000000000000001c uValue=00000000000c0a0b 'init_vga_card'
init_vga_card:                               ; 0xc0a0b LB 0x1c
    push bp                                   ; 55                          ; 0xc0a0b vgabios.c:144
    mov bp, sp                                ; 89 e5                       ; 0xc0a0c
    push dx                                   ; 52                          ; 0xc0a0e
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc0a0f vgabios.c:147
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc0a11
    out DX, AL                                ; ee                          ; 0xc0a14
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc0a15 vgabios.c:150
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0a17
    out DX, AL                                ; ee                          ; 0xc0a1a
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc0a1b vgabios.c:151
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc0a1d
    out DX, AL                                ; ee                          ; 0xc0a20
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0a21 vgabios.c:156
    pop dx                                    ; 5a                          ; 0xc0a24
    pop bp                                    ; 5d                          ; 0xc0a25
    retn                                      ; c3                          ; 0xc0a26
  ; disGetNextSymbol 0xc0a27 LB 0x3b9e -> off=0x0 cb=000000000000003e uValue=00000000000c0a27 'init_bios_area'
init_bios_area:                              ; 0xc0a27 LB 0x3e
    push bx                                   ; 53                          ; 0xc0a27 vgabios.c:222
    push bp                                   ; 55                          ; 0xc0a28
    mov bp, sp                                ; 89 e5                       ; 0xc0a29
    xor bx, bx                                ; 31 db                       ; 0xc0a2b vgabios.c:226
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0a2d
    mov es, ax                                ; 8e c0                       ; 0xc0a30
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc0a32 vgabios.c:229
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc0a36
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc0a38
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc0a3a
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc0a3e vgabios.c:233
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc0a44 vgabios.c:235
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc0a4b vgabios.c:239
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc0a51 vgabios.c:241
    mov word [es:bx+000a8h], 05550h           ; 26 c7 87 a8 00 50 55        ; 0xc0a56 vgabios.c:243
    mov [es:bx+000aah], ds                    ; 26 8c 9f aa 00              ; 0xc0a5d
    pop bp                                    ; 5d                          ; 0xc0a62 vgabios.c:244
    pop bx                                    ; 5b                          ; 0xc0a63
    retn                                      ; c3                          ; 0xc0a64
  ; disGetNextSymbol 0xc0a65 LB 0x3b60 -> off=0x0 cb=0000000000000031 uValue=00000000000c0a65 'vgabios_init_func'
vgabios_init_func:                           ; 0xc0a65 LB 0x31
    inc bp                                    ; 45                          ; 0xc0a65 vgabios.c:251
    push bp                                   ; 55                          ; 0xc0a66
    mov bp, sp                                ; 89 e5                       ; 0xc0a67
    call 00a0bh                               ; e8 9f ff                    ; 0xc0a69 vgabios.c:253
    call 00a27h                               ; e8 b8 ff                    ; 0xc0a6c vgabios.c:254
    call 03f3fh                               ; e8 cd 34                    ; 0xc0a6f vgabios.c:256
    mov bx, strict word 00028h                ; bb 28 00                    ; 0xc0a72 vgabios.c:258
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a75
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc0a78
    call 009f0h                               ; e8 72 ff                    ; 0xc0a7b
    mov bx, strict word 00028h                ; bb 28 00                    ; 0xc0a7e vgabios.c:259
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a81
    mov ax, strict word 0006dh                ; b8 6d 00                    ; 0xc0a84
    call 009f0h                               ; e8 66 ff                    ; 0xc0a87
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a8a vgabios.c:285
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a8d
    int 010h                                  ; cd 10                       ; 0xc0a8f
    mov sp, bp                                ; 89 ec                       ; 0xc0a91 vgabios.c:288
    pop bp                                    ; 5d                          ; 0xc0a93
    dec bp                                    ; 4d                          ; 0xc0a94
    retf                                      ; cb                          ; 0xc0a95
  ; disGetNextSymbol 0xc0a96 LB 0x3b2f -> off=0x0 cb=0000000000000040 uValue=00000000000c0a96 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a96 LB 0x40
    push si                                   ; 56                          ; 0xc0a96 vgabios.c:357
    push di                                   ; 57                          ; 0xc0a97
    push bp                                   ; 55                          ; 0xc0a98
    mov bp, sp                                ; 89 e5                       ; 0xc0a99
    mov si, dx                                ; 89 d6                       ; 0xc0a9b
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a9d vgabios.c:359
    jbe short 00aafh                          ; 76 0e                       ; 0xc0a9f
    push SS                                   ; 16                          ; 0xc0aa1 vgabios.c:360
    pop ES                                    ; 07                          ; 0xc0aa2
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0aa3
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0aa8 vgabios.c:361
    jmp short 00ad2h                          ; eb 23                       ; 0xc0aad vgabios.c:362
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0aaf vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0ab2
    mov es, dx                                ; 8e c2                       ; 0xc0ab5
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0ab7
    push SS                                   ; 16                          ; 0xc0aba vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0abb
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0abc
    xor ah, ah                                ; 30 e4                       ; 0xc0abf vgabios.c:365
    mov si, ax                                ; 89 c6                       ; 0xc0ac1
    add si, ax                                ; 01 c6                       ; 0xc0ac3
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0ac5
    mov es, dx                                ; 8e c2                       ; 0xc0ac8 vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc0aca
    push SS                                   ; 16                          ; 0xc0acd vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0ace
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0acf
    pop bp                                    ; 5d                          ; 0xc0ad2 vgabios.c:367
    pop di                                    ; 5f                          ; 0xc0ad3
    pop si                                    ; 5e                          ; 0xc0ad4
    retn                                      ; c3                          ; 0xc0ad5
  ; disGetNextSymbol 0xc0ad6 LB 0x3aef -> off=0x0 cb=000000000000005e uValue=00000000000c0ad6 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0ad6 LB 0x5e
    push bp                                   ; 55                          ; 0xc0ad6 vgabios.c:370
    mov bp, sp                                ; 89 e5                       ; 0xc0ad7
    push si                                   ; 56                          ; 0xc0ad9
    push di                                   ; 57                          ; 0xc0ada
    push ax                                   ; 50                          ; 0xc0adb
    push ax                                   ; 50                          ; 0xc0adc
    push dx                                   ; 52                          ; 0xc0add
    push bx                                   ; 53                          ; 0xc0ade
    mov bl, cl                                ; 88 cb                       ; 0xc0adf
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0ae1 vgabios.c:372
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0ae6 vgabios.c:374
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0ae9
    je short 00b28h                           ; 74 39                       ; 0xc0aed
    mov cl, byte [bp+006h]                    ; 8a 4e 06                    ; 0xc0aef vgabios.c:375
    xor ch, ch                                ; 30 ed                       ; 0xc0af2
    mov dx, ss                                ; 8c d2                       ; 0xc0af4
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0af6
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0af9
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0afc
    push DS                                   ; 1e                          ; 0xc0aff
    mov ds, dx                                ; 8e da                       ; 0xc0b00
    rep cmpsb                                 ; f3 a6                       ; 0xc0b02
    pop DS                                    ; 1f                          ; 0xc0b04
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0b05
    je short 00b0ch                           ; 74 02                       ; 0xc0b08
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0b0a
    test ax, ax                               ; 85 c0                       ; 0xc0b0c
    jne short 00b1ch                          ; 75 0c                       ; 0xc0b0e
    mov al, bl                                ; 88 d8                       ; 0xc0b10 vgabios.c:376
    xor ah, ah                                ; 30 e4                       ; 0xc0b12
    or ah, 080h                               ; 80 cc 80                    ; 0xc0b14
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0b17
    jmp short 00b28h                          ; eb 0c                       ; 0xc0b1a vgabios.c:377
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc0b1c vgabios.c:379
    xor ah, ah                                ; 30 e4                       ; 0xc0b1f
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0b21
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0b24 vgabios.c:380
    jmp short 00ae6h                          ; eb be                       ; 0xc0b26 vgabios.c:381
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0b28 vgabios.c:383
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0b2b
    pop di                                    ; 5f                          ; 0xc0b2e
    pop si                                    ; 5e                          ; 0xc0b2f
    pop bp                                    ; 5d                          ; 0xc0b30
    retn 00004h                               ; c2 04 00                    ; 0xc0b31
  ; disGetNextSymbol 0xc0b34 LB 0x3a91 -> off=0x0 cb=0000000000000046 uValue=00000000000c0b34 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0b34 LB 0x46
    push bp                                   ; 55                          ; 0xc0b34 vgabios.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc0b35
    push si                                   ; 56                          ; 0xc0b37
    push di                                   ; 57                          ; 0xc0b38
    push ax                                   ; 50                          ; 0xc0b39
    push ax                                   ; 50                          ; 0xc0b3a
    mov si, ax                                ; 89 c6                       ; 0xc0b3b
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0b3d
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0b40
    mov bx, cx                                ; 89 cb                       ; 0xc0b43
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0b45 vgabios.c:392
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0b48
    out DX, ax                                ; ef                          ; 0xc0b4b
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0b4c vgabios.c:394
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0b4f
    je short 00b6ah                           ; 74 15                       ; 0xc0b53
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0b55 vgabios.c:395
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0b58
    not al                                    ; f6 d0                       ; 0xc0b5b
    mov di, bx                                ; 89 df                       ; 0xc0b5d
    inc bx                                    ; 43                          ; 0xc0b5f
    push SS                                   ; 16                          ; 0xc0b60
    pop ES                                    ; 07                          ; 0xc0b61
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0b62
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0b65 vgabios.c:396
    jmp short 00b4ch                          ; eb e2                       ; 0xc0b68 vgabios.c:397
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0b6a vgabios.c:400
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0b6d
    out DX, ax                                ; ef                          ; 0xc0b70
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0b71 vgabios.c:401
    pop di                                    ; 5f                          ; 0xc0b74
    pop si                                    ; 5e                          ; 0xc0b75
    pop bp                                    ; 5d                          ; 0xc0b76
    retn 00002h                               ; c2 02 00                    ; 0xc0b77
  ; disGetNextSymbol 0xc0b7a LB 0x3a4b -> off=0x0 cb=000000000000002f uValue=00000000000c0b7a 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0b7a LB 0x2f
    push si                                   ; 56                          ; 0xc0b7a vgabios.c:403
    push bp                                   ; 55                          ; 0xc0b7b
    mov bp, sp                                ; 89 e5                       ; 0xc0b7c
    mov ch, al                                ; 88 c5                       ; 0xc0b7e
    mov al, dl                                ; 88 d0                       ; 0xc0b80
    xor ah, ah                                ; 30 e4                       ; 0xc0b82 vgabios.c:407
    mul bx                                    ; f7 e3                       ; 0xc0b84
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc0b86
    xor bh, bh                                ; 30 ff                       ; 0xc0b89
    mul bx                                    ; f7 e3                       ; 0xc0b8b
    mov bl, ch                                ; 88 eb                       ; 0xc0b8d
    add bx, ax                                ; 01 c3                       ; 0xc0b8f
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc0b91 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0b94
    mov es, ax                                ; 8e c0                       ; 0xc0b97
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0b99
    mov al, cl                                ; 88 c8                       ; 0xc0b9c vgabios.c:58
    xor ah, ah                                ; 30 e4                       ; 0xc0b9e
    mul si                                    ; f7 e6                       ; 0xc0ba0
    add ax, bx                                ; 01 d8                       ; 0xc0ba2
    pop bp                                    ; 5d                          ; 0xc0ba4 vgabios.c:411
    pop si                                    ; 5e                          ; 0xc0ba5
    retn 00002h                               ; c2 02 00                    ; 0xc0ba6
  ; disGetNextSymbol 0xc0ba9 LB 0x3a1c -> off=0x0 cb=0000000000000040 uValue=00000000000c0ba9 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0ba9 LB 0x40
    push bp                                   ; 55                          ; 0xc0ba9 vgabios.c:413
    mov bp, sp                                ; 89 e5                       ; 0xc0baa
    push cx                                   ; 51                          ; 0xc0bac
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0bad
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc0bb0 vgabios.c:417
    mov byte [bp-003h], 000h                  ; c6 46 fd 00                 ; 0xc0bb3
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0bb7
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0bba
    mov bx, ax                                ; 89 c3                       ; 0xc0bbd
    mov ax, dx                                ; 89 d0                       ; 0xc0bbf
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0bc1
    call 00b34h                               ; e8 6d ff                    ; 0xc0bc4
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0bc7 vgabios.c:420
    push 00100h                               ; 68 00 01                    ; 0xc0bca
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0bcd vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0bd0
    mov es, ax                                ; 8e c0                       ; 0xc0bd2
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0bd4
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0bd7
    xor cx, cx                                ; 31 c9                       ; 0xc0bdb vgabios.c:68
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0bdd
    call 00ad6h                               ; e8 f3 fe                    ; 0xc0be0
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0be3 vgabios.c:421
    pop cx                                    ; 59                          ; 0xc0be6
    pop bp                                    ; 5d                          ; 0xc0be7
    retn                                      ; c3                          ; 0xc0be8
  ; disGetNextSymbol 0xc0be9 LB 0x39dc -> off=0x0 cb=0000000000000024 uValue=00000000000c0be9 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0be9 LB 0x24
    enter 00002h, 000h                        ; c8 02 00 00                 ; 0xc0be9 vgabios.c:423
    mov byte [bp-002h], al                    ; 88 46 fe                    ; 0xc0bed
    mov al, dl                                ; 88 d0                       ; 0xc0bf0 vgabios.c:427
    xor ah, ah                                ; 30 e4                       ; 0xc0bf2
    mul bx                                    ; f7 e3                       ; 0xc0bf4
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc0bf6
    xor dh, dh                                ; 30 f6                       ; 0xc0bf9
    mul dx                                    ; f7 e2                       ; 0xc0bfb
    mov dx, ax                                ; 89 c2                       ; 0xc0bfd
    mov al, byte [bp-002h]                    ; 8a 46 fe                    ; 0xc0bff
    xor ah, ah                                ; 30 e4                       ; 0xc0c02
    add ax, dx                                ; 01 d0                       ; 0xc0c04
    sal ax, 003h                              ; c1 e0 03                    ; 0xc0c06 vgabios.c:428
    leave                                     ; c9                          ; 0xc0c09 vgabios.c:430
    retn 00002h                               ; c2 02 00                    ; 0xc0c0a
  ; disGetNextSymbol 0xc0c0d LB 0x39b8 -> off=0x0 cb=000000000000004b uValue=00000000000c0c0d 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0c0d LB 0x4b
    push si                                   ; 56                          ; 0xc0c0d vgabios.c:432
    push di                                   ; 57                          ; 0xc0c0e
    enter 00004h, 000h                        ; c8 04 00 00                 ; 0xc0c0f
    mov si, ax                                ; 89 c6                       ; 0xc0c13
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0c15
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0c18
    mov bx, cx                                ; 89 cb                       ; 0xc0c1b
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0c1d vgabios.c:438
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0c20
    je short 00c52h                           ; 74 2c                       ; 0xc0c24
    xor dh, dh                                ; 30 f6                       ; 0xc0c26 vgabios.c:439
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0c28 vgabios.c:440
    xor ax, ax                                ; 31 c0                       ; 0xc0c2a vgabios.c:441
    jmp short 00c33h                          ; eb 05                       ; 0xc0c2c
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0c2e
    jnl short 00c47h                          ; 7d 14                       ; 0xc0c31
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0c33 vgabios.c:442
    mov di, si                                ; 89 f7                       ; 0xc0c36
    add di, ax                                ; 01 c7                       ; 0xc0c38
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0c3a
    je short 00c42h                           ; 74 02                       ; 0xc0c3e
    or dh, dl                                 ; 08 d6                       ; 0xc0c40 vgabios.c:443
    shr dl, 1                                 ; d0 ea                       ; 0xc0c42 vgabios.c:444
    inc ax                                    ; 40                          ; 0xc0c44 vgabios.c:445
    jmp short 00c2eh                          ; eb e7                       ; 0xc0c45
    mov di, bx                                ; 89 df                       ; 0xc0c47 vgabios.c:446
    inc bx                                    ; 43                          ; 0xc0c49
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0c4a
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0c4d vgabios.c:447
    jmp short 00c1dh                          ; eb cb                       ; 0xc0c50 vgabios.c:448
    leave                                     ; c9                          ; 0xc0c52 vgabios.c:449
    pop di                                    ; 5f                          ; 0xc0c53
    pop si                                    ; 5e                          ; 0xc0c54
    retn 00002h                               ; c2 02 00                    ; 0xc0c55
  ; disGetNextSymbol 0xc0c58 LB 0x396d -> off=0x0 cb=0000000000000045 uValue=00000000000c0c58 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0c58 LB 0x45
    push bp                                   ; 55                          ; 0xc0c58 vgabios.c:451
    mov bp, sp                                ; 89 e5                       ; 0xc0c59
    push cx                                   ; 51                          ; 0xc0c5b
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0c5c
    mov cx, ax                                ; 89 c1                       ; 0xc0c5f
    mov ax, dx                                ; 89 d0                       ; 0xc0c61
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc0c63 vgabios.c:455
    mov byte [bp-003h], 000h                  ; c6 46 fd 00                 ; 0xc0c66
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0c6a
    mov bx, cx                                ; 89 cb                       ; 0xc0c6d
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0c6f
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0c72
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0c75
    call 00c0dh                               ; e8 92 ff                    ; 0xc0c78
    push word [bp-004h]                       ; ff 76 fc                    ; 0xc0c7b vgabios.c:458
    push 00100h                               ; 68 00 01                    ; 0xc0c7e
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0c81 vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0c84
    mov es, ax                                ; 8e c0                       ; 0xc0c86
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c88
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0c8b
    xor cx, cx                                ; 31 c9                       ; 0xc0c8f vgabios.c:68
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0c91
    call 00ad6h                               ; e8 3f fe                    ; 0xc0c94
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0c97 vgabios.c:459
    pop cx                                    ; 59                          ; 0xc0c9a
    pop bp                                    ; 5d                          ; 0xc0c9b
    retn                                      ; c3                          ; 0xc0c9c
  ; disGetNextSymbol 0xc0c9d LB 0x3928 -> off=0x0 cb=0000000000000035 uValue=00000000000c0c9d 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0c9d LB 0x35
    push bp                                   ; 55                          ; 0xc0c9d vgabios.c:461
    mov bp, sp                                ; 89 e5                       ; 0xc0c9e
    push bx                                   ; 53                          ; 0xc0ca0
    push cx                                   ; 51                          ; 0xc0ca1
    mov bx, ax                                ; 89 c3                       ; 0xc0ca2
    mov es, dx                                ; 8e c2                       ; 0xc0ca4
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0ca6 vgabios.c:467
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0ca9 vgabios.c:468
    xor dl, dl                                ; 30 d2                       ; 0xc0cab vgabios.c:469
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0cad vgabios.c:470
    xchg ah, al                               ; 86 c4                       ; 0xc0cb0
    xor bx, bx                                ; 31 db                       ; 0xc0cb2 vgabios.c:472
    jmp short 00cbbh                          ; eb 05                       ; 0xc0cb4
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0cb6
    jnl short 00cc9h                          ; 7d 0e                       ; 0xc0cb9
    test ax, cx                               ; 85 c8                       ; 0xc0cbb vgabios.c:473
    je short 00cc1h                           ; 74 02                       ; 0xc0cbd
    or dl, dh                                 ; 08 f2                       ; 0xc0cbf vgabios.c:474
    shr dh, 1                                 ; d0 ee                       ; 0xc0cc1 vgabios.c:475
    shr cx, 002h                              ; c1 e9 02                    ; 0xc0cc3 vgabios.c:476
    inc bx                                    ; 43                          ; 0xc0cc6 vgabios.c:477
    jmp short 00cb6h                          ; eb ed                       ; 0xc0cc7
    mov al, dl                                ; 88 d0                       ; 0xc0cc9 vgabios.c:479
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0ccb
    pop cx                                    ; 59                          ; 0xc0cce
    pop bx                                    ; 5b                          ; 0xc0ccf
    pop bp                                    ; 5d                          ; 0xc0cd0
    retn                                      ; c3                          ; 0xc0cd1
  ; disGetNextSymbol 0xc0cd2 LB 0x38f3 -> off=0x0 cb=0000000000000084 uValue=00000000000c0cd2 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0cd2 LB 0x84
    push bp                                   ; 55                          ; 0xc0cd2 vgabios.c:481
    mov bp, sp                                ; 89 e5                       ; 0xc0cd3
    push cx                                   ; 51                          ; 0xc0cd5
    push si                                   ; 56                          ; 0xc0cd6
    push di                                   ; 57                          ; 0xc0cd7
    push ax                                   ; 50                          ; 0xc0cd8
    mov si, dx                                ; 89 d6                       ; 0xc0cd9
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0cdb vgabios.c:489
    je short 00d1ah                           ; 74 3a                       ; 0xc0cde
    mov bx, ax                                ; 89 c3                       ; 0xc0ce0 vgabios.c:491
    add bx, ax                                ; 01 c3                       ; 0xc0ce2
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0ce4
    xor cx, cx                                ; 31 c9                       ; 0xc0ce9 vgabios.c:493
    jmp short 00cf2h                          ; eb 05                       ; 0xc0ceb
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0ced
    jnl short 00d4eh                          ; 7d 5c                       ; 0xc0cf0
    mov ax, bx                                ; 89 d8                       ; 0xc0cf2 vgabios.c:494
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0cf4
    call 00c9dh                               ; e8 a3 ff                    ; 0xc0cf7
    mov di, si                                ; 89 f7                       ; 0xc0cfa
    inc si                                    ; 46                          ; 0xc0cfc
    push SS                                   ; 16                          ; 0xc0cfd
    pop ES                                    ; 07                          ; 0xc0cfe
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cff
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0d02 vgabios.c:495
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0d06
    call 00c9dh                               ; e8 91 ff                    ; 0xc0d09
    mov di, si                                ; 89 f7                       ; 0xc0d0c
    inc si                                    ; 46                          ; 0xc0d0e
    push SS                                   ; 16                          ; 0xc0d0f
    pop ES                                    ; 07                          ; 0xc0d10
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d11
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0d14 vgabios.c:496
    inc cx                                    ; 41                          ; 0xc0d17 vgabios.c:497
    jmp short 00cedh                          ; eb d3                       ; 0xc0d18
    mov bx, ax                                ; 89 c3                       ; 0xc0d1a vgabios.c:499
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0d1c
    xor cx, cx                                ; 31 c9                       ; 0xc0d21 vgabios.c:500
    jmp short 00d2ah                          ; eb 05                       ; 0xc0d23
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0d25
    jnl short 00d4eh                          ; 7d 24                       ; 0xc0d28
    mov di, si                                ; 89 f7                       ; 0xc0d2a vgabios.c:501
    inc si                                    ; 46                          ; 0xc0d2c
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0d2d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d30
    push SS                                   ; 16                          ; 0xc0d33
    pop ES                                    ; 07                          ; 0xc0d34
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d35
    mov di, si                                ; 89 f7                       ; 0xc0d38 vgabios.c:502
    inc si                                    ; 46                          ; 0xc0d3a
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0d3b
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0d3e
    push SS                                   ; 16                          ; 0xc0d43
    pop ES                                    ; 07                          ; 0xc0d44
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d45
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0d48 vgabios.c:503
    inc cx                                    ; 41                          ; 0xc0d4b vgabios.c:504
    jmp short 00d25h                          ; eb d7                       ; 0xc0d4c
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0d4e vgabios.c:506
    pop di                                    ; 5f                          ; 0xc0d51
    pop si                                    ; 5e                          ; 0xc0d52
    pop cx                                    ; 59                          ; 0xc0d53
    pop bp                                    ; 5d                          ; 0xc0d54
    retn                                      ; c3                          ; 0xc0d55
  ; disGetNextSymbol 0xc0d56 LB 0x386f -> off=0x0 cb=000000000000001a uValue=00000000000c0d56 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0d56 LB 0x1a
    push cx                                   ; 51                          ; 0xc0d56 vgabios.c:508
    push bp                                   ; 55                          ; 0xc0d57
    mov bp, sp                                ; 89 e5                       ; 0xc0d58
    mov cl, al                                ; 88 c1                       ; 0xc0d5a
    mov al, dl                                ; 88 d0                       ; 0xc0d5c
    xor ah, ah                                ; 30 e4                       ; 0xc0d5e vgabios.c:513
    mul bx                                    ; f7 e3                       ; 0xc0d60
    mov bx, ax                                ; 89 c3                       ; 0xc0d62
    sal bx, 002h                              ; c1 e3 02                    ; 0xc0d64
    mov al, cl                                ; 88 c8                       ; 0xc0d67
    xor ah, ah                                ; 30 e4                       ; 0xc0d69
    add ax, bx                                ; 01 d8                       ; 0xc0d6b
    pop bp                                    ; 5d                          ; 0xc0d6d vgabios.c:514
    pop cx                                    ; 59                          ; 0xc0d6e
    retn                                      ; c3                          ; 0xc0d6f
  ; disGetNextSymbol 0xc0d70 LB 0x3855 -> off=0x0 cb=0000000000000066 uValue=00000000000c0d70 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0d70 LB 0x66
    push bp                                   ; 55                          ; 0xc0d70 vgabios.c:516
    mov bp, sp                                ; 89 e5                       ; 0xc0d71
    push bx                                   ; 53                          ; 0xc0d73
    push cx                                   ; 51                          ; 0xc0d74
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0d75
    mov bl, dl                                ; 88 d3                       ; 0xc0d78 vgabios.c:522
    xor bh, bh                                ; 30 ff                       ; 0xc0d7a
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0d7c
    call 00cd2h                               ; e8 50 ff                    ; 0xc0d7f
    push strict byte 00008h                   ; 6a 08                       ; 0xc0d82 vgabios.c:525
    push 00080h                               ; 68 80 00                    ; 0xc0d84
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0d87 vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0d8a
    mov es, ax                                ; 8e c0                       ; 0xc0d8c
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d8e
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d91
    xor cx, cx                                ; 31 c9                       ; 0xc0d95 vgabios.c:68
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d97
    call 00ad6h                               ; e8 39 fd                    ; 0xc0d9a
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d9d
    test ah, 080h                             ; f6 c4 80                    ; 0xc0da0 vgabios.c:527
    jne short 00dcch                          ; 75 27                       ; 0xc0da3
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0da5 vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0da8
    mov es, ax                                ; 8e c0                       ; 0xc0daa
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0dac
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0daf
    test dx, dx                               ; 85 d2                       ; 0xc0db3 vgabios.c:531
    jne short 00dbbh                          ; 75 04                       ; 0xc0db5
    test ax, ax                               ; 85 c0                       ; 0xc0db7
    je short 00dcch                           ; 74 11                       ; 0xc0db9
    push strict byte 00008h                   ; 6a 08                       ; 0xc0dbb vgabios.c:532
    push 00080h                               ; 68 80 00                    ; 0xc0dbd
    mov cx, 00080h                            ; b9 80 00                    ; 0xc0dc0
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0dc3
    call 00ad6h                               ; e8 0d fd                    ; 0xc0dc6
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0dc9
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0dcc vgabios.c:535
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0dcf
    pop cx                                    ; 59                          ; 0xc0dd2
    pop bx                                    ; 5b                          ; 0xc0dd3
    pop bp                                    ; 5d                          ; 0xc0dd4
    retn                                      ; c3                          ; 0xc0dd5
  ; disGetNextSymbol 0xc0dd6 LB 0x37ef -> off=0x0 cb=0000000000000130 uValue=00000000000c0dd6 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0dd6 LB 0x130
    push bp                                   ; 55                          ; 0xc0dd6 vgabios.c:537
    mov bp, sp                                ; 89 e5                       ; 0xc0dd7
    push bx                                   ; 53                          ; 0xc0dd9
    push cx                                   ; 51                          ; 0xc0dda
    push si                                   ; 56                          ; 0xc0ddb
    push di                                   ; 57                          ; 0xc0ddc
    sub sp, strict byte 00014h                ; 83 ec 14                    ; 0xc0ddd
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0de0
    mov si, dx                                ; 89 d6                       ; 0xc0de3
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0de5 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0de8
    mov es, ax                                ; 8e c0                       ; 0xc0deb
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ded
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0df0 vgabios.c:48
    xor ah, ah                                ; 30 e4                       ; 0xc0df3 vgabios.c:545
    call 038c2h                               ; e8 ca 2a                    ; 0xc0df5
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0df8
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0dfb vgabios.c:546
    jne short 00e02h                          ; 75 03                       ; 0xc0dfd
    jmp near 00efdh                           ; e9 fb 00                    ; 0xc0dff
    mov cl, byte [bp-00eh]                    ; 8a 4e f2                    ; 0xc0e02 vgabios.c:550
    xor ch, ch                                ; 30 ed                       ; 0xc0e05
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc0e07
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc0e0a
    mov ax, cx                                ; 89 c8                       ; 0xc0e0d
    call 00a96h                               ; e8 84 fc                    ; 0xc0e0f
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc0e12 vgabios.c:551
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0e15
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc0e18 vgabios.c:552
    xor al, al                                ; 30 c0                       ; 0xc0e1b
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0e1d
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc0e20
    mov dl, byte [bp-016h]                    ; 8a 56 ea                    ; 0xc0e23
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0e26 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e29
    mov es, ax                                ; 8e c0                       ; 0xc0e2c
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0e2e
    xor ah, ah                                ; 30 e4                       ; 0xc0e31 vgabios.c:48
    inc ax                                    ; 40                          ; 0xc0e33
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc0e34
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0e37 vgabios.c:57
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0e3a
    mov word [bp-018h], di                    ; 89 7e e8                    ; 0xc0e3d vgabios.c:58
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc0e40 vgabios.c:558
    xor bh, bh                                ; 30 ff                       ; 0xc0e43
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0e45
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0e48
    jne short 00e7fh                          ; 75 30                       ; 0xc0e4d
    mov ax, di                                ; 89 f8                       ; 0xc0e4f vgabios.c:560
    mul word [bp-014h]                        ; f7 66 ec                    ; 0xc0e51
    add ax, ax                                ; 01 c0                       ; 0xc0e54
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc0e56
    inc ax                                    ; 40                          ; 0xc0e58
    mul cx                                    ; f7 e1                       ; 0xc0e59
    mov cx, ax                                ; 89 c1                       ; 0xc0e5b
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc0e5d
    xor ah, ah                                ; 30 e4                       ; 0xc0e60
    mul di                                    ; f7 e7                       ; 0xc0e62
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc0e64
    xor dh, dh                                ; 30 f6                       ; 0xc0e67
    mov di, ax                                ; 89 c7                       ; 0xc0e69
    add di, dx                                ; 01 d7                       ; 0xc0e6b
    add di, di                                ; 01 ff                       ; 0xc0e6d
    add di, cx                                ; 01 cf                       ; 0xc0e6f
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc0e71 vgabios.c:55
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0e75
    push SS                                   ; 16                          ; 0xc0e78 vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0e79
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0e7a
    jmp short 00dffh                          ; eb 80                       ; 0xc0e7d vgabios.c:562
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc0e7f vgabios.c:563
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0e83
    je short 00ed6h                           ; 74 4e                       ; 0xc0e86
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0e88
    jc short 00efdh                           ; 72 70                       ; 0xc0e8b
    jbe short 00e96h                          ; 76 07                       ; 0xc0e8d
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0e8f
    jbe short 00eafh                          ; 76 1b                       ; 0xc0e92
    jmp short 00efdh                          ; eb 67                       ; 0xc0e94
    xor dh, dh                                ; 30 f6                       ; 0xc0e96 vgabios.c:566
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0e98
    xor ah, ah                                ; 30 e4                       ; 0xc0e9b
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc0e9d
    call 00d56h                               ; e8 b3 fe                    ; 0xc0ea0
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc0ea3 vgabios.c:567
    xor dh, dh                                ; 30 f6                       ; 0xc0ea6
    call 00d70h                               ; e8 c5 fe                    ; 0xc0ea8
    xor ah, ah                                ; 30 e4                       ; 0xc0eab
    jmp short 00e78h                          ; eb c9                       ; 0xc0ead
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0eaf vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0eb2
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0eb5 vgabios.c:572
    mov byte [bp-011h], ch                    ; 88 6e ef                    ; 0xc0eb8
    push word [bp-012h]                       ; ff 76 ee                    ; 0xc0ebb
    xor dh, dh                                ; 30 f6                       ; 0xc0ebe
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0ec0
    xor ah, ah                                ; 30 e4                       ; 0xc0ec3
    mov bx, di                                ; 89 fb                       ; 0xc0ec5
    call 00b7ah                               ; e8 b0 fc                    ; 0xc0ec7
    mov bx, word [bp-012h]                    ; 8b 5e ee                    ; 0xc0eca vgabios.c:573
    mov dx, ax                                ; 89 c2                       ; 0xc0ecd
    mov ax, di                                ; 89 f8                       ; 0xc0ecf
    call 00ba9h                               ; e8 d5 fc                    ; 0xc0ed1
    jmp short 00eabh                          ; eb d5                       ; 0xc0ed4
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0ed6 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0ed9
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0edc vgabios.c:577
    mov byte [bp-011h], ch                    ; 88 6e ef                    ; 0xc0edf
    push word [bp-012h]                       ; ff 76 ee                    ; 0xc0ee2
    xor dh, dh                                ; 30 f6                       ; 0xc0ee5
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc0ee7
    xor ah, ah                                ; 30 e4                       ; 0xc0eea
    mov bx, di                                ; 89 fb                       ; 0xc0eec
    call 00be9h                               ; e8 f8 fc                    ; 0xc0eee
    mov bx, word [bp-012h]                    ; 8b 5e ee                    ; 0xc0ef1 vgabios.c:578
    mov dx, ax                                ; 89 c2                       ; 0xc0ef4
    mov ax, di                                ; 89 f8                       ; 0xc0ef6
    call 00c58h                               ; e8 5d fd                    ; 0xc0ef8
    jmp short 00eabh                          ; eb ae                       ; 0xc0efb
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0efd vgabios.c:587
    pop di                                    ; 5f                          ; 0xc0f00
    pop si                                    ; 5e                          ; 0xc0f01
    pop cx                                    ; 59                          ; 0xc0f02
    pop bx                                    ; 5b                          ; 0xc0f03
    pop bp                                    ; 5d                          ; 0xc0f04
    retn                                      ; c3                          ; 0xc0f05
  ; disGetNextSymbol 0xc0f06 LB 0x36bf -> off=0x10 cb=0000000000000083 uValue=00000000000c0f16 'vga_get_font_info'
    db  02dh, 00fh, 072h, 00fh, 077h, 00fh, 07eh, 00fh, 083h, 00fh, 088h, 00fh, 08dh, 00fh, 092h, 00fh
vga_get_font_info:                           ; 0xc0f16 LB 0x83
    push si                                   ; 56                          ; 0xc0f16 vgabios.c:589
    push di                                   ; 57                          ; 0xc0f17
    push bp                                   ; 55                          ; 0xc0f18
    mov bp, sp                                ; 89 e5                       ; 0xc0f19
    mov si, dx                                ; 89 d6                       ; 0xc0f1b
    mov di, bx                                ; 89 df                       ; 0xc0f1d
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0f1f vgabios.c:594
    jnbe short 00f6ch                         ; 77 48                       ; 0xc0f22
    mov bx, ax                                ; 89 c3                       ; 0xc0f24
    add bx, ax                                ; 01 c3                       ; 0xc0f26
    jmp word [cs:bx+00f06h]                   ; 2e ff a7 06 0f              ; 0xc0f28
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0f2d vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0f30
    mov es, ax                                ; 8e c0                       ; 0xc0f32
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0f34
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02                 ; 0xc0f37
    push SS                                   ; 16                          ; 0xc0f3b vgabios.c:597
    pop ES                                    ; 07                          ; 0xc0f3c
    mov word [es:di], dx                      ; 26 89 15                    ; 0xc0f3d
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0f40
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0f43
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f46
    mov es, ax                                ; 8e c0                       ; 0xc0f49
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f4b
    xor ah, ah                                ; 30 e4                       ; 0xc0f4e
    push SS                                   ; 16                          ; 0xc0f50
    pop ES                                    ; 07                          ; 0xc0f51
    mov bx, cx                                ; 89 cb                       ; 0xc0f52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f54
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0f57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f5a
    mov es, ax                                ; 8e c0                       ; 0xc0f5d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f5f
    xor ah, ah                                ; 30 e4                       ; 0xc0f62
    push SS                                   ; 16                          ; 0xc0f64
    pop ES                                    ; 07                          ; 0xc0f65
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0f66
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f69
    pop bp                                    ; 5d                          ; 0xc0f6c
    pop di                                    ; 5f                          ; 0xc0f6d
    pop si                                    ; 5e                          ; 0xc0f6e
    retn 00002h                               ; c2 02 00                    ; 0xc0f6f
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0f72 vgabios.c:67
    jmp short 00f30h                          ; eb b9                       ; 0xc0f75
    mov dx, 05d6ch                            ; ba 6c 5d                    ; 0xc0f77 vgabios.c:602
    mov ax, ds                                ; 8c d8                       ; 0xc0f7a
    jmp short 00f3bh                          ; eb bd                       ; 0xc0f7c vgabios.c:603
    mov dx, 0556ch                            ; ba 6c 55                    ; 0xc0f7e vgabios.c:605
    jmp short 00f7ah                          ; eb f7                       ; 0xc0f81
    mov dx, 0596ch                            ; ba 6c 59                    ; 0xc0f83 vgabios.c:608
    jmp short 00f7ah                          ; eb f2                       ; 0xc0f86
    mov dx, 07b6ch                            ; ba 6c 7b                    ; 0xc0f88 vgabios.c:611
    jmp short 00f7ah                          ; eb ed                       ; 0xc0f8b
    mov dx, 06b6ch                            ; ba 6c 6b                    ; 0xc0f8d vgabios.c:614
    jmp short 00f7ah                          ; eb e8                       ; 0xc0f90
    mov dx, 07c99h                            ; ba 99 7c                    ; 0xc0f92 vgabios.c:617
    jmp short 00f7ah                          ; eb e3                       ; 0xc0f95
    jmp short 00f6ch                          ; eb d3                       ; 0xc0f97 vgabios.c:623
  ; disGetNextSymbol 0xc0f99 LB 0x362c -> off=0x0 cb=0000000000000166 uValue=00000000000c0f99 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0f99 LB 0x166
    push bp                                   ; 55                          ; 0xc0f99 vgabios.c:636
    mov bp, sp                                ; 89 e5                       ; 0xc0f9a
    push si                                   ; 56                          ; 0xc0f9c
    push di                                   ; 57                          ; 0xc0f9d
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc0f9e
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0fa1
    mov si, dx                                ; 89 d6                       ; 0xc0fa4
    mov dx, bx                                ; 89 da                       ; 0xc0fa6
    mov word [bp-00ch], cx                    ; 89 4e f4                    ; 0xc0fa8
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0fab vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0fae
    mov es, ax                                ; 8e c0                       ; 0xc0fb1
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0fb3
    xor ah, ah                                ; 30 e4                       ; 0xc0fb6 vgabios.c:643
    call 038c2h                               ; e8 07 29                    ; 0xc0fb8
    mov ah, al                                ; 88 c4                       ; 0xc0fbb
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0fbd vgabios.c:644
    je short 00fcfh                           ; 74 0e                       ; 0xc0fbf
    mov bl, al                                ; 88 c3                       ; 0xc0fc1 vgabios.c:646
    xor bh, bh                                ; 30 ff                       ; 0xc0fc3
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0fc5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0fc8
    jne short 00fd2h                          ; 75 03                       ; 0xc0fcd
    jmp near 010f8h                           ; e9 26 01                    ; 0xc0fcf vgabios.c:647
    mov ch, byte [bx+047b0h]                  ; 8a af b0 47                 ; 0xc0fd2 vgabios.c:650
    cmp ch, 003h                              ; 80 fd 03                    ; 0xc0fd6
    jc short 00feah                           ; 72 0f                       ; 0xc0fd9
    jbe short 00ff2h                          ; 76 15                       ; 0xc0fdb
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc0fdd
    je short 01029h                           ; 74 47                       ; 0xc0fe0
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc0fe2
    je short 00ff2h                           ; 74 0b                       ; 0xc0fe5
    jmp near 010eeh                           ; e9 04 01                    ; 0xc0fe7
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc0fea
    je short 01060h                           ; 74 71                       ; 0xc0fed
    jmp near 010eeh                           ; e9 fc 00                    ; 0xc0fef
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0ff2 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0ff5
    mov es, ax                                ; 8e c0                       ; 0xc0ff8
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc0ffa
    mov ax, dx                                ; 89 d0                       ; 0xc0ffd vgabios.c:58
    mul bx                                    ; f7 e3                       ; 0xc0fff
    mov bx, si                                ; 89 f3                       ; 0xc1001
    shr bx, 003h                              ; c1 eb 03                    ; 0xc1003
    add bx, ax                                ; 01 c3                       ; 0xc1006
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc1008 vgabios.c:57
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc100b
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc100e vgabios.c:58
    xor dh, dh                                ; 30 f6                       ; 0xc1011
    mul dx                                    ; f7 e2                       ; 0xc1013
    add bx, ax                                ; 01 c3                       ; 0xc1015
    mov cx, si                                ; 89 f1                       ; 0xc1017 vgabios.c:655
    and cx, strict byte 00007h                ; 83 e1 07                    ; 0xc1019
    mov ax, 00080h                            ; b8 80 00                    ; 0xc101c
    sar ax, CL                                ; d3 f8                       ; 0xc101f
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1021
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc1024 vgabios.c:657
    jmp short 01032h                          ; eb 09                       ; 0xc1027
    jmp near 010ceh                           ; e9 a2 00                    ; 0xc1029
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc102c
    jnc short 0105dh                          ; 73 2b                       ; 0xc1030
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1032 vgabios.c:658
    xor ah, ah                                ; 30 e4                       ; 0xc1035
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1037
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc103a
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc103c
    out DX, ax                                ; ef                          ; 0xc103f
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1040 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc1043
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1045
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc1048 vgabios.c:48
    test al, al                               ; 84 c0                       ; 0xc104b vgabios.c:660
    jbe short 01058h                          ; 76 09                       ; 0xc104d
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc104f vgabios.c:661
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc1052
    sal al, CL                                ; d2 e0                       ; 0xc1054
    or ch, al                                 ; 08 c5                       ; 0xc1056
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1058 vgabios.c:662
    jmp short 0102ch                          ; eb cf                       ; 0xc105b
    jmp near 010f0h                           ; e9 90 00                    ; 0xc105d
    mov cl, byte [bx+047b1h]                  ; 8a 8f b1 47                 ; 0xc1060 vgabios.c:665
    xor ch, ch                                ; 30 ed                       ; 0xc1064
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc1066
    sub bx, cx                                ; 29 cb                       ; 0xc1069
    mov cx, bx                                ; 89 d9                       ; 0xc106b
    mov bx, si                                ; 89 f3                       ; 0xc106d
    shr bx, CL                                ; d3 eb                       ; 0xc106f
    mov cx, bx                                ; 89 d9                       ; 0xc1071
    mov bx, dx                                ; 89 d3                       ; 0xc1073
    shr bx, 1                                 ; d1 eb                       ; 0xc1075
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc1077
    add bx, cx                                ; 01 cb                       ; 0xc107a
    test dl, 001h                             ; f6 c2 01                    ; 0xc107c vgabios.c:666
    je short 01084h                           ; 74 03                       ; 0xc107f
    add bh, 020h                              ; 80 c7 20                    ; 0xc1081 vgabios.c:667
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1084 vgabios.c:47
    mov es, dx                                ; 8e c2                       ; 0xc1087
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1089
    mov bl, ah                                ; 88 e3                       ; 0xc108c vgabios.c:669
    xor bh, bh                                ; 30 ff                       ; 0xc108e
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1090
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc1093
    jne short 010b5h                          ; 75 1b                       ; 0xc1098
    mov cx, si                                ; 89 f1                       ; 0xc109a vgabios.c:670
    xor ch, ch                                ; 30 ed                       ; 0xc109c
    and cl, 003h                              ; 80 e1 03                    ; 0xc109e
    mov dx, strict word 00003h                ; ba 03 00                    ; 0xc10a1
    sub dx, cx                                ; 29 ca                       ; 0xc10a4
    mov cx, dx                                ; 89 d1                       ; 0xc10a6
    add cx, dx                                ; 01 d1                       ; 0xc10a8
    xor ah, ah                                ; 30 e4                       ; 0xc10aa
    sar ax, CL                                ; d3 f8                       ; 0xc10ac
    mov ch, al                                ; 88 c5                       ; 0xc10ae
    and ch, 003h                              ; 80 e5 03                    ; 0xc10b0
    jmp short 010f0h                          ; eb 3b                       ; 0xc10b3 vgabios.c:671
    mov cx, si                                ; 89 f1                       ; 0xc10b5 vgabios.c:672
    xor ch, ch                                ; 30 ed                       ; 0xc10b7
    and cl, 007h                              ; 80 e1 07                    ; 0xc10b9
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc10bc
    sub dx, cx                                ; 29 ca                       ; 0xc10bf
    mov cx, dx                                ; 89 d1                       ; 0xc10c1
    xor ah, ah                                ; 30 e4                       ; 0xc10c3
    sar ax, CL                                ; d3 f8                       ; 0xc10c5
    mov ch, al                                ; 88 c5                       ; 0xc10c7
    and ch, 001h                              ; 80 e5 01                    ; 0xc10c9
    jmp short 010f0h                          ; eb 22                       ; 0xc10cc vgabios.c:673
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc10ce vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc10d1
    mov es, ax                                ; 8e c0                       ; 0xc10d4
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc10d6
    sal bx, 003h                              ; c1 e3 03                    ; 0xc10d9 vgabios.c:58
    mov ax, dx                                ; 89 d0                       ; 0xc10dc
    mul bx                                    ; f7 e3                       ; 0xc10de
    mov bx, si                                ; 89 f3                       ; 0xc10e0
    add bx, ax                                ; 01 c3                       ; 0xc10e2
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc10e4 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc10e7
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc10e9
    jmp short 010f0h                          ; eb 02                       ; 0xc10ec vgabios.c:677
    xor ch, ch                                ; 30 ed                       ; 0xc10ee vgabios.c:682
    push SS                                   ; 16                          ; 0xc10f0 vgabios.c:684
    pop ES                                    ; 07                          ; 0xc10f1
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc10f2
    mov byte [es:bx], ch                      ; 26 88 2f                    ; 0xc10f5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc10f8 vgabios.c:685
    pop di                                    ; 5f                          ; 0xc10fb
    pop si                                    ; 5e                          ; 0xc10fc
    pop bp                                    ; 5d                          ; 0xc10fd
    retn                                      ; c3                          ; 0xc10fe
  ; disGetNextSymbol 0xc10ff LB 0x34c6 -> off=0x0 cb=000000000000008d uValue=00000000000c10ff 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc10ff LB 0x8d
    push bp                                   ; 55                          ; 0xc10ff vgabios.c:690
    mov bp, sp                                ; 89 e5                       ; 0xc1100
    push bx                                   ; 53                          ; 0xc1102
    push cx                                   ; 51                          ; 0xc1103
    push si                                   ; 56                          ; 0xc1104
    push di                                   ; 57                          ; 0xc1105
    push ax                                   ; 50                          ; 0xc1106
    push ax                                   ; 50                          ; 0xc1107
    mov bx, ax                                ; 89 c3                       ; 0xc1108
    mov di, dx                                ; 89 d7                       ; 0xc110a
    mov dx, 003dah                            ; ba da 03                    ; 0xc110c vgabios.c:695
    in AL, DX                                 ; ec                          ; 0xc110f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1110
    xor al, al                                ; 30 c0                       ; 0xc1112 vgabios.c:696
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1114
    out DX, AL                                ; ee                          ; 0xc1117
    xor si, si                                ; 31 f6                       ; 0xc1118 vgabios.c:698
    cmp si, di                                ; 39 fe                       ; 0xc111a
    jnc short 01171h                          ; 73 53                       ; 0xc111c
    mov al, bl                                ; 88 d8                       ; 0xc111e vgabios.c:701
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc1120
    out DX, AL                                ; ee                          ; 0xc1123
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1124 vgabios.c:703
    in AL, DX                                 ; ec                          ; 0xc1127
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1128
    mov cx, ax                                ; 89 c1                       ; 0xc112a
    in AL, DX                                 ; ec                          ; 0xc112c vgabios.c:704
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc112d
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc112f
    in AL, DX                                 ; ec                          ; 0xc1132 vgabios.c:705
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1133
    xor ch, ch                                ; 30 ed                       ; 0xc1135 vgabios.c:708
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d                    ; 0xc1137
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc113a
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc113d
    xor ch, ch                                ; 30 ed                       ; 0xc1140
    imul cx, cx, 00097h                       ; 69 c9 97 00                 ; 0xc1142
    add cx, word [bp-00ah]                    ; 03 4e f6                    ; 0xc1146
    xor ah, ah                                ; 30 e4                       ; 0xc1149
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c                    ; 0xc114b
    add cx, ax                                ; 01 c1                       ; 0xc114e
    add cx, 00080h                            ; 81 c1 80 00                 ; 0xc1150
    sar cx, 008h                              ; c1 f9 08                    ; 0xc1154
    cmp cx, strict byte 0003fh                ; 83 f9 3f                    ; 0xc1157 vgabios.c:710
    jbe short 0115fh                          ; 76 03                       ; 0xc115a
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc115c
    mov al, bl                                ; 88 d8                       ; 0xc115f vgabios.c:713
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc1161
    out DX, AL                                ; ee                          ; 0xc1164
    mov al, cl                                ; 88 c8                       ; 0xc1165 vgabios.c:715
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1167
    out DX, AL                                ; ee                          ; 0xc116a
    out DX, AL                                ; ee                          ; 0xc116b vgabios.c:716
    out DX, AL                                ; ee                          ; 0xc116c vgabios.c:717
    inc bx                                    ; 43                          ; 0xc116d vgabios.c:718
    inc si                                    ; 46                          ; 0xc116e vgabios.c:719
    jmp short 0111ah                          ; eb a9                       ; 0xc116f
    mov dx, 003dah                            ; ba da 03                    ; 0xc1171 vgabios.c:720
    in AL, DX                                 ; ec                          ; 0xc1174
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1175
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc1177 vgabios.c:721
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1179
    out DX, AL                                ; ee                          ; 0xc117c
    mov dx, 003dah                            ; ba da 03                    ; 0xc117d vgabios.c:723
    in AL, DX                                 ; ec                          ; 0xc1180
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1181
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1183 vgabios.c:725
    pop di                                    ; 5f                          ; 0xc1186
    pop si                                    ; 5e                          ; 0xc1187
    pop cx                                    ; 59                          ; 0xc1188
    pop bx                                    ; 5b                          ; 0xc1189
    pop bp                                    ; 5d                          ; 0xc118a
    retn                                      ; c3                          ; 0xc118b
  ; disGetNextSymbol 0xc118c LB 0x3439 -> off=0x0 cb=0000000000000107 uValue=00000000000c118c 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc118c LB 0x107
    push bp                                   ; 55                          ; 0xc118c vgabios.c:728
    mov bp, sp                                ; 89 e5                       ; 0xc118d
    push bx                                   ; 53                          ; 0xc118f
    push cx                                   ; 51                          ; 0xc1190
    push si                                   ; 56                          ; 0xc1191
    push ax                                   ; 50                          ; 0xc1192
    push ax                                   ; 50                          ; 0xc1193
    mov bl, al                                ; 88 c3                       ; 0xc1194
    mov ah, dl                                ; 88 d4                       ; 0xc1196
    mov dl, al                                ; 88 c2                       ; 0xc1198 vgabios.c:734
    xor dh, dh                                ; 30 f6                       ; 0xc119a
    mov cx, dx                                ; 89 d1                       ; 0xc119c
    sal cx, 008h                              ; c1 e1 08                    ; 0xc119e
    mov dl, ah                                ; 88 e2                       ; 0xc11a1
    add dx, cx                                ; 01 ca                       ; 0xc11a3
    mov si, strict word 00060h                ; be 60 00                    ; 0xc11a5 vgabios.c:62
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc11a8
    mov es, cx                                ; 8e c1                       ; 0xc11ab
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc11ad
    mov si, 00087h                            ; be 87 00                    ; 0xc11b0 vgabios.c:47
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc11b3
    test dl, 008h                             ; f6 c2 08                    ; 0xc11b6 vgabios.c:48
    jne short 011f8h                          ; 75 3d                       ; 0xc11b9
    mov dl, al                                ; 88 c2                       ; 0xc11bb vgabios.c:740
    and dl, 060h                              ; 80 e2 60                    ; 0xc11bd
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc11c0
    jne short 011cbh                          ; 75 06                       ; 0xc11c3
    mov BL, strict byte 01eh                  ; b3 1e                       ; 0xc11c5 vgabios.c:742
    xor ah, ah                                ; 30 e4                       ; 0xc11c7 vgabios.c:743
    jmp short 011f8h                          ; eb 2d                       ; 0xc11c9 vgabios.c:744
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc11cb vgabios.c:47
    test dl, 001h                             ; f6 c2 01                    ; 0xc11ce vgabios.c:48
    jne short 0122dh                          ; 75 5a                       ; 0xc11d1
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc11d3
    jnc short 0122dh                          ; 73 55                       ; 0xc11d6
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc11d8
    jnc short 0122dh                          ; 73 50                       ; 0xc11db
    mov si, 00085h                            ; be 85 00                    ; 0xc11dd vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc11e0
    mov es, dx                                ; 8e c2                       ; 0xc11e3
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc11e5
    mov dx, cx                                ; 89 ca                       ; 0xc11e8 vgabios.c:58
    cmp ah, bl                                ; 38 dc                       ; 0xc11ea vgabios.c:755
    jnc short 011fah                          ; 73 0c                       ; 0xc11ec
    test ah, ah                               ; 84 e4                       ; 0xc11ee vgabios.c:757
    je short 0122dh                           ; 74 3b                       ; 0xc11f0
    xor bl, bl                                ; 30 db                       ; 0xc11f2 vgabios.c:758
    mov ah, cl                                ; 88 cc                       ; 0xc11f4 vgabios.c:759
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc11f6
    jmp short 0122dh                          ; eb 33                       ; 0xc11f8 vgabios.c:761
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc11fa vgabios.c:762
    xor al, al                                ; 30 c0                       ; 0xc11fd
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc11ff
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1202
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc1205
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc1208
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc120b
    cmp si, cx                                ; 39 ce                       ; 0xc120e
    jnc short 0122fh                          ; 73 1d                       ; 0xc1210
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc1212
    mov byte [bp-009h], al                    ; 88 46 f7                    ; 0xc1215
    mov si, cx                                ; 89 ce                       ; 0xc1218
    dec si                                    ; 4e                          ; 0xc121a
    cmp si, word [bp-00ah]                    ; 3b 76 f6                    ; 0xc121b
    je short 01269h                           ; 74 49                       ; 0xc121e
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc1220
    mov byte [bp-007h], al                    ; 88 46 f9                    ; 0xc1223
    dec cx                                    ; 49                          ; 0xc1226
    dec cx                                    ; 49                          ; 0xc1227
    cmp cx, word [bp-008h]                    ; 3b 4e f8                    ; 0xc1228
    jne short 0122fh                          ; 75 02                       ; 0xc122b
    jmp short 01269h                          ; eb 3a                       ; 0xc122d
    cmp ah, 003h                              ; 80 fc 03                    ; 0xc122f vgabios.c:764
    jbe short 01269h                          ; 76 35                       ; 0xc1232
    mov cl, bl                                ; 88 d9                       ; 0xc1234 vgabios.c:765
    xor ch, ch                                ; 30 ed                       ; 0xc1236
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc1238
    mov byte [bp-009h], ch                    ; 88 6e f7                    ; 0xc123b
    mov si, cx                                ; 89 ce                       ; 0xc123e
    inc si                                    ; 46                          ; 0xc1240
    inc si                                    ; 46                          ; 0xc1241
    mov cl, dl                                ; 88 d1                       ; 0xc1242
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc1244
    cmp si, word [bp-00ah]                    ; 3b 76 f6                    ; 0xc1246
    jl short 0125eh                           ; 7c 13                       ; 0xc1249
    sub bl, ah                                ; 28 e3                       ; 0xc124b vgabios.c:767
    add bl, dl                                ; 00 d3                       ; 0xc124d
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc124f
    mov ah, cl                                ; 88 cc                       ; 0xc1251 vgabios.c:768
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc1253 vgabios.c:769
    jc short 01269h                           ; 72 11                       ; 0xc1256
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc1258 vgabios.c:771
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc125a vgabios.c:772
    jmp short 01269h                          ; eb 0b                       ; 0xc125c vgabios.c:774
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc125e
    jbe short 01267h                          ; 76 04                       ; 0xc1261
    shr dx, 1                                 ; d1 ea                       ; 0xc1263 vgabios.c:776
    mov bl, dl                                ; 88 d3                       ; 0xc1265
    mov ah, cl                                ; 88 cc                       ; 0xc1267 vgabios.c:780
    mov si, strict word 00063h                ; be 63 00                    ; 0xc1269 vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc126c
    mov es, dx                                ; 8e c2                       ; 0xc126f
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc1271
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc1274 vgabios.c:791
    mov dx, cx                                ; 89 ca                       ; 0xc1276
    out DX, AL                                ; ee                          ; 0xc1278
    mov si, cx                                ; 89 ce                       ; 0xc1279 vgabios.c:792
    inc si                                    ; 46                          ; 0xc127b
    mov al, bl                                ; 88 d8                       ; 0xc127c
    mov dx, si                                ; 89 f2                       ; 0xc127e
    out DX, AL                                ; ee                          ; 0xc1280
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc1281 vgabios.c:793
    mov dx, cx                                ; 89 ca                       ; 0xc1283
    out DX, AL                                ; ee                          ; 0xc1285
    mov al, ah                                ; 88 e0                       ; 0xc1286 vgabios.c:794
    mov dx, si                                ; 89 f2                       ; 0xc1288
    out DX, AL                                ; ee                          ; 0xc128a
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc128b vgabios.c:795
    pop si                                    ; 5e                          ; 0xc128e
    pop cx                                    ; 59                          ; 0xc128f
    pop bx                                    ; 5b                          ; 0xc1290
    pop bp                                    ; 5d                          ; 0xc1291
    retn                                      ; c3                          ; 0xc1292
  ; disGetNextSymbol 0xc1293 LB 0x3332 -> off=0x0 cb=000000000000008f uValue=00000000000c1293 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc1293 LB 0x8f
    push bp                                   ; 55                          ; 0xc1293 vgabios.c:798
    mov bp, sp                                ; 89 e5                       ; 0xc1294
    push bx                                   ; 53                          ; 0xc1296
    push cx                                   ; 51                          ; 0xc1297
    push si                                   ; 56                          ; 0xc1298
    push di                                   ; 57                          ; 0xc1299
    push ax                                   ; 50                          ; 0xc129a
    mov bl, al                                ; 88 c3                       ; 0xc129b
    mov cx, dx                                ; 89 d1                       ; 0xc129d
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc129f vgabios.c:804
    jnbe short 01319h                         ; 77 76                       ; 0xc12a1
    xor ah, ah                                ; 30 e4                       ; 0xc12a3 vgabios.c:807
    mov si, ax                                ; 89 c6                       ; 0xc12a5
    add si, ax                                ; 01 c6                       ; 0xc12a7
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc12a9
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12ac vgabios.c:62
    mov es, ax                                ; 8e c0                       ; 0xc12af
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc12b1
    mov si, strict word 00062h                ; be 62 00                    ; 0xc12b4 vgabios.c:47
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc12b7
    cmp bl, al                                ; 38 c3                       ; 0xc12ba vgabios.c:811
    jne short 01319h                          ; 75 5b                       ; 0xc12bc
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc12be vgabios.c:57
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc12c1
    mov si, 00084h                            ; be 84 00                    ; 0xc12c4 vgabios.c:47
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc12c7
    xor ah, ah                                ; 30 e4                       ; 0xc12ca vgabios.c:48
    mov si, ax                                ; 89 c6                       ; 0xc12cc
    inc si                                    ; 46                          ; 0xc12ce
    mov ax, dx                                ; 89 d0                       ; 0xc12cf vgabios.c:817
    xor al, dl                                ; 30 d0                       ; 0xc12d1
    shr ax, 008h                              ; c1 e8 08                    ; 0xc12d3
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc12d6
    mov ax, di                                ; 89 f8                       ; 0xc12d9 vgabios.c:820
    mul si                                    ; f7 e6                       ; 0xc12db
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc12dd
    xor bh, bh                                ; 30 ff                       ; 0xc12df
    inc ax                                    ; 40                          ; 0xc12e1
    mul bx                                    ; f7 e3                       ; 0xc12e2
    mov bl, cl                                ; 88 cb                       ; 0xc12e4
    mov si, bx                                ; 89 de                       ; 0xc12e6
    add si, ax                                ; 01 c6                       ; 0xc12e8
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc12ea
    xor ah, ah                                ; 30 e4                       ; 0xc12ed
    mul di                                    ; f7 e7                       ; 0xc12ef
    add si, ax                                ; 01 c6                       ; 0xc12f1
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc12f3 vgabios.c:57
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc12f6
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc12f9 vgabios.c:824
    mov dx, bx                                ; 89 da                       ; 0xc12fb
    out DX, AL                                ; ee                          ; 0xc12fd
    mov ax, si                                ; 89 f0                       ; 0xc12fe vgabios.c:825
    xor al, al                                ; 30 c0                       ; 0xc1300
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1302
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc1305
    mov dx, cx                                ; 89 ca                       ; 0xc1308
    out DX, AL                                ; ee                          ; 0xc130a
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc130b vgabios.c:826
    mov dx, bx                                ; 89 da                       ; 0xc130d
    out DX, AL                                ; ee                          ; 0xc130f
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc1310 vgabios.c:827
    mov ax, si                                ; 89 f0                       ; 0xc1314
    mov dx, cx                                ; 89 ca                       ; 0xc1316
    out DX, AL                                ; ee                          ; 0xc1318
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1319 vgabios.c:829
    pop di                                    ; 5f                          ; 0xc131c
    pop si                                    ; 5e                          ; 0xc131d
    pop cx                                    ; 59                          ; 0xc131e
    pop bx                                    ; 5b                          ; 0xc131f
    pop bp                                    ; 5d                          ; 0xc1320
    retn                                      ; c3                          ; 0xc1321
  ; disGetNextSymbol 0xc1322 LB 0x32a3 -> off=0x0 cb=00000000000000d8 uValue=00000000000c1322 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc1322 LB 0xd8
    push bp                                   ; 55                          ; 0xc1322 vgabios.c:832
    mov bp, sp                                ; 89 e5                       ; 0xc1323
    push bx                                   ; 53                          ; 0xc1325
    push cx                                   ; 51                          ; 0xc1326
    push dx                                   ; 52                          ; 0xc1327
    push si                                   ; 56                          ; 0xc1328
    push di                                   ; 57                          ; 0xc1329
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc132a
    mov cl, al                                ; 88 c1                       ; 0xc132d
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc132f vgabios.c:838
    jnbe short 01349h                         ; 77 16                       ; 0xc1331
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1333 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1336
    mov es, ax                                ; 8e c0                       ; 0xc1339
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc133b
    xor ah, ah                                ; 30 e4                       ; 0xc133e vgabios.c:842
    call 038c2h                               ; e8 7f 25                    ; 0xc1340
    mov ch, al                                ; 88 c5                       ; 0xc1343
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1345 vgabios.c:843
    jne short 0134ch                          ; 75 03                       ; 0xc1347
    jmp near 013f0h                           ; e9 a4 00                    ; 0xc1349
    mov al, cl                                ; 88 c8                       ; 0xc134c vgabios.c:846
    xor ah, ah                                ; 30 e4                       ; 0xc134e
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc1350
    lea dx, [bp-010h]                         ; 8d 56 f0                    ; 0xc1353
    call 00a96h                               ; e8 3d f7                    ; 0xc1356
    mov bl, ch                                ; 88 eb                       ; 0xc1359 vgabios.c:848
    xor bh, bh                                ; 30 ff                       ; 0xc135b
    mov si, bx                                ; 89 de                       ; 0xc135d
    sal si, 003h                              ; c1 e6 03                    ; 0xc135f
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc1362
    jne short 013a8h                          ; 75 3f                       ; 0xc1367
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1369 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc136c
    mov es, ax                                ; 8e c0                       ; 0xc136f
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc1371
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1374 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1377
    xor ah, ah                                ; 30 e4                       ; 0xc137a vgabios.c:48
    mov bx, ax                                ; 89 c3                       ; 0xc137c
    inc bx                                    ; 43                          ; 0xc137e
    mov ax, dx                                ; 89 d0                       ; 0xc137f vgabios.c:855
    mul bx                                    ; f7 e3                       ; 0xc1381
    mov di, ax                                ; 89 c7                       ; 0xc1383
    add ax, ax                                ; 01 c0                       ; 0xc1385
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1387
    mov byte [bp-00ch], cl                    ; 88 4e f4                    ; 0xc1389
    mov byte [bp-00bh], 000h                  ; c6 46 f5 00                 ; 0xc138c
    inc ax                                    ; 40                          ; 0xc1390
    mul word [bp-00ch]                        ; f7 66 f4                    ; 0xc1391
    mov bx, ax                                ; 89 c3                       ; 0xc1394
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc1396 vgabios.c:62
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc1399
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc139c vgabios.c:859
    lea ax, [di+001h]                         ; 8d 45 01                    ; 0xc13a0
    mul word [bp-00ch]                        ; f7 66 f4                    ; 0xc13a3
    jmp short 013b7h                          ; eb 0f                       ; 0xc13a6 vgabios.c:861
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc13a8 vgabios.c:863
    sal bx, 006h                              ; c1 e3 06                    ; 0xc13ac
    mov al, cl                                ; 88 c8                       ; 0xc13af
    xor ah, ah                                ; 30 e4                       ; 0xc13b1
    mul word [bx+04845h]                      ; f7 a7 45 48                 ; 0xc13b3
    mov bx, ax                                ; 89 c3                       ; 0xc13b7
    mov si, strict word 00063h                ; be 63 00                    ; 0xc13b9 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc13bc
    mov es, ax                                ; 8e c0                       ; 0xc13bf
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc13c1
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc13c4 vgabios.c:868
    mov dx, si                                ; 89 f2                       ; 0xc13c6
    out DX, AL                                ; ee                          ; 0xc13c8
    mov ax, bx                                ; 89 d8                       ; 0xc13c9 vgabios.c:869
    xor al, bl                                ; 30 d8                       ; 0xc13cb
    shr ax, 008h                              ; c1 e8 08                    ; 0xc13cd
    lea di, [si+001h]                         ; 8d 7c 01                    ; 0xc13d0
    mov dx, di                                ; 89 fa                       ; 0xc13d3
    out DX, AL                                ; ee                          ; 0xc13d5
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc13d6 vgabios.c:870
    mov dx, si                                ; 89 f2                       ; 0xc13d8
    out DX, AL                                ; ee                          ; 0xc13da
    xor bh, bh                                ; 30 ff                       ; 0xc13db vgabios.c:871
    mov ax, bx                                ; 89 d8                       ; 0xc13dd
    mov dx, di                                ; 89 fa                       ; 0xc13df
    out DX, AL                                ; ee                          ; 0xc13e1
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc13e2 vgabios.c:52
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc13e5
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc13e8 vgabios.c:881
    mov al, cl                                ; 88 c8                       ; 0xc13eb
    call 01293h                               ; e8 a3 fe                    ; 0xc13ed
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc13f0 vgabios.c:882
    pop di                                    ; 5f                          ; 0xc13f3
    pop si                                    ; 5e                          ; 0xc13f4
    pop dx                                    ; 5a                          ; 0xc13f5
    pop cx                                    ; 59                          ; 0xc13f6
    pop bx                                    ; 5b                          ; 0xc13f7
    pop bp                                    ; 5d                          ; 0xc13f8
    retn                                      ; c3                          ; 0xc13f9
  ; disGetNextSymbol 0xc13fa LB 0x31cb -> off=0x0 cb=0000000000000045 uValue=00000000000c13fa 'find_vpti'
find_vpti:                                   ; 0xc13fa LB 0x45
    push bx                                   ; 53                          ; 0xc13fa vgabios.c:917
    push si                                   ; 56                          ; 0xc13fb
    push bp                                   ; 55                          ; 0xc13fc
    mov bp, sp                                ; 89 e5                       ; 0xc13fd
    mov bl, al                                ; 88 c3                       ; 0xc13ff vgabios.c:922
    xor bh, bh                                ; 30 ff                       ; 0xc1401
    mov si, bx                                ; 89 de                       ; 0xc1403
    sal si, 003h                              ; c1 e6 03                    ; 0xc1405
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc1408
    jne short 01435h                          ; 75 26                       ; 0xc140d
    mov si, 00089h                            ; be 89 00                    ; 0xc140f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1412
    mov es, ax                                ; 8e c0                       ; 0xc1415
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc1417
    test AL, strict byte 010h                 ; a8 10                       ; 0xc141a vgabios.c:924
    je short 01424h                           ; 74 06                       ; 0xc141c
    mov al, byte [bx+07df5h]                  ; 8a 87 f5 7d                 ; 0xc141e vgabios.c:925
    jmp short 01432h                          ; eb 0e                       ; 0xc1422 vgabios.c:926
    test AL, strict byte 080h                 ; a8 80                       ; 0xc1424
    je short 0142eh                           ; 74 06                       ; 0xc1426
    mov al, byte [bx+07de5h]                  ; 8a 87 e5 7d                 ; 0xc1428 vgabios.c:927
    jmp short 01432h                          ; eb 04                       ; 0xc142c vgabios.c:928
    mov al, byte [bx+07dedh]                  ; 8a 87 ed 7d                 ; 0xc142e vgabios.c:929
    cbw                                       ; 98                          ; 0xc1432
    jmp short 0143bh                          ; eb 06                       ; 0xc1433 vgabios.c:930
    mov al, byte [bx+0482eh]                  ; 8a 87 2e 48                 ; 0xc1435 vgabios.c:931
    xor ah, ah                                ; 30 e4                       ; 0xc1439
    pop bp                                    ; 5d                          ; 0xc143b vgabios.c:934
    pop si                                    ; 5e                          ; 0xc143c
    pop bx                                    ; 5b                          ; 0xc143d
    retn                                      ; c3                          ; 0xc143e
  ; disGetNextSymbol 0xc143f LB 0x3186 -> off=0x0 cb=00000000000004a3 uValue=00000000000c143f 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc143f LB 0x4a3
    push bp                                   ; 55                          ; 0xc143f vgabios.c:938
    mov bp, sp                                ; 89 e5                       ; 0xc1440
    push bx                                   ; 53                          ; 0xc1442
    push cx                                   ; 51                          ; 0xc1443
    push dx                                   ; 52                          ; 0xc1444
    push si                                   ; 56                          ; 0xc1445
    push di                                   ; 57                          ; 0xc1446
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1447
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc144a
    and AL, strict byte 080h                  ; 24 80                       ; 0xc144d vgabios.c:942
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc144f
    call 007bfh                               ; e8 6a f3                    ; 0xc1452 vgabios.c:952
    test ax, ax                               ; 85 c0                       ; 0xc1455
    je short 01465h                           ; 74 0c                       ; 0xc1457
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc1459 vgabios.c:954
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc145b
    out DX, AL                                ; ee                          ; 0xc145e
    xor al, al                                ; 30 c0                       ; 0xc145f vgabios.c:955
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1461
    out DX, AL                                ; ee                          ; 0xc1464
    and byte [bp-010h], 07fh                  ; 80 66 f0 7f                 ; 0xc1465 vgabios.c:960
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1469 vgabios.c:966
    xor ah, ah                                ; 30 e4                       ; 0xc146c
    call 038c2h                               ; e8 51 24                    ; 0xc146e
    mov cl, al                                ; 88 c1                       ; 0xc1471
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1473
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1476 vgabios.c:972
    je short 014e5h                           ; 74 6b                       ; 0xc1478
    mov bx, 000a8h                            ; bb a8 00                    ; 0xc147a vgabios.c:67
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc147d
    mov es, ax                                ; 8e c0                       ; 0xc1480
    mov di, word [es:bx]                      ; 26 8b 3f                    ; 0xc1482
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02                 ; 0xc1485
    mov bx, di                                ; 89 fb                       ; 0xc1489 vgabios.c:68
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc148b
    xor ch, ch                                ; 30 ed                       ; 0xc148e vgabios.c:978
    mov ax, cx                                ; 89 c8                       ; 0xc1490
    call 013fah                               ; e8 65 ff                    ; 0xc1492
    mov es, [bp-018h]                         ; 8e 46 e8                    ; 0xc1495 vgabios.c:979
    mov si, word [es:di]                      ; 26 8b 35                    ; 0xc1498
    mov dx, word [es:di+002h]                 ; 26 8b 55 02                 ; 0xc149b
    mov word [bp-01eh], dx                    ; 89 56 e2                    ; 0xc149f
    xor ah, ah                                ; 30 e4                       ; 0xc14a2 vgabios.c:980
    sal ax, 006h                              ; c1 e0 06                    ; 0xc14a4
    add si, ax                                ; 01 c6                       ; 0xc14a7
    mov di, 00089h                            ; bf 89 00                    ; 0xc14a9 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc14ac
    mov es, ax                                ; 8e c0                       ; 0xc14af
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc14b1
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc14b4 vgabios.c:48
    test AL, strict byte 008h                 ; a8 08                       ; 0xc14b7 vgabios.c:997
    jne short 01501h                          ; 75 46                       ; 0xc14b9
    mov di, cx                                ; 89 cf                       ; 0xc14bb vgabios.c:999
    sal di, 003h                              ; c1 e7 03                    ; 0xc14bd
    mov al, byte [di+047b4h]                  ; 8a 85 b4 47                 ; 0xc14c0
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc14c4
    out DX, AL                                ; ee                          ; 0xc14c7
    xor al, al                                ; 30 c0                       ; 0xc14c8 vgabios.c:1002
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc14ca
    out DX, AL                                ; ee                          ; 0xc14cd
    mov cl, byte [di+047b5h]                  ; 8a 8d b5 47                 ; 0xc14ce vgabios.c:1005
    cmp cl, 001h                              ; 80 f9 01                    ; 0xc14d2
    jc short 014e8h                           ; 72 11                       ; 0xc14d5
    jbe short 014f3h                          ; 76 1a                       ; 0xc14d7
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc14d9
    je short 01504h                           ; 74 26                       ; 0xc14dc
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc14de
    je short 014fah                           ; 74 17                       ; 0xc14e1
    jmp short 01509h                          ; eb 24                       ; 0xc14e3
    jmp near 018d8h                           ; e9 f0 03                    ; 0xc14e5
    test cl, cl                               ; 84 c9                       ; 0xc14e8
    jne short 01509h                          ; 75 1d                       ; 0xc14ea
    mov word [bp-014h], 04fc2h                ; c7 46 ec c2 4f              ; 0xc14ec vgabios.c:1007
    jmp short 01509h                          ; eb 16                       ; 0xc14f1 vgabios.c:1008
    mov word [bp-014h], 05082h                ; c7 46 ec 82 50              ; 0xc14f3 vgabios.c:1010
    jmp short 01509h                          ; eb 0f                       ; 0xc14f8 vgabios.c:1011
    mov word [bp-014h], 05142h                ; c7 46 ec 42 51              ; 0xc14fa vgabios.c:1013
    jmp short 01509h                          ; eb 08                       ; 0xc14ff vgabios.c:1014
    jmp near 01578h                           ; e9 74 00                    ; 0xc1501
    mov word [bp-014h], 05202h                ; c7 46 ec 02 52              ; 0xc1504 vgabios.c:1016
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1509 vgabios.c:1020
    xor ah, ah                                ; 30 e4                       ; 0xc150c
    mov di, ax                                ; 89 c7                       ; 0xc150e
    sal di, 003h                              ; c1 e7 03                    ; 0xc1510
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc1513
    jne short 01529h                          ; 75 0f                       ; 0xc1518
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc151a vgabios.c:1022
    cmp byte [es:si+002h], 008h               ; 26 80 7c 02 08              ; 0xc151d
    jne short 01529h                          ; 75 05                       ; 0xc1522
    mov word [bp-014h], 05082h                ; c7 46 ec 82 50              ; 0xc1524 vgabios.c:1023
    xor cx, cx                                ; 31 c9                       ; 0xc1529 vgabios.c:1026
    jmp short 0153ch                          ; eb 0f                       ; 0xc152b
    xor al, al                                ; 30 c0                       ; 0xc152d vgabios.c:1033
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc152f
    out DX, AL                                ; ee                          ; 0xc1532
    out DX, AL                                ; ee                          ; 0xc1533 vgabios.c:1034
    out DX, AL                                ; ee                          ; 0xc1534 vgabios.c:1035
    inc cx                                    ; 41                          ; 0xc1535 vgabios.c:1037
    cmp cx, 00100h                            ; 81 f9 00 01                 ; 0xc1536
    jnc short 0156ah                          ; 73 2e                       ; 0xc153a
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc153c
    xor ah, ah                                ; 30 e4                       ; 0xc153f
    mov di, ax                                ; 89 c7                       ; 0xc1541
    sal di, 003h                              ; c1 e7 03                    ; 0xc1543
    mov al, byte [di+047b5h]                  ; 8a 85 b5 47                 ; 0xc1546
    mov di, ax                                ; 89 c7                       ; 0xc154a
    mov al, byte [di+0483eh]                  ; 8a 85 3e 48                 ; 0xc154c
    cmp cx, ax                                ; 39 c1                       ; 0xc1550
    jnbe short 0152dh                         ; 77 d9                       ; 0xc1552
    imul di, cx, strict byte 00003h           ; 6b f9 03                    ; 0xc1554
    add di, word [bp-014h]                    ; 03 7e ec                    ; 0xc1557
    mov al, byte [di]                         ; 8a 05                       ; 0xc155a
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc155c
    out DX, AL                                ; ee                          ; 0xc155f
    mov al, byte [di+001h]                    ; 8a 45 01                    ; 0xc1560
    out DX, AL                                ; ee                          ; 0xc1563
    mov al, byte [di+002h]                    ; 8a 45 02                    ; 0xc1564
    out DX, AL                                ; ee                          ; 0xc1567
    jmp short 01535h                          ; eb cb                       ; 0xc1568
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc156a vgabios.c:1038
    je short 01578h                           ; 74 08                       ; 0xc156e
    mov dx, 00100h                            ; ba 00 01                    ; 0xc1570 vgabios.c:1040
    xor ax, ax                                ; 31 c0                       ; 0xc1573
    call 010ffh                               ; e8 87 fb                    ; 0xc1575
    mov dx, 003dah                            ; ba da 03                    ; 0xc1578 vgabios.c:1045
    in AL, DX                                 ; ec                          ; 0xc157b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc157c
    xor cx, cx                                ; 31 c9                       ; 0xc157e vgabios.c:1048
    jmp short 01587h                          ; eb 05                       ; 0xc1580
    cmp cx, strict byte 00013h                ; 83 f9 13                    ; 0xc1582
    jnbe short 0159ch                         ; 77 15                       ; 0xc1585
    mov al, cl                                ; 88 c8                       ; 0xc1587 vgabios.c:1049
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1589
    out DX, AL                                ; ee                          ; 0xc158c
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc158d vgabios.c:1050
    mov di, si                                ; 89 f7                       ; 0xc1590
    add di, cx                                ; 01 cf                       ; 0xc1592
    mov al, byte [es:di+023h]                 ; 26 8a 45 23                 ; 0xc1594
    out DX, AL                                ; ee                          ; 0xc1598
    inc cx                                    ; 41                          ; 0xc1599 vgabios.c:1051
    jmp short 01582h                          ; eb e6                       ; 0xc159a
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc159c vgabios.c:1052
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc159e
    out DX, AL                                ; ee                          ; 0xc15a1
    xor al, al                                ; 30 c0                       ; 0xc15a2 vgabios.c:1053
    out DX, AL                                ; ee                          ; 0xc15a4
    mov es, [bp-018h]                         ; 8e 46 e8                    ; 0xc15a5 vgabios.c:1056
    mov dx, word [es:bx+004h]                 ; 26 8b 57 04                 ; 0xc15a8
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06                 ; 0xc15ac
    test ax, ax                               ; 85 c0                       ; 0xc15b0
    jne short 015b8h                          ; 75 04                       ; 0xc15b2
    test dx, dx                               ; 85 d2                       ; 0xc15b4
    je short 015f5h                           ; 74 3d                       ; 0xc15b6
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc15b8 vgabios.c:1060
    xor cx, cx                                ; 31 c9                       ; 0xc15bb vgabios.c:1061
    jmp short 015c4h                          ; eb 05                       ; 0xc15bd
    cmp cx, strict byte 00010h                ; 83 f9 10                    ; 0xc15bf
    jnc short 015e5h                          ; 73 21                       ; 0xc15c2
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc15c4 vgabios.c:1062
    mov di, si                                ; 89 f7                       ; 0xc15c7
    add di, cx                                ; 01 cf                       ; 0xc15c9
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc15cb
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc15ce
    mov ax, dx                                ; 89 d0                       ; 0xc15d1
    add ax, cx                                ; 01 c8                       ; 0xc15d3
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc15d5
    mov al, byte [es:di+023h]                 ; 26 8a 45 23                 ; 0xc15d8
    les di, [bp-022h]                         ; c4 7e de                    ; 0xc15dc
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc15df
    inc cx                                    ; 41                          ; 0xc15e2
    jmp short 015bfh                          ; eb da                       ; 0xc15e3
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc15e5 vgabios.c:1063
    mov al, byte [es:si+034h]                 ; 26 8a 44 34                 ; 0xc15e8
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc15ec
    mov di, dx                                ; 89 d7                       ; 0xc15ef
    mov byte [es:di+010h], al                 ; 26 88 45 10                 ; 0xc15f1
    xor al, al                                ; 30 c0                       ; 0xc15f5 vgabios.c:1067
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc15f7
    out DX, AL                                ; ee                          ; 0xc15fa
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc15fb vgabios.c:1068
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc15fd
    out DX, AL                                ; ee                          ; 0xc1600
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc1601 vgabios.c:1069
    jmp short 0160bh                          ; eb 05                       ; 0xc1604
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc1606
    jnbe short 01623h                         ; 77 18                       ; 0xc1609
    mov al, cl                                ; 88 c8                       ; 0xc160b vgabios.c:1070
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc160d
    out DX, AL                                ; ee                          ; 0xc1610
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc1611 vgabios.c:1071
    mov di, si                                ; 89 f7                       ; 0xc1614
    add di, cx                                ; 01 cf                       ; 0xc1616
    mov al, byte [es:di+004h]                 ; 26 8a 45 04                 ; 0xc1618
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc161c
    out DX, AL                                ; ee                          ; 0xc161f
    inc cx                                    ; 41                          ; 0xc1620 vgabios.c:1072
    jmp short 01606h                          ; eb e3                       ; 0xc1621
    xor cx, cx                                ; 31 c9                       ; 0xc1623 vgabios.c:1075
    jmp short 0162ch                          ; eb 05                       ; 0xc1625
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc1627
    jnbe short 01644h                         ; 77 18                       ; 0xc162a
    mov al, cl                                ; 88 c8                       ; 0xc162c vgabios.c:1076
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc162e
    out DX, AL                                ; ee                          ; 0xc1631
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc1632 vgabios.c:1077
    mov di, si                                ; 89 f7                       ; 0xc1635
    add di, cx                                ; 01 cf                       ; 0xc1637
    mov al, byte [es:di+037h]                 ; 26 8a 45 37                 ; 0xc1639
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc163d
    out DX, AL                                ; ee                          ; 0xc1640
    inc cx                                    ; 41                          ; 0xc1641 vgabios.c:1078
    jmp short 01627h                          ; eb e3                       ; 0xc1642
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1644 vgabios.c:1081
    xor ah, ah                                ; 30 e4                       ; 0xc1647
    mov di, ax                                ; 89 c7                       ; 0xc1649
    sal di, 003h                              ; c1 e7 03                    ; 0xc164b
    cmp byte [di+047b0h], 001h                ; 80 bd b0 47 01              ; 0xc164e
    jne short 0165ah                          ; 75 05                       ; 0xc1653
    mov cx, 003b4h                            ; b9 b4 03                    ; 0xc1655
    jmp short 0165dh                          ; eb 03                       ; 0xc1658
    mov cx, 003d4h                            ; b9 d4 03                    ; 0xc165a
    mov word [bp-016h], cx                    ; 89 4e ea                    ; 0xc165d
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc1660 vgabios.c:1084
    mov al, byte [es:si+009h]                 ; 26 8a 44 09                 ; 0xc1663
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc1667
    out DX, AL                                ; ee                          ; 0xc166a
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc166b vgabios.c:1087
    mov dx, cx                                ; 89 ca                       ; 0xc166e
    out DX, ax                                ; ef                          ; 0xc1670
    xor cx, cx                                ; 31 c9                       ; 0xc1671 vgabios.c:1089
    jmp short 0167ah                          ; eb 05                       ; 0xc1673
    cmp cx, strict byte 00018h                ; 83 f9 18                    ; 0xc1675
    jnbe short 01690h                         ; 77 16                       ; 0xc1678
    mov al, cl                                ; 88 c8                       ; 0xc167a vgabios.c:1090
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc167c
    out DX, AL                                ; ee                          ; 0xc167f
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc1680 vgabios.c:1091
    mov di, si                                ; 89 f7                       ; 0xc1683
    add di, cx                                ; 01 cf                       ; 0xc1685
    inc dx                                    ; 42                          ; 0xc1687
    mov al, byte [es:di+00ah]                 ; 26 8a 45 0a                 ; 0xc1688
    out DX, AL                                ; ee                          ; 0xc168c
    inc cx                                    ; 41                          ; 0xc168d vgabios.c:1092
    jmp short 01675h                          ; eb e5                       ; 0xc168e
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc1690 vgabios.c:1095
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1692
    out DX, AL                                ; ee                          ; 0xc1695
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc1696 vgabios.c:1096
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc1699
    in AL, DX                                 ; ec                          ; 0xc169c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc169d
    cmp byte [bp-00eh], 000h                  ; 80 7e f2 00                 ; 0xc169f vgabios.c:1098
    jne short 01704h                          ; 75 5f                       ; 0xc16a3
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc16a5 vgabios.c:1100
    xor ah, ah                                ; 30 e4                       ; 0xc16a8
    mov di, ax                                ; 89 c7                       ; 0xc16aa
    sal di, 003h                              ; c1 e7 03                    ; 0xc16ac
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc16af
    jne short 016c8h                          ; 75 12                       ; 0xc16b4
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc16b6 vgabios.c:1102
    mov cx, 04000h                            ; b9 00 40                    ; 0xc16ba
    mov ax, 00720h                            ; b8 20 07                    ; 0xc16bd
    xor di, di                                ; 31 ff                       ; 0xc16c0
    jcxz 016c6h                               ; e3 02                       ; 0xc16c2
    rep stosw                                 ; f3 ab                       ; 0xc16c4
    jmp short 01704h                          ; eb 3c                       ; 0xc16c6 vgabios.c:1104
    cmp byte [bp-010h], 00dh                  ; 80 7e f0 0d                 ; 0xc16c8 vgabios.c:1106
    jnc short 016dfh                          ; 73 11                       ; 0xc16cc
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc16ce vgabios.c:1108
    mov cx, 04000h                            ; b9 00 40                    ; 0xc16d2
    xor al, al                                ; 30 c0                       ; 0xc16d5
    xor di, di                                ; 31 ff                       ; 0xc16d7
    jcxz 016ddh                               ; e3 02                       ; 0xc16d9
    rep stosw                                 ; f3 ab                       ; 0xc16db
    jmp short 01704h                          ; eb 25                       ; 0xc16dd vgabios.c:1110
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc16df vgabios.c:1112
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc16e1
    out DX, AL                                ; ee                          ; 0xc16e4
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc16e5 vgabios.c:1113
    in AL, DX                                 ; ec                          ; 0xc16e8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc16e9
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc16eb
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc16ee vgabios.c:1114
    out DX, AL                                ; ee                          ; 0xc16f0
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc16f1 vgabios.c:1115
    mov cx, 08000h                            ; b9 00 80                    ; 0xc16f5
    xor ax, ax                                ; 31 c0                       ; 0xc16f8
    xor di, di                                ; 31 ff                       ; 0xc16fa
    jcxz 01700h                               ; e3 02                       ; 0xc16fc
    rep stosw                                 ; f3 ab                       ; 0xc16fe
    mov al, byte [bp-020h]                    ; 8a 46 e0                    ; 0xc1700 vgabios.c:1116
    out DX, AL                                ; ee                          ; 0xc1703
    mov di, strict word 00049h                ; bf 49 00                    ; 0xc1704 vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1707
    mov es, ax                                ; 8e c0                       ; 0xc170a
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc170c
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc170f
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc1712 vgabios.c:1123
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc1715
    xor ah, ah                                ; 30 e4                       ; 0xc1718
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc171a vgabios.c:62
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc171d
    mov es, dx                                ; 8e c2                       ; 0xc1720
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc1722
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc1725 vgabios.c:60
    mov ax, word [es:si+003h]                 ; 26 8b 44 03                 ; 0xc1728
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc172c vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc172f
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc1731
    mov di, strict word 00063h                ; bf 63 00                    ; 0xc1734 vgabios.c:62
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1737
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc173a
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc173d vgabios.c:50
    mov al, byte [es:si+001h]                 ; 26 8a 44 01                 ; 0xc1740
    mov di, 00084h                            ; bf 84 00                    ; 0xc1744 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc1747
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc1749
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc174c vgabios.c:1127
    mov al, byte [es:si+002h]                 ; 26 8a 44 02                 ; 0xc174f
    xor ah, ah                                ; 30 e4                       ; 0xc1753
    mov di, 00085h                            ; bf 85 00                    ; 0xc1755 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc1758
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc175a
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc175d vgabios.c:1128
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc1760
    mov di, 00087h                            ; bf 87 00                    ; 0xc1762 vgabios.c:52
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc1765
    mov di, 00088h                            ; bf 88 00                    ; 0xc1768 vgabios.c:52
    mov byte [es:di], 0f9h                    ; 26 c6 05 f9                 ; 0xc176b
    mov di, 0008ah                            ; bf 8a 00                    ; 0xc176f vgabios.c:52
    mov byte [es:di], 008h                    ; 26 c6 05 08                 ; 0xc1772
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1776 vgabios.c:1134
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc1779
    jnbe short 017a2h                         ; 77 25                       ; 0xc177b
    mov di, ax                                ; 89 c7                       ; 0xc177d vgabios.c:1136
    mov al, byte [di+07dddh]                  ; 8a 85 dd 7d                 ; 0xc177f
    mov di, strict word 00065h                ; bf 65 00                    ; 0xc1783 vgabios.c:52
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc1786
    cmp byte [bp-010h], 006h                  ; 80 7e f0 06                 ; 0xc1789 vgabios.c:1137
    jne short 01794h                          ; 75 05                       ; 0xc178d
    mov ax, strict word 0003fh                ; b8 3f 00                    ; 0xc178f
    jmp short 01797h                          ; eb 03                       ; 0xc1792
    mov ax, strict word 00030h                ; b8 30 00                    ; 0xc1794
    mov di, strict word 00066h                ; bf 66 00                    ; 0xc1797 vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc179a
    mov es, dx                                ; 8e c2                       ; 0xc179d
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc179f
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc17a2 vgabios.c:1141
    xor ah, ah                                ; 30 e4                       ; 0xc17a5
    mov di, ax                                ; 89 c7                       ; 0xc17a7
    sal di, 003h                              ; c1 e7 03                    ; 0xc17a9
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc17ac
    jne short 017bch                          ; 75 09                       ; 0xc17b1
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc17b3 vgabios.c:1143
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc17b6
    call 0118ch                               ; e8 d0 f9                    ; 0xc17b9
    xor cx, cx                                ; 31 c9                       ; 0xc17bc vgabios.c:1148
    jmp short 017c5h                          ; eb 05                       ; 0xc17be
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc17c0
    jnc short 017d1h                          ; 73 0c                       ; 0xc17c3
    mov al, cl                                ; 88 c8                       ; 0xc17c5 vgabios.c:1149
    xor ah, ah                                ; 30 e4                       ; 0xc17c7
    xor dx, dx                                ; 31 d2                       ; 0xc17c9
    call 01293h                               ; e8 c5 fa                    ; 0xc17cb
    inc cx                                    ; 41                          ; 0xc17ce
    jmp short 017c0h                          ; eb ef                       ; 0xc17cf
    xor ax, ax                                ; 31 c0                       ; 0xc17d1 vgabios.c:1152
    call 01322h                               ; e8 4c fb                    ; 0xc17d3
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc17d6 vgabios.c:1155
    xor ah, ah                                ; 30 e4                       ; 0xc17d9
    mov di, ax                                ; 89 c7                       ; 0xc17db
    sal di, 003h                              ; c1 e7 03                    ; 0xc17dd
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc17e0
    jne short 0182dh                          ; 75 46                       ; 0xc17e5
    mov es, [bp-018h]                         ; 8e 46 e8                    ; 0xc17e7 vgabios.c:1157
    mov di, word [es:bx+008h]                 ; 26 8b 7f 08                 ; 0xc17ea
    mov ax, word [es:bx+00ah]                 ; 26 8b 47 0a                 ; 0xc17ee
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc17f2
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc17f5 vgabios.c:1159
    mov bl, byte [es:si+002h]                 ; 26 8a 5c 02                 ; 0xc17f8
    cmp bl, 00eh                              ; 80 fb 0e                    ; 0xc17fc
    je short 0181ch                           ; 74 1b                       ; 0xc17ff
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc1801
    jne short 01830h                          ; 75 2a                       ; 0xc1804
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc1806 vgabios.c:1161
    mov al, byte [es:si+002h]                 ; 26 8a 44 02                 ; 0xc1809
    xor ah, ah                                ; 30 e4                       ; 0xc180d
    push ax                                   ; 50                          ; 0xc180f
    push strict byte 00000h                   ; 6a 00                       ; 0xc1810
    push strict byte 00000h                   ; 6a 00                       ; 0xc1812
    mov cx, 00100h                            ; b9 00 01                    ; 0xc1814
    mov bx, 0556ch                            ; bb 6c 55                    ; 0xc1817
    jmp short 0183fh                          ; eb 23                       ; 0xc181a vgabios.c:1162
    mov al, bl                                ; 88 d8                       ; 0xc181c vgabios.c:1164
    xor ah, ah                                ; 30 e4                       ; 0xc181e
    push ax                                   ; 50                          ; 0xc1820
    push strict byte 00000h                   ; 6a 00                       ; 0xc1821
    push strict byte 00000h                   ; 6a 00                       ; 0xc1823
    mov cx, 00100h                            ; b9 00 01                    ; 0xc1825
    mov bx, 05d6ch                            ; bb 6c 5d                    ; 0xc1828
    jmp short 0183fh                          ; eb 12                       ; 0xc182b
    jmp near 018a3h                           ; e9 73 00                    ; 0xc182d
    mov al, bl                                ; 88 d8                       ; 0xc1830 vgabios.c:1167
    xor ah, ah                                ; 30 e4                       ; 0xc1832
    push ax                                   ; 50                          ; 0xc1834
    push strict byte 00000h                   ; 6a 00                       ; 0xc1835
    push strict byte 00000h                   ; 6a 00                       ; 0xc1837
    mov cx, 00100h                            ; b9 00 01                    ; 0xc1839
    mov bx, 06b6ch                            ; bb 6c 6b                    ; 0xc183c
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc183f
    xor al, al                                ; 30 c0                       ; 0xc1842
    call 02d78h                               ; e8 31 15                    ; 0xc1844
    cmp word [bp-01ch], strict byte 00000h    ; 83 7e e4 00                 ; 0xc1847 vgabios.c:1169
    jne short 01851h                          ; 75 04                       ; 0xc184b
    test di, di                               ; 85 ff                       ; 0xc184d
    je short 0189bh                           ; 74 4a                       ; 0xc184f
    xor cx, cx                                ; 31 c9                       ; 0xc1851 vgabios.c:1174
    mov es, [bp-01ch]                         ; 8e 46 e4                    ; 0xc1853 vgabios.c:1176
    mov bx, di                                ; 89 fb                       ; 0xc1856
    add bx, cx                                ; 01 cb                       ; 0xc1858
    mov al, byte [es:bx+00bh]                 ; 26 8a 47 0b                 ; 0xc185a
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc185e
    je short 0186ah                           ; 74 08                       ; 0xc1860
    cmp al, byte [bp-010h]                    ; 3a 46 f0                    ; 0xc1862 vgabios.c:1178
    je short 0186ah                           ; 74 03                       ; 0xc1865
    inc cx                                    ; 41                          ; 0xc1867 vgabios.c:1180
    jmp short 01853h                          ; eb e9                       ; 0xc1868 vgabios.c:1181
    mov es, [bp-01ch]                         ; 8e 46 e4                    ; 0xc186a vgabios.c:1183
    mov bx, di                                ; 89 fb                       ; 0xc186d
    add bx, cx                                ; 01 cb                       ; 0xc186f
    mov al, byte [es:bx+00bh]                 ; 26 8a 47 0b                 ; 0xc1871
    cmp al, byte [bp-010h]                    ; 3a 46 f0                    ; 0xc1875
    jne short 0189bh                          ; 75 21                       ; 0xc1878
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc187a vgabios.c:1188
    xor ah, ah                                ; 30 e4                       ; 0xc187d
    push ax                                   ; 50                          ; 0xc187f
    mov al, byte [es:di+001h]                 ; 26 8a 45 01                 ; 0xc1880
    push ax                                   ; 50                          ; 0xc1884
    push word [es:di+004h]                    ; 26 ff 75 04                 ; 0xc1885
    mov cx, word [es:di+002h]                 ; 26 8b 4d 02                 ; 0xc1889
    mov bx, word [es:di+006h]                 ; 26 8b 5d 06                 ; 0xc188d
    mov dx, word [es:di+008h]                 ; 26 8b 55 08                 ; 0xc1891
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc1895
    call 02d78h                               ; e8 dd 14                    ; 0xc1898
    xor bl, bl                                ; 30 db                       ; 0xc189b vgabios.c:1192
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc189d
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc189f
    int 06dh                                  ; cd 6d                       ; 0xc18a1
    mov bx, 0596ch                            ; bb 6c 59                    ; 0xc18a3 vgabios.c:1196
    mov cx, ds                                ; 8c d9                       ; 0xc18a6
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc18a8
    call 009f0h                               ; e8 42 f1                    ; 0xc18ab
    mov es, [bp-01eh]                         ; 8e 46 e2                    ; 0xc18ae vgabios.c:1198
    mov al, byte [es:si+002h]                 ; 26 8a 44 02                 ; 0xc18b1
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc18b5
    je short 018d3h                           ; 74 1a                       ; 0xc18b7
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc18b9
    je short 018ceh                           ; 74 11                       ; 0xc18bb
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc18bd
    jne short 018d8h                          ; 75 17                       ; 0xc18bf
    mov bx, 0556ch                            ; bb 6c 55                    ; 0xc18c1 vgabios.c:1200
    mov cx, ds                                ; 8c d9                       ; 0xc18c4
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc18c6
    call 009f0h                               ; e8 24 f1                    ; 0xc18c9
    jmp short 018d8h                          ; eb 0a                       ; 0xc18cc vgabios.c:1201
    mov bx, 05d6ch                            ; bb 6c 5d                    ; 0xc18ce vgabios.c:1203
    jmp short 018c4h                          ; eb f1                       ; 0xc18d1
    mov bx, 06b6ch                            ; bb 6c 6b                    ; 0xc18d3 vgabios.c:1206
    jmp short 018c4h                          ; eb ec                       ; 0xc18d6
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc18d8 vgabios.c:1209
    pop di                                    ; 5f                          ; 0xc18db
    pop si                                    ; 5e                          ; 0xc18dc
    pop dx                                    ; 5a                          ; 0xc18dd
    pop cx                                    ; 59                          ; 0xc18de
    pop bx                                    ; 5b                          ; 0xc18df
    pop bp                                    ; 5d                          ; 0xc18e0
    retn                                      ; c3                          ; 0xc18e1
  ; disGetNextSymbol 0xc18e2 LB 0x2ce3 -> off=0x0 cb=000000000000008e uValue=00000000000c18e2 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc18e2 LB 0x8e
    push bp                                   ; 55                          ; 0xc18e2 vgabios.c:1212
    mov bp, sp                                ; 89 e5                       ; 0xc18e3
    push si                                   ; 56                          ; 0xc18e5
    push di                                   ; 57                          ; 0xc18e6
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc18e7
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc18ea
    mov al, dl                                ; 88 d0                       ; 0xc18ed
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc18ef
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc18f2
    xor ah, ah                                ; 30 e4                       ; 0xc18f5 vgabios.c:1218
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc18f7
    xor dh, dh                                ; 30 f6                       ; 0xc18fa
    mov cx, dx                                ; 89 d1                       ; 0xc18fc
    imul dx                                   ; f7 ea                       ; 0xc18fe
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1900
    xor dh, dh                                ; 30 f6                       ; 0xc1903
    mov si, dx                                ; 89 d6                       ; 0xc1905
    imul dx                                   ; f7 ea                       ; 0xc1907
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1909
    xor dh, dh                                ; 30 f6                       ; 0xc190c
    mov bx, dx                                ; 89 d3                       ; 0xc190e
    add ax, dx                                ; 01 d0                       ; 0xc1910
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1912
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1915 vgabios.c:1219
    xor ah, ah                                ; 30 e4                       ; 0xc1918
    imul cx                                   ; f7 e9                       ; 0xc191a
    imul si                                   ; f7 ee                       ; 0xc191c
    add ax, bx                                ; 01 d8                       ; 0xc191e
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1920
    mov ax, 00105h                            ; b8 05 01                    ; 0xc1923 vgabios.c:1220
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1926
    out DX, ax                                ; ef                          ; 0xc1929
    xor bl, bl                                ; 30 db                       ; 0xc192a vgabios.c:1221
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc192c
    jnc short 01960h                          ; 73 2f                       ; 0xc192f
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1931 vgabios.c:1223
    xor ah, ah                                ; 30 e4                       ; 0xc1934
    mov cx, ax                                ; 89 c1                       ; 0xc1936
    mov al, bl                                ; 88 d8                       ; 0xc1938
    mov dx, ax                                ; 89 c2                       ; 0xc193a
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc193c
    mov si, ax                                ; 89 c6                       ; 0xc193f
    mov ax, dx                                ; 89 d0                       ; 0xc1941
    imul si                                   ; f7 ee                       ; 0xc1943
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1945
    add si, ax                                ; 01 c6                       ; 0xc1948
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc194a
    add di, ax                                ; 01 c7                       ; 0xc194d
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc194f
    mov es, dx                                ; 8e c2                       ; 0xc1952
    jcxz 0195ch                               ; e3 06                       ; 0xc1954
    push DS                                   ; 1e                          ; 0xc1956
    mov ds, dx                                ; 8e da                       ; 0xc1957
    rep movsb                                 ; f3 a4                       ; 0xc1959
    pop DS                                    ; 1f                          ; 0xc195b
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc195c vgabios.c:1224
    jmp short 0192ch                          ; eb cc                       ; 0xc195e
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1960 vgabios.c:1225
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1963
    out DX, ax                                ; ef                          ; 0xc1966
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1967 vgabios.c:1226
    pop di                                    ; 5f                          ; 0xc196a
    pop si                                    ; 5e                          ; 0xc196b
    pop bp                                    ; 5d                          ; 0xc196c
    retn 00004h                               ; c2 04 00                    ; 0xc196d
  ; disGetNextSymbol 0xc1970 LB 0x2c55 -> off=0x0 cb=000000000000007b uValue=00000000000c1970 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc1970 LB 0x7b
    push bp                                   ; 55                          ; 0xc1970 vgabios.c:1229
    mov bp, sp                                ; 89 e5                       ; 0xc1971
    push si                                   ; 56                          ; 0xc1973
    push di                                   ; 57                          ; 0xc1974
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc1975
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1978
    mov al, dl                                ; 88 d0                       ; 0xc197b
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc197d
    mov bh, cl                                ; 88 cf                       ; 0xc1980
    xor ah, ah                                ; 30 e4                       ; 0xc1982 vgabios.c:1235
    mov dx, ax                                ; 89 c2                       ; 0xc1984
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1986
    mov cx, ax                                ; 89 c1                       ; 0xc1989
    mov ax, dx                                ; 89 d0                       ; 0xc198b
    imul cx                                   ; f7 e9                       ; 0xc198d
    mov dl, bh                                ; 88 fa                       ; 0xc198f
    xor dh, dh                                ; 30 f6                       ; 0xc1991
    imul dx                                   ; f7 ea                       ; 0xc1993
    mov dx, ax                                ; 89 c2                       ; 0xc1995
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1997
    xor ah, ah                                ; 30 e4                       ; 0xc199a
    add dx, ax                                ; 01 c2                       ; 0xc199c
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc199e
    mov ax, 00205h                            ; b8 05 02                    ; 0xc19a1 vgabios.c:1236
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc19a4
    out DX, ax                                ; ef                          ; 0xc19a7
    xor bl, bl                                ; 30 db                       ; 0xc19a8 vgabios.c:1237
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc19aa
    jnc short 019dbh                          ; 73 2c                       ; 0xc19ad
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc19af vgabios.c:1239
    xor ch, ch                                ; 30 ed                       ; 0xc19b2
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc19b4
    xor ah, ah                                ; 30 e4                       ; 0xc19b7
    mov si, ax                                ; 89 c6                       ; 0xc19b9
    mov al, bl                                ; 88 d8                       ; 0xc19bb
    mov dx, ax                                ; 89 c2                       ; 0xc19bd
    mov al, bh                                ; 88 f8                       ; 0xc19bf
    mov di, ax                                ; 89 c7                       ; 0xc19c1
    mov ax, dx                                ; 89 d0                       ; 0xc19c3
    imul di                                   ; f7 ef                       ; 0xc19c5
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc19c7
    add di, ax                                ; 01 c7                       ; 0xc19ca
    mov ax, si                                ; 89 f0                       ; 0xc19cc
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc19ce
    mov es, dx                                ; 8e c2                       ; 0xc19d1
    jcxz 019d7h                               ; e3 02                       ; 0xc19d3
    rep stosb                                 ; f3 aa                       ; 0xc19d5
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc19d7 vgabios.c:1240
    jmp short 019aah                          ; eb cf                       ; 0xc19d9
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc19db vgabios.c:1241
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc19de
    out DX, ax                                ; ef                          ; 0xc19e1
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc19e2 vgabios.c:1242
    pop di                                    ; 5f                          ; 0xc19e5
    pop si                                    ; 5e                          ; 0xc19e6
    pop bp                                    ; 5d                          ; 0xc19e7
    retn 00004h                               ; c2 04 00                    ; 0xc19e8
  ; disGetNextSymbol 0xc19eb LB 0x2bda -> off=0x0 cb=00000000000000b6 uValue=00000000000c19eb 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc19eb LB 0xb6
    push bp                                   ; 55                          ; 0xc19eb vgabios.c:1245
    mov bp, sp                                ; 89 e5                       ; 0xc19ec
    push si                                   ; 56                          ; 0xc19ee
    push di                                   ; 57                          ; 0xc19ef
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc19f0
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc19f3
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc19f6
    mov byte [bp-00ah], cl                    ; 88 4e f6                    ; 0xc19f9
    mov al, dl                                ; 88 d0                       ; 0xc19fc vgabios.c:1251
    xor ah, ah                                ; 30 e4                       ; 0xc19fe
    mov bx, ax                                ; 89 c3                       ; 0xc1a00
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a02
    mov si, ax                                ; 89 c6                       ; 0xc1a05
    mov ax, bx                                ; 89 d8                       ; 0xc1a07
    imul si                                   ; f7 ee                       ; 0xc1a09
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1a0b
    mov di, bx                                ; 89 df                       ; 0xc1a0e
    imul bx                                   ; f7 eb                       ; 0xc1a10
    mov dx, ax                                ; 89 c2                       ; 0xc1a12
    sar dx, 1                                 ; d1 fa                       ; 0xc1a14
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1a16
    xor ah, ah                                ; 30 e4                       ; 0xc1a19
    mov bx, ax                                ; 89 c3                       ; 0xc1a1b
    add dx, ax                                ; 01 c2                       ; 0xc1a1d
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1a1f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1a22 vgabios.c:1252
    imul si                                   ; f7 ee                       ; 0xc1a25
    imul di                                   ; f7 ef                       ; 0xc1a27
    sar ax, 1                                 ; d1 f8                       ; 0xc1a29
    add ax, bx                                ; 01 d8                       ; 0xc1a2b
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1a2d
    mov byte [bp-006h], bh                    ; 88 7e fa                    ; 0xc1a30 vgabios.c:1253
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1a33
    xor ah, ah                                ; 30 e4                       ; 0xc1a36
    cwd                                       ; 99                          ; 0xc1a38
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1a39
    sar ax, 1                                 ; d1 f8                       ; 0xc1a3b
    mov bx, ax                                ; 89 c3                       ; 0xc1a3d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1a3f
    xor ah, ah                                ; 30 e4                       ; 0xc1a42
    cmp ax, bx                                ; 39 d8                       ; 0xc1a44
    jnl short 01a98h                          ; 7d 50                       ; 0xc1a46
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1a48 vgabios.c:1255
    xor bh, bh                                ; 30 ff                       ; 0xc1a4b
    mov word [bp-012h], bx                    ; 89 5e ee                    ; 0xc1a4d
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc1a50
    imul bx                                   ; f7 eb                       ; 0xc1a53
    mov bx, ax                                ; 89 c3                       ; 0xc1a55
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1a57
    add si, ax                                ; 01 c6                       ; 0xc1a5a
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc1a5c
    add di, ax                                ; 01 c7                       ; 0xc1a5f
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc1a61
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1a64
    mov es, dx                                ; 8e c2                       ; 0xc1a67
    jcxz 01a71h                               ; e3 06                       ; 0xc1a69
    push DS                                   ; 1e                          ; 0xc1a6b
    mov ds, dx                                ; 8e da                       ; 0xc1a6c
    rep movsb                                 ; f3 a4                       ; 0xc1a6e
    pop DS                                    ; 1f                          ; 0xc1a70
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc1a71 vgabios.c:1256
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc1a74
    add si, bx                                ; 01 de                       ; 0xc1a78
    mov di, word [bp-010h]                    ; 8b 7e f0                    ; 0xc1a7a
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1a7d
    add di, bx                                ; 01 df                       ; 0xc1a81
    mov cx, word [bp-012h]                    ; 8b 4e ee                    ; 0xc1a83
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1a86
    mov es, dx                                ; 8e c2                       ; 0xc1a89
    jcxz 01a93h                               ; e3 06                       ; 0xc1a8b
    push DS                                   ; 1e                          ; 0xc1a8d
    mov ds, dx                                ; 8e da                       ; 0xc1a8e
    rep movsb                                 ; f3 a4                       ; 0xc1a90
    pop DS                                    ; 1f                          ; 0xc1a92
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1a93 vgabios.c:1257
    jmp short 01a33h                          ; eb 9b                       ; 0xc1a96
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a98 vgabios.c:1258
    pop di                                    ; 5f                          ; 0xc1a9b
    pop si                                    ; 5e                          ; 0xc1a9c
    pop bp                                    ; 5d                          ; 0xc1a9d
    retn 00004h                               ; c2 04 00                    ; 0xc1a9e
  ; disGetNextSymbol 0xc1aa1 LB 0x2b24 -> off=0x0 cb=0000000000000094 uValue=00000000000c1aa1 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc1aa1 LB 0x94
    push bp                                   ; 55                          ; 0xc1aa1 vgabios.c:1261
    mov bp, sp                                ; 89 e5                       ; 0xc1aa2
    push si                                   ; 56                          ; 0xc1aa4
    push di                                   ; 57                          ; 0xc1aa5
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1aa6
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1aa9
    mov al, dl                                ; 88 d0                       ; 0xc1aac
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1aae
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1ab1
    xor ah, ah                                ; 30 e4                       ; 0xc1ab4 vgabios.c:1267
    mov dx, ax                                ; 89 c2                       ; 0xc1ab6
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ab8
    mov bx, ax                                ; 89 c3                       ; 0xc1abb
    mov ax, dx                                ; 89 d0                       ; 0xc1abd
    imul bx                                   ; f7 eb                       ; 0xc1abf
    mov dl, cl                                ; 88 ca                       ; 0xc1ac1
    xor dh, dh                                ; 30 f6                       ; 0xc1ac3
    imul dx                                   ; f7 ea                       ; 0xc1ac5
    mov dx, ax                                ; 89 c2                       ; 0xc1ac7
    sar dx, 1                                 ; d1 fa                       ; 0xc1ac9
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1acb
    xor ah, ah                                ; 30 e4                       ; 0xc1ace
    add dx, ax                                ; 01 c2                       ; 0xc1ad0
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1ad2
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1ad5 vgabios.c:1268
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ad8
    xor ah, ah                                ; 30 e4                       ; 0xc1adb
    cwd                                       ; 99                          ; 0xc1add
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1ade
    sar ax, 1                                 ; d1 f8                       ; 0xc1ae0
    mov dx, ax                                ; 89 c2                       ; 0xc1ae2
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1ae4
    xor ah, ah                                ; 30 e4                       ; 0xc1ae7
    cmp ax, dx                                ; 39 d0                       ; 0xc1ae9
    jnl short 01b2ch                          ; 7d 3f                       ; 0xc1aeb
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc1aed vgabios.c:1270
    xor bh, bh                                ; 30 ff                       ; 0xc1af0
    mov dl, byte [bp+006h]                    ; 8a 56 06                    ; 0xc1af2
    xor dh, dh                                ; 30 f6                       ; 0xc1af5
    mov si, dx                                ; 89 d6                       ; 0xc1af7
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1af9
    imul dx                                   ; f7 ea                       ; 0xc1afc
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc1afe
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1b01
    add di, ax                                ; 01 c7                       ; 0xc1b04
    mov cx, bx                                ; 89 d9                       ; 0xc1b06
    mov ax, si                                ; 89 f0                       ; 0xc1b08
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1b0a
    mov es, dx                                ; 8e c2                       ; 0xc1b0d
    jcxz 01b13h                               ; e3 02                       ; 0xc1b0f
    rep stosb                                 ; f3 aa                       ; 0xc1b11
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1b13 vgabios.c:1271
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1b16
    add di, word [bp-010h]                    ; 03 7e f0                    ; 0xc1b1a
    mov cx, bx                                ; 89 d9                       ; 0xc1b1d
    mov ax, si                                ; 89 f0                       ; 0xc1b1f
    mov es, dx                                ; 8e c2                       ; 0xc1b21
    jcxz 01b27h                               ; e3 02                       ; 0xc1b23
    rep stosb                                 ; f3 aa                       ; 0xc1b25
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1b27 vgabios.c:1272
    jmp short 01ad8h                          ; eb ac                       ; 0xc1b2a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1b2c vgabios.c:1273
    pop di                                    ; 5f                          ; 0xc1b2f
    pop si                                    ; 5e                          ; 0xc1b30
    pop bp                                    ; 5d                          ; 0xc1b31
    retn 00004h                               ; c2 04 00                    ; 0xc1b32
  ; disGetNextSymbol 0xc1b35 LB 0x2a90 -> off=0x0 cb=0000000000000081 uValue=00000000000c1b35 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc1b35 LB 0x81
    push bp                                   ; 55                          ; 0xc1b35 vgabios.c:1276
    mov bp, sp                                ; 89 e5                       ; 0xc1b36
    push si                                   ; 56                          ; 0xc1b38
    push di                                   ; 57                          ; 0xc1b39
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1b3a
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1b3d
    mov al, dl                                ; 88 d0                       ; 0xc1b40
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc1b42
    mov bx, cx                                ; 89 cb                       ; 0xc1b45
    xor ah, ah                                ; 30 e4                       ; 0xc1b47 vgabios.c:1282
    mov si, ax                                ; 89 c6                       ; 0xc1b49
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1b4b
    mov di, ax                                ; 89 c7                       ; 0xc1b4e
    mov ax, si                                ; 89 f0                       ; 0xc1b50
    imul di                                   ; f7 ef                       ; 0xc1b52
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc1b54
    mov si, ax                                ; 89 c6                       ; 0xc1b57
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1b59
    xor ah, ah                                ; 30 e4                       ; 0xc1b5c
    mov cx, ax                                ; 89 c1                       ; 0xc1b5e
    add si, ax                                ; 01 c6                       ; 0xc1b60
    sal si, 003h                              ; c1 e6 03                    ; 0xc1b62
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc1b65
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1b68 vgabios.c:1283
    imul di                                   ; f7 ef                       ; 0xc1b6b
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc1b6d
    add ax, cx                                ; 01 c8                       ; 0xc1b70
    sal ax, 003h                              ; c1 e0 03                    ; 0xc1b72
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1b75
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1b78 vgabios.c:1284
    sal word [bp+004h], 003h                  ; c1 66 04 03                 ; 0xc1b7b vgabios.c:1285
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc1b7f vgabios.c:1286
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b82
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc1b85
    jnc short 01badh                          ; 73 23                       ; 0xc1b88
    xor ah, ah                                ; 30 e4                       ; 0xc1b8a vgabios.c:1288
    mul word [bp+004h]                        ; f7 66 04                    ; 0xc1b8c
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc1b8f
    add si, ax                                ; 01 c6                       ; 0xc1b92
    mov di, word [bp-00eh]                    ; 8b 7e f2                    ; 0xc1b94
    add di, ax                                ; 01 c7                       ; 0xc1b97
    mov cx, bx                                ; 89 d9                       ; 0xc1b99
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1b9b
    mov es, dx                                ; 8e c2                       ; 0xc1b9e
    jcxz 01ba8h                               ; e3 06                       ; 0xc1ba0
    push DS                                   ; 1e                          ; 0xc1ba2
    mov ds, dx                                ; 8e da                       ; 0xc1ba3
    rep movsb                                 ; f3 a4                       ; 0xc1ba5
    pop DS                                    ; 1f                          ; 0xc1ba7
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1ba8 vgabios.c:1289
    jmp short 01b82h                          ; eb d5                       ; 0xc1bab
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1bad vgabios.c:1290
    pop di                                    ; 5f                          ; 0xc1bb0
    pop si                                    ; 5e                          ; 0xc1bb1
    pop bp                                    ; 5d                          ; 0xc1bb2
    retn 00004h                               ; c2 04 00                    ; 0xc1bb3
  ; disGetNextSymbol 0xc1bb6 LB 0x2a0f -> off=0x0 cb=000000000000006d uValue=00000000000c1bb6 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc1bb6 LB 0x6d
    push bp                                   ; 55                          ; 0xc1bb6 vgabios.c:1293
    mov bp, sp                                ; 89 e5                       ; 0xc1bb7
    push si                                   ; 56                          ; 0xc1bb9
    push di                                   ; 57                          ; 0xc1bba
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc1bbb
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1bbe
    mov al, dl                                ; 88 d0                       ; 0xc1bc1
    mov si, cx                                ; 89 ce                       ; 0xc1bc3
    xor ah, ah                                ; 30 e4                       ; 0xc1bc5 vgabios.c:1299
    mov dx, ax                                ; 89 c2                       ; 0xc1bc7
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1bc9
    mov di, ax                                ; 89 c7                       ; 0xc1bcc
    mov ax, dx                                ; 89 d0                       ; 0xc1bce
    imul di                                   ; f7 ef                       ; 0xc1bd0
    mul cx                                    ; f7 e1                       ; 0xc1bd2
    mov dx, ax                                ; 89 c2                       ; 0xc1bd4
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1bd6
    xor ah, ah                                ; 30 e4                       ; 0xc1bd9
    add ax, dx                                ; 01 d0                       ; 0xc1bdb
    sal ax, 003h                              ; c1 e0 03                    ; 0xc1bdd
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc1be0
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1be3 vgabios.c:1300
    sal si, 003h                              ; c1 e6 03                    ; 0xc1be6 vgabios.c:1301
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc1be9 vgabios.c:1302
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1bed
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc1bf0
    jnc short 01c1ah                          ; 73 25                       ; 0xc1bf3
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1bf5 vgabios.c:1304
    xor ah, ah                                ; 30 e4                       ; 0xc1bf8
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1bfa
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1bfd
    mul si                                    ; f7 e6                       ; 0xc1c00
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1c02
    add di, ax                                ; 01 c7                       ; 0xc1c05
    mov cx, bx                                ; 89 d9                       ; 0xc1c07
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc1c09
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1c0c
    mov es, dx                                ; 8e c2                       ; 0xc1c0f
    jcxz 01c15h                               ; e3 02                       ; 0xc1c11
    rep stosb                                 ; f3 aa                       ; 0xc1c13
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc1c15 vgabios.c:1305
    jmp short 01bedh                          ; eb d3                       ; 0xc1c18
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1c1a vgabios.c:1306
    pop di                                    ; 5f                          ; 0xc1c1d
    pop si                                    ; 5e                          ; 0xc1c1e
    pop bp                                    ; 5d                          ; 0xc1c1f
    retn 00004h                               ; c2 04 00                    ; 0xc1c20
  ; disGetNextSymbol 0xc1c23 LB 0x29a2 -> off=0x0 cb=0000000000000688 uValue=00000000000c1c23 'biosfn_scroll'
biosfn_scroll:                               ; 0xc1c23 LB 0x688
    push bp                                   ; 55                          ; 0xc1c23 vgabios.c:1309
    mov bp, sp                                ; 89 e5                       ; 0xc1c24
    push si                                   ; 56                          ; 0xc1c26
    push di                                   ; 57                          ; 0xc1c27
    sub sp, strict byte 0001eh                ; 83 ec 1e                    ; 0xc1c28
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1c2b
    mov byte [bp-010h], dl                    ; 88 56 f0                    ; 0xc1c2e
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1c31
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc1c34
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1c37 vgabios.c:1318
    jnbe short 01c58h                         ; 77 1c                       ; 0xc1c3a
    cmp cl, byte [bp+006h]                    ; 3a 4e 06                    ; 0xc1c3c vgabios.c:1319
    jnbe short 01c58h                         ; 77 17                       ; 0xc1c3f
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1c41 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1c44
    mov es, ax                                ; 8e c0                       ; 0xc1c47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1c49
    xor ah, ah                                ; 30 e4                       ; 0xc1c4c vgabios.c:1323
    call 038c2h                               ; e8 71 1c                    ; 0xc1c4e
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1c51
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1c54 vgabios.c:1324
    jne short 01c5bh                          ; 75 03                       ; 0xc1c56
    jmp near 022a2h                           ; e9 47 06                    ; 0xc1c58
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1c5b vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1c5e
    mov es, ax                                ; 8e c0                       ; 0xc1c61
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1c63
    xor ah, ah                                ; 30 e4                       ; 0xc1c66 vgabios.c:48
    inc ax                                    ; 40                          ; 0xc1c68
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1c69
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1c6c vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc1c6f
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1c72 vgabios.c:58
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc1c75 vgabios.c:1331
    jne short 01c84h                          ; 75 09                       ; 0xc1c79
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1c7b vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1c7e
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc1c81 vgabios.c:48
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1c84 vgabios.c:1334
    xor ah, ah                                ; 30 e4                       ; 0xc1c87
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1c89
    jc short 01c96h                           ; 72 08                       ; 0xc1c8c
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc1c8e
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1c91
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc1c93
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1c96 vgabios.c:1335
    xor ah, ah                                ; 30 e4                       ; 0xc1c99
    cmp ax, word [bp-01eh]                    ; 3b 46 e2                    ; 0xc1c9b
    jc short 01ca8h                           ; 72 08                       ; 0xc1c9e
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1ca0
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1ca3
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc1ca5
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1ca8 vgabios.c:1336
    xor ah, ah                                ; 30 e4                       ; 0xc1cab
    cmp ax, word [bp-016h]                    ; 3b 46 ea                    ; 0xc1cad
    jbe short 01cb5h                          ; 76 03                       ; 0xc1cb0
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1cb2
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1cb5 vgabios.c:1337
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1cb8
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1cbb
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1cbd
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc1cc0 vgabios.c:1339
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc1cc3
    mov byte [bp-019h], 000h                  ; c6 46 e7 00                 ; 0xc1cc6
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1cca
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1ccd
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc1cd0
    dec ax                                    ; 48                          ; 0xc1cd3
    mov word [bp-022h], ax                    ; 89 46 de                    ; 0xc1cd4
    mov di, word [bp-016h]                    ; 8b 7e ea                    ; 0xc1cd7
    dec di                                    ; 4f                          ; 0xc1cda
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc1cdb
    mul word [bp-016h]                        ; f7 66 ea                    ; 0xc1cde
    mov cx, ax                                ; 89 c1                       ; 0xc1ce1
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1ce3
    jne short 01d33h                          ; 75 49                       ; 0xc1ce8
    add ax, ax                                ; 01 c0                       ; 0xc1cea vgabios.c:1342
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc1cec
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc1cee
    xor dh, dh                                ; 30 f6                       ; 0xc1cf1
    inc ax                                    ; 40                          ; 0xc1cf3
    mul dx                                    ; f7 e2                       ; 0xc1cf4
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc1cf6
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1cf9 vgabios.c:1347
    jne short 01d36h                          ; 75 37                       ; 0xc1cfd
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1cff
    jne short 01d36h                          ; 75 31                       ; 0xc1d03
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d05
    jne short 01d36h                          ; 75 2b                       ; 0xc1d09
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1d0b
    xor ah, ah                                ; 30 e4                       ; 0xc1d0e
    cmp ax, di                                ; 39 f8                       ; 0xc1d10
    jne short 01d36h                          ; 75 22                       ; 0xc1d12
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1d14
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc1d17
    jne short 01d36h                          ; 75 1a                       ; 0xc1d1a
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1d1c vgabios.c:1349
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1d1f
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1d22
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1d25
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1d29
    jcxz 01d30h                               ; e3 02                       ; 0xc1d2c
    rep stosw                                 ; f3 ab                       ; 0xc1d2e
    jmp near 022a2h                           ; e9 6f 05                    ; 0xc1d30 vgabios.c:1351
    jmp near 01ea6h                           ; e9 70 01                    ; 0xc1d33
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1d36 vgabios.c:1353
    jne short 01d9ch                          ; 75 60                       ; 0xc1d3a
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1d3c vgabios.c:1354
    xor ah, ah                                ; 30 e4                       ; 0xc1d3f
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1d41
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc1d44
    xor dh, dh                                ; 30 f6                       ; 0xc1d47
    cmp dx, word [bp-01ch]                    ; 3b 56 e4                    ; 0xc1d49
    jc short 01d9eh                           ; 72 50                       ; 0xc1d4c
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1d4e vgabios.c:1356
    xor ah, ah                                ; 30 e4                       ; 0xc1d51
    add ax, word [bp-01ch]                    ; 03 46 e4                    ; 0xc1d53
    cmp ax, dx                                ; 39 d0                       ; 0xc1d56
    jnbe short 01d60h                         ; 77 06                       ; 0xc1d58
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1d5a
    jne short 01da1h                          ; 75 41                       ; 0xc1d5e
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1d60 vgabios.c:1357
    xor ch, ch                                ; 30 ed                       ; 0xc1d63
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1d65
    xor ah, ah                                ; 30 e4                       ; 0xc1d68
    mov si, ax                                ; 89 c6                       ; 0xc1d6a
    sal si, 008h                              ; c1 e6 08                    ; 0xc1d6c
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1d6f
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1d72
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1d75
    mov dx, ax                                ; 89 c2                       ; 0xc1d78
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1d7a
    xor ah, ah                                ; 30 e4                       ; 0xc1d7d
    mov di, ax                                ; 89 c7                       ; 0xc1d7f
    add di, dx                                ; 01 d7                       ; 0xc1d81
    add di, di                                ; 01 ff                       ; 0xc1d83
    add di, word [bp-020h]                    ; 03 7e e0                    ; 0xc1d85
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1d88
    xor bh, bh                                ; 30 ff                       ; 0xc1d8b
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1d8d
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1d90
    mov ax, si                                ; 89 f0                       ; 0xc1d94
    jcxz 01d9ah                               ; e3 02                       ; 0xc1d96
    rep stosw                                 ; f3 ab                       ; 0xc1d98
    jmp short 01de1h                          ; eb 45                       ; 0xc1d9a vgabios.c:1358
    jmp short 01de7h                          ; eb 49                       ; 0xc1d9c
    jmp near 022a2h                           ; e9 01 05                    ; 0xc1d9e
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1da1 vgabios.c:1359
    xor ch, ch                                ; 30 ed                       ; 0xc1da4
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1da6
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc1da9
    mov byte [bp-018h], dl                    ; 88 56 e8                    ; 0xc1dac
    mov byte [bp-017h], ch                    ; 88 6e e9                    ; 0xc1daf
    mov si, ax                                ; 89 c6                       ; 0xc1db2
    add si, word [bp-018h]                    ; 03 76 e8                    ; 0xc1db4
    add si, si                                ; 01 f6                       ; 0xc1db7
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1db9
    xor bh, bh                                ; 30 ff                       ; 0xc1dbc
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1dbe
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1dc1
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1dc5
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1dc8
    add ax, word [bp-018h]                    ; 03 46 e8                    ; 0xc1dcb
    add ax, ax                                ; 01 c0                       ; 0xc1dce
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1dd0
    add di, ax                                ; 01 c7                       ; 0xc1dd3
    mov dx, bx                                ; 89 da                       ; 0xc1dd5
    mov es, bx                                ; 8e c3                       ; 0xc1dd7
    jcxz 01de1h                               ; e3 06                       ; 0xc1dd9
    push DS                                   ; 1e                          ; 0xc1ddb
    mov ds, dx                                ; 8e da                       ; 0xc1ddc
    rep movsw                                 ; f3 a5                       ; 0xc1dde
    pop DS                                    ; 1f                          ; 0xc1de0
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc1de1 vgabios.c:1360
    jmp near 01d44h                           ; e9 5d ff                    ; 0xc1de4
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1de7 vgabios.c:1363
    xor ah, ah                                ; 30 e4                       ; 0xc1dea
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1dec
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1def
    xor ah, ah                                ; 30 e4                       ; 0xc1df2
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1df4
    jnbe short 01d9eh                         ; 77 a5                       ; 0xc1df7
    mov dl, al                                ; 88 c2                       ; 0xc1df9 vgabios.c:1365
    xor dh, dh                                ; 30 f6                       ; 0xc1dfb
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1dfd
    add ax, dx                                ; 01 d0                       ; 0xc1e00
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1e02
    jnbe short 01e0dh                         ; 77 06                       ; 0xc1e05
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1e07
    jne short 01e49h                          ; 75 3c                       ; 0xc1e0b
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1e0d vgabios.c:1366
    xor ch, ch                                ; 30 ed                       ; 0xc1e10
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1e12
    xor ah, ah                                ; 30 e4                       ; 0xc1e15
    mov si, ax                                ; 89 c6                       ; 0xc1e17
    sal si, 008h                              ; c1 e6 08                    ; 0xc1e19
    add si, strict byte 00020h                ; 83 c6 20                    ; 0xc1e1c
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1e1f
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1e22
    mov dx, ax                                ; 89 c2                       ; 0xc1e25
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1e27
    xor ah, ah                                ; 30 e4                       ; 0xc1e2a
    add ax, dx                                ; 01 d0                       ; 0xc1e2c
    add ax, ax                                ; 01 c0                       ; 0xc1e2e
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1e30
    add di, ax                                ; 01 c7                       ; 0xc1e33
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1e35
    xor bh, bh                                ; 30 ff                       ; 0xc1e38
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1e3a
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1e3d
    mov ax, si                                ; 89 f0                       ; 0xc1e41
    jcxz 01e47h                               ; e3 02                       ; 0xc1e43
    rep stosw                                 ; f3 ab                       ; 0xc1e45
    jmp short 01e96h                          ; eb 4d                       ; 0xc1e47 vgabios.c:1367
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc1e49 vgabios.c:1368
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc1e4c
    mov byte [bp-017h], dh                    ; 88 76 e9                    ; 0xc1e4f
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1e52
    xor ah, ah                                ; 30 e4                       ; 0xc1e55
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc1e57
    sub dx, ax                                ; 29 c2                       ; 0xc1e5a
    mov ax, dx                                ; 89 d0                       ; 0xc1e5c
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1e5e
    mov cl, byte [bp-008h]                    ; 8a 4e f8                    ; 0xc1e61
    xor ch, ch                                ; 30 ed                       ; 0xc1e64
    mov si, ax                                ; 89 c6                       ; 0xc1e66
    add si, cx                                ; 01 ce                       ; 0xc1e68
    add si, si                                ; 01 f6                       ; 0xc1e6a
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1e6c
    xor bh, bh                                ; 30 ff                       ; 0xc1e6f
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1e71
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1e74
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc1e78
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1e7b
    add ax, cx                                ; 01 c8                       ; 0xc1e7e
    add ax, ax                                ; 01 c0                       ; 0xc1e80
    mov di, word [bp-020h]                    ; 8b 7e e0                    ; 0xc1e82
    add di, ax                                ; 01 c7                       ; 0xc1e85
    mov cx, word [bp-018h]                    ; 8b 4e e8                    ; 0xc1e87
    mov dx, bx                                ; 89 da                       ; 0xc1e8a
    mov es, bx                                ; 8e c3                       ; 0xc1e8c
    jcxz 01e96h                               ; e3 06                       ; 0xc1e8e
    push DS                                   ; 1e                          ; 0xc1e90
    mov ds, dx                                ; 8e da                       ; 0xc1e91
    rep movsw                                 ; f3 a5                       ; 0xc1e93
    pop DS                                    ; 1f                          ; 0xc1e95
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1e96 vgabios.c:1369
    xor ah, ah                                ; 30 e4                       ; 0xc1e99
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1e9b
    jc short 01ed3h                           ; 72 33                       ; 0xc1e9e
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc1ea0 vgabios.c:1370
    jmp near 01defh                           ; e9 49 ff                    ; 0xc1ea3
    mov si, word [bp-01ah]                    ; 8b 76 e6                    ; 0xc1ea6 vgabios.c:1376
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc1ea9
    xor ah, ah                                ; 30 e4                       ; 0xc1ead
    mov si, ax                                ; 89 c6                       ; 0xc1eaf
    sal si, 006h                              ; c1 e6 06                    ; 0xc1eb1
    mov al, byte [si+04844h]                  ; 8a 84 44 48                 ; 0xc1eb4
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1eb8
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc1ebb vgabios.c:1377
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc1ebf
    jc short 01ecfh                           ; 72 0c                       ; 0xc1ec1
    jbe short 01ed6h                          ; 76 11                       ; 0xc1ec3
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc1ec5
    je short 01f04h                           ; 74 3b                       ; 0xc1ec7
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc1ec9
    je short 01ed6h                           ; 74 09                       ; 0xc1ecb
    jmp short 01ed3h                          ; eb 04                       ; 0xc1ecd
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc1ecf
    je short 01f07h                           ; 74 34                       ; 0xc1ed1
    jmp near 022a2h                           ; e9 cc 03                    ; 0xc1ed3
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1ed6 vgabios.c:1381
    jne short 01f02h                          ; 75 26                       ; 0xc1eda
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1edc
    jne short 01f44h                          ; 75 62                       ; 0xc1ee0
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1ee2
    jne short 01f44h                          ; 75 5c                       ; 0xc1ee6
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1ee8
    xor ah, ah                                ; 30 e4                       ; 0xc1eeb
    mov dx, word [bp-016h]                    ; 8b 56 ea                    ; 0xc1eed
    dec dx                                    ; 4a                          ; 0xc1ef0
    cmp ax, dx                                ; 39 d0                       ; 0xc1ef1
    jne short 01f44h                          ; 75 4f                       ; 0xc1ef3
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc1ef5
    xor ah, dh                                ; 30 f4                       ; 0xc1ef8
    mov dx, word [bp-01eh]                    ; 8b 56 e2                    ; 0xc1efa
    dec dx                                    ; 4a                          ; 0xc1efd
    cmp ax, dx                                ; 39 d0                       ; 0xc1efe
    je short 01f0ah                           ; 74 08                       ; 0xc1f00
    jmp short 01f44h                          ; eb 40                       ; 0xc1f02
    jmp near 0217ah                           ; e9 73 02                    ; 0xc1f04
    jmp near 02034h                           ; e9 2a 01                    ; 0xc1f07
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1f0a vgabios.c:1383
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1f0d
    out DX, ax                                ; ef                          ; 0xc1f10
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc1f11 vgabios.c:1384
    mul word [bp-01eh]                        ; f7 66 e2                    ; 0xc1f14
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc1f17
    xor dh, dh                                ; 30 f6                       ; 0xc1f1a
    mul dx                                    ; f7 e2                       ; 0xc1f1c
    mov dl, byte [bp-010h]                    ; 8a 56 f0                    ; 0xc1f1e
    xor dh, dh                                ; 30 f6                       ; 0xc1f21
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc1f23
    xor bh, bh                                ; 30 ff                       ; 0xc1f26
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1f28
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1f2b
    mov cx, ax                                ; 89 c1                       ; 0xc1f2f
    mov ax, dx                                ; 89 d0                       ; 0xc1f31
    xor di, di                                ; 31 ff                       ; 0xc1f33
    mov es, bx                                ; 8e c3                       ; 0xc1f35
    jcxz 01f3bh                               ; e3 02                       ; 0xc1f37
    rep stosb                                 ; f3 aa                       ; 0xc1f39
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1f3b vgabios.c:1385
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1f3e
    out DX, ax                                ; ef                          ; 0xc1f41
    jmp short 01ed3h                          ; eb 8f                       ; 0xc1f42 vgabios.c:1387
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1f44 vgabios.c:1389
    jne short 01fbfh                          ; 75 75                       ; 0xc1f48
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1f4a vgabios.c:1390
    xor ah, ah                                ; 30 e4                       ; 0xc1f4d
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1f4f
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1f52
    xor ah, ah                                ; 30 e4                       ; 0xc1f55
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1f57
    jc short 01fbch                           ; 72 60                       ; 0xc1f5a
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc1f5c vgabios.c:1392
    xor dh, dh                                ; 30 f6                       ; 0xc1f5f
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc1f61
    cmp dx, ax                                ; 39 c2                       ; 0xc1f64
    jnbe short 01f6eh                         ; 77 06                       ; 0xc1f66
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1f68
    jne short 01f8fh                          ; 75 21                       ; 0xc1f6c
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1f6e vgabios.c:1393
    xor ah, ah                                ; 30 e4                       ; 0xc1f71
    push ax                                   ; 50                          ; 0xc1f73
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f74
    push ax                                   ; 50                          ; 0xc1f77
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1f78
    xor ch, ch                                ; 30 ed                       ; 0xc1f7b
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1f7d
    xor bh, bh                                ; 30 ff                       ; 0xc1f80
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1f82
    xor dh, dh                                ; 30 f6                       ; 0xc1f85
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1f87
    call 01970h                               ; e8 e3 f9                    ; 0xc1f8a
    jmp short 01fb7h                          ; eb 28                       ; 0xc1f8d vgabios.c:1394
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1f8f vgabios.c:1395
    push ax                                   ; 50                          ; 0xc1f92
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc1f93
    push ax                                   ; 50                          ; 0xc1f96
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc1f97
    xor ch, ch                                ; 30 ed                       ; 0xc1f9a
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc1f9c
    xor bh, bh                                ; 30 ff                       ; 0xc1f9f
    mov dl, bl                                ; 88 da                       ; 0xc1fa1
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc1fa3
    xor dh, dh                                ; 30 f6                       ; 0xc1fa6
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1fa8
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc1fab
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc1fae
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc1fb1
    call 018e2h                               ; e8 2b f9                    ; 0xc1fb4
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc1fb7 vgabios.c:1396
    jmp short 01f52h                          ; eb 96                       ; 0xc1fba
    jmp near 022a2h                           ; e9 e3 02                    ; 0xc1fbc
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc1fbf vgabios.c:1399
    xor ah, ah                                ; 30 e4                       ; 0xc1fc2
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1fc4
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1fc7
    xor ah, ah                                ; 30 e4                       ; 0xc1fca
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1fcc
    jnbe short 01fbch                         ; 77 eb                       ; 0xc1fcf
    mov dl, al                                ; 88 c2                       ; 0xc1fd1 vgabios.c:1401
    xor dh, dh                                ; 30 f6                       ; 0xc1fd3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1fd5
    add ax, dx                                ; 01 d0                       ; 0xc1fd8
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc1fda
    jnbe short 01fe5h                         ; 77 06                       ; 0xc1fdd
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc1fdf
    jne short 02006h                          ; 75 21                       ; 0xc1fe3
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1fe5 vgabios.c:1402
    xor ah, ah                                ; 30 e4                       ; 0xc1fe8
    push ax                                   ; 50                          ; 0xc1fea
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1feb
    push ax                                   ; 50                          ; 0xc1fee
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc1fef
    xor ch, ch                                ; 30 ed                       ; 0xc1ff2
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc1ff4
    xor bh, bh                                ; 30 ff                       ; 0xc1ff7
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc1ff9
    xor dh, dh                                ; 30 f6                       ; 0xc1ffc
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc1ffe
    call 01970h                               ; e8 6c f9                    ; 0xc2001
    jmp short 02025h                          ; eb 1f                       ; 0xc2004 vgabios.c:1403
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2006 vgabios.c:1404
    xor ah, ah                                ; 30 e4                       ; 0xc2009
    push ax                                   ; 50                          ; 0xc200b
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc200c
    push ax                                   ; 50                          ; 0xc200f
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc2010
    xor ch, ch                                ; 30 ed                       ; 0xc2013
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc2015
    xor bh, bh                                ; 30 ff                       ; 0xc2018
    mov dl, bl                                ; 88 da                       ; 0xc201a
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc201c
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc201f
    call 018e2h                               ; e8 bd f8                    ; 0xc2022
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2025 vgabios.c:1405
    xor ah, ah                                ; 30 e4                       ; 0xc2028
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc202a
    jc short 0207dh                           ; 72 4e                       ; 0xc202d
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc202f vgabios.c:1406
    jmp short 01fc7h                          ; eb 93                       ; 0xc2032
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc2034 vgabios.c:1411
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc2038
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc203b vgabios.c:1412
    jne short 02080h                          ; 75 3f                       ; 0xc203f
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc2041
    jne short 02080h                          ; 75 39                       ; 0xc2045
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc2047
    jne short 02080h                          ; 75 33                       ; 0xc204b
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc204d
    cmp ax, di                                ; 39 f8                       ; 0xc2050
    jne short 02080h                          ; 75 2c                       ; 0xc2052
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2054
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc2057
    jne short 02080h                          ; 75 24                       ; 0xc205a
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc205c vgabios.c:1414
    xor dh, dh                                ; 30 f6                       ; 0xc205f
    mov ax, cx                                ; 89 c8                       ; 0xc2061
    mul dx                                    ; f7 e2                       ; 0xc2063
    mov dl, byte [bp-014h]                    ; 8a 56 ec                    ; 0xc2065
    xor dh, dh                                ; 30 f6                       ; 0xc2068
    mul dx                                    ; f7 e2                       ; 0xc206a
    mov cx, ax                                ; 89 c1                       ; 0xc206c
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc206e
    xor ah, ah                                ; 30 e4                       ; 0xc2071
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2073
    xor di, di                                ; 31 ff                       ; 0xc2077
    jcxz 0207dh                               ; e3 02                       ; 0xc2079
    rep stosb                                 ; f3 aa                       ; 0xc207b
    jmp near 022a2h                           ; e9 22 02                    ; 0xc207d vgabios.c:1416
    cmp byte [bp-014h], 002h                  ; 80 7e ec 02                 ; 0xc2080 vgabios.c:1418
    jne short 0208fh                          ; 75 09                       ; 0xc2084
    sal byte [bp-008h], 1                     ; d0 66 f8                    ; 0xc2086 vgabios.c:1420
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc2089 vgabios.c:1421
    sal word [bp-01eh], 1                     ; d1 66 e2                    ; 0xc208c vgabios.c:1422
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc208f vgabios.c:1425
    jne short 020feh                          ; 75 69                       ; 0xc2093
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2095 vgabios.c:1426
    xor ah, ah                                ; 30 e4                       ; 0xc2098
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc209a
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc209d
    xor ah, ah                                ; 30 e4                       ; 0xc20a0
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc20a2
    jc short 0207dh                           ; 72 d6                       ; 0xc20a5
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc20a7 vgabios.c:1428
    xor dh, dh                                ; 30 f6                       ; 0xc20aa
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc20ac
    cmp dx, ax                                ; 39 c2                       ; 0xc20af
    jnbe short 020b9h                         ; 77 06                       ; 0xc20b1
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc20b3
    jne short 020dah                          ; 75 21                       ; 0xc20b7
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc20b9 vgabios.c:1429
    xor ah, ah                                ; 30 e4                       ; 0xc20bc
    push ax                                   ; 50                          ; 0xc20be
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc20bf
    push ax                                   ; 50                          ; 0xc20c2
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc20c3
    xor ch, ch                                ; 30 ed                       ; 0xc20c6
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc20c8
    xor bh, bh                                ; 30 ff                       ; 0xc20cb
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc20cd
    xor dh, dh                                ; 30 f6                       ; 0xc20d0
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc20d2
    call 01aa1h                               ; e8 c9 f9                    ; 0xc20d5
    jmp short 020f9h                          ; eb 1f                       ; 0xc20d8 vgabios.c:1430
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc20da vgabios.c:1431
    push ax                                   ; 50                          ; 0xc20dd
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc20de
    push ax                                   ; 50                          ; 0xc20e1
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc20e2
    xor ch, ch                                ; 30 ed                       ; 0xc20e5
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc20e7
    xor bh, bh                                ; 30 ff                       ; 0xc20ea
    mov dl, bl                                ; 88 da                       ; 0xc20ec
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc20ee
    xor dh, dh                                ; 30 f6                       ; 0xc20f1
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc20f3
    call 019ebh                               ; e8 f2 f8                    ; 0xc20f6
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc20f9 vgabios.c:1432
    jmp short 0209dh                          ; eb 9f                       ; 0xc20fc
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc20fe vgabios.c:1435
    xor ah, ah                                ; 30 e4                       ; 0xc2101
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc2103
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2106
    xor ah, ah                                ; 30 e4                       ; 0xc2109
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc210b
    jnbe short 02178h                         ; 77 68                       ; 0xc210e
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2110 vgabios.c:1437
    xor dh, dh                                ; 30 f6                       ; 0xc2113
    add ax, dx                                ; 01 d0                       ; 0xc2115
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc2117
    jnbe short 02120h                         ; 77 04                       ; 0xc211a
    test dl, dl                               ; 84 d2                       ; 0xc211c
    jne short 0214ah                          ; 75 2a                       ; 0xc211e
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2120 vgabios.c:1438
    xor ah, ah                                ; 30 e4                       ; 0xc2123
    push ax                                   ; 50                          ; 0xc2125
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2126
    push ax                                   ; 50                          ; 0xc2129
    mov cl, byte [bp-01eh]                    ; 8a 4e e2                    ; 0xc212a
    xor ch, ch                                ; 30 ed                       ; 0xc212d
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc212f
    xor bh, bh                                ; 30 ff                       ; 0xc2132
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc2134
    xor dh, dh                                ; 30 f6                       ; 0xc2137
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2139
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc213c
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc213f
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc2142
    call 01aa1h                               ; e8 59 f9                    ; 0xc2145
    jmp short 02169h                          ; eb 1f                       ; 0xc2148 vgabios.c:1439
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc214a vgabios.c:1440
    xor ah, ah                                ; 30 e4                       ; 0xc214d
    push ax                                   ; 50                          ; 0xc214f
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc2150
    push ax                                   ; 50                          ; 0xc2153
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc2154
    xor ch, ch                                ; 30 ed                       ; 0xc2157
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc2159
    xor bh, bh                                ; 30 ff                       ; 0xc215c
    mov dl, bl                                ; 88 da                       ; 0xc215e
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc2160
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2163
    call 019ebh                               ; e8 82 f8                    ; 0xc2166
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2169 vgabios.c:1441
    xor ah, ah                                ; 30 e4                       ; 0xc216c
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc216e
    jc short 021b8h                           ; 72 45                       ; 0xc2171
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc2173 vgabios.c:1442
    jmp short 02106h                          ; eb 8e                       ; 0xc2176
    jmp short 021b8h                          ; eb 3e                       ; 0xc2178
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc217a vgabios.c:1447
    jne short 021bbh                          ; 75 3b                       ; 0xc217e
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc2180
    jne short 021bbh                          ; 75 35                       ; 0xc2184
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc2186
    jne short 021bbh                          ; 75 2f                       ; 0xc218a
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc218c
    cmp ax, di                                ; 39 f8                       ; 0xc218f
    jne short 021bbh                          ; 75 28                       ; 0xc2191
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2193
    cmp ax, word [bp-022h]                    ; 3b 46 de                    ; 0xc2196
    jne short 021bbh                          ; 75 20                       ; 0xc2199
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc219b vgabios.c:1449
    xor dh, dh                                ; 30 f6                       ; 0xc219e
    mov ax, cx                                ; 89 c8                       ; 0xc21a0
    mul dx                                    ; f7 e2                       ; 0xc21a2
    mov cx, ax                                ; 89 c1                       ; 0xc21a4
    sal cx, 003h                              ; c1 e1 03                    ; 0xc21a6
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc21a9
    xor ah, ah                                ; 30 e4                       ; 0xc21ac
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc21ae
    xor di, di                                ; 31 ff                       ; 0xc21b2
    jcxz 021b8h                               ; e3 02                       ; 0xc21b4
    rep stosb                                 ; f3 aa                       ; 0xc21b6
    jmp near 022a2h                           ; e9 e7 00                    ; 0xc21b8 vgabios.c:1451
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc21bb vgabios.c:1454
    jne short 02230h                          ; 75 6f                       ; 0xc21bf
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc21c1 vgabios.c:1455
    xor ah, ah                                ; 30 e4                       ; 0xc21c4
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc21c6
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc21c9
    xor ah, ah                                ; 30 e4                       ; 0xc21cc
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc21ce
    jc short 021b8h                           ; 72 e5                       ; 0xc21d1
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc21d3 vgabios.c:1457
    xor dh, dh                                ; 30 f6                       ; 0xc21d6
    add dx, word [bp-01ch]                    ; 03 56 e4                    ; 0xc21d8
    cmp dx, ax                                ; 39 c2                       ; 0xc21db
    jnbe short 021e5h                         ; 77 06                       ; 0xc21dd
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc21df
    jne short 02204h                          ; 75 1f                       ; 0xc21e3
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc21e5 vgabios.c:1458
    xor ah, ah                                ; 30 e4                       ; 0xc21e8
    push ax                                   ; 50                          ; 0xc21ea
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc21eb
    push ax                                   ; 50                          ; 0xc21ee
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc21ef
    xor bh, bh                                ; 30 ff                       ; 0xc21f2
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc21f4
    xor dh, dh                                ; 30 f6                       ; 0xc21f7
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc21f9
    mov cx, word [bp-01eh]                    ; 8b 4e e2                    ; 0xc21fc
    call 01bb6h                               ; e8 b4 f9                    ; 0xc21ff
    jmp short 0222bh                          ; eb 27                       ; 0xc2202 vgabios.c:1459
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2204 vgabios.c:1460
    push ax                                   ; 50                          ; 0xc2207
    push word [bp-01eh]                       ; ff 76 e2                    ; 0xc2208
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc220b
    xor ch, ch                                ; 30 ed                       ; 0xc220e
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc2210
    xor bh, bh                                ; 30 ff                       ; 0xc2213
    mov dl, bl                                ; 88 da                       ; 0xc2215
    add dl, byte [bp-006h]                    ; 02 56 fa                    ; 0xc2217
    xor dh, dh                                ; 30 f6                       ; 0xc221a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc221c
    mov byte [bp-018h], al                    ; 88 46 e8                    ; 0xc221f
    mov byte [bp-017h], ah                    ; 88 66 e9                    ; 0xc2222
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc2225
    call 01b35h                               ; e8 0a f9                    ; 0xc2228
    inc word [bp-01ch]                        ; ff 46 e4                    ; 0xc222b vgabios.c:1461
    jmp short 021c9h                          ; eb 99                       ; 0xc222e
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2230 vgabios.c:1464
    xor ah, ah                                ; 30 e4                       ; 0xc2233
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc2235
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2238
    xor ah, ah                                ; 30 e4                       ; 0xc223b
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc223d
    jnbe short 022a2h                         ; 77 60                       ; 0xc2240
    mov dl, al                                ; 88 c2                       ; 0xc2242 vgabios.c:1466
    xor dh, dh                                ; 30 f6                       ; 0xc2244
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2246
    add ax, dx                                ; 01 d0                       ; 0xc2249
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc224b
    jnbe short 02256h                         ; 77 06                       ; 0xc224e
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2250
    jne short 02275h                          ; 75 1f                       ; 0xc2254
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2256 vgabios.c:1467
    xor ah, ah                                ; 30 e4                       ; 0xc2259
    push ax                                   ; 50                          ; 0xc225b
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc225c
    push ax                                   ; 50                          ; 0xc225f
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc2260
    xor bh, bh                                ; 30 ff                       ; 0xc2263
    mov dl, byte [bp-01ch]                    ; 8a 56 e4                    ; 0xc2265
    xor dh, dh                                ; 30 f6                       ; 0xc2268
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc226a
    mov cx, word [bp-01eh]                    ; 8b 4e e2                    ; 0xc226d
    call 01bb6h                               ; e8 43 f9                    ; 0xc2270
    jmp short 02293h                          ; eb 1e                       ; 0xc2273 vgabios.c:1468
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2275 vgabios.c:1469
    xor ah, ah                                ; 30 e4                       ; 0xc2278
    push ax                                   ; 50                          ; 0xc227a
    push word [bp-01eh]                       ; ff 76 e2                    ; 0xc227b
    mov cl, byte [bp-00ah]                    ; 8a 4e f6                    ; 0xc227e
    xor ch, ch                                ; 30 ed                       ; 0xc2281
    mov bl, byte [bp-01ch]                    ; 8a 5e e4                    ; 0xc2283
    xor bh, bh                                ; 30 ff                       ; 0xc2286
    mov dl, bl                                ; 88 da                       ; 0xc2288
    sub dl, byte [bp-006h]                    ; 2a 56 fa                    ; 0xc228a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc228d
    call 01b35h                               ; e8 a2 f8                    ; 0xc2290
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2293 vgabios.c:1470
    xor ah, ah                                ; 30 e4                       ; 0xc2296
    cmp ax, word [bp-01ch]                    ; 3b 46 e4                    ; 0xc2298
    jc short 022a2h                           ; 72 05                       ; 0xc229b
    dec word [bp-01ch]                        ; ff 4e e4                    ; 0xc229d vgabios.c:1471
    jmp short 02238h                          ; eb 96                       ; 0xc22a0
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc22a2 vgabios.c:1482
    pop di                                    ; 5f                          ; 0xc22a5
    pop si                                    ; 5e                          ; 0xc22a6
    pop bp                                    ; 5d                          ; 0xc22a7
    retn 00008h                               ; c2 08 00                    ; 0xc22a8
  ; disGetNextSymbol 0xc22ab LB 0x231a -> off=0x0 cb=0000000000000111 uValue=00000000000c22ab 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc22ab LB 0x111
    push bp                                   ; 55                          ; 0xc22ab vgabios.c:1485
    mov bp, sp                                ; 89 e5                       ; 0xc22ac
    push si                                   ; 56                          ; 0xc22ae
    push di                                   ; 57                          ; 0xc22af
    sub sp, strict byte 0000eh                ; 83 ec 0e                    ; 0xc22b0
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc22b3
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc22b6
    mov ch, bl                                ; 88 dd                       ; 0xc22b9
    mov al, cl                                ; 88 c8                       ; 0xc22bb
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc22bd vgabios.c:67
    xor dx, dx                                ; 31 d2                       ; 0xc22c0
    mov es, dx                                ; 8e c2                       ; 0xc22c2
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc22c4
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc22c7
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc22cb vgabios.c:68
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc22ce
    xor ah, ah                                ; 30 e4                       ; 0xc22d1 vgabios.c:1494
    mov bl, byte [bp+006h]                    ; 8a 5e 06                    ; 0xc22d3
    xor bh, bh                                ; 30 ff                       ; 0xc22d6
    imul bx                                   ; f7 eb                       ; 0xc22d8
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc22da
    xor dh, dh                                ; 30 f6                       ; 0xc22dd
    imul dx                                   ; f7 ea                       ; 0xc22df
    mov si, ax                                ; 89 c6                       ; 0xc22e1
    mov al, ch                                ; 88 e8                       ; 0xc22e3
    xor ah, ah                                ; 30 e4                       ; 0xc22e5
    add si, ax                                ; 01 c6                       ; 0xc22e7
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc22e9 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc22ec
    mov es, ax                                ; 8e c0                       ; 0xc22ef
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc22f1
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc22f4 vgabios.c:58
    xor dh, dh                                ; 30 f6                       ; 0xc22f7
    mul dx                                    ; f7 e2                       ; 0xc22f9
    add si, ax                                ; 01 c6                       ; 0xc22fb
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc22fd vgabios.c:1496
    xor ah, ah                                ; 30 e4                       ; 0xc2300
    imul bx                                   ; f7 eb                       ; 0xc2302
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc2304
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc2307 vgabios.c:1497
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc230a
    out DX, ax                                ; ef                          ; 0xc230d
    mov ax, 00205h                            ; b8 05 02                    ; 0xc230e vgabios.c:1498
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2311
    out DX, ax                                ; ef                          ; 0xc2314
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc2315 vgabios.c:1499
    je short 02321h                           ; 74 06                       ; 0xc2319
    mov ax, 01803h                            ; b8 03 18                    ; 0xc231b vgabios.c:1501
    out DX, ax                                ; ef                          ; 0xc231e
    jmp short 02325h                          ; eb 04                       ; 0xc231f vgabios.c:1503
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2321 vgabios.c:1505
    out DX, ax                                ; ef                          ; 0xc2324
    xor ch, ch                                ; 30 ed                       ; 0xc2325 vgabios.c:1507
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc2327
    jnc short 0239eh                          ; 73 72                       ; 0xc232a
    mov al, ch                                ; 88 e8                       ; 0xc232c vgabios.c:1509
    xor ah, ah                                ; 30 e4                       ; 0xc232e
    mov bl, byte [bp+004h]                    ; 8a 5e 04                    ; 0xc2330
    xor bh, bh                                ; 30 ff                       ; 0xc2333
    imul bx                                   ; f7 eb                       ; 0xc2335
    mov bx, si                                ; 89 f3                       ; 0xc2337
    add bx, ax                                ; 01 c3                       ; 0xc2339
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc233b vgabios.c:1510
    jmp short 02353h                          ; eb 12                       ; 0xc233f
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2341 vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc2344
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2346
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc234a vgabios.c:1523
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc234d
    jnc short 023a0h                          ; 73 4d                       ; 0xc2351
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc2353
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2356
    sar ax, CL                                ; d3 f8                       ; 0xc2359
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc235b
    mov byte [bp-00dh], 000h                  ; c6 46 f3 00                 ; 0xc235e
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2362
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2365
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc2368
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc236a
    out DX, ax                                ; ef                          ; 0xc236d
    mov dx, bx                                ; 89 da                       ; 0xc236e
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2370
    call 038eah                               ; e8 74 15                    ; 0xc2373
    mov al, ch                                ; 88 e8                       ; 0xc2376
    xor ah, ah                                ; 30 e4                       ; 0xc2378
    add ax, word [bp-010h]                    ; 03 46 f0                    ; 0xc237a
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc237d
    mov di, word [bp-012h]                    ; 8b 7e ee                    ; 0xc2380
    add di, ax                                ; 01 c7                       ; 0xc2383
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc2385
    xor ah, ah                                ; 30 e4                       ; 0xc2388
    test word [bp-00eh], ax                   ; 85 46 f2                    ; 0xc238a
    je short 02341h                           ; 74 b2                       ; 0xc238d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc238f
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc2392
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc2394
    mov es, dx                                ; 8e c2                       ; 0xc2397
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2399
    jmp short 0234ah                          ; eb ac                       ; 0xc239c
    jmp short 023a4h                          ; eb 04                       ; 0xc239e
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc23a0 vgabios.c:1524
    jmp short 02327h                          ; eb 83                       ; 0xc23a2
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc23a4 vgabios.c:1525
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc23a7
    out DX, ax                                ; ef                          ; 0xc23aa
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc23ab vgabios.c:1526
    out DX, ax                                ; ef                          ; 0xc23ae
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc23af vgabios.c:1527
    out DX, ax                                ; ef                          ; 0xc23b2
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc23b3 vgabios.c:1528
    pop di                                    ; 5f                          ; 0xc23b6
    pop si                                    ; 5e                          ; 0xc23b7
    pop bp                                    ; 5d                          ; 0xc23b8
    retn 00006h                               ; c2 06 00                    ; 0xc23b9
  ; disGetNextSymbol 0xc23bc LB 0x2209 -> off=0x0 cb=0000000000000112 uValue=00000000000c23bc 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc23bc LB 0x112
    push si                                   ; 56                          ; 0xc23bc vgabios.c:1531
    push di                                   ; 57                          ; 0xc23bd
    enter 0000ch, 000h                        ; c8 0c 00 00                 ; 0xc23be
    mov bh, al                                ; 88 c7                       ; 0xc23c2
    mov ch, dl                                ; 88 d5                       ; 0xc23c4
    mov al, bl                                ; 88 d8                       ; 0xc23c6
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc23c8 vgabios.c:1538
    xor ah, ah                                ; 30 e4                       ; 0xc23cb vgabios.c:1539
    mov dl, byte [bp+00ah]                    ; 8a 56 0a                    ; 0xc23cd
    xor dh, dh                                ; 30 f6                       ; 0xc23d0
    imul dx                                   ; f7 ea                       ; 0xc23d2
    mov dl, cl                                ; 88 ca                       ; 0xc23d4
    xor dh, dh                                ; 30 f6                       ; 0xc23d6
    imul dx, dx, 00140h                       ; 69 d2 40 01                 ; 0xc23d8
    add ax, dx                                ; 01 d0                       ; 0xc23dc
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc23de
    mov al, bh                                ; 88 f8                       ; 0xc23e1 vgabios.c:1540
    xor ah, ah                                ; 30 e4                       ; 0xc23e3
    sal ax, 003h                              ; c1 e0 03                    ; 0xc23e5
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc23e8
    xor ah, ah                                ; 30 e4                       ; 0xc23eb vgabios.c:1541
    jmp near 0240ch                           ; e9 1c 00                    ; 0xc23ed
    mov dl, ah                                ; 88 e2                       ; 0xc23f0 vgabios.c:1556
    xor dh, dh                                ; 30 f6                       ; 0xc23f2
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc23f4
    mov si, di                                ; 89 fe                       ; 0xc23f7
    add si, dx                                ; 01 d6                       ; 0xc23f9
    mov al, byte [si]                         ; 8a 04                       ; 0xc23fb
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc23fd vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc2400
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2402
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc2405 vgabios.c:1560
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc2407
    jnc short 02463h                          ; 73 57                       ; 0xc240a
    mov dl, ah                                ; 88 e2                       ; 0xc240c
    xor dh, dh                                ; 30 f6                       ; 0xc240e
    sar dx, 1                                 ; d1 fa                       ; 0xc2410
    imul dx, dx, strict byte 00050h           ; 6b d2 50                    ; 0xc2412
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2415
    add bx, dx                                ; 01 d3                       ; 0xc2418
    test ah, 001h                             ; f6 c4 01                    ; 0xc241a
    je short 02422h                           ; 74 03                       ; 0xc241d
    add bh, 020h                              ; 80 c7 20                    ; 0xc241f
    mov byte [bp-002h], 080h                  ; c6 46 fe 80                 ; 0xc2422
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc2426
    jne short 02448h                          ; 75 1c                       ; 0xc242a
    test ch, 080h                             ; f6 c5 80                    ; 0xc242c
    je short 023f0h                           ; 74 bf                       ; 0xc242f
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2431
    mov es, dx                                ; 8e c2                       ; 0xc2434
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2436
    mov dl, ah                                ; 88 e2                       ; 0xc2439
    xor dh, dh                                ; 30 f6                       ; 0xc243b
    add dx, word [bp-00ch]                    ; 03 56 f4                    ; 0xc243d
    mov si, di                                ; 89 fe                       ; 0xc2440
    add si, dx                                ; 01 d6                       ; 0xc2442
    xor al, byte [si]                         ; 32 04                       ; 0xc2444
    jmp short 023fdh                          ; eb b5                       ; 0xc2446
    cmp byte [bp-002h], 000h                  ; 80 7e fe 00                 ; 0xc2448 vgabios.c:1562
    jbe short 02405h                          ; 76 b7                       ; 0xc244c
    test ch, 080h                             ; f6 c5 80                    ; 0xc244e vgabios.c:1564
    je short 0245dh                           ; 74 0a                       ; 0xc2451
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc2453 vgabios.c:47
    mov es, dx                                ; 8e c2                       ; 0xc2456
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2458
    jmp short 0245fh                          ; eb 02                       ; 0xc245b vgabios.c:1568
    xor al, al                                ; 30 c0                       ; 0xc245d vgabios.c:1570
    xor dl, dl                                ; 30 d2                       ; 0xc245f vgabios.c:1572
    jmp short 0246ah                          ; eb 07                       ; 0xc2461
    jmp short 024c8h                          ; eb 63                       ; 0xc2463
    cmp dl, 004h                              ; 80 fa 04                    ; 0xc2465
    jnc short 024bdh                          ; 73 53                       ; 0xc2468
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc246a vgabios.c:1574
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc246d
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc2471
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc2474
    add si, di                                ; 01 fe                       ; 0xc2477
    mov dh, byte [si]                         ; 8a 34                       ; 0xc2479
    mov byte [bp-006h], dh                    ; 88 76 fa                    ; 0xc247b
    mov byte [bp-005h], 000h                  ; c6 46 fb 00                 ; 0xc247e
    mov dh, byte [bp-002h]                    ; 8a 76 fe                    ; 0xc2482
    mov byte [bp-00ah], dh                    ; 88 76 f6                    ; 0xc2485
    mov byte [bp-009h], 000h                  ; c6 46 f7 00                 ; 0xc2488
    mov si, word [bp-006h]                    ; 8b 76 fa                    ; 0xc248c
    test word [bp-00ah], si                   ; 85 76 f6                    ; 0xc248f
    je short 024b6h                           ; 74 22                       ; 0xc2492
    mov DH, strict byte 003h                  ; b6 03                       ; 0xc2494 vgabios.c:1575
    sub dh, dl                                ; 28 d6                       ; 0xc2496
    mov cl, ch                                ; 88 e9                       ; 0xc2498
    and cl, 003h                              ; 80 e1 03                    ; 0xc249a
    mov byte [bp-004h], cl                    ; 88 4e fc                    ; 0xc249d
    mov cl, dh                                ; 88 f1                       ; 0xc24a0
    add cl, dh                                ; 00 f1                       ; 0xc24a2
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc24a4
    sal dh, CL                                ; d2 e6                       ; 0xc24a7
    mov cl, dh                                ; 88 f1                       ; 0xc24a9
    test ch, 080h                             ; f6 c5 80                    ; 0xc24ab vgabios.c:1576
    je short 024b4h                           ; 74 04                       ; 0xc24ae
    xor al, dh                                ; 30 f0                       ; 0xc24b0 vgabios.c:1578
    jmp short 024b6h                          ; eb 02                       ; 0xc24b2 vgabios.c:1580
    or al, dh                                 ; 08 f0                       ; 0xc24b4 vgabios.c:1582
    shr byte [bp-002h], 1                     ; d0 6e fe                    ; 0xc24b6 vgabios.c:1585
    db  0feh, 0c2h
    ; inc dl                                    ; fe c2                     ; 0xc24b9 vgabios.c:1586
    jmp short 02465h                          ; eb a8                       ; 0xc24bb
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc24bd vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc24c0
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc24c2
    inc bx                                    ; 43                          ; 0xc24c5 vgabios.c:1588
    jmp short 02448h                          ; eb 80                       ; 0xc24c6 vgabios.c:1589
    leave                                     ; c9                          ; 0xc24c8 vgabios.c:1592
    pop di                                    ; 5f                          ; 0xc24c9
    pop si                                    ; 5e                          ; 0xc24ca
    retn 00004h                               ; c2 04 00                    ; 0xc24cb
  ; disGetNextSymbol 0xc24ce LB 0x20f7 -> off=0x0 cb=000000000000009b uValue=00000000000c24ce 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc24ce LB 0x9b
    push si                                   ; 56                          ; 0xc24ce vgabios.c:1595
    push di                                   ; 57                          ; 0xc24cf
    enter 00008h, 000h                        ; c8 08 00 00                 ; 0xc24d0
    mov bh, al                                ; 88 c7                       ; 0xc24d4
    mov ch, dl                                ; 88 d5                       ; 0xc24d6
    mov al, cl                                ; 88 c8                       ; 0xc24d8
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc24da vgabios.c:1602
    xor ah, ah                                ; 30 e4                       ; 0xc24dd vgabios.c:1603
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc24df
    xor dh, dh                                ; 30 f6                       ; 0xc24e2
    imul dx                                   ; f7 ea                       ; 0xc24e4
    mov dx, ax                                ; 89 c2                       ; 0xc24e6
    sal dx, 006h                              ; c1 e2 06                    ; 0xc24e8
    mov al, bl                                ; 88 d8                       ; 0xc24eb
    xor ah, ah                                ; 30 e4                       ; 0xc24ed
    sal ax, 003h                              ; c1 e0 03                    ; 0xc24ef
    add ax, dx                                ; 01 d0                       ; 0xc24f2
    mov word [bp-002h], ax                    ; 89 46 fe                    ; 0xc24f4
    mov al, bh                                ; 88 f8                       ; 0xc24f7 vgabios.c:1604
    xor ah, ah                                ; 30 e4                       ; 0xc24f9
    sal ax, 003h                              ; c1 e0 03                    ; 0xc24fb
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc24fe
    xor bl, bl                                ; 30 db                       ; 0xc2501 vgabios.c:1605
    jmp short 02547h                          ; eb 42                       ; 0xc2503
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc2505 vgabios.c:1609
    jnc short 02540h                          ; 73 37                       ; 0xc2507
    xor bh, bh                                ; 30 ff                       ; 0xc2509 vgabios.c:1611
    mov dl, bl                                ; 88 da                       ; 0xc250b vgabios.c:1612
    xor dh, dh                                ; 30 f6                       ; 0xc250d
    add dx, word [bp-006h]                    ; 03 56 fa                    ; 0xc250f
    mov si, di                                ; 89 fe                       ; 0xc2512
    add si, dx                                ; 01 d6                       ; 0xc2514
    mov dl, byte [si]                         ; 8a 14                       ; 0xc2516
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc2518
    mov byte [bp-003h], bh                    ; 88 7e fd                    ; 0xc251b
    mov dl, ah                                ; 88 e2                       ; 0xc251e
    xor dh, dh                                ; 30 f6                       ; 0xc2520
    test word [bp-004h], dx                   ; 85 56 fc                    ; 0xc2522
    je short 02529h                           ; 74 02                       ; 0xc2525
    mov bh, ch                                ; 88 ef                       ; 0xc2527 vgabios.c:1614
    mov dl, al                                ; 88 c2                       ; 0xc2529 vgabios.c:1616
    xor dh, dh                                ; 30 f6                       ; 0xc252b
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc252d
    add si, dx                                ; 01 d6                       ; 0xc2530
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc2532 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc2535
    mov byte [es:si], bh                      ; 26 88 3c                    ; 0xc2537
    shr ah, 1                                 ; d0 ec                       ; 0xc253a vgabios.c:1617
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc253c vgabios.c:1618
    jmp short 02505h                          ; eb c5                       ; 0xc253e
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc2540 vgabios.c:1619
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2542
    jnc short 02563h                          ; 73 1c                       ; 0xc2545
    mov al, bl                                ; 88 d8                       ; 0xc2547
    xor ah, ah                                ; 30 e4                       ; 0xc2549
    mov dl, byte [bp+008h]                    ; 8a 56 08                    ; 0xc254b
    xor dh, dh                                ; 30 f6                       ; 0xc254e
    imul dx                                   ; f7 ea                       ; 0xc2550
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2552
    mov dx, word [bp-002h]                    ; 8b 56 fe                    ; 0xc2555
    add dx, ax                                ; 01 c2                       ; 0xc2558
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc255a
    mov AH, strict byte 080h                  ; b4 80                       ; 0xc255d
    xor al, al                                ; 30 c0                       ; 0xc255f
    jmp short 02509h                          ; eb a6                       ; 0xc2561
    leave                                     ; c9                          ; 0xc2563 vgabios.c:1620
    pop di                                    ; 5f                          ; 0xc2564
    pop si                                    ; 5e                          ; 0xc2565
    retn 00002h                               ; c2 02 00                    ; 0xc2566
  ; disGetNextSymbol 0xc2569 LB 0x205c -> off=0x0 cb=0000000000000187 uValue=00000000000c2569 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc2569 LB 0x187
    push bp                                   ; 55                          ; 0xc2569 vgabios.c:1623
    mov bp, sp                                ; 89 e5                       ; 0xc256a
    push si                                   ; 56                          ; 0xc256c
    push di                                   ; 57                          ; 0xc256d
    sub sp, strict byte 0001ch                ; 83 ec 1c                    ; 0xc256e
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2571
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc2574
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc2577
    mov si, cx                                ; 89 ce                       ; 0xc257a
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc257c vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc257f
    mov es, ax                                ; 8e c0                       ; 0xc2582
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2584
    xor ah, ah                                ; 30 e4                       ; 0xc2587 vgabios.c:1631
    call 038c2h                               ; e8 36 13                    ; 0xc2589
    mov cl, al                                ; 88 c1                       ; 0xc258c
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc258e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2591 vgabios.c:1632
    jne short 02598h                          ; 75 03                       ; 0xc2593
    jmp near 026e9h                           ; e9 51 01                    ; 0xc2595
    mov al, dl                                ; 88 d0                       ; 0xc2598 vgabios.c:1635
    xor ah, ah                                ; 30 e4                       ; 0xc259a
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc259c
    lea dx, [bp-020h]                         ; 8d 56 e0                    ; 0xc259f
    call 00a96h                               ; e8 f1 e4                    ; 0xc25a2
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc25a5 vgabios.c:1636
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc25a8
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc25ab
    xor al, al                                ; 30 c0                       ; 0xc25ae
    shr ax, 008h                              ; c1 e8 08                    ; 0xc25b0
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc25b3
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc25b6
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc25b9
    mov bx, 00084h                            ; bb 84 00                    ; 0xc25bc vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc25bf
    mov es, ax                                ; 8e c0                       ; 0xc25c2
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc25c4
    xor ah, ah                                ; 30 e4                       ; 0xc25c7 vgabios.c:48
    mov dx, ax                                ; 89 c2                       ; 0xc25c9
    inc dx                                    ; 42                          ; 0xc25cb
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc25cc vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc25cf
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc25d2
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc25d5 vgabios.c:58
    mov bl, cl                                ; 88 cb                       ; 0xc25d8 vgabios.c:1642
    xor bh, bh                                ; 30 ff                       ; 0xc25da
    mov di, bx                                ; 89 df                       ; 0xc25dc
    sal di, 003h                              ; c1 e7 03                    ; 0xc25de
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc25e1
    jne short 02631h                          ; 75 49                       ; 0xc25e6
    mul dx                                    ; f7 e2                       ; 0xc25e8 vgabios.c:1645
    add ax, ax                                ; 01 c0                       ; 0xc25ea
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc25ec
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc25ee
    xor dh, dh                                ; 30 f6                       ; 0xc25f1
    inc ax                                    ; 40                          ; 0xc25f3
    mul dx                                    ; f7 e2                       ; 0xc25f4
    mov bx, ax                                ; 89 c3                       ; 0xc25f6
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc25f8
    xor ah, ah                                ; 30 e4                       ; 0xc25fb
    mul word [bp-018h]                        ; f7 66 e8                    ; 0xc25fd
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2600
    xor dh, dh                                ; 30 f6                       ; 0xc2603
    add ax, dx                                ; 01 d0                       ; 0xc2605
    add ax, ax                                ; 01 c0                       ; 0xc2607
    mov dx, bx                                ; 89 da                       ; 0xc2609
    add dx, ax                                ; 01 c2                       ; 0xc260b
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc260d vgabios.c:1647
    xor ah, ah                                ; 30 e4                       ; 0xc2610
    mov bx, ax                                ; 89 c3                       ; 0xc2612
    sal bx, 008h                              ; c1 e3 08                    ; 0xc2614
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2617
    add bx, ax                                ; 01 c3                       ; 0xc261a
    mov word [bp-020h], bx                    ; 89 5e e0                    ; 0xc261c
    mov ax, word [bp-020h]                    ; 8b 46 e0                    ; 0xc261f vgabios.c:1648
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2622
    mov cx, si                                ; 89 f1                       ; 0xc2626
    mov di, dx                                ; 89 d7                       ; 0xc2628
    jcxz 0262eh                               ; e3 02                       ; 0xc262a
    rep stosw                                 ; f3 ab                       ; 0xc262c
    jmp near 026e9h                           ; e9 b8 00                    ; 0xc262e vgabios.c:1650
    mov bl, byte [bx+0482eh]                  ; 8a 9f 2e 48                 ; 0xc2631 vgabios.c:1653
    sal bx, 006h                              ; c1 e3 06                    ; 0xc2635
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc2638
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc263c
    mov al, byte [di+047b1h]                  ; 8a 85 b1 47                 ; 0xc263f vgabios.c:1654
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc2643
    dec si                                    ; 4e                          ; 0xc2646 vgabios.c:1655
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2647
    je short 0269ch                           ; 74 50                       ; 0xc264a
    mov bl, byte [bp-010h]                    ; 8a 5e f0                    ; 0xc264c vgabios.c:1657
    xor bh, bh                                ; 30 ff                       ; 0xc264f
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2651
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2654
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2658
    jc short 0266ch                           ; 72 0f                       ; 0xc265b
    jbe short 02673h                          ; 76 14                       ; 0xc265d
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc265f
    je short 026c8h                           ; 74 64                       ; 0xc2662
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2664
    je short 02677h                           ; 74 0e                       ; 0xc2667
    jmp near 026e3h                           ; e9 77 00                    ; 0xc2669
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc266c
    je short 0269eh                           ; 74 2d                       ; 0xc266f
    jmp short 026e3h                          ; eb 70                       ; 0xc2671
    or byte [bp-006h], 001h                   ; 80 4e fa 01                 ; 0xc2673 vgabios.c:1660
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2677 vgabios.c:1662
    xor ah, ah                                ; 30 e4                       ; 0xc267a
    push ax                                   ; 50                          ; 0xc267c
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc267d
    push ax                                   ; 50                          ; 0xc2680
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2681
    push ax                                   ; 50                          ; 0xc2684
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2685
    xor ch, ch                                ; 30 ed                       ; 0xc2688
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc268a
    xor bh, bh                                ; 30 ff                       ; 0xc268d
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc268f
    xor dh, dh                                ; 30 f6                       ; 0xc2692
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2694
    call 022abh                               ; e8 11 fc                    ; 0xc2697
    jmp short 026e3h                          ; eb 47                       ; 0xc269a vgabios.c:1663
    jmp short 026e9h                          ; eb 4b                       ; 0xc269c
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc269e vgabios.c:1665
    xor ah, ah                                ; 30 e4                       ; 0xc26a1
    push ax                                   ; 50                          ; 0xc26a3
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc26a4
    push ax                                   ; 50                          ; 0xc26a7
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc26a8
    xor ch, ch                                ; 30 ed                       ; 0xc26ab
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc26ad
    xor bh, bh                                ; 30 ff                       ; 0xc26b0
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc26b2
    xor dh, dh                                ; 30 f6                       ; 0xc26b5
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc26b7
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc26ba
    mov byte [bp-015h], ah                    ; 88 66 eb                    ; 0xc26bd
    mov ax, word [bp-016h]                    ; 8b 46 ea                    ; 0xc26c0
    call 023bch                               ; e8 f6 fc                    ; 0xc26c3
    jmp short 026e3h                          ; eb 1b                       ; 0xc26c6 vgabios.c:1666
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc26c8 vgabios.c:1668
    xor ah, ah                                ; 30 e4                       ; 0xc26cb
    push ax                                   ; 50                          ; 0xc26cd
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc26ce
    xor ch, ch                                ; 30 ed                       ; 0xc26d1
    mov bl, byte [bp-008h]                    ; 8a 5e f8                    ; 0xc26d3
    xor bh, bh                                ; 30 ff                       ; 0xc26d6
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc26d8
    xor dh, dh                                ; 30 f6                       ; 0xc26db
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc26dd
    call 024ceh                               ; e8 eb fd                    ; 0xc26e0
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc26e3 vgabios.c:1675
    jmp near 02646h                           ; e9 5d ff                    ; 0xc26e6 vgabios.c:1676
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc26e9 vgabios.c:1678
    pop di                                    ; 5f                          ; 0xc26ec
    pop si                                    ; 5e                          ; 0xc26ed
    pop bp                                    ; 5d                          ; 0xc26ee
    retn                                      ; c3                          ; 0xc26ef
  ; disGetNextSymbol 0xc26f0 LB 0x1ed5 -> off=0x0 cb=0000000000000181 uValue=00000000000c26f0 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc26f0 LB 0x181
    push bp                                   ; 55                          ; 0xc26f0 vgabios.c:1681
    mov bp, sp                                ; 89 e5                       ; 0xc26f1
    push si                                   ; 56                          ; 0xc26f3
    push di                                   ; 57                          ; 0xc26f4
    sub sp, strict byte 0001ch                ; 83 ec 1c                    ; 0xc26f5
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc26f8
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc26fb
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc26fe
    mov si, cx                                ; 89 ce                       ; 0xc2701
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2703 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2706
    mov es, ax                                ; 8e c0                       ; 0xc2709
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc270b
    xor ah, ah                                ; 30 e4                       ; 0xc270e vgabios.c:1689
    call 038c2h                               ; e8 af 11                    ; 0xc2710
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2713
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2716
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2719 vgabios.c:1690
    jne short 02720h                          ; 75 03                       ; 0xc271b
    jmp near 0286ah                           ; e9 4a 01                    ; 0xc271d
    mov al, dl                                ; 88 d0                       ; 0xc2720 vgabios.c:1693
    xor ah, ah                                ; 30 e4                       ; 0xc2722
    lea bx, [bp-01eh]                         ; 8d 5e e2                    ; 0xc2724
    lea dx, [bp-020h]                         ; 8d 56 e0                    ; 0xc2727
    call 00a96h                               ; e8 69 e3                    ; 0xc272a
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc272d vgabios.c:1694
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2730
    mov ax, word [bp-01eh]                    ; 8b 46 e2                    ; 0xc2733
    xor al, al                                ; 30 c0                       ; 0xc2736
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2738
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc273b
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc273e
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2741
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2744 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2747
    mov es, ax                                ; 8e c0                       ; 0xc274a
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc274c
    xor ah, ah                                ; 30 e4                       ; 0xc274f vgabios.c:48
    mov dx, ax                                ; 89 c2                       ; 0xc2751
    inc dx                                    ; 42                          ; 0xc2753
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2754 vgabios.c:57
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc2757
    mov word [bp-01ch], cx                    ; 89 4e e4                    ; 0xc275a vgabios.c:58
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc275d vgabios.c:1700
    mov bx, ax                                ; 89 c3                       ; 0xc2760
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2762
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2765
    jne short 027aeh                          ; 75 42                       ; 0xc276a
    mov ax, cx                                ; 89 c8                       ; 0xc276c vgabios.c:1703
    mul dx                                    ; f7 e2                       ; 0xc276e
    add ax, ax                                ; 01 c0                       ; 0xc2770
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2772
    mov dl, byte [bp-00eh]                    ; 8a 56 f2                    ; 0xc2774
    xor dh, dh                                ; 30 f6                       ; 0xc2777
    inc ax                                    ; 40                          ; 0xc2779
    mul dx                                    ; f7 e2                       ; 0xc277a
    mov bx, ax                                ; 89 c3                       ; 0xc277c
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc277e
    xor ah, ah                                ; 30 e4                       ; 0xc2781
    mul cx                                    ; f7 e1                       ; 0xc2783
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc2785
    xor dh, dh                                ; 30 f6                       ; 0xc2788
    add ax, dx                                ; 01 d0                       ; 0xc278a
    add ax, ax                                ; 01 c0                       ; 0xc278c
    add bx, ax                                ; 01 c3                       ; 0xc278e
    dec si                                    ; 4e                          ; 0xc2790 vgabios.c:1705
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2791
    je short 0271dh                           ; 74 87                       ; 0xc2794
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2796 vgabios.c:1706
    xor ah, ah                                ; 30 e4                       ; 0xc2799
    mov di, ax                                ; 89 c7                       ; 0xc279b
    sal di, 003h                              ; c1 e7 03                    ; 0xc279d
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc27a0 vgabios.c:50
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc27a4 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc27a7
    inc bx                                    ; 43                          ; 0xc27aa vgabios.c:1707
    inc bx                                    ; 43                          ; 0xc27ab
    jmp short 02790h                          ; eb e2                       ; 0xc27ac vgabios.c:1708
    mov di, ax                                ; 89 c7                       ; 0xc27ae vgabios.c:1713
    mov al, byte [di+0482eh]                  ; 8a 85 2e 48                 ; 0xc27b0
    mov di, ax                                ; 89 c7                       ; 0xc27b4
    sal di, 006h                              ; c1 e7 06                    ; 0xc27b6
    mov al, byte [di+04844h]                  ; 8a 85 44 48                 ; 0xc27b9
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc27bd
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc27c0 vgabios.c:1714
    mov byte [bp-016h], al                    ; 88 46 ea                    ; 0xc27c4
    dec si                                    ; 4e                          ; 0xc27c7 vgabios.c:1715
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc27c8
    je short 0281dh                           ; 74 50                       ; 0xc27cb
    mov bl, byte [bp-012h]                    ; 8a 5e ee                    ; 0xc27cd vgabios.c:1717
    xor bh, bh                                ; 30 ff                       ; 0xc27d0
    sal bx, 003h                              ; c1 e3 03                    ; 0xc27d2
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc27d5
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc27d9
    jc short 027edh                           ; 72 0f                       ; 0xc27dc
    jbe short 027f4h                          ; 76 14                       ; 0xc27de
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc27e0
    je short 02849h                           ; 74 64                       ; 0xc27e3
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc27e5
    je short 027f8h                           ; 74 0e                       ; 0xc27e8
    jmp near 02864h                           ; e9 77 00                    ; 0xc27ea
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc27ed
    je short 0281fh                           ; 74 2d                       ; 0xc27f0
    jmp short 02864h                          ; eb 70                       ; 0xc27f2
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc27f4 vgabios.c:1720
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc27f8 vgabios.c:1722
    xor ah, ah                                ; 30 e4                       ; 0xc27fb
    push ax                                   ; 50                          ; 0xc27fd
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc27fe
    push ax                                   ; 50                          ; 0xc2801
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2802
    push ax                                   ; 50                          ; 0xc2805
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2806
    xor ch, ch                                ; 30 ed                       ; 0xc2809
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc280b
    xor bh, bh                                ; 30 ff                       ; 0xc280e
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2810
    xor dh, dh                                ; 30 f6                       ; 0xc2813
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2815
    call 022abh                               ; e8 90 fa                    ; 0xc2818
    jmp short 02864h                          ; eb 47                       ; 0xc281b vgabios.c:1723
    jmp short 0286ah                          ; eb 4b                       ; 0xc281d
    mov al, byte [bp-016h]                    ; 8a 46 ea                    ; 0xc281f vgabios.c:1725
    xor ah, ah                                ; 30 e4                       ; 0xc2822
    push ax                                   ; 50                          ; 0xc2824
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2825
    push ax                                   ; 50                          ; 0xc2828
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc2829
    xor ch, ch                                ; 30 ed                       ; 0xc282c
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc282e
    xor bh, bh                                ; 30 ff                       ; 0xc2831
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2833
    xor dh, dh                                ; 30 f6                       ; 0xc2836
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2838
    mov byte [bp-01ah], al                    ; 88 46 e6                    ; 0xc283b
    mov byte [bp-019h], ah                    ; 88 66 e7                    ; 0xc283e
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc2841
    call 023bch                               ; e8 75 fb                    ; 0xc2844
    jmp short 02864h                          ; eb 1b                       ; 0xc2847 vgabios.c:1726
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2849 vgabios.c:1728
    xor ah, ah                                ; 30 e4                       ; 0xc284c
    push ax                                   ; 50                          ; 0xc284e
    mov cl, byte [bp-00ch]                    ; 8a 4e f4                    ; 0xc284f
    xor ch, ch                                ; 30 ed                       ; 0xc2852
    mov bl, byte [bp-006h]                    ; 8a 5e fa                    ; 0xc2854
    xor bh, bh                                ; 30 ff                       ; 0xc2857
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2859
    xor dh, dh                                ; 30 f6                       ; 0xc285c
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc285e
    call 024ceh                               ; e8 6a fc                    ; 0xc2861
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2864 vgabios.c:1735
    jmp near 027c7h                           ; e9 5d ff                    ; 0xc2867 vgabios.c:1736
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc286a vgabios.c:1738
    pop di                                    ; 5f                          ; 0xc286d
    pop si                                    ; 5e                          ; 0xc286e
    pop bp                                    ; 5d                          ; 0xc286f
    retn                                      ; c3                          ; 0xc2870
  ; disGetNextSymbol 0xc2871 LB 0x1d54 -> off=0x0 cb=0000000000000173 uValue=00000000000c2871 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc2871 LB 0x173
    push bp                                   ; 55                          ; 0xc2871 vgabios.c:1741
    mov bp, sp                                ; 89 e5                       ; 0xc2872
    push si                                   ; 56                          ; 0xc2874
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc2875
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2878
    mov byte [bp-004h], dl                    ; 88 56 fc                    ; 0xc287b
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc287e
    mov dx, cx                                ; 89 ca                       ; 0xc2881
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2883 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2886
    mov es, ax                                ; 8e c0                       ; 0xc2889
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc288b
    xor ah, ah                                ; 30 e4                       ; 0xc288e vgabios.c:1748
    call 038c2h                               ; e8 2f 10                    ; 0xc2890
    mov cl, al                                ; 88 c1                       ; 0xc2893
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2895 vgabios.c:1749
    je short 028bfh                           ; 74 26                       ; 0xc2897
    mov bl, al                                ; 88 c3                       ; 0xc2899 vgabios.c:1750
    xor bh, bh                                ; 30 ff                       ; 0xc289b
    sal bx, 003h                              ; c1 e3 03                    ; 0xc289d
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc28a0
    je short 028bfh                           ; 74 18                       ; 0xc28a5
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc28a7 vgabios.c:1752
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc28ab
    jc short 028bbh                           ; 72 0c                       ; 0xc28ad
    jbe short 028c5h                          ; 76 14                       ; 0xc28af
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc28b1
    je short 028c2h                           ; 74 0d                       ; 0xc28b3
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc28b5
    je short 028c5h                           ; 74 0c                       ; 0xc28b7
    jmp short 028bfh                          ; eb 04                       ; 0xc28b9
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc28bb
    je short 02936h                           ; 74 77                       ; 0xc28bd
    jmp near 029deh                           ; e9 1c 01                    ; 0xc28bf
    jmp near 029bch                           ; e9 f7 00                    ; 0xc28c2
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc28c5 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc28c8
    mov es, ax                                ; 8e c0                       ; 0xc28cb
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc28cd
    mov ax, dx                                ; 89 d0                       ; 0xc28d0 vgabios.c:58
    mul bx                                    ; f7 e3                       ; 0xc28d2
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc28d4
    shr bx, 003h                              ; c1 eb 03                    ; 0xc28d7
    add bx, ax                                ; 01 c3                       ; 0xc28da
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc28dc vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc28df
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc28e2 vgabios.c:58
    xor dh, dh                                ; 30 f6                       ; 0xc28e5
    mul dx                                    ; f7 e2                       ; 0xc28e7
    add bx, ax                                ; 01 c3                       ; 0xc28e9
    mov cx, word [bp-008h]                    ; 8b 4e f8                    ; 0xc28eb vgabios.c:1758
    and cl, 007h                              ; 80 e1 07                    ; 0xc28ee
    mov ax, 00080h                            ; b8 80 00                    ; 0xc28f1
    sar ax, CL                                ; d3 f8                       ; 0xc28f4
    xor ah, ah                                ; 30 e4                       ; 0xc28f6 vgabios.c:1759
    sal ax, 008h                              ; c1 e0 08                    ; 0xc28f8
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc28fb
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc28fd
    out DX, ax                                ; ef                          ; 0xc2900
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2901 vgabios.c:1760
    out DX, ax                                ; ef                          ; 0xc2904
    mov dx, bx                                ; 89 da                       ; 0xc2905 vgabios.c:1761
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2907
    call 038eah                               ; e8 dd 0f                    ; 0xc290a
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc290d vgabios.c:1762
    je short 0291ah                           ; 74 07                       ; 0xc2911
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2913 vgabios.c:1764
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2916
    out DX, ax                                ; ef                          ; 0xc2919
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc291a vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc291d
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc291f
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2922
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2925 vgabios.c:1767
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2928
    out DX, ax                                ; ef                          ; 0xc292b
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc292c vgabios.c:1768
    out DX, ax                                ; ef                          ; 0xc292f
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2930 vgabios.c:1769
    out DX, ax                                ; ef                          ; 0xc2933
    jmp short 028bfh                          ; eb 89                       ; 0xc2934 vgabios.c:1770
    mov ax, dx                                ; 89 d0                       ; 0xc2936 vgabios.c:1772
    shr ax, 1                                 ; d1 e8                       ; 0xc2938
    imul ax, ax, strict byte 00050h           ; 6b c0 50                    ; 0xc293a
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc293d
    jne short 0294ch                          ; 75 08                       ; 0xc2942
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc2944 vgabios.c:1774
    shr bx, 002h                              ; c1 eb 02                    ; 0xc2947
    jmp short 02952h                          ; eb 06                       ; 0xc294a vgabios.c:1776
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc294c vgabios.c:1778
    shr bx, 003h                              ; c1 eb 03                    ; 0xc294f
    add bx, ax                                ; 01 c3                       ; 0xc2952
    test dl, 001h                             ; f6 c2 01                    ; 0xc2954 vgabios.c:1780
    je short 0295ch                           ; 74 03                       ; 0xc2957
    add bh, 020h                              ; 80 c7 20                    ; 0xc2959
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc295c vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc295f
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc2961
    mov al, cl                                ; 88 c8                       ; 0xc2964 vgabios.c:1782
    xor ah, ah                                ; 30 e4                       ; 0xc2966
    mov si, ax                                ; 89 c6                       ; 0xc2968
    sal si, 003h                              ; c1 e6 03                    ; 0xc296a
    cmp byte [si+047b1h], 002h                ; 80 bc b1 47 02              ; 0xc296d
    jne short 0298dh                          ; 75 19                       ; 0xc2972
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2974 vgabios.c:1784
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2977
    mov AH, strict byte 003h                  ; b4 03                       ; 0xc2979
    sub ah, al                                ; 28 c4                       ; 0xc297b
    mov cl, ah                                ; 88 e1                       ; 0xc297d
    add cl, ah                                ; 00 e1                       ; 0xc297f
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc2981
    and dh, 003h                              ; 80 e6 03                    ; 0xc2984
    sal dh, CL                                ; d2 e6                       ; 0xc2987
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc2989 vgabios.c:1785
    jmp short 029a0h                          ; eb 13                       ; 0xc298b vgabios.c:1787
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc298d vgabios.c:1789
    and AL, strict byte 007h                  ; 24 07                       ; 0xc2990
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc2992
    sub cl, al                                ; 28 c1                       ; 0xc2994
    mov dh, byte [bp-004h]                    ; 8a 76 fc                    ; 0xc2996
    and dh, 001h                              ; 80 e6 01                    ; 0xc2999
    sal dh, CL                                ; d2 e6                       ; 0xc299c
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc299e vgabios.c:1790
    sal al, CL                                ; d2 e0                       ; 0xc29a0
    test byte [bp-004h], 080h                 ; f6 46 fc 80                 ; 0xc29a2 vgabios.c:1792
    je short 029ach                           ; 74 04                       ; 0xc29a6
    xor dl, dh                                ; 30 f2                       ; 0xc29a8 vgabios.c:1794
    jmp short 029b2h                          ; eb 06                       ; 0xc29aa vgabios.c:1796
    not al                                    ; f6 d0                       ; 0xc29ac vgabios.c:1798
    and dl, al                                ; 20 c2                       ; 0xc29ae
    or dl, dh                                 ; 08 f2                       ; 0xc29b0 vgabios.c:1799
    mov ax, 0b800h                            ; b8 00 b8                    ; 0xc29b2 vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc29b5
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc29b7
    jmp short 029deh                          ; eb 22                       ; 0xc29ba vgabios.c:1802
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc29bc vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc29bf
    mov es, ax                                ; 8e c0                       ; 0xc29c2
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc29c4
    sal bx, 003h                              ; c1 e3 03                    ; 0xc29c7 vgabios.c:58
    mov ax, dx                                ; 89 d0                       ; 0xc29ca
    mul bx                                    ; f7 e3                       ; 0xc29cc
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc29ce
    add bx, ax                                ; 01 c3                       ; 0xc29d1
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc29d3 vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc29d6
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc29d8
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc29db
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc29de vgabios.c:1812
    pop si                                    ; 5e                          ; 0xc29e1
    pop bp                                    ; 5d                          ; 0xc29e2
    retn                                      ; c3                          ; 0xc29e3
  ; disGetNextSymbol 0xc29e4 LB 0x1be1 -> off=0x0 cb=0000000000000258 uValue=00000000000c29e4 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc29e4 LB 0x258
    push bp                                   ; 55                          ; 0xc29e4 vgabios.c:1815
    mov bp, sp                                ; 89 e5                       ; 0xc29e5
    push si                                   ; 56                          ; 0xc29e7
    sub sp, strict byte 00014h                ; 83 ec 14                    ; 0xc29e8
    mov ch, al                                ; 88 c5                       ; 0xc29eb
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc29ed
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc29f0
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc29f3 vgabios.c:1823
    jne short 02a06h                          ; 75 0e                       ; 0xc29f6
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc29f8 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc29fb
    mov es, ax                                ; 8e c0                       ; 0xc29fe
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2a00
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2a03 vgabios.c:48
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2a06 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2a09
    mov es, ax                                ; 8e c0                       ; 0xc2a0c
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2a0e
    xor ah, ah                                ; 30 e4                       ; 0xc2a11 vgabios.c:1828
    call 038c2h                               ; e8 ac 0e                    ; 0xc2a13
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2a16
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2a19 vgabios.c:1829
    je short 02a83h                           ; 74 66                       ; 0xc2a1b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2a1d vgabios.c:1832
    xor ah, ah                                ; 30 e4                       ; 0xc2a20
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc2a22
    lea dx, [bp-016h]                         ; 8d 56 ea                    ; 0xc2a25
    call 00a96h                               ; e8 6b e0                    ; 0xc2a28
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc2a2b vgabios.c:1833
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc2a2e
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc2a31
    xor al, al                                ; 30 c0                       ; 0xc2a34
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2a36
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2a39
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2a3c vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2a3f
    mov es, dx                                ; 8e c2                       ; 0xc2a42
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc2a44
    xor dh, dh                                ; 30 f6                       ; 0xc2a47 vgabios.c:48
    inc dx                                    ; 42                          ; 0xc2a49
    mov word [bp-012h], dx                    ; 89 56 ee                    ; 0xc2a4a
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2a4d vgabios.c:57
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2a50
    mov word [bp-010h], dx                    ; 89 56 f0                    ; 0xc2a53 vgabios.c:58
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc2a56 vgabios.c:1839
    jc short 02a69h                           ; 72 0e                       ; 0xc2a59
    jbe short 02a71h                          ; 76 14                       ; 0xc2a5b
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc2a5d
    je short 02a86h                           ; 74 24                       ; 0xc2a60
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc2a62
    je short 02a7ch                           ; 74 15                       ; 0xc2a65
    jmp short 02a8dh                          ; eb 24                       ; 0xc2a67
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc2a69
    jne short 02a8dh                          ; 75 1f                       ; 0xc2a6c
    jmp near 02b93h                           ; e9 22 01                    ; 0xc2a6e
    cmp byte [bp-004h], 000h                  ; 80 7e fc 00                 ; 0xc2a71 vgabios.c:1846
    jbe short 02a8ah                          ; 76 13                       ; 0xc2a75
    dec byte [bp-004h]                        ; fe 4e fc                    ; 0xc2a77
    jmp short 02a8ah                          ; eb 0e                       ; 0xc2a7a vgabios.c:1847
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2a7c vgabios.c:1850
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2a7e
    jmp short 02a8ah                          ; eb 07                       ; 0xc2a81 vgabios.c:1851
    jmp near 02c36h                           ; e9 b0 01                    ; 0xc2a83
    mov byte [bp-004h], 000h                  ; c6 46 fc 00                 ; 0xc2a86 vgabios.c:1854
    jmp near 02b93h                           ; e9 06 01                    ; 0xc2a8a vgabios.c:1855
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc2a8d vgabios.c:1859
    xor ah, ah                                ; 30 e4                       ; 0xc2a90
    mov bx, ax                                ; 89 c3                       ; 0xc2a92
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2a94
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2a97
    jne short 02ae0h                          ; 75 42                       ; 0xc2a9c
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc2a9e vgabios.c:1862
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc2aa1
    add ax, ax                                ; 01 c0                       ; 0xc2aa4
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2aa6
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2aa8
    xor dh, dh                                ; 30 f6                       ; 0xc2aab
    inc ax                                    ; 40                          ; 0xc2aad
    mul dx                                    ; f7 e2                       ; 0xc2aae
    mov si, ax                                ; 89 c6                       ; 0xc2ab0
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2ab2
    xor ah, ah                                ; 30 e4                       ; 0xc2ab5
    mul word [bp-010h]                        ; f7 66 f0                    ; 0xc2ab7
    mov dx, ax                                ; 89 c2                       ; 0xc2aba
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2abc
    xor ah, ah                                ; 30 e4                       ; 0xc2abf
    add ax, dx                                ; 01 d0                       ; 0xc2ac1
    add ax, ax                                ; 01 c0                       ; 0xc2ac3
    add si, ax                                ; 01 c6                       ; 0xc2ac5
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2ac7 vgabios.c:50
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc2acb vgabios.c:52
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc2ace vgabios.c:1867
    jne short 02b0fh                          ; 75 3c                       ; 0xc2ad1
    inc si                                    ; 46                          ; 0xc2ad3 vgabios.c:1868
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2ad4 vgabios.c:50
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2ad8
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2adb
    jmp short 02b0fh                          ; eb 2f                       ; 0xc2ade vgabios.c:1870
    mov si, ax                                ; 89 c6                       ; 0xc2ae0 vgabios.c:1873
    mov al, byte [si+0482eh]                  ; 8a 84 2e 48                 ; 0xc2ae2
    mov si, ax                                ; 89 c6                       ; 0xc2ae6
    sal si, 006h                              ; c1 e6 06                    ; 0xc2ae8
    mov dl, byte [si+04844h]                  ; 8a 94 44 48                 ; 0xc2aeb
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc2aef vgabios.c:1874
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2af3 vgabios.c:1875
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2af7
    jc short 02b0ah                           ; 72 0e                       ; 0xc2afa
    jbe short 02b11h                          ; 76 13                       ; 0xc2afc
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2afe
    je short 02b61h                           ; 74 5e                       ; 0xc2b01
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2b03
    je short 02b15h                           ; 74 0d                       ; 0xc2b06
    jmp short 02b80h                          ; eb 76                       ; 0xc2b08
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2b0a
    je short 02b3fh                           ; 74 30                       ; 0xc2b0d
    jmp short 02b80h                          ; eb 6f                       ; 0xc2b0f
    or byte [bp-00ah], 001h                   ; 80 4e f6 01                 ; 0xc2b11 vgabios.c:1878
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2b15 vgabios.c:1880
    xor ah, ah                                ; 30 e4                       ; 0xc2b18
    push ax                                   ; 50                          ; 0xc2b1a
    mov al, dl                                ; 88 d0                       ; 0xc2b1b
    push ax                                   ; 50                          ; 0xc2b1d
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2b1e
    push ax                                   ; 50                          ; 0xc2b21
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2b22
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc2b25
    xor bh, bh                                ; 30 ff                       ; 0xc2b28
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2b2a
    xor dh, dh                                ; 30 f6                       ; 0xc2b2d
    mov byte [bp-00eh], ch                    ; 88 6e f2                    ; 0xc2b2f
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc2b32
    mov cx, ax                                ; 89 c1                       ; 0xc2b35
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2b37
    call 022abh                               ; e8 6e f7                    ; 0xc2b3a
    jmp short 02b80h                          ; eb 41                       ; 0xc2b3d vgabios.c:1881
    push ax                                   ; 50                          ; 0xc2b3f vgabios.c:1883
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2b40
    push ax                                   ; 50                          ; 0xc2b43
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2b44
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc2b47
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc2b4a
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc2b4d
    xor bh, bh                                ; 30 ff                       ; 0xc2b50
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2b52
    xor dh, dh                                ; 30 f6                       ; 0xc2b55
    mov al, ch                                ; 88 e8                       ; 0xc2b57
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc2b59
    call 023bch                               ; e8 5d f8                    ; 0xc2b5c
    jmp short 02b80h                          ; eb 1f                       ; 0xc2b5f vgabios.c:1884
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc2b61 vgabios.c:1886
    push ax                                   ; 50                          ; 0xc2b64
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2b65
    mov bl, byte [bp-004h]                    ; 8a 5e fc                    ; 0xc2b68
    xor bh, bh                                ; 30 ff                       ; 0xc2b6b
    mov dl, byte [bp-00ah]                    ; 8a 56 f6                    ; 0xc2b6d
    xor dh, dh                                ; 30 f6                       ; 0xc2b70
    mov byte [bp-00eh], ch                    ; 88 6e f2                    ; 0xc2b72
    mov byte [bp-00dh], ah                    ; 88 66 f3                    ; 0xc2b75
    mov cx, ax                                ; 89 c1                       ; 0xc2b78
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2b7a
    call 024ceh                               ; e8 4e f9                    ; 0xc2b7d
    inc byte [bp-004h]                        ; fe 46 fc                    ; 0xc2b80 vgabios.c:1894
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2b83 vgabios.c:1896
    xor ah, ah                                ; 30 e4                       ; 0xc2b86
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc2b88
    jne short 02b93h                          ; 75 06                       ; 0xc2b8b
    mov byte [bp-004h], ah                    ; 88 66 fc                    ; 0xc2b8d vgabios.c:1897
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2b90 vgabios.c:1898
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2b93 vgabios.c:1903
    xor ah, ah                                ; 30 e4                       ; 0xc2b96
    cmp ax, word [bp-012h]                    ; 3b 46 ee                    ; 0xc2b98
    jne short 02bfeh                          ; 75 61                       ; 0xc2b9b
    mov bl, byte [bp-00ch]                    ; 8a 5e f4                    ; 0xc2b9d vgabios.c:1905
    xor bh, bh                                ; 30 ff                       ; 0xc2ba0
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2ba2
    mov ch, byte [bp-012h]                    ; 8a 6e ee                    ; 0xc2ba5
    db  0feh, 0cdh
    ; dec ch                                    ; fe cd                     ; 0xc2ba8
    mov cl, byte [bp-010h]                    ; 8a 4e f0                    ; 0xc2baa
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc2bad
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2baf
    jne short 02c00h                          ; 75 4a                       ; 0xc2bb4
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc2bb6 vgabios.c:1907
    mul word [bp-012h]                        ; f7 66 ee                    ; 0xc2bb9
    add ax, ax                                ; 01 c0                       ; 0xc2bbc
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc2bbe
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc2bc0
    xor dh, dh                                ; 30 f6                       ; 0xc2bc3
    inc ax                                    ; 40                          ; 0xc2bc5
    mul dx                                    ; f7 e2                       ; 0xc2bc6
    mov si, ax                                ; 89 c6                       ; 0xc2bc8
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2bca
    xor ah, ah                                ; 30 e4                       ; 0xc2bcd
    dec ax                                    ; 48                          ; 0xc2bcf
    mul word [bp-010h]                        ; f7 66 f0                    ; 0xc2bd0
    mov dx, ax                                ; 89 c2                       ; 0xc2bd3
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2bd5
    xor ah, ah                                ; 30 e4                       ; 0xc2bd8
    add ax, dx                                ; 01 d0                       ; 0xc2bda
    add ax, ax                                ; 01 c0                       ; 0xc2bdc
    add si, ax                                ; 01 c6                       ; 0xc2bde
    inc si                                    ; 46                          ; 0xc2be0 vgabios.c:1908
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc2be1 vgabios.c:45
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc2be5
    push strict byte 00001h                   ; 6a 01                       ; 0xc2be8 vgabios.c:1909
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2bea
    xor ah, ah                                ; 30 e4                       ; 0xc2bed
    push ax                                   ; 50                          ; 0xc2bef
    mov al, cl                                ; 88 c8                       ; 0xc2bf0
    push ax                                   ; 50                          ; 0xc2bf2
    mov al, ch                                ; 88 e8                       ; 0xc2bf3
    push ax                                   ; 50                          ; 0xc2bf5
    xor dh, dh                                ; 30 f6                       ; 0xc2bf6
    xor cx, cx                                ; 31 c9                       ; 0xc2bf8
    xor bx, bx                                ; 31 db                       ; 0xc2bfa
    jmp short 02c12h                          ; eb 14                       ; 0xc2bfc vgabios.c:1911
    jmp short 02c1bh                          ; eb 1b                       ; 0xc2bfe
    push strict byte 00001h                   ; 6a 01                       ; 0xc2c00 vgabios.c:1913
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2c02
    push ax                                   ; 50                          ; 0xc2c05
    mov al, cl                                ; 88 c8                       ; 0xc2c06
    push ax                                   ; 50                          ; 0xc2c08
    mov al, ch                                ; 88 e8                       ; 0xc2c09
    push ax                                   ; 50                          ; 0xc2c0b
    xor cx, cx                                ; 31 c9                       ; 0xc2c0c
    xor bx, bx                                ; 31 db                       ; 0xc2c0e
    xor dx, dx                                ; 31 d2                       ; 0xc2c10
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2c12
    call 01c23h                               ; e8 0b f0                    ; 0xc2c15
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc2c18 vgabios.c:1915
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2c1b vgabios.c:1919
    xor ah, ah                                ; 30 e4                       ; 0xc2c1e
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc2c20
    sal word [bp-014h], 008h                  ; c1 66 ec 08                 ; 0xc2c23
    mov al, byte [bp-004h]                    ; 8a 46 fc                    ; 0xc2c27
    add word [bp-014h], ax                    ; 01 46 ec                    ; 0xc2c2a
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc2c2d vgabios.c:1920
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2c30
    call 01293h                               ; e8 5d e6                    ; 0xc2c33
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2c36 vgabios.c:1921
    pop si                                    ; 5e                          ; 0xc2c39
    pop bp                                    ; 5d                          ; 0xc2c3a
    retn                                      ; c3                          ; 0xc2c3b
  ; disGetNextSymbol 0xc2c3c LB 0x1989 -> off=0x0 cb=000000000000002c uValue=00000000000c2c3c 'get_font_access'
get_font_access:                             ; 0xc2c3c LB 0x2c
    push bp                                   ; 55                          ; 0xc2c3c vgabios.c:1924
    mov bp, sp                                ; 89 e5                       ; 0xc2c3d
    push dx                                   ; 52                          ; 0xc2c3f
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2c40 vgabios.c:1926
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2c43
    out DX, ax                                ; ef                          ; 0xc2c46
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2c47 vgabios.c:1927
    out DX, ax                                ; ef                          ; 0xc2c4a
    mov ax, 00704h                            ; b8 04 07                    ; 0xc2c4b vgabios.c:1928
    out DX, ax                                ; ef                          ; 0xc2c4e
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2c4f vgabios.c:1929
    out DX, ax                                ; ef                          ; 0xc2c52
    mov ax, 00204h                            ; b8 04 02                    ; 0xc2c53 vgabios.c:1930
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2c56
    out DX, ax                                ; ef                          ; 0xc2c59
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2c5a vgabios.c:1931
    out DX, ax                                ; ef                          ; 0xc2c5d
    mov ax, 00406h                            ; b8 06 04                    ; 0xc2c5e vgabios.c:1932
    out DX, ax                                ; ef                          ; 0xc2c61
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2c62 vgabios.c:1933
    pop dx                                    ; 5a                          ; 0xc2c65
    pop bp                                    ; 5d                          ; 0xc2c66
    retn                                      ; c3                          ; 0xc2c67
  ; disGetNextSymbol 0xc2c68 LB 0x195d -> off=0x0 cb=000000000000003c uValue=00000000000c2c68 'release_font_access'
release_font_access:                         ; 0xc2c68 LB 0x3c
    push bp                                   ; 55                          ; 0xc2c68 vgabios.c:1935
    mov bp, sp                                ; 89 e5                       ; 0xc2c69
    push dx                                   ; 52                          ; 0xc2c6b
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2c6c vgabios.c:1937
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2c6f
    out DX, ax                                ; ef                          ; 0xc2c72
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2c73 vgabios.c:1938
    out DX, ax                                ; ef                          ; 0xc2c76
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2c77 vgabios.c:1939
    out DX, ax                                ; ef                          ; 0xc2c7a
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2c7b vgabios.c:1940
    out DX, ax                                ; ef                          ; 0xc2c7e
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2c7f vgabios.c:1941
    in AL, DX                                 ; ec                          ; 0xc2c82
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2c83
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2c85
    sal ax, 002h                              ; c1 e0 02                    ; 0xc2c88
    or AL, strict byte 00ah                   ; 0c 0a                       ; 0xc2c8b
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2c8d
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2c90
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2c92
    out DX, ax                                ; ef                          ; 0xc2c95
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2c96 vgabios.c:1942
    out DX, ax                                ; ef                          ; 0xc2c99
    mov ax, 01005h                            ; b8 05 10                    ; 0xc2c9a vgabios.c:1943
    out DX, ax                                ; ef                          ; 0xc2c9d
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2c9e vgabios.c:1944
    pop dx                                    ; 5a                          ; 0xc2ca1
    pop bp                                    ; 5d                          ; 0xc2ca2
    retn                                      ; c3                          ; 0xc2ca3
  ; disGetNextSymbol 0xc2ca4 LB 0x1921 -> off=0x0 cb=00000000000000b1 uValue=00000000000c2ca4 'set_scan_lines'
set_scan_lines:                              ; 0xc2ca4 LB 0xb1
    push bp                                   ; 55                          ; 0xc2ca4 vgabios.c:1946
    mov bp, sp                                ; 89 e5                       ; 0xc2ca5
    push bx                                   ; 53                          ; 0xc2ca7
    push cx                                   ; 51                          ; 0xc2ca8
    push dx                                   ; 52                          ; 0xc2ca9
    push si                                   ; 56                          ; 0xc2caa
    push di                                   ; 57                          ; 0xc2cab
    mov bl, al                                ; 88 c3                       ; 0xc2cac
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2cae vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cb1
    mov es, ax                                ; 8e c0                       ; 0xc2cb4
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc2cb6
    mov cx, si                                ; 89 f1                       ; 0xc2cb9 vgabios.c:58
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc2cbb vgabios.c:1952
    mov dx, si                                ; 89 f2                       ; 0xc2cbd
    out DX, AL                                ; ee                          ; 0xc2cbf
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2cc0 vgabios.c:1953
    in AL, DX                                 ; ec                          ; 0xc2cc3
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2cc4
    mov ah, al                                ; 88 c4                       ; 0xc2cc6 vgabios.c:1954
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2cc8
    mov al, bl                                ; 88 d8                       ; 0xc2ccb
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2ccd
    or al, ah                                 ; 08 e0                       ; 0xc2ccf
    out DX, AL                                ; ee                          ; 0xc2cd1 vgabios.c:1955
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2cd2 vgabios.c:1956
    jne short 02cdfh                          ; 75 08                       ; 0xc2cd5
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2cd7 vgabios.c:1958
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2cda
    jmp short 02cech                          ; eb 0d                       ; 0xc2cdd vgabios.c:1960
    mov dl, bl                                ; 88 da                       ; 0xc2cdf vgabios.c:1962
    sub dl, 003h                              ; 80 ea 03                    ; 0xc2ce1
    xor dh, dh                                ; 30 f6                       ; 0xc2ce4
    mov al, bl                                ; 88 d8                       ; 0xc2ce6
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2ce8
    xor ah, ah                                ; 30 e4                       ; 0xc2cea
    call 0118ch                               ; e8 9d e4                    ; 0xc2cec
    xor bh, bh                                ; 30 ff                       ; 0xc2cef vgabios.c:1964
    mov si, 00085h                            ; be 85 00                    ; 0xc2cf1 vgabios.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2cf4
    mov es, ax                                ; 8e c0                       ; 0xc2cf7
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc2cf9
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2cfc vgabios.c:1965
    mov dx, cx                                ; 89 ca                       ; 0xc2cfe
    out DX, AL                                ; ee                          ; 0xc2d00
    mov si, cx                                ; 89 ce                       ; 0xc2d01 vgabios.c:1966
    inc si                                    ; 46                          ; 0xc2d03
    mov dx, si                                ; 89 f2                       ; 0xc2d04
    in AL, DX                                 ; ec                          ; 0xc2d06
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2d07
    mov di, ax                                ; 89 c7                       ; 0xc2d09
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2d0b vgabios.c:1967
    mov dx, cx                                ; 89 ca                       ; 0xc2d0d
    out DX, AL                                ; ee                          ; 0xc2d0f
    mov dx, si                                ; 89 f2                       ; 0xc2d10 vgabios.c:1968
    in AL, DX                                 ; ec                          ; 0xc2d12
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2d13
    mov dl, al                                ; 88 c2                       ; 0xc2d15 vgabios.c:1969
    and dl, 002h                              ; 80 e2 02                    ; 0xc2d17
    xor dh, dh                                ; 30 f6                       ; 0xc2d1a
    sal dx, 007h                              ; c1 e2 07                    ; 0xc2d1c
    and AL, strict byte 040h                  ; 24 40                       ; 0xc2d1f
    xor ah, ah                                ; 30 e4                       ; 0xc2d21
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2d23
    add ax, dx                                ; 01 d0                       ; 0xc2d26
    inc ax                                    ; 40                          ; 0xc2d28
    add ax, di                                ; 01 f8                       ; 0xc2d29
    xor dx, dx                                ; 31 d2                       ; 0xc2d2b vgabios.c:1970
    div bx                                    ; f7 f3                       ; 0xc2d2d
    mov dl, al                                ; 88 c2                       ; 0xc2d2f vgabios.c:1971
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc2d31
    mov si, 00084h                            ; be 84 00                    ; 0xc2d33 vgabios.c:52
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc2d36
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc2d39 vgabios.c:57
    mov dx, word [es:si]                      ; 26 8b 14                    ; 0xc2d3c
    xor ah, ah                                ; 30 e4                       ; 0xc2d3f vgabios.c:1973
    mul dx                                    ; f7 e2                       ; 0xc2d41
    add ax, ax                                ; 01 c0                       ; 0xc2d43
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2d45 vgabios.c:62
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc2d48
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc2d4b vgabios.c:1974
    pop di                                    ; 5f                          ; 0xc2d4e
    pop si                                    ; 5e                          ; 0xc2d4f
    pop dx                                    ; 5a                          ; 0xc2d50
    pop cx                                    ; 59                          ; 0xc2d51
    pop bx                                    ; 5b                          ; 0xc2d52
    pop bp                                    ; 5d                          ; 0xc2d53
    retn                                      ; c3                          ; 0xc2d54
  ; disGetNextSymbol 0xc2d55 LB 0x1870 -> off=0x0 cb=0000000000000023 uValue=00000000000c2d55 'biosfn_set_font_block'
biosfn_set_font_block:                       ; 0xc2d55 LB 0x23
    push bp                                   ; 55                          ; 0xc2d55 vgabios.c:1976
    mov bp, sp                                ; 89 e5                       ; 0xc2d56
    push bx                                   ; 53                          ; 0xc2d58
    push dx                                   ; 52                          ; 0xc2d59
    mov bl, al                                ; 88 c3                       ; 0xc2d5a
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2d5c vgabios.c:1978
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2d5f
    out DX, ax                                ; ef                          ; 0xc2d62
    mov al, bl                                ; 88 d8                       ; 0xc2d63 vgabios.c:1979
    xor ah, ah                                ; 30 e4                       ; 0xc2d65
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2d67
    or AL, strict byte 003h                   ; 0c 03                       ; 0xc2d6a
    out DX, ax                                ; ef                          ; 0xc2d6c
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2d6d vgabios.c:1980
    out DX, ax                                ; ef                          ; 0xc2d70
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2d71 vgabios.c:1981
    pop dx                                    ; 5a                          ; 0xc2d74
    pop bx                                    ; 5b                          ; 0xc2d75
    pop bp                                    ; 5d                          ; 0xc2d76
    retn                                      ; c3                          ; 0xc2d77
  ; disGetNextSymbol 0xc2d78 LB 0x184d -> off=0x0 cb=000000000000007f uValue=00000000000c2d78 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2d78 LB 0x7f
    push bp                                   ; 55                          ; 0xc2d78 vgabios.c:1983
    mov bp, sp                                ; 89 e5                       ; 0xc2d79
    push si                                   ; 56                          ; 0xc2d7b
    push di                                   ; 57                          ; 0xc2d7c
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2d7d
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2d80
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc2d83
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc2d86
    mov word [bp-00eh], cx                    ; 89 4e f2                    ; 0xc2d89
    call 02c3ch                               ; e8 ad fe                    ; 0xc2d8c vgabios.c:1988
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2d8f vgabios.c:1989
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2d92
    xor ah, ah                                ; 30 e4                       ; 0xc2d94
    mov bx, ax                                ; 89 c3                       ; 0xc2d96
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2d98
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2d9b
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2d9e
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2da0
    add bx, ax                                ; 01 c3                       ; 0xc2da3
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2da5
    xor bx, bx                                ; 31 db                       ; 0xc2da8 vgabios.c:1990
    cmp bx, word [bp-00eh]                    ; 3b 5e f2                    ; 0xc2daa
    jnc short 02dddh                          ; 73 2e                       ; 0xc2dad
    mov cl, byte [bp+008h]                    ; 8a 4e 08                    ; 0xc2daf vgabios.c:1992
    xor ch, ch                                ; 30 ed                       ; 0xc2db2
    mov ax, bx                                ; 89 d8                       ; 0xc2db4
    mul cx                                    ; f7 e1                       ; 0xc2db6
    mov si, word [bp-00ah]                    ; 8b 76 f6                    ; 0xc2db8
    add si, ax                                ; 01 c6                       ; 0xc2dbb
    mov ax, word [bp+004h]                    ; 8b 46 04                    ; 0xc2dbd vgabios.c:1993
    add ax, bx                                ; 01 d8                       ; 0xc2dc0
    sal ax, 005h                              ; c1 e0 05                    ; 0xc2dc2
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc2dc5
    add di, ax                                ; 01 c7                       ; 0xc2dc8
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc2dca vgabios.c:1994
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2dcd
    mov es, ax                                ; 8e c0                       ; 0xc2dd0
    jcxz 02ddah                               ; e3 06                       ; 0xc2dd2
    push DS                                   ; 1e                          ; 0xc2dd4
    mov ds, dx                                ; 8e da                       ; 0xc2dd5
    rep movsb                                 ; f3 a4                       ; 0xc2dd7
    pop DS                                    ; 1f                          ; 0xc2dd9
    inc bx                                    ; 43                          ; 0xc2dda vgabios.c:1995
    jmp short 02daah                          ; eb cd                       ; 0xc2ddb
    call 02c68h                               ; e8 88 fe                    ; 0xc2ddd vgabios.c:1996
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2de0 vgabios.c:1997
    jc short 02deeh                           ; 72 08                       ; 0xc2de4
    mov al, byte [bp+008h]                    ; 8a 46 08                    ; 0xc2de6 vgabios.c:1999
    xor ah, ah                                ; 30 e4                       ; 0xc2de9
    call 02ca4h                               ; e8 b6 fe                    ; 0xc2deb
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2dee vgabios.c:2001
    pop di                                    ; 5f                          ; 0xc2df1
    pop si                                    ; 5e                          ; 0xc2df2
    pop bp                                    ; 5d                          ; 0xc2df3
    retn 00006h                               ; c2 06 00                    ; 0xc2df4
  ; disGetNextSymbol 0xc2df7 LB 0x17ce -> off=0x0 cb=000000000000006d uValue=00000000000c2df7 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2df7 LB 0x6d
    push bp                                   ; 55                          ; 0xc2df7 vgabios.c:2003
    mov bp, sp                                ; 89 e5                       ; 0xc2df8
    push bx                                   ; 53                          ; 0xc2dfa
    push cx                                   ; 51                          ; 0xc2dfb
    push si                                   ; 56                          ; 0xc2dfc
    push di                                   ; 57                          ; 0xc2dfd
    push ax                                   ; 50                          ; 0xc2dfe
    push ax                                   ; 50                          ; 0xc2dff
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2e00
    call 02c3ch                               ; e8 36 fe                    ; 0xc2e03 vgabios.c:2007
    mov al, dl                                ; 88 d0                       ; 0xc2e06 vgabios.c:2008
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2e08
    xor ah, ah                                ; 30 e4                       ; 0xc2e0a
    mov bx, ax                                ; 89 c3                       ; 0xc2e0c
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2e0e
    mov al, dl                                ; 88 d0                       ; 0xc2e11
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2e13
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2e15
    add bx, ax                                ; 01 c3                       ; 0xc2e18
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2e1a
    xor bx, bx                                ; 31 db                       ; 0xc2e1d vgabios.c:2009
    jmp short 02e27h                          ; eb 06                       ; 0xc2e1f
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2e21
    jnc short 02e4ch                          ; 73 25                       ; 0xc2e25
    imul si, bx, strict byte 0000eh           ; 6b f3 0e                    ; 0xc2e27 vgabios.c:2011
    mov di, bx                                ; 89 df                       ; 0xc2e2a vgabios.c:2012
    sal di, 005h                              ; c1 e7 05                    ; 0xc2e2c
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2e2f
    add si, 05d6ch                            ; 81 c6 6c 5d                 ; 0xc2e32 vgabios.c:2013
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2e36
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2e39
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2e3c
    mov es, ax                                ; 8e c0                       ; 0xc2e3f
    jcxz 02e49h                               ; e3 06                       ; 0xc2e41
    push DS                                   ; 1e                          ; 0xc2e43
    mov ds, dx                                ; 8e da                       ; 0xc2e44
    rep movsb                                 ; f3 a4                       ; 0xc2e46
    pop DS                                    ; 1f                          ; 0xc2e48
    inc bx                                    ; 43                          ; 0xc2e49 vgabios.c:2014
    jmp short 02e21h                          ; eb d5                       ; 0xc2e4a
    call 02c68h                               ; e8 19 fe                    ; 0xc2e4c vgabios.c:2015
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2e4f vgabios.c:2016
    jc short 02e5bh                           ; 72 06                       ; 0xc2e53
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2e55 vgabios.c:2018
    call 02ca4h                               ; e8 49 fe                    ; 0xc2e58
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2e5b vgabios.c:2020
    pop di                                    ; 5f                          ; 0xc2e5e
    pop si                                    ; 5e                          ; 0xc2e5f
    pop cx                                    ; 59                          ; 0xc2e60
    pop bx                                    ; 5b                          ; 0xc2e61
    pop bp                                    ; 5d                          ; 0xc2e62
    retn                                      ; c3                          ; 0xc2e63
  ; disGetNextSymbol 0xc2e64 LB 0x1761 -> off=0x0 cb=000000000000006f uValue=00000000000c2e64 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2e64 LB 0x6f
    push bp                                   ; 55                          ; 0xc2e64 vgabios.c:2022
    mov bp, sp                                ; 89 e5                       ; 0xc2e65
    push bx                                   ; 53                          ; 0xc2e67
    push cx                                   ; 51                          ; 0xc2e68
    push si                                   ; 56                          ; 0xc2e69
    push di                                   ; 57                          ; 0xc2e6a
    push ax                                   ; 50                          ; 0xc2e6b
    push ax                                   ; 50                          ; 0xc2e6c
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2e6d
    call 02c3ch                               ; e8 c9 fd                    ; 0xc2e70 vgabios.c:2026
    mov al, dl                                ; 88 d0                       ; 0xc2e73 vgabios.c:2027
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2e75
    xor ah, ah                                ; 30 e4                       ; 0xc2e77
    mov bx, ax                                ; 89 c3                       ; 0xc2e79
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2e7b
    mov al, dl                                ; 88 d0                       ; 0xc2e7e
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2e80
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2e82
    add bx, ax                                ; 01 c3                       ; 0xc2e85
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2e87
    xor bx, bx                                ; 31 db                       ; 0xc2e8a vgabios.c:2028
    jmp short 02e94h                          ; eb 06                       ; 0xc2e8c
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2e8e
    jnc short 02ebbh                          ; 73 27                       ; 0xc2e92
    mov si, bx                                ; 89 de                       ; 0xc2e94 vgabios.c:2030
    sal si, 003h                              ; c1 e6 03                    ; 0xc2e96
    mov di, bx                                ; 89 df                       ; 0xc2e99 vgabios.c:2031
    sal di, 005h                              ; c1 e7 05                    ; 0xc2e9b
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2e9e
    add si, 0556ch                            ; 81 c6 6c 55                 ; 0xc2ea1 vgabios.c:2032
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2ea5
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2ea8
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2eab
    mov es, ax                                ; 8e c0                       ; 0xc2eae
    jcxz 02eb8h                               ; e3 06                       ; 0xc2eb0
    push DS                                   ; 1e                          ; 0xc2eb2
    mov ds, dx                                ; 8e da                       ; 0xc2eb3
    rep movsb                                 ; f3 a4                       ; 0xc2eb5
    pop DS                                    ; 1f                          ; 0xc2eb7
    inc bx                                    ; 43                          ; 0xc2eb8 vgabios.c:2033
    jmp short 02e8eh                          ; eb d3                       ; 0xc2eb9
    call 02c68h                               ; e8 aa fd                    ; 0xc2ebb vgabios.c:2034
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2ebe vgabios.c:2035
    jc short 02ecah                           ; 72 06                       ; 0xc2ec2
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2ec4 vgabios.c:2037
    call 02ca4h                               ; e8 da fd                    ; 0xc2ec7
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2eca vgabios.c:2039
    pop di                                    ; 5f                          ; 0xc2ecd
    pop si                                    ; 5e                          ; 0xc2ece
    pop cx                                    ; 59                          ; 0xc2ecf
    pop bx                                    ; 5b                          ; 0xc2ed0
    pop bp                                    ; 5d                          ; 0xc2ed1
    retn                                      ; c3                          ; 0xc2ed2
  ; disGetNextSymbol 0xc2ed3 LB 0x16f2 -> off=0x0 cb=000000000000006f uValue=00000000000c2ed3 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2ed3 LB 0x6f
    push bp                                   ; 55                          ; 0xc2ed3 vgabios.c:2042
    mov bp, sp                                ; 89 e5                       ; 0xc2ed4
    push bx                                   ; 53                          ; 0xc2ed6
    push cx                                   ; 51                          ; 0xc2ed7
    push si                                   ; 56                          ; 0xc2ed8
    push di                                   ; 57                          ; 0xc2ed9
    push ax                                   ; 50                          ; 0xc2eda
    push ax                                   ; 50                          ; 0xc2edb
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2edc
    call 02c3ch                               ; e8 5a fd                    ; 0xc2edf vgabios.c:2046
    mov al, dl                                ; 88 d0                       ; 0xc2ee2 vgabios.c:2047
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2ee4
    xor ah, ah                                ; 30 e4                       ; 0xc2ee6
    mov bx, ax                                ; 89 c3                       ; 0xc2ee8
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2eea
    mov al, dl                                ; 88 d0                       ; 0xc2eed
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2eef
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2ef1
    add bx, ax                                ; 01 c3                       ; 0xc2ef4
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2ef6
    xor bx, bx                                ; 31 db                       ; 0xc2ef9 vgabios.c:2048
    jmp short 02f03h                          ; eb 06                       ; 0xc2efb
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2efd
    jnc short 02f2ah                          ; 73 27                       ; 0xc2f01
    mov si, bx                                ; 89 de                       ; 0xc2f03 vgabios.c:2050
    sal si, 004h                              ; c1 e6 04                    ; 0xc2f05
    mov di, bx                                ; 89 df                       ; 0xc2f08 vgabios.c:2051
    sal di, 005h                              ; c1 e7 05                    ; 0xc2f0a
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2f0d
    add si, 06b6ch                            ; 81 c6 6c 6b                 ; 0xc2f10 vgabios.c:2052
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2f14
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2f17
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2f1a
    mov es, ax                                ; 8e c0                       ; 0xc2f1d
    jcxz 02f27h                               ; e3 06                       ; 0xc2f1f
    push DS                                   ; 1e                          ; 0xc2f21
    mov ds, dx                                ; 8e da                       ; 0xc2f22
    rep movsb                                 ; f3 a4                       ; 0xc2f24
    pop DS                                    ; 1f                          ; 0xc2f26
    inc bx                                    ; 43                          ; 0xc2f27 vgabios.c:2053
    jmp short 02efdh                          ; eb d3                       ; 0xc2f28
    call 02c68h                               ; e8 3b fd                    ; 0xc2f2a vgabios.c:2054
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2f2d vgabios.c:2055
    jc short 02f39h                           ; 72 06                       ; 0xc2f31
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2f33 vgabios.c:2057
    call 02ca4h                               ; e8 6b fd                    ; 0xc2f36
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2f39 vgabios.c:2059
    pop di                                    ; 5f                          ; 0xc2f3c
    pop si                                    ; 5e                          ; 0xc2f3d
    pop cx                                    ; 59                          ; 0xc2f3e
    pop bx                                    ; 5b                          ; 0xc2f3f
    pop bp                                    ; 5d                          ; 0xc2f40
    retn                                      ; c3                          ; 0xc2f41
  ; disGetNextSymbol 0xc2f42 LB 0x1683 -> off=0x0 cb=0000000000000016 uValue=00000000000c2f42 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2f42 LB 0x16
    push bp                                   ; 55                          ; 0xc2f42 vgabios.c:2061
    mov bp, sp                                ; 89 e5                       ; 0xc2f43
    push bx                                   ; 53                          ; 0xc2f45
    push cx                                   ; 51                          ; 0xc2f46
    mov bx, dx                                ; 89 d3                       ; 0xc2f47 vgabios.c:2063
    mov cx, ax                                ; 89 c1                       ; 0xc2f49
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc2f4b
    call 009f0h                               ; e8 9f da                    ; 0xc2f4e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2f51 vgabios.c:2064
    pop cx                                    ; 59                          ; 0xc2f54
    pop bx                                    ; 5b                          ; 0xc2f55
    pop bp                                    ; 5d                          ; 0xc2f56
    retn                                      ; c3                          ; 0xc2f57
  ; disGetNextSymbol 0xc2f58 LB 0x166d -> off=0x0 cb=000000000000004d uValue=00000000000c2f58 'set_gfx_font'
set_gfx_font:                                ; 0xc2f58 LB 0x4d
    push bp                                   ; 55                          ; 0xc2f58 vgabios.c:2066
    mov bp, sp                                ; 89 e5                       ; 0xc2f59
    push si                                   ; 56                          ; 0xc2f5b
    push di                                   ; 57                          ; 0xc2f5c
    mov si, ax                                ; 89 c6                       ; 0xc2f5d
    mov ax, dx                                ; 89 d0                       ; 0xc2f5f
    mov di, bx                                ; 89 df                       ; 0xc2f61
    mov dl, cl                                ; 88 ca                       ; 0xc2f63
    mov bx, si                                ; 89 f3                       ; 0xc2f65 vgabios.c:2070
    mov cx, ax                                ; 89 c1                       ; 0xc2f67
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc2f69
    call 009f0h                               ; e8 81 da                    ; 0xc2f6c
    test dl, dl                               ; 84 d2                       ; 0xc2f6f vgabios.c:2071
    je short 02f85h                           ; 74 12                       ; 0xc2f71
    cmp dl, 003h                              ; 80 fa 03                    ; 0xc2f73 vgabios.c:2072
    jbe short 02f7ah                          ; 76 02                       ; 0xc2f76
    mov DL, strict byte 002h                  ; b2 02                       ; 0xc2f78 vgabios.c:2073
    mov bl, dl                                ; 88 d3                       ; 0xc2f7a vgabios.c:2074
    xor bh, bh                                ; 30 ff                       ; 0xc2f7c
    mov al, byte [bx+07dfdh]                  ; 8a 87 fd 7d                 ; 0xc2f7e
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2f82
    mov bx, 00085h                            ; bb 85 00                    ; 0xc2f85 vgabios.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f88
    mov es, ax                                ; 8e c0                       ; 0xc2f8b
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc2f8d
    mov al, byte [bp+004h]                    ; 8a 46 04                    ; 0xc2f90 vgabios.c:2079
    xor ah, ah                                ; 30 e4                       ; 0xc2f93
    dec ax                                    ; 48                          ; 0xc2f95
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2f96 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2f99
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2f9c vgabios.c:2080
    pop di                                    ; 5f                          ; 0xc2f9f
    pop si                                    ; 5e                          ; 0xc2fa0
    pop bp                                    ; 5d                          ; 0xc2fa1
    retn 00002h                               ; c2 02 00                    ; 0xc2fa2
  ; disGetNextSymbol 0xc2fa5 LB 0x1620 -> off=0x0 cb=000000000000001d uValue=00000000000c2fa5 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc2fa5 LB 0x1d
    push bp                                   ; 55                          ; 0xc2fa5 vgabios.c:2082
    mov bp, sp                                ; 89 e5                       ; 0xc2fa6
    push si                                   ; 56                          ; 0xc2fa8
    mov si, ax                                ; 89 c6                       ; 0xc2fa9
    mov ax, dx                                ; 89 d0                       ; 0xc2fab
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc2fad vgabios.c:2085
    xor dh, dh                                ; 30 f6                       ; 0xc2fb0
    push dx                                   ; 52                          ; 0xc2fb2
    xor ch, ch                                ; 30 ed                       ; 0xc2fb3
    mov dx, si                                ; 89 f2                       ; 0xc2fb5
    call 02f58h                               ; e8 9e ff                    ; 0xc2fb7
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2fba vgabios.c:2086
    pop si                                    ; 5e                          ; 0xc2fbd
    pop bp                                    ; 5d                          ; 0xc2fbe
    retn 00002h                               ; c2 02 00                    ; 0xc2fbf
  ; disGetNextSymbol 0xc2fc2 LB 0x1603 -> off=0x0 cb=0000000000000022 uValue=00000000000c2fc2 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc2fc2 LB 0x22
    push bp                                   ; 55                          ; 0xc2fc2 vgabios.c:2091
    mov bp, sp                                ; 89 e5                       ; 0xc2fc3
    push bx                                   ; 53                          ; 0xc2fc5
    push cx                                   ; 51                          ; 0xc2fc6
    mov bl, al                                ; 88 c3                       ; 0xc2fc7
    mov al, dl                                ; 88 d0                       ; 0xc2fc9
    xor ah, ah                                ; 30 e4                       ; 0xc2fcb vgabios.c:2093
    push ax                                   ; 50                          ; 0xc2fcd
    mov al, bl                                ; 88 d8                       ; 0xc2fce
    mov cx, ax                                ; 89 c1                       ; 0xc2fd0
    mov bx, strict word 0000eh                ; bb 0e 00                    ; 0xc2fd2
    mov ax, 05d6ch                            ; b8 6c 5d                    ; 0xc2fd5
    mov dx, ds                                ; 8c da                       ; 0xc2fd8
    call 02f58h                               ; e8 7b ff                    ; 0xc2fda
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2fdd vgabios.c:2094
    pop cx                                    ; 59                          ; 0xc2fe0
    pop bx                                    ; 5b                          ; 0xc2fe1
    pop bp                                    ; 5d                          ; 0xc2fe2
    retn                                      ; c3                          ; 0xc2fe3
  ; disGetNextSymbol 0xc2fe4 LB 0x15e1 -> off=0x0 cb=0000000000000022 uValue=00000000000c2fe4 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc2fe4 LB 0x22
    push bp                                   ; 55                          ; 0xc2fe4 vgabios.c:2095
    mov bp, sp                                ; 89 e5                       ; 0xc2fe5
    push bx                                   ; 53                          ; 0xc2fe7
    push cx                                   ; 51                          ; 0xc2fe8
    mov bl, al                                ; 88 c3                       ; 0xc2fe9
    mov al, dl                                ; 88 d0                       ; 0xc2feb
    xor ah, ah                                ; 30 e4                       ; 0xc2fed vgabios.c:2097
    push ax                                   ; 50                          ; 0xc2fef
    mov al, bl                                ; 88 d8                       ; 0xc2ff0
    mov cx, ax                                ; 89 c1                       ; 0xc2ff2
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc2ff4
    mov ax, 0556ch                            ; b8 6c 55                    ; 0xc2ff7
    mov dx, ds                                ; 8c da                       ; 0xc2ffa
    call 02f58h                               ; e8 59 ff                    ; 0xc2ffc
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2fff vgabios.c:2098
    pop cx                                    ; 59                          ; 0xc3002
    pop bx                                    ; 5b                          ; 0xc3003
    pop bp                                    ; 5d                          ; 0xc3004
    retn                                      ; c3                          ; 0xc3005
  ; disGetNextSymbol 0xc3006 LB 0x15bf -> off=0x0 cb=0000000000000022 uValue=00000000000c3006 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc3006 LB 0x22
    push bp                                   ; 55                          ; 0xc3006 vgabios.c:2099
    mov bp, sp                                ; 89 e5                       ; 0xc3007
    push bx                                   ; 53                          ; 0xc3009
    push cx                                   ; 51                          ; 0xc300a
    mov bl, al                                ; 88 c3                       ; 0xc300b
    mov al, dl                                ; 88 d0                       ; 0xc300d
    xor ah, ah                                ; 30 e4                       ; 0xc300f vgabios.c:2101
    push ax                                   ; 50                          ; 0xc3011
    mov al, bl                                ; 88 d8                       ; 0xc3012
    mov cx, ax                                ; 89 c1                       ; 0xc3014
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc3016
    mov ax, 06b6ch                            ; b8 6c 6b                    ; 0xc3019
    mov dx, ds                                ; 8c da                       ; 0xc301c
    call 02f58h                               ; e8 37 ff                    ; 0xc301e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3021 vgabios.c:2102
    pop cx                                    ; 59                          ; 0xc3024
    pop bx                                    ; 5b                          ; 0xc3025
    pop bp                                    ; 5d                          ; 0xc3026
    retn                                      ; c3                          ; 0xc3027
  ; disGetNextSymbol 0xc3028 LB 0x159d -> off=0x0 cb=0000000000000005 uValue=00000000000c3028 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc3028 LB 0x5
    push bp                                   ; 55                          ; 0xc3028 vgabios.c:2104
    mov bp, sp                                ; 89 e5                       ; 0xc3029
    pop bp                                    ; 5d                          ; 0xc302b vgabios.c:2109
    retn                                      ; c3                          ; 0xc302c
  ; disGetNextSymbol 0xc302d LB 0x1598 -> off=0x0 cb=0000000000000032 uValue=00000000000c302d 'biosfn_set_txt_lines'
biosfn_set_txt_lines:                        ; 0xc302d LB 0x32
    push bx                                   ; 53                          ; 0xc302d vgabios.c:2111
    push si                                   ; 56                          ; 0xc302e
    push bp                                   ; 55                          ; 0xc302f
    mov bp, sp                                ; 89 e5                       ; 0xc3030
    mov bl, al                                ; 88 c3                       ; 0xc3032
    mov si, 00089h                            ; be 89 00                    ; 0xc3034 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3037
    mov es, ax                                ; 8e c0                       ; 0xc303a
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc303c
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc303f vgabios.c:2117
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc3041 vgabios.c:2119
    je short 0304eh                           ; 74 08                       ; 0xc3044
    test bl, bl                               ; 84 db                       ; 0xc3046
    jne short 03050h                          ; 75 06                       ; 0xc3048
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc304a vgabios.c:2122
    jmp short 03050h                          ; eb 02                       ; 0xc304c vgabios.c:2123
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc304e vgabios.c:2125
    mov bx, 00089h                            ; bb 89 00                    ; 0xc3050 vgabios.c:52
    mov si, strict word 00040h                ; be 40 00                    ; 0xc3053
    mov es, si                                ; 8e c6                       ; 0xc3056
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3058
    pop bp                                    ; 5d                          ; 0xc305b vgabios.c:2129
    pop si                                    ; 5e                          ; 0xc305c
    pop bx                                    ; 5b                          ; 0xc305d
    retn                                      ; c3                          ; 0xc305e
  ; disGetNextSymbol 0xc305f LB 0x1566 -> off=0x0 cb=0000000000000005 uValue=00000000000c305f 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc305f LB 0x5
    push bp                                   ; 55                          ; 0xc305f vgabios.c:2132
    mov bp, sp                                ; 89 e5                       ; 0xc3060
    pop bp                                    ; 5d                          ; 0xc3062 vgabios.c:2137
    retn                                      ; c3                          ; 0xc3063
  ; disGetNextSymbol 0xc3064 LB 0x1561 -> off=0x0 cb=0000000000000005 uValue=00000000000c3064 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc3064 LB 0x5
    push bp                                   ; 55                          ; 0xc3064 vgabios.c:2138
    mov bp, sp                                ; 89 e5                       ; 0xc3065
    pop bp                                    ; 5d                          ; 0xc3067 vgabios.c:2143
    retn                                      ; c3                          ; 0xc3068
  ; disGetNextSymbol 0xc3069 LB 0x155c -> off=0x0 cb=000000000000009d uValue=00000000000c3069 'biosfn_write_string'
biosfn_write_string:                         ; 0xc3069 LB 0x9d
    push bp                                   ; 55                          ; 0xc3069 vgabios.c:2146
    mov bp, sp                                ; 89 e5                       ; 0xc306a
    push si                                   ; 56                          ; 0xc306c
    push di                                   ; 57                          ; 0xc306d
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc306e
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3071
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc3074
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc3077
    mov si, cx                                ; 89 ce                       ; 0xc307a
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc307c
    mov al, dl                                ; 88 d0                       ; 0xc307f vgabios.c:2153
    xor ah, ah                                ; 30 e4                       ; 0xc3081
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc3083
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc3086
    call 00a96h                               ; e8 0a da                    ; 0xc3089
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc308c vgabios.c:2156
    jne short 030a3h                          ; 75 11                       ; 0xc3090
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3092 vgabios.c:2157
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc3095
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc3098 vgabios.c:2158
    xor al, al                                ; 30 c0                       ; 0xc309b
    shr ax, 008h                              ; c1 e8 08                    ; 0xc309d
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc30a0
    mov dl, byte [bp+004h]                    ; 8a 56 04                    ; 0xc30a3 vgabios.c:2161
    xor dh, dh                                ; 30 f6                       ; 0xc30a6
    sal dx, 008h                              ; c1 e2 08                    ; 0xc30a8
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc30ab
    xor ah, ah                                ; 30 e4                       ; 0xc30ae
    add dx, ax                                ; 01 c2                       ; 0xc30b0
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc30b2 vgabios.c:2162
    call 01293h                               ; e8 db e1                    ; 0xc30b5
    dec si                                    ; 4e                          ; 0xc30b8 vgabios.c:2164
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc30b9
    je short 030ech                           ; 74 2e                       ; 0xc30bc
    mov bx, di                                ; 89 fb                       ; 0xc30be vgabios.c:2166
    inc di                                    ; 47                          ; 0xc30c0
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc30c1 vgabios.c:47
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc30c4
    test byte [bp-006h], 002h                 ; f6 46 fa 02                 ; 0xc30c7 vgabios.c:2167
    je short 030d6h                           ; 74 09                       ; 0xc30cb
    mov bx, di                                ; 89 fb                       ; 0xc30cd vgabios.c:2168
    inc di                                    ; 47                          ; 0xc30cf
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc30d0 vgabios.c:47
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc30d3 vgabios.c:48
    mov bl, byte [bp-00ah]                    ; 8a 5e f6                    ; 0xc30d6 vgabios.c:2170
    xor bh, bh                                ; 30 ff                       ; 0xc30d9
    mov dl, byte [bp-008h]                    ; 8a 56 f8                    ; 0xc30db
    xor dh, dh                                ; 30 f6                       ; 0xc30de
    mov al, ah                                ; 88 e0                       ; 0xc30e0
    xor ah, ah                                ; 30 e4                       ; 0xc30e2
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc30e4
    call 029e4h                               ; e8 fa f8                    ; 0xc30e7
    jmp short 030b8h                          ; eb cc                       ; 0xc30ea vgabios.c:2171
    test byte [bp-006h], 001h                 ; f6 46 fa 01                 ; 0xc30ec vgabios.c:2174
    jne short 030fdh                          ; 75 0b                       ; 0xc30f0
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc30f2 vgabios.c:2175
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc30f5
    xor ah, ah                                ; 30 e4                       ; 0xc30f8
    call 01293h                               ; e8 96 e1                    ; 0xc30fa
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc30fd vgabios.c:2176
    pop di                                    ; 5f                          ; 0xc3100
    pop si                                    ; 5e                          ; 0xc3101
    pop bp                                    ; 5d                          ; 0xc3102
    retn 00008h                               ; c2 08 00                    ; 0xc3103
  ; disGetNextSymbol 0xc3106 LB 0x14bf -> off=0x0 cb=00000000000001ef uValue=00000000000c3106 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc3106 LB 0x1ef
    push bp                                   ; 55                          ; 0xc3106 vgabios.c:2179
    mov bp, sp                                ; 89 e5                       ; 0xc3107
    push cx                                   ; 51                          ; 0xc3109
    push si                                   ; 56                          ; 0xc310a
    push di                                   ; 57                          ; 0xc310b
    push ax                                   ; 50                          ; 0xc310c
    push ax                                   ; 50                          ; 0xc310d
    push dx                                   ; 52                          ; 0xc310e
    mov si, strict word 00049h                ; be 49 00                    ; 0xc310f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3112
    mov es, ax                                ; 8e c0                       ; 0xc3115
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3117
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc311a vgabios.c:48
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc311d vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3120
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3123 vgabios.c:58
    mov ax, ds                                ; 8c d8                       ; 0xc3126 vgabios.c:2190
    mov es, dx                                ; 8e c2                       ; 0xc3128 vgabios.c:72
    mov word [es:bx], 05502h                  ; 26 c7 07 02 55              ; 0xc312a
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc312f
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc3133 vgabios.c:2195
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc3136
    mov si, strict word 00049h                ; be 49 00                    ; 0xc3139
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc313c
    jcxz 03147h                               ; e3 06                       ; 0xc313f
    push DS                                   ; 1e                          ; 0xc3141
    mov ds, dx                                ; 8e da                       ; 0xc3142
    rep movsb                                 ; f3 a4                       ; 0xc3144
    pop DS                                    ; 1f                          ; 0xc3146
    mov si, 00084h                            ; be 84 00                    ; 0xc3147 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc314a
    mov es, ax                                ; 8e c0                       ; 0xc314d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc314f
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc3152 vgabios.c:48
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc3154
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3157 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc315a
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc315d vgabios.c:2197
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc3160
    mov si, 00085h                            ; be 85 00                    ; 0xc3163
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3166
    jcxz 03171h                               ; e3 06                       ; 0xc3169
    push DS                                   ; 1e                          ; 0xc316b
    mov ds, dx                                ; 8e da                       ; 0xc316c
    rep movsb                                 ; f3 a4                       ; 0xc316e
    pop DS                                    ; 1f                          ; 0xc3170
    mov si, 0008ah                            ; be 8a 00                    ; 0xc3171 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3174
    mov es, ax                                ; 8e c0                       ; 0xc3177
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3179
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc317c vgabios.c:48
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc317f vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3182
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc3185 vgabios.c:2200
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc3188 vgabios.c:52
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc318c vgabios.c:2201
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc318f vgabios.c:62
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc3194 vgabios.c:2202
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc3197 vgabios.c:52
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc319b vgabios.c:2203
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc319e vgabios.c:52
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc31a2 vgabios.c:2204
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc31a5 vgabios.c:52
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc31a9 vgabios.c:2205
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc31ac vgabios.c:52
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc31b0 vgabios.c:2206
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc31b3 vgabios.c:52
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc31b7 vgabios.c:2207
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc31ba vgabios.c:52
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc31be vgabios.c:2208
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc31c1 vgabios.c:52
    mov si, 00089h                            ; be 89 00                    ; 0xc31c5 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc31c8
    mov es, ax                                ; 8e c0                       ; 0xc31cb
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc31cd
    mov dl, al                                ; 88 c2                       ; 0xc31d0 vgabios.c:2213
    and dl, 080h                              ; 80 e2 80                    ; 0xc31d2
    xor dh, dh                                ; 30 f6                       ; 0xc31d5
    sar dx, 006h                              ; c1 fa 06                    ; 0xc31d7
    and AL, strict byte 010h                  ; 24 10                       ; 0xc31da
    xor ah, ah                                ; 30 e4                       ; 0xc31dc
    sar ax, 004h                              ; c1 f8 04                    ; 0xc31de
    or ax, dx                                 ; 09 d0                       ; 0xc31e1
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc31e3 vgabios.c:2214
    je short 031f9h                           ; 74 11                       ; 0xc31e6
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc31e8
    je short 031f5h                           ; 74 08                       ; 0xc31eb
    test ax, ax                               ; 85 c0                       ; 0xc31ed
    jne short 031f9h                          ; 75 08                       ; 0xc31ef
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc31f1 vgabios.c:2215
    jmp short 031fbh                          ; eb 06                       ; 0xc31f3
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc31f5 vgabios.c:2216
    jmp short 031fbh                          ; eb 02                       ; 0xc31f7
    xor al, al                                ; 30 c0                       ; 0xc31f9 vgabios.c:2218
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc31fb vgabios.c:2220
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc31fe vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3201
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3204 vgabios.c:2223
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc3207
    jc short 0322ah                           ; 72 1f                       ; 0xc3209
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc320b
    jnbe short 0322ah                         ; 77 1b                       ; 0xc320d
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc320f vgabios.c:2224
    test ax, ax                               ; 85 c0                       ; 0xc3212
    je short 0326ch                           ; 74 56                       ; 0xc3214
    mov si, ax                                ; 89 c6                       ; 0xc3216 vgabios.c:2225
    shr si, 002h                              ; c1 ee 02                    ; 0xc3218
    mov ax, 04000h                            ; b8 00 40                    ; 0xc321b
    xor dx, dx                                ; 31 d2                       ; 0xc321e
    div si                                    ; f7 f6                       ; 0xc3220
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc3222
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3225 vgabios.c:52
    jmp short 0326ch                          ; eb 42                       ; 0xc3228 vgabios.c:2226
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc322a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc322d
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc3230
    jne short 03245h                          ; 75 11                       ; 0xc3232
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3234 vgabios.c:52
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc3237
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc323b vgabios.c:2228
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc323e vgabios.c:62
    jmp short 0326ch                          ; eb 27                       ; 0xc3243 vgabios.c:2229
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc3245
    jc short 0326ch                           ; 72 23                       ; 0xc3247
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc3249
    jnbe short 0326ch                         ; 77 1f                       ; 0xc324b
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc324d vgabios.c:2231
    je short 03261h                           ; 74 0e                       ; 0xc3251
    mov ax, 04000h                            ; b8 00 40                    ; 0xc3253 vgabios.c:2232
    xor dx, dx                                ; 31 d2                       ; 0xc3256
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc3258
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc325b vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc325e
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc3261 vgabios.c:2233
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3264 vgabios.c:62
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc3267
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc326c vgabios.c:2235
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc326f
    je short 03277h                           ; 74 04                       ; 0xc3271
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc3273
    jne short 03282h                          ; 75 0b                       ; 0xc3275
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc3277 vgabios.c:2236
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc327a vgabios.c:62
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc327d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3282 vgabios.c:2238
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc3285
    jc short 032deh                           ; 72 55                       ; 0xc3287
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc3289
    je short 032deh                           ; 74 51                       ; 0xc328b
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc328d vgabios.c:2239
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3290 vgabios.c:52
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc3293
    mov si, 00084h                            ; be 84 00                    ; 0xc3297 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc329a
    mov es, ax                                ; 8e c0                       ; 0xc329d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc329f
    xor ah, ah                                ; 30 e4                       ; 0xc32a2 vgabios.c:48
    inc ax                                    ; 40                          ; 0xc32a4
    mov si, 00085h                            ; be 85 00                    ; 0xc32a5 vgabios.c:47
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc32a8
    xor dh, dh                                ; 30 f6                       ; 0xc32ab vgabios.c:48
    imul dx                                   ; f7 ea                       ; 0xc32ad
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc32af vgabios.c:2241
    jc short 032c2h                           ; 72 0e                       ; 0xc32b2
    jbe short 032cbh                          ; 76 15                       ; 0xc32b4
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc32b6
    je short 032d3h                           ; 74 18                       ; 0xc32b9
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc32bb
    je short 032cfh                           ; 74 0f                       ; 0xc32be
    jmp short 032d3h                          ; eb 11                       ; 0xc32c0
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc32c2
    jne short 032d3h                          ; 75 0c                       ; 0xc32c5
    xor al, al                                ; 30 c0                       ; 0xc32c7 vgabios.c:2242
    jmp short 032d5h                          ; eb 0a                       ; 0xc32c9
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc32cb vgabios.c:2243
    jmp short 032d5h                          ; eb 06                       ; 0xc32cd
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc32cf vgabios.c:2244
    jmp short 032d5h                          ; eb 02                       ; 0xc32d1
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc32d3 vgabios.c:2246
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc32d5 vgabios.c:2248
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc32d8 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32db
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc32de vgabios.c:2251
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc32e1
    xor ax, ax                                ; 31 c0                       ; 0xc32e4
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc32e6
    jcxz 032edh                               ; e3 02                       ; 0xc32e9
    rep stosb                                 ; f3 aa                       ; 0xc32eb
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc32ed vgabios.c:2252
    pop di                                    ; 5f                          ; 0xc32f0
    pop si                                    ; 5e                          ; 0xc32f1
    pop cx                                    ; 59                          ; 0xc32f2
    pop bp                                    ; 5d                          ; 0xc32f3
    retn                                      ; c3                          ; 0xc32f4
  ; disGetNextSymbol 0xc32f5 LB 0x12d0 -> off=0x0 cb=0000000000000023 uValue=00000000000c32f5 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc32f5 LB 0x23
    push dx                                   ; 52                          ; 0xc32f5 vgabios.c:2255
    push bp                                   ; 55                          ; 0xc32f6
    mov bp, sp                                ; 89 e5                       ; 0xc32f7
    mov dx, ax                                ; 89 c2                       ; 0xc32f9
    xor ax, ax                                ; 31 c0                       ; 0xc32fb vgabios.c:2259
    test dl, 001h                             ; f6 c2 01                    ; 0xc32fd vgabios.c:2260
    je short 03305h                           ; 74 03                       ; 0xc3300
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc3302 vgabios.c:2261
    test dl, 002h                             ; f6 c2 02                    ; 0xc3305 vgabios.c:2263
    je short 0330dh                           ; 74 03                       ; 0xc3308
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc330a vgabios.c:2264
    test dl, 004h                             ; f6 c2 04                    ; 0xc330d vgabios.c:2266
    je short 03315h                           ; 74 03                       ; 0xc3310
    add ax, 00304h                            ; 05 04 03                    ; 0xc3312 vgabios.c:2267
    pop bp                                    ; 5d                          ; 0xc3315 vgabios.c:2270
    pop dx                                    ; 5a                          ; 0xc3316
    retn                                      ; c3                          ; 0xc3317
  ; disGetNextSymbol 0xc3318 LB 0x12ad -> off=0x0 cb=0000000000000018 uValue=00000000000c3318 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc3318 LB 0x18
    push bp                                   ; 55                          ; 0xc3318 vgabios.c:2272
    mov bp, sp                                ; 89 e5                       ; 0xc3319
    push bx                                   ; 53                          ; 0xc331b
    mov bx, dx                                ; 89 d3                       ; 0xc331c
    call 032f5h                               ; e8 d4 ff                    ; 0xc331e vgabios.c:2275
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc3321
    shr ax, 006h                              ; c1 e8 06                    ; 0xc3324
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc3327
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc332a vgabios.c:2276
    pop bx                                    ; 5b                          ; 0xc332d
    pop bp                                    ; 5d                          ; 0xc332e
    retn                                      ; c3                          ; 0xc332f
  ; disGetNextSymbol 0xc3330 LB 0x1295 -> off=0x0 cb=00000000000002d8 uValue=00000000000c3330 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc3330 LB 0x2d8
    push bp                                   ; 55                          ; 0xc3330 vgabios.c:2278
    mov bp, sp                                ; 89 e5                       ; 0xc3331
    push cx                                   ; 51                          ; 0xc3333
    push si                                   ; 56                          ; 0xc3334
    push di                                   ; 57                          ; 0xc3335
    push ax                                   ; 50                          ; 0xc3336
    push ax                                   ; 50                          ; 0xc3337
    push ax                                   ; 50                          ; 0xc3338
    mov cx, dx                                ; 89 d1                       ; 0xc3339
    mov si, strict word 00063h                ; be 63 00                    ; 0xc333b vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc333e
    mov es, ax                                ; 8e c0                       ; 0xc3341
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc3343
    mov si, di                                ; 89 fe                       ; 0xc3346 vgabios.c:58
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc3348 vgabios.c:2283
    je short 033b4h                           ; 74 66                       ; 0xc334c
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc334e vgabios.c:2284
    in AL, DX                                 ; ec                          ; 0xc3351
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3352
    mov es, cx                                ; 8e c1                       ; 0xc3354 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3356
    inc bx                                    ; 43                          ; 0xc3359 vgabios.c:2284
    mov dx, di                                ; 89 fa                       ; 0xc335a
    in AL, DX                                 ; ec                          ; 0xc335c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc335d
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc335f vgabios.c:52
    inc bx                                    ; 43                          ; 0xc3362 vgabios.c:2285
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3363
    in AL, DX                                 ; ec                          ; 0xc3366
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3367
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3369 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc336c vgabios.c:2286
    mov dx, 003dah                            ; ba da 03                    ; 0xc336d
    in AL, DX                                 ; ec                          ; 0xc3370
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3371
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3373 vgabios.c:2288
    in AL, DX                                 ; ec                          ; 0xc3376
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3377
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3379
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc337c vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc337f
    inc bx                                    ; 43                          ; 0xc3382 vgabios.c:2289
    mov dx, 003cah                            ; ba ca 03                    ; 0xc3383
    in AL, DX                                 ; ec                          ; 0xc3386
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3387
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3389 vgabios.c:52
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc338c vgabios.c:2292
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc338f
    add bx, ax                                ; 01 c3                       ; 0xc3392 vgabios.c:2290
    jmp short 0339ch                          ; eb 06                       ; 0xc3394
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3396
    jnbe short 033b7h                         ; 77 1b                       ; 0xc339a
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc339c vgabios.c:2293
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc339f
    out DX, AL                                ; ee                          ; 0xc33a2
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc33a3 vgabios.c:2294
    in AL, DX                                 ; ec                          ; 0xc33a6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33a7
    mov es, cx                                ; 8e c1                       ; 0xc33a9 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33ab
    inc bx                                    ; 43                          ; 0xc33ae vgabios.c:2294
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc33af vgabios.c:2295
    jmp short 03396h                          ; eb e2                       ; 0xc33b2
    jmp near 03464h                           ; e9 ad 00                    ; 0xc33b4
    xor al, al                                ; 30 c0                       ; 0xc33b7 vgabios.c:2296
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc33b9
    out DX, AL                                ; ee                          ; 0xc33bc
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc33bd vgabios.c:2297
    in AL, DX                                 ; ec                          ; 0xc33c0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33c1
    mov es, cx                                ; 8e c1                       ; 0xc33c3 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33c5
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc33c8 vgabios.c:2299
    inc bx                                    ; 43                          ; 0xc33cd vgabios.c:2297
    jmp short 033d6h                          ; eb 06                       ; 0xc33ce
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc33d0
    jnbe short 033edh                         ; 77 17                       ; 0xc33d4
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc33d6 vgabios.c:2300
    mov dx, si                                ; 89 f2                       ; 0xc33d9
    out DX, AL                                ; ee                          ; 0xc33db
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc33dc vgabios.c:2301
    in AL, DX                                 ; ec                          ; 0xc33df
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33e0
    mov es, cx                                ; 8e c1                       ; 0xc33e2 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33e4
    inc bx                                    ; 43                          ; 0xc33e7 vgabios.c:2301
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc33e8 vgabios.c:2302
    jmp short 033d0h                          ; eb e3                       ; 0xc33eb
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc33ed vgabios.c:2304
    jmp short 033fah                          ; eb 06                       ; 0xc33f2
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc33f4
    jnbe short 0341eh                         ; 77 24                       ; 0xc33f8
    mov dx, 003dah                            ; ba da 03                    ; 0xc33fa vgabios.c:2305
    in AL, DX                                 ; ec                          ; 0xc33fd
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33fe
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3400 vgabios.c:2306
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc3403
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc3406
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc3409
    out DX, AL                                ; ee                          ; 0xc340c
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc340d vgabios.c:2307
    in AL, DX                                 ; ec                          ; 0xc3410
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3411
    mov es, cx                                ; 8e c1                       ; 0xc3413 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3415
    inc bx                                    ; 43                          ; 0xc3418 vgabios.c:2307
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3419 vgabios.c:2308
    jmp short 033f4h                          ; eb d6                       ; 0xc341c
    mov dx, 003dah                            ; ba da 03                    ; 0xc341e vgabios.c:2309
    in AL, DX                                 ; ec                          ; 0xc3421
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3422
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3424 vgabios.c:2311
    jmp short 03431h                          ; eb 06                       ; 0xc3429
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc342b
    jnbe short 03449h                         ; 77 18                       ; 0xc342f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3431 vgabios.c:2312
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3434
    out DX, AL                                ; ee                          ; 0xc3437
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc3438 vgabios.c:2313
    in AL, DX                                 ; ec                          ; 0xc343b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc343c
    mov es, cx                                ; 8e c1                       ; 0xc343e vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3440
    inc bx                                    ; 43                          ; 0xc3443 vgabios.c:2313
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3444 vgabios.c:2314
    jmp short 0342bh                          ; eb e2                       ; 0xc3447
    mov es, cx                                ; 8e c1                       ; 0xc3449 vgabios.c:62
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc344b
    inc bx                                    ; 43                          ; 0xc344e vgabios.c:2316
    inc bx                                    ; 43                          ; 0xc344f
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3450 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc3454 vgabios.c:2319
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3455 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc3459 vgabios.c:2320
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc345a vgabios.c:52
    inc bx                                    ; 43                          ; 0xc345e vgabios.c:2321
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc345f vgabios.c:52
    inc bx                                    ; 43                          ; 0xc3463 vgabios.c:2322
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc3464 vgabios.c:2324
    jne short 0346dh                          ; 75 03                       ; 0xc3468
    jmp near 035ach                           ; e9 3f 01                    ; 0xc346a
    mov si, strict word 00049h                ; be 49 00                    ; 0xc346d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3470
    mov es, ax                                ; 8e c0                       ; 0xc3473
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3475
    mov es, cx                                ; 8e c1                       ; 0xc3478 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc347a
    inc bx                                    ; 43                          ; 0xc347d vgabios.c:2325
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc347e vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3481
    mov es, ax                                ; 8e c0                       ; 0xc3484
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3486
    mov es, cx                                ; 8e c1                       ; 0xc3489 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc348b
    inc bx                                    ; 43                          ; 0xc348e vgabios.c:2326
    inc bx                                    ; 43                          ; 0xc348f
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc3490 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3493
    mov es, ax                                ; 8e c0                       ; 0xc3496
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3498
    mov es, cx                                ; 8e c1                       ; 0xc349b vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc349d
    inc bx                                    ; 43                          ; 0xc34a0 vgabios.c:2327
    inc bx                                    ; 43                          ; 0xc34a1
    mov si, strict word 00063h                ; be 63 00                    ; 0xc34a2 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc34a5
    mov es, ax                                ; 8e c0                       ; 0xc34a8
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc34aa
    mov es, cx                                ; 8e c1                       ; 0xc34ad vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc34af
    inc bx                                    ; 43                          ; 0xc34b2 vgabios.c:2328
    inc bx                                    ; 43                          ; 0xc34b3
    mov si, 00084h                            ; be 84 00                    ; 0xc34b4 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc34b7
    mov es, ax                                ; 8e c0                       ; 0xc34ba
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34bc
    mov es, cx                                ; 8e c1                       ; 0xc34bf vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc34c1
    inc bx                                    ; 43                          ; 0xc34c4 vgabios.c:2329
    mov si, 00085h                            ; be 85 00                    ; 0xc34c5 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc34c8
    mov es, ax                                ; 8e c0                       ; 0xc34cb
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc34cd
    mov es, cx                                ; 8e c1                       ; 0xc34d0 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc34d2
    inc bx                                    ; 43                          ; 0xc34d5 vgabios.c:2330
    inc bx                                    ; 43                          ; 0xc34d6
    mov si, 00087h                            ; be 87 00                    ; 0xc34d7 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc34da
    mov es, ax                                ; 8e c0                       ; 0xc34dd
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34df
    mov es, cx                                ; 8e c1                       ; 0xc34e2 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc34e4
    inc bx                                    ; 43                          ; 0xc34e7 vgabios.c:2331
    mov si, 00088h                            ; be 88 00                    ; 0xc34e8 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc34eb
    mov es, ax                                ; 8e c0                       ; 0xc34ee
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc34f0
    mov es, cx                                ; 8e c1                       ; 0xc34f3 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc34f5
    inc bx                                    ; 43                          ; 0xc34f8 vgabios.c:2332
    mov si, 00089h                            ; be 89 00                    ; 0xc34f9 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc34fc
    mov es, ax                                ; 8e c0                       ; 0xc34ff
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3501
    mov es, cx                                ; 8e c1                       ; 0xc3504 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3506
    inc bx                                    ; 43                          ; 0xc3509 vgabios.c:2333
    mov si, strict word 00060h                ; be 60 00                    ; 0xc350a vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc350d
    mov es, ax                                ; 8e c0                       ; 0xc3510
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3512
    mov es, cx                                ; 8e c1                       ; 0xc3515 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3517
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc351a vgabios.c:2335
    inc bx                                    ; 43                          ; 0xc351f vgabios.c:2334
    inc bx                                    ; 43                          ; 0xc3520
    jmp short 03529h                          ; eb 06                       ; 0xc3521
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3523
    jnc short 03545h                          ; 73 1c                       ; 0xc3527
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc3529 vgabios.c:2336
    add si, si                                ; 01 f6                       ; 0xc352c
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc352e
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3531 vgabios.c:57
    mov es, ax                                ; 8e c0                       ; 0xc3534
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3536
    mov es, cx                                ; 8e c1                       ; 0xc3539 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc353b
    inc bx                                    ; 43                          ; 0xc353e vgabios.c:2337
    inc bx                                    ; 43                          ; 0xc353f
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3540 vgabios.c:2338
    jmp short 03523h                          ; eb de                       ; 0xc3543
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc3545 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3548
    mov es, ax                                ; 8e c0                       ; 0xc354b
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc354d
    mov es, cx                                ; 8e c1                       ; 0xc3550 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3552
    inc bx                                    ; 43                          ; 0xc3555 vgabios.c:2339
    inc bx                                    ; 43                          ; 0xc3556
    mov si, strict word 00062h                ; be 62 00                    ; 0xc3557 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc355a
    mov es, ax                                ; 8e c0                       ; 0xc355d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc355f
    mov es, cx                                ; 8e c1                       ; 0xc3562 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3564
    inc bx                                    ; 43                          ; 0xc3567 vgabios.c:2340
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc3568 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc356b
    mov es, ax                                ; 8e c0                       ; 0xc356d
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc356f
    mov es, cx                                ; 8e c1                       ; 0xc3572 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3574
    inc bx                                    ; 43                          ; 0xc3577 vgabios.c:2342
    inc bx                                    ; 43                          ; 0xc3578
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc3579 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc357c
    mov es, ax                                ; 8e c0                       ; 0xc357e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3580
    mov es, cx                                ; 8e c1                       ; 0xc3583 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3585
    inc bx                                    ; 43                          ; 0xc3588 vgabios.c:2343
    inc bx                                    ; 43                          ; 0xc3589
    mov si, 0010ch                            ; be 0c 01                    ; 0xc358a vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc358d
    mov es, ax                                ; 8e c0                       ; 0xc358f
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3591
    mov es, cx                                ; 8e c1                       ; 0xc3594 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3596
    inc bx                                    ; 43                          ; 0xc3599 vgabios.c:2344
    inc bx                                    ; 43                          ; 0xc359a
    mov si, 0010eh                            ; be 0e 01                    ; 0xc359b vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc359e
    mov es, ax                                ; 8e c0                       ; 0xc35a0
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc35a2
    mov es, cx                                ; 8e c1                       ; 0xc35a5 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc35a7
    inc bx                                    ; 43                          ; 0xc35aa vgabios.c:2345
    inc bx                                    ; 43                          ; 0xc35ab
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc35ac vgabios.c:2347
    je short 035feh                           ; 74 4c                       ; 0xc35b0
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc35b2 vgabios.c:2349
    in AL, DX                                 ; ec                          ; 0xc35b5
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc35b6
    mov es, cx                                ; 8e c1                       ; 0xc35b8 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc35ba
    inc bx                                    ; 43                          ; 0xc35bd vgabios.c:2349
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc35be
    in AL, DX                                 ; ec                          ; 0xc35c1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc35c2
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc35c4 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc35c7 vgabios.c:2350
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc35c8
    in AL, DX                                 ; ec                          ; 0xc35cb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc35cc
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc35ce vgabios.c:52
    inc bx                                    ; 43                          ; 0xc35d1 vgabios.c:2351
    xor al, al                                ; 30 c0                       ; 0xc35d2
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc35d4
    out DX, AL                                ; ee                          ; 0xc35d7
    xor ah, ah                                ; 30 e4                       ; 0xc35d8 vgabios.c:2354
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc35da
    jmp short 035e6h                          ; eb 07                       ; 0xc35dd
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc35df
    jnc short 035f7h                          ; 73 11                       ; 0xc35e4
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc35e6 vgabios.c:2355
    in AL, DX                                 ; ec                          ; 0xc35e9
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc35ea
    mov es, cx                                ; 8e c1                       ; 0xc35ec vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc35ee
    inc bx                                    ; 43                          ; 0xc35f1 vgabios.c:2355
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc35f2 vgabios.c:2356
    jmp short 035dfh                          ; eb e8                       ; 0xc35f5
    mov es, cx                                ; 8e c1                       ; 0xc35f7 vgabios.c:52
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc35f9
    inc bx                                    ; 43                          ; 0xc35fd vgabios.c:2357
    mov ax, bx                                ; 89 d8                       ; 0xc35fe vgabios.c:2360
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3600
    pop di                                    ; 5f                          ; 0xc3603
    pop si                                    ; 5e                          ; 0xc3604
    pop cx                                    ; 59                          ; 0xc3605
    pop bp                                    ; 5d                          ; 0xc3606
    retn                                      ; c3                          ; 0xc3607
  ; disGetNextSymbol 0xc3608 LB 0xfbd -> off=0x0 cb=00000000000002ba uValue=00000000000c3608 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc3608 LB 0x2ba
    push bp                                   ; 55                          ; 0xc3608 vgabios.c:2362
    mov bp, sp                                ; 89 e5                       ; 0xc3609
    push cx                                   ; 51                          ; 0xc360b
    push si                                   ; 56                          ; 0xc360c
    push di                                   ; 57                          ; 0xc360d
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc360e
    push ax                                   ; 50                          ; 0xc3611
    mov cx, dx                                ; 89 d1                       ; 0xc3612
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc3614 vgabios.c:2366
    je short 0368eh                           ; 74 74                       ; 0xc3618
    mov dx, 003dah                            ; ba da 03                    ; 0xc361a vgabios.c:2368
    in AL, DX                                 ; ec                          ; 0xc361d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc361e
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc3620 vgabios.c:2370
    mov es, cx                                ; 8e c1                       ; 0xc3623 vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3625
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc3628 vgabios.c:58
    mov si, bx                                ; 89 de                       ; 0xc362b vgabios.c:2371
    mov word [bp-008h], strict word 00001h    ; c7 46 f8 01 00              ; 0xc362d vgabios.c:2374
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc3632 vgabios.c:2372
    jmp short 0363dh                          ; eb 06                       ; 0xc3635
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3637
    jnbe short 03653h                         ; 77 16                       ; 0xc363b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc363d vgabios.c:2375
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3640
    out DX, AL                                ; ee                          ; 0xc3643
    mov es, cx                                ; 8e c1                       ; 0xc3644 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3646
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3649 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc364c
    inc bx                                    ; 43                          ; 0xc364d vgabios.c:2376
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc364e vgabios.c:2377
    jmp short 03637h                          ; eb e4                       ; 0xc3651
    xor al, al                                ; 30 c0                       ; 0xc3653 vgabios.c:2378
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3655
    out DX, AL                                ; ee                          ; 0xc3658
    mov es, cx                                ; 8e c1                       ; 0xc3659 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc365b
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc365e vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3661
    inc bx                                    ; 43                          ; 0xc3662 vgabios.c:2379
    mov dx, 003cch                            ; ba cc 03                    ; 0xc3663
    in AL, DX                                 ; ec                          ; 0xc3666
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3667
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc3669
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc366b
    cmp word [bp-00ch], 003d4h                ; 81 7e f4 d4 03              ; 0xc366e vgabios.c:2383
    jne short 03679h                          ; 75 04                       ; 0xc3673
    or byte [bp-00eh], 001h                   ; 80 4e f2 01                 ; 0xc3675 vgabios.c:2384
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3679 vgabios.c:2385
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc367c
    out DX, AL                                ; ee                          ; 0xc367f
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc3680 vgabios.c:2388
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3683
    out DX, ax                                ; ef                          ; 0xc3686
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3687 vgabios.c:2390
    jmp short 03697h                          ; eb 09                       ; 0xc368c
    jmp near 03751h                           ; e9 c0 00                    ; 0xc368e
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc3691
    jnbe short 036b1h                         ; 77 1a                       ; 0xc3695
    cmp word [bp-008h], strict byte 00011h    ; 83 7e f8 11                 ; 0xc3697 vgabios.c:2391
    je short 036abh                           ; 74 0e                       ; 0xc369b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc369d vgabios.c:2392
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc36a0
    out DX, AL                                ; ee                          ; 0xc36a3
    mov es, cx                                ; 8e c1                       ; 0xc36a4 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc36a6
    inc dx                                    ; 42                          ; 0xc36a9 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc36aa
    inc bx                                    ; 43                          ; 0xc36ab vgabios.c:2395
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc36ac vgabios.c:2396
    jmp short 03691h                          ; eb e0                       ; 0xc36af
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc36b1 vgabios.c:2398
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc36b3
    out DX, AL                                ; ee                          ; 0xc36b6
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc36b7 vgabios.c:2399
    mov es, cx                                ; 8e c1                       ; 0xc36bb vgabios.c:47
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc36bd
    inc dx                                    ; 42                          ; 0xc36c0 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc36c1
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc36c2 vgabios.c:2402
    mov dl, byte [es:di]                      ; 26 8a 15                    ; 0xc36c5 vgabios.c:47
    xor dh, dh                                ; 30 f6                       ; 0xc36c8 vgabios.c:48
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc36ca
    mov dx, 003dah                            ; ba da 03                    ; 0xc36cd vgabios.c:2403
    in AL, DX                                 ; ec                          ; 0xc36d0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc36d1
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc36d3 vgabios.c:2404
    jmp short 036e0h                          ; eb 06                       ; 0xc36d8
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc36da
    jnbe short 036f9h                         ; 77 19                       ; 0xc36de
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc36e0 vgabios.c:2405
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc36e3
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc36e6
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc36e9
    out DX, AL                                ; ee                          ; 0xc36ec
    mov es, cx                                ; 8e c1                       ; 0xc36ed vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc36ef
    out DX, AL                                ; ee                          ; 0xc36f2 vgabios.c:48
    inc bx                                    ; 43                          ; 0xc36f3 vgabios.c:2406
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc36f4 vgabios.c:2407
    jmp short 036dah                          ; eb e1                       ; 0xc36f7
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc36f9 vgabios.c:2408
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc36fc
    out DX, AL                                ; ee                          ; 0xc36ff
    mov dx, 003dah                            ; ba da 03                    ; 0xc3700 vgabios.c:2409
    in AL, DX                                 ; ec                          ; 0xc3703
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3704
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3706 vgabios.c:2411
    jmp short 03713h                          ; eb 06                       ; 0xc370b
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc370d
    jnbe short 03729h                         ; 77 16                       ; 0xc3711
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3713 vgabios.c:2412
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3716
    out DX, AL                                ; ee                          ; 0xc3719
    mov es, cx                                ; 8e c1                       ; 0xc371a vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc371c
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc371f vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3722
    inc bx                                    ; 43                          ; 0xc3723 vgabios.c:2413
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3724 vgabios.c:2414
    jmp short 0370dh                          ; eb e4                       ; 0xc3727
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc3729 vgabios.c:2415
    mov es, cx                                ; 8e c1                       ; 0xc372c vgabios.c:47
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc372e
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3731 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3734
    inc si                                    ; 46                          ; 0xc3735 vgabios.c:2418
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3736 vgabios.c:47
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc3739 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc373c
    inc si                                    ; 46                          ; 0xc373d vgabios.c:2419
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc373e vgabios.c:47
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3741 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3744
    inc si                                    ; 46                          ; 0xc3745 vgabios.c:2420
    inc si                                    ; 46                          ; 0xc3746
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3747 vgabios.c:47
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc374a vgabios.c:48
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc374d
    out DX, AL                                ; ee                          ; 0xc3750
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc3751 vgabios.c:2424
    jne short 0375ah                          ; 75 03                       ; 0xc3755
    jmp near 03875h                           ; e9 1b 01                    ; 0xc3757
    mov es, cx                                ; 8e c1                       ; 0xc375a vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc375c
    mov si, strict word 00049h                ; be 49 00                    ; 0xc375f vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3762
    mov es, dx                                ; 8e c2                       ; 0xc3765
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3767
    inc bx                                    ; 43                          ; 0xc376a vgabios.c:2425
    mov es, cx                                ; 8e c1                       ; 0xc376b vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc376d
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc3770 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3773
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3775
    inc bx                                    ; 43                          ; 0xc3778 vgabios.c:2426
    inc bx                                    ; 43                          ; 0xc3779
    mov es, cx                                ; 8e c1                       ; 0xc377a vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc377c
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc377f vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3782
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3784
    inc bx                                    ; 43                          ; 0xc3787 vgabios.c:2427
    inc bx                                    ; 43                          ; 0xc3788
    mov es, cx                                ; 8e c1                       ; 0xc3789 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc378b
    mov si, strict word 00063h                ; be 63 00                    ; 0xc378e vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3791
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3793
    inc bx                                    ; 43                          ; 0xc3796 vgabios.c:2428
    inc bx                                    ; 43                          ; 0xc3797
    mov es, cx                                ; 8e c1                       ; 0xc3798 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc379a
    mov si, 00084h                            ; be 84 00                    ; 0xc379d vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc37a0
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc37a2
    inc bx                                    ; 43                          ; 0xc37a5 vgabios.c:2429
    mov es, cx                                ; 8e c1                       ; 0xc37a6 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc37a8
    mov si, 00085h                            ; be 85 00                    ; 0xc37ab vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc37ae
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc37b0
    inc bx                                    ; 43                          ; 0xc37b3 vgabios.c:2430
    inc bx                                    ; 43                          ; 0xc37b4
    mov es, cx                                ; 8e c1                       ; 0xc37b5 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc37b7
    mov si, 00087h                            ; be 87 00                    ; 0xc37ba vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc37bd
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc37bf
    inc bx                                    ; 43                          ; 0xc37c2 vgabios.c:2431
    mov es, cx                                ; 8e c1                       ; 0xc37c3 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc37c5
    mov si, 00088h                            ; be 88 00                    ; 0xc37c8 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc37cb
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc37cd
    inc bx                                    ; 43                          ; 0xc37d0 vgabios.c:2432
    mov es, cx                                ; 8e c1                       ; 0xc37d1 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc37d3
    mov si, 00089h                            ; be 89 00                    ; 0xc37d6 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc37d9
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc37db
    inc bx                                    ; 43                          ; 0xc37de vgabios.c:2433
    mov es, cx                                ; 8e c1                       ; 0xc37df vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc37e1
    mov si, strict word 00060h                ; be 60 00                    ; 0xc37e4 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc37e7
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc37e9
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc37ec vgabios.c:2435
    inc bx                                    ; 43                          ; 0xc37f1 vgabios.c:2434
    inc bx                                    ; 43                          ; 0xc37f2
    jmp short 037fbh                          ; eb 06                       ; 0xc37f3
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc37f5
    jnc short 03817h                          ; 73 1c                       ; 0xc37f9
    mov es, cx                                ; 8e c1                       ; 0xc37fb vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc37fd
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc3800 vgabios.c:58
    add si, si                                ; 01 f6                       ; 0xc3803
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc3805
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3808 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc380b
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc380d
    inc bx                                    ; 43                          ; 0xc3810 vgabios.c:2437
    inc bx                                    ; 43                          ; 0xc3811
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3812 vgabios.c:2438
    jmp short 037f5h                          ; eb de                       ; 0xc3815
    mov es, cx                                ; 8e c1                       ; 0xc3817 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3819
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc381c vgabios.c:62
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc381f
    mov es, dx                                ; 8e c2                       ; 0xc3822
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3824
    inc bx                                    ; 43                          ; 0xc3827 vgabios.c:2439
    inc bx                                    ; 43                          ; 0xc3828
    mov es, cx                                ; 8e c1                       ; 0xc3829 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc382b
    mov si, strict word 00062h                ; be 62 00                    ; 0xc382e vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3831
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3833
    inc bx                                    ; 43                          ; 0xc3836 vgabios.c:2440
    mov es, cx                                ; 8e c1                       ; 0xc3837 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3839
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc383c vgabios.c:62
    xor dx, dx                                ; 31 d2                       ; 0xc383f
    mov es, dx                                ; 8e c2                       ; 0xc3841
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3843
    inc bx                                    ; 43                          ; 0xc3846 vgabios.c:2442
    inc bx                                    ; 43                          ; 0xc3847
    mov es, cx                                ; 8e c1                       ; 0xc3848 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc384a
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc384d vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3850
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3852
    inc bx                                    ; 43                          ; 0xc3855 vgabios.c:2443
    inc bx                                    ; 43                          ; 0xc3856
    mov es, cx                                ; 8e c1                       ; 0xc3857 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3859
    mov si, 0010ch                            ; be 0c 01                    ; 0xc385c vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc385f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3861
    inc bx                                    ; 43                          ; 0xc3864 vgabios.c:2444
    inc bx                                    ; 43                          ; 0xc3865
    mov es, cx                                ; 8e c1                       ; 0xc3866 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3868
    mov si, 0010eh                            ; be 0e 01                    ; 0xc386b vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc386e
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3870
    inc bx                                    ; 43                          ; 0xc3873 vgabios.c:2445
    inc bx                                    ; 43                          ; 0xc3874
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc3875 vgabios.c:2447
    je short 038b8h                           ; 74 3d                       ; 0xc3879
    inc bx                                    ; 43                          ; 0xc387b vgabios.c:2448
    mov es, cx                                ; 8e c1                       ; 0xc387c vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc387e
    xor ah, ah                                ; 30 e4                       ; 0xc3881 vgabios.c:48
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3883
    inc bx                                    ; 43                          ; 0xc3886 vgabios.c:2449
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3887 vgabios.c:47
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc388a vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc388d
    inc bx                                    ; 43                          ; 0xc388e vgabios.c:2450
    xor al, al                                ; 30 c0                       ; 0xc388f
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3891
    out DX, AL                                ; ee                          ; 0xc3894
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3895 vgabios.c:2453
    jmp short 038a1h                          ; eb 07                       ; 0xc3898
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc389a
    jnc short 038b0h                          ; 73 0f                       ; 0xc389f
    mov es, cx                                ; 8e c1                       ; 0xc38a1 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc38a3
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc38a6 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc38a9
    inc bx                                    ; 43                          ; 0xc38aa vgabios.c:2454
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc38ab vgabios.c:2455
    jmp short 0389ah                          ; eb ea                       ; 0xc38ae
    inc bx                                    ; 43                          ; 0xc38b0 vgabios.c:2456
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc38b1
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc38b4
    out DX, AL                                ; ee                          ; 0xc38b7
    mov ax, bx                                ; 89 d8                       ; 0xc38b8 vgabios.c:2460
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc38ba
    pop di                                    ; 5f                          ; 0xc38bd
    pop si                                    ; 5e                          ; 0xc38be
    pop cx                                    ; 59                          ; 0xc38bf
    pop bp                                    ; 5d                          ; 0xc38c0
    retn                                      ; c3                          ; 0xc38c1
  ; disGetNextSymbol 0xc38c2 LB 0xd03 -> off=0x0 cb=0000000000000028 uValue=00000000000c38c2 'find_vga_entry'
find_vga_entry:                              ; 0xc38c2 LB 0x28
    push bx                                   ; 53                          ; 0xc38c2 vgabios.c:2469
    push dx                                   ; 52                          ; 0xc38c3
    push bp                                   ; 55                          ; 0xc38c4
    mov bp, sp                                ; 89 e5                       ; 0xc38c5
    mov dl, al                                ; 88 c2                       ; 0xc38c7
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc38c9 vgabios.c:2471
    xor al, al                                ; 30 c0                       ; 0xc38cb vgabios.c:2472
    jmp short 038d5h                          ; eb 06                       ; 0xc38cd
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc38cf vgabios.c:2473
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc38d1
    jnbe short 038e4h                         ; 77 0f                       ; 0xc38d3
    mov bl, al                                ; 88 c3                       ; 0xc38d5
    xor bh, bh                                ; 30 ff                       ; 0xc38d7
    sal bx, 003h                              ; c1 e3 03                    ; 0xc38d9
    cmp dl, byte [bx+047aeh]                  ; 3a 97 ae 47                 ; 0xc38dc
    jne short 038cfh                          ; 75 ed                       ; 0xc38e0
    mov ah, al                                ; 88 c4                       ; 0xc38e2
    mov al, ah                                ; 88 e0                       ; 0xc38e4 vgabios.c:2478
    pop bp                                    ; 5d                          ; 0xc38e6
    pop dx                                    ; 5a                          ; 0xc38e7
    pop bx                                    ; 5b                          ; 0xc38e8
    retn                                      ; c3                          ; 0xc38e9
  ; disGetNextSymbol 0xc38ea LB 0xcdb -> off=0x0 cb=000000000000000e uValue=00000000000c38ea 'readx_byte'
readx_byte:                                  ; 0xc38ea LB 0xe
    push bx                                   ; 53                          ; 0xc38ea vgabios.c:2490
    push bp                                   ; 55                          ; 0xc38eb
    mov bp, sp                                ; 89 e5                       ; 0xc38ec
    mov bx, dx                                ; 89 d3                       ; 0xc38ee
    mov es, ax                                ; 8e c0                       ; 0xc38f0 vgabios.c:2492
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc38f2
    pop bp                                    ; 5d                          ; 0xc38f5 vgabios.c:2493
    pop bx                                    ; 5b                          ; 0xc38f6
    retn                                      ; c3                          ; 0xc38f7
  ; disGetNextSymbol 0xc38f8 LB 0xccd -> off=0x8a cb=0000000000000489 uValue=00000000000c3982 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 004h, 03eh, 0abh, 039h, 0e8h, 039h, 0fdh, 039h, 00dh, 03ah
    db  020h, 03ah, 030h, 03ah, 03ah, 03ah, 07ch, 03ah, 0b0h, 03ah, 0c1h, 03ah, 0e7h, 03ah, 002h, 03bh
    db  021h, 03bh, 03eh, 03bh, 054h, 03bh, 060h, 03bh, 043h, 03ch, 0c7h, 03ch, 0f4h, 03ch, 009h, 03dh
    db  04bh, 03dh, 0d6h, 03dh, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 003h
    db  002h, 001h, 000h, 004h, 03eh, 07fh, 03bh, 0a0h, 03bh, 0afh, 03bh, 0beh, 03bh, 0c8h, 03bh, 07fh
    db  03bh, 0a0h, 03bh, 0afh, 03bh, 0c8h, 03bh, 0d8h, 03bh, 0e3h, 03bh, 0feh, 03bh, 00dh, 03ch, 01ch
    db  03ch, 02bh, 03ch, 00ah, 009h, 006h, 004h, 002h, 001h, 000h, 0c8h, 03dh, 071h, 03dh, 07fh, 03dh
    db  090h, 03dh, 0a0h, 03dh, 0b5h, 03dh, 0c8h, 03dh, 0c8h, 03dh
int10_func:                                  ; 0xc3982 LB 0x489
    push bp                                   ; 55                          ; 0xc3982 vgabios.c:2571
    mov bp, sp                                ; 89 e5                       ; 0xc3983
    push si                                   ; 56                          ; 0xc3985
    push di                                   ; 57                          ; 0xc3986
    push ax                                   ; 50                          ; 0xc3987
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc3988
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc398b vgabios.c:2576
    shr ax, 008h                              ; c1 e8 08                    ; 0xc398e
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc3991
    jnbe short 039fah                         ; 77 64                       ; 0xc3994
    push CS                                   ; 0e                          ; 0xc3996
    pop ES                                    ; 07                          ; 0xc3997
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc3998
    mov di, 038f8h                            ; bf f8 38                    ; 0xc399b
    repne scasb                               ; f2 ae                       ; 0xc399e
    sal cx, 1                                 ; d1 e1                       ; 0xc39a0
    mov di, cx                                ; 89 cf                       ; 0xc39a2
    mov ax, word [cs:di+0390eh]               ; 2e 8b 85 0e 39              ; 0xc39a4
    jmp ax                                    ; ff e0                       ; 0xc39a9
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc39ab vgabios.c:2579
    xor ah, ah                                ; 30 e4                       ; 0xc39ae
    call 0143fh                               ; e8 8c da                    ; 0xc39b0
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39b3 vgabios.c:2580
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc39b6
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc39b9
    je short 039d3h                           ; 74 15                       ; 0xc39bc
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc39be
    je short 039cah                           ; 74 07                       ; 0xc39c1
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc39c3
    jbe short 039d3h                          ; 76 0b                       ; 0xc39c6
    jmp short 039dch                          ; eb 12                       ; 0xc39c8
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39ca vgabios.c:2582
    xor al, al                                ; 30 c0                       ; 0xc39cd
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc39cf
    jmp short 039e3h                          ; eb 10                       ; 0xc39d1 vgabios.c:2583
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39d3 vgabios.c:2591
    xor al, al                                ; 30 c0                       ; 0xc39d6
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc39d8
    jmp short 039e3h                          ; eb 07                       ; 0xc39da
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc39dc vgabios.c:2594
    xor al, al                                ; 30 c0                       ; 0xc39df
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc39e1
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc39e3
    jmp short 039fah                          ; eb 12                       ; 0xc39e6 vgabios.c:2596
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc39e8 vgabios.c:2598
    xor ah, ah                                ; 30 e4                       ; 0xc39eb
    mov dx, ax                                ; 89 c2                       ; 0xc39ed
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc39ef
    shr ax, 008h                              ; c1 e8 08                    ; 0xc39f2
    xor ah, ah                                ; 30 e4                       ; 0xc39f5
    call 0118ch                               ; e8 92 d7                    ; 0xc39f7
    jmp near 03e04h                           ; e9 07 04                    ; 0xc39fa vgabios.c:2599
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc39fd vgabios.c:2601
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3a00
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3a03
    xor ah, ah                                ; 30 e4                       ; 0xc3a06
    call 01293h                               ; e8 88 d8                    ; 0xc3a08
    jmp short 039fah                          ; eb ed                       ; 0xc3a0b vgabios.c:2602
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc3a0d vgabios.c:2604
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc3a10
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3a13
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3a16
    xor ah, ah                                ; 30 e4                       ; 0xc3a19
    call 00a96h                               ; e8 78 d0                    ; 0xc3a1b
    jmp short 039fah                          ; eb da                       ; 0xc3a1e vgabios.c:2605
    xor ax, ax                                ; 31 c0                       ; 0xc3a20 vgabios.c:2611
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3a22
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc3a25 vgabios.c:2612
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc3a28 vgabios.c:2613
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc3a2b vgabios.c:2614
    jmp short 039fah                          ; eb ca                       ; 0xc3a2e vgabios.c:2615
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3a30 vgabios.c:2617
    xor ah, ah                                ; 30 e4                       ; 0xc3a33
    call 01322h                               ; e8 ea d8                    ; 0xc3a35
    jmp short 039fah                          ; eb c0                       ; 0xc3a38 vgabios.c:2618
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3a3a vgabios.c:2620
    push ax                                   ; 50                          ; 0xc3a3d
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3a3e
    push ax                                   ; 50                          ; 0xc3a41
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3a42
    xor ah, ah                                ; 30 e4                       ; 0xc3a45
    push ax                                   ; 50                          ; 0xc3a47
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3a48
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3a4b
    xor ah, ah                                ; 30 e4                       ; 0xc3a4e
    push ax                                   ; 50                          ; 0xc3a50
    mov cl, byte [bp+010h]                    ; 8a 4e 10                    ; 0xc3a51
    xor ch, ch                                ; 30 ed                       ; 0xc3a54
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a56
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3a59
    xor ah, ah                                ; 30 e4                       ; 0xc3a5c
    mov bx, ax                                ; 89 c3                       ; 0xc3a5e
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3a60
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3a63
    xor ah, ah                                ; 30 e4                       ; 0xc3a66
    mov dx, ax                                ; 89 c2                       ; 0xc3a68
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3a6a
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3a6d
    mov byte [bp-005h], ch                    ; 88 6e fb                    ; 0xc3a70
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3a73
    call 01c23h                               ; e8 aa e1                    ; 0xc3a76
    jmp near 03e04h                           ; e9 88 03                    ; 0xc3a79 vgabios.c:2621
    xor ax, ax                                ; 31 c0                       ; 0xc3a7c vgabios.c:2623
    push ax                                   ; 50                          ; 0xc3a7e
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3a7f
    push ax                                   ; 50                          ; 0xc3a82
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3a83
    xor ah, ah                                ; 30 e4                       ; 0xc3a86
    push ax                                   ; 50                          ; 0xc3a88
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3a89
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3a8c
    xor ah, ah                                ; 30 e4                       ; 0xc3a8f
    push ax                                   ; 50                          ; 0xc3a91
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc3a92
    mov cx, ax                                ; 89 c1                       ; 0xc3a95
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3a97
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3a9a
    xor ah, ah                                ; 30 e4                       ; 0xc3a9d
    mov bx, ax                                ; 89 c3                       ; 0xc3a9f
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3aa1
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3aa4
    xor ah, ah                                ; 30 e4                       ; 0xc3aa7
    mov dx, ax                                ; 89 c2                       ; 0xc3aa9
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3aab
    jmp short 03a76h                          ; eb c6                       ; 0xc3aae
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc3ab0 vgabios.c:2626
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3ab3
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3ab6
    xor ah, ah                                ; 30 e4                       ; 0xc3ab9
    call 00dd6h                               ; e8 18 d3                    ; 0xc3abb
    jmp near 03e04h                           ; e9 43 03                    ; 0xc3abe vgabios.c:2627
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3ac1 vgabios.c:2629
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3ac4
    xor ah, ah                                ; 30 e4                       ; 0xc3ac7
    mov bx, ax                                ; 89 c3                       ; 0xc3ac9
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3acb
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3ace
    xor ah, ah                                ; 30 e4                       ; 0xc3ad1
    mov dx, ax                                ; 89 c2                       ; 0xc3ad3
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3ad5
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3ad8
    mov byte [bp-005h], bh                    ; 88 7e fb                    ; 0xc3adb
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3ade
    call 02569h                               ; e8 85 ea                    ; 0xc3ae1
    jmp near 03e04h                           ; e9 1d 03                    ; 0xc3ae4 vgabios.c:2630
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3ae7 vgabios.c:2632
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3aea
    xor ah, ah                                ; 30 e4                       ; 0xc3aed
    mov bx, ax                                ; 89 c3                       ; 0xc3aef
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3af1
    shr dx, 008h                              ; c1 ea 08                    ; 0xc3af4
    xor dh, dh                                ; 30 f6                       ; 0xc3af7
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3af9
    call 026f0h                               ; e8 f1 eb                    ; 0xc3afc
    jmp near 03e04h                           ; e9 02 03                    ; 0xc3aff vgabios.c:2633
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc3b02 vgabios.c:2635
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3b05
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3b08
    xor ah, ah                                ; 30 e4                       ; 0xc3b0b
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3b0d
    shr dx, 008h                              ; c1 ea 08                    ; 0xc3b10
    xor dh, dh                                ; 30 f6                       ; 0xc3b13
    mov si, dx                                ; 89 d6                       ; 0xc3b15
    mov dx, ax                                ; 89 c2                       ; 0xc3b17
    mov ax, si                                ; 89 f0                       ; 0xc3b19
    call 02871h                               ; e8 53 ed                    ; 0xc3b1b
    jmp near 03e04h                           ; e9 e3 02                    ; 0xc3b1e vgabios.c:2636
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc3b21 vgabios.c:2638
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3b24
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3b27
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3b2a
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3b2d
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3b30
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3b33
    xor ah, ah                                ; 30 e4                       ; 0xc3b36
    call 00f99h                               ; e8 5e d4                    ; 0xc3b38
    jmp near 03e04h                           ; e9 c6 02                    ; 0xc3b3b vgabios.c:2639
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc3b3e vgabios.c:2647
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc3b41
    xor bh, bh                                ; 30 ff                       ; 0xc3b44
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc3b46
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3b49
    xor ah, ah                                ; 30 e4                       ; 0xc3b4c
    call 029e4h                               ; e8 93 ee                    ; 0xc3b4e
    jmp near 03e04h                           ; e9 b0 02                    ; 0xc3b51 vgabios.c:2648
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3b54 vgabios.c:2651
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3b57
    call 010ffh                               ; e8 a2 d5                    ; 0xc3b5a
    jmp near 03e04h                           ; e9 a4 02                    ; 0xc3b5d vgabios.c:2652
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3b60 vgabios.c:2654
    xor ah, ah                                ; 30 e4                       ; 0xc3b63
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3b65
    jnbe short 03bd5h                         ; 77 6b                       ; 0xc3b68
    push CS                                   ; 0e                          ; 0xc3b6a
    pop ES                                    ; 07                          ; 0xc3b6b
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc3b6c
    mov di, 0393ch                            ; bf 3c 39                    ; 0xc3b6f
    repne scasb                               ; f2 ae                       ; 0xc3b72
    sal cx, 1                                 ; d1 e1                       ; 0xc3b74
    mov di, cx                                ; 89 cf                       ; 0xc3b76
    mov ax, word [cs:di+0394bh]               ; 2e 8b 85 4b 39              ; 0xc3b78
    jmp ax                                    ; ff e0                       ; 0xc3b7d
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3b7f vgabios.c:2658
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3b82
    xor ah, ah                                ; 30 e4                       ; 0xc3b85
    push ax                                   ; 50                          ; 0xc3b87
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3b88
    push ax                                   ; 50                          ; 0xc3b8b
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc3b8c
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3b8f
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3b92
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc3b95
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3b98
    call 02d78h                               ; e8 da f1                    ; 0xc3b9b
    jmp short 03bd5h                          ; eb 35                       ; 0xc3b9e vgabios.c:2659
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc3ba0 vgabios.c:2662
    xor dh, dh                                ; 30 f6                       ; 0xc3ba3
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3ba5
    xor ah, ah                                ; 30 e4                       ; 0xc3ba8
    call 02df7h                               ; e8 4a f2                    ; 0xc3baa
    jmp short 03bd5h                          ; eb 26                       ; 0xc3bad vgabios.c:2663
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc3baf vgabios.c:2666
    xor dh, dh                                ; 30 f6                       ; 0xc3bb2
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3bb4
    xor ah, ah                                ; 30 e4                       ; 0xc3bb7
    call 02e64h                               ; e8 a8 f2                    ; 0xc3bb9
    jmp short 03bd5h                          ; eb 17                       ; 0xc3bbc vgabios.c:2667
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3bbe vgabios.c:2669
    xor ah, ah                                ; 30 e4                       ; 0xc3bc1
    call 02d55h                               ; e8 8f f1                    ; 0xc3bc3
    jmp short 03bd5h                          ; eb 0d                       ; 0xc3bc6 vgabios.c:2670
    mov dl, byte [bp+00ch]                    ; 8a 56 0c                    ; 0xc3bc8 vgabios.c:2673
    xor dh, dh                                ; 30 f6                       ; 0xc3bcb
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3bcd
    xor ah, ah                                ; 30 e4                       ; 0xc3bd0
    call 02ed3h                               ; e8 fe f2                    ; 0xc3bd2
    jmp near 03e04h                           ; e9 2c 02                    ; 0xc3bd5 vgabios.c:2674
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3bd8 vgabios.c:2676
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc3bdb
    call 02f42h                               ; e8 61 f3                    ; 0xc3bde
    jmp short 03bd5h                          ; eb f2                       ; 0xc3be1 vgabios.c:2677
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3be3 vgabios.c:2679
    xor ah, ah                                ; 30 e4                       ; 0xc3be6
    push ax                                   ; 50                          ; 0xc3be8
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3be9
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3bec
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3bef
    mov si, word [bp+016h]                    ; 8b 76 16                    ; 0xc3bf2
    mov cx, ax                                ; 89 c1                       ; 0xc3bf5
    mov ax, si                                ; 89 f0                       ; 0xc3bf7
    call 02fa5h                               ; e8 a9 f3                    ; 0xc3bf9
    jmp short 03bd5h                          ; eb d7                       ; 0xc3bfc vgabios.c:2680
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3bfe vgabios.c:2682
    xor ah, ah                                ; 30 e4                       ; 0xc3c01
    mov dx, ax                                ; 89 c2                       ; 0xc3c03
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3c05
    call 02fc2h                               ; e8 b7 f3                    ; 0xc3c08
    jmp short 03bd5h                          ; eb c8                       ; 0xc3c0b vgabios.c:2683
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3c0d vgabios.c:2685
    xor ah, ah                                ; 30 e4                       ; 0xc3c10
    mov dx, ax                                ; 89 c2                       ; 0xc3c12
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3c14
    call 02fe4h                               ; e8 ca f3                    ; 0xc3c17
    jmp short 03bd5h                          ; eb b9                       ; 0xc3c1a vgabios.c:2686
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3c1c vgabios.c:2688
    xor ah, ah                                ; 30 e4                       ; 0xc3c1f
    mov dx, ax                                ; 89 c2                       ; 0xc3c21
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3c23
    call 03006h                               ; e8 dd f3                    ; 0xc3c26
    jmp short 03bd5h                          ; eb aa                       ; 0xc3c29 vgabios.c:2689
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc3c2b vgabios.c:2691
    push ax                                   ; 50                          ; 0xc3c2e
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc3c2f
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc3c32
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc3c35
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3c38
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3c3b
    call 00f16h                               ; e8 d5 d2                    ; 0xc3c3e
    jmp short 03bd5h                          ; eb 92                       ; 0xc3c41 vgabios.c:2699
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3c43 vgabios.c:2701
    xor ah, ah                                ; 30 e4                       ; 0xc3c46
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc3c48
    jc short 03c5ch                           ; 72 0f                       ; 0xc3c4b
    jbe short 03c87h                          ; 76 38                       ; 0xc3c4d
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc3c4f
    je short 03cafh                           ; 74 5b                       ; 0xc3c52
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc3c54
    je short 03cb1h                           ; 74 58                       ; 0xc3c57
    jmp near 03e04h                           ; e9 a8 01                    ; 0xc3c59
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3c5c
    je short 03c6bh                           ; 74 0a                       ; 0xc3c5f
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc3c61
    jne short 03cach                          ; 75 46                       ; 0xc3c64
    call 03028h                               ; e8 bf f3                    ; 0xc3c66 vgabios.c:2704
    jmp short 03cach                          ; eb 41                       ; 0xc3c69 vgabios.c:2705
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3c6b vgabios.c:2707
    xor ah, ah                                ; 30 e4                       ; 0xc3c6e
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3c70
    jnbe short 03cach                         ; 77 37                       ; 0xc3c73
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3c75 vgabios.c:2708
    call 0302dh                               ; e8 b2 f3                    ; 0xc3c78
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3c7b vgabios.c:2709
    xor al, al                                ; 30 c0                       ; 0xc3c7e
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc3c80
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3c82
    jmp short 03cach                          ; eb 25                       ; 0xc3c85 vgabios.c:2711
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3c87 vgabios.c:2713
    xor ah, ah                                ; 30 e4                       ; 0xc3c8a
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3c8c
    jnc short 03ca9h                          ; 73 18                       ; 0xc3c8f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3c91 vgabios.c:45
    mov es, ax                                ; 8e c0                       ; 0xc3c94
    mov si, 00087h                            ; be 87 00                    ; 0xc3c96
    mov ah, byte [es:si]                      ; 26 8a 24                    ; 0xc3c99 vgabios.c:47
    and ah, 0feh                              ; 80 e4 fe                    ; 0xc3c9c vgabios.c:48
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3c9f
    or al, ah                                 ; 08 e0                       ; 0xc3ca2
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3ca4 vgabios.c:52
    jmp short 03c7bh                          ; eb d2                       ; 0xc3ca7
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc3ca9 vgabios.c:2719
    jmp near 03e04h                           ; e9 55 01                    ; 0xc3cac vgabios.c:2720
    jmp short 03cbfh                          ; eb 0e                       ; 0xc3caf
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3cb1 vgabios.c:2722
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3cb4
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3cb7
    call 0305fh                               ; e8 a2 f3                    ; 0xc3cba
    jmp short 03c7bh                          ; eb bc                       ; 0xc3cbd
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3cbf vgabios.c:2726
    call 03064h                               ; e8 9f f3                    ; 0xc3cc2
    jmp short 03c7bh                          ; eb b4                       ; 0xc3cc5
    push word [bp+008h]                       ; ff 76 08                    ; 0xc3cc7 vgabios.c:2736
    push word [bp+016h]                       ; ff 76 16                    ; 0xc3cca
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3ccd
    xor ah, ah                                ; 30 e4                       ; 0xc3cd0
    push ax                                   ; 50                          ; 0xc3cd2
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3cd3
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3cd6
    xor ah, ah                                ; 30 e4                       ; 0xc3cd9
    push ax                                   ; 50                          ; 0xc3cdb
    mov bl, byte [bp+00ch]                    ; 8a 5e 0c                    ; 0xc3cdc
    xor bh, bh                                ; 30 ff                       ; 0xc3cdf
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3ce1
    shr dx, 008h                              ; c1 ea 08                    ; 0xc3ce4
    xor dh, dh                                ; 30 f6                       ; 0xc3ce7
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3ce9
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3cec
    call 03069h                               ; e8 77 f3                    ; 0xc3cef
    jmp short 03cach                          ; eb b8                       ; 0xc3cf2 vgabios.c:2737
    mov bx, si                                ; 89 f3                       ; 0xc3cf4 vgabios.c:2739
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3cf6
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3cf9
    call 03106h                               ; e8 07 f4                    ; 0xc3cfc
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3cff vgabios.c:2740
    xor al, al                                ; 30 c0                       ; 0xc3d02
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc3d04
    jmp near 03c82h                           ; e9 79 ff                    ; 0xc3d06
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3d09 vgabios.c:2743
    xor ah, ah                                ; 30 e4                       ; 0xc3d0c
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3d0e
    je short 03d35h                           ; 74 22                       ; 0xc3d11
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3d13
    je short 03d27h                           ; 74 0f                       ; 0xc3d16
    test ax, ax                               ; 85 c0                       ; 0xc3d18
    jne short 03d41h                          ; 75 25                       ; 0xc3d1a
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3d1c vgabios.c:2746
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3d1f
    call 03318h                               ; e8 f3 f5                    ; 0xc3d22
    jmp short 03d41h                          ; eb 1a                       ; 0xc3d25 vgabios.c:2747
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3d27 vgabios.c:2749
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3d2a
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3d2d
    call 03330h                               ; e8 fd f5                    ; 0xc3d30
    jmp short 03d41h                          ; eb 0c                       ; 0xc3d33 vgabios.c:2750
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3d35 vgabios.c:2752
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3d38
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3d3b
    call 03608h                               ; e8 c7 f8                    ; 0xc3d3e
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3d41 vgabios.c:2759
    xor al, al                                ; 30 c0                       ; 0xc3d44
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3d46
    jmp near 03c82h                           ; e9 37 ff                    ; 0xc3d48
    call 007bfh                               ; e8 71 ca                    ; 0xc3d4b vgabios.c:2764
    test ax, ax                               ; 85 c0                       ; 0xc3d4e
    je short 03dc6h                           ; 74 74                       ; 0xc3d50
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3d52 vgabios.c:2765
    xor ah, ah                                ; 30 e4                       ; 0xc3d55
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3d57
    jnbe short 03dc8h                         ; 77 6c                       ; 0xc3d5a
    push CS                                   ; 0e                          ; 0xc3d5c
    pop ES                                    ; 07                          ; 0xc3d5d
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc3d5e
    mov di, 0396bh                            ; bf 6b 39                    ; 0xc3d61
    repne scasb                               ; f2 ae                       ; 0xc3d64
    sal cx, 1                                 ; d1 e1                       ; 0xc3d66
    mov di, cx                                ; 89 cf                       ; 0xc3d68
    mov ax, word [cs:di+03972h]               ; 2e 8b 85 72 39              ; 0xc3d6a
    jmp ax                                    ; ff e0                       ; 0xc3d6f
    mov bx, si                                ; 89 f3                       ; 0xc3d71 vgabios.c:2768
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3d73
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3d76
    call 03fd5h                               ; e8 59 02                    ; 0xc3d79
    jmp near 03e04h                           ; e9 85 00                    ; 0xc3d7c vgabios.c:2769
    mov cx, si                                ; 89 f1                       ; 0xc3d7f vgabios.c:2771
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3d81
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3d84
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3d87
    call 04100h                               ; e8 73 03                    ; 0xc3d8a
    jmp near 03e04h                           ; e9 74 00                    ; 0xc3d8d vgabios.c:2772
    mov cx, si                                ; 89 f1                       ; 0xc3d90 vgabios.c:2774
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3d92
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3d95
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3d98
    call 0419fh                               ; e8 01 04                    ; 0xc3d9b
    jmp short 03e04h                          ; eb 64                       ; 0xc3d9e vgabios.c:2775
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3da0 vgabios.c:2777
    push ax                                   ; 50                          ; 0xc3da3
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc3da4
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3da7
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3daa
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3dad
    call 04368h                               ; e8 b5 05                    ; 0xc3db0
    jmp short 03e04h                          ; eb 4f                       ; 0xc3db3 vgabios.c:2778
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3db5 vgabios.c:2780
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3db8
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3dbb
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3dbe
    call 043f4h                               ; e8 30 06                    ; 0xc3dc1
    jmp short 03e04h                          ; eb 3e                       ; 0xc3dc4 vgabios.c:2781
    jmp short 03dcfh                          ; eb 07                       ; 0xc3dc6
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3dc8 vgabios.c:2803
    jmp short 03e04h                          ; eb 35                       ; 0xc3dcd vgabios.c:2806
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3dcf vgabios.c:2808
    jmp short 03e04h                          ; eb 2e                       ; 0xc3dd4 vgabios.c:2810
    call 007bfh                               ; e8 e6 c9                    ; 0xc3dd6 vgabios.c:2812
    test ax, ax                               ; 85 c0                       ; 0xc3dd9
    je short 03dffh                           ; 74 22                       ; 0xc3ddb
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3ddd vgabios.c:2813
    xor ah, ah                                ; 30 e4                       ; 0xc3de0
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3de2
    jne short 03df8h                          ; 75 11                       ; 0xc3de5
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3de7 vgabios.c:2816
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3dea
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3ded
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3df0
    call 044d3h                               ; e8 dd 06                    ; 0xc3df3
    jmp short 03e04h                          ; eb 0c                       ; 0xc3df6 vgabios.c:2817
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3df8 vgabios.c:2819
    jmp short 03e04h                          ; eb 05                       ; 0xc3dfd vgabios.c:2822
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3dff vgabios.c:2824
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e04 vgabios.c:2834
    pop di                                    ; 5f                          ; 0xc3e07
    pop si                                    ; 5e                          ; 0xc3e08
    pop bp                                    ; 5d                          ; 0xc3e09
    retn                                      ; c3                          ; 0xc3e0a
  ; disGetNextSymbol 0xc3e0b LB 0x7ba -> off=0x0 cb=000000000000001f uValue=00000000000c3e0b 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3e0b LB 0x1f
    push bp                                   ; 55                          ; 0xc3e0b vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3e0c
    push bx                                   ; 53                          ; 0xc3e0e
    push dx                                   ; 52                          ; 0xc3e0f
    mov bx, ax                                ; 89 c3                       ; 0xc3e10
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3e12 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e15
    call 00570h                               ; e8 55 c7                    ; 0xc3e18
    mov ax, bx                                ; 89 d8                       ; 0xc3e1b vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e1d
    call 00570h                               ; e8 4d c7                    ; 0xc3e20
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e23 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3e26
    pop bx                                    ; 5b                          ; 0xc3e27
    pop bp                                    ; 5d                          ; 0xc3e28
    retn                                      ; c3                          ; 0xc3e29
  ; disGetNextSymbol 0xc3e2a LB 0x79b -> off=0x0 cb=000000000000001f uValue=00000000000c3e2a 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3e2a LB 0x1f
    push bp                                   ; 55                          ; 0xc3e2a vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3e2b
    push bx                                   ; 53                          ; 0xc3e2d
    push dx                                   ; 52                          ; 0xc3e2e
    mov bx, ax                                ; 89 c3                       ; 0xc3e2f
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3e31 vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e34
    call 00570h                               ; e8 36 c7                    ; 0xc3e37
    mov ax, bx                                ; 89 d8                       ; 0xc3e3a vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e3c
    call 00570h                               ; e8 2e c7                    ; 0xc3e3f
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e42 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3e45
    pop bx                                    ; 5b                          ; 0xc3e46
    pop bp                                    ; 5d                          ; 0xc3e47
    retn                                      ; c3                          ; 0xc3e48
  ; disGetNextSymbol 0xc3e49 LB 0x77c -> off=0x0 cb=0000000000000019 uValue=00000000000c3e49 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3e49 LB 0x19
    push bp                                   ; 55                          ; 0xc3e49 vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3e4a
    push dx                                   ; 52                          ; 0xc3e4c
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3e4d vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e50
    call 00570h                               ; e8 1a c7                    ; 0xc3e53
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e56 vbe.c:121
    call 00577h                               ; e8 1b c7                    ; 0xc3e59
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3e5c vbe.c:122
    pop dx                                    ; 5a                          ; 0xc3e5f
    pop bp                                    ; 5d                          ; 0xc3e60
    retn                                      ; c3                          ; 0xc3e61
  ; disGetNextSymbol 0xc3e62 LB 0x763 -> off=0x0 cb=000000000000001f uValue=00000000000c3e62 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3e62 LB 0x1f
    push bp                                   ; 55                          ; 0xc3e62 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3e63
    push bx                                   ; 53                          ; 0xc3e65
    push dx                                   ; 52                          ; 0xc3e66
    mov bx, ax                                ; 89 c3                       ; 0xc3e67
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3e69 vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e6c
    call 00570h                               ; e8 fe c6                    ; 0xc3e6f
    mov ax, bx                                ; 89 d8                       ; 0xc3e72 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e74
    call 00570h                               ; e8 f6 c6                    ; 0xc3e77
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e7a vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3e7d
    pop bx                                    ; 5b                          ; 0xc3e7e
    pop bp                                    ; 5d                          ; 0xc3e7f
    retn                                      ; c3                          ; 0xc3e80
  ; disGetNextSymbol 0xc3e81 LB 0x744 -> off=0x0 cb=0000000000000019 uValue=00000000000c3e81 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3e81 LB 0x19
    push bp                                   ; 55                          ; 0xc3e81 vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3e82
    push dx                                   ; 52                          ; 0xc3e84
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3e85 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3e88
    call 00570h                               ; e8 e2 c6                    ; 0xc3e8b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3e8e vbe.c:136
    call 00577h                               ; e8 e3 c6                    ; 0xc3e91
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3e94 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3e97
    pop bp                                    ; 5d                          ; 0xc3e98
    retn                                      ; c3                          ; 0xc3e99
  ; disGetNextSymbol 0xc3e9a LB 0x72b -> off=0x0 cb=000000000000001f uValue=00000000000c3e9a 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3e9a LB 0x1f
    push bp                                   ; 55                          ; 0xc3e9a vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3e9b
    push bx                                   ; 53                          ; 0xc3e9d
    push dx                                   ; 52                          ; 0xc3e9e
    mov bx, ax                                ; 89 c3                       ; 0xc3e9f
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3ea1 vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ea4
    call 00570h                               ; e8 c6 c6                    ; 0xc3ea7
    mov ax, bx                                ; 89 d8                       ; 0xc3eaa vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3eac
    call 00570h                               ; e8 be c6                    ; 0xc3eaf
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3eb2 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3eb5
    pop bx                                    ; 5b                          ; 0xc3eb6
    pop bp                                    ; 5d                          ; 0xc3eb7
    retn                                      ; c3                          ; 0xc3eb8
  ; disGetNextSymbol 0xc3eb9 LB 0x70c -> off=0x0 cb=0000000000000019 uValue=00000000000c3eb9 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3eb9 LB 0x19
    push bp                                   ; 55                          ; 0xc3eb9 vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3eba
    push dx                                   ; 52                          ; 0xc3ebc
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3ebd vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ec0
    call 00570h                               ; e8 aa c6                    ; 0xc3ec3
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3ec6 vbe.c:151
    call 00577h                               ; e8 ab c6                    ; 0xc3ec9
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3ecc vbe.c:152
    pop dx                                    ; 5a                          ; 0xc3ecf
    pop bp                                    ; 5d                          ; 0xc3ed0
    retn                                      ; c3                          ; 0xc3ed1
  ; disGetNextSymbol 0xc3ed2 LB 0x6f3 -> off=0x0 cb=0000000000000019 uValue=00000000000c3ed2 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3ed2 LB 0x19
    push bp                                   ; 55                          ; 0xc3ed2 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3ed3
    push dx                                   ; 52                          ; 0xc3ed5
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc3ed6 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ed9
    call 00570h                               ; e8 91 c6                    ; 0xc3edc
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3edf vbe.c:157
    call 00577h                               ; e8 92 c6                    ; 0xc3ee2
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3ee5 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc3ee8
    pop bp                                    ; 5d                          ; 0xc3ee9
    retn                                      ; c3                          ; 0xc3eea
  ; disGetNextSymbol 0xc3eeb LB 0x6da -> off=0x0 cb=0000000000000012 uValue=00000000000c3eeb 'in_word'
in_word:                                     ; 0xc3eeb LB 0x12
    push bp                                   ; 55                          ; 0xc3eeb vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc3eec
    push bx                                   ; 53                          ; 0xc3eee
    mov bx, ax                                ; 89 c3                       ; 0xc3eef
    mov ax, dx                                ; 89 d0                       ; 0xc3ef1
    mov dx, bx                                ; 89 da                       ; 0xc3ef3 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc3ef5
    in ax, DX                                 ; ed                          ; 0xc3ef6 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3ef7 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc3efa
    pop bp                                    ; 5d                          ; 0xc3efb
    retn                                      ; c3                          ; 0xc3efc
  ; disGetNextSymbol 0xc3efd LB 0x6c8 -> off=0x0 cb=0000000000000014 uValue=00000000000c3efd 'in_byte'
in_byte:                                     ; 0xc3efd LB 0x14
    push bp                                   ; 55                          ; 0xc3efd vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc3efe
    push bx                                   ; 53                          ; 0xc3f00
    mov bx, ax                                ; 89 c3                       ; 0xc3f01
    mov ax, dx                                ; 89 d0                       ; 0xc3f03
    mov dx, bx                                ; 89 da                       ; 0xc3f05 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc3f07
    in AL, DX                                 ; ec                          ; 0xc3f08 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3f09
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3f0b vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3f0e
    pop bp                                    ; 5d                          ; 0xc3f0f
    retn                                      ; c3                          ; 0xc3f10
  ; disGetNextSymbol 0xc3f11 LB 0x6b4 -> off=0x0 cb=0000000000000014 uValue=00000000000c3f11 'dispi_get_id'
dispi_get_id:                                ; 0xc3f11 LB 0x14
    push bp                                   ; 55                          ; 0xc3f11 vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3f12
    push dx                                   ; 52                          ; 0xc3f14
    xor ax, ax                                ; 31 c0                       ; 0xc3f15 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f17
    out DX, ax                                ; ef                          ; 0xc3f1a
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f1b vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3f1e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3f1f vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3f22
    pop bp                                    ; 5d                          ; 0xc3f23
    retn                                      ; c3                          ; 0xc3f24
  ; disGetNextSymbol 0xc3f25 LB 0x6a0 -> off=0x0 cb=000000000000001a uValue=00000000000c3f25 'dispi_set_id'
dispi_set_id:                                ; 0xc3f25 LB 0x1a
    push bp                                   ; 55                          ; 0xc3f25 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3f26
    push bx                                   ; 53                          ; 0xc3f28
    push dx                                   ; 52                          ; 0xc3f29
    mov bx, ax                                ; 89 c3                       ; 0xc3f2a
    xor ax, ax                                ; 31 c0                       ; 0xc3f2c vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f2e
    out DX, ax                                ; ef                          ; 0xc3f31
    mov ax, bx                                ; 89 d8                       ; 0xc3f32 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f34
    out DX, ax                                ; ef                          ; 0xc3f37
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f38 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3f3b
    pop bx                                    ; 5b                          ; 0xc3f3c
    pop bp                                    ; 5d                          ; 0xc3f3d
    retn                                      ; c3                          ; 0xc3f3e
  ; disGetNextSymbol 0xc3f3f LB 0x686 -> off=0x0 cb=000000000000002a uValue=00000000000c3f3f 'vbe_init'
vbe_init:                                    ; 0xc3f3f LB 0x2a
    push bp                                   ; 55                          ; 0xc3f3f vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc3f40
    push bx                                   ; 53                          ; 0xc3f42
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc3f43 vbe.c:190
    call 03f25h                               ; e8 dc ff                    ; 0xc3f46
    call 03f11h                               ; e8 c5 ff                    ; 0xc3f49 vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3f4c
    jne short 03f63h                          ; 75 12                       ; 0xc3f4f
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc3f51 vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3f54
    mov es, ax                                ; 8e c0                       ; 0xc3f57
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc3f59
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3f5d vbe.c:194
    call 03f25h                               ; e8 c2 ff                    ; 0xc3f60
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3f63 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc3f66
    pop bp                                    ; 5d                          ; 0xc3f67
    retn                                      ; c3                          ; 0xc3f68
  ; disGetNextSymbol 0xc3f69 LB 0x65c -> off=0x0 cb=000000000000006c uValue=00000000000c3f69 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3f69 LB 0x6c
    push bp                                   ; 55                          ; 0xc3f69 vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3f6a
    push bx                                   ; 53                          ; 0xc3f6c
    push cx                                   ; 51                          ; 0xc3f6d
    push si                                   ; 56                          ; 0xc3f6e
    push di                                   ; 57                          ; 0xc3f6f
    mov di, ax                                ; 89 c7                       ; 0xc3f70
    mov si, dx                                ; 89 d6                       ; 0xc3f72
    xor dx, dx                                ; 31 d2                       ; 0xc3f74 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3f76
    call 03eebh                               ; e8 6f ff                    ; 0xc3f79
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3f7c vbe.c:209
    jne short 03fcah                          ; 75 49                       ; 0xc3f7f
    test si, si                               ; 85 f6                       ; 0xc3f81 vbe.c:213
    je short 03f98h                           ; 74 13                       ; 0xc3f83
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3f85 vbe.c:220
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f88
    call 00570h                               ; e8 e2 c5                    ; 0xc3f8b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f8e vbe.c:221
    call 00577h                               ; e8 e3 c5                    ; 0xc3f91
    test ax, ax                               ; 85 c0                       ; 0xc3f94 vbe.c:222
    je short 03fcch                           ; 74 34                       ; 0xc3f96
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3f98 vbe.c:226
    mov dx, bx                                ; 89 da                       ; 0xc3f9b vbe.c:232
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3f9d
    call 03eebh                               ; e8 48 ff                    ; 0xc3fa0
    mov cx, ax                                ; 89 c1                       ; 0xc3fa3
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3fa5 vbe.c:233
    je short 03fcah                           ; 74 20                       ; 0xc3fa8
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3faa vbe.c:235
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3fad
    call 03eebh                               ; e8 38 ff                    ; 0xc3fb0
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3fb3
    cmp cx, di                                ; 39 f9                       ; 0xc3fb6 vbe.c:237
    jne short 03fc6h                          ; 75 0c                       ; 0xc3fb8
    test si, si                               ; 85 f6                       ; 0xc3fba vbe.c:239
    jne short 03fc2h                          ; 75 04                       ; 0xc3fbc
    mov ax, bx                                ; 89 d8                       ; 0xc3fbe vbe.c:240
    jmp short 03fcch                          ; eb 0a                       ; 0xc3fc0
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3fc2 vbe.c:241
    jne short 03fbeh                          ; 75 f8                       ; 0xc3fc4
    mov bx, dx                                ; 89 d3                       ; 0xc3fc6 vbe.c:244
    jmp short 03f9dh                          ; eb d3                       ; 0xc3fc8 vbe.c:249
    xor ax, ax                                ; 31 c0                       ; 0xc3fca vbe.c:252
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3fcc vbe.c:253
    pop di                                    ; 5f                          ; 0xc3fcf
    pop si                                    ; 5e                          ; 0xc3fd0
    pop cx                                    ; 59                          ; 0xc3fd1
    pop bx                                    ; 5b                          ; 0xc3fd2
    pop bp                                    ; 5d                          ; 0xc3fd3
    retn                                      ; c3                          ; 0xc3fd4
  ; disGetNextSymbol 0xc3fd5 LB 0x5f0 -> off=0x0 cb=000000000000012b uValue=00000000000c3fd5 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc3fd5 LB 0x12b
    push bp                                   ; 55                          ; 0xc3fd5 vbe.c:284
    mov bp, sp                                ; 89 e5                       ; 0xc3fd6
    push cx                                   ; 51                          ; 0xc3fd8
    push si                                   ; 56                          ; 0xc3fd9
    push di                                   ; 57                          ; 0xc3fda
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3fdb
    mov si, ax                                ; 89 c6                       ; 0xc3fde
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3fe0
    mov di, bx                                ; 89 df                       ; 0xc3fe3
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc3fe5 vbe.c:289
    call 005b7h                               ; e8 ca c5                    ; 0xc3fea vbe.c:292
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc3fed
    mov bx, di                                ; 89 fb                       ; 0xc3ff0 vbe.c:295
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3ff2
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3ff5
    xor dx, dx                                ; 31 d2                       ; 0xc3ff8 vbe.c:298
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ffa
    call 03eebh                               ; e8 eb fe                    ; 0xc3ffd
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc4000 vbe.c:299
    je short 0400fh                           ; 74 0a                       ; 0xc4003
    push SS                                   ; 16                          ; 0xc4005 vbe.c:301
    pop ES                                    ; 07                          ; 0xc4006
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc4007
    jmp near 040f8h                           ; e9 e9 00                    ; 0xc400c vbe.c:305
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc400f vbe.c:307
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc4012 vbe.c:314
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc4017 vbe.c:322
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc401a
    jne short 04029h                          ; 75 07                       ; 0xc4020
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc4022
    je short 04038h                           ; 74 0f                       ; 0xc4027
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc4029
    jne short 0403dh                          ; 75 0c                       ; 0xc402f
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc4031
    jne short 0403dh                          ; 75 05                       ; 0xc4036
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc4038 vbe.c:324
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc403d vbe.c:332
    mov word [es:bx], 04556h                  ; 26 c7 07 56 45              ; 0xc4040
    mov word [es:bx+002h], 04153h             ; 26 c7 47 02 53 41           ; 0xc4045 vbe.c:334
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc404b vbe.c:338
    mov word [es:bx+006h], 07e02h             ; 26 c7 47 06 02 7e           ; 0xc4051 vbe.c:341
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc4057
    mov word [es:bx+00ah], strict word 00001h ; 26 c7 47 0a 01 00           ; 0xc405b vbe.c:344
    mov word [es:bx+00ch], strict word 00000h ; 26 c7 47 0c 00 00           ; 0xc4061 vbe.c:346
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4067 vbe.c:350
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc406a
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc406e vbe.c:351
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc4071
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc4075 vbe.c:354
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4078
    call 03eebh                               ; e8 6d fe                    ; 0xc407b
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc407e
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc4081
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc4085 vbe.c:356
    je short 040afh                           ; 74 24                       ; 0xc4089
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc408b vbe.c:359
    mov word [es:bx+016h], 07e17h             ; 26 c7 47 16 17 7e           ; 0xc4091 vbe.c:360
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc4097
    mov word [es:bx+01ah], 07e34h             ; 26 c7 47 1a 34 7e           ; 0xc409b vbe.c:361
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc40a1
    mov word [es:bx+01eh], 07e55h             ; 26 c7 47 1e 55 7e           ; 0xc40a5 vbe.c:362
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc40ab
    mov dx, cx                                ; 89 ca                       ; 0xc40af vbe.c:369
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc40b1
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc40b4
    call 03efdh                               ; e8 43 fe                    ; 0xc40b7
    xor ah, ah                                ; 30 e4                       ; 0xc40ba vbe.c:370
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc40bc
    jnbe short 040d8h                         ; 77 17                       ; 0xc40bf
    mov dx, cx                                ; 89 ca                       ; 0xc40c1 vbe.c:372
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc40c3
    call 03eebh                               ; e8 22 fe                    ; 0xc40c6
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc40c9 vbe.c:376
    add bx, di                                ; 01 fb                       ; 0xc40cc
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc40ce vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc40d1
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc40d4 vbe.c:378
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc40d8 vbe.c:380
    mov dx, cx                                ; 89 ca                       ; 0xc40db vbe.c:381
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc40dd
    call 03eebh                               ; e8 08 fe                    ; 0xc40e0
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc40e3 vbe.c:382
    jne short 040afh                          ; 75 c7                       ; 0xc40e6
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc40e8 vbe.c:385
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc40eb vbe.c:62
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc40ee
    push SS                                   ; 16                          ; 0xc40f1 vbe.c:386
    pop ES                                    ; 07                          ; 0xc40f2
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc40f3
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc40f8 vbe.c:387
    pop di                                    ; 5f                          ; 0xc40fb
    pop si                                    ; 5e                          ; 0xc40fc
    pop cx                                    ; 59                          ; 0xc40fd
    pop bp                                    ; 5d                          ; 0xc40fe
    retn                                      ; c3                          ; 0xc40ff
  ; disGetNextSymbol 0xc4100 LB 0x4c5 -> off=0x0 cb=000000000000009f uValue=00000000000c4100 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc4100 LB 0x9f
    push bp                                   ; 55                          ; 0xc4100 vbe.c:399
    mov bp, sp                                ; 89 e5                       ; 0xc4101
    push si                                   ; 56                          ; 0xc4103
    push di                                   ; 57                          ; 0xc4104
    push ax                                   ; 50                          ; 0xc4105
    push ax                                   ; 50                          ; 0xc4106
    mov ax, dx                                ; 89 d0                       ; 0xc4107
    mov si, bx                                ; 89 de                       ; 0xc4109
    mov bx, cx                                ; 89 cb                       ; 0xc410b
    test dh, 040h                             ; f6 c6 40                    ; 0xc410d vbe.c:410
    je short 04117h                           ; 74 05                       ; 0xc4110
    mov dx, strict word 00001h                ; ba 01 00                    ; 0xc4112
    jmp short 04119h                          ; eb 02                       ; 0xc4115
    xor dx, dx                                ; 31 d2                       ; 0xc4117
    and ah, 001h                              ; 80 e4 01                    ; 0xc4119 vbe.c:411
    call 03f69h                               ; e8 4a fe                    ; 0xc411c vbe.c:413
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc411f
    test ax, ax                               ; 85 c0                       ; 0xc4122 vbe.c:415
    je short 0418dh                           ; 74 67                       ; 0xc4124
    mov cx, 00100h                            ; b9 00 01                    ; 0xc4126 vbe.c:420
    xor ax, ax                                ; 31 c0                       ; 0xc4129
    mov di, bx                                ; 89 df                       ; 0xc412b
    mov es, si                                ; 8e c6                       ; 0xc412d
    jcxz 04133h                               ; e3 02                       ; 0xc412f
    rep stosb                                 ; f3 aa                       ; 0xc4131
    xor cx, cx                                ; 31 c9                       ; 0xc4133 vbe.c:421
    jmp short 0413ch                          ; eb 05                       ; 0xc4135
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc4137
    jnc short 04155h                          ; 73 19                       ; 0xc413a
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc413c vbe.c:424
    inc dx                                    ; 42                          ; 0xc413f
    inc dx                                    ; 42                          ; 0xc4140
    add dx, cx                                ; 01 ca                       ; 0xc4141
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4143
    call 03efdh                               ; e8 b4 fd                    ; 0xc4146
    mov di, bx                                ; 89 df                       ; 0xc4149 vbe.c:425
    add di, cx                                ; 01 cf                       ; 0xc414b
    mov es, si                                ; 8e c6                       ; 0xc414d vbe.c:52
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc414f
    inc cx                                    ; 41                          ; 0xc4152 vbe.c:426
    jmp short 04137h                          ; eb e2                       ; 0xc4153
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc4155 vbe.c:427
    mov es, si                                ; 8e c6                       ; 0xc4158 vbe.c:47
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc415a
    test AL, strict byte 001h                 ; a8 01                       ; 0xc415d vbe.c:428
    je short 04171h                           ; 74 10                       ; 0xc415f
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc4161 vbe.c:429
    mov word [es:di], 00629h                  ; 26 c7 05 29 06              ; 0xc4164 vbe.c:62
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc4169 vbe.c:431
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc416c vbe.c:62
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc4171 vbe.c:434
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4174
    call 00570h                               ; e8 f6 c3                    ; 0xc4177
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc417a vbe.c:435
    call 00577h                               ; e8 f7 c3                    ; 0xc417d
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc4180
    mov es, si                                ; 8e c6                       ; 0xc4183 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4185
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc4188 vbe.c:437
    jmp short 04190h                          ; eb 03                       ; 0xc418b vbe.c:438
    mov ax, 00100h                            ; b8 00 01                    ; 0xc418d vbe.c:442
    push SS                                   ; 16                          ; 0xc4190 vbe.c:445
    pop ES                                    ; 07                          ; 0xc4191
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc4192
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4195
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc4198 vbe.c:446
    pop di                                    ; 5f                          ; 0xc419b
    pop si                                    ; 5e                          ; 0xc419c
    pop bp                                    ; 5d                          ; 0xc419d
    retn                                      ; c3                          ; 0xc419e
  ; disGetNextSymbol 0xc419f LB 0x426 -> off=0x0 cb=00000000000000e7 uValue=00000000000c419f 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc419f LB 0xe7
    push bp                                   ; 55                          ; 0xc419f vbe.c:458
    mov bp, sp                                ; 89 e5                       ; 0xc41a0
    push si                                   ; 56                          ; 0xc41a2
    push di                                   ; 57                          ; 0xc41a3
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc41a4
    mov si, ax                                ; 89 c6                       ; 0xc41a7
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc41a9
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc41ac vbe.c:466
    je short 041b7h                           ; 74 05                       ; 0xc41b0
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc41b2
    jmp short 041b9h                          ; eb 02                       ; 0xc41b5
    xor ax, ax                                ; 31 c0                       ; 0xc41b7
    mov dx, ax                                ; 89 c2                       ; 0xc41b9
    test ax, ax                               ; 85 c0                       ; 0xc41bb vbe.c:467
    je short 041c2h                           ; 74 03                       ; 0xc41bd
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc41bf
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc41c2
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc41c5 vbe.c:468
    je short 041d0h                           ; 74 05                       ; 0xc41c9
    mov ax, 00080h                            ; b8 80 00                    ; 0xc41cb
    jmp short 041d2h                          ; eb 02                       ; 0xc41ce
    xor ax, ax                                ; 31 c0                       ; 0xc41d0
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc41d2
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc41d5 vbe.c:470
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc41d9 vbe.c:473
    jnc short 041f3h                          ; 73 13                       ; 0xc41de
    xor ax, ax                                ; 31 c0                       ; 0xc41e0 vbe.c:477
    call 005ddh                               ; e8 f8 c3                    ; 0xc41e2
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc41e5 vbe.c:481
    xor ah, ah                                ; 30 e4                       ; 0xc41e8
    call 0143fh                               ; e8 52 d2                    ; 0xc41ea
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc41ed vbe.c:482
    jmp near 0427ah                           ; e9 87 00                    ; 0xc41f0 vbe.c:483
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc41f3 vbe.c:486
    call 03f69h                               ; e8 70 fd                    ; 0xc41f6
    mov bx, ax                                ; 89 c3                       ; 0xc41f9
    test ax, ax                               ; 85 c0                       ; 0xc41fb vbe.c:488
    je short 04277h                           ; 74 78                       ; 0xc41fd
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc41ff vbe.c:493
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4202
    call 03eebh                               ; e8 e3 fc                    ; 0xc4205
    mov cx, ax                                ; 89 c1                       ; 0xc4208
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc420a vbe.c:494
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc420d
    call 03eebh                               ; e8 d8 fc                    ; 0xc4210
    mov di, ax                                ; 89 c7                       ; 0xc4213
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc4215 vbe.c:495
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4218
    call 03efdh                               ; e8 df fc                    ; 0xc421b
    mov bl, al                                ; 88 c3                       ; 0xc421e
    mov dl, al                                ; 88 c2                       ; 0xc4220
    xor ax, ax                                ; 31 c0                       ; 0xc4222 vbe.c:503
    call 005ddh                               ; e8 b6 c3                    ; 0xc4224
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc4227 vbe.c:505
    jne short 04232h                          ; 75 06                       ; 0xc422a
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc422c vbe.c:507
    call 0143fh                               ; e8 0d d2                    ; 0xc422f
    mov al, dl                                ; 88 d0                       ; 0xc4232 vbe.c:510
    xor ah, ah                                ; 30 e4                       ; 0xc4234
    call 03e62h                               ; e8 29 fc                    ; 0xc4236
    mov ax, cx                                ; 89 c8                       ; 0xc4239 vbe.c:511
    call 03e0bh                               ; e8 cd fb                    ; 0xc423b
    mov ax, di                                ; 89 f8                       ; 0xc423e vbe.c:512
    call 03e2ah                               ; e8 e7 fb                    ; 0xc4240
    xor ax, ax                                ; 31 c0                       ; 0xc4243 vbe.c:513
    call 00603h                               ; e8 bb c3                    ; 0xc4245
    mov dl, byte [bp-006h]                    ; 8a 56 fa                    ; 0xc4248 vbe.c:514
    or dl, 001h                               ; 80 ca 01                    ; 0xc424b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc424e
    xor ah, ah                                ; 30 e4                       ; 0xc4251
    or al, dl                                 ; 08 d0                       ; 0xc4253
    call 005ddh                               ; e8 85 c3                    ; 0xc4255
    call 006d2h                               ; e8 77 c4                    ; 0xc4258 vbe.c:515
    mov bx, 000bah                            ; bb ba 00                    ; 0xc425b vbe.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc425e
    mov es, ax                                ; 8e c0                       ; 0xc4261
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4263
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4266
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc4269 vbe.c:518
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc426c
    mov bx, 00087h                            ; bb 87 00                    ; 0xc426e vbe.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc4271
    jmp near 041edh                           ; e9 76 ff                    ; 0xc4274
    mov ax, 00100h                            ; b8 00 01                    ; 0xc4277 vbe.c:527
    push SS                                   ; 16                          ; 0xc427a vbe.c:531
    pop ES                                    ; 07                          ; 0xc427b
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc427c
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc427f vbe.c:532
    pop di                                    ; 5f                          ; 0xc4282
    pop si                                    ; 5e                          ; 0xc4283
    pop bp                                    ; 5d                          ; 0xc4284
    retn                                      ; c3                          ; 0xc4285
  ; disGetNextSymbol 0xc4286 LB 0x33f -> off=0x0 cb=0000000000000008 uValue=00000000000c4286 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc4286 LB 0x8
    push bp                                   ; 55                          ; 0xc4286 vbe.c:534
    mov bp, sp                                ; 89 e5                       ; 0xc4287
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc4289 vbe.c:537
    pop bp                                    ; 5d                          ; 0xc428c
    retn                                      ; c3                          ; 0xc428d
  ; disGetNextSymbol 0xc428e LB 0x337 -> off=0x0 cb=000000000000004b uValue=00000000000c428e 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc428e LB 0x4b
    push bp                                   ; 55                          ; 0xc428e vbe.c:539
    mov bp, sp                                ; 89 e5                       ; 0xc428f
    push bx                                   ; 53                          ; 0xc4291
    push cx                                   ; 51                          ; 0xc4292
    push si                                   ; 56                          ; 0xc4293
    mov si, ax                                ; 89 c6                       ; 0xc4294
    mov bx, dx                                ; 89 d3                       ; 0xc4296
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc4298 vbe.c:543
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc429b
    out DX, ax                                ; ef                          ; 0xc429e
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc429f vbe.c:544
    in ax, DX                                 ; ed                          ; 0xc42a2
    mov es, si                                ; 8e c6                       ; 0xc42a3 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc42a5
    inc bx                                    ; 43                          ; 0xc42a8 vbe.c:546
    inc bx                                    ; 43                          ; 0xc42a9
    test AL, strict byte 001h                 ; a8 01                       ; 0xc42aa vbe.c:547
    je short 042d1h                           ; 74 23                       ; 0xc42ac
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc42ae vbe.c:549
    jmp short 042b8h                          ; eb 05                       ; 0xc42b1
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc42b3
    jnbe short 042d1h                         ; 77 19                       ; 0xc42b6
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc42b8 vbe.c:550
    je short 042ceh                           ; 74 11                       ; 0xc42bb
    mov ax, cx                                ; 89 c8                       ; 0xc42bd vbe.c:551
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc42bf
    out DX, ax                                ; ef                          ; 0xc42c2
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc42c3 vbe.c:552
    in ax, DX                                 ; ed                          ; 0xc42c6
    mov es, si                                ; 8e c6                       ; 0xc42c7 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc42c9
    inc bx                                    ; 43                          ; 0xc42cc vbe.c:553
    inc bx                                    ; 43                          ; 0xc42cd
    inc cx                                    ; 41                          ; 0xc42ce vbe.c:555
    jmp short 042b3h                          ; eb e2                       ; 0xc42cf
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc42d1 vbe.c:556
    pop si                                    ; 5e                          ; 0xc42d4
    pop cx                                    ; 59                          ; 0xc42d5
    pop bx                                    ; 5b                          ; 0xc42d6
    pop bp                                    ; 5d                          ; 0xc42d7
    retn                                      ; c3                          ; 0xc42d8
  ; disGetNextSymbol 0xc42d9 LB 0x2ec -> off=0x0 cb=000000000000008f uValue=00000000000c42d9 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc42d9 LB 0x8f
    push bp                                   ; 55                          ; 0xc42d9 vbe.c:559
    mov bp, sp                                ; 89 e5                       ; 0xc42da
    push bx                                   ; 53                          ; 0xc42dc
    push cx                                   ; 51                          ; 0xc42dd
    push si                                   ; 56                          ; 0xc42de
    push ax                                   ; 50                          ; 0xc42df
    mov cx, ax                                ; 89 c1                       ; 0xc42e0
    mov bx, dx                                ; 89 d3                       ; 0xc42e2
    mov es, ax                                ; 8e c0                       ; 0xc42e4 vbe.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc42e6
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc42e9
    inc bx                                    ; 43                          ; 0xc42ec vbe.c:564
    inc bx                                    ; 43                          ; 0xc42ed
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc42ee vbe.c:566
    jne short 04304h                          ; 75 10                       ; 0xc42f2
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc42f4 vbe.c:567
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc42f7
    out DX, ax                                ; ef                          ; 0xc42fa
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc42fb vbe.c:568
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc42fe
    out DX, ax                                ; ef                          ; 0xc4301
    jmp short 04360h                          ; eb 5c                       ; 0xc4302 vbe.c:569
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc4304 vbe.c:570
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4307
    out DX, ax                                ; ef                          ; 0xc430a
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc430b vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc430e vbe.c:58
    out DX, ax                                ; ef                          ; 0xc4311
    inc bx                                    ; 43                          ; 0xc4312 vbe.c:572
    inc bx                                    ; 43                          ; 0xc4313
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc4314
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4317
    out DX, ax                                ; ef                          ; 0xc431a
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc431b vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc431e vbe.c:58
    out DX, ax                                ; ef                          ; 0xc4321
    inc bx                                    ; 43                          ; 0xc4322 vbe.c:575
    inc bx                                    ; 43                          ; 0xc4323
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc4324
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4327
    out DX, ax                                ; ef                          ; 0xc432a
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc432b vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc432e vbe.c:58
    out DX, ax                                ; ef                          ; 0xc4331
    inc bx                                    ; 43                          ; 0xc4332 vbe.c:578
    inc bx                                    ; 43                          ; 0xc4333
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc4334
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4337
    out DX, ax                                ; ef                          ; 0xc433a
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc433b vbe.c:580
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc433e
    out DX, ax                                ; ef                          ; 0xc4341
    mov si, strict word 00005h                ; be 05 00                    ; 0xc4342 vbe.c:582
    jmp short 0434ch                          ; eb 05                       ; 0xc4345
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc4347
    jnbe short 04360h                         ; 77 14                       ; 0xc434a
    mov ax, si                                ; 89 f0                       ; 0xc434c vbe.c:583
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc434e
    out DX, ax                                ; ef                          ; 0xc4351
    mov es, cx                                ; 8e c1                       ; 0xc4352 vbe.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc4354
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4357 vbe.c:58
    out DX, ax                                ; ef                          ; 0xc435a
    inc bx                                    ; 43                          ; 0xc435b vbe.c:585
    inc bx                                    ; 43                          ; 0xc435c
    inc si                                    ; 46                          ; 0xc435d vbe.c:586
    jmp short 04347h                          ; eb e7                       ; 0xc435e
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc4360 vbe.c:588
    pop si                                    ; 5e                          ; 0xc4363
    pop cx                                    ; 59                          ; 0xc4364
    pop bx                                    ; 5b                          ; 0xc4365
    pop bp                                    ; 5d                          ; 0xc4366
    retn                                      ; c3                          ; 0xc4367
  ; disGetNextSymbol 0xc4368 LB 0x25d -> off=0x0 cb=000000000000008c uValue=00000000000c4368 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc4368 LB 0x8c
    push bp                                   ; 55                          ; 0xc4368 vbe.c:604
    mov bp, sp                                ; 89 e5                       ; 0xc4369
    push si                                   ; 56                          ; 0xc436b
    push di                                   ; 57                          ; 0xc436c
    push ax                                   ; 50                          ; 0xc436d
    mov si, ax                                ; 89 c6                       ; 0xc436e
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc4370
    mov ax, bx                                ; 89 d8                       ; 0xc4373
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc4375
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc4378 vbe.c:609
    xor ah, ah                                ; 30 e4                       ; 0xc437b vbe.c:610
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc437d
    je short 043c7h                           ; 74 45                       ; 0xc4380
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc4382
    je short 043abh                           ; 74 24                       ; 0xc4385
    test ax, ax                               ; 85 c0                       ; 0xc4387
    jne short 043e3h                          ; 75 58                       ; 0xc4389
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc438b vbe.c:612
    call 032f5h                               ; e8 64 ef                    ; 0xc438e
    mov cx, ax                                ; 89 c1                       ; 0xc4391
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4393 vbe.c:616
    je short 0439eh                           ; 74 05                       ; 0xc4397
    call 04286h                               ; e8 ea fe                    ; 0xc4399 vbe.c:617
    add ax, cx                                ; 01 c8                       ; 0xc439c
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc439e vbe.c:618
    shr ax, 006h                              ; c1 e8 06                    ; 0xc43a1
    push SS                                   ; 16                          ; 0xc43a4
    pop ES                                    ; 07                          ; 0xc43a5
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc43a6
    jmp short 043e6h                          ; eb 3b                       ; 0xc43a9 vbe.c:619
    push SS                                   ; 16                          ; 0xc43ab vbe.c:621
    pop ES                                    ; 07                          ; 0xc43ac
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc43ad
    mov dx, cx                                ; 89 ca                       ; 0xc43b0 vbe.c:622
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc43b2
    call 03330h                               ; e8 78 ef                    ; 0xc43b5
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc43b8 vbe.c:626
    je short 043e6h                           ; 74 28                       ; 0xc43bc
    mov dx, ax                                ; 89 c2                       ; 0xc43be vbe.c:627
    mov ax, cx                                ; 89 c8                       ; 0xc43c0
    call 0428eh                               ; e8 c9 fe                    ; 0xc43c2
    jmp short 043e6h                          ; eb 1f                       ; 0xc43c5 vbe.c:628
    push SS                                   ; 16                          ; 0xc43c7 vbe.c:630
    pop ES                                    ; 07                          ; 0xc43c8
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc43c9
    mov dx, cx                                ; 89 ca                       ; 0xc43cc vbe.c:631
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc43ce
    call 03608h                               ; e8 34 f2                    ; 0xc43d1
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc43d4 vbe.c:635
    je short 043e6h                           ; 74 0c                       ; 0xc43d8
    mov dx, ax                                ; 89 c2                       ; 0xc43da vbe.c:636
    mov ax, cx                                ; 89 c8                       ; 0xc43dc
    call 042d9h                               ; e8 f8 fe                    ; 0xc43de
    jmp short 043e6h                          ; eb 03                       ; 0xc43e1 vbe.c:637
    mov di, 00100h                            ; bf 00 01                    ; 0xc43e3 vbe.c:640
    push SS                                   ; 16                          ; 0xc43e6 vbe.c:643
    pop ES                                    ; 07                          ; 0xc43e7
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc43e8
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc43eb vbe.c:644
    pop di                                    ; 5f                          ; 0xc43ee
    pop si                                    ; 5e                          ; 0xc43ef
    pop bp                                    ; 5d                          ; 0xc43f0
    retn 00002h                               ; c2 02 00                    ; 0xc43f1
  ; disGetNextSymbol 0xc43f4 LB 0x1d1 -> off=0x0 cb=00000000000000df uValue=00000000000c43f4 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc43f4 LB 0xdf
    push bp                                   ; 55                          ; 0xc43f4 vbe.c:665
    mov bp, sp                                ; 89 e5                       ; 0xc43f5
    push si                                   ; 56                          ; 0xc43f7
    push di                                   ; 57                          ; 0xc43f8
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc43f9
    push ax                                   ; 50                          ; 0xc43fc
    mov di, dx                                ; 89 d7                       ; 0xc43fd
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc43ff
    mov si, cx                                ; 89 ce                       ; 0xc4402
    call 03e81h                               ; e8 7a fa                    ; 0xc4404 vbe.c:674
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc4407 vbe.c:675
    jne short 04410h                          ; 75 05                       ; 0xc4409
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc440b
    jmp short 04414h                          ; eb 04                       ; 0xc440e
    xor ah, ah                                ; 30 e4                       ; 0xc4410
    mov bx, ax                                ; 89 c3                       ; 0xc4412
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc4414
    call 03eb9h                               ; e8 9f fa                    ; 0xc4417 vbe.c:676
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc441a
    mov word [bp-00ch], strict word 0004fh    ; c7 46 f4 4f 00              ; 0xc441d vbe.c:677
    push SS                                   ; 16                          ; 0xc4422 vbe.c:678
    pop ES                                    ; 07                          ; 0xc4423
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc4424
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4427
    mov cl, byte [es:di]                      ; 26 8a 0d                    ; 0xc442a vbe.c:679
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc442d vbe.c:683
    je short 0443eh                           ; 74 0c                       ; 0xc4430
    cmp cl, 001h                              ; 80 f9 01                    ; 0xc4432
    je short 04464h                           ; 74 2d                       ; 0xc4435
    test cl, cl                               ; 84 c9                       ; 0xc4437
    je short 0445fh                           ; 74 24                       ; 0xc4439
    jmp near 044bch                           ; e9 7e 00                    ; 0xc443b
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc443e vbe.c:685
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc4441
    jne short 0444ah                          ; 75 05                       ; 0xc4443
    sal bx, 003h                              ; c1 e3 03                    ; 0xc4445 vbe.c:686
    jmp short 0445fh                          ; eb 15                       ; 0xc4448 vbe.c:687
    xor ah, ah                                ; 30 e4                       ; 0xc444a vbe.c:688
    cwd                                       ; 99                          ; 0xc444c
    sal dx, 003h                              ; c1 e2 03                    ; 0xc444d
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4450
    sar ax, 003h                              ; c1 f8 03                    ; 0xc4452
    mov cx, ax                                ; 89 c1                       ; 0xc4455
    mov ax, bx                                ; 89 d8                       ; 0xc4457
    xor dx, dx                                ; 31 d2                       ; 0xc4459
    div cx                                    ; f7 f1                       ; 0xc445b
    mov bx, ax                                ; 89 c3                       ; 0xc445d
    mov ax, bx                                ; 89 d8                       ; 0xc445f vbe.c:691
    call 03e9ah                               ; e8 36 fa                    ; 0xc4461
    call 03eb9h                               ; e8 52 fa                    ; 0xc4464 vbe.c:694
    mov cx, ax                                ; 89 c1                       ; 0xc4467
    push SS                                   ; 16                          ; 0xc4469 vbe.c:695
    pop ES                                    ; 07                          ; 0xc446a
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc446b
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc446e
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc4471 vbe.c:696
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc4474
    jne short 0447fh                          ; 75 07                       ; 0xc4476
    mov bx, cx                                ; 89 cb                       ; 0xc4478 vbe.c:697
    shr bx, 003h                              ; c1 eb 03                    ; 0xc447a
    jmp short 04492h                          ; eb 13                       ; 0xc447d vbe.c:698
    xor ah, ah                                ; 30 e4                       ; 0xc447f vbe.c:699
    cwd                                       ; 99                          ; 0xc4481
    sal dx, 003h                              ; c1 e2 03                    ; 0xc4482
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4485
    sar ax, 003h                              ; c1 f8 03                    ; 0xc4487
    mov bx, ax                                ; 89 c3                       ; 0xc448a
    mov ax, cx                                ; 89 c8                       ; 0xc448c
    mul bx                                    ; f7 e3                       ; 0xc448e
    mov bx, ax                                ; 89 c3                       ; 0xc4490
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc4492 vbe.c:700
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc4495
    push SS                                   ; 16                          ; 0xc4498 vbe.c:701
    pop ES                                    ; 07                          ; 0xc4499
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc449a
    call 03ed2h                               ; e8 32 fa                    ; 0xc449d vbe.c:702
    push SS                                   ; 16                          ; 0xc44a0
    pop ES                                    ; 07                          ; 0xc44a1
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc44a2
    call 03e49h                               ; e8 a1 f9                    ; 0xc44a5 vbe.c:703
    push SS                                   ; 16                          ; 0xc44a8
    pop ES                                    ; 07                          ; 0xc44a9
    cmp ax, word [es:si]                      ; 26 3b 04                    ; 0xc44aa
    jbe short 044c1h                          ; 76 12                       ; 0xc44ad
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc44af vbe.c:704
    call 03e9ah                               ; e8 e5 f9                    ; 0xc44b2
    mov word [bp-00ch], 00200h                ; c7 46 f4 00 02              ; 0xc44b5 vbe.c:705
    jmp short 044c1h                          ; eb 05                       ; 0xc44ba vbe.c:707
    mov word [bp-00ch], 00100h                ; c7 46 f4 00 01              ; 0xc44bc vbe.c:710
    push SS                                   ; 16                          ; 0xc44c1 vbe.c:713
    pop ES                                    ; 07                          ; 0xc44c2
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc44c3
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc44c6
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc44c9
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc44cc vbe.c:714
    pop di                                    ; 5f                          ; 0xc44cf
    pop si                                    ; 5e                          ; 0xc44d0
    pop bp                                    ; 5d                          ; 0xc44d1
    retn                                      ; c3                          ; 0xc44d2
  ; disGetNextSymbol 0xc44d3 LB 0xf2 -> off=0x0 cb=00000000000000f2 uValue=00000000000c44d3 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc44d3 LB 0xf2
    push bp                                   ; 55                          ; 0xc44d3 vbe.c:740
    mov bp, sp                                ; 89 e5                       ; 0xc44d4
    push si                                   ; 56                          ; 0xc44d6
    push di                                   ; 57                          ; 0xc44d7
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc44d8
    mov di, ax                                ; 89 c7                       ; 0xc44db
    mov si, dx                                ; 89 d6                       ; 0xc44dd
    mov dx, cx                                ; 89 ca                       ; 0xc44df
    mov word [bp-00ah], strict word 0004fh    ; c7 46 f6 4f 00              ; 0xc44e1 vbe.c:753
    push SS                                   ; 16                          ; 0xc44e6 vbe.c:754
    pop ES                                    ; 07                          ; 0xc44e7
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc44e8
    test al, al                               ; 84 c0                       ; 0xc44eb vbe.c:755
    jne short 04511h                          ; 75 22                       ; 0xc44ed
    push SS                                   ; 16                          ; 0xc44ef vbe.c:757
    pop ES                                    ; 07                          ; 0xc44f0
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc44f1
    mov bx, dx                                ; 89 d3                       ; 0xc44f4 vbe.c:758
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc44f6
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc44f9 vbe.c:759
    shr ax, 008h                              ; c1 e8 08                    ; 0xc44fc
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc44ff
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc4502
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc4505 vbe.c:764
    je short 04519h                           ; 74 10                       ; 0xc4507
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc4509
    je short 04519h                           ; 74 0c                       ; 0xc450b
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc450d
    je short 04519h                           ; 74 08                       ; 0xc450f
    mov word [bp-00ah], 00100h                ; c7 46 f6 00 01              ; 0xc4511 vbe.c:765
    jmp near 045b6h                           ; e9 9d 00                    ; 0xc4516 vbe.c:766
    push SS                                   ; 16                          ; 0xc4519 vbe.c:770
    pop ES                                    ; 07                          ; 0xc451a
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc451b
    je short 04527h                           ; 74 05                       ; 0xc4520
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc4522
    jmp short 04529h                          ; eb 02                       ; 0xc4525
    xor ax, ax                                ; 31 c0                       ; 0xc4527
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc4529
    cmp cx, 00280h                            ; 81 f9 80 02                 ; 0xc452c vbe.c:773
    jnc short 04537h                          ; 73 05                       ; 0xc4530
    mov cx, 00280h                            ; b9 80 02                    ; 0xc4532 vbe.c:774
    jmp short 04540h                          ; eb 09                       ; 0xc4535 vbe.c:775
    cmp cx, 00a00h                            ; 81 f9 00 0a                 ; 0xc4537
    jbe short 04540h                          ; 76 03                       ; 0xc453b
    mov cx, 00a00h                            ; b9 00 0a                    ; 0xc453d vbe.c:776
    cmp bx, 001e0h                            ; 81 fb e0 01                 ; 0xc4540 vbe.c:777
    jnc short 0454bh                          ; 73 05                       ; 0xc4544
    mov bx, 001e0h                            ; bb e0 01                    ; 0xc4546 vbe.c:778
    jmp short 04554h                          ; eb 09                       ; 0xc4549 vbe.c:779
    cmp bx, 00780h                            ; 81 fb 80 07                 ; 0xc454b
    jbe short 04554h                          ; 76 03                       ; 0xc454f
    mov bx, 00780h                            ; bb 80 07                    ; 0xc4551 vbe.c:780
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc4554 vbe.c:786
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc4557
    call 03eebh                               ; e8 8e f9                    ; 0xc455a
    mov si, ax                                ; 89 c6                       ; 0xc455d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc455f vbe.c:789
    xor ah, ah                                ; 30 e4                       ; 0xc4562
    cwd                                       ; 99                          ; 0xc4564
    sal dx, 003h                              ; c1 e2 03                    ; 0xc4565
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4568
    sar ax, 003h                              ; c1 f8 03                    ; 0xc456a
    mov dx, ax                                ; 89 c2                       ; 0xc456d
    mov ax, cx                                ; 89 c8                       ; 0xc456f
    mul dx                                    ; f7 e2                       ; 0xc4571
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc4573 vbe.c:790
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc4576
    mov dx, bx                                ; 89 da                       ; 0xc4578 vbe.c:792
    mul dx                                    ; f7 e2                       ; 0xc457a
    cmp dx, si                                ; 39 f2                       ; 0xc457c vbe.c:794
    jnbe short 04586h                         ; 77 06                       ; 0xc457e
    jne short 0458dh                          ; 75 0b                       ; 0xc4580
    test ax, ax                               ; 85 c0                       ; 0xc4582
    jbe short 0458dh                          ; 76 07                       ; 0xc4584
    mov word [bp-00ah], 00200h                ; c7 46 f6 00 02              ; 0xc4586 vbe.c:796
    jmp short 045b6h                          ; eb 29                       ; 0xc458b vbe.c:797
    xor ax, ax                                ; 31 c0                       ; 0xc458d vbe.c:801
    call 005ddh                               ; e8 4b c0                    ; 0xc458f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc4592 vbe.c:802
    xor ah, ah                                ; 30 e4                       ; 0xc4595
    call 03e62h                               ; e8 c8 f8                    ; 0xc4597
    mov ax, cx                                ; 89 c8                       ; 0xc459a vbe.c:803
    call 03e0bh                               ; e8 6c f8                    ; 0xc459c
    mov ax, bx                                ; 89 d8                       ; 0xc459f vbe.c:804
    call 03e2ah                               ; e8 86 f8                    ; 0xc45a1
    xor ax, ax                                ; 31 c0                       ; 0xc45a4 vbe.c:805
    call 00603h                               ; e8 5a c0                    ; 0xc45a6
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc45a9 vbe.c:806
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc45ac
    xor ah, ah                                ; 30 e4                       ; 0xc45ae
    call 005ddh                               ; e8 2a c0                    ; 0xc45b0
    call 006d2h                               ; e8 1c c1                    ; 0xc45b3 vbe.c:807
    push SS                                   ; 16                          ; 0xc45b6 vbe.c:815
    pop ES                                    ; 07                          ; 0xc45b7
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc45b8
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc45bb
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc45be vbe.c:816
    pop di                                    ; 5f                          ; 0xc45c1
    pop si                                    ; 5e                          ; 0xc45c2
    pop bp                                    ; 5d                          ; 0xc45c3
    retn                                      ; c3                          ; 0xc45c4

  ; Padding 0x7b bytes at 0xc45c5
  times 123 db 0

section VBE32 progbits vstart=0x4640 align=1 ; size=0x115 class=CODE group=AUTO
  ; disGetNextSymbol 0xc4640 LB 0x115 -> off=0x0 cb=0000000000000114 uValue=00000000000c0000 'vesa_pm_start'
vesa_pm_start:                               ; 0xc4640 LB 0x114
    sbb byte [bx+si], al                      ; 18 00                       ; 0xc4640
    dec di                                    ; 4f                          ; 0xc4642
    add byte [bx+si], dl                      ; 00 10                       ; 0xc4643
    add word [bx+si], cx                      ; 01 08                       ; 0xc4645
    add dh, cl                                ; 00 ce                       ; 0xc4647
    add di, cx                                ; 01 cf                       ; 0xc4649
    add di, cx                                ; 01 cf                       ; 0xc464b
    add ax, dx                                ; 01 d0                       ; 0xc464d
    add word [bp-048fdh], si                  ; 01 b6 03 b7                 ; 0xc464f
    db  003h, 0ffh
    ; add di, di                                ; 03 ff                     ; 0xc4653
    db  0ffh
    db  0ffh
    jmp word [bp-07dh]                        ; ff 66 83                    ; 0xc4657
    sti                                       ; fb                          ; 0xc465a
    add byte [si+005h], dh                    ; 00 74 05                    ; 0xc465b
    mov eax, strict dword 066c30100h          ; 66 b8 00 01 c3 66           ; 0xc465e vberom.asm:825
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc4664
    push edx                                  ; 66 52                       ; 0xc4666 vberom.asm:829
    push eax                                  ; 66 50                       ; 0xc4668 vberom.asm:830
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc466a vberom.asm:831
    add ax, 06600h                            ; 05 00 66                    ; 0xc4670
    out DX, ax                                ; ef                          ; 0xc4673
    pop eax                                   ; 66 58                       ; 0xc4674 vberom.asm:834
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc4676 vberom.asm:835
    in eax, DX                                ; 66 ed                       ; 0xc467c vberom.asm:837
    pop edx                                   ; 66 5a                       ; 0xc467e vberom.asm:838
    db  066h, 03bh, 0d0h
    ; cmp edx, eax                              ; 66 3b d0                  ; 0xc4680 vberom.asm:839
    jne short 0468ah                          ; 75 05                       ; 0xc4683 vberom.asm:840
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc4685 vberom.asm:841
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc468b
    retn                                      ; c3                          ; 0xc468e vberom.asm:845
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc468f vberom.asm:847
    je short 0469eh                           ; 74 0a                       ; 0xc4692 vberom.asm:848
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc4694 vberom.asm:849
    je short 046aeh                           ; 74 15                       ; 0xc4697 vberom.asm:850
    mov eax, strict dword 052c30100h          ; 66 b8 00 01 c3 52           ; 0xc4699 vberom.asm:851
    mov edx, strict dword 0a8ec03dah          ; 66 ba da 03 ec a8           ; 0xc469f vberom.asm:855
    or byte [di-005h], dh                     ; 08 75 fb                    ; 0xc46a5
    in AL, DX                                 ; ec                          ; 0xc46a8 vberom.asm:861
    test AL, strict byte 008h                 ; a8 08                       ; 0xc46a9 vberom.asm:862
    je short 046a8h                           ; 74 fb                       ; 0xc46ab vberom.asm:863
    pop dx                                    ; 5a                          ; 0xc46ad vberom.asm:864
    push ax                                   ; 50                          ; 0xc46ae vberom.asm:868
    push cx                                   ; 51                          ; 0xc46af vberom.asm:869
    push dx                                   ; 52                          ; 0xc46b0 vberom.asm:870
    push si                                   ; 56                          ; 0xc46b1 vberom.asm:871
    push di                                   ; 57                          ; 0xc46b2 vberom.asm:872
    sal dx, 010h                              ; c1 e2 10                    ; 0xc46b3 vberom.asm:873
    and cx, strict word 0ffffh                ; 81 e1 ff ff                 ; 0xc46b6 vberom.asm:874
    add byte [bx+si], al                      ; 00 00                       ; 0xc46ba
    db  00bh, 0cah
    ; or cx, dx                                 ; 0b ca                     ; 0xc46bc vberom.asm:875
    sal cx, 002h                              ; c1 e1 02                    ; 0xc46be vberom.asm:876
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc46c1 vberom.asm:877
    push ax                                   ; 50                          ; 0xc46c3 vberom.asm:878
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc46c4 vberom.asm:879
    push ES                                   ; 06                          ; 0xc46ca
    add byte [bp-011h], ah                    ; 00 66 ef                    ; 0xc46cb
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc46ce vberom.asm:882
    db  00fh, 0b7h, 0c8h
    ; movzx cx, ax                              ; 0f b7 c8                  ; 0xc46d4 vberom.asm:884
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc46d7 vberom.asm:885
    add ax, word [bx+si]                      ; 03 00                       ; 0xc46dd
    out DX, eax                               ; 66 ef                       ; 0xc46df vberom.asm:887
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc46e1 vberom.asm:888
    db  00fh, 0b7h, 0f0h
    ; movzx si, ax                              ; 0f b7 f0                  ; 0xc46e7 vberom.asm:890
    pop ax                                    ; 58                          ; 0xc46ea vberom.asm:891
    cmp si, strict byte 00004h                ; 83 fe 04                    ; 0xc46eb vberom.asm:893
    je short 04707h                           ; 74 17                       ; 0xc46ee vberom.asm:894
    add si, strict byte 00007h                ; 83 c6 07                    ; 0xc46f0 vberom.asm:895
    shr si, 003h                              ; c1 ee 03                    ; 0xc46f3 vberom.asm:896
    imul cx, si                               ; 0f af ce                    ; 0xc46f6 vberom.asm:897
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc46f9 vberom.asm:898
    div cx                                    ; f7 f1                       ; 0xc46fb vberom.asm:899
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc46fd vberom.asm:900
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc46ff vberom.asm:901
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc4701 vberom.asm:902
    div si                                    ; f7 f6                       ; 0xc4703 vberom.asm:903
    jmp short 04713h                          ; eb 0c                       ; 0xc4705 vberom.asm:904
    shr cx, 1                                 ; d1 e9                       ; 0xc4707 vberom.asm:907
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc4709 vberom.asm:908
    div cx                                    ; f7 f1                       ; 0xc470b vberom.asm:909
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc470d vberom.asm:910
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc470f vberom.asm:911
    sal ax, 1                                 ; d1 e0                       ; 0xc4711 vberom.asm:912
    push edx                                  ; 66 52                       ; 0xc4713 vberom.asm:915
    push eax                                  ; 66 50                       ; 0xc4715 vberom.asm:916
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4717 vberom.asm:917
    or byte [bx+si], al                       ; 08 00                       ; 0xc471d
    out DX, eax                               ; 66 ef                       ; 0xc471f vberom.asm:919
    pop eax                                   ; 66 58                       ; 0xc4721 vberom.asm:920
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc4723 vberom.asm:921
    pop edx                                   ; 66 5a                       ; 0xc4729 vberom.asm:923
    db  066h, 08bh, 0c7h
    ; mov eax, edi                              ; 66 8b c7                  ; 0xc472b vberom.asm:925
    push edx                                  ; 66 52                       ; 0xc472e vberom.asm:926
    push eax                                  ; 66 50                       ; 0xc4730 vberom.asm:927
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4732 vberom.asm:928
    or word [bx+si], ax                       ; 09 00                       ; 0xc4738
    out DX, eax                               ; 66 ef                       ; 0xc473a vberom.asm:930
    pop eax                                   ; 66 58                       ; 0xc473c vberom.asm:931
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc473e vberom.asm:932
    pop edx                                   ; 66 5a                       ; 0xc4744 vberom.asm:934
    pop di                                    ; 5f                          ; 0xc4746 vberom.asm:936
    pop si                                    ; 5e                          ; 0xc4747 vberom.asm:937
    pop dx                                    ; 5a                          ; 0xc4748 vberom.asm:938
    pop cx                                    ; 59                          ; 0xc4749 vberom.asm:939
    pop ax                                    ; 58                          ; 0xc474a vberom.asm:940
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc474b vberom.asm:941
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc4751
  ; disGetNextSymbol 0xc4754 LB 0x1 -> off=0x0 cb=0000000000000001 uValue=0000000000000114 'vesa_pm_end'
vesa_pm_end:                                 ; 0xc4754 LB 0x1
    retn                                      ; c3                          ; 0xc4754 vberom.asm:946

  ; Padding 0x2b bytes at 0xc4755
  times 43 db 0

section _DATA progbits vstart=0x4780 align=1 ; size=0x374c class=DATA group=DGROUP
  ; disGetNextSymbol 0xc4780 LB 0x374c -> off=0x0 cb=000000000000002e uValue=00000000000c0000 '_msg_vga_init'
_msg_vga_init:                               ; 0xc4780 LB 0x2e
    db  'Oracle VM VirtualBox Version 7.0.2 VGA BIOS', 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc47ae LB 0x371e -> off=0x0 cb=0000000000000080 uValue=00000000000c002e 'vga_modes'
vga_modes:                                   ; 0xc47ae LB 0x80
    db  000h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 001h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  002h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 003h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  004h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h, 005h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h
    db  006h, 001h, 002h, 001h, 000h, 0b8h, 0ffh, 001h, 007h, 000h, 001h, 004h, 000h, 0b0h, 0ffh, 000h
    db  00dh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h, 00eh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h
    db  00fh, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 000h, 010h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  011h, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 002h, 012h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  013h, 001h, 005h, 008h, 000h, 0a0h, 0ffh, 003h, 06ah, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
  ; disGetNextSymbol 0xc482e LB 0x369e -> off=0x0 cb=0000000000000010 uValue=00000000000c00ae 'line_to_vpti'
line_to_vpti:                                ; 0xc482e LB 0x10
    db  017h, 017h, 018h, 018h, 004h, 005h, 006h, 007h, 00dh, 00eh, 011h, 012h, 01ah, 01bh, 01ch, 01dh
  ; disGetNextSymbol 0xc483e LB 0x368e -> off=0x0 cb=0000000000000004 uValue=00000000000c00be 'dac_regs'
dac_regs:                                    ; 0xc483e LB 0x4
    dd  0ff3f3f3fh
  ; disGetNextSymbol 0xc4842 LB 0x368a -> off=0x0 cb=0000000000000780 uValue=00000000000c00c2 'video_param_table'
video_param_table:                           ; 0xc4842 LB 0x780
    db  028h, 018h, 008h, 000h, 008h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 0c7h, 006h, 007h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  028h, 018h, 008h, 000h, 008h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 0c7h, 006h, 007h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  050h, 018h, 008h, 000h, 010h, 001h, 003h, 000h, 002h, 063h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 0c7h, 006h, 007h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  050h, 018h, 008h, 000h, 010h, 001h, 003h, 000h, 002h, 063h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 0c7h, 006h, 007h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  028h, 018h, 008h, 000h, 040h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 080h
    db  0bfh, 01fh, 000h, 0c1h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 000h, 096h
    db  0b9h, 0a2h, 0ffh, 000h, 013h, 015h, 017h, 002h, 004h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 003h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 00fh, 00fh, 0ffh
    db  028h, 018h, 008h, 000h, 040h, 009h, 003h, 000h, 002h, 063h, 02dh, 027h, 028h, 090h, 02bh, 080h
    db  0bfh, 01fh, 000h, 0c1h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 000h, 096h
    db  0b9h, 0a2h, 0ffh, 000h, 013h, 015h, 017h, 002h, 004h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 003h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 00fh, 00fh, 0ffh
    db  050h, 018h, 008h, 000h, 040h, 001h, 001h, 000h, 006h, 063h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 0c1h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 000h, 096h
    db  0b9h, 0c2h, 0ffh, 000h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h, 017h
    db  017h, 017h, 017h, 001h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 00dh, 00fh, 0ffh
    db  050h, 018h, 010h, 000h, 010h, 000h, 003h, 000h, 002h, 066h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 00fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 008h, 008h, 008h, 008h, 008h, 008h, 008h, 010h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 00eh, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00ah, 00fh, 0ffh
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
    db  028h, 018h, 008h, 000h, 020h, 009h, 00fh, 000h, 006h, 063h, 02dh, 027h, 028h, 090h, 02bh, 080h
    db  0bfh, 01fh, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 000h, 096h
    db  0b9h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  050h, 018h, 008h, 000h, 040h, 001h, 00fh, 000h, 006h, 063h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 000h, 096h
    db  0b9h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 010h, 011h, 012h, 013h, 014h
    db  015h, 016h, 017h, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  050h, 018h, 00eh, 000h, 080h, 001h, 00fh, 000h, 006h, 0a3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 028h, 00fh, 063h
    db  0bah, 0e3h, 0ffh, 000h, 008h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 008h, 000h, 000h, 000h
    db  018h, 000h, 000h, 001h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  050h, 018h, 00eh, 000h, 080h, 001h, 00fh, 000h, 006h, 0a3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 028h, 00fh, 063h
    db  0bah, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  028h, 018h, 00eh, 000h, 008h, 009h, 003h, 000h, 002h, 0a3h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 04dh, 00bh, 00ch, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 014h, 01fh, 063h
    db  0bah, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 008h, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  028h, 018h, 00eh, 000h, 008h, 009h, 003h, 000h, 002h, 0a3h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 04dh, 00bh, 00ch, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 014h, 01fh, 063h
    db  0bah, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 008h, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  050h, 018h, 00eh, 000h, 010h, 001h, 003h, 000h, 002h, 0a3h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04dh, 00bh, 00ch, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 028h, 01fh, 063h
    db  0bah, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  050h, 018h, 00eh, 000h, 010h, 001h, 003h, 000h, 002h, 0a3h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04dh, 00bh, 00ch, 000h, 000h, 000h, 000h, 083h, 085h, 05dh, 028h, 01fh, 063h
    db  0bah, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 008h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 000h, 0ffh
    db  028h, 018h, 010h, 000h, 008h, 008h, 003h, 000h, 002h, 067h, 02dh, 027h, 028h, 090h, 02bh, 0a0h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 014h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 00ch, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 00fh, 0ffh
    db  050h, 018h, 010h, 000h, 010h, 000h, 003h, 000h, 002h, 067h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 00ch, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 00fh, 0ffh
    db  050h, 018h, 010h, 000h, 010h, 000h, 003h, 000h, 002h, 066h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 00fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 008h, 008h, 008h, 008h, 008h, 008h, 008h, 010h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 00eh, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00ah, 00fh, 0ffh
    db  050h, 01dh, 010h, 000h, 0a0h, 001h, 00fh, 000h, 006h, 0e3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  00bh, 03eh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 0eah, 08ch, 0dfh, 028h, 000h, 0e7h
    db  004h, 0c3h, 0ffh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h, 03fh, 000h
    db  03fh, 000h, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  050h, 01dh, 010h, 000h, 0a0h, 001h, 00fh, 000h, 006h, 0e3h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  00bh, 03eh, 000h, 040h, 000h, 000h, 000h, 000h, 000h, 000h, 0eah, 08ch, 0dfh, 028h, 000h, 0e7h
    db  004h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
    db  028h, 018h, 008h, 000h, 020h, 001h, 00fh, 000h, 00eh, 063h, 05fh, 04fh, 050h, 082h, 054h, 080h
    db  0bfh, 01fh, 000h, 041h, 000h, 000h, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 040h, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 006h, 007h, 008h, 009h, 00ah, 00bh, 00ch
    db  00dh, 00eh, 00fh, 041h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 040h, 005h, 00fh, 0ffh
    db  064h, 024h, 010h, 000h, 000h, 001h, 00fh, 000h, 006h, 0e3h, 07fh, 063h, 063h, 083h, 06bh, 01bh
    db  072h, 0f0h, 000h, 060h, 000h, 000h, 000h, 000h, 000h, 000h, 059h, 08dh, 057h, 032h, 000h, 057h
    db  073h, 0e3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 001h, 000h, 00fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 005h, 00fh, 0ffh
  ; disGetNextSymbol 0xc4fc2 LB 0x2f0a -> off=0x0 cb=00000000000000c0 uValue=00000000000c0842 'palette0'
palette0:                                    ; 0xc4fc2 LB 0xc0
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh
    db  03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah
    db  02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 02ah, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh
    db  03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh, 03fh
  ; disGetNextSymbol 0xc5082 LB 0x2e4a -> off=0x0 cb=00000000000000c0 uValue=00000000000c0902 'palette1'
palette1:                                    ; 0xc5082 LB 0xc0
    db  000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah, 000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah
    db  000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah
    db  000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah, 000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah
    db  015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh, 015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh
    db  015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh
    db  015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh
    db  000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah, 000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah
    db  000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah
    db  000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah, 000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah
    db  015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh, 015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh
    db  015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh
    db  015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh
  ; disGetNextSymbol 0xc5142 LB 0x2d8a -> off=0x0 cb=00000000000000c0 uValue=00000000000c09c2 'palette2'
palette2:                                    ; 0xc5142 LB 0xc0
    db  000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah, 000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah
    db  000h, 02ah, 02ah, 02ah, 000h, 02ah, 02ah, 02ah, 000h, 000h, 015h, 000h, 000h, 03fh, 000h, 02ah
    db  015h, 000h, 02ah, 03fh, 02ah, 000h, 015h, 02ah, 000h, 03fh, 02ah, 02ah, 015h, 02ah, 02ah, 03fh
    db  000h, 015h, 000h, 000h, 015h, 02ah, 000h, 03fh, 000h, 000h, 03fh, 02ah, 02ah, 015h, 000h, 02ah
    db  015h, 02ah, 02ah, 03fh, 000h, 02ah, 03fh, 02ah, 000h, 015h, 015h, 000h, 015h, 03fh, 000h, 03fh
    db  015h, 000h, 03fh, 03fh, 02ah, 015h, 015h, 02ah, 015h, 03fh, 02ah, 03fh, 015h, 02ah, 03fh, 03fh
    db  015h, 000h, 000h, 015h, 000h, 02ah, 015h, 02ah, 000h, 015h, 02ah, 02ah, 03fh, 000h, 000h, 03fh
    db  000h, 02ah, 03fh, 02ah, 000h, 03fh, 02ah, 02ah, 015h, 000h, 015h, 015h, 000h, 03fh, 015h, 02ah
    db  015h, 015h, 02ah, 03fh, 03fh, 000h, 015h, 03fh, 000h, 03fh, 03fh, 02ah, 015h, 03fh, 02ah, 03fh
    db  015h, 015h, 000h, 015h, 015h, 02ah, 015h, 03fh, 000h, 015h, 03fh, 02ah, 03fh, 015h, 000h, 03fh
    db  015h, 02ah, 03fh, 03fh, 000h, 03fh, 03fh, 02ah, 015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh
    db  015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh
  ; disGetNextSymbol 0xc5202 LB 0x2cca -> off=0x0 cb=0000000000000300 uValue=00000000000c0a82 'palette3'
palette3:                                    ; 0xc5202 LB 0x300
    db  000h, 000h, 000h, 000h, 000h, 02ah, 000h, 02ah, 000h, 000h, 02ah, 02ah, 02ah, 000h, 000h, 02ah
    db  000h, 02ah, 02ah, 015h, 000h, 02ah, 02ah, 02ah, 015h, 015h, 015h, 015h, 015h, 03fh, 015h, 03fh
    db  015h, 015h, 03fh, 03fh, 03fh, 015h, 015h, 03fh, 015h, 03fh, 03fh, 03fh, 015h, 03fh, 03fh, 03fh
    db  000h, 000h, 000h, 005h, 005h, 005h, 008h, 008h, 008h, 00bh, 00bh, 00bh, 00eh, 00eh, 00eh, 011h
    db  011h, 011h, 014h, 014h, 014h, 018h, 018h, 018h, 01ch, 01ch, 01ch, 020h, 020h, 020h, 024h, 024h
    db  024h, 028h, 028h, 028h, 02dh, 02dh, 02dh, 032h, 032h, 032h, 038h, 038h, 038h, 03fh, 03fh, 03fh
    db  000h, 000h, 03fh, 010h, 000h, 03fh, 01fh, 000h, 03fh, 02fh, 000h, 03fh, 03fh, 000h, 03fh, 03fh
    db  000h, 02fh, 03fh, 000h, 01fh, 03fh, 000h, 010h, 03fh, 000h, 000h, 03fh, 010h, 000h, 03fh, 01fh
    db  000h, 03fh, 02fh, 000h, 03fh, 03fh, 000h, 02fh, 03fh, 000h, 01fh, 03fh, 000h, 010h, 03fh, 000h
    db  000h, 03fh, 000h, 000h, 03fh, 010h, 000h, 03fh, 01fh, 000h, 03fh, 02fh, 000h, 03fh, 03fh, 000h
    db  02fh, 03fh, 000h, 01fh, 03fh, 000h, 010h, 03fh, 01fh, 01fh, 03fh, 027h, 01fh, 03fh, 02fh, 01fh
    db  03fh, 037h, 01fh, 03fh, 03fh, 01fh, 03fh, 03fh, 01fh, 037h, 03fh, 01fh, 02fh, 03fh, 01fh, 027h
    db  03fh, 01fh, 01fh, 03fh, 027h, 01fh, 03fh, 02fh, 01fh, 03fh, 037h, 01fh, 03fh, 03fh, 01fh, 037h
    db  03fh, 01fh, 02fh, 03fh, 01fh, 027h, 03fh, 01fh, 01fh, 03fh, 01fh, 01fh, 03fh, 027h, 01fh, 03fh
    db  02fh, 01fh, 03fh, 037h, 01fh, 03fh, 03fh, 01fh, 037h, 03fh, 01fh, 02fh, 03fh, 01fh, 027h, 03fh
    db  02dh, 02dh, 03fh, 031h, 02dh, 03fh, 036h, 02dh, 03fh, 03ah, 02dh, 03fh, 03fh, 02dh, 03fh, 03fh
    db  02dh, 03ah, 03fh, 02dh, 036h, 03fh, 02dh, 031h, 03fh, 02dh, 02dh, 03fh, 031h, 02dh, 03fh, 036h
    db  02dh, 03fh, 03ah, 02dh, 03fh, 03fh, 02dh, 03ah, 03fh, 02dh, 036h, 03fh, 02dh, 031h, 03fh, 02dh
    db  02dh, 03fh, 02dh, 02dh, 03fh, 031h, 02dh, 03fh, 036h, 02dh, 03fh, 03ah, 02dh, 03fh, 03fh, 02dh
    db  03ah, 03fh, 02dh, 036h, 03fh, 02dh, 031h, 03fh, 000h, 000h, 01ch, 007h, 000h, 01ch, 00eh, 000h
    db  01ch, 015h, 000h, 01ch, 01ch, 000h, 01ch, 01ch, 000h, 015h, 01ch, 000h, 00eh, 01ch, 000h, 007h
    db  01ch, 000h, 000h, 01ch, 007h, 000h, 01ch, 00eh, 000h, 01ch, 015h, 000h, 01ch, 01ch, 000h, 015h
    db  01ch, 000h, 00eh, 01ch, 000h, 007h, 01ch, 000h, 000h, 01ch, 000h, 000h, 01ch, 007h, 000h, 01ch
    db  00eh, 000h, 01ch, 015h, 000h, 01ch, 01ch, 000h, 015h, 01ch, 000h, 00eh, 01ch, 000h, 007h, 01ch
    db  00eh, 00eh, 01ch, 011h, 00eh, 01ch, 015h, 00eh, 01ch, 018h, 00eh, 01ch, 01ch, 00eh, 01ch, 01ch
    db  00eh, 018h, 01ch, 00eh, 015h, 01ch, 00eh, 011h, 01ch, 00eh, 00eh, 01ch, 011h, 00eh, 01ch, 015h
    db  00eh, 01ch, 018h, 00eh, 01ch, 01ch, 00eh, 018h, 01ch, 00eh, 015h, 01ch, 00eh, 011h, 01ch, 00eh
    db  00eh, 01ch, 00eh, 00eh, 01ch, 011h, 00eh, 01ch, 015h, 00eh, 01ch, 018h, 00eh, 01ch, 01ch, 00eh
    db  018h, 01ch, 00eh, 015h, 01ch, 00eh, 011h, 01ch, 014h, 014h, 01ch, 016h, 014h, 01ch, 018h, 014h
    db  01ch, 01ah, 014h, 01ch, 01ch, 014h, 01ch, 01ch, 014h, 01ah, 01ch, 014h, 018h, 01ch, 014h, 016h
    db  01ch, 014h, 014h, 01ch, 016h, 014h, 01ch, 018h, 014h, 01ch, 01ah, 014h, 01ch, 01ch, 014h, 01ah
    db  01ch, 014h, 018h, 01ch, 014h, 016h, 01ch, 014h, 014h, 01ch, 014h, 014h, 01ch, 016h, 014h, 01ch
    db  018h, 014h, 01ch, 01ah, 014h, 01ch, 01ch, 014h, 01ah, 01ch, 014h, 018h, 01ch, 014h, 016h, 01ch
    db  000h, 000h, 010h, 004h, 000h, 010h, 008h, 000h, 010h, 00ch, 000h, 010h, 010h, 000h, 010h, 010h
    db  000h, 00ch, 010h, 000h, 008h, 010h, 000h, 004h, 010h, 000h, 000h, 010h, 004h, 000h, 010h, 008h
    db  000h, 010h, 00ch, 000h, 010h, 010h, 000h, 00ch, 010h, 000h, 008h, 010h, 000h, 004h, 010h, 000h
    db  000h, 010h, 000h, 000h, 010h, 004h, 000h, 010h, 008h, 000h, 010h, 00ch, 000h, 010h, 010h, 000h
    db  00ch, 010h, 000h, 008h, 010h, 000h, 004h, 010h, 008h, 008h, 010h, 00ah, 008h, 010h, 00ch, 008h
    db  010h, 00eh, 008h, 010h, 010h, 008h, 010h, 010h, 008h, 00eh, 010h, 008h, 00ch, 010h, 008h, 00ah
    db  010h, 008h, 008h, 010h, 00ah, 008h, 010h, 00ch, 008h, 010h, 00eh, 008h, 010h, 010h, 008h, 00eh
    db  010h, 008h, 00ch, 010h, 008h, 00ah, 010h, 008h, 008h, 010h, 008h, 008h, 010h, 00ah, 008h, 010h
    db  00ch, 008h, 010h, 00eh, 008h, 010h, 010h, 008h, 00eh, 010h, 008h, 00ch, 010h, 008h, 00ah, 010h
    db  00bh, 00bh, 010h, 00ch, 00bh, 010h, 00dh, 00bh, 010h, 00fh, 00bh, 010h, 010h, 00bh, 010h, 010h
    db  00bh, 00fh, 010h, 00bh, 00dh, 010h, 00bh, 00ch, 010h, 00bh, 00bh, 010h, 00ch, 00bh, 010h, 00dh
    db  00bh, 010h, 00fh, 00bh, 010h, 010h, 00bh, 00fh, 010h, 00bh, 00dh, 010h, 00bh, 00ch, 010h, 00bh
    db  00bh, 010h, 00bh, 00bh, 010h, 00ch, 00bh, 010h, 00dh, 00bh, 010h, 00fh, 00bh, 010h, 010h, 00bh
    db  00fh, 010h, 00bh, 00dh, 010h, 00bh, 00ch, 010h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5502 LB 0x29ca -> off=0x0 cb=0000000000000010 uValue=00000000000c0d82 'static_functionality'
static_functionality:                        ; 0xc5502 LB 0x10
    db  0ffh, 0e0h, 00fh, 000h, 000h, 000h, 000h, 007h, 002h, 008h, 0e7h, 00ch, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5512 LB 0x29ba -> off=0x0 cb=0000000000000024 uValue=00000000000c0d92 '_dcc_table'
_dcc_table:                                  ; 0xc5512 LB 0x24
    db  010h, 001h, 007h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5536 LB 0x2996 -> off=0x0 cb=000000000000001a uValue=00000000000c0db6 '_secondary_save_area'
_secondary_save_area:                        ; 0xc5536 LB 0x1a
    db  01ah, 000h, 012h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5550 LB 0x297c -> off=0x0 cb=000000000000001c uValue=00000000000c0dd0 '_video_save_pointer_table'
_video_save_pointer_table:                   ; 0xc5550 LB 0x1c
    db  042h, 048h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc556c LB 0x2960 -> off=0x0 cb=0000000000000800 uValue=00000000000c0dec 'vgafont8'
vgafont8:                                    ; 0xc556c LB 0x800
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
    db  078h, 0cch, 0c0h, 0cch, 078h, 018h, 00ch, 078h, 000h, 0cch, 000h, 0cch, 0cch, 0cch, 07eh, 000h
    db  01ch, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h, 07eh, 0c3h, 03ch, 006h, 03eh, 066h, 03fh, 000h
    db  0cch, 000h, 078h, 00ch, 07ch, 0cch, 07eh, 000h, 0e0h, 000h, 078h, 00ch, 07ch, 0cch, 07eh, 000h
    db  030h, 030h, 078h, 00ch, 07ch, 0cch, 07eh, 000h, 000h, 000h, 078h, 0c0h, 0c0h, 078h, 00ch, 038h
    db  07eh, 0c3h, 03ch, 066h, 07eh, 060h, 03ch, 000h, 0cch, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h
    db  0e0h, 000h, 078h, 0cch, 0fch, 0c0h, 078h, 000h, 0cch, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  07ch, 0c6h, 038h, 018h, 018h, 018h, 03ch, 000h, 0e0h, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  0c6h, 038h, 06ch, 0c6h, 0feh, 0c6h, 0c6h, 000h, 030h, 030h, 000h, 078h, 0cch, 0fch, 0cch, 000h
    db  01ch, 000h, 0fch, 060h, 078h, 060h, 0fch, 000h, 000h, 000h, 07fh, 00ch, 07fh, 0cch, 07fh, 000h
    db  03eh, 06ch, 0cch, 0feh, 0cch, 0cch, 0ceh, 000h, 078h, 0cch, 000h, 078h, 0cch, 0cch, 078h, 000h
    db  000h, 0cch, 000h, 078h, 0cch, 0cch, 078h, 000h, 000h, 0e0h, 000h, 078h, 0cch, 0cch, 078h, 000h
    db  078h, 0cch, 000h, 0cch, 0cch, 0cch, 07eh, 000h, 000h, 0e0h, 000h, 0cch, 0cch, 0cch, 07eh, 000h
    db  000h, 0cch, 000h, 0cch, 0cch, 07ch, 00ch, 0f8h, 0c3h, 018h, 03ch, 066h, 066h, 03ch, 018h, 000h
    db  0cch, 000h, 0cch, 0cch, 0cch, 0cch, 078h, 000h, 018h, 018h, 07eh, 0c0h, 0c0h, 07eh, 018h, 018h
    db  038h, 06ch, 064h, 0f0h, 060h, 0e6h, 0fch, 000h, 0cch, 0cch, 078h, 0fch, 030h, 0fch, 030h, 030h
    db  0f8h, 0cch, 0cch, 0fah, 0c6h, 0cfh, 0c6h, 0c7h, 00eh, 01bh, 018h, 03ch, 018h, 018h, 0d8h, 070h
    db  01ch, 000h, 078h, 00ch, 07ch, 0cch, 07eh, 000h, 038h, 000h, 070h, 030h, 030h, 030h, 078h, 000h
    db  000h, 01ch, 000h, 078h, 0cch, 0cch, 078h, 000h, 000h, 01ch, 000h, 0cch, 0cch, 0cch, 07eh, 000h
    db  000h, 0f8h, 000h, 0f8h, 0cch, 0cch, 0cch, 000h, 0fch, 000h, 0cch, 0ech, 0fch, 0dch, 0cch, 000h
    db  03ch, 06ch, 06ch, 03eh, 000h, 07eh, 000h, 000h, 038h, 06ch, 06ch, 038h, 000h, 07ch, 000h, 000h
    db  030h, 000h, 030h, 060h, 0c0h, 0cch, 078h, 000h, 000h, 000h, 000h, 0fch, 0c0h, 0c0h, 000h, 000h
    db  000h, 000h, 000h, 0fch, 00ch, 00ch, 000h, 000h, 0c3h, 0c6h, 0cch, 0deh, 033h, 066h, 0cch, 00fh
    db  0c3h, 0c6h, 0cch, 0dbh, 037h, 06fh, 0cfh, 003h, 018h, 018h, 000h, 018h, 018h, 018h, 018h, 000h
    db  000h, 033h, 066h, 0cch, 066h, 033h, 000h, 000h, 000h, 0cch, 066h, 033h, 066h, 0cch, 000h, 000h
    db  022h, 088h, 022h, 088h, 022h, 088h, 022h, 088h, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah
    db  0dbh, 077h, 0dbh, 0eeh, 0dbh, 077h, 0dbh, 0eeh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 018h, 018h, 018h
    db  036h, 036h, 036h, 036h, 0f6h, 036h, 036h, 036h, 000h, 000h, 000h, 000h, 0feh, 036h, 036h, 036h
    db  000h, 000h, 0f8h, 018h, 0f8h, 018h, 018h, 018h, 036h, 036h, 0f6h, 006h, 0f6h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 000h, 000h, 0feh, 006h, 0f6h, 036h, 036h, 036h
    db  036h, 036h, 0f6h, 006h, 0feh, 000h, 000h, 000h, 036h, 036h, 036h, 036h, 0feh, 000h, 000h, 000h
    db  018h, 018h, 0f8h, 018h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 01fh, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 0ffh, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 0ffh, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 0ffh, 018h, 018h, 018h
    db  018h, 018h, 01fh, 018h, 01fh, 018h, 018h, 018h, 036h, 036h, 036h, 036h, 037h, 036h, 036h, 036h
    db  036h, 036h, 037h, 030h, 03fh, 000h, 000h, 000h, 000h, 000h, 03fh, 030h, 037h, 036h, 036h, 036h
    db  036h, 036h, 0f7h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0f7h, 036h, 036h, 036h
    db  036h, 036h, 037h, 030h, 037h, 036h, 036h, 036h, 000h, 000h, 0ffh, 000h, 0ffh, 000h, 000h, 000h
    db  036h, 036h, 0f7h, 000h, 0f7h, 036h, 036h, 036h, 018h, 018h, 0ffh, 000h, 0ffh, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 0ffh, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0ffh, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 03fh, 000h, 000h, 000h
    db  018h, 018h, 01fh, 018h, 01fh, 000h, 000h, 000h, 000h, 000h, 01fh, 018h, 01fh, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 03fh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 036h, 036h, 036h
    db  018h, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 01fh, 018h, 018h, 018h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  000h, 000h, 000h, 000h, 0ffh, 0ffh, 0ffh, 0ffh, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h
    db  00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 000h, 000h
    db  000h, 000h, 076h, 0dch, 0c8h, 0dch, 076h, 000h, 000h, 078h, 0cch, 0f8h, 0cch, 0f8h, 0c0h, 0c0h
    db  000h, 0fch, 0cch, 0c0h, 0c0h, 0c0h, 0c0h, 000h, 000h, 0feh, 06ch, 06ch, 06ch, 06ch, 06ch, 000h
    db  0fch, 0cch, 060h, 030h, 060h, 0cch, 0fch, 000h, 000h, 000h, 07eh, 0d8h, 0d8h, 0d8h, 070h, 000h
    db  000h, 066h, 066h, 066h, 066h, 07ch, 060h, 0c0h, 000h, 076h, 0dch, 018h, 018h, 018h, 018h, 000h
    db  0fch, 030h, 078h, 0cch, 0cch, 078h, 030h, 0fch, 038h, 06ch, 0c6h, 0feh, 0c6h, 06ch, 038h, 000h
    db  038h, 06ch, 0c6h, 0c6h, 06ch, 06ch, 0eeh, 000h, 01ch, 030h, 018h, 07ch, 0cch, 0cch, 078h, 000h
    db  000h, 000h, 07eh, 0dbh, 0dbh, 07eh, 000h, 000h, 006h, 00ch, 07eh, 0dbh, 0dbh, 07eh, 060h, 0c0h
    db  038h, 060h, 0c0h, 0f8h, 0c0h, 060h, 038h, 000h, 078h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 000h
    db  000h, 0fch, 000h, 0fch, 000h, 0fch, 000h, 000h, 030h, 030h, 0fch, 030h, 030h, 000h, 0fch, 000h
    db  060h, 030h, 018h, 030h, 060h, 000h, 0fch, 000h, 018h, 030h, 060h, 030h, 018h, 000h, 0fch, 000h
    db  00eh, 01bh, 01bh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0d8h, 0d8h, 070h
    db  030h, 030h, 000h, 0fch, 000h, 030h, 030h, 000h, 000h, 076h, 0dch, 000h, 076h, 0dch, 000h, 000h
    db  038h, 06ch, 06ch, 038h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 000h, 000h, 000h, 00fh, 00ch, 00ch, 00ch, 0ech, 06ch, 03ch, 01ch
    db  078h, 06ch, 06ch, 06ch, 06ch, 000h, 000h, 000h, 070h, 018h, 030h, 060h, 078h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 03ch, 03ch, 03ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5d6c LB 0x2160 -> off=0x0 cb=0000000000000e00 uValue=00000000000c15ec 'vgafont14'
vgafont14:                                   ; 0xc5d6c LB 0xe00
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  07eh, 081h, 0a5h, 081h, 081h, 0bdh, 099h, 081h, 07eh, 000h, 000h, 000h, 000h, 000h, 07eh, 0ffh
    db  0dbh, 0ffh, 0ffh, 0c3h, 0e7h, 0ffh, 07eh, 000h, 000h, 000h, 000h, 000h, 000h, 06ch, 0feh, 0feh
    db  0feh, 0feh, 07ch, 038h, 010h, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 07ch, 0feh, 07ch
    db  038h, 010h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 03ch, 03ch, 0e7h, 0e7h, 0e7h, 018h, 018h
    db  03ch, 000h, 000h, 000h, 000h, 000h, 018h, 03ch, 07eh, 0ffh, 0ffh, 07eh, 018h, 018h, 03ch, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 03ch, 03ch, 018h, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0e7h, 0c3h, 0c3h, 0e7h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h
    db  000h, 000h, 03ch, 066h, 042h, 042h, 066h, 03ch, 000h, 000h, 000h, 000h, 0ffh, 0ffh, 0ffh, 0ffh
    db  0c3h, 099h, 0bdh, 0bdh, 099h, 0c3h, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 01eh, 00eh, 01ah, 032h
    db  078h, 0cch, 0cch, 0cch, 078h, 000h, 000h, 000h, 000h, 000h, 03ch, 066h, 066h, 066h, 03ch, 018h
    db  07eh, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 03fh, 033h, 03fh, 030h, 030h, 030h, 070h, 0f0h
    db  0e0h, 000h, 000h, 000h, 000h, 000h, 07fh, 063h, 07fh, 063h, 063h, 063h, 067h, 0e7h, 0e6h, 0c0h
    db  000h, 000h, 000h, 000h, 018h, 018h, 0dbh, 03ch, 0e7h, 03ch, 0dbh, 018h, 018h, 000h, 000h, 000h
    db  000h, 000h, 080h, 0c0h, 0e0h, 0f8h, 0feh, 0f8h, 0e0h, 0c0h, 080h, 000h, 000h, 000h, 000h, 000h
    db  002h, 006h, 00eh, 03eh, 0feh, 03eh, 00eh, 006h, 002h, 000h, 000h, 000h, 000h, 000h, 018h, 03ch
    db  07eh, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 066h, 066h
    db  066h, 066h, 000h, 066h, 066h, 000h, 000h, 000h, 000h, 000h, 07fh, 0dbh, 0dbh, 0dbh, 07bh, 01bh
    db  01bh, 01bh, 01bh, 000h, 000h, 000h, 000h, 07ch, 0c6h, 060h, 038h, 06ch, 0c6h, 0c6h, 06ch, 038h
    db  00ch, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 0feh, 0feh, 000h
    db  000h, 000h, 000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 07eh, 03ch, 018h, 07eh, 000h, 000h
    db  000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 00ch, 0feh, 00ch, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 030h, 060h
    db  0feh, 060h, 030h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0c0h, 0c0h, 0c0h
    db  0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 028h, 06ch, 0feh, 06ch, 028h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 038h, 07ch, 07ch, 0feh, 0feh, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0feh, 0feh, 07ch, 07ch, 038h, 038h, 010h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 03ch, 03ch, 03ch, 018h, 018h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 066h, 066h, 066h
    db  024h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 06ch, 06ch, 0feh, 06ch
    db  06ch, 06ch, 0feh, 06ch, 06ch, 000h, 000h, 000h, 018h, 018h, 07ch, 0c6h, 0c2h, 0c0h, 07ch, 006h
    db  086h, 0c6h, 07ch, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 0c2h, 0c6h, 00ch, 018h, 030h, 066h
    db  0c6h, 000h, 000h, 000h, 000h, 000h, 038h, 06ch, 06ch, 038h, 076h, 0dch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 000h, 030h, 030h, 030h, 060h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 00ch, 018h, 030h, 030h, 030h, 030h, 030h, 018h, 00ch, 000h, 000h, 000h, 000h, 000h
    db  030h, 018h, 00ch, 00ch, 00ch, 00ch, 00ch, 018h, 030h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  066h, 03ch, 0ffh, 03ch, 066h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h
    db  07eh, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 030h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h
    db  000h, 000h, 000h, 000h, 002h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 080h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0ceh, 0deh, 0f6h, 0e6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  018h, 038h, 078h, 018h, 018h, 018h, 018h, 018h, 07eh, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h
    db  006h, 00ch, 018h, 030h, 060h, 0c6h, 0feh, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 006h, 006h
    db  03ch, 006h, 006h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 00ch, 01ch, 03ch, 06ch, 0cch, 0feh
    db  00ch, 00ch, 01eh, 000h, 000h, 000h, 000h, 000h, 0feh, 0c0h, 0c0h, 0c0h, 0fch, 006h, 006h, 0c6h
    db  07ch, 000h, 000h, 000h, 000h, 000h, 038h, 060h, 0c0h, 0c0h, 0fch, 0c6h, 0c6h, 0c6h, 07ch, 000h
    db  000h, 000h, 000h, 000h, 0feh, 0c6h, 006h, 00ch, 018h, 030h, 030h, 030h, 030h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 07ch, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  07ch, 0c6h, 0c6h, 0c6h, 07eh, 006h, 006h, 00ch, 078h, 000h, 000h, 000h, 000h, 000h, 000h, 018h
    db  018h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h
    db  000h, 000h, 018h, 018h, 030h, 000h, 000h, 000h, 000h, 000h, 006h, 00ch, 018h, 030h, 060h, 030h
    db  018h, 00ch, 006h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 000h, 000h, 07eh, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 060h, 030h, 018h, 00ch, 006h, 00ch, 018h, 030h, 060h, 000h
    db  000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 00ch, 018h, 018h, 000h, 018h, 018h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0deh, 0deh, 0deh, 0dch, 0c0h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h, 000h, 0fch, 066h
    db  066h, 066h, 07ch, 066h, 066h, 066h, 0fch, 000h, 000h, 000h, 000h, 000h, 03ch, 066h, 0c2h, 0c0h
    db  0c0h, 0c0h, 0c2h, 066h, 03ch, 000h, 000h, 000h, 000h, 000h, 0f8h, 06ch, 066h, 066h, 066h, 066h
    db  066h, 06ch, 0f8h, 000h, 000h, 000h, 000h, 000h, 0feh, 066h, 062h, 068h, 078h, 068h, 062h, 066h
    db  0feh, 000h, 000h, 000h, 000h, 000h, 0feh, 066h, 062h, 068h, 078h, 068h, 060h, 060h, 0f0h, 000h
    db  000h, 000h, 000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0deh, 0c6h, 066h, 03ah, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h, 000h
    db  03ch, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 01eh, 00ch
    db  00ch, 00ch, 00ch, 00ch, 0cch, 0cch, 078h, 000h, 000h, 000h, 000h, 000h, 0e6h, 066h, 06ch, 06ch
    db  078h, 06ch, 06ch, 066h, 0e6h, 000h, 000h, 000h, 000h, 000h, 0f0h, 060h, 060h, 060h, 060h, 060h
    db  062h, 066h, 0feh, 000h, 000h, 000h, 000h, 000h, 0c6h, 0eeh, 0feh, 0feh, 0d6h, 0c6h, 0c6h, 0c6h
    db  0c6h, 000h, 000h, 000h, 000h, 000h, 0c6h, 0e6h, 0f6h, 0feh, 0deh, 0ceh, 0c6h, 0c6h, 0c6h, 000h
    db  000h, 000h, 000h, 000h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 06ch, 038h, 000h, 000h, 000h
    db  000h, 000h, 0fch, 066h, 066h, 066h, 07ch, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h, 000h
    db  07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0d6h, 0deh, 07ch, 00ch, 00eh, 000h, 000h, 000h, 000h, 0fch, 066h
    db  066h, 066h, 07ch, 06ch, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 060h
    db  038h, 00ch, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 07eh, 07eh, 05ah, 018h, 018h, 018h
    db  018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h
    db  07ch, 000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 06ch, 038h, 010h, 000h
    db  000h, 000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0d6h, 0d6h, 0feh, 07ch, 06ch, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 06ch, 038h, 038h, 038h, 06ch, 0c6h, 0c6h, 000h, 000h, 000h, 000h, 000h
    db  066h, 066h, 066h, 066h, 03ch, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 0feh, 0c6h
    db  08ch, 018h, 030h, 060h, 0c2h, 0c6h, 0feh, 000h, 000h, 000h, 000h, 000h, 03ch, 030h, 030h, 030h
    db  030h, 030h, 030h, 030h, 03ch, 000h, 000h, 000h, 000h, 000h, 080h, 0c0h, 0e0h, 070h, 038h, 01ch
    db  00eh, 006h, 002h, 000h, 000h, 000h, 000h, 000h, 03ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch
    db  03ch, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h
    db  030h, 030h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 000h, 0e0h, 060h
    db  060h, 078h, 06ch, 066h, 066h, 066h, 07ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch
    db  0c6h, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 01ch, 00ch, 00ch, 03ch, 06ch, 0cch
    db  0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h
    db  07ch, 000h, 000h, 000h, 000h, 000h, 038h, 06ch, 064h, 060h, 0f0h, 060h, 060h, 060h, 0f0h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 076h, 0cch, 0cch, 0cch, 07ch, 00ch, 0cch, 078h, 000h
    db  000h, 000h, 0e0h, 060h, 060h, 06ch, 076h, 066h, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 000h, 038h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 006h, 006h
    db  000h, 00eh, 006h, 006h, 006h, 006h, 066h, 066h, 03ch, 000h, 000h, 000h, 0e0h, 060h, 060h, 066h
    db  06ch, 078h, 06ch, 066h, 0e6h, 000h, 000h, 000h, 000h, 000h, 038h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 03ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ech, 0feh, 0d6h, 0d6h, 0d6h
    db  0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0dch, 066h, 066h, 066h, 066h, 066h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0dch, 066h, 066h, 066h, 07ch, 060h, 060h, 0f0h, 000h, 000h, 000h
    db  000h, 000h, 000h, 076h, 0cch, 0cch, 0cch, 07ch, 00ch, 00ch, 01eh, 000h, 000h, 000h, 000h, 000h
    db  000h, 0dch, 076h, 066h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch
    db  0c6h, 070h, 01ch, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 010h, 030h, 030h, 0fch, 030h, 030h
    db  030h, 036h, 01ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch
    db  076h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 066h, 066h, 03ch, 018h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 0d6h, 0d6h, 0feh, 06ch, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c6h, 06ch, 038h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 07eh, 006h, 00ch, 0f8h, 000h, 000h, 000h, 000h, 000h
    db  000h, 0feh, 0cch, 018h, 030h, 066h, 0feh, 000h, 000h, 000h, 000h, 000h, 00eh, 018h, 018h, 018h
    db  070h, 018h, 018h, 018h, 00eh, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 000h, 018h
    db  018h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 070h, 018h, 018h, 018h, 00eh, 018h, 018h, 018h
    db  070h, 000h, 000h, 000h, 000h, 000h, 076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0c2h, 066h, 03ch, 00ch, 006h, 07ch, 000h, 000h, 000h
    db  0cch, 0cch, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 00ch, 018h, 030h
    db  000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 000h, 078h
    db  00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 000h, 0cch, 0cch, 000h, 078h, 00ch, 07ch
    db  0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 060h, 030h, 018h, 000h, 078h, 00ch, 07ch, 0cch, 0cch
    db  076h, 000h, 000h, 000h, 000h, 038h, 06ch, 038h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 03ch, 066h, 060h, 066h, 03ch, 00ch, 006h, 03ch, 000h, 000h
    db  000h, 010h, 038h, 06ch, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  0cch, 0cch, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 060h, 030h, 018h
    db  000h, 07ch, 0c6h, 0feh, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 000h, 038h
    db  018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 018h, 03ch, 066h, 000h, 038h, 018h, 018h
    db  018h, 018h, 03ch, 000h, 000h, 000h, 000h, 060h, 030h, 018h, 000h, 038h, 018h, 018h, 018h, 018h
    db  03ch, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 000h
    db  000h, 000h, 038h, 06ch, 038h, 000h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 000h, 000h, 000h
    db  018h, 030h, 060h, 000h, 0feh, 066h, 060h, 07ch, 060h, 066h, 0feh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0cch, 076h, 036h, 07eh, 0d8h, 0d8h, 06eh, 000h, 000h, 000h, 000h, 000h, 03eh, 06ch
    db  0cch, 0cch, 0feh, 0cch, 0cch, 0cch, 0ceh, 000h, 000h, 000h, 000h, 010h, 038h, 06ch, 000h, 07ch
    db  0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 000h, 07ch, 0c6h, 0c6h
    db  0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 060h, 030h, 018h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h
    db  07ch, 000h, 000h, 000h, 000h, 030h, 078h, 0cch, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h
    db  000h, 000h, 000h, 060h, 030h, 018h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 07eh, 006h, 00ch, 078h, 000h, 000h, 0c6h
    db  0c6h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 0c6h, 06ch, 038h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 000h
    db  0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 018h, 018h, 03ch, 066h, 060h
    db  060h, 066h, 03ch, 018h, 018h, 000h, 000h, 000h, 000h, 038h, 06ch, 064h, 060h, 0f0h, 060h, 060h
    db  060h, 0e6h, 0fch, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 03ch, 018h, 07eh, 018h, 07eh, 018h
    db  018h, 000h, 000h, 000h, 000h, 0f8h, 0cch, 0cch, 0f8h, 0c4h, 0cch, 0deh, 0cch, 0cch, 0c6h, 000h
    db  000h, 000h, 000h, 00eh, 01bh, 018h, 018h, 018h, 07eh, 018h, 018h, 018h, 018h, 0d8h, 070h, 000h
    db  000h, 018h, 030h, 060h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 00ch
    db  018h, 030h, 000h, 038h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 018h, 030h, 060h
    db  000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 018h, 030h, 060h, 000h, 0cch
    db  0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h, 000h, 076h, 0dch, 000h, 0dch, 066h, 066h
    db  066h, 066h, 066h, 000h, 000h, 000h, 076h, 0dch, 000h, 0c6h, 0e6h, 0f6h, 0feh, 0deh, 0ceh, 0c6h
    db  0c6h, 000h, 000h, 000h, 000h, 03ch, 06ch, 06ch, 03eh, 000h, 07eh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 038h, 06ch, 06ch, 038h, 000h, 07ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 030h, 030h, 000h, 030h, 030h, 060h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0feh, 0c0h, 0c0h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 006h, 006h, 006h, 000h, 000h, 000h, 000h, 000h, 0c0h, 0c0h, 0c6h, 0cch, 0d8h
    db  030h, 060h, 0dch, 086h, 00ch, 018h, 03eh, 000h, 000h, 0c0h, 0c0h, 0c6h, 0cch, 0d8h, 030h, 066h
    db  0ceh, 09eh, 03eh, 006h, 006h, 000h, 000h, 000h, 018h, 018h, 000h, 018h, 018h, 03ch, 03ch, 03ch
    db  018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 036h, 06ch, 0d8h, 06ch, 036h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 0d8h, 06ch, 036h, 06ch, 0d8h, 000h, 000h, 000h, 000h, 000h
    db  011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 055h, 0aah
    db  055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 0ddh, 077h, 0ddh, 077h
    db  0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 018h, 018h
    db  018h, 018h, 018h, 018h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0f6h, 036h, 036h, 036h, 036h
    db  036h, 036h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 036h, 036h
    db  036h, 036h, 036h, 0f6h, 006h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 000h, 000h, 000h, 000h, 000h, 0feh
    db  006h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0f6h, 006h, 0feh
    db  000h, 000h, 000h, 000h, 000h, 000h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0feh, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0ffh, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 018h, 01fh, 018h, 018h, 018h, 018h
    db  018h, 018h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 037h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 037h, 030h, 03fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 03fh, 030h, 037h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 0f7h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  000h, 0f7h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 037h, 030h, 037h
    db  036h, 036h, 036h, 036h, 036h, 036h, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0ffh, 000h, 000h
    db  000h, 000h, 000h, 000h, 036h, 036h, 036h, 036h, 036h, 0f7h, 000h, 0f7h, 036h, 036h, 036h, 036h
    db  036h, 036h, 018h, 018h, 018h, 018h, 018h, 0ffh, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0ffh, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 03fh, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 018h, 018h, 018h, 01fh, 018h, 01fh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 01fh, 018h, 01fh, 018h, 018h
    db  018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 03fh, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h
    db  018h, 018h, 018h, 018h, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h
    db  0f0h, 0f0h, 0f0h, 0f0h, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh
    db  00fh, 00fh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0dch, 0d8h, 0d8h, 0dch, 076h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0fch, 0c6h, 0c6h, 0fch, 0c0h, 0c0h, 040h, 000h, 000h, 000h, 0feh, 0c6h
    db  0c6h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 06ch
    db  06ch, 06ch, 06ch, 06ch, 06ch, 000h, 000h, 000h, 000h, 000h, 0feh, 0c6h, 060h, 030h, 018h, 030h
    db  060h, 0c6h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 0d8h, 0d8h, 0d8h, 0d8h
    db  070h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 066h, 066h, 066h, 066h, 07ch, 060h, 060h, 0c0h
    db  000h, 000h, 000h, 000h, 000h, 000h, 076h, 0dch, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h
    db  000h, 000h, 07eh, 018h, 03ch, 066h, 066h, 066h, 03ch, 018h, 07eh, 000h, 000h, 000h, 000h, 000h
    db  038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 06ch, 038h, 000h, 000h, 000h, 000h, 000h, 038h, 06ch
    db  0c6h, 0c6h, 0c6h, 06ch, 06ch, 06ch, 0eeh, 000h, 000h, 000h, 000h, 000h, 01eh, 030h, 018h, 00ch
    db  03eh, 066h, 066h, 066h, 03ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07eh, 0dbh, 0dbh
    db  07eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 003h, 006h, 07eh, 0dbh, 0dbh, 0f3h, 07eh, 060h
    db  0c0h, 000h, 000h, 000h, 000h, 000h, 01ch, 030h, 060h, 060h, 07ch, 060h, 060h, 030h, 01ch, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0feh, 000h, 000h, 0feh, 000h, 000h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 018h, 018h, 07eh, 018h, 018h, 000h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 030h, 018h
    db  00ch, 006h, 00ch, 018h, 030h, 000h, 07eh, 000h, 000h, 000h, 000h, 000h, 00ch, 018h, 030h, 060h
    db  030h, 018h, 00ch, 000h, 07eh, 000h, 000h, 000h, 000h, 000h, 00eh, 01bh, 01bh, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0d8h, 0d8h
    db  070h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h, 07eh, 000h, 018h, 018h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 076h, 0dch, 000h, 076h, 0dch, 000h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 06ch, 038h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 00fh, 00ch, 00ch, 00ch, 00ch
    db  00ch, 0ech, 06ch, 03ch, 01ch, 000h, 000h, 000h, 000h, 0d8h, 06ch, 06ch, 06ch, 06ch, 06ch, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 070h, 0d8h, 030h, 060h, 0c8h, 0f8h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 07ch, 07ch, 07ch, 07ch, 07ch, 07ch, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc6b6c LB 0x1360 -> off=0x0 cb=0000000000001000 uValue=00000000000c23ec 'vgafont16'
vgafont16:                                   ; 0xc6b6c LB 0x1000
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07eh, 081h, 0a5h, 081h, 081h, 0bdh, 099h, 081h, 081h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 07eh, 0ffh, 0dbh, 0ffh, 0ffh, 0c3h, 0e7h, 0ffh, 0ffh, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 06ch, 0feh, 0feh, 0feh, 0feh, 07ch, 038h, 010h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 010h, 038h, 07ch, 0feh, 07ch, 038h, 010h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 018h, 03ch, 03ch, 0e7h, 0e7h, 0e7h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 018h, 03ch, 07eh, 0ffh, 0ffh, 07eh, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 018h, 03ch, 03ch, 018h, 000h, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0e7h, 0c3h, 0c3h, 0e7h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 03ch, 066h, 042h, 042h, 066h, 03ch, 000h, 000h, 000h, 000h, 000h
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0c3h, 099h, 0bdh, 0bdh, 099h, 0c3h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  000h, 000h, 01eh, 00eh, 01ah, 032h, 078h, 0cch, 0cch, 0cch, 0cch, 078h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 066h, 066h, 066h, 03ch, 018h, 07eh, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03fh, 033h, 03fh, 030h, 030h, 030h, 030h, 070h, 0f0h, 0e0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07fh, 063h, 07fh, 063h, 063h, 063h, 063h, 067h, 0e7h, 0e6h, 0c0h, 000h, 000h, 000h
    db  000h, 000h, 000h, 018h, 018h, 0dbh, 03ch, 0e7h, 03ch, 0dbh, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 080h, 0c0h, 0e0h, 0f0h, 0f8h, 0feh, 0f8h, 0f0h, 0e0h, 0c0h, 080h, 000h, 000h, 000h, 000h
    db  000h, 002h, 006h, 00eh, 01eh, 03eh, 0feh, 03eh, 01eh, 00eh, 006h, 002h, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 066h, 066h, 066h, 066h, 066h, 066h, 066h, 000h, 066h, 066h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07fh, 0dbh, 0dbh, 0dbh, 07bh, 01bh, 01bh, 01bh, 01bh, 01bh, 000h, 000h, 000h, 000h
    db  000h, 07ch, 0c6h, 060h, 038h, 06ch, 0c6h, 0c6h, 06ch, 038h, 00ch, 0c6h, 07ch, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 0feh, 0feh, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 07eh, 03ch, 018h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 03ch, 07eh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 07eh, 03ch, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 018h, 00ch, 0feh, 00ch, 018h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 030h, 060h, 0feh, 060h, 030h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 0c0h, 0c0h, 0c0h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 010h, 038h, 038h, 07ch, 07ch, 0feh, 0feh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0feh, 0feh, 07ch, 07ch, 038h, 038h, 010h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 03ch, 03ch, 03ch, 018h, 018h, 018h, 000h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 066h, 066h, 066h, 024h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 06ch, 06ch, 0feh, 06ch, 06ch, 06ch, 0feh, 06ch, 06ch, 000h, 000h, 000h, 000h
    db  018h, 018h, 07ch, 0c6h, 0c2h, 0c0h, 07ch, 006h, 006h, 086h, 0c6h, 07ch, 018h, 018h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0c2h, 0c6h, 00ch, 018h, 030h, 060h, 0c6h, 086h, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 06ch, 06ch, 038h, 076h, 0dch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 030h, 030h, 030h, 060h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 00ch, 018h, 030h, 030h, 030h, 030h, 030h, 030h, 018h, 00ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 030h, 018h, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 018h, 030h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 066h, 03ch, 0ffh, 03ch, 066h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 018h, 018h, 07eh, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 018h, 030h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 002h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 080h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c3h, 0c3h, 0dbh, 0dbh, 0c3h, 0c3h, 066h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 038h, 078h, 018h, 018h, 018h, 018h, 018h, 018h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 006h, 00ch, 018h, 030h, 060h, 0c0h, 0c6h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 006h, 006h, 03ch, 006h, 006h, 006h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 00ch, 01ch, 03ch, 06ch, 0cch, 0feh, 00ch, 00ch, 00ch, 01eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 0c0h, 0c0h, 0c0h, 0fch, 006h, 006h, 006h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 060h, 0c0h, 0c0h, 0fch, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 0c6h, 006h, 006h, 00ch, 018h, 030h, 030h, 030h, 030h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 07eh, 006h, 006h, 006h, 00ch, 078h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 018h, 018h, 030h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 006h, 00ch, 018h, 030h, 060h, 030h, 018h, 00ch, 006h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07eh, 000h, 000h, 07eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 060h, 030h, 018h, 00ch, 006h, 00ch, 018h, 030h, 060h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 00ch, 018h, 018h, 018h, 000h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0deh, 0deh, 0deh, 0dch, 0c0h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0fch, 066h, 066h, 066h, 07ch, 066h, 066h, 066h, 066h, 0fch, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0c0h, 0c0h, 0c2h, 066h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0f8h, 06ch, 066h, 066h, 066h, 066h, 066h, 066h, 06ch, 0f8h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 066h, 062h, 068h, 078h, 068h, 060h, 062h, 066h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 066h, 062h, 068h, 078h, 068h, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0deh, 0c6h, 0c6h, 066h, 03ah, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 01eh, 00ch, 00ch, 00ch, 00ch, 00ch, 0cch, 0cch, 0cch, 078h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0e6h, 066h, 066h, 06ch, 078h, 078h, 06ch, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0f0h, 060h, 060h, 060h, 060h, 060h, 060h, 062h, 066h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0e7h, 0ffh, 0ffh, 0dbh, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0e6h, 0f6h, 0feh, 0deh, 0ceh, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0fch, 066h, 066h, 066h, 07ch, 060h, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0d6h, 0deh, 07ch, 00ch, 00eh, 000h, 000h
    db  000h, 000h, 0fch, 066h, 066h, 066h, 07ch, 06ch, 066h, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 07ch, 0c6h, 0c6h, 060h, 038h, 00ch, 006h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0ffh, 0dbh, 099h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh, 066h, 066h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0c3h, 066h, 03ch, 018h, 018h, 03ch, 066h, 0c3h, 0c3h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0ffh, 0c3h, 086h, 00ch, 018h, 030h, 060h, 0c1h, 0c3h, 0ffh, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 030h, 030h, 030h, 030h, 030h, 030h, 030h, 030h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 080h, 0c0h, 0e0h, 070h, 038h, 01ch, 00eh, 006h, 002h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 00ch, 03ch, 000h, 000h, 000h, 000h
    db  010h, 038h, 06ch, 0c6h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 000h
    db  030h, 030h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0e0h, 060h, 060h, 078h, 06ch, 066h, 066h, 066h, 066h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c0h, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 01ch, 00ch, 00ch, 03ch, 06ch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 06ch, 064h, 060h, 0f0h, 060h, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0cch, 0cch, 0cch, 0cch, 0cch, 07ch, 00ch, 0cch, 078h, 000h
    db  000h, 000h, 0e0h, 060h, 060h, 06ch, 076h, 066h, 066h, 066h, 066h, 0e6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 018h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 006h, 006h, 000h, 00eh, 006h, 006h, 006h, 006h, 006h, 006h, 066h, 066h, 03ch, 000h
    db  000h, 000h, 0e0h, 060h, 060h, 066h, 06ch, 078h, 078h, 06ch, 066h, 0e6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0e6h, 0ffh, 0dbh, 0dbh, 0dbh, 0dbh, 0dbh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0dch, 066h, 066h, 066h, 066h, 066h, 066h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0dch, 066h, 066h, 066h, 066h, 066h, 07ch, 060h, 060h, 0f0h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0cch, 0cch, 0cch, 0cch, 0cch, 07ch, 00ch, 00ch, 01eh, 000h
    db  000h, 000h, 000h, 000h, 000h, 0dch, 076h, 066h, 060h, 060h, 060h, 0f0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07ch, 0c6h, 060h, 038h, 00ch, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 010h, 030h, 030h, 0fch, 030h, 030h, 030h, 030h, 036h, 01ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh, 066h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c3h, 066h, 03ch, 018h, 03ch, 066h, 0c3h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07eh, 006h, 00ch, 0f8h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0feh, 0cch, 018h, 030h, 060h, 0c6h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 00eh, 018h, 018h, 018h, 070h, 018h, 018h, 018h, 018h, 00eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 018h, 018h, 018h, 018h, 000h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 070h, 018h, 018h, 018h, 00eh, 018h, 018h, 018h, 018h, 070h, 000h, 000h, 000h, 000h
    db  000h, 000h, 076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 0feh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03ch, 066h, 0c2h, 0c0h, 0c0h, 0c0h, 0c2h, 066h, 03ch, 00ch, 006h, 07ch, 000h, 000h
    db  000h, 000h, 0cch, 000h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 00ch, 018h, 030h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 010h, 038h, 06ch, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0cch, 000h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 038h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 03ch, 066h, 060h, 060h, 066h, 03ch, 00ch, 006h, 03ch, 000h, 000h, 000h
    db  000h, 010h, 038h, 06ch, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 000h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 07ch, 0c6h, 0feh, 0c0h, 0c0h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 066h, 000h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 018h, 03ch, 066h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 0c6h, 000h, 010h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  038h, 06ch, 038h, 000h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  018h, 030h, 060h, 000h, 0feh, 066h, 060h, 07ch, 060h, 060h, 066h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 06eh, 03bh, 01bh, 07eh, 0d8h, 0dch, 077h, 000h, 000h, 000h, 000h
    db  000h, 000h, 03eh, 06ch, 0cch, 0cch, 0feh, 0cch, 0cch, 0cch, 0cch, 0ceh, 000h, 000h, 000h, 000h
    db  000h, 010h, 038h, 06ch, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 030h, 078h, 0cch, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 060h, 030h, 018h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c6h, 000h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07eh, 006h, 00ch, 078h, 000h
    db  000h, 0c6h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 0c6h, 000h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 018h, 018h, 07eh, 0c3h, 0c0h, 0c0h, 0c0h, 0c3h, 07eh, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 064h, 060h, 0f0h, 060h, 060h, 060h, 060h, 0e6h, 0fch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 066h, 03ch, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 0fch, 066h, 066h, 07ch, 062h, 066h, 06fh, 066h, 066h, 066h, 0f3h, 000h, 000h, 000h, 000h
    db  000h, 00eh, 01bh, 018h, 018h, 018h, 07eh, 018h, 018h, 018h, 018h, 018h, 0d8h, 070h, 000h, 000h
    db  000h, 018h, 030h, 060h, 000h, 078h, 00ch, 07ch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 00ch, 018h, 030h, 000h, 038h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 018h, 030h, 060h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 018h, 030h, 060h, 000h, 0cch, 0cch, 0cch, 0cch, 0cch, 0cch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 076h, 0dch, 000h, 0dch, 066h, 066h, 066h, 066h, 066h, 066h, 000h, 000h, 000h, 000h
    db  076h, 0dch, 000h, 0c6h, 0e6h, 0f6h, 0feh, 0deh, 0ceh, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 03ch, 06ch, 06ch, 03eh, 000h, 07eh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 06ch, 038h, 000h, 07ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 030h, 030h, 000h, 030h, 030h, 060h, 0c0h, 0c6h, 0c6h, 07ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 0feh, 0c0h, 0c0h, 0c0h, 0c0h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 0feh, 006h, 006h, 006h, 006h, 000h, 000h, 000h, 000h, 000h
    db  000h, 0c0h, 0c0h, 0c2h, 0c6h, 0cch, 018h, 030h, 060h, 0ceh, 09bh, 006h, 00ch, 01fh, 000h, 000h
    db  000h, 0c0h, 0c0h, 0c2h, 0c6h, 0cch, 018h, 030h, 066h, 0ceh, 096h, 03eh, 006h, 006h, 000h, 000h
    db  000h, 000h, 018h, 018h, 000h, 018h, 018h, 018h, 03ch, 03ch, 03ch, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 036h, 06ch, 0d8h, 06ch, 036h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0d8h, 06ch, 036h, 06ch, 0d8h, 000h, 000h, 000h, 000h, 000h, 000h
    db  011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h, 011h, 044h
    db  055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah, 055h, 0aah
    db  0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h, 0ddh, 077h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0feh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  036h, 036h, 036h, 036h, 036h, 0f6h, 006h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 0feh, 006h, 0f6h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 0f6h, 006h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0feh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 0f8h, 018h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0f8h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 01fh, 018h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 037h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 037h, 030h, 03fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 03fh, 030h, 037h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 0f7h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0f7h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 037h, 030h, 037h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 036h, 0f7h, 000h, 0f7h, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  018h, 018h, 018h, 018h, 018h, 0ffh, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 0ffh, 000h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 03fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  018h, 018h, 018h, 018h, 018h, 01fh, 018h, 01fh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 01fh, 018h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 03fh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  036h, 036h, 036h, 036h, 036h, 036h, 036h, 0ffh, 036h, 036h, 036h, 036h, 036h, 036h, 036h, 036h
    db  018h, 018h, 018h, 018h, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 01fh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh
    db  0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h, 0f0h
    db  00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh, 00fh
    db  0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0dch, 0d8h, 0d8h, 0d8h, 0dch, 076h, 000h, 000h, 000h, 000h
    db  000h, 000h, 078h, 0cch, 0cch, 0cch, 0d8h, 0cch, 0c6h, 0c6h, 0c6h, 0cch, 000h, 000h, 000h, 000h
    db  000h, 000h, 0feh, 0c6h, 0c6h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 0c0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0feh, 06ch, 06ch, 06ch, 06ch, 06ch, 06ch, 06ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 0feh, 0c6h, 060h, 030h, 018h, 030h, 060h, 0c6h, 0feh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07eh, 0d8h, 0d8h, 0d8h, 0d8h, 0d8h, 070h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 066h, 066h, 066h, 066h, 066h, 07ch, 060h, 060h, 0c0h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 076h, 0dch, 018h, 018h, 018h, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 07eh, 018h, 03ch, 066h, 066h, 066h, 03ch, 018h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 038h, 06ch, 0c6h, 0c6h, 0feh, 0c6h, 0c6h, 06ch, 038h, 000h, 000h, 000h, 000h
    db  000h, 000h, 038h, 06ch, 0c6h, 0c6h, 0c6h, 06ch, 06ch, 06ch, 06ch, 0eeh, 000h, 000h, 000h, 000h
    db  000h, 000h, 01eh, 030h, 018h, 00ch, 03eh, 066h, 066h, 066h, 066h, 03ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 07eh, 0dbh, 0dbh, 0dbh, 07eh, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 003h, 006h, 07eh, 0dbh, 0dbh, 0f3h, 07eh, 060h, 0c0h, 000h, 000h, 000h, 000h
    db  000h, 000h, 01ch, 030h, 060h, 060h, 07ch, 060h, 060h, 060h, 030h, 01ch, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 07ch, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 0c6h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 0feh, 000h, 000h, 0feh, 000h, 000h, 0feh, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 07eh, 018h, 018h, 000h, 000h, 0ffh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 030h, 018h, 00ch, 006h, 00ch, 018h, 030h, 000h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 00ch, 018h, 030h, 060h, 030h, 018h, 00ch, 000h, 07eh, 000h, 000h, 000h, 000h
    db  000h, 000h, 00eh, 01bh, 01bh, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h
    db  018h, 018h, 018h, 018h, 018h, 018h, 018h, 018h, 0d8h, 0d8h, 0d8h, 070h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 018h, 018h, 000h, 07eh, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 076h, 0dch, 000h, 076h, 0dch, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 038h, 06ch, 06ch, 038h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 018h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 00fh, 00ch, 00ch, 00ch, 00ch, 00ch, 0ech, 06ch, 06ch, 03ch, 01ch, 000h, 000h, 000h, 000h
    db  000h, 0d8h, 06ch, 06ch, 06ch, 06ch, 06ch, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 070h, 0d8h, 030h, 060h, 0c8h, 0f8h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 07ch, 07ch, 07ch, 07ch, 07ch, 07ch, 07ch, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc7b6c LB 0x360 -> off=0x0 cb=000000000000012d uValue=00000000000c33ec 'vgafont14alt'
vgafont14alt:                                ; 0xc7b6c LB 0x12d
    db  01dh, 000h, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h, 000h, 000h, 000h, 022h
    db  000h, 063h, 063h, 063h, 022h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 02bh, 000h
    db  000h, 000h, 018h, 018h, 018h, 0ffh, 018h, 018h, 018h, 000h, 000h, 000h, 000h, 02dh, 000h, 000h
    db  000h, 000h, 000h, 000h, 0ffh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 04dh, 000h, 000h, 0c3h
    db  0e7h, 0ffh, 0dbh, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 000h, 000h, 000h, 054h, 000h, 000h, 0ffh, 0dbh
    db  099h, 018h, 018h, 018h, 018h, 018h, 03ch, 000h, 000h, 000h, 056h, 000h, 000h, 0c3h, 0c3h, 0c3h
    db  0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 000h, 000h, 000h, 057h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h
    db  0dbh, 0dbh, 0ffh, 066h, 066h, 000h, 000h, 000h, 058h, 000h, 000h, 0c3h, 0c3h, 066h, 03ch, 018h
    db  03ch, 066h, 0c3h, 0c3h, 000h, 000h, 000h, 059h, 000h, 000h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h
    db  018h, 018h, 03ch, 000h, 000h, 000h, 05ah, 000h, 000h, 0ffh, 0c3h, 086h, 00ch, 018h, 030h, 061h
    db  0c3h, 0ffh, 000h, 000h, 000h, 06dh, 000h, 000h, 000h, 000h, 000h, 0e6h, 0ffh, 0dbh, 0dbh, 0dbh
    db  0dbh, 000h, 000h, 000h, 076h, 000h, 000h, 000h, 000h, 000h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h
    db  000h, 000h, 000h, 077h, 000h, 000h, 000h, 000h, 000h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh, 066h, 000h
    db  000h, 000h, 091h, 000h, 000h, 000h, 000h, 06eh, 03bh, 01bh, 07eh, 0d8h, 0dch, 077h, 000h, 000h
    db  000h, 09bh, 000h, 018h, 018h, 07eh, 0c3h, 0c0h, 0c0h, 0c3h, 07eh, 018h, 018h, 000h, 000h, 000h
    db  09dh, 000h, 000h, 0c3h, 066h, 03ch, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 000h, 000h, 000h, 09eh
    db  000h, 0fch, 066h, 066h, 07ch, 062h, 066h, 06fh, 066h, 066h, 0f3h, 000h, 000h, 000h, 0f1h, 000h
    db  000h, 018h, 018h, 018h, 0ffh, 018h, 018h, 018h, 000h, 0ffh, 000h, 000h, 000h, 0f6h, 000h, 000h
    db  018h, 018h, 000h, 000h, 0ffh, 000h, 000h, 018h, 018h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc7c99 LB 0x233 -> off=0x0 cb=0000000000000144 uValue=00000000000c3519 'vgafont16alt'
vgafont16alt:                                ; 0xc7c99 LB 0x144
    db  01dh, 000h, 000h, 000h, 000h, 000h, 024h, 066h, 0ffh, 066h, 024h, 000h, 000h, 000h, 000h, 000h
    db  000h, 030h, 000h, 000h, 03ch, 066h, 0c3h, 0c3h, 0dbh, 0dbh, 0c3h, 0c3h, 066h, 03ch, 000h, 000h
    db  000h, 000h, 04dh, 000h, 000h, 0c3h, 0e7h, 0ffh, 0ffh, 0dbh, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 000h
    db  000h, 000h, 000h, 054h, 000h, 000h, 0ffh, 0dbh, 099h, 018h, 018h, 018h, 018h, 018h, 018h, 03ch
    db  000h, 000h, 000h, 000h, 056h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 066h, 03ch
    db  018h, 000h, 000h, 000h, 000h, 057h, 000h, 000h, 0c3h, 0c3h, 0c3h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh
    db  066h, 066h, 000h, 000h, 000h, 000h, 058h, 000h, 000h, 0c3h, 0c3h, 066h, 03ch, 018h, 018h, 03ch
    db  066h, 0c3h, 0c3h, 000h, 000h, 000h, 000h, 059h, 000h, 000h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h
    db  018h, 018h, 018h, 03ch, 000h, 000h, 000h, 000h, 05ah, 000h, 000h, 0ffh, 0c3h, 086h, 00ch, 018h
    db  030h, 060h, 0c1h, 0c3h, 0ffh, 000h, 000h, 000h, 000h, 06dh, 000h, 000h, 000h, 000h, 000h, 0e6h
    db  0ffh, 0dbh, 0dbh, 0dbh, 0dbh, 0dbh, 000h, 000h, 000h, 000h, 076h, 000h, 000h, 000h, 000h, 000h
    db  0c3h, 0c3h, 0c3h, 0c3h, 066h, 03ch, 018h, 000h, 000h, 000h, 000h, 077h, 000h, 000h, 000h, 000h
    db  000h, 0c3h, 0c3h, 0c3h, 0dbh, 0dbh, 0ffh, 066h, 000h, 000h, 000h, 000h, 078h, 000h, 000h, 000h
    db  000h, 000h, 0c3h, 066h, 03ch, 018h, 03ch, 066h, 0c3h, 000h, 000h, 000h, 000h, 091h, 000h, 000h
    db  000h, 000h, 000h, 06eh, 03bh, 01bh, 07eh, 0d8h, 0dch, 077h, 000h, 000h, 000h, 000h, 09bh, 000h
    db  018h, 018h, 07eh, 0c3h, 0c0h, 0c0h, 0c0h, 0c3h, 07eh, 018h, 018h, 000h, 000h, 000h, 000h, 09dh
    db  000h, 000h, 0c3h, 066h, 03ch, 018h, 0ffh, 018h, 0ffh, 018h, 018h, 018h, 000h, 000h, 000h, 000h
    db  09eh, 000h, 0fch, 066h, 066h, 07ch, 062h, 066h, 06fh, 066h, 066h, 066h, 0f3h, 000h, 000h, 000h
    db  000h, 0abh, 000h, 0c0h, 0c0h, 0c2h, 0c6h, 0cch, 018h, 030h, 060h, 0ceh, 09bh, 006h, 00ch, 01fh
    db  000h, 000h, 0ach, 000h, 0c0h, 0c0h, 0c2h, 0c6h, 0cch, 018h, 030h, 066h, 0ceh, 096h, 03eh, 006h
    db  006h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc7ddd LB 0xef -> off=0x0 cb=0000000000000008 uValue=00000000000c365d '_cga_msr'
_cga_msr:                                    ; 0xc7ddd LB 0x8
    db  02ch, 028h, 02dh, 029h, 02ah, 02eh, 01eh, 029h
  ; disGetNextSymbol 0xc7de5 LB 0xe7 -> off=0x0 cb=0000000000000008 uValue=00000000000c3665 'line_to_vpti_200'
line_to_vpti_200:                            ; 0xc7de5 LB 0x8
    db  000h, 001h, 002h, 003h, 0ffh, 0ffh, 0ffh, 007h
  ; disGetNextSymbol 0xc7ded LB 0xdf -> off=0x0 cb=0000000000000008 uValue=00000000000c366d 'line_to_vpti_350'
line_to_vpti_350:                            ; 0xc7ded LB 0x8
    db  013h, 014h, 015h, 016h, 0ffh, 0ffh, 0ffh, 007h
  ; disGetNextSymbol 0xc7df5 LB 0xd7 -> off=0x0 cb=0000000000000008 uValue=00000000000c3675 'line_to_vpti_400'
line_to_vpti_400:                            ; 0xc7df5 LB 0x8
    db  017h, 017h, 018h, 018h, 0ffh, 0ffh, 0ffh, 019h
  ; disGetNextSymbol 0xc7dfd LB 0xcf -> off=0x0 cb=0000000000000005 uValue=00000000000c367d 'row_tbl'
row_tbl:                                     ; 0xc7dfd LB 0x5
    db  000h, 00eh, 019h, 02bh, 000h
  ; disGetNextSymbol 0xc7e02 LB 0xca -> off=0x0 cb=0000000000000015 uValue=00000000000c3682 '_vbebios_copyright'
_vbebios_copyright:                          ; 0xc7e02 LB 0x15
    db  'VirtualBox VESA BIOS', 000h
  ; disGetNextSymbol 0xc7e17 LB 0xb5 -> off=0x0 cb=000000000000001d uValue=00000000000c3697 '_vbebios_vendor_name'
_vbebios_vendor_name:                        ; 0xc7e17 LB 0x1d
    db  'Oracle and/or its affiliates', 000h
  ; disGetNextSymbol 0xc7e34 LB 0x98 -> off=0x0 cb=0000000000000021 uValue=00000000000c36b4 '_vbebios_product_name'
_vbebios_product_name:                       ; 0xc7e34 LB 0x21
    db  'Oracle VM VirtualBox VBE Adapter', 000h
  ; disGetNextSymbol 0xc7e55 LB 0x77 -> off=0x0 cb=0000000000000023 uValue=00000000000c36d5 '_vbebios_product_revision'
_vbebios_product_revision:                   ; 0xc7e55 LB 0x23
    db  'Oracle VM VirtualBox Version 7.0.2', 000h
  ; disGetNextSymbol 0xc7e78 LB 0x54 -> off=0x0 cb=000000000000002b uValue=00000000000c36f8 '_vbebios_info_string'
_vbebios_info_string:                        ; 0xc7e78 LB 0x2b
    db  'VirtualBox VBE Display Adapter enabled', 00dh, 00ah, 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc7ea3 LB 0x29 -> off=0x0 cb=0000000000000029 uValue=00000000000c3723 '_no_vbebios_info_string'
_no_vbebios_info_string:                     ; 0xc7ea3 LB 0x29
    db  'No VirtualBox VBE support available!', 00dh, 00ah, 00dh, 00ah, 000h

section CONST progbits vstart=0x7ecc align=1 ; size=0x0 class=DATA group=DGROUP

section CONST2 progbits vstart=0x7ecc align=1 ; size=0x0 class=DATA group=DGROUP

  ; Padding 0x134 bytes at 0xc7ecc
    db  001h, 000h, 000h, 000h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 02fh, 068h, 06fh, 06dh
    db  065h, 02fh, 073h, 062h, 075h, 072h, 063h, 068h, 069h, 06ch, 02fh, 076h, 062h, 05fh, 073h, 072h
    db  063h, 02fh, 074h, 072h, 075h, 06eh, 06bh, 02fh, 06fh, 075h, 074h, 02fh, 06ch, 069h, 06eh, 075h
    db  078h, 02eh, 061h, 06dh, 064h, 036h, 034h, 02fh, 072h, 065h, 06ch, 065h, 061h, 073h, 065h, 02fh
    db  06fh, 062h, 06ah, 02fh, 056h, 042h, 06fh, 078h, 056h, 067h, 061h, 042h, 069h, 06fh, 073h, 032h
    db  038h, 036h, 02fh, 056h, 042h, 06fh, 078h, 056h, 067h, 061h, 042h, 069h, 06fh, 073h, 032h, 038h
    db  036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 0b8h
