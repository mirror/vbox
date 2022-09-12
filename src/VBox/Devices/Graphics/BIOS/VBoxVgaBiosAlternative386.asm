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





section VGAROM progbits vstart=0x0 align=1 ; size=0x907 class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x907 -> off=0x28 cb=0000000000000548 uValue=00000000000c0028 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0ebh, 01dh, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h, 00eh, 01fh, 0fch, 0e9h, 03ch, 00ah
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
    call 008f3h                               ; e8 14 08                    ; 0xc00dc vgarom.asm:197
    jmp short 000edh                          ; eb 0c                       ; 0xc00df vgarom.asm:198
    push ES                                   ; 06                          ; 0xc00e1 vgarom.asm:202
    push DS                                   ; 1e                          ; 0xc00e2 vgarom.asm:203
    pushaw                                    ; 60                          ; 0xc00e3 vgarom.asm:107
    push CS                                   ; 0e                          ; 0xc00e4 vgarom.asm:207
    pop DS                                    ; 1f                          ; 0xc00e5 vgarom.asm:208
    cld                                       ; fc                          ; 0xc00e6 vgarom.asm:209
    call 03765h                               ; e8 7b 36                    ; 0xc00e7 vgarom.asm:210
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
  ; disGetNextSymbol 0xc0570 LB 0x397 -> off=0x0 cb=0000000000000007 uValue=00000000000c0570 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0570 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0570 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0572 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0573 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0575 vberom.asm:72
    retn                                      ; c3                          ; 0xc0576 vberom.asm:73
  ; disGetNextSymbol 0xc0577 LB 0x390 -> off=0x0 cb=0000000000000040 uValue=00000000000c0577 'do_in_ax_dx'
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
  ; disGetNextSymbol 0xc05b7 LB 0x350 -> off=0x0 cb=0000000000000026 uValue=00000000000c05b7 '_dispi_get_max_bpp'
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
  ; disGetNextSymbol 0xc05dd LB 0x32a -> off=0x0 cb=0000000000000026 uValue=00000000000c05dd 'dispi_set_enable_'
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
  ; disGetNextSymbol 0xc0603 LB 0x304 -> off=0x0 cb=0000000000000026 uValue=00000000000c0603 'dispi_set_bank_'
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
  ; disGetNextSymbol 0xc0629 LB 0x2de -> off=0x0 cb=00000000000000a9 uValue=00000000000c0629 '_dispi_set_bank_farcall'
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
  ; disGetNextSymbol 0xc06d2 LB 0x235 -> off=0x0 cb=00000000000000ed uValue=00000000000c06d2 '_vga_compat_setup'
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
  ; disGetNextSymbol 0xc07bf LB 0x148 -> off=0x0 cb=0000000000000013 uValue=00000000000c07bf '_vbe_has_vbe_display'
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
  ; disGetNextSymbol 0xc07d2 LB 0x135 -> off=0x0 cb=0000000000000025 uValue=00000000000c07d2 'vbe_biosfn_return_current_mode'
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
  ; disGetNextSymbol 0xc07f7 LB 0x110 -> off=0x0 cb=000000000000002d uValue=00000000000c07f7 'vbe_biosfn_display_window_control'
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
  ; disGetNextSymbol 0xc0824 LB 0xe3 -> off=0x0 cb=0000000000000034 uValue=00000000000c0824 'vbe_biosfn_set_get_display_start'
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
  ; disGetNextSymbol 0xc0858 LB 0xaf -> off=0x0 cb=0000000000000037 uValue=00000000000c0858 'vbe_biosfn_set_get_dac_palette_format'
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
  ; disGetNextSymbol 0xc088f LB 0x78 -> off=0x0 cb=0000000000000064 uValue=00000000000c088f 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc088f LB 0x64
    test bl, bl                               ; 84 db                       ; 0xc088f vberom.asm:683
    je short 008a2h                           ; 74 0f                       ; 0xc0891 vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0893 vberom.asm:685
    je short 008cah                           ; 74 32                       ; 0xc0896 vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0898 vberom.asm:687
    jbe short 008efh                          ; 76 52                       ; 0xc089b vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc089d vberom.asm:689
    jne short 008ebh                          ; 75 49                       ; 0xc08a0 vberom.asm:690
    pushad                                    ; 66 60                       ; 0xc08a2 vberom.asm:141
    push DS                                   ; 1e                          ; 0xc08a4 vberom.asm:696
    push ES                                   ; 06                          ; 0xc08a5 vberom.asm:697
    pop DS                                    ; 1f                          ; 0xc08a6 vberom.asm:698
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08a7 vberom.asm:699
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc08a9 vberom.asm:700
    out DX, AL                                ; ee                          ; 0xc08ac vberom.asm:701
    inc dx                                    ; 42                          ; 0xc08ad vberom.asm:702
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7                     ; 0xc08ae vberom.asm:703
    lodsd                                     ; 66 ad                       ; 0xc08b0 vberom.asm:706
    ror eax, 010h                             ; 66 c1 c8 10                 ; 0xc08b2 vberom.asm:707
    out DX, AL                                ; ee                          ; 0xc08b6 vberom.asm:708
    rol eax, 008h                             ; 66 c1 c0 08                 ; 0xc08b7 vberom.asm:709
    out DX, AL                                ; ee                          ; 0xc08bb vberom.asm:710
    rol eax, 008h                             ; 66 c1 c0 08                 ; 0xc08bc vberom.asm:711
    out DX, AL                                ; ee                          ; 0xc08c0 vberom.asm:712
    loop 008b0h                               ; e2 ed                       ; 0xc08c1 vberom.asm:723
    pop DS                                    ; 1f                          ; 0xc08c3 vberom.asm:724
    popad                                     ; 66 61                       ; 0xc08c4 vberom.asm:160
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08c6 vberom.asm:727
    retn                                      ; c3                          ; 0xc08c9 vberom.asm:728
    pushad                                    ; 66 60                       ; 0xc08ca vberom.asm:141
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08cc vberom.asm:732
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc08ce vberom.asm:733
    out DX, AL                                ; ee                          ; 0xc08d1 vberom.asm:734
    add dl, 002h                              ; 80 c2 02                    ; 0xc08d2 vberom.asm:735
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0                  ; 0xc08d5 vberom.asm:738
    in AL, DX                                 ; ec                          ; 0xc08d8 vberom.asm:739
    sal eax, 008h                             ; 66 c1 e0 08                 ; 0xc08d9 vberom.asm:740
    in AL, DX                                 ; ec                          ; 0xc08dd vberom.asm:741
    sal eax, 008h                             ; 66 c1 e0 08                 ; 0xc08de vberom.asm:742
    in AL, DX                                 ; ec                          ; 0xc08e2 vberom.asm:743
    stosd                                     ; 66 ab                       ; 0xc08e3 vberom.asm:744
    loop 008d5h                               ; e2 ee                       ; 0xc08e5 vberom.asm:757
    popad                                     ; 66 61                       ; 0xc08e7 vberom.asm:160
    jmp short 008c6h                          ; eb db                       ; 0xc08e9 vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08eb vberom.asm:762
    retn                                      ; c3                          ; 0xc08ee vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc08ef vberom.asm:765
    retn                                      ; c3                          ; 0xc08f2 vberom.asm:766
  ; disGetNextSymbol 0xc08f3 LB 0x14 -> off=0x0 cb=0000000000000014 uValue=00000000000c08f3 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc08f3 LB 0x14
    test bl, bl                               ; 84 db                       ; 0xc08f3 vberom.asm:780
    jne short 00903h                          ; 75 0c                       ; 0xc08f5 vberom.asm:781
    push CS                                   ; 0e                          ; 0xc08f7 vberom.asm:782
    pop ES                                    ; 07                          ; 0xc08f8 vberom.asm:783
    mov di, 04640h                            ; bf 40 46                    ; 0xc08f9 vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc08fc vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08ff vberom.asm:786
    retn                                      ; c3                          ; 0xc0902 vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0903 vberom.asm:789
    retn                                      ; c3                          ; 0xc0906 vberom.asm:790

  ; Padding 0xe9 bytes at 0xc0907
  times 233 db 0

section _TEXT progbits vstart=0x9f0 align=1 ; size=0x396a class=CODE group=AUTO
  ; disGetNextSymbol 0xc09f0 LB 0x396a -> off=0x0 cb=000000000000001a uValue=00000000000c09f0 'set_int_vector'
set_int_vector:                              ; 0xc09f0 LB 0x1a
    push dx                                   ; 52                          ; 0xc09f0 vgabios.c:88
    push bp                                   ; 55                          ; 0xc09f1
    mov bp, sp                                ; 89 e5                       ; 0xc09f2
    mov dx, bx                                ; 89 da                       ; 0xc09f4
    movzx bx, al                              ; 0f b6 d8                    ; 0xc09f6 vgabios.c:92
    sal bx, 002h                              ; c1 e3 02                    ; 0xc09f9
    xor ax, ax                                ; 31 c0                       ; 0xc09fc
    mov es, ax                                ; 8e c0                       ; 0xc09fe
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0a00
    mov word [es:bx+002h], cx                 ; 26 89 4f 02                 ; 0xc0a03
    pop bp                                    ; 5d                          ; 0xc0a07 vgabios.c:93
    pop dx                                    ; 5a                          ; 0xc0a08
    retn                                      ; c3                          ; 0xc0a09
  ; disGetNextSymbol 0xc0a0a LB 0x3950 -> off=0x0 cb=000000000000001c uValue=00000000000c0a0a 'init_vga_card'
init_vga_card:                               ; 0xc0a0a LB 0x1c
    push bp                                   ; 55                          ; 0xc0a0a vgabios.c:144
    mov bp, sp                                ; 89 e5                       ; 0xc0a0b
    push dx                                   ; 52                          ; 0xc0a0d
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc0a0e vgabios.c:147
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc0a10
    out DX, AL                                ; ee                          ; 0xc0a13
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc0a14 vgabios.c:150
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0a16
    out DX, AL                                ; ee                          ; 0xc0a19
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc0a1a vgabios.c:151
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc0a1c
    out DX, AL                                ; ee                          ; 0xc0a1f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc0a20 vgabios.c:156
    pop dx                                    ; 5a                          ; 0xc0a23
    pop bp                                    ; 5d                          ; 0xc0a24
    retn                                      ; c3                          ; 0xc0a25
  ; disGetNextSymbol 0xc0a26 LB 0x3934 -> off=0x0 cb=000000000000003e uValue=00000000000c0a26 'init_bios_area'
init_bios_area:                              ; 0xc0a26 LB 0x3e
    push bx                                   ; 53                          ; 0xc0a26 vgabios.c:222
    push bp                                   ; 55                          ; 0xc0a27
    mov bp, sp                                ; 89 e5                       ; 0xc0a28
    xor bx, bx                                ; 31 db                       ; 0xc0a2a vgabios.c:226
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0a2c
    mov es, ax                                ; 8e c0                       ; 0xc0a2f
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc0a31 vgabios.c:229
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc0a35
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc0a37
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc0a39
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc0a3d vgabios.c:233
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc0a43 vgabios.c:235
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc0a4a vgabios.c:239
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc0a50 vgabios.c:241
    mov word [es:bx+000a8h], 05556h           ; 26 c7 87 a8 00 56 55        ; 0xc0a55 vgabios.c:243
    mov [es:bx+000aah], ds                    ; 26 8c 9f aa 00              ; 0xc0a5c
    pop bp                                    ; 5d                          ; 0xc0a61 vgabios.c:244
    pop bx                                    ; 5b                          ; 0xc0a62
    retn                                      ; c3                          ; 0xc0a63
  ; disGetNextSymbol 0xc0a64 LB 0x38f6 -> off=0x0 cb=000000000000002f uValue=00000000000c0a64 'vgabios_init_func'
vgabios_init_func:                           ; 0xc0a64 LB 0x2f
    push bp                                   ; 55                          ; 0xc0a64 vgabios.c:251
    mov bp, sp                                ; 89 e5                       ; 0xc0a65
    call 00a0ah                               ; e8 a0 ff                    ; 0xc0a67 vgabios.c:253
    call 00a26h                               ; e8 b9 ff                    ; 0xc0a6a vgabios.c:254
    call 03cfdh                               ; e8 8d 32                    ; 0xc0a6d vgabios.c:256
    mov bx, strict word 00028h                ; bb 28 00                    ; 0xc0a70 vgabios.c:258
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a73
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc0a76
    call 009f0h                               ; e8 74 ff                    ; 0xc0a79
    mov bx, strict word 00028h                ; bb 28 00                    ; 0xc0a7c vgabios.c:259
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a7f
    mov ax, strict word 0006dh                ; b8 6d 00                    ; 0xc0a82
    call 009f0h                               ; e8 68 ff                    ; 0xc0a85
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a88 vgabios.c:285
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a8b
    int 010h                                  ; cd 10                       ; 0xc0a8d
    mov sp, bp                                ; 89 ec                       ; 0xc0a8f vgabios.c:288
    pop bp                                    ; 5d                          ; 0xc0a91
    retf                                      ; cb                          ; 0xc0a92
  ; disGetNextSymbol 0xc0a93 LB 0x38c7 -> off=0x0 cb=000000000000003f uValue=00000000000c0a93 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a93 LB 0x3f
    push si                                   ; 56                          ; 0xc0a93 vgabios.c:357
    push di                                   ; 57                          ; 0xc0a94
    push bp                                   ; 55                          ; 0xc0a95
    mov bp, sp                                ; 89 e5                       ; 0xc0a96
    mov si, dx                                ; 89 d6                       ; 0xc0a98
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a9a vgabios.c:359
    jbe short 00aach                          ; 76 0e                       ; 0xc0a9c
    push SS                                   ; 16                          ; 0xc0a9e vgabios.c:360
    pop ES                                    ; 07                          ; 0xc0a9f
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0aa0
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0aa5 vgabios.c:361
    jmp short 00aceh                          ; eb 22                       ; 0xc0aaa vgabios.c:362
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0aac vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0aaf
    mov es, dx                                ; 8e c2                       ; 0xc0ab2
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0ab4
    push SS                                   ; 16                          ; 0xc0ab7 vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0ab8
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0ab9
    movzx si, al                              ; 0f b6 f0                    ; 0xc0abc vgabios.c:365
    add si, si                                ; 01 f6                       ; 0xc0abf
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0ac1
    mov es, dx                                ; 8e c2                       ; 0xc0ac4 vgabios.c:57
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0ac6
    push SS                                   ; 16                          ; 0xc0ac9 vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0aca
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc0acb
    pop bp                                    ; 5d                          ; 0xc0ace vgabios.c:367
    pop di                                    ; 5f                          ; 0xc0acf
    pop si                                    ; 5e                          ; 0xc0ad0
    retn                                      ; c3                          ; 0xc0ad1
  ; disGetNextSymbol 0xc0ad2 LB 0x3888 -> off=0x0 cb=000000000000005d uValue=00000000000c0ad2 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0ad2 LB 0x5d
    push bp                                   ; 55                          ; 0xc0ad2 vgabios.c:370
    mov bp, sp                                ; 89 e5                       ; 0xc0ad3
    push si                                   ; 56                          ; 0xc0ad5
    push di                                   ; 57                          ; 0xc0ad6
    push ax                                   ; 50                          ; 0xc0ad7
    push ax                                   ; 50                          ; 0xc0ad8
    push dx                                   ; 52                          ; 0xc0ad9
    push bx                                   ; 53                          ; 0xc0ada
    mov bl, cl                                ; 88 cb                       ; 0xc0adb
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0add vgabios.c:372
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0ae2 vgabios.c:374
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0ae5
    je short 00b23h                           ; 74 38                       ; 0xc0ae9
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc0aeb vgabios.c:375
    mov dx, ss                                ; 8c d2                       ; 0xc0aef
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0af1
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0af4
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0af7
    push DS                                   ; 1e                          ; 0xc0afa
    mov ds, dx                                ; 8e da                       ; 0xc0afb
    rep cmpsb                                 ; f3 a6                       ; 0xc0afd
    pop DS                                    ; 1f                          ; 0xc0aff
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0b00
    je near 00b09h                            ; 0f 84 02 00                 ; 0xc0b03
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0b07
    test ax, ax                               ; 85 c0                       ; 0xc0b09
    jne short 00b18h                          ; 75 0b                       ; 0xc0b0b
    movzx ax, bl                              ; 0f b6 c3                    ; 0xc0b0d vgabios.c:376
    or ah, 080h                               ; 80 cc 80                    ; 0xc0b10
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0b13
    jmp short 00b23h                          ; eb 0b                       ; 0xc0b16 vgabios.c:377
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc0b18 vgabios.c:379
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0b1c
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0b1f vgabios.c:380
    jmp short 00ae2h                          ; eb bf                       ; 0xc0b21 vgabios.c:381
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0b23 vgabios.c:383
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0b26
    pop di                                    ; 5f                          ; 0xc0b29
    pop si                                    ; 5e                          ; 0xc0b2a
    pop bp                                    ; 5d                          ; 0xc0b2b
    retn 00004h                               ; c2 04 00                    ; 0xc0b2c
  ; disGetNextSymbol 0xc0b2f LB 0x382b -> off=0x0 cb=0000000000000046 uValue=00000000000c0b2f 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0b2f LB 0x46
    push bp                                   ; 55                          ; 0xc0b2f vgabios.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc0b30
    push si                                   ; 56                          ; 0xc0b32
    push di                                   ; 57                          ; 0xc0b33
    push ax                                   ; 50                          ; 0xc0b34
    push ax                                   ; 50                          ; 0xc0b35
    mov si, ax                                ; 89 c6                       ; 0xc0b36
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0b38
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0b3b
    mov bx, cx                                ; 89 cb                       ; 0xc0b3e
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0b40 vgabios.c:392
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0b43
    out DX, ax                                ; ef                          ; 0xc0b46
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0b47 vgabios.c:394
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0b4a
    je short 00b65h                           ; 74 15                       ; 0xc0b4e
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0b50 vgabios.c:395
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0b53
    not al                                    ; f6 d0                       ; 0xc0b56
    mov di, bx                                ; 89 df                       ; 0xc0b58
    inc bx                                    ; 43                          ; 0xc0b5a
    push SS                                   ; 16                          ; 0xc0b5b
    pop ES                                    ; 07                          ; 0xc0b5c
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0b5d
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0b60 vgabios.c:396
    jmp short 00b47h                          ; eb e2                       ; 0xc0b63 vgabios.c:397
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0b65 vgabios.c:400
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0b68
    out DX, ax                                ; ef                          ; 0xc0b6b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0b6c vgabios.c:401
    pop di                                    ; 5f                          ; 0xc0b6f
    pop si                                    ; 5e                          ; 0xc0b70
    pop bp                                    ; 5d                          ; 0xc0b71
    retn 00002h                               ; c2 02 00                    ; 0xc0b72
  ; disGetNextSymbol 0xc0b75 LB 0x37e5 -> off=0x0 cb=000000000000002a uValue=00000000000c0b75 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0b75 LB 0x2a
    push bp                                   ; 55                          ; 0xc0b75 vgabios.c:403
    mov bp, sp                                ; 89 e5                       ; 0xc0b76
    xor dh, dh                                ; 30 f6                       ; 0xc0b78 vgabios.c:407
    imul bx, dx                               ; 0f af da                    ; 0xc0b7a
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc0b7d
    imul bx, dx                               ; 0f af da                    ; 0xc0b81
    xor ah, ah                                ; 30 e4                       ; 0xc0b84
    add ax, bx                                ; 01 d8                       ; 0xc0b86
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc0b88 vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0b8b
    mov es, dx                                ; 8e c2                       ; 0xc0b8e
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0b90
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc0b93 vgabios.c:58
    imul dx, bx                               ; 0f af d3                    ; 0xc0b96
    add ax, dx                                ; 01 d0                       ; 0xc0b99
    pop bp                                    ; 5d                          ; 0xc0b9b vgabios.c:411
    retn 00002h                               ; c2 02 00                    ; 0xc0b9c
  ; disGetNextSymbol 0xc0b9f LB 0x37bb -> off=0x0 cb=000000000000003e uValue=00000000000c0b9f 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0b9f LB 0x3e
    push bp                                   ; 55                          ; 0xc0b9f vgabios.c:413
    mov bp, sp                                ; 89 e5                       ; 0xc0ba0
    push cx                                   ; 51                          ; 0xc0ba2
    push si                                   ; 56                          ; 0xc0ba3
    push di                                   ; 57                          ; 0xc0ba4
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc0ba5
    mov si, ax                                ; 89 c6                       ; 0xc0ba8
    mov ax, dx                                ; 89 d0                       ; 0xc0baa
    movzx di, bl                              ; 0f b6 fb                    ; 0xc0bac vgabios.c:417
    push di                                   ; 57                          ; 0xc0baf
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0bb0
    mov bx, si                                ; 89 f3                       ; 0xc0bb3
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0bb5
    call 00b2fh                               ; e8 74 ff                    ; 0xc0bb8
    push di                                   ; 57                          ; 0xc0bbb vgabios.c:420
    push 00100h                               ; 68 00 01                    ; 0xc0bbc
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0bbf vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0bc2
    mov es, ax                                ; 8e c0                       ; 0xc0bc4
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0bc6
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0bc9
    xor cx, cx                                ; 31 c9                       ; 0xc0bcd vgabios.c:68
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0bcf
    call 00ad2h                               ; e8 fd fe                    ; 0xc0bd2
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0bd5 vgabios.c:421
    pop di                                    ; 5f                          ; 0xc0bd8
    pop si                                    ; 5e                          ; 0xc0bd9
    pop cx                                    ; 59                          ; 0xc0bda
    pop bp                                    ; 5d                          ; 0xc0bdb
    retn                                      ; c3                          ; 0xc0bdc
  ; disGetNextSymbol 0xc0bdd LB 0x377d -> off=0x0 cb=000000000000001a uValue=00000000000c0bdd 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0bdd LB 0x1a
    push bp                                   ; 55                          ; 0xc0bdd vgabios.c:423
    mov bp, sp                                ; 89 e5                       ; 0xc0bde
    xor dh, dh                                ; 30 f6                       ; 0xc0be0 vgabios.c:427
    imul dx, bx                               ; 0f af d3                    ; 0xc0be2
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc0be5
    imul bx, dx                               ; 0f af da                    ; 0xc0be9
    xor ah, ah                                ; 30 e4                       ; 0xc0bec
    add ax, bx                                ; 01 d8                       ; 0xc0bee
    sal ax, 003h                              ; c1 e0 03                    ; 0xc0bf0 vgabios.c:428
    pop bp                                    ; 5d                          ; 0xc0bf3 vgabios.c:430
    retn 00002h                               ; c2 02 00                    ; 0xc0bf4
  ; disGetNextSymbol 0xc0bf7 LB 0x3763 -> off=0x0 cb=000000000000004b uValue=00000000000c0bf7 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0bf7 LB 0x4b
    push si                                   ; 56                          ; 0xc0bf7 vgabios.c:432
    push di                                   ; 57                          ; 0xc0bf8
    enter 00004h, 000h                        ; c8 04 00 00                 ; 0xc0bf9
    mov si, ax                                ; 89 c6                       ; 0xc0bfd
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0bff
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0c02
    mov bx, cx                                ; 89 cb                       ; 0xc0c05
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0c07 vgabios.c:438
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0c0a
    je short 00c3ch                           ; 74 2c                       ; 0xc0c0e
    xor dh, dh                                ; 30 f6                       ; 0xc0c10 vgabios.c:439
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0c12 vgabios.c:440
    xor ax, ax                                ; 31 c0                       ; 0xc0c14 vgabios.c:441
    jmp short 00c1dh                          ; eb 05                       ; 0xc0c16
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0c18
    jnl short 00c31h                          ; 7d 14                       ; 0xc0c1b
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0c1d vgabios.c:442
    mov di, si                                ; 89 f7                       ; 0xc0c20
    add di, ax                                ; 01 c7                       ; 0xc0c22
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0c24
    je short 00c2ch                           ; 74 02                       ; 0xc0c28
    or dh, dl                                 ; 08 d6                       ; 0xc0c2a vgabios.c:443
    shr dl, 1                                 ; d0 ea                       ; 0xc0c2c vgabios.c:444
    inc ax                                    ; 40                          ; 0xc0c2e vgabios.c:445
    jmp short 00c18h                          ; eb e7                       ; 0xc0c2f
    mov di, bx                                ; 89 df                       ; 0xc0c31 vgabios.c:446
    inc bx                                    ; 43                          ; 0xc0c33
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0c34
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0c37 vgabios.c:447
    jmp short 00c07h                          ; eb cb                       ; 0xc0c3a vgabios.c:448
    leave                                     ; c9                          ; 0xc0c3c vgabios.c:449
    pop di                                    ; 5f                          ; 0xc0c3d
    pop si                                    ; 5e                          ; 0xc0c3e
    retn 00002h                               ; c2 02 00                    ; 0xc0c3f
  ; disGetNextSymbol 0xc0c42 LB 0x3718 -> off=0x0 cb=000000000000003f uValue=00000000000c0c42 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0c42 LB 0x3f
    push bp                                   ; 55                          ; 0xc0c42 vgabios.c:451
    mov bp, sp                                ; 89 e5                       ; 0xc0c43
    push cx                                   ; 51                          ; 0xc0c45
    push si                                   ; 56                          ; 0xc0c46
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc0c47
    mov cx, ax                                ; 89 c1                       ; 0xc0c4a
    mov ax, dx                                ; 89 d0                       ; 0xc0c4c
    movzx si, bl                              ; 0f b6 f3                    ; 0xc0c4e vgabios.c:455
    push si                                   ; 56                          ; 0xc0c51
    mov bx, cx                                ; 89 cb                       ; 0xc0c52
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0c54
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0c57
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0c5a
    call 00bf7h                               ; e8 97 ff                    ; 0xc0c5d
    push si                                   ; 56                          ; 0xc0c60 vgabios.c:458
    push 00100h                               ; 68 00 01                    ; 0xc0c61
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0c64 vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0c67
    mov es, ax                                ; 8e c0                       ; 0xc0c69
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c6b
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0c6e
    xor cx, cx                                ; 31 c9                       ; 0xc0c72 vgabios.c:68
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0c74
    call 00ad2h                               ; e8 58 fe                    ; 0xc0c77
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c7a vgabios.c:459
    pop si                                    ; 5e                          ; 0xc0c7d
    pop cx                                    ; 59                          ; 0xc0c7e
    pop bp                                    ; 5d                          ; 0xc0c7f
    retn                                      ; c3                          ; 0xc0c80
  ; disGetNextSymbol 0xc0c81 LB 0x36d9 -> off=0x0 cb=0000000000000035 uValue=00000000000c0c81 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0c81 LB 0x35
    push bp                                   ; 55                          ; 0xc0c81 vgabios.c:461
    mov bp, sp                                ; 89 e5                       ; 0xc0c82
    push bx                                   ; 53                          ; 0xc0c84
    push cx                                   ; 51                          ; 0xc0c85
    mov bx, ax                                ; 89 c3                       ; 0xc0c86
    mov es, dx                                ; 8e c2                       ; 0xc0c88
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0c8a vgabios.c:467
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0c8d vgabios.c:468
    xor dl, dl                                ; 30 d2                       ; 0xc0c8f vgabios.c:469
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c91 vgabios.c:470
    xchg ah, al                               ; 86 c4                       ; 0xc0c94
    xor bx, bx                                ; 31 db                       ; 0xc0c96 vgabios.c:472
    jmp short 00c9fh                          ; eb 05                       ; 0xc0c98
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0c9a
    jnl short 00cadh                          ; 7d 0e                       ; 0xc0c9d
    test ax, cx                               ; 85 c8                       ; 0xc0c9f vgabios.c:473
    je short 00ca5h                           ; 74 02                       ; 0xc0ca1
    or dl, dh                                 ; 08 f2                       ; 0xc0ca3 vgabios.c:474
    shr dh, 1                                 ; d0 ee                       ; 0xc0ca5 vgabios.c:475
    shr cx, 002h                              ; c1 e9 02                    ; 0xc0ca7 vgabios.c:476
    inc bx                                    ; 43                          ; 0xc0caa vgabios.c:477
    jmp short 00c9ah                          ; eb ed                       ; 0xc0cab
    mov al, dl                                ; 88 d0                       ; 0xc0cad vgabios.c:479
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0caf
    pop cx                                    ; 59                          ; 0xc0cb2
    pop bx                                    ; 5b                          ; 0xc0cb3
    pop bp                                    ; 5d                          ; 0xc0cb4
    retn                                      ; c3                          ; 0xc0cb5
  ; disGetNextSymbol 0xc0cb6 LB 0x36a4 -> off=0x0 cb=0000000000000084 uValue=00000000000c0cb6 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0cb6 LB 0x84
    push bp                                   ; 55                          ; 0xc0cb6 vgabios.c:481
    mov bp, sp                                ; 89 e5                       ; 0xc0cb7
    push cx                                   ; 51                          ; 0xc0cb9
    push si                                   ; 56                          ; 0xc0cba
    push di                                   ; 57                          ; 0xc0cbb
    push ax                                   ; 50                          ; 0xc0cbc
    mov si, dx                                ; 89 d6                       ; 0xc0cbd
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0cbf vgabios.c:489
    je short 00cfeh                           ; 74 3a                       ; 0xc0cc2
    mov bx, ax                                ; 89 c3                       ; 0xc0cc4 vgabios.c:491
    add bx, ax                                ; 01 c3                       ; 0xc0cc6
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0cc8
    xor cx, cx                                ; 31 c9                       ; 0xc0ccd vgabios.c:493
    jmp short 00cd6h                          ; eb 05                       ; 0xc0ccf
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0cd1
    jnl short 00d32h                          ; 7d 5c                       ; 0xc0cd4
    mov ax, bx                                ; 89 d8                       ; 0xc0cd6 vgabios.c:494
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0cd8
    call 00c81h                               ; e8 a3 ff                    ; 0xc0cdb
    mov di, si                                ; 89 f7                       ; 0xc0cde
    inc si                                    ; 46                          ; 0xc0ce0
    push SS                                   ; 16                          ; 0xc0ce1
    pop ES                                    ; 07                          ; 0xc0ce2
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ce3
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0ce6 vgabios.c:495
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0cea
    call 00c81h                               ; e8 91 ff                    ; 0xc0ced
    mov di, si                                ; 89 f7                       ; 0xc0cf0
    inc si                                    ; 46                          ; 0xc0cf2
    push SS                                   ; 16                          ; 0xc0cf3
    pop ES                                    ; 07                          ; 0xc0cf4
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cf5
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0cf8 vgabios.c:496
    inc cx                                    ; 41                          ; 0xc0cfb vgabios.c:497
    jmp short 00cd1h                          ; eb d3                       ; 0xc0cfc
    mov bx, ax                                ; 89 c3                       ; 0xc0cfe vgabios.c:499
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0d00
    xor cx, cx                                ; 31 c9                       ; 0xc0d05 vgabios.c:500
    jmp short 00d0eh                          ; eb 05                       ; 0xc0d07
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0d09
    jnl short 00d32h                          ; 7d 24                       ; 0xc0d0c
    mov di, si                                ; 89 f7                       ; 0xc0d0e vgabios.c:501
    inc si                                    ; 46                          ; 0xc0d10
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0d11
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d14
    push SS                                   ; 16                          ; 0xc0d17
    pop ES                                    ; 07                          ; 0xc0d18
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d19
    mov di, si                                ; 89 f7                       ; 0xc0d1c vgabios.c:502
    inc si                                    ; 46                          ; 0xc0d1e
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0d1f
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0d22
    push SS                                   ; 16                          ; 0xc0d27
    pop ES                                    ; 07                          ; 0xc0d28
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0d29
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0d2c vgabios.c:503
    inc cx                                    ; 41                          ; 0xc0d2f vgabios.c:504
    jmp short 00d09h                          ; eb d7                       ; 0xc0d30
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0d32 vgabios.c:506
    pop di                                    ; 5f                          ; 0xc0d35
    pop si                                    ; 5e                          ; 0xc0d36
    pop cx                                    ; 59                          ; 0xc0d37
    pop bp                                    ; 5d                          ; 0xc0d38
    retn                                      ; c3                          ; 0xc0d39
  ; disGetNextSymbol 0xc0d3a LB 0x3620 -> off=0x0 cb=0000000000000011 uValue=00000000000c0d3a 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0d3a LB 0x11
    push bp                                   ; 55                          ; 0xc0d3a vgabios.c:508
    mov bp, sp                                ; 89 e5                       ; 0xc0d3b
    xor dh, dh                                ; 30 f6                       ; 0xc0d3d vgabios.c:513
    imul dx, bx                               ; 0f af d3                    ; 0xc0d3f
    sal dx, 002h                              ; c1 e2 02                    ; 0xc0d42
    xor ah, ah                                ; 30 e4                       ; 0xc0d45
    add ax, dx                                ; 01 d0                       ; 0xc0d47
    pop bp                                    ; 5d                          ; 0xc0d49 vgabios.c:514
    retn                                      ; c3                          ; 0xc0d4a
  ; disGetNextSymbol 0xc0d4b LB 0x360f -> off=0x0 cb=0000000000000065 uValue=00000000000c0d4b 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0d4b LB 0x65
    push bp                                   ; 55                          ; 0xc0d4b vgabios.c:516
    mov bp, sp                                ; 89 e5                       ; 0xc0d4c
    push bx                                   ; 53                          ; 0xc0d4e
    push cx                                   ; 51                          ; 0xc0d4f
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0d50
    movzx bx, dl                              ; 0f b6 da                    ; 0xc0d53 vgabios.c:522
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0d56
    call 00cb6h                               ; e8 5a ff                    ; 0xc0d59
    push strict byte 00008h                   ; 6a 08                       ; 0xc0d5c vgabios.c:525
    push 00080h                               ; 68 80 00                    ; 0xc0d5e
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0d61 vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0d64
    mov es, ax                                ; 8e c0                       ; 0xc0d66
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d68
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d6b
    xor cx, cx                                ; 31 c9                       ; 0xc0d6f vgabios.c:68
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d71
    call 00ad2h                               ; e8 5b fd                    ; 0xc0d74
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d77
    test ah, 080h                             ; f6 c4 80                    ; 0xc0d7a vgabios.c:527
    jne short 00da6h                          ; 75 27                       ; 0xc0d7d
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0d7f vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0d82
    mov es, ax                                ; 8e c0                       ; 0xc0d84
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d86
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d89
    test dx, dx                               ; 85 d2                       ; 0xc0d8d vgabios.c:531
    jne short 00d95h                          ; 75 04                       ; 0xc0d8f
    test ax, ax                               ; 85 c0                       ; 0xc0d91
    je short 00da6h                           ; 74 11                       ; 0xc0d93
    push strict byte 00008h                   ; 6a 08                       ; 0xc0d95 vgabios.c:532
    push 00080h                               ; 68 80 00                    ; 0xc0d97
    mov cx, 00080h                            ; b9 80 00                    ; 0xc0d9a
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d9d
    call 00ad2h                               ; e8 2f fd                    ; 0xc0da0
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0da3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0da6 vgabios.c:535
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0da9
    pop cx                                    ; 59                          ; 0xc0dac
    pop bx                                    ; 5b                          ; 0xc0dad
    pop bp                                    ; 5d                          ; 0xc0dae
    retn                                      ; c3                          ; 0xc0daf
  ; disGetNextSymbol 0xc0db0 LB 0x35aa -> off=0x0 cb=0000000000000127 uValue=00000000000c0db0 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0db0 LB 0x127
    push bp                                   ; 55                          ; 0xc0db0 vgabios.c:537
    mov bp, sp                                ; 89 e5                       ; 0xc0db1
    push bx                                   ; 53                          ; 0xc0db3
    push cx                                   ; 51                          ; 0xc0db4
    push si                                   ; 56                          ; 0xc0db5
    push di                                   ; 57                          ; 0xc0db6
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0db7
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0dba
    mov si, dx                                ; 89 d6                       ; 0xc0dbd
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0dbf vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0dc2
    mov es, ax                                ; 8e c0                       ; 0xc0dc5
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0dc7
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0dca vgabios.c:48
    xor ah, ah                                ; 30 e4                       ; 0xc0dcd vgabios.c:545
    call 036a6h                               ; e8 d4 28                    ; 0xc0dcf
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0dd2
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0dd5 vgabios.c:546
    je near 00eceh                            ; 0f 84 f3 00                 ; 0xc0dd7
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc0ddb vgabios.c:550
    lea bx, [bp-018h]                         ; 8d 5e e8                    ; 0xc0ddf
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc0de2
    mov ax, cx                                ; 89 c8                       ; 0xc0de5
    call 00a93h                               ; e8 a9 fc                    ; 0xc0de7
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc0dea vgabios.c:551
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0ded
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc0df0 vgabios.c:552
    xor al, al                                ; 30 c0                       ; 0xc0df3
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0df5
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0df8
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0dfb vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0dfe
    mov es, dx                                ; 8e c2                       ; 0xc0e01
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc0e03
    xor dh, dh                                ; 30 f6                       ; 0xc0e06 vgabios.c:48
    inc dx                                    ; 42                          ; 0xc0e08
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0e09 vgabios.c:57
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0e0c
    mov word [bp-014h], di                    ; 89 7e ec                    ; 0xc0e0f vgabios.c:58
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc0e12 vgabios.c:558
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0e16
    cmp byte [bx+047b5h], 000h                ; 80 bf b5 47 00              ; 0xc0e19
    jne short 00e56h                          ; 75 36                       ; 0xc0e1e
    imul dx, di                               ; 0f af d7                    ; 0xc0e20 vgabios.c:560
    add dx, dx                                ; 01 d2                       ; 0xc0e23
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc0e25
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0e28
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc0e2b
    mov cx, word [bp-016h]                    ; 8b 4e ea                    ; 0xc0e2f
    inc cx                                    ; 41                          ; 0xc0e32
    imul dx, cx                               ; 0f af d1                    ; 0xc0e33
    xor ah, ah                                ; 30 e4                       ; 0xc0e36
    imul di, ax                               ; 0f af f8                    ; 0xc0e38
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0e3b
    add ax, di                                ; 01 f8                       ; 0xc0e3f
    add ax, ax                                ; 01 c0                       ; 0xc0e41
    mov di, dx                                ; 89 d7                       ; 0xc0e43
    add di, ax                                ; 01 c7                       ; 0xc0e45
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc0e47 vgabios.c:55
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0e4b
    push SS                                   ; 16                          ; 0xc0e4e vgabios.c:58
    pop ES                                    ; 07                          ; 0xc0e4f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0e50
    jmp near 00eceh                           ; e9 78 00                    ; 0xc0e53 vgabios.c:562
    mov bl, byte [bx+047b6h]                  ; 8a 9f b6 47                 ; 0xc0e56 vgabios.c:563
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0e5a
    je short 00eaah                           ; 74 4b                       ; 0xc0e5d
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0e5f
    jc short 00eceh                           ; 72 6a                       ; 0xc0e62
    jbe short 00e6dh                          ; 76 07                       ; 0xc0e64
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0e66
    jbe short 00e86h                          ; 76 1b                       ; 0xc0e69
    jmp short 00eceh                          ; eb 61                       ; 0xc0e6b
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc0e6d vgabios.c:566
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0e71
    mov bx, word [bp-014h]                    ; 8b 5e ec                    ; 0xc0e75
    call 00d3ah                               ; e8 bf fe                    ; 0xc0e78
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc0e7b vgabios.c:567
    call 00d4bh                               ; e8 c9 fe                    ; 0xc0e7f
    xor ah, ah                                ; 30 e4                       ; 0xc0e82
    jmp short 00e4eh                          ; eb c8                       ; 0xc0e84
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e86 vgabios.c:57
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0e89
    xor dh, dh                                ; 30 f6                       ; 0xc0e8c vgabios.c:572
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0e8e
    push dx                                   ; 52                          ; 0xc0e91
    movzx dx, al                              ; 0f b6 d0                    ; 0xc0e92
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0e95
    mov bx, di                                ; 89 fb                       ; 0xc0e99
    call 00b75h                               ; e8 d7 fc                    ; 0xc0e9b
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0e9e vgabios.c:573
    mov dx, ax                                ; 89 c2                       ; 0xc0ea1
    mov ax, di                                ; 89 f8                       ; 0xc0ea3
    call 00b9fh                               ; e8 f7 fc                    ; 0xc0ea5
    jmp short 00e82h                          ; eb d8                       ; 0xc0ea8
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0eaa vgabios.c:57
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0ead
    xor dh, dh                                ; 30 f6                       ; 0xc0eb0 vgabios.c:577
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0eb2
    push dx                                   ; 52                          ; 0xc0eb5
    movzx dx, al                              ; 0f b6 d0                    ; 0xc0eb6
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0eb9
    mov bx, di                                ; 89 fb                       ; 0xc0ebd
    call 00bddh                               ; e8 1b fd                    ; 0xc0ebf
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0ec2 vgabios.c:578
    mov dx, ax                                ; 89 c2                       ; 0xc0ec5
    mov ax, di                                ; 89 f8                       ; 0xc0ec7
    call 00c42h                               ; e8 76 fd                    ; 0xc0ec9
    jmp short 00e82h                          ; eb b4                       ; 0xc0ecc
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0ece vgabios.c:587
    pop di                                    ; 5f                          ; 0xc0ed1
    pop si                                    ; 5e                          ; 0xc0ed2
    pop cx                                    ; 59                          ; 0xc0ed3
    pop bx                                    ; 5b                          ; 0xc0ed4
    pop bp                                    ; 5d                          ; 0xc0ed5
    retn                                      ; c3                          ; 0xc0ed6
  ; disGetNextSymbol 0xc0ed7 LB 0x3483 -> off=0x10 cb=0000000000000083 uValue=00000000000c0ee7 'vga_get_font_info'
    db  0feh, 00eh, 043h, 00fh, 048h, 00fh, 04fh, 00fh, 054h, 00fh, 059h, 00fh, 05eh, 00fh, 063h, 00fh
vga_get_font_info:                           ; 0xc0ee7 LB 0x83
    push si                                   ; 56                          ; 0xc0ee7 vgabios.c:589
    push di                                   ; 57                          ; 0xc0ee8
    push bp                                   ; 55                          ; 0xc0ee9
    mov bp, sp                                ; 89 e5                       ; 0xc0eea
    mov di, dx                                ; 89 d7                       ; 0xc0eec
    mov si, bx                                ; 89 de                       ; 0xc0eee
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0ef0 vgabios.c:594
    jnbe short 00f3dh                         ; 77 48                       ; 0xc0ef3
    mov bx, ax                                ; 89 c3                       ; 0xc0ef5
    add bx, ax                                ; 01 c3                       ; 0xc0ef7
    jmp word [cs:bx+00ed7h]                   ; 2e ff a7 d7 0e              ; 0xc0ef9
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0efe vgabios.c:67
    xor ax, ax                                ; 31 c0                       ; 0xc0f01
    mov es, ax                                ; 8e c0                       ; 0xc0f03
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0f05
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02                 ; 0xc0f08
    push SS                                   ; 16                          ; 0xc0f0c vgabios.c:597
    pop ES                                    ; 07                          ; 0xc0f0d
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc0f0e
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc0f11
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0f14
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f17
    mov es, ax                                ; 8e c0                       ; 0xc0f1a
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f1c
    xor ah, ah                                ; 30 e4                       ; 0xc0f1f
    push SS                                   ; 16                          ; 0xc0f21
    pop ES                                    ; 07                          ; 0xc0f22
    mov bx, cx                                ; 89 cb                       ; 0xc0f23
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f25
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0f28
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f2b
    mov es, ax                                ; 8e c0                       ; 0xc0f2e
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f30
    xor ah, ah                                ; 30 e4                       ; 0xc0f33
    push SS                                   ; 16                          ; 0xc0f35
    pop ES                                    ; 07                          ; 0xc0f36
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0f37
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0f3a
    pop bp                                    ; 5d                          ; 0xc0f3d
    pop di                                    ; 5f                          ; 0xc0f3e
    pop si                                    ; 5e                          ; 0xc0f3f
    retn 00002h                               ; c2 02 00                    ; 0xc0f40
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0f43 vgabios.c:67
    jmp short 00f01h                          ; eb b9                       ; 0xc0f46
    mov dx, 05d72h                            ; ba 72 5d                    ; 0xc0f48 vgabios.c:602
    mov ax, ds                                ; 8c d8                       ; 0xc0f4b
    jmp short 00f0ch                          ; eb bd                       ; 0xc0f4d vgabios.c:603
    mov dx, 05572h                            ; ba 72 55                    ; 0xc0f4f vgabios.c:605
    jmp short 00f4bh                          ; eb f7                       ; 0xc0f52
    mov dx, 05972h                            ; ba 72 59                    ; 0xc0f54 vgabios.c:608
    jmp short 00f4bh                          ; eb f2                       ; 0xc0f57
    mov dx, 07b72h                            ; ba 72 7b                    ; 0xc0f59 vgabios.c:611
    jmp short 00f4bh                          ; eb ed                       ; 0xc0f5c
    mov dx, 06b72h                            ; ba 72 6b                    ; 0xc0f5e vgabios.c:614
    jmp short 00f4bh                          ; eb e8                       ; 0xc0f61
    mov dx, 07c9fh                            ; ba 9f 7c                    ; 0xc0f63 vgabios.c:617
    jmp short 00f4bh                          ; eb e3                       ; 0xc0f66
    jmp short 00f3dh                          ; eb d3                       ; 0xc0f68 vgabios.c:623
  ; disGetNextSymbol 0xc0f6a LB 0x33f0 -> off=0x0 cb=0000000000000156 uValue=00000000000c0f6a 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0f6a LB 0x156
    push bp                                   ; 55                          ; 0xc0f6a vgabios.c:636
    mov bp, sp                                ; 89 e5                       ; 0xc0f6b
    push si                                   ; 56                          ; 0xc0f6d
    push di                                   ; 57                          ; 0xc0f6e
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc0f6f
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0f72
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc0f75
    mov si, cx                                ; 89 ce                       ; 0xc0f78
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0f7a vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f7d
    mov es, ax                                ; 8e c0                       ; 0xc0f80
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f82
    xor ah, ah                                ; 30 e4                       ; 0xc0f85 vgabios.c:643
    call 036a6h                               ; e8 1c 27                    ; 0xc0f87
    mov ah, al                                ; 88 c4                       ; 0xc0f8a
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f8c vgabios.c:644
    je near 010b9h                            ; 0f 84 27 01                 ; 0xc0f8e
    movzx bx, al                              ; 0f b6 d8                    ; 0xc0f92 vgabios.c:646
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0f95
    cmp byte [bx+047b5h], 000h                ; 80 bf b5 47 00              ; 0xc0f98
    je near 010b9h                            ; 0f 84 18 01                 ; 0xc0f9d
    mov ch, byte [bx+047b6h]                  ; 8a af b6 47                 ; 0xc0fa1 vgabios.c:650
    cmp ch, 003h                              ; 80 fd 03                    ; 0xc0fa5
    jc short 00fbbh                           ; 72 11                       ; 0xc0fa8
    jbe short 00fc3h                          ; 76 17                       ; 0xc0faa
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc0fac
    je near 01092h                            ; 0f 84 df 00                 ; 0xc0faf
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc0fb3
    je short 00fc3h                           ; 74 0b                       ; 0xc0fb6
    jmp near 010b2h                           ; e9 f7 00                    ; 0xc0fb8
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc0fbb
    je short 0102eh                           ; 74 6e                       ; 0xc0fbe
    jmp near 010b2h                           ; e9 ef 00                    ; 0xc0fc0
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0fc3 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0fc6
    mov es, ax                                ; 8e c0                       ; 0xc0fc9
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0fcb
    imul ax, word [bp-00ch]                   ; 0f af 46 f4                 ; 0xc0fce vgabios.c:58
    mov bx, dx                                ; 89 d3                       ; 0xc0fd2
    shr bx, 003h                              ; c1 eb 03                    ; 0xc0fd4
    add bx, ax                                ; 01 c3                       ; 0xc0fd7
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc0fd9 vgabios.c:57
    mov cx, word [es:di]                      ; 26 8b 0d                    ; 0xc0fdc
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc0fdf vgabios.c:58
    imul ax, cx                               ; 0f af c1                    ; 0xc0fe3
    add bx, ax                                ; 01 c3                       ; 0xc0fe6
    mov cl, dl                                ; 88 d1                       ; 0xc0fe8 vgabios.c:655
    and cl, 007h                              ; 80 e1 07                    ; 0xc0fea
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0fed
    sar ax, CL                                ; d3 f8                       ; 0xc0ff0
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0ff2
    xor ch, ch                                ; 30 ed                       ; 0xc0ff5 vgabios.c:656
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc0ff7 vgabios.c:657
    jmp short 01004h                          ; eb 08                       ; 0xc0ffa
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc0ffc
    jnc near 010b4h                           ; 0f 83 b0 00                 ; 0xc1000
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1004 vgabios.c:658
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1008
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc100b
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc100d
    out DX, ax                                ; ef                          ; 0xc1010
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1011 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc1014
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1016
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc1019 vgabios.c:48
    test al, al                               ; 84 c0                       ; 0xc101c vgabios.c:660
    jbe short 01029h                          ; 76 09                       ; 0xc101e
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc1020 vgabios.c:661
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc1023
    sal al, CL                                ; d2 e0                       ; 0xc1025
    or ch, al                                 ; 08 c5                       ; 0xc1027
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1029 vgabios.c:662
    jmp short 00ffch                          ; eb ce                       ; 0xc102c
    movzx cx, byte [bx+047b7h]                ; 0f b6 8f b7 47              ; 0xc102e vgabios.c:665
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc1033
    sub bx, cx                                ; 29 cb                       ; 0xc1036
    mov cx, bx                                ; 89 d9                       ; 0xc1038
    mov bx, dx                                ; 89 d3                       ; 0xc103a
    shr bx, CL                                ; d3 eb                       ; 0xc103c
    mov cx, bx                                ; 89 d9                       ; 0xc103e
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc1040
    shr bx, 1                                 ; d1 eb                       ; 0xc1043
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc1045
    add bx, cx                                ; 01 cb                       ; 0xc1048
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc104a vgabios.c:666
    je short 01053h                           ; 74 03                       ; 0xc104e
    add bh, 020h                              ; 80 c7 20                    ; 0xc1050 vgabios.c:667
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc1053 vgabios.c:47
    mov es, cx                                ; 8e c1                       ; 0xc1056
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1058
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc105b vgabios.c:669
    sal bx, 003h                              ; c1 e3 03                    ; 0xc105e
    cmp byte [bx+047b7h], 002h                ; 80 bf b7 47 02              ; 0xc1061
    jne short 0107dh                          ; 75 15                       ; 0xc1066
    and dx, strict byte 00003h                ; 83 e2 03                    ; 0xc1068 vgabios.c:670
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc106b
    sub cx, dx                                ; 29 d1                       ; 0xc106e
    add cx, cx                                ; 01 c9                       ; 0xc1070
    xor ah, ah                                ; 30 e4                       ; 0xc1072
    sar ax, CL                                ; d3 f8                       ; 0xc1074
    mov ch, al                                ; 88 c5                       ; 0xc1076
    and ch, 003h                              ; 80 e5 03                    ; 0xc1078
    jmp short 010b4h                          ; eb 37                       ; 0xc107b vgabios.c:671
    xor dh, dh                                ; 30 f6                       ; 0xc107d vgabios.c:672
    and dl, 007h                              ; 80 e2 07                    ; 0xc107f
    mov cx, strict word 00007h                ; b9 07 00                    ; 0xc1082
    sub cx, dx                                ; 29 d1                       ; 0xc1085
    xor ah, ah                                ; 30 e4                       ; 0xc1087
    sar ax, CL                                ; d3 f8                       ; 0xc1089
    mov ch, al                                ; 88 c5                       ; 0xc108b
    and ch, 001h                              ; 80 e5 01                    ; 0xc108d
    jmp short 010b4h                          ; eb 22                       ; 0xc1090 vgabios.c:673
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1092 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1095
    mov es, ax                                ; 8e c0                       ; 0xc1098
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc109a
    sal ax, 003h                              ; c1 e0 03                    ; 0xc109d vgabios.c:58
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc10a0
    imul bx, ax                               ; 0f af d8                    ; 0xc10a3
    add bx, dx                                ; 01 d3                       ; 0xc10a6
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc10a8 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc10ab
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc10ad
    jmp short 010b4h                          ; eb 02                       ; 0xc10b0 vgabios.c:677
    xor ch, ch                                ; 30 ed                       ; 0xc10b2 vgabios.c:682
    push SS                                   ; 16                          ; 0xc10b4 vgabios.c:684
    pop ES                                    ; 07                          ; 0xc10b5
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc10b6
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc10b9 vgabios.c:685
    pop di                                    ; 5f                          ; 0xc10bc
    pop si                                    ; 5e                          ; 0xc10bd
    pop bp                                    ; 5d                          ; 0xc10be
    retn                                      ; c3                          ; 0xc10bf
  ; disGetNextSymbol 0xc10c0 LB 0x329a -> off=0x0 cb=000000000000008c uValue=00000000000c10c0 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc10c0 LB 0x8c
    push bp                                   ; 55                          ; 0xc10c0 vgabios.c:690
    mov bp, sp                                ; 89 e5                       ; 0xc10c1
    push bx                                   ; 53                          ; 0xc10c3
    push cx                                   ; 51                          ; 0xc10c4
    push si                                   ; 56                          ; 0xc10c5
    push di                                   ; 57                          ; 0xc10c6
    push ax                                   ; 50                          ; 0xc10c7
    push ax                                   ; 50                          ; 0xc10c8
    mov bx, ax                                ; 89 c3                       ; 0xc10c9
    mov di, dx                                ; 89 d7                       ; 0xc10cb
    mov dx, 003dah                            ; ba da 03                    ; 0xc10cd vgabios.c:695
    in AL, DX                                 ; ec                          ; 0xc10d0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10d1
    xor al, al                                ; 30 c0                       ; 0xc10d3 vgabios.c:696
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc10d5
    out DX, AL                                ; ee                          ; 0xc10d8
    xor si, si                                ; 31 f6                       ; 0xc10d9 vgabios.c:698
    cmp si, di                                ; 39 fe                       ; 0xc10db
    jnc short 01131h                          ; 73 52                       ; 0xc10dd
    mov al, bl                                ; 88 d8                       ; 0xc10df vgabios.c:701
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc10e1
    out DX, AL                                ; ee                          ; 0xc10e4
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10e5 vgabios.c:703
    in AL, DX                                 ; ec                          ; 0xc10e8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10e9
    mov cx, ax                                ; 89 c1                       ; 0xc10eb
    in AL, DX                                 ; ec                          ; 0xc10ed vgabios.c:704
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10ee
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc10f0
    in AL, DX                                 ; ec                          ; 0xc10f3 vgabios.c:705
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10f4
    xor ch, ch                                ; 30 ed                       ; 0xc10f6 vgabios.c:708
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d                    ; 0xc10f8
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc10fb
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4                 ; 0xc10fe
    imul cx, cx, 00097h                       ; 69 c9 97 00                 ; 0xc1102
    add cx, word [bp-00ah]                    ; 03 4e f6                    ; 0xc1106
    xor ah, ah                                ; 30 e4                       ; 0xc1109
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c                    ; 0xc110b
    add cx, ax                                ; 01 c1                       ; 0xc110e
    add cx, 00080h                            ; 81 c1 80 00                 ; 0xc1110
    sar cx, 008h                              ; c1 f9 08                    ; 0xc1114
    cmp cx, strict byte 0003fh                ; 83 f9 3f                    ; 0xc1117 vgabios.c:710
    jbe short 0111fh                          ; 76 03                       ; 0xc111a
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc111c
    mov al, bl                                ; 88 d8                       ; 0xc111f vgabios.c:713
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc1121
    out DX, AL                                ; ee                          ; 0xc1124
    mov al, cl                                ; 88 c8                       ; 0xc1125 vgabios.c:715
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1127
    out DX, AL                                ; ee                          ; 0xc112a
    out DX, AL                                ; ee                          ; 0xc112b vgabios.c:716
    out DX, AL                                ; ee                          ; 0xc112c vgabios.c:717
    inc bx                                    ; 43                          ; 0xc112d vgabios.c:718
    inc si                                    ; 46                          ; 0xc112e vgabios.c:719
    jmp short 010dbh                          ; eb aa                       ; 0xc112f
    mov dx, 003dah                            ; ba da 03                    ; 0xc1131 vgabios.c:720
    in AL, DX                                 ; ec                          ; 0xc1134
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1135
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc1137 vgabios.c:721
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1139
    out DX, AL                                ; ee                          ; 0xc113c
    mov dx, 003dah                            ; ba da 03                    ; 0xc113d vgabios.c:723
    in AL, DX                                 ; ec                          ; 0xc1140
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1141
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1143 vgabios.c:725
    pop di                                    ; 5f                          ; 0xc1146
    pop si                                    ; 5e                          ; 0xc1147
    pop cx                                    ; 59                          ; 0xc1148
    pop bx                                    ; 5b                          ; 0xc1149
    pop bp                                    ; 5d                          ; 0xc114a
    retn                                      ; c3                          ; 0xc114b
  ; disGetNextSymbol 0xc114c LB 0x320e -> off=0x0 cb=00000000000000f6 uValue=00000000000c114c 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc114c LB 0xf6
    push bp                                   ; 55                          ; 0xc114c vgabios.c:728
    mov bp, sp                                ; 89 e5                       ; 0xc114d
    push bx                                   ; 53                          ; 0xc114f
    push cx                                   ; 51                          ; 0xc1150
    push si                                   ; 56                          ; 0xc1151
    push di                                   ; 57                          ; 0xc1152
    push ax                                   ; 50                          ; 0xc1153
    mov bl, al                                ; 88 c3                       ; 0xc1154
    mov ah, dl                                ; 88 d4                       ; 0xc1156
    movzx cx, al                              ; 0f b6 c8                    ; 0xc1158 vgabios.c:734
    sal cx, 008h                              ; c1 e1 08                    ; 0xc115b
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc115e
    add dx, cx                                ; 01 ca                       ; 0xc1161
    mov si, strict word 00060h                ; be 60 00                    ; 0xc1163 vgabios.c:62
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc1166
    mov es, cx                                ; 8e c1                       ; 0xc1169
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc116b
    mov si, 00087h                            ; be 87 00                    ; 0xc116e vgabios.c:47
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc1171
    test dl, 008h                             ; f6 c2 08                    ; 0xc1174 vgabios.c:48
    jne near 01217h                           ; 0f 85 9c 00                 ; 0xc1177
    mov dl, al                                ; 88 c2                       ; 0xc117b vgabios.c:740
    and dl, 060h                              ; 80 e2 60                    ; 0xc117d
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc1180
    jne short 0118ch                          ; 75 07                       ; 0xc1183
    mov BL, strict byte 01eh                  ; b3 1e                       ; 0xc1185 vgabios.c:742
    xor ah, ah                                ; 30 e4                       ; 0xc1187 vgabios.c:743
    jmp near 01217h                           ; e9 8b 00                    ; 0xc1189 vgabios.c:744
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc118c vgabios.c:47
    test dl, 001h                             ; f6 c2 01                    ; 0xc118f vgabios.c:48
    jne near 01217h                           ; 0f 85 81 00                 ; 0xc1192
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc1196
    jnc near 01217h                           ; 0f 83 7a 00                 ; 0xc1199
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc119d
    jnc near 01217h                           ; 0f 83 73 00                 ; 0xc11a0
    mov si, 00085h                            ; be 85 00                    ; 0xc11a4 vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc11a7
    mov es, dx                                ; 8e c2                       ; 0xc11aa
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc11ac
    mov dx, cx                                ; 89 ca                       ; 0xc11af vgabios.c:58
    cmp ah, bl                                ; 38 dc                       ; 0xc11b1 vgabios.c:755
    jnc short 011c1h                          ; 73 0c                       ; 0xc11b3
    test ah, ah                               ; 84 e4                       ; 0xc11b5 vgabios.c:757
    je short 01217h                           ; 74 5e                       ; 0xc11b7
    xor bl, bl                                ; 30 db                       ; 0xc11b9 vgabios.c:758
    mov ah, cl                                ; 88 cc                       ; 0xc11bb vgabios.c:759
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc11bd
    jmp short 01217h                          ; eb 56                       ; 0xc11bf vgabios.c:761
    movzx si, ah                              ; 0f b6 f4                    ; 0xc11c1 vgabios.c:762
    mov word [bp-00ah], si                    ; 89 76 f6                    ; 0xc11c4
    movzx si, bl                              ; 0f b6 f3                    ; 0xc11c7
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc11ca
    cmp si, cx                                ; 39 ce                       ; 0xc11cd
    jnc short 011e4h                          ; 73 13                       ; 0xc11cf
    movzx di, ah                              ; 0f b6 fc                    ; 0xc11d1
    mov si, cx                                ; 89 ce                       ; 0xc11d4
    dec si                                    ; 4e                          ; 0xc11d6
    cmp di, si                                ; 39 f7                       ; 0xc11d7
    je short 01217h                           ; 74 3c                       ; 0xc11d9
    movzx si, bl                              ; 0f b6 f3                    ; 0xc11db
    dec cx                                    ; 49                          ; 0xc11de
    dec cx                                    ; 49                          ; 0xc11df
    cmp si, cx                                ; 39 ce                       ; 0xc11e0
    je short 01217h                           ; 74 33                       ; 0xc11e2
    cmp ah, 003h                              ; 80 fc 03                    ; 0xc11e4 vgabios.c:764
    jbe short 01217h                          ; 76 2e                       ; 0xc11e7
    movzx si, bl                              ; 0f b6 f3                    ; 0xc11e9 vgabios.c:765
    movzx di, ah                              ; 0f b6 fc                    ; 0xc11ec
    inc si                                    ; 46                          ; 0xc11ef
    inc si                                    ; 46                          ; 0xc11f0
    mov cl, dl                                ; 88 d1                       ; 0xc11f1
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc11f3
    cmp di, si                                ; 39 f7                       ; 0xc11f5
    jnle short 0120ch                         ; 7f 13                       ; 0xc11f7
    sub bl, ah                                ; 28 e3                       ; 0xc11f9 vgabios.c:767
    add bl, dl                                ; 00 d3                       ; 0xc11fb
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc11fd
    mov ah, cl                                ; 88 cc                       ; 0xc11ff vgabios.c:768
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc1201 vgabios.c:769
    jc short 01217h                           ; 72 11                       ; 0xc1204
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc1206 vgabios.c:771
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc1208 vgabios.c:772
    jmp short 01217h                          ; eb 0b                       ; 0xc120a vgabios.c:774
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc120c
    jbe short 01215h                          ; 76 04                       ; 0xc120f
    shr dx, 1                                 ; d1 ea                       ; 0xc1211 vgabios.c:776
    mov bl, dl                                ; 88 d3                       ; 0xc1213
    mov ah, cl                                ; 88 cc                       ; 0xc1215 vgabios.c:780
    mov si, strict word 00063h                ; be 63 00                    ; 0xc1217 vgabios.c:57
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc121a
    mov es, dx                                ; 8e c2                       ; 0xc121d
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc121f
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc1222 vgabios.c:791
    mov dx, cx                                ; 89 ca                       ; 0xc1224
    out DX, AL                                ; ee                          ; 0xc1226
    mov si, cx                                ; 89 ce                       ; 0xc1227 vgabios.c:792
    inc si                                    ; 46                          ; 0xc1229
    mov al, bl                                ; 88 d8                       ; 0xc122a
    mov dx, si                                ; 89 f2                       ; 0xc122c
    out DX, AL                                ; ee                          ; 0xc122e
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc122f vgabios.c:793
    mov dx, cx                                ; 89 ca                       ; 0xc1231
    out DX, AL                                ; ee                          ; 0xc1233
    mov al, ah                                ; 88 e0                       ; 0xc1234 vgabios.c:794
    mov dx, si                                ; 89 f2                       ; 0xc1236
    out DX, AL                                ; ee                          ; 0xc1238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc1239 vgabios.c:795
    pop di                                    ; 5f                          ; 0xc123c
    pop si                                    ; 5e                          ; 0xc123d
    pop cx                                    ; 59                          ; 0xc123e
    pop bx                                    ; 5b                          ; 0xc123f
    pop bp                                    ; 5d                          ; 0xc1240
    retn                                      ; c3                          ; 0xc1241
  ; disGetNextSymbol 0xc1242 LB 0x3118 -> off=0x0 cb=0000000000000089 uValue=00000000000c1242 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc1242 LB 0x89
    push bp                                   ; 55                          ; 0xc1242 vgabios.c:798
    mov bp, sp                                ; 89 e5                       ; 0xc1243
    push bx                                   ; 53                          ; 0xc1245
    push cx                                   ; 51                          ; 0xc1246
    push si                                   ; 56                          ; 0xc1247
    push ax                                   ; 50                          ; 0xc1248
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc1249 vgabios.c:804
    jnbe short 012c3h                         ; 77 76                       ; 0xc124b
    movzx bx, al                              ; 0f b6 d8                    ; 0xc124d vgabios.c:807
    add bx, bx                                ; 01 db                       ; 0xc1250
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc1252
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc1255 vgabios.c:62
    mov es, cx                                ; 8e c1                       ; 0xc1258
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc125a
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc125d vgabios.c:47
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc1260
    cmp al, ah                                ; 38 e0                       ; 0xc1263 vgabios.c:811
    jne short 012c3h                          ; 75 5c                       ; 0xc1265
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1267 vgabios.c:57
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc126a
    mov bx, 00084h                            ; bb 84 00                    ; 0xc126d vgabios.c:47
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc1270
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc1273 vgabios.c:48
    inc bx                                    ; 43                          ; 0xc1276
    mov si, dx                                ; 89 d6                       ; 0xc1277 vgabios.c:817
    and si, 0ff00h                            ; 81 e6 00 ff                 ; 0xc1279
    shr si, 008h                              ; c1 ee 08                    ; 0xc127d
    mov word [bp-008h], si                    ; 89 76 f8                    ; 0xc1280
    imul bx, cx                               ; 0f af d9                    ; 0xc1283 vgabios.c:820
    or bl, 0ffh                               ; 80 cb ff                    ; 0xc1286
    xor ah, ah                                ; 30 e4                       ; 0xc1289
    inc bx                                    ; 43                          ; 0xc128b
    imul ax, bx                               ; 0f af c3                    ; 0xc128c
    movzx si, dl                              ; 0f b6 f2                    ; 0xc128f
    add si, ax                                ; 01 c6                       ; 0xc1292
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1294
    imul ax, cx                               ; 0f af c1                    ; 0xc1298
    add si, ax                                ; 01 c6                       ; 0xc129b
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc129d vgabios.c:57
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc12a0
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc12a3 vgabios.c:824
    mov dx, bx                                ; 89 da                       ; 0xc12a5
    out DX, AL                                ; ee                          ; 0xc12a7
    mov ax, si                                ; 89 f0                       ; 0xc12a8 vgabios.c:825
    xor al, al                                ; 30 c0                       ; 0xc12aa
    shr ax, 008h                              ; c1 e8 08                    ; 0xc12ac
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc12af
    mov dx, cx                                ; 89 ca                       ; 0xc12b2
    out DX, AL                                ; ee                          ; 0xc12b4
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc12b5 vgabios.c:826
    mov dx, bx                                ; 89 da                       ; 0xc12b7
    out DX, AL                                ; ee                          ; 0xc12b9
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc12ba vgabios.c:827
    mov ax, si                                ; 89 f0                       ; 0xc12be
    mov dx, cx                                ; 89 ca                       ; 0xc12c0
    out DX, AL                                ; ee                          ; 0xc12c2
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc12c3 vgabios.c:829
    pop si                                    ; 5e                          ; 0xc12c6
    pop cx                                    ; 59                          ; 0xc12c7
    pop bx                                    ; 5b                          ; 0xc12c8
    pop bp                                    ; 5d                          ; 0xc12c9
    retn                                      ; c3                          ; 0xc12ca
  ; disGetNextSymbol 0xc12cb LB 0x308f -> off=0x0 cb=00000000000000cd uValue=00000000000c12cb 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc12cb LB 0xcd
    push bp                                   ; 55                          ; 0xc12cb vgabios.c:832
    mov bp, sp                                ; 89 e5                       ; 0xc12cc
    push bx                                   ; 53                          ; 0xc12ce
    push cx                                   ; 51                          ; 0xc12cf
    push dx                                   ; 52                          ; 0xc12d0
    push si                                   ; 56                          ; 0xc12d1
    push di                                   ; 57                          ; 0xc12d2
    push ax                                   ; 50                          ; 0xc12d3
    push ax                                   ; 50                          ; 0xc12d4
    mov cl, al                                ; 88 c1                       ; 0xc12d5
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc12d7 vgabios.c:838
    jnbe near 0138eh                          ; 0f 87 b1 00                 ; 0xc12d9
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc12dd vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12e0
    mov es, ax                                ; 8e c0                       ; 0xc12e3
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc12e5
    xor ah, ah                                ; 30 e4                       ; 0xc12e8 vgabios.c:842
    call 036a6h                               ; e8 b9 23                    ; 0xc12ea
    mov ch, al                                ; 88 c5                       ; 0xc12ed
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc12ef vgabios.c:843
    je near 0138eh                            ; 0f 84 99 00                 ; 0xc12f1
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc12f5 vgabios.c:846
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc12f8
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc12fb
    call 00a93h                               ; e8 92 f7                    ; 0xc12fe
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc1301 vgabios.c:848
    mov si, bx                                ; 89 de                       ; 0xc1304
    sal si, 003h                              ; c1 e6 03                    ; 0xc1306
    cmp byte [si+047b5h], 000h                ; 80 bc b5 47 00              ; 0xc1309
    jne short 01344h                          ; 75 34                       ; 0xc130e
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1310 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1313
    mov es, ax                                ; 8e c0                       ; 0xc1316
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc1318
    mov bx, 00084h                            ; bb 84 00                    ; 0xc131b vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc131e
    xor ah, ah                                ; 30 e4                       ; 0xc1321 vgabios.c:48
    inc ax                                    ; 40                          ; 0xc1323
    imul dx, ax                               ; 0f af d0                    ; 0xc1324 vgabios.c:855
    mov ax, dx                                ; 89 d0                       ; 0xc1327
    add ax, dx                                ; 01 d0                       ; 0xc1329
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc132b
    mov bx, ax                                ; 89 c3                       ; 0xc132d
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc132f
    inc bx                                    ; 43                          ; 0xc1332
    imul bx, ax                               ; 0f af d8                    ; 0xc1333
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc1336 vgabios.c:62
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc1339
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc133c vgabios.c:859
    mov bx, dx                                ; 89 d3                       ; 0xc133f
    inc bx                                    ; 43                          ; 0xc1341
    jmp short 01353h                          ; eb 0f                       ; 0xc1342 vgabios.c:861
    movzx bx, byte [bx+04834h]                ; 0f b6 9f 34 48              ; 0xc1344 vgabios.c:863
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1349
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc134c
    mov bx, word [bx+0484bh]                  ; 8b 9f 4b 48                 ; 0xc134f
    imul bx, ax                               ; 0f af d8                    ; 0xc1353
    mov si, strict word 00063h                ; be 63 00                    ; 0xc1356 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1359
    mov es, ax                                ; 8e c0                       ; 0xc135c
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc135e
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc1361 vgabios.c:868
    mov dx, si                                ; 89 f2                       ; 0xc1363
    out DX, AL                                ; ee                          ; 0xc1365
    mov ax, bx                                ; 89 d8                       ; 0xc1366 vgabios.c:869
    xor al, bl                                ; 30 d8                       ; 0xc1368
    shr ax, 008h                              ; c1 e8 08                    ; 0xc136a
    lea di, [si+001h]                         ; 8d 7c 01                    ; 0xc136d
    mov dx, di                                ; 89 fa                       ; 0xc1370
    out DX, AL                                ; ee                          ; 0xc1372
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc1373 vgabios.c:870
    mov dx, si                                ; 89 f2                       ; 0xc1375
    out DX, AL                                ; ee                          ; 0xc1377
    xor bh, bh                                ; 30 ff                       ; 0xc1378 vgabios.c:871
    mov ax, bx                                ; 89 d8                       ; 0xc137a
    mov dx, di                                ; 89 fa                       ; 0xc137c
    out DX, AL                                ; ee                          ; 0xc137e
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc137f vgabios.c:52
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc1382
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc1385 vgabios.c:881
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc1388
    call 01242h                               ; e8 b4 fe                    ; 0xc138b
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc138e vgabios.c:882
    pop di                                    ; 5f                          ; 0xc1391
    pop si                                    ; 5e                          ; 0xc1392
    pop dx                                    ; 5a                          ; 0xc1393
    pop cx                                    ; 59                          ; 0xc1394
    pop bx                                    ; 5b                          ; 0xc1395
    pop bp                                    ; 5d                          ; 0xc1396
    retn                                      ; c3                          ; 0xc1397
  ; disGetNextSymbol 0xc1398 LB 0x2fc2 -> off=0x0 cb=0000000000000045 uValue=00000000000c1398 'find_vpti'
find_vpti:                                   ; 0xc1398 LB 0x45
    push bx                                   ; 53                          ; 0xc1398 vgabios.c:917
    push si                                   ; 56                          ; 0xc1399
    push bp                                   ; 55                          ; 0xc139a
    mov bp, sp                                ; 89 e5                       ; 0xc139b
    movzx bx, al                              ; 0f b6 d8                    ; 0xc139d vgabios.c:922
    mov si, bx                                ; 89 de                       ; 0xc13a0
    sal si, 003h                              ; c1 e6 03                    ; 0xc13a2
    cmp byte [si+047b5h], 000h                ; 80 bc b5 47 00              ; 0xc13a5
    jne short 013d4h                          ; 75 28                       ; 0xc13aa
    mov si, 00089h                            ; be 89 00                    ; 0xc13ac vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc13af
    mov es, ax                                ; 8e c0                       ; 0xc13b2
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc13b4
    test AL, strict byte 010h                 ; a8 10                       ; 0xc13b7 vgabios.c:924
    je short 013c2h                           ; 74 07                       ; 0xc13b9
    movsx ax, byte [bx+07dfbh]                ; 0f be 87 fb 7d              ; 0xc13bb vgabios.c:925
    jmp short 013d9h                          ; eb 17                       ; 0xc13c0 vgabios.c:926
    test AL, strict byte 080h                 ; a8 80                       ; 0xc13c2
    je short 013cdh                           ; 74 07                       ; 0xc13c4
    movsx ax, byte [bx+07debh]                ; 0f be 87 eb 7d              ; 0xc13c6 vgabios.c:927
    jmp short 013d9h                          ; eb 0c                       ; 0xc13cb vgabios.c:928
    movsx ax, byte [bx+07df3h]                ; 0f be 87 f3 7d              ; 0xc13cd vgabios.c:929
    jmp short 013d9h                          ; eb 05                       ; 0xc13d2 vgabios.c:930
    movzx ax, byte [bx+04834h]                ; 0f b6 87 34 48              ; 0xc13d4 vgabios.c:931
    pop bp                                    ; 5d                          ; 0xc13d9 vgabios.c:934
    pop si                                    ; 5e                          ; 0xc13da
    pop bx                                    ; 5b                          ; 0xc13db
    retn                                      ; c3                          ; 0xc13dc
  ; disGetNextSymbol 0xc13dd LB 0x2f7d -> off=0x0 cb=000000000000048a uValue=00000000000c13dd 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc13dd LB 0x48a
    push bp                                   ; 55                          ; 0xc13dd vgabios.c:938
    mov bp, sp                                ; 89 e5                       ; 0xc13de
    push bx                                   ; 53                          ; 0xc13e0
    push cx                                   ; 51                          ; 0xc13e1
    push dx                                   ; 52                          ; 0xc13e2
    push si                                   ; 56                          ; 0xc13e3
    push di                                   ; 57                          ; 0xc13e4
    sub sp, strict byte 00016h                ; 83 ec 16                    ; 0xc13e5
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc13e8
    and AL, strict byte 080h                  ; 24 80                       ; 0xc13eb vgabios.c:942
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc13ed
    call 007bfh                               ; e8 cc f3                    ; 0xc13f0 vgabios.c:952
    test ax, ax                               ; 85 c0                       ; 0xc13f3
    je short 01403h                           ; 74 0c                       ; 0xc13f5
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc13f7 vgabios.c:954
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc13f9
    out DX, AL                                ; ee                          ; 0xc13fc
    xor al, al                                ; 30 c0                       ; 0xc13fd vgabios.c:955
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc13ff
    out DX, AL                                ; ee                          ; 0xc1402
    and byte [bp-00eh], 07fh                  ; 80 66 f2 7f                 ; 0xc1403 vgabios.c:960
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1407 vgabios.c:966
    call 036a6h                               ; e8 98 22                    ; 0xc140b
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc140e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1411 vgabios.c:972
    je near 0185dh                            ; 0f 84 46 04                 ; 0xc1413
    mov bx, 000a8h                            ; bb a8 00                    ; 0xc1417 vgabios.c:67
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc141a
    mov es, dx                                ; 8e c2                       ; 0xc141d
    mov di, word [es:bx]                      ; 26 8b 3f                    ; 0xc141f
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc1422
    mov bx, di                                ; 89 fb                       ; 0xc1426 vgabios.c:68
    mov word [bp-014h], dx                    ; 89 56 ec                    ; 0xc1428
    movzx cx, al                              ; 0f b6 c8                    ; 0xc142b vgabios.c:978
    mov ax, cx                                ; 89 c8                       ; 0xc142e
    call 01398h                               ; e8 65 ff                    ; 0xc1430
    mov es, dx                                ; 8e c2                       ; 0xc1433 vgabios.c:979
    mov si, word [es:di]                      ; 26 8b 35                    ; 0xc1435
    mov dx, word [es:di+002h]                 ; 26 8b 55 02                 ; 0xc1438
    mov word [bp-01ah], dx                    ; 89 56 e6                    ; 0xc143c
    xor ah, ah                                ; 30 e4                       ; 0xc143f vgabios.c:980
    sal ax, 006h                              ; c1 e0 06                    ; 0xc1441
    add si, ax                                ; 01 c6                       ; 0xc1444
    mov di, 00089h                            ; bf 89 00                    ; 0xc1446 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1449
    mov es, ax                                ; 8e c0                       ; 0xc144c
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc144e
    mov ah, al                                ; 88 c4                       ; 0xc1451 vgabios.c:48
    test AL, strict byte 008h                 ; a8 08                       ; 0xc1453 vgabios.c:997
    jne near 01509h                           ; 0f 85 b0 00                 ; 0xc1455
    mov di, cx                                ; 89 cf                       ; 0xc1459 vgabios.c:999
    sal di, 003h                              ; c1 e7 03                    ; 0xc145b
    mov al, byte [di+047bah]                  ; 8a 85 ba 47                 ; 0xc145e
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc1462
    out DX, AL                                ; ee                          ; 0xc1465
    xor al, al                                ; 30 c0                       ; 0xc1466 vgabios.c:1002
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc1468
    out DX, AL                                ; ee                          ; 0xc146b
    mov cl, byte [di+047bbh]                  ; 8a 8d bb 47                 ; 0xc146c vgabios.c:1005
    cmp cl, 001h                              ; 80 f9 01                    ; 0xc1470
    jc short 01483h                           ; 72 0e                       ; 0xc1473
    jbe short 0148eh                          ; 76 17                       ; 0xc1475
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc1477
    je short 0149ch                           ; 74 20                       ; 0xc147a
    cmp cl, 002h                              ; 80 f9 02                    ; 0xc147c
    je short 01495h                           ; 74 14                       ; 0xc147f
    jmp short 014a1h                          ; eb 1e                       ; 0xc1481
    test cl, cl                               ; 84 c9                       ; 0xc1483
    jne short 014a1h                          ; 75 1a                       ; 0xc1485
    mov word [bp-016h], 04fc8h                ; c7 46 ea c8 4f              ; 0xc1487 vgabios.c:1007
    jmp short 014a1h                          ; eb 13                       ; 0xc148c vgabios.c:1008
    mov word [bp-016h], 05088h                ; c7 46 ea 88 50              ; 0xc148e vgabios.c:1010
    jmp short 014a1h                          ; eb 0c                       ; 0xc1493 vgabios.c:1011
    mov word [bp-016h], 05148h                ; c7 46 ea 48 51              ; 0xc1495 vgabios.c:1013
    jmp short 014a1h                          ; eb 05                       ; 0xc149a vgabios.c:1014
    mov word [bp-016h], 05208h                ; c7 46 ea 08 52              ; 0xc149c vgabios.c:1016
    movzx di, byte [bp-010h]                  ; 0f b6 7e f0                 ; 0xc14a1 vgabios.c:1020
    sal di, 003h                              ; c1 e7 03                    ; 0xc14a5
    cmp byte [di+047b5h], 000h                ; 80 bd b5 47 00              ; 0xc14a8
    jne short 014beh                          ; 75 0f                       ; 0xc14ad
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc14af vgabios.c:1022
    cmp byte [es:si+002h], 008h               ; 26 80 7c 02 08              ; 0xc14b2
    jne short 014beh                          ; 75 05                       ; 0xc14b7
    mov word [bp-016h], 05088h                ; c7 46 ea 88 50              ; 0xc14b9 vgabios.c:1023
    xor cx, cx                                ; 31 c9                       ; 0xc14be vgabios.c:1026
    jmp short 014d1h                          ; eb 0f                       ; 0xc14c0
    xor al, al                                ; 30 c0                       ; 0xc14c2 vgabios.c:1033
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc14c4
    out DX, AL                                ; ee                          ; 0xc14c7
    out DX, AL                                ; ee                          ; 0xc14c8 vgabios.c:1034
    out DX, AL                                ; ee                          ; 0xc14c9 vgabios.c:1035
    inc cx                                    ; 41                          ; 0xc14ca vgabios.c:1037
    cmp cx, 00100h                            ; 81 f9 00 01                 ; 0xc14cb
    jnc short 014fch                          ; 73 2b                       ; 0xc14cf
    movzx di, byte [bp-010h]                  ; 0f b6 7e f0                 ; 0xc14d1
    sal di, 003h                              ; c1 e7 03                    ; 0xc14d5
    movzx di, byte [di+047bbh]                ; 0f b6 bd bb 47              ; 0xc14d8
    movzx di, byte [di+04844h]                ; 0f b6 bd 44 48              ; 0xc14dd
    cmp cx, di                                ; 39 f9                       ; 0xc14e2
    jnbe short 014c2h                         ; 77 dc                       ; 0xc14e4
    imul di, cx, strict byte 00003h           ; 6b f9 03                    ; 0xc14e6
    add di, word [bp-016h]                    ; 03 7e ea                    ; 0xc14e9
    mov al, byte [di]                         ; 8a 05                       ; 0xc14ec
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc14ee
    out DX, AL                                ; ee                          ; 0xc14f1
    mov al, byte [di+001h]                    ; 8a 45 01                    ; 0xc14f2
    out DX, AL                                ; ee                          ; 0xc14f5
    mov al, byte [di+002h]                    ; 8a 45 02                    ; 0xc14f6
    out DX, AL                                ; ee                          ; 0xc14f9
    jmp short 014cah                          ; eb ce                       ; 0xc14fa
    test ah, 002h                             ; f6 c4 02                    ; 0xc14fc vgabios.c:1038
    je short 01509h                           ; 74 08                       ; 0xc14ff
    mov dx, 00100h                            ; ba 00 01                    ; 0xc1501 vgabios.c:1040
    xor ax, ax                                ; 31 c0                       ; 0xc1504
    call 010c0h                               ; e8 b7 fb                    ; 0xc1506
    mov dx, 003dah                            ; ba da 03                    ; 0xc1509 vgabios.c:1045
    in AL, DX                                 ; ec                          ; 0xc150c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc150d
    xor cx, cx                                ; 31 c9                       ; 0xc150f vgabios.c:1048
    jmp short 01518h                          ; eb 05                       ; 0xc1511
    cmp cx, strict byte 00013h                ; 83 f9 13                    ; 0xc1513
    jnbe short 0152dh                         ; 77 15                       ; 0xc1516
    mov al, cl                                ; 88 c8                       ; 0xc1518 vgabios.c:1049
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc151a
    out DX, AL                                ; ee                          ; 0xc151d
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc151e vgabios.c:1050
    mov di, si                                ; 89 f7                       ; 0xc1521
    add di, cx                                ; 01 cf                       ; 0xc1523
    mov al, byte [es:di+023h]                 ; 26 8a 45 23                 ; 0xc1525
    out DX, AL                                ; ee                          ; 0xc1529
    inc cx                                    ; 41                          ; 0xc152a vgabios.c:1051
    jmp short 01513h                          ; eb e6                       ; 0xc152b
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc152d vgabios.c:1052
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc152f
    out DX, AL                                ; ee                          ; 0xc1532
    xor al, al                                ; 30 c0                       ; 0xc1533 vgabios.c:1053
    out DX, AL                                ; ee                          ; 0xc1535
    mov es, [bp-014h]                         ; 8e 46 ec                    ; 0xc1536 vgabios.c:1056
    mov dx, word [es:bx+004h]                 ; 26 8b 57 04                 ; 0xc1539
    mov ax, word [es:bx+006h]                 ; 26 8b 47 06                 ; 0xc153d
    test ax, ax                               ; 85 c0                       ; 0xc1541
    jne short 01549h                          ; 75 04                       ; 0xc1543
    test dx, dx                               ; 85 d2                       ; 0xc1545
    je short 01589h                           ; 74 40                       ; 0xc1547
    mov word [bp-01ch], ax                    ; 89 46 e4                    ; 0xc1549 vgabios.c:1060
    xor cx, cx                                ; 31 c9                       ; 0xc154c vgabios.c:1061
    jmp short 01555h                          ; eb 05                       ; 0xc154e
    cmp cx, strict byte 00010h                ; 83 f9 10                    ; 0xc1550
    jnc short 01579h                          ; 73 24                       ; 0xc1553
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1555 vgabios.c:1062
    mov di, si                                ; 89 f7                       ; 0xc1558
    add di, cx                                ; 01 cf                       ; 0xc155a
    mov ax, word [bp-01ch]                    ; 8b 46 e4                    ; 0xc155c
    mov word [bp-020h], ax                    ; 89 46 e0                    ; 0xc155f
    mov ax, dx                                ; 89 d0                       ; 0xc1562
    add ax, cx                                ; 01 c8                       ; 0xc1564
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1566
    mov al, byte [es:di+023h]                 ; 26 8a 45 23                 ; 0xc1569
    mov es, [bp-020h]                         ; 8e 46 e0                    ; 0xc156d
    mov di, word [bp-01eh]                    ; 8b 7e e2                    ; 0xc1570
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc1573
    inc cx                                    ; 41                          ; 0xc1576
    jmp short 01550h                          ; eb d7                       ; 0xc1577
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1579 vgabios.c:1063
    mov al, byte [es:si+034h]                 ; 26 8a 44 34                 ; 0xc157c
    mov es, [bp-01ch]                         ; 8e 46 e4                    ; 0xc1580
    mov di, dx                                ; 89 d7                       ; 0xc1583
    mov byte [es:di+010h], al                 ; 26 88 45 10                 ; 0xc1585
    xor al, al                                ; 30 c0                       ; 0xc1589 vgabios.c:1067
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc158b
    out DX, AL                                ; ee                          ; 0xc158e
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc158f vgabios.c:1068
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1591
    out DX, AL                                ; ee                          ; 0xc1594
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc1595 vgabios.c:1069
    jmp short 0159fh                          ; eb 05                       ; 0xc1598
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc159a
    jnbe short 015b7h                         ; 77 18                       ; 0xc159d
    mov al, cl                                ; 88 c8                       ; 0xc159f vgabios.c:1070
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc15a1
    out DX, AL                                ; ee                          ; 0xc15a4
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc15a5 vgabios.c:1071
    mov di, si                                ; 89 f7                       ; 0xc15a8
    add di, cx                                ; 01 cf                       ; 0xc15aa
    mov al, byte [es:di+004h]                 ; 26 8a 45 04                 ; 0xc15ac
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc15b0
    out DX, AL                                ; ee                          ; 0xc15b3
    inc cx                                    ; 41                          ; 0xc15b4 vgabios.c:1072
    jmp short 0159ah                          ; eb e3                       ; 0xc15b5
    xor cx, cx                                ; 31 c9                       ; 0xc15b7 vgabios.c:1075
    jmp short 015c0h                          ; eb 05                       ; 0xc15b9
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc15bb
    jnbe short 015d8h                         ; 77 18                       ; 0xc15be
    mov al, cl                                ; 88 c8                       ; 0xc15c0 vgabios.c:1076
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc15c2
    out DX, AL                                ; ee                          ; 0xc15c5
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc15c6 vgabios.c:1077
    mov di, si                                ; 89 f7                       ; 0xc15c9
    add di, cx                                ; 01 cf                       ; 0xc15cb
    mov al, byte [es:di+037h]                 ; 26 8a 45 37                 ; 0xc15cd
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc15d1
    out DX, AL                                ; ee                          ; 0xc15d4
    inc cx                                    ; 41                          ; 0xc15d5 vgabios.c:1078
    jmp short 015bbh                          ; eb e3                       ; 0xc15d6
    movzx di, byte [bp-010h]                  ; 0f b6 7e f0                 ; 0xc15d8 vgabios.c:1081
    sal di, 003h                              ; c1 e7 03                    ; 0xc15dc
    cmp byte [di+047b6h], 001h                ; 80 bd b6 47 01              ; 0xc15df
    jne short 015ebh                          ; 75 05                       ; 0xc15e4
    mov cx, 003b4h                            ; b9 b4 03                    ; 0xc15e6
    jmp short 015eeh                          ; eb 03                       ; 0xc15e9
    mov cx, 003d4h                            ; b9 d4 03                    ; 0xc15eb
    mov word [bp-018h], cx                    ; 89 4e e8                    ; 0xc15ee
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc15f1 vgabios.c:1084
    mov al, byte [es:si+009h]                 ; 26 8a 44 09                 ; 0xc15f4
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc15f8
    out DX, AL                                ; ee                          ; 0xc15fb
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc15fc vgabios.c:1087
    mov dx, cx                                ; 89 ca                       ; 0xc15ff
    out DX, ax                                ; ef                          ; 0xc1601
    xor cx, cx                                ; 31 c9                       ; 0xc1602 vgabios.c:1089
    jmp short 0160bh                          ; eb 05                       ; 0xc1604
    cmp cx, strict byte 00018h                ; 83 f9 18                    ; 0xc1606
    jnbe short 01621h                         ; 77 16                       ; 0xc1609
    mov al, cl                                ; 88 c8                       ; 0xc160b vgabios.c:1090
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc160d
    out DX, AL                                ; ee                          ; 0xc1610
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1611 vgabios.c:1091
    mov di, si                                ; 89 f7                       ; 0xc1614
    add di, cx                                ; 01 cf                       ; 0xc1616
    inc dx                                    ; 42                          ; 0xc1618
    mov al, byte [es:di+00ah]                 ; 26 8a 45 0a                 ; 0xc1619
    out DX, AL                                ; ee                          ; 0xc161d
    inc cx                                    ; 41                          ; 0xc161e vgabios.c:1092
    jmp short 01606h                          ; eb e5                       ; 0xc161f
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc1621 vgabios.c:1095
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1623
    out DX, AL                                ; ee                          ; 0xc1626
    mov dx, word [bp-018h]                    ; 8b 56 e8                    ; 0xc1627 vgabios.c:1096
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc162a
    in AL, DX                                 ; ec                          ; 0xc162d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc162e
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1630 vgabios.c:1098
    jne short 01692h                          ; 75 5c                       ; 0xc1634
    movzx di, byte [bp-010h]                  ; 0f b6 7e f0                 ; 0xc1636 vgabios.c:1100
    sal di, 003h                              ; c1 e7 03                    ; 0xc163a
    cmp byte [di+047b5h], 000h                ; 80 bd b5 47 00              ; 0xc163d
    jne short 01656h                          ; 75 12                       ; 0xc1642
    mov es, [di+047b8h]                       ; 8e 85 b8 47                 ; 0xc1644 vgabios.c:1102
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1648
    mov ax, 00720h                            ; b8 20 07                    ; 0xc164b
    xor di, di                                ; 31 ff                       ; 0xc164e
    jcxz 01654h                               ; e3 02                       ; 0xc1650
    rep stosw                                 ; f3 ab                       ; 0xc1652
    jmp short 01692h                          ; eb 3c                       ; 0xc1654 vgabios.c:1104
    cmp byte [bp-00eh], 00dh                  ; 80 7e f2 0d                 ; 0xc1656 vgabios.c:1106
    jnc short 0166dh                          ; 73 11                       ; 0xc165a
    mov es, [di+047b8h]                       ; 8e 85 b8 47                 ; 0xc165c vgabios.c:1108
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1660
    xor ax, ax                                ; 31 c0                       ; 0xc1663
    xor di, di                                ; 31 ff                       ; 0xc1665
    jcxz 0166bh                               ; e3 02                       ; 0xc1667
    rep stosw                                 ; f3 ab                       ; 0xc1669
    jmp short 01692h                          ; eb 25                       ; 0xc166b vgabios.c:1110
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc166d vgabios.c:1112
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc166f
    out DX, AL                                ; ee                          ; 0xc1672
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc1673 vgabios.c:1113
    in AL, DX                                 ; ec                          ; 0xc1676
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1677
    mov word [bp-01eh], ax                    ; 89 46 e2                    ; 0xc1679
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc167c vgabios.c:1114
    out DX, AL                                ; ee                          ; 0xc167e
    mov es, [di+047b8h]                       ; 8e 85 b8 47                 ; 0xc167f vgabios.c:1115
    mov cx, 08000h                            ; b9 00 80                    ; 0xc1683
    xor ax, ax                                ; 31 c0                       ; 0xc1686
    xor di, di                                ; 31 ff                       ; 0xc1688
    jcxz 0168eh                               ; e3 02                       ; 0xc168a
    rep stosw                                 ; f3 ab                       ; 0xc168c
    mov al, byte [bp-01eh]                    ; 8a 46 e2                    ; 0xc168e vgabios.c:1116
    out DX, AL                                ; ee                          ; 0xc1691
    mov di, strict word 00049h                ; bf 49 00                    ; 0xc1692 vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1695
    mov es, ax                                ; 8e c0                       ; 0xc1698
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc169a
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc169d
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc16a0 vgabios.c:1123
    movzx ax, byte [es:si]                    ; 26 0f b6 04                 ; 0xc16a3
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc16a7 vgabios.c:62
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc16aa
    mov es, dx                                ; 8e c2                       ; 0xc16ad
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc16af
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc16b2 vgabios.c:60
    mov ax, word [es:si+003h]                 ; 26 8b 44 03                 ; 0xc16b5
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc16b9 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc16bc
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc16be
    mov di, strict word 00063h                ; bf 63 00                    ; 0xc16c1 vgabios.c:62
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc16c4
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc16c7
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc16ca vgabios.c:50
    mov al, byte [es:si+001h]                 ; 26 8a 44 01                 ; 0xc16cd
    mov di, 00084h                            ; bf 84 00                    ; 0xc16d1 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc16d4
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc16d6
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc16d9 vgabios.c:1127
    movzx ax, byte [es:si+002h]               ; 26 0f b6 44 02              ; 0xc16dc
    mov di, 00085h                            ; bf 85 00                    ; 0xc16e1 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc16e4
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc16e6
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc16e9 vgabios.c:1128
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc16ec
    mov di, 00087h                            ; bf 87 00                    ; 0xc16ee vgabios.c:52
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc16f1
    mov di, 00088h                            ; bf 88 00                    ; 0xc16f4 vgabios.c:52
    mov byte [es:di], 0f9h                    ; 26 c6 05 f9                 ; 0xc16f7
    mov di, 0008ah                            ; bf 8a 00                    ; 0xc16fb vgabios.c:52
    mov byte [es:di], 008h                    ; 26 c6 05 08                 ; 0xc16fe
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc1702 vgabios.c:1134
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc1705
    jnbe short 0172fh                         ; 77 26                       ; 0xc1707
    movzx di, al                              ; 0f b6 f8                    ; 0xc1709 vgabios.c:1136
    mov al, byte [di+07de3h]                  ; 8a 85 e3 7d                 ; 0xc170c vgabios.c:50
    mov di, strict word 00065h                ; bf 65 00                    ; 0xc1710 vgabios.c:52
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc1713
    cmp byte [bp-00eh], 006h                  ; 80 7e f2 06                 ; 0xc1716 vgabios.c:1137
    jne short 01721h                          ; 75 05                       ; 0xc171a
    mov dx, strict word 0003fh                ; ba 3f 00                    ; 0xc171c
    jmp short 01724h                          ; eb 03                       ; 0xc171f
    mov dx, strict word 00030h                ; ba 30 00                    ; 0xc1721
    mov di, strict word 00066h                ; bf 66 00                    ; 0xc1724 vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1727
    mov es, ax                                ; 8e c0                       ; 0xc172a
    mov byte [es:di], dl                      ; 26 88 15                    ; 0xc172c
    movzx di, byte [bp-010h]                  ; 0f b6 7e f0                 ; 0xc172f vgabios.c:1141
    sal di, 003h                              ; c1 e7 03                    ; 0xc1733
    cmp byte [di+047b5h], 000h                ; 80 bd b5 47 00              ; 0xc1736
    jne short 01746h                          ; 75 09                       ; 0xc173b
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc173d vgabios.c:1143
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc1740
    call 0114ch                               ; e8 06 fa                    ; 0xc1743
    xor cx, cx                                ; 31 c9                       ; 0xc1746 vgabios.c:1148
    jmp short 0174fh                          ; eb 05                       ; 0xc1748
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc174a
    jnc short 0175ah                          ; 73 0b                       ; 0xc174d
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc174f vgabios.c:1149
    xor dx, dx                                ; 31 d2                       ; 0xc1752
    call 01242h                               ; e8 eb fa                    ; 0xc1754
    inc cx                                    ; 41                          ; 0xc1757
    jmp short 0174ah                          ; eb f0                       ; 0xc1758
    xor ax, ax                                ; 31 c0                       ; 0xc175a vgabios.c:1152
    call 012cbh                               ; e8 6c fb                    ; 0xc175c
    movzx di, byte [bp-010h]                  ; 0f b6 7e f0                 ; 0xc175f vgabios.c:1155
    sal di, 003h                              ; c1 e7 03                    ; 0xc1763
    cmp byte [di+047b5h], 000h                ; 80 bd b5 47 00              ; 0xc1766
    jne near 01828h                           ; 0f 85 b9 00                 ; 0xc176b
    mov es, [bp-014h]                         ; 8e 46 ec                    ; 0xc176f vgabios.c:1157
    mov di, word [es:bx+008h]                 ; 26 8b 7f 08                 ; 0xc1772
    mov ax, word [es:bx+00ah]                 ; 26 8b 47 0a                 ; 0xc1776
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc177a
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc177d vgabios.c:1159
    mov al, byte [es:si+002h]                 ; 26 8a 44 02                 ; 0xc1780
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc1784
    je short 017a5h                           ; 74 1d                       ; 0xc1786
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc1788
    jne short 017b8h                          ; 75 2c                       ; 0xc178a
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc178c vgabios.c:1161
    movzx ax, byte [es:si+002h]               ; 26 0f b6 44 02              ; 0xc178f
    push ax                                   ; 50                          ; 0xc1794
    push dword 000000000h                     ; 66 6a 00                    ; 0xc1795
    mov cx, 00100h                            ; b9 00 01                    ; 0xc1798
    mov bx, 05572h                            ; bb 72 55                    ; 0xc179b
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc179e
    xor ax, ax                                ; 31 c0                       ; 0xc17a1
    jmp short 017c9h                          ; eb 24                       ; 0xc17a3 vgabios.c:1162
    xor ah, ah                                ; 30 e4                       ; 0xc17a5 vgabios.c:1164
    push ax                                   ; 50                          ; 0xc17a7
    push dword 000000000h                     ; 66 6a 00                    ; 0xc17a8
    mov cx, 00100h                            ; b9 00 01                    ; 0xc17ab
    mov bx, 05d72h                            ; bb 72 5d                    ; 0xc17ae
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc17b1
    xor al, al                                ; 30 c0                       ; 0xc17b4
    jmp short 017c9h                          ; eb 11                       ; 0xc17b6
    xor ah, ah                                ; 30 e4                       ; 0xc17b8 vgabios.c:1167
    push ax                                   ; 50                          ; 0xc17ba
    push dword 000000000h                     ; 66 6a 00                    ; 0xc17bb
    mov cx, 00100h                            ; b9 00 01                    ; 0xc17be
    mov bx, 06b72h                            ; bb 72 6b                    ; 0xc17c1
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc17c4
    xor al, al                                ; 30 c0                       ; 0xc17c7
    call 02b72h                               ; e8 a6 13                    ; 0xc17c9
    cmp word [bp-012h], strict byte 00000h    ; 83 7e ee 00                 ; 0xc17cc vgabios.c:1169
    jne short 017d6h                          ; 75 04                       ; 0xc17d0
    test di, di                               ; 85 ff                       ; 0xc17d2
    je short 01820h                           ; 74 4a                       ; 0xc17d4
    xor cx, cx                                ; 31 c9                       ; 0xc17d6 vgabios.c:1174
    mov es, [bp-012h]                         ; 8e 46 ee                    ; 0xc17d8 vgabios.c:1176
    mov bx, di                                ; 89 fb                       ; 0xc17db
    add bx, cx                                ; 01 cb                       ; 0xc17dd
    mov al, byte [es:bx+00bh]                 ; 26 8a 47 0b                 ; 0xc17df
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc17e3
    je short 017efh                           ; 74 08                       ; 0xc17e5
    cmp al, byte [bp-00eh]                    ; 3a 46 f2                    ; 0xc17e7 vgabios.c:1178
    je short 017efh                           ; 74 03                       ; 0xc17ea
    inc cx                                    ; 41                          ; 0xc17ec vgabios.c:1180
    jmp short 017d8h                          ; eb e9                       ; 0xc17ed vgabios.c:1181
    mov es, [bp-012h]                         ; 8e 46 ee                    ; 0xc17ef vgabios.c:1183
    mov bx, di                                ; 89 fb                       ; 0xc17f2
    add bx, cx                                ; 01 cb                       ; 0xc17f4
    mov al, byte [es:bx+00bh]                 ; 26 8a 47 0b                 ; 0xc17f6
    cmp al, byte [bp-00eh]                    ; 3a 46 f2                    ; 0xc17fa
    jne short 01820h                          ; 75 21                       ; 0xc17fd
    movzx ax, byte [es:di]                    ; 26 0f b6 05                 ; 0xc17ff vgabios.c:1188
    push ax                                   ; 50                          ; 0xc1803
    movzx ax, byte [es:di+001h]               ; 26 0f b6 45 01              ; 0xc1804
    push ax                                   ; 50                          ; 0xc1809
    push word [es:di+004h]                    ; 26 ff 75 04                 ; 0xc180a
    mov cx, word [es:di+002h]                 ; 26 8b 4d 02                 ; 0xc180e
    mov bx, word [es:di+006h]                 ; 26 8b 5d 06                 ; 0xc1812
    mov dx, word [es:di+008h]                 ; 26 8b 55 08                 ; 0xc1816
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc181a
    call 02b72h                               ; e8 52 13                    ; 0xc181d
    xor bl, bl                                ; 30 db                       ; 0xc1820 vgabios.c:1192
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc1822
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc1824
    int 06dh                                  ; cd 6d                       ; 0xc1826
    mov bx, 05972h                            ; bb 72 59                    ; 0xc1828 vgabios.c:1196
    mov cx, ds                                ; 8c d9                       ; 0xc182b
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc182d
    call 009f0h                               ; e8 bd f1                    ; 0xc1830
    mov es, [bp-01ah]                         ; 8e 46 e6                    ; 0xc1833 vgabios.c:1198
    mov al, byte [es:si+002h]                 ; 26 8a 44 02                 ; 0xc1836
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc183a
    je short 01858h                           ; 74 1a                       ; 0xc183c
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc183e
    je short 01853h                           ; 74 11                       ; 0xc1840
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc1842
    jne short 0185dh                          ; 75 17                       ; 0xc1844
    mov bx, 05572h                            ; bb 72 55                    ; 0xc1846 vgabios.c:1200
    mov cx, ds                                ; 8c d9                       ; 0xc1849
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc184b
    call 009f0h                               ; e8 9f f1                    ; 0xc184e
    jmp short 0185dh                          ; eb 0a                       ; 0xc1851 vgabios.c:1201
    mov bx, 05d72h                            ; bb 72 5d                    ; 0xc1853 vgabios.c:1203
    jmp short 01849h                          ; eb f1                       ; 0xc1856
    mov bx, 06b72h                            ; bb 72 6b                    ; 0xc1858 vgabios.c:1206
    jmp short 01849h                          ; eb ec                       ; 0xc185b
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc185d vgabios.c:1209
    pop di                                    ; 5f                          ; 0xc1860
    pop si                                    ; 5e                          ; 0xc1861
    pop dx                                    ; 5a                          ; 0xc1862
    pop cx                                    ; 59                          ; 0xc1863
    pop bx                                    ; 5b                          ; 0xc1864
    pop bp                                    ; 5d                          ; 0xc1865
    retn                                      ; c3                          ; 0xc1866
  ; disGetNextSymbol 0xc1867 LB 0x2af3 -> off=0x0 cb=0000000000000075 uValue=00000000000c1867 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc1867 LB 0x75
    push bp                                   ; 55                          ; 0xc1867 vgabios.c:1212
    mov bp, sp                                ; 89 e5                       ; 0xc1868
    push si                                   ; 56                          ; 0xc186a
    push di                                   ; 57                          ; 0xc186b
    push ax                                   ; 50                          ; 0xc186c
    push ax                                   ; 50                          ; 0xc186d
    mov bh, cl                                ; 88 cf                       ; 0xc186e
    movzx di, dl                              ; 0f b6 fa                    ; 0xc1870 vgabios.c:1218
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc1873
    imul di, cx                               ; 0f af f9                    ; 0xc1877
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc187a
    imul di, si                               ; 0f af fe                    ; 0xc187e
    xor ah, ah                                ; 30 e4                       ; 0xc1881
    add di, ax                                ; 01 c7                       ; 0xc1883
    mov word [bp-008h], di                    ; 89 7e f8                    ; 0xc1885
    movzx di, bl                              ; 0f b6 fb                    ; 0xc1888 vgabios.c:1219
    imul cx, di                               ; 0f af cf                    ; 0xc188b
    imul cx, si                               ; 0f af ce                    ; 0xc188e
    add cx, ax                                ; 01 c1                       ; 0xc1891
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc1893
    mov ax, 00105h                            ; b8 05 01                    ; 0xc1896 vgabios.c:1220
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1899
    out DX, ax                                ; ef                          ; 0xc189c
    xor bl, bl                                ; 30 db                       ; 0xc189d vgabios.c:1221
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc189f
    jnc short 018cch                          ; 73 28                       ; 0xc18a2
    movzx cx, bh                              ; 0f b6 cf                    ; 0xc18a4 vgabios.c:1223
    movzx si, bl                              ; 0f b6 f3                    ; 0xc18a7
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc18aa
    imul ax, si                               ; 0f af c6                    ; 0xc18ae
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc18b1
    add si, ax                                ; 01 c6                       ; 0xc18b4
    mov di, word [bp-006h]                    ; 8b 7e fa                    ; 0xc18b6
    add di, ax                                ; 01 c7                       ; 0xc18b9
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc18bb
    mov es, dx                                ; 8e c2                       ; 0xc18be
    jcxz 018c8h                               ; e3 06                       ; 0xc18c0
    push DS                                   ; 1e                          ; 0xc18c2
    mov ds, dx                                ; 8e da                       ; 0xc18c3
    rep movsb                                 ; f3 a4                       ; 0xc18c5
    pop DS                                    ; 1f                          ; 0xc18c7
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc18c8 vgabios.c:1224
    jmp short 0189fh                          ; eb d3                       ; 0xc18ca
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc18cc vgabios.c:1225
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc18cf
    out DX, ax                                ; ef                          ; 0xc18d2
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc18d3 vgabios.c:1226
    pop di                                    ; 5f                          ; 0xc18d6
    pop si                                    ; 5e                          ; 0xc18d7
    pop bp                                    ; 5d                          ; 0xc18d8
    retn 00004h                               ; c2 04 00                    ; 0xc18d9
  ; disGetNextSymbol 0xc18dc LB 0x2a7e -> off=0x0 cb=0000000000000060 uValue=00000000000c18dc 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc18dc LB 0x60
    push bp                                   ; 55                          ; 0xc18dc vgabios.c:1229
    mov bp, sp                                ; 89 e5                       ; 0xc18dd
    push di                                   ; 57                          ; 0xc18df
    push ax                                   ; 50                          ; 0xc18e0
    push ax                                   ; 50                          ; 0xc18e1
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc18e2
    mov bh, cl                                ; 88 cf                       ; 0xc18e5
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc18e7 vgabios.c:1235
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc18ea
    imul cx, dx                               ; 0f af ca                    ; 0xc18ee
    movzx dx, bh                              ; 0f b6 d7                    ; 0xc18f1
    imul dx, cx                               ; 0f af d1                    ; 0xc18f4
    xor ah, ah                                ; 30 e4                       ; 0xc18f7
    add dx, ax                                ; 01 c2                       ; 0xc18f9
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc18fb
    mov ax, 00205h                            ; b8 05 02                    ; 0xc18fe vgabios.c:1236
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1901
    out DX, ax                                ; ef                          ; 0xc1904
    xor bl, bl                                ; 30 db                       ; 0xc1905 vgabios.c:1237
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1907
    jnc short 0192dh                          ; 73 21                       ; 0xc190a
    movzx cx, byte [bp-004h]                  ; 0f b6 4e fc                 ; 0xc190c vgabios.c:1239
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1910
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc1914
    movzx di, bh                              ; 0f b6 ff                    ; 0xc1917
    imul di, dx                               ; 0f af fa                    ; 0xc191a
    add di, word [bp-006h]                    ; 03 7e fa                    ; 0xc191d
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1920
    mov es, dx                                ; 8e c2                       ; 0xc1923
    jcxz 01929h                               ; e3 02                       ; 0xc1925
    rep stosb                                 ; f3 aa                       ; 0xc1927
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc1929 vgabios.c:1240
    jmp short 01907h                          ; eb da                       ; 0xc192b
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc192d vgabios.c:1241
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1930
    out DX, ax                                ; ef                          ; 0xc1933
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc1934 vgabios.c:1242
    pop di                                    ; 5f                          ; 0xc1937
    pop bp                                    ; 5d                          ; 0xc1938
    retn 00004h                               ; c2 04 00                    ; 0xc1939
  ; disGetNextSymbol 0xc193c LB 0x2a1e -> off=0x0 cb=00000000000000a3 uValue=00000000000c193c 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc193c LB 0xa3
    push bp                                   ; 55                          ; 0xc193c vgabios.c:1245
    mov bp, sp                                ; 89 e5                       ; 0xc193d
    push si                                   ; 56                          ; 0xc193f
    push di                                   ; 57                          ; 0xc1940
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1941
    mov dh, bl                                ; 88 de                       ; 0xc1944
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc1946
    movzx di, dl                              ; 0f b6 fa                    ; 0xc1949 vgabios.c:1251
    movzx si, byte [bp+006h]                  ; 0f b6 76 06                 ; 0xc194c
    imul di, si                               ; 0f af fe                    ; 0xc1950
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc1953
    imul di, bx                               ; 0f af fb                    ; 0xc1957
    sar di, 1                                 ; d1 ff                       ; 0xc195a
    xor ah, ah                                ; 30 e4                       ; 0xc195c
    add di, ax                                ; 01 c7                       ; 0xc195e
    mov word [bp-00ch], di                    ; 89 7e f4                    ; 0xc1960
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc1963 vgabios.c:1252
    imul dx, si                               ; 0f af d6                    ; 0xc1966
    imul dx, bx                               ; 0f af d3                    ; 0xc1969
    sar dx, 1                                 ; d1 fa                       ; 0xc196c
    add dx, ax                                ; 01 c2                       ; 0xc196e
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc1970
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc1973 vgabios.c:1253
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1976
    cwd                                       ; 99                          ; 0xc197a
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc197b
    sar ax, 1                                 ; d1 f8                       ; 0xc197d
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc197f
    cmp bx, ax                                ; 39 c3                       ; 0xc1983
    jnl short 019d6h                          ; 7d 4f                       ; 0xc1985
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1987 vgabios.c:1255
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc198b
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc198e
    imul bx, ax                               ; 0f af d8                    ; 0xc1992
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc1995
    add si, bx                                ; 01 de                       ; 0xc1998
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc199a
    add di, bx                                ; 01 df                       ; 0xc199d
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc199f
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc19a2
    mov es, dx                                ; 8e c2                       ; 0xc19a5
    jcxz 019afh                               ; e3 06                       ; 0xc19a7
    push DS                                   ; 1e                          ; 0xc19a9
    mov ds, dx                                ; 8e da                       ; 0xc19aa
    rep movsb                                 ; f3 a4                       ; 0xc19ac
    pop DS                                    ; 1f                          ; 0xc19ae
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc19af vgabios.c:1256
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc19b2
    add si, bx                                ; 01 de                       ; 0xc19b6
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc19b8
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc19bb
    add di, bx                                ; 01 df                       ; 0xc19bf
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc19c1
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc19c4
    mov es, dx                                ; 8e c2                       ; 0xc19c7
    jcxz 019d1h                               ; e3 06                       ; 0xc19c9
    push DS                                   ; 1e                          ; 0xc19cb
    mov ds, dx                                ; 8e da                       ; 0xc19cc
    rep movsb                                 ; f3 a4                       ; 0xc19ce
    pop DS                                    ; 1f                          ; 0xc19d0
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc19d1 vgabios.c:1257
    jmp short 01976h                          ; eb a0                       ; 0xc19d4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc19d6 vgabios.c:1258
    pop di                                    ; 5f                          ; 0xc19d9
    pop si                                    ; 5e                          ; 0xc19da
    pop bp                                    ; 5d                          ; 0xc19db
    retn 00004h                               ; c2 04 00                    ; 0xc19dc
  ; disGetNextSymbol 0xc19df LB 0x297b -> off=0x0 cb=0000000000000081 uValue=00000000000c19df 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc19df LB 0x81
    push bp                                   ; 55                          ; 0xc19df vgabios.c:1261
    mov bp, sp                                ; 89 e5                       ; 0xc19e0
    push si                                   ; 56                          ; 0xc19e2
    push di                                   ; 57                          ; 0xc19e3
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc19e4
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc19e7
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc19ea
    movzx bx, dl                              ; 0f b6 da                    ; 0xc19ed vgabios.c:1267
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc19f0
    imul bx, dx                               ; 0f af da                    ; 0xc19f4
    movzx dx, cl                              ; 0f b6 d1                    ; 0xc19f7
    imul dx, bx                               ; 0f af d3                    ; 0xc19fa
    sar dx, 1                                 ; d1 fa                       ; 0xc19fd
    xor ah, ah                                ; 30 e4                       ; 0xc19ff
    add dx, ax                                ; 01 c2                       ; 0xc1a01
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc1a03
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc1a06 vgabios.c:1268
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1a09
    cwd                                       ; 99                          ; 0xc1a0d
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1a0e
    sar ax, 1                                 ; d1 f8                       ; 0xc1a10
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc1a12
    cmp dx, ax                                ; 39 c2                       ; 0xc1a16
    jnl short 01a57h                          ; 7d 3d                       ; 0xc1a18
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6                 ; 0xc1a1a vgabios.c:1270
    movzx bx, byte [bp+006h]                  ; 0f b6 5e 06                 ; 0xc1a1e
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1a22
    imul dx, ax                               ; 0f af d0                    ; 0xc1a26
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1a29
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc1a2c
    add di, dx                                ; 01 d7                       ; 0xc1a2f
    mov cx, si                                ; 89 f1                       ; 0xc1a31
    mov ax, bx                                ; 89 d8                       ; 0xc1a33
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc1a35
    mov es, dx                                ; 8e c2                       ; 0xc1a38
    jcxz 01a3eh                               ; e3 02                       ; 0xc1a3a
    rep stosb                                 ; f3 aa                       ; 0xc1a3c
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc1a3e vgabios.c:1271
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc1a41
    add di, word [bp-00eh]                    ; 03 7e f2                    ; 0xc1a45
    mov cx, si                                ; 89 f1                       ; 0xc1a48
    mov ax, bx                                ; 89 d8                       ; 0xc1a4a
    mov es, dx                                ; 8e c2                       ; 0xc1a4c
    jcxz 01a52h                               ; e3 02                       ; 0xc1a4e
    rep stosb                                 ; f3 aa                       ; 0xc1a50
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1a52 vgabios.c:1272
    jmp short 01a09h                          ; eb b2                       ; 0xc1a55
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1a57 vgabios.c:1273
    pop di                                    ; 5f                          ; 0xc1a5a
    pop si                                    ; 5e                          ; 0xc1a5b
    pop bp                                    ; 5d                          ; 0xc1a5c
    retn 00004h                               ; c2 04 00                    ; 0xc1a5d
  ; disGetNextSymbol 0xc1a60 LB 0x28fa -> off=0x0 cb=0000000000000079 uValue=00000000000c1a60 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc1a60 LB 0x79
    push bp                                   ; 55                          ; 0xc1a60 vgabios.c:1276
    mov bp, sp                                ; 89 e5                       ; 0xc1a61
    push si                                   ; 56                          ; 0xc1a63
    push di                                   ; 57                          ; 0xc1a64
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc1a65
    mov ah, al                                ; 88 c4                       ; 0xc1a68
    mov al, bl                                ; 88 d8                       ; 0xc1a6a
    mov bx, cx                                ; 89 cb                       ; 0xc1a6c
    xor dh, dh                                ; 30 f6                       ; 0xc1a6e vgabios.c:1282
    movzx di, byte [bp+006h]                  ; 0f b6 7e 06                 ; 0xc1a70
    imul dx, di                               ; 0f af d7                    ; 0xc1a74
    imul dx, word [bp+004h]                   ; 0f af 56 04                 ; 0xc1a77
    movzx si, ah                              ; 0f b6 f4                    ; 0xc1a7b
    add dx, si                                ; 01 f2                       ; 0xc1a7e
    sal dx, 003h                              ; c1 e2 03                    ; 0xc1a80
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc1a83
    xor ah, ah                                ; 30 e4                       ; 0xc1a86 vgabios.c:1283
    imul ax, di                               ; 0f af c7                    ; 0xc1a88
    imul ax, word [bp+004h]                   ; 0f af 46 04                 ; 0xc1a8b
    add si, ax                                ; 01 c6                       ; 0xc1a8f
    sal si, 003h                              ; c1 e6 03                    ; 0xc1a91
    mov word [bp-00ah], si                    ; 89 76 f6                    ; 0xc1a94
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1a97 vgabios.c:1284
    sal word [bp+004h], 003h                  ; c1 66 04 03                 ; 0xc1a9a vgabios.c:1285
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc1a9e vgabios.c:1286
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1aa2
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc1aa5
    jnc short 01ad0h                          ; 73 26                       ; 0xc1aa8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1aaa vgabios.c:1288
    imul ax, word [bp+004h]                   ; 0f af 46 04                 ; 0xc1aae
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc1ab2
    add si, ax                                ; 01 c6                       ; 0xc1ab5
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc1ab7
    add di, ax                                ; 01 c7                       ; 0xc1aba
    mov cx, bx                                ; 89 d9                       ; 0xc1abc
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1abe
    mov es, dx                                ; 8e c2                       ; 0xc1ac1
    jcxz 01acbh                               ; e3 06                       ; 0xc1ac3
    push DS                                   ; 1e                          ; 0xc1ac5
    mov ds, dx                                ; 8e da                       ; 0xc1ac6
    rep movsb                                 ; f3 a4                       ; 0xc1ac8
    pop DS                                    ; 1f                          ; 0xc1aca
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1acb vgabios.c:1289
    jmp short 01aa2h                          ; eb d2                       ; 0xc1ace
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1ad0 vgabios.c:1290
    pop di                                    ; 5f                          ; 0xc1ad3
    pop si                                    ; 5e                          ; 0xc1ad4
    pop bp                                    ; 5d                          ; 0xc1ad5
    retn 00004h                               ; c2 04 00                    ; 0xc1ad6
  ; disGetNextSymbol 0xc1ad9 LB 0x2881 -> off=0x0 cb=000000000000005c uValue=00000000000c1ad9 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc1ad9 LB 0x5c
    push bp                                   ; 55                          ; 0xc1ad9 vgabios.c:1293
    mov bp, sp                                ; 89 e5                       ; 0xc1ada
    push si                                   ; 56                          ; 0xc1adc
    push di                                   ; 57                          ; 0xc1add
    push ax                                   ; 50                          ; 0xc1ade
    push ax                                   ; 50                          ; 0xc1adf
    mov si, bx                                ; 89 de                       ; 0xc1ae0
    mov bx, cx                                ; 89 cb                       ; 0xc1ae2
    xor dh, dh                                ; 30 f6                       ; 0xc1ae4 vgabios.c:1299
    movzx di, byte [bp+004h]                  ; 0f b6 7e 04                 ; 0xc1ae6
    imul dx, di                               ; 0f af d7                    ; 0xc1aea
    imul dx, cx                               ; 0f af d1                    ; 0xc1aed
    xor ah, ah                                ; 30 e4                       ; 0xc1af0
    add ax, dx                                ; 01 d0                       ; 0xc1af2
    sal ax, 003h                              ; c1 e0 03                    ; 0xc1af4
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc1af7
    sal si, 003h                              ; c1 e6 03                    ; 0xc1afa vgabios.c:1300
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1afd vgabios.c:1301
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc1b00 vgabios.c:1302
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc1b04
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc1b07
    jnc short 01b2ch                          ; 73 20                       ; 0xc1b0a
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1b0c vgabios.c:1304
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc1b10
    imul dx, bx                               ; 0f af d3                    ; 0xc1b14
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc1b17
    add di, dx                                ; 01 d7                       ; 0xc1b1a
    mov cx, si                                ; 89 f1                       ; 0xc1b1c
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1b1e
    mov es, dx                                ; 8e c2                       ; 0xc1b21
    jcxz 01b27h                               ; e3 02                       ; 0xc1b23
    rep stosb                                 ; f3 aa                       ; 0xc1b25
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1b27 vgabios.c:1305
    jmp short 01b04h                          ; eb d8                       ; 0xc1b2a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1b2c vgabios.c:1306
    pop di                                    ; 5f                          ; 0xc1b2f
    pop si                                    ; 5e                          ; 0xc1b30
    pop bp                                    ; 5d                          ; 0xc1b31
    retn 00004h                               ; c2 04 00                    ; 0xc1b32
  ; disGetNextSymbol 0xc1b35 LB 0x2825 -> off=0x0 cb=0000000000000628 uValue=00000000000c1b35 'biosfn_scroll'
biosfn_scroll:                               ; 0xc1b35 LB 0x628
    push bp                                   ; 55                          ; 0xc1b35 vgabios.c:1309
    mov bp, sp                                ; 89 e5                       ; 0xc1b36
    push si                                   ; 56                          ; 0xc1b38
    push di                                   ; 57                          ; 0xc1b39
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1b3a
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1b3d
    mov byte [bp-012h], dl                    ; 88 56 ee                    ; 0xc1b40
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc1b43
    mov byte [bp-010h], cl                    ; 88 4e f0                    ; 0xc1b46
    mov dh, byte [bp+006h]                    ; 8a 76 06                    ; 0xc1b49
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1b4c vgabios.c:1318
    jnbe near 02154h                          ; 0f 87 01 06                 ; 0xc1b4f
    cmp dh, cl                                ; 38 ce                       ; 0xc1b53 vgabios.c:1319
    jc near 02154h                            ; 0f 82 fb 05                 ; 0xc1b55
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1b59 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1b5c
    mov es, ax                                ; 8e c0                       ; 0xc1b5f
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1b61
    xor ah, ah                                ; 30 e4                       ; 0xc1b64 vgabios.c:1323
    call 036a6h                               ; e8 3d 1b                    ; 0xc1b66
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1b69
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1b6c vgabios.c:1324
    je near 02154h                            ; 0f 84 e2 05                 ; 0xc1b6e
    mov bx, 00084h                            ; bb 84 00                    ; 0xc1b72 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1b75
    mov es, ax                                ; 8e c0                       ; 0xc1b78
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1b7a
    movzx cx, al                              ; 0f b6 c8                    ; 0xc1b7d vgabios.c:48
    inc cx                                    ; 41                          ; 0xc1b80
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1b81 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc1b84
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1b87 vgabios.c:58
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc1b8a vgabios.c:1331
    jne short 01b99h                          ; 75 09                       ; 0xc1b8e
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1b90 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1b93
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc1b96 vgabios.c:48
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1b99 vgabios.c:1334
    cmp ax, cx                                ; 39 c8                       ; 0xc1b9d
    jc short 01ba8h                           ; 72 07                       ; 0xc1b9f
    mov al, cl                                ; 88 c8                       ; 0xc1ba1
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc1ba3
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc1ba5
    movzx ax, dh                              ; 0f b6 c6                    ; 0xc1ba8 vgabios.c:1335
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc1bab
    jc short 01bb5h                           ; 72 05                       ; 0xc1bae
    mov dh, byte [bp-014h]                    ; 8a 76 ec                    ; 0xc1bb0
    db  0feh, 0ceh
    ; dec dh                                    ; fe ce                     ; 0xc1bb3
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1bb5 vgabios.c:1336
    cmp ax, cx                                ; 39 c8                       ; 0xc1bb9
    jbe short 01bc1h                          ; 76 04                       ; 0xc1bbb
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc1bbd
    mov al, dh                                ; 88 f0                       ; 0xc1bc1 vgabios.c:1337
    sub al, byte [bp-010h]                    ; 2a 46 f0                    ; 0xc1bc3
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc1bc6
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc1bc8
    movzx di, byte [bp-006h]                  ; 0f b6 7e fa                 ; 0xc1bcb vgabios.c:1339
    mov bx, di                                ; 89 fb                       ; 0xc1bcf
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1bd1
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1bd4
    dec ax                                    ; 48                          ; 0xc1bd7
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc1bd8
    mov ax, cx                                ; 89 c8                       ; 0xc1bdb
    dec ax                                    ; 48                          ; 0xc1bdd
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc1bde
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1be1
    imul ax, cx                               ; 0f af c1                    ; 0xc1be4
    cmp byte [bx+047b5h], 000h                ; 80 bf b5 47 00              ; 0xc1be7
    jne near 01d8bh                           ; 0f 85 9b 01                 ; 0xc1bec
    mov cx, ax                                ; 89 c1                       ; 0xc1bf0 vgabios.c:1342
    add cx, ax                                ; 01 c1                       ; 0xc1bf2
    or cl, 0ffh                               ; 80 c9 ff                    ; 0xc1bf4
    movzx si, byte [bp+008h]                  ; 0f b6 76 08                 ; 0xc1bf7
    inc cx                                    ; 41                          ; 0xc1bfb
    imul cx, si                               ; 0f af ce                    ; 0xc1bfc
    mov word [bp-01ch], cx                    ; 89 4e e4                    ; 0xc1bff
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1c02 vgabios.c:1347
    jne short 01c43h                          ; 75 3b                       ; 0xc1c06
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1c08
    jne short 01c43h                          ; 75 35                       ; 0xc1c0c
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1c0e
    jne short 01c43h                          ; 75 2f                       ; 0xc1c12
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1c14
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc1c18
    jne short 01c43h                          ; 75 26                       ; 0xc1c1b
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc1c1d
    cmp dx, word [bp-018h]                    ; 3b 56 e8                    ; 0xc1c20
    jne short 01c43h                          ; 75 1e                       ; 0xc1c23
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc1c25 vgabios.c:1349
    sal dx, 008h                              ; c1 e2 08                    ; 0xc1c29
    add dx, strict byte 00020h                ; 83 c2 20                    ; 0xc1c2c
    mov bx, word [bx+047b8h]                  ; 8b 9f b8 47                 ; 0xc1c2f
    mov cx, ax                                ; 89 c1                       ; 0xc1c33
    mov ax, dx                                ; 89 d0                       ; 0xc1c35
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1c37
    mov es, bx                                ; 8e c3                       ; 0xc1c3a
    jcxz 01c40h                               ; e3 02                       ; 0xc1c3c
    rep stosw                                 ; f3 ab                       ; 0xc1c3e
    jmp near 02154h                           ; e9 11 05                    ; 0xc1c40 vgabios.c:1351
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1c43 vgabios.c:1353
    jne near 01ce0h                           ; 0f 85 95 00                 ; 0xc1c47
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1c4b vgabios.c:1354
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1c4f
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc1c52
    cmp dx, word [bp-01ah]                    ; 3b 56 e6                    ; 0xc1c56
    jc near 02154h                            ; 0f 82 f7 04                 ; 0xc1c59
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1c5d vgabios.c:1356
    add ax, word [bp-01ah]                    ; 03 46 e6                    ; 0xc1c61
    cmp ax, dx                                ; 39 d0                       ; 0xc1c64
    jnbe short 01c6eh                         ; 77 06                       ; 0xc1c66
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1c68
    jne short 01ca1h                          ; 75 33                       ; 0xc1c6c
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1c6e vgabios.c:1357
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1c72
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1c76
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1c79
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1c7c
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1c7f
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1c83
    add dx, bx                                ; 01 da                       ; 0xc1c87
    add dx, dx                                ; 01 d2                       ; 0xc1c89
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1c8b
    add di, dx                                ; 01 d7                       ; 0xc1c8e
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1c90
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1c94
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc1c97
    jcxz 01c9fh                               ; e3 02                       ; 0xc1c9b
    rep stosw                                 ; f3 ab                       ; 0xc1c9d
    jmp short 01cdah                          ; eb 39                       ; 0xc1c9f vgabios.c:1358
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1ca1 vgabios.c:1359
    mov si, ax                                ; 89 c6                       ; 0xc1ca5
    imul si, word [bp-014h]                   ; 0f af 76 ec                 ; 0xc1ca7
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1cab
    add si, dx                                ; 01 d6                       ; 0xc1caf
    add si, si                                ; 01 f6                       ; 0xc1cb1
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1cb3
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1cb7
    mov ax, word [bx+047b8h]                  ; 8b 87 b8 47                 ; 0xc1cba
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1cbe
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1cc1
    mov di, dx                                ; 89 d7                       ; 0xc1cc5
    add di, bx                                ; 01 df                       ; 0xc1cc7
    add di, di                                ; 01 ff                       ; 0xc1cc9
    add di, word [bp-01ch]                    ; 03 7e e4                    ; 0xc1ccb
    mov dx, ax                                ; 89 c2                       ; 0xc1cce
    mov es, ax                                ; 8e c0                       ; 0xc1cd0
    jcxz 01cdah                               ; e3 06                       ; 0xc1cd2
    push DS                                   ; 1e                          ; 0xc1cd4
    mov ds, dx                                ; 8e da                       ; 0xc1cd5
    rep movsw                                 ; f3 a5                       ; 0xc1cd7
    pop DS                                    ; 1f                          ; 0xc1cd9
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1cda vgabios.c:1360
    jmp near 01c52h                           ; e9 72 ff                    ; 0xc1cdd
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1ce0 vgabios.c:1363
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1ce4
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1ce7
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1ceb
    jnbe near 02154h                          ; 0f 87 62 04                 ; 0xc1cee
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1cf2 vgabios.c:1365
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1cf6
    add ax, dx                                ; 01 d0                       ; 0xc1cfa
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1cfc
    jnbe short 01d07h                         ; 77 06                       ; 0xc1cff
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d01
    jne short 01d3ah                          ; 75 33                       ; 0xc1d05
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1d07 vgabios.c:1366
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1d0b
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1d0f
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1d12
    mov dx, word [bp-01ah]                    ; 8b 56 e6                    ; 0xc1d15
    imul dx, word [bp-014h]                   ; 0f af 56 ec                 ; 0xc1d18
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc1d1c
    add dx, bx                                ; 01 da                       ; 0xc1d20
    add dx, dx                                ; 01 d2                       ; 0xc1d22
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1d24
    add di, dx                                ; 01 d7                       ; 0xc1d27
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1d29
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1d2d
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc1d30
    jcxz 01d38h                               ; e3 02                       ; 0xc1d34
    rep stosw                                 ; f3 ab                       ; 0xc1d36
    jmp short 01d7ah                          ; eb 40                       ; 0xc1d38 vgabios.c:1367
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1d3a vgabios.c:1368
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1d3e
    mov si, word [bp-01ah]                    ; 8b 76 e6                    ; 0xc1d42
    sub si, ax                                ; 29 c6                       ; 0xc1d45
    imul si, word [bp-014h]                   ; 0f af 76 ec                 ; 0xc1d47
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1d4b
    add si, dx                                ; 01 d6                       ; 0xc1d4f
    add si, si                                ; 01 f6                       ; 0xc1d51
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1d53
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1d57
    mov ax, word [bx+047b8h]                  ; 8b 87 b8 47                 ; 0xc1d5a
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1d5e
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1d61
    add dx, bx                                ; 01 da                       ; 0xc1d65
    add dx, dx                                ; 01 d2                       ; 0xc1d67
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1d69
    add di, dx                                ; 01 d7                       ; 0xc1d6c
    mov dx, ax                                ; 89 c2                       ; 0xc1d6e
    mov es, ax                                ; 8e c0                       ; 0xc1d70
    jcxz 01d7ah                               ; e3 06                       ; 0xc1d72
    push DS                                   ; 1e                          ; 0xc1d74
    mov ds, dx                                ; 8e da                       ; 0xc1d75
    rep movsw                                 ; f3 a5                       ; 0xc1d77
    pop DS                                    ; 1f                          ; 0xc1d79
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1d7a vgabios.c:1369
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1d7e
    jc near 02154h                            ; 0f 82 cf 03                 ; 0xc1d81
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1d85 vgabios.c:1370
    jmp near 01ce7h                           ; e9 5c ff                    ; 0xc1d88
    movzx di, byte [di+04834h]                ; 0f b6 bd 34 48              ; 0xc1d8b vgabios.c:1376
    sal di, 006h                              ; c1 e7 06                    ; 0xc1d90
    mov dl, byte [di+0484ah]                  ; 8a 95 4a 48                 ; 0xc1d93
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc1d97
    mov dl, byte [bx+047b6h]                  ; 8a 97 b6 47                 ; 0xc1d9a vgabios.c:1377
    cmp dl, 003h                              ; 80 fa 03                    ; 0xc1d9e
    jc short 01db4h                           ; 72 11                       ; 0xc1da1
    jbe short 01dbeh                          ; 76 19                       ; 0xc1da3
    cmp dl, 005h                              ; 80 fa 05                    ; 0xc1da5
    je near 02037h                            ; 0f 84 8b 02                 ; 0xc1da8
    cmp dl, 004h                              ; 80 fa 04                    ; 0xc1dac
    je short 01dbeh                           ; 74 0d                       ; 0xc1daf
    jmp near 02154h                           ; e9 a0 03                    ; 0xc1db1
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc1db4
    je near 01efdh                            ; 0f 84 42 01                 ; 0xc1db7
    jmp near 02154h                           ; e9 96 03                    ; 0xc1dbb
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1dbe vgabios.c:1381
    jne short 01e16h                          ; 75 52                       ; 0xc1dc2
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1dc4
    jne short 01e16h                          ; 75 4c                       ; 0xc1dc8
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1dca
    jne short 01e16h                          ; 75 46                       ; 0xc1dce
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc1dd0
    mov ax, cx                                ; 89 c8                       ; 0xc1dd4
    dec ax                                    ; 48                          ; 0xc1dd6
    cmp bx, ax                                ; 39 c3                       ; 0xc1dd7
    jne short 01e16h                          ; 75 3b                       ; 0xc1dd9
    movzx ax, dh                              ; 0f b6 c6                    ; 0xc1ddb
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc1dde
    dec dx                                    ; 4a                          ; 0xc1de1
    cmp ax, dx                                ; 39 d0                       ; 0xc1de2
    jne short 01e16h                          ; 75 30                       ; 0xc1de4
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1de6 vgabios.c:1383
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1de9
    out DX, ax                                ; ef                          ; 0xc1dec
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1ded vgabios.c:1384
    imul ax, cx                               ; 0f af c1                    ; 0xc1df0
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc1df3
    imul cx, ax                               ; 0f af c8                    ; 0xc1df7
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1dfa
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1dfe
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1e02
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc1e05
    xor di, di                                ; 31 ff                       ; 0xc1e09
    jcxz 01e0fh                               ; e3 02                       ; 0xc1e0b
    rep stosb                                 ; f3 aa                       ; 0xc1e0d
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1e0f vgabios.c:1385
    out DX, ax                                ; ef                          ; 0xc1e12
    jmp near 02154h                           ; e9 3e 03                    ; 0xc1e13 vgabios.c:1387
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1e16 vgabios.c:1389
    jne short 01e85h                          ; 75 69                       ; 0xc1e1a
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1e1c vgabios.c:1390
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1e20
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1e23
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1e27
    jc near 02154h                            ; 0f 82 26 03                 ; 0xc1e2a
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1e2e vgabios.c:1392
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc1e32
    cmp dx, ax                                ; 39 c2                       ; 0xc1e35
    jnbe short 01e3fh                         ; 77 06                       ; 0xc1e37
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1e39
    jne short 01e5eh                          ; 75 1f                       ; 0xc1e3d
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1e3f vgabios.c:1393
    push ax                                   ; 50                          ; 0xc1e43
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1e44
    push ax                                   ; 50                          ; 0xc1e48
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1e49
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1e4d
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1e51
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1e55
    call 018dch                               ; e8 80 fa                    ; 0xc1e59
    jmp short 01e80h                          ; eb 22                       ; 0xc1e5c vgabios.c:1394
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1e5e vgabios.c:1395
    push ax                                   ; 50                          ; 0xc1e62
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1e63
    push ax                                   ; 50                          ; 0xc1e67
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1e68
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1e6c
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1e70
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc1e73
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1e76
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1e79
    call 01867h                               ; e8 e7 f9                    ; 0xc1e7d
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1e80 vgabios.c:1396
    jmp short 01e23h                          ; eb 9e                       ; 0xc1e83
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1e85 vgabios.c:1399
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1e89
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1e8c
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1e90
    jnbe near 02154h                          ; 0f 87 bd 02                 ; 0xc1e93
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1e97 vgabios.c:1401
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1e9b
    add ax, dx                                ; 01 d0                       ; 0xc1e9f
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1ea1
    jnbe short 01each                         ; 77 06                       ; 0xc1ea4
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1ea6
    jne short 01ecbh                          ; 75 1f                       ; 0xc1eaa
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1eac vgabios.c:1402
    push ax                                   ; 50                          ; 0xc1eb0
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1eb1
    push ax                                   ; 50                          ; 0xc1eb5
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1eb6
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1eba
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1ebe
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ec2
    call 018dch                               ; e8 13 fa                    ; 0xc1ec6
    jmp short 01eedh                          ; eb 22                       ; 0xc1ec9 vgabios.c:1403
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1ecb vgabios.c:1404
    push ax                                   ; 50                          ; 0xc1ecf
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1ed0
    push ax                                   ; 50                          ; 0xc1ed4
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1ed5
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1ed9
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1edd
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1ee0
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1ee3
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ee6
    call 01867h                               ; e8 7a f9                    ; 0xc1eea
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1eed vgabios.c:1405
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1ef1
    jc near 02154h                            ; 0f 82 5c 02                 ; 0xc1ef4
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1ef8 vgabios.c:1406
    jmp short 01e8ch                          ; eb 8f                       ; 0xc1efb
    mov dl, byte [bx+047b7h]                  ; 8a 97 b7 47                 ; 0xc1efd vgabios.c:1411
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1f01 vgabios.c:1412
    jne short 01f42h                          ; 75 3b                       ; 0xc1f05
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1f07
    jne short 01f42h                          ; 75 35                       ; 0xc1f0b
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1f0d
    jne short 01f42h                          ; 75 2f                       ; 0xc1f11
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1f13
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc1f17
    jne short 01f42h                          ; 75 26                       ; 0xc1f1a
    movzx cx, dh                              ; 0f b6 ce                    ; 0xc1f1c
    cmp cx, word [bp-018h]                    ; 3b 4e e8                    ; 0xc1f1f
    jne short 01f42h                          ; 75 1e                       ; 0xc1f22
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc1f24 vgabios.c:1414
    imul ax, cx                               ; 0f af c1                    ; 0xc1f28
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc1f2b
    imul cx, ax                               ; 0f af c8                    ; 0xc1f2e
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1f31
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc1f35
    xor di, di                                ; 31 ff                       ; 0xc1f39
    jcxz 01f3fh                               ; e3 02                       ; 0xc1f3b
    rep stosb                                 ; f3 aa                       ; 0xc1f3d
    jmp near 02154h                           ; e9 12 02                    ; 0xc1f3f vgabios.c:1416
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc1f42 vgabios.c:1418
    jne short 01f50h                          ; 75 09                       ; 0xc1f45
    sal byte [bp-010h], 1                     ; d0 66 f0                    ; 0xc1f47 vgabios.c:1420
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc1f4a vgabios.c:1421
    sal word [bp-014h], 1                     ; d1 66 ec                    ; 0xc1f4d vgabios.c:1422
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1f50 vgabios.c:1425
    jne short 01fbfh                          ; 75 69                       ; 0xc1f54
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1f56 vgabios.c:1426
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1f5a
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1f5d
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1f61
    jc near 02154h                            ; 0f 82 ec 01                 ; 0xc1f64
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1f68 vgabios.c:1428
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc1f6c
    cmp dx, ax                                ; 39 c2                       ; 0xc1f6f
    jnbe short 01f79h                         ; 77 06                       ; 0xc1f71
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1f73
    jne short 01f98h                          ; 75 1f                       ; 0xc1f77
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1f79 vgabios.c:1429
    push ax                                   ; 50                          ; 0xc1f7d
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1f7e
    push ax                                   ; 50                          ; 0xc1f82
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1f83
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1f87
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1f8b
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1f8f
    call 019dfh                               ; e8 49 fa                    ; 0xc1f93
    jmp short 01fbah                          ; eb 22                       ; 0xc1f96 vgabios.c:1430
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1f98 vgabios.c:1431
    push ax                                   ; 50                          ; 0xc1f9c
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1f9d
    push ax                                   ; 50                          ; 0xc1fa1
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1fa2
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1fa6
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1faa
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc1fad
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1fb0
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1fb3
    call 0193ch                               ; e8 82 f9                    ; 0xc1fb7
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1fba vgabios.c:1432
    jmp short 01f5dh                          ; eb 9e                       ; 0xc1fbd
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1fbf vgabios.c:1435
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1fc3
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1fc6
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1fca
    jnbe near 02154h                          ; 0f 87 83 01                 ; 0xc1fcd
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1fd1 vgabios.c:1437
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1fd5
    add ax, dx                                ; 01 d0                       ; 0xc1fd9
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1fdb
    jnbe short 01fe6h                         ; 77 06                       ; 0xc1fde
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1fe0
    jne short 02005h                          ; 75 1f                       ; 0xc1fe4
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1fe6 vgabios.c:1438
    push ax                                   ; 50                          ; 0xc1fea
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1feb
    push ax                                   ; 50                          ; 0xc1fef
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1ff0
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1ff4
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1ff8
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ffc
    call 019dfh                               ; e8 dc f9                    ; 0xc2000
    jmp short 02027h                          ; eb 22                       ; 0xc2003 vgabios.c:1439
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc2005 vgabios.c:1440
    push ax                                   ; 50                          ; 0xc2009
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc200a
    push ax                                   ; 50                          ; 0xc200e
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc200f
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc2013
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2017
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc201a
    movzx dx, al                              ; 0f b6 d0                    ; 0xc201d
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc2020
    call 0193ch                               ; e8 15 f9                    ; 0xc2024
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc2027 vgabios.c:1441
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc202b
    jc near 02154h                            ; 0f 82 22 01                 ; 0xc202e
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc2032 vgabios.c:1442
    jmp short 01fc6h                          ; eb 8f                       ; 0xc2035
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc2037 vgabios.c:1447
    jne short 02077h                          ; 75 3a                       ; 0xc203b
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc203d
    jne short 02077h                          ; 75 34                       ; 0xc2041
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc2043
    jne short 02077h                          ; 75 2e                       ; 0xc2047
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc2049
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc204d
    jne short 02077h                          ; 75 25                       ; 0xc2050
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc2052
    cmp dx, word [bp-018h]                    ; 3b 56 e8                    ; 0xc2055
    jne short 02077h                          ; 75 1d                       ; 0xc2058
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2                 ; 0xc205a vgabios.c:1449
    mov cx, ax                                ; 89 c1                       ; 0xc205e
    imul cx, dx                               ; 0f af ca                    ; 0xc2060
    sal cx, 003h                              ; c1 e1 03                    ; 0xc2063
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc2066
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc206a
    xor di, di                                ; 31 ff                       ; 0xc206e
    jcxz 02074h                               ; e3 02                       ; 0xc2070
    rep stosb                                 ; f3 aa                       ; 0xc2072
    jmp near 02154h                           ; e9 dd 00                    ; 0xc2074 vgabios.c:1451
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc2077 vgabios.c:1454
    jne short 020e3h                          ; 75 66                       ; 0xc207b
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc207d vgabios.c:1455
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc2081
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc2084
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc2088
    jc near 02154h                            ; 0f 82 c5 00                 ; 0xc208b
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc208f vgabios.c:1457
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc2093
    cmp dx, ax                                ; 39 c2                       ; 0xc2096
    jnbe short 020a0h                         ; 77 06                       ; 0xc2098
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc209a
    jne short 020beh                          ; 75 1e                       ; 0xc209e
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc20a0 vgabios.c:1458
    push ax                                   ; 50                          ; 0xc20a4
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc20a5
    push ax                                   ; 50                          ; 0xc20a9
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc20aa
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc20ae
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc20b2
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc20b6
    call 01ad9h                               ; e8 1d fa                    ; 0xc20b9
    jmp short 020deh                          ; eb 20                       ; 0xc20bc vgabios.c:1459
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc20be vgabios.c:1460
    push ax                                   ; 50                          ; 0xc20c2
    push word [bp-014h]                       ; ff 76 ec                    ; 0xc20c3
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc20c6
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc20ca
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc20ce
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc20d1
    movzx dx, al                              ; 0f b6 d0                    ; 0xc20d4
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc20d7
    call 01a60h                               ; e8 82 f9                    ; 0xc20db
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc20de vgabios.c:1461
    jmp short 02084h                          ; eb a1                       ; 0xc20e1
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc20e3 vgabios.c:1464
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc20e7
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc20ea
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc20ee
    jnbe short 02154h                         ; 77 61                       ; 0xc20f1
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc20f3 vgabios.c:1466
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc20f7
    add ax, dx                                ; 01 d0                       ; 0xc20fb
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc20fd
    jnbe short 02108h                         ; 77 06                       ; 0xc2100
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc2102
    jne short 02126h                          ; 75 1e                       ; 0xc2106
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc2108 vgabios.c:1467
    push ax                                   ; 50                          ; 0xc210c
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc210d
    push ax                                   ; 50                          ; 0xc2111
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc2112
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc2116
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc211a
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc211e
    call 01ad9h                               ; e8 b5 f9                    ; 0xc2121
    jmp short 02146h                          ; eb 20                       ; 0xc2124 vgabios.c:1468
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc2126 vgabios.c:1469
    push ax                                   ; 50                          ; 0xc212a
    push word [bp-014h]                       ; ff 76 ec                    ; 0xc212b
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc212e
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc2132
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2136
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc2139
    movzx dx, al                              ; 0f b6 d0                    ; 0xc213c
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc213f
    call 01a60h                               ; e8 1a f9                    ; 0xc2143
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc2146 vgabios.c:1470
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc214a
    jc short 02154h                           ; 72 05                       ; 0xc214d
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc214f vgabios.c:1471
    jmp short 020eah                          ; eb 96                       ; 0xc2152
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2154 vgabios.c:1482
    pop di                                    ; 5f                          ; 0xc2157
    pop si                                    ; 5e                          ; 0xc2158
    pop bp                                    ; 5d                          ; 0xc2159
    retn 00008h                               ; c2 08 00                    ; 0xc215a
  ; disGetNextSymbol 0xc215d LB 0x21fd -> off=0x0 cb=00000000000000ff uValue=00000000000c215d 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc215d LB 0xff
    push bp                                   ; 55                          ; 0xc215d vgabios.c:1485
    mov bp, sp                                ; 89 e5                       ; 0xc215e
    push si                                   ; 56                          ; 0xc2160
    push di                                   ; 57                          ; 0xc2161
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc2162
    mov ah, al                                ; 88 c4                       ; 0xc2165
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc2167
    mov al, bl                                ; 88 d8                       ; 0xc216a
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc216c vgabios.c:67
    xor si, si                                ; 31 f6                       ; 0xc216f
    mov es, si                                ; 8e c6                       ; 0xc2171
    mov si, word [es:bx]                      ; 26 8b 37                    ; 0xc2173
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc2176
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc217a vgabios.c:68
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc217d
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc2180 vgabios.c:1494
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc2183
    imul bx, cx                               ; 0f af d9                    ; 0xc2187
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc218a
    imul si, bx                               ; 0f af f3                    ; 0xc218e
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2191
    add si, bx                                ; 01 de                       ; 0xc2194
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc2196 vgabios.c:57
    mov di, strict word 00040h                ; bf 40 00                    ; 0xc2199
    mov es, di                                ; 8e c7                       ; 0xc219c
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc219e
    movzx di, byte [bp+008h]                  ; 0f b6 7e 08                 ; 0xc21a1 vgabios.c:58
    imul bx, di                               ; 0f af df                    ; 0xc21a5
    add si, bx                                ; 01 de                       ; 0xc21a8
    movzx ax, ah                              ; 0f b6 c4                    ; 0xc21aa vgabios.c:1496
    imul ax, cx                               ; 0f af c1                    ; 0xc21ad
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc21b0
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc21b3 vgabios.c:1497
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc21b6
    out DX, ax                                ; ef                          ; 0xc21b9
    mov ax, 00205h                            ; b8 05 02                    ; 0xc21ba vgabios.c:1498
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc21bd
    out DX, ax                                ; ef                          ; 0xc21c0
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc21c1 vgabios.c:1499
    je short 021cdh                           ; 74 06                       ; 0xc21c5
    mov ax, 01803h                            ; b8 03 18                    ; 0xc21c7 vgabios.c:1501
    out DX, ax                                ; ef                          ; 0xc21ca
    jmp short 021d1h                          ; eb 04                       ; 0xc21cb vgabios.c:1503
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc21cd vgabios.c:1505
    out DX, ax                                ; ef                          ; 0xc21d0
    xor ch, ch                                ; 30 ed                       ; 0xc21d1 vgabios.c:1507
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc21d3
    jnc short 02244h                          ; 73 6c                       ; 0xc21d6
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc21d8 vgabios.c:1509
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc21db
    imul bx, ax                               ; 0f af d8                    ; 0xc21df
    add bx, si                                ; 01 f3                       ; 0xc21e2
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc21e4 vgabios.c:1510
    jmp short 021fch                          ; eb 12                       ; 0xc21e8
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc21ea vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc21ed
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc21ef
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc21f3 vgabios.c:1523
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc21f6
    jnc short 02240h                          ; 73 44                       ; 0xc21fa
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc21fc
    mov cl, al                                ; 88 c1                       ; 0xc2200
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2202
    sar ax, CL                                ; d3 f8                       ; 0xc2205
    xor ah, ah                                ; 30 e4                       ; 0xc2207
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc2209
    sal ax, 008h                              ; c1 e0 08                    ; 0xc220c
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc220f
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2211
    out DX, ax                                ; ef                          ; 0xc2214
    mov dx, bx                                ; 89 da                       ; 0xc2215
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2217
    call 036cdh                               ; e8 b0 14                    ; 0xc221a
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc221d
    add ax, word [bp-00eh]                    ; 03 46 f2                    ; 0xc2220
    les di, [bp-00ch]                         ; c4 7e f4                    ; 0xc2223
    add di, ax                                ; 01 c7                       ; 0xc2226
    movzx ax, byte [es:di]                    ; 26 0f b6 05                 ; 0xc2228
    test word [bp-010h], ax                   ; 85 46 f0                    ; 0xc222c
    je short 021eah                           ; 74 b9                       ; 0xc222f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2231
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc2234
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc2236
    mov es, di                                ; 8e c7                       ; 0xc2239
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc223b
    jmp short 021f3h                          ; eb b3                       ; 0xc223e
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc2240 vgabios.c:1524
    jmp short 021d3h                          ; eb 8f                       ; 0xc2242
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2244 vgabios.c:1525
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2247
    out DX, ax                                ; ef                          ; 0xc224a
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc224b vgabios.c:1526
    out DX, ax                                ; ef                          ; 0xc224e
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc224f vgabios.c:1527
    out DX, ax                                ; ef                          ; 0xc2252
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2253 vgabios.c:1528
    pop di                                    ; 5f                          ; 0xc2256
    pop si                                    ; 5e                          ; 0xc2257
    pop bp                                    ; 5d                          ; 0xc2258
    retn 00006h                               ; c2 06 00                    ; 0xc2259
  ; disGetNextSymbol 0xc225c LB 0x20fe -> off=0x0 cb=00000000000000dd uValue=00000000000c225c 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc225c LB 0xdd
    push si                                   ; 56                          ; 0xc225c vgabios.c:1531
    push di                                   ; 57                          ; 0xc225d
    enter 00006h, 000h                        ; c8 06 00 00                 ; 0xc225e
    mov di, 05572h                            ; bf 72 55                    ; 0xc2262 vgabios.c:1538
    xor bh, bh                                ; 30 ff                       ; 0xc2265 vgabios.c:1539
    movzx si, byte [bp+00ah]                  ; 0f b6 76 0a                 ; 0xc2267
    imul si, bx                               ; 0f af f3                    ; 0xc226b
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc226e
    imul bx, bx, 00140h                       ; 69 db 40 01                 ; 0xc2271
    add si, bx                                ; 01 de                       ; 0xc2275
    mov word [bp-004h], si                    ; 89 76 fc                    ; 0xc2277
    xor ah, ah                                ; 30 e4                       ; 0xc227a vgabios.c:1540
    sal ax, 003h                              ; c1 e0 03                    ; 0xc227c
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc227f
    xor ah, ah                                ; 30 e4                       ; 0xc2282 vgabios.c:1541
    jmp near 022a2h                           ; e9 1b 00                    ; 0xc2284
    movzx si, ah                              ; 0f b6 f4                    ; 0xc2287 vgabios.c:1556
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc228a
    add si, di                                ; 01 fe                       ; 0xc228d
    mov al, byte [si]                         ; 8a 04                       ; 0xc228f
    mov si, 0b800h                            ; be 00 b8                    ; 0xc2291 vgabios.c:52
    mov es, si                                ; 8e c6                       ; 0xc2294
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2296
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc2299 vgabios.c:1560
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc229b
    jnc near 02333h                           ; 0f 83 91 00                 ; 0xc229e
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc22a2
    sar bx, 1                                 ; d1 fb                       ; 0xc22a5
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc22a7
    add bx, word [bp-004h]                    ; 03 5e fc                    ; 0xc22aa
    test ah, 001h                             ; f6 c4 01                    ; 0xc22ad
    je short 022b5h                           ; 74 03                       ; 0xc22b0
    add bh, 020h                              ; 80 c7 20                    ; 0xc22b2
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc22b5
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc22b7
    jne short 022d5h                          ; 75 18                       ; 0xc22bb
    test dl, dh                               ; 84 f2                       ; 0xc22bd
    je short 02287h                           ; 74 c6                       ; 0xc22bf
    mov si, 0b800h                            ; be 00 b8                    ; 0xc22c1
    mov es, si                                ; 8e c6                       ; 0xc22c4
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc22c6
    movzx si, ah                              ; 0f b6 f4                    ; 0xc22c9
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc22cc
    add si, di                                ; 01 fe                       ; 0xc22cf
    xor al, byte [si]                         ; 32 04                       ; 0xc22d1
    jmp short 02291h                          ; eb bc                       ; 0xc22d3
    test dh, dh                               ; 84 f6                       ; 0xc22d5 vgabios.c:1562
    jbe short 02299h                          ; 76 c0                       ; 0xc22d7
    test dl, 080h                             ; f6 c2 80                    ; 0xc22d9 vgabios.c:1564
    je short 022e8h                           ; 74 0a                       ; 0xc22dc
    mov si, 0b800h                            ; be 00 b8                    ; 0xc22de vgabios.c:47
    mov es, si                                ; 8e c6                       ; 0xc22e1
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc22e3
    jmp short 022eah                          ; eb 02                       ; 0xc22e6 vgabios.c:1568
    xor al, al                                ; 30 c0                       ; 0xc22e8 vgabios.c:1570
    mov byte [bp-002h], 000h                  ; c6 46 fe 00                 ; 0xc22ea vgabios.c:1572
    jmp short 022fdh                          ; eb 0d                       ; 0xc22ee
    or al, ch                                 ; 08 e8                       ; 0xc22f0 vgabios.c:1582
    shr dh, 1                                 ; d0 ee                       ; 0xc22f2 vgabios.c:1585
    inc byte [bp-002h]                        ; fe 46 fe                    ; 0xc22f4 vgabios.c:1586
    cmp byte [bp-002h], 004h                  ; 80 7e fe 04                 ; 0xc22f7
    jnc short 02328h                          ; 73 2b                       ; 0xc22fb
    movzx si, ah                              ; 0f b6 f4                    ; 0xc22fd
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc2300
    add si, di                                ; 01 fe                       ; 0xc2303
    movzx si, byte [si]                       ; 0f b6 34                    ; 0xc2305
    movzx cx, dh                              ; 0f b6 ce                    ; 0xc2308
    test si, cx                               ; 85 ce                       ; 0xc230b
    je short 022f2h                           ; 74 e3                       ; 0xc230d
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc230f
    sub cl, byte [bp-002h]                    ; 2a 4e fe                    ; 0xc2311
    mov ch, dl                                ; 88 d5                       ; 0xc2314
    and ch, 003h                              ; 80 e5 03                    ; 0xc2316
    add cl, cl                                ; 00 c9                       ; 0xc2319
    sal ch, CL                                ; d2 e5                       ; 0xc231b
    mov cl, ch                                ; 88 e9                       ; 0xc231d
    test dl, 080h                             ; f6 c2 80                    ; 0xc231f
    je short 022f0h                           ; 74 cc                       ; 0xc2322
    xor al, ch                                ; 30 e8                       ; 0xc2324
    jmp short 022f2h                          ; eb ca                       ; 0xc2326
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc2328 vgabios.c:52
    mov es, cx                                ; 8e c1                       ; 0xc232b
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc232d
    inc bx                                    ; 43                          ; 0xc2330 vgabios.c:1588
    jmp short 022d5h                          ; eb a2                       ; 0xc2331 vgabios.c:1589
    leave                                     ; c9                          ; 0xc2333 vgabios.c:1592
    pop di                                    ; 5f                          ; 0xc2334
    pop si                                    ; 5e                          ; 0xc2335
    retn 00004h                               ; c2 04 00                    ; 0xc2336
  ; disGetNextSymbol 0xc2339 LB 0x2021 -> off=0x0 cb=0000000000000085 uValue=00000000000c2339 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc2339 LB 0x85
    push si                                   ; 56                          ; 0xc2339 vgabios.c:1595
    push di                                   ; 57                          ; 0xc233a
    enter 00006h, 000h                        ; c8 06 00 00                 ; 0xc233b
    mov dh, dl                                ; 88 d6                       ; 0xc233f
    mov word [bp-002h], 05572h                ; c7 46 fe 72 55              ; 0xc2341 vgabios.c:1602
    movzx si, cl                              ; 0f b6 f1                    ; 0xc2346 vgabios.c:1603
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08                 ; 0xc2349
    imul cx, si                               ; 0f af ce                    ; 0xc234d
    sal cx, 006h                              ; c1 e1 06                    ; 0xc2350
    xor bh, bh                                ; 30 ff                       ; 0xc2353
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2355
    add bx, cx                                ; 01 cb                       ; 0xc2358
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc235a
    xor ah, ah                                ; 30 e4                       ; 0xc235d vgabios.c:1604
    mov si, ax                                ; 89 c6                       ; 0xc235f
    sal si, 003h                              ; c1 e6 03                    ; 0xc2361
    xor al, al                                ; 30 c0                       ; 0xc2364 vgabios.c:1605
    jmp short 0239dh                          ; eb 35                       ; 0xc2366
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc2368 vgabios.c:1609
    jnc short 02397h                          ; 73 2a                       ; 0xc236b
    xor cl, cl                                ; 30 c9                       ; 0xc236d vgabios.c:1611
    movzx bx, al                              ; 0f b6 d8                    ; 0xc236f vgabios.c:1612
    add bx, si                                ; 01 f3                       ; 0xc2372
    add bx, word [bp-002h]                    ; 03 5e fe                    ; 0xc2374
    movzx bx, byte [bx]                       ; 0f b6 1f                    ; 0xc2377
    movzx di, dl                              ; 0f b6 fa                    ; 0xc237a
    test bx, di                               ; 85 fb                       ; 0xc237d
    je short 02383h                           ; 74 02                       ; 0xc237f
    mov cl, dh                                ; 88 f1                       ; 0xc2381 vgabios.c:1614
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc2383 vgabios.c:1616
    add bx, word [bp-006h]                    ; 03 5e fa                    ; 0xc2386
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc2389 vgabios.c:52
    mov es, di                                ; 8e c7                       ; 0xc238c
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc238e
    shr dl, 1                                 ; d0 ea                       ; 0xc2391 vgabios.c:1617
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc2393 vgabios.c:1618
    jmp short 02368h                          ; eb d1                       ; 0xc2395
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2397 vgabios.c:1619
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc2399
    jnc short 023b8h                          ; 73 1b                       ; 0xc239b
    movzx cx, al                              ; 0f b6 c8                    ; 0xc239d
    movzx bx, byte [bp+008h]                  ; 0f b6 5e 08                 ; 0xc23a0
    imul bx, cx                               ; 0f af d9                    ; 0xc23a4
    sal bx, 003h                              ; c1 e3 03                    ; 0xc23a7
    mov cx, word [bp-004h]                    ; 8b 4e fc                    ; 0xc23aa
    add cx, bx                                ; 01 d9                       ; 0xc23ad
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc23af
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc23b2
    xor ah, ah                                ; 30 e4                       ; 0xc23b4
    jmp short 0236dh                          ; eb b5                       ; 0xc23b6
    leave                                     ; c9                          ; 0xc23b8 vgabios.c:1620
    pop di                                    ; 5f                          ; 0xc23b9
    pop si                                    ; 5e                          ; 0xc23ba
    retn 00002h                               ; c2 02 00                    ; 0xc23bb
  ; disGetNextSymbol 0xc23be LB 0x1f9c -> off=0x0 cb=0000000000000165 uValue=00000000000c23be 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc23be LB 0x165
    push bp                                   ; 55                          ; 0xc23be vgabios.c:1623
    mov bp, sp                                ; 89 e5                       ; 0xc23bf
    push si                                   ; 56                          ; 0xc23c1
    push di                                   ; 57                          ; 0xc23c2
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc23c3
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc23c6
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc23c9
    mov byte [bp-012h], bl                    ; 88 5e ee                    ; 0xc23cc
    mov si, cx                                ; 89 ce                       ; 0xc23cf
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc23d1 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc23d4
    mov es, ax                                ; 8e c0                       ; 0xc23d7
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc23d9
    xor ah, ah                                ; 30 e4                       ; 0xc23dc vgabios.c:1631
    call 036a6h                               ; e8 c5 12                    ; 0xc23de
    mov cl, al                                ; 88 c1                       ; 0xc23e1
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc23e3
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc23e6 vgabios.c:1632
    je near 0251ch                            ; 0f 84 30 01                 ; 0xc23e8
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc23ec vgabios.c:1635
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc23ef
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc23f2
    call 00a93h                               ; e8 9b e6                    ; 0xc23f5
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc23f8 vgabios.c:1636
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc23fb
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc23fe
    xor dl, dl                                ; 30 d2                       ; 0xc2401
    shr dx, 008h                              ; c1 ea 08                    ; 0xc2403
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc2406
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2409 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc240c
    mov es, ax                                ; 8e c0                       ; 0xc240f
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2411
    xor ah, ah                                ; 30 e4                       ; 0xc2414 vgabios.c:48
    inc ax                                    ; 40                          ; 0xc2416
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc2417
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc241a vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc241d
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2420 vgabios.c:58
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc2423 vgabios.c:1642
    mov di, bx                                ; 89 df                       ; 0xc2426
    sal di, 003h                              ; c1 e7 03                    ; 0xc2428
    cmp byte [di+047b5h], 000h                ; 80 bd b5 47 00              ; 0xc242b
    jne short 02478h                          ; 75 46                       ; 0xc2430
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc2432 vgabios.c:1645
    imul bx, ax                               ; 0f af d8                    ; 0xc2435
    add bx, bx                                ; 01 db                       ; 0xc2438
    or bl, 0ffh                               ; 80 cb ff                    ; 0xc243a
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc243d
    inc bx                                    ; 43                          ; 0xc2441
    imul bx, cx                               ; 0f af d9                    ; 0xc2442
    xor dh, dh                                ; 30 f6                       ; 0xc2445
    imul ax, dx                               ; 0f af c2                    ; 0xc2447
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc244a
    add ax, dx                                ; 01 d0                       ; 0xc244e
    add ax, ax                                ; 01 c0                       ; 0xc2450
    mov dx, bx                                ; 89 da                       ; 0xc2452
    add dx, ax                                ; 01 c2                       ; 0xc2454
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc2456 vgabios.c:1647
    sal ax, 008h                              ; c1 e0 08                    ; 0xc245a
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc245d
    add ax, bx                                ; 01 d8                       ; 0xc2461
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc2463
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc2466 vgabios.c:1648
    mov es, [di+047b8h]                       ; 8e 85 b8 47                 ; 0xc2469
    mov cx, si                                ; 89 f1                       ; 0xc246d
    mov di, dx                                ; 89 d7                       ; 0xc246f
    jcxz 02475h                               ; e3 02                       ; 0xc2471
    rep stosw                                 ; f3 ab                       ; 0xc2473
    jmp near 0251ch                           ; e9 a4 00                    ; 0xc2475 vgabios.c:1650
    movzx bx, byte [bx+04834h]                ; 0f b6 9f 34 48              ; 0xc2478 vgabios.c:1653
    sal bx, 006h                              ; c1 e3 06                    ; 0xc247d
    mov al, byte [bx+0484ah]                  ; 8a 87 4a 48                 ; 0xc2480
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2484
    mov al, byte [di+047b7h]                  ; 8a 85 b7 47                 ; 0xc2487 vgabios.c:1654
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc248b
    dec si                                    ; 4e                          ; 0xc248e vgabios.c:1655
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc248f
    je near 0251ch                            ; 0f 84 86 00                 ; 0xc2492
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc2496 vgabios.c:1657
    sal bx, 003h                              ; c1 e3 03                    ; 0xc249a
    mov al, byte [bx+047b6h]                  ; 8a 87 b6 47                 ; 0xc249d
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc24a1
    jc short 024b1h                           ; 72 0c                       ; 0xc24a3
    jbe short 024b7h                          ; 76 10                       ; 0xc24a5
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc24a7
    je short 024feh                           ; 74 53                       ; 0xc24a9
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc24ab
    je short 024bbh                           ; 74 0c                       ; 0xc24ad
    jmp short 02516h                          ; eb 65                       ; 0xc24af
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc24b1
    je short 024dfh                           ; 74 2a                       ; 0xc24b3
    jmp short 02516h                          ; eb 5f                       ; 0xc24b5
    or byte [bp-012h], 001h                   ; 80 4e ee 01                 ; 0xc24b7 vgabios.c:1660
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc24bb vgabios.c:1662
    push ax                                   ; 50                          ; 0xc24bf
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc24c0
    push ax                                   ; 50                          ; 0xc24c4
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc24c5
    push ax                                   ; 50                          ; 0xc24c9
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc24ca
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc24ce
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc24d2
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc24d6
    call 0215dh                               ; e8 80 fc                    ; 0xc24da
    jmp short 02516h                          ; eb 37                       ; 0xc24dd vgabios.c:1663
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc24df vgabios.c:1665
    push ax                                   ; 50                          ; 0xc24e3
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc24e4
    push ax                                   ; 50                          ; 0xc24e8
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc24e9
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc24ed
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc24f1
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc24f5
    call 0225ch                               ; e8 60 fd                    ; 0xc24f9
    jmp short 02516h                          ; eb 18                       ; 0xc24fc vgabios.c:1666
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc24fe vgabios.c:1668
    push ax                                   ; 50                          ; 0xc2502
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc2503
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2507
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc250b
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc250f
    call 02339h                               ; e8 23 fe                    ; 0xc2513
    inc byte [bp-010h]                        ; fe 46 f0                    ; 0xc2516 vgabios.c:1675
    jmp near 0248eh                           ; e9 72 ff                    ; 0xc2519 vgabios.c:1676
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc251c vgabios.c:1678
    pop di                                    ; 5f                          ; 0xc251f
    pop si                                    ; 5e                          ; 0xc2520
    pop bp                                    ; 5d                          ; 0xc2521
    retn                                      ; c3                          ; 0xc2522
  ; disGetNextSymbol 0xc2523 LB 0x1e37 -> off=0x0 cb=0000000000000162 uValue=00000000000c2523 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc2523 LB 0x162
    push bp                                   ; 55                          ; 0xc2523 vgabios.c:1681
    mov bp, sp                                ; 89 e5                       ; 0xc2524
    push si                                   ; 56                          ; 0xc2526
    push di                                   ; 57                          ; 0xc2527
    sub sp, strict byte 00016h                ; 83 ec 16                    ; 0xc2528
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc252b
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc252e
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc2531
    mov si, cx                                ; 89 ce                       ; 0xc2534
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2536 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2539
    mov es, ax                                ; 8e c0                       ; 0xc253c
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc253e
    xor ah, ah                                ; 30 e4                       ; 0xc2541 vgabios.c:1689
    call 036a6h                               ; e8 60 11                    ; 0xc2543
    mov cl, al                                ; 88 c1                       ; 0xc2546
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2548
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc254b vgabios.c:1690
    je near 0267eh                            ; 0f 84 2d 01                 ; 0xc254d
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc2551 vgabios.c:1693
    lea bx, [bp-01ah]                         ; 8d 5e e6                    ; 0xc2554
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc2557
    call 00a93h                               ; e8 36 e5                    ; 0xc255a
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc255d vgabios.c:1694
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2560
    mov dx, word [bp-01ah]                    ; 8b 56 e6                    ; 0xc2563
    xor dl, dl                                ; 30 d2                       ; 0xc2566
    shr dx, 008h                              ; c1 ea 08                    ; 0xc2568
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc256b
    mov bx, 00084h                            ; bb 84 00                    ; 0xc256e vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2571
    mov es, ax                                ; 8e c0                       ; 0xc2574
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2576
    xor ah, ah                                ; 30 e4                       ; 0xc2579 vgabios.c:48
    mov di, ax                                ; 89 c7                       ; 0xc257b
    inc di                                    ; 47                          ; 0xc257d
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc257e vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc2581
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2584 vgabios.c:58
    xor ch, ch                                ; 30 ed                       ; 0xc2587 vgabios.c:1700
    mov bx, cx                                ; 89 cb                       ; 0xc2589
    sal bx, 003h                              ; c1 e3 03                    ; 0xc258b
    cmp byte [bx+047b5h], 000h                ; 80 bf b5 47 00              ; 0xc258e
    jne short 025d2h                          ; 75 3d                       ; 0xc2593
    imul di, ax                               ; 0f af f8                    ; 0xc2595 vgabios.c:1703
    add di, di                                ; 01 ff                       ; 0xc2598
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc259a
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc259e
    inc di                                    ; 47                          ; 0xc25a2
    imul bx, di                               ; 0f af df                    ; 0xc25a3
    xor dh, dh                                ; 30 f6                       ; 0xc25a6
    imul ax, dx                               ; 0f af c2                    ; 0xc25a8
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc25ab
    add ax, dx                                ; 01 d0                       ; 0xc25af
    add ax, ax                                ; 01 c0                       ; 0xc25b1
    add bx, ax                                ; 01 c3                       ; 0xc25b3
    dec si                                    ; 4e                          ; 0xc25b5 vgabios.c:1705
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc25b6
    je near 0267eh                            ; 0f 84 c1 00                 ; 0xc25b9
    movzx di, byte [bp-012h]                  ; 0f b6 7e ee                 ; 0xc25bd vgabios.c:1706
    sal di, 003h                              ; c1 e7 03                    ; 0xc25c1
    mov es, [di+047b8h]                       ; 8e 85 b8 47                 ; 0xc25c4 vgabios.c:50
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc25c8
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc25cb
    inc bx                                    ; 43                          ; 0xc25ce vgabios.c:1707
    inc bx                                    ; 43                          ; 0xc25cf
    jmp short 025b5h                          ; eb e3                       ; 0xc25d0 vgabios.c:1708
    mov di, cx                                ; 89 cf                       ; 0xc25d2 vgabios.c:1713
    movzx ax, byte [di+04834h]                ; 0f b6 85 34 48              ; 0xc25d4
    mov di, ax                                ; 89 c7                       ; 0xc25d9
    sal di, 006h                              ; c1 e7 06                    ; 0xc25db
    mov al, byte [di+0484ah]                  ; 8a 85 4a 48                 ; 0xc25de
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc25e2
    mov al, byte [bx+047b7h]                  ; 8a 87 b7 47                 ; 0xc25e5 vgabios.c:1714
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc25e9
    dec si                                    ; 4e                          ; 0xc25ec vgabios.c:1715
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc25ed
    je near 0267eh                            ; 0f 84 8a 00                 ; 0xc25f0
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc25f4 vgabios.c:1717
    sal bx, 003h                              ; c1 e3 03                    ; 0xc25f8
    mov bl, byte [bx+047b6h]                  ; 8a 9f b6 47                 ; 0xc25fb
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc25ff
    jc short 02612h                           ; 72 0e                       ; 0xc2602
    jbe short 02619h                          ; 76 13                       ; 0xc2604
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc2606
    je short 02660h                           ; 74 55                       ; 0xc2609
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc260b
    je short 0261dh                           ; 74 0d                       ; 0xc260e
    jmp short 02678h                          ; eb 66                       ; 0xc2610
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2612
    je short 02641h                           ; 74 2a                       ; 0xc2615
    jmp short 02678h                          ; eb 5f                       ; 0xc2617
    or byte [bp-006h], 001h                   ; 80 4e fa 01                 ; 0xc2619 vgabios.c:1720
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc261d vgabios.c:1722
    push ax                                   ; 50                          ; 0xc2621
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc2622
    push ax                                   ; 50                          ; 0xc2626
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2627
    push ax                                   ; 50                          ; 0xc262b
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc262c
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2630
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2634
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2638
    call 0215dh                               ; e8 1e fb                    ; 0xc263c
    jmp short 02678h                          ; eb 37                       ; 0xc263f vgabios.c:1723
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc2641 vgabios.c:1725
    push ax                                   ; 50                          ; 0xc2645
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2646
    push ax                                   ; 50                          ; 0xc264a
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc264b
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc264f
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2653
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2657
    call 0225ch                               ; e8 fe fb                    ; 0xc265b
    jmp short 02678h                          ; eb 18                       ; 0xc265e vgabios.c:1726
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2660 vgabios.c:1728
    push ax                                   ; 50                          ; 0xc2664
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc2665
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2669
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc266d
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2671
    call 02339h                               ; e8 c1 fc                    ; 0xc2675
    inc byte [bp-010h]                        ; fe 46 f0                    ; 0xc2678 vgabios.c:1735
    jmp near 025ech                           ; e9 6e ff                    ; 0xc267b vgabios.c:1736
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc267e vgabios.c:1738
    pop di                                    ; 5f                          ; 0xc2681
    pop si                                    ; 5e                          ; 0xc2682
    pop bp                                    ; 5d                          ; 0xc2683
    retn                                      ; c3                          ; 0xc2684
  ; disGetNextSymbol 0xc2685 LB 0x1cd5 -> off=0x0 cb=0000000000000165 uValue=00000000000c2685 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc2685 LB 0x165
    push bp                                   ; 55                          ; 0xc2685 vgabios.c:1741
    mov bp, sp                                ; 89 e5                       ; 0xc2686
    push si                                   ; 56                          ; 0xc2688
    push ax                                   ; 50                          ; 0xc2689
    push ax                                   ; 50                          ; 0xc268a
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc268b
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc268e
    mov dx, bx                                ; 89 da                       ; 0xc2691
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2693 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2696
    mov es, ax                                ; 8e c0                       ; 0xc2699
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc269b
    xor ah, ah                                ; 30 e4                       ; 0xc269e vgabios.c:1748
    call 036a6h                               ; e8 03 10                    ; 0xc26a0
    mov ah, al                                ; 88 c4                       ; 0xc26a3
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc26a5 vgabios.c:1749
    je near 027c5h                            ; 0f 84 1a 01                 ; 0xc26a7
    movzx bx, al                              ; 0f b6 d8                    ; 0xc26ab vgabios.c:1750
    sal bx, 003h                              ; c1 e3 03                    ; 0xc26ae
    cmp byte [bx+047b5h], 000h                ; 80 bf b5 47 00              ; 0xc26b1
    je near 027c5h                            ; 0f 84 0b 01                 ; 0xc26b6
    mov al, byte [bx+047b6h]                  ; 8a 87 b6 47                 ; 0xc26ba vgabios.c:1752
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc26be
    jc short 026d1h                           ; 72 0f                       ; 0xc26c0
    jbe short 026d8h                          ; 76 14                       ; 0xc26c2
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc26c4
    je near 027cbh                            ; 0f 84 01 01                 ; 0xc26c6
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc26ca
    je short 026d8h                           ; 74 0a                       ; 0xc26cc
    jmp near 027c5h                           ; e9 f4 00                    ; 0xc26ce
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc26d1
    je short 02747h                           ; 74 72                       ; 0xc26d3
    jmp near 027c5h                           ; e9 ed 00                    ; 0xc26d5
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc26d8 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc26db
    mov es, ax                                ; 8e c0                       ; 0xc26de
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc26e0
    imul ax, cx                               ; 0f af c1                    ; 0xc26e3 vgabios.c:58
    mov bx, dx                                ; 89 d3                       ; 0xc26e6
    shr bx, 003h                              ; c1 eb 03                    ; 0xc26e8
    add bx, ax                                ; 01 c3                       ; 0xc26eb
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc26ed vgabios.c:57
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc26f0
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc26f3 vgabios.c:58
    imul ax, cx                               ; 0f af c1                    ; 0xc26f7
    add bx, ax                                ; 01 c3                       ; 0xc26fa
    mov cl, dl                                ; 88 d1                       ; 0xc26fc vgabios.c:1758
    and cl, 007h                              ; 80 e1 07                    ; 0xc26fe
    mov ax, 00080h                            ; b8 80 00                    ; 0xc2701
    sar ax, CL                                ; d3 f8                       ; 0xc2704
    xor ah, ah                                ; 30 e4                       ; 0xc2706 vgabios.c:1759
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2708
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc270b
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc270d
    out DX, ax                                ; ef                          ; 0xc2710
    mov ax, 00205h                            ; b8 05 02                    ; 0xc2711 vgabios.c:1760
    out DX, ax                                ; ef                          ; 0xc2714
    mov dx, bx                                ; 89 da                       ; 0xc2715 vgabios.c:1761
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2717
    call 036cdh                               ; e8 b0 0f                    ; 0xc271a
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc271d vgabios.c:1762
    je short 0272ah                           ; 74 07                       ; 0xc2721
    mov ax, 01803h                            ; b8 03 18                    ; 0xc2723 vgabios.c:1764
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2726
    out DX, ax                                ; ef                          ; 0xc2729
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc272a vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc272d
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc272f
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2732
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc2735 vgabios.c:1767
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2738
    out DX, ax                                ; ef                          ; 0xc273b
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc273c vgabios.c:1768
    out DX, ax                                ; ef                          ; 0xc273f
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2740 vgabios.c:1769
    out DX, ax                                ; ef                          ; 0xc2743
    jmp near 027c5h                           ; e9 7e 00                    ; 0xc2744 vgabios.c:1770
    mov si, cx                                ; 89 ce                       ; 0xc2747 vgabios.c:1772
    shr si, 1                                 ; d1 ee                       ; 0xc2749
    imul si, si, strict byte 00050h           ; 6b f6 50                    ; 0xc274b
    cmp al, byte [bx+047b7h]                  ; 3a 87 b7 47                 ; 0xc274e
    jne short 0275bh                          ; 75 07                       ; 0xc2752
    mov bx, dx                                ; 89 d3                       ; 0xc2754 vgabios.c:1774
    shr bx, 002h                              ; c1 eb 02                    ; 0xc2756
    jmp short 02760h                          ; eb 05                       ; 0xc2759 vgabios.c:1776
    mov bx, dx                                ; 89 d3                       ; 0xc275b vgabios.c:1778
    shr bx, 003h                              ; c1 eb 03                    ; 0xc275d
    add bx, si                                ; 01 f3                       ; 0xc2760
    test cl, 001h                             ; f6 c1 01                    ; 0xc2762 vgabios.c:1780
    je short 0276ah                           ; 74 03                       ; 0xc2765
    add bh, 020h                              ; 80 c7 20                    ; 0xc2767
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc276a vgabios.c:47
    mov es, cx                                ; 8e c1                       ; 0xc276d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc276f
    movzx si, ah                              ; 0f b6 f4                    ; 0xc2772 vgabios.c:1782
    sal si, 003h                              ; c1 e6 03                    ; 0xc2775
    cmp byte [si+047b7h], 002h                ; 80 bc b7 47 02              ; 0xc2778
    jne short 02796h                          ; 75 17                       ; 0xc277d
    mov ah, dl                                ; 88 d4                       ; 0xc277f vgabios.c:1784
    and ah, 003h                              ; 80 e4 03                    ; 0xc2781
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2784
    sub cl, ah                                ; 28 e1                       ; 0xc2786
    add cl, cl                                ; 00 c9                       ; 0xc2788
    mov dh, byte [bp-006h]                    ; 8a 76 fa                    ; 0xc278a
    and dh, 003h                              ; 80 e6 03                    ; 0xc278d
    sal dh, CL                                ; d2 e6                       ; 0xc2790
    mov DL, strict byte 003h                  ; b2 03                       ; 0xc2792 vgabios.c:1785
    jmp short 027a9h                          ; eb 13                       ; 0xc2794 vgabios.c:1787
    mov ah, dl                                ; 88 d4                       ; 0xc2796 vgabios.c:1789
    and ah, 007h                              ; 80 e4 07                    ; 0xc2798
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc279b
    sub cl, ah                                ; 28 e1                       ; 0xc279d
    mov dh, byte [bp-006h]                    ; 8a 76 fa                    ; 0xc279f
    and dh, 001h                              ; 80 e6 01                    ; 0xc27a2
    sal dh, CL                                ; d2 e6                       ; 0xc27a5
    mov DL, strict byte 001h                  ; b2 01                       ; 0xc27a7 vgabios.c:1790
    sal dl, CL                                ; d2 e2                       ; 0xc27a9
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc27ab vgabios.c:1792
    je short 027b5h                           ; 74 04                       ; 0xc27af
    xor al, dh                                ; 30 f0                       ; 0xc27b1 vgabios.c:1794
    jmp short 027bdh                          ; eb 08                       ; 0xc27b3 vgabios.c:1796
    mov ah, dl                                ; 88 d4                       ; 0xc27b5 vgabios.c:1798
    not ah                                    ; f6 d4                       ; 0xc27b7
    and al, ah                                ; 20 e0                       ; 0xc27b9
    or al, dh                                 ; 08 f0                       ; 0xc27bb vgabios.c:1799
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc27bd vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc27c0
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc27c2
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc27c5 vgabios.c:1802
    pop si                                    ; 5e                          ; 0xc27c8
    pop bp                                    ; 5d                          ; 0xc27c9
    retn                                      ; c3                          ; 0xc27ca
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc27cb vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc27ce
    mov es, ax                                ; 8e c0                       ; 0xc27d1
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc27d3
    sal ax, 003h                              ; c1 e0 03                    ; 0xc27d6 vgabios.c:58
    imul ax, cx                               ; 0f af c1                    ; 0xc27d9
    mov bx, dx                                ; 89 d3                       ; 0xc27dc
    add bx, ax                                ; 01 c3                       ; 0xc27de
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc27e0 vgabios.c:52
    mov es, ax                                ; 8e c0                       ; 0xc27e3
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc27e5
    jmp short 027c2h                          ; eb d8                       ; 0xc27e8
  ; disGetNextSymbol 0xc27ea LB 0x1b70 -> off=0x0 cb=000000000000024a uValue=00000000000c27ea 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc27ea LB 0x24a
    push bp                                   ; 55                          ; 0xc27ea vgabios.c:1815
    mov bp, sp                                ; 89 e5                       ; 0xc27eb
    push si                                   ; 56                          ; 0xc27ed
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc27ee
    mov ch, al                                ; 88 c5                       ; 0xc27f1
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc27f3
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc27f6
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc27f9 vgabios.c:1823
    jne short 0280ch                          ; 75 0e                       ; 0xc27fc
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc27fe vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2801
    mov es, ax                                ; 8e c0                       ; 0xc2804
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2806
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2809 vgabios.c:48
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc280c vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc280f
    mov es, ax                                ; 8e c0                       ; 0xc2812
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2814
    xor ah, ah                                ; 30 e4                       ; 0xc2817 vgabios.c:1828
    call 036a6h                               ; e8 8a 0e                    ; 0xc2819
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc281c
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc281f vgabios.c:1829
    je near 02a2eh                            ; 0f 84 09 02                 ; 0xc2821
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc2825 vgabios.c:1832
    lea bx, [bp-012h]                         ; 8d 5e ee                    ; 0xc2829
    lea dx, [bp-014h]                         ; 8d 56 ec                    ; 0xc282c
    call 00a93h                               ; e8 61 e2                    ; 0xc282f
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc2832 vgabios.c:1833
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2835
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2838
    xor al, al                                ; 30 c0                       ; 0xc283b
    shr ax, 008h                              ; c1 e8 08                    ; 0xc283d
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc2840
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2843 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2846
    mov es, dx                                ; 8e c2                       ; 0xc2849
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc284b
    xor dh, dh                                ; 30 f6                       ; 0xc284e vgabios.c:48
    inc dx                                    ; 42                          ; 0xc2850
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc2851
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2854 vgabios.c:57
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2857
    mov word [bp-010h], dx                    ; 89 56 f0                    ; 0xc285a vgabios.c:58
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc285d vgabios.c:1839
    jc short 02870h                           ; 72 0e                       ; 0xc2860
    jbe short 02879h                          ; 76 15                       ; 0xc2862
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc2864
    je short 0288fh                           ; 74 26                       ; 0xc2867
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc2869
    je short 02887h                           ; 74 19                       ; 0xc286c
    jmp short 02896h                          ; eb 26                       ; 0xc286e
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc2870
    je near 0298ah                            ; 0f 84 13 01                 ; 0xc2873
    jmp short 02896h                          ; eb 1d                       ; 0xc2877
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2879 vgabios.c:1846
    jbe near 0298ah                           ; 0f 86 09 01                 ; 0xc287d
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc2881
    jmp near 0298ah                           ; e9 03 01                    ; 0xc2884 vgabios.c:1847
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2887 vgabios.c:1850
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc2889
    jmp near 0298ah                           ; e9 fb 00                    ; 0xc288c vgabios.c:1851
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc288f vgabios.c:1854
    jmp near 0298ah                           ; e9 f4 00                    ; 0xc2893 vgabios.c:1855
    movzx si, byte [bp-00ch]                  ; 0f b6 76 f4                 ; 0xc2896 vgabios.c:1859
    mov bx, si                                ; 89 f3                       ; 0xc289a
    sal bx, 003h                              ; c1 e3 03                    ; 0xc289c
    cmp byte [bx+047b5h], 000h                ; 80 bf b5 47 00              ; 0xc289f
    jne short 028e9h                          ; 75 43                       ; 0xc28a4
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc28a6 vgabios.c:1862
    imul ax, word [bp-00eh]                   ; 0f af 46 f2                 ; 0xc28a9
    add ax, ax                                ; 01 c0                       ; 0xc28ad
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc28af
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc28b1
    mov si, ax                                ; 89 c6                       ; 0xc28b5
    inc si                                    ; 46                          ; 0xc28b7
    imul si, dx                               ; 0f af f2                    ; 0xc28b8
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc28bb
    imul ax, word [bp-010h]                   ; 0f af 46 f0                 ; 0xc28bf
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc28c3
    add ax, dx                                ; 01 d0                       ; 0xc28c7
    add ax, ax                                ; 01 c0                       ; 0xc28c9
    add si, ax                                ; 01 c6                       ; 0xc28cb
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc28cd vgabios.c:50
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc28d1
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc28d4 vgabios.c:1867
    jne near 02977h                           ; 0f 85 9c 00                 ; 0xc28d7
    inc si                                    ; 46                          ; 0xc28db vgabios.c:1868
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc28dc vgabios.c:50
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc28e0
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc28e3
    jmp near 02977h                           ; e9 8e 00                    ; 0xc28e6 vgabios.c:1870
    movzx si, byte [si+04834h]                ; 0f b6 b4 34 48              ; 0xc28e9 vgabios.c:1873
    sal si, 006h                              ; c1 e6 06                    ; 0xc28ee
    mov ah, byte [si+0484ah]                  ; 8a a4 4a 48                 ; 0xc28f1
    mov dl, byte [bx+047b7h]                  ; 8a 97 b7 47                 ; 0xc28f5 vgabios.c:1874
    mov al, byte [bx+047b6h]                  ; 8a 87 b6 47                 ; 0xc28f9 vgabios.c:1875
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc28fd
    jc short 0290dh                           ; 72 0c                       ; 0xc28ff
    jbe short 02913h                          ; 76 10                       ; 0xc2901
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc2903
    je short 0295eh                           ; 74 57                       ; 0xc2905
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2907
    je short 02917h                           ; 74 0c                       ; 0xc2909
    jmp short 02977h                          ; eb 6a                       ; 0xc290b
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc290d
    je short 0293dh                           ; 74 2c                       ; 0xc290f
    jmp short 02977h                          ; eb 64                       ; 0xc2911
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc2913 vgabios.c:1878
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc2917 vgabios.c:1880
    push dx                                   ; 52                          ; 0xc291b
    movzx ax, ah                              ; 0f b6 c4                    ; 0xc291c
    push ax                                   ; 50                          ; 0xc291f
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc2920
    push ax                                   ; 50                          ; 0xc2924
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc                 ; 0xc2925
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa                 ; 0xc2929
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc292d
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc2931
    mov cx, bx                                ; 89 d9                       ; 0xc2934
    mov bx, si                                ; 89 f3                       ; 0xc2936
    call 0215dh                               ; e8 22 f8                    ; 0xc2938
    jmp short 02977h                          ; eb 3a                       ; 0xc293b vgabios.c:1881
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc293d vgabios.c:1883
    push ax                                   ; 50                          ; 0xc2940
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc2941
    push ax                                   ; 50                          ; 0xc2945
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc2946
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc294a
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc294e
    movzx si, ch                              ; 0f b6 f5                    ; 0xc2952
    mov cx, ax                                ; 89 c1                       ; 0xc2955
    mov ax, si                                ; 89 f0                       ; 0xc2957
    call 0225ch                               ; e8 00 f9                    ; 0xc2959
    jmp short 02977h                          ; eb 19                       ; 0xc295c vgabios.c:1884
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc295e vgabios.c:1886
    push ax                                   ; 50                          ; 0xc2962
    movzx si, byte [bp-004h]                  ; 0f b6 76 fc                 ; 0xc2963
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc2967
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc296b
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc296f
    mov cx, si                                ; 89 f1                       ; 0xc2972
    call 02339h                               ; e8 c2 f9                    ; 0xc2974
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2977 vgabios.c:1894
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc297a vgabios.c:1896
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc297e
    jne short 0298ah                          ; 75 07                       ; 0xc2981
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc2983 vgabios.c:1897
    inc byte [bp-004h]                        ; fe 46 fc                    ; 0xc2987 vgabios.c:1898
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc298a vgabios.c:1903
    cmp ax, word [bp-00eh]                    ; 3b 46 f2                    ; 0xc298e
    jne near 02a12h                           ; 0f 85 7d 00                 ; 0xc2991
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc2995 vgabios.c:1905
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2999
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc299c
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc299f
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc29a1
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc29a4
    cmp byte [bx+047b5h], 000h                ; 80 bf b5 47 00              ; 0xc29a6
    jne short 029f5h                          ; 75 48                       ; 0xc29ab
    mov dx, word [bp-010h]                    ; 8b 56 f0                    ; 0xc29ad vgabios.c:1907
    imul dx, word [bp-00eh]                   ; 0f af 56 f2                 ; 0xc29b0
    add dx, dx                                ; 01 d2                       ; 0xc29b4
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc29b6
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6                 ; 0xc29b9
    inc dx                                    ; 42                          ; 0xc29bd
    imul si, dx                               ; 0f af f2                    ; 0xc29be
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc                 ; 0xc29c1
    dec dx                                    ; 4a                          ; 0xc29c5
    mov cx, word [bp-010h]                    ; 8b 4e f0                    ; 0xc29c6
    imul cx, dx                               ; 0f af ca                    ; 0xc29c9
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc29cc
    add dx, cx                                ; 01 ca                       ; 0xc29d0
    add dx, dx                                ; 01 d2                       ; 0xc29d2
    add si, dx                                ; 01 d6                       ; 0xc29d4
    inc si                                    ; 46                          ; 0xc29d6 vgabios.c:1908
    mov es, [bx+047b8h]                       ; 8e 87 b8 47                 ; 0xc29d7 vgabios.c:45
    mov bl, byte [es:si]                      ; 26 8a 1c                    ; 0xc29db
    push strict byte 00001h                   ; 6a 01                       ; 0xc29de vgabios.c:1909
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc29e0
    push dx                                   ; 52                          ; 0xc29e4
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc29e5
    push dx                                   ; 52                          ; 0xc29e8
    xor ah, ah                                ; 30 e4                       ; 0xc29e9
    push ax                                   ; 50                          ; 0xc29eb
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc29ec
    xor cx, cx                                ; 31 c9                       ; 0xc29ef
    xor bx, bx                                ; 31 db                       ; 0xc29f1
    jmp short 02a09h                          ; eb 14                       ; 0xc29f3 vgabios.c:1911
    push strict byte 00001h                   ; 6a 01                       ; 0xc29f5 vgabios.c:1913
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc29f7
    push dx                                   ; 52                          ; 0xc29fb
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc29fc
    push dx                                   ; 52                          ; 0xc29ff
    xor ah, ah                                ; 30 e4                       ; 0xc2a00
    push ax                                   ; 50                          ; 0xc2a02
    xor cx, cx                                ; 31 c9                       ; 0xc2a03
    xor bx, bx                                ; 31 db                       ; 0xc2a05
    xor dx, dx                                ; 31 d2                       ; 0xc2a07
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2a09
    call 01b35h                               ; e8 26 f1                    ; 0xc2a0c
    dec byte [bp-004h]                        ; fe 4e fc                    ; 0xc2a0f vgabios.c:1915
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc2a12 vgabios.c:1919
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc2a16
    sal word [bp-012h], 008h                  ; c1 66 ee 08                 ; 0xc2a19
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2a1d
    add word [bp-012h], ax                    ; 01 46 ee                    ; 0xc2a21
    mov dx, word [bp-012h]                    ; 8b 56 ee                    ; 0xc2a24 vgabios.c:1920
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc2a27
    call 01242h                               ; e8 14 e8                    ; 0xc2a2b
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2a2e vgabios.c:1921
    pop si                                    ; 5e                          ; 0xc2a31
    pop bp                                    ; 5d                          ; 0xc2a32
    retn                                      ; c3                          ; 0xc2a33
  ; disGetNextSymbol 0xc2a34 LB 0x1926 -> off=0x0 cb=000000000000002c uValue=00000000000c2a34 'get_font_access'
get_font_access:                             ; 0xc2a34 LB 0x2c
    push bp                                   ; 55                          ; 0xc2a34 vgabios.c:1924
    mov bp, sp                                ; 89 e5                       ; 0xc2a35
    push dx                                   ; 52                          ; 0xc2a37
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2a38 vgabios.c:1926
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2a3b
    out DX, ax                                ; ef                          ; 0xc2a3e
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2a3f vgabios.c:1927
    out DX, ax                                ; ef                          ; 0xc2a42
    mov ax, 00704h                            ; b8 04 07                    ; 0xc2a43 vgabios.c:1928
    out DX, ax                                ; ef                          ; 0xc2a46
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2a47 vgabios.c:1929
    out DX, ax                                ; ef                          ; 0xc2a4a
    mov ax, 00204h                            ; b8 04 02                    ; 0xc2a4b vgabios.c:1930
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2a4e
    out DX, ax                                ; ef                          ; 0xc2a51
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2a52 vgabios.c:1931
    out DX, ax                                ; ef                          ; 0xc2a55
    mov ax, 00406h                            ; b8 06 04                    ; 0xc2a56 vgabios.c:1932
    out DX, ax                                ; ef                          ; 0xc2a59
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2a5a vgabios.c:1933
    pop dx                                    ; 5a                          ; 0xc2a5d
    pop bp                                    ; 5d                          ; 0xc2a5e
    retn                                      ; c3                          ; 0xc2a5f
  ; disGetNextSymbol 0xc2a60 LB 0x18fa -> off=0x0 cb=000000000000003c uValue=00000000000c2a60 'release_font_access'
release_font_access:                         ; 0xc2a60 LB 0x3c
    push bp                                   ; 55                          ; 0xc2a60 vgabios.c:1935
    mov bp, sp                                ; 89 e5                       ; 0xc2a61
    push dx                                   ; 52                          ; 0xc2a63
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2a64 vgabios.c:1937
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2a67
    out DX, ax                                ; ef                          ; 0xc2a6a
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2a6b vgabios.c:1938
    out DX, ax                                ; ef                          ; 0xc2a6e
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2a6f vgabios.c:1939
    out DX, ax                                ; ef                          ; 0xc2a72
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2a73 vgabios.c:1940
    out DX, ax                                ; ef                          ; 0xc2a76
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2a77 vgabios.c:1941
    in AL, DX                                 ; ec                          ; 0xc2a7a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2a7b
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2a7d
    sal ax, 002h                              ; c1 e0 02                    ; 0xc2a80
    or AL, strict byte 00ah                   ; 0c 0a                       ; 0xc2a83
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2a85
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2a88
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2a8a
    out DX, ax                                ; ef                          ; 0xc2a8d
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2a8e vgabios.c:1942
    out DX, ax                                ; ef                          ; 0xc2a91
    mov ax, 01005h                            ; b8 05 10                    ; 0xc2a92 vgabios.c:1943
    out DX, ax                                ; ef                          ; 0xc2a95
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2a96 vgabios.c:1944
    pop dx                                    ; 5a                          ; 0xc2a99
    pop bp                                    ; 5d                          ; 0xc2a9a
    retn                                      ; c3                          ; 0xc2a9b
  ; disGetNextSymbol 0xc2a9c LB 0x18be -> off=0x0 cb=00000000000000b4 uValue=00000000000c2a9c 'set_scan_lines'
set_scan_lines:                              ; 0xc2a9c LB 0xb4
    push bp                                   ; 55                          ; 0xc2a9c vgabios.c:1946
    mov bp, sp                                ; 89 e5                       ; 0xc2a9d
    push bx                                   ; 53                          ; 0xc2a9f
    push cx                                   ; 51                          ; 0xc2aa0
    push dx                                   ; 52                          ; 0xc2aa1
    push si                                   ; 56                          ; 0xc2aa2
    push di                                   ; 57                          ; 0xc2aa3
    mov bl, al                                ; 88 c3                       ; 0xc2aa4
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2aa6 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2aa9
    mov es, ax                                ; 8e c0                       ; 0xc2aac
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc2aae
    mov cx, si                                ; 89 f1                       ; 0xc2ab1 vgabios.c:58
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc2ab3 vgabios.c:1952
    mov dx, si                                ; 89 f2                       ; 0xc2ab5
    out DX, AL                                ; ee                          ; 0xc2ab7
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2ab8 vgabios.c:1953
    in AL, DX                                 ; ec                          ; 0xc2abb
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2abc
    mov ah, al                                ; 88 c4                       ; 0xc2abe vgabios.c:1954
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc2ac0
    mov al, bl                                ; 88 d8                       ; 0xc2ac3
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc2ac5
    or al, ah                                 ; 08 e0                       ; 0xc2ac7
    out DX, AL                                ; ee                          ; 0xc2ac9 vgabios.c:1955
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc2aca vgabios.c:1956
    jne short 02ad7h                          ; 75 08                       ; 0xc2acd
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc2acf vgabios.c:1958
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc2ad2
    jmp short 02ae4h                          ; eb 0d                       ; 0xc2ad5 vgabios.c:1960
    mov al, bl                                ; 88 d8                       ; 0xc2ad7 vgabios.c:1962
    sub AL, strict byte 003h                  ; 2c 03                       ; 0xc2ad9
    movzx dx, al                              ; 0f b6 d0                    ; 0xc2adb
    mov al, bl                                ; 88 d8                       ; 0xc2ade
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc2ae0
    xor ah, ah                                ; 30 e4                       ; 0xc2ae2
    call 0114ch                               ; e8 65 e6                    ; 0xc2ae4
    movzx di, bl                              ; 0f b6 fb                    ; 0xc2ae7 vgabios.c:1964
    mov bx, 00085h                            ; bb 85 00                    ; 0xc2aea vgabios.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2aed
    mov es, ax                                ; 8e c0                       ; 0xc2af0
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc2af2
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc2af5 vgabios.c:1965
    mov dx, cx                                ; 89 ca                       ; 0xc2af7
    out DX, AL                                ; ee                          ; 0xc2af9
    mov bx, cx                                ; 89 cb                       ; 0xc2afa vgabios.c:1966
    inc bx                                    ; 43                          ; 0xc2afc
    mov dx, bx                                ; 89 da                       ; 0xc2afd
    in AL, DX                                 ; ec                          ; 0xc2aff
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b00
    mov si, ax                                ; 89 c6                       ; 0xc2b02
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc2b04 vgabios.c:1967
    mov dx, cx                                ; 89 ca                       ; 0xc2b06
    out DX, AL                                ; ee                          ; 0xc2b08
    mov dx, bx                                ; 89 da                       ; 0xc2b09 vgabios.c:1968
    in AL, DX                                 ; ec                          ; 0xc2b0b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2b0c
    mov ah, al                                ; 88 c4                       ; 0xc2b0e vgabios.c:1969
    and ah, 002h                              ; 80 e4 02                    ; 0xc2b10
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc2b13
    sal dx, 007h                              ; c1 e2 07                    ; 0xc2b16
    and AL, strict byte 040h                  ; 24 40                       ; 0xc2b19
    xor ah, ah                                ; 30 e4                       ; 0xc2b1b
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2b1d
    add ax, dx                                ; 01 d0                       ; 0xc2b20
    inc ax                                    ; 40                          ; 0xc2b22
    add ax, si                                ; 01 f0                       ; 0xc2b23
    xor dx, dx                                ; 31 d2                       ; 0xc2b25 vgabios.c:1970
    div di                                    ; f7 f7                       ; 0xc2b27
    mov dl, al                                ; 88 c2                       ; 0xc2b29 vgabios.c:1971
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc2b2b
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2b2d vgabios.c:52
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc2b30
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2b33 vgabios.c:57
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2b36
    xor ah, ah                                ; 30 e4                       ; 0xc2b39 vgabios.c:1973
    imul dx, ax                               ; 0f af d0                    ; 0xc2b3b
    add dx, dx                                ; 01 d2                       ; 0xc2b3e
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc2b40 vgabios.c:62
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc2b43
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc2b46 vgabios.c:1974
    pop di                                    ; 5f                          ; 0xc2b49
    pop si                                    ; 5e                          ; 0xc2b4a
    pop dx                                    ; 5a                          ; 0xc2b4b
    pop cx                                    ; 59                          ; 0xc2b4c
    pop bx                                    ; 5b                          ; 0xc2b4d
    pop bp                                    ; 5d                          ; 0xc2b4e
    retn                                      ; c3                          ; 0xc2b4f
  ; disGetNextSymbol 0xc2b50 LB 0x180a -> off=0x0 cb=0000000000000022 uValue=00000000000c2b50 'biosfn_set_font_block'
biosfn_set_font_block:                       ; 0xc2b50 LB 0x22
    push bp                                   ; 55                          ; 0xc2b50 vgabios.c:1976
    mov bp, sp                                ; 89 e5                       ; 0xc2b51
    push bx                                   ; 53                          ; 0xc2b53
    push dx                                   ; 52                          ; 0xc2b54
    mov bl, al                                ; 88 c3                       ; 0xc2b55
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2b57 vgabios.c:1978
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2b5a
    out DX, ax                                ; ef                          ; 0xc2b5d
    movzx ax, bl                              ; 0f b6 c3                    ; 0xc2b5e vgabios.c:1979
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2b61
    or AL, strict byte 003h                   ; 0c 03                       ; 0xc2b64
    out DX, ax                                ; ef                          ; 0xc2b66
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2b67 vgabios.c:1980
    out DX, ax                                ; ef                          ; 0xc2b6a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2b6b vgabios.c:1981
    pop dx                                    ; 5a                          ; 0xc2b6e
    pop bx                                    ; 5b                          ; 0xc2b6f
    pop bp                                    ; 5d                          ; 0xc2b70
    retn                                      ; c3                          ; 0xc2b71
  ; disGetNextSymbol 0xc2b72 LB 0x17e8 -> off=0x0 cb=000000000000007c uValue=00000000000c2b72 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2b72 LB 0x7c
    push bp                                   ; 55                          ; 0xc2b72 vgabios.c:1983
    mov bp, sp                                ; 89 e5                       ; 0xc2b73
    push si                                   ; 56                          ; 0xc2b75
    push di                                   ; 57                          ; 0xc2b76
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2b77
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2b7a
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc2b7d
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2b80
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc2b83
    call 02a34h                               ; e8 ab fe                    ; 0xc2b86 vgabios.c:1988
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2b89 vgabios.c:1989
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2b8c
    xor ah, ah                                ; 30 e4                       ; 0xc2b8e
    mov bx, ax                                ; 89 c3                       ; 0xc2b90
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2b92
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2b95
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2b98
    xor ah, ah                                ; 30 e4                       ; 0xc2b9a
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2b9c
    add bx, ax                                ; 01 c3                       ; 0xc2b9f
    mov word [bp-00eh], bx                    ; 89 5e f2                    ; 0xc2ba1
    xor bx, bx                                ; 31 db                       ; 0xc2ba4 vgabios.c:1990
    cmp bx, word [bp-00ah]                    ; 3b 5e f6                    ; 0xc2ba6
    jnc short 02bd5h                          ; 73 2a                       ; 0xc2ba9
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08                 ; 0xc2bab vgabios.c:1992
    mov si, bx                                ; 89 de                       ; 0xc2baf
    imul si, cx                               ; 0f af f1                    ; 0xc2bb1
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc2bb4
    mov di, word [bp+004h]                    ; 8b 7e 04                    ; 0xc2bb7 vgabios.c:1993
    add di, bx                                ; 01 df                       ; 0xc2bba
    sal di, 005h                              ; c1 e7 05                    ; 0xc2bbc
    add di, word [bp-00eh]                    ; 03 7e f2                    ; 0xc2bbf
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc2bc2 vgabios.c:1994
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2bc5
    mov es, ax                                ; 8e c0                       ; 0xc2bc8
    jcxz 02bd2h                               ; e3 06                       ; 0xc2bca
    push DS                                   ; 1e                          ; 0xc2bcc
    mov ds, dx                                ; 8e da                       ; 0xc2bcd
    rep movsb                                 ; f3 a4                       ; 0xc2bcf
    pop DS                                    ; 1f                          ; 0xc2bd1
    inc bx                                    ; 43                          ; 0xc2bd2 vgabios.c:1995
    jmp short 02ba6h                          ; eb d1                       ; 0xc2bd3
    call 02a60h                               ; e8 88 fe                    ; 0xc2bd5 vgabios.c:1996
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc2bd8 vgabios.c:1997
    jc short 02be5h                           ; 72 07                       ; 0xc2bdc
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08                 ; 0xc2bde vgabios.c:1999
    call 02a9ch                               ; e8 b7 fe                    ; 0xc2be2
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2be5 vgabios.c:2001
    pop di                                    ; 5f                          ; 0xc2be8
    pop si                                    ; 5e                          ; 0xc2be9
    pop bp                                    ; 5d                          ; 0xc2bea
    retn 00006h                               ; c2 06 00                    ; 0xc2beb
  ; disGetNextSymbol 0xc2bee LB 0x176c -> off=0x0 cb=000000000000006f uValue=00000000000c2bee 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc2bee LB 0x6f
    push bp                                   ; 55                          ; 0xc2bee vgabios.c:2003
    mov bp, sp                                ; 89 e5                       ; 0xc2bef
    push bx                                   ; 53                          ; 0xc2bf1
    push cx                                   ; 51                          ; 0xc2bf2
    push si                                   ; 56                          ; 0xc2bf3
    push di                                   ; 57                          ; 0xc2bf4
    push ax                                   ; 50                          ; 0xc2bf5
    push ax                                   ; 50                          ; 0xc2bf6
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2bf7
    call 02a34h                               ; e8 37 fe                    ; 0xc2bfa vgabios.c:2007
    mov al, dl                                ; 88 d0                       ; 0xc2bfd vgabios.c:2008
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2bff
    xor ah, ah                                ; 30 e4                       ; 0xc2c01
    mov bx, ax                                ; 89 c3                       ; 0xc2c03
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2c05
    mov al, dl                                ; 88 d0                       ; 0xc2c08
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c0a
    xor ah, ah                                ; 30 e4                       ; 0xc2c0c
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2c0e
    add bx, ax                                ; 01 c3                       ; 0xc2c11
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2c13
    xor bx, bx                                ; 31 db                       ; 0xc2c16 vgabios.c:2009
    jmp short 02c20h                          ; eb 06                       ; 0xc2c18
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2c1a
    jnc short 02c45h                          ; 73 25                       ; 0xc2c1e
    imul si, bx, strict byte 0000eh           ; 6b f3 0e                    ; 0xc2c20 vgabios.c:2011
    mov di, bx                                ; 89 df                       ; 0xc2c23 vgabios.c:2012
    sal di, 005h                              ; c1 e7 05                    ; 0xc2c25
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2c28
    add si, 05d72h                            ; 81 c6 72 5d                 ; 0xc2c2b vgabios.c:2013
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2c2f
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2c32
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2c35
    mov es, ax                                ; 8e c0                       ; 0xc2c38
    jcxz 02c42h                               ; e3 06                       ; 0xc2c3a
    push DS                                   ; 1e                          ; 0xc2c3c
    mov ds, dx                                ; 8e da                       ; 0xc2c3d
    rep movsb                                 ; f3 a4                       ; 0xc2c3f
    pop DS                                    ; 1f                          ; 0xc2c41
    inc bx                                    ; 43                          ; 0xc2c42 vgabios.c:2014
    jmp short 02c1ah                          ; eb d5                       ; 0xc2c43
    call 02a60h                               ; e8 18 fe                    ; 0xc2c45 vgabios.c:2015
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2c48 vgabios.c:2016
    jc short 02c54h                           ; 72 06                       ; 0xc2c4c
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2c4e vgabios.c:2018
    call 02a9ch                               ; e8 48 fe                    ; 0xc2c51
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2c54 vgabios.c:2020
    pop di                                    ; 5f                          ; 0xc2c57
    pop si                                    ; 5e                          ; 0xc2c58
    pop cx                                    ; 59                          ; 0xc2c59
    pop bx                                    ; 5b                          ; 0xc2c5a
    pop bp                                    ; 5d                          ; 0xc2c5b
    retn                                      ; c3                          ; 0xc2c5c
  ; disGetNextSymbol 0xc2c5d LB 0x16fd -> off=0x0 cb=0000000000000071 uValue=00000000000c2c5d 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2c5d LB 0x71
    push bp                                   ; 55                          ; 0xc2c5d vgabios.c:2022
    mov bp, sp                                ; 89 e5                       ; 0xc2c5e
    push bx                                   ; 53                          ; 0xc2c60
    push cx                                   ; 51                          ; 0xc2c61
    push si                                   ; 56                          ; 0xc2c62
    push di                                   ; 57                          ; 0xc2c63
    push ax                                   ; 50                          ; 0xc2c64
    push ax                                   ; 50                          ; 0xc2c65
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2c66
    call 02a34h                               ; e8 c8 fd                    ; 0xc2c69 vgabios.c:2026
    mov al, dl                                ; 88 d0                       ; 0xc2c6c vgabios.c:2027
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2c6e
    xor ah, ah                                ; 30 e4                       ; 0xc2c70
    mov bx, ax                                ; 89 c3                       ; 0xc2c72
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2c74
    mov al, dl                                ; 88 d0                       ; 0xc2c77
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2c79
    xor ah, ah                                ; 30 e4                       ; 0xc2c7b
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2c7d
    add bx, ax                                ; 01 c3                       ; 0xc2c80
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2c82
    xor bx, bx                                ; 31 db                       ; 0xc2c85 vgabios.c:2028
    jmp short 02c8fh                          ; eb 06                       ; 0xc2c87
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2c89
    jnc short 02cb6h                          ; 73 27                       ; 0xc2c8d
    mov si, bx                                ; 89 de                       ; 0xc2c8f vgabios.c:2030
    sal si, 003h                              ; c1 e6 03                    ; 0xc2c91
    mov di, bx                                ; 89 df                       ; 0xc2c94 vgabios.c:2031
    sal di, 005h                              ; c1 e7 05                    ; 0xc2c96
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2c99
    add si, 05572h                            ; 81 c6 72 55                 ; 0xc2c9c vgabios.c:2032
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2ca0
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2ca3
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2ca6
    mov es, ax                                ; 8e c0                       ; 0xc2ca9
    jcxz 02cb3h                               ; e3 06                       ; 0xc2cab
    push DS                                   ; 1e                          ; 0xc2cad
    mov ds, dx                                ; 8e da                       ; 0xc2cae
    rep movsb                                 ; f3 a4                       ; 0xc2cb0
    pop DS                                    ; 1f                          ; 0xc2cb2
    inc bx                                    ; 43                          ; 0xc2cb3 vgabios.c:2033
    jmp short 02c89h                          ; eb d3                       ; 0xc2cb4
    call 02a60h                               ; e8 a7 fd                    ; 0xc2cb6 vgabios.c:2034
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2cb9 vgabios.c:2035
    jc short 02cc5h                           ; 72 06                       ; 0xc2cbd
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2cbf vgabios.c:2037
    call 02a9ch                               ; e8 d7 fd                    ; 0xc2cc2
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2cc5 vgabios.c:2039
    pop di                                    ; 5f                          ; 0xc2cc8
    pop si                                    ; 5e                          ; 0xc2cc9
    pop cx                                    ; 59                          ; 0xc2cca
    pop bx                                    ; 5b                          ; 0xc2ccb
    pop bp                                    ; 5d                          ; 0xc2ccc
    retn                                      ; c3                          ; 0xc2ccd
  ; disGetNextSymbol 0xc2cce LB 0x168c -> off=0x0 cb=0000000000000071 uValue=00000000000c2cce 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2cce LB 0x71
    push bp                                   ; 55                          ; 0xc2cce vgabios.c:2042
    mov bp, sp                                ; 89 e5                       ; 0xc2ccf
    push bx                                   ; 53                          ; 0xc2cd1
    push cx                                   ; 51                          ; 0xc2cd2
    push si                                   ; 56                          ; 0xc2cd3
    push di                                   ; 57                          ; 0xc2cd4
    push ax                                   ; 50                          ; 0xc2cd5
    push ax                                   ; 50                          ; 0xc2cd6
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2cd7
    call 02a34h                               ; e8 57 fd                    ; 0xc2cda vgabios.c:2046
    mov al, dl                                ; 88 d0                       ; 0xc2cdd vgabios.c:2047
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2cdf
    xor ah, ah                                ; 30 e4                       ; 0xc2ce1
    mov bx, ax                                ; 89 c3                       ; 0xc2ce3
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2ce5
    mov al, dl                                ; 88 d0                       ; 0xc2ce8
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2cea
    xor ah, ah                                ; 30 e4                       ; 0xc2cec
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2cee
    add bx, ax                                ; 01 c3                       ; 0xc2cf1
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2cf3
    xor bx, bx                                ; 31 db                       ; 0xc2cf6 vgabios.c:2048
    jmp short 02d00h                          ; eb 06                       ; 0xc2cf8
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2cfa
    jnc short 02d27h                          ; 73 27                       ; 0xc2cfe
    mov si, bx                                ; 89 de                       ; 0xc2d00 vgabios.c:2050
    sal si, 004h                              ; c1 e6 04                    ; 0xc2d02
    mov di, bx                                ; 89 df                       ; 0xc2d05 vgabios.c:2051
    sal di, 005h                              ; c1 e7 05                    ; 0xc2d07
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2d0a
    add si, 06b72h                            ; 81 c6 72 6b                 ; 0xc2d0d vgabios.c:2052
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2d11
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2d14
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2d17
    mov es, ax                                ; 8e c0                       ; 0xc2d1a
    jcxz 02d24h                               ; e3 06                       ; 0xc2d1c
    push DS                                   ; 1e                          ; 0xc2d1e
    mov ds, dx                                ; 8e da                       ; 0xc2d1f
    rep movsb                                 ; f3 a4                       ; 0xc2d21
    pop DS                                    ; 1f                          ; 0xc2d23
    inc bx                                    ; 43                          ; 0xc2d24 vgabios.c:2053
    jmp short 02cfah                          ; eb d3                       ; 0xc2d25
    call 02a60h                               ; e8 36 fd                    ; 0xc2d27 vgabios.c:2054
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2d2a vgabios.c:2055
    jc short 02d36h                           ; 72 06                       ; 0xc2d2e
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2d30 vgabios.c:2057
    call 02a9ch                               ; e8 66 fd                    ; 0xc2d33
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2d36 vgabios.c:2059
    pop di                                    ; 5f                          ; 0xc2d39
    pop si                                    ; 5e                          ; 0xc2d3a
    pop cx                                    ; 59                          ; 0xc2d3b
    pop bx                                    ; 5b                          ; 0xc2d3c
    pop bp                                    ; 5d                          ; 0xc2d3d
    retn                                      ; c3                          ; 0xc2d3e
  ; disGetNextSymbol 0xc2d3f LB 0x161b -> off=0x0 cb=0000000000000016 uValue=00000000000c2d3f 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2d3f LB 0x16
    push bp                                   ; 55                          ; 0xc2d3f vgabios.c:2061
    mov bp, sp                                ; 89 e5                       ; 0xc2d40
    push bx                                   ; 53                          ; 0xc2d42
    push cx                                   ; 51                          ; 0xc2d43
    mov bx, dx                                ; 89 d3                       ; 0xc2d44 vgabios.c:2063
    mov cx, ax                                ; 89 c1                       ; 0xc2d46
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc2d48
    call 009f0h                               ; e8 a2 dc                    ; 0xc2d4b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2d4e vgabios.c:2064
    pop cx                                    ; 59                          ; 0xc2d51
    pop bx                                    ; 5b                          ; 0xc2d52
    pop bp                                    ; 5d                          ; 0xc2d53
    retn                                      ; c3                          ; 0xc2d54
  ; disGetNextSymbol 0xc2d55 LB 0x1605 -> off=0x0 cb=0000000000000049 uValue=00000000000c2d55 'set_gfx_font'
set_gfx_font:                                ; 0xc2d55 LB 0x49
    push bp                                   ; 55                          ; 0xc2d55 vgabios.c:2066
    mov bp, sp                                ; 89 e5                       ; 0xc2d56
    push si                                   ; 56                          ; 0xc2d58
    push di                                   ; 57                          ; 0xc2d59
    mov si, dx                                ; 89 d6                       ; 0xc2d5a
    mov di, bx                                ; 89 df                       ; 0xc2d5c
    mov dl, cl                                ; 88 ca                       ; 0xc2d5e
    mov bx, ax                                ; 89 c3                       ; 0xc2d60 vgabios.c:2070
    mov cx, si                                ; 89 f1                       ; 0xc2d62
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc2d64
    call 009f0h                               ; e8 86 dc                    ; 0xc2d67
    test dl, dl                               ; 84 d2                       ; 0xc2d6a vgabios.c:2071
    je short 02d7fh                           ; 74 11                       ; 0xc2d6c
    cmp dl, 003h                              ; 80 fa 03                    ; 0xc2d6e vgabios.c:2072
    jbe short 02d75h                          ; 76 02                       ; 0xc2d71
    mov DL, strict byte 002h                  ; b2 02                       ; 0xc2d73 vgabios.c:2073
    movzx bx, dl                              ; 0f b6 da                    ; 0xc2d75 vgabios.c:2074
    mov al, byte [bx+07e03h]                  ; 8a 87 03 7e                 ; 0xc2d78
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2d7c
    mov bx, 00085h                            ; bb 85 00                    ; 0xc2d7f vgabios.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d82
    mov es, ax                                ; 8e c0                       ; 0xc2d85
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc2d87
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc2d8a vgabios.c:2079
    dec ax                                    ; 48                          ; 0xc2d8e
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2d8f vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2d92
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2d95 vgabios.c:2080
    pop di                                    ; 5f                          ; 0xc2d98
    pop si                                    ; 5e                          ; 0xc2d99
    pop bp                                    ; 5d                          ; 0xc2d9a
    retn 00002h                               ; c2 02 00                    ; 0xc2d9b
  ; disGetNextSymbol 0xc2d9e LB 0x15bc -> off=0x0 cb=000000000000001c uValue=00000000000c2d9e 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc2d9e LB 0x1c
    push bp                                   ; 55                          ; 0xc2d9e vgabios.c:2082
    mov bp, sp                                ; 89 e5                       ; 0xc2d9f
    push si                                   ; 56                          ; 0xc2da1
    mov si, ax                                ; 89 c6                       ; 0xc2da2
    mov ax, dx                                ; 89 d0                       ; 0xc2da4
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc2da6 vgabios.c:2085
    push dx                                   ; 52                          ; 0xc2daa
    xor ch, ch                                ; 30 ed                       ; 0xc2dab
    mov dx, si                                ; 89 f2                       ; 0xc2dad
    call 02d55h                               ; e8 a3 ff                    ; 0xc2daf
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2db2 vgabios.c:2086
    pop si                                    ; 5e                          ; 0xc2db5
    pop bp                                    ; 5d                          ; 0xc2db6
    retn 00002h                               ; c2 02 00                    ; 0xc2db7
  ; disGetNextSymbol 0xc2dba LB 0x15a0 -> off=0x0 cb=000000000000001e uValue=00000000000c2dba 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc2dba LB 0x1e
    push bp                                   ; 55                          ; 0xc2dba vgabios.c:2091
    mov bp, sp                                ; 89 e5                       ; 0xc2dbb
    push bx                                   ; 53                          ; 0xc2dbd
    push cx                                   ; 51                          ; 0xc2dbe
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc2dbf vgabios.c:2093
    push cx                                   ; 51                          ; 0xc2dc2
    movzx cx, al                              ; 0f b6 c8                    ; 0xc2dc3
    mov bx, strict word 0000eh                ; bb 0e 00                    ; 0xc2dc6
    mov ax, 05d72h                            ; b8 72 5d                    ; 0xc2dc9
    mov dx, ds                                ; 8c da                       ; 0xc2dcc
    call 02d55h                               ; e8 84 ff                    ; 0xc2dce
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2dd1 vgabios.c:2094
    pop cx                                    ; 59                          ; 0xc2dd4
    pop bx                                    ; 5b                          ; 0xc2dd5
    pop bp                                    ; 5d                          ; 0xc2dd6
    retn                                      ; c3                          ; 0xc2dd7
  ; disGetNextSymbol 0xc2dd8 LB 0x1582 -> off=0x0 cb=000000000000001e uValue=00000000000c2dd8 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc2dd8 LB 0x1e
    push bp                                   ; 55                          ; 0xc2dd8 vgabios.c:2095
    mov bp, sp                                ; 89 e5                       ; 0xc2dd9
    push bx                                   ; 53                          ; 0xc2ddb
    push cx                                   ; 51                          ; 0xc2ddc
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc2ddd vgabios.c:2097
    push cx                                   ; 51                          ; 0xc2de0
    movzx cx, al                              ; 0f b6 c8                    ; 0xc2de1
    mov bx, strict word 00008h                ; bb 08 00                    ; 0xc2de4
    mov ax, 05572h                            ; b8 72 55                    ; 0xc2de7
    mov dx, ds                                ; 8c da                       ; 0xc2dea
    call 02d55h                               ; e8 66 ff                    ; 0xc2dec
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2def vgabios.c:2098
    pop cx                                    ; 59                          ; 0xc2df2
    pop bx                                    ; 5b                          ; 0xc2df3
    pop bp                                    ; 5d                          ; 0xc2df4
    retn                                      ; c3                          ; 0xc2df5
  ; disGetNextSymbol 0xc2df6 LB 0x1564 -> off=0x0 cb=000000000000001e uValue=00000000000c2df6 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc2df6 LB 0x1e
    push bp                                   ; 55                          ; 0xc2df6 vgabios.c:2099
    mov bp, sp                                ; 89 e5                       ; 0xc2df7
    push bx                                   ; 53                          ; 0xc2df9
    push cx                                   ; 51                          ; 0xc2dfa
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc2dfb vgabios.c:2101
    push cx                                   ; 51                          ; 0xc2dfe
    movzx cx, al                              ; 0f b6 c8                    ; 0xc2dff
    mov bx, strict word 00010h                ; bb 10 00                    ; 0xc2e02
    mov ax, 06b72h                            ; b8 72 6b                    ; 0xc2e05
    mov dx, ds                                ; 8c da                       ; 0xc2e08
    call 02d55h                               ; e8 48 ff                    ; 0xc2e0a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2e0d vgabios.c:2102
    pop cx                                    ; 59                          ; 0xc2e10
    pop bx                                    ; 5b                          ; 0xc2e11
    pop bp                                    ; 5d                          ; 0xc2e12
    retn                                      ; c3                          ; 0xc2e13
  ; disGetNextSymbol 0xc2e14 LB 0x1546 -> off=0x0 cb=0000000000000005 uValue=00000000000c2e14 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc2e14 LB 0x5
    push bp                                   ; 55                          ; 0xc2e14 vgabios.c:2104
    mov bp, sp                                ; 89 e5                       ; 0xc2e15
    pop bp                                    ; 5d                          ; 0xc2e17 vgabios.c:2109
    retn                                      ; c3                          ; 0xc2e18
  ; disGetNextSymbol 0xc2e19 LB 0x1541 -> off=0x0 cb=0000000000000032 uValue=00000000000c2e19 'biosfn_set_txt_lines'
biosfn_set_txt_lines:                        ; 0xc2e19 LB 0x32
    push bx                                   ; 53                          ; 0xc2e19 vgabios.c:2111
    push si                                   ; 56                          ; 0xc2e1a
    push bp                                   ; 55                          ; 0xc2e1b
    mov bp, sp                                ; 89 e5                       ; 0xc2e1c
    mov bl, al                                ; 88 c3                       ; 0xc2e1e
    mov si, 00089h                            ; be 89 00                    ; 0xc2e20 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e23
    mov es, ax                                ; 8e c0                       ; 0xc2e26
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2e28
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc2e2b vgabios.c:2117
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc2e2d vgabios.c:2119
    je short 02e3ah                           ; 74 08                       ; 0xc2e30
    test bl, bl                               ; 84 db                       ; 0xc2e32
    jne short 02e3ch                          ; 75 06                       ; 0xc2e34
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc2e36 vgabios.c:2122
    jmp short 02e3ch                          ; eb 02                       ; 0xc2e38 vgabios.c:2123
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc2e3a vgabios.c:2125
    mov bx, 00089h                            ; bb 89 00                    ; 0xc2e3c vgabios.c:52
    mov si, strict word 00040h                ; be 40 00                    ; 0xc2e3f
    mov es, si                                ; 8e c6                       ; 0xc2e42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e44
    pop bp                                    ; 5d                          ; 0xc2e47 vgabios.c:2129
    pop si                                    ; 5e                          ; 0xc2e48
    pop bx                                    ; 5b                          ; 0xc2e49
    retn                                      ; c3                          ; 0xc2e4a
  ; disGetNextSymbol 0xc2e4b LB 0x150f -> off=0x0 cb=0000000000000005 uValue=00000000000c2e4b 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc2e4b LB 0x5
    push bp                                   ; 55                          ; 0xc2e4b vgabios.c:2132
    mov bp, sp                                ; 89 e5                       ; 0xc2e4c
    pop bp                                    ; 5d                          ; 0xc2e4e vgabios.c:2137
    retn                                      ; c3                          ; 0xc2e4f
  ; disGetNextSymbol 0xc2e50 LB 0x150a -> off=0x0 cb=0000000000000005 uValue=00000000000c2e50 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc2e50 LB 0x5
    push bp                                   ; 55                          ; 0xc2e50 vgabios.c:2138
    mov bp, sp                                ; 89 e5                       ; 0xc2e51
    pop bp                                    ; 5d                          ; 0xc2e53 vgabios.c:2143
    retn                                      ; c3                          ; 0xc2e54
  ; disGetNextSymbol 0xc2e55 LB 0x1505 -> off=0x0 cb=0000000000000096 uValue=00000000000c2e55 'biosfn_write_string'
biosfn_write_string:                         ; 0xc2e55 LB 0x96
    push bp                                   ; 55                          ; 0xc2e55 vgabios.c:2146
    mov bp, sp                                ; 89 e5                       ; 0xc2e56
    push si                                   ; 56                          ; 0xc2e58
    push di                                   ; 57                          ; 0xc2e59
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2e5a
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2e5d
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2e60
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2e63
    mov si, cx                                ; 89 ce                       ; 0xc2e66
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc2e68
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc2e6b vgabios.c:2153
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc2e6e
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc2e71
    call 00a93h                               ; e8 1c dc                    ; 0xc2e74
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc2e77 vgabios.c:2156
    jne short 02e8eh                          ; 75 11                       ; 0xc2e7b
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2e7d vgabios.c:2157
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc2e80
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2e83 vgabios.c:2158
    xor al, al                                ; 30 c0                       ; 0xc2e86
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2e88
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2e8b
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc2e8e vgabios.c:2161
    sal dx, 008h                              ; c1 e2 08                    ; 0xc2e92
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc2e95
    add dx, ax                                ; 01 c2                       ; 0xc2e99
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2e9b vgabios.c:2162
    call 01242h                               ; e8 a0 e3                    ; 0xc2e9f
    dec si                                    ; 4e                          ; 0xc2ea2 vgabios.c:2164
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2ea3
    je short 02ed2h                           ; 74 2a                       ; 0xc2ea6
    mov bx, di                                ; 89 fb                       ; 0xc2ea8 vgabios.c:2166
    inc di                                    ; 47                          ; 0xc2eaa
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc2eab vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2eae
    test byte [bp-008h], 002h                 ; f6 46 f8 02                 ; 0xc2eb1 vgabios.c:2167
    je short 02ec0h                           ; 74 09                       ; 0xc2eb5
    mov bx, di                                ; 89 fb                       ; 0xc2eb7 vgabios.c:2168
    inc di                                    ; 47                          ; 0xc2eb9
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc2eba vgabios.c:47
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc2ebd vgabios.c:48
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc2ec0 vgabios.c:2170
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2ec4
    xor ah, ah                                ; 30 e4                       ; 0xc2ec8
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2eca
    call 027eah                               ; e8 1a f9                    ; 0xc2ecd
    jmp short 02ea2h                          ; eb d0                       ; 0xc2ed0 vgabios.c:2171
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc2ed2 vgabios.c:2174
    jne short 02ee2h                          ; 75 0a                       ; 0xc2ed6
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2ed8 vgabios.c:2175
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2edb
    call 01242h                               ; e8 60 e3                    ; 0xc2edf
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2ee2 vgabios.c:2176
    pop di                                    ; 5f                          ; 0xc2ee5
    pop si                                    ; 5e                          ; 0xc2ee6
    pop bp                                    ; 5d                          ; 0xc2ee7
    retn 00008h                               ; c2 08 00                    ; 0xc2ee8
  ; disGetNextSymbol 0xc2eeb LB 0x146f -> off=0x0 cb=00000000000001f2 uValue=00000000000c2eeb 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc2eeb LB 0x1f2
    push bp                                   ; 55                          ; 0xc2eeb vgabios.c:2179
    mov bp, sp                                ; 89 e5                       ; 0xc2eec
    push cx                                   ; 51                          ; 0xc2eee
    push si                                   ; 56                          ; 0xc2eef
    push di                                   ; 57                          ; 0xc2ef0
    push ax                                   ; 50                          ; 0xc2ef1
    push ax                                   ; 50                          ; 0xc2ef2
    push dx                                   ; 52                          ; 0xc2ef3
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2ef4 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ef7
    mov es, ax                                ; 8e c0                       ; 0xc2efa
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2efc
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2eff vgabios.c:48
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2f02 vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2f05
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2f08 vgabios.c:58
    mov ax, ds                                ; 8c d8                       ; 0xc2f0b vgabios.c:2190
    mov es, dx                                ; 8e c2                       ; 0xc2f0d vgabios.c:72
    mov word [es:bx], 05508h                  ; 26 c7 07 08 55              ; 0xc2f0f
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc2f14
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc2f18 vgabios.c:2195
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc2f1b
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2f1e
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2f21
    jcxz 02f2ch                               ; e3 06                       ; 0xc2f24
    push DS                                   ; 1e                          ; 0xc2f26
    mov ds, dx                                ; 8e da                       ; 0xc2f27
    rep movsb                                 ; f3 a4                       ; 0xc2f29
    pop DS                                    ; 1f                          ; 0xc2f2b
    mov si, 00084h                            ; be 84 00                    ; 0xc2f2c vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f2f
    mov es, ax                                ; 8e c0                       ; 0xc2f32
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f34
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2f37 vgabios.c:48
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc2f39
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f3c vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f3f
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc2f42 vgabios.c:2197
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc2f45
    mov si, 00085h                            ; be 85 00                    ; 0xc2f48
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2f4b
    jcxz 02f56h                               ; e3 06                       ; 0xc2f4e
    push DS                                   ; 1e                          ; 0xc2f50
    mov ds, dx                                ; 8e da                       ; 0xc2f51
    rep movsb                                 ; f3 a4                       ; 0xc2f53
    pop DS                                    ; 1f                          ; 0xc2f55
    mov si, 0008ah                            ; be 8a 00                    ; 0xc2f56 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f59
    mov es, ax                                ; 8e c0                       ; 0xc2f5c
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f5e
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc2f61 vgabios.c:48
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2f64 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2f67
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc2f6a vgabios.c:2200
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f6d vgabios.c:52
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2f71 vgabios.c:2201
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc2f74 vgabios.c:62
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2f79 vgabios.c:2202
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc2f7c vgabios.c:52
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2f80 vgabios.c:2203
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc2f83 vgabios.c:52
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc2f87 vgabios.c:2204
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f8a vgabios.c:52
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc2f8e vgabios.c:2205
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2f91 vgabios.c:52
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2f95 vgabios.c:2206
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc2f98 vgabios.c:52
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc2f9c vgabios.c:2207
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc2f9f vgabios.c:52
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc2fa3 vgabios.c:2208
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2fa6 vgabios.c:52
    mov si, 00089h                            ; be 89 00                    ; 0xc2faa vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fad
    mov es, ax                                ; 8e c0                       ; 0xc2fb0
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2fb2
    mov ah, al                                ; 88 c4                       ; 0xc2fb5 vgabios.c:2213
    and ah, 080h                              ; 80 e4 80                    ; 0xc2fb7
    movzx si, ah                              ; 0f b6 f4                    ; 0xc2fba
    sar si, 006h                              ; c1 fe 06                    ; 0xc2fbd
    and AL, strict byte 010h                  ; 24 10                       ; 0xc2fc0
    xor ah, ah                                ; 30 e4                       ; 0xc2fc2
    sar ax, 004h                              ; c1 f8 04                    ; 0xc2fc4
    or ax, si                                 ; 09 f0                       ; 0xc2fc7
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc2fc9 vgabios.c:2214
    je short 02fdfh                           ; 74 11                       ; 0xc2fcc
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc2fce
    je short 02fdbh                           ; 74 08                       ; 0xc2fd1
    test ax, ax                               ; 85 c0                       ; 0xc2fd3
    jne short 02fdfh                          ; 75 08                       ; 0xc2fd5
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2fd7 vgabios.c:2215
    jmp short 02fe1h                          ; eb 06                       ; 0xc2fd9
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2fdb vgabios.c:2216
    jmp short 02fe1h                          ; eb 02                       ; 0xc2fdd
    xor al, al                                ; 30 c0                       ; 0xc2fdf vgabios.c:2218
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2fe1 vgabios.c:2220
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2fe4 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2fe7
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2fea vgabios.c:2223
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc2fed
    jc short 03010h                           ; 72 1f                       ; 0xc2fef
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc2ff1
    jnbe short 03010h                         ; 77 1b                       ; 0xc2ff3
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2ff5 vgabios.c:2224
    test ax, ax                               ; 85 c0                       ; 0xc2ff8
    je short 03052h                           ; 74 56                       ; 0xc2ffa
    mov si, ax                                ; 89 c6                       ; 0xc2ffc vgabios.c:2225
    shr si, 002h                              ; c1 ee 02                    ; 0xc2ffe
    mov ax, 04000h                            ; b8 00 40                    ; 0xc3001
    xor dx, dx                                ; 31 d2                       ; 0xc3004
    div si                                    ; f7 f6                       ; 0xc3006
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc3008
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc300b vgabios.c:52
    jmp short 03052h                          ; eb 42                       ; 0xc300e vgabios.c:2226
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc3010
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3013
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc3016
    jne short 0302bh                          ; 75 11                       ; 0xc3018
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc301a vgabios.c:52
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc301d
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc3021 vgabios.c:2228
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc3024 vgabios.c:62
    jmp short 03052h                          ; eb 27                       ; 0xc3029 vgabios.c:2229
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc302b
    jc short 03052h                           ; 72 23                       ; 0xc302d
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc302f
    jnbe short 03052h                         ; 77 1f                       ; 0xc3031
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc3033 vgabios.c:2231
    je short 03047h                           ; 74 0e                       ; 0xc3037
    mov ax, 04000h                            ; b8 00 40                    ; 0xc3039 vgabios.c:2232
    xor dx, dx                                ; 31 d2                       ; 0xc303c
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc303e
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3041 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3044
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc3047 vgabios.c:2233
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc304a vgabios.c:62
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc304d
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3052 vgabios.c:2235
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc3055
    je short 0305dh                           ; 74 04                       ; 0xc3057
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc3059
    jne short 03068h                          ; 75 0b                       ; 0xc305b
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc305d vgabios.c:2236
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3060 vgabios.c:62
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc3063
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3068 vgabios.c:2238
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc306b
    jc short 030c6h                           ; 72 57                       ; 0xc306d
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc306f
    je short 030c6h                           ; 74 53                       ; 0xc3071
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc3073 vgabios.c:2239
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc3076 vgabios.c:52
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc3079
    mov si, 00084h                            ; be 84 00                    ; 0xc307d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3080
    mov es, ax                                ; 8e c0                       ; 0xc3083
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3085
    movzx di, al                              ; 0f b6 f8                    ; 0xc3088 vgabios.c:48
    inc di                                    ; 47                          ; 0xc308b
    mov si, 00085h                            ; be 85 00                    ; 0xc308c vgabios.c:47
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc308f
    xor ah, ah                                ; 30 e4                       ; 0xc3092 vgabios.c:48
    imul ax, di                               ; 0f af c7                    ; 0xc3094
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc3097 vgabios.c:2241
    jc short 030aah                           ; 72 0e                       ; 0xc309a
    jbe short 030b3h                          ; 76 15                       ; 0xc309c
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc309e
    je short 030bbh                           ; 74 18                       ; 0xc30a1
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc30a3
    je short 030b7h                           ; 74 0f                       ; 0xc30a6
    jmp short 030bbh                          ; eb 11                       ; 0xc30a8
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc30aa
    jne short 030bbh                          ; 75 0c                       ; 0xc30ad
    xor al, al                                ; 30 c0                       ; 0xc30af vgabios.c:2242
    jmp short 030bdh                          ; eb 0a                       ; 0xc30b1
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc30b3 vgabios.c:2243
    jmp short 030bdh                          ; eb 06                       ; 0xc30b5
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc30b7 vgabios.c:2244
    jmp short 030bdh                          ; eb 02                       ; 0xc30b9
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc30bb vgabios.c:2246
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc30bd vgabios.c:2248
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc30c0 vgabios.c:52
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc30c3
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc30c6 vgabios.c:2251
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc30c9
    xor ax, ax                                ; 31 c0                       ; 0xc30cc
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc30ce
    jcxz 030d5h                               ; e3 02                       ; 0xc30d1
    rep stosb                                 ; f3 aa                       ; 0xc30d3
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc30d5 vgabios.c:2252
    pop di                                    ; 5f                          ; 0xc30d8
    pop si                                    ; 5e                          ; 0xc30d9
    pop cx                                    ; 59                          ; 0xc30da
    pop bp                                    ; 5d                          ; 0xc30db
    retn                                      ; c3                          ; 0xc30dc
  ; disGetNextSymbol 0xc30dd LB 0x127d -> off=0x0 cb=0000000000000023 uValue=00000000000c30dd 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc30dd LB 0x23
    push dx                                   ; 52                          ; 0xc30dd vgabios.c:2255
    push bp                                   ; 55                          ; 0xc30de
    mov bp, sp                                ; 89 e5                       ; 0xc30df
    mov dx, ax                                ; 89 c2                       ; 0xc30e1
    xor ax, ax                                ; 31 c0                       ; 0xc30e3 vgabios.c:2259
    test dl, 001h                             ; f6 c2 01                    ; 0xc30e5 vgabios.c:2260
    je short 030edh                           ; 74 03                       ; 0xc30e8
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc30ea vgabios.c:2261
    test dl, 002h                             ; f6 c2 02                    ; 0xc30ed vgabios.c:2263
    je short 030f5h                           ; 74 03                       ; 0xc30f0
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc30f2 vgabios.c:2264
    test dl, 004h                             ; f6 c2 04                    ; 0xc30f5 vgabios.c:2266
    je short 030fdh                           ; 74 03                       ; 0xc30f8
    add ax, 00304h                            ; 05 04 03                    ; 0xc30fa vgabios.c:2267
    pop bp                                    ; 5d                          ; 0xc30fd vgabios.c:2270
    pop dx                                    ; 5a                          ; 0xc30fe
    retn                                      ; c3                          ; 0xc30ff
  ; disGetNextSymbol 0xc3100 LB 0x125a -> off=0x0 cb=0000000000000018 uValue=00000000000c3100 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc3100 LB 0x18
    push bp                                   ; 55                          ; 0xc3100 vgabios.c:2272
    mov bp, sp                                ; 89 e5                       ; 0xc3101
    push bx                                   ; 53                          ; 0xc3103
    mov bx, dx                                ; 89 d3                       ; 0xc3104
    call 030ddh                               ; e8 d4 ff                    ; 0xc3106 vgabios.c:2275
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc3109
    shr ax, 006h                              ; c1 e8 06                    ; 0xc310c
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc310f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3112 vgabios.c:2276
    pop bx                                    ; 5b                          ; 0xc3115
    pop bp                                    ; 5d                          ; 0xc3116
    retn                                      ; c3                          ; 0xc3117
  ; disGetNextSymbol 0xc3118 LB 0x1242 -> off=0x0 cb=00000000000002d6 uValue=00000000000c3118 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc3118 LB 0x2d6
    push bp                                   ; 55                          ; 0xc3118 vgabios.c:2278
    mov bp, sp                                ; 89 e5                       ; 0xc3119
    push cx                                   ; 51                          ; 0xc311b
    push si                                   ; 56                          ; 0xc311c
    push di                                   ; 57                          ; 0xc311d
    push ax                                   ; 50                          ; 0xc311e
    push ax                                   ; 50                          ; 0xc311f
    push ax                                   ; 50                          ; 0xc3120
    mov cx, dx                                ; 89 d1                       ; 0xc3121
    mov si, strict word 00063h                ; be 63 00                    ; 0xc3123 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3126
    mov es, ax                                ; 8e c0                       ; 0xc3129
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc312b
    mov si, di                                ; 89 fe                       ; 0xc312e vgabios.c:58
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc3130 vgabios.c:2283
    je near 0324bh                            ; 0f 84 13 01                 ; 0xc3134
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3138 vgabios.c:2284
    in AL, DX                                 ; ec                          ; 0xc313b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc313c
    mov es, cx                                ; 8e c1                       ; 0xc313e vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3140
    inc bx                                    ; 43                          ; 0xc3143 vgabios.c:2284
    mov dx, di                                ; 89 fa                       ; 0xc3144
    in AL, DX                                 ; ec                          ; 0xc3146
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3147
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3149 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc314c vgabios.c:2285
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc314d
    in AL, DX                                 ; ec                          ; 0xc3150
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3151
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3153 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc3156 vgabios.c:2286
    mov dx, 003dah                            ; ba da 03                    ; 0xc3157
    in AL, DX                                 ; ec                          ; 0xc315a
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc315b
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc315d vgabios.c:2288
    in AL, DX                                 ; ec                          ; 0xc3160
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3161
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3163
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc3166 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3169
    inc bx                                    ; 43                          ; 0xc316c vgabios.c:2289
    mov dx, 003cah                            ; ba ca 03                    ; 0xc316d
    in AL, DX                                 ; ec                          ; 0xc3170
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3171
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3173 vgabios.c:52
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3176 vgabios.c:2292
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3179
    add bx, ax                                ; 01 c3                       ; 0xc317c vgabios.c:2290
    jmp short 03186h                          ; eb 06                       ; 0xc317e
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc3180
    jnbe short 0319eh                         ; 77 18                       ; 0xc3184
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3186 vgabios.c:2293
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3189
    out DX, AL                                ; ee                          ; 0xc318c
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc318d vgabios.c:2294
    in AL, DX                                 ; ec                          ; 0xc3190
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3191
    mov es, cx                                ; 8e c1                       ; 0xc3193 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3195
    inc bx                                    ; 43                          ; 0xc3198 vgabios.c:2294
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3199 vgabios.c:2295
    jmp short 03180h                          ; eb e2                       ; 0xc319c
    xor al, al                                ; 30 c0                       ; 0xc319e vgabios.c:2296
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc31a0
    out DX, AL                                ; ee                          ; 0xc31a3
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc31a4 vgabios.c:2297
    in AL, DX                                 ; ec                          ; 0xc31a7
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31a8
    mov es, cx                                ; 8e c1                       ; 0xc31aa vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31ac
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc31af vgabios.c:2299
    inc bx                                    ; 43                          ; 0xc31b4 vgabios.c:2297
    jmp short 031bdh                          ; eb 06                       ; 0xc31b5
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc31b7
    jnbe short 031d4h                         ; 77 17                       ; 0xc31bb
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc31bd vgabios.c:2300
    mov dx, si                                ; 89 f2                       ; 0xc31c0
    out DX, AL                                ; ee                          ; 0xc31c2
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc31c3 vgabios.c:2301
    in AL, DX                                 ; ec                          ; 0xc31c6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31c7
    mov es, cx                                ; 8e c1                       ; 0xc31c9 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31cb
    inc bx                                    ; 43                          ; 0xc31ce vgabios.c:2301
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc31cf vgabios.c:2302
    jmp short 031b7h                          ; eb e3                       ; 0xc31d2
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc31d4 vgabios.c:2304
    jmp short 031e1h                          ; eb 06                       ; 0xc31d9
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc31db
    jnbe short 03205h                         ; 77 24                       ; 0xc31df
    mov dx, 003dah                            ; ba da 03                    ; 0xc31e1 vgabios.c:2305
    in AL, DX                                 ; ec                          ; 0xc31e4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31e5
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc31e7 vgabios.c:2306
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc31ea
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc31ed
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc31f0
    out DX, AL                                ; ee                          ; 0xc31f3
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc31f4 vgabios.c:2307
    in AL, DX                                 ; ec                          ; 0xc31f7
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31f8
    mov es, cx                                ; 8e c1                       ; 0xc31fa vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc31fc
    inc bx                                    ; 43                          ; 0xc31ff vgabios.c:2307
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3200 vgabios.c:2308
    jmp short 031dbh                          ; eb d6                       ; 0xc3203
    mov dx, 003dah                            ; ba da 03                    ; 0xc3205 vgabios.c:2309
    in AL, DX                                 ; ec                          ; 0xc3208
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3209
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc320b vgabios.c:2311
    jmp short 03218h                          ; eb 06                       ; 0xc3210
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3212
    jnbe short 03230h                         ; 77 18                       ; 0xc3216
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3218 vgabios.c:2312
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc321b
    out DX, AL                                ; ee                          ; 0xc321e
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc321f vgabios.c:2313
    in AL, DX                                 ; ec                          ; 0xc3222
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3223
    mov es, cx                                ; 8e c1                       ; 0xc3225 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3227
    inc bx                                    ; 43                          ; 0xc322a vgabios.c:2313
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc322b vgabios.c:2314
    jmp short 03212h                          ; eb e2                       ; 0xc322e
    mov es, cx                                ; 8e c1                       ; 0xc3230 vgabios.c:62
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc3232
    inc bx                                    ; 43                          ; 0xc3235 vgabios.c:2316
    inc bx                                    ; 43                          ; 0xc3236
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3237 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc323b vgabios.c:2319
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc323c vgabios.c:52
    inc bx                                    ; 43                          ; 0xc3240 vgabios.c:2320
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3241 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc3245 vgabios.c:2321
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc3246 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc324a vgabios.c:2322
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc324b vgabios.c:2324
    je near 03392h                            ; 0f 84 3f 01                 ; 0xc324f
    mov si, strict word 00049h                ; be 49 00                    ; 0xc3253 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3256
    mov es, ax                                ; 8e c0                       ; 0xc3259
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc325b
    mov es, cx                                ; 8e c1                       ; 0xc325e vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3260
    inc bx                                    ; 43                          ; 0xc3263 vgabios.c:2325
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc3264 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3267
    mov es, ax                                ; 8e c0                       ; 0xc326a
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc326c
    mov es, cx                                ; 8e c1                       ; 0xc326f vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3271
    inc bx                                    ; 43                          ; 0xc3274 vgabios.c:2326
    inc bx                                    ; 43                          ; 0xc3275
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc3276 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3279
    mov es, ax                                ; 8e c0                       ; 0xc327c
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc327e
    mov es, cx                                ; 8e c1                       ; 0xc3281 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3283
    inc bx                                    ; 43                          ; 0xc3286 vgabios.c:2327
    inc bx                                    ; 43                          ; 0xc3287
    mov si, strict word 00063h                ; be 63 00                    ; 0xc3288 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc328b
    mov es, ax                                ; 8e c0                       ; 0xc328e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3290
    mov es, cx                                ; 8e c1                       ; 0xc3293 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3295
    inc bx                                    ; 43                          ; 0xc3298 vgabios.c:2328
    inc bx                                    ; 43                          ; 0xc3299
    mov si, 00084h                            ; be 84 00                    ; 0xc329a vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc329d
    mov es, ax                                ; 8e c0                       ; 0xc32a0
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc32a2
    mov es, cx                                ; 8e c1                       ; 0xc32a5 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32a7
    inc bx                                    ; 43                          ; 0xc32aa vgabios.c:2329
    mov si, 00085h                            ; be 85 00                    ; 0xc32ab vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32ae
    mov es, ax                                ; 8e c0                       ; 0xc32b1
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32b3
    mov es, cx                                ; 8e c1                       ; 0xc32b6 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32b8
    inc bx                                    ; 43                          ; 0xc32bb vgabios.c:2330
    inc bx                                    ; 43                          ; 0xc32bc
    mov si, 00087h                            ; be 87 00                    ; 0xc32bd vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32c0
    mov es, ax                                ; 8e c0                       ; 0xc32c3
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc32c5
    mov es, cx                                ; 8e c1                       ; 0xc32c8 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32ca
    inc bx                                    ; 43                          ; 0xc32cd vgabios.c:2331
    mov si, 00088h                            ; be 88 00                    ; 0xc32ce vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32d1
    mov es, ax                                ; 8e c0                       ; 0xc32d4
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc32d6
    mov es, cx                                ; 8e c1                       ; 0xc32d9 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32db
    inc bx                                    ; 43                          ; 0xc32de vgabios.c:2332
    mov si, 00089h                            ; be 89 00                    ; 0xc32df vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32e2
    mov es, ax                                ; 8e c0                       ; 0xc32e5
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc32e7
    mov es, cx                                ; 8e c1                       ; 0xc32ea vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc32ec
    inc bx                                    ; 43                          ; 0xc32ef vgabios.c:2333
    mov si, strict word 00060h                ; be 60 00                    ; 0xc32f0 vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc32f3
    mov es, ax                                ; 8e c0                       ; 0xc32f6
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc32f8
    mov es, cx                                ; 8e c1                       ; 0xc32fb vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc32fd
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc3300 vgabios.c:2335
    inc bx                                    ; 43                          ; 0xc3305 vgabios.c:2334
    inc bx                                    ; 43                          ; 0xc3306
    jmp short 0330fh                          ; eb 06                       ; 0xc3307
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3309
    jnc short 0332bh                          ; 73 1c                       ; 0xc330d
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc330f vgabios.c:2336
    add si, si                                ; 01 f6                       ; 0xc3312
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc3314
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3317 vgabios.c:57
    mov es, ax                                ; 8e c0                       ; 0xc331a
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc331c
    mov es, cx                                ; 8e c1                       ; 0xc331f vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3321
    inc bx                                    ; 43                          ; 0xc3324 vgabios.c:2337
    inc bx                                    ; 43                          ; 0xc3325
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3326 vgabios.c:2338
    jmp short 03309h                          ; eb de                       ; 0xc3329
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc332b vgabios.c:57
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc332e
    mov es, ax                                ; 8e c0                       ; 0xc3331
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3333
    mov es, cx                                ; 8e c1                       ; 0xc3336 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3338
    inc bx                                    ; 43                          ; 0xc333b vgabios.c:2339
    inc bx                                    ; 43                          ; 0xc333c
    mov si, strict word 00062h                ; be 62 00                    ; 0xc333d vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3340
    mov es, ax                                ; 8e c0                       ; 0xc3343
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3345
    mov es, cx                                ; 8e c1                       ; 0xc3348 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc334a
    inc bx                                    ; 43                          ; 0xc334d vgabios.c:2340
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc334e vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc3351
    mov es, ax                                ; 8e c0                       ; 0xc3353
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3355
    mov es, cx                                ; 8e c1                       ; 0xc3358 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc335a
    inc bx                                    ; 43                          ; 0xc335d vgabios.c:2342
    inc bx                                    ; 43                          ; 0xc335e
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc335f vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc3362
    mov es, ax                                ; 8e c0                       ; 0xc3364
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3366
    mov es, cx                                ; 8e c1                       ; 0xc3369 vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc336b
    inc bx                                    ; 43                          ; 0xc336e vgabios.c:2343
    inc bx                                    ; 43                          ; 0xc336f
    mov si, 0010ch                            ; be 0c 01                    ; 0xc3370 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc3373
    mov es, ax                                ; 8e c0                       ; 0xc3375
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3377
    mov es, cx                                ; 8e c1                       ; 0xc337a vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc337c
    inc bx                                    ; 43                          ; 0xc337f vgabios.c:2344
    inc bx                                    ; 43                          ; 0xc3380
    mov si, 0010eh                            ; be 0e 01                    ; 0xc3381 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc3384
    mov es, ax                                ; 8e c0                       ; 0xc3386
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3388
    mov es, cx                                ; 8e c1                       ; 0xc338b vgabios.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc338d
    inc bx                                    ; 43                          ; 0xc3390 vgabios.c:2345
    inc bx                                    ; 43                          ; 0xc3391
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc3392 vgabios.c:2347
    je short 033e4h                           ; 74 4c                       ; 0xc3396
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc3398 vgabios.c:2349
    in AL, DX                                 ; ec                          ; 0xc339b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc339c
    mov es, cx                                ; 8e c1                       ; 0xc339e vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33a0
    inc bx                                    ; 43                          ; 0xc33a3 vgabios.c:2349
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc33a4
    in AL, DX                                 ; ec                          ; 0xc33a7
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33a8
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33aa vgabios.c:52
    inc bx                                    ; 43                          ; 0xc33ad vgabios.c:2350
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc33ae
    in AL, DX                                 ; ec                          ; 0xc33b1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33b2
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33b4 vgabios.c:52
    inc bx                                    ; 43                          ; 0xc33b7 vgabios.c:2351
    xor al, al                                ; 30 c0                       ; 0xc33b8
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc33ba
    out DX, AL                                ; ee                          ; 0xc33bd
    xor ah, ah                                ; 30 e4                       ; 0xc33be vgabios.c:2354
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc33c0
    jmp short 033cch                          ; eb 07                       ; 0xc33c3
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc33c5
    jnc short 033ddh                          ; 73 11                       ; 0xc33ca
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc33cc vgabios.c:2355
    in AL, DX                                 ; ec                          ; 0xc33cf
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc33d0
    mov es, cx                                ; 8e c1                       ; 0xc33d2 vgabios.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc33d4
    inc bx                                    ; 43                          ; 0xc33d7 vgabios.c:2355
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc33d8 vgabios.c:2356
    jmp short 033c5h                          ; eb e8                       ; 0xc33db
    mov es, cx                                ; 8e c1                       ; 0xc33dd vgabios.c:52
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc33df
    inc bx                                    ; 43                          ; 0xc33e3 vgabios.c:2357
    mov ax, bx                                ; 89 d8                       ; 0xc33e4 vgabios.c:2360
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc33e6
    pop di                                    ; 5f                          ; 0xc33e9
    pop si                                    ; 5e                          ; 0xc33ea
    pop cx                                    ; 59                          ; 0xc33eb
    pop bp                                    ; 5d                          ; 0xc33ec
    retn                                      ; c3                          ; 0xc33ed
  ; disGetNextSymbol 0xc33ee LB 0xf6c -> off=0x0 cb=00000000000002b8 uValue=00000000000c33ee 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc33ee LB 0x2b8
    push bp                                   ; 55                          ; 0xc33ee vgabios.c:2362
    mov bp, sp                                ; 89 e5                       ; 0xc33ef
    push cx                                   ; 51                          ; 0xc33f1
    push si                                   ; 56                          ; 0xc33f2
    push di                                   ; 57                          ; 0xc33f3
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc33f4
    push ax                                   ; 50                          ; 0xc33f7
    mov cx, dx                                ; 89 d1                       ; 0xc33f8
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc33fa vgabios.c:2366
    je near 03536h                            ; 0f 84 34 01                 ; 0xc33fe
    mov dx, 003dah                            ; ba da 03                    ; 0xc3402 vgabios.c:2368
    in AL, DX                                 ; ec                          ; 0xc3405
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3406
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc3408 vgabios.c:2370
    mov es, cx                                ; 8e c1                       ; 0xc340b vgabios.c:57
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc340d
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3410 vgabios.c:58
    mov si, bx                                ; 89 de                       ; 0xc3413 vgabios.c:2371
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3415 vgabios.c:2374
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc341a vgabios.c:2372
    jmp short 03425h                          ; eb 06                       ; 0xc341d
    cmp word [bp-00eh], strict byte 00004h    ; 83 7e f2 04                 ; 0xc341f
    jnbe short 0343bh                         ; 77 16                       ; 0xc3423
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3425 vgabios.c:2375
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3428
    out DX, AL                                ; ee                          ; 0xc342b
    mov es, cx                                ; 8e c1                       ; 0xc342c vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc342e
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3431 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3434
    inc bx                                    ; 43                          ; 0xc3435 vgabios.c:2376
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc3436 vgabios.c:2377
    jmp short 0341fh                          ; eb e4                       ; 0xc3439
    xor al, al                                ; 30 c0                       ; 0xc343b vgabios.c:2378
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc343d
    out DX, AL                                ; ee                          ; 0xc3440
    mov es, cx                                ; 8e c1                       ; 0xc3441 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3443
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3446 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3449
    inc bx                                    ; 43                          ; 0xc344a vgabios.c:2379
    mov dx, 003cch                            ; ba cc 03                    ; 0xc344b
    in AL, DX                                 ; ec                          ; 0xc344e
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc344f
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc3451
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3453
    cmp word [bp-00ah], 003d4h                ; 81 7e f6 d4 03              ; 0xc3456 vgabios.c:2383
    jne short 03461h                          ; 75 04                       ; 0xc345b
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc345d vgabios.c:2384
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3461 vgabios.c:2385
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc3464
    out DX, AL                                ; ee                          ; 0xc3467
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc3468 vgabios.c:2388
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc346b
    out DX, ax                                ; ef                          ; 0xc346e
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc346f vgabios.c:2390
    jmp short 0347ch                          ; eb 06                       ; 0xc3474
    cmp word [bp-00eh], strict byte 00018h    ; 83 7e f2 18                 ; 0xc3476
    jnbe short 03496h                         ; 77 1a                       ; 0xc347a
    cmp word [bp-00eh], strict byte 00011h    ; 83 7e f2 11                 ; 0xc347c vgabios.c:2391
    je short 03490h                           ; 74 0e                       ; 0xc3480
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3482 vgabios.c:2392
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3485
    out DX, AL                                ; ee                          ; 0xc3488
    mov es, cx                                ; 8e c1                       ; 0xc3489 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc348b
    inc dx                                    ; 42                          ; 0xc348e vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc348f
    inc bx                                    ; 43                          ; 0xc3490 vgabios.c:2395
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc3491 vgabios.c:2396
    jmp short 03476h                          ; eb e0                       ; 0xc3494
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc3496 vgabios.c:2398
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3498
    out DX, AL                                ; ee                          ; 0xc349b
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc349c vgabios.c:2399
    mov es, cx                                ; 8e c1                       ; 0xc34a0 vgabios.c:47
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc34a2
    inc dx                                    ; 42                          ; 0xc34a5 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc34a6
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc34a7 vgabios.c:2402
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc34aa vgabios.c:47
    xor ah, ah                                ; 30 e4                       ; 0xc34ad vgabios.c:48
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc34af
    mov dx, 003dah                            ; ba da 03                    ; 0xc34b2 vgabios.c:2403
    in AL, DX                                 ; ec                          ; 0xc34b5
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc34b6
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc34b8 vgabios.c:2404
    jmp short 034c5h                          ; eb 06                       ; 0xc34bd
    cmp word [bp-00eh], strict byte 00013h    ; 83 7e f2 13                 ; 0xc34bf
    jnbe short 034deh                         ; 77 19                       ; 0xc34c3
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc34c5 vgabios.c:2405
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc34c8
    or ax, word [bp-00eh]                     ; 0b 46 f2                    ; 0xc34cb
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc34ce
    out DX, AL                                ; ee                          ; 0xc34d1
    mov es, cx                                ; 8e c1                       ; 0xc34d2 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc34d4
    out DX, AL                                ; ee                          ; 0xc34d7 vgabios.c:48
    inc bx                                    ; 43                          ; 0xc34d8 vgabios.c:2406
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc34d9 vgabios.c:2407
    jmp short 034bfh                          ; eb e1                       ; 0xc34dc
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc34de vgabios.c:2408
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc34e1
    out DX, AL                                ; ee                          ; 0xc34e4
    mov dx, 003dah                            ; ba da 03                    ; 0xc34e5 vgabios.c:2409
    in AL, DX                                 ; ec                          ; 0xc34e8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc34e9
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc34eb vgabios.c:2411
    jmp short 034f8h                          ; eb 06                       ; 0xc34f0
    cmp word [bp-00eh], strict byte 00008h    ; 83 7e f2 08                 ; 0xc34f2
    jnbe short 0350eh                         ; 77 16                       ; 0xc34f6
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc34f8 vgabios.c:2412
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc34fb
    out DX, AL                                ; ee                          ; 0xc34fe
    mov es, cx                                ; 8e c1                       ; 0xc34ff vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3501
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc3504 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3507
    inc bx                                    ; 43                          ; 0xc3508 vgabios.c:2413
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc3509 vgabios.c:2414
    jmp short 034f2h                          ; eb e4                       ; 0xc350c
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc350e vgabios.c:2415
    mov es, cx                                ; 8e c1                       ; 0xc3511 vgabios.c:47
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3513
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3516 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3519
    inc si                                    ; 46                          ; 0xc351a vgabios.c:2418
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc351b vgabios.c:47
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc351e vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3521
    inc si                                    ; 46                          ; 0xc3522 vgabios.c:2419
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3523 vgabios.c:47
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3526 vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3529
    inc si                                    ; 46                          ; 0xc352a vgabios.c:2420
    inc si                                    ; 46                          ; 0xc352b
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc352c vgabios.c:47
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc352f vgabios.c:48
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc3532
    out DX, AL                                ; ee                          ; 0xc3535
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc3536 vgabios.c:2424
    je near 03659h                            ; 0f 84 1b 01                 ; 0xc353a
    mov es, cx                                ; 8e c1                       ; 0xc353e vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3540
    mov si, strict word 00049h                ; be 49 00                    ; 0xc3543 vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3546
    mov es, dx                                ; 8e c2                       ; 0xc3549
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc354b
    inc bx                                    ; 43                          ; 0xc354e vgabios.c:2425
    mov es, cx                                ; 8e c1                       ; 0xc354f vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3551
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc3554 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3557
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3559
    inc bx                                    ; 43                          ; 0xc355c vgabios.c:2426
    inc bx                                    ; 43                          ; 0xc355d
    mov es, cx                                ; 8e c1                       ; 0xc355e vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3560
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc3563 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3566
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3568
    inc bx                                    ; 43                          ; 0xc356b vgabios.c:2427
    inc bx                                    ; 43                          ; 0xc356c
    mov es, cx                                ; 8e c1                       ; 0xc356d vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc356f
    mov si, strict word 00063h                ; be 63 00                    ; 0xc3572 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3575
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3577
    inc bx                                    ; 43                          ; 0xc357a vgabios.c:2428
    inc bx                                    ; 43                          ; 0xc357b
    mov es, cx                                ; 8e c1                       ; 0xc357c vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc357e
    mov si, 00084h                            ; be 84 00                    ; 0xc3581 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3584
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3586
    inc bx                                    ; 43                          ; 0xc3589 vgabios.c:2429
    mov es, cx                                ; 8e c1                       ; 0xc358a vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc358c
    mov si, 00085h                            ; be 85 00                    ; 0xc358f vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3592
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3594
    inc bx                                    ; 43                          ; 0xc3597 vgabios.c:2430
    inc bx                                    ; 43                          ; 0xc3598
    mov es, cx                                ; 8e c1                       ; 0xc3599 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc359b
    mov si, 00087h                            ; be 87 00                    ; 0xc359e vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35a1
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc35a3
    inc bx                                    ; 43                          ; 0xc35a6 vgabios.c:2431
    mov es, cx                                ; 8e c1                       ; 0xc35a7 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc35a9
    mov si, 00088h                            ; be 88 00                    ; 0xc35ac vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35af
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc35b1
    inc bx                                    ; 43                          ; 0xc35b4 vgabios.c:2432
    mov es, cx                                ; 8e c1                       ; 0xc35b5 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc35b7
    mov si, 00089h                            ; be 89 00                    ; 0xc35ba vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc35bd
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc35bf
    inc bx                                    ; 43                          ; 0xc35c2 vgabios.c:2433
    mov es, cx                                ; 8e c1                       ; 0xc35c3 vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35c5
    mov si, strict word 00060h                ; be 60 00                    ; 0xc35c8 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc35cb
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35cd
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc35d0 vgabios.c:2435
    inc bx                                    ; 43                          ; 0xc35d5 vgabios.c:2434
    inc bx                                    ; 43                          ; 0xc35d6
    jmp short 035dfh                          ; eb 06                       ; 0xc35d7
    cmp word [bp-00eh], strict byte 00008h    ; 83 7e f2 08                 ; 0xc35d9
    jnc short 035fbh                          ; 73 1c                       ; 0xc35dd
    mov es, cx                                ; 8e c1                       ; 0xc35df vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35e1
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc35e4 vgabios.c:58
    add si, si                                ; 01 f6                       ; 0xc35e7
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc35e9
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc35ec vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc35ef
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc35f1
    inc bx                                    ; 43                          ; 0xc35f4 vgabios.c:2437
    inc bx                                    ; 43                          ; 0xc35f5
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc35f6 vgabios.c:2438
    jmp short 035d9h                          ; eb de                       ; 0xc35f9
    mov es, cx                                ; 8e c1                       ; 0xc35fb vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc35fd
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc3600 vgabios.c:62
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3603
    mov es, dx                                ; 8e c2                       ; 0xc3606
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3608
    inc bx                                    ; 43                          ; 0xc360b vgabios.c:2439
    inc bx                                    ; 43                          ; 0xc360c
    mov es, cx                                ; 8e c1                       ; 0xc360d vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc360f
    mov si, strict word 00062h                ; be 62 00                    ; 0xc3612 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3615
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3617
    inc bx                                    ; 43                          ; 0xc361a vgabios.c:2440
    mov es, cx                                ; 8e c1                       ; 0xc361b vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc361d
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc3620 vgabios.c:62
    xor dx, dx                                ; 31 d2                       ; 0xc3623
    mov es, dx                                ; 8e c2                       ; 0xc3625
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3627
    inc bx                                    ; 43                          ; 0xc362a vgabios.c:2442
    inc bx                                    ; 43                          ; 0xc362b
    mov es, cx                                ; 8e c1                       ; 0xc362c vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc362e
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc3631 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3634
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3636
    inc bx                                    ; 43                          ; 0xc3639 vgabios.c:2443
    inc bx                                    ; 43                          ; 0xc363a
    mov es, cx                                ; 8e c1                       ; 0xc363b vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc363d
    mov si, 0010ch                            ; be 0c 01                    ; 0xc3640 vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3643
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3645
    inc bx                                    ; 43                          ; 0xc3648 vgabios.c:2444
    inc bx                                    ; 43                          ; 0xc3649
    mov es, cx                                ; 8e c1                       ; 0xc364a vgabios.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc364c
    mov si, 0010eh                            ; be 0e 01                    ; 0xc364f vgabios.c:62
    mov es, dx                                ; 8e c2                       ; 0xc3652
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3654
    inc bx                                    ; 43                          ; 0xc3657 vgabios.c:2445
    inc bx                                    ; 43                          ; 0xc3658
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc3659 vgabios.c:2447
    je short 0369ch                           ; 74 3d                       ; 0xc365d
    inc bx                                    ; 43                          ; 0xc365f vgabios.c:2448
    mov es, cx                                ; 8e c1                       ; 0xc3660 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3662
    xor ah, ah                                ; 30 e4                       ; 0xc3665 vgabios.c:48
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3667
    inc bx                                    ; 43                          ; 0xc366a vgabios.c:2449
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc366b vgabios.c:47
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc366e vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc3671
    inc bx                                    ; 43                          ; 0xc3672 vgabios.c:2450
    xor al, al                                ; 30 c0                       ; 0xc3673
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3675
    out DX, AL                                ; ee                          ; 0xc3678
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3679 vgabios.c:2453
    jmp short 03685h                          ; eb 07                       ; 0xc367c
    cmp word [bp-00eh], 00300h                ; 81 7e f2 00 03              ; 0xc367e
    jnc short 03694h                          ; 73 0f                       ; 0xc3683
    mov es, cx                                ; 8e c1                       ; 0xc3685 vgabios.c:47
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3687
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc368a vgabios.c:48
    out DX, AL                                ; ee                          ; 0xc368d
    inc bx                                    ; 43                          ; 0xc368e vgabios.c:2454
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc368f vgabios.c:2455
    jmp short 0367eh                          ; eb ea                       ; 0xc3692
    inc bx                                    ; 43                          ; 0xc3694 vgabios.c:2456
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3695
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3698
    out DX, AL                                ; ee                          ; 0xc369b
    mov ax, bx                                ; 89 d8                       ; 0xc369c vgabios.c:2460
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc369e
    pop di                                    ; 5f                          ; 0xc36a1
    pop si                                    ; 5e                          ; 0xc36a2
    pop cx                                    ; 59                          ; 0xc36a3
    pop bp                                    ; 5d                          ; 0xc36a4
    retn                                      ; c3                          ; 0xc36a5
  ; disGetNextSymbol 0xc36a6 LB 0xcb4 -> off=0x0 cb=0000000000000027 uValue=00000000000c36a6 'find_vga_entry'
find_vga_entry:                              ; 0xc36a6 LB 0x27
    push bx                                   ; 53                          ; 0xc36a6 vgabios.c:2469
    push dx                                   ; 52                          ; 0xc36a7
    push bp                                   ; 55                          ; 0xc36a8
    mov bp, sp                                ; 89 e5                       ; 0xc36a9
    mov dl, al                                ; 88 c2                       ; 0xc36ab
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc36ad vgabios.c:2471
    xor al, al                                ; 30 c0                       ; 0xc36af vgabios.c:2472
    jmp short 036b9h                          ; eb 06                       ; 0xc36b1
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc36b3 vgabios.c:2473
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc36b5
    jnbe short 036c7h                         ; 77 0e                       ; 0xc36b7
    movzx bx, al                              ; 0f b6 d8                    ; 0xc36b9
    sal bx, 003h                              ; c1 e3 03                    ; 0xc36bc
    cmp dl, byte [bx+047b4h]                  ; 3a 97 b4 47                 ; 0xc36bf
    jne short 036b3h                          ; 75 ee                       ; 0xc36c3
    mov ah, al                                ; 88 c4                       ; 0xc36c5
    mov al, ah                                ; 88 e0                       ; 0xc36c7 vgabios.c:2478
    pop bp                                    ; 5d                          ; 0xc36c9
    pop dx                                    ; 5a                          ; 0xc36ca
    pop bx                                    ; 5b                          ; 0xc36cb
    retn                                      ; c3                          ; 0xc36cc
  ; disGetNextSymbol 0xc36cd LB 0xc8d -> off=0x0 cb=000000000000000e uValue=00000000000c36cd 'readx_byte'
readx_byte:                                  ; 0xc36cd LB 0xe
    push bx                                   ; 53                          ; 0xc36cd vgabios.c:2490
    push bp                                   ; 55                          ; 0xc36ce
    mov bp, sp                                ; 89 e5                       ; 0xc36cf
    mov bx, dx                                ; 89 d3                       ; 0xc36d1
    mov es, ax                                ; 8e c0                       ; 0xc36d3 vgabios.c:2492
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc36d5
    pop bp                                    ; 5d                          ; 0xc36d8 vgabios.c:2493
    pop bx                                    ; 5b                          ; 0xc36d9
    retn                                      ; c3                          ; 0xc36da
  ; disGetNextSymbol 0xc36db LB 0xc7f -> off=0x8a cb=0000000000000464 uValue=00000000000c3765 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 0c2h, 03bh, 090h, 037h, 0cdh, 037h, 0e1h, 037h, 0f2h, 037h
    db  006h, 038h, 017h, 038h, 022h, 038h, 05ch, 038h, 060h, 038h, 071h, 038h, 08eh, 038h, 0abh, 038h
    db  0cbh, 038h, 0e8h, 038h, 0ffh, 038h, 00bh, 039h, 0f8h, 039h, 085h, 03ah, 0b2h, 03ah, 0c7h, 03ah
    db  009h, 03bh, 094h, 03bh, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 003h
    db  002h, 001h, 000h, 0c2h, 03bh, 02ch, 039h, 050h, 039h, 05eh, 039h, 06ch, 039h, 077h, 039h, 02ch
    db  039h, 050h, 039h, 05eh, 039h, 077h, 039h, 085h, 039h, 091h, 039h, 0ach, 039h, 0bdh, 039h, 0ceh
    db  039h, 0dfh, 039h, 00ah, 009h, 006h, 004h, 002h, 001h, 000h, 086h, 03bh, 031h, 03bh, 03fh, 03bh
    db  050h, 03bh, 060h, 03bh, 075h, 03bh, 086h, 03bh, 086h, 03bh
int10_func:                                  ; 0xc3765 LB 0x464
    push bp                                   ; 55                          ; 0xc3765 vgabios.c:2571
    mov bp, sp                                ; 89 e5                       ; 0xc3766
    push si                                   ; 56                          ; 0xc3768
    push di                                   ; 57                          ; 0xc3769
    push ax                                   ; 50                          ; 0xc376a
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc376b
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc376e vgabios.c:2576
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3771
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc3774
    jnbe near 03bc2h                          ; 0f 87 47 04                 ; 0xc3777
    push CS                                   ; 0e                          ; 0xc377b
    pop ES                                    ; 07                          ; 0xc377c
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc377d
    mov di, 036dbh                            ; bf db 36                    ; 0xc3780
    repne scasb                               ; f2 ae                       ; 0xc3783
    sal cx, 1                                 ; d1 e1                       ; 0xc3785
    mov di, cx                                ; 89 cf                       ; 0xc3787
    mov ax, word [cs:di+036f1h]               ; 2e 8b 85 f1 36              ; 0xc3789
    jmp ax                                    ; ff e0                       ; 0xc378e
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3790 vgabios.c:2579
    call 013ddh                               ; e8 46 dc                    ; 0xc3794
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3797 vgabios.c:2580
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc379a
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc379d
    je short 037b7h                           ; 74 15                       ; 0xc37a0
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc37a2
    je short 037aeh                           ; 74 07                       ; 0xc37a5
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc37a7
    jbe short 037b7h                          ; 76 0b                       ; 0xc37aa
    jmp short 037c0h                          ; eb 12                       ; 0xc37ac
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc37ae vgabios.c:2582
    xor al, al                                ; 30 c0                       ; 0xc37b1
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc37b3
    jmp short 037c7h                          ; eb 10                       ; 0xc37b5 vgabios.c:2583
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc37b7 vgabios.c:2591
    xor al, al                                ; 30 c0                       ; 0xc37ba
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc37bc
    jmp short 037c7h                          ; eb 07                       ; 0xc37be
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc37c0 vgabios.c:2594
    xor al, al                                ; 30 c0                       ; 0xc37c3
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc37c5
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc37c7
    jmp near 03bc2h                           ; e9 f5 03                    ; 0xc37ca vgabios.c:2596
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc37cd vgabios.c:2598
    movzx dx, al                              ; 0f b6 d0                    ; 0xc37d0
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc37d3
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37d6
    xor ah, ah                                ; 30 e4                       ; 0xc37d9
    call 0114ch                               ; e8 6e d9                    ; 0xc37db
    jmp near 03bc2h                           ; e9 e1 03                    ; 0xc37de vgabios.c:2599
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc37e1 vgabios.c:2601
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37e4
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37e7
    xor ah, ah                                ; 30 e4                       ; 0xc37ea
    call 01242h                               ; e8 53 da                    ; 0xc37ec
    jmp near 03bc2h                           ; e9 d0 03                    ; 0xc37ef vgabios.c:2602
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc37f2 vgabios.c:2604
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc37f5
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc37f8
    shr ax, 008h                              ; c1 e8 08                    ; 0xc37fb
    xor ah, ah                                ; 30 e4                       ; 0xc37fe
    call 00a93h                               ; e8 90 d2                    ; 0xc3800
    jmp near 03bc2h                           ; e9 bc 03                    ; 0xc3803 vgabios.c:2605
    xor ax, ax                                ; 31 c0                       ; 0xc3806 vgabios.c:2611
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3808
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc380b vgabios.c:2612
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc380e vgabios.c:2613
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc3811 vgabios.c:2614
    jmp near 03bc2h                           ; e9 ab 03                    ; 0xc3814 vgabios.c:2615
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3817 vgabios.c:2617
    xor ah, ah                                ; 30 e4                       ; 0xc381a
    call 012cbh                               ; e8 ac da                    ; 0xc381c
    jmp near 03bc2h                           ; e9 a0 03                    ; 0xc381f vgabios.c:2618
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3822 vgabios.c:2620
    push ax                                   ; 50                          ; 0xc3825
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc3826
    push ax                                   ; 50                          ; 0xc3829
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc382a
    xor ah, ah                                ; 30 e4                       ; 0xc382d
    push ax                                   ; 50                          ; 0xc382f
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3830
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3833
    xor ah, ah                                ; 30 e4                       ; 0xc3836
    push ax                                   ; 50                          ; 0xc3838
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc3839
    movzx cx, al                              ; 0f b6 c8                    ; 0xc383c
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc383f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3842
    movzx bx, al                              ; 0f b6 d8                    ; 0xc3845
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3848
    shr ax, 008h                              ; c1 e8 08                    ; 0xc384b
    movzx dx, al                              ; 0f b6 d0                    ; 0xc384e
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3851
    xor ah, ah                                ; 30 e4                       ; 0xc3854
    call 01b35h                               ; e8 dc e2                    ; 0xc3856
    jmp near 03bc2h                           ; e9 66 03                    ; 0xc3859 vgabios.c:2621
    xor ax, ax                                ; 31 c0                       ; 0xc385c vgabios.c:2623
    jmp short 03825h                          ; eb c5                       ; 0xc385e
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc3860 vgabios.c:2626
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3863
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3866
    xor ah, ah                                ; 30 e4                       ; 0xc3869
    call 00db0h                               ; e8 42 d5                    ; 0xc386b
    jmp near 03bc2h                           ; e9 51 03                    ; 0xc386e vgabios.c:2627
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3871 vgabios.c:2629
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3874
    movzx bx, al                              ; 0f b6 d8                    ; 0xc3877
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc387a
    shr ax, 008h                              ; c1 e8 08                    ; 0xc387d
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3880
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3883
    xor ah, ah                                ; 30 e4                       ; 0xc3886
    call 023beh                               ; e8 33 eb                    ; 0xc3888
    jmp near 03bc2h                           ; e9 34 03                    ; 0xc388b vgabios.c:2630
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc388e vgabios.c:2632
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3891
    movzx bx, al                              ; 0f b6 d8                    ; 0xc3894
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3897
    shr ax, 008h                              ; c1 e8 08                    ; 0xc389a
    movzx dx, al                              ; 0f b6 d0                    ; 0xc389d
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38a0
    xor ah, ah                                ; 30 e4                       ; 0xc38a3
    call 02523h                               ; e8 7b ec                    ; 0xc38a5
    jmp near 03bc2h                           ; e9 17 03                    ; 0xc38a8 vgabios.c:2633
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc38ab vgabios.c:2635
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc38ae
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38b1
    movzx dx, al                              ; 0f b6 d0                    ; 0xc38b4
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc38b7
    shr ax, 008h                              ; c1 e8 08                    ; 0xc38ba
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc38bd
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc38c0
    xor ah, ah                                ; 30 e4                       ; 0xc38c3
    call 02685h                               ; e8 bd ed                    ; 0xc38c5
    jmp near 03bc2h                           ; e9 f7 02                    ; 0xc38c8 vgabios.c:2636
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc38cb vgabios.c:2638
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc38ce
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc38d1
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc38d4
    shr ax, 008h                              ; c1 e8 08                    ; 0xc38d7
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc38da
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc38dd
    xor ah, ah                                ; 30 e4                       ; 0xc38e0
    call 00f6ah                               ; e8 85 d6                    ; 0xc38e2
    jmp near 03bc2h                           ; e9 da 02                    ; 0xc38e5 vgabios.c:2639
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc38e8 vgabios.c:2647
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc38eb
    movzx bx, al                              ; 0f b6 d8                    ; 0xc38ee
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc38f1
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc38f4
    xor ah, ah                                ; 30 e4                       ; 0xc38f7
    call 027eah                               ; e8 ee ee                    ; 0xc38f9
    jmp near 03bc2h                           ; e9 c3 02                    ; 0xc38fc vgabios.c:2648
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc38ff vgabios.c:2651
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3902
    call 010c0h                               ; e8 b8 d7                    ; 0xc3905
    jmp near 03bc2h                           ; e9 b7 02                    ; 0xc3908 vgabios.c:2652
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc390b vgabios.c:2654
    xor ah, ah                                ; 30 e4                       ; 0xc390e
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3910
    jnbe near 03bc2h                          ; 0f 87 ab 02                 ; 0xc3913
    push CS                                   ; 0e                          ; 0xc3917
    pop ES                                    ; 07                          ; 0xc3918
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc3919
    mov di, 0371fh                            ; bf 1f 37                    ; 0xc391c
    repne scasb                               ; f2 ae                       ; 0xc391f
    sal cx, 1                                 ; d1 e1                       ; 0xc3921
    mov di, cx                                ; 89 cf                       ; 0xc3923
    mov ax, word [cs:di+0372eh]               ; 2e 8b 85 2e 37              ; 0xc3925
    jmp ax                                    ; ff e0                       ; 0xc392a
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc392c vgabios.c:2658
    shr ax, 008h                              ; c1 e8 08                    ; 0xc392f
    xor ah, ah                                ; 30 e4                       ; 0xc3932
    push ax                                   ; 50                          ; 0xc3934
    movzx ax, byte [bp+00ch]                  ; 0f b6 46 0c                 ; 0xc3935
    push ax                                   ; 50                          ; 0xc3939
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc393a
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc393d
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3941
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc3944
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3947
    call 02b72h                               ; e8 25 f2                    ; 0xc394a
    jmp near 03bc2h                           ; e9 72 02                    ; 0xc394d vgabios.c:2659
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc3950 vgabios.c:2662
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3954
    call 02beeh                               ; e8 93 f2                    ; 0xc3958
    jmp near 03bc2h                           ; e9 64 02                    ; 0xc395b vgabios.c:2663
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc395e vgabios.c:2666
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3962
    call 02c5dh                               ; e8 f4 f2                    ; 0xc3966
    jmp near 03bc2h                           ; e9 56 02                    ; 0xc3969 vgabios.c:2667
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc396c vgabios.c:2669
    xor ah, ah                                ; 30 e4                       ; 0xc396f
    call 02b50h                               ; e8 dc f1                    ; 0xc3971
    jmp near 03bc2h                           ; e9 4b 02                    ; 0xc3974 vgabios.c:2670
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc3977 vgabios.c:2673
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc397b
    call 02cceh                               ; e8 4c f3                    ; 0xc397f
    jmp near 03bc2h                           ; e9 3d 02                    ; 0xc3982 vgabios.c:2674
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3985 vgabios.c:2676
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc3988
    call 02d3fh                               ; e8 b1 f3                    ; 0xc398b
    jmp near 03bc2h                           ; e9 31 02                    ; 0xc398e vgabios.c:2677
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3991 vgabios.c:2679
    xor ah, ah                                ; 30 e4                       ; 0xc3994
    push ax                                   ; 50                          ; 0xc3996
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3997
    movzx cx, al                              ; 0f b6 c8                    ; 0xc399a
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc399d
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc39a0
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc39a3
    call 02d9eh                               ; e8 f5 f3                    ; 0xc39a6
    jmp near 03bc2h                           ; e9 16 02                    ; 0xc39a9 vgabios.c:2680
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc39ac vgabios.c:2682
    movzx dx, al                              ; 0f b6 d0                    ; 0xc39af
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc39b2
    xor ah, ah                                ; 30 e4                       ; 0xc39b5
    call 02dbah                               ; e8 00 f4                    ; 0xc39b7
    jmp near 03bc2h                           ; e9 05 02                    ; 0xc39ba vgabios.c:2683
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc39bd vgabios.c:2685
    movzx dx, al                              ; 0f b6 d0                    ; 0xc39c0
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc39c3
    xor ah, ah                                ; 30 e4                       ; 0xc39c6
    call 02dd8h                               ; e8 0d f4                    ; 0xc39c8
    jmp near 03bc2h                           ; e9 f4 01                    ; 0xc39cb vgabios.c:2686
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc39ce vgabios.c:2688
    movzx dx, al                              ; 0f b6 d0                    ; 0xc39d1
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc39d4
    xor ah, ah                                ; 30 e4                       ; 0xc39d7
    call 02df6h                               ; e8 1a f4                    ; 0xc39d9
    jmp near 03bc2h                           ; e9 e3 01                    ; 0xc39dc vgabios.c:2689
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc39df vgabios.c:2691
    push ax                                   ; 50                          ; 0xc39e2
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc39e3
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc39e6
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc39e9
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc39ec
    shr ax, 008h                              ; c1 e8 08                    ; 0xc39ef
    call 00ee7h                               ; e8 f2 d4                    ; 0xc39f2
    jmp near 03bc2h                           ; e9 ca 01                    ; 0xc39f5 vgabios.c:2699
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc39f8 vgabios.c:2701
    xor ah, ah                                ; 30 e4                       ; 0xc39fb
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc39fd
    jc short 03a11h                           ; 72 0f                       ; 0xc3a00
    jbe short 03a44h                          ; 76 40                       ; 0xc3a02
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc3a04
    je short 03a7bh                           ; 74 72                       ; 0xc3a07
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc3a09
    je short 03a6ch                           ; 74 5e                       ; 0xc3a0c
    jmp near 03bc2h                           ; e9 b1 01                    ; 0xc3a0e
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3a11
    je short 03a23h                           ; 74 0d                       ; 0xc3a14
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc3a16
    jne near 03bc2h                           ; 0f 85 a5 01                 ; 0xc3a19
    call 02e14h                               ; e8 f4 f3                    ; 0xc3a1d vgabios.c:2704
    jmp near 03bc2h                           ; e9 9f 01                    ; 0xc3a20 vgabios.c:2705
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a23 vgabios.c:2707
    xor ah, ah                                ; 30 e4                       ; 0xc3a26
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3a28
    jnbe near 03bc2h                          ; 0f 87 93 01                 ; 0xc3a2b
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3a2f vgabios.c:2708
    xor ah, ah                                ; 30 e4                       ; 0xc3a32
    call 02e19h                               ; e8 e2 f3                    ; 0xc3a34
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a37 vgabios.c:2709
    xor al, al                                ; 30 c0                       ; 0xc3a3a
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc3a3c
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3a3e
    jmp near 03bc2h                           ; e9 7e 01                    ; 0xc3a41 vgabios.c:2711
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3a44 vgabios.c:2713
    xor ah, ah                                ; 30 e4                       ; 0xc3a47
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3a49
    jnc short 03a66h                          ; 73 18                       ; 0xc3a4c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3a4e vgabios.c:45
    mov si, 00087h                            ; be 87 00                    ; 0xc3a51
    mov es, ax                                ; 8e c0                       ; 0xc3a54 vgabios.c:47
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc3a56
    and dl, 0feh                              ; 80 e2 fe                    ; 0xc3a59 vgabios.c:48
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3a5c
    or dl, al                                 ; 08 c2                       ; 0xc3a5f
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc3a61 vgabios.c:52
    jmp short 03a37h                          ; eb d1                       ; 0xc3a64
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc3a66 vgabios.c:2719
    jmp near 03bc2h                           ; e9 56 01                    ; 0xc3a69 vgabios.c:2720
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3a6c vgabios.c:2722
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3a70
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3a73
    call 02e4bh                               ; e8 d2 f3                    ; 0xc3a76
    jmp short 03a37h                          ; eb bc                       ; 0xc3a79
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3a7b vgabios.c:2726
    xor ah, ah                                ; 30 e4                       ; 0xc3a7e
    call 02e50h                               ; e8 cd f3                    ; 0xc3a80
    jmp short 03a37h                          ; eb b2                       ; 0xc3a83
    push word [bp+008h]                       ; ff 76 08                    ; 0xc3a85 vgabios.c:2736
    push word [bp+016h]                       ; ff 76 16                    ; 0xc3a88
    movzx ax, byte [bp+00eh]                  ; 0f b6 46 0e                 ; 0xc3a8b
    push ax                                   ; 50                          ; 0xc3a8f
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3a90
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3a93
    xor ah, ah                                ; 30 e4                       ; 0xc3a96
    push ax                                   ; 50                          ; 0xc3a98
    movzx bx, byte [bp+00ch]                  ; 0f b6 5e 0c                 ; 0xc3a99
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3a9d
    shr dx, 008h                              ; c1 ea 08                    ; 0xc3aa0
    xor dh, dh                                ; 30 f6                       ; 0xc3aa3
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3aa5
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3aa9
    call 02e55h                               ; e8 a6 f3                    ; 0xc3aac
    jmp near 03bc2h                           ; e9 10 01                    ; 0xc3aaf vgabios.c:2737
    mov bx, si                                ; 89 f3                       ; 0xc3ab2 vgabios.c:2739
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3ab4
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3ab7
    call 02eebh                               ; e8 2e f4                    ; 0xc3aba
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3abd vgabios.c:2740
    xor al, al                                ; 30 c0                       ; 0xc3ac0
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc3ac2
    jmp near 03a3eh                           ; e9 77 ff                    ; 0xc3ac4
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3ac7 vgabios.c:2743
    xor ah, ah                                ; 30 e4                       ; 0xc3aca
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3acc
    je short 03af3h                           ; 74 22                       ; 0xc3acf
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3ad1
    je short 03ae5h                           ; 74 0f                       ; 0xc3ad4
    test ax, ax                               ; 85 c0                       ; 0xc3ad6
    jne short 03affh                          ; 75 25                       ; 0xc3ad8
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3ada vgabios.c:2746
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3add
    call 03100h                               ; e8 1d f6                    ; 0xc3ae0
    jmp short 03affh                          ; eb 1a                       ; 0xc3ae3 vgabios.c:2747
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3ae5 vgabios.c:2749
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3ae8
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3aeb
    call 03118h                               ; e8 27 f6                    ; 0xc3aee
    jmp short 03affh                          ; eb 0c                       ; 0xc3af1 vgabios.c:2750
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc3af3 vgabios.c:2752
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3af6
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3af9
    call 033eeh                               ; e8 ef f8                    ; 0xc3afc
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3aff vgabios.c:2759
    xor al, al                                ; 30 c0                       ; 0xc3b02
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc3b04
    jmp near 03a3eh                           ; e9 35 ff                    ; 0xc3b06
    call 007bfh                               ; e8 b3 cc                    ; 0xc3b09 vgabios.c:2764
    test ax, ax                               ; 85 c0                       ; 0xc3b0c
    je near 03b8dh                            ; 0f 84 7b 00                 ; 0xc3b0e
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3b12 vgabios.c:2765
    xor ah, ah                                ; 30 e4                       ; 0xc3b15
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc3b17
    jnbe short 03b86h                         ; 77 6a                       ; 0xc3b1a
    push CS                                   ; 0e                          ; 0xc3b1c
    pop ES                                    ; 07                          ; 0xc3b1d
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc3b1e
    mov di, 0374eh                            ; bf 4e 37                    ; 0xc3b21
    repne scasb                               ; f2 ae                       ; 0xc3b24
    sal cx, 1                                 ; d1 e1                       ; 0xc3b26
    mov di, cx                                ; 89 cf                       ; 0xc3b28
    mov ax, word [cs:di+03755h]               ; 2e 8b 85 55 37              ; 0xc3b2a
    jmp ax                                    ; ff e0                       ; 0xc3b2f
    mov bx, si                                ; 89 f3                       ; 0xc3b31 vgabios.c:2768
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3b33
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3b36
    call 03d93h                               ; e8 57 02                    ; 0xc3b39
    jmp near 03bc2h                           ; e9 83 00                    ; 0xc3b3c vgabios.c:2769
    mov cx, si                                ; 89 f1                       ; 0xc3b3f vgabios.c:2771
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3b41
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3b44
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3b47
    call 03eb8h                               ; e8 6b 03                    ; 0xc3b4a
    jmp near 03bc2h                           ; e9 72 00                    ; 0xc3b4d vgabios.c:2772
    mov cx, si                                ; 89 f1                       ; 0xc3b50 vgabios.c:2774
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3b52
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3b55
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3b58
    call 03f53h                               ; e8 f5 03                    ; 0xc3b5b
    jmp short 03bc2h                          ; eb 62                       ; 0xc3b5e vgabios.c:2775
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3b60 vgabios.c:2777
    push ax                                   ; 50                          ; 0xc3b63
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc3b64
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3b67
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3b6a
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3b6d
    call 0411ah                               ; e8 a7 05                    ; 0xc3b70
    jmp short 03bc2h                          ; eb 4d                       ; 0xc3b73 vgabios.c:2778
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3b75 vgabios.c:2780
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3b78
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3b7b
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3b7e
    call 041a6h                               ; e8 22 06                    ; 0xc3b81
    jmp short 03bc2h                          ; eb 3c                       ; 0xc3b84 vgabios.c:2781
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3b86 vgabios.c:2803
    jmp short 03bc2h                          ; eb 35                       ; 0xc3b8b vgabios.c:2806
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3b8d vgabios.c:2808
    jmp short 03bc2h                          ; eb 2e                       ; 0xc3b92 vgabios.c:2810
    call 007bfh                               ; e8 28 cc                    ; 0xc3b94 vgabios.c:2812
    test ax, ax                               ; 85 c0                       ; 0xc3b97
    je short 03bbdh                           ; 74 22                       ; 0xc3b99
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3b9b vgabios.c:2813
    xor ah, ah                                ; 30 e4                       ; 0xc3b9e
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3ba0
    jne short 03bb6h                          ; 75 11                       ; 0xc3ba3
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc3ba5 vgabios.c:2816
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc3ba8
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3bab
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3bae
    call 04275h                               ; e8 c1 06                    ; 0xc3bb1
    jmp short 03bc2h                          ; eb 0c                       ; 0xc3bb4 vgabios.c:2817
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3bb6 vgabios.c:2819
    jmp short 03bc2h                          ; eb 05                       ; 0xc3bbb vgabios.c:2822
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3bbd vgabios.c:2824
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3bc2 vgabios.c:2834
    pop di                                    ; 5f                          ; 0xc3bc5
    pop si                                    ; 5e                          ; 0xc3bc6
    pop bp                                    ; 5d                          ; 0xc3bc7
    retn                                      ; c3                          ; 0xc3bc8
  ; disGetNextSymbol 0xc3bc9 LB 0x791 -> off=0x0 cb=000000000000001f uValue=00000000000c3bc9 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3bc9 LB 0x1f
    push bp                                   ; 55                          ; 0xc3bc9 vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3bca
    push bx                                   ; 53                          ; 0xc3bcc
    push dx                                   ; 52                          ; 0xc3bcd
    mov bx, ax                                ; 89 c3                       ; 0xc3bce
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3bd0 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bd3
    call 00570h                               ; e8 97 c9                    ; 0xc3bd6
    mov ax, bx                                ; 89 d8                       ; 0xc3bd9 vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bdb
    call 00570h                               ; e8 8f c9                    ; 0xc3bde
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3be1 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc3be4
    pop bx                                    ; 5b                          ; 0xc3be5
    pop bp                                    ; 5d                          ; 0xc3be6
    retn                                      ; c3                          ; 0xc3be7
  ; disGetNextSymbol 0xc3be8 LB 0x772 -> off=0x0 cb=000000000000001f uValue=00000000000c3be8 'dispi_set_yres'
dispi_set_yres:                              ; 0xc3be8 LB 0x1f
    push bp                                   ; 55                          ; 0xc3be8 vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc3be9
    push bx                                   ; 53                          ; 0xc3beb
    push dx                                   ; 52                          ; 0xc3bec
    mov bx, ax                                ; 89 c3                       ; 0xc3bed
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3bef vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bf2
    call 00570h                               ; e8 78 c9                    ; 0xc3bf5
    mov ax, bx                                ; 89 d8                       ; 0xc3bf8 vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bfa
    call 00570h                               ; e8 70 c9                    ; 0xc3bfd
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3c00 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc3c03
    pop bx                                    ; 5b                          ; 0xc3c04
    pop bp                                    ; 5d                          ; 0xc3c05
    retn                                      ; c3                          ; 0xc3c06
  ; disGetNextSymbol 0xc3c07 LB 0x753 -> off=0x0 cb=0000000000000019 uValue=00000000000c3c07 'dispi_get_yres'
dispi_get_yres:                              ; 0xc3c07 LB 0x19
    push bp                                   ; 55                          ; 0xc3c07 vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc3c08
    push dx                                   ; 52                          ; 0xc3c0a
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3c0b vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c0e
    call 00570h                               ; e8 5c c9                    ; 0xc3c11
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c14 vbe.c:121
    call 00577h                               ; e8 5d c9                    ; 0xc3c17
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3c1a vbe.c:122
    pop dx                                    ; 5a                          ; 0xc3c1d
    pop bp                                    ; 5d                          ; 0xc3c1e
    retn                                      ; c3                          ; 0xc3c1f
  ; disGetNextSymbol 0xc3c20 LB 0x73a -> off=0x0 cb=000000000000001f uValue=00000000000c3c20 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc3c20 LB 0x1f
    push bp                                   ; 55                          ; 0xc3c20 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc3c21
    push bx                                   ; 53                          ; 0xc3c23
    push dx                                   ; 52                          ; 0xc3c24
    mov bx, ax                                ; 89 c3                       ; 0xc3c25
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3c27 vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c2a
    call 00570h                               ; e8 40 c9                    ; 0xc3c2d
    mov ax, bx                                ; 89 d8                       ; 0xc3c30 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c32
    call 00570h                               ; e8 38 c9                    ; 0xc3c35
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3c38 vbe.c:131
    pop dx                                    ; 5a                          ; 0xc3c3b
    pop bx                                    ; 5b                          ; 0xc3c3c
    pop bp                                    ; 5d                          ; 0xc3c3d
    retn                                      ; c3                          ; 0xc3c3e
  ; disGetNextSymbol 0xc3c3f LB 0x71b -> off=0x0 cb=0000000000000019 uValue=00000000000c3c3f 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc3c3f LB 0x19
    push bp                                   ; 55                          ; 0xc3c3f vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc3c40
    push dx                                   ; 52                          ; 0xc3c42
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3c43 vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c46
    call 00570h                               ; e8 24 c9                    ; 0xc3c49
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c4c vbe.c:136
    call 00577h                               ; e8 25 c9                    ; 0xc3c4f
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3c52 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc3c55
    pop bp                                    ; 5d                          ; 0xc3c56
    retn                                      ; c3                          ; 0xc3c57
  ; disGetNextSymbol 0xc3c58 LB 0x702 -> off=0x0 cb=000000000000001f uValue=00000000000c3c58 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc3c58 LB 0x1f
    push bp                                   ; 55                          ; 0xc3c58 vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3c59
    push bx                                   ; 53                          ; 0xc3c5b
    push dx                                   ; 52                          ; 0xc3c5c
    mov bx, ax                                ; 89 c3                       ; 0xc3c5d
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3c5f vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c62
    call 00570h                               ; e8 08 c9                    ; 0xc3c65
    mov ax, bx                                ; 89 d8                       ; 0xc3c68 vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c6a
    call 00570h                               ; e8 00 c9                    ; 0xc3c6d
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3c70 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc3c73
    pop bx                                    ; 5b                          ; 0xc3c74
    pop bp                                    ; 5d                          ; 0xc3c75
    retn                                      ; c3                          ; 0xc3c76
  ; disGetNextSymbol 0xc3c77 LB 0x6e3 -> off=0x0 cb=0000000000000019 uValue=00000000000c3c77 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc3c77 LB 0x19
    push bp                                   ; 55                          ; 0xc3c77 vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc3c78
    push dx                                   ; 52                          ; 0xc3c7a
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3c7b vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c7e
    call 00570h                               ; e8 ec c8                    ; 0xc3c81
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c84 vbe.c:151
    call 00577h                               ; e8 ed c8                    ; 0xc3c87
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3c8a vbe.c:152
    pop dx                                    ; 5a                          ; 0xc3c8d
    pop bp                                    ; 5d                          ; 0xc3c8e
    retn                                      ; c3                          ; 0xc3c8f
  ; disGetNextSymbol 0xc3c90 LB 0x6ca -> off=0x0 cb=0000000000000019 uValue=00000000000c3c90 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3c90 LB 0x19
    push bp                                   ; 55                          ; 0xc3c90 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3c91
    push dx                                   ; 52                          ; 0xc3c93
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc3c94 vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3c97
    call 00570h                               ; e8 d3 c8                    ; 0xc3c9a
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3c9d vbe.c:157
    call 00577h                               ; e8 d4 c8                    ; 0xc3ca0
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3ca3 vbe.c:158
    pop dx                                    ; 5a                          ; 0xc3ca6
    pop bp                                    ; 5d                          ; 0xc3ca7
    retn                                      ; c3                          ; 0xc3ca8
  ; disGetNextSymbol 0xc3ca9 LB 0x6b1 -> off=0x0 cb=0000000000000012 uValue=00000000000c3ca9 'in_word'
in_word:                                     ; 0xc3ca9 LB 0x12
    push bp                                   ; 55                          ; 0xc3ca9 vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc3caa
    push bx                                   ; 53                          ; 0xc3cac
    mov bx, ax                                ; 89 c3                       ; 0xc3cad
    mov ax, dx                                ; 89 d0                       ; 0xc3caf
    mov dx, bx                                ; 89 da                       ; 0xc3cb1 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc3cb3
    in ax, DX                                 ; ed                          ; 0xc3cb4 vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3cb5 vbe.c:164
    pop bx                                    ; 5b                          ; 0xc3cb8
    pop bp                                    ; 5d                          ; 0xc3cb9
    retn                                      ; c3                          ; 0xc3cba
  ; disGetNextSymbol 0xc3cbb LB 0x69f -> off=0x0 cb=0000000000000014 uValue=00000000000c3cbb 'in_byte'
in_byte:                                     ; 0xc3cbb LB 0x14
    push bp                                   ; 55                          ; 0xc3cbb vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc3cbc
    push bx                                   ; 53                          ; 0xc3cbe
    mov bx, ax                                ; 89 c3                       ; 0xc3cbf
    mov ax, dx                                ; 89 d0                       ; 0xc3cc1
    mov dx, bx                                ; 89 da                       ; 0xc3cc3 vbe.c:168
    out DX, ax                                ; ef                          ; 0xc3cc5
    in AL, DX                                 ; ec                          ; 0xc3cc6 vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3cc7
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3cc9 vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3ccc
    pop bp                                    ; 5d                          ; 0xc3ccd
    retn                                      ; c3                          ; 0xc3cce
  ; disGetNextSymbol 0xc3ccf LB 0x68b -> off=0x0 cb=0000000000000014 uValue=00000000000c3ccf 'dispi_get_id'
dispi_get_id:                                ; 0xc3ccf LB 0x14
    push bp                                   ; 55                          ; 0xc3ccf vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3cd0
    push dx                                   ; 52                          ; 0xc3cd2
    xor ax, ax                                ; 31 c0                       ; 0xc3cd3 vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3cd5
    out DX, ax                                ; ef                          ; 0xc3cd8
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3cd9 vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3cdc
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3cdd vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3ce0
    pop bp                                    ; 5d                          ; 0xc3ce1
    retn                                      ; c3                          ; 0xc3ce2
  ; disGetNextSymbol 0xc3ce3 LB 0x677 -> off=0x0 cb=000000000000001a uValue=00000000000c3ce3 'dispi_set_id'
dispi_set_id:                                ; 0xc3ce3 LB 0x1a
    push bp                                   ; 55                          ; 0xc3ce3 vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc3ce4
    push bx                                   ; 53                          ; 0xc3ce6
    push dx                                   ; 52                          ; 0xc3ce7
    mov bx, ax                                ; 89 c3                       ; 0xc3ce8
    xor ax, ax                                ; 31 c0                       ; 0xc3cea vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3cec
    out DX, ax                                ; ef                          ; 0xc3cef
    mov ax, bx                                ; 89 d8                       ; 0xc3cf0 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3cf2
    out DX, ax                                ; ef                          ; 0xc3cf5
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3cf6 vbe.c:183
    pop dx                                    ; 5a                          ; 0xc3cf9
    pop bx                                    ; 5b                          ; 0xc3cfa
    pop bp                                    ; 5d                          ; 0xc3cfb
    retn                                      ; c3                          ; 0xc3cfc
  ; disGetNextSymbol 0xc3cfd LB 0x65d -> off=0x0 cb=000000000000002a uValue=00000000000c3cfd 'vbe_init'
vbe_init:                                    ; 0xc3cfd LB 0x2a
    push bp                                   ; 55                          ; 0xc3cfd vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc3cfe
    push bx                                   ; 53                          ; 0xc3d00
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc3d01 vbe.c:190
    call 03ce3h                               ; e8 dc ff                    ; 0xc3d04
    call 03ccfh                               ; e8 c5 ff                    ; 0xc3d07 vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc3d0a
    jne short 03d21h                          ; 75 12                       ; 0xc3d0d
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc3d0f vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3d12
    mov es, ax                                ; 8e c0                       ; 0xc3d15
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc3d17
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc3d1b vbe.c:194
    call 03ce3h                               ; e8 c2 ff                    ; 0xc3d1e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3d21 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc3d24
    pop bp                                    ; 5d                          ; 0xc3d25
    retn                                      ; c3                          ; 0xc3d26
  ; disGetNextSymbol 0xc3d27 LB 0x633 -> off=0x0 cb=000000000000006c uValue=00000000000c3d27 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc3d27 LB 0x6c
    push bp                                   ; 55                          ; 0xc3d27 vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc3d28
    push bx                                   ; 53                          ; 0xc3d2a
    push cx                                   ; 51                          ; 0xc3d2b
    push si                                   ; 56                          ; 0xc3d2c
    push di                                   ; 57                          ; 0xc3d2d
    mov di, ax                                ; 89 c7                       ; 0xc3d2e
    mov si, dx                                ; 89 d6                       ; 0xc3d30
    xor dx, dx                                ; 31 d2                       ; 0xc3d32 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d34
    call 03ca9h                               ; e8 6f ff                    ; 0xc3d37
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3d3a vbe.c:209
    jne short 03d88h                          ; 75 49                       ; 0xc3d3d
    test si, si                               ; 85 f6                       ; 0xc3d3f vbe.c:213
    je short 03d56h                           ; 74 13                       ; 0xc3d41
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3d43 vbe.c:220
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d46
    call 00570h                               ; e8 24 c8                    ; 0xc3d49
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d4c vbe.c:221
    call 00577h                               ; e8 25 c8                    ; 0xc3d4f
    test ax, ax                               ; 85 c0                       ; 0xc3d52 vbe.c:222
    je short 03d8ah                           ; 74 34                       ; 0xc3d54
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc3d56 vbe.c:226
    mov dx, bx                                ; 89 da                       ; 0xc3d59 vbe.c:232
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d5b
    call 03ca9h                               ; e8 48 ff                    ; 0xc3d5e
    mov cx, ax                                ; 89 c1                       ; 0xc3d61
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3d63 vbe.c:233
    je short 03d88h                           ; 74 20                       ; 0xc3d66
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3d68 vbe.c:235
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3d6b
    call 03ca9h                               ; e8 38 ff                    ; 0xc3d6e
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3d71
    cmp cx, di                                ; 39 f9                       ; 0xc3d74 vbe.c:237
    jne short 03d84h                          ; 75 0c                       ; 0xc3d76
    test si, si                               ; 85 f6                       ; 0xc3d78 vbe.c:239
    jne short 03d80h                          ; 75 04                       ; 0xc3d7a
    mov ax, bx                                ; 89 d8                       ; 0xc3d7c vbe.c:240
    jmp short 03d8ah                          ; eb 0a                       ; 0xc3d7e
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3d80 vbe.c:241
    jne short 03d7ch                          ; 75 f8                       ; 0xc3d82
    mov bx, dx                                ; 89 d3                       ; 0xc3d84 vbe.c:244
    jmp short 03d5bh                          ; eb d3                       ; 0xc3d86 vbe.c:249
    xor ax, ax                                ; 31 c0                       ; 0xc3d88 vbe.c:252
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3d8a vbe.c:253
    pop di                                    ; 5f                          ; 0xc3d8d
    pop si                                    ; 5e                          ; 0xc3d8e
    pop cx                                    ; 59                          ; 0xc3d8f
    pop bx                                    ; 5b                          ; 0xc3d90
    pop bp                                    ; 5d                          ; 0xc3d91
    retn                                      ; c3                          ; 0xc3d92
  ; disGetNextSymbol 0xc3d93 LB 0x5c7 -> off=0x0 cb=0000000000000125 uValue=00000000000c3d93 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc3d93 LB 0x125
    push bp                                   ; 55                          ; 0xc3d93 vbe.c:284
    mov bp, sp                                ; 89 e5                       ; 0xc3d94
    push cx                                   ; 51                          ; 0xc3d96
    push si                                   ; 56                          ; 0xc3d97
    push di                                   ; 57                          ; 0xc3d98
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3d99
    mov si, ax                                ; 89 c6                       ; 0xc3d9c
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3d9e
    mov di, bx                                ; 89 df                       ; 0xc3da1
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc3da3 vbe.c:289
    call 005b7h                               ; e8 0c c8                    ; 0xc3da8 vbe.c:292
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc3dab
    mov bx, di                                ; 89 fb                       ; 0xc3dae vbe.c:295
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3db0
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3db3
    xor dx, dx                                ; 31 d2                       ; 0xc3db6 vbe.c:298
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3db8
    call 03ca9h                               ; e8 eb fe                    ; 0xc3dbb
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3dbe vbe.c:299
    je short 03dcdh                           ; 74 0a                       ; 0xc3dc1
    push SS                                   ; 16                          ; 0xc3dc3 vbe.c:301
    pop ES                                    ; 07                          ; 0xc3dc4
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc3dc5
    jmp near 03eb0h                           ; e9 e3 00                    ; 0xc3dca vbe.c:305
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc3dcd vbe.c:307
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc3dd0 vbe.c:314
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3dd5 vbe.c:322
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc3dd8
    jne short 03de7h                          ; 75 07                       ; 0xc3dde
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc3de0
    je short 03df6h                           ; 74 0f                       ; 0xc3de5
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc3de7
    jne short 03dfbh                          ; 75 0c                       ; 0xc3ded
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3def
    jne short 03dfbh                          ; 75 05                       ; 0xc3df4
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3df6 vbe.c:324
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3dfb vbe.c:332
    db  066h, 026h, 0c7h, 007h, 056h, 045h, 053h, 041h
    ; mov dword [es:bx], strict dword 041534556h ; 66 26 c7 07 56 45 53 41  ; 0xc3dfe
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3e06 vbe.c:338
    mov word [es:bx+006h], 07e08h             ; 26 c7 47 06 08 7e           ; 0xc3e0c vbe.c:341
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc3e12
    db  066h, 026h, 0c7h, 047h, 00ah, 001h, 000h, 000h, 000h
    ; mov dword [es:bx+00ah], strict dword 000000001h ; 66 26 c7 47 0a 01 00 00 00; 0xc3e16 vbe.c:344
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3e1f vbe.c:350
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc3e22
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc3e26 vbe.c:351
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3e29
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3e2d vbe.c:354
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3e30
    call 03ca9h                               ; e8 73 fe                    ; 0xc3e33
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3e36
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3e39
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc3e3d vbe.c:356
    je short 03e67h                           ; 74 24                       ; 0xc3e41
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3e43 vbe.c:359
    mov word [es:bx+016h], 07e1dh             ; 26 c7 47 16 1d 7e           ; 0xc3e49 vbe.c:360
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc3e4f
    mov word [es:bx+01ah], 07e3ah             ; 26 c7 47 1a 3a 7e           ; 0xc3e53 vbe.c:361
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3e59
    mov word [es:bx+01eh], 07e5bh             ; 26 c7 47 1e 5b 7e           ; 0xc3e5d vbe.c:362
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3e63
    mov dx, cx                                ; 89 ca                       ; 0xc3e67 vbe.c:369
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3e69
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3e6c
    call 03cbbh                               ; e8 49 fe                    ; 0xc3e6f
    xor ah, ah                                ; 30 e4                       ; 0xc3e72 vbe.c:370
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3e74
    jnbe short 03e90h                         ; 77 17                       ; 0xc3e77
    mov dx, cx                                ; 89 ca                       ; 0xc3e79 vbe.c:372
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3e7b
    call 03ca9h                               ; e8 28 fe                    ; 0xc3e7e
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc3e81 vbe.c:376
    add bx, di                                ; 01 fb                       ; 0xc3e84
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3e86 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3e89
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc3e8c vbe.c:378
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc3e90 vbe.c:380
    mov dx, cx                                ; 89 ca                       ; 0xc3e93 vbe.c:381
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3e95
    call 03ca9h                               ; e8 0e fe                    ; 0xc3e98
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc3e9b vbe.c:382
    jne short 03e67h                          ; 75 c7                       ; 0xc3e9e
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc3ea0 vbe.c:385
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3ea3 vbe.c:62
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc3ea6
    push SS                                   ; 16                          ; 0xc3ea9 vbe.c:386
    pop ES                                    ; 07                          ; 0xc3eaa
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc3eab
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3eb0 vbe.c:387
    pop di                                    ; 5f                          ; 0xc3eb3
    pop si                                    ; 5e                          ; 0xc3eb4
    pop cx                                    ; 59                          ; 0xc3eb5
    pop bp                                    ; 5d                          ; 0xc3eb6
    retn                                      ; c3                          ; 0xc3eb7
  ; disGetNextSymbol 0xc3eb8 LB 0x4a2 -> off=0x0 cb=000000000000009b uValue=00000000000c3eb8 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc3eb8 LB 0x9b
    push bp                                   ; 55                          ; 0xc3eb8 vbe.c:399
    mov bp, sp                                ; 89 e5                       ; 0xc3eb9
    push si                                   ; 56                          ; 0xc3ebb
    push di                                   ; 57                          ; 0xc3ebc
    push ax                                   ; 50                          ; 0xc3ebd
    push ax                                   ; 50                          ; 0xc3ebe
    mov ax, dx                                ; 89 d0                       ; 0xc3ebf
    mov si, bx                                ; 89 de                       ; 0xc3ec1
    mov bx, cx                                ; 89 cb                       ; 0xc3ec3
    test dh, 040h                             ; f6 c6 40                    ; 0xc3ec5 vbe.c:410
    db  00fh, 095h, 0c2h
    ; setne dl                                  ; 0f 95 c2                  ; 0xc3ec8
    xor dh, dh                                ; 30 f6                       ; 0xc3ecb
    and ah, 001h                              ; 80 e4 01                    ; 0xc3ecd vbe.c:411
    call 03d27h                               ; e8 54 fe                    ; 0xc3ed0 vbe.c:413
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3ed3
    test ax, ax                               ; 85 c0                       ; 0xc3ed6 vbe.c:415
    je short 03f41h                           ; 74 67                       ; 0xc3ed8
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3eda vbe.c:420
    xor ax, ax                                ; 31 c0                       ; 0xc3edd
    mov di, bx                                ; 89 df                       ; 0xc3edf
    mov es, si                                ; 8e c6                       ; 0xc3ee1
    jcxz 03ee7h                               ; e3 02                       ; 0xc3ee3
    rep stosb                                 ; f3 aa                       ; 0xc3ee5
    xor cx, cx                                ; 31 c9                       ; 0xc3ee7 vbe.c:421
    jmp short 03ef0h                          ; eb 05                       ; 0xc3ee9
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3eeb
    jnc short 03f09h                          ; 73 19                       ; 0xc3eee
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3ef0 vbe.c:424
    inc dx                                    ; 42                          ; 0xc3ef3
    inc dx                                    ; 42                          ; 0xc3ef4
    add dx, cx                                ; 01 ca                       ; 0xc3ef5
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ef7
    call 03cbbh                               ; e8 be fd                    ; 0xc3efa
    mov di, bx                                ; 89 df                       ; 0xc3efd vbe.c:425
    add di, cx                                ; 01 cf                       ; 0xc3eff
    mov es, si                                ; 8e c6                       ; 0xc3f01 vbe.c:52
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc3f03
    inc cx                                    ; 41                          ; 0xc3f06 vbe.c:426
    jmp short 03eebh                          ; eb e2                       ; 0xc3f07
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc3f09 vbe.c:427
    mov es, si                                ; 8e c6                       ; 0xc3f0c vbe.c:47
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3f0e
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3f11 vbe.c:428
    je short 03f25h                           ; 74 10                       ; 0xc3f13
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc3f15 vbe.c:429
    mov word [es:di], 00629h                  ; 26 c7 05 29 06              ; 0xc3f18 vbe.c:62
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc3f1d vbe.c:431
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc3f20 vbe.c:62
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3f25 vbe.c:434
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3f28
    call 00570h                               ; e8 42 c6                    ; 0xc3f2b
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3f2e vbe.c:435
    call 00577h                               ; e8 43 c6                    ; 0xc3f31
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc3f34
    mov es, si                                ; 8e c6                       ; 0xc3f37 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f39
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3f3c vbe.c:437
    jmp short 03f44h                          ; eb 03                       ; 0xc3f3f vbe.c:438
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3f41 vbe.c:442
    push SS                                   ; 16                          ; 0xc3f44 vbe.c:445
    pop ES                                    ; 07                          ; 0xc3f45
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3f46
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f49
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f4c vbe.c:446
    pop di                                    ; 5f                          ; 0xc3f4f
    pop si                                    ; 5e                          ; 0xc3f50
    pop bp                                    ; 5d                          ; 0xc3f51
    retn                                      ; c3                          ; 0xc3f52
  ; disGetNextSymbol 0xc3f53 LB 0x407 -> off=0x0 cb=00000000000000e5 uValue=00000000000c3f53 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3f53 LB 0xe5
    push bp                                   ; 55                          ; 0xc3f53 vbe.c:458
    mov bp, sp                                ; 89 e5                       ; 0xc3f54
    push si                                   ; 56                          ; 0xc3f56
    push di                                   ; 57                          ; 0xc3f57
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc3f58
    mov si, ax                                ; 89 c6                       ; 0xc3f5b
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3f5d
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc3f60 vbe.c:466
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0                  ; 0xc3f64
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3f67
    mov ax, dx                                ; 89 d0                       ; 0xc3f6a
    test dx, dx                               ; 85 d2                       ; 0xc3f6c vbe.c:467
    je short 03f73h                           ; 74 03                       ; 0xc3f6e
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3f70
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc3f73
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc3f76 vbe.c:468
    je short 03f81h                           ; 74 05                       ; 0xc3f7a
    mov dx, 00080h                            ; ba 80 00                    ; 0xc3f7c
    jmp short 03f83h                          ; eb 02                       ; 0xc3f7f
    xor dx, dx                                ; 31 d2                       ; 0xc3f81
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc3f83
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc3f86 vbe.c:470
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc3f8a vbe.c:473
    jnc short 03fa3h                          ; 73 12                       ; 0xc3f8f
    xor ax, ax                                ; 31 c0                       ; 0xc3f91 vbe.c:477
    call 005ddh                               ; e8 47 c6                    ; 0xc3f93
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc3f96 vbe.c:481
    call 013ddh                               ; e8 40 d4                    ; 0xc3f9a
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3f9d vbe.c:482
    jmp near 0402ch                           ; e9 89 00                    ; 0xc3fa0 vbe.c:483
    mov dx, ax                                ; 89 c2                       ; 0xc3fa3 vbe.c:486
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3fa5
    call 03d27h                               ; e8 7c fd                    ; 0xc3fa8
    mov bx, ax                                ; 89 c3                       ; 0xc3fab
    test ax, ax                               ; 85 c0                       ; 0xc3fad vbe.c:488
    je short 04029h                           ; 74 78                       ; 0xc3faf
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc3fb1 vbe.c:493
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3fb4
    call 03ca9h                               ; e8 ef fc                    ; 0xc3fb7
    mov cx, ax                                ; 89 c1                       ; 0xc3fba
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3fbc vbe.c:494
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3fbf
    call 03ca9h                               ; e8 e4 fc                    ; 0xc3fc2
    mov di, ax                                ; 89 c7                       ; 0xc3fc5
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3fc7 vbe.c:495
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3fca
    call 03cbbh                               ; e8 eb fc                    ; 0xc3fcd
    mov bl, al                                ; 88 c3                       ; 0xc3fd0
    mov dl, al                                ; 88 c2                       ; 0xc3fd2
    xor ax, ax                                ; 31 c0                       ; 0xc3fd4 vbe.c:503
    call 005ddh                               ; e8 04 c6                    ; 0xc3fd6
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3fd9 vbe.c:505
    jne short 03fe4h                          ; 75 06                       ; 0xc3fdc
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3fde vbe.c:507
    call 013ddh                               ; e8 f9 d3                    ; 0xc3fe1
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc3fe4 vbe.c:510
    call 03c20h                               ; e8 36 fc                    ; 0xc3fe7
    mov ax, cx                                ; 89 c8                       ; 0xc3fea vbe.c:511
    call 03bc9h                               ; e8 da fb                    ; 0xc3fec
    mov ax, di                                ; 89 f8                       ; 0xc3fef vbe.c:512
    call 03be8h                               ; e8 f4 fb                    ; 0xc3ff1
    xor ax, ax                                ; 31 c0                       ; 0xc3ff4 vbe.c:513
    call 00603h                               ; e8 0a c6                    ; 0xc3ff6
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3ff9 vbe.c:514
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc3ffc
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3ffe
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc4001
    or ax, dx                                 ; 09 d0                       ; 0xc4005
    call 005ddh                               ; e8 d3 c5                    ; 0xc4007
    call 006d2h                               ; e8 c5 c6                    ; 0xc400a vbe.c:515
    mov bx, 000bah                            ; bb ba 00                    ; 0xc400d vbe.c:62
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc4010
    mov es, ax                                ; 8e c0                       ; 0xc4013
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4015
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4018
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc401b vbe.c:518
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc401e
    mov bx, 00087h                            ; bb 87 00                    ; 0xc4020 vbe.c:52
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc4023
    jmp near 03f9dh                           ; e9 74 ff                    ; 0xc4026
    mov ax, 00100h                            ; b8 00 01                    ; 0xc4029 vbe.c:527
    push SS                                   ; 16                          ; 0xc402c vbe.c:531
    pop ES                                    ; 07                          ; 0xc402d
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc402e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc4031 vbe.c:532
    pop di                                    ; 5f                          ; 0xc4034
    pop si                                    ; 5e                          ; 0xc4035
    pop bp                                    ; 5d                          ; 0xc4036
    retn                                      ; c3                          ; 0xc4037
  ; disGetNextSymbol 0xc4038 LB 0x322 -> off=0x0 cb=0000000000000008 uValue=00000000000c4038 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc4038 LB 0x8
    push bp                                   ; 55                          ; 0xc4038 vbe.c:534
    mov bp, sp                                ; 89 e5                       ; 0xc4039
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc403b vbe.c:537
    pop bp                                    ; 5d                          ; 0xc403e
    retn                                      ; c3                          ; 0xc403f
  ; disGetNextSymbol 0xc4040 LB 0x31a -> off=0x0 cb=000000000000004b uValue=00000000000c4040 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc4040 LB 0x4b
    push bp                                   ; 55                          ; 0xc4040 vbe.c:539
    mov bp, sp                                ; 89 e5                       ; 0xc4041
    push bx                                   ; 53                          ; 0xc4043
    push cx                                   ; 51                          ; 0xc4044
    push si                                   ; 56                          ; 0xc4045
    mov si, ax                                ; 89 c6                       ; 0xc4046
    mov bx, dx                                ; 89 d3                       ; 0xc4048
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc404a vbe.c:543
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc404d
    out DX, ax                                ; ef                          ; 0xc4050
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4051 vbe.c:544
    in ax, DX                                 ; ed                          ; 0xc4054
    mov es, si                                ; 8e c6                       ; 0xc4055 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4057
    inc bx                                    ; 43                          ; 0xc405a vbe.c:546
    inc bx                                    ; 43                          ; 0xc405b
    test AL, strict byte 001h                 ; a8 01                       ; 0xc405c vbe.c:547
    je short 04083h                           ; 74 23                       ; 0xc405e
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc4060 vbe.c:549
    jmp short 0406ah                          ; eb 05                       ; 0xc4063
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc4065
    jnbe short 04083h                         ; 77 19                       ; 0xc4068
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc406a vbe.c:550
    je short 04080h                           ; 74 11                       ; 0xc406d
    mov ax, cx                                ; 89 c8                       ; 0xc406f vbe.c:551
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4071
    out DX, ax                                ; ef                          ; 0xc4074
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4075 vbe.c:552
    in ax, DX                                 ; ed                          ; 0xc4078
    mov es, si                                ; 8e c6                       ; 0xc4079 vbe.c:62
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc407b
    inc bx                                    ; 43                          ; 0xc407e vbe.c:553
    inc bx                                    ; 43                          ; 0xc407f
    inc cx                                    ; 41                          ; 0xc4080 vbe.c:555
    jmp short 04065h                          ; eb e2                       ; 0xc4081
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc4083 vbe.c:556
    pop si                                    ; 5e                          ; 0xc4086
    pop cx                                    ; 59                          ; 0xc4087
    pop bx                                    ; 5b                          ; 0xc4088
    pop bp                                    ; 5d                          ; 0xc4089
    retn                                      ; c3                          ; 0xc408a
  ; disGetNextSymbol 0xc408b LB 0x2cf -> off=0x0 cb=000000000000008f uValue=00000000000c408b 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc408b LB 0x8f
    push bp                                   ; 55                          ; 0xc408b vbe.c:559
    mov bp, sp                                ; 89 e5                       ; 0xc408c
    push bx                                   ; 53                          ; 0xc408e
    push cx                                   ; 51                          ; 0xc408f
    push si                                   ; 56                          ; 0xc4090
    push ax                                   ; 50                          ; 0xc4091
    mov cx, ax                                ; 89 c1                       ; 0xc4092
    mov bx, dx                                ; 89 d3                       ; 0xc4094
    mov es, ax                                ; 8e c0                       ; 0xc4096 vbe.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc4098
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc409b
    inc bx                                    ; 43                          ; 0xc409e vbe.c:564
    inc bx                                    ; 43                          ; 0xc409f
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc40a0 vbe.c:566
    jne short 040b6h                          ; 75 10                       ; 0xc40a4
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc40a6 vbe.c:567
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc40a9
    out DX, ax                                ; ef                          ; 0xc40ac
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc40ad vbe.c:568
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc40b0
    out DX, ax                                ; ef                          ; 0xc40b3
    jmp short 04112h                          ; eb 5c                       ; 0xc40b4 vbe.c:569
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc40b6 vbe.c:570
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc40b9
    out DX, ax                                ; ef                          ; 0xc40bc
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc40bd vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc40c0 vbe.c:58
    out DX, ax                                ; ef                          ; 0xc40c3
    inc bx                                    ; 43                          ; 0xc40c4 vbe.c:572
    inc bx                                    ; 43                          ; 0xc40c5
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc40c6
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc40c9
    out DX, ax                                ; ef                          ; 0xc40cc
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc40cd vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc40d0 vbe.c:58
    out DX, ax                                ; ef                          ; 0xc40d3
    inc bx                                    ; 43                          ; 0xc40d4 vbe.c:575
    inc bx                                    ; 43                          ; 0xc40d5
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc40d6
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc40d9
    out DX, ax                                ; ef                          ; 0xc40dc
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc40dd vbe.c:57
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc40e0 vbe.c:58
    out DX, ax                                ; ef                          ; 0xc40e3
    inc bx                                    ; 43                          ; 0xc40e4 vbe.c:578
    inc bx                                    ; 43                          ; 0xc40e5
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc40e6
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc40e9
    out DX, ax                                ; ef                          ; 0xc40ec
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc40ed vbe.c:580
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc40f0
    out DX, ax                                ; ef                          ; 0xc40f3
    mov si, strict word 00005h                ; be 05 00                    ; 0xc40f4 vbe.c:582
    jmp short 040feh                          ; eb 05                       ; 0xc40f7
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc40f9
    jnbe short 04112h                         ; 77 14                       ; 0xc40fc
    mov ax, si                                ; 89 f0                       ; 0xc40fe vbe.c:583
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc4100
    out DX, ax                                ; ef                          ; 0xc4103
    mov es, cx                                ; 8e c1                       ; 0xc4104 vbe.c:57
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc4106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc4109 vbe.c:58
    out DX, ax                                ; ef                          ; 0xc410c
    inc bx                                    ; 43                          ; 0xc410d vbe.c:585
    inc bx                                    ; 43                          ; 0xc410e
    inc si                                    ; 46                          ; 0xc410f vbe.c:586
    jmp short 040f9h                          ; eb e7                       ; 0xc4110
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc4112 vbe.c:588
    pop si                                    ; 5e                          ; 0xc4115
    pop cx                                    ; 59                          ; 0xc4116
    pop bx                                    ; 5b                          ; 0xc4117
    pop bp                                    ; 5d                          ; 0xc4118
    retn                                      ; c3                          ; 0xc4119
  ; disGetNextSymbol 0xc411a LB 0x240 -> off=0x0 cb=000000000000008c uValue=00000000000c411a 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc411a LB 0x8c
    push bp                                   ; 55                          ; 0xc411a vbe.c:604
    mov bp, sp                                ; 89 e5                       ; 0xc411b
    push si                                   ; 56                          ; 0xc411d
    push di                                   ; 57                          ; 0xc411e
    push ax                                   ; 50                          ; 0xc411f
    mov si, ax                                ; 89 c6                       ; 0xc4120
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc4122
    mov ax, bx                                ; 89 d8                       ; 0xc4125
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc4127
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc412a vbe.c:609
    xor ah, ah                                ; 30 e4                       ; 0xc412d vbe.c:610
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc412f
    je short 04179h                           ; 74 45                       ; 0xc4132
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc4134
    je short 0415dh                           ; 74 24                       ; 0xc4137
    test ax, ax                               ; 85 c0                       ; 0xc4139
    jne short 04195h                          ; 75 58                       ; 0xc413b
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc413d vbe.c:612
    call 030ddh                               ; e8 9a ef                    ; 0xc4140
    mov cx, ax                                ; 89 c1                       ; 0xc4143
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4145 vbe.c:616
    je short 04150h                           ; 74 05                       ; 0xc4149
    call 04038h                               ; e8 ea fe                    ; 0xc414b vbe.c:617
    add ax, cx                                ; 01 c8                       ; 0xc414e
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc4150 vbe.c:618
    shr ax, 006h                              ; c1 e8 06                    ; 0xc4153
    push SS                                   ; 16                          ; 0xc4156
    pop ES                                    ; 07                          ; 0xc4157
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4158
    jmp short 04198h                          ; eb 3b                       ; 0xc415b vbe.c:619
    push SS                                   ; 16                          ; 0xc415d vbe.c:621
    pop ES                                    ; 07                          ; 0xc415e
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc415f
    mov dx, cx                                ; 89 ca                       ; 0xc4162 vbe.c:622
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4164
    call 03118h                               ; e8 ae ef                    ; 0xc4167
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc416a vbe.c:626
    je short 04198h                           ; 74 28                       ; 0xc416e
    mov dx, ax                                ; 89 c2                       ; 0xc4170 vbe.c:627
    mov ax, cx                                ; 89 c8                       ; 0xc4172
    call 04040h                               ; e8 c9 fe                    ; 0xc4174
    jmp short 04198h                          ; eb 1f                       ; 0xc4177 vbe.c:628
    push SS                                   ; 16                          ; 0xc4179 vbe.c:630
    pop ES                                    ; 07                          ; 0xc417a
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc417b
    mov dx, cx                                ; 89 ca                       ; 0xc417e vbe.c:631
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4180
    call 033eeh                               ; e8 68 f2                    ; 0xc4183
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc4186 vbe.c:635
    je short 04198h                           ; 74 0c                       ; 0xc418a
    mov dx, ax                                ; 89 c2                       ; 0xc418c vbe.c:636
    mov ax, cx                                ; 89 c8                       ; 0xc418e
    call 0408bh                               ; e8 f8 fe                    ; 0xc4190
    jmp short 04198h                          ; eb 03                       ; 0xc4193 vbe.c:637
    mov di, 00100h                            ; bf 00 01                    ; 0xc4195 vbe.c:640
    push SS                                   ; 16                          ; 0xc4198 vbe.c:643
    pop ES                                    ; 07                          ; 0xc4199
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc419a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc419d vbe.c:644
    pop di                                    ; 5f                          ; 0xc41a0
    pop si                                    ; 5e                          ; 0xc41a1
    pop bp                                    ; 5d                          ; 0xc41a2
    retn 00002h                               ; c2 02 00                    ; 0xc41a3
  ; disGetNextSymbol 0xc41a6 LB 0x1b4 -> off=0x0 cb=00000000000000cf uValue=00000000000c41a6 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc41a6 LB 0xcf
    push bp                                   ; 55                          ; 0xc41a6 vbe.c:665
    mov bp, sp                                ; 89 e5                       ; 0xc41a7
    push si                                   ; 56                          ; 0xc41a9
    push di                                   ; 57                          ; 0xc41aa
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc41ab
    push ax                                   ; 50                          ; 0xc41ae
    mov di, dx                                ; 89 d7                       ; 0xc41af
    mov si, bx                                ; 89 de                       ; 0xc41b1
    mov word [bp-008h], cx                    ; 89 4e f8                    ; 0xc41b3
    call 03c3fh                               ; e8 86 fa                    ; 0xc41b6 vbe.c:674
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc41b9 vbe.c:675
    jne short 041c2h                          ; 75 05                       ; 0xc41bb
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc41bd
    jmp short 041c5h                          ; eb 03                       ; 0xc41c0
    movzx cx, al                              ; 0f b6 c8                    ; 0xc41c2
    call 03c77h                               ; e8 af fa                    ; 0xc41c5 vbe.c:676
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc41c8
    mov word [bp-006h], strict word 0004fh    ; c7 46 fa 4f 00              ; 0xc41cb vbe.c:677
    push SS                                   ; 16                          ; 0xc41d0 vbe.c:678
    pop ES                                    ; 07                          ; 0xc41d1
    mov bx, word [es:si]                      ; 26 8b 1c                    ; 0xc41d2
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc41d5 vbe.c:679
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc41d8 vbe.c:683
    je short 041e7h                           ; 74 0b                       ; 0xc41da
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc41dc
    je short 0420eh                           ; 74 2e                       ; 0xc41de
    test al, al                               ; 84 c0                       ; 0xc41e0
    je short 04209h                           ; 74 25                       ; 0xc41e2
    jmp near 0425eh                           ; e9 77 00                    ; 0xc41e4
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc41e7 vbe.c:685
    jne short 041f1h                          ; 75 05                       ; 0xc41ea
    sal bx, 003h                              ; c1 e3 03                    ; 0xc41ec vbe.c:686
    jmp short 04209h                          ; eb 18                       ; 0xc41ef vbe.c:687
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc41f1 vbe.c:688
    cwd                                       ; 99                          ; 0xc41f4
    sal dx, 003h                              ; c1 e2 03                    ; 0xc41f5
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc41f8
    sar ax, 003h                              ; c1 f8 03                    ; 0xc41fa
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc41fd
    mov ax, bx                                ; 89 d8                       ; 0xc4200
    xor dx, dx                                ; 31 d2                       ; 0xc4202
    div word [bp-00ch]                        ; f7 76 f4                    ; 0xc4204
    mov bx, ax                                ; 89 c3                       ; 0xc4207
    mov ax, bx                                ; 89 d8                       ; 0xc4209 vbe.c:691
    call 03c58h                               ; e8 4a fa                    ; 0xc420b
    call 03c77h                               ; e8 66 fa                    ; 0xc420e vbe.c:694
    mov bx, ax                                ; 89 c3                       ; 0xc4211
    push SS                                   ; 16                          ; 0xc4213 vbe.c:695
    pop ES                                    ; 07                          ; 0xc4214
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc4215
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc4218 vbe.c:696
    jne short 04222h                          ; 75 05                       ; 0xc421b
    shr bx, 003h                              ; c1 eb 03                    ; 0xc421d vbe.c:697
    jmp short 04231h                          ; eb 0f                       ; 0xc4220 vbe.c:698
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc4222 vbe.c:699
    cwd                                       ; 99                          ; 0xc4225
    sal dx, 003h                              ; c1 e2 03                    ; 0xc4226
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4229
    sar ax, 003h                              ; c1 f8 03                    ; 0xc422b
    imul bx, ax                               ; 0f af d8                    ; 0xc422e
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc4231 vbe.c:700
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc4234
    push SS                                   ; 16                          ; 0xc4237 vbe.c:701
    pop ES                                    ; 07                          ; 0xc4238
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc4239
    call 03c90h                               ; e8 51 fa                    ; 0xc423c vbe.c:702
    push SS                                   ; 16                          ; 0xc423f
    pop ES                                    ; 07                          ; 0xc4240
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc4241
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc4244
    call 03c07h                               ; e8 bd f9                    ; 0xc4247 vbe.c:703
    push SS                                   ; 16                          ; 0xc424a
    pop ES                                    ; 07                          ; 0xc424b
    cmp ax, word [es:bx]                      ; 26 3b 07                    ; 0xc424c
    jbe short 04263h                          ; 76 12                       ; 0xc424f
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc4251 vbe.c:704
    call 03c58h                               ; e8 01 fa                    ; 0xc4254
    mov word [bp-006h], 00200h                ; c7 46 fa 00 02              ; 0xc4257 vbe.c:705
    jmp short 04263h                          ; eb 05                       ; 0xc425c vbe.c:707
    mov word [bp-006h], 00100h                ; c7 46 fa 00 01              ; 0xc425e vbe.c:710
    push SS                                   ; 16                          ; 0xc4263 vbe.c:713
    pop ES                                    ; 07                          ; 0xc4264
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc4265
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc4268
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc426b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc426e vbe.c:714
    pop di                                    ; 5f                          ; 0xc4271
    pop si                                    ; 5e                          ; 0xc4272
    pop bp                                    ; 5d                          ; 0xc4273
    retn                                      ; c3                          ; 0xc4274
  ; disGetNextSymbol 0xc4275 LB 0xe5 -> off=0x0 cb=00000000000000e5 uValue=00000000000c4275 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc4275 LB 0xe5
    push bp                                   ; 55                          ; 0xc4275 vbe.c:740
    mov bp, sp                                ; 89 e5                       ; 0xc4276
    push si                                   ; 56                          ; 0xc4278
    push di                                   ; 57                          ; 0xc4279
    push ax                                   ; 50                          ; 0xc427a
    push ax                                   ; 50                          ; 0xc427b
    push ax                                   ; 50                          ; 0xc427c
    mov si, dx                                ; 89 d6                       ; 0xc427d
    mov dx, cx                                ; 89 ca                       ; 0xc427f
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc4281 vbe.c:753
    push SS                                   ; 16                          ; 0xc4284 vbe.c:754
    pop ES                                    ; 07                          ; 0xc4285
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc4286
    test al, al                               ; 84 c0                       ; 0xc4289 vbe.c:755
    jne short 042afh                          ; 75 22                       ; 0xc428b
    push SS                                   ; 16                          ; 0xc428d vbe.c:757
    pop ES                                    ; 07                          ; 0xc428e
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc428f
    mov bx, dx                                ; 89 d3                       ; 0xc4292 vbe.c:758
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc4294
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc4297 vbe.c:759
    shr ax, 008h                              ; c1 e8 08                    ; 0xc429a
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc429d
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc42a0
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc42a3 vbe.c:764
    je short 042b5h                           ; 74 0e                       ; 0xc42a5
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc42a7
    je short 042b5h                           ; 74 0a                       ; 0xc42a9
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc42ab
    je short 042b5h                           ; 74 06                       ; 0xc42ad
    mov di, 00100h                            ; bf 00 01                    ; 0xc42af vbe.c:765
    jmp near 0434bh                           ; e9 96 00                    ; 0xc42b2 vbe.c:766
    push SS                                   ; 16                          ; 0xc42b5 vbe.c:770
    pop ES                                    ; 07                          ; 0xc42b6
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc42b7
    je short 042c3h                           ; 74 05                       ; 0xc42bc
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc42be
    jmp short 042c5h                          ; eb 02                       ; 0xc42c1
    xor ax, ax                                ; 31 c0                       ; 0xc42c3
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc42c5
    cmp cx, 00280h                            ; 81 f9 80 02                 ; 0xc42c8 vbe.c:773
    jnc short 042d3h                          ; 73 05                       ; 0xc42cc
    mov cx, 00280h                            ; b9 80 02                    ; 0xc42ce vbe.c:774
    jmp short 042dch                          ; eb 09                       ; 0xc42d1 vbe.c:775
    cmp cx, 00a00h                            ; 81 f9 00 0a                 ; 0xc42d3
    jbe short 042dch                          ; 76 03                       ; 0xc42d7
    mov cx, 00a00h                            ; b9 00 0a                    ; 0xc42d9 vbe.c:776
    cmp bx, 001e0h                            ; 81 fb e0 01                 ; 0xc42dc vbe.c:777
    jnc short 042e7h                          ; 73 05                       ; 0xc42e0
    mov bx, 001e0h                            ; bb e0 01                    ; 0xc42e2 vbe.c:778
    jmp short 042f0h                          ; eb 09                       ; 0xc42e5 vbe.c:779
    cmp bx, 00780h                            ; 81 fb 80 07                 ; 0xc42e7
    jbe short 042f0h                          ; 76 03                       ; 0xc42eb
    mov bx, 00780h                            ; bb 80 07                    ; 0xc42ed vbe.c:780
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc42f0 vbe.c:786
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc42f3
    call 03ca9h                               ; e8 b0 f9                    ; 0xc42f6
    mov si, ax                                ; 89 c6                       ; 0xc42f9
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc42fb vbe.c:789
    cwd                                       ; 99                          ; 0xc42ff
    sal dx, 003h                              ; c1 e2 03                    ; 0xc4300
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc4303
    sar ax, 003h                              ; c1 f8 03                    ; 0xc4305
    imul ax, cx                               ; 0f af c1                    ; 0xc4308
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc430b vbe.c:790
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc430e
    mov dx, bx                                ; 89 da                       ; 0xc4310 vbe.c:792
    mul dx                                    ; f7 e2                       ; 0xc4312
    cmp dx, si                                ; 39 f2                       ; 0xc4314 vbe.c:794
    jnbe short 0431eh                         ; 77 06                       ; 0xc4316
    jne short 04323h                          ; 75 09                       ; 0xc4318
    test ax, ax                               ; 85 c0                       ; 0xc431a
    jbe short 04323h                          ; 76 05                       ; 0xc431c
    mov di, 00200h                            ; bf 00 02                    ; 0xc431e vbe.c:796
    jmp short 0434bh                          ; eb 28                       ; 0xc4321 vbe.c:797
    xor ax, ax                                ; 31 c0                       ; 0xc4323 vbe.c:801
    call 005ddh                               ; e8 b5 c2                    ; 0xc4325
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc4328 vbe.c:802
    call 03c20h                               ; e8 f1 f8                    ; 0xc432c
    mov ax, cx                                ; 89 c8                       ; 0xc432f vbe.c:803
    call 03bc9h                               ; e8 95 f8                    ; 0xc4331
    mov ax, bx                                ; 89 d8                       ; 0xc4334 vbe.c:804
    call 03be8h                               ; e8 af f8                    ; 0xc4336
    xor ax, ax                                ; 31 c0                       ; 0xc4339 vbe.c:805
    call 00603h                               ; e8 c5 c2                    ; 0xc433b
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc433e vbe.c:806
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc4341
    xor ah, ah                                ; 30 e4                       ; 0xc4343
    call 005ddh                               ; e8 95 c2                    ; 0xc4345
    call 006d2h                               ; e8 87 c3                    ; 0xc4348 vbe.c:807
    push SS                                   ; 16                          ; 0xc434b vbe.c:815
    pop ES                                    ; 07                          ; 0xc434c
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc434d
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc4350
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc4353 vbe.c:816
    pop di                                    ; 5f                          ; 0xc4356
    pop si                                    ; 5e                          ; 0xc4357
    pop bp                                    ; 5d                          ; 0xc4358
    retn                                      ; c3                          ; 0xc4359

  ; Padding 0x2e6 bytes at 0xc435a
  times 742 db 0

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

section _DATA progbits vstart=0x4780 align=1 ; size=0x3758 class=DATA group=DGROUP
  ; disGetNextSymbol 0xc4780 LB 0x3758 -> off=0x0 cb=0000000000000034 uValue=00000000000c0000 '_msg_vga_init'
_msg_vga_init:                               ; 0xc4780 LB 0x34
    db  'Oracle VM VirtualBox Version 7.0.0_BETA2 VGA BIOS', 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc47b4 LB 0x3724 -> off=0x0 cb=0000000000000080 uValue=00000000000c0034 'vga_modes'
vga_modes:                                   ; 0xc47b4 LB 0x80
    db  000h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 001h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  002h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 003h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  004h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h, 005h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h
    db  006h, 001h, 002h, 001h, 000h, 0b8h, 0ffh, 001h, 007h, 000h, 001h, 004h, 000h, 0b0h, 0ffh, 000h
    db  00dh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h, 00eh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h
    db  00fh, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 000h, 010h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  011h, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 002h, 012h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  013h, 001h, 005h, 008h, 000h, 0a0h, 0ffh, 003h, 06ah, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
  ; disGetNextSymbol 0xc4834 LB 0x36a4 -> off=0x0 cb=0000000000000010 uValue=00000000000c00b4 'line_to_vpti'
line_to_vpti:                                ; 0xc4834 LB 0x10
    db  017h, 017h, 018h, 018h, 004h, 005h, 006h, 007h, 00dh, 00eh, 011h, 012h, 01ah, 01bh, 01ch, 01dh
  ; disGetNextSymbol 0xc4844 LB 0x3694 -> off=0x0 cb=0000000000000004 uValue=00000000000c00c4 'dac_regs'
dac_regs:                                    ; 0xc4844 LB 0x4
    dd  0ff3f3f3fh
  ; disGetNextSymbol 0xc4848 LB 0x3690 -> off=0x0 cb=0000000000000780 uValue=00000000000c00c8 'video_param_table'
video_param_table:                           ; 0xc4848 LB 0x780
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
  ; disGetNextSymbol 0xc4fc8 LB 0x2f10 -> off=0x0 cb=00000000000000c0 uValue=00000000000c0848 'palette0'
palette0:                                    ; 0xc4fc8 LB 0xc0
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
  ; disGetNextSymbol 0xc5088 LB 0x2e50 -> off=0x0 cb=00000000000000c0 uValue=00000000000c0908 'palette1'
palette1:                                    ; 0xc5088 LB 0xc0
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
  ; disGetNextSymbol 0xc5148 LB 0x2d90 -> off=0x0 cb=00000000000000c0 uValue=00000000000c09c8 'palette2'
palette2:                                    ; 0xc5148 LB 0xc0
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
  ; disGetNextSymbol 0xc5208 LB 0x2cd0 -> off=0x0 cb=0000000000000300 uValue=00000000000c0a88 'palette3'
palette3:                                    ; 0xc5208 LB 0x300
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
  ; disGetNextSymbol 0xc5508 LB 0x29d0 -> off=0x0 cb=0000000000000010 uValue=00000000000c0d88 'static_functionality'
static_functionality:                        ; 0xc5508 LB 0x10
    db  0ffh, 0e0h, 00fh, 000h, 000h, 000h, 000h, 007h, 002h, 008h, 0e7h, 00ch, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5518 LB 0x29c0 -> off=0x0 cb=0000000000000024 uValue=00000000000c0d98 '_dcc_table'
_dcc_table:                                  ; 0xc5518 LB 0x24
    db  010h, 001h, 007h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc553c LB 0x299c -> off=0x0 cb=000000000000001a uValue=00000000000c0dbc '_secondary_save_area'
_secondary_save_area:                        ; 0xc553c LB 0x1a
    db  01ah, 000h, 018h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5556 LB 0x2982 -> off=0x0 cb=000000000000001c uValue=00000000000c0dd6 '_video_save_pointer_table'
_video_save_pointer_table:                   ; 0xc5556 LB 0x1c
    db  048h, 048h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  03ch, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5572 LB 0x2966 -> off=0x0 cb=0000000000000800 uValue=00000000000c0df2 'vgafont8'
vgafont8:                                    ; 0xc5572 LB 0x800
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
  ; disGetNextSymbol 0xc5d72 LB 0x2166 -> off=0x0 cb=0000000000000e00 uValue=00000000000c15f2 'vgafont14'
vgafont14:                                   ; 0xc5d72 LB 0xe00
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
  ; disGetNextSymbol 0xc6b72 LB 0x1366 -> off=0x0 cb=0000000000001000 uValue=00000000000c23f2 'vgafont16'
vgafont16:                                   ; 0xc6b72 LB 0x1000
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
  ; disGetNextSymbol 0xc7b72 LB 0x366 -> off=0x0 cb=000000000000012d uValue=00000000000c33f2 'vgafont14alt'
vgafont14alt:                                ; 0xc7b72 LB 0x12d
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
  ; disGetNextSymbol 0xc7c9f LB 0x239 -> off=0x0 cb=0000000000000144 uValue=00000000000c351f 'vgafont16alt'
vgafont16alt:                                ; 0xc7c9f LB 0x144
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
  ; disGetNextSymbol 0xc7de3 LB 0xf5 -> off=0x0 cb=0000000000000008 uValue=00000000000c3663 '_cga_msr'
_cga_msr:                                    ; 0xc7de3 LB 0x8
    db  02ch, 028h, 02dh, 029h, 02ah, 02eh, 01eh, 029h
  ; disGetNextSymbol 0xc7deb LB 0xed -> off=0x0 cb=0000000000000008 uValue=00000000000c366b 'line_to_vpti_200'
line_to_vpti_200:                            ; 0xc7deb LB 0x8
    db  000h, 001h, 002h, 003h, 0ffh, 0ffh, 0ffh, 007h
  ; disGetNextSymbol 0xc7df3 LB 0xe5 -> off=0x0 cb=0000000000000008 uValue=00000000000c3673 'line_to_vpti_350'
line_to_vpti_350:                            ; 0xc7df3 LB 0x8
    db  013h, 014h, 015h, 016h, 0ffh, 0ffh, 0ffh, 007h
  ; disGetNextSymbol 0xc7dfb LB 0xdd -> off=0x0 cb=0000000000000008 uValue=00000000000c367b 'line_to_vpti_400'
line_to_vpti_400:                            ; 0xc7dfb LB 0x8
    db  017h, 017h, 018h, 018h, 0ffh, 0ffh, 0ffh, 019h
  ; disGetNextSymbol 0xc7e03 LB 0xd5 -> off=0x0 cb=0000000000000005 uValue=00000000000c3683 'row_tbl'
row_tbl:                                     ; 0xc7e03 LB 0x5
    db  000h, 00eh, 019h, 02bh, 000h
  ; disGetNextSymbol 0xc7e08 LB 0xd0 -> off=0x0 cb=0000000000000015 uValue=00000000000c3688 '_vbebios_copyright'
_vbebios_copyright:                          ; 0xc7e08 LB 0x15
    db  'VirtualBox VESA BIOS', 000h
  ; disGetNextSymbol 0xc7e1d LB 0xbb -> off=0x0 cb=000000000000001d uValue=00000000000c369d '_vbebios_vendor_name'
_vbebios_vendor_name:                        ; 0xc7e1d LB 0x1d
    db  'Oracle and/or its affiliates', 000h
  ; disGetNextSymbol 0xc7e3a LB 0x9e -> off=0x0 cb=0000000000000021 uValue=00000000000c36ba '_vbebios_product_name'
_vbebios_product_name:                       ; 0xc7e3a LB 0x21
    db  'Oracle VM VirtualBox VBE Adapter', 000h
  ; disGetNextSymbol 0xc7e5b LB 0x7d -> off=0x0 cb=0000000000000029 uValue=00000000000c36db '_vbebios_product_revision'
_vbebios_product_revision:                   ; 0xc7e5b LB 0x29
    db  'Oracle VM VirtualBox Version 7.0.0_BETA2', 000h
  ; disGetNextSymbol 0xc7e84 LB 0x54 -> off=0x0 cb=000000000000002b uValue=00000000000c3704 '_vbebios_info_string'
_vbebios_info_string:                        ; 0xc7e84 LB 0x2b
    db  'VirtualBox VBE Display Adapter enabled', 00dh, 00ah, 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc7eaf LB 0x29 -> off=0x0 cb=0000000000000029 uValue=00000000000c372f '_no_vbebios_info_string'
_no_vbebios_info_string:                     ; 0xc7eaf LB 0x29
    db  'No VirtualBox VBE support available!', 00dh, 00ah, 00dh, 00ah, 000h

section CONST progbits vstart=0x7ed8 align=1 ; size=0x0 class=DATA group=DGROUP

section CONST2 progbits vstart=0x7ed8 align=1 ; size=0x0 class=DATA group=DGROUP

  ; Padding 0x128 bytes at 0xc7ed8
    db  001h, 000h, 000h, 000h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 02fh, 068h, 06fh, 06dh
    db  065h, 02fh, 075h, 062h, 075h, 06eh, 074h, 075h, 02fh, 074h, 072h, 075h, 06eh, 06bh, 02fh, 06fh
    db  075h, 074h, 02fh, 06ch, 069h, 06eh, 075h, 078h, 02eh, 061h, 06dh, 064h, 036h, 034h, 02fh, 072h
    db  065h, 06ch, 065h, 061h, 073h, 065h, 02fh, 06fh, 062h, 06ah, 02fh, 056h, 042h, 06fh, 078h, 056h
    db  067h, 061h, 042h, 069h, 06fh, 073h, 033h, 038h, 036h, 02fh, 056h, 042h, 06fh, 078h, 056h, 067h
    db  061h, 042h, 069h, 06fh, 073h, 033h, 038h, 036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 0fdh
