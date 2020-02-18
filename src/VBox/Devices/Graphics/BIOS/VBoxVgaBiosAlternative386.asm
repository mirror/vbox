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





section VGAROM progbits vstart=0x0 align=1 ; size=0x8f7 class=CODE group=AUTO
  ; disGetNextSymbol 0xc0000 LB 0x8f7 -> off=0x22 cb=000000000000053e uValue=00000000000c0022 'vgabios_int10_handler'
    db  055h, 0aah, 040h, 0e9h, 0e2h, 009h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 049h, 042h
    db  04dh, 000h
vgabios_int10_handler:                       ; 0xc0022 LB 0x53e
    pushfw                                    ; 9c                          ; 0xc0022 vgarom.asm:84
    cmp ah, 00fh                              ; 80 fc 0f                    ; 0xc0023 vgarom.asm:97
    jne short 0002eh                          ; 75 06                       ; 0xc0026 vgarom.asm:98
    call 00177h                               ; e8 4c 01                    ; 0xc0028 vgarom.asm:99
    jmp near 000e7h                           ; e9 b9 00                    ; 0xc002b vgarom.asm:100
    cmp ah, 01ah                              ; 80 fc 1a                    ; 0xc002e vgarom.asm:102
    jne short 00039h                          ; 75 06                       ; 0xc0031 vgarom.asm:103
    call 0052ch                               ; e8 f6 04                    ; 0xc0033 vgarom.asm:104
    jmp near 000e7h                           ; e9 ae 00                    ; 0xc0036 vgarom.asm:105
    cmp ah, 00bh                              ; 80 fc 0b                    ; 0xc0039 vgarom.asm:107
    jne short 00044h                          ; 75 06                       ; 0xc003c vgarom.asm:108
    call 000e9h                               ; e8 a8 00                    ; 0xc003e vgarom.asm:109
    jmp near 000e7h                           ; e9 a3 00                    ; 0xc0041 vgarom.asm:110
    cmp ax, 01103h                            ; 3d 03 11                    ; 0xc0044 vgarom.asm:112
    jne short 0004fh                          ; 75 06                       ; 0xc0047 vgarom.asm:113
    call 00423h                               ; e8 d7 03                    ; 0xc0049 vgarom.asm:114
    jmp near 000e7h                           ; e9 98 00                    ; 0xc004c vgarom.asm:115
    cmp ah, 012h                              ; 80 fc 12                    ; 0xc004f vgarom.asm:117
    jne short 00091h                          ; 75 3d                       ; 0xc0052 vgarom.asm:118
    cmp bl, 010h                              ; 80 fb 10                    ; 0xc0054 vgarom.asm:119
    jne short 0005fh                          ; 75 06                       ; 0xc0057 vgarom.asm:120
    call 00430h                               ; e8 d4 03                    ; 0xc0059 vgarom.asm:121
    jmp near 000e7h                           ; e9 88 00                    ; 0xc005c vgarom.asm:122
    cmp bl, 030h                              ; 80 fb 30                    ; 0xc005f vgarom.asm:124
    jne short 00069h                          ; 75 05                       ; 0xc0062 vgarom.asm:125
    call 00453h                               ; e8 ec 03                    ; 0xc0064 vgarom.asm:126
    jmp short 000e7h                          ; eb 7e                       ; 0xc0067 vgarom.asm:127
    cmp bl, 031h                              ; 80 fb 31                    ; 0xc0069 vgarom.asm:129
    jne short 00073h                          ; 75 05                       ; 0xc006c vgarom.asm:130
    call 004a6h                               ; e8 35 04                    ; 0xc006e vgarom.asm:131
    jmp short 000e7h                          ; eb 74                       ; 0xc0071 vgarom.asm:132
    cmp bl, 032h                              ; 80 fb 32                    ; 0xc0073 vgarom.asm:134
    jne short 0007dh                          ; 75 05                       ; 0xc0076 vgarom.asm:135
    call 004c8h                               ; e8 4d 04                    ; 0xc0078 vgarom.asm:136
    jmp short 000e7h                          ; eb 6a                       ; 0xc007b vgarom.asm:137
    cmp bl, 033h                              ; 80 fb 33                    ; 0xc007d vgarom.asm:139
    jne short 00087h                          ; 75 05                       ; 0xc0080 vgarom.asm:140
    call 004e6h                               ; e8 61 04                    ; 0xc0082 vgarom.asm:141
    jmp short 000e7h                          ; eb 60                       ; 0xc0085 vgarom.asm:142
    cmp bl, 034h                              ; 80 fb 34                    ; 0xc0087 vgarom.asm:144
    jne short 000dbh                          ; 75 4f                       ; 0xc008a vgarom.asm:145
    call 0050ah                               ; e8 7b 04                    ; 0xc008c vgarom.asm:146
    jmp short 000e7h                          ; eb 56                       ; 0xc008f vgarom.asm:147
    cmp ax, 0101bh                            ; 3d 1b 10                    ; 0xc0091 vgarom.asm:149
    je short 000dbh                           ; 74 45                       ; 0xc0094 vgarom.asm:150
    cmp ah, 010h                              ; 80 fc 10                    ; 0xc0096 vgarom.asm:151
    jne short 000a0h                          ; 75 05                       ; 0xc0099 vgarom.asm:155
    call 0019eh                               ; e8 00 01                    ; 0xc009b vgarom.asm:157
    jmp short 000e7h                          ; eb 47                       ; 0xc009e vgarom.asm:158
    cmp ah, 04fh                              ; 80 fc 4f                    ; 0xc00a0 vgarom.asm:161
    jne short 000dbh                          ; 75 36                       ; 0xc00a3 vgarom.asm:162
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc00a5 vgarom.asm:163
    jne short 000aeh                          ; 75 05                       ; 0xc00a7 vgarom.asm:164
    call 007c2h                               ; e8 16 07                    ; 0xc00a9 vgarom.asm:165
    jmp short 000e7h                          ; eb 39                       ; 0xc00ac vgarom.asm:166
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc00ae vgarom.asm:168
    jne short 000b7h                          ; 75 05                       ; 0xc00b0 vgarom.asm:169
    call 007e7h                               ; e8 32 07                    ; 0xc00b2 vgarom.asm:170
    jmp short 000e7h                          ; eb 30                       ; 0xc00b5 vgarom.asm:171
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc00b7 vgarom.asm:173
    jne short 000c0h                          ; 75 05                       ; 0xc00b9 vgarom.asm:174
    call 00814h                               ; e8 56 07                    ; 0xc00bb vgarom.asm:175
    jmp short 000e7h                          ; eb 27                       ; 0xc00be vgarom.asm:176
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc00c0 vgarom.asm:178
    jne short 000c9h                          ; 75 05                       ; 0xc00c2 vgarom.asm:179
    call 00848h                               ; e8 81 07                    ; 0xc00c4 vgarom.asm:180
    jmp short 000e7h                          ; eb 1e                       ; 0xc00c7 vgarom.asm:181
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc00c9 vgarom.asm:183
    jne short 000d2h                          ; 75 05                       ; 0xc00cb vgarom.asm:184
    call 0087fh                               ; e8 af 07                    ; 0xc00cd vgarom.asm:185
    jmp short 000e7h                          ; eb 15                       ; 0xc00d0 vgarom.asm:186
    cmp AL, strict byte 00ah                  ; 3c 0a                       ; 0xc00d2 vgarom.asm:188
    jne short 000dbh                          ; 75 05                       ; 0xc00d4 vgarom.asm:189
    call 008e3h                               ; e8 0a 08                    ; 0xc00d6 vgarom.asm:190
    jmp short 000e7h                          ; eb 0c                       ; 0xc00d9 vgarom.asm:191
    push ES                                   ; 06                          ; 0xc00db vgarom.asm:195
    push DS                                   ; 1e                          ; 0xc00dc vgarom.asm:196
    pushaw                                    ; 60                          ; 0xc00dd vgarom.asm:97
    push CS                                   ; 0e                          ; 0xc00de vgarom.asm:200
    pop DS                                    ; 1f                          ; 0xc00df vgarom.asm:201
    cld                                       ; fc                          ; 0xc00e0 vgarom.asm:202
    call 0345dh                               ; e8 79 33                    ; 0xc00e1 vgarom.asm:203
    popaw                                     ; 61                          ; 0xc00e4 vgarom.asm:114
    pop DS                                    ; 1f                          ; 0xc00e5 vgarom.asm:206
    pop ES                                    ; 07                          ; 0xc00e6 vgarom.asm:207
    popfw                                     ; 9d                          ; 0xc00e7 vgarom.asm:209
    iret                                      ; cf                          ; 0xc00e8 vgarom.asm:210
    cmp bh, 000h                              ; 80 ff 00                    ; 0xc00e9 vgarom.asm:215
    je short 000f4h                           ; 74 06                       ; 0xc00ec vgarom.asm:216
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc00ee vgarom.asm:217
    je short 00145h                           ; 74 52                       ; 0xc00f1 vgarom.asm:218
    retn                                      ; c3                          ; 0xc00f3 vgarom.asm:222
    push ax                                   ; 50                          ; 0xc00f4 vgarom.asm:224
    push bx                                   ; 53                          ; 0xc00f5 vgarom.asm:225
    push cx                                   ; 51                          ; 0xc00f6 vgarom.asm:226
    push dx                                   ; 52                          ; 0xc00f7 vgarom.asm:227
    push DS                                   ; 1e                          ; 0xc00f8 vgarom.asm:228
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc00f9 vgarom.asm:229
    mov ds, dx                                ; 8e da                       ; 0xc00fc vgarom.asm:230
    mov dx, 003dah                            ; ba da 03                    ; 0xc00fe vgarom.asm:231
    in AL, DX                                 ; ec                          ; 0xc0101 vgarom.asm:232
    cmp byte [word 00049h], 003h              ; 80 3e 49 00 03              ; 0xc0102 vgarom.asm:233
    jbe short 00138h                          ; 76 2f                       ; 0xc0107 vgarom.asm:234
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0109 vgarom.asm:235
    mov AL, strict byte 000h                  ; b0 00                       ; 0xc010c vgarom.asm:236
    out DX, AL                                ; ee                          ; 0xc010e vgarom.asm:237
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc010f vgarom.asm:238
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc0111 vgarom.asm:239
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0113 vgarom.asm:240
    je short 00119h                           ; 74 02                       ; 0xc0115 vgarom.asm:241
    add AL, strict byte 008h                  ; 04 08                       ; 0xc0117 vgarom.asm:242
    out DX, AL                                ; ee                          ; 0xc0119 vgarom.asm:244
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc011a vgarom.asm:245
    and bl, 010h                              ; 80 e3 10                    ; 0xc011c vgarom.asm:246
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc011f vgarom.asm:248
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0122 vgarom.asm:249
    out DX, AL                                ; ee                          ; 0xc0124 vgarom.asm:250
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0125 vgarom.asm:251
    in AL, DX                                 ; ec                          ; 0xc0128 vgarom.asm:252
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc0129 vgarom.asm:253
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc012b vgarom.asm:254
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc012d vgarom.asm:255
    out DX, AL                                ; ee                          ; 0xc0130 vgarom.asm:256
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0131 vgarom.asm:257
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0133 vgarom.asm:258
    jne short 0011fh                          ; 75 e7                       ; 0xc0136 vgarom.asm:259
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0138 vgarom.asm:261
    out DX, AL                                ; ee                          ; 0xc013a vgarom.asm:262
    mov dx, 003dah                            ; ba da 03                    ; 0xc013b vgarom.asm:264
    in AL, DX                                 ; ec                          ; 0xc013e vgarom.asm:265
    pop DS                                    ; 1f                          ; 0xc013f vgarom.asm:267
    pop dx                                    ; 5a                          ; 0xc0140 vgarom.asm:268
    pop cx                                    ; 59                          ; 0xc0141 vgarom.asm:269
    pop bx                                    ; 5b                          ; 0xc0142 vgarom.asm:270
    pop ax                                    ; 58                          ; 0xc0143 vgarom.asm:271
    retn                                      ; c3                          ; 0xc0144 vgarom.asm:272
    push ax                                   ; 50                          ; 0xc0145 vgarom.asm:274
    push bx                                   ; 53                          ; 0xc0146 vgarom.asm:275
    push cx                                   ; 51                          ; 0xc0147 vgarom.asm:276
    push dx                                   ; 52                          ; 0xc0148 vgarom.asm:277
    mov dx, 003dah                            ; ba da 03                    ; 0xc0149 vgarom.asm:278
    in AL, DX                                 ; ec                          ; 0xc014c vgarom.asm:279
    mov CL, strict byte 001h                  ; b1 01                       ; 0xc014d vgarom.asm:280
    and bl, 001h                              ; 80 e3 01                    ; 0xc014f vgarom.asm:281
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0152 vgarom.asm:283
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0155 vgarom.asm:284
    out DX, AL                                ; ee                          ; 0xc0157 vgarom.asm:285
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0158 vgarom.asm:286
    in AL, DX                                 ; ec                          ; 0xc015b vgarom.asm:287
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc015c vgarom.asm:288
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc015e vgarom.asm:289
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0160 vgarom.asm:290
    out DX, AL                                ; ee                          ; 0xc0163 vgarom.asm:291
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0164 vgarom.asm:292
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc0166 vgarom.asm:293
    jne short 00152h                          ; 75 e7                       ; 0xc0169 vgarom.asm:294
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc016b vgarom.asm:295
    out DX, AL                                ; ee                          ; 0xc016d vgarom.asm:296
    mov dx, 003dah                            ; ba da 03                    ; 0xc016e vgarom.asm:298
    in AL, DX                                 ; ec                          ; 0xc0171 vgarom.asm:299
    pop dx                                    ; 5a                          ; 0xc0172 vgarom.asm:301
    pop cx                                    ; 59                          ; 0xc0173 vgarom.asm:302
    pop bx                                    ; 5b                          ; 0xc0174 vgarom.asm:303
    pop ax                                    ; 58                          ; 0xc0175 vgarom.asm:304
    retn                                      ; c3                          ; 0xc0176 vgarom.asm:305
    push DS                                   ; 1e                          ; 0xc0177 vgarom.asm:310
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0178 vgarom.asm:311
    mov ds, ax                                ; 8e d8                       ; 0xc017b vgarom.asm:312
    push bx                                   ; 53                          ; 0xc017d vgarom.asm:313
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc017e vgarom.asm:314
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0181 vgarom.asm:315
    pop bx                                    ; 5b                          ; 0xc0183 vgarom.asm:316
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0184 vgarom.asm:317
    push bx                                   ; 53                          ; 0xc0186 vgarom.asm:318
    mov bx, 00087h                            ; bb 87 00                    ; 0xc0187 vgarom.asm:319
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc018a vgarom.asm:320
    and ah, 080h                              ; 80 e4 80                    ; 0xc018c vgarom.asm:321
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc018f vgarom.asm:322
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0192 vgarom.asm:323
    db  00ah, 0c4h
    ; or al, ah                                 ; 0a c4                     ; 0xc0194 vgarom.asm:324
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0196 vgarom.asm:325
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc0199 vgarom.asm:326
    pop bx                                    ; 5b                          ; 0xc019b vgarom.asm:327
    pop DS                                    ; 1f                          ; 0xc019c vgarom.asm:328
    retn                                      ; c3                          ; 0xc019d vgarom.asm:329
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc019e vgarom.asm:334
    jne short 001a4h                          ; 75 02                       ; 0xc01a0 vgarom.asm:335
    jmp short 00205h                          ; eb 61                       ; 0xc01a2 vgarom.asm:336
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc01a4 vgarom.asm:338
    jne short 001aah                          ; 75 02                       ; 0xc01a6 vgarom.asm:339
    jmp short 00223h                          ; eb 79                       ; 0xc01a8 vgarom.asm:340
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc01aa vgarom.asm:342
    jne short 001b0h                          ; 75 02                       ; 0xc01ac vgarom.asm:343
    jmp short 0022bh                          ; eb 7b                       ; 0xc01ae vgarom.asm:344
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc01b0 vgarom.asm:346
    jne short 001b7h                          ; 75 03                       ; 0xc01b2 vgarom.asm:347
    jmp near 0025ch                           ; e9 a5 00                    ; 0xc01b4 vgarom.asm:348
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc01b7 vgarom.asm:350
    jne short 001beh                          ; 75 03                       ; 0xc01b9 vgarom.asm:351
    jmp near 00286h                           ; e9 c8 00                    ; 0xc01bb vgarom.asm:352
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc01be vgarom.asm:354
    jne short 001c5h                          ; 75 03                       ; 0xc01c0 vgarom.asm:355
    jmp near 002aeh                           ; e9 e9 00                    ; 0xc01c2 vgarom.asm:356
    cmp AL, strict byte 009h                  ; 3c 09                       ; 0xc01c5 vgarom.asm:358
    jne short 001cch                          ; 75 03                       ; 0xc01c7 vgarom.asm:359
    jmp near 002bch                           ; e9 f0 00                    ; 0xc01c9 vgarom.asm:360
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc01cc vgarom.asm:362
    jne short 001d3h                          ; 75 03                       ; 0xc01ce vgarom.asm:363
    jmp near 00301h                           ; e9 2e 01                    ; 0xc01d0 vgarom.asm:364
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc01d3 vgarom.asm:366
    jne short 001dah                          ; 75 03                       ; 0xc01d5 vgarom.asm:367
    jmp near 0031ah                           ; e9 40 01                    ; 0xc01d7 vgarom.asm:368
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc01da vgarom.asm:370
    jne short 001e1h                          ; 75 03                       ; 0xc01dc vgarom.asm:371
    jmp near 00342h                           ; e9 61 01                    ; 0xc01de vgarom.asm:372
    cmp AL, strict byte 015h                  ; 3c 15                       ; 0xc01e1 vgarom.asm:374
    jne short 001e8h                          ; 75 03                       ; 0xc01e3 vgarom.asm:375
    jmp near 00389h                           ; e9 a1 01                    ; 0xc01e5 vgarom.asm:376
    cmp AL, strict byte 017h                  ; 3c 17                       ; 0xc01e8 vgarom.asm:378
    jne short 001efh                          ; 75 03                       ; 0xc01ea vgarom.asm:379
    jmp near 003a4h                           ; e9 b5 01                    ; 0xc01ec vgarom.asm:380
    cmp AL, strict byte 018h                  ; 3c 18                       ; 0xc01ef vgarom.asm:382
    jne short 001f6h                          ; 75 03                       ; 0xc01f1 vgarom.asm:383
    jmp near 003cch                           ; e9 d6 01                    ; 0xc01f3 vgarom.asm:384
    cmp AL, strict byte 019h                  ; 3c 19                       ; 0xc01f6 vgarom.asm:386
    jne short 001fdh                          ; 75 03                       ; 0xc01f8 vgarom.asm:387
    jmp near 003d7h                           ; e9 da 01                    ; 0xc01fa vgarom.asm:388
    cmp AL, strict byte 01ah                  ; 3c 1a                       ; 0xc01fd vgarom.asm:390
    jne short 00204h                          ; 75 03                       ; 0xc01ff vgarom.asm:391
    jmp near 003e2h                           ; e9 de 01                    ; 0xc0201 vgarom.asm:392
    retn                                      ; c3                          ; 0xc0204 vgarom.asm:397
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0205 vgarom.asm:400
    jnbe short 00222h                         ; 77 18                       ; 0xc0208 vgarom.asm:401
    push ax                                   ; 50                          ; 0xc020a vgarom.asm:402
    push dx                                   ; 52                          ; 0xc020b vgarom.asm:403
    mov dx, 003dah                            ; ba da 03                    ; 0xc020c vgarom.asm:404
    in AL, DX                                 ; ec                          ; 0xc020f vgarom.asm:405
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0210 vgarom.asm:406
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0213 vgarom.asm:407
    out DX, AL                                ; ee                          ; 0xc0215 vgarom.asm:408
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc0216 vgarom.asm:409
    out DX, AL                                ; ee                          ; 0xc0218 vgarom.asm:410
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0219 vgarom.asm:411
    out DX, AL                                ; ee                          ; 0xc021b vgarom.asm:412
    mov dx, 003dah                            ; ba da 03                    ; 0xc021c vgarom.asm:414
    in AL, DX                                 ; ec                          ; 0xc021f vgarom.asm:415
    pop dx                                    ; 5a                          ; 0xc0220 vgarom.asm:417
    pop ax                                    ; 58                          ; 0xc0221 vgarom.asm:418
    retn                                      ; c3                          ; 0xc0222 vgarom.asm:420
    push bx                                   ; 53                          ; 0xc0223 vgarom.asm:425
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc0224 vgarom.asm:426
    call 00205h                               ; e8 dc ff                    ; 0xc0226 vgarom.asm:427
    pop bx                                    ; 5b                          ; 0xc0229 vgarom.asm:428
    retn                                      ; c3                          ; 0xc022a vgarom.asm:429
    push ax                                   ; 50                          ; 0xc022b vgarom.asm:434
    push bx                                   ; 53                          ; 0xc022c vgarom.asm:435
    push cx                                   ; 51                          ; 0xc022d vgarom.asm:436
    push dx                                   ; 52                          ; 0xc022e vgarom.asm:437
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc022f vgarom.asm:438
    mov dx, 003dah                            ; ba da 03                    ; 0xc0231 vgarom.asm:439
    in AL, DX                                 ; ec                          ; 0xc0234 vgarom.asm:440
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc0235 vgarom.asm:441
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0237 vgarom.asm:442
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc023a vgarom.asm:444
    out DX, AL                                ; ee                          ; 0xc023c vgarom.asm:445
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc023d vgarom.asm:446
    out DX, AL                                ; ee                          ; 0xc0240 vgarom.asm:447
    inc bx                                    ; 43                          ; 0xc0241 vgarom.asm:448
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc0242 vgarom.asm:449
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc0244 vgarom.asm:450
    jne short 0023ah                          ; 75 f1                       ; 0xc0247 vgarom.asm:451
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc0249 vgarom.asm:452
    out DX, AL                                ; ee                          ; 0xc024b vgarom.asm:453
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc024c vgarom.asm:454
    out DX, AL                                ; ee                          ; 0xc024f vgarom.asm:455
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0250 vgarom.asm:456
    out DX, AL                                ; ee                          ; 0xc0252 vgarom.asm:457
    mov dx, 003dah                            ; ba da 03                    ; 0xc0253 vgarom.asm:459
    in AL, DX                                 ; ec                          ; 0xc0256 vgarom.asm:460
    pop dx                                    ; 5a                          ; 0xc0257 vgarom.asm:462
    pop cx                                    ; 59                          ; 0xc0258 vgarom.asm:463
    pop bx                                    ; 5b                          ; 0xc0259 vgarom.asm:464
    pop ax                                    ; 58                          ; 0xc025a vgarom.asm:465
    retn                                      ; c3                          ; 0xc025b vgarom.asm:466
    push ax                                   ; 50                          ; 0xc025c vgarom.asm:471
    push bx                                   ; 53                          ; 0xc025d vgarom.asm:472
    push dx                                   ; 52                          ; 0xc025e vgarom.asm:473
    mov dx, 003dah                            ; ba da 03                    ; 0xc025f vgarom.asm:474
    in AL, DX                                 ; ec                          ; 0xc0262 vgarom.asm:475
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0263 vgarom.asm:476
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0266 vgarom.asm:477
    out DX, AL                                ; ee                          ; 0xc0268 vgarom.asm:478
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0269 vgarom.asm:479
    in AL, DX                                 ; ec                          ; 0xc026c vgarom.asm:480
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc026d vgarom.asm:481
    and bl, 001h                              ; 80 e3 01                    ; 0xc026f vgarom.asm:482
    sal bl, 003h                              ; c0 e3 03                    ; 0xc0272 vgarom.asm:484
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc0275 vgarom.asm:490
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0277 vgarom.asm:491
    out DX, AL                                ; ee                          ; 0xc027a vgarom.asm:492
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc027b vgarom.asm:493
    out DX, AL                                ; ee                          ; 0xc027d vgarom.asm:494
    mov dx, 003dah                            ; ba da 03                    ; 0xc027e vgarom.asm:496
    in AL, DX                                 ; ec                          ; 0xc0281 vgarom.asm:497
    pop dx                                    ; 5a                          ; 0xc0282 vgarom.asm:499
    pop bx                                    ; 5b                          ; 0xc0283 vgarom.asm:500
    pop ax                                    ; 58                          ; 0xc0284 vgarom.asm:501
    retn                                      ; c3                          ; 0xc0285 vgarom.asm:502
    cmp bl, 014h                              ; 80 fb 14                    ; 0xc0286 vgarom.asm:507
    jnbe short 002adh                         ; 77 22                       ; 0xc0289 vgarom.asm:508
    push ax                                   ; 50                          ; 0xc028b vgarom.asm:509
    push dx                                   ; 52                          ; 0xc028c vgarom.asm:510
    mov dx, 003dah                            ; ba da 03                    ; 0xc028d vgarom.asm:511
    in AL, DX                                 ; ec                          ; 0xc0290 vgarom.asm:512
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0291 vgarom.asm:513
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0294 vgarom.asm:514
    out DX, AL                                ; ee                          ; 0xc0296 vgarom.asm:515
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0297 vgarom.asm:516
    in AL, DX                                 ; ec                          ; 0xc029a vgarom.asm:517
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc029b vgarom.asm:518
    mov dx, 003dah                            ; ba da 03                    ; 0xc029d vgarom.asm:519
    in AL, DX                                 ; ec                          ; 0xc02a0 vgarom.asm:520
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02a1 vgarom.asm:521
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02a4 vgarom.asm:522
    out DX, AL                                ; ee                          ; 0xc02a6 vgarom.asm:523
    mov dx, 003dah                            ; ba da 03                    ; 0xc02a7 vgarom.asm:525
    in AL, DX                                 ; ec                          ; 0xc02aa vgarom.asm:526
    pop dx                                    ; 5a                          ; 0xc02ab vgarom.asm:528
    pop ax                                    ; 58                          ; 0xc02ac vgarom.asm:529
    retn                                      ; c3                          ; 0xc02ad vgarom.asm:531
    push ax                                   ; 50                          ; 0xc02ae vgarom.asm:536
    push bx                                   ; 53                          ; 0xc02af vgarom.asm:537
    mov BL, strict byte 011h                  ; b3 11                       ; 0xc02b0 vgarom.asm:538
    call 00286h                               ; e8 d1 ff                    ; 0xc02b2 vgarom.asm:539
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc02b5 vgarom.asm:540
    pop bx                                    ; 5b                          ; 0xc02b7 vgarom.asm:541
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc02b8 vgarom.asm:542
    pop ax                                    ; 58                          ; 0xc02ba vgarom.asm:543
    retn                                      ; c3                          ; 0xc02bb vgarom.asm:544
    push ax                                   ; 50                          ; 0xc02bc vgarom.asm:549
    push bx                                   ; 53                          ; 0xc02bd vgarom.asm:550
    push cx                                   ; 51                          ; 0xc02be vgarom.asm:551
    push dx                                   ; 52                          ; 0xc02bf vgarom.asm:552
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc02c0 vgarom.asm:553
    mov CL, strict byte 000h                  ; b1 00                       ; 0xc02c2 vgarom.asm:554
    mov dx, 003dah                            ; ba da 03                    ; 0xc02c4 vgarom.asm:556
    in AL, DX                                 ; ec                          ; 0xc02c7 vgarom.asm:557
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02c8 vgarom.asm:558
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc02cb vgarom.asm:559
    out DX, AL                                ; ee                          ; 0xc02cd vgarom.asm:560
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02ce vgarom.asm:561
    in AL, DX                                 ; ec                          ; 0xc02d1 vgarom.asm:562
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02d2 vgarom.asm:563
    inc bx                                    ; 43                          ; 0xc02d5 vgarom.asm:564
    db  0feh, 0c1h
    ; inc cl                                    ; fe c1                     ; 0xc02d6 vgarom.asm:565
    cmp cl, 010h                              ; 80 f9 10                    ; 0xc02d8 vgarom.asm:566
    jne short 002c4h                          ; 75 e7                       ; 0xc02db vgarom.asm:567
    mov dx, 003dah                            ; ba da 03                    ; 0xc02dd vgarom.asm:568
    in AL, DX                                 ; ec                          ; 0xc02e0 vgarom.asm:569
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02e1 vgarom.asm:570
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc02e4 vgarom.asm:571
    out DX, AL                                ; ee                          ; 0xc02e6 vgarom.asm:572
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc02e7 vgarom.asm:573
    in AL, DX                                 ; ec                          ; 0xc02ea vgarom.asm:574
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc02eb vgarom.asm:575
    mov dx, 003dah                            ; ba da 03                    ; 0xc02ee vgarom.asm:576
    in AL, DX                                 ; ec                          ; 0xc02f1 vgarom.asm:577
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc02f2 vgarom.asm:578
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc02f5 vgarom.asm:579
    out DX, AL                                ; ee                          ; 0xc02f7 vgarom.asm:580
    mov dx, 003dah                            ; ba da 03                    ; 0xc02f8 vgarom.asm:582
    in AL, DX                                 ; ec                          ; 0xc02fb vgarom.asm:583
    pop dx                                    ; 5a                          ; 0xc02fc vgarom.asm:585
    pop cx                                    ; 59                          ; 0xc02fd vgarom.asm:586
    pop bx                                    ; 5b                          ; 0xc02fe vgarom.asm:587
    pop ax                                    ; 58                          ; 0xc02ff vgarom.asm:588
    retn                                      ; c3                          ; 0xc0300 vgarom.asm:589
    push ax                                   ; 50                          ; 0xc0301 vgarom.asm:594
    push dx                                   ; 52                          ; 0xc0302 vgarom.asm:595
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0303 vgarom.asm:596
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0306 vgarom.asm:597
    out DX, AL                                ; ee                          ; 0xc0308 vgarom.asm:598
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0309 vgarom.asm:599
    pop ax                                    ; 58                          ; 0xc030c vgarom.asm:600
    push ax                                   ; 50                          ; 0xc030d vgarom.asm:601
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc030e vgarom.asm:602
    out DX, AL                                ; ee                          ; 0xc0310 vgarom.asm:603
    db  08ah, 0c5h
    ; mov al, ch                                ; 8a c5                     ; 0xc0311 vgarom.asm:604
    out DX, AL                                ; ee                          ; 0xc0313 vgarom.asm:605
    db  08ah, 0c1h
    ; mov al, cl                                ; 8a c1                     ; 0xc0314 vgarom.asm:606
    out DX, AL                                ; ee                          ; 0xc0316 vgarom.asm:607
    pop dx                                    ; 5a                          ; 0xc0317 vgarom.asm:608
    pop ax                                    ; 58                          ; 0xc0318 vgarom.asm:609
    retn                                      ; c3                          ; 0xc0319 vgarom.asm:610
    push ax                                   ; 50                          ; 0xc031a vgarom.asm:615
    push bx                                   ; 53                          ; 0xc031b vgarom.asm:616
    push cx                                   ; 51                          ; 0xc031c vgarom.asm:617
    push dx                                   ; 52                          ; 0xc031d vgarom.asm:618
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc031e vgarom.asm:619
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc0321 vgarom.asm:620
    out DX, AL                                ; ee                          ; 0xc0323 vgarom.asm:621
    pop dx                                    ; 5a                          ; 0xc0324 vgarom.asm:622
    push dx                                   ; 52                          ; 0xc0325 vgarom.asm:623
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc0326 vgarom.asm:624
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0328 vgarom.asm:625
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc032b vgarom.asm:627
    out DX, AL                                ; ee                          ; 0xc032e vgarom.asm:628
    inc bx                                    ; 43                          ; 0xc032f vgarom.asm:629
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0330 vgarom.asm:630
    out DX, AL                                ; ee                          ; 0xc0333 vgarom.asm:631
    inc bx                                    ; 43                          ; 0xc0334 vgarom.asm:632
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0335 vgarom.asm:633
    out DX, AL                                ; ee                          ; 0xc0338 vgarom.asm:634
    inc bx                                    ; 43                          ; 0xc0339 vgarom.asm:635
    dec cx                                    ; 49                          ; 0xc033a vgarom.asm:636
    jne short 0032bh                          ; 75 ee                       ; 0xc033b vgarom.asm:637
    pop dx                                    ; 5a                          ; 0xc033d vgarom.asm:638
    pop cx                                    ; 59                          ; 0xc033e vgarom.asm:639
    pop bx                                    ; 5b                          ; 0xc033f vgarom.asm:640
    pop ax                                    ; 58                          ; 0xc0340 vgarom.asm:641
    retn                                      ; c3                          ; 0xc0341 vgarom.asm:642
    push ax                                   ; 50                          ; 0xc0342 vgarom.asm:647
    push bx                                   ; 53                          ; 0xc0343 vgarom.asm:648
    push dx                                   ; 52                          ; 0xc0344 vgarom.asm:649
    mov dx, 003dah                            ; ba da 03                    ; 0xc0345 vgarom.asm:650
    in AL, DX                                 ; ec                          ; 0xc0348 vgarom.asm:651
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0349 vgarom.asm:652
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc034c vgarom.asm:653
    out DX, AL                                ; ee                          ; 0xc034e vgarom.asm:654
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc034f vgarom.asm:655
    in AL, DX                                 ; ec                          ; 0xc0352 vgarom.asm:656
    and bl, 001h                              ; 80 e3 01                    ; 0xc0353 vgarom.asm:657
    jne short 00365h                          ; 75 0d                       ; 0xc0356 vgarom.asm:658
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc0358 vgarom.asm:659
    sal bh, 007h                              ; c0 e7 07                    ; 0xc035a vgarom.asm:661
    db  00ah, 0c7h
    ; or al, bh                                 ; 0a c7                     ; 0xc035d vgarom.asm:671
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc035f vgarom.asm:672
    out DX, AL                                ; ee                          ; 0xc0362 vgarom.asm:673
    jmp short 0037eh                          ; eb 19                       ; 0xc0363 vgarom.asm:674
    push ax                                   ; 50                          ; 0xc0365 vgarom.asm:676
    mov dx, 003dah                            ; ba da 03                    ; 0xc0366 vgarom.asm:677
    in AL, DX                                 ; ec                          ; 0xc0369 vgarom.asm:678
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc036a vgarom.asm:679
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc036d vgarom.asm:680
    out DX, AL                                ; ee                          ; 0xc036f vgarom.asm:681
    pop ax                                    ; 58                          ; 0xc0370 vgarom.asm:682
    and AL, strict byte 080h                  ; 24 80                       ; 0xc0371 vgarom.asm:683
    jne short 00378h                          ; 75 03                       ; 0xc0373 vgarom.asm:684
    sal bh, 002h                              ; c0 e7 02                    ; 0xc0375 vgarom.asm:686
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc0378 vgarom.asm:692
    db  08ah, 0c7h
    ; mov al, bh                                ; 8a c7                     ; 0xc037b vgarom.asm:693
    out DX, AL                                ; ee                          ; 0xc037d vgarom.asm:694
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc037e vgarom.asm:696
    out DX, AL                                ; ee                          ; 0xc0380 vgarom.asm:697
    mov dx, 003dah                            ; ba da 03                    ; 0xc0381 vgarom.asm:699
    in AL, DX                                 ; ec                          ; 0xc0384 vgarom.asm:700
    pop dx                                    ; 5a                          ; 0xc0385 vgarom.asm:702
    pop bx                                    ; 5b                          ; 0xc0386 vgarom.asm:703
    pop ax                                    ; 58                          ; 0xc0387 vgarom.asm:704
    retn                                      ; c3                          ; 0xc0388 vgarom.asm:705
    push ax                                   ; 50                          ; 0xc0389 vgarom.asm:710
    push dx                                   ; 52                          ; 0xc038a vgarom.asm:711
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc038b vgarom.asm:712
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc038e vgarom.asm:713
    out DX, AL                                ; ee                          ; 0xc0390 vgarom.asm:714
    pop ax                                    ; 58                          ; 0xc0391 vgarom.asm:715
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc0392 vgarom.asm:716
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc0394 vgarom.asm:717
    in AL, DX                                 ; ec                          ; 0xc0397 vgarom.asm:718
    xchg al, ah                               ; 86 e0                       ; 0xc0398 vgarom.asm:719
    push ax                                   ; 50                          ; 0xc039a vgarom.asm:720
    in AL, DX                                 ; ec                          ; 0xc039b vgarom.asm:721
    db  08ah, 0e8h
    ; mov ch, al                                ; 8a e8                     ; 0xc039c vgarom.asm:722
    in AL, DX                                 ; ec                          ; 0xc039e vgarom.asm:723
    db  08ah, 0c8h
    ; mov cl, al                                ; 8a c8                     ; 0xc039f vgarom.asm:724
    pop dx                                    ; 5a                          ; 0xc03a1 vgarom.asm:725
    pop ax                                    ; 58                          ; 0xc03a2 vgarom.asm:726
    retn                                      ; c3                          ; 0xc03a3 vgarom.asm:727
    push ax                                   ; 50                          ; 0xc03a4 vgarom.asm:732
    push bx                                   ; 53                          ; 0xc03a5 vgarom.asm:733
    push cx                                   ; 51                          ; 0xc03a6 vgarom.asm:734
    push dx                                   ; 52                          ; 0xc03a7 vgarom.asm:735
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc03a8 vgarom.asm:736
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03ab vgarom.asm:737
    out DX, AL                                ; ee                          ; 0xc03ad vgarom.asm:738
    pop dx                                    ; 5a                          ; 0xc03ae vgarom.asm:739
    push dx                                   ; 52                          ; 0xc03af vgarom.asm:740
    db  08bh, 0dah
    ; mov bx, dx                                ; 8b da                     ; 0xc03b0 vgarom.asm:741
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc03b2 vgarom.asm:742
    in AL, DX                                 ; ec                          ; 0xc03b5 vgarom.asm:744
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03b6 vgarom.asm:745
    inc bx                                    ; 43                          ; 0xc03b9 vgarom.asm:746
    in AL, DX                                 ; ec                          ; 0xc03ba vgarom.asm:747
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03bb vgarom.asm:748
    inc bx                                    ; 43                          ; 0xc03be vgarom.asm:749
    in AL, DX                                 ; ec                          ; 0xc03bf vgarom.asm:750
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc03c0 vgarom.asm:751
    inc bx                                    ; 43                          ; 0xc03c3 vgarom.asm:752
    dec cx                                    ; 49                          ; 0xc03c4 vgarom.asm:753
    jne short 003b5h                          ; 75 ee                       ; 0xc03c5 vgarom.asm:754
    pop dx                                    ; 5a                          ; 0xc03c7 vgarom.asm:755
    pop cx                                    ; 59                          ; 0xc03c8 vgarom.asm:756
    pop bx                                    ; 5b                          ; 0xc03c9 vgarom.asm:757
    pop ax                                    ; 58                          ; 0xc03ca vgarom.asm:758
    retn                                      ; c3                          ; 0xc03cb vgarom.asm:759
    push ax                                   ; 50                          ; 0xc03cc vgarom.asm:764
    push dx                                   ; 52                          ; 0xc03cd vgarom.asm:765
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03ce vgarom.asm:766
    db  08ah, 0c3h
    ; mov al, bl                                ; 8a c3                     ; 0xc03d1 vgarom.asm:767
    out DX, AL                                ; ee                          ; 0xc03d3 vgarom.asm:768
    pop dx                                    ; 5a                          ; 0xc03d4 vgarom.asm:769
    pop ax                                    ; 58                          ; 0xc03d5 vgarom.asm:770
    retn                                      ; c3                          ; 0xc03d6 vgarom.asm:771
    push ax                                   ; 50                          ; 0xc03d7 vgarom.asm:776
    push dx                                   ; 52                          ; 0xc03d8 vgarom.asm:777
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc03d9 vgarom.asm:778
    in AL, DX                                 ; ec                          ; 0xc03dc vgarom.asm:779
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03dd vgarom.asm:780
    pop dx                                    ; 5a                          ; 0xc03df vgarom.asm:781
    pop ax                                    ; 58                          ; 0xc03e0 vgarom.asm:782
    retn                                      ; c3                          ; 0xc03e1 vgarom.asm:783
    push ax                                   ; 50                          ; 0xc03e2 vgarom.asm:788
    push dx                                   ; 52                          ; 0xc03e3 vgarom.asm:789
    mov dx, 003dah                            ; ba da 03                    ; 0xc03e4 vgarom.asm:790
    in AL, DX                                 ; ec                          ; 0xc03e7 vgarom.asm:791
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc03e8 vgarom.asm:792
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc03eb vgarom.asm:793
    out DX, AL                                ; ee                          ; 0xc03ed vgarom.asm:794
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc03ee vgarom.asm:795
    in AL, DX                                 ; ec                          ; 0xc03f1 vgarom.asm:796
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc03f2 vgarom.asm:797
    shr bl, 007h                              ; c0 eb 07                    ; 0xc03f4 vgarom.asm:799
    mov dx, 003dah                            ; ba da 03                    ; 0xc03f7 vgarom.asm:809
    in AL, DX                                 ; ec                          ; 0xc03fa vgarom.asm:810
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc03fb vgarom.asm:811
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc03fe vgarom.asm:812
    out DX, AL                                ; ee                          ; 0xc0400 vgarom.asm:813
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0401 vgarom.asm:814
    in AL, DX                                 ; ec                          ; 0xc0404 vgarom.asm:815
    db  08ah, 0f8h
    ; mov bh, al                                ; 8a f8                     ; 0xc0405 vgarom.asm:816
    and bh, 00fh                              ; 80 e7 0f                    ; 0xc0407 vgarom.asm:817
    test bl, 001h                             ; f6 c3 01                    ; 0xc040a vgarom.asm:818
    jne short 00412h                          ; 75 03                       ; 0xc040d vgarom.asm:819
    shr bh, 002h                              ; c0 ef 02                    ; 0xc040f vgarom.asm:821
    mov dx, 003dah                            ; ba da 03                    ; 0xc0412 vgarom.asm:827
    in AL, DX                                 ; ec                          ; 0xc0415 vgarom.asm:828
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0416 vgarom.asm:829
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0419 vgarom.asm:830
    out DX, AL                                ; ee                          ; 0xc041b vgarom.asm:831
    mov dx, 003dah                            ; ba da 03                    ; 0xc041c vgarom.asm:833
    in AL, DX                                 ; ec                          ; 0xc041f vgarom.asm:834
    pop dx                                    ; 5a                          ; 0xc0420 vgarom.asm:836
    pop ax                                    ; 58                          ; 0xc0421 vgarom.asm:837
    retn                                      ; c3                          ; 0xc0422 vgarom.asm:838
    push ax                                   ; 50                          ; 0xc0423 vgarom.asm:843
    push dx                                   ; 52                          ; 0xc0424 vgarom.asm:844
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0425 vgarom.asm:845
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc0428 vgarom.asm:846
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc042a vgarom.asm:847
    out DX, ax                                ; ef                          ; 0xc042c vgarom.asm:848
    pop dx                                    ; 5a                          ; 0xc042d vgarom.asm:849
    pop ax                                    ; 58                          ; 0xc042e vgarom.asm:850
    retn                                      ; c3                          ; 0xc042f vgarom.asm:851
    push DS                                   ; 1e                          ; 0xc0430 vgarom.asm:856
    push ax                                   ; 50                          ; 0xc0431 vgarom.asm:857
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0432 vgarom.asm:858
    mov ds, ax                                ; 8e d8                       ; 0xc0435 vgarom.asm:859
    db  032h, 0edh
    ; xor ch, ch                                ; 32 ed                     ; 0xc0437 vgarom.asm:860
    mov bx, 00088h                            ; bb 88 00                    ; 0xc0439 vgarom.asm:861
    mov cl, byte [bx]                         ; 8a 0f                       ; 0xc043c vgarom.asm:862
    and cl, 00fh                              ; 80 e1 0f                    ; 0xc043e vgarom.asm:863
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc0441 vgarom.asm:864
    mov ax, word [bx]                         ; 8b 07                       ; 0xc0444 vgarom.asm:865
    mov bx, strict word 00003h                ; bb 03 00                    ; 0xc0446 vgarom.asm:866
    cmp ax, 003b4h                            ; 3d b4 03                    ; 0xc0449 vgarom.asm:867
    jne short 00450h                          ; 75 02                       ; 0xc044c vgarom.asm:868
    mov BH, strict byte 001h                  ; b7 01                       ; 0xc044e vgarom.asm:869
    pop ax                                    ; 58                          ; 0xc0450 vgarom.asm:871
    pop DS                                    ; 1f                          ; 0xc0451 vgarom.asm:872
    retn                                      ; c3                          ; 0xc0452 vgarom.asm:873
    push DS                                   ; 1e                          ; 0xc0453 vgarom.asm:881
    push bx                                   ; 53                          ; 0xc0454 vgarom.asm:882
    push dx                                   ; 52                          ; 0xc0455 vgarom.asm:883
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc0456 vgarom.asm:884
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0458 vgarom.asm:885
    mov ds, ax                                ; 8e d8                       ; 0xc045b vgarom.asm:886
    mov bx, 00089h                            ; bb 89 00                    ; 0xc045d vgarom.asm:887
    mov al, byte [bx]                         ; 8a 07                       ; 0xc0460 vgarom.asm:888
    mov bx, 00088h                            ; bb 88 00                    ; 0xc0462 vgarom.asm:889
    mov ah, byte [bx]                         ; 8a 27                       ; 0xc0465 vgarom.asm:890
    cmp dl, 001h                              ; 80 fa 01                    ; 0xc0467 vgarom.asm:891
    je short 00481h                           ; 74 15                       ; 0xc046a vgarom.asm:892
    jc short 0048bh                           ; 72 1d                       ; 0xc046c vgarom.asm:893
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc046e vgarom.asm:894
    je short 00475h                           ; 74 02                       ; 0xc0471 vgarom.asm:895
    jmp short 0049fh                          ; eb 2a                       ; 0xc0473 vgarom.asm:905
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc0475 vgarom.asm:911
    or AL, strict byte 010h                   ; 0c 10                       ; 0xc0477 vgarom.asm:912
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc0479 vgarom.asm:913
    or ah, 009h                               ; 80 cc 09                    ; 0xc047c vgarom.asm:914
    jne short 00495h                          ; 75 14                       ; 0xc047f vgarom.asm:915
    and AL, strict byte 06fh                  ; 24 6f                       ; 0xc0481 vgarom.asm:921
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc0483 vgarom.asm:922
    or ah, 009h                               ; 80 cc 09                    ; 0xc0486 vgarom.asm:923
    jne short 00495h                          ; 75 0a                       ; 0xc0489 vgarom.asm:924
    and AL, strict byte 0efh                  ; 24 ef                       ; 0xc048b vgarom.asm:930
    or AL, strict byte 080h                   ; 0c 80                       ; 0xc048d vgarom.asm:931
    and ah, 0f0h                              ; 80 e4 f0                    ; 0xc048f vgarom.asm:932
    or ah, 008h                               ; 80 cc 08                    ; 0xc0492 vgarom.asm:933
    mov bx, 00089h                            ; bb 89 00                    ; 0xc0495 vgarom.asm:935
    mov byte [bx], al                         ; 88 07                       ; 0xc0498 vgarom.asm:936
    mov bx, 00088h                            ; bb 88 00                    ; 0xc049a vgarom.asm:937
    mov byte [bx], ah                         ; 88 27                       ; 0xc049d vgarom.asm:938
    mov ax, 01212h                            ; b8 12 12                    ; 0xc049f vgarom.asm:940
    pop dx                                    ; 5a                          ; 0xc04a2 vgarom.asm:941
    pop bx                                    ; 5b                          ; 0xc04a3 vgarom.asm:942
    pop DS                                    ; 1f                          ; 0xc04a4 vgarom.asm:943
    retn                                      ; c3                          ; 0xc04a5 vgarom.asm:944
    push DS                                   ; 1e                          ; 0xc04a6 vgarom.asm:953
    push bx                                   ; 53                          ; 0xc04a7 vgarom.asm:954
    push dx                                   ; 52                          ; 0xc04a8 vgarom.asm:955
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04a9 vgarom.asm:956
    and dl, 001h                              ; 80 e2 01                    ; 0xc04ab vgarom.asm:957
    sal dl, 003h                              ; c0 e2 03                    ; 0xc04ae vgarom.asm:959
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04b1 vgarom.asm:965
    mov ds, ax                                ; 8e d8                       ; 0xc04b4 vgarom.asm:966
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04b6 vgarom.asm:967
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04b9 vgarom.asm:968
    and AL, strict byte 0f7h                  ; 24 f7                       ; 0xc04bb vgarom.asm:969
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc04bd vgarom.asm:970
    mov byte [bx], al                         ; 88 07                       ; 0xc04bf vgarom.asm:971
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04c1 vgarom.asm:972
    pop dx                                    ; 5a                          ; 0xc04c4 vgarom.asm:973
    pop bx                                    ; 5b                          ; 0xc04c5 vgarom.asm:974
    pop DS                                    ; 1f                          ; 0xc04c6 vgarom.asm:975
    retn                                      ; c3                          ; 0xc04c7 vgarom.asm:976
    push bx                                   ; 53                          ; 0xc04c8 vgarom.asm:980
    push dx                                   ; 52                          ; 0xc04c9 vgarom.asm:981
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc04ca vgarom.asm:982
    and bl, 001h                              ; 80 e3 01                    ; 0xc04cc vgarom.asm:983
    xor bl, 001h                              ; 80 f3 01                    ; 0xc04cf vgarom.asm:984
    sal bl, 1                                 ; d0 e3                       ; 0xc04d2 vgarom.asm:985
    mov dx, 003cch                            ; ba cc 03                    ; 0xc04d4 vgarom.asm:986
    in AL, DX                                 ; ec                          ; 0xc04d7 vgarom.asm:987
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc04d8 vgarom.asm:988
    db  00ah, 0c3h
    ; or al, bl                                 ; 0a c3                     ; 0xc04da vgarom.asm:989
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc04dc vgarom.asm:990
    out DX, AL                                ; ee                          ; 0xc04df vgarom.asm:991
    mov ax, 01212h                            ; b8 12 12                    ; 0xc04e0 vgarom.asm:992
    pop dx                                    ; 5a                          ; 0xc04e3 vgarom.asm:993
    pop bx                                    ; 5b                          ; 0xc04e4 vgarom.asm:994
    retn                                      ; c3                          ; 0xc04e5 vgarom.asm:995
    push DS                                   ; 1e                          ; 0xc04e6 vgarom.asm:999
    push bx                                   ; 53                          ; 0xc04e7 vgarom.asm:1000
    push dx                                   ; 52                          ; 0xc04e8 vgarom.asm:1001
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc04e9 vgarom.asm:1002
    and dl, 001h                              ; 80 e2 01                    ; 0xc04eb vgarom.asm:1003
    xor dl, 001h                              ; 80 f2 01                    ; 0xc04ee vgarom.asm:1004
    sal dl, 1                                 ; d0 e2                       ; 0xc04f1 vgarom.asm:1005
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc04f3 vgarom.asm:1006
    mov ds, ax                                ; 8e d8                       ; 0xc04f6 vgarom.asm:1007
    mov bx, 00089h                            ; bb 89 00                    ; 0xc04f8 vgarom.asm:1008
    mov al, byte [bx]                         ; 8a 07                       ; 0xc04fb vgarom.asm:1009
    and AL, strict byte 0fdh                  ; 24 fd                       ; 0xc04fd vgarom.asm:1010
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc04ff vgarom.asm:1011
    mov byte [bx], al                         ; 88 07                       ; 0xc0501 vgarom.asm:1012
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0503 vgarom.asm:1013
    pop dx                                    ; 5a                          ; 0xc0506 vgarom.asm:1014
    pop bx                                    ; 5b                          ; 0xc0507 vgarom.asm:1015
    pop DS                                    ; 1f                          ; 0xc0508 vgarom.asm:1016
    retn                                      ; c3                          ; 0xc0509 vgarom.asm:1017
    push DS                                   ; 1e                          ; 0xc050a vgarom.asm:1021
    push bx                                   ; 53                          ; 0xc050b vgarom.asm:1022
    push dx                                   ; 52                          ; 0xc050c vgarom.asm:1023
    db  08ah, 0d0h
    ; mov dl, al                                ; 8a d0                     ; 0xc050d vgarom.asm:1024
    and dl, 001h                              ; 80 e2 01                    ; 0xc050f vgarom.asm:1025
    xor dl, 001h                              ; 80 f2 01                    ; 0xc0512 vgarom.asm:1026
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0515 vgarom.asm:1027
    mov ds, ax                                ; 8e d8                       ; 0xc0518 vgarom.asm:1028
    mov bx, 00089h                            ; bb 89 00                    ; 0xc051a vgarom.asm:1029
    mov al, byte [bx]                         ; 8a 07                       ; 0xc051d vgarom.asm:1030
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc051f vgarom.asm:1031
    db  00ah, 0c2h
    ; or al, dl                                 ; 0a c2                     ; 0xc0521 vgarom.asm:1032
    mov byte [bx], al                         ; 88 07                       ; 0xc0523 vgarom.asm:1033
    mov ax, 01212h                            ; b8 12 12                    ; 0xc0525 vgarom.asm:1034
    pop dx                                    ; 5a                          ; 0xc0528 vgarom.asm:1035
    pop bx                                    ; 5b                          ; 0xc0529 vgarom.asm:1036
    pop DS                                    ; 1f                          ; 0xc052a vgarom.asm:1037
    retn                                      ; c3                          ; 0xc052b vgarom.asm:1038
    cmp AL, strict byte 000h                  ; 3c 00                       ; 0xc052c vgarom.asm:1043
    je short 00535h                           ; 74 05                       ; 0xc052e vgarom.asm:1044
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc0530 vgarom.asm:1045
    je short 0054ah                           ; 74 16                       ; 0xc0532 vgarom.asm:1046
    retn                                      ; c3                          ; 0xc0534 vgarom.asm:1050
    push DS                                   ; 1e                          ; 0xc0535 vgarom.asm:1052
    push ax                                   ; 50                          ; 0xc0536 vgarom.asm:1053
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0537 vgarom.asm:1054
    mov ds, ax                                ; 8e d8                       ; 0xc053a vgarom.asm:1055
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc053c vgarom.asm:1056
    mov al, byte [bx]                         ; 8a 07                       ; 0xc053f vgarom.asm:1057
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc0541 vgarom.asm:1058
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0543 vgarom.asm:1059
    pop ax                                    ; 58                          ; 0xc0545 vgarom.asm:1060
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc0546 vgarom.asm:1061
    pop DS                                    ; 1f                          ; 0xc0548 vgarom.asm:1062
    retn                                      ; c3                          ; 0xc0549 vgarom.asm:1063
    push DS                                   ; 1e                          ; 0xc054a vgarom.asm:1065
    push ax                                   ; 50                          ; 0xc054b vgarom.asm:1066
    push bx                                   ; 53                          ; 0xc054c vgarom.asm:1067
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc054d vgarom.asm:1068
    mov ds, ax                                ; 8e d8                       ; 0xc0550 vgarom.asm:1069
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc0552 vgarom.asm:1070
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc0554 vgarom.asm:1071
    mov byte [bx], al                         ; 88 07                       ; 0xc0557 vgarom.asm:1072
    pop bx                                    ; 5b                          ; 0xc0559 vgarom.asm:1082
    pop ax                                    ; 58                          ; 0xc055a vgarom.asm:1083
    db  08ah, 0c4h
    ; mov al, ah                                ; 8a c4                     ; 0xc055b vgarom.asm:1084
    pop DS                                    ; 1f                          ; 0xc055d vgarom.asm:1085
    retn                                      ; c3                          ; 0xc055e vgarom.asm:1086
    times 0x1 db 0
  ; disGetNextSymbol 0xc0560 LB 0x397 -> off=0x0 cb=0000000000000007 uValue=00000000000c0560 'do_out_dx_ax'
do_out_dx_ax:                                ; 0xc0560 LB 0x7
    xchg ah, al                               ; 86 c4                       ; 0xc0560 vberom.asm:69
    out DX, AL                                ; ee                          ; 0xc0562 vberom.asm:70
    xchg ah, al                               ; 86 c4                       ; 0xc0563 vberom.asm:71
    out DX, AL                                ; ee                          ; 0xc0565 vberom.asm:72
    retn                                      ; c3                          ; 0xc0566 vberom.asm:73
  ; disGetNextSymbol 0xc0567 LB 0x390 -> off=0x0 cb=0000000000000040 uValue=00000000000c0567 'do_in_ax_dx'
do_in_ax_dx:                                 ; 0xc0567 LB 0x40
    in AL, DX                                 ; ec                          ; 0xc0567 vberom.asm:76
    xchg ah, al                               ; 86 c4                       ; 0xc0568 vberom.asm:77
    in AL, DX                                 ; ec                          ; 0xc056a vberom.asm:78
    retn                                      ; c3                          ; 0xc056b vberom.asm:79
    push ax                                   ; 50                          ; 0xc056c vberom.asm:90
    push dx                                   ; 52                          ; 0xc056d vberom.asm:91
    mov dx, 003dah                            ; ba da 03                    ; 0xc056e vberom.asm:92
    in AL, DX                                 ; ec                          ; 0xc0571 vberom.asm:94
    test AL, strict byte 008h                 ; a8 08                       ; 0xc0572 vberom.asm:95
    je short 00571h                           ; 74 fb                       ; 0xc0574 vberom.asm:96
    pop dx                                    ; 5a                          ; 0xc0576 vberom.asm:97
    pop ax                                    ; 58                          ; 0xc0577 vberom.asm:98
    retn                                      ; c3                          ; 0xc0578 vberom.asm:99
    push ax                                   ; 50                          ; 0xc0579 vberom.asm:102
    push dx                                   ; 52                          ; 0xc057a vberom.asm:103
    mov dx, 003dah                            ; ba da 03                    ; 0xc057b vberom.asm:104
    in AL, DX                                 ; ec                          ; 0xc057e vberom.asm:106
    test AL, strict byte 008h                 ; a8 08                       ; 0xc057f vberom.asm:107
    jne short 0057eh                          ; 75 fb                       ; 0xc0581 vberom.asm:108
    pop dx                                    ; 5a                          ; 0xc0583 vberom.asm:109
    pop ax                                    ; 58                          ; 0xc0584 vberom.asm:110
    retn                                      ; c3                          ; 0xc0585 vberom.asm:111
    push dx                                   ; 52                          ; 0xc0586 vberom.asm:116
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0587 vberom.asm:117
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc058a vberom.asm:118
    call 00560h                               ; e8 d0 ff                    ; 0xc058d vberom.asm:119
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0590 vberom.asm:120
    call 00567h                               ; e8 d1 ff                    ; 0xc0593 vberom.asm:121
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc0596 vberom.asm:122
    jbe short 005a5h                          ; 76 0b                       ; 0xc0598 vberom.asm:123
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc059a vberom.asm:124
    shr ah, 003h                              ; c0 ec 03                    ; 0xc059c vberom.asm:126
    test AL, strict byte 007h                 ; a8 07                       ; 0xc059f vberom.asm:132
    je short 005a5h                           ; 74 02                       ; 0xc05a1 vberom.asm:133
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc05a3 vberom.asm:134
    pop dx                                    ; 5a                          ; 0xc05a5 vberom.asm:136
    retn                                      ; c3                          ; 0xc05a6 vberom.asm:137
  ; disGetNextSymbol 0xc05a7 LB 0x350 -> off=0x0 cb=0000000000000026 uValue=00000000000c05a7 '_dispi_get_max_bpp'
_dispi_get_max_bpp:                          ; 0xc05a7 LB 0x26
    push dx                                   ; 52                          ; 0xc05a7 vberom.asm:142
    push bx                                   ; 53                          ; 0xc05a8 vberom.asm:143
    call 005e1h                               ; e8 35 00                    ; 0xc05a9 vberom.asm:144
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc05ac vberom.asm:145
    or ax, strict byte 00002h                 ; 83 c8 02                    ; 0xc05ae vberom.asm:146
    call 005cdh                               ; e8 19 00                    ; 0xc05b1 vberom.asm:147
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05b4 vberom.asm:148
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc05b7 vberom.asm:149
    call 00560h                               ; e8 a3 ff                    ; 0xc05ba vberom.asm:150
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05bd vberom.asm:151
    call 00567h                               ; e8 a4 ff                    ; 0xc05c0 vberom.asm:152
    push ax                                   ; 50                          ; 0xc05c3 vberom.asm:153
    db  08bh, 0c3h
    ; mov ax, bx                                ; 8b c3                     ; 0xc05c4 vberom.asm:154
    call 005cdh                               ; e8 04 00                    ; 0xc05c6 vberom.asm:155
    pop ax                                    ; 58                          ; 0xc05c9 vberom.asm:156
    pop bx                                    ; 5b                          ; 0xc05ca vberom.asm:157
    pop dx                                    ; 5a                          ; 0xc05cb vberom.asm:158
    retn                                      ; c3                          ; 0xc05cc vberom.asm:159
  ; disGetNextSymbol 0xc05cd LB 0x32a -> off=0x0 cb=0000000000000026 uValue=00000000000c05cd 'dispi_set_enable_'
dispi_set_enable_:                           ; 0xc05cd LB 0x26
    push dx                                   ; 52                          ; 0xc05cd vberom.asm:162
    push ax                                   ; 50                          ; 0xc05ce vberom.asm:163
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05cf vberom.asm:164
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc05d2 vberom.asm:165
    call 00560h                               ; e8 88 ff                    ; 0xc05d5 vberom.asm:166
    pop ax                                    ; 58                          ; 0xc05d8 vberom.asm:167
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05d9 vberom.asm:168
    call 00560h                               ; e8 81 ff                    ; 0xc05dc vberom.asm:169
    pop dx                                    ; 5a                          ; 0xc05df vberom.asm:170
    retn                                      ; c3                          ; 0xc05e0 vberom.asm:171
    push dx                                   ; 52                          ; 0xc05e1 vberom.asm:174
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05e2 vberom.asm:175
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc05e5 vberom.asm:176
    call 00560h                               ; e8 75 ff                    ; 0xc05e8 vberom.asm:177
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05eb vberom.asm:178
    call 00567h                               ; e8 76 ff                    ; 0xc05ee vberom.asm:179
    pop dx                                    ; 5a                          ; 0xc05f1 vberom.asm:180
    retn                                      ; c3                          ; 0xc05f2 vberom.asm:181
  ; disGetNextSymbol 0xc05f3 LB 0x304 -> off=0x0 cb=0000000000000026 uValue=00000000000c05f3 'dispi_set_bank_'
dispi_set_bank_:                             ; 0xc05f3 LB 0x26
    push dx                                   ; 52                          ; 0xc05f3 vberom.asm:184
    push ax                                   ; 50                          ; 0xc05f4 vberom.asm:185
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc05f5 vberom.asm:186
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc05f8 vberom.asm:187
    call 00560h                               ; e8 62 ff                    ; 0xc05fb vberom.asm:188
    pop ax                                    ; 58                          ; 0xc05fe vberom.asm:189
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc05ff vberom.asm:190
    call 00560h                               ; e8 5b ff                    ; 0xc0602 vberom.asm:191
    pop dx                                    ; 5a                          ; 0xc0605 vberom.asm:192
    retn                                      ; c3                          ; 0xc0606 vberom.asm:193
    push dx                                   ; 52                          ; 0xc0607 vberom.asm:196
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0608 vberom.asm:197
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc060b vberom.asm:198
    call 00560h                               ; e8 4f ff                    ; 0xc060e vberom.asm:199
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0611 vberom.asm:200
    call 00567h                               ; e8 50 ff                    ; 0xc0614 vberom.asm:201
    pop dx                                    ; 5a                          ; 0xc0617 vberom.asm:202
    retn                                      ; c3                          ; 0xc0618 vberom.asm:203
  ; disGetNextSymbol 0xc0619 LB 0x2de -> off=0x0 cb=00000000000000a9 uValue=00000000000c0619 '_dispi_set_bank_farcall'
_dispi_set_bank_farcall:                     ; 0xc0619 LB 0xa9
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc0619 vberom.asm:206
    je short 00643h                           ; 74 24                       ; 0xc061d vberom.asm:207
    db  00bh, 0dbh
    ; or bx, bx                                 ; 0b db                     ; 0xc061f vberom.asm:208
    jne short 00655h                          ; 75 32                       ; 0xc0621 vberom.asm:209
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc0623 vberom.asm:210
    push dx                                   ; 52                          ; 0xc0625 vberom.asm:211
    push ax                                   ; 50                          ; 0xc0626 vberom.asm:212
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0627 vberom.asm:213
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc062a vberom.asm:214
    call 00560h                               ; e8 30 ff                    ; 0xc062d vberom.asm:215
    pop ax                                    ; 58                          ; 0xc0630 vberom.asm:216
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0631 vberom.asm:217
    call 00560h                               ; e8 29 ff                    ; 0xc0634 vberom.asm:218
    call 00567h                               ; e8 2d ff                    ; 0xc0637 vberom.asm:219
    pop dx                                    ; 5a                          ; 0xc063a vberom.asm:220
    db  03bh, 0d0h
    ; cmp dx, ax                                ; 3b d0                     ; 0xc063b vberom.asm:221
    jne short 00655h                          ; 75 16                       ; 0xc063d vberom.asm:222
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc063f vberom.asm:223
    retf                                      ; cb                          ; 0xc0642 vberom.asm:224
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0643 vberom.asm:226
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0646 vberom.asm:227
    call 00560h                               ; e8 14 ff                    ; 0xc0649 vberom.asm:228
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc064c vberom.asm:229
    call 00567h                               ; e8 15 ff                    ; 0xc064f vberom.asm:230
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0652 vberom.asm:231
    retf                                      ; cb                          ; 0xc0654 vberom.asm:232
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0655 vberom.asm:234
    retf                                      ; cb                          ; 0xc0658 vberom.asm:235
    push dx                                   ; 52                          ; 0xc0659 vberom.asm:238
    push ax                                   ; 50                          ; 0xc065a vberom.asm:239
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc065b vberom.asm:240
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc065e vberom.asm:241
    call 00560h                               ; e8 fc fe                    ; 0xc0661 vberom.asm:242
    pop ax                                    ; 58                          ; 0xc0664 vberom.asm:243
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0665 vberom.asm:244
    call 00560h                               ; e8 f5 fe                    ; 0xc0668 vberom.asm:245
    pop dx                                    ; 5a                          ; 0xc066b vberom.asm:246
    retn                                      ; c3                          ; 0xc066c vberom.asm:247
    push dx                                   ; 52                          ; 0xc066d vberom.asm:250
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc066e vberom.asm:251
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc0671 vberom.asm:252
    call 00560h                               ; e8 e9 fe                    ; 0xc0674 vberom.asm:253
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0677 vberom.asm:254
    call 00567h                               ; e8 ea fe                    ; 0xc067a vberom.asm:255
    pop dx                                    ; 5a                          ; 0xc067d vberom.asm:256
    retn                                      ; c3                          ; 0xc067e vberom.asm:257
    push dx                                   ; 52                          ; 0xc067f vberom.asm:260
    push ax                                   ; 50                          ; 0xc0680 vberom.asm:261
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0681 vberom.asm:262
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc0684 vberom.asm:263
    call 00560h                               ; e8 d6 fe                    ; 0xc0687 vberom.asm:264
    pop ax                                    ; 58                          ; 0xc068a vberom.asm:265
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc068b vberom.asm:266
    call 00560h                               ; e8 cf fe                    ; 0xc068e vberom.asm:267
    pop dx                                    ; 5a                          ; 0xc0691 vberom.asm:268
    retn                                      ; c3                          ; 0xc0692 vberom.asm:269
    push dx                                   ; 52                          ; 0xc0693 vberom.asm:272
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc0694 vberom.asm:273
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc0697 vberom.asm:274
    call 00560h                               ; e8 c3 fe                    ; 0xc069a vberom.asm:275
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc069d vberom.asm:276
    call 00567h                               ; e8 c4 fe                    ; 0xc06a0 vberom.asm:277
    pop dx                                    ; 5a                          ; 0xc06a3 vberom.asm:278
    retn                                      ; c3                          ; 0xc06a4 vberom.asm:279
    push ax                                   ; 50                          ; 0xc06a5 vberom.asm:282
    push bx                                   ; 53                          ; 0xc06a6 vberom.asm:283
    push dx                                   ; 52                          ; 0xc06a7 vberom.asm:284
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc06a8 vberom.asm:285
    call 00586h                               ; e8 d9 fe                    ; 0xc06aa vberom.asm:286
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc06ad vberom.asm:287
    jnbe short 006b3h                         ; 77 02                       ; 0xc06af vberom.asm:288
    shr bx, 1                                 ; d1 eb                       ; 0xc06b1 vberom.asm:289
    shr bx, 003h                              ; c1 eb 03                    ; 0xc06b3 vberom.asm:292
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06b6 vberom.asm:298
    db  08ah, 0e3h
    ; mov ah, bl                                ; 8a e3                     ; 0xc06b9 vberom.asm:299
    mov AL, strict byte 013h                  ; b0 13                       ; 0xc06bb vberom.asm:300
    out DX, ax                                ; ef                          ; 0xc06bd vberom.asm:301
    pop dx                                    ; 5a                          ; 0xc06be vberom.asm:302
    pop bx                                    ; 5b                          ; 0xc06bf vberom.asm:303
    pop ax                                    ; 58                          ; 0xc06c0 vberom.asm:304
    retn                                      ; c3                          ; 0xc06c1 vberom.asm:305
  ; disGetNextSymbol 0xc06c2 LB 0x235 -> off=0x0 cb=00000000000000ed uValue=00000000000c06c2 '_vga_compat_setup'
_vga_compat_setup:                           ; 0xc06c2 LB 0xed
    push ax                                   ; 50                          ; 0xc06c2 vberom.asm:308
    push dx                                   ; 52                          ; 0xc06c3 vberom.asm:309
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06c4 vberom.asm:312
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc06c7 vberom.asm:313
    call 00560h                               ; e8 93 fe                    ; 0xc06ca vberom.asm:314
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06cd vberom.asm:315
    call 00567h                               ; e8 94 fe                    ; 0xc06d0 vberom.asm:316
    push ax                                   ; 50                          ; 0xc06d3 vberom.asm:317
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06d4 vberom.asm:318
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc06d7 vberom.asm:319
    out DX, ax                                ; ef                          ; 0xc06da vberom.asm:320
    pop ax                                    ; 58                          ; 0xc06db vberom.asm:321
    push ax                                   ; 50                          ; 0xc06dc vberom.asm:322
    shr ax, 003h                              ; c1 e8 03                    ; 0xc06dd vberom.asm:324
    dec ax                                    ; 48                          ; 0xc06e0 vberom.asm:330
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc06e1 vberom.asm:331
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc06e3 vberom.asm:332
    out DX, ax                                ; ef                          ; 0xc06e5 vberom.asm:333
    pop ax                                    ; 58                          ; 0xc06e6 vberom.asm:334
    call 006a5h                               ; e8 bb ff                    ; 0xc06e7 vberom.asm:335
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc06ea vberom.asm:338
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc06ed vberom.asm:339
    call 00560h                               ; e8 6d fe                    ; 0xc06f0 vberom.asm:340
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc06f3 vberom.asm:341
    call 00567h                               ; e8 6e fe                    ; 0xc06f6 vberom.asm:342
    dec ax                                    ; 48                          ; 0xc06f9 vberom.asm:343
    push ax                                   ; 50                          ; 0xc06fa vberom.asm:344
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc06fb vberom.asm:345
    db  08ah, 0e0h
    ; mov ah, al                                ; 8a e0                     ; 0xc06fe vberom.asm:346
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc0700 vberom.asm:347
    out DX, ax                                ; ef                          ; 0xc0702 vberom.asm:348
    pop ax                                    ; 58                          ; 0xc0703 vberom.asm:349
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc0704 vberom.asm:350
    out DX, AL                                ; ee                          ; 0xc0706 vberom.asm:351
    inc dx                                    ; 42                          ; 0xc0707 vberom.asm:352
    in AL, DX                                 ; ec                          ; 0xc0708 vberom.asm:353
    and AL, strict byte 0bdh                  ; 24 bd                       ; 0xc0709 vberom.asm:354
    test ah, 001h                             ; f6 c4 01                    ; 0xc070b vberom.asm:355
    je short 00712h                           ; 74 02                       ; 0xc070e vberom.asm:356
    or AL, strict byte 002h                   ; 0c 02                       ; 0xc0710 vberom.asm:357
    test ah, 002h                             ; f6 c4 02                    ; 0xc0712 vberom.asm:359
    je short 00719h                           ; 74 02                       ; 0xc0715 vberom.asm:360
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0717 vberom.asm:361
    out DX, AL                                ; ee                          ; 0xc0719 vberom.asm:363
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc071a vberom.asm:366
    mov ax, strict word 00009h                ; b8 09 00                    ; 0xc071d vberom.asm:367
    out DX, AL                                ; ee                          ; 0xc0720 vberom.asm:368
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc0721 vberom.asm:369
    in AL, DX                                 ; ec                          ; 0xc0724 vberom.asm:370
    and AL, strict byte 060h                  ; 24 60                       ; 0xc0725 vberom.asm:371
    out DX, AL                                ; ee                          ; 0xc0727 vberom.asm:372
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc0728 vberom.asm:373
    mov AL, strict byte 017h                  ; b0 17                       ; 0xc072b vberom.asm:374
    out DX, AL                                ; ee                          ; 0xc072d vberom.asm:375
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc072e vberom.asm:376
    in AL, DX                                 ; ec                          ; 0xc0731 vberom.asm:377
    or AL, strict byte 003h                   ; 0c 03                       ; 0xc0732 vberom.asm:378
    out DX, AL                                ; ee                          ; 0xc0734 vberom.asm:379
    mov dx, 003dah                            ; ba da 03                    ; 0xc0735 vberom.asm:380
    in AL, DX                                 ; ec                          ; 0xc0738 vberom.asm:381
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0739 vberom.asm:382
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc073c vberom.asm:383
    out DX, AL                                ; ee                          ; 0xc073e vberom.asm:384
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc073f vberom.asm:385
    in AL, DX                                 ; ec                          ; 0xc0742 vberom.asm:386
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc0743 vberom.asm:387
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc0745 vberom.asm:388
    out DX, AL                                ; ee                          ; 0xc0748 vberom.asm:389
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc0749 vberom.asm:390
    out DX, AL                                ; ee                          ; 0xc074b vberom.asm:391
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc074c vberom.asm:392
    mov ax, 00506h                            ; b8 06 05                    ; 0xc074f vberom.asm:393
    out DX, ax                                ; ef                          ; 0xc0752 vberom.asm:394
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0753 vberom.asm:395
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc0756 vberom.asm:396
    out DX, ax                                ; ef                          ; 0xc0759 vberom.asm:397
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc075a vberom.asm:400
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc075d vberom.asm:401
    call 00560h                               ; e8 fd fd                    ; 0xc0760 vberom.asm:402
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc0763 vberom.asm:403
    call 00567h                               ; e8 fe fd                    ; 0xc0766 vberom.asm:404
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc0769 vberom.asm:405
    jc short 007adh                           ; 72 40                       ; 0xc076b vberom.asm:406
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc076d vberom.asm:407
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc0770 vberom.asm:408
    out DX, AL                                ; ee                          ; 0xc0772 vberom.asm:409
    mov dx, 003d5h                            ; ba d5 03                    ; 0xc0773 vberom.asm:410
    in AL, DX                                 ; ec                          ; 0xc0776 vberom.asm:411
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0777 vberom.asm:412
    out DX, AL                                ; ee                          ; 0xc0779 vberom.asm:413
    mov dx, 003dah                            ; ba da 03                    ; 0xc077a vberom.asm:414
    in AL, DX                                 ; ec                          ; 0xc077d vberom.asm:415
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc077e vberom.asm:416
    mov AL, strict byte 010h                  ; b0 10                       ; 0xc0781 vberom.asm:417
    out DX, AL                                ; ee                          ; 0xc0783 vberom.asm:418
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc0784 vberom.asm:419
    in AL, DX                                 ; ec                          ; 0xc0787 vberom.asm:420
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc0788 vberom.asm:421
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc078a vberom.asm:422
    out DX, AL                                ; ee                          ; 0xc078d vberom.asm:423
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc078e vberom.asm:424
    out DX, AL                                ; ee                          ; 0xc0790 vberom.asm:425
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc0791 vberom.asm:426
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc0794 vberom.asm:427
    out DX, AL                                ; ee                          ; 0xc0796 vberom.asm:428
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc0797 vberom.asm:429
    in AL, DX                                 ; ec                          ; 0xc079a vberom.asm:430
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc079b vberom.asm:431
    out DX, AL                                ; ee                          ; 0xc079d vberom.asm:432
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc079e vberom.asm:433
    mov AL, strict byte 005h                  ; b0 05                       ; 0xc07a1 vberom.asm:434
    out DX, AL                                ; ee                          ; 0xc07a3 vberom.asm:435
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc07a4 vberom.asm:436
    in AL, DX                                 ; ec                          ; 0xc07a7 vberom.asm:437
    and AL, strict byte 09fh                  ; 24 9f                       ; 0xc07a8 vberom.asm:438
    or AL, strict byte 040h                   ; 0c 40                       ; 0xc07aa vberom.asm:439
    out DX, AL                                ; ee                          ; 0xc07ac vberom.asm:440
    pop dx                                    ; 5a                          ; 0xc07ad vberom.asm:443
    pop ax                                    ; 58                          ; 0xc07ae vberom.asm:444
  ; disGetNextSymbol 0xc07af LB 0x148 -> off=0x0 cb=0000000000000013 uValue=00000000000c07af '_vbe_has_vbe_display'
_vbe_has_vbe_display:                        ; 0xc07af LB 0x13
    push DS                                   ; 1e                          ; 0xc07af vberom.asm:450
    push bx                                   ; 53                          ; 0xc07b0 vberom.asm:451
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07b1 vberom.asm:452
    mov ds, ax                                ; 8e d8                       ; 0xc07b4 vberom.asm:453
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc07b6 vberom.asm:454
    mov al, byte [bx]                         ; 8a 07                       ; 0xc07b9 vberom.asm:455
    and AL, strict byte 001h                  ; 24 01                       ; 0xc07bb vberom.asm:456
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc07bd vberom.asm:457
    pop bx                                    ; 5b                          ; 0xc07bf vberom.asm:458
    pop DS                                    ; 1f                          ; 0xc07c0 vberom.asm:459
    retn                                      ; c3                          ; 0xc07c1 vberom.asm:460
  ; disGetNextSymbol 0xc07c2 LB 0x135 -> off=0x0 cb=0000000000000025 uValue=00000000000c07c2 'vbe_biosfn_return_current_mode'
vbe_biosfn_return_current_mode:              ; 0xc07c2 LB 0x25
    push DS                                   ; 1e                          ; 0xc07c2 vberom.asm:473
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc07c3 vberom.asm:474
    mov ds, ax                                ; 8e d8                       ; 0xc07c6 vberom.asm:475
    call 005e1h                               ; e8 16 fe                    ; 0xc07c8 vberom.asm:476
    and ax, strict byte 00001h                ; 83 e0 01                    ; 0xc07cb vberom.asm:477
    je short 007d9h                           ; 74 09                       ; 0xc07ce vberom.asm:478
    mov bx, 000bah                            ; bb ba 00                    ; 0xc07d0 vberom.asm:479
    mov ax, word [bx]                         ; 8b 07                       ; 0xc07d3 vberom.asm:480
    db  08bh, 0d8h
    ; mov bx, ax                                ; 8b d8                     ; 0xc07d5 vberom.asm:481
    jne short 007e2h                          ; 75 09                       ; 0xc07d7 vberom.asm:482
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc07d9 vberom.asm:484
    mov al, byte [bx]                         ; 8a 07                       ; 0xc07dc vberom.asm:485
    db  08ah, 0d8h
    ; mov bl, al                                ; 8a d8                     ; 0xc07de vberom.asm:486
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc07e0 vberom.asm:487
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc07e2 vberom.asm:489
    pop DS                                    ; 1f                          ; 0xc07e5 vberom.asm:490
    retn                                      ; c3                          ; 0xc07e6 vberom.asm:491
  ; disGetNextSymbol 0xc07e7 LB 0x110 -> off=0x0 cb=000000000000002d uValue=00000000000c07e7 'vbe_biosfn_display_window_control'
vbe_biosfn_display_window_control:           ; 0xc07e7 LB 0x2d
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc07e7 vberom.asm:515
    jne short 00810h                          ; 75 24                       ; 0xc07ea vberom.asm:516
    cmp bh, 001h                              ; 80 ff 01                    ; 0xc07ec vberom.asm:517
    je short 00807h                           ; 74 16                       ; 0xc07ef vberom.asm:518
    jc short 007f7h                           ; 72 04                       ; 0xc07f1 vberom.asm:519
    mov ax, 00100h                            ; b8 00 01                    ; 0xc07f3 vberom.asm:520
    retn                                      ; c3                          ; 0xc07f6 vberom.asm:521
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc07f7 vberom.asm:523
    call 005f3h                               ; e8 f7 fd                    ; 0xc07f9 vberom.asm:524
    call 00607h                               ; e8 08 fe                    ; 0xc07fc vberom.asm:525
    db  03bh, 0c2h
    ; cmp ax, dx                                ; 3b c2                     ; 0xc07ff vberom.asm:526
    jne short 00810h                          ; 75 0d                       ; 0xc0801 vberom.asm:527
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0803 vberom.asm:528
    retn                                      ; c3                          ; 0xc0806 vberom.asm:529
    call 00607h                               ; e8 fd fd                    ; 0xc0807 vberom.asm:531
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc080a vberom.asm:532
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc080c vberom.asm:533
    retn                                      ; c3                          ; 0xc080f vberom.asm:534
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc0810 vberom.asm:536
    retn                                      ; c3                          ; 0xc0813 vberom.asm:537
  ; disGetNextSymbol 0xc0814 LB 0xe3 -> off=0x0 cb=0000000000000034 uValue=00000000000c0814 'vbe_biosfn_set_get_display_start'
vbe_biosfn_set_get_display_start:            ; 0xc0814 LB 0x34
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc0814 vberom.asm:577
    je short 00824h                           ; 74 0b                       ; 0xc0817 vberom.asm:578
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0819 vberom.asm:579
    je short 00838h                           ; 74 1a                       ; 0xc081c vberom.asm:580
    jc short 0082ah                           ; 72 0a                       ; 0xc081e vberom.asm:581
    mov ax, 00100h                            ; b8 00 01                    ; 0xc0820 vberom.asm:582
    retn                                      ; c3                          ; 0xc0823 vberom.asm:583
    call 00579h                               ; e8 52 fd                    ; 0xc0824 vberom.asm:585
    call 0056ch                               ; e8 42 fd                    ; 0xc0827 vberom.asm:586
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc082a vberom.asm:588
    call 00659h                               ; e8 2a fe                    ; 0xc082c vberom.asm:589
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc082f vberom.asm:590
    call 0067fh                               ; e8 4b fe                    ; 0xc0831 vberom.asm:591
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0834 vberom.asm:592
    retn                                      ; c3                          ; 0xc0837 vberom.asm:593
    call 0066dh                               ; e8 32 fe                    ; 0xc0838 vberom.asm:595
    db  08bh, 0c8h
    ; mov cx, ax                                ; 8b c8                     ; 0xc083b vberom.asm:596
    call 00693h                               ; e8 53 fe                    ; 0xc083d vberom.asm:597
    db  08bh, 0d0h
    ; mov dx, ax                                ; 8b d0                     ; 0xc0840 vberom.asm:598
    db  032h, 0ffh
    ; xor bh, bh                                ; 32 ff                     ; 0xc0842 vberom.asm:599
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0844 vberom.asm:600
    retn                                      ; c3                          ; 0xc0847 vberom.asm:601
  ; disGetNextSymbol 0xc0848 LB 0xaf -> off=0x0 cb=0000000000000037 uValue=00000000000c0848 'vbe_biosfn_set_get_dac_palette_format'
vbe_biosfn_set_get_dac_palette_format:       ; 0xc0848 LB 0x37
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0848 vberom.asm:616
    je short 0086bh                           ; 74 1e                       ; 0xc084b vberom.asm:617
    jc short 00853h                           ; 72 04                       ; 0xc084d vberom.asm:618
    mov ax, 00100h                            ; b8 00 01                    ; 0xc084f vberom.asm:619
    retn                                      ; c3                          ; 0xc0852 vberom.asm:620
    call 005e1h                               ; e8 8b fd                    ; 0xc0853 vberom.asm:622
    cmp bh, 006h                              ; 80 ff 06                    ; 0xc0856 vberom.asm:623
    je short 00865h                           ; 74 0a                       ; 0xc0859 vberom.asm:624
    cmp bh, 008h                              ; 80 ff 08                    ; 0xc085b vberom.asm:625
    jne short 0087bh                          ; 75 1b                       ; 0xc085e vberom.asm:626
    or ax, strict byte 00020h                 ; 83 c8 20                    ; 0xc0860 vberom.asm:627
    jne short 00868h                          ; 75 03                       ; 0xc0863 vberom.asm:628
    and ax, strict byte 0ffdfh                ; 83 e0 df                    ; 0xc0865 vberom.asm:630
    call 005cdh                               ; e8 62 fd                    ; 0xc0868 vberom.asm:632
    mov BH, strict byte 006h                  ; b7 06                       ; 0xc086b vberom.asm:634
    call 005e1h                               ; e8 71 fd                    ; 0xc086d vberom.asm:635
    and ax, strict byte 00020h                ; 83 e0 20                    ; 0xc0870 vberom.asm:636
    je short 00877h                           ; 74 02                       ; 0xc0873 vberom.asm:637
    mov BH, strict byte 008h                  ; b7 08                       ; 0xc0875 vberom.asm:638
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc0877 vberom.asm:640
    retn                                      ; c3                          ; 0xc087a vberom.asm:641
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc087b vberom.asm:643
    retn                                      ; c3                          ; 0xc087e vberom.asm:644
  ; disGetNextSymbol 0xc087f LB 0x78 -> off=0x0 cb=0000000000000064 uValue=00000000000c087f 'vbe_biosfn_set_get_palette_data'
vbe_biosfn_set_get_palette_data:             ; 0xc087f LB 0x64
    test bl, bl                               ; 84 db                       ; 0xc087f vberom.asm:683
    je short 00892h                           ; 74 0f                       ; 0xc0881 vberom.asm:684
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc0883 vberom.asm:685
    je short 008bah                           ; 74 32                       ; 0xc0886 vberom.asm:686
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc0888 vberom.asm:687
    jbe short 008dfh                          ; 76 52                       ; 0xc088b vberom.asm:688
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc088d vberom.asm:689
    jne short 008dbh                          ; 75 49                       ; 0xc0890 vberom.asm:690
    pushad                                    ; 66 60                       ; 0xc0892 vberom.asm:131
    push DS                                   ; 1e                          ; 0xc0894 vberom.asm:696
    push ES                                   ; 06                          ; 0xc0895 vberom.asm:697
    pop DS                                    ; 1f                          ; 0xc0896 vberom.asm:698
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc0897 vberom.asm:699
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc0899 vberom.asm:700
    out DX, AL                                ; ee                          ; 0xc089c vberom.asm:701
    inc dx                                    ; 42                          ; 0xc089d vberom.asm:702
    db  08bh, 0f7h
    ; mov si, di                                ; 8b f7                     ; 0xc089e vberom.asm:703
    lodsd                                     ; 66 ad                       ; 0xc08a0 vberom.asm:706
    ror eax, 010h                             ; 66 c1 c8 10                 ; 0xc08a2 vberom.asm:707
    out DX, AL                                ; ee                          ; 0xc08a6 vberom.asm:708
    rol eax, 008h                             ; 66 c1 c0 08                 ; 0xc08a7 vberom.asm:709
    out DX, AL                                ; ee                          ; 0xc08ab vberom.asm:710
    rol eax, 008h                             ; 66 c1 c0 08                 ; 0xc08ac vberom.asm:711
    out DX, AL                                ; ee                          ; 0xc08b0 vberom.asm:712
    loop 008a0h                               ; e2 ed                       ; 0xc08b1 vberom.asm:723
    pop DS                                    ; 1f                          ; 0xc08b3 vberom.asm:724
    popad                                     ; 66 61                       ; 0xc08b4 vberom.asm:150
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08b6 vberom.asm:727
    retn                                      ; c3                          ; 0xc08b9 vberom.asm:728
    pushad                                    ; 66 60                       ; 0xc08ba vberom.asm:131
    db  08ah, 0c2h
    ; mov al, dl                                ; 8a c2                     ; 0xc08bc vberom.asm:732
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc08be vberom.asm:733
    out DX, AL                                ; ee                          ; 0xc08c1 vberom.asm:734
    add dl, 002h                              ; 80 c2 02                    ; 0xc08c2 vberom.asm:735
    db  066h, 033h, 0c0h
    ; xor eax, eax                              ; 66 33 c0                  ; 0xc08c5 vberom.asm:738
    in AL, DX                                 ; ec                          ; 0xc08c8 vberom.asm:739
    sal eax, 008h                             ; 66 c1 e0 08                 ; 0xc08c9 vberom.asm:740
    in AL, DX                                 ; ec                          ; 0xc08cd vberom.asm:741
    sal eax, 008h                             ; 66 c1 e0 08                 ; 0xc08ce vberom.asm:742
    in AL, DX                                 ; ec                          ; 0xc08d2 vberom.asm:743
    stosd                                     ; 66 ab                       ; 0xc08d3 vberom.asm:744
    loop 008c5h                               ; e2 ee                       ; 0xc08d5 vberom.asm:757
    popad                                     ; 66 61                       ; 0xc08d7 vberom.asm:150
    jmp short 008b6h                          ; eb db                       ; 0xc08d9 vberom.asm:759
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08db vberom.asm:762
    retn                                      ; c3                          ; 0xc08de vberom.asm:763
    mov ax, 0024fh                            ; b8 4f 02                    ; 0xc08df vberom.asm:765
    retn                                      ; c3                          ; 0xc08e2 vberom.asm:766
  ; disGetNextSymbol 0xc08e3 LB 0x14 -> off=0x0 cb=0000000000000014 uValue=00000000000c08e3 'vbe_biosfn_return_protected_mode_interface'
vbe_biosfn_return_protected_mode_interface: ; 0xc08e3 LB 0x14
    test bl, bl                               ; 84 db                       ; 0xc08e3 vberom.asm:780
    jne short 008f3h                          ; 75 0c                       ; 0xc08e5 vberom.asm:781
    push CS                                   ; 0e                          ; 0xc08e7 vberom.asm:782
    pop ES                                    ; 07                          ; 0xc08e8 vberom.asm:783
    mov di, 04600h                            ; bf 00 46                    ; 0xc08e9 vberom.asm:784
    mov cx, 00115h                            ; b9 15 01                    ; 0xc08ec vberom.asm:785
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc08ef vberom.asm:786
    retn                                      ; c3                          ; 0xc08f2 vberom.asm:787
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc08f3 vberom.asm:789
    retn                                      ; c3                          ; 0xc08f6 vberom.asm:790

  ; Padding 0x89 bytes at 0xc08f7
  times 137 db 0

section _TEXT progbits vstart=0x980 align=1 ; size=0x367a class=CODE group=AUTO
  ; disGetNextSymbol 0xc0980 LB 0x367a -> off=0x0 cb=000000000000001a uValue=00000000000c0980 'set_int_vector'
set_int_vector:                              ; 0xc0980 LB 0x1a
    push dx                                   ; 52                          ; 0xc0980 vgabios.c:88
    push bp                                   ; 55                          ; 0xc0981
    mov bp, sp                                ; 89 e5                       ; 0xc0982
    mov dx, bx                                ; 89 da                       ; 0xc0984
    movzx bx, al                              ; 0f b6 d8                    ; 0xc0986 vgabios.c:92
    sal bx, 002h                              ; c1 e3 02                    ; 0xc0989
    xor ax, ax                                ; 31 c0                       ; 0xc098c
    mov es, ax                                ; 8e c0                       ; 0xc098e
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc0990
    mov word [es:bx+002h], cx                 ; 26 89 4f 02                 ; 0xc0993
    pop bp                                    ; 5d                          ; 0xc0997 vgabios.c:93
    pop dx                                    ; 5a                          ; 0xc0998
    retn                                      ; c3                          ; 0xc0999
  ; disGetNextSymbol 0xc099a LB 0x3660 -> off=0x0 cb=000000000000001c uValue=00000000000c099a 'init_vga_card'
init_vga_card:                               ; 0xc099a LB 0x1c
    push bp                                   ; 55                          ; 0xc099a vgabios.c:144
    mov bp, sp                                ; 89 e5                       ; 0xc099b
    push dx                                   ; 52                          ; 0xc099d
    mov AL, strict byte 0c3h                  ; b0 c3                       ; 0xc099e vgabios.c:147
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc09a0
    out DX, AL                                ; ee                          ; 0xc09a3
    mov AL, strict byte 004h                  ; b0 04                       ; 0xc09a4 vgabios.c:150
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc09a6
    out DX, AL                                ; ee                          ; 0xc09a9
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc09aa vgabios.c:151
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc09ac
    out DX, AL                                ; ee                          ; 0xc09af
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc09b0 vgabios.c:156
    pop dx                                    ; 5a                          ; 0xc09b3
    pop bp                                    ; 5d                          ; 0xc09b4
    retn                                      ; c3                          ; 0xc09b5
  ; disGetNextSymbol 0xc09b6 LB 0x3644 -> off=0x0 cb=0000000000000032 uValue=00000000000c09b6 'init_bios_area'
init_bios_area:                              ; 0xc09b6 LB 0x32
    push bx                                   ; 53                          ; 0xc09b6 vgabios.c:165
    push bp                                   ; 55                          ; 0xc09b7
    mov bp, sp                                ; 89 e5                       ; 0xc09b8
    xor bx, bx                                ; 31 db                       ; 0xc09ba vgabios.c:169
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc09bc
    mov es, ax                                ; 8e c0                       ; 0xc09bf
    mov al, byte [es:bx+010h]                 ; 26 8a 47 10                 ; 0xc09c1 vgabios.c:172
    and AL, strict byte 0cfh                  ; 24 cf                       ; 0xc09c5
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc09c7
    mov byte [es:bx+010h], al                 ; 26 88 47 10                 ; 0xc09c9
    mov byte [es:bx+00085h], 010h             ; 26 c6 87 85 00 10           ; 0xc09cd vgabios.c:176
    mov word [es:bx+00087h], 0f960h           ; 26 c7 87 87 00 60 f9        ; 0xc09d3 vgabios.c:178
    mov byte [es:bx+00089h], 051h             ; 26 c6 87 89 00 51           ; 0xc09da vgabios.c:182
    mov byte [es:bx+065h], 009h               ; 26 c6 47 65 09              ; 0xc09e0 vgabios.c:184
    pop bp                                    ; 5d                          ; 0xc09e5 vgabios.c:185
    pop bx                                    ; 5b                          ; 0xc09e6
    retn                                      ; c3                          ; 0xc09e7
  ; disGetNextSymbol 0xc09e8 LB 0x3612 -> off=0x0 cb=000000000000002f uValue=00000000000c09e8 'vgabios_init_func'
vgabios_init_func:                           ; 0xc09e8 LB 0x2f
    push bp                                   ; 55                          ; 0xc09e8 vgabios.c:225
    mov bp, sp                                ; 89 e5                       ; 0xc09e9
    call 0099ah                               ; e8 ac ff                    ; 0xc09eb vgabios.c:227
    call 009b6h                               ; e8 c5 ff                    ; 0xc09ee vgabios.c:228
    call 039b4h                               ; e8 c0 2f                    ; 0xc09f1 vgabios.c:230
    mov bx, strict word 00022h                ; bb 22 00                    ; 0xc09f4 vgabios.c:232
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc09f7
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc09fa
    call 00980h                               ; e8 80 ff                    ; 0xc09fd
    mov bx, strict word 00022h                ; bb 22 00                    ; 0xc0a00 vgabios.c:233
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0a03
    mov ax, strict word 0006dh                ; b8 6d 00                    ; 0xc0a06
    call 00980h                               ; e8 74 ff                    ; 0xc0a09
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc0a0c vgabios.c:259
    db  032h, 0e4h
    ; xor ah, ah                                ; 32 e4                     ; 0xc0a0f
    int 010h                                  ; cd 10                       ; 0xc0a11
    mov sp, bp                                ; 89 ec                       ; 0xc0a13 vgabios.c:262
    pop bp                                    ; 5d                          ; 0xc0a15
    retf                                      ; cb                          ; 0xc0a16
  ; disGetNextSymbol 0xc0a17 LB 0x35e3 -> off=0x0 cb=000000000000003f uValue=00000000000c0a17 'vga_get_cursor_pos'
vga_get_cursor_pos:                          ; 0xc0a17 LB 0x3f
    push si                                   ; 56                          ; 0xc0a17 vgabios.c:331
    push di                                   ; 57                          ; 0xc0a18
    push bp                                   ; 55                          ; 0xc0a19
    mov bp, sp                                ; 89 e5                       ; 0xc0a1a
    mov si, dx                                ; 89 d6                       ; 0xc0a1c
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc0a1e vgabios.c:333
    jbe short 00a30h                          ; 76 0e                       ; 0xc0a20
    push SS                                   ; 16                          ; 0xc0a22 vgabios.c:334
    pop ES                                    ; 07                          ; 0xc0a23
    mov word [es:si], strict word 00000h      ; 26 c7 04 00 00              ; 0xc0a24
    mov word [es:bx], strict word 00000h      ; 26 c7 07 00 00              ; 0xc0a29 vgabios.c:335
    jmp short 00a52h                          ; eb 22                       ; 0xc0a2e vgabios.c:336
    mov di, strict word 00060h                ; bf 60 00                    ; 0xc0a30 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0a33
    mov es, dx                                ; 8e c2                       ; 0xc0a36
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0a38
    push SS                                   ; 16                          ; 0xc0a3b vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a3c
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc0a3d
    movzx si, al                              ; 0f b6 f0                    ; 0xc0a40 vgabios.c:339
    add si, si                                ; 01 f6                       ; 0xc0a43
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc0a45
    mov es, dx                                ; 8e c2                       ; 0xc0a48 vgabios.c:47
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc0a4a
    push SS                                   ; 16                          ; 0xc0a4d vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0a4e
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc0a4f
    pop bp                                    ; 5d                          ; 0xc0a52 vgabios.c:341
    pop di                                    ; 5f                          ; 0xc0a53
    pop si                                    ; 5e                          ; 0xc0a54
    retn                                      ; c3                          ; 0xc0a55
  ; disGetNextSymbol 0xc0a56 LB 0x35a4 -> off=0x0 cb=000000000000005d uValue=00000000000c0a56 'vga_find_glyph'
vga_find_glyph:                              ; 0xc0a56 LB 0x5d
    push bp                                   ; 55                          ; 0xc0a56 vgabios.c:344
    mov bp, sp                                ; 89 e5                       ; 0xc0a57
    push si                                   ; 56                          ; 0xc0a59
    push di                                   ; 57                          ; 0xc0a5a
    push ax                                   ; 50                          ; 0xc0a5b
    push ax                                   ; 50                          ; 0xc0a5c
    push dx                                   ; 52                          ; 0xc0a5d
    push bx                                   ; 53                          ; 0xc0a5e
    mov bl, cl                                ; 88 cb                       ; 0xc0a5f
    mov word [bp-006h], strict word 00000h    ; c7 46 fa 00 00              ; 0xc0a61 vgabios.c:346
    dec word [bp+004h]                        ; ff 4e 04                    ; 0xc0a66 vgabios.c:348
    cmp word [bp+004h], strict byte 0ffffh    ; 83 7e 04 ff                 ; 0xc0a69
    je short 00aa7h                           ; 74 38                       ; 0xc0a6d
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc0a6f vgabios.c:349
    mov dx, ss                                ; 8c d2                       ; 0xc0a73
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc0a75
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc0a78
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc0a7b
    push DS                                   ; 1e                          ; 0xc0a7e
    mov ds, dx                                ; 8e da                       ; 0xc0a7f
    rep cmpsb                                 ; f3 a6                       ; 0xc0a81
    pop DS                                    ; 1f                          ; 0xc0a83
    mov ax, strict word 00000h                ; b8 00 00                    ; 0xc0a84
    je near 00a8dh                            ; 0f 84 02 00                 ; 0xc0a87
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc0a8b
    test ax, ax                               ; 85 c0                       ; 0xc0a8d
    jne short 00a9ch                          ; 75 0b                       ; 0xc0a8f
    movzx ax, bl                              ; 0f b6 c3                    ; 0xc0a91 vgabios.c:350
    or ah, 080h                               ; 80 cc 80                    ; 0xc0a94
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0a97
    jmp short 00aa7h                          ; eb 0b                       ; 0xc0a9a vgabios.c:351
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc0a9c vgabios.c:353
    add word [bp-008h], ax                    ; 01 46 f8                    ; 0xc0aa0
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc0aa3 vgabios.c:354
    jmp short 00a66h                          ; eb bf                       ; 0xc0aa5 vgabios.c:355
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc0aa7 vgabios.c:357
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0aaa
    pop di                                    ; 5f                          ; 0xc0aad
    pop si                                    ; 5e                          ; 0xc0aae
    pop bp                                    ; 5d                          ; 0xc0aaf
    retn 00004h                               ; c2 04 00                    ; 0xc0ab0
  ; disGetNextSymbol 0xc0ab3 LB 0x3547 -> off=0x0 cb=0000000000000046 uValue=00000000000c0ab3 'vga_read_glyph_planar'
vga_read_glyph_planar:                       ; 0xc0ab3 LB 0x46
    push bp                                   ; 55                          ; 0xc0ab3 vgabios.c:359
    mov bp, sp                                ; 89 e5                       ; 0xc0ab4
    push si                                   ; 56                          ; 0xc0ab6
    push di                                   ; 57                          ; 0xc0ab7
    push ax                                   ; 50                          ; 0xc0ab8
    push ax                                   ; 50                          ; 0xc0ab9
    mov si, ax                                ; 89 c6                       ; 0xc0aba
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc0abc
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc0abf
    mov bx, cx                                ; 89 cb                       ; 0xc0ac2
    mov ax, 00805h                            ; b8 05 08                    ; 0xc0ac4 vgabios.c:366
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0ac7
    out DX, ax                                ; ef                          ; 0xc0aca
    dec byte [bp+004h]                        ; fe 4e 04                    ; 0xc0acb vgabios.c:368
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc0ace
    je short 00ae9h                           ; 74 15                       ; 0xc0ad2
    mov es, [bp-006h]                         ; 8e 46 fa                    ; 0xc0ad4 vgabios.c:369
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc0ad7
    not al                                    ; f6 d0                       ; 0xc0ada
    mov di, bx                                ; 89 df                       ; 0xc0adc
    inc bx                                    ; 43                          ; 0xc0ade
    push SS                                   ; 16                          ; 0xc0adf
    pop ES                                    ; 07                          ; 0xc0ae0
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0ae1
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc0ae4 vgabios.c:370
    jmp short 00acbh                          ; eb e2                       ; 0xc0ae7 vgabios.c:371
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc0ae9 vgabios.c:374
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0aec
    out DX, ax                                ; ef                          ; 0xc0aef
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0af0 vgabios.c:375
    pop di                                    ; 5f                          ; 0xc0af3
    pop si                                    ; 5e                          ; 0xc0af4
    pop bp                                    ; 5d                          ; 0xc0af5
    retn 00002h                               ; c2 02 00                    ; 0xc0af6
  ; disGetNextSymbol 0xc0af9 LB 0x3501 -> off=0x0 cb=000000000000002a uValue=00000000000c0af9 'vga_char_ofs_planar'
vga_char_ofs_planar:                         ; 0xc0af9 LB 0x2a
    push bp                                   ; 55                          ; 0xc0af9 vgabios.c:377
    mov bp, sp                                ; 89 e5                       ; 0xc0afa
    xor dh, dh                                ; 30 f6                       ; 0xc0afc vgabios.c:381
    imul bx, dx                               ; 0f af da                    ; 0xc0afe
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc0b01
    imul bx, dx                               ; 0f af da                    ; 0xc0b05
    xor ah, ah                                ; 30 e4                       ; 0xc0b08
    add ax, bx                                ; 01 d8                       ; 0xc0b0a
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc0b0c vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0b0f
    mov es, dx                                ; 8e c2                       ; 0xc0b12
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0b14
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc0b17 vgabios.c:48
    imul dx, bx                               ; 0f af d3                    ; 0xc0b1a
    add ax, dx                                ; 01 d0                       ; 0xc0b1d
    pop bp                                    ; 5d                          ; 0xc0b1f vgabios.c:385
    retn 00002h                               ; c2 02 00                    ; 0xc0b20
  ; disGetNextSymbol 0xc0b23 LB 0x34d7 -> off=0x0 cb=000000000000003e uValue=00000000000c0b23 'vga_read_char_planar'
vga_read_char_planar:                        ; 0xc0b23 LB 0x3e
    push bp                                   ; 55                          ; 0xc0b23 vgabios.c:387
    mov bp, sp                                ; 89 e5                       ; 0xc0b24
    push cx                                   ; 51                          ; 0xc0b26
    push si                                   ; 56                          ; 0xc0b27
    push di                                   ; 57                          ; 0xc0b28
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc0b29
    mov si, ax                                ; 89 c6                       ; 0xc0b2c
    mov ax, dx                                ; 89 d0                       ; 0xc0b2e
    movzx di, bl                              ; 0f b6 fb                    ; 0xc0b30 vgabios.c:391
    push di                                   ; 57                          ; 0xc0b33
    lea cx, [bp-016h]                         ; 8d 4e ea                    ; 0xc0b34
    mov bx, si                                ; 89 f3                       ; 0xc0b37
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0b39
    call 00ab3h                               ; e8 74 ff                    ; 0xc0b3c
    push di                                   ; 57                          ; 0xc0b3f vgabios.c:394
    push 00100h                               ; 68 00 01                    ; 0xc0b40
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0b43 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0b46
    mov es, ax                                ; 8e c0                       ; 0xc0b48
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0b4a
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0b4d
    xor cx, cx                                ; 31 c9                       ; 0xc0b51 vgabios.c:58
    lea bx, [bp-016h]                         ; 8d 5e ea                    ; 0xc0b53
    call 00a56h                               ; e8 fd fe                    ; 0xc0b56
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0b59 vgabios.c:395
    pop di                                    ; 5f                          ; 0xc0b5c
    pop si                                    ; 5e                          ; 0xc0b5d
    pop cx                                    ; 59                          ; 0xc0b5e
    pop bp                                    ; 5d                          ; 0xc0b5f
    retn                                      ; c3                          ; 0xc0b60
  ; disGetNextSymbol 0xc0b61 LB 0x3499 -> off=0x0 cb=000000000000001a uValue=00000000000c0b61 'vga_char_ofs_linear'
vga_char_ofs_linear:                         ; 0xc0b61 LB 0x1a
    push bp                                   ; 55                          ; 0xc0b61 vgabios.c:397
    mov bp, sp                                ; 89 e5                       ; 0xc0b62
    xor dh, dh                                ; 30 f6                       ; 0xc0b64 vgabios.c:401
    imul dx, bx                               ; 0f af d3                    ; 0xc0b66
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc0b69
    imul bx, dx                               ; 0f af da                    ; 0xc0b6d
    xor ah, ah                                ; 30 e4                       ; 0xc0b70
    add ax, bx                                ; 01 d8                       ; 0xc0b72
    sal ax, 003h                              ; c1 e0 03                    ; 0xc0b74 vgabios.c:402
    pop bp                                    ; 5d                          ; 0xc0b77 vgabios.c:404
    retn 00002h                               ; c2 02 00                    ; 0xc0b78
  ; disGetNextSymbol 0xc0b7b LB 0x347f -> off=0x0 cb=000000000000004b uValue=00000000000c0b7b 'vga_read_glyph_linear'
vga_read_glyph_linear:                       ; 0xc0b7b LB 0x4b
    push si                                   ; 56                          ; 0xc0b7b vgabios.c:406
    push di                                   ; 57                          ; 0xc0b7c
    enter 00004h, 000h                        ; c8 04 00 00                 ; 0xc0b7d
    mov si, ax                                ; 89 c6                       ; 0xc0b81
    mov word [bp-002h], dx                    ; 89 56 fe                    ; 0xc0b83
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc0b86
    mov bx, cx                                ; 89 cb                       ; 0xc0b89
    dec byte [bp+008h]                        ; fe 4e 08                    ; 0xc0b8b vgabios.c:412
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc0b8e
    je short 00bc0h                           ; 74 2c                       ; 0xc0b92
    xor dh, dh                                ; 30 f6                       ; 0xc0b94 vgabios.c:413
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc0b96 vgabios.c:414
    xor ax, ax                                ; 31 c0                       ; 0xc0b98 vgabios.c:415
    jmp short 00ba1h                          ; eb 05                       ; 0xc0b9a
    cmp ax, strict word 00008h                ; 3d 08 00                    ; 0xc0b9c
    jnl short 00bb5h                          ; 7d 14                       ; 0xc0b9f
    mov es, [bp-002h]                         ; 8e 46 fe                    ; 0xc0ba1 vgabios.c:416
    mov di, si                                ; 89 f7                       ; 0xc0ba4
    add di, ax                                ; 01 c7                       ; 0xc0ba6
    cmp byte [es:di], 000h                    ; 26 80 3d 00                 ; 0xc0ba8
    je short 00bb0h                           ; 74 02                       ; 0xc0bac
    or dh, dl                                 ; 08 d6                       ; 0xc0bae vgabios.c:417
    shr dl, 1                                 ; d0 ea                       ; 0xc0bb0 vgabios.c:418
    inc ax                                    ; 40                          ; 0xc0bb2 vgabios.c:419
    jmp short 00b9ch                          ; eb e7                       ; 0xc0bb3
    mov di, bx                                ; 89 df                       ; 0xc0bb5 vgabios.c:420
    inc bx                                    ; 43                          ; 0xc0bb7
    mov byte [ss:di], dh                      ; 36 88 35                    ; 0xc0bb8
    add si, word [bp-004h]                    ; 03 76 fc                    ; 0xc0bbb vgabios.c:421
    jmp short 00b8bh                          ; eb cb                       ; 0xc0bbe vgabios.c:422
    leave                                     ; c9                          ; 0xc0bc0 vgabios.c:423
    pop di                                    ; 5f                          ; 0xc0bc1
    pop si                                    ; 5e                          ; 0xc0bc2
    retn 00002h                               ; c2 02 00                    ; 0xc0bc3
  ; disGetNextSymbol 0xc0bc6 LB 0x3434 -> off=0x0 cb=000000000000003f uValue=00000000000c0bc6 'vga_read_char_linear'
vga_read_char_linear:                        ; 0xc0bc6 LB 0x3f
    push bp                                   ; 55                          ; 0xc0bc6 vgabios.c:425
    mov bp, sp                                ; 89 e5                       ; 0xc0bc7
    push cx                                   ; 51                          ; 0xc0bc9
    push si                                   ; 56                          ; 0xc0bca
    sub sp, strict byte 00010h                ; 83 ec 10                    ; 0xc0bcb
    mov cx, ax                                ; 89 c1                       ; 0xc0bce
    mov ax, dx                                ; 89 d0                       ; 0xc0bd0
    movzx si, bl                              ; 0f b6 f3                    ; 0xc0bd2 vgabios.c:429
    push si                                   ; 56                          ; 0xc0bd5
    mov bx, cx                                ; 89 cb                       ; 0xc0bd6
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0bd8
    lea cx, [bp-014h]                         ; 8d 4e ec                    ; 0xc0bdb
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc0bde
    call 00b7bh                               ; e8 97 ff                    ; 0xc0be1
    push si                                   ; 56                          ; 0xc0be4 vgabios.c:432
    push 00100h                               ; 68 00 01                    ; 0xc0be5
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0be8 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0beb
    mov es, ax                                ; 8e c0                       ; 0xc0bed
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0bef
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0bf2
    xor cx, cx                                ; 31 c9                       ; 0xc0bf6 vgabios.c:58
    lea bx, [bp-014h]                         ; 8d 5e ec                    ; 0xc0bf8
    call 00a56h                               ; e8 58 fe                    ; 0xc0bfb
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0bfe vgabios.c:433
    pop si                                    ; 5e                          ; 0xc0c01
    pop cx                                    ; 59                          ; 0xc0c02
    pop bp                                    ; 5d                          ; 0xc0c03
    retn                                      ; c3                          ; 0xc0c04
  ; disGetNextSymbol 0xc0c05 LB 0x33f5 -> off=0x0 cb=0000000000000035 uValue=00000000000c0c05 'vga_read_2bpp_char'
vga_read_2bpp_char:                          ; 0xc0c05 LB 0x35
    push bp                                   ; 55                          ; 0xc0c05 vgabios.c:435
    mov bp, sp                                ; 89 e5                       ; 0xc0c06
    push bx                                   ; 53                          ; 0xc0c08
    push cx                                   ; 51                          ; 0xc0c09
    mov bx, ax                                ; 89 c3                       ; 0xc0c0a
    mov es, dx                                ; 8e c2                       ; 0xc0c0c
    mov cx, 0c000h                            ; b9 00 c0                    ; 0xc0c0e vgabios.c:441
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc0c11 vgabios.c:442
    xor dl, dl                                ; 30 d2                       ; 0xc0c13 vgabios.c:443
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0c15 vgabios.c:444
    xchg ah, al                               ; 86 c4                       ; 0xc0c18
    xor bx, bx                                ; 31 db                       ; 0xc0c1a vgabios.c:446
    jmp short 00c23h                          ; eb 05                       ; 0xc0c1c
    cmp bx, strict byte 00008h                ; 83 fb 08                    ; 0xc0c1e
    jnl short 00c31h                          ; 7d 0e                       ; 0xc0c21
    test ax, cx                               ; 85 c8                       ; 0xc0c23 vgabios.c:447
    je short 00c29h                           ; 74 02                       ; 0xc0c25
    or dl, dh                                 ; 08 f2                       ; 0xc0c27 vgabios.c:448
    shr dh, 1                                 ; d0 ee                       ; 0xc0c29 vgabios.c:449
    shr cx, 002h                              ; c1 e9 02                    ; 0xc0c2b vgabios.c:450
    inc bx                                    ; 43                          ; 0xc0c2e vgabios.c:451
    jmp short 00c1eh                          ; eb ed                       ; 0xc0c2f
    mov al, dl                                ; 88 d0                       ; 0xc0c31 vgabios.c:453
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0c33
    pop cx                                    ; 59                          ; 0xc0c36
    pop bx                                    ; 5b                          ; 0xc0c37
    pop bp                                    ; 5d                          ; 0xc0c38
    retn                                      ; c3                          ; 0xc0c39
  ; disGetNextSymbol 0xc0c3a LB 0x33c0 -> off=0x0 cb=0000000000000084 uValue=00000000000c0c3a 'vga_read_glyph_cga'
vga_read_glyph_cga:                          ; 0xc0c3a LB 0x84
    push bp                                   ; 55                          ; 0xc0c3a vgabios.c:455
    mov bp, sp                                ; 89 e5                       ; 0xc0c3b
    push cx                                   ; 51                          ; 0xc0c3d
    push si                                   ; 56                          ; 0xc0c3e
    push di                                   ; 57                          ; 0xc0c3f
    push ax                                   ; 50                          ; 0xc0c40
    mov si, dx                                ; 89 d6                       ; 0xc0c41
    cmp bl, 006h                              ; 80 fb 06                    ; 0xc0c43 vgabios.c:463
    je short 00c82h                           ; 74 3a                       ; 0xc0c46
    mov bx, ax                                ; 89 c3                       ; 0xc0c48 vgabios.c:465
    add bx, ax                                ; 01 c3                       ; 0xc0c4a
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c4c
    xor cx, cx                                ; 31 c9                       ; 0xc0c51 vgabios.c:467
    jmp short 00c5ah                          ; eb 05                       ; 0xc0c53
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c55
    jnl short 00cb6h                          ; 7d 5c                       ; 0xc0c58
    mov ax, bx                                ; 89 d8                       ; 0xc0c5a vgabios.c:468
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c5c
    call 00c05h                               ; e8 a3 ff                    ; 0xc0c5f
    mov di, si                                ; 89 f7                       ; 0xc0c62
    inc si                                    ; 46                          ; 0xc0c64
    push SS                                   ; 16                          ; 0xc0c65
    pop ES                                    ; 07                          ; 0xc0c66
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c67
    lea ax, [bx+02000h]                       ; 8d 87 00 20                 ; 0xc0c6a vgabios.c:469
    mov dx, word [bp-008h]                    ; 8b 56 f8                    ; 0xc0c6e
    call 00c05h                               ; e8 91 ff                    ; 0xc0c71
    mov di, si                                ; 89 f7                       ; 0xc0c74
    inc si                                    ; 46                          ; 0xc0c76
    push SS                                   ; 16                          ; 0xc0c77
    pop ES                                    ; 07                          ; 0xc0c78
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c79
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0c7c vgabios.c:470
    inc cx                                    ; 41                          ; 0xc0c7f vgabios.c:471
    jmp short 00c55h                          ; eb d3                       ; 0xc0c80
    mov bx, ax                                ; 89 c3                       ; 0xc0c82 vgabios.c:473
    mov word [bp-008h], 0b800h                ; c7 46 f8 00 b8              ; 0xc0c84
    xor cx, cx                                ; 31 c9                       ; 0xc0c89 vgabios.c:474
    jmp short 00c92h                          ; eb 05                       ; 0xc0c8b
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc0c8d
    jnl short 00cb6h                          ; 7d 24                       ; 0xc0c90
    mov di, si                                ; 89 f7                       ; 0xc0c92 vgabios.c:475
    inc si                                    ; 46                          ; 0xc0c94
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0c95
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0c98
    push SS                                   ; 16                          ; 0xc0c9b
    pop ES                                    ; 07                          ; 0xc0c9c
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0c9d
    mov di, si                                ; 89 f7                       ; 0xc0ca0 vgabios.c:476
    inc si                                    ; 46                          ; 0xc0ca2
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc0ca3
    mov al, byte [es:bx+02000h]               ; 26 8a 87 00 20              ; 0xc0ca6
    push SS                                   ; 16                          ; 0xc0cab
    pop ES                                    ; 07                          ; 0xc0cac
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc0cad
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc0cb0 vgabios.c:477
    inc cx                                    ; 41                          ; 0xc0cb3 vgabios.c:478
    jmp short 00c8dh                          ; eb d7                       ; 0xc0cb4
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc0cb6 vgabios.c:480
    pop di                                    ; 5f                          ; 0xc0cb9
    pop si                                    ; 5e                          ; 0xc0cba
    pop cx                                    ; 59                          ; 0xc0cbb
    pop bp                                    ; 5d                          ; 0xc0cbc
    retn                                      ; c3                          ; 0xc0cbd
  ; disGetNextSymbol 0xc0cbe LB 0x333c -> off=0x0 cb=0000000000000011 uValue=00000000000c0cbe 'vga_char_ofs_cga'
vga_char_ofs_cga:                            ; 0xc0cbe LB 0x11
    push bp                                   ; 55                          ; 0xc0cbe vgabios.c:482
    mov bp, sp                                ; 89 e5                       ; 0xc0cbf
    xor dh, dh                                ; 30 f6                       ; 0xc0cc1 vgabios.c:487
    imul dx, bx                               ; 0f af d3                    ; 0xc0cc3
    sal dx, 002h                              ; c1 e2 02                    ; 0xc0cc6
    xor ah, ah                                ; 30 e4                       ; 0xc0cc9
    add ax, dx                                ; 01 d0                       ; 0xc0ccb
    pop bp                                    ; 5d                          ; 0xc0ccd vgabios.c:488
    retn                                      ; c3                          ; 0xc0cce
  ; disGetNextSymbol 0xc0ccf LB 0x332b -> off=0x0 cb=0000000000000065 uValue=00000000000c0ccf 'vga_read_char_cga'
vga_read_char_cga:                           ; 0xc0ccf LB 0x65
    push bp                                   ; 55                          ; 0xc0ccf vgabios.c:490
    mov bp, sp                                ; 89 e5                       ; 0xc0cd0
    push bx                                   ; 53                          ; 0xc0cd2
    push cx                                   ; 51                          ; 0xc0cd3
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc0cd4
    movzx bx, dl                              ; 0f b6 da                    ; 0xc0cd7 vgabios.c:496
    lea dx, [bp-00eh]                         ; 8d 56 f2                    ; 0xc0cda
    call 00c3ah                               ; e8 5a ff                    ; 0xc0cdd
    push strict byte 00008h                   ; 6a 08                       ; 0xc0ce0 vgabios.c:499
    push 00080h                               ; 68 80 00                    ; 0xc0ce2
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0ce5 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0ce8
    mov es, ax                                ; 8e c0                       ; 0xc0cea
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0cec
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0cef
    xor cx, cx                                ; 31 c9                       ; 0xc0cf3 vgabios.c:58
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0cf5
    call 00a56h                               ; e8 5b fd                    ; 0xc0cf8
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0cfb
    test ah, 080h                             ; f6 c4 80                    ; 0xc0cfe vgabios.c:501
    jne short 00d2ah                          ; 75 27                       ; 0xc0d01
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0d03 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0d06
    mov es, ax                                ; 8e c0                       ; 0xc0d08
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0d0a
    mov dx, word [es:bx+002h]                 ; 26 8b 57 02                 ; 0xc0d0d
    test dx, dx                               ; 85 d2                       ; 0xc0d11 vgabios.c:505
    jne short 00d19h                          ; 75 04                       ; 0xc0d13
    test ax, ax                               ; 85 c0                       ; 0xc0d15
    je short 00d2ah                           ; 74 11                       ; 0xc0d17
    push strict byte 00008h                   ; 6a 08                       ; 0xc0d19 vgabios.c:506
    push 00080h                               ; 68 80 00                    ; 0xc0d1b
    mov cx, 00080h                            ; b9 80 00                    ; 0xc0d1e
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc0d21
    call 00a56h                               ; e8 2f fd                    ; 0xc0d24
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc0d27
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc0d2a vgabios.c:509
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc0d2d
    pop cx                                    ; 59                          ; 0xc0d30
    pop bx                                    ; 5b                          ; 0xc0d31
    pop bp                                    ; 5d                          ; 0xc0d32
    retn                                      ; c3                          ; 0xc0d33
  ; disGetNextSymbol 0xc0d34 LB 0x32c6 -> off=0x0 cb=0000000000000127 uValue=00000000000c0d34 'vga_read_char_attr'
vga_read_char_attr:                          ; 0xc0d34 LB 0x127
    push bp                                   ; 55                          ; 0xc0d34 vgabios.c:511
    mov bp, sp                                ; 89 e5                       ; 0xc0d35
    push bx                                   ; 53                          ; 0xc0d37
    push cx                                   ; 51                          ; 0xc0d38
    push si                                   ; 56                          ; 0xc0d39
    push di                                   ; 57                          ; 0xc0d3a
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc0d3b
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0d3e
    mov si, dx                                ; 89 d6                       ; 0xc0d41
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0d43 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0d46
    mov es, ax                                ; 8e c0                       ; 0xc0d49
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0d4b
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc0d4e vgabios.c:38
    xor ah, ah                                ; 30 e4                       ; 0xc0d51 vgabios.c:519
    call 033a1h                               ; e8 4b 26                    ; 0xc0d53
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc0d56
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0d59 vgabios.c:520
    je near 00e52h                            ; 0f 84 f3 00                 ; 0xc0d5b
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc0d5f vgabios.c:524
    lea bx, [bp-018h]                         ; 8d 5e e8                    ; 0xc0d63
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc0d66
    mov ax, cx                                ; 89 c8                       ; 0xc0d69
    call 00a17h                               ; e8 a9 fc                    ; 0xc0d6b
    mov al, byte [bp-018h]                    ; 8a 46 e8                    ; 0xc0d6e vgabios.c:525
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc0d71
    mov ax, word [bp-018h]                    ; 8b 46 e8                    ; 0xc0d74 vgabios.c:526
    xor al, al                                ; 30 c0                       ; 0xc0d77
    shr ax, 008h                              ; c1 e8 08                    ; 0xc0d79
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc0d7c
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0d7f vgabios.c:37
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc0d82
    mov es, dx                                ; 8e c2                       ; 0xc0d85
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc0d87
    xor dh, dh                                ; 30 f6                       ; 0xc0d8a vgabios.c:38
    inc dx                                    ; 42                          ; 0xc0d8c
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc0d8d vgabios.c:47
    mov di, word [es:di]                      ; 26 8b 3d                    ; 0xc0d90
    mov word [bp-014h], di                    ; 89 7e ec                    ; 0xc0d93 vgabios.c:48
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc0d96 vgabios.c:532
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0d9a
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0d9d
    jne short 00ddah                          ; 75 36                       ; 0xc0da2
    imul dx, di                               ; 0f af d7                    ; 0xc0da4 vgabios.c:534
    add dx, dx                                ; 01 d2                       ; 0xc0da7
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc0da9
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0dac
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc0daf
    mov cx, word [bp-016h]                    ; 8b 4e ea                    ; 0xc0db3
    inc cx                                    ; 41                          ; 0xc0db6
    imul dx, cx                               ; 0f af d1                    ; 0xc0db7
    xor ah, ah                                ; 30 e4                       ; 0xc0dba
    imul di, ax                               ; 0f af f8                    ; 0xc0dbc
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0dbf
    add ax, di                                ; 01 f8                       ; 0xc0dc3
    add ax, ax                                ; 01 c0                       ; 0xc0dc5
    mov di, dx                                ; 89 d7                       ; 0xc0dc7
    add di, ax                                ; 01 c7                       ; 0xc0dc9
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc0dcb vgabios.c:45
    mov ax, word [es:di]                      ; 26 8b 05                    ; 0xc0dcf
    push SS                                   ; 16                          ; 0xc0dd2 vgabios.c:48
    pop ES                                    ; 07                          ; 0xc0dd3
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc0dd4
    jmp near 00e52h                           ; e9 78 00                    ; 0xc0dd7 vgabios.c:536
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc0dda vgabios.c:537
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc0dde
    je short 00e2eh                           ; 74 4b                       ; 0xc0de1
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc0de3
    jc short 00e52h                           ; 72 6a                       ; 0xc0de6
    jbe short 00df1h                          ; 76 07                       ; 0xc0de8
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc0dea
    jbe short 00e0ah                          ; 76 1b                       ; 0xc0ded
    jmp short 00e52h                          ; eb 61                       ; 0xc0def
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc0df1 vgabios.c:540
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0df5
    mov bx, word [bp-014h]                    ; 8b 5e ec                    ; 0xc0df9
    call 00cbeh                               ; e8 bf fe                    ; 0xc0dfc
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc0dff vgabios.c:541
    call 00ccfh                               ; e8 c9 fe                    ; 0xc0e03
    xor ah, ah                                ; 30 e4                       ; 0xc0e06
    jmp short 00dd2h                          ; eb c8                       ; 0xc0e08
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e0a vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0e0d
    xor dh, dh                                ; 30 f6                       ; 0xc0e10 vgabios.c:546
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0e12
    push dx                                   ; 52                          ; 0xc0e15
    movzx dx, al                              ; 0f b6 d0                    ; 0xc0e16
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0e19
    mov bx, di                                ; 89 fb                       ; 0xc0e1d
    call 00af9h                               ; e8 d7 fc                    ; 0xc0e1f
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0e22 vgabios.c:547
    mov dx, ax                                ; 89 c2                       ; 0xc0e25
    mov ax, di                                ; 89 f8                       ; 0xc0e27
    call 00b23h                               ; e8 f7 fc                    ; 0xc0e29
    jmp short 00e06h                          ; eb d8                       ; 0xc0e2c
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e2e vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0e31
    xor dh, dh                                ; 30 f6                       ; 0xc0e34 vgabios.c:551
    mov word [bp-016h], dx                    ; 89 56 ea                    ; 0xc0e36
    push dx                                   ; 52                          ; 0xc0e39
    movzx dx, al                              ; 0f b6 d0                    ; 0xc0e3a
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc0e3d
    mov bx, di                                ; 89 fb                       ; 0xc0e41
    call 00b61h                               ; e8 1b fd                    ; 0xc0e43
    mov bx, word [bp-016h]                    ; 8b 5e ea                    ; 0xc0e46 vgabios.c:552
    mov dx, ax                                ; 89 c2                       ; 0xc0e49
    mov ax, di                                ; 89 f8                       ; 0xc0e4b
    call 00bc6h                               ; e8 76 fd                    ; 0xc0e4d
    jmp short 00e06h                          ; eb b4                       ; 0xc0e50
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc0e52 vgabios.c:561
    pop di                                    ; 5f                          ; 0xc0e55
    pop si                                    ; 5e                          ; 0xc0e56
    pop cx                                    ; 59                          ; 0xc0e57
    pop bx                                    ; 5b                          ; 0xc0e58
    pop bp                                    ; 5d                          ; 0xc0e59
    retn                                      ; c3                          ; 0xc0e5a
  ; disGetNextSymbol 0xc0e5b LB 0x319f -> off=0x10 cb=0000000000000083 uValue=00000000000c0e6b 'vga_get_font_info'
    db  082h, 00eh, 0c7h, 00eh, 0cch, 00eh, 0d3h, 00eh, 0d8h, 00eh, 0ddh, 00eh, 0e2h, 00eh, 0e7h, 00eh
vga_get_font_info:                           ; 0xc0e6b LB 0x83
    push si                                   ; 56                          ; 0xc0e6b vgabios.c:563
    push di                                   ; 57                          ; 0xc0e6c
    push bp                                   ; 55                          ; 0xc0e6d
    mov bp, sp                                ; 89 e5                       ; 0xc0e6e
    mov di, dx                                ; 89 d7                       ; 0xc0e70
    mov si, bx                                ; 89 de                       ; 0xc0e72
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc0e74 vgabios.c:568
    jnbe short 00ec1h                         ; 77 48                       ; 0xc0e77
    mov bx, ax                                ; 89 c3                       ; 0xc0e79
    add bx, ax                                ; 01 c3                       ; 0xc0e7b
    jmp word [cs:bx+00e5bh]                   ; 2e ff a7 5b 0e              ; 0xc0e7d
    mov bx, strict word 0007ch                ; bb 7c 00                    ; 0xc0e82 vgabios.c:57
    xor ax, ax                                ; 31 c0                       ; 0xc0e85
    mov es, ax                                ; 8e c0                       ; 0xc0e87
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc0e89
    mov ax, word [es:bx+002h]                 ; 26 8b 47 02                 ; 0xc0e8c
    push SS                                   ; 16                          ; 0xc0e90 vgabios.c:571
    pop ES                                    ; 07                          ; 0xc0e91
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc0e92
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc0e95
    mov bx, 00085h                            ; bb 85 00                    ; 0xc0e98
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0e9b
    mov es, ax                                ; 8e c0                       ; 0xc0e9e
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0ea0
    xor ah, ah                                ; 30 e4                       ; 0xc0ea3
    push SS                                   ; 16                          ; 0xc0ea5
    pop ES                                    ; 07                          ; 0xc0ea6
    mov bx, cx                                ; 89 cb                       ; 0xc0ea7
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ea9
    mov bx, 00084h                            ; bb 84 00                    ; 0xc0eac
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0eaf
    mov es, ax                                ; 8e c0                       ; 0xc0eb2
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0eb4
    xor ah, ah                                ; 30 e4                       ; 0xc0eb7
    push SS                                   ; 16                          ; 0xc0eb9
    pop ES                                    ; 07                          ; 0xc0eba
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc0ebb
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc0ebe
    pop bp                                    ; 5d                          ; 0xc0ec1
    pop di                                    ; 5f                          ; 0xc0ec2
    pop si                                    ; 5e                          ; 0xc0ec3
    retn 00002h                               ; c2 02 00                    ; 0xc0ec4
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc0ec7 vgabios.c:57
    jmp short 00e85h                          ; eb b9                       ; 0xc0eca
    mov dx, 05d6ch                            ; ba 6c 5d                    ; 0xc0ecc vgabios.c:576
    mov ax, ds                                ; 8c d8                       ; 0xc0ecf
    jmp short 00e90h                          ; eb bd                       ; 0xc0ed1 vgabios.c:577
    mov dx, 0556ch                            ; ba 6c 55                    ; 0xc0ed3 vgabios.c:579
    jmp short 00ecfh                          ; eb f7                       ; 0xc0ed6
    mov dx, 0596ch                            ; ba 6c 59                    ; 0xc0ed8 vgabios.c:582
    jmp short 00ecfh                          ; eb f2                       ; 0xc0edb
    mov dx, 07b6ch                            ; ba 6c 7b                    ; 0xc0edd vgabios.c:585
    jmp short 00ecfh                          ; eb ed                       ; 0xc0ee0
    mov dx, 06b6ch                            ; ba 6c 6b                    ; 0xc0ee2 vgabios.c:588
    jmp short 00ecfh                          ; eb e8                       ; 0xc0ee5
    mov dx, 07c99h                            ; ba 99 7c                    ; 0xc0ee7 vgabios.c:591
    jmp short 00ecfh                          ; eb e3                       ; 0xc0eea
    jmp short 00ec1h                          ; eb d3                       ; 0xc0eec vgabios.c:597
  ; disGetNextSymbol 0xc0eee LB 0x310c -> off=0x0 cb=0000000000000156 uValue=00000000000c0eee 'vga_read_pixel'
vga_read_pixel:                              ; 0xc0eee LB 0x156
    push bp                                   ; 55                          ; 0xc0eee vgabios.c:610
    mov bp, sp                                ; 89 e5                       ; 0xc0eef
    push si                                   ; 56                          ; 0xc0ef1
    push di                                   ; 57                          ; 0xc0ef2
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc0ef3
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc0ef6
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc0ef9
    mov si, cx                                ; 89 ce                       ; 0xc0efc
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc0efe vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f01
    mov es, ax                                ; 8e c0                       ; 0xc0f04
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f06
    xor ah, ah                                ; 30 e4                       ; 0xc0f09 vgabios.c:617
    call 033a1h                               ; e8 93 24                    ; 0xc0f0b
    mov ah, al                                ; 88 c4                       ; 0xc0f0e
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc0f10 vgabios.c:618
    je near 0103dh                            ; 0f 84 27 01                 ; 0xc0f12
    movzx bx, al                              ; 0f b6 d8                    ; 0xc0f16 vgabios.c:620
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0f19
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc0f1c
    je near 0103dh                            ; 0f 84 18 01                 ; 0xc0f21
    mov ch, byte [bx+047b0h]                  ; 8a af b0 47                 ; 0xc0f25 vgabios.c:624
    cmp ch, 003h                              ; 80 fd 03                    ; 0xc0f29
    jc short 00f3fh                           ; 72 11                       ; 0xc0f2c
    jbe short 00f47h                          ; 76 17                       ; 0xc0f2e
    cmp ch, 005h                              ; 80 fd 05                    ; 0xc0f30
    je near 01016h                            ; 0f 84 df 00                 ; 0xc0f33
    cmp ch, 004h                              ; 80 fd 04                    ; 0xc0f37
    je short 00f47h                           ; 74 0b                       ; 0xc0f3a
    jmp near 01036h                           ; e9 f7 00                    ; 0xc0f3c
    cmp ch, 002h                              ; 80 fd 02                    ; 0xc0f3f
    je short 00fb2h                           ; 74 6e                       ; 0xc0f42
    jmp near 01036h                           ; e9 ef 00                    ; 0xc0f44
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc0f47 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc0f4a
    mov es, ax                                ; 8e c0                       ; 0xc0f4d
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc0f4f
    imul ax, word [bp-00ch]                   ; 0f af 46 f4                 ; 0xc0f52 vgabios.c:48
    mov bx, dx                                ; 89 d3                       ; 0xc0f56
    shr bx, 003h                              ; c1 eb 03                    ; 0xc0f58
    add bx, ax                                ; 01 c3                       ; 0xc0f5b
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc0f5d vgabios.c:47
    mov cx, word [es:di]                      ; 26 8b 0d                    ; 0xc0f60
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc0f63 vgabios.c:48
    imul ax, cx                               ; 0f af c1                    ; 0xc0f67
    add bx, ax                                ; 01 c3                       ; 0xc0f6a
    mov cl, dl                                ; 88 d1                       ; 0xc0f6c vgabios.c:629
    and cl, 007h                              ; 80 e1 07                    ; 0xc0f6e
    mov ax, 00080h                            ; b8 80 00                    ; 0xc0f71
    sar ax, CL                                ; d3 f8                       ; 0xc0f74
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc0f76
    xor ch, ch                                ; 30 ed                       ; 0xc0f79 vgabios.c:630
    mov byte [bp-006h], ch                    ; 88 6e fa                    ; 0xc0f7b vgabios.c:631
    jmp short 00f88h                          ; eb 08                       ; 0xc0f7e
    cmp byte [bp-006h], 004h                  ; 80 7e fa 04                 ; 0xc0f80
    jnc near 01038h                           ; 0f 83 b0 00                 ; 0xc0f84
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc0f88 vgabios.c:632
    sal ax, 008h                              ; c1 e0 08                    ; 0xc0f8c
    or AL, strict byte 004h                   ; 0c 04                       ; 0xc0f8f
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc0f91
    out DX, ax                                ; ef                          ; 0xc0f94
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc0f95 vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc0f98
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0f9a
    and al, byte [bp-008h]                    ; 22 46 f8                    ; 0xc0f9d vgabios.c:38
    test al, al                               ; 84 c0                       ; 0xc0fa0 vgabios.c:634
    jbe short 00fadh                          ; 76 09                       ; 0xc0fa2
    mov cl, byte [bp-006h]                    ; 8a 4e fa                    ; 0xc0fa4 vgabios.c:635
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc0fa7
    sal al, CL                                ; d2 e0                       ; 0xc0fa9
    or ch, al                                 ; 08 c5                       ; 0xc0fab
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc0fad vgabios.c:636
    jmp short 00f80h                          ; eb ce                       ; 0xc0fb0
    movzx cx, byte [bx+047b1h]                ; 0f b6 8f b1 47              ; 0xc0fb2 vgabios.c:639
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc0fb7
    sub bx, cx                                ; 29 cb                       ; 0xc0fba
    mov cx, bx                                ; 89 d9                       ; 0xc0fbc
    mov bx, dx                                ; 89 d3                       ; 0xc0fbe
    shr bx, CL                                ; d3 eb                       ; 0xc0fc0
    mov cx, bx                                ; 89 d9                       ; 0xc0fc2
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc0fc4
    shr bx, 1                                 ; d1 eb                       ; 0xc0fc7
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc0fc9
    add bx, cx                                ; 01 cb                       ; 0xc0fcc
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc0fce vgabios.c:640
    je short 00fd7h                           ; 74 03                       ; 0xc0fd2
    add bh, 020h                              ; 80 c7 20                    ; 0xc0fd4 vgabios.c:641
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc0fd7 vgabios.c:37
    mov es, cx                                ; 8e c1                       ; 0xc0fda
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc0fdc
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc0fdf vgabios.c:643
    sal bx, 003h                              ; c1 e3 03                    ; 0xc0fe2
    cmp byte [bx+047b1h], 002h                ; 80 bf b1 47 02              ; 0xc0fe5
    jne short 01001h                          ; 75 15                       ; 0xc0fea
    and dx, strict byte 00003h                ; 83 e2 03                    ; 0xc0fec vgabios.c:644
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc0fef
    sub cx, dx                                ; 29 d1                       ; 0xc0ff2
    add cx, cx                                ; 01 c9                       ; 0xc0ff4
    xor ah, ah                                ; 30 e4                       ; 0xc0ff6
    sar ax, CL                                ; d3 f8                       ; 0xc0ff8
    mov ch, al                                ; 88 c5                       ; 0xc0ffa
    and ch, 003h                              ; 80 e5 03                    ; 0xc0ffc
    jmp short 01038h                          ; eb 37                       ; 0xc0fff vgabios.c:645
    xor dh, dh                                ; 30 f6                       ; 0xc1001 vgabios.c:646
    and dl, 007h                              ; 80 e2 07                    ; 0xc1003
    mov cx, strict word 00007h                ; b9 07 00                    ; 0xc1006
    sub cx, dx                                ; 29 d1                       ; 0xc1009
    xor ah, ah                                ; 30 e4                       ; 0xc100b
    sar ax, CL                                ; d3 f8                       ; 0xc100d
    mov ch, al                                ; 88 c5                       ; 0xc100f
    and ch, 001h                              ; 80 e5 01                    ; 0xc1011
    jmp short 01038h                          ; eb 22                       ; 0xc1014 vgabios.c:647
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1016 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1019
    mov es, ax                                ; 8e c0                       ; 0xc101c
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc101e
    sal ax, 003h                              ; c1 e0 03                    ; 0xc1021 vgabios.c:48
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc1024
    imul bx, ax                               ; 0f af d8                    ; 0xc1027
    add bx, dx                                ; 01 d3                       ; 0xc102a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc102c vgabios.c:37
    mov es, ax                                ; 8e c0                       ; 0xc102f
    mov ch, byte [es:bx]                      ; 26 8a 2f                    ; 0xc1031
    jmp short 01038h                          ; eb 02                       ; 0xc1034 vgabios.c:651
    xor ch, ch                                ; 30 ed                       ; 0xc1036 vgabios.c:656
    push SS                                   ; 16                          ; 0xc1038 vgabios.c:658
    pop ES                                    ; 07                          ; 0xc1039
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc103a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc103d vgabios.c:659
    pop di                                    ; 5f                          ; 0xc1040
    pop si                                    ; 5e                          ; 0xc1041
    pop bp                                    ; 5d                          ; 0xc1042
    retn                                      ; c3                          ; 0xc1043
  ; disGetNextSymbol 0xc1044 LB 0x2fb6 -> off=0x0 cb=000000000000008c uValue=00000000000c1044 'biosfn_perform_gray_scale_summing'
biosfn_perform_gray_scale_summing:           ; 0xc1044 LB 0x8c
    push bp                                   ; 55                          ; 0xc1044 vgabios.c:664
    mov bp, sp                                ; 89 e5                       ; 0xc1045
    push bx                                   ; 53                          ; 0xc1047
    push cx                                   ; 51                          ; 0xc1048
    push si                                   ; 56                          ; 0xc1049
    push di                                   ; 57                          ; 0xc104a
    push ax                                   ; 50                          ; 0xc104b
    push ax                                   ; 50                          ; 0xc104c
    mov bx, ax                                ; 89 c3                       ; 0xc104d
    mov di, dx                                ; 89 d7                       ; 0xc104f
    mov dx, 003dah                            ; ba da 03                    ; 0xc1051 vgabios.c:669
    in AL, DX                                 ; ec                          ; 0xc1054
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1055
    xor al, al                                ; 30 c0                       ; 0xc1057 vgabios.c:670
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1059
    out DX, AL                                ; ee                          ; 0xc105c
    xor si, si                                ; 31 f6                       ; 0xc105d vgabios.c:672
    cmp si, di                                ; 39 fe                       ; 0xc105f
    jnc short 010b5h                          ; 73 52                       ; 0xc1061
    mov al, bl                                ; 88 d8                       ; 0xc1063 vgabios.c:675
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc1065
    out DX, AL                                ; ee                          ; 0xc1068
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc1069 vgabios.c:677
    in AL, DX                                 ; ec                          ; 0xc106c
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc106d
    mov cx, ax                                ; 89 c1                       ; 0xc106f
    in AL, DX                                 ; ec                          ; 0xc1071 vgabios.c:678
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1072
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc1074
    in AL, DX                                 ; ec                          ; 0xc1077 vgabios.c:679
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1078
    xor ch, ch                                ; 30 ed                       ; 0xc107a vgabios.c:682
    imul cx, cx, strict byte 0004dh           ; 6b c9 4d                    ; 0xc107c
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc107f
    movzx cx, byte [bp-00ch]                  ; 0f b6 4e f4                 ; 0xc1082
    imul cx, cx, 00097h                       ; 69 c9 97 00                 ; 0xc1086
    add cx, word [bp-00ah]                    ; 03 4e f6                    ; 0xc108a
    xor ah, ah                                ; 30 e4                       ; 0xc108d
    imul ax, ax, strict byte 0001ch           ; 6b c0 1c                    ; 0xc108f
    add cx, ax                                ; 01 c1                       ; 0xc1092
    add cx, 00080h                            ; 81 c1 80 00                 ; 0xc1094
    sar cx, 008h                              ; c1 f9 08                    ; 0xc1098
    cmp cx, strict byte 0003fh                ; 83 f9 3f                    ; 0xc109b vgabios.c:684
    jbe short 010a3h                          ; 76 03                       ; 0xc109e
    mov cx, strict word 0003fh                ; b9 3f 00                    ; 0xc10a0
    mov al, bl                                ; 88 d8                       ; 0xc10a3 vgabios.c:687
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc10a5
    out DX, AL                                ; ee                          ; 0xc10a8
    mov al, cl                                ; 88 c8                       ; 0xc10a9 vgabios.c:689
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc10ab
    out DX, AL                                ; ee                          ; 0xc10ae
    out DX, AL                                ; ee                          ; 0xc10af vgabios.c:690
    out DX, AL                                ; ee                          ; 0xc10b0 vgabios.c:691
    inc bx                                    ; 43                          ; 0xc10b1 vgabios.c:692
    inc si                                    ; 46                          ; 0xc10b2 vgabios.c:693
    jmp short 0105fh                          ; eb aa                       ; 0xc10b3
    mov dx, 003dah                            ; ba da 03                    ; 0xc10b5 vgabios.c:694
    in AL, DX                                 ; ec                          ; 0xc10b8
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10b9
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc10bb vgabios.c:695
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc10bd
    out DX, AL                                ; ee                          ; 0xc10c0
    mov dx, 003dah                            ; ba da 03                    ; 0xc10c1 vgabios.c:697
    in AL, DX                                 ; ec                          ; 0xc10c4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc10c5
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc10c7 vgabios.c:699
    pop di                                    ; 5f                          ; 0xc10ca
    pop si                                    ; 5e                          ; 0xc10cb
    pop cx                                    ; 59                          ; 0xc10cc
    pop bx                                    ; 5b                          ; 0xc10cd
    pop bp                                    ; 5d                          ; 0xc10ce
    retn                                      ; c3                          ; 0xc10cf
  ; disGetNextSymbol 0xc10d0 LB 0x2f2a -> off=0x0 cb=00000000000000f6 uValue=00000000000c10d0 'biosfn_set_cursor_shape'
biosfn_set_cursor_shape:                     ; 0xc10d0 LB 0xf6
    push bp                                   ; 55                          ; 0xc10d0 vgabios.c:702
    mov bp, sp                                ; 89 e5                       ; 0xc10d1
    push bx                                   ; 53                          ; 0xc10d3
    push cx                                   ; 51                          ; 0xc10d4
    push si                                   ; 56                          ; 0xc10d5
    push di                                   ; 57                          ; 0xc10d6
    push ax                                   ; 50                          ; 0xc10d7
    mov bl, al                                ; 88 c3                       ; 0xc10d8
    mov ah, dl                                ; 88 d4                       ; 0xc10da
    movzx cx, al                              ; 0f b6 c8                    ; 0xc10dc vgabios.c:708
    sal cx, 008h                              ; c1 e1 08                    ; 0xc10df
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc10e2
    add dx, cx                                ; 01 ca                       ; 0xc10e5
    mov si, strict word 00060h                ; be 60 00                    ; 0xc10e7 vgabios.c:52
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc10ea
    mov es, cx                                ; 8e c1                       ; 0xc10ed
    mov word [es:si], dx                      ; 26 89 14                    ; 0xc10ef
    mov si, 00087h                            ; be 87 00                    ; 0xc10f2 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc10f5
    test dl, 008h                             ; f6 c2 08                    ; 0xc10f8 vgabios.c:38
    jne near 0119bh                           ; 0f 85 9c 00                 ; 0xc10fb
    mov dl, al                                ; 88 c2                       ; 0xc10ff vgabios.c:714
    and dl, 060h                              ; 80 e2 60                    ; 0xc1101
    cmp dl, 020h                              ; 80 fa 20                    ; 0xc1104
    jne short 01110h                          ; 75 07                       ; 0xc1107
    mov BL, strict byte 01eh                  ; b3 1e                       ; 0xc1109 vgabios.c:716
    xor ah, ah                                ; 30 e4                       ; 0xc110b vgabios.c:717
    jmp near 0119bh                           ; e9 8b 00                    ; 0xc110d vgabios.c:718
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc1110 vgabios.c:37
    test dl, 001h                             ; f6 c2 01                    ; 0xc1113 vgabios.c:38
    jne near 0119bh                           ; 0f 85 81 00                 ; 0xc1116
    cmp bl, 020h                              ; 80 fb 20                    ; 0xc111a
    jnc near 0119bh                           ; 0f 83 7a 00                 ; 0xc111d
    cmp ah, 020h                              ; 80 fc 20                    ; 0xc1121
    jnc near 0119bh                           ; 0f 83 73 00                 ; 0xc1124
    mov si, 00085h                            ; be 85 00                    ; 0xc1128 vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc112b
    mov es, dx                                ; 8e c2                       ; 0xc112e
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc1130
    mov dx, cx                                ; 89 ca                       ; 0xc1133 vgabios.c:48
    cmp ah, bl                                ; 38 dc                       ; 0xc1135 vgabios.c:729
    jnc short 01145h                          ; 73 0c                       ; 0xc1137
    test ah, ah                               ; 84 e4                       ; 0xc1139 vgabios.c:731
    je short 0119bh                           ; 74 5e                       ; 0xc113b
    xor bl, bl                                ; 30 db                       ; 0xc113d vgabios.c:732
    mov ah, cl                                ; 88 cc                       ; 0xc113f vgabios.c:733
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc1141
    jmp short 0119bh                          ; eb 56                       ; 0xc1143 vgabios.c:735
    movzx si, ah                              ; 0f b6 f4                    ; 0xc1145 vgabios.c:736
    mov word [bp-00ah], si                    ; 89 76 f6                    ; 0xc1148
    movzx si, bl                              ; 0f b6 f3                    ; 0xc114b
    or si, word [bp-00ah]                     ; 0b 76 f6                    ; 0xc114e
    cmp si, cx                                ; 39 ce                       ; 0xc1151
    jnc short 01168h                          ; 73 13                       ; 0xc1153
    movzx di, ah                              ; 0f b6 fc                    ; 0xc1155
    mov si, cx                                ; 89 ce                       ; 0xc1158
    dec si                                    ; 4e                          ; 0xc115a
    cmp di, si                                ; 39 f7                       ; 0xc115b
    je short 0119bh                           ; 74 3c                       ; 0xc115d
    movzx si, bl                              ; 0f b6 f3                    ; 0xc115f
    dec cx                                    ; 49                          ; 0xc1162
    dec cx                                    ; 49                          ; 0xc1163
    cmp si, cx                                ; 39 ce                       ; 0xc1164
    je short 0119bh                           ; 74 33                       ; 0xc1166
    cmp ah, 003h                              ; 80 fc 03                    ; 0xc1168 vgabios.c:738
    jbe short 0119bh                          ; 76 2e                       ; 0xc116b
    movzx si, bl                              ; 0f b6 f3                    ; 0xc116d vgabios.c:739
    movzx di, ah                              ; 0f b6 fc                    ; 0xc1170
    inc si                                    ; 46                          ; 0xc1173
    inc si                                    ; 46                          ; 0xc1174
    mov cl, dl                                ; 88 d1                       ; 0xc1175
    db  0feh, 0c9h
    ; dec cl                                    ; fe c9                     ; 0xc1177
    cmp di, si                                ; 39 f7                       ; 0xc1179
    jnle short 01190h                         ; 7f 13                       ; 0xc117b
    sub bl, ah                                ; 28 e3                       ; 0xc117d vgabios.c:741
    add bl, dl                                ; 00 d3                       ; 0xc117f
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc1181
    mov ah, cl                                ; 88 cc                       ; 0xc1183 vgabios.c:742
    cmp dx, strict byte 0000eh                ; 83 fa 0e                    ; 0xc1185 vgabios.c:743
    jc short 0119bh                           ; 72 11                       ; 0xc1188
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc118a vgabios.c:745
    db  0feh, 0cbh
    ; dec bl                                    ; fe cb                     ; 0xc118c vgabios.c:746
    jmp short 0119bh                          ; eb 0b                       ; 0xc118e vgabios.c:748
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc1190
    jbe short 01199h                          ; 76 04                       ; 0xc1193
    shr dx, 1                                 ; d1 ea                       ; 0xc1195 vgabios.c:750
    mov bl, dl                                ; 88 d3                       ; 0xc1197
    mov ah, cl                                ; 88 cc                       ; 0xc1199 vgabios.c:754
    mov si, strict word 00063h                ; be 63 00                    ; 0xc119b vgabios.c:47
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc119e
    mov es, dx                                ; 8e c2                       ; 0xc11a1
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc11a3
    mov AL, strict byte 00ah                  ; b0 0a                       ; 0xc11a6 vgabios.c:765
    mov dx, cx                                ; 89 ca                       ; 0xc11a8
    out DX, AL                                ; ee                          ; 0xc11aa
    mov si, cx                                ; 89 ce                       ; 0xc11ab vgabios.c:766
    inc si                                    ; 46                          ; 0xc11ad
    mov al, bl                                ; 88 d8                       ; 0xc11ae
    mov dx, si                                ; 89 f2                       ; 0xc11b0
    out DX, AL                                ; ee                          ; 0xc11b2
    mov AL, strict byte 00bh                  ; b0 0b                       ; 0xc11b3 vgabios.c:767
    mov dx, cx                                ; 89 ca                       ; 0xc11b5
    out DX, AL                                ; ee                          ; 0xc11b7
    mov al, ah                                ; 88 e0                       ; 0xc11b8 vgabios.c:768
    mov dx, si                                ; 89 f2                       ; 0xc11ba
    out DX, AL                                ; ee                          ; 0xc11bc
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc11bd vgabios.c:769
    pop di                                    ; 5f                          ; 0xc11c0
    pop si                                    ; 5e                          ; 0xc11c1
    pop cx                                    ; 59                          ; 0xc11c2
    pop bx                                    ; 5b                          ; 0xc11c3
    pop bp                                    ; 5d                          ; 0xc11c4
    retn                                      ; c3                          ; 0xc11c5
  ; disGetNextSymbol 0xc11c6 LB 0x2e34 -> off=0x0 cb=0000000000000089 uValue=00000000000c11c6 'biosfn_set_cursor_pos'
biosfn_set_cursor_pos:                       ; 0xc11c6 LB 0x89
    push bp                                   ; 55                          ; 0xc11c6 vgabios.c:772
    mov bp, sp                                ; 89 e5                       ; 0xc11c7
    push bx                                   ; 53                          ; 0xc11c9
    push cx                                   ; 51                          ; 0xc11ca
    push si                                   ; 56                          ; 0xc11cb
    push ax                                   ; 50                          ; 0xc11cc
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc11cd vgabios.c:778
    jnbe short 01247h                         ; 77 76                       ; 0xc11cf
    movzx bx, al                              ; 0f b6 d8                    ; 0xc11d1 vgabios.c:781
    add bx, bx                                ; 01 db                       ; 0xc11d4
    add bx, strict byte 00050h                ; 83 c3 50                    ; 0xc11d6
    mov cx, strict word 00040h                ; b9 40 00                    ; 0xc11d9 vgabios.c:52
    mov es, cx                                ; 8e c1                       ; 0xc11dc
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc11de
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc11e1 vgabios.c:37
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc11e4
    cmp al, ah                                ; 38 e0                       ; 0xc11e7 vgabios.c:785
    jne short 01247h                          ; 75 5c                       ; 0xc11e9
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc11eb vgabios.c:47
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc11ee
    mov bx, 00084h                            ; bb 84 00                    ; 0xc11f1 vgabios.c:37
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc11f4
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc11f7 vgabios.c:38
    inc bx                                    ; 43                          ; 0xc11fa
    mov si, dx                                ; 89 d6                       ; 0xc11fb vgabios.c:791
    and si, 0ff00h                            ; 81 e6 00 ff                 ; 0xc11fd
    shr si, 008h                              ; c1 ee 08                    ; 0xc1201
    mov word [bp-008h], si                    ; 89 76 f8                    ; 0xc1204
    imul bx, cx                               ; 0f af d9                    ; 0xc1207 vgabios.c:794
    or bl, 0ffh                               ; 80 cb ff                    ; 0xc120a
    xor ah, ah                                ; 30 e4                       ; 0xc120d
    inc bx                                    ; 43                          ; 0xc120f
    imul ax, bx                               ; 0f af c3                    ; 0xc1210
    movzx si, dl                              ; 0f b6 f2                    ; 0xc1213
    add si, ax                                ; 01 c6                       ; 0xc1216
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1218
    imul ax, cx                               ; 0f af c1                    ; 0xc121c
    add si, ax                                ; 01 c6                       ; 0xc121f
    mov bx, strict word 00063h                ; bb 63 00                    ; 0xc1221 vgabios.c:47
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1224
    mov AL, strict byte 00eh                  ; b0 0e                       ; 0xc1227 vgabios.c:798
    mov dx, bx                                ; 89 da                       ; 0xc1229
    out DX, AL                                ; ee                          ; 0xc122b
    mov ax, si                                ; 89 f0                       ; 0xc122c vgabios.c:799
    xor al, al                                ; 30 c0                       ; 0xc122e
    shr ax, 008h                              ; c1 e8 08                    ; 0xc1230
    lea cx, [bx+001h]                         ; 8d 4f 01                    ; 0xc1233
    mov dx, cx                                ; 89 ca                       ; 0xc1236
    out DX, AL                                ; ee                          ; 0xc1238
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc1239 vgabios.c:800
    mov dx, bx                                ; 89 da                       ; 0xc123b
    out DX, AL                                ; ee                          ; 0xc123d
    and si, 000ffh                            ; 81 e6 ff 00                 ; 0xc123e vgabios.c:801
    mov ax, si                                ; 89 f0                       ; 0xc1242
    mov dx, cx                                ; 89 ca                       ; 0xc1244
    out DX, AL                                ; ee                          ; 0xc1246
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc1247 vgabios.c:803
    pop si                                    ; 5e                          ; 0xc124a
    pop cx                                    ; 59                          ; 0xc124b
    pop bx                                    ; 5b                          ; 0xc124c
    pop bp                                    ; 5d                          ; 0xc124d
    retn                                      ; c3                          ; 0xc124e
  ; disGetNextSymbol 0xc124f LB 0x2dab -> off=0x0 cb=00000000000000cd uValue=00000000000c124f 'biosfn_set_active_page'
biosfn_set_active_page:                      ; 0xc124f LB 0xcd
    push bp                                   ; 55                          ; 0xc124f vgabios.c:806
    mov bp, sp                                ; 89 e5                       ; 0xc1250
    push bx                                   ; 53                          ; 0xc1252
    push cx                                   ; 51                          ; 0xc1253
    push dx                                   ; 52                          ; 0xc1254
    push si                                   ; 56                          ; 0xc1255
    push di                                   ; 57                          ; 0xc1256
    push ax                                   ; 50                          ; 0xc1257
    push ax                                   ; 50                          ; 0xc1258
    mov cl, al                                ; 88 c1                       ; 0xc1259
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc125b vgabios.c:812
    jnbe near 01312h                          ; 0f 87 b1 00                 ; 0xc125d
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1261 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1264
    mov es, ax                                ; 8e c0                       ; 0xc1267
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1269
    xor ah, ah                                ; 30 e4                       ; 0xc126c vgabios.c:816
    call 033a1h                               ; e8 30 21                    ; 0xc126e
    mov ch, al                                ; 88 c5                       ; 0xc1271
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1273 vgabios.c:817
    je near 01312h                            ; 0f 84 99 00                 ; 0xc1275
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc1279 vgabios.c:820
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc127c
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc127f
    call 00a17h                               ; e8 92 f7                    ; 0xc1282
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc1285 vgabios.c:822
    mov si, bx                                ; 89 de                       ; 0xc1288
    sal si, 003h                              ; c1 e6 03                    ; 0xc128a
    cmp byte [si+047afh], 000h                ; 80 bc af 47 00              ; 0xc128d
    jne short 012c8h                          ; 75 34                       ; 0xc1292
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc1294 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1297
    mov es, ax                                ; 8e c0                       ; 0xc129a
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc129c
    mov bx, 00084h                            ; bb 84 00                    ; 0xc129f vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc12a2
    xor ah, ah                                ; 30 e4                       ; 0xc12a5 vgabios.c:38
    inc ax                                    ; 40                          ; 0xc12a7
    imul dx, ax                               ; 0f af d0                    ; 0xc12a8 vgabios.c:829
    mov ax, dx                                ; 89 d0                       ; 0xc12ab
    add ax, dx                                ; 01 d0                       ; 0xc12ad
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc12af
    mov bx, ax                                ; 89 c3                       ; 0xc12b1
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc12b3
    inc bx                                    ; 43                          ; 0xc12b6
    imul bx, ax                               ; 0f af d8                    ; 0xc12b7
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc12ba vgabios.c:52
    mov word [es:si], bx                      ; 26 89 1c                    ; 0xc12bd
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc12c0 vgabios.c:833
    mov bx, dx                                ; 89 d3                       ; 0xc12c3
    inc bx                                    ; 43                          ; 0xc12c5
    jmp short 012d7h                          ; eb 0f                       ; 0xc12c6 vgabios.c:835
    movzx bx, byte [bx+0482eh]                ; 0f b6 9f 2e 48              ; 0xc12c8 vgabios.c:837
    sal bx, 006h                              ; c1 e3 06                    ; 0xc12cd
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc12d0
    mov bx, word [bx+04845h]                  ; 8b 9f 45 48                 ; 0xc12d3
    imul bx, ax                               ; 0f af d8                    ; 0xc12d7
    mov si, strict word 00063h                ; be 63 00                    ; 0xc12da vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc12dd
    mov es, ax                                ; 8e c0                       ; 0xc12e0
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc12e2
    mov AL, strict byte 00ch                  ; b0 0c                       ; 0xc12e5 vgabios.c:842
    mov dx, si                                ; 89 f2                       ; 0xc12e7
    out DX, AL                                ; ee                          ; 0xc12e9
    mov ax, bx                                ; 89 d8                       ; 0xc12ea vgabios.c:843
    xor al, bl                                ; 30 d8                       ; 0xc12ec
    shr ax, 008h                              ; c1 e8 08                    ; 0xc12ee
    lea di, [si+001h]                         ; 8d 7c 01                    ; 0xc12f1
    mov dx, di                                ; 89 fa                       ; 0xc12f4
    out DX, AL                                ; ee                          ; 0xc12f6
    mov AL, strict byte 00dh                  ; b0 0d                       ; 0xc12f7 vgabios.c:844
    mov dx, si                                ; 89 f2                       ; 0xc12f9
    out DX, AL                                ; ee                          ; 0xc12fb
    xor bh, bh                                ; 30 ff                       ; 0xc12fc vgabios.c:845
    mov ax, bx                                ; 89 d8                       ; 0xc12fe
    mov dx, di                                ; 89 fa                       ; 0xc1300
    out DX, AL                                ; ee                          ; 0xc1302
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1303 vgabios.c:42
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc1306
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc1309 vgabios.c:855
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc130c
    call 011c6h                               ; e8 b4 fe                    ; 0xc130f
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc1312 vgabios.c:856
    pop di                                    ; 5f                          ; 0xc1315
    pop si                                    ; 5e                          ; 0xc1316
    pop dx                                    ; 5a                          ; 0xc1317
    pop cx                                    ; 59                          ; 0xc1318
    pop bx                                    ; 5b                          ; 0xc1319
    pop bp                                    ; 5d                          ; 0xc131a
    retn                                      ; c3                          ; 0xc131b
  ; disGetNextSymbol 0xc131c LB 0x2cde -> off=0x0 cb=0000000000000354 uValue=00000000000c131c 'biosfn_set_video_mode'
biosfn_set_video_mode:                       ; 0xc131c LB 0x354
    push bp                                   ; 55                          ; 0xc131c vgabios.c:876
    mov bp, sp                                ; 89 e5                       ; 0xc131d
    push bx                                   ; 53                          ; 0xc131f
    push cx                                   ; 51                          ; 0xc1320
    push dx                                   ; 52                          ; 0xc1321
    push si                                   ; 56                          ; 0xc1322
    push di                                   ; 57                          ; 0xc1323
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc1324
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc1327
    and AL, strict byte 080h                  ; 24 80                       ; 0xc132a vgabios.c:880
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc132c
    call 007afh                               ; e8 7d f4                    ; 0xc132f vgabios.c:888
    test ax, ax                               ; 85 c0                       ; 0xc1332
    je short 01342h                           ; 74 0c                       ; 0xc1334
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc1336 vgabios.c:890
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1338
    out DX, AL                                ; ee                          ; 0xc133b
    xor al, al                                ; 30 c0                       ; 0xc133c vgabios.c:891
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc133e
    out DX, AL                                ; ee                          ; 0xc1341
    and byte [bp-010h], 07fh                  ; 80 66 f0 7f                 ; 0xc1342 vgabios.c:896
    cmp byte [bp-010h], 007h                  ; 80 7e f0 07                 ; 0xc1346 vgabios.c:900
    jne short 01350h                          ; 75 04                       ; 0xc134a
    mov byte [bp-010h], 000h                  ; c6 46 f0 00                 ; 0xc134c vgabios.c:901
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1350 vgabios.c:904
    call 033a1h                               ; e8 4a 20                    ; 0xc1354
    mov byte [bp-00eh], al                    ; 88 46 f2                    ; 0xc1357
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc135a vgabios.c:910
    je near 01666h                            ; 0f 84 06 03                 ; 0xc135c
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1360 vgabios.c:913
    mov al, byte [bx+0482eh]                  ; 8a 87 2e 48                 ; 0xc1363
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc1367
    mov di, 00089h                            ; bf 89 00                    ; 0xc136a vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc136d
    mov es, ax                                ; 8e c0                       ; 0xc1370
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc1372
    mov ah, al                                ; 88 c4                       ; 0xc1375 vgabios.c:38
    test AL, strict byte 008h                 ; a8 08                       ; 0xc1377 vgabios.c:930
    jne near 01405h                           ; 0f 85 88 00                 ; 0xc1379
    sal bx, 003h                              ; c1 e3 03                    ; 0xc137d vgabios.c:932
    mov al, byte [bx+047b4h]                  ; 8a 87 b4 47                 ; 0xc1380
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc1384
    out DX, AL                                ; ee                          ; 0xc1387
    xor al, al                                ; 30 c0                       ; 0xc1388 vgabios.c:935
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc138a
    out DX, AL                                ; ee                          ; 0xc138d
    mov bl, byte [bx+047b5h]                  ; 8a 9f b5 47                 ; 0xc138e vgabios.c:938
    cmp bl, 001h                              ; 80 fb 01                    ; 0xc1392
    jc short 013a5h                           ; 72 0e                       ; 0xc1395
    jbe short 013aeh                          ; 76 15                       ; 0xc1397
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc1399
    je short 013b8h                           ; 74 1a                       ; 0xc139c
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc139e
    je short 013b3h                           ; 74 10                       ; 0xc13a1
    jmp short 013bbh                          ; eb 16                       ; 0xc13a3
    test bl, bl                               ; 84 db                       ; 0xc13a5
    jne short 013bbh                          ; 75 12                       ; 0xc13a7
    mov si, 04fc2h                            ; be c2 4f                    ; 0xc13a9 vgabios.c:940
    jmp short 013bbh                          ; eb 0d                       ; 0xc13ac vgabios.c:941
    mov si, 05082h                            ; be 82 50                    ; 0xc13ae vgabios.c:943
    jmp short 013bbh                          ; eb 08                       ; 0xc13b1 vgabios.c:944
    mov si, 05142h                            ; be 42 51                    ; 0xc13b3 vgabios.c:946
    jmp short 013bbh                          ; eb 03                       ; 0xc13b6 vgabios.c:947
    mov si, 05202h                            ; be 02 52                    ; 0xc13b8 vgabios.c:949
    xor cx, cx                                ; 31 c9                       ; 0xc13bb vgabios.c:953
    jmp short 013ceh                          ; eb 0f                       ; 0xc13bd
    xor al, al                                ; 30 c0                       ; 0xc13bf vgabios.c:960
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc13c1
    out DX, AL                                ; ee                          ; 0xc13c4
    out DX, AL                                ; ee                          ; 0xc13c5 vgabios.c:961
    out DX, AL                                ; ee                          ; 0xc13c6 vgabios.c:962
    inc cx                                    ; 41                          ; 0xc13c7 vgabios.c:964
    cmp cx, 00100h                            ; 81 f9 00 01                 ; 0xc13c8
    jnc short 013f8h                          ; 73 2a                       ; 0xc13cc
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc13ce
    sal bx, 003h                              ; c1 e3 03                    ; 0xc13d2
    movzx bx, byte [bx+047b5h]                ; 0f b6 9f b5 47              ; 0xc13d5
    movzx dx, byte [bx+0483eh]                ; 0f b6 97 3e 48              ; 0xc13da
    cmp cx, dx                                ; 39 d1                       ; 0xc13df
    jnbe short 013bfh                         ; 77 dc                       ; 0xc13e1
    imul bx, cx, strict byte 00003h           ; 6b d9 03                    ; 0xc13e3
    add bx, si                                ; 01 f3                       ; 0xc13e6
    mov al, byte [bx]                         ; 8a 07                       ; 0xc13e8
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc13ea
    out DX, AL                                ; ee                          ; 0xc13ed
    mov al, byte [bx+001h]                    ; 8a 47 01                    ; 0xc13ee
    out DX, AL                                ; ee                          ; 0xc13f1
    mov al, byte [bx+002h]                    ; 8a 47 02                    ; 0xc13f2
    out DX, AL                                ; ee                          ; 0xc13f5
    jmp short 013c7h                          ; eb cf                       ; 0xc13f6
    test ah, 002h                             ; f6 c4 02                    ; 0xc13f8 vgabios.c:965
    je short 01405h                           ; 74 08                       ; 0xc13fb
    mov dx, 00100h                            ; ba 00 01                    ; 0xc13fd vgabios.c:967
    xor ax, ax                                ; 31 c0                       ; 0xc1400
    call 01044h                               ; e8 3f fc                    ; 0xc1402
    mov dx, 003dah                            ; ba da 03                    ; 0xc1405 vgabios.c:972
    in AL, DX                                 ; ec                          ; 0xc1408
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1409
    xor cx, cx                                ; 31 c9                       ; 0xc140b vgabios.c:975
    jmp short 01414h                          ; eb 05                       ; 0xc140d
    cmp cx, strict byte 00013h                ; 83 f9 13                    ; 0xc140f
    jnbe short 0142bh                         ; 77 17                       ; 0xc1412
    mov al, cl                                ; 88 c8                       ; 0xc1414 vgabios.c:976
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc1416
    out DX, AL                                ; ee                          ; 0xc1419
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc141a vgabios.c:977
    sal bx, 006h                              ; c1 e3 06                    ; 0xc141e
    add bx, cx                                ; 01 cb                       ; 0xc1421
    mov al, byte [bx+04865h]                  ; 8a 87 65 48                 ; 0xc1423
    out DX, AL                                ; ee                          ; 0xc1427
    inc cx                                    ; 41                          ; 0xc1428 vgabios.c:978
    jmp short 0140fh                          ; eb e4                       ; 0xc1429
    mov AL, strict byte 014h                  ; b0 14                       ; 0xc142b vgabios.c:979
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc142d
    out DX, AL                                ; ee                          ; 0xc1430
    xor al, al                                ; 30 c0                       ; 0xc1431 vgabios.c:980
    out DX, AL                                ; ee                          ; 0xc1433
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1434 vgabios.c:983
    out DX, AL                                ; ee                          ; 0xc1437
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc1438 vgabios.c:984
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc143a
    out DX, AL                                ; ee                          ; 0xc143d
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc143e vgabios.c:985
    jmp short 01448h                          ; eb 05                       ; 0xc1441
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc1443
    jnbe short 01462h                         ; 77 1a                       ; 0xc1446
    mov al, cl                                ; 88 c8                       ; 0xc1448 vgabios.c:986
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc144a
    out DX, AL                                ; ee                          ; 0xc144d
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc144e vgabios.c:987
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1452
    add bx, cx                                ; 01 cb                       ; 0xc1455
    mov al, byte [bx+04846h]                  ; 8a 87 46 48                 ; 0xc1457
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc145b
    out DX, AL                                ; ee                          ; 0xc145e
    inc cx                                    ; 41                          ; 0xc145f vgabios.c:988
    jmp short 01443h                          ; eb e1                       ; 0xc1460
    xor cx, cx                                ; 31 c9                       ; 0xc1462 vgabios.c:991
    jmp short 0146bh                          ; eb 05                       ; 0xc1464
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc1466
    jnbe short 01485h                         ; 77 1a                       ; 0xc1469
    mov al, cl                                ; 88 c8                       ; 0xc146b vgabios.c:992
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc146d
    out DX, AL                                ; ee                          ; 0xc1470
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc1471 vgabios.c:993
    sal bx, 006h                              ; c1 e3 06                    ; 0xc1475
    add bx, cx                                ; 01 cb                       ; 0xc1478
    mov al, byte [bx+04879h]                  ; 8a 87 79 48                 ; 0xc147a
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc147e
    out DX, AL                                ; ee                          ; 0xc1481
    inc cx                                    ; 41                          ; 0xc1482 vgabios.c:994
    jmp short 01466h                          ; eb e1                       ; 0xc1483
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc1485 vgabios.c:997
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1489
    cmp byte [bx+047b0h], 001h                ; 80 bf b0 47 01              ; 0xc148c
    jne short 01498h                          ; 75 05                       ; 0xc1491
    mov dx, 003b4h                            ; ba b4 03                    ; 0xc1493
    jmp short 0149bh                          ; eb 03                       ; 0xc1496
    mov dx, 003d4h                            ; ba d4 03                    ; 0xc1498
    mov si, dx                                ; 89 d6                       ; 0xc149b
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc149d vgabios.c:1000
    out DX, ax                                ; ef                          ; 0xc14a0
    xor cx, cx                                ; 31 c9                       ; 0xc14a1 vgabios.c:1002
    jmp short 014aah                          ; eb 05                       ; 0xc14a3
    cmp cx, strict byte 00018h                ; 83 f9 18                    ; 0xc14a5
    jnbe short 014c5h                         ; 77 1b                       ; 0xc14a8
    mov al, cl                                ; 88 c8                       ; 0xc14aa vgabios.c:1003
    mov dx, si                                ; 89 f2                       ; 0xc14ac
    out DX, AL                                ; ee                          ; 0xc14ae
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc14af vgabios.c:1004
    sal bx, 006h                              ; c1 e3 06                    ; 0xc14b3
    mov di, bx                                ; 89 df                       ; 0xc14b6
    add di, cx                                ; 01 cf                       ; 0xc14b8
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc14ba
    mov al, byte [di+0484ch]                  ; 8a 85 4c 48                 ; 0xc14bd
    out DX, AL                                ; ee                          ; 0xc14c1
    inc cx                                    ; 41                          ; 0xc14c2 vgabios.c:1005
    jmp short 014a5h                          ; eb e0                       ; 0xc14c3
    mov al, byte [bx+0484bh]                  ; 8a 87 4b 48                 ; 0xc14c5 vgabios.c:1008
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc14c9
    out DX, AL                                ; ee                          ; 0xc14cc
    mov AL, strict byte 020h                  ; b0 20                       ; 0xc14cd vgabios.c:1011
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc14cf
    out DX, AL                                ; ee                          ; 0xc14d2
    mov dx, 003dah                            ; ba da 03                    ; 0xc14d3 vgabios.c:1012
    in AL, DX                                 ; ec                          ; 0xc14d6
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc14d7
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc14d9 vgabios.c:1014
    jne short 0153bh                          ; 75 5c                       ; 0xc14dd
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc14df vgabios.c:1016
    sal bx, 003h                              ; c1 e3 03                    ; 0xc14e3
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc14e6
    jne short 014ffh                          ; 75 12                       ; 0xc14eb
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc14ed vgabios.c:1018
    mov cx, 04000h                            ; b9 00 40                    ; 0xc14f1
    mov ax, 00720h                            ; b8 20 07                    ; 0xc14f4
    xor di, di                                ; 31 ff                       ; 0xc14f7
    jcxz 014fdh                               ; e3 02                       ; 0xc14f9
    rep stosw                                 ; f3 ab                       ; 0xc14fb
    jmp short 0153bh                          ; eb 3c                       ; 0xc14fd vgabios.c:1020
    cmp byte [bp-010h], 00dh                  ; 80 7e f0 0d                 ; 0xc14ff vgabios.c:1022
    jnc short 01516h                          ; 73 11                       ; 0xc1503
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1505 vgabios.c:1024
    mov cx, 04000h                            ; b9 00 40                    ; 0xc1509
    xor ax, ax                                ; 31 c0                       ; 0xc150c
    xor di, di                                ; 31 ff                       ; 0xc150e
    jcxz 01514h                               ; e3 02                       ; 0xc1510
    rep stosw                                 ; f3 ab                       ; 0xc1512
    jmp short 0153bh                          ; eb 25                       ; 0xc1514 vgabios.c:1026
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc1516 vgabios.c:1028
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1518
    out DX, AL                                ; ee                          ; 0xc151b
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc151c vgabios.c:1029
    in AL, DX                                 ; ec                          ; 0xc151f
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc1520
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1522
    mov AL, strict byte 00fh                  ; b0 0f                       ; 0xc1525 vgabios.c:1030
    out DX, AL                                ; ee                          ; 0xc1527
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1528 vgabios.c:1031
    mov cx, 08000h                            ; b9 00 80                    ; 0xc152c
    xor ax, ax                                ; 31 c0                       ; 0xc152f
    xor di, di                                ; 31 ff                       ; 0xc1531
    jcxz 01537h                               ; e3 02                       ; 0xc1533
    rep stosw                                 ; f3 ab                       ; 0xc1535
    mov al, byte [bp-014h]                    ; 8a 46 ec                    ; 0xc1537 vgabios.c:1032
    out DX, AL                                ; ee                          ; 0xc153a
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc153b vgabios.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc153e
    mov es, ax                                ; 8e c0                       ; 0xc1541
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc1543
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1546
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc1549 vgabios.c:1039
    sal bx, 006h                              ; c1 e3 06                    ; 0xc154d
    movzx ax, byte [bx+04842h]                ; 0f b6 87 42 48              ; 0xc1550
    mov di, strict word 0004ah                ; bf 4a 00                    ; 0xc1555 vgabios.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc1558
    mov ax, word [bx+04845h]                  ; 8b 87 45 48                 ; 0xc155b vgabios.c:50
    mov di, strict word 0004ch                ; bf 4c 00                    ; 0xc155f vgabios.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc1562
    mov di, strict word 00063h                ; bf 63 00                    ; 0xc1565 vgabios.c:52
    mov word [es:di], si                      ; 26 89 35                    ; 0xc1568
    mov al, byte [bx+04843h]                  ; 8a 87 43 48                 ; 0xc156b vgabios.c:40
    mov si, 00084h                            ; be 84 00                    ; 0xc156f vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc1572
    movzx ax, byte [bx+04844h]                ; 0f b6 87 44 48              ; 0xc1575 vgabios.c:1043
    mov bx, 00085h                            ; bb 85 00                    ; 0xc157a vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc157d
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc1580 vgabios.c:1044
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc1583
    mov bx, 00087h                            ; bb 87 00                    ; 0xc1585 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc1588
    mov bx, 00088h                            ; bb 88 00                    ; 0xc158b vgabios.c:42
    mov byte [es:bx], 0f9h                    ; 26 c6 07 f9                 ; 0xc158e
    mov bx, 00089h                            ; bb 89 00                    ; 0xc1592 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1595
    and AL, strict byte 07fh                  ; 24 7f                       ; 0xc1598 vgabios.c:38
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc159a vgabios.c:42
    mov bx, 0008ah                            ; bb 8a 00                    ; 0xc159d vgabios.c:42
    mov byte [es:bx], 008h                    ; 26 c6 07 08                 ; 0xc15a0
    mov ax, ds                                ; 8c d8                       ; 0xc15a4 vgabios.c:1050
    mov bx, 000a8h                            ; bb a8 00                    ; 0xc15a6 vgabios.c:62
    mov word [es:bx], 05550h                  ; 26 c7 07 50 55              ; 0xc15a9
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc15ae
    mov al, byte [bp-010h]                    ; 8a 46 f0                    ; 0xc15b2 vgabios.c:1052
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc15b5
    jnbe short 015dfh                         ; 77 26                       ; 0xc15b7
    movzx bx, al                              ; 0f b6 d8                    ; 0xc15b9 vgabios.c:1054
    mov al, byte [bx+07dddh]                  ; 8a 87 dd 7d                 ; 0xc15bc vgabios.c:40
    mov bx, strict word 00065h                ; bb 65 00                    ; 0xc15c0 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc15c3
    cmp byte [bp-010h], 006h                  ; 80 7e f0 06                 ; 0xc15c6 vgabios.c:1055
    jne short 015d1h                          ; 75 05                       ; 0xc15ca
    mov ax, strict word 0003fh                ; b8 3f 00                    ; 0xc15cc
    jmp short 015d4h                          ; eb 03                       ; 0xc15cf
    mov ax, strict word 00030h                ; b8 30 00                    ; 0xc15d1
    mov bx, strict word 00066h                ; bb 66 00                    ; 0xc15d4 vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc15d7
    mov es, dx                                ; 8e c2                       ; 0xc15da
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc15dc
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc15df vgabios.c:1059
    sal bx, 003h                              ; c1 e3 03                    ; 0xc15e3
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc15e6
    jne short 015f6h                          ; 75 09                       ; 0xc15eb
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc15ed vgabios.c:1061
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc15f0
    call 010d0h                               ; e8 da fa                    ; 0xc15f3
    xor cx, cx                                ; 31 c9                       ; 0xc15f6 vgabios.c:1065
    jmp short 015ffh                          ; eb 05                       ; 0xc15f8
    cmp cx, strict byte 00008h                ; 83 f9 08                    ; 0xc15fa
    jnc short 0160ah                          ; 73 0b                       ; 0xc15fd
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc15ff vgabios.c:1066
    xor dx, dx                                ; 31 d2                       ; 0xc1602
    call 011c6h                               ; e8 bf fb                    ; 0xc1604
    inc cx                                    ; 41                          ; 0xc1607
    jmp short 015fah                          ; eb f0                       ; 0xc1608
    xor ax, ax                                ; 31 c0                       ; 0xc160a vgabios.c:1069
    call 0124fh                               ; e8 40 fc                    ; 0xc160c
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc160f vgabios.c:1072
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1613
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc1616
    jne short 0162dh                          ; 75 10                       ; 0xc161b
    xor dx, dx                                ; 31 d2                       ; 0xc161d vgabios.c:1074
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc161f
    call 02ab5h                               ; e8 90 14                    ; 0xc1622
    xor bl, bl                                ; 30 db                       ; 0xc1625 vgabios.c:1075
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc1627
    mov AH, strict byte 011h                  ; b4 11                       ; 0xc1629
    int 06dh                                  ; cd 6d                       ; 0xc162b
    mov bx, 0596ch                            ; bb 6c 59                    ; 0xc162d vgabios.c:1079
    mov cx, ds                                ; 8c d9                       ; 0xc1630
    mov ax, strict word 0001fh                ; b8 1f 00                    ; 0xc1632
    call 00980h                               ; e8 48 f3                    ; 0xc1635
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc1638 vgabios.c:1081
    sal bx, 006h                              ; c1 e3 06                    ; 0xc163c
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc163f
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc1643
    je short 01661h                           ; 74 1a                       ; 0xc1645
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc1647
    je short 0165ch                           ; 74 11                       ; 0xc1649
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc164b
    jne short 01666h                          ; 75 17                       ; 0xc164d
    mov bx, 0556ch                            ; bb 6c 55                    ; 0xc164f vgabios.c:1083
    mov cx, ds                                ; 8c d9                       ; 0xc1652
    mov ax, strict word 00043h                ; b8 43 00                    ; 0xc1654
    call 00980h                               ; e8 26 f3                    ; 0xc1657
    jmp short 01666h                          ; eb 0a                       ; 0xc165a vgabios.c:1084
    mov bx, 05d6ch                            ; bb 6c 5d                    ; 0xc165c vgabios.c:1086
    jmp short 01652h                          ; eb f1                       ; 0xc165f
    mov bx, 06b6ch                            ; bb 6c 6b                    ; 0xc1661 vgabios.c:1089
    jmp short 01652h                          ; eb ec                       ; 0xc1664
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc1666 vgabios.c:1092
    pop di                                    ; 5f                          ; 0xc1669
    pop si                                    ; 5e                          ; 0xc166a
    pop dx                                    ; 5a                          ; 0xc166b
    pop cx                                    ; 59                          ; 0xc166c
    pop bx                                    ; 5b                          ; 0xc166d
    pop bp                                    ; 5d                          ; 0xc166e
    retn                                      ; c3                          ; 0xc166f
  ; disGetNextSymbol 0xc1670 LB 0x298a -> off=0x0 cb=0000000000000075 uValue=00000000000c1670 'vgamem_copy_pl4'
vgamem_copy_pl4:                             ; 0xc1670 LB 0x75
    push bp                                   ; 55                          ; 0xc1670 vgabios.c:1095
    mov bp, sp                                ; 89 e5                       ; 0xc1671
    push si                                   ; 56                          ; 0xc1673
    push di                                   ; 57                          ; 0xc1674
    push ax                                   ; 50                          ; 0xc1675
    push ax                                   ; 50                          ; 0xc1676
    mov bh, cl                                ; 88 cf                       ; 0xc1677
    movzx di, dl                              ; 0f b6 fa                    ; 0xc1679 vgabios.c:1101
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc167c
    imul di, cx                               ; 0f af f9                    ; 0xc1680
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc1683
    imul di, si                               ; 0f af fe                    ; 0xc1687
    xor ah, ah                                ; 30 e4                       ; 0xc168a
    add di, ax                                ; 01 c7                       ; 0xc168c
    mov word [bp-008h], di                    ; 89 7e f8                    ; 0xc168e
    movzx di, bl                              ; 0f b6 fb                    ; 0xc1691 vgabios.c:1102
    imul cx, di                               ; 0f af cf                    ; 0xc1694
    imul cx, si                               ; 0f af ce                    ; 0xc1697
    add cx, ax                                ; 01 c1                       ; 0xc169a
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc169c
    mov ax, 00105h                            ; b8 05 01                    ; 0xc169f vgabios.c:1103
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc16a2
    out DX, ax                                ; ef                          ; 0xc16a5
    xor bl, bl                                ; 30 db                       ; 0xc16a6 vgabios.c:1104
    cmp bl, byte [bp+006h]                    ; 3a 5e 06                    ; 0xc16a8
    jnc short 016d5h                          ; 73 28                       ; 0xc16ab
    movzx cx, bh                              ; 0f b6 cf                    ; 0xc16ad vgabios.c:1106
    movzx si, bl                              ; 0f b6 f3                    ; 0xc16b0
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc16b3
    imul ax, si                               ; 0f af c6                    ; 0xc16b7
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc16ba
    add si, ax                                ; 01 c6                       ; 0xc16bd
    mov di, word [bp-006h]                    ; 8b 7e fa                    ; 0xc16bf
    add di, ax                                ; 01 c7                       ; 0xc16c2
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc16c4
    mov es, dx                                ; 8e c2                       ; 0xc16c7
    jcxz 016d1h                               ; e3 06                       ; 0xc16c9
    push DS                                   ; 1e                          ; 0xc16cb
    mov ds, dx                                ; 8e da                       ; 0xc16cc
    rep movsb                                 ; f3 a4                       ; 0xc16ce
    pop DS                                    ; 1f                          ; 0xc16d0
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc16d1 vgabios.c:1107
    jmp short 016a8h                          ; eb d3                       ; 0xc16d3
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc16d5 vgabios.c:1108
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc16d8
    out DX, ax                                ; ef                          ; 0xc16db
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc16dc vgabios.c:1109
    pop di                                    ; 5f                          ; 0xc16df
    pop si                                    ; 5e                          ; 0xc16e0
    pop bp                                    ; 5d                          ; 0xc16e1
    retn 00004h                               ; c2 04 00                    ; 0xc16e2
  ; disGetNextSymbol 0xc16e5 LB 0x2915 -> off=0x0 cb=0000000000000060 uValue=00000000000c16e5 'vgamem_fill_pl4'
vgamem_fill_pl4:                             ; 0xc16e5 LB 0x60
    push bp                                   ; 55                          ; 0xc16e5 vgabios.c:1112
    mov bp, sp                                ; 89 e5                       ; 0xc16e6
    push di                                   ; 57                          ; 0xc16e8
    push ax                                   ; 50                          ; 0xc16e9
    push ax                                   ; 50                          ; 0xc16ea
    mov byte [bp-004h], bl                    ; 88 5e fc                    ; 0xc16eb
    mov bh, cl                                ; 88 cf                       ; 0xc16ee
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc16f0 vgabios.c:1118
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc16f3
    imul cx, dx                               ; 0f af ca                    ; 0xc16f7
    movzx dx, bh                              ; 0f b6 d7                    ; 0xc16fa
    imul dx, cx                               ; 0f af d1                    ; 0xc16fd
    xor ah, ah                                ; 30 e4                       ; 0xc1700
    add dx, ax                                ; 01 c2                       ; 0xc1702
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc1704
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1707 vgabios.c:1119
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc170a
    out DX, ax                                ; ef                          ; 0xc170d
    xor bl, bl                                ; 30 db                       ; 0xc170e vgabios.c:1120
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1710
    jnc short 01736h                          ; 73 21                       ; 0xc1713
    movzx cx, byte [bp-004h]                  ; 0f b6 4e fc                 ; 0xc1715 vgabios.c:1122
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1719
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc171d
    movzx di, bh                              ; 0f b6 ff                    ; 0xc1720
    imul di, dx                               ; 0f af fa                    ; 0xc1723
    add di, word [bp-006h]                    ; 03 7e fa                    ; 0xc1726
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1729
    mov es, dx                                ; 8e c2                       ; 0xc172c
    jcxz 01732h                               ; e3 02                       ; 0xc172e
    rep stosb                                 ; f3 aa                       ; 0xc1730
    db  0feh, 0c3h
    ; inc bl                                    ; fe c3                     ; 0xc1732 vgabios.c:1123
    jmp short 01710h                          ; eb da                       ; 0xc1734
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1736 vgabios.c:1124
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1739
    out DX, ax                                ; ef                          ; 0xc173c
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc173d vgabios.c:1125
    pop di                                    ; 5f                          ; 0xc1740
    pop bp                                    ; 5d                          ; 0xc1741
    retn 00004h                               ; c2 04 00                    ; 0xc1742
  ; disGetNextSymbol 0xc1745 LB 0x28b5 -> off=0x0 cb=00000000000000a3 uValue=00000000000c1745 'vgamem_copy_cga'
vgamem_copy_cga:                             ; 0xc1745 LB 0xa3
    push bp                                   ; 55                          ; 0xc1745 vgabios.c:1128
    mov bp, sp                                ; 89 e5                       ; 0xc1746
    push si                                   ; 56                          ; 0xc1748
    push di                                   ; 57                          ; 0xc1749
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc174a
    mov dh, bl                                ; 88 de                       ; 0xc174d
    mov byte [bp-006h], cl                    ; 88 4e fa                    ; 0xc174f
    movzx di, dl                              ; 0f b6 fa                    ; 0xc1752 vgabios.c:1134
    movzx si, byte [bp+006h]                  ; 0f b6 76 06                 ; 0xc1755
    imul di, si                               ; 0f af fe                    ; 0xc1759
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc175c
    imul di, bx                               ; 0f af fb                    ; 0xc1760
    sar di, 1                                 ; d1 ff                       ; 0xc1763
    xor ah, ah                                ; 30 e4                       ; 0xc1765
    add di, ax                                ; 01 c7                       ; 0xc1767
    mov word [bp-00ch], di                    ; 89 7e f4                    ; 0xc1769
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc176c vgabios.c:1135
    imul dx, si                               ; 0f af d6                    ; 0xc176f
    imul dx, bx                               ; 0f af d3                    ; 0xc1772
    sar dx, 1                                 ; d1 fa                       ; 0xc1775
    add dx, ax                                ; 01 c2                       ; 0xc1777
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc1779
    mov byte [bp-008h], ah                    ; 88 66 f8                    ; 0xc177c vgabios.c:1136
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc177f
    cwd                                       ; 99                          ; 0xc1783
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1784
    sar ax, 1                                 ; d1 f8                       ; 0xc1786
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc1788
    cmp bx, ax                                ; 39 c3                       ; 0xc178c
    jnl short 017dfh                          ; 7d 4f                       ; 0xc178e
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc1790 vgabios.c:1138
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1794
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1797
    imul bx, ax                               ; 0f af d8                    ; 0xc179b
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc179e
    add si, bx                                ; 01 de                       ; 0xc17a1
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc17a3
    add di, bx                                ; 01 df                       ; 0xc17a6
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc17a8
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc17ab
    mov es, dx                                ; 8e c2                       ; 0xc17ae
    jcxz 017b8h                               ; e3 06                       ; 0xc17b0
    push DS                                   ; 1e                          ; 0xc17b2
    mov ds, dx                                ; 8e da                       ; 0xc17b3
    rep movsb                                 ; f3 a4                       ; 0xc17b5
    pop DS                                    ; 1f                          ; 0xc17b7
    mov si, word [bp-00ch]                    ; 8b 76 f4                    ; 0xc17b8 vgabios.c:1139
    add si, 02000h                            ; 81 c6 00 20                 ; 0xc17bb
    add si, bx                                ; 01 de                       ; 0xc17bf
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc17c1
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc17c4
    add di, bx                                ; 01 df                       ; 0xc17c8
    mov cx, word [bp-00eh]                    ; 8b 4e f2                    ; 0xc17ca
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc17cd
    mov es, dx                                ; 8e c2                       ; 0xc17d0
    jcxz 017dah                               ; e3 06                       ; 0xc17d2
    push DS                                   ; 1e                          ; 0xc17d4
    mov ds, dx                                ; 8e da                       ; 0xc17d5
    rep movsb                                 ; f3 a4                       ; 0xc17d7
    pop DS                                    ; 1f                          ; 0xc17d9
    inc byte [bp-008h]                        ; fe 46 f8                    ; 0xc17da vgabios.c:1140
    jmp short 0177fh                          ; eb a0                       ; 0xc17dd
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc17df vgabios.c:1141
    pop di                                    ; 5f                          ; 0xc17e2
    pop si                                    ; 5e                          ; 0xc17e3
    pop bp                                    ; 5d                          ; 0xc17e4
    retn 00004h                               ; c2 04 00                    ; 0xc17e5
  ; disGetNextSymbol 0xc17e8 LB 0x2812 -> off=0x0 cb=0000000000000081 uValue=00000000000c17e8 'vgamem_fill_cga'
vgamem_fill_cga:                             ; 0xc17e8 LB 0x81
    push bp                                   ; 55                          ; 0xc17e8 vgabios.c:1144
    mov bp, sp                                ; 89 e5                       ; 0xc17e9
    push si                                   ; 56                          ; 0xc17eb
    push di                                   ; 57                          ; 0xc17ec
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc17ed
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc17f0
    mov byte [bp-008h], cl                    ; 88 4e f8                    ; 0xc17f3
    movzx bx, dl                              ; 0f b6 da                    ; 0xc17f6 vgabios.c:1150
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc17f9
    imul bx, dx                               ; 0f af da                    ; 0xc17fd
    movzx dx, cl                              ; 0f b6 d1                    ; 0xc1800
    imul dx, bx                               ; 0f af d3                    ; 0xc1803
    sar dx, 1                                 ; d1 fa                       ; 0xc1806
    xor ah, ah                                ; 30 e4                       ; 0xc1808
    add dx, ax                                ; 01 c2                       ; 0xc180a
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc180c
    mov byte [bp-006h], ah                    ; 88 66 fa                    ; 0xc180f vgabios.c:1151
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1812
    cwd                                       ; 99                          ; 0xc1816
    db  02bh, 0c2h
    ; sub ax, dx                                ; 2b c2                     ; 0xc1817
    sar ax, 1                                 ; d1 f8                       ; 0xc1819
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc181b
    cmp dx, ax                                ; 39 c2                       ; 0xc181f
    jnl short 01860h                          ; 7d 3d                       ; 0xc1821
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6                 ; 0xc1823 vgabios.c:1153
    movzx bx, byte [bp+006h]                  ; 0f b6 5e 06                 ; 0xc1827
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc182b
    imul dx, ax                               ; 0f af d0                    ; 0xc182f
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc1832
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc1835
    add di, dx                                ; 01 d7                       ; 0xc1838
    mov cx, si                                ; 89 f1                       ; 0xc183a
    mov ax, bx                                ; 89 d8                       ; 0xc183c
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc183e
    mov es, dx                                ; 8e c2                       ; 0xc1841
    jcxz 01847h                               ; e3 02                       ; 0xc1843
    rep stosb                                 ; f3 aa                       ; 0xc1845
    mov di, word [bp-00ch]                    ; 8b 7e f4                    ; 0xc1847 vgabios.c:1154
    add di, 02000h                            ; 81 c7 00 20                 ; 0xc184a
    add di, word [bp-00eh]                    ; 03 7e f2                    ; 0xc184e
    mov cx, si                                ; 89 f1                       ; 0xc1851
    mov ax, bx                                ; 89 d8                       ; 0xc1853
    mov es, dx                                ; 8e c2                       ; 0xc1855
    jcxz 0185bh                               ; e3 02                       ; 0xc1857
    rep stosb                                 ; f3 aa                       ; 0xc1859
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc185b vgabios.c:1155
    jmp short 01812h                          ; eb b2                       ; 0xc185e
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1860 vgabios.c:1156
    pop di                                    ; 5f                          ; 0xc1863
    pop si                                    ; 5e                          ; 0xc1864
    pop bp                                    ; 5d                          ; 0xc1865
    retn 00004h                               ; c2 04 00                    ; 0xc1866
  ; disGetNextSymbol 0xc1869 LB 0x2791 -> off=0x0 cb=0000000000000079 uValue=00000000000c1869 'vgamem_copy_linear'
vgamem_copy_linear:                          ; 0xc1869 LB 0x79
    push bp                                   ; 55                          ; 0xc1869 vgabios.c:1159
    mov bp, sp                                ; 89 e5                       ; 0xc186a
    push si                                   ; 56                          ; 0xc186c
    push di                                   ; 57                          ; 0xc186d
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc186e
    mov ah, al                                ; 88 c4                       ; 0xc1871
    mov al, bl                                ; 88 d8                       ; 0xc1873
    mov bx, cx                                ; 89 cb                       ; 0xc1875
    xor dh, dh                                ; 30 f6                       ; 0xc1877 vgabios.c:1165
    movzx di, byte [bp+006h]                  ; 0f b6 7e 06                 ; 0xc1879
    imul dx, di                               ; 0f af d7                    ; 0xc187d
    imul dx, word [bp+004h]                   ; 0f af 56 04                 ; 0xc1880
    movzx si, ah                              ; 0f b6 f4                    ; 0xc1884
    add dx, si                                ; 01 f2                       ; 0xc1887
    sal dx, 003h                              ; c1 e2 03                    ; 0xc1889
    mov word [bp-008h], dx                    ; 89 56 f8                    ; 0xc188c
    xor ah, ah                                ; 30 e4                       ; 0xc188f vgabios.c:1166
    imul ax, di                               ; 0f af c7                    ; 0xc1891
    imul ax, word [bp+004h]                   ; 0f af 46 04                 ; 0xc1894
    add si, ax                                ; 01 c6                       ; 0xc1898
    sal si, 003h                              ; c1 e6 03                    ; 0xc189a
    mov word [bp-00ah], si                    ; 89 76 f6                    ; 0xc189d
    sal bx, 003h                              ; c1 e3 03                    ; 0xc18a0 vgabios.c:1167
    sal word [bp+004h], 003h                  ; c1 66 04 03                 ; 0xc18a3 vgabios.c:1168
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc18a7 vgabios.c:1169
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc18ab
    cmp al, byte [bp+006h]                    ; 3a 46 06                    ; 0xc18ae
    jnc short 018d9h                          ; 73 26                       ; 0xc18b1
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc18b3 vgabios.c:1171
    imul ax, word [bp+004h]                   ; 0f af 46 04                 ; 0xc18b7
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc18bb
    add si, ax                                ; 01 c6                       ; 0xc18be
    mov di, word [bp-00ah]                    ; 8b 7e f6                    ; 0xc18c0
    add di, ax                                ; 01 c7                       ; 0xc18c3
    mov cx, bx                                ; 89 d9                       ; 0xc18c5
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc18c7
    mov es, dx                                ; 8e c2                       ; 0xc18ca
    jcxz 018d4h                               ; e3 06                       ; 0xc18cc
    push DS                                   ; 1e                          ; 0xc18ce
    mov ds, dx                                ; 8e da                       ; 0xc18cf
    rep movsb                                 ; f3 a4                       ; 0xc18d1
    pop DS                                    ; 1f                          ; 0xc18d3
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc18d4 vgabios.c:1172
    jmp short 018abh                          ; eb d2                       ; 0xc18d7
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc18d9 vgabios.c:1173
    pop di                                    ; 5f                          ; 0xc18dc
    pop si                                    ; 5e                          ; 0xc18dd
    pop bp                                    ; 5d                          ; 0xc18de
    retn 00004h                               ; c2 04 00                    ; 0xc18df
  ; disGetNextSymbol 0xc18e2 LB 0x2718 -> off=0x0 cb=000000000000005c uValue=00000000000c18e2 'vgamem_fill_linear'
vgamem_fill_linear:                          ; 0xc18e2 LB 0x5c
    push bp                                   ; 55                          ; 0xc18e2 vgabios.c:1176
    mov bp, sp                                ; 89 e5                       ; 0xc18e3
    push si                                   ; 56                          ; 0xc18e5
    push di                                   ; 57                          ; 0xc18e6
    push ax                                   ; 50                          ; 0xc18e7
    push ax                                   ; 50                          ; 0xc18e8
    mov si, bx                                ; 89 de                       ; 0xc18e9
    mov bx, cx                                ; 89 cb                       ; 0xc18eb
    xor dh, dh                                ; 30 f6                       ; 0xc18ed vgabios.c:1182
    movzx di, byte [bp+004h]                  ; 0f b6 7e 04                 ; 0xc18ef
    imul dx, di                               ; 0f af d7                    ; 0xc18f3
    imul dx, cx                               ; 0f af d1                    ; 0xc18f6
    xor ah, ah                                ; 30 e4                       ; 0xc18f9
    add ax, dx                                ; 01 d0                       ; 0xc18fb
    sal ax, 003h                              ; c1 e0 03                    ; 0xc18fd
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc1900
    sal si, 003h                              ; c1 e6 03                    ; 0xc1903 vgabios.c:1183
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1906 vgabios.c:1184
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc1909 vgabios.c:1185
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc190d
    cmp al, byte [bp+004h]                    ; 3a 46 04                    ; 0xc1910
    jnc short 01935h                          ; 73 20                       ; 0xc1913
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc1915 vgabios.c:1187
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc1919
    imul dx, bx                               ; 0f af d3                    ; 0xc191d
    mov di, word [bp-008h]                    ; 8b 7e f8                    ; 0xc1920
    add di, dx                                ; 01 d7                       ; 0xc1923
    mov cx, si                                ; 89 f1                       ; 0xc1925
    mov dx, 0a000h                            ; ba 00 a0                    ; 0xc1927
    mov es, dx                                ; 8e c2                       ; 0xc192a
    jcxz 01930h                               ; e3 02                       ; 0xc192c
    rep stosb                                 ; f3 aa                       ; 0xc192e
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1930 vgabios.c:1188
    jmp short 0190dh                          ; eb d8                       ; 0xc1933
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1935 vgabios.c:1189
    pop di                                    ; 5f                          ; 0xc1938
    pop si                                    ; 5e                          ; 0xc1939
    pop bp                                    ; 5d                          ; 0xc193a
    retn 00004h                               ; c2 04 00                    ; 0xc193b
  ; disGetNextSymbol 0xc193e LB 0x26bc -> off=0x0 cb=0000000000000628 uValue=00000000000c193e 'biosfn_scroll'
biosfn_scroll:                               ; 0xc193e LB 0x628
    push bp                                   ; 55                          ; 0xc193e vgabios.c:1192
    mov bp, sp                                ; 89 e5                       ; 0xc193f
    push si                                   ; 56                          ; 0xc1941
    push di                                   ; 57                          ; 0xc1942
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc1943
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc1946
    mov byte [bp-012h], dl                    ; 88 56 ee                    ; 0xc1949
    mov byte [bp-00ch], bl                    ; 88 5e f4                    ; 0xc194c
    mov byte [bp-010h], cl                    ; 88 4e f0                    ; 0xc194f
    mov dh, byte [bp+006h]                    ; 8a 76 06                    ; 0xc1952
    cmp bl, byte [bp+004h]                    ; 3a 5e 04                    ; 0xc1955 vgabios.c:1201
    jnbe near 01f5dh                          ; 0f 87 01 06                 ; 0xc1958
    cmp dh, cl                                ; 38 ce                       ; 0xc195c vgabios.c:1202
    jc near 01f5dh                            ; 0f 82 fb 05                 ; 0xc195e
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc1962 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc1965
    mov es, ax                                ; 8e c0                       ; 0xc1968
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc196a
    xor ah, ah                                ; 30 e4                       ; 0xc196d vgabios.c:1206
    call 033a1h                               ; e8 2f 1a                    ; 0xc196f
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc1972
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc1975 vgabios.c:1207
    je near 01f5dh                            ; 0f 84 e2 05                 ; 0xc1977
    mov bx, 00084h                            ; bb 84 00                    ; 0xc197b vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc197e
    mov es, ax                                ; 8e c0                       ; 0xc1981
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc1983
    movzx cx, al                              ; 0f b6 c8                    ; 0xc1986 vgabios.c:38
    inc cx                                    ; 41                          ; 0xc1989
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc198a vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc198d
    mov word [bp-014h], ax                    ; 89 46 ec                    ; 0xc1990 vgabios.c:48
    cmp byte [bp+008h], 0ffh                  ; 80 7e 08 ff                 ; 0xc1993 vgabios.c:1214
    jne short 019a2h                          ; 75 09                       ; 0xc1997
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc1999 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc199c
    mov byte [bp+008h], al                    ; 88 46 08                    ; 0xc199f vgabios.c:38
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc19a2 vgabios.c:1217
    cmp ax, cx                                ; 39 c8                       ; 0xc19a6
    jc short 019b1h                           ; 72 07                       ; 0xc19a8
    mov al, cl                                ; 88 c8                       ; 0xc19aa
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc19ac
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc19ae
    movzx ax, dh                              ; 0f b6 c6                    ; 0xc19b1 vgabios.c:1218
    cmp ax, word [bp-014h]                    ; 3b 46 ec                    ; 0xc19b4
    jc short 019beh                           ; 72 05                       ; 0xc19b7
    mov dh, byte [bp-014h]                    ; 8a 76 ec                    ; 0xc19b9
    db  0feh, 0ceh
    ; dec dh                                    ; fe ce                     ; 0xc19bc
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc19be vgabios.c:1219
    cmp ax, cx                                ; 39 c8                       ; 0xc19c2
    jbe short 019cah                          ; 76 04                       ; 0xc19c4
    mov byte [bp-008h], 000h                  ; c6 46 f8 00                 ; 0xc19c6
    mov al, dh                                ; 88 f0                       ; 0xc19ca vgabios.c:1220
    sub al, byte [bp-010h]                    ; 2a 46 f0                    ; 0xc19cc
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc19cf
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc19d1
    movzx di, byte [bp-006h]                  ; 0f b6 7e fa                 ; 0xc19d4 vgabios.c:1222
    mov bx, di                                ; 89 fb                       ; 0xc19d8
    sal bx, 003h                              ; c1 e3 03                    ; 0xc19da
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc19dd
    dec ax                                    ; 48                          ; 0xc19e0
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc19e1
    mov ax, cx                                ; 89 c8                       ; 0xc19e4
    dec ax                                    ; 48                          ; 0xc19e6
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc19e7
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc19ea
    imul ax, cx                               ; 0f af c1                    ; 0xc19ed
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc19f0
    jne near 01b94h                           ; 0f 85 9b 01                 ; 0xc19f5
    mov cx, ax                                ; 89 c1                       ; 0xc19f9 vgabios.c:1225
    add cx, ax                                ; 01 c1                       ; 0xc19fb
    or cl, 0ffh                               ; 80 c9 ff                    ; 0xc19fd
    movzx si, byte [bp+008h]                  ; 0f b6 76 08                 ; 0xc1a00
    inc cx                                    ; 41                          ; 0xc1a04
    imul cx, si                               ; 0f af ce                    ; 0xc1a05
    mov word [bp-01ch], cx                    ; 89 4e e4                    ; 0xc1a08
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1a0b vgabios.c:1230
    jne short 01a4ch                          ; 75 3b                       ; 0xc1a0f
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1a11
    jne short 01a4ch                          ; 75 35                       ; 0xc1a15
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1a17
    jne short 01a4ch                          ; 75 2f                       ; 0xc1a1b
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1a1d
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc1a21
    jne short 01a4ch                          ; 75 26                       ; 0xc1a24
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc1a26
    cmp dx, word [bp-018h]                    ; 3b 56 e8                    ; 0xc1a29
    jne short 01a4ch                          ; 75 1e                       ; 0xc1a2c
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc1a2e vgabios.c:1232
    sal dx, 008h                              ; c1 e2 08                    ; 0xc1a32
    add dx, strict byte 00020h                ; 83 c2 20                    ; 0xc1a35
    mov bx, word [bx+047b2h]                  ; 8b 9f b2 47                 ; 0xc1a38
    mov cx, ax                                ; 89 c1                       ; 0xc1a3c
    mov ax, dx                                ; 89 d0                       ; 0xc1a3e
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1a40
    mov es, bx                                ; 8e c3                       ; 0xc1a43
    jcxz 01a49h                               ; e3 02                       ; 0xc1a45
    rep stosw                                 ; f3 ab                       ; 0xc1a47
    jmp near 01f5dh                           ; e9 11 05                    ; 0xc1a49 vgabios.c:1234
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1a4c vgabios.c:1236
    jne near 01ae9h                           ; 0f 85 95 00                 ; 0xc1a50
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1a54 vgabios.c:1237
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1a58
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc1a5b
    cmp dx, word [bp-01ah]                    ; 3b 56 e6                    ; 0xc1a5f
    jc near 01f5dh                            ; 0f 82 f7 04                 ; 0xc1a62
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1a66 vgabios.c:1239
    add ax, word [bp-01ah]                    ; 03 46 e6                    ; 0xc1a6a
    cmp ax, dx                                ; 39 d0                       ; 0xc1a6d
    jnbe short 01a77h                         ; 77 06                       ; 0xc1a6f
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1a71
    jne short 01aaah                          ; 75 33                       ; 0xc1a75
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1a77 vgabios.c:1240
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1a7b
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1a7f
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1a82
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1a85
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1a88
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1a8c
    add dx, bx                                ; 01 da                       ; 0xc1a90
    add dx, dx                                ; 01 d2                       ; 0xc1a92
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1a94
    add di, dx                                ; 01 d7                       ; 0xc1a97
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1a99
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1a9d
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1aa0
    jcxz 01aa8h                               ; e3 02                       ; 0xc1aa4
    rep stosw                                 ; f3 ab                       ; 0xc1aa6
    jmp short 01ae3h                          ; eb 39                       ; 0xc1aa8 vgabios.c:1241
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1aaa vgabios.c:1242
    mov si, ax                                ; 89 c6                       ; 0xc1aae
    imul si, word [bp-014h]                   ; 0f af 76 ec                 ; 0xc1ab0
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1ab4
    add si, dx                                ; 01 d6                       ; 0xc1ab8
    add si, si                                ; 01 f6                       ; 0xc1aba
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1abc
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1ac0
    mov ax, word [bx+047b2h]                  ; 8b 87 b2 47                 ; 0xc1ac3
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1ac7
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1aca
    mov di, dx                                ; 89 d7                       ; 0xc1ace
    add di, bx                                ; 01 df                       ; 0xc1ad0
    add di, di                                ; 01 ff                       ; 0xc1ad2
    add di, word [bp-01ch]                    ; 03 7e e4                    ; 0xc1ad4
    mov dx, ax                                ; 89 c2                       ; 0xc1ad7
    mov es, ax                                ; 8e c0                       ; 0xc1ad9
    jcxz 01ae3h                               ; e3 06                       ; 0xc1adb
    push DS                                   ; 1e                          ; 0xc1add
    mov ds, dx                                ; 8e da                       ; 0xc1ade
    rep movsw                                 ; f3 a5                       ; 0xc1ae0
    pop DS                                    ; 1f                          ; 0xc1ae2
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1ae3 vgabios.c:1243
    jmp near 01a5bh                           ; e9 72 ff                    ; 0xc1ae6
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1ae9 vgabios.c:1246
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1aed
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1af0
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1af4
    jnbe near 01f5dh                          ; 0f 87 62 04                 ; 0xc1af7
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1afb vgabios.c:1248
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1aff
    add ax, dx                                ; 01 d0                       ; 0xc1b03
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1b05
    jnbe short 01b10h                         ; 77 06                       ; 0xc1b08
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1b0a
    jne short 01b43h                          ; 75 33                       ; 0xc1b0e
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1b10 vgabios.c:1249
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1b14
    sal ax, 008h                              ; c1 e0 08                    ; 0xc1b18
    add ax, strict word 00020h                ; 05 20 00                    ; 0xc1b1b
    mov dx, word [bp-01ah]                    ; 8b 56 e6                    ; 0xc1b1e
    imul dx, word [bp-014h]                   ; 0f af 56 ec                 ; 0xc1b21
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc1b25
    add dx, bx                                ; 01 da                       ; 0xc1b29
    add dx, dx                                ; 01 d2                       ; 0xc1b2b
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1b2d
    add di, dx                                ; 01 d7                       ; 0xc1b30
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1b32
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1b36
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1b39
    jcxz 01b41h                               ; e3 02                       ; 0xc1b3d
    rep stosw                                 ; f3 ab                       ; 0xc1b3f
    jmp short 01b83h                          ; eb 40                       ; 0xc1b41 vgabios.c:1250
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1b43 vgabios.c:1251
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1b47
    mov si, word [bp-01ah]                    ; 8b 76 e6                    ; 0xc1b4b
    sub si, ax                                ; 29 c6                       ; 0xc1b4e
    imul si, word [bp-014h]                   ; 0f af 76 ec                 ; 0xc1b50
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc1b54
    add si, dx                                ; 01 d6                       ; 0xc1b58
    add si, si                                ; 01 f6                       ; 0xc1b5a
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1b5c
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1b60
    mov ax, word [bx+047b2h]                  ; 8b 87 b2 47                 ; 0xc1b63
    mov bx, word [bp-01ah]                    ; 8b 5e e6                    ; 0xc1b67
    imul bx, word [bp-014h]                   ; 0f af 5e ec                 ; 0xc1b6a
    add dx, bx                                ; 01 da                       ; 0xc1b6e
    add dx, dx                                ; 01 d2                       ; 0xc1b70
    mov di, word [bp-01ch]                    ; 8b 7e e4                    ; 0xc1b72
    add di, dx                                ; 01 d7                       ; 0xc1b75
    mov dx, ax                                ; 89 c2                       ; 0xc1b77
    mov es, ax                                ; 8e c0                       ; 0xc1b79
    jcxz 01b83h                               ; e3 06                       ; 0xc1b7b
    push DS                                   ; 1e                          ; 0xc1b7d
    mov ds, dx                                ; 8e da                       ; 0xc1b7e
    rep movsw                                 ; f3 a5                       ; 0xc1b80
    pop DS                                    ; 1f                          ; 0xc1b82
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1b83 vgabios.c:1252
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1b87
    jc near 01f5dh                            ; 0f 82 cf 03                 ; 0xc1b8a
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1b8e vgabios.c:1253
    jmp near 01af0h                           ; e9 5c ff                    ; 0xc1b91
    movzx di, byte [di+0482eh]                ; 0f b6 bd 2e 48              ; 0xc1b94 vgabios.c:1259
    sal di, 006h                              ; c1 e7 06                    ; 0xc1b99
    mov dl, byte [di+04844h]                  ; 8a 95 44 48                 ; 0xc1b9c
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc1ba0
    mov dl, byte [bx+047b0h]                  ; 8a 97 b0 47                 ; 0xc1ba3 vgabios.c:1260
    cmp dl, 003h                              ; 80 fa 03                    ; 0xc1ba7
    jc short 01bbdh                           ; 72 11                       ; 0xc1baa
    jbe short 01bc7h                          ; 76 19                       ; 0xc1bac
    cmp dl, 005h                              ; 80 fa 05                    ; 0xc1bae
    je near 01e40h                            ; 0f 84 8b 02                 ; 0xc1bb1
    cmp dl, 004h                              ; 80 fa 04                    ; 0xc1bb5
    je short 01bc7h                           ; 74 0d                       ; 0xc1bb8
    jmp near 01f5dh                           ; e9 a0 03                    ; 0xc1bba
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc1bbd
    je near 01d06h                            ; 0f 84 42 01                 ; 0xc1bc0
    jmp near 01f5dh                           ; e9 96 03                    ; 0xc1bc4
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1bc7 vgabios.c:1264
    jne short 01c1fh                          ; 75 52                       ; 0xc1bcb
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1bcd
    jne short 01c1fh                          ; 75 4c                       ; 0xc1bd1
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1bd3
    jne short 01c1fh                          ; 75 46                       ; 0xc1bd7
    movzx bx, byte [bp+004h]                  ; 0f b6 5e 04                 ; 0xc1bd9
    mov ax, cx                                ; 89 c8                       ; 0xc1bdd
    dec ax                                    ; 48                          ; 0xc1bdf
    cmp bx, ax                                ; 39 c3                       ; 0xc1be0
    jne short 01c1fh                          ; 75 3b                       ; 0xc1be2
    movzx ax, dh                              ; 0f b6 c6                    ; 0xc1be4
    mov dx, word [bp-014h]                    ; 8b 56 ec                    ; 0xc1be7
    dec dx                                    ; 4a                          ; 0xc1bea
    cmp ax, dx                                ; 39 d0                       ; 0xc1beb
    jne short 01c1fh                          ; 75 30                       ; 0xc1bed
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1bef vgabios.c:1266
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1bf2
    out DX, ax                                ; ef                          ; 0xc1bf5
    mov ax, word [bp-014h]                    ; 8b 46 ec                    ; 0xc1bf6 vgabios.c:1267
    imul ax, cx                               ; 0f af c1                    ; 0xc1bf9
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc1bfc
    imul cx, ax                               ; 0f af c8                    ; 0xc1c00
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1c03
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc1c07
    sal bx, 003h                              ; c1 e3 03                    ; 0xc1c0b
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1c0e
    xor di, di                                ; 31 ff                       ; 0xc1c12
    jcxz 01c18h                               ; e3 02                       ; 0xc1c14
    rep stosb                                 ; f3 aa                       ; 0xc1c16
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc1c18 vgabios.c:1268
    out DX, ax                                ; ef                          ; 0xc1c1b
    jmp near 01f5dh                           ; e9 3e 03                    ; 0xc1c1c vgabios.c:1270
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1c1f vgabios.c:1272
    jne short 01c8eh                          ; 75 69                       ; 0xc1c23
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1c25 vgabios.c:1273
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1c29
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1c2c
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1c30
    jc near 01f5dh                            ; 0f 82 26 03                 ; 0xc1c33
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1c37 vgabios.c:1275
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc1c3b
    cmp dx, ax                                ; 39 c2                       ; 0xc1c3e
    jnbe short 01c48h                         ; 77 06                       ; 0xc1c40
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1c42
    jne short 01c67h                          ; 75 1f                       ; 0xc1c46
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1c48 vgabios.c:1276
    push ax                                   ; 50                          ; 0xc1c4c
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1c4d
    push ax                                   ; 50                          ; 0xc1c51
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1c52
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1c56
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1c5a
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1c5e
    call 016e5h                               ; e8 80 fa                    ; 0xc1c62
    jmp short 01c89h                          ; eb 22                       ; 0xc1c65 vgabios.c:1277
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1c67 vgabios.c:1278
    push ax                                   ; 50                          ; 0xc1c6b
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1c6c
    push ax                                   ; 50                          ; 0xc1c70
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1c71
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1c75
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1c79
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc1c7c
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1c7f
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1c82
    call 01670h                               ; e8 e7 f9                    ; 0xc1c86
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1c89 vgabios.c:1279
    jmp short 01c2ch                          ; eb 9e                       ; 0xc1c8c
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1c8e vgabios.c:1282
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1c92
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1c95
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1c99
    jnbe near 01f5dh                          ; 0f 87 bd 02                 ; 0xc1c9c
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1ca0 vgabios.c:1284
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1ca4
    add ax, dx                                ; 01 d0                       ; 0xc1ca8
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1caa
    jnbe short 01cb5h                         ; 77 06                       ; 0xc1cad
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1caf
    jne short 01cd4h                          ; 75 1f                       ; 0xc1cb3
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1cb5 vgabios.c:1285
    push ax                                   ; 50                          ; 0xc1cb9
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1cba
    push ax                                   ; 50                          ; 0xc1cbe
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1cbf
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1cc3
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1cc7
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ccb
    call 016e5h                               ; e8 13 fa                    ; 0xc1ccf
    jmp short 01cf6h                          ; eb 22                       ; 0xc1cd2 vgabios.c:1286
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1cd4 vgabios.c:1287
    push ax                                   ; 50                          ; 0xc1cd8
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1cd9
    push ax                                   ; 50                          ; 0xc1cdd
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1cde
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1ce2
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1ce6
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1ce9
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1cec
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1cef
    call 01670h                               ; e8 7a f9                    ; 0xc1cf3
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1cf6 vgabios.c:1288
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1cfa
    jc near 01f5dh                            ; 0f 82 5c 02                 ; 0xc1cfd
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1d01 vgabios.c:1289
    jmp short 01c95h                          ; eb 8f                       ; 0xc1d04
    mov dl, byte [bx+047b1h]                  ; 8a 97 b1 47                 ; 0xc1d06 vgabios.c:1294
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d0a vgabios.c:1295
    jne short 01d4bh                          ; 75 3b                       ; 0xc1d0e
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1d10
    jne short 01d4bh                          ; 75 35                       ; 0xc1d14
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1d16
    jne short 01d4bh                          ; 75 2f                       ; 0xc1d1a
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1d1c
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc1d20
    jne short 01d4bh                          ; 75 26                       ; 0xc1d23
    movzx cx, dh                              ; 0f b6 ce                    ; 0xc1d25
    cmp cx, word [bp-018h]                    ; 3b 4e e8                    ; 0xc1d28
    jne short 01d4bh                          ; 75 1e                       ; 0xc1d2b
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc1d2d vgabios.c:1297
    imul ax, cx                               ; 0f af c1                    ; 0xc1d31
    movzx cx, dl                              ; 0f b6 ca                    ; 0xc1d34
    imul cx, ax                               ; 0f af c8                    ; 0xc1d37
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1d3a
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1d3e
    xor di, di                                ; 31 ff                       ; 0xc1d42
    jcxz 01d48h                               ; e3 02                       ; 0xc1d44
    rep stosb                                 ; f3 aa                       ; 0xc1d46
    jmp near 01f5dh                           ; e9 12 02                    ; 0xc1d48 vgabios.c:1299
    cmp dl, 002h                              ; 80 fa 02                    ; 0xc1d4b vgabios.c:1301
    jne short 01d59h                          ; 75 09                       ; 0xc1d4e
    sal byte [bp-010h], 1                     ; d0 66 f0                    ; 0xc1d50 vgabios.c:1303
    sal byte [bp-00ah], 1                     ; d0 66 f6                    ; 0xc1d53 vgabios.c:1304
    sal word [bp-014h], 1                     ; d1 66 ec                    ; 0xc1d56 vgabios.c:1305
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1d59 vgabios.c:1308
    jne short 01dc8h                          ; 75 69                       ; 0xc1d5d
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1d5f vgabios.c:1309
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1d63
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1d66
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1d6a
    jc near 01f5dh                            ; 0f 82 ec 01                 ; 0xc1d6d
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1d71 vgabios.c:1311
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc1d75
    cmp dx, ax                                ; 39 c2                       ; 0xc1d78
    jnbe short 01d82h                         ; 77 06                       ; 0xc1d7a
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1d7c
    jne short 01da1h                          ; 75 1f                       ; 0xc1d80
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1d82 vgabios.c:1312
    push ax                                   ; 50                          ; 0xc1d86
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1d87
    push ax                                   ; 50                          ; 0xc1d8b
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1d8c
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1d90
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1d94
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1d98
    call 017e8h                               ; e8 49 fa                    ; 0xc1d9c
    jmp short 01dc3h                          ; eb 22                       ; 0xc1d9f vgabios.c:1313
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1da1 vgabios.c:1314
    push ax                                   ; 50                          ; 0xc1da5
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1da6
    push ax                                   ; 50                          ; 0xc1daa
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1dab
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1daf
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1db3
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc1db6
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1db9
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1dbc
    call 01745h                               ; e8 82 f9                    ; 0xc1dc0
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1dc3 vgabios.c:1315
    jmp short 01d66h                          ; eb 9e                       ; 0xc1dc6
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1dc8 vgabios.c:1318
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1dcc
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1dcf
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1dd3
    jnbe near 01f5dh                          ; 0f 87 83 01                 ; 0xc1dd6
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1dda vgabios.c:1320
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1dde
    add ax, dx                                ; 01 d0                       ; 0xc1de2
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1de4
    jnbe short 01defh                         ; 77 06                       ; 0xc1de7
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1de9
    jne short 01e0eh                          ; 75 1f                       ; 0xc1ded
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1def vgabios.c:1321
    push ax                                   ; 50                          ; 0xc1df3
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1df4
    push ax                                   ; 50                          ; 0xc1df8
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc1df9
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1dfd
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1e01
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1e05
    call 017e8h                               ; e8 dc f9                    ; 0xc1e09
    jmp short 01e30h                          ; eb 22                       ; 0xc1e0c vgabios.c:1322
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1e0e vgabios.c:1323
    push ax                                   ; 50                          ; 0xc1e12
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc1e13
    push ax                                   ; 50                          ; 0xc1e17
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1e18
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1e1c
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1e20
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1e23
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1e26
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1e29
    call 01745h                               ; e8 15 f9                    ; 0xc1e2d
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1e30 vgabios.c:1324
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1e34
    jc near 01f5dh                            ; 0f 82 22 01                 ; 0xc1e37
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1e3b vgabios.c:1325
    jmp short 01dcfh                          ; eb 8f                       ; 0xc1e3e
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1e40 vgabios.c:1330
    jne short 01e80h                          ; 75 3a                       ; 0xc1e44
    cmp byte [bp-00ch], 000h                  ; 80 7e f4 00                 ; 0xc1e46
    jne short 01e80h                          ; 75 34                       ; 0xc1e4a
    cmp byte [bp-010h], 000h                  ; 80 7e f0 00                 ; 0xc1e4c
    jne short 01e80h                          ; 75 2e                       ; 0xc1e50
    movzx cx, byte [bp+004h]                  ; 0f b6 4e 04                 ; 0xc1e52
    cmp cx, word [bp-016h]                    ; 3b 4e ea                    ; 0xc1e56
    jne short 01e80h                          ; 75 25                       ; 0xc1e59
    movzx dx, dh                              ; 0f b6 d6                    ; 0xc1e5b
    cmp dx, word [bp-018h]                    ; 3b 56 e8                    ; 0xc1e5e
    jne short 01e80h                          ; 75 1d                       ; 0xc1e61
    movzx dx, byte [bp-00eh]                  ; 0f b6 56 f2                 ; 0xc1e63 vgabios.c:1332
    mov cx, ax                                ; 89 c1                       ; 0xc1e67
    imul cx, dx                               ; 0f af ca                    ; 0xc1e69
    sal cx, 003h                              ; c1 e1 03                    ; 0xc1e6c
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1e6f
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc1e73
    xor di, di                                ; 31 ff                       ; 0xc1e77
    jcxz 01e7dh                               ; e3 02                       ; 0xc1e79
    rep stosb                                 ; f3 aa                       ; 0xc1e7b
    jmp near 01f5dh                           ; e9 dd 00                    ; 0xc1e7d vgabios.c:1334
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc1e80 vgabios.c:1337
    jne short 01eech                          ; 75 66                       ; 0xc1e84
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1e86 vgabios.c:1338
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1e8a
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1e8d
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1e91
    jc near 01f5dh                            ; 0f 82 c5 00                 ; 0xc1e94
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc1e98 vgabios.c:1340
    add dx, word [bp-01ah]                    ; 03 56 e6                    ; 0xc1e9c
    cmp dx, ax                                ; 39 c2                       ; 0xc1e9f
    jnbe short 01ea9h                         ; 77 06                       ; 0xc1ea1
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1ea3
    jne short 01ec7h                          ; 75 1e                       ; 0xc1ea7
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1ea9 vgabios.c:1341
    push ax                                   ; 50                          ; 0xc1ead
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1eae
    push ax                                   ; 50                          ; 0xc1eb2
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1eb3
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1eb7
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ebb
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc1ebf
    call 018e2h                               ; e8 1d fa                    ; 0xc1ec2
    jmp short 01ee7h                          ; eb 20                       ; 0xc1ec5 vgabios.c:1342
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1ec7 vgabios.c:1343
    push ax                                   ; 50                          ; 0xc1ecb
    push word [bp-014h]                       ; ff 76 ec                    ; 0xc1ecc
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1ecf
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1ed3
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1ed7
    add al, byte [bp-008h]                    ; 02 46 f8                    ; 0xc1eda
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1edd
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1ee0
    call 01869h                               ; e8 82 f9                    ; 0xc1ee4
    inc word [bp-01ah]                        ; ff 46 e6                    ; 0xc1ee7 vgabios.c:1344
    jmp short 01e8dh                          ; eb a1                       ; 0xc1eea
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1eec vgabios.c:1347
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc1ef0
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc1ef3
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1ef7
    jnbe short 01f5dh                         ; 77 61                       ; 0xc1efa
    movzx dx, byte [bp-00ch]                  ; 0f b6 56 f4                 ; 0xc1efc vgabios.c:1349
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc1f00
    add ax, dx                                ; 01 d0                       ; 0xc1f04
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1f06
    jnbe short 01f11h                         ; 77 06                       ; 0xc1f09
    cmp byte [bp-008h], 000h                  ; 80 7e f8 00                 ; 0xc1f0b
    jne short 01f2fh                          ; 75 1e                       ; 0xc1f0f
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc1f11 vgabios.c:1350
    push ax                                   ; 50                          ; 0xc1f15
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1f16
    push ax                                   ; 50                          ; 0xc1f1a
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc1f1b
    movzx dx, byte [bp-01ah]                  ; 0f b6 56 e6                 ; 0xc1f1f
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1f23
    mov cx, word [bp-014h]                    ; 8b 4e ec                    ; 0xc1f27
    call 018e2h                               ; e8 b5 f9                    ; 0xc1f2a
    jmp short 01f4fh                          ; eb 20                       ; 0xc1f2d vgabios.c:1351
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc1f2f vgabios.c:1352
    push ax                                   ; 50                          ; 0xc1f33
    push word [bp-014h]                       ; ff 76 ec                    ; 0xc1f34
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc1f37
    movzx bx, byte [bp-01ah]                  ; 0f b6 5e e6                 ; 0xc1f3b
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc1f3f
    sub al, byte [bp-008h]                    ; 2a 46 f8                    ; 0xc1f42
    movzx dx, al                              ; 0f b6 d0                    ; 0xc1f45
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc1f48
    call 01869h                               ; e8 1a f9                    ; 0xc1f4c
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1f4f vgabios.c:1353
    cmp ax, word [bp-01ah]                    ; 3b 46 e6                    ; 0xc1f53
    jc short 01f5dh                           ; 72 05                       ; 0xc1f56
    dec word [bp-01ah]                        ; ff 4e e6                    ; 0xc1f58 vgabios.c:1354
    jmp short 01ef3h                          ; eb 96                       ; 0xc1f5b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc1f5d vgabios.c:1365
    pop di                                    ; 5f                          ; 0xc1f60
    pop si                                    ; 5e                          ; 0xc1f61
    pop bp                                    ; 5d                          ; 0xc1f62
    retn 00008h                               ; c2 08 00                    ; 0xc1f63
  ; disGetNextSymbol 0xc1f66 LB 0x2094 -> off=0x0 cb=00000000000000ff uValue=00000000000c1f66 'write_gfx_char_pl4'
write_gfx_char_pl4:                          ; 0xc1f66 LB 0xff
    push bp                                   ; 55                          ; 0xc1f66 vgabios.c:1368
    mov bp, sp                                ; 89 e5                       ; 0xc1f67
    push si                                   ; 56                          ; 0xc1f69
    push di                                   ; 57                          ; 0xc1f6a
    sub sp, strict byte 0000ch                ; 83 ec 0c                    ; 0xc1f6b
    mov ah, al                                ; 88 c4                       ; 0xc1f6e
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc1f70
    mov al, bl                                ; 88 d8                       ; 0xc1f73
    mov bx, 0010ch                            ; bb 0c 01                    ; 0xc1f75 vgabios.c:57
    xor si, si                                ; 31 f6                       ; 0xc1f78
    mov es, si                                ; 8e c6                       ; 0xc1f7a
    mov si, word [es:bx]                      ; 26 8b 37                    ; 0xc1f7c
    mov bx, word [es:bx+002h]                 ; 26 8b 5f 02                 ; 0xc1f7f
    mov word [bp-00ch], si                    ; 89 76 f4                    ; 0xc1f83 vgabios.c:58
    mov word [bp-00ah], bx                    ; 89 5e f6                    ; 0xc1f86
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc1f89 vgabios.c:1377
    movzx cx, byte [bp+006h]                  ; 0f b6 4e 06                 ; 0xc1f8c
    imul bx, cx                               ; 0f af d9                    ; 0xc1f90
    movzx si, byte [bp+004h]                  ; 0f b6 76 04                 ; 0xc1f93
    imul si, bx                               ; 0f af f3                    ; 0xc1f97
    movzx bx, al                              ; 0f b6 d8                    ; 0xc1f9a
    add si, bx                                ; 01 de                       ; 0xc1f9d
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc1f9f vgabios.c:47
    mov di, strict word 00040h                ; bf 40 00                    ; 0xc1fa2
    mov es, di                                ; 8e c7                       ; 0xc1fa5
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc1fa7
    movzx di, byte [bp+008h]                  ; 0f b6 7e 08                 ; 0xc1faa vgabios.c:48
    imul bx, di                               ; 0f af df                    ; 0xc1fae
    add si, bx                                ; 01 de                       ; 0xc1fb1
    movzx ax, ah                              ; 0f b6 c4                    ; 0xc1fb3 vgabios.c:1379
    imul ax, cx                               ; 0f af c1                    ; 0xc1fb6
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc1fb9
    mov ax, 00f02h                            ; b8 02 0f                    ; 0xc1fbc vgabios.c:1380
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc1fbf
    out DX, ax                                ; ef                          ; 0xc1fc2
    mov ax, 00205h                            ; b8 05 02                    ; 0xc1fc3 vgabios.c:1381
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc1fc6
    out DX, ax                                ; ef                          ; 0xc1fc9
    test byte [bp-008h], 080h                 ; f6 46 f8 80                 ; 0xc1fca vgabios.c:1382
    je short 01fd6h                           ; 74 06                       ; 0xc1fce
    mov ax, 01803h                            ; b8 03 18                    ; 0xc1fd0 vgabios.c:1384
    out DX, ax                                ; ef                          ; 0xc1fd3
    jmp short 01fdah                          ; eb 04                       ; 0xc1fd4 vgabios.c:1386
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc1fd6 vgabios.c:1388
    out DX, ax                                ; ef                          ; 0xc1fd9
    xor ch, ch                                ; 30 ed                       ; 0xc1fda vgabios.c:1390
    cmp ch, byte [bp+006h]                    ; 3a 6e 06                    ; 0xc1fdc
    jnc short 0204dh                          ; 73 6c                       ; 0xc1fdf
    movzx bx, ch                              ; 0f b6 dd                    ; 0xc1fe1 vgabios.c:1392
    movzx ax, byte [bp+004h]                  ; 0f b6 46 04                 ; 0xc1fe4
    imul bx, ax                               ; 0f af d8                    ; 0xc1fe8
    add bx, si                                ; 01 f3                       ; 0xc1feb
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc1fed vgabios.c:1393
    jmp short 02005h                          ; eb 12                       ; 0xc1ff1
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc1ff3 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc1ff6
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc1ff8
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc1ffc vgabios.c:1406
    cmp byte [bp-006h], 008h                  ; 80 7e fa 08                 ; 0xc1fff
    jnc short 02049h                          ; 73 44                       ; 0xc2003
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2005
    mov cl, al                                ; 88 c1                       ; 0xc2009
    mov ax, 00080h                            ; b8 80 00                    ; 0xc200b
    sar ax, CL                                ; d3 f8                       ; 0xc200e
    xor ah, ah                                ; 30 e4                       ; 0xc2010
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc2012
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2015
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc2018
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc201a
    out DX, ax                                ; ef                          ; 0xc201d
    mov dx, bx                                ; 89 da                       ; 0xc201e
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2020
    call 033c8h                               ; e8 a2 13                    ; 0xc2023
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc2026
    add ax, word [bp-00eh]                    ; 03 46 f2                    ; 0xc2029
    les di, [bp-00ch]                         ; c4 7e f4                    ; 0xc202c
    add di, ax                                ; 01 c7                       ; 0xc202f
    movzx ax, byte [es:di]                    ; 26 0f b6 05                 ; 0xc2031
    test word [bp-010h], ax                   ; 85 46 f0                    ; 0xc2035
    je short 01ff3h                           ; 74 b9                       ; 0xc2038
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc203a
    and AL, strict byte 00fh                  ; 24 0f                       ; 0xc203d
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc203f
    mov es, di                                ; 8e c7                       ; 0xc2042
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2044
    jmp short 01ffch                          ; eb b3                       ; 0xc2047
    db  0feh, 0c5h
    ; inc ch                                    ; fe c5                     ; 0xc2049 vgabios.c:1407
    jmp short 01fdch                          ; eb 8f                       ; 0xc204b
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc204d vgabios.c:1408
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2050
    out DX, ax                                ; ef                          ; 0xc2053
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2054 vgabios.c:1409
    out DX, ax                                ; ef                          ; 0xc2057
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2058 vgabios.c:1410
    out DX, ax                                ; ef                          ; 0xc205b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc205c vgabios.c:1411
    pop di                                    ; 5f                          ; 0xc205f
    pop si                                    ; 5e                          ; 0xc2060
    pop bp                                    ; 5d                          ; 0xc2061
    retn 00006h                               ; c2 06 00                    ; 0xc2062
  ; disGetNextSymbol 0xc2065 LB 0x1f95 -> off=0x0 cb=00000000000000dd uValue=00000000000c2065 'write_gfx_char_cga'
write_gfx_char_cga:                          ; 0xc2065 LB 0xdd
    push si                                   ; 56                          ; 0xc2065 vgabios.c:1414
    push di                                   ; 57                          ; 0xc2066
    enter 00006h, 000h                        ; c8 06 00 00                 ; 0xc2067
    mov di, 0556ch                            ; bf 6c 55                    ; 0xc206b vgabios.c:1421
    xor bh, bh                                ; 30 ff                       ; 0xc206e vgabios.c:1422
    movzx si, byte [bp+00ah]                  ; 0f b6 76 0a                 ; 0xc2070
    imul si, bx                               ; 0f af f3                    ; 0xc2074
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc2077
    imul bx, bx, 00140h                       ; 69 db 40 01                 ; 0xc207a
    add si, bx                                ; 01 de                       ; 0xc207e
    mov word [bp-004h], si                    ; 89 76 fc                    ; 0xc2080
    xor ah, ah                                ; 30 e4                       ; 0xc2083 vgabios.c:1423
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2085
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc2088
    xor ah, ah                                ; 30 e4                       ; 0xc208b vgabios.c:1424
    jmp near 020abh                           ; e9 1b 00                    ; 0xc208d
    movzx si, ah                              ; 0f b6 f4                    ; 0xc2090 vgabios.c:1439
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc2093
    add si, di                                ; 01 fe                       ; 0xc2096
    mov al, byte [si]                         ; 8a 04                       ; 0xc2098
    mov si, 0b800h                            ; be 00 b8                    ; 0xc209a vgabios.c:42
    mov es, si                                ; 8e c6                       ; 0xc209d
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc209f
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc20a2 vgabios.c:1443
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc20a4
    jnc near 0213ch                           ; 0f 83 91 00                 ; 0xc20a7
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc20ab
    sar bx, 1                                 ; d1 fb                       ; 0xc20ae
    imul bx, bx, strict byte 00050h           ; 6b db 50                    ; 0xc20b0
    add bx, word [bp-004h]                    ; 03 5e fc                    ; 0xc20b3
    test ah, 001h                             ; f6 c4 01                    ; 0xc20b6
    je short 020beh                           ; 74 03                       ; 0xc20b9
    add bh, 020h                              ; 80 c7 20                    ; 0xc20bb
    mov DH, strict byte 080h                  ; b6 80                       ; 0xc20be
    cmp byte [bp+00ah], 001h                  ; 80 7e 0a 01                 ; 0xc20c0
    jne short 020deh                          ; 75 18                       ; 0xc20c4
    test dl, dh                               ; 84 f2                       ; 0xc20c6
    je short 02090h                           ; 74 c6                       ; 0xc20c8
    mov si, 0b800h                            ; be 00 b8                    ; 0xc20ca
    mov es, si                                ; 8e c6                       ; 0xc20cd
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc20cf
    movzx si, ah                              ; 0f b6 f4                    ; 0xc20d2
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc20d5
    add si, di                                ; 01 fe                       ; 0xc20d8
    xor al, byte [si]                         ; 32 04                       ; 0xc20da
    jmp short 0209ah                          ; eb bc                       ; 0xc20dc
    test dh, dh                               ; 84 f6                       ; 0xc20de vgabios.c:1445
    jbe short 020a2h                          ; 76 c0                       ; 0xc20e0
    test dl, 080h                             ; f6 c2 80                    ; 0xc20e2 vgabios.c:1447
    je short 020f1h                           ; 74 0a                       ; 0xc20e5
    mov si, 0b800h                            ; be 00 b8                    ; 0xc20e7 vgabios.c:37
    mov es, si                                ; 8e c6                       ; 0xc20ea
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc20ec
    jmp short 020f3h                          ; eb 02                       ; 0xc20ef vgabios.c:1451
    xor al, al                                ; 30 c0                       ; 0xc20f1 vgabios.c:1453
    mov byte [bp-002h], 000h                  ; c6 46 fe 00                 ; 0xc20f3 vgabios.c:1455
    jmp short 02106h                          ; eb 0d                       ; 0xc20f7
    or al, ch                                 ; 08 e8                       ; 0xc20f9 vgabios.c:1465
    shr dh, 1                                 ; d0 ee                       ; 0xc20fb vgabios.c:1468
    inc byte [bp-002h]                        ; fe 46 fe                    ; 0xc20fd vgabios.c:1469
    cmp byte [bp-002h], 004h                  ; 80 7e fe 04                 ; 0xc2100
    jnc short 02131h                          ; 73 2b                       ; 0xc2104
    movzx si, ah                              ; 0f b6 f4                    ; 0xc2106
    add si, word [bp-006h]                    ; 03 76 fa                    ; 0xc2109
    add si, di                                ; 01 fe                       ; 0xc210c
    movzx si, byte [si]                       ; 0f b6 34                    ; 0xc210e
    movzx cx, dh                              ; 0f b6 ce                    ; 0xc2111
    test si, cx                               ; 85 ce                       ; 0xc2114
    je short 020fbh                           ; 74 e3                       ; 0xc2116
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc2118
    sub cl, byte [bp-002h]                    ; 2a 4e fe                    ; 0xc211a
    mov ch, dl                                ; 88 d5                       ; 0xc211d
    and ch, 003h                              ; 80 e5 03                    ; 0xc211f
    add cl, cl                                ; 00 c9                       ; 0xc2122
    sal ch, CL                                ; d2 e5                       ; 0xc2124
    mov cl, ch                                ; 88 e9                       ; 0xc2126
    test dl, 080h                             ; f6 c2 80                    ; 0xc2128
    je short 020f9h                           ; 74 cc                       ; 0xc212b
    xor al, ch                                ; 30 e8                       ; 0xc212d
    jmp short 020fbh                          ; eb ca                       ; 0xc212f
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc2131 vgabios.c:42
    mov es, cx                                ; 8e c1                       ; 0xc2134
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2136
    inc bx                                    ; 43                          ; 0xc2139 vgabios.c:1471
    jmp short 020deh                          ; eb a2                       ; 0xc213a vgabios.c:1472
    leave                                     ; c9                          ; 0xc213c vgabios.c:1475
    pop di                                    ; 5f                          ; 0xc213d
    pop si                                    ; 5e                          ; 0xc213e
    retn 00004h                               ; c2 04 00                    ; 0xc213f
  ; disGetNextSymbol 0xc2142 LB 0x1eb8 -> off=0x0 cb=0000000000000085 uValue=00000000000c2142 'write_gfx_char_lin'
write_gfx_char_lin:                          ; 0xc2142 LB 0x85
    push si                                   ; 56                          ; 0xc2142 vgabios.c:1478
    push di                                   ; 57                          ; 0xc2143
    enter 00006h, 000h                        ; c8 06 00 00                 ; 0xc2144
    mov dh, dl                                ; 88 d6                       ; 0xc2148
    mov word [bp-002h], 0556ch                ; c7 46 fe 6c 55              ; 0xc214a vgabios.c:1485
    movzx si, cl                              ; 0f b6 f1                    ; 0xc214f vgabios.c:1486
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08                 ; 0xc2152
    imul cx, si                               ; 0f af ce                    ; 0xc2156
    sal cx, 006h                              ; c1 e1 06                    ; 0xc2159
    xor bh, bh                                ; 30 ff                       ; 0xc215c
    sal bx, 003h                              ; c1 e3 03                    ; 0xc215e
    add bx, cx                                ; 01 cb                       ; 0xc2161
    mov word [bp-004h], bx                    ; 89 5e fc                    ; 0xc2163
    xor ah, ah                                ; 30 e4                       ; 0xc2166 vgabios.c:1487
    mov si, ax                                ; 89 c6                       ; 0xc2168
    sal si, 003h                              ; c1 e6 03                    ; 0xc216a
    xor al, al                                ; 30 c0                       ; 0xc216d vgabios.c:1488
    jmp short 021a6h                          ; eb 35                       ; 0xc216f
    cmp ah, 008h                              ; 80 fc 08                    ; 0xc2171 vgabios.c:1492
    jnc short 021a0h                          ; 73 2a                       ; 0xc2174
    xor cl, cl                                ; 30 c9                       ; 0xc2176 vgabios.c:1494
    movzx bx, al                              ; 0f b6 d8                    ; 0xc2178 vgabios.c:1495
    add bx, si                                ; 01 f3                       ; 0xc217b
    add bx, word [bp-002h]                    ; 03 5e fe                    ; 0xc217d
    movzx bx, byte [bx]                       ; 0f b6 1f                    ; 0xc2180
    movzx di, dl                              ; 0f b6 fa                    ; 0xc2183
    test bx, di                               ; 85 fb                       ; 0xc2186
    je short 0218ch                           ; 74 02                       ; 0xc2188
    mov cl, dh                                ; 88 f1                       ; 0xc218a vgabios.c:1497
    movzx bx, ah                              ; 0f b6 dc                    ; 0xc218c vgabios.c:1499
    add bx, word [bp-006h]                    ; 03 5e fa                    ; 0xc218f
    mov di, 0a000h                            ; bf 00 a0                    ; 0xc2192 vgabios.c:42
    mov es, di                                ; 8e c7                       ; 0xc2195
    mov byte [es:bx], cl                      ; 26 88 0f                    ; 0xc2197
    shr dl, 1                                 ; d0 ea                       ; 0xc219a vgabios.c:1500
    db  0feh, 0c4h
    ; inc ah                                    ; fe c4                     ; 0xc219c vgabios.c:1501
    jmp short 02171h                          ; eb d1                       ; 0xc219e
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc21a0 vgabios.c:1502
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc21a2
    jnc short 021c1h                          ; 73 1b                       ; 0xc21a4
    movzx cx, al                              ; 0f b6 c8                    ; 0xc21a6
    movzx bx, byte [bp+008h]                  ; 0f b6 5e 08                 ; 0xc21a9
    imul bx, cx                               ; 0f af d9                    ; 0xc21ad
    sal bx, 003h                              ; c1 e3 03                    ; 0xc21b0
    mov cx, word [bp-004h]                    ; 8b 4e fc                    ; 0xc21b3
    add cx, bx                                ; 01 d9                       ; 0xc21b6
    mov word [bp-006h], cx                    ; 89 4e fa                    ; 0xc21b8
    mov DL, strict byte 080h                  ; b2 80                       ; 0xc21bb
    xor ah, ah                                ; 30 e4                       ; 0xc21bd
    jmp short 02176h                          ; eb b5                       ; 0xc21bf
    leave                                     ; c9                          ; 0xc21c1 vgabios.c:1503
    pop di                                    ; 5f                          ; 0xc21c2
    pop si                                    ; 5e                          ; 0xc21c3
    retn 00002h                               ; c2 02 00                    ; 0xc21c4
  ; disGetNextSymbol 0xc21c7 LB 0x1e33 -> off=0x0 cb=0000000000000165 uValue=00000000000c21c7 'biosfn_write_char_attr'
biosfn_write_char_attr:                      ; 0xc21c7 LB 0x165
    push bp                                   ; 55                          ; 0xc21c7 vgabios.c:1506
    mov bp, sp                                ; 89 e5                       ; 0xc21c8
    push si                                   ; 56                          ; 0xc21ca
    push di                                   ; 57                          ; 0xc21cb
    sub sp, strict byte 00018h                ; 83 ec 18                    ; 0xc21cc
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc21cf
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc21d2
    mov byte [bp-012h], bl                    ; 88 5e ee                    ; 0xc21d5
    mov si, cx                                ; 89 ce                       ; 0xc21d8
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc21da vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc21dd
    mov es, ax                                ; 8e c0                       ; 0xc21e0
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc21e2
    xor ah, ah                                ; 30 e4                       ; 0xc21e5 vgabios.c:1514
    call 033a1h                               ; e8 b7 11                    ; 0xc21e7
    mov cl, al                                ; 88 c1                       ; 0xc21ea
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc21ec
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc21ef vgabios.c:1515
    je near 02325h                            ; 0f 84 30 01                 ; 0xc21f1
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc21f5 vgabios.c:1518
    lea bx, [bp-01ch]                         ; 8d 5e e4                    ; 0xc21f8
    lea dx, [bp-01ah]                         ; 8d 56 e6                    ; 0xc21fb
    call 00a17h                               ; e8 16 e8                    ; 0xc21fe
    mov al, byte [bp-01ch]                    ; 8a 46 e4                    ; 0xc2201 vgabios.c:1519
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2204
    mov dx, word [bp-01ch]                    ; 8b 56 e4                    ; 0xc2207
    xor dl, dl                                ; 30 d2                       ; 0xc220a
    shr dx, 008h                              ; c1 ea 08                    ; 0xc220c
    mov byte [bp-014h], dl                    ; 88 56 ec                    ; 0xc220f
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2212 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2215
    mov es, ax                                ; 8e c0                       ; 0xc2218
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc221a
    xor ah, ah                                ; 30 e4                       ; 0xc221d vgabios.c:38
    inc ax                                    ; 40                          ; 0xc221f
    mov word [bp-018h], ax                    ; 89 46 e8                    ; 0xc2220
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2223 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc2226
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc2229 vgabios.c:48
    movzx bx, cl                              ; 0f b6 d9                    ; 0xc222c vgabios.c:1525
    mov di, bx                                ; 89 df                       ; 0xc222f
    sal di, 003h                              ; c1 e7 03                    ; 0xc2231
    cmp byte [di+047afh], 000h                ; 80 bd af 47 00              ; 0xc2234
    jne short 02281h                          ; 75 46                       ; 0xc2239
    mov bx, word [bp-018h]                    ; 8b 5e e8                    ; 0xc223b vgabios.c:1528
    imul bx, ax                               ; 0f af d8                    ; 0xc223e
    add bx, bx                                ; 01 db                       ; 0xc2241
    or bl, 0ffh                               ; 80 cb ff                    ; 0xc2243
    movzx cx, byte [bp-00eh]                  ; 0f b6 4e f2                 ; 0xc2246
    inc bx                                    ; 43                          ; 0xc224a
    imul bx, cx                               ; 0f af d9                    ; 0xc224b
    xor dh, dh                                ; 30 f6                       ; 0xc224e
    imul ax, dx                               ; 0f af c2                    ; 0xc2250
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc2253
    add ax, dx                                ; 01 d0                       ; 0xc2257
    add ax, ax                                ; 01 c0                       ; 0xc2259
    mov dx, bx                                ; 89 da                       ; 0xc225b
    add dx, ax                                ; 01 c2                       ; 0xc225d
    movzx ax, byte [bp-012h]                  ; 0f b6 46 ee                 ; 0xc225f vgabios.c:1530
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2263
    movzx bx, byte [bp-008h]                  ; 0f b6 5e f8                 ; 0xc2266
    add ax, bx                                ; 01 d8                       ; 0xc226a
    mov word [bp-01ah], ax                    ; 89 46 e6                    ; 0xc226c
    mov ax, word [bp-01ah]                    ; 8b 46 e6                    ; 0xc226f vgabios.c:1531
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc2272
    mov cx, si                                ; 89 f1                       ; 0xc2276
    mov di, dx                                ; 89 d7                       ; 0xc2278
    jcxz 0227eh                               ; e3 02                       ; 0xc227a
    rep stosw                                 ; f3 ab                       ; 0xc227c
    jmp near 02325h                           ; e9 a4 00                    ; 0xc227e vgabios.c:1533
    movzx bx, byte [bx+0482eh]                ; 0f b6 9f 2e 48              ; 0xc2281 vgabios.c:1536
    sal bx, 006h                              ; c1 e3 06                    ; 0xc2286
    mov al, byte [bx+04844h]                  ; 8a 87 44 48                 ; 0xc2289
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc228d
    mov al, byte [di+047b1h]                  ; 8a 85 b1 47                 ; 0xc2290 vgabios.c:1537
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2294
    dec si                                    ; 4e                          ; 0xc2297 vgabios.c:1538
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2298
    je near 02325h                            ; 0f 84 86 00                 ; 0xc229b
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc229f vgabios.c:1540
    sal bx, 003h                              ; c1 e3 03                    ; 0xc22a3
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc22a6
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc22aa
    jc short 022bah                           ; 72 0c                       ; 0xc22ac
    jbe short 022c0h                          ; 76 10                       ; 0xc22ae
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc22b0
    je short 02307h                           ; 74 53                       ; 0xc22b2
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc22b4
    je short 022c4h                           ; 74 0c                       ; 0xc22b6
    jmp short 0231fh                          ; eb 65                       ; 0xc22b8
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc22ba
    je short 022e8h                           ; 74 2a                       ; 0xc22bc
    jmp short 0231fh                          ; eb 5f                       ; 0xc22be
    or byte [bp-012h], 001h                   ; 80 4e ee 01                 ; 0xc22c0 vgabios.c:1543
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc22c4 vgabios.c:1545
    push ax                                   ; 50                          ; 0xc22c8
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc22c9
    push ax                                   ; 50                          ; 0xc22cd
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc22ce
    push ax                                   ; 50                          ; 0xc22d2
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc22d3
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc22d7
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc22db
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc22df
    call 01f66h                               ; e8 80 fc                    ; 0xc22e3
    jmp short 0231fh                          ; eb 37                       ; 0xc22e6 vgabios.c:1546
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc22e8 vgabios.c:1548
    push ax                                   ; 50                          ; 0xc22ec
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc22ed
    push ax                                   ; 50                          ; 0xc22f1
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc22f2
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc22f6
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc22fa
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc22fe
    call 02065h                               ; e8 60 fd                    ; 0xc2302
    jmp short 0231fh                          ; eb 18                       ; 0xc2305 vgabios.c:1549
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2307 vgabios.c:1551
    push ax                                   ; 50                          ; 0xc230b
    movzx cx, byte [bp-014h]                  ; 0f b6 4e ec                 ; 0xc230c
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2310
    movzx dx, byte [bp-012h]                  ; 0f b6 56 ee                 ; 0xc2314
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2318
    call 02142h                               ; e8 23 fe                    ; 0xc231c
    inc byte [bp-010h]                        ; fe 46 f0                    ; 0xc231f vgabios.c:1558
    jmp near 02297h                           ; e9 72 ff                    ; 0xc2322 vgabios.c:1559
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2325 vgabios.c:1561
    pop di                                    ; 5f                          ; 0xc2328
    pop si                                    ; 5e                          ; 0xc2329
    pop bp                                    ; 5d                          ; 0xc232a
    retn                                      ; c3                          ; 0xc232b
  ; disGetNextSymbol 0xc232c LB 0x1cce -> off=0x0 cb=0000000000000162 uValue=00000000000c232c 'biosfn_write_char_only'
biosfn_write_char_only:                      ; 0xc232c LB 0x162
    push bp                                   ; 55                          ; 0xc232c vgabios.c:1564
    mov bp, sp                                ; 89 e5                       ; 0xc232d
    push si                                   ; 56                          ; 0xc232f
    push di                                   ; 57                          ; 0xc2330
    sub sp, strict byte 00016h                ; 83 ec 16                    ; 0xc2331
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2334
    mov byte [bp-00eh], dl                    ; 88 56 f2                    ; 0xc2337
    mov byte [bp-006h], bl                    ; 88 5e fa                    ; 0xc233a
    mov si, cx                                ; 89 ce                       ; 0xc233d
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc233f vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2342
    mov es, ax                                ; 8e c0                       ; 0xc2345
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2347
    xor ah, ah                                ; 30 e4                       ; 0xc234a vgabios.c:1572
    call 033a1h                               ; e8 52 10                    ; 0xc234c
    mov cl, al                                ; 88 c1                       ; 0xc234f
    mov byte [bp-012h], al                    ; 88 46 ee                    ; 0xc2351
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2354 vgabios.c:1573
    je near 02487h                            ; 0f 84 2d 01                 ; 0xc2356
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc235a vgabios.c:1576
    lea bx, [bp-01ah]                         ; 8d 5e e6                    ; 0xc235d
    lea dx, [bp-018h]                         ; 8d 56 e8                    ; 0xc2360
    call 00a17h                               ; e8 b1 e6                    ; 0xc2363
    mov al, byte [bp-01ah]                    ; 8a 46 e6                    ; 0xc2366 vgabios.c:1577
    mov byte [bp-010h], al                    ; 88 46 f0                    ; 0xc2369
    mov dx, word [bp-01ah]                    ; 8b 56 e6                    ; 0xc236c
    xor dl, dl                                ; 30 d2                       ; 0xc236f
    shr dx, 008h                              ; c1 ea 08                    ; 0xc2371
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc2374
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2377 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc237a
    mov es, ax                                ; 8e c0                       ; 0xc237d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc237f
    xor ah, ah                                ; 30 e4                       ; 0xc2382 vgabios.c:38
    mov di, ax                                ; 89 c7                       ; 0xc2384
    inc di                                    ; 47                          ; 0xc2386
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc2387 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc238a
    mov word [bp-016h], ax                    ; 89 46 ea                    ; 0xc238d vgabios.c:48
    xor ch, ch                                ; 30 ed                       ; 0xc2390 vgabios.c:1583
    mov bx, cx                                ; 89 cb                       ; 0xc2392
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2394
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc2397
    jne short 023dbh                          ; 75 3d                       ; 0xc239c
    imul di, ax                               ; 0f af f8                    ; 0xc239e vgabios.c:1586
    add di, di                                ; 01 ff                       ; 0xc23a1
    or di, 000ffh                             ; 81 cf ff 00                 ; 0xc23a3
    movzx bx, byte [bp-00eh]                  ; 0f b6 5e f2                 ; 0xc23a7
    inc di                                    ; 47                          ; 0xc23ab
    imul bx, di                               ; 0f af df                    ; 0xc23ac
    xor dh, dh                                ; 30 f6                       ; 0xc23af
    imul ax, dx                               ; 0f af c2                    ; 0xc23b1
    movzx dx, byte [bp-010h]                  ; 0f b6 56 f0                 ; 0xc23b4
    add ax, dx                                ; 01 d0                       ; 0xc23b8
    add ax, ax                                ; 01 c0                       ; 0xc23ba
    add bx, ax                                ; 01 c3                       ; 0xc23bc
    dec si                                    ; 4e                          ; 0xc23be vgabios.c:1588
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc23bf
    je near 02487h                            ; 0f 84 c1 00                 ; 0xc23c2
    movzx di, byte [bp-012h]                  ; 0f b6 7e ee                 ; 0xc23c6 vgabios.c:1589
    sal di, 003h                              ; c1 e7 03                    ; 0xc23ca
    mov es, [di+047b2h]                       ; 8e 85 b2 47                 ; 0xc23cd vgabios.c:40
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc23d1
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc23d4
    inc bx                                    ; 43                          ; 0xc23d7 vgabios.c:1590
    inc bx                                    ; 43                          ; 0xc23d8
    jmp short 023beh                          ; eb e3                       ; 0xc23d9 vgabios.c:1591
    mov di, cx                                ; 89 cf                       ; 0xc23db vgabios.c:1596
    movzx ax, byte [di+0482eh]                ; 0f b6 85 2e 48              ; 0xc23dd
    mov di, ax                                ; 89 c7                       ; 0xc23e2
    sal di, 006h                              ; c1 e7 06                    ; 0xc23e4
    mov al, byte [di+04844h]                  ; 8a 85 44 48                 ; 0xc23e7
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc23eb
    mov al, byte [bx+047b1h]                  ; 8a 87 b1 47                 ; 0xc23ee vgabios.c:1597
    mov byte [bp-014h], al                    ; 88 46 ec                    ; 0xc23f2
    dec si                                    ; 4e                          ; 0xc23f5 vgabios.c:1598
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc23f6
    je near 02487h                            ; 0f 84 8a 00                 ; 0xc23f9
    movzx bx, byte [bp-012h]                  ; 0f b6 5e ee                 ; 0xc23fd vgabios.c:1600
    sal bx, 003h                              ; c1 e3 03                    ; 0xc2401
    mov bl, byte [bx+047b0h]                  ; 8a 9f b0 47                 ; 0xc2404
    cmp bl, 003h                              ; 80 fb 03                    ; 0xc2408
    jc short 0241bh                           ; 72 0e                       ; 0xc240b
    jbe short 02422h                          ; 76 13                       ; 0xc240d
    cmp bl, 005h                              ; 80 fb 05                    ; 0xc240f
    je short 02469h                           ; 74 55                       ; 0xc2412
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc2414
    je short 02426h                           ; 74 0d                       ; 0xc2417
    jmp short 02481h                          ; eb 66                       ; 0xc2419
    cmp bl, 002h                              ; 80 fb 02                    ; 0xc241b
    je short 0244ah                           ; 74 2a                       ; 0xc241e
    jmp short 02481h                          ; eb 5f                       ; 0xc2420
    or byte [bp-006h], 001h                   ; 80 4e fa 01                 ; 0xc2422 vgabios.c:1603
    movzx ax, byte [bp-00eh]                  ; 0f b6 46 f2                 ; 0xc2426 vgabios.c:1605
    push ax                                   ; 50                          ; 0xc242a
    movzx ax, byte [bp-00ch]                  ; 0f b6 46 f4                 ; 0xc242b
    push ax                                   ; 50                          ; 0xc242f
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2430
    push ax                                   ; 50                          ; 0xc2434
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc2435
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2439
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc243d
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2441
    call 01f66h                               ; e8 1e fb                    ; 0xc2445
    jmp short 02481h                          ; eb 37                       ; 0xc2448 vgabios.c:1606
    movzx ax, byte [bp-014h]                  ; 0f b6 46 ec                 ; 0xc244a vgabios.c:1608
    push ax                                   ; 50                          ; 0xc244e
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc244f
    push ax                                   ; 50                          ; 0xc2453
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc2454
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2458
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc245c
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc2460
    call 02065h                               ; e8 fe fb                    ; 0xc2464
    jmp short 02481h                          ; eb 18                       ; 0xc2467 vgabios.c:1609
    movzx ax, byte [bp-016h]                  ; 0f b6 46 ea                 ; 0xc2469 vgabios.c:1611
    push ax                                   ; 50                          ; 0xc246d
    movzx cx, byte [bp-00ah]                  ; 0f b6 4e f6                 ; 0xc246e
    movzx bx, byte [bp-010h]                  ; 0f b6 5e f0                 ; 0xc2472
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2476
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc247a
    call 02142h                               ; e8 c1 fc                    ; 0xc247e
    inc byte [bp-010h]                        ; fe 46 f0                    ; 0xc2481 vgabios.c:1618
    jmp near 023f5h                           ; e9 6e ff                    ; 0xc2484 vgabios.c:1619
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2487 vgabios.c:1621
    pop di                                    ; 5f                          ; 0xc248a
    pop si                                    ; 5e                          ; 0xc248b
    pop bp                                    ; 5d                          ; 0xc248c
    retn                                      ; c3                          ; 0xc248d
  ; disGetNextSymbol 0xc248e LB 0x1b6c -> off=0x0 cb=0000000000000165 uValue=00000000000c248e 'biosfn_write_pixel'
biosfn_write_pixel:                          ; 0xc248e LB 0x165
    push bp                                   ; 55                          ; 0xc248e vgabios.c:1624
    mov bp, sp                                ; 89 e5                       ; 0xc248f
    push si                                   ; 56                          ; 0xc2491
    push ax                                   ; 50                          ; 0xc2492
    push ax                                   ; 50                          ; 0xc2493
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc2494
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2497
    mov dx, bx                                ; 89 da                       ; 0xc249a
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc249c vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc249f
    mov es, ax                                ; 8e c0                       ; 0xc24a2
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc24a4
    xor ah, ah                                ; 30 e4                       ; 0xc24a7 vgabios.c:1631
    call 033a1h                               ; e8 f5 0e                    ; 0xc24a9
    mov ah, al                                ; 88 c4                       ; 0xc24ac
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc24ae vgabios.c:1632
    je near 025ceh                            ; 0f 84 1a 01                 ; 0xc24b0
    movzx bx, al                              ; 0f b6 d8                    ; 0xc24b4 vgabios.c:1633
    sal bx, 003h                              ; c1 e3 03                    ; 0xc24b7
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc24ba
    je near 025ceh                            ; 0f 84 0b 01                 ; 0xc24bf
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc24c3 vgabios.c:1635
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc24c7
    jc short 024dah                           ; 72 0f                       ; 0xc24c9
    jbe short 024e1h                          ; 76 14                       ; 0xc24cb
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc24cd
    je near 025d4h                            ; 0f 84 01 01                 ; 0xc24cf
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc24d3
    je short 024e1h                           ; 74 0a                       ; 0xc24d5
    jmp near 025ceh                           ; e9 f4 00                    ; 0xc24d7
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc24da
    je short 02550h                           ; 74 72                       ; 0xc24dc
    jmp near 025ceh                           ; e9 ed 00                    ; 0xc24de
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc24e1 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc24e4
    mov es, ax                                ; 8e c0                       ; 0xc24e7
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc24e9
    imul ax, cx                               ; 0f af c1                    ; 0xc24ec vgabios.c:48
    mov bx, dx                                ; 89 d3                       ; 0xc24ef
    shr bx, 003h                              ; c1 eb 03                    ; 0xc24f1
    add bx, ax                                ; 01 c3                       ; 0xc24f4
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc24f6 vgabios.c:47
    mov cx, word [es:si]                      ; 26 8b 0c                    ; 0xc24f9
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc24fc vgabios.c:48
    imul ax, cx                               ; 0f af c1                    ; 0xc2500
    add bx, ax                                ; 01 c3                       ; 0xc2503
    mov cl, dl                                ; 88 d1                       ; 0xc2505 vgabios.c:1641
    and cl, 007h                              ; 80 e1 07                    ; 0xc2507
    mov ax, 00080h                            ; b8 80 00                    ; 0xc250a
    sar ax, CL                                ; d3 f8                       ; 0xc250d
    xor ah, ah                                ; 30 e4                       ; 0xc250f vgabios.c:1642
    sal ax, 008h                              ; c1 e0 08                    ; 0xc2511
    or AL, strict byte 008h                   ; 0c 08                       ; 0xc2514
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2516
    out DX, ax                                ; ef                          ; 0xc2519
    mov ax, 00205h                            ; b8 05 02                    ; 0xc251a vgabios.c:1643
    out DX, ax                                ; ef                          ; 0xc251d
    mov dx, bx                                ; 89 da                       ; 0xc251e vgabios.c:1644
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2520
    call 033c8h                               ; e8 a2 0e                    ; 0xc2523
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc2526 vgabios.c:1645
    je short 02533h                           ; 74 07                       ; 0xc252a
    mov ax, 01803h                            ; b8 03 18                    ; 0xc252c vgabios.c:1647
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc252f
    out DX, ax                                ; ef                          ; 0xc2532
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2533 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc2536
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc2538
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc253b
    mov ax, 0ff08h                            ; b8 08 ff                    ; 0xc253e vgabios.c:1650
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2541
    out DX, ax                                ; ef                          ; 0xc2544
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc2545 vgabios.c:1651
    out DX, ax                                ; ef                          ; 0xc2548
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc2549 vgabios.c:1652
    out DX, ax                                ; ef                          ; 0xc254c
    jmp near 025ceh                           ; e9 7e 00                    ; 0xc254d vgabios.c:1653
    mov si, cx                                ; 89 ce                       ; 0xc2550 vgabios.c:1655
    shr si, 1                                 ; d1 ee                       ; 0xc2552
    imul si, si, strict byte 00050h           ; 6b f6 50                    ; 0xc2554
    cmp al, byte [bx+047b1h]                  ; 3a 87 b1 47                 ; 0xc2557
    jne short 02564h                          ; 75 07                       ; 0xc255b
    mov bx, dx                                ; 89 d3                       ; 0xc255d vgabios.c:1657
    shr bx, 002h                              ; c1 eb 02                    ; 0xc255f
    jmp short 02569h                          ; eb 05                       ; 0xc2562 vgabios.c:1659
    mov bx, dx                                ; 89 d3                       ; 0xc2564 vgabios.c:1661
    shr bx, 003h                              ; c1 eb 03                    ; 0xc2566
    add bx, si                                ; 01 f3                       ; 0xc2569
    test cl, 001h                             ; f6 c1 01                    ; 0xc256b vgabios.c:1663
    je short 02573h                           ; 74 03                       ; 0xc256e
    add bh, 020h                              ; 80 c7 20                    ; 0xc2570
    mov cx, 0b800h                            ; b9 00 b8                    ; 0xc2573 vgabios.c:37
    mov es, cx                                ; 8e c1                       ; 0xc2576
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2578
    movzx si, ah                              ; 0f b6 f4                    ; 0xc257b vgabios.c:1665
    sal si, 003h                              ; c1 e6 03                    ; 0xc257e
    cmp byte [si+047b1h], 002h                ; 80 bc b1 47 02              ; 0xc2581
    jne short 0259fh                          ; 75 17                       ; 0xc2586
    mov ah, dl                                ; 88 d4                       ; 0xc2588 vgabios.c:1667
    and ah, 003h                              ; 80 e4 03                    ; 0xc258a
    mov CL, strict byte 003h                  ; b1 03                       ; 0xc258d
    sub cl, ah                                ; 28 e1                       ; 0xc258f
    add cl, cl                                ; 00 c9                       ; 0xc2591
    mov dh, byte [bp-006h]                    ; 8a 76 fa                    ; 0xc2593
    and dh, 003h                              ; 80 e6 03                    ; 0xc2596
    sal dh, CL                                ; d2 e6                       ; 0xc2599
    mov DL, strict byte 003h                  ; b2 03                       ; 0xc259b vgabios.c:1668
    jmp short 025b2h                          ; eb 13                       ; 0xc259d vgabios.c:1670
    mov ah, dl                                ; 88 d4                       ; 0xc259f vgabios.c:1672
    and ah, 007h                              ; 80 e4 07                    ; 0xc25a1
    mov CL, strict byte 007h                  ; b1 07                       ; 0xc25a4
    sub cl, ah                                ; 28 e1                       ; 0xc25a6
    mov dh, byte [bp-006h]                    ; 8a 76 fa                    ; 0xc25a8
    and dh, 001h                              ; 80 e6 01                    ; 0xc25ab
    sal dh, CL                                ; d2 e6                       ; 0xc25ae
    mov DL, strict byte 001h                  ; b2 01                       ; 0xc25b0 vgabios.c:1673
    sal dl, CL                                ; d2 e2                       ; 0xc25b2
    test byte [bp-006h], 080h                 ; f6 46 fa 80                 ; 0xc25b4 vgabios.c:1675
    je short 025beh                           ; 74 04                       ; 0xc25b8
    xor al, dh                                ; 30 f0                       ; 0xc25ba vgabios.c:1677
    jmp short 025c6h                          ; eb 08                       ; 0xc25bc vgabios.c:1679
    mov ah, dl                                ; 88 d4                       ; 0xc25be vgabios.c:1681
    not ah                                    ; f6 d4                       ; 0xc25c0
    and al, ah                                ; 20 e0                       ; 0xc25c2
    or al, dh                                 ; 08 f0                       ; 0xc25c4 vgabios.c:1682
    mov dx, 0b800h                            ; ba 00 b8                    ; 0xc25c6 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc25c9
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc25cb
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc25ce vgabios.c:1685
    pop si                                    ; 5e                          ; 0xc25d1
    pop bp                                    ; 5d                          ; 0xc25d2
    retn                                      ; c3                          ; 0xc25d3
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc25d4 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc25d7
    mov es, ax                                ; 8e c0                       ; 0xc25da
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc25dc
    sal ax, 003h                              ; c1 e0 03                    ; 0xc25df vgabios.c:48
    imul ax, cx                               ; 0f af c1                    ; 0xc25e2
    mov bx, dx                                ; 89 d3                       ; 0xc25e5
    add bx, ax                                ; 01 c3                       ; 0xc25e7
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc25e9 vgabios.c:42
    mov es, ax                                ; 8e c0                       ; 0xc25ec
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc25ee
    jmp short 025cbh                          ; eb d8                       ; 0xc25f1
  ; disGetNextSymbol 0xc25f3 LB 0x1a07 -> off=0x0 cb=000000000000024a uValue=00000000000c25f3 'biosfn_write_teletype'
biosfn_write_teletype:                       ; 0xc25f3 LB 0x24a
    push bp                                   ; 55                          ; 0xc25f3 vgabios.c:1698
    mov bp, sp                                ; 89 e5                       ; 0xc25f4
    push si                                   ; 56                          ; 0xc25f6
    sub sp, strict byte 00012h                ; 83 ec 12                    ; 0xc25f7
    mov ch, al                                ; 88 c5                       ; 0xc25fa
    mov byte [bp-00ah], dl                    ; 88 56 f6                    ; 0xc25fc
    mov byte [bp-008h], bl                    ; 88 5e f8                    ; 0xc25ff
    cmp dl, 0ffh                              ; 80 fa ff                    ; 0xc2602 vgabios.c:1706
    jne short 02615h                          ; 75 0e                       ; 0xc2605
    mov bx, strict word 00062h                ; bb 62 00                    ; 0xc2607 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc260a
    mov es, ax                                ; 8e c0                       ; 0xc260d
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc260f
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2612 vgabios.c:38
    mov bx, strict word 00049h                ; bb 49 00                    ; 0xc2615 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2618
    mov es, ax                                ; 8e c0                       ; 0xc261b
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc261d
    xor ah, ah                                ; 30 e4                       ; 0xc2620 vgabios.c:1711
    call 033a1h                               ; e8 7c 0d                    ; 0xc2622
    mov byte [bp-00ch], al                    ; 88 46 f4                    ; 0xc2625
    cmp AL, strict byte 0ffh                  ; 3c ff                       ; 0xc2628 vgabios.c:1712
    je near 02837h                            ; 0f 84 09 02                 ; 0xc262a
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc262e vgabios.c:1715
    lea bx, [bp-012h]                         ; 8d 5e ee                    ; 0xc2632
    lea dx, [bp-014h]                         ; 8d 56 ec                    ; 0xc2635
    call 00a17h                               ; e8 dc e3                    ; 0xc2638
    mov al, byte [bp-012h]                    ; 8a 46 ee                    ; 0xc263b vgabios.c:1716
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc263e
    mov ax, word [bp-012h]                    ; 8b 46 ee                    ; 0xc2641
    xor al, al                                ; 30 c0                       ; 0xc2644
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2646
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc2649
    mov bx, 00084h                            ; bb 84 00                    ; 0xc264c vgabios.c:37
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc264f
    mov es, dx                                ; 8e c2                       ; 0xc2652
    mov dl, byte [es:bx]                      ; 26 8a 17                    ; 0xc2654
    xor dh, dh                                ; 30 f6                       ; 0xc2657 vgabios.c:38
    inc dx                                    ; 42                          ; 0xc2659
    mov word [bp-00eh], dx                    ; 89 56 f2                    ; 0xc265a
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc265d vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc2660
    mov word [bp-010h], dx                    ; 89 56 f0                    ; 0xc2663 vgabios.c:48
    cmp ch, 008h                              ; 80 fd 08                    ; 0xc2666 vgabios.c:1722
    jc short 02679h                           ; 72 0e                       ; 0xc2669
    jbe short 02682h                          ; 76 15                       ; 0xc266b
    cmp ch, 00dh                              ; 80 fd 0d                    ; 0xc266d
    je short 02698h                           ; 74 26                       ; 0xc2670
    cmp ch, 00ah                              ; 80 fd 0a                    ; 0xc2672
    je short 02690h                           ; 74 19                       ; 0xc2675
    jmp short 0269fh                          ; eb 26                       ; 0xc2677
    cmp ch, 007h                              ; 80 fd 07                    ; 0xc2679
    je near 02793h                            ; 0f 84 13 01                 ; 0xc267c
    jmp short 0269fh                          ; eb 1d                       ; 0xc2680
    cmp byte [bp-006h], 000h                  ; 80 7e fa 00                 ; 0xc2682 vgabios.c:1729
    jbe near 02793h                           ; 0f 86 09 01                 ; 0xc2686
    dec byte [bp-006h]                        ; fe 4e fa                    ; 0xc268a
    jmp near 02793h                           ; e9 03 01                    ; 0xc268d vgabios.c:1730
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2690 vgabios.c:1733
    mov byte [bp-004h], al                    ; 88 46 fc                    ; 0xc2692
    jmp near 02793h                           ; e9 fb 00                    ; 0xc2695 vgabios.c:1734
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc2698 vgabios.c:1737
    jmp near 02793h                           ; e9 f4 00                    ; 0xc269c vgabios.c:1738
    movzx si, byte [bp-00ch]                  ; 0f b6 76 f4                 ; 0xc269f vgabios.c:1742
    mov bx, si                                ; 89 f3                       ; 0xc26a3
    sal bx, 003h                              ; c1 e3 03                    ; 0xc26a5
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc26a8
    jne short 026f2h                          ; 75 43                       ; 0xc26ad
    mov ax, word [bp-010h]                    ; 8b 46 f0                    ; 0xc26af vgabios.c:1745
    imul ax, word [bp-00eh]                   ; 0f af 46 f2                 ; 0xc26b2
    add ax, ax                                ; 01 c0                       ; 0xc26b6
    or AL, strict byte 0ffh                   ; 0c ff                       ; 0xc26b8
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc26ba
    mov si, ax                                ; 89 c6                       ; 0xc26be
    inc si                                    ; 46                          ; 0xc26c0
    imul si, dx                               ; 0f af f2                    ; 0xc26c1
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc26c4
    imul ax, word [bp-010h]                   ; 0f af 46 f0                 ; 0xc26c8
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc26cc
    add ax, dx                                ; 01 d0                       ; 0xc26d0
    add ax, ax                                ; 01 c0                       ; 0xc26d2
    add si, ax                                ; 01 c6                       ; 0xc26d4
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc26d6 vgabios.c:40
    mov byte [es:si], ch                      ; 26 88 2c                    ; 0xc26da
    cmp cl, 003h                              ; 80 f9 03                    ; 0xc26dd vgabios.c:1750
    jne near 02780h                           ; 0f 85 9c 00                 ; 0xc26e0
    inc si                                    ; 46                          ; 0xc26e4 vgabios.c:1751
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc26e5 vgabios.c:40
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc26e9
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc26ec
    jmp near 02780h                           ; e9 8e 00                    ; 0xc26ef vgabios.c:1753
    movzx si, byte [si+0482eh]                ; 0f b6 b4 2e 48              ; 0xc26f2 vgabios.c:1756
    sal si, 006h                              ; c1 e6 06                    ; 0xc26f7
    mov ah, byte [si+04844h]                  ; 8a a4 44 48                 ; 0xc26fa
    mov dl, byte [bx+047b1h]                  ; 8a 97 b1 47                 ; 0xc26fe vgabios.c:1757
    mov al, byte [bx+047b0h]                  ; 8a 87 b0 47                 ; 0xc2702 vgabios.c:1758
    cmp AL, strict byte 003h                  ; 3c 03                       ; 0xc2706
    jc short 02716h                           ; 72 0c                       ; 0xc2708
    jbe short 0271ch                          ; 76 10                       ; 0xc270a
    cmp AL, strict byte 005h                  ; 3c 05                       ; 0xc270c
    je short 02767h                           ; 74 57                       ; 0xc270e
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2710
    je short 02720h                           ; 74 0c                       ; 0xc2712
    jmp short 02780h                          ; eb 6a                       ; 0xc2714
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc2716
    je short 02746h                           ; 74 2c                       ; 0xc2718
    jmp short 02780h                          ; eb 64                       ; 0xc271a
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc271c vgabios.c:1761
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc2720 vgabios.c:1763
    push dx                                   ; 52                          ; 0xc2724
    movzx ax, ah                              ; 0f b6 c4                    ; 0xc2725
    push ax                                   ; 50                          ; 0xc2728
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc2729
    push ax                                   ; 50                          ; 0xc272d
    movzx bx, byte [bp-004h]                  ; 0f b6 5e fc                 ; 0xc272e
    movzx si, byte [bp-006h]                  ; 0f b6 76 fa                 ; 0xc2732
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc2736
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc273a
    mov cx, bx                                ; 89 d9                       ; 0xc273d
    mov bx, si                                ; 89 f3                       ; 0xc273f
    call 01f66h                               ; e8 22 f8                    ; 0xc2741
    jmp short 02780h                          ; eb 3a                       ; 0xc2744 vgabios.c:1764
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc2746 vgabios.c:1766
    push ax                                   ; 50                          ; 0xc2749
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc274a
    push ax                                   ; 50                          ; 0xc274e
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc274f
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc2753
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc2757
    movzx si, ch                              ; 0f b6 f5                    ; 0xc275b
    mov cx, ax                                ; 89 c1                       ; 0xc275e
    mov ax, si                                ; 89 f0                       ; 0xc2760
    call 02065h                               ; e8 00 f9                    ; 0xc2762
    jmp short 02780h                          ; eb 19                       ; 0xc2765 vgabios.c:1767
    movzx ax, byte [bp-010h]                  ; 0f b6 46 f0                 ; 0xc2767 vgabios.c:1769
    push ax                                   ; 50                          ; 0xc276b
    movzx si, byte [bp-004h]                  ; 0f b6 76 fc                 ; 0xc276c
    movzx bx, byte [bp-006h]                  ; 0f b6 5e fa                 ; 0xc2770
    movzx dx, byte [bp-008h]                  ; 0f b6 56 f8                 ; 0xc2774
    movzx ax, ch                              ; 0f b6 c5                    ; 0xc2778
    mov cx, si                                ; 89 f1                       ; 0xc277b
    call 02142h                               ; e8 c2 f9                    ; 0xc277d
    inc byte [bp-006h]                        ; fe 46 fa                    ; 0xc2780 vgabios.c:1777
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2783 vgabios.c:1779
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc2787
    jne short 02793h                          ; 75 07                       ; 0xc278a
    mov byte [bp-006h], 000h                  ; c6 46 fa 00                 ; 0xc278c vgabios.c:1780
    inc byte [bp-004h]                        ; fe 46 fc                    ; 0xc2790 vgabios.c:1781
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc2793 vgabios.c:1786
    cmp ax, word [bp-00eh]                    ; 3b 46 f2                    ; 0xc2797
    jne near 0281bh                           ; 0f 85 7d 00                 ; 0xc279a
    movzx bx, byte [bp-00ch]                  ; 0f b6 5e f4                 ; 0xc279e vgabios.c:1788
    sal bx, 003h                              ; c1 e3 03                    ; 0xc27a2
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc27a5
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc27a8
    mov ah, byte [bp-010h]                    ; 8a 66 f0                    ; 0xc27aa
    db  0feh, 0cch
    ; dec ah                                    ; fe cc                     ; 0xc27ad
    cmp byte [bx+047afh], 000h                ; 80 bf af 47 00              ; 0xc27af
    jne short 027feh                          ; 75 48                       ; 0xc27b4
    mov dx, word [bp-010h]                    ; 8b 56 f0                    ; 0xc27b6 vgabios.c:1790
    imul dx, word [bp-00eh]                   ; 0f af 56 f2                 ; 0xc27b9
    add dx, dx                                ; 01 d2                       ; 0xc27bd
    or dl, 0ffh                               ; 80 ca ff                    ; 0xc27bf
    movzx si, byte [bp-00ah]                  ; 0f b6 76 f6                 ; 0xc27c2
    inc dx                                    ; 42                          ; 0xc27c6
    imul si, dx                               ; 0f af f2                    ; 0xc27c7
    movzx dx, byte [bp-004h]                  ; 0f b6 56 fc                 ; 0xc27ca
    dec dx                                    ; 4a                          ; 0xc27ce
    mov cx, word [bp-010h]                    ; 8b 4e f0                    ; 0xc27cf
    imul cx, dx                               ; 0f af ca                    ; 0xc27d2
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc27d5
    add dx, cx                                ; 01 ca                       ; 0xc27d9
    add dx, dx                                ; 01 d2                       ; 0xc27db
    add si, dx                                ; 01 d6                       ; 0xc27dd
    inc si                                    ; 46                          ; 0xc27df vgabios.c:1791
    mov es, [bx+047b2h]                       ; 8e 87 b2 47                 ; 0xc27e0 vgabios.c:35
    mov bl, byte [es:si]                      ; 26 8a 1c                    ; 0xc27e4
    push strict byte 00001h                   ; 6a 01                       ; 0xc27e7 vgabios.c:1792
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc27e9
    push dx                                   ; 52                          ; 0xc27ed
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc27ee
    push dx                                   ; 52                          ; 0xc27f1
    xor ah, ah                                ; 30 e4                       ; 0xc27f2
    push ax                                   ; 50                          ; 0xc27f4
    movzx dx, bl                              ; 0f b6 d3                    ; 0xc27f5
    xor cx, cx                                ; 31 c9                       ; 0xc27f8
    xor bx, bx                                ; 31 db                       ; 0xc27fa
    jmp short 02812h                          ; eb 14                       ; 0xc27fc vgabios.c:1794
    push strict byte 00001h                   ; 6a 01                       ; 0xc27fe vgabios.c:1796
    movzx dx, byte [bp-00ah]                  ; 0f b6 56 f6                 ; 0xc2800
    push dx                                   ; 52                          ; 0xc2804
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc2805
    push dx                                   ; 52                          ; 0xc2808
    xor ah, ah                                ; 30 e4                       ; 0xc2809
    push ax                                   ; 50                          ; 0xc280b
    xor cx, cx                                ; 31 c9                       ; 0xc280c
    xor bx, bx                                ; 31 db                       ; 0xc280e
    xor dx, dx                                ; 31 d2                       ; 0xc2810
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2812
    call 0193eh                               ; e8 26 f1                    ; 0xc2815
    dec byte [bp-004h]                        ; fe 4e fc                    ; 0xc2818 vgabios.c:1798
    movzx ax, byte [bp-004h]                  ; 0f b6 46 fc                 ; 0xc281b vgabios.c:1802
    mov word [bp-012h], ax                    ; 89 46 ee                    ; 0xc281f
    sal word [bp-012h], 008h                  ; c1 66 ee 08                 ; 0xc2822
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2826
    add word [bp-012h], ax                    ; 01 46 ee                    ; 0xc282a
    mov dx, word [bp-012h]                    ; 8b 56 ee                    ; 0xc282d vgabios.c:1803
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc2830
    call 011c6h                               ; e8 8f e9                    ; 0xc2834
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2837 vgabios.c:1804
    pop si                                    ; 5e                          ; 0xc283a
    pop bp                                    ; 5d                          ; 0xc283b
    retn                                      ; c3                          ; 0xc283c
  ; disGetNextSymbol 0xc283d LB 0x17bd -> off=0x0 cb=000000000000002c uValue=00000000000c283d 'get_font_access'
get_font_access:                             ; 0xc283d LB 0x2c
    push bp                                   ; 55                          ; 0xc283d vgabios.c:1807
    mov bp, sp                                ; 89 e5                       ; 0xc283e
    push dx                                   ; 52                          ; 0xc2840
    mov ax, 00100h                            ; b8 00 01                    ; 0xc2841 vgabios.c:1809
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2844
    out DX, ax                                ; ef                          ; 0xc2847
    mov ax, 00402h                            ; b8 02 04                    ; 0xc2848 vgabios.c:1810
    out DX, ax                                ; ef                          ; 0xc284b
    mov ax, 00704h                            ; b8 04 07                    ; 0xc284c vgabios.c:1811
    out DX, ax                                ; ef                          ; 0xc284f
    mov ax, 00300h                            ; b8 00 03                    ; 0xc2850 vgabios.c:1812
    out DX, ax                                ; ef                          ; 0xc2853
    mov ax, 00204h                            ; b8 04 02                    ; 0xc2854 vgabios.c:1813
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2857
    out DX, ax                                ; ef                          ; 0xc285a
    mov ax, strict word 00005h                ; b8 05 00                    ; 0xc285b vgabios.c:1814
    out DX, ax                                ; ef                          ; 0xc285e
    mov ax, 00406h                            ; b8 06 04                    ; 0xc285f vgabios.c:1815
    out DX, ax                                ; ef                          ; 0xc2862
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2863 vgabios.c:1816
    pop dx                                    ; 5a                          ; 0xc2866
    pop bp                                    ; 5d                          ; 0xc2867
    retn                                      ; c3                          ; 0xc2868
  ; disGetNextSymbol 0xc2869 LB 0x1791 -> off=0x0 cb=000000000000003c uValue=00000000000c2869 'release_font_access'
release_font_access:                         ; 0xc2869 LB 0x3c
    push bp                                   ; 55                          ; 0xc2869 vgabios.c:1818
    mov bp, sp                                ; 89 e5                       ; 0xc286a
    push dx                                   ; 52                          ; 0xc286c
    mov ax, 00100h                            ; b8 00 01                    ; 0xc286d vgabios.c:1820
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2870
    out DX, ax                                ; ef                          ; 0xc2873
    mov ax, 00302h                            ; b8 02 03                    ; 0xc2874 vgabios.c:1821
    out DX, ax                                ; ef                          ; 0xc2877
    mov ax, 00304h                            ; b8 04 03                    ; 0xc2878 vgabios.c:1822
    out DX, ax                                ; ef                          ; 0xc287b
    mov ax, 00300h                            ; b8 00 03                    ; 0xc287c vgabios.c:1823
    out DX, ax                                ; ef                          ; 0xc287f
    mov dx, 003cch                            ; ba cc 03                    ; 0xc2880 vgabios.c:1824
    in AL, DX                                 ; ec                          ; 0xc2883
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2884
    and ax, strict word 00001h                ; 25 01 00                    ; 0xc2886
    sal ax, 002h                              ; c1 e0 02                    ; 0xc2889
    or AL, strict byte 00ah                   ; 0c 0a                       ; 0xc288c
    sal ax, 008h                              ; c1 e0 08                    ; 0xc288e
    or AL, strict byte 006h                   ; 0c 06                       ; 0xc2891
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2893
    out DX, ax                                ; ef                          ; 0xc2896
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc2897 vgabios.c:1825
    out DX, ax                                ; ef                          ; 0xc289a
    mov ax, 01005h                            ; b8 05 10                    ; 0xc289b vgabios.c:1826
    out DX, ax                                ; ef                          ; 0xc289e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc289f vgabios.c:1827
    pop dx                                    ; 5a                          ; 0xc28a2
    pop bp                                    ; 5d                          ; 0xc28a3
    retn                                      ; c3                          ; 0xc28a4
  ; disGetNextSymbol 0xc28a5 LB 0x1755 -> off=0x0 cb=00000000000000b4 uValue=00000000000c28a5 'set_scan_lines'
set_scan_lines:                              ; 0xc28a5 LB 0xb4
    push bp                                   ; 55                          ; 0xc28a5 vgabios.c:1829
    mov bp, sp                                ; 89 e5                       ; 0xc28a6
    push bx                                   ; 53                          ; 0xc28a8
    push cx                                   ; 51                          ; 0xc28a9
    push dx                                   ; 52                          ; 0xc28aa
    push si                                   ; 56                          ; 0xc28ab
    push di                                   ; 57                          ; 0xc28ac
    mov bl, al                                ; 88 c3                       ; 0xc28ad
    mov si, strict word 00063h                ; be 63 00                    ; 0xc28af vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc28b2
    mov es, ax                                ; 8e c0                       ; 0xc28b5
    mov si, word [es:si]                      ; 26 8b 34                    ; 0xc28b7
    mov cx, si                                ; 89 f1                       ; 0xc28ba vgabios.c:48
    mov AL, strict byte 009h                  ; b0 09                       ; 0xc28bc vgabios.c:1835
    mov dx, si                                ; 89 f2                       ; 0xc28be
    out DX, AL                                ; ee                          ; 0xc28c0
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc28c1 vgabios.c:1836
    in AL, DX                                 ; ec                          ; 0xc28c4
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc28c5
    mov ah, al                                ; 88 c4                       ; 0xc28c7 vgabios.c:1837
    and ah, 0e0h                              ; 80 e4 e0                    ; 0xc28c9
    mov al, bl                                ; 88 d8                       ; 0xc28cc
    db  0feh, 0c8h
    ; dec al                                    ; fe c8                     ; 0xc28ce
    or al, ah                                 ; 08 e0                       ; 0xc28d0
    out DX, AL                                ; ee                          ; 0xc28d2 vgabios.c:1838
    cmp bl, 008h                              ; 80 fb 08                    ; 0xc28d3 vgabios.c:1839
    jne short 028e0h                          ; 75 08                       ; 0xc28d6
    mov dx, strict word 00007h                ; ba 07 00                    ; 0xc28d8 vgabios.c:1841
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc28db
    jmp short 028edh                          ; eb 0d                       ; 0xc28de vgabios.c:1843
    mov al, bl                                ; 88 d8                       ; 0xc28e0 vgabios.c:1845
    sub AL, strict byte 003h                  ; 2c 03                       ; 0xc28e2
    movzx dx, al                              ; 0f b6 d0                    ; 0xc28e4
    mov al, bl                                ; 88 d8                       ; 0xc28e7
    sub AL, strict byte 004h                  ; 2c 04                       ; 0xc28e9
    xor ah, ah                                ; 30 e4                       ; 0xc28eb
    call 010d0h                               ; e8 e0 e7                    ; 0xc28ed
    movzx di, bl                              ; 0f b6 fb                    ; 0xc28f0 vgabios.c:1847
    mov bx, 00085h                            ; bb 85 00                    ; 0xc28f3 vgabios.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc28f6
    mov es, ax                                ; 8e c0                       ; 0xc28f9
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc28fb
    mov AL, strict byte 012h                  ; b0 12                       ; 0xc28fe vgabios.c:1848
    mov dx, cx                                ; 89 ca                       ; 0xc2900
    out DX, AL                                ; ee                          ; 0xc2902
    mov bx, cx                                ; 89 cb                       ; 0xc2903 vgabios.c:1849
    inc bx                                    ; 43                          ; 0xc2905
    mov dx, bx                                ; 89 da                       ; 0xc2906
    in AL, DX                                 ; ec                          ; 0xc2908
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2909
    mov si, ax                                ; 89 c6                       ; 0xc290b
    mov AL, strict byte 007h                  ; b0 07                       ; 0xc290d vgabios.c:1850
    mov dx, cx                                ; 89 ca                       ; 0xc290f
    out DX, AL                                ; ee                          ; 0xc2911
    mov dx, bx                                ; 89 da                       ; 0xc2912 vgabios.c:1851
    in AL, DX                                 ; ec                          ; 0xc2914
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2915
    mov ah, al                                ; 88 c4                       ; 0xc2917 vgabios.c:1852
    and ah, 002h                              ; 80 e4 02                    ; 0xc2919
    movzx dx, ah                              ; 0f b6 d4                    ; 0xc291c
    sal dx, 007h                              ; c1 e2 07                    ; 0xc291f
    and AL, strict byte 040h                  ; 24 40                       ; 0xc2922
    xor ah, ah                                ; 30 e4                       ; 0xc2924
    sal ax, 003h                              ; c1 e0 03                    ; 0xc2926
    add ax, dx                                ; 01 d0                       ; 0xc2929
    inc ax                                    ; 40                          ; 0xc292b
    add ax, si                                ; 01 f0                       ; 0xc292c
    xor dx, dx                                ; 31 d2                       ; 0xc292e vgabios.c:1853
    div di                                    ; f7 f7                       ; 0xc2930
    mov dl, al                                ; 88 c2                       ; 0xc2932 vgabios.c:1854
    db  0feh, 0cah
    ; dec dl                                    ; fe ca                     ; 0xc2934
    mov bx, 00084h                            ; bb 84 00                    ; 0xc2936 vgabios.c:42
    mov byte [es:bx], dl                      ; 26 88 17                    ; 0xc2939
    mov bx, strict word 0004ah                ; bb 4a 00                    ; 0xc293c vgabios.c:47
    mov dx, word [es:bx]                      ; 26 8b 17                    ; 0xc293f
    xor ah, ah                                ; 30 e4                       ; 0xc2942 vgabios.c:1856
    imul dx, ax                               ; 0f af d0                    ; 0xc2944
    add dx, dx                                ; 01 d2                       ; 0xc2947
    mov bx, strict word 0004ch                ; bb 4c 00                    ; 0xc2949 vgabios.c:52
    mov word [es:bx], dx                      ; 26 89 17                    ; 0xc294c
    lea sp, [bp-00ah]                         ; 8d 66 f6                    ; 0xc294f vgabios.c:1857
    pop di                                    ; 5f                          ; 0xc2952
    pop si                                    ; 5e                          ; 0xc2953
    pop dx                                    ; 5a                          ; 0xc2954
    pop cx                                    ; 59                          ; 0xc2955
    pop bx                                    ; 5b                          ; 0xc2956
    pop bp                                    ; 5d                          ; 0xc2957
    retn                                      ; c3                          ; 0xc2958
  ; disGetNextSymbol 0xc2959 LB 0x16a1 -> off=0x0 cb=000000000000007c uValue=00000000000c2959 'biosfn_load_text_user_pat'
biosfn_load_text_user_pat:                   ; 0xc2959 LB 0x7c
    push bp                                   ; 55                          ; 0xc2959 vgabios.c:1859
    mov bp, sp                                ; 89 e5                       ; 0xc295a
    push si                                   ; 56                          ; 0xc295c
    push di                                   ; 57                          ; 0xc295d
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc295e
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc2961
    mov word [bp-00ch], dx                    ; 89 56 f4                    ; 0xc2964
    mov word [bp-008h], bx                    ; 89 5e f8                    ; 0xc2967
    mov word [bp-00ah], cx                    ; 89 4e f6                    ; 0xc296a
    call 0283dh                               ; e8 cd fe                    ; 0xc296d vgabios.c:1864
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc2970 vgabios.c:1865
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2973
    xor ah, ah                                ; 30 e4                       ; 0xc2975
    mov bx, ax                                ; 89 c3                       ; 0xc2977
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2979
    mov al, byte [bp+006h]                    ; 8a 46 06                    ; 0xc297c
    and AL, strict byte 004h                  ; 24 04                       ; 0xc297f
    xor ah, ah                                ; 30 e4                       ; 0xc2981
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2983
    add bx, ax                                ; 01 c3                       ; 0xc2986
    mov word [bp-00eh], bx                    ; 89 5e f2                    ; 0xc2988
    xor bx, bx                                ; 31 db                       ; 0xc298b vgabios.c:1866
    cmp bx, word [bp-00ah]                    ; 3b 5e f6                    ; 0xc298d
    jnc short 029bch                          ; 73 2a                       ; 0xc2990
    movzx cx, byte [bp+008h]                  ; 0f b6 4e 08                 ; 0xc2992 vgabios.c:1868
    mov si, bx                                ; 89 de                       ; 0xc2996
    imul si, cx                               ; 0f af f1                    ; 0xc2998
    add si, word [bp-008h]                    ; 03 76 f8                    ; 0xc299b
    mov di, word [bp+004h]                    ; 8b 7e 04                    ; 0xc299e vgabios.c:1869
    add di, bx                                ; 01 df                       ; 0xc29a1
    sal di, 005h                              ; c1 e7 05                    ; 0xc29a3
    add di, word [bp-00eh]                    ; 03 7e f2                    ; 0xc29a6
    mov dx, word [bp-00ch]                    ; 8b 56 f4                    ; 0xc29a9 vgabios.c:1870
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc29ac
    mov es, ax                                ; 8e c0                       ; 0xc29af
    jcxz 029b9h                               ; e3 06                       ; 0xc29b1
    push DS                                   ; 1e                          ; 0xc29b3
    mov ds, dx                                ; 8e da                       ; 0xc29b4
    rep movsb                                 ; f3 a4                       ; 0xc29b6
    pop DS                                    ; 1f                          ; 0xc29b8
    inc bx                                    ; 43                          ; 0xc29b9 vgabios.c:1871
    jmp short 0298dh                          ; eb d1                       ; 0xc29ba
    call 02869h                               ; e8 aa fe                    ; 0xc29bc vgabios.c:1872
    cmp byte [bp-006h], 010h                  ; 80 7e fa 10                 ; 0xc29bf vgabios.c:1873
    jc short 029cch                           ; 72 07                       ; 0xc29c3
    movzx ax, byte [bp+008h]                  ; 0f b6 46 08                 ; 0xc29c5 vgabios.c:1875
    call 028a5h                               ; e8 d9 fe                    ; 0xc29c9
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc29cc vgabios.c:1877
    pop di                                    ; 5f                          ; 0xc29cf
    pop si                                    ; 5e                          ; 0xc29d0
    pop bp                                    ; 5d                          ; 0xc29d1
    retn 00006h                               ; c2 06 00                    ; 0xc29d2
  ; disGetNextSymbol 0xc29d5 LB 0x1625 -> off=0x0 cb=000000000000006f uValue=00000000000c29d5 'biosfn_load_text_8_14_pat'
biosfn_load_text_8_14_pat:                   ; 0xc29d5 LB 0x6f
    push bp                                   ; 55                          ; 0xc29d5 vgabios.c:1879
    mov bp, sp                                ; 89 e5                       ; 0xc29d6
    push bx                                   ; 53                          ; 0xc29d8
    push cx                                   ; 51                          ; 0xc29d9
    push si                                   ; 56                          ; 0xc29da
    push di                                   ; 57                          ; 0xc29db
    push ax                                   ; 50                          ; 0xc29dc
    push ax                                   ; 50                          ; 0xc29dd
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc29de
    call 0283dh                               ; e8 59 fe                    ; 0xc29e1 vgabios.c:1883
    mov al, dl                                ; 88 d0                       ; 0xc29e4 vgabios.c:1884
    and AL, strict byte 003h                  ; 24 03                       ; 0xc29e6
    xor ah, ah                                ; 30 e4                       ; 0xc29e8
    mov bx, ax                                ; 89 c3                       ; 0xc29ea
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc29ec
    mov al, dl                                ; 88 d0                       ; 0xc29ef
    and AL, strict byte 004h                  ; 24 04                       ; 0xc29f1
    xor ah, ah                                ; 30 e4                       ; 0xc29f3
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc29f5
    add bx, ax                                ; 01 c3                       ; 0xc29f8
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc29fa
    xor bx, bx                                ; 31 db                       ; 0xc29fd vgabios.c:1885
    jmp short 02a07h                          ; eb 06                       ; 0xc29ff
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2a01
    jnc short 02a2ch                          ; 73 25                       ; 0xc2a05
    imul si, bx, strict byte 0000eh           ; 6b f3 0e                    ; 0xc2a07 vgabios.c:1887
    mov di, bx                                ; 89 df                       ; 0xc2a0a vgabios.c:1888
    sal di, 005h                              ; c1 e7 05                    ; 0xc2a0c
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2a0f
    add si, 05d6ch                            ; 81 c6 6c 5d                 ; 0xc2a12 vgabios.c:1889
    mov cx, strict word 0000eh                ; b9 0e 00                    ; 0xc2a16
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2a19
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2a1c
    mov es, ax                                ; 8e c0                       ; 0xc2a1f
    jcxz 02a29h                               ; e3 06                       ; 0xc2a21
    push DS                                   ; 1e                          ; 0xc2a23
    mov ds, dx                                ; 8e da                       ; 0xc2a24
    rep movsb                                 ; f3 a4                       ; 0xc2a26
    pop DS                                    ; 1f                          ; 0xc2a28
    inc bx                                    ; 43                          ; 0xc2a29 vgabios.c:1890
    jmp short 02a01h                          ; eb d5                       ; 0xc2a2a
    call 02869h                               ; e8 3a fe                    ; 0xc2a2c vgabios.c:1891
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2a2f vgabios.c:1892
    jc short 02a3bh                           ; 72 06                       ; 0xc2a33
    mov ax, strict word 0000eh                ; b8 0e 00                    ; 0xc2a35 vgabios.c:1894
    call 028a5h                               ; e8 6a fe                    ; 0xc2a38
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2a3b vgabios.c:1896
    pop di                                    ; 5f                          ; 0xc2a3e
    pop si                                    ; 5e                          ; 0xc2a3f
    pop cx                                    ; 59                          ; 0xc2a40
    pop bx                                    ; 5b                          ; 0xc2a41
    pop bp                                    ; 5d                          ; 0xc2a42
    retn                                      ; c3                          ; 0xc2a43
  ; disGetNextSymbol 0xc2a44 LB 0x15b6 -> off=0x0 cb=0000000000000071 uValue=00000000000c2a44 'biosfn_load_text_8_8_pat'
biosfn_load_text_8_8_pat:                    ; 0xc2a44 LB 0x71
    push bp                                   ; 55                          ; 0xc2a44 vgabios.c:1898
    mov bp, sp                                ; 89 e5                       ; 0xc2a45
    push bx                                   ; 53                          ; 0xc2a47
    push cx                                   ; 51                          ; 0xc2a48
    push si                                   ; 56                          ; 0xc2a49
    push di                                   ; 57                          ; 0xc2a4a
    push ax                                   ; 50                          ; 0xc2a4b
    push ax                                   ; 50                          ; 0xc2a4c
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2a4d
    call 0283dh                               ; e8 ea fd                    ; 0xc2a50 vgabios.c:1902
    mov al, dl                                ; 88 d0                       ; 0xc2a53 vgabios.c:1903
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2a55
    xor ah, ah                                ; 30 e4                       ; 0xc2a57
    mov bx, ax                                ; 89 c3                       ; 0xc2a59
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2a5b
    mov al, dl                                ; 88 d0                       ; 0xc2a5e
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2a60
    xor ah, ah                                ; 30 e4                       ; 0xc2a62
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2a64
    add bx, ax                                ; 01 c3                       ; 0xc2a67
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2a69
    xor bx, bx                                ; 31 db                       ; 0xc2a6c vgabios.c:1904
    jmp short 02a76h                          ; eb 06                       ; 0xc2a6e
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2a70
    jnc short 02a9dh                          ; 73 27                       ; 0xc2a74
    mov si, bx                                ; 89 de                       ; 0xc2a76 vgabios.c:1906
    sal si, 003h                              ; c1 e6 03                    ; 0xc2a78
    mov di, bx                                ; 89 df                       ; 0xc2a7b vgabios.c:1907
    sal di, 005h                              ; c1 e7 05                    ; 0xc2a7d
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2a80
    add si, 0556ch                            ; 81 c6 6c 55                 ; 0xc2a83 vgabios.c:1908
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc2a87
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2a8a
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2a8d
    mov es, ax                                ; 8e c0                       ; 0xc2a90
    jcxz 02a9ah                               ; e3 06                       ; 0xc2a92
    push DS                                   ; 1e                          ; 0xc2a94
    mov ds, dx                                ; 8e da                       ; 0xc2a95
    rep movsb                                 ; f3 a4                       ; 0xc2a97
    pop DS                                    ; 1f                          ; 0xc2a99
    inc bx                                    ; 43                          ; 0xc2a9a vgabios.c:1909
    jmp short 02a70h                          ; eb d3                       ; 0xc2a9b
    call 02869h                               ; e8 c9 fd                    ; 0xc2a9d vgabios.c:1910
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2aa0 vgabios.c:1911
    jc short 02aach                           ; 72 06                       ; 0xc2aa4
    mov ax, strict word 00008h                ; b8 08 00                    ; 0xc2aa6 vgabios.c:1913
    call 028a5h                               ; e8 f9 fd                    ; 0xc2aa9
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2aac vgabios.c:1915
    pop di                                    ; 5f                          ; 0xc2aaf
    pop si                                    ; 5e                          ; 0xc2ab0
    pop cx                                    ; 59                          ; 0xc2ab1
    pop bx                                    ; 5b                          ; 0xc2ab2
    pop bp                                    ; 5d                          ; 0xc2ab3
    retn                                      ; c3                          ; 0xc2ab4
  ; disGetNextSymbol 0xc2ab5 LB 0x1545 -> off=0x0 cb=0000000000000071 uValue=00000000000c2ab5 'biosfn_load_text_8_16_pat'
biosfn_load_text_8_16_pat:                   ; 0xc2ab5 LB 0x71
    push bp                                   ; 55                          ; 0xc2ab5 vgabios.c:1918
    mov bp, sp                                ; 89 e5                       ; 0xc2ab6
    push bx                                   ; 53                          ; 0xc2ab8
    push cx                                   ; 51                          ; 0xc2ab9
    push si                                   ; 56                          ; 0xc2aba
    push di                                   ; 57                          ; 0xc2abb
    push ax                                   ; 50                          ; 0xc2abc
    push ax                                   ; 50                          ; 0xc2abd
    mov byte [bp-00ah], al                    ; 88 46 f6                    ; 0xc2abe
    call 0283dh                               ; e8 79 fd                    ; 0xc2ac1 vgabios.c:1922
    mov al, dl                                ; 88 d0                       ; 0xc2ac4 vgabios.c:1923
    and AL, strict byte 003h                  ; 24 03                       ; 0xc2ac6
    xor ah, ah                                ; 30 e4                       ; 0xc2ac8
    mov bx, ax                                ; 89 c3                       ; 0xc2aca
    sal bx, 00eh                              ; c1 e3 0e                    ; 0xc2acc
    mov al, dl                                ; 88 d0                       ; 0xc2acf
    and AL, strict byte 004h                  ; 24 04                       ; 0xc2ad1
    xor ah, ah                                ; 30 e4                       ; 0xc2ad3
    sal ax, 00bh                              ; c1 e0 0b                    ; 0xc2ad5
    add bx, ax                                ; 01 c3                       ; 0xc2ad8
    mov word [bp-00ch], bx                    ; 89 5e f4                    ; 0xc2ada
    xor bx, bx                                ; 31 db                       ; 0xc2add vgabios.c:1924
    jmp short 02ae7h                          ; eb 06                       ; 0xc2adf
    cmp bx, 00100h                            ; 81 fb 00 01                 ; 0xc2ae1
    jnc short 02b0eh                          ; 73 27                       ; 0xc2ae5
    mov si, bx                                ; 89 de                       ; 0xc2ae7 vgabios.c:1926
    sal si, 004h                              ; c1 e6 04                    ; 0xc2ae9
    mov di, bx                                ; 89 df                       ; 0xc2aec vgabios.c:1927
    sal di, 005h                              ; c1 e7 05                    ; 0xc2aee
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc2af1
    add si, 06b6ch                            ; 81 c6 6c 6b                 ; 0xc2af4 vgabios.c:1928
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc2af8
    mov dx, 0c000h                            ; ba 00 c0                    ; 0xc2afb
    mov ax, 0a000h                            ; b8 00 a0                    ; 0xc2afe
    mov es, ax                                ; 8e c0                       ; 0xc2b01
    jcxz 02b0bh                               ; e3 06                       ; 0xc2b03
    push DS                                   ; 1e                          ; 0xc2b05
    mov ds, dx                                ; 8e da                       ; 0xc2b06
    rep movsb                                 ; f3 a4                       ; 0xc2b08
    pop DS                                    ; 1f                          ; 0xc2b0a
    inc bx                                    ; 43                          ; 0xc2b0b vgabios.c:1929
    jmp short 02ae1h                          ; eb d3                       ; 0xc2b0c
    call 02869h                               ; e8 58 fd                    ; 0xc2b0e vgabios.c:1930
    cmp byte [bp-00ah], 010h                  ; 80 7e f6 10                 ; 0xc2b11 vgabios.c:1931
    jc short 02b1dh                           ; 72 06                       ; 0xc2b15
    mov ax, strict word 00010h                ; b8 10 00                    ; 0xc2b17 vgabios.c:1933
    call 028a5h                               ; e8 88 fd                    ; 0xc2b1a
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc2b1d vgabios.c:1935
    pop di                                    ; 5f                          ; 0xc2b20
    pop si                                    ; 5e                          ; 0xc2b21
    pop cx                                    ; 59                          ; 0xc2b22
    pop bx                                    ; 5b                          ; 0xc2b23
    pop bp                                    ; 5d                          ; 0xc2b24
    retn                                      ; c3                          ; 0xc2b25
  ; disGetNextSymbol 0xc2b26 LB 0x14d4 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b26 'biosfn_load_gfx_8_8_chars'
biosfn_load_gfx_8_8_chars:                   ; 0xc2b26 LB 0x5
    push bp                                   ; 55                          ; 0xc2b26 vgabios.c:1937
    mov bp, sp                                ; 89 e5                       ; 0xc2b27
    pop bp                                    ; 5d                          ; 0xc2b29 vgabios.c:1942
    retn                                      ; c3                          ; 0xc2b2a
  ; disGetNextSymbol 0xc2b2b LB 0x14cf -> off=0x0 cb=0000000000000007 uValue=00000000000c2b2b 'biosfn_load_gfx_user_chars'
biosfn_load_gfx_user_chars:                  ; 0xc2b2b LB 0x7
    push bp                                   ; 55                          ; 0xc2b2b vgabios.c:1943
    mov bp, sp                                ; 89 e5                       ; 0xc2b2c
    pop bp                                    ; 5d                          ; 0xc2b2e vgabios.c:1949
    retn 00002h                               ; c2 02 00                    ; 0xc2b2f
  ; disGetNextSymbol 0xc2b32 LB 0x14c8 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b32 'biosfn_load_gfx_8_14_chars'
biosfn_load_gfx_8_14_chars:                  ; 0xc2b32 LB 0x5
    push bp                                   ; 55                          ; 0xc2b32 vgabios.c:1950
    mov bp, sp                                ; 89 e5                       ; 0xc2b33
    pop bp                                    ; 5d                          ; 0xc2b35 vgabios.c:1955
    retn                                      ; c3                          ; 0xc2b36
  ; disGetNextSymbol 0xc2b37 LB 0x14c3 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b37 'biosfn_load_gfx_8_8_dd_chars'
biosfn_load_gfx_8_8_dd_chars:                ; 0xc2b37 LB 0x5
    push bp                                   ; 55                          ; 0xc2b37 vgabios.c:1956
    mov bp, sp                                ; 89 e5                       ; 0xc2b38
    pop bp                                    ; 5d                          ; 0xc2b3a vgabios.c:1961
    retn                                      ; c3                          ; 0xc2b3b
  ; disGetNextSymbol 0xc2b3c LB 0x14be -> off=0x0 cb=0000000000000005 uValue=00000000000c2b3c 'biosfn_load_gfx_8_16_chars'
biosfn_load_gfx_8_16_chars:                  ; 0xc2b3c LB 0x5
    push bp                                   ; 55                          ; 0xc2b3c vgabios.c:1962
    mov bp, sp                                ; 89 e5                       ; 0xc2b3d
    pop bp                                    ; 5d                          ; 0xc2b3f vgabios.c:1967
    retn                                      ; c3                          ; 0xc2b40
  ; disGetNextSymbol 0xc2b41 LB 0x14b9 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b41 'biosfn_alternate_prtsc'
biosfn_alternate_prtsc:                      ; 0xc2b41 LB 0x5
    push bp                                   ; 55                          ; 0xc2b41 vgabios.c:1969
    mov bp, sp                                ; 89 e5                       ; 0xc2b42
    pop bp                                    ; 5d                          ; 0xc2b44 vgabios.c:1974
    retn                                      ; c3                          ; 0xc2b45
  ; disGetNextSymbol 0xc2b46 LB 0x14b4 -> off=0x0 cb=0000000000000005 uValue=00000000000c2b46 'biosfn_switch_video_interface'
biosfn_switch_video_interface:               ; 0xc2b46 LB 0x5
    push bp                                   ; 55                          ; 0xc2b46 vgabios.c:1977
    mov bp, sp                                ; 89 e5                       ; 0xc2b47
    pop bp                                    ; 5d                          ; 0xc2b49 vgabios.c:1982
    retn                                      ; c3                          ; 0xc2b4a
  ; disGetNextSymbol 0xc2b4b LB 0x14af -> off=0x0 cb=0000000000000005 uValue=00000000000c2b4b 'biosfn_enable_video_refresh_control'
biosfn_enable_video_refresh_control:         ; 0xc2b4b LB 0x5
    push bp                                   ; 55                          ; 0xc2b4b vgabios.c:1983
    mov bp, sp                                ; 89 e5                       ; 0xc2b4c
    pop bp                                    ; 5d                          ; 0xc2b4e vgabios.c:1988
    retn                                      ; c3                          ; 0xc2b4f
  ; disGetNextSymbol 0xc2b50 LB 0x14aa -> off=0x0 cb=0000000000000096 uValue=00000000000c2b50 'biosfn_write_string'
biosfn_write_string:                         ; 0xc2b50 LB 0x96
    push bp                                   ; 55                          ; 0xc2b50 vgabios.c:1991
    mov bp, sp                                ; 89 e5                       ; 0xc2b51
    push si                                   ; 56                          ; 0xc2b53
    push di                                   ; 57                          ; 0xc2b54
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc2b55
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2b58
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc2b5b
    mov byte [bp-00ah], bl                    ; 88 5e f6                    ; 0xc2b5e
    mov si, cx                                ; 89 ce                       ; 0xc2b61
    mov di, word [bp+00ah]                    ; 8b 7e 0a                    ; 0xc2b63
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc2b66 vgabios.c:1998
    lea bx, [bp-00eh]                         ; 8d 5e f2                    ; 0xc2b69
    lea dx, [bp-00ch]                         ; 8d 56 f4                    ; 0xc2b6c
    call 00a17h                               ; e8 a5 de                    ; 0xc2b6f
    cmp byte [bp+004h], 0ffh                  ; 80 7e 04 ff                 ; 0xc2b72 vgabios.c:2001
    jne short 02b89h                          ; 75 11                       ; 0xc2b76
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc2b78 vgabios.c:2002
    mov byte [bp+006h], al                    ; 88 46 06                    ; 0xc2b7b
    mov ax, word [bp-00eh]                    ; 8b 46 f2                    ; 0xc2b7e vgabios.c:2003
    xor al, al                                ; 30 c0                       ; 0xc2b81
    shr ax, 008h                              ; c1 e8 08                    ; 0xc2b83
    mov byte [bp+004h], al                    ; 88 46 04                    ; 0xc2b86
    movzx dx, byte [bp+004h]                  ; 0f b6 56 04                 ; 0xc2b89 vgabios.c:2006
    sal dx, 008h                              ; c1 e2 08                    ; 0xc2b8d
    movzx ax, byte [bp+006h]                  ; 0f b6 46 06                 ; 0xc2b90
    add dx, ax                                ; 01 c2                       ; 0xc2b94
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2b96 vgabios.c:2007
    call 011c6h                               ; e8 29 e6                    ; 0xc2b9a
    dec si                                    ; 4e                          ; 0xc2b9d vgabios.c:2009
    cmp si, strict byte 0ffffh                ; 83 fe ff                    ; 0xc2b9e
    je short 02bcdh                           ; 74 2a                       ; 0xc2ba1
    mov bx, di                                ; 89 fb                       ; 0xc2ba3 vgabios.c:2011
    inc di                                    ; 47                          ; 0xc2ba5
    mov es, [bp+008h]                         ; 8e 46 08                    ; 0xc2ba6 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc2ba9
    test byte [bp-008h], 002h                 ; f6 46 f8 02                 ; 0xc2bac vgabios.c:2012
    je short 02bbbh                           ; 74 09                       ; 0xc2bb0
    mov bx, di                                ; 89 fb                       ; 0xc2bb2 vgabios.c:2013
    inc di                                    ; 47                          ; 0xc2bb4
    mov ah, byte [es:bx]                      ; 26 8a 27                    ; 0xc2bb5 vgabios.c:37
    mov byte [bp-00ah], ah                    ; 88 66 f6                    ; 0xc2bb8 vgabios.c:38
    movzx bx, byte [bp-00ah]                  ; 0f b6 5e f6                 ; 0xc2bbb vgabios.c:2015
    movzx dx, byte [bp-006h]                  ; 0f b6 56 fa                 ; 0xc2bbf
    xor ah, ah                                ; 30 e4                       ; 0xc2bc3
    mov cx, strict word 00003h                ; b9 03 00                    ; 0xc2bc5
    call 025f3h                               ; e8 28 fa                    ; 0xc2bc8
    jmp short 02b9dh                          ; eb d0                       ; 0xc2bcb vgabios.c:2016
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc2bcd vgabios.c:2019
    jne short 02bddh                          ; 75 0a                       ; 0xc2bd1
    mov dx, word [bp-00eh]                    ; 8b 56 f2                    ; 0xc2bd3 vgabios.c:2020
    movzx ax, byte [bp-006h]                  ; 0f b6 46 fa                 ; 0xc2bd6
    call 011c6h                               ; e8 e9 e5                    ; 0xc2bda
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc2bdd vgabios.c:2021
    pop di                                    ; 5f                          ; 0xc2be0
    pop si                                    ; 5e                          ; 0xc2be1
    pop bp                                    ; 5d                          ; 0xc2be2
    retn 00008h                               ; c2 08 00                    ; 0xc2be3
  ; disGetNextSymbol 0xc2be6 LB 0x1414 -> off=0x0 cb=00000000000001f2 uValue=00000000000c2be6 'biosfn_read_state_info'
biosfn_read_state_info:                      ; 0xc2be6 LB 0x1f2
    push bp                                   ; 55                          ; 0xc2be6 vgabios.c:2024
    mov bp, sp                                ; 89 e5                       ; 0xc2be7
    push cx                                   ; 51                          ; 0xc2be9
    push si                                   ; 56                          ; 0xc2bea
    push di                                   ; 57                          ; 0xc2beb
    push ax                                   ; 50                          ; 0xc2bec
    push ax                                   ; 50                          ; 0xc2bed
    push dx                                   ; 52                          ; 0xc2bee
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2bef vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2bf2
    mov es, ax                                ; 8e c0                       ; 0xc2bf5
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2bf7
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc2bfa vgabios.c:38
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2bfd vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2c00
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2c03 vgabios.c:48
    mov ax, ds                                ; 8c d8                       ; 0xc2c06 vgabios.c:2035
    mov es, dx                                ; 8e c2                       ; 0xc2c08 vgabios.c:62
    mov word [es:bx], 05502h                  ; 26 c7 07 02 55              ; 0xc2c0a
    mov [es:bx+002h], ds                      ; 26 8c 5f 02                 ; 0xc2c0f
    lea di, [bx+004h]                         ; 8d 7f 04                    ; 0xc2c13 vgabios.c:2040
    mov cx, strict word 0001eh                ; b9 1e 00                    ; 0xc2c16
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2c19
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2c1c
    jcxz 02c27h                               ; e3 06                       ; 0xc2c1f
    push DS                                   ; 1e                          ; 0xc2c21
    mov ds, dx                                ; 8e da                       ; 0xc2c22
    rep movsb                                 ; f3 a4                       ; 0xc2c24
    pop DS                                    ; 1f                          ; 0xc2c26
    mov si, 00084h                            ; be 84 00                    ; 0xc2c27 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c2a
    mov es, ax                                ; 8e c0                       ; 0xc2c2d
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2c2f
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc2c32 vgabios.c:38
    lea si, [bx+022h]                         ; 8d 77 22                    ; 0xc2c34
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2c37 vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2c3a
    lea di, [bx+023h]                         ; 8d 7f 23                    ; 0xc2c3d vgabios.c:2042
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc2c40
    mov si, 00085h                            ; be 85 00                    ; 0xc2c43
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc2c46
    jcxz 02c51h                               ; e3 06                       ; 0xc2c49
    push DS                                   ; 1e                          ; 0xc2c4b
    mov ds, dx                                ; 8e da                       ; 0xc2c4c
    rep movsb                                 ; f3 a4                       ; 0xc2c4e
    pop DS                                    ; 1f                          ; 0xc2c50
    mov si, 0008ah                            ; be 8a 00                    ; 0xc2c51 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2c54
    mov es, ax                                ; 8e c0                       ; 0xc2c57
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2c59
    lea si, [bx+025h]                         ; 8d 77 25                    ; 0xc2c5c vgabios.c:38
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2c5f vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2c62
    lea si, [bx+026h]                         ; 8d 77 26                    ; 0xc2c65 vgabios.c:2045
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2c68 vgabios.c:42
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2c6c vgabios.c:2046
    mov word [es:si], strict word 00010h      ; 26 c7 04 10 00              ; 0xc2c6f vgabios.c:52
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2c74 vgabios.c:2047
    mov byte [es:si], 008h                    ; 26 c6 04 08                 ; 0xc2c77 vgabios.c:42
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2c7b vgabios.c:2048
    mov byte [es:si], 002h                    ; 26 c6 04 02                 ; 0xc2c7e vgabios.c:42
    lea si, [bx+02bh]                         ; 8d 77 2b                    ; 0xc2c82 vgabios.c:2049
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2c85 vgabios.c:42
    lea si, [bx+02ch]                         ; 8d 77 2c                    ; 0xc2c89 vgabios.c:2050
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2c8c vgabios.c:42
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2c90 vgabios.c:2051
    mov byte [es:si], 021h                    ; 26 c6 04 21                 ; 0xc2c93 vgabios.c:42
    lea si, [bx+031h]                         ; 8d 77 31                    ; 0xc2c97 vgabios.c:2052
    mov byte [es:si], 003h                    ; 26 c6 04 03                 ; 0xc2c9a vgabios.c:42
    lea si, [bx+032h]                         ; 8d 77 32                    ; 0xc2c9e vgabios.c:2053
    mov byte [es:si], 000h                    ; 26 c6 04 00                 ; 0xc2ca1 vgabios.c:42
    mov si, 00089h                            ; be 89 00                    ; 0xc2ca5 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2ca8
    mov es, ax                                ; 8e c0                       ; 0xc2cab
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2cad
    mov ah, al                                ; 88 c4                       ; 0xc2cb0 vgabios.c:2058
    and ah, 080h                              ; 80 e4 80                    ; 0xc2cb2
    movzx si, ah                              ; 0f b6 f4                    ; 0xc2cb5
    sar si, 006h                              ; c1 fe 06                    ; 0xc2cb8
    and AL, strict byte 010h                  ; 24 10                       ; 0xc2cbb
    xor ah, ah                                ; 30 e4                       ; 0xc2cbd
    sar ax, 004h                              ; c1 f8 04                    ; 0xc2cbf
    or ax, si                                 ; 09 f0                       ; 0xc2cc2
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc2cc4 vgabios.c:2059
    je short 02cdah                           ; 74 11                       ; 0xc2cc7
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc2cc9
    je short 02cd6h                           ; 74 08                       ; 0xc2ccc
    test ax, ax                               ; 85 c0                       ; 0xc2cce
    jne short 02cdah                          ; 75 08                       ; 0xc2cd0
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2cd2 vgabios.c:2060
    jmp short 02cdch                          ; eb 06                       ; 0xc2cd4
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2cd6 vgabios.c:2061
    jmp short 02cdch                          ; eb 02                       ; 0xc2cd8
    xor al, al                                ; 30 c0                       ; 0xc2cda vgabios.c:2063
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2cdc vgabios.c:2065
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2cdf vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2ce2
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2ce5 vgabios.c:2068
    cmp AL, strict byte 00eh                  ; 3c 0e                       ; 0xc2ce8
    jc short 02d0bh                           ; 72 1f                       ; 0xc2cea
    cmp AL, strict byte 012h                  ; 3c 12                       ; 0xc2cec
    jnbe short 02d0bh                         ; 77 1b                       ; 0xc2cee
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2cf0 vgabios.c:2069
    test ax, ax                               ; 85 c0                       ; 0xc2cf3
    je short 02d4dh                           ; 74 56                       ; 0xc2cf5
    mov si, ax                                ; 89 c6                       ; 0xc2cf7 vgabios.c:2070
    shr si, 002h                              ; c1 ee 02                    ; 0xc2cf9
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2cfc
    xor dx, dx                                ; 31 d2                       ; 0xc2cff
    div si                                    ; f7 f6                       ; 0xc2d01
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2d03
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2d06 vgabios.c:42
    jmp short 02d4dh                          ; eb 42                       ; 0xc2d09 vgabios.c:2071
    lea si, [bx+029h]                         ; 8d 77 29                    ; 0xc2d0b
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2d0e
    cmp AL, strict byte 013h                  ; 3c 13                       ; 0xc2d11
    jne short 02d26h                          ; 75 11                       ; 0xc2d13
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d15 vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2d18
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2d1c vgabios.c:2073
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc2d1f vgabios.c:52
    jmp short 02d4dh                          ; eb 27                       ; 0xc2d24 vgabios.c:2074
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2d26
    jc short 02d4dh                           ; 72 23                       ; 0xc2d28
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2d2a
    jnbe short 02d4dh                         ; 77 1f                       ; 0xc2d2c
    cmp word [bp-00ah], strict byte 00000h    ; 83 7e f6 00                 ; 0xc2d2e vgabios.c:2076
    je short 02d42h                           ; 74 0e                       ; 0xc2d32
    mov ax, 04000h                            ; b8 00 40                    ; 0xc2d34 vgabios.c:2077
    xor dx, dx                                ; 31 d2                       ; 0xc2d37
    div word [bp-00ah]                        ; f7 76 f6                    ; 0xc2d39
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d3c vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2d3f
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2d42 vgabios.c:2078
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d45 vgabios.c:52
    mov word [es:si], strict word 00004h      ; 26 c7 04 04 00              ; 0xc2d48
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2d4d vgabios.c:2080
    cmp AL, strict byte 006h                  ; 3c 06                       ; 0xc2d50
    je short 02d58h                           ; 74 04                       ; 0xc2d52
    cmp AL, strict byte 011h                  ; 3c 11                       ; 0xc2d54
    jne short 02d63h                          ; 75 0b                       ; 0xc2d56
    lea si, [bx+027h]                         ; 8d 77 27                    ; 0xc2d58 vgabios.c:2081
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d5b vgabios.c:52
    mov word [es:si], strict word 00002h      ; 26 c7 04 02 00              ; 0xc2d5e
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2d63 vgabios.c:2083
    cmp AL, strict byte 004h                  ; 3c 04                       ; 0xc2d66
    jc short 02dc1h                           ; 72 57                       ; 0xc2d68
    cmp AL, strict byte 007h                  ; 3c 07                       ; 0xc2d6a
    je short 02dc1h                           ; 74 53                       ; 0xc2d6c
    lea si, [bx+02dh]                         ; 8d 77 2d                    ; 0xc2d6e vgabios.c:2084
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2d71 vgabios.c:42
    mov byte [es:si], 001h                    ; 26 c6 04 01                 ; 0xc2d74
    mov si, 00084h                            ; be 84 00                    ; 0xc2d78 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2d7b
    mov es, ax                                ; 8e c0                       ; 0xc2d7e
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2d80
    movzx di, al                              ; 0f b6 f8                    ; 0xc2d83 vgabios.c:38
    inc di                                    ; 47                          ; 0xc2d86
    mov si, 00085h                            ; be 85 00                    ; 0xc2d87 vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2d8a
    xor ah, ah                                ; 30 e4                       ; 0xc2d8d vgabios.c:38
    imul ax, di                               ; 0f af c7                    ; 0xc2d8f
    cmp ax, 0015eh                            ; 3d 5e 01                    ; 0xc2d92 vgabios.c:2086
    jc short 02da5h                           ; 72 0e                       ; 0xc2d95
    jbe short 02daeh                          ; 76 15                       ; 0xc2d97
    cmp ax, 001e0h                            ; 3d e0 01                    ; 0xc2d99
    je short 02db6h                           ; 74 18                       ; 0xc2d9c
    cmp ax, 00190h                            ; 3d 90 01                    ; 0xc2d9e
    je short 02db2h                           ; 74 0f                       ; 0xc2da1
    jmp short 02db6h                          ; eb 11                       ; 0xc2da3
    cmp ax, 000c8h                            ; 3d c8 00                    ; 0xc2da5
    jne short 02db6h                          ; 75 0c                       ; 0xc2da8
    xor al, al                                ; 30 c0                       ; 0xc2daa vgabios.c:2087
    jmp short 02db8h                          ; eb 0a                       ; 0xc2dac
    mov AL, strict byte 001h                  ; b0 01                       ; 0xc2dae vgabios.c:2088
    jmp short 02db8h                          ; eb 06                       ; 0xc2db0
    mov AL, strict byte 002h                  ; b0 02                       ; 0xc2db2 vgabios.c:2089
    jmp short 02db8h                          ; eb 02                       ; 0xc2db4
    mov AL, strict byte 003h                  ; b0 03                       ; 0xc2db6 vgabios.c:2091
    lea si, [bx+02ah]                         ; 8d 77 2a                    ; 0xc2db8 vgabios.c:2093
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2dbb vgabios.c:42
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc2dbe
    lea di, [bx+033h]                         ; 8d 7f 33                    ; 0xc2dc1 vgabios.c:2096
    mov cx, strict word 0000dh                ; b9 0d 00                    ; 0xc2dc4
    xor ax, ax                                ; 31 c0                       ; 0xc2dc7
    mov es, [bp-00ch]                         ; 8e 46 f4                    ; 0xc2dc9
    jcxz 02dd0h                               ; e3 02                       ; 0xc2dcc
    rep stosb                                 ; f3 aa                       ; 0xc2dce
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc2dd0 vgabios.c:2097
    pop di                                    ; 5f                          ; 0xc2dd3
    pop si                                    ; 5e                          ; 0xc2dd4
    pop cx                                    ; 59                          ; 0xc2dd5
    pop bp                                    ; 5d                          ; 0xc2dd6
    retn                                      ; c3                          ; 0xc2dd7
  ; disGetNextSymbol 0xc2dd8 LB 0x1222 -> off=0x0 cb=0000000000000023 uValue=00000000000c2dd8 'biosfn_read_video_state_size2'
biosfn_read_video_state_size2:               ; 0xc2dd8 LB 0x23
    push dx                                   ; 52                          ; 0xc2dd8 vgabios.c:2100
    push bp                                   ; 55                          ; 0xc2dd9
    mov bp, sp                                ; 89 e5                       ; 0xc2dda
    mov dx, ax                                ; 89 c2                       ; 0xc2ddc
    xor ax, ax                                ; 31 c0                       ; 0xc2dde vgabios.c:2104
    test dl, 001h                             ; f6 c2 01                    ; 0xc2de0 vgabios.c:2105
    je short 02de8h                           ; 74 03                       ; 0xc2de3
    mov ax, strict word 00046h                ; b8 46 00                    ; 0xc2de5 vgabios.c:2106
    test dl, 002h                             ; f6 c2 02                    ; 0xc2de8 vgabios.c:2108
    je short 02df0h                           ; 74 03                       ; 0xc2deb
    add ax, strict word 0002ah                ; 05 2a 00                    ; 0xc2ded vgabios.c:2109
    test dl, 004h                             ; f6 c2 04                    ; 0xc2df0 vgabios.c:2111
    je short 02df8h                           ; 74 03                       ; 0xc2df3
    add ax, 00304h                            ; 05 04 03                    ; 0xc2df5 vgabios.c:2112
    pop bp                                    ; 5d                          ; 0xc2df8 vgabios.c:2115
    pop dx                                    ; 5a                          ; 0xc2df9
    retn                                      ; c3                          ; 0xc2dfa
  ; disGetNextSymbol 0xc2dfb LB 0x11ff -> off=0x0 cb=0000000000000018 uValue=00000000000c2dfb 'vga_get_video_state_size'
vga_get_video_state_size:                    ; 0xc2dfb LB 0x18
    push bp                                   ; 55                          ; 0xc2dfb vgabios.c:2117
    mov bp, sp                                ; 89 e5                       ; 0xc2dfc
    push bx                                   ; 53                          ; 0xc2dfe
    mov bx, dx                                ; 89 d3                       ; 0xc2dff
    call 02dd8h                               ; e8 d4 ff                    ; 0xc2e01 vgabios.c:2120
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc2e04
    shr ax, 006h                              ; c1 e8 06                    ; 0xc2e07
    mov word [ss:bx], ax                      ; 36 89 07                    ; 0xc2e0a
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc2e0d vgabios.c:2121
    pop bx                                    ; 5b                          ; 0xc2e10
    pop bp                                    ; 5d                          ; 0xc2e11
    retn                                      ; c3                          ; 0xc2e12
  ; disGetNextSymbol 0xc2e13 LB 0x11e7 -> off=0x0 cb=00000000000002d6 uValue=00000000000c2e13 'biosfn_save_video_state'
biosfn_save_video_state:                     ; 0xc2e13 LB 0x2d6
    push bp                                   ; 55                          ; 0xc2e13 vgabios.c:2123
    mov bp, sp                                ; 89 e5                       ; 0xc2e14
    push cx                                   ; 51                          ; 0xc2e16
    push si                                   ; 56                          ; 0xc2e17
    push di                                   ; 57                          ; 0xc2e18
    push ax                                   ; 50                          ; 0xc2e19
    push ax                                   ; 50                          ; 0xc2e1a
    push ax                                   ; 50                          ; 0xc2e1b
    mov cx, dx                                ; 89 d1                       ; 0xc2e1c
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2e1e vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2e21
    mov es, ax                                ; 8e c0                       ; 0xc2e24
    mov di, word [es:si]                      ; 26 8b 3c                    ; 0xc2e26
    mov si, di                                ; 89 fe                       ; 0xc2e29 vgabios.c:48
    test byte [bp-00ch], 001h                 ; f6 46 f4 01                 ; 0xc2e2b vgabios.c:2128
    je near 02f46h                            ; 0f 84 13 01                 ; 0xc2e2f
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2e33 vgabios.c:2129
    in AL, DX                                 ; ec                          ; 0xc2e36
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e37
    mov es, cx                                ; 8e c1                       ; 0xc2e39 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e3b
    inc bx                                    ; 43                          ; 0xc2e3e vgabios.c:2129
    mov dx, di                                ; 89 fa                       ; 0xc2e3f
    in AL, DX                                 ; ec                          ; 0xc2e41
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e44 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2e47 vgabios.c:2130
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2e48
    in AL, DX                                 ; ec                          ; 0xc2e4b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e4c
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e4e vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2e51 vgabios.c:2131
    mov dx, 003dah                            ; ba da 03                    ; 0xc2e52
    in AL, DX                                 ; ec                          ; 0xc2e55
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e56
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2e58 vgabios.c:2133
    in AL, DX                                 ; ec                          ; 0xc2e5b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e5c
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc2e5e
    mov al, byte [bp-00ah]                    ; 8a 46 f6                    ; 0xc2e61 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e64
    inc bx                                    ; 43                          ; 0xc2e67 vgabios.c:2134
    mov dx, 003cah                            ; ba ca 03                    ; 0xc2e68
    in AL, DX                                 ; ec                          ; 0xc2e6b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e6c
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e6e vgabios.c:42
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc2e71 vgabios.c:2137
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc2e74
    add bx, ax                                ; 01 c3                       ; 0xc2e77 vgabios.c:2135
    jmp short 02e81h                          ; eb 06                       ; 0xc2e79
    cmp word [bp-008h], strict byte 00004h    ; 83 7e f8 04                 ; 0xc2e7b
    jnbe short 02e99h                         ; 77 18                       ; 0xc2e7f
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2e81 vgabios.c:2138
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2e84
    out DX, AL                                ; ee                          ; 0xc2e87
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2e88 vgabios.c:2139
    in AL, DX                                 ; ec                          ; 0xc2e8b
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2e8c
    mov es, cx                                ; 8e c1                       ; 0xc2e8e vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2e90
    inc bx                                    ; 43                          ; 0xc2e93 vgabios.c:2139
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2e94 vgabios.c:2140
    jmp short 02e7bh                          ; eb e2                       ; 0xc2e97
    xor al, al                                ; 30 c0                       ; 0xc2e99 vgabios.c:2141
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc2e9b
    out DX, AL                                ; ee                          ; 0xc2e9e
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc2e9f vgabios.c:2142
    in AL, DX                                 ; ec                          ; 0xc2ea2
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ea3
    mov es, cx                                ; 8e c1                       ; 0xc2ea5 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2ea7
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2eaa vgabios.c:2144
    inc bx                                    ; 43                          ; 0xc2eaf vgabios.c:2142
    jmp short 02eb8h                          ; eb 06                       ; 0xc2eb0
    cmp word [bp-008h], strict byte 00018h    ; 83 7e f8 18                 ; 0xc2eb2
    jnbe short 02ecfh                         ; 77 17                       ; 0xc2eb6
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2eb8 vgabios.c:2145
    mov dx, si                                ; 89 f2                       ; 0xc2ebb
    out DX, AL                                ; ee                          ; 0xc2ebd
    lea dx, [si+001h]                         ; 8d 54 01                    ; 0xc2ebe vgabios.c:2146
    in AL, DX                                 ; ec                          ; 0xc2ec1
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ec2
    mov es, cx                                ; 8e c1                       ; 0xc2ec4 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2ec6
    inc bx                                    ; 43                          ; 0xc2ec9 vgabios.c:2146
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2eca vgabios.c:2147
    jmp short 02eb2h                          ; eb e3                       ; 0xc2ecd
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2ecf vgabios.c:2149
    jmp short 02edch                          ; eb 06                       ; 0xc2ed4
    cmp word [bp-008h], strict byte 00013h    ; 83 7e f8 13                 ; 0xc2ed6
    jnbe short 02f00h                         ; 77 24                       ; 0xc2eda
    mov dx, 003dah                            ; ba da 03                    ; 0xc2edc vgabios.c:2150
    in AL, DX                                 ; ec                          ; 0xc2edf
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ee0
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc2ee2 vgabios.c:2151
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc2ee5
    or ax, word [bp-008h]                     ; 0b 46 f8                    ; 0xc2ee8
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc2eeb
    out DX, AL                                ; ee                          ; 0xc2eee
    mov dx, 003c1h                            ; ba c1 03                    ; 0xc2eef vgabios.c:2152
    in AL, DX                                 ; ec                          ; 0xc2ef2
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2ef3
    mov es, cx                                ; 8e c1                       ; 0xc2ef5 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2ef7
    inc bx                                    ; 43                          ; 0xc2efa vgabios.c:2152
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2efb vgabios.c:2153
    jmp short 02ed6h                          ; eb d6                       ; 0xc2efe
    mov dx, 003dah                            ; ba da 03                    ; 0xc2f00 vgabios.c:2154
    in AL, DX                                 ; ec                          ; 0xc2f03
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2f04
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2f06 vgabios.c:2156
    jmp short 02f13h                          ; eb 06                       ; 0xc2f0b
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc2f0d
    jnbe short 02f2bh                         ; 77 18                       ; 0xc2f11
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc2f13 vgabios.c:2157
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc2f16
    out DX, AL                                ; ee                          ; 0xc2f19
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc2f1a vgabios.c:2158
    in AL, DX                                 ; ec                          ; 0xc2f1d
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc2f1e
    mov es, cx                                ; 8e c1                       ; 0xc2f20 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2f22
    inc bx                                    ; 43                          ; 0xc2f25 vgabios.c:2158
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc2f26 vgabios.c:2159
    jmp short 02f0dh                          ; eb e2                       ; 0xc2f29
    mov es, cx                                ; 8e c1                       ; 0xc2f2b vgabios.c:52
    mov word [es:bx], si                      ; 26 89 37                    ; 0xc2f2d
    inc bx                                    ; 43                          ; 0xc2f30 vgabios.c:2161
    inc bx                                    ; 43                          ; 0xc2f31
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2f32 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2f36 vgabios.c:2164
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2f37 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2f3b vgabios.c:2165
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2f3c vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2f40 vgabios.c:2166
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc2f41 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc2f45 vgabios.c:2167
    test byte [bp-00ch], 002h                 ; f6 46 f4 02                 ; 0xc2f46 vgabios.c:2169
    je near 0308dh                            ; 0f 84 3f 01                 ; 0xc2f4a
    mov si, strict word 00049h                ; be 49 00                    ; 0xc2f4e vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f51
    mov es, ax                                ; 8e c0                       ; 0xc2f54
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f56
    mov es, cx                                ; 8e c1                       ; 0xc2f59 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2f5b
    inc bx                                    ; 43                          ; 0xc2f5e vgabios.c:2170
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc2f5f vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f62
    mov es, ax                                ; 8e c0                       ; 0xc2f65
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2f67
    mov es, cx                                ; 8e c1                       ; 0xc2f6a vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2f6c
    inc bx                                    ; 43                          ; 0xc2f6f vgabios.c:2171
    inc bx                                    ; 43                          ; 0xc2f70
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc2f71 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f74
    mov es, ax                                ; 8e c0                       ; 0xc2f77
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2f79
    mov es, cx                                ; 8e c1                       ; 0xc2f7c vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2f7e
    inc bx                                    ; 43                          ; 0xc2f81 vgabios.c:2172
    inc bx                                    ; 43                          ; 0xc2f82
    mov si, strict word 00063h                ; be 63 00                    ; 0xc2f83 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f86
    mov es, ax                                ; 8e c0                       ; 0xc2f89
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2f8b
    mov es, cx                                ; 8e c1                       ; 0xc2f8e vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2f90
    inc bx                                    ; 43                          ; 0xc2f93 vgabios.c:2173
    inc bx                                    ; 43                          ; 0xc2f94
    mov si, 00084h                            ; be 84 00                    ; 0xc2f95 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2f98
    mov es, ax                                ; 8e c0                       ; 0xc2f9b
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2f9d
    mov es, cx                                ; 8e c1                       ; 0xc2fa0 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2fa2
    inc bx                                    ; 43                          ; 0xc2fa5 vgabios.c:2174
    mov si, 00085h                            ; be 85 00                    ; 0xc2fa6 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fa9
    mov es, ax                                ; 8e c0                       ; 0xc2fac
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2fae
    mov es, cx                                ; 8e c1                       ; 0xc2fb1 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2fb3
    inc bx                                    ; 43                          ; 0xc2fb6 vgabios.c:2175
    inc bx                                    ; 43                          ; 0xc2fb7
    mov si, 00087h                            ; be 87 00                    ; 0xc2fb8 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fbb
    mov es, ax                                ; 8e c0                       ; 0xc2fbe
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2fc0
    mov es, cx                                ; 8e c1                       ; 0xc2fc3 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2fc5
    inc bx                                    ; 43                          ; 0xc2fc8 vgabios.c:2176
    mov si, 00088h                            ; be 88 00                    ; 0xc2fc9 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fcc
    mov es, ax                                ; 8e c0                       ; 0xc2fcf
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2fd1
    mov es, cx                                ; 8e c1                       ; 0xc2fd4 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2fd6
    inc bx                                    ; 43                          ; 0xc2fd9 vgabios.c:2177
    mov si, 00089h                            ; be 89 00                    ; 0xc2fda vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fdd
    mov es, ax                                ; 8e c0                       ; 0xc2fe0
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc2fe2
    mov es, cx                                ; 8e c1                       ; 0xc2fe5 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc2fe7
    inc bx                                    ; 43                          ; 0xc2fea vgabios.c:2178
    mov si, strict word 00060h                ; be 60 00                    ; 0xc2feb vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc2fee
    mov es, ax                                ; 8e c0                       ; 0xc2ff1
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc2ff3
    mov es, cx                                ; 8e c1                       ; 0xc2ff6 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc2ff8
    mov word [bp-008h], strict word 00000h    ; c7 46 f8 00 00              ; 0xc2ffb vgabios.c:2180
    inc bx                                    ; 43                          ; 0xc3000 vgabios.c:2179
    inc bx                                    ; 43                          ; 0xc3001
    jmp short 0300ah                          ; eb 06                       ; 0xc3002
    cmp word [bp-008h], strict byte 00008h    ; 83 7e f8 08                 ; 0xc3004
    jnc short 03026h                          ; 73 1c                       ; 0xc3008
    mov si, word [bp-008h]                    ; 8b 76 f8                    ; 0xc300a vgabios.c:2181
    add si, si                                ; 01 f6                       ; 0xc300d
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc300f
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3012 vgabios.c:47
    mov es, ax                                ; 8e c0                       ; 0xc3015
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3017
    mov es, cx                                ; 8e c1                       ; 0xc301a vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc301c
    inc bx                                    ; 43                          ; 0xc301f vgabios.c:2182
    inc bx                                    ; 43                          ; 0xc3020
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc3021 vgabios.c:2183
    jmp short 03004h                          ; eb de                       ; 0xc3024
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc3026 vgabios.c:47
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3029
    mov es, ax                                ; 8e c0                       ; 0xc302c
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc302e
    mov es, cx                                ; 8e c1                       ; 0xc3031 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3033
    inc bx                                    ; 43                          ; 0xc3036 vgabios.c:2184
    inc bx                                    ; 43                          ; 0xc3037
    mov si, strict word 00062h                ; be 62 00                    ; 0xc3038 vgabios.c:37
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc303b
    mov es, ax                                ; 8e c0                       ; 0xc303e
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3040
    mov es, cx                                ; 8e c1                       ; 0xc3043 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3045
    inc bx                                    ; 43                          ; 0xc3048 vgabios.c:2185
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc3049 vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc304c
    mov es, ax                                ; 8e c0                       ; 0xc304e
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3050
    mov es, cx                                ; 8e c1                       ; 0xc3053 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3055
    inc bx                                    ; 43                          ; 0xc3058 vgabios.c:2187
    inc bx                                    ; 43                          ; 0xc3059
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc305a vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc305d
    mov es, ax                                ; 8e c0                       ; 0xc305f
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3061
    mov es, cx                                ; 8e c1                       ; 0xc3064 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3066
    inc bx                                    ; 43                          ; 0xc3069 vgabios.c:2188
    inc bx                                    ; 43                          ; 0xc306a
    mov si, 0010ch                            ; be 0c 01                    ; 0xc306b vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc306e
    mov es, ax                                ; 8e c0                       ; 0xc3070
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3072
    mov es, cx                                ; 8e c1                       ; 0xc3075 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3077
    inc bx                                    ; 43                          ; 0xc307a vgabios.c:2189
    inc bx                                    ; 43                          ; 0xc307b
    mov si, 0010eh                            ; be 0e 01                    ; 0xc307c vgabios.c:47
    xor ax, ax                                ; 31 c0                       ; 0xc307f
    mov es, ax                                ; 8e c0                       ; 0xc3081
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3083
    mov es, cx                                ; 8e c1                       ; 0xc3086 vgabios.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3088
    inc bx                                    ; 43                          ; 0xc308b vgabios.c:2190
    inc bx                                    ; 43                          ; 0xc308c
    test byte [bp-00ch], 004h                 ; f6 46 f4 04                 ; 0xc308d vgabios.c:2192
    je short 030dfh                           ; 74 4c                       ; 0xc3091
    mov dx, 003c7h                            ; ba c7 03                    ; 0xc3093 vgabios.c:2194
    in AL, DX                                 ; ec                          ; 0xc3096
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3097
    mov es, cx                                ; 8e c1                       ; 0xc3099 vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc309b
    inc bx                                    ; 43                          ; 0xc309e vgabios.c:2194
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc309f
    in AL, DX                                 ; ec                          ; 0xc30a2
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30a3
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30a5 vgabios.c:42
    inc bx                                    ; 43                          ; 0xc30a8 vgabios.c:2195
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc30a9
    in AL, DX                                 ; ec                          ; 0xc30ac
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30ad
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30af vgabios.c:42
    inc bx                                    ; 43                          ; 0xc30b2 vgabios.c:2196
    xor al, al                                ; 30 c0                       ; 0xc30b3
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc30b5
    out DX, AL                                ; ee                          ; 0xc30b8
    xor ah, ah                                ; 30 e4                       ; 0xc30b9 vgabios.c:2199
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc30bb
    jmp short 030c7h                          ; eb 07                       ; 0xc30be
    cmp word [bp-008h], 00300h                ; 81 7e f8 00 03              ; 0xc30c0
    jnc short 030d8h                          ; 73 11                       ; 0xc30c5
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc30c7 vgabios.c:2200
    in AL, DX                                 ; ec                          ; 0xc30ca
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc30cb
    mov es, cx                                ; 8e c1                       ; 0xc30cd vgabios.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc30cf
    inc bx                                    ; 43                          ; 0xc30d2 vgabios.c:2200
    inc word [bp-008h]                        ; ff 46 f8                    ; 0xc30d3 vgabios.c:2201
    jmp short 030c0h                          ; eb e8                       ; 0xc30d6
    mov es, cx                                ; 8e c1                       ; 0xc30d8 vgabios.c:42
    mov byte [es:bx], 000h                    ; 26 c6 07 00                 ; 0xc30da
    inc bx                                    ; 43                          ; 0xc30de vgabios.c:2202
    mov ax, bx                                ; 89 d8                       ; 0xc30df vgabios.c:2205
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc30e1
    pop di                                    ; 5f                          ; 0xc30e4
    pop si                                    ; 5e                          ; 0xc30e5
    pop cx                                    ; 59                          ; 0xc30e6
    pop bp                                    ; 5d                          ; 0xc30e7
    retn                                      ; c3                          ; 0xc30e8
  ; disGetNextSymbol 0xc30e9 LB 0xf11 -> off=0x0 cb=00000000000002b8 uValue=00000000000c30e9 'biosfn_restore_video_state'
biosfn_restore_video_state:                  ; 0xc30e9 LB 0x2b8
    push bp                                   ; 55                          ; 0xc30e9 vgabios.c:2207
    mov bp, sp                                ; 89 e5                       ; 0xc30ea
    push cx                                   ; 51                          ; 0xc30ec
    push si                                   ; 56                          ; 0xc30ed
    push di                                   ; 57                          ; 0xc30ee
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc30ef
    push ax                                   ; 50                          ; 0xc30f2
    mov cx, dx                                ; 89 d1                       ; 0xc30f3
    test byte [bp-010h], 001h                 ; f6 46 f0 01                 ; 0xc30f5 vgabios.c:2211
    je near 03231h                            ; 0f 84 34 01                 ; 0xc30f9
    mov dx, 003dah                            ; ba da 03                    ; 0xc30fd vgabios.c:2213
    in AL, DX                                 ; ec                          ; 0xc3100
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3101
    lea si, [bx+040h]                         ; 8d 77 40                    ; 0xc3103 vgabios.c:2215
    mov es, cx                                ; 8e c1                       ; 0xc3106 vgabios.c:47
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3108
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc310b vgabios.c:48
    mov si, bx                                ; 89 de                       ; 0xc310e vgabios.c:2216
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3110 vgabios.c:2219
    add bx, strict byte 00005h                ; 83 c3 05                    ; 0xc3115 vgabios.c:2217
    jmp short 03120h                          ; eb 06                       ; 0xc3118
    cmp word [bp-00eh], strict byte 00004h    ; 83 7e f2 04                 ; 0xc311a
    jnbe short 03136h                         ; 77 16                       ; 0xc311e
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3120 vgabios.c:2220
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3123
    out DX, AL                                ; ee                          ; 0xc3126
    mov es, cx                                ; 8e c1                       ; 0xc3127 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3129
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc312c vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc312f
    inc bx                                    ; 43                          ; 0xc3130 vgabios.c:2221
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc3131 vgabios.c:2222
    jmp short 0311ah                          ; eb e4                       ; 0xc3134
    xor al, al                                ; 30 c0                       ; 0xc3136 vgabios.c:2223
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3138
    out DX, AL                                ; ee                          ; 0xc313b
    mov es, cx                                ; 8e c1                       ; 0xc313c vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc313e
    mov dx, 003c5h                            ; ba c5 03                    ; 0xc3141 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3144
    inc bx                                    ; 43                          ; 0xc3145 vgabios.c:2224
    mov ax, strict word 00011h                ; b8 11 00                    ; 0xc3146
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3149
    out DX, ax                                ; ef                          ; 0xc314c
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc314d vgabios.c:2229
    jmp short 0315ah                          ; eb 06                       ; 0xc3152
    cmp word [bp-00eh], strict byte 00018h    ; 83 7e f2 18                 ; 0xc3154
    jnbe short 03174h                         ; 77 1a                       ; 0xc3158
    cmp word [bp-00eh], strict byte 00011h    ; 83 7e f2 11                 ; 0xc315a vgabios.c:2230
    je short 0316eh                           ; 74 0e                       ; 0xc315e
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc3160 vgabios.c:2231
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3163
    out DX, AL                                ; ee                          ; 0xc3166
    mov es, cx                                ; 8e c1                       ; 0xc3167 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3169
    inc dx                                    ; 42                          ; 0xc316c vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc316d
    inc bx                                    ; 43                          ; 0xc316e vgabios.c:2234
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc316f vgabios.c:2235
    jmp short 03154h                          ; eb e0                       ; 0xc3172
    mov dx, 003cch                            ; ba cc 03                    ; 0xc3174 vgabios.c:2237
    in AL, DX                                 ; ec                          ; 0xc3177
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc3178
    and AL, strict byte 0feh                  ; 24 fe                       ; 0xc317a
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc317c
    cmp word [bp-00ah], 003d4h                ; 81 7e f6 d4 03              ; 0xc317f vgabios.c:2238
    jne short 0318ah                          ; 75 04                       ; 0xc3184
    or byte [bp-008h], 001h                   ; 80 4e f8 01                 ; 0xc3186 vgabios.c:2239
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc318a vgabios.c:2240
    mov dx, 003c2h                            ; ba c2 03                    ; 0xc318d
    out DX, AL                                ; ee                          ; 0xc3190
    mov AL, strict byte 011h                  ; b0 11                       ; 0xc3191 vgabios.c:2243
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3193
    out DX, AL                                ; ee                          ; 0xc3196
    lea di, [word bx-00007h]                  ; 8d bf f9 ff                 ; 0xc3197 vgabios.c:2244
    mov es, cx                                ; 8e c1                       ; 0xc319b vgabios.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc319d
    inc dx                                    ; 42                          ; 0xc31a0 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc31a1
    lea di, [si+003h]                         ; 8d 7c 03                    ; 0xc31a2 vgabios.c:2247
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc31a5 vgabios.c:37
    xor ah, ah                                ; 30 e4                       ; 0xc31a8 vgabios.c:38
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc31aa
    mov dx, 003dah                            ; ba da 03                    ; 0xc31ad vgabios.c:2248
    in AL, DX                                 ; ec                          ; 0xc31b0
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31b1
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc31b3 vgabios.c:2249
    jmp short 031c0h                          ; eb 06                       ; 0xc31b8
    cmp word [bp-00eh], strict byte 00013h    ; 83 7e f2 13                 ; 0xc31ba
    jnbe short 031d9h                         ; 77 19                       ; 0xc31be
    mov ax, word [bp-00ch]                    ; 8b 46 f4                    ; 0xc31c0 vgabios.c:2250
    and ax, strict word 00020h                ; 25 20 00                    ; 0xc31c3
    or ax, word [bp-00eh]                     ; 0b 46 f2                    ; 0xc31c6
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc31c9
    out DX, AL                                ; ee                          ; 0xc31cc
    mov es, cx                                ; 8e c1                       ; 0xc31cd vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc31cf
    out DX, AL                                ; ee                          ; 0xc31d2 vgabios.c:38
    inc bx                                    ; 43                          ; 0xc31d3 vgabios.c:2251
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc31d4 vgabios.c:2252
    jmp short 031bah                          ; eb e1                       ; 0xc31d7
    mov al, byte [bp-00ch]                    ; 8a 46 f4                    ; 0xc31d9 vgabios.c:2253
    mov dx, 003c0h                            ; ba c0 03                    ; 0xc31dc
    out DX, AL                                ; ee                          ; 0xc31df
    mov dx, 003dah                            ; ba da 03                    ; 0xc31e0 vgabios.c:2254
    in AL, DX                                 ; ec                          ; 0xc31e3
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc31e4
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc31e6 vgabios.c:2256
    jmp short 031f3h                          ; eb 06                       ; 0xc31eb
    cmp word [bp-00eh], strict byte 00008h    ; 83 7e f2 08                 ; 0xc31ed
    jnbe short 03209h                         ; 77 16                       ; 0xc31f1
    mov al, byte [bp-00eh]                    ; 8a 46 f2                    ; 0xc31f3 vgabios.c:2257
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc31f6
    out DX, AL                                ; ee                          ; 0xc31f9
    mov es, cx                                ; 8e c1                       ; 0xc31fa vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc31fc
    mov dx, 003cfh                            ; ba cf 03                    ; 0xc31ff vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3202
    inc bx                                    ; 43                          ; 0xc3203 vgabios.c:2258
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc3204 vgabios.c:2259
    jmp short 031edh                          ; eb e4                       ; 0xc3207
    add bx, strict byte 00006h                ; 83 c3 06                    ; 0xc3209 vgabios.c:2260
    mov es, cx                                ; 8e c1                       ; 0xc320c vgabios.c:37
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc320e
    mov dx, 003c4h                            ; ba c4 03                    ; 0xc3211 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3214
    inc si                                    ; 46                          ; 0xc3215 vgabios.c:2263
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3216 vgabios.c:37
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc3219 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc321c
    inc si                                    ; 46                          ; 0xc321d vgabios.c:2264
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc321e vgabios.c:37
    mov dx, 003ceh                            ; ba ce 03                    ; 0xc3221 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3224
    inc si                                    ; 46                          ; 0xc3225 vgabios.c:2265
    inc si                                    ; 46                          ; 0xc3226
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3227 vgabios.c:37
    mov dx, word [bp-00ah]                    ; 8b 56 f6                    ; 0xc322a vgabios.c:38
    add dx, strict byte 00006h                ; 83 c2 06                    ; 0xc322d
    out DX, AL                                ; ee                          ; 0xc3230
    test byte [bp-010h], 002h                 ; f6 46 f0 02                 ; 0xc3231 vgabios.c:2269
    je near 03354h                            ; 0f 84 1b 01                 ; 0xc3235
    mov es, cx                                ; 8e c1                       ; 0xc3239 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc323b
    mov si, strict word 00049h                ; be 49 00                    ; 0xc323e vgabios.c:42
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3241
    mov es, dx                                ; 8e c2                       ; 0xc3244
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3246
    inc bx                                    ; 43                          ; 0xc3249 vgabios.c:2270
    mov es, cx                                ; 8e c1                       ; 0xc324a vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc324c
    mov si, strict word 0004ah                ; be 4a 00                    ; 0xc324f vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3252
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3254
    inc bx                                    ; 43                          ; 0xc3257 vgabios.c:2271
    inc bx                                    ; 43                          ; 0xc3258
    mov es, cx                                ; 8e c1                       ; 0xc3259 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc325b
    mov si, strict word 0004ch                ; be 4c 00                    ; 0xc325e vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3261
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3263
    inc bx                                    ; 43                          ; 0xc3266 vgabios.c:2272
    inc bx                                    ; 43                          ; 0xc3267
    mov es, cx                                ; 8e c1                       ; 0xc3268 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc326a
    mov si, strict word 00063h                ; be 63 00                    ; 0xc326d vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc3270
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3272
    inc bx                                    ; 43                          ; 0xc3275 vgabios.c:2273
    inc bx                                    ; 43                          ; 0xc3276
    mov es, cx                                ; 8e c1                       ; 0xc3277 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3279
    mov si, 00084h                            ; be 84 00                    ; 0xc327c vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc327f
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3281
    inc bx                                    ; 43                          ; 0xc3284 vgabios.c:2274
    mov es, cx                                ; 8e c1                       ; 0xc3285 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3287
    mov si, 00085h                            ; be 85 00                    ; 0xc328a vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc328d
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc328f
    inc bx                                    ; 43                          ; 0xc3292 vgabios.c:2275
    inc bx                                    ; 43                          ; 0xc3293
    mov es, cx                                ; 8e c1                       ; 0xc3294 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3296
    mov si, 00087h                            ; be 87 00                    ; 0xc3299 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc329c
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc329e
    inc bx                                    ; 43                          ; 0xc32a1 vgabios.c:2276
    mov es, cx                                ; 8e c1                       ; 0xc32a2 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc32a4
    mov si, 00088h                            ; be 88 00                    ; 0xc32a7 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc32aa
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32ac
    inc bx                                    ; 43                          ; 0xc32af vgabios.c:2277
    mov es, cx                                ; 8e c1                       ; 0xc32b0 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc32b2
    mov si, 00089h                            ; be 89 00                    ; 0xc32b5 vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc32b8
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc32ba
    inc bx                                    ; 43                          ; 0xc32bd vgabios.c:2278
    mov es, cx                                ; 8e c1                       ; 0xc32be vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc32c0
    mov si, strict word 00060h                ; be 60 00                    ; 0xc32c3 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc32c6
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc32c8
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc32cb vgabios.c:2280
    inc bx                                    ; 43                          ; 0xc32d0 vgabios.c:2279
    inc bx                                    ; 43                          ; 0xc32d1
    jmp short 032dah                          ; eb 06                       ; 0xc32d2
    cmp word [bp-00eh], strict byte 00008h    ; 83 7e f2 08                 ; 0xc32d4
    jnc short 032f6h                          ; 73 1c                       ; 0xc32d8
    mov es, cx                                ; 8e c1                       ; 0xc32da vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc32dc
    mov si, word [bp-00eh]                    ; 8b 76 f2                    ; 0xc32df vgabios.c:48
    add si, si                                ; 01 f6                       ; 0xc32e2
    add si, strict byte 00050h                ; 83 c6 50                    ; 0xc32e4
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc32e7 vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc32ea
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc32ec
    inc bx                                    ; 43                          ; 0xc32ef vgabios.c:2282
    inc bx                                    ; 43                          ; 0xc32f0
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc32f1 vgabios.c:2283
    jmp short 032d4h                          ; eb de                       ; 0xc32f4
    mov es, cx                                ; 8e c1                       ; 0xc32f6 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc32f8
    mov si, strict word 0004eh                ; be 4e 00                    ; 0xc32fb vgabios.c:52
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc32fe
    mov es, dx                                ; 8e c2                       ; 0xc3301
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3303
    inc bx                                    ; 43                          ; 0xc3306 vgabios.c:2284
    inc bx                                    ; 43                          ; 0xc3307
    mov es, cx                                ; 8e c1                       ; 0xc3308 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc330a
    mov si, strict word 00062h                ; be 62 00                    ; 0xc330d vgabios.c:42
    mov es, dx                                ; 8e c2                       ; 0xc3310
    mov byte [es:si], al                      ; 26 88 04                    ; 0xc3312
    inc bx                                    ; 43                          ; 0xc3315 vgabios.c:2285
    mov es, cx                                ; 8e c1                       ; 0xc3316 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3318
    mov si, strict word 0007ch                ; be 7c 00                    ; 0xc331b vgabios.c:52
    xor dx, dx                                ; 31 d2                       ; 0xc331e
    mov es, dx                                ; 8e c2                       ; 0xc3320
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3322
    inc bx                                    ; 43                          ; 0xc3325 vgabios.c:2287
    inc bx                                    ; 43                          ; 0xc3326
    mov es, cx                                ; 8e c1                       ; 0xc3327 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3329
    mov si, strict word 0007eh                ; be 7e 00                    ; 0xc332c vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc332f
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3331
    inc bx                                    ; 43                          ; 0xc3334 vgabios.c:2288
    inc bx                                    ; 43                          ; 0xc3335
    mov es, cx                                ; 8e c1                       ; 0xc3336 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3338
    mov si, 0010ch                            ; be 0c 01                    ; 0xc333b vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc333e
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3340
    inc bx                                    ; 43                          ; 0xc3343 vgabios.c:2289
    inc bx                                    ; 43                          ; 0xc3344
    mov es, cx                                ; 8e c1                       ; 0xc3345 vgabios.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3347
    mov si, 0010eh                            ; be 0e 01                    ; 0xc334a vgabios.c:52
    mov es, dx                                ; 8e c2                       ; 0xc334d
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc334f
    inc bx                                    ; 43                          ; 0xc3352 vgabios.c:2290
    inc bx                                    ; 43                          ; 0xc3353
    test byte [bp-010h], 004h                 ; f6 46 f0 04                 ; 0xc3354 vgabios.c:2292
    je short 03397h                           ; 74 3d                       ; 0xc3358
    inc bx                                    ; 43                          ; 0xc335a vgabios.c:2293
    mov es, cx                                ; 8e c1                       ; 0xc335b vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc335d
    xor ah, ah                                ; 30 e4                       ; 0xc3360 vgabios.c:38
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3362
    inc bx                                    ; 43                          ; 0xc3365 vgabios.c:2294
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3366 vgabios.c:37
    mov dx, 003c6h                            ; ba c6 03                    ; 0xc3369 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc336c
    inc bx                                    ; 43                          ; 0xc336d vgabios.c:2295
    xor al, al                                ; 30 c0                       ; 0xc336e
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3370
    out DX, AL                                ; ee                          ; 0xc3373
    mov word [bp-00eh], ax                    ; 89 46 f2                    ; 0xc3374 vgabios.c:2298
    jmp short 03380h                          ; eb 07                       ; 0xc3377
    cmp word [bp-00eh], 00300h                ; 81 7e f2 00 03              ; 0xc3379
    jnc short 0338fh                          ; 73 0f                       ; 0xc337e
    mov es, cx                                ; 8e c1                       ; 0xc3380 vgabios.c:37
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc3382
    mov dx, 003c9h                            ; ba c9 03                    ; 0xc3385 vgabios.c:38
    out DX, AL                                ; ee                          ; 0xc3388
    inc bx                                    ; 43                          ; 0xc3389 vgabios.c:2299
    inc word [bp-00eh]                        ; ff 46 f2                    ; 0xc338a vgabios.c:2300
    jmp short 03379h                          ; eb ea                       ; 0xc338d
    inc bx                                    ; 43                          ; 0xc338f vgabios.c:2301
    mov al, byte [bp-008h]                    ; 8a 46 f8                    ; 0xc3390
    mov dx, 003c8h                            ; ba c8 03                    ; 0xc3393
    out DX, AL                                ; ee                          ; 0xc3396
    mov ax, bx                                ; 89 d8                       ; 0xc3397 vgabios.c:2305
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3399
    pop di                                    ; 5f                          ; 0xc339c
    pop si                                    ; 5e                          ; 0xc339d
    pop cx                                    ; 59                          ; 0xc339e
    pop bp                                    ; 5d                          ; 0xc339f
    retn                                      ; c3                          ; 0xc33a0
  ; disGetNextSymbol 0xc33a1 LB 0xc59 -> off=0x0 cb=0000000000000027 uValue=00000000000c33a1 'find_vga_entry'
find_vga_entry:                              ; 0xc33a1 LB 0x27
    push bx                                   ; 53                          ; 0xc33a1 vgabios.c:2314
    push dx                                   ; 52                          ; 0xc33a2
    push bp                                   ; 55                          ; 0xc33a3
    mov bp, sp                                ; 89 e5                       ; 0xc33a4
    mov dl, al                                ; 88 c2                       ; 0xc33a6
    mov AH, strict byte 0ffh                  ; b4 ff                       ; 0xc33a8 vgabios.c:2316
    xor al, al                                ; 30 c0                       ; 0xc33aa vgabios.c:2317
    jmp short 033b4h                          ; eb 06                       ; 0xc33ac
    db  0feh, 0c0h
    ; inc al                                    ; fe c0                     ; 0xc33ae vgabios.c:2318
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc33b0
    jnbe short 033c2h                         ; 77 0e                       ; 0xc33b2
    movzx bx, al                              ; 0f b6 d8                    ; 0xc33b4
    sal bx, 003h                              ; c1 e3 03                    ; 0xc33b7
    cmp dl, byte [bx+047aeh]                  ; 3a 97 ae 47                 ; 0xc33ba
    jne short 033aeh                          ; 75 ee                       ; 0xc33be
    mov ah, al                                ; 88 c4                       ; 0xc33c0
    mov al, ah                                ; 88 e0                       ; 0xc33c2 vgabios.c:2323
    pop bp                                    ; 5d                          ; 0xc33c4
    pop dx                                    ; 5a                          ; 0xc33c5
    pop bx                                    ; 5b                          ; 0xc33c6
    retn                                      ; c3                          ; 0xc33c7
  ; disGetNextSymbol 0xc33c8 LB 0xc32 -> off=0x0 cb=000000000000000e uValue=00000000000c33c8 'readx_byte'
readx_byte:                                  ; 0xc33c8 LB 0xe
    push bx                                   ; 53                          ; 0xc33c8 vgabios.c:2335
    push bp                                   ; 55                          ; 0xc33c9
    mov bp, sp                                ; 89 e5                       ; 0xc33ca
    mov bx, dx                                ; 89 d3                       ; 0xc33cc
    mov es, ax                                ; 8e c0                       ; 0xc33ce vgabios.c:2337
    mov al, byte [es:bx]                      ; 26 8a 07                    ; 0xc33d0
    pop bp                                    ; 5d                          ; 0xc33d3 vgabios.c:2338
    pop bx                                    ; 5b                          ; 0xc33d4
    retn                                      ; c3                          ; 0xc33d5
  ; disGetNextSymbol 0xc33d6 LB 0xc24 -> off=0x87 cb=0000000000000423 uValue=00000000000c345d 'int10_func'
    db  056h, 04fh, 01ch, 01bh, 013h, 012h, 011h, 010h, 00eh, 00dh, 00ch, 00ah, 009h, 008h, 007h, 006h
    db  005h, 004h, 003h, 002h, 001h, 000h, 079h, 038h, 088h, 034h, 0c5h, 034h, 0d9h, 034h, 0eah, 034h
    db  0feh, 034h, 00fh, 035h, 01ah, 035h, 054h, 035h, 058h, 035h, 069h, 035h, 086h, 035h, 0a3h, 035h
    db  0bdh, 035h, 0dah, 035h, 0f1h, 035h, 0fdh, 035h, 0cdh, 036h, 03ch, 037h, 069h, 037h, 07eh, 037h
    db  0c0h, 037h, 04bh, 038h, 030h, 024h, 023h, 022h, 021h, 020h, 014h, 012h, 011h, 010h, 004h, 002h
    db  001h, 000h, 079h, 038h, 01eh, 036h, 042h, 036h, 050h, 036h, 05eh, 036h, 01eh, 036h, 042h, 036h
    db  050h, 036h, 05eh, 036h, 06ch, 036h, 078h, 036h, 093h, 036h, 09eh, 036h, 0a9h, 036h, 0b4h, 036h
    db  00ah, 009h, 006h, 004h, 002h, 001h, 000h, 03dh, 038h, 0e8h, 037h, 0f6h, 037h, 007h, 038h, 017h
    db  038h, 02ch, 038h, 03dh, 038h, 03dh, 038h
int10_func:                                  ; 0xc345d LB 0x423
    push bp                                   ; 55                          ; 0xc345d vgabios.c:2416
    mov bp, sp                                ; 89 e5                       ; 0xc345e
    push si                                   ; 56                          ; 0xc3460
    push di                                   ; 57                          ; 0xc3461
    push ax                                   ; 50                          ; 0xc3462
    mov si, word [bp+004h]                    ; 8b 76 04                    ; 0xc3463
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3466 vgabios.c:2421
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3469
    cmp ax, strict word 00056h                ; 3d 56 00                    ; 0xc346c
    jnbe near 03879h                          ; 0f 87 06 04                 ; 0xc346f
    push CS                                   ; 0e                          ; 0xc3473
    pop ES                                    ; 07                          ; 0xc3474
    mov cx, strict word 00017h                ; b9 17 00                    ; 0xc3475
    mov di, 033d6h                            ; bf d6 33                    ; 0xc3478
    repne scasb                               ; f2 ae                       ; 0xc347b
    sal cx, 1                                 ; d1 e1                       ; 0xc347d
    mov di, cx                                ; 89 cf                       ; 0xc347f
    mov ax, word [cs:di+033ech]               ; 2e 8b 85 ec 33              ; 0xc3481
    jmp ax                                    ; ff e0                       ; 0xc3486
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3488 vgabios.c:2424
    call 0131ch                               ; e8 8d de                    ; 0xc348c
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc348f vgabios.c:2425
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc3492
    cmp ax, strict word 00007h                ; 3d 07 00                    ; 0xc3495
    je short 034afh                           ; 74 15                       ; 0xc3498
    cmp ax, strict word 00006h                ; 3d 06 00                    ; 0xc349a
    je short 034a6h                           ; 74 07                       ; 0xc349d
    cmp ax, strict word 00005h                ; 3d 05 00                    ; 0xc349f
    jbe short 034afh                          ; 76 0b                       ; 0xc34a2
    jmp short 034b8h                          ; eb 12                       ; 0xc34a4
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34a6 vgabios.c:2427
    xor al, al                                ; 30 c0                       ; 0xc34a9
    or AL, strict byte 03fh                   ; 0c 3f                       ; 0xc34ab
    jmp short 034bfh                          ; eb 10                       ; 0xc34ad vgabios.c:2428
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34af vgabios.c:2436
    xor al, al                                ; 30 c0                       ; 0xc34b2
    or AL, strict byte 030h                   ; 0c 30                       ; 0xc34b4
    jmp short 034bfh                          ; eb 07                       ; 0xc34b6
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc34b8 vgabios.c:2439
    xor al, al                                ; 30 c0                       ; 0xc34bb
    or AL, strict byte 020h                   ; 0c 20                       ; 0xc34bd
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc34bf
    jmp near 03879h                           ; e9 b4 03                    ; 0xc34c2 vgabios.c:2441
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc34c5 vgabios.c:2443
    movzx dx, al                              ; 0f b6 d0                    ; 0xc34c8
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc34cb
    shr ax, 008h                              ; c1 e8 08                    ; 0xc34ce
    xor ah, ah                                ; 30 e4                       ; 0xc34d1
    call 010d0h                               ; e8 fa db                    ; 0xc34d3
    jmp near 03879h                           ; e9 a0 03                    ; 0xc34d6 vgabios.c:2444
    mov dx, word [bp+00eh]                    ; 8b 56 0e                    ; 0xc34d9 vgabios.c:2446
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc34dc
    shr ax, 008h                              ; c1 e8 08                    ; 0xc34df
    xor ah, ah                                ; 30 e4                       ; 0xc34e2
    call 011c6h                               ; e8 df dc                    ; 0xc34e4
    jmp near 03879h                           ; e9 8f 03                    ; 0xc34e7 vgabios.c:2447
    lea bx, [bp+00eh]                         ; 8d 5e 0e                    ; 0xc34ea vgabios.c:2449
    lea dx, [bp+010h]                         ; 8d 56 10                    ; 0xc34ed
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc34f0
    shr ax, 008h                              ; c1 e8 08                    ; 0xc34f3
    xor ah, ah                                ; 30 e4                       ; 0xc34f6
    call 00a17h                               ; e8 1c d5                    ; 0xc34f8
    jmp near 03879h                           ; e9 7b 03                    ; 0xc34fb vgabios.c:2450
    xor ax, ax                                ; 31 c0                       ; 0xc34fe vgabios.c:2456
    mov word [bp+012h], ax                    ; 89 46 12                    ; 0xc3500
    mov word [bp+00ch], ax                    ; 89 46 0c                    ; 0xc3503 vgabios.c:2457
    mov word [bp+010h], ax                    ; 89 46 10                    ; 0xc3506 vgabios.c:2458
    mov word [bp+00eh], ax                    ; 89 46 0e                    ; 0xc3509 vgabios.c:2459
    jmp near 03879h                           ; e9 6a 03                    ; 0xc350c vgabios.c:2460
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc350f vgabios.c:2462
    xor ah, ah                                ; 30 e4                       ; 0xc3512
    call 0124fh                               ; e8 38 dd                    ; 0xc3514
    jmp near 03879h                           ; e9 5f 03                    ; 0xc3517 vgabios.c:2463
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc351a vgabios.c:2465
    push ax                                   ; 50                          ; 0xc351d
    mov ax, 000ffh                            ; b8 ff 00                    ; 0xc351e
    push ax                                   ; 50                          ; 0xc3521
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3522
    xor ah, ah                                ; 30 e4                       ; 0xc3525
    push ax                                   ; 50                          ; 0xc3527
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3528
    shr ax, 008h                              ; c1 e8 08                    ; 0xc352b
    xor ah, ah                                ; 30 e4                       ; 0xc352e
    push ax                                   ; 50                          ; 0xc3530
    mov al, byte [bp+010h]                    ; 8a 46 10                    ; 0xc3531
    movzx cx, al                              ; 0f b6 c8                    ; 0xc3534
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3537
    shr ax, 008h                              ; c1 e8 08                    ; 0xc353a
    movzx bx, al                              ; 0f b6 d8                    ; 0xc353d
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3540
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3543
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3546
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3549
    xor ah, ah                                ; 30 e4                       ; 0xc354c
    call 0193eh                               ; e8 ed e3                    ; 0xc354e
    jmp near 03879h                           ; e9 25 03                    ; 0xc3551 vgabios.c:2466
    xor ax, ax                                ; 31 c0                       ; 0xc3554 vgabios.c:2468
    jmp short 0351dh                          ; eb c5                       ; 0xc3556
    lea dx, [bp+012h]                         ; 8d 56 12                    ; 0xc3558 vgabios.c:2471
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc355b
    shr ax, 008h                              ; c1 e8 08                    ; 0xc355e
    xor ah, ah                                ; 30 e4                       ; 0xc3561
    call 00d34h                               ; e8 ce d7                    ; 0xc3563
    jmp near 03879h                           ; e9 10 03                    ; 0xc3566 vgabios.c:2472
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3569 vgabios.c:2474
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc356c
    movzx bx, al                              ; 0f b6 d8                    ; 0xc356f
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc3572
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3575
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3578
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc357b
    xor ah, ah                                ; 30 e4                       ; 0xc357e
    call 021c7h                               ; e8 44 ec                    ; 0xc3580
    jmp near 03879h                           ; e9 f3 02                    ; 0xc3583 vgabios.c:2475
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3586 vgabios.c:2477
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3589
    movzx bx, al                              ; 0f b6 d8                    ; 0xc358c
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc358f
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3592
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3595
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3598
    xor ah, ah                                ; 30 e4                       ; 0xc359b
    call 0232ch                               ; e8 8c ed                    ; 0xc359d
    jmp near 03879h                           ; e9 d6 02                    ; 0xc35a0 vgabios.c:2478
    mov cx, word [bp+00eh]                    ; 8b 4e 0e                    ; 0xc35a3 vgabios.c:2480
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc35a6
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc35a9
    movzx dx, al                              ; 0f b6 d0                    ; 0xc35ac
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc35af
    shr ax, 008h                              ; c1 e8 08                    ; 0xc35b2
    xor ah, ah                                ; 30 e4                       ; 0xc35b5
    call 0248eh                               ; e8 d4 ee                    ; 0xc35b7
    jmp near 03879h                           ; e9 bc 02                    ; 0xc35ba vgabios.c:2481
    lea cx, [bp+012h]                         ; 8d 4e 12                    ; 0xc35bd vgabios.c:2483
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc35c0
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc35c3
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc35c6
    shr ax, 008h                              ; c1 e8 08                    ; 0xc35c9
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc35cc
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc35cf
    xor ah, ah                                ; 30 e4                       ; 0xc35d2
    call 00eeeh                               ; e8 17 d9                    ; 0xc35d4
    jmp near 03879h                           ; e9 9f 02                    ; 0xc35d7 vgabios.c:2484
    mov cx, strict word 00002h                ; b9 02 00                    ; 0xc35da vgabios.c:2492
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc35dd
    movzx bx, al                              ; 0f b6 d8                    ; 0xc35e0
    mov dx, 000ffh                            ; ba ff 00                    ; 0xc35e3
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc35e6
    xor ah, ah                                ; 30 e4                       ; 0xc35e9
    call 025f3h                               ; e8 05 f0                    ; 0xc35eb
    jmp near 03879h                           ; e9 88 02                    ; 0xc35ee vgabios.c:2493
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc35f1 vgabios.c:2496
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc35f4
    call 01044h                               ; e8 4a da                    ; 0xc35f7
    jmp near 03879h                           ; e9 7c 02                    ; 0xc35fa vgabios.c:2497
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc35fd vgabios.c:2499
    xor ah, ah                                ; 30 e4                       ; 0xc3600
    cmp ax, strict word 00030h                ; 3d 30 00                    ; 0xc3602
    jnbe near 03879h                          ; 0f 87 70 02                 ; 0xc3605
    push CS                                   ; 0e                          ; 0xc3609
    pop ES                                    ; 07                          ; 0xc360a
    mov cx, strict word 0000fh                ; b9 0f 00                    ; 0xc360b
    mov di, 0341ah                            ; bf 1a 34                    ; 0xc360e
    repne scasb                               ; f2 ae                       ; 0xc3611
    sal cx, 1                                 ; d1 e1                       ; 0xc3613
    mov di, cx                                ; 89 cf                       ; 0xc3615
    mov ax, word [cs:di+03428h]               ; 2e 8b 85 28 34              ; 0xc3617
    jmp ax                                    ; ff e0                       ; 0xc361c
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc361e vgabios.c:2503
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3621
    xor ah, ah                                ; 30 e4                       ; 0xc3624
    push ax                                   ; 50                          ; 0xc3626
    movzx ax, byte [bp+00ch]                  ; 0f b6 46 0c                 ; 0xc3627
    push ax                                   ; 50                          ; 0xc362b
    push word [bp+00eh]                       ; ff 76 0e                    ; 0xc362c
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc362f
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3633
    mov bx, word [bp+008h]                    ; 8b 5e 08                    ; 0xc3636
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc3639
    call 02959h                               ; e8 1a f3                    ; 0xc363c
    jmp near 03879h                           ; e9 37 02                    ; 0xc363f vgabios.c:2504
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc3642 vgabios.c:2507
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3646
    call 029d5h                               ; e8 88 f3                    ; 0xc364a
    jmp near 03879h                           ; e9 29 02                    ; 0xc364d vgabios.c:2508
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc3650 vgabios.c:2511
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3654
    call 02a44h                               ; e8 e9 f3                    ; 0xc3658
    jmp near 03879h                           ; e9 1b 02                    ; 0xc365b vgabios.c:2512
    movzx dx, byte [bp+00ch]                  ; 0f b6 56 0c                 ; 0xc365e vgabios.c:2515
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3662
    call 02ab5h                               ; e8 4c f4                    ; 0xc3666
    jmp near 03879h                           ; e9 0d 02                    ; 0xc3669 vgabios.c:2516
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc366c vgabios.c:2518
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc366f
    call 02b26h                               ; e8 b1 f4                    ; 0xc3672
    jmp near 03879h                           ; e9 01 02                    ; 0xc3675 vgabios.c:2519
    mov al, byte [bp+00eh]                    ; 8a 46 0e                    ; 0xc3678 vgabios.c:2521
    xor ah, ah                                ; 30 e4                       ; 0xc367b
    push ax                                   ; 50                          ; 0xc367d
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc367e
    movzx cx, al                              ; 0f b6 c8                    ; 0xc3681
    mov bx, word [bp+010h]                    ; 8b 5e 10                    ; 0xc3684
    mov dx, word [bp+008h]                    ; 8b 56 08                    ; 0xc3687
    mov ax, word [bp+016h]                    ; 8b 46 16                    ; 0xc368a
    call 02b2bh                               ; e8 9b f4                    ; 0xc368d
    jmp near 03879h                           ; e9 e6 01                    ; 0xc3690 vgabios.c:2522
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc3693 vgabios.c:2524
    xor ah, ah                                ; 30 e4                       ; 0xc3696
    call 02b32h                               ; e8 97 f4                    ; 0xc3698
    jmp near 03879h                           ; e9 db 01                    ; 0xc369b vgabios.c:2525
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc369e vgabios.c:2527
    xor ah, ah                                ; 30 e4                       ; 0xc36a1
    call 02b37h                               ; e8 91 f4                    ; 0xc36a3
    jmp near 03879h                           ; e9 d0 01                    ; 0xc36a6 vgabios.c:2528
    mov al, byte [bp+00ch]                    ; 8a 46 0c                    ; 0xc36a9 vgabios.c:2530
    xor ah, ah                                ; 30 e4                       ; 0xc36ac
    call 02b3ch                               ; e8 8b f4                    ; 0xc36ae
    jmp near 03879h                           ; e9 c5 01                    ; 0xc36b1 vgabios.c:2531
    lea ax, [bp+00eh]                         ; 8d 46 0e                    ; 0xc36b4 vgabios.c:2533
    push ax                                   ; 50                          ; 0xc36b7
    lea cx, [bp+010h]                         ; 8d 4e 10                    ; 0xc36b8
    lea bx, [bp+008h]                         ; 8d 5e 08                    ; 0xc36bb
    lea dx, [bp+016h]                         ; 8d 56 16                    ; 0xc36be
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc36c1
    shr ax, 008h                              ; c1 e8 08                    ; 0xc36c4
    call 00e6bh                               ; e8 a1 d7                    ; 0xc36c7
    jmp near 03879h                           ; e9 ac 01                    ; 0xc36ca vgabios.c:2541
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc36cd vgabios.c:2543
    xor ah, ah                                ; 30 e4                       ; 0xc36d0
    cmp ax, strict word 00034h                ; 3d 34 00                    ; 0xc36d2
    jc short 036e6h                           ; 72 0f                       ; 0xc36d5
    jbe short 036f3h                          ; 76 1a                       ; 0xc36d7
    cmp ax, strict word 00036h                ; 3d 36 00                    ; 0xc36d9
    je short 03732h                           ; 74 54                       ; 0xc36dc
    cmp ax, strict word 00035h                ; 3d 35 00                    ; 0xc36de
    je short 03723h                           ; 74 40                       ; 0xc36e1
    jmp near 03879h                           ; e9 93 01                    ; 0xc36e3
    cmp ax, strict word 00020h                ; 3d 20 00                    ; 0xc36e6
    jne near 03879h                           ; 0f 85 8c 01                 ; 0xc36e9
    call 02b41h                               ; e8 51 f4                    ; 0xc36ed vgabios.c:2546
    jmp near 03879h                           ; e9 86 01                    ; 0xc36f0 vgabios.c:2547
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc36f3 vgabios.c:2549
    xor ah, ah                                ; 30 e4                       ; 0xc36f6
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc36f8
    jnc short 0371dh                          ; 73 20                       ; 0xc36fb
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc36fd vgabios.c:35
    mov si, 00087h                            ; be 87 00                    ; 0xc3700
    mov es, ax                                ; 8e c0                       ; 0xc3703 vgabios.c:37
    mov dl, byte [es:si]                      ; 26 8a 14                    ; 0xc3705
    and dl, 0feh                              ; 80 e2 fe                    ; 0xc3708 vgabios.c:38
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc370b
    or dl, al                                 ; 08 c2                       ; 0xc370e
    mov byte [es:si], dl                      ; 26 88 14                    ; 0xc3710 vgabios.c:42
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3713 vgabios.c:2552
    xor al, al                                ; 30 c0                       ; 0xc3716
    or AL, strict byte 012h                   ; 0c 12                       ; 0xc3718
    jmp near 034bfh                           ; e9 a2 fd                    ; 0xc371a
    mov byte [bp+012h], ah                    ; 88 66 12                    ; 0xc371d vgabios.c:2555
    jmp near 03879h                           ; e9 56 01                    ; 0xc3720 vgabios.c:2556
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc3723 vgabios.c:2558
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc3727
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc372a
    call 02b46h                               ; e8 16 f4                    ; 0xc372d
    jmp short 03713h                          ; eb e1                       ; 0xc3730
    mov al, byte [bp+012h]                    ; 8a 46 12                    ; 0xc3732 vgabios.c:2562
    xor ah, ah                                ; 30 e4                       ; 0xc3735
    call 02b4bh                               ; e8 11 f4                    ; 0xc3737
    jmp short 03713h                          ; eb d7                       ; 0xc373a
    push word [bp+008h]                       ; ff 76 08                    ; 0xc373c vgabios.c:2572
    push word [bp+016h]                       ; ff 76 16                    ; 0xc373f
    movzx ax, byte [bp+00eh]                  ; 0f b6 46 0e                 ; 0xc3742
    push ax                                   ; 50                          ; 0xc3746
    mov ax, word [bp+00eh]                    ; 8b 46 0e                    ; 0xc3747
    shr ax, 008h                              ; c1 e8 08                    ; 0xc374a
    xor ah, ah                                ; 30 e4                       ; 0xc374d
    push ax                                   ; 50                          ; 0xc374f
    movzx bx, byte [bp+00ch]                  ; 0f b6 5e 0c                 ; 0xc3750
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc3754
    shr dx, 008h                              ; c1 ea 08                    ; 0xc3757
    xor dh, dh                                ; 30 f6                       ; 0xc375a
    movzx ax, byte [bp+012h]                  ; 0f b6 46 12                 ; 0xc375c
    mov cx, word [bp+010h]                    ; 8b 4e 10                    ; 0xc3760
    call 02b50h                               ; e8 ea f3                    ; 0xc3763
    jmp near 03879h                           ; e9 10 01                    ; 0xc3766 vgabios.c:2573
    mov bx, si                                ; 89 f3                       ; 0xc3769 vgabios.c:2575
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc376b
    mov ax, word [bp+00ch]                    ; 8b 46 0c                    ; 0xc376e
    call 02be6h                               ; e8 72 f4                    ; 0xc3771
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3774 vgabios.c:2576
    xor al, al                                ; 30 c0                       ; 0xc3777
    or AL, strict byte 01bh                   ; 0c 1b                       ; 0xc3779
    jmp near 034bfh                           ; e9 41 fd                    ; 0xc377b
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc377e vgabios.c:2579
    xor ah, ah                                ; 30 e4                       ; 0xc3781
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3783
    je short 037aah                           ; 74 22                       ; 0xc3786
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3788
    je short 0379ch                           ; 74 0f                       ; 0xc378b
    test ax, ax                               ; 85 c0                       ; 0xc378d
    jne short 037b6h                          ; 75 25                       ; 0xc378f
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3791 vgabios.c:2582
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc3794
    call 02dfbh                               ; e8 61 f6                    ; 0xc3797
    jmp short 037b6h                          ; eb 1a                       ; 0xc379a vgabios.c:2583
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc379c vgabios.c:2585
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc379f
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc37a2
    call 02e13h                               ; e8 6b f6                    ; 0xc37a5
    jmp short 037b6h                          ; eb 0c                       ; 0xc37a8 vgabios.c:2586
    mov bx, word [bp+00ch]                    ; 8b 5e 0c                    ; 0xc37aa vgabios.c:2588
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc37ad
    mov ax, word [bp+010h]                    ; 8b 46 10                    ; 0xc37b0
    call 030e9h                               ; e8 33 f9                    ; 0xc37b3
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc37b6 vgabios.c:2595
    xor al, al                                ; 30 c0                       ; 0xc37b9
    or AL, strict byte 01ch                   ; 0c 1c                       ; 0xc37bb
    jmp near 034bfh                           ; e9 ff fc                    ; 0xc37bd
    call 007afh                               ; e8 ec cf                    ; 0xc37c0 vgabios.c:2600
    test ax, ax                               ; 85 c0                       ; 0xc37c3
    je near 03844h                            ; 0f 84 7b 00                 ; 0xc37c5
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc37c9 vgabios.c:2601
    xor ah, ah                                ; 30 e4                       ; 0xc37cc
    cmp ax, strict word 0000ah                ; 3d 0a 00                    ; 0xc37ce
    jnbe short 0383dh                         ; 77 6a                       ; 0xc37d1
    push CS                                   ; 0e                          ; 0xc37d3
    pop ES                                    ; 07                          ; 0xc37d4
    mov cx, strict word 00008h                ; b9 08 00                    ; 0xc37d5
    mov di, 03446h                            ; bf 46 34                    ; 0xc37d8
    repne scasb                               ; f2 ae                       ; 0xc37db
    sal cx, 1                                 ; d1 e1                       ; 0xc37dd
    mov di, cx                                ; 89 cf                       ; 0xc37df
    mov ax, word [cs:di+0344dh]               ; 2e 8b 85 4d 34              ; 0xc37e1
    jmp ax                                    ; ff e0                       ; 0xc37e6
    mov bx, si                                ; 89 f3                       ; 0xc37e8 vgabios.c:2604
    mov dx, word [bp+016h]                    ; 8b 56 16                    ; 0xc37ea
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc37ed
    call 03a33h                               ; e8 40 02                    ; 0xc37f0
    jmp near 03879h                           ; e9 83 00                    ; 0xc37f3 vgabios.c:2605
    mov cx, si                                ; 89 f1                       ; 0xc37f6 vgabios.c:2607
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc37f8
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc37fb
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc37fe
    call 03b58h                               ; e8 54 03                    ; 0xc3801
    jmp near 03879h                           ; e9 72 00                    ; 0xc3804 vgabios.c:2608
    mov cx, si                                ; 89 f1                       ; 0xc3807 vgabios.c:2610
    mov bx, word [bp+016h]                    ; 8b 5e 16                    ; 0xc3809
    mov dx, word [bp+00ch]                    ; 8b 56 0c                    ; 0xc380c
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc380f
    call 03bf3h                               ; e8 de 03                    ; 0xc3812
    jmp short 03879h                          ; eb 62                       ; 0xc3815 vgabios.c:2611
    lea ax, [bp+00ch]                         ; 8d 46 0c                    ; 0xc3817 vgabios.c:2613
    push ax                                   ; 50                          ; 0xc381a
    mov cx, word [bp+016h]                    ; 8b 4e 16                    ; 0xc381b
    mov bx, word [bp+00eh]                    ; 8b 5e 0e                    ; 0xc381e
    mov dx, word [bp+010h]                    ; 8b 56 10                    ; 0xc3821
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3824
    call 03dbah                               ; e8 90 05                    ; 0xc3827
    jmp short 03879h                          ; eb 4d                       ; 0xc382a vgabios.c:2614
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc382c vgabios.c:2616
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc382f
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3832
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3835
    call 03e46h                               ; e8 0b 06                    ; 0xc3838
    jmp short 03879h                          ; eb 3c                       ; 0xc383b vgabios.c:2617
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc383d vgabios.c:2639
    jmp short 03879h                          ; eb 35                       ; 0xc3842 vgabios.c:2642
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3844 vgabios.c:2644
    jmp short 03879h                          ; eb 2e                       ; 0xc3849 vgabios.c:2646
    call 007afh                               ; e8 61 cf                    ; 0xc384b vgabios.c:2648
    test ax, ax                               ; 85 c0                       ; 0xc384e
    je short 03874h                           ; 74 22                       ; 0xc3850
    mov ax, word [bp+012h]                    ; 8b 46 12                    ; 0xc3852 vgabios.c:2649
    xor ah, ah                                ; 30 e4                       ; 0xc3855
    cmp ax, strict word 00042h                ; 3d 42 00                    ; 0xc3857
    jne short 0386dh                          ; 75 11                       ; 0xc385a
    lea cx, [bp+00eh]                         ; 8d 4e 0e                    ; 0xc385c vgabios.c:2652
    lea bx, [bp+010h]                         ; 8d 5e 10                    ; 0xc385f
    lea dx, [bp+00ch]                         ; 8d 56 0c                    ; 0xc3862
    lea ax, [bp+012h]                         ; 8d 46 12                    ; 0xc3865
    call 03f15h                               ; e8 aa 06                    ; 0xc3868
    jmp short 03879h                          ; eb 0c                       ; 0xc386b vgabios.c:2653
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc386d vgabios.c:2655
    jmp short 03879h                          ; eb 05                       ; 0xc3872 vgabios.c:2658
    mov word [bp+012h], 00100h                ; c7 46 12 00 01              ; 0xc3874 vgabios.c:2660
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3879 vgabios.c:2670
    pop di                                    ; 5f                          ; 0xc387c
    pop si                                    ; 5e                          ; 0xc387d
    pop bp                                    ; 5d                          ; 0xc387e
    retn                                      ; c3                          ; 0xc387f
  ; disGetNextSymbol 0xc3880 LB 0x77a -> off=0x0 cb=000000000000001f uValue=00000000000c3880 'dispi_set_xres'
dispi_set_xres:                              ; 0xc3880 LB 0x1f
    push bp                                   ; 55                          ; 0xc3880 vbe.c:100
    mov bp, sp                                ; 89 e5                       ; 0xc3881
    push bx                                   ; 53                          ; 0xc3883
    push dx                                   ; 52                          ; 0xc3884
    mov bx, ax                                ; 89 c3                       ; 0xc3885
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3887 vbe.c:105
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc388a
    call 00560h                               ; e8 d0 cc                    ; 0xc388d
    mov ax, bx                                ; 89 d8                       ; 0xc3890 vbe.c:106
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3892
    call 00560h                               ; e8 c8 cc                    ; 0xc3895
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3898 vbe.c:107
    pop dx                                    ; 5a                          ; 0xc389b
    pop bx                                    ; 5b                          ; 0xc389c
    pop bp                                    ; 5d                          ; 0xc389d
    retn                                      ; c3                          ; 0xc389e
  ; disGetNextSymbol 0xc389f LB 0x75b -> off=0x0 cb=000000000000001f uValue=00000000000c389f 'dispi_set_yres'
dispi_set_yres:                              ; 0xc389f LB 0x1f
    push bp                                   ; 55                          ; 0xc389f vbe.c:109
    mov bp, sp                                ; 89 e5                       ; 0xc38a0
    push bx                                   ; 53                          ; 0xc38a2
    push dx                                   ; 52                          ; 0xc38a3
    mov bx, ax                                ; 89 c3                       ; 0xc38a4
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc38a6 vbe.c:114
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38a9
    call 00560h                               ; e8 b1 cc                    ; 0xc38ac
    mov ax, bx                                ; 89 d8                       ; 0xc38af vbe.c:115
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc38b1
    call 00560h                               ; e8 a9 cc                    ; 0xc38b4
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc38b7 vbe.c:116
    pop dx                                    ; 5a                          ; 0xc38ba
    pop bx                                    ; 5b                          ; 0xc38bb
    pop bp                                    ; 5d                          ; 0xc38bc
    retn                                      ; c3                          ; 0xc38bd
  ; disGetNextSymbol 0xc38be LB 0x73c -> off=0x0 cb=0000000000000019 uValue=00000000000c38be 'dispi_get_yres'
dispi_get_yres:                              ; 0xc38be LB 0x19
    push bp                                   ; 55                          ; 0xc38be vbe.c:118
    mov bp, sp                                ; 89 e5                       ; 0xc38bf
    push dx                                   ; 52                          ; 0xc38c1
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc38c2 vbe.c:120
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38c5
    call 00560h                               ; e8 95 cc                    ; 0xc38c8
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc38cb vbe.c:121
    call 00567h                               ; e8 96 cc                    ; 0xc38ce
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc38d1 vbe.c:122
    pop dx                                    ; 5a                          ; 0xc38d4
    pop bp                                    ; 5d                          ; 0xc38d5
    retn                                      ; c3                          ; 0xc38d6
  ; disGetNextSymbol 0xc38d7 LB 0x723 -> off=0x0 cb=000000000000001f uValue=00000000000c38d7 'dispi_set_bpp'
dispi_set_bpp:                               ; 0xc38d7 LB 0x1f
    push bp                                   ; 55                          ; 0xc38d7 vbe.c:124
    mov bp, sp                                ; 89 e5                       ; 0xc38d8
    push bx                                   ; 53                          ; 0xc38da
    push dx                                   ; 52                          ; 0xc38db
    mov bx, ax                                ; 89 c3                       ; 0xc38dc
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc38de vbe.c:129
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38e1
    call 00560h                               ; e8 79 cc                    ; 0xc38e4
    mov ax, bx                                ; 89 d8                       ; 0xc38e7 vbe.c:130
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc38e9
    call 00560h                               ; e8 71 cc                    ; 0xc38ec
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc38ef vbe.c:131
    pop dx                                    ; 5a                          ; 0xc38f2
    pop bx                                    ; 5b                          ; 0xc38f3
    pop bp                                    ; 5d                          ; 0xc38f4
    retn                                      ; c3                          ; 0xc38f5
  ; disGetNextSymbol 0xc38f6 LB 0x704 -> off=0x0 cb=0000000000000019 uValue=00000000000c38f6 'dispi_get_bpp'
dispi_get_bpp:                               ; 0xc38f6 LB 0x19
    push bp                                   ; 55                          ; 0xc38f6 vbe.c:133
    mov bp, sp                                ; 89 e5                       ; 0xc38f7
    push dx                                   ; 52                          ; 0xc38f9
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc38fa vbe.c:135
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc38fd
    call 00560h                               ; e8 5d cc                    ; 0xc3900
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3903 vbe.c:136
    call 00567h                               ; e8 5e cc                    ; 0xc3906
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3909 vbe.c:137
    pop dx                                    ; 5a                          ; 0xc390c
    pop bp                                    ; 5d                          ; 0xc390d
    retn                                      ; c3                          ; 0xc390e
  ; disGetNextSymbol 0xc390f LB 0x6eb -> off=0x0 cb=000000000000001f uValue=00000000000c390f 'dispi_set_virt_width'
dispi_set_virt_width:                        ; 0xc390f LB 0x1f
    push bp                                   ; 55                          ; 0xc390f vbe.c:139
    mov bp, sp                                ; 89 e5                       ; 0xc3910
    push bx                                   ; 53                          ; 0xc3912
    push dx                                   ; 52                          ; 0xc3913
    mov bx, ax                                ; 89 c3                       ; 0xc3914
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3916 vbe.c:144
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3919
    call 00560h                               ; e8 41 cc                    ; 0xc391c
    mov ax, bx                                ; 89 d8                       ; 0xc391f vbe.c:145
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3921
    call 00560h                               ; e8 39 cc                    ; 0xc3924
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3927 vbe.c:146
    pop dx                                    ; 5a                          ; 0xc392a
    pop bx                                    ; 5b                          ; 0xc392b
    pop bp                                    ; 5d                          ; 0xc392c
    retn                                      ; c3                          ; 0xc392d
  ; disGetNextSymbol 0xc392e LB 0x6cc -> off=0x0 cb=0000000000000019 uValue=00000000000c392e 'dispi_get_virt_width'
dispi_get_virt_width:                        ; 0xc392e LB 0x19
    push bp                                   ; 55                          ; 0xc392e vbe.c:148
    mov bp, sp                                ; 89 e5                       ; 0xc392f
    push dx                                   ; 52                          ; 0xc3931
    mov ax, strict word 00006h                ; b8 06 00                    ; 0xc3932 vbe.c:150
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3935
    call 00560h                               ; e8 25 cc                    ; 0xc3938
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc393b vbe.c:151
    call 00567h                               ; e8 26 cc                    ; 0xc393e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3941 vbe.c:152
    pop dx                                    ; 5a                          ; 0xc3944
    pop bp                                    ; 5d                          ; 0xc3945
    retn                                      ; c3                          ; 0xc3946
  ; disGetNextSymbol 0xc3947 LB 0x6b3 -> off=0x0 cb=0000000000000019 uValue=00000000000c3947 'dispi_get_virt_height'
dispi_get_virt_height:                       ; 0xc3947 LB 0x19
    push bp                                   ; 55                          ; 0xc3947 vbe.c:154
    mov bp, sp                                ; 89 e5                       ; 0xc3948
    push dx                                   ; 52                          ; 0xc394a
    mov ax, strict word 00007h                ; b8 07 00                    ; 0xc394b vbe.c:156
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc394e
    call 00560h                               ; e8 0c cc                    ; 0xc3951
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3954 vbe.c:157
    call 00567h                               ; e8 0d cc                    ; 0xc3957
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc395a vbe.c:158
    pop dx                                    ; 5a                          ; 0xc395d
    pop bp                                    ; 5d                          ; 0xc395e
    retn                                      ; c3                          ; 0xc395f
  ; disGetNextSymbol 0xc3960 LB 0x69a -> off=0x0 cb=0000000000000012 uValue=00000000000c3960 'in_word'
in_word:                                     ; 0xc3960 LB 0x12
    push bp                                   ; 55                          ; 0xc3960 vbe.c:160
    mov bp, sp                                ; 89 e5                       ; 0xc3961
    push bx                                   ; 53                          ; 0xc3963
    mov bx, ax                                ; 89 c3                       ; 0xc3964
    mov ax, dx                                ; 89 d0                       ; 0xc3966
    mov dx, bx                                ; 89 da                       ; 0xc3968 vbe.c:162
    out DX, ax                                ; ef                          ; 0xc396a
    in ax, DX                                 ; ed                          ; 0xc396b vbe.c:163
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc396c vbe.c:164
    pop bx                                    ; 5b                          ; 0xc396f
    pop bp                                    ; 5d                          ; 0xc3970
    retn                                      ; c3                          ; 0xc3971
  ; disGetNextSymbol 0xc3972 LB 0x688 -> off=0x0 cb=0000000000000014 uValue=00000000000c3972 'in_byte'
in_byte:                                     ; 0xc3972 LB 0x14
    push bp                                   ; 55                          ; 0xc3972 vbe.c:166
    mov bp, sp                                ; 89 e5                       ; 0xc3973
    push bx                                   ; 53                          ; 0xc3975
    mov bx, ax                                ; 89 c3                       ; 0xc3976
    mov ax, dx                                ; 89 d0                       ; 0xc3978
    mov dx, bx                                ; 89 da                       ; 0xc397a vbe.c:168
    out DX, ax                                ; ef                          ; 0xc397c
    in AL, DX                                 ; ec                          ; 0xc397d vbe.c:169
    db  02ah, 0e4h
    ; sub ah, ah                                ; 2a e4                     ; 0xc397e
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3980 vbe.c:170
    pop bx                                    ; 5b                          ; 0xc3983
    pop bp                                    ; 5d                          ; 0xc3984
    retn                                      ; c3                          ; 0xc3985
  ; disGetNextSymbol 0xc3986 LB 0x674 -> off=0x0 cb=0000000000000014 uValue=00000000000c3986 'dispi_get_id'
dispi_get_id:                                ; 0xc3986 LB 0x14
    push bp                                   ; 55                          ; 0xc3986 vbe.c:173
    mov bp, sp                                ; 89 e5                       ; 0xc3987
    push dx                                   ; 52                          ; 0xc3989
    xor ax, ax                                ; 31 c0                       ; 0xc398a vbe.c:175
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc398c
    out DX, ax                                ; ef                          ; 0xc398f
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3990 vbe.c:176
    in ax, DX                                 ; ed                          ; 0xc3993
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc3994 vbe.c:177
    pop dx                                    ; 5a                          ; 0xc3997
    pop bp                                    ; 5d                          ; 0xc3998
    retn                                      ; c3                          ; 0xc3999
  ; disGetNextSymbol 0xc399a LB 0x660 -> off=0x0 cb=000000000000001a uValue=00000000000c399a 'dispi_set_id'
dispi_set_id:                                ; 0xc399a LB 0x1a
    push bp                                   ; 55                          ; 0xc399a vbe.c:179
    mov bp, sp                                ; 89 e5                       ; 0xc399b
    push bx                                   ; 53                          ; 0xc399d
    push dx                                   ; 52                          ; 0xc399e
    mov bx, ax                                ; 89 c3                       ; 0xc399f
    xor ax, ax                                ; 31 c0                       ; 0xc39a1 vbe.c:181
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc39a3
    out DX, ax                                ; ef                          ; 0xc39a6
    mov ax, bx                                ; 89 d8                       ; 0xc39a7 vbe.c:182
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc39a9
    out DX, ax                                ; ef                          ; 0xc39ac
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc39ad vbe.c:183
    pop dx                                    ; 5a                          ; 0xc39b0
    pop bx                                    ; 5b                          ; 0xc39b1
    pop bp                                    ; 5d                          ; 0xc39b2
    retn                                      ; c3                          ; 0xc39b3
  ; disGetNextSymbol 0xc39b4 LB 0x646 -> off=0x0 cb=000000000000002a uValue=00000000000c39b4 'vbe_init'
vbe_init:                                    ; 0xc39b4 LB 0x2a
    push bp                                   ; 55                          ; 0xc39b4 vbe.c:188
    mov bp, sp                                ; 89 e5                       ; 0xc39b5
    push bx                                   ; 53                          ; 0xc39b7
    mov ax, 0b0c0h                            ; b8 c0 b0                    ; 0xc39b8 vbe.c:190
    call 0399ah                               ; e8 dc ff                    ; 0xc39bb
    call 03986h                               ; e8 c5 ff                    ; 0xc39be vbe.c:191
    cmp ax, 0b0c0h                            ; 3d c0 b0                    ; 0xc39c1
    jne short 039d8h                          ; 75 12                       ; 0xc39c4
    mov bx, 000b9h                            ; bb b9 00                    ; 0xc39c6 vbe.c:42
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc39c9
    mov es, ax                                ; 8e c0                       ; 0xc39cc
    mov byte [es:bx], 001h                    ; 26 c6 07 01                 ; 0xc39ce
    mov ax, 0b0c4h                            ; b8 c4 b0                    ; 0xc39d2 vbe.c:194
    call 0399ah                               ; e8 c2 ff                    ; 0xc39d5
    lea sp, [bp-002h]                         ; 8d 66 fe                    ; 0xc39d8 vbe.c:199
    pop bx                                    ; 5b                          ; 0xc39db
    pop bp                                    ; 5d                          ; 0xc39dc
    retn                                      ; c3                          ; 0xc39dd
  ; disGetNextSymbol 0xc39de LB 0x61c -> off=0x0 cb=0000000000000055 uValue=00000000000c39de 'mode_info_find_mode'
mode_info_find_mode:                         ; 0xc39de LB 0x55
    push bp                                   ; 55                          ; 0xc39de vbe.c:202
    mov bp, sp                                ; 89 e5                       ; 0xc39df
    push bx                                   ; 53                          ; 0xc39e1
    push cx                                   ; 51                          ; 0xc39e2
    push si                                   ; 56                          ; 0xc39e3
    push di                                   ; 57                          ; 0xc39e4
    mov di, ax                                ; 89 c7                       ; 0xc39e5
    mov si, dx                                ; 89 d6                       ; 0xc39e7
    xor dx, dx                                ; 31 d2                       ; 0xc39e9 vbe.c:208
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc39eb
    call 03960h                               ; e8 6f ff                    ; 0xc39ee
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc39f1 vbe.c:209
    jne short 03a28h                          ; 75 32                       ; 0xc39f4
    mov bx, strict word 00004h                ; bb 04 00                    ; 0xc39f6 vbe.c:213
    mov dx, bx                                ; 89 da                       ; 0xc39f9 vbe.c:218
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc39fb
    call 03960h                               ; e8 5f ff                    ; 0xc39fe
    mov cx, ax                                ; 89 c1                       ; 0xc3a01
    cmp cx, strict byte 0ffffh                ; 83 f9 ff                    ; 0xc3a03 vbe.c:219
    je short 03a28h                           ; 74 20                       ; 0xc3a06
    lea dx, [bx+002h]                         ; 8d 57 02                    ; 0xc3a08 vbe.c:221
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a0b
    call 03960h                               ; e8 4f ff                    ; 0xc3a0e
    lea dx, [bx+044h]                         ; 8d 57 44                    ; 0xc3a11
    cmp cx, di                                ; 39 f9                       ; 0xc3a14 vbe.c:223
    jne short 03a24h                          ; 75 0c                       ; 0xc3a16
    test si, si                               ; 85 f6                       ; 0xc3a18 vbe.c:225
    jne short 03a20h                          ; 75 04                       ; 0xc3a1a
    mov ax, bx                                ; 89 d8                       ; 0xc3a1c vbe.c:226
    jmp short 03a2ah                          ; eb 0a                       ; 0xc3a1e
    test AL, strict byte 080h                 ; a8 80                       ; 0xc3a20 vbe.c:227
    jne short 03a1ch                          ; 75 f8                       ; 0xc3a22
    mov bx, dx                                ; 89 d3                       ; 0xc3a24 vbe.c:230
    jmp short 039fbh                          ; eb d3                       ; 0xc3a26 vbe.c:235
    xor ax, ax                                ; 31 c0                       ; 0xc3a28 vbe.c:238
    lea sp, [bp-008h]                         ; 8d 66 f8                    ; 0xc3a2a vbe.c:239
    pop di                                    ; 5f                          ; 0xc3a2d
    pop si                                    ; 5e                          ; 0xc3a2e
    pop cx                                    ; 59                          ; 0xc3a2f
    pop bx                                    ; 5b                          ; 0xc3a30
    pop bp                                    ; 5d                          ; 0xc3a31
    retn                                      ; c3                          ; 0xc3a32
  ; disGetNextSymbol 0xc3a33 LB 0x5c7 -> off=0x0 cb=0000000000000125 uValue=00000000000c3a33 'vbe_biosfn_return_controller_information'
vbe_biosfn_return_controller_information: ; 0xc3a33 LB 0x125
    push bp                                   ; 55                          ; 0xc3a33 vbe.c:270
    mov bp, sp                                ; 89 e5                       ; 0xc3a34
    push cx                                   ; 51                          ; 0xc3a36
    push si                                   ; 56                          ; 0xc3a37
    push di                                   ; 57                          ; 0xc3a38
    sub sp, strict byte 0000ah                ; 83 ec 0a                    ; 0xc3a39
    mov si, ax                                ; 89 c6                       ; 0xc3a3c
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3a3e
    mov di, bx                                ; 89 df                       ; 0xc3a41
    mov word [bp-00ch], strict word 00022h    ; c7 46 f4 22 00              ; 0xc3a43 vbe.c:275
    call 005a7h                               ; e8 5c cb                    ; 0xc3a48 vbe.c:278
    mov word [bp-010h], ax                    ; 89 46 f0                    ; 0xc3a4b
    mov bx, di                                ; 89 fb                       ; 0xc3a4e vbe.c:281
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3a50
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3a53
    xor dx, dx                                ; 31 d2                       ; 0xc3a56 vbe.c:284
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3a58
    call 03960h                               ; e8 02 ff                    ; 0xc3a5b
    cmp ax, 077cch                            ; 3d cc 77                    ; 0xc3a5e vbe.c:285
    je short 03a6dh                           ; 74 0a                       ; 0xc3a61
    push SS                                   ; 16                          ; 0xc3a63 vbe.c:287
    pop ES                                    ; 07                          ; 0xc3a64
    mov word [es:si], 00100h                  ; 26 c7 04 00 01              ; 0xc3a65
    jmp near 03b50h                           ; e9 e3 00                    ; 0xc3a6a vbe.c:291
    mov cx, strict word 00004h                ; b9 04 00                    ; 0xc3a6d vbe.c:293
    mov word [bp-00eh], strict word 00000h    ; c7 46 f2 00 00              ; 0xc3a70 vbe.c:300
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3a75 vbe.c:308
    cmp word [es:bx+002h], 03245h             ; 26 81 7f 02 45 32           ; 0xc3a78
    jne short 03a87h                          ; 75 07                       ; 0xc3a7e
    cmp word [es:bx], 04256h                  ; 26 81 3f 56 42              ; 0xc3a80
    je short 03a96h                           ; 74 0f                       ; 0xc3a85
    cmp word [es:bx+002h], 04153h             ; 26 81 7f 02 53 41           ; 0xc3a87
    jne short 03a9bh                          ; 75 0c                       ; 0xc3a8d
    cmp word [es:bx], 04556h                  ; 26 81 3f 56 45              ; 0xc3a8f
    jne short 03a9bh                          ; 75 05                       ; 0xc3a94
    mov word [bp-00eh], strict word 00001h    ; c7 46 f2 01 00              ; 0xc3a96 vbe.c:310
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3a9b vbe.c:318
    db  066h, 026h, 0c7h, 007h, 056h, 045h, 053h, 041h
    ; mov dword [es:bx], strict dword 041534556h ; 66 26 c7 07 56 45 53 41  ; 0xc3a9e
    mov word [es:bx+004h], 00200h             ; 26 c7 47 04 00 02           ; 0xc3aa6 vbe.c:324
    mov word [es:bx+006h], 07de6h             ; 26 c7 47 06 e6 7d           ; 0xc3aac vbe.c:327
    mov [es:bx+008h], ds                      ; 26 8c 5f 08                 ; 0xc3ab2
    db  066h, 026h, 0c7h, 047h, 00ah, 001h, 000h, 000h, 000h
    ; mov dword [es:bx+00ah], strict dword 000000001h ; 66 26 c7 47 0a 01 00 00 00; 0xc3ab6 vbe.c:330
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3abf vbe.c:336
    mov word [es:bx+010h], ax                 ; 26 89 47 10                 ; 0xc3ac2
    lea ax, [di+022h]                         ; 8d 45 22                    ; 0xc3ac6 vbe.c:337
    mov word [es:bx+00eh], ax                 ; 26 89 47 0e                 ; 0xc3ac9
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3acd vbe.c:340
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3ad0
    call 03960h                               ; e8 8a fe                    ; 0xc3ad3
    mov es, [bp-008h]                         ; 8e 46 f8                    ; 0xc3ad6
    mov word [es:bx+012h], ax                 ; 26 89 47 12                 ; 0xc3ad9
    cmp word [bp-00eh], strict byte 00000h    ; 83 7e f2 00                 ; 0xc3add vbe.c:342
    je short 03b07h                           ; 74 24                       ; 0xc3ae1
    mov word [es:bx+014h], strict word 00003h ; 26 c7 47 14 03 00           ; 0xc3ae3 vbe.c:345
    mov word [es:bx+016h], 07dfbh             ; 26 c7 47 16 fb 7d           ; 0xc3ae9 vbe.c:346
    mov [es:bx+018h], ds                      ; 26 8c 5f 18                 ; 0xc3aef
    mov word [es:bx+01ah], 07e0eh             ; 26 c7 47 1a 0e 7e           ; 0xc3af3 vbe.c:347
    mov [es:bx+01ch], ds                      ; 26 8c 5f 1c                 ; 0xc3af9
    mov word [es:bx+01eh], 07e2fh             ; 26 c7 47 1e 2f 7e           ; 0xc3afd vbe.c:348
    mov [es:bx+020h], ds                      ; 26 8c 5f 20                 ; 0xc3b03
    mov dx, cx                                ; 89 ca                       ; 0xc3b07 vbe.c:355
    add dx, strict byte 0001bh                ; 83 c2 1b                    ; 0xc3b09
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3b0c
    call 03972h                               ; e8 60 fe                    ; 0xc3b0f
    xor ah, ah                                ; 30 e4                       ; 0xc3b12 vbe.c:356
    cmp ax, word [bp-010h]                    ; 3b 46 f0                    ; 0xc3b14
    jnbe short 03b30h                         ; 77 17                       ; 0xc3b17
    mov dx, cx                                ; 89 ca                       ; 0xc3b19 vbe.c:358
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3b1b
    call 03960h                               ; e8 3f fe                    ; 0xc3b1e
    mov bx, word [bp-00ch]                    ; 8b 5e f4                    ; 0xc3b21 vbe.c:362
    add bx, di                                ; 01 fb                       ; 0xc3b24
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3b26 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3b29
    add word [bp-00ch], strict byte 00002h    ; 83 46 f4 02                 ; 0xc3b2c vbe.c:364
    add cx, strict byte 00044h                ; 83 c1 44                    ; 0xc3b30 vbe.c:366
    mov dx, cx                                ; 89 ca                       ; 0xc3b33 vbe.c:367
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3b35
    call 03960h                               ; e8 25 fe                    ; 0xc3b38
    cmp ax, strict word 0ffffh                ; 3d ff ff                    ; 0xc3b3b vbe.c:368
    jne short 03b07h                          ; 75 c7                       ; 0xc3b3e
    add di, word [bp-00ch]                    ; 03 7e f4                    ; 0xc3b40 vbe.c:371
    mov es, [bp-00ah]                         ; 8e 46 f6                    ; 0xc3b43 vbe.c:52
    mov word [es:di], ax                      ; 26 89 05                    ; 0xc3b46
    push SS                                   ; 16                          ; 0xc3b49 vbe.c:372
    pop ES                                    ; 07                          ; 0xc3b4a
    mov word [es:si], strict word 0004fh      ; 26 c7 04 4f 00              ; 0xc3b4b
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3b50 vbe.c:373
    pop di                                    ; 5f                          ; 0xc3b53
    pop si                                    ; 5e                          ; 0xc3b54
    pop cx                                    ; 59                          ; 0xc3b55
    pop bp                                    ; 5d                          ; 0xc3b56
    retn                                      ; c3                          ; 0xc3b57
  ; disGetNextSymbol 0xc3b58 LB 0x4a2 -> off=0x0 cb=000000000000009b uValue=00000000000c3b58 'vbe_biosfn_return_mode_information'
vbe_biosfn_return_mode_information:          ; 0xc3b58 LB 0x9b
    push bp                                   ; 55                          ; 0xc3b58 vbe.c:385
    mov bp, sp                                ; 89 e5                       ; 0xc3b59
    push si                                   ; 56                          ; 0xc3b5b
    push di                                   ; 57                          ; 0xc3b5c
    push ax                                   ; 50                          ; 0xc3b5d
    push ax                                   ; 50                          ; 0xc3b5e
    mov ax, dx                                ; 89 d0                       ; 0xc3b5f
    mov si, bx                                ; 89 de                       ; 0xc3b61
    mov bx, cx                                ; 89 cb                       ; 0xc3b63
    test dh, 040h                             ; f6 c6 40                    ; 0xc3b65 vbe.c:396
    db  00fh, 095h, 0c2h
    ; setne dl                                  ; 0f 95 c2                  ; 0xc3b68
    xor dh, dh                                ; 30 f6                       ; 0xc3b6b
    and ah, 001h                              ; 80 e4 01                    ; 0xc3b6d vbe.c:397
    call 039deh                               ; e8 6b fe                    ; 0xc3b70 vbe.c:399
    mov word [bp-006h], ax                    ; 89 46 fa                    ; 0xc3b73
    test ax, ax                               ; 85 c0                       ; 0xc3b76 vbe.c:401
    je short 03be1h                           ; 74 67                       ; 0xc3b78
    mov cx, 00100h                            ; b9 00 01                    ; 0xc3b7a vbe.c:406
    xor ax, ax                                ; 31 c0                       ; 0xc3b7d
    mov di, bx                                ; 89 df                       ; 0xc3b7f
    mov es, si                                ; 8e c6                       ; 0xc3b81
    jcxz 03b87h                               ; e3 02                       ; 0xc3b83
    rep stosb                                 ; f3 aa                       ; 0xc3b85
    xor cx, cx                                ; 31 c9                       ; 0xc3b87 vbe.c:407
    jmp short 03b90h                          ; eb 05                       ; 0xc3b89
    cmp cx, strict byte 00042h                ; 83 f9 42                    ; 0xc3b8b
    jnc short 03ba9h                          ; 73 19                       ; 0xc3b8e
    mov dx, word [bp-006h]                    ; 8b 56 fa                    ; 0xc3b90 vbe.c:410
    inc dx                                    ; 42                          ; 0xc3b93
    inc dx                                    ; 42                          ; 0xc3b94
    add dx, cx                                ; 01 ca                       ; 0xc3b95
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3b97
    call 03972h                               ; e8 d5 fd                    ; 0xc3b9a
    mov di, bx                                ; 89 df                       ; 0xc3b9d vbe.c:411
    add di, cx                                ; 01 cf                       ; 0xc3b9f
    mov es, si                                ; 8e c6                       ; 0xc3ba1 vbe.c:42
    mov byte [es:di], al                      ; 26 88 05                    ; 0xc3ba3
    inc cx                                    ; 41                          ; 0xc3ba6 vbe.c:412
    jmp short 03b8bh                          ; eb e2                       ; 0xc3ba7
    lea di, [bx+002h]                         ; 8d 7f 02                    ; 0xc3ba9 vbe.c:413
    mov es, si                                ; 8e c6                       ; 0xc3bac vbe.c:37
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3bae
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3bb1 vbe.c:414
    je short 03bc5h                           ; 74 10                       ; 0xc3bb3
    lea di, [bx+00ch]                         ; 8d 7f 0c                    ; 0xc3bb5 vbe.c:415
    mov word [es:di], 00619h                  ; 26 c7 05 19 06              ; 0xc3bb8 vbe.c:52
    lea di, [bx+00eh]                         ; 8d 7f 0e                    ; 0xc3bbd vbe.c:417
    mov word [es:di], 0c000h                  ; 26 c7 05 00 c0              ; 0xc3bc0 vbe.c:52
    mov ax, strict word 0000bh                ; b8 0b 00                    ; 0xc3bc5 vbe.c:420
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3bc8
    call 00560h                               ; e8 92 c9                    ; 0xc3bcb
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3bce vbe.c:421
    call 00567h                               ; e8 93 c9                    ; 0xc3bd1
    add bx, strict byte 0002ah                ; 83 c3 2a                    ; 0xc3bd4
    mov es, si                                ; 8e c6                       ; 0xc3bd7 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3bd9
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3bdc vbe.c:423
    jmp short 03be4h                          ; eb 03                       ; 0xc3bdf vbe.c:424
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3be1 vbe.c:428
    push SS                                   ; 16                          ; 0xc3be4 vbe.c:431
    pop ES                                    ; 07                          ; 0xc3be5
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3be6
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3be9
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3bec vbe.c:432
    pop di                                    ; 5f                          ; 0xc3bef
    pop si                                    ; 5e                          ; 0xc3bf0
    pop bp                                    ; 5d                          ; 0xc3bf1
    retn                                      ; c3                          ; 0xc3bf2
  ; disGetNextSymbol 0xc3bf3 LB 0x407 -> off=0x0 cb=00000000000000e5 uValue=00000000000c3bf3 'vbe_biosfn_set_mode'
vbe_biosfn_set_mode:                         ; 0xc3bf3 LB 0xe5
    push bp                                   ; 55                          ; 0xc3bf3 vbe.c:444
    mov bp, sp                                ; 89 e5                       ; 0xc3bf4
    push si                                   ; 56                          ; 0xc3bf6
    push di                                   ; 57                          ; 0xc3bf7
    sub sp, strict byte 00006h                ; 83 ec 06                    ; 0xc3bf8
    mov si, ax                                ; 89 c6                       ; 0xc3bfb
    mov word [bp-00ah], dx                    ; 89 56 f6                    ; 0xc3bfd
    test byte [bp-009h], 040h                 ; f6 46 f7 40                 ; 0xc3c00 vbe.c:452
    db  00fh, 095h, 0c0h
    ; setne al                                  ; 0f 95 c0                  ; 0xc3c04
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3c07
    mov ax, dx                                ; 89 d0                       ; 0xc3c0a
    test dx, dx                               ; 85 d2                       ; 0xc3c0c vbe.c:453
    je short 03c13h                           ; 74 03                       ; 0xc3c0e
    mov dx, strict word 00040h                ; ba 40 00                    ; 0xc3c10
    mov byte [bp-008h], dl                    ; 88 56 f8                    ; 0xc3c13
    test byte [bp-009h], 080h                 ; f6 46 f7 80                 ; 0xc3c16 vbe.c:454
    je short 03c21h                           ; 74 05                       ; 0xc3c1a
    mov dx, 00080h                            ; ba 80 00                    ; 0xc3c1c
    jmp short 03c23h                          ; eb 02                       ; 0xc3c1f
    xor dx, dx                                ; 31 d2                       ; 0xc3c21
    mov byte [bp-006h], dl                    ; 88 56 fa                    ; 0xc3c23
    and byte [bp-009h], 001h                  ; 80 66 f7 01                 ; 0xc3c26 vbe.c:456
    cmp word [bp-00ah], 00100h                ; 81 7e f6 00 01              ; 0xc3c2a vbe.c:459
    jnc short 03c43h                          ; 73 12                       ; 0xc3c2f
    xor ax, ax                                ; 31 c0                       ; 0xc3c31 vbe.c:463
    call 005cdh                               ; e8 97 c9                    ; 0xc3c33
    movzx ax, byte [bp-00ah]                  ; 0f b6 46 f6                 ; 0xc3c36 vbe.c:467
    call 0131ch                               ; e8 df d6                    ; 0xc3c3a
    mov ax, strict word 0004fh                ; b8 4f 00                    ; 0xc3c3d vbe.c:468
    jmp near 03ccch                           ; e9 89 00                    ; 0xc3c40 vbe.c:469
    mov dx, ax                                ; 89 c2                       ; 0xc3c43 vbe.c:472
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3c45
    call 039deh                               ; e8 93 fd                    ; 0xc3c48
    mov bx, ax                                ; 89 c3                       ; 0xc3c4b
    test ax, ax                               ; 85 c0                       ; 0xc3c4d vbe.c:474
    je short 03cc9h                           ; 74 78                       ; 0xc3c4f
    lea dx, [bx+014h]                         ; 8d 57 14                    ; 0xc3c51 vbe.c:479
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c54
    call 03960h                               ; e8 06 fd                    ; 0xc3c57
    mov cx, ax                                ; 89 c1                       ; 0xc3c5a
    lea dx, [bx+016h]                         ; 8d 57 16                    ; 0xc3c5c vbe.c:480
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c5f
    call 03960h                               ; e8 fb fc                    ; 0xc3c62
    mov di, ax                                ; 89 c7                       ; 0xc3c65
    lea dx, [bx+01bh]                         ; 8d 57 1b                    ; 0xc3c67 vbe.c:481
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3c6a
    call 03972h                               ; e8 02 fd                    ; 0xc3c6d
    mov bl, al                                ; 88 c3                       ; 0xc3c70
    mov dl, al                                ; 88 c2                       ; 0xc3c72
    xor ax, ax                                ; 31 c0                       ; 0xc3c74 vbe.c:489
    call 005cdh                               ; e8 54 c9                    ; 0xc3c76
    cmp bl, 004h                              ; 80 fb 04                    ; 0xc3c79 vbe.c:491
    jne short 03c84h                          ; 75 06                       ; 0xc3c7c
    mov ax, strict word 0006ah                ; b8 6a 00                    ; 0xc3c7e vbe.c:493
    call 0131ch                               ; e8 98 d6                    ; 0xc3c81
    movzx ax, dl                              ; 0f b6 c2                    ; 0xc3c84 vbe.c:496
    call 038d7h                               ; e8 4d fc                    ; 0xc3c87
    mov ax, cx                                ; 89 c8                       ; 0xc3c8a vbe.c:497
    call 03880h                               ; e8 f1 fb                    ; 0xc3c8c
    mov ax, di                                ; 89 f8                       ; 0xc3c8f vbe.c:498
    call 0389fh                               ; e8 0b fc                    ; 0xc3c91
    xor ax, ax                                ; 31 c0                       ; 0xc3c94 vbe.c:499
    call 005f3h                               ; e8 5a c9                    ; 0xc3c96
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3c99 vbe.c:500
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc3c9c
    movzx dx, al                              ; 0f b6 d0                    ; 0xc3c9e
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc3ca1
    or ax, dx                                 ; 09 d0                       ; 0xc3ca5
    call 005cdh                               ; e8 23 c9                    ; 0xc3ca7
    call 006c2h                               ; e8 15 ca                    ; 0xc3caa vbe.c:501
    mov bx, 000bah                            ; bb ba 00                    ; 0xc3cad vbe.c:52
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3cb0
    mov es, ax                                ; 8e c0                       ; 0xc3cb3
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3cb5
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3cb8
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3cbb vbe.c:504
    or AL, strict byte 060h                   ; 0c 60                       ; 0xc3cbe
    mov bx, 00087h                            ; bb 87 00                    ; 0xc3cc0 vbe.c:42
    mov byte [es:bx], al                      ; 26 88 07                    ; 0xc3cc3
    jmp near 03c3dh                           ; e9 74 ff                    ; 0xc3cc6
    mov ax, 00100h                            ; b8 00 01                    ; 0xc3cc9 vbe.c:513
    push SS                                   ; 16                          ; 0xc3ccc vbe.c:517
    pop ES                                    ; 07                          ; 0xc3ccd
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3cce
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3cd1 vbe.c:518
    pop di                                    ; 5f                          ; 0xc3cd4
    pop si                                    ; 5e                          ; 0xc3cd5
    pop bp                                    ; 5d                          ; 0xc3cd6
    retn                                      ; c3                          ; 0xc3cd7
  ; disGetNextSymbol 0xc3cd8 LB 0x322 -> off=0x0 cb=0000000000000008 uValue=00000000000c3cd8 'vbe_biosfn_read_video_state_size'
vbe_biosfn_read_video_state_size:            ; 0xc3cd8 LB 0x8
    push bp                                   ; 55                          ; 0xc3cd8 vbe.c:520
    mov bp, sp                                ; 89 e5                       ; 0xc3cd9
    mov ax, strict word 00012h                ; b8 12 00                    ; 0xc3cdb vbe.c:523
    pop bp                                    ; 5d                          ; 0xc3cde
    retn                                      ; c3                          ; 0xc3cdf
  ; disGetNextSymbol 0xc3ce0 LB 0x31a -> off=0x0 cb=000000000000004b uValue=00000000000c3ce0 'vbe_biosfn_save_video_state'
vbe_biosfn_save_video_state:                 ; 0xc3ce0 LB 0x4b
    push bp                                   ; 55                          ; 0xc3ce0 vbe.c:525
    mov bp, sp                                ; 89 e5                       ; 0xc3ce1
    push bx                                   ; 53                          ; 0xc3ce3
    push cx                                   ; 51                          ; 0xc3ce4
    push si                                   ; 56                          ; 0xc3ce5
    mov si, ax                                ; 89 c6                       ; 0xc3ce6
    mov bx, dx                                ; 89 d3                       ; 0xc3ce8
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3cea vbe.c:529
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3ced
    out DX, ax                                ; ef                          ; 0xc3cf0
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3cf1 vbe.c:530
    in ax, DX                                 ; ed                          ; 0xc3cf4
    mov es, si                                ; 8e c6                       ; 0xc3cf5 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3cf7
    inc bx                                    ; 43                          ; 0xc3cfa vbe.c:532
    inc bx                                    ; 43                          ; 0xc3cfb
    test AL, strict byte 001h                 ; a8 01                       ; 0xc3cfc vbe.c:533
    je short 03d23h                           ; 74 23                       ; 0xc3cfe
    mov cx, strict word 00001h                ; b9 01 00                    ; 0xc3d00 vbe.c:535
    jmp short 03d0ah                          ; eb 05                       ; 0xc3d03
    cmp cx, strict byte 00009h                ; 83 f9 09                    ; 0xc3d05
    jnbe short 03d23h                         ; 77 19                       ; 0xc3d08
    cmp cx, strict byte 00004h                ; 83 f9 04                    ; 0xc3d0a vbe.c:536
    je short 03d20h                           ; 74 11                       ; 0xc3d0d
    mov ax, cx                                ; 89 c8                       ; 0xc3d0f vbe.c:537
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d11
    out DX, ax                                ; ef                          ; 0xc3d14
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d15 vbe.c:538
    in ax, DX                                 ; ed                          ; 0xc3d18
    mov es, si                                ; 8e c6                       ; 0xc3d19 vbe.c:52
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3d1b
    inc bx                                    ; 43                          ; 0xc3d1e vbe.c:539
    inc bx                                    ; 43                          ; 0xc3d1f
    inc cx                                    ; 41                          ; 0xc3d20 vbe.c:541
    jmp short 03d05h                          ; eb e2                       ; 0xc3d21
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3d23 vbe.c:542
    pop si                                    ; 5e                          ; 0xc3d26
    pop cx                                    ; 59                          ; 0xc3d27
    pop bx                                    ; 5b                          ; 0xc3d28
    pop bp                                    ; 5d                          ; 0xc3d29
    retn                                      ; c3                          ; 0xc3d2a
  ; disGetNextSymbol 0xc3d2b LB 0x2cf -> off=0x0 cb=000000000000008f uValue=00000000000c3d2b 'vbe_biosfn_restore_video_state'
vbe_biosfn_restore_video_state:              ; 0xc3d2b LB 0x8f
    push bp                                   ; 55                          ; 0xc3d2b vbe.c:545
    mov bp, sp                                ; 89 e5                       ; 0xc3d2c
    push bx                                   ; 53                          ; 0xc3d2e
    push cx                                   ; 51                          ; 0xc3d2f
    push si                                   ; 56                          ; 0xc3d30
    push ax                                   ; 50                          ; 0xc3d31
    mov cx, ax                                ; 89 c1                       ; 0xc3d32
    mov bx, dx                                ; 89 d3                       ; 0xc3d34
    mov es, ax                                ; 8e c0                       ; 0xc3d36 vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3d38
    mov word [bp-008h], ax                    ; 89 46 f8                    ; 0xc3d3b
    inc bx                                    ; 43                          ; 0xc3d3e vbe.c:550
    inc bx                                    ; 43                          ; 0xc3d3f
    test byte [bp-008h], 001h                 ; f6 46 f8 01                 ; 0xc3d40 vbe.c:552
    jne short 03d56h                          ; 75 10                       ; 0xc3d44
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3d46 vbe.c:553
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d49
    out DX, ax                                ; ef                          ; 0xc3d4c
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3d4d vbe.c:554
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d50
    out DX, ax                                ; ef                          ; 0xc3d53
    jmp short 03db2h                          ; eb 5c                       ; 0xc3d54 vbe.c:555
    mov ax, strict word 00001h                ; b8 01 00                    ; 0xc3d56 vbe.c:556
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d59
    out DX, ax                                ; ef                          ; 0xc3d5c
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3d5d vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d60 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3d63
    inc bx                                    ; 43                          ; 0xc3d64 vbe.c:558
    inc bx                                    ; 43                          ; 0xc3d65
    mov ax, strict word 00002h                ; b8 02 00                    ; 0xc3d66
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d69
    out DX, ax                                ; ef                          ; 0xc3d6c
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3d6d vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d70 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3d73
    inc bx                                    ; 43                          ; 0xc3d74 vbe.c:561
    inc bx                                    ; 43                          ; 0xc3d75
    mov ax, strict word 00003h                ; b8 03 00                    ; 0xc3d76
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d79
    out DX, ax                                ; ef                          ; 0xc3d7c
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3d7d vbe.c:47
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d80 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3d83
    inc bx                                    ; 43                          ; 0xc3d84 vbe.c:564
    inc bx                                    ; 43                          ; 0xc3d85
    mov ax, strict word 00004h                ; b8 04 00                    ; 0xc3d86
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3d89
    out DX, ax                                ; ef                          ; 0xc3d8c
    mov ax, word [bp-008h]                    ; 8b 46 f8                    ; 0xc3d8d vbe.c:566
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3d90
    out DX, ax                                ; ef                          ; 0xc3d93
    mov si, strict word 00005h                ; be 05 00                    ; 0xc3d94 vbe.c:568
    jmp short 03d9eh                          ; eb 05                       ; 0xc3d97
    cmp si, strict byte 00009h                ; 83 fe 09                    ; 0xc3d99
    jnbe short 03db2h                         ; 77 14                       ; 0xc3d9c
    mov ax, si                                ; 89 f0                       ; 0xc3d9e vbe.c:569
    mov dx, 001ceh                            ; ba ce 01                    ; 0xc3da0
    out DX, ax                                ; ef                          ; 0xc3da3
    mov es, cx                                ; 8e c1                       ; 0xc3da4 vbe.c:47
    mov ax, word [es:bx]                      ; 26 8b 07                    ; 0xc3da6
    mov dx, 001cfh                            ; ba cf 01                    ; 0xc3da9 vbe.c:48
    out DX, ax                                ; ef                          ; 0xc3dac
    inc bx                                    ; 43                          ; 0xc3dad vbe.c:571
    inc bx                                    ; 43                          ; 0xc3dae
    inc si                                    ; 46                          ; 0xc3daf vbe.c:572
    jmp short 03d99h                          ; eb e7                       ; 0xc3db0
    lea sp, [bp-006h]                         ; 8d 66 fa                    ; 0xc3db2 vbe.c:574
    pop si                                    ; 5e                          ; 0xc3db5
    pop cx                                    ; 59                          ; 0xc3db6
    pop bx                                    ; 5b                          ; 0xc3db7
    pop bp                                    ; 5d                          ; 0xc3db8
    retn                                      ; c3                          ; 0xc3db9
  ; disGetNextSymbol 0xc3dba LB 0x240 -> off=0x0 cb=000000000000008c uValue=00000000000c3dba 'vbe_biosfn_save_restore_state'
vbe_biosfn_save_restore_state:               ; 0xc3dba LB 0x8c
    push bp                                   ; 55                          ; 0xc3dba vbe.c:590
    mov bp, sp                                ; 89 e5                       ; 0xc3dbb
    push si                                   ; 56                          ; 0xc3dbd
    push di                                   ; 57                          ; 0xc3dbe
    push ax                                   ; 50                          ; 0xc3dbf
    mov si, ax                                ; 89 c6                       ; 0xc3dc0
    mov word [bp-006h], dx                    ; 89 56 fa                    ; 0xc3dc2
    mov ax, bx                                ; 89 d8                       ; 0xc3dc5
    mov bx, word [bp+004h]                    ; 8b 5e 04                    ; 0xc3dc7
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc3dca vbe.c:595
    xor ah, ah                                ; 30 e4                       ; 0xc3dcd vbe.c:596
    cmp ax, strict word 00002h                ; 3d 02 00                    ; 0xc3dcf
    je short 03e19h                           ; 74 45                       ; 0xc3dd2
    cmp ax, strict word 00001h                ; 3d 01 00                    ; 0xc3dd4
    je short 03dfdh                           ; 74 24                       ; 0xc3dd7
    test ax, ax                               ; 85 c0                       ; 0xc3dd9
    jne short 03e35h                          ; 75 58                       ; 0xc3ddb
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3ddd vbe.c:598
    call 02dd8h                               ; e8 f5 ef                    ; 0xc3de0
    mov cx, ax                                ; 89 c1                       ; 0xc3de3
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3de5 vbe.c:602
    je short 03df0h                           ; 74 05                       ; 0xc3de9
    call 03cd8h                               ; e8 ea fe                    ; 0xc3deb vbe.c:603
    add ax, cx                                ; 01 c8                       ; 0xc3dee
    add ax, strict word 0003fh                ; 05 3f 00                    ; 0xc3df0 vbe.c:604
    shr ax, 006h                              ; c1 e8 06                    ; 0xc3df3
    push SS                                   ; 16                          ; 0xc3df6
    pop ES                                    ; 07                          ; 0xc3df7
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3df8
    jmp short 03e38h                          ; eb 3b                       ; 0xc3dfb vbe.c:605
    push SS                                   ; 16                          ; 0xc3dfd vbe.c:607
    pop ES                                    ; 07                          ; 0xc3dfe
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3dff
    mov dx, cx                                ; 89 ca                       ; 0xc3e02 vbe.c:608
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3e04
    call 02e13h                               ; e8 09 f0                    ; 0xc3e07
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3e0a vbe.c:612
    je short 03e38h                           ; 74 28                       ; 0xc3e0e
    mov dx, ax                                ; 89 c2                       ; 0xc3e10 vbe.c:613
    mov ax, cx                                ; 89 c8                       ; 0xc3e12
    call 03ce0h                               ; e8 c9 fe                    ; 0xc3e14
    jmp short 03e38h                          ; eb 1f                       ; 0xc3e17 vbe.c:614
    push SS                                   ; 16                          ; 0xc3e19 vbe.c:616
    pop ES                                    ; 07                          ; 0xc3e1a
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3e1b
    mov dx, cx                                ; 89 ca                       ; 0xc3e1e vbe.c:617
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3e20
    call 030e9h                               ; e8 c3 f2                    ; 0xc3e23
    test byte [bp-006h], 008h                 ; f6 46 fa 08                 ; 0xc3e26 vbe.c:621
    je short 03e38h                           ; 74 0c                       ; 0xc3e2a
    mov dx, ax                                ; 89 c2                       ; 0xc3e2c vbe.c:622
    mov ax, cx                                ; 89 c8                       ; 0xc3e2e
    call 03d2bh                               ; e8 f8 fe                    ; 0xc3e30
    jmp short 03e38h                          ; eb 03                       ; 0xc3e33 vbe.c:623
    mov di, 00100h                            ; bf 00 01                    ; 0xc3e35 vbe.c:626
    push SS                                   ; 16                          ; 0xc3e38 vbe.c:629
    pop ES                                    ; 07                          ; 0xc3e39
    mov word [es:si], di                      ; 26 89 3c                    ; 0xc3e3a
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3e3d vbe.c:630
    pop di                                    ; 5f                          ; 0xc3e40
    pop si                                    ; 5e                          ; 0xc3e41
    pop bp                                    ; 5d                          ; 0xc3e42
    retn 00002h                               ; c2 02 00                    ; 0xc3e43
  ; disGetNextSymbol 0xc3e46 LB 0x1b4 -> off=0x0 cb=00000000000000cf uValue=00000000000c3e46 'vbe_biosfn_get_set_scanline_length'
vbe_biosfn_get_set_scanline_length:          ; 0xc3e46 LB 0xcf
    push bp                                   ; 55                          ; 0xc3e46 vbe.c:651
    mov bp, sp                                ; 89 e5                       ; 0xc3e47
    push si                                   ; 56                          ; 0xc3e49
    push di                                   ; 57                          ; 0xc3e4a
    sub sp, strict byte 00008h                ; 83 ec 08                    ; 0xc3e4b
    push ax                                   ; 50                          ; 0xc3e4e
    mov di, dx                                ; 89 d7                       ; 0xc3e4f
    mov si, bx                                ; 89 de                       ; 0xc3e51
    mov word [bp-008h], cx                    ; 89 4e f8                    ; 0xc3e53
    call 038f6h                               ; e8 9d fa                    ; 0xc3e56 vbe.c:660
    cmp AL, strict byte 00fh                  ; 3c 0f                       ; 0xc3e59 vbe.c:661
    jne short 03e62h                          ; 75 05                       ; 0xc3e5b
    mov cx, strict word 00010h                ; b9 10 00                    ; 0xc3e5d
    jmp short 03e65h                          ; eb 03                       ; 0xc3e60
    movzx cx, al                              ; 0f b6 c8                    ; 0xc3e62
    call 0392eh                               ; e8 c6 fa                    ; 0xc3e65 vbe.c:662
    mov word [bp-00ah], ax                    ; 89 46 f6                    ; 0xc3e68
    mov word [bp-006h], strict word 0004fh    ; c7 46 fa 4f 00              ; 0xc3e6b vbe.c:663
    push SS                                   ; 16                          ; 0xc3e70 vbe.c:664
    pop ES                                    ; 07                          ; 0xc3e71
    mov bx, word [es:si]                      ; 26 8b 1c                    ; 0xc3e72
    mov al, byte [es:di]                      ; 26 8a 05                    ; 0xc3e75 vbe.c:665
    cmp AL, strict byte 002h                  ; 3c 02                       ; 0xc3e78 vbe.c:669
    je short 03e87h                           ; 74 0b                       ; 0xc3e7a
    cmp AL, strict byte 001h                  ; 3c 01                       ; 0xc3e7c
    je short 03eaeh                           ; 74 2e                       ; 0xc3e7e
    test al, al                               ; 84 c0                       ; 0xc3e80
    je short 03ea9h                           ; 74 25                       ; 0xc3e82
    jmp near 03efeh                           ; e9 77 00                    ; 0xc3e84
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc3e87 vbe.c:671
    jne short 03e91h                          ; 75 05                       ; 0xc3e8a
    sal bx, 003h                              ; c1 e3 03                    ; 0xc3e8c vbe.c:672
    jmp short 03ea9h                          ; eb 18                       ; 0xc3e8f vbe.c:673
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc3e91 vbe.c:674
    cwd                                       ; 99                          ; 0xc3e94
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3e95
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3e98
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3e9a
    mov word [bp-00ch], ax                    ; 89 46 f4                    ; 0xc3e9d
    mov ax, bx                                ; 89 d8                       ; 0xc3ea0
    xor dx, dx                                ; 31 d2                       ; 0xc3ea2
    div word [bp-00ch]                        ; f7 76 f4                    ; 0xc3ea4
    mov bx, ax                                ; 89 c3                       ; 0xc3ea7
    mov ax, bx                                ; 89 d8                       ; 0xc3ea9 vbe.c:677
    call 0390fh                               ; e8 61 fa                    ; 0xc3eab
    call 0392eh                               ; e8 7d fa                    ; 0xc3eae vbe.c:680
    mov bx, ax                                ; 89 c3                       ; 0xc3eb1
    push SS                                   ; 16                          ; 0xc3eb3 vbe.c:681
    pop ES                                    ; 07                          ; 0xc3eb4
    mov word [es:si], ax                      ; 26 89 04                    ; 0xc3eb5
    cmp cl, 004h                              ; 80 f9 04                    ; 0xc3eb8 vbe.c:682
    jne short 03ec2h                          ; 75 05                       ; 0xc3ebb
    shr bx, 003h                              ; c1 eb 03                    ; 0xc3ebd vbe.c:683
    jmp short 03ed1h                          ; eb 0f                       ; 0xc3ec0 vbe.c:684
    movzx ax, cl                              ; 0f b6 c1                    ; 0xc3ec2 vbe.c:685
    cwd                                       ; 99                          ; 0xc3ec5
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3ec6
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3ec9
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3ecb
    imul bx, ax                               ; 0f af d8                    ; 0xc3ece
    add bx, strict byte 00003h                ; 83 c3 03                    ; 0xc3ed1 vbe.c:686
    and bl, 0fch                              ; 80 e3 fc                    ; 0xc3ed4
    push SS                                   ; 16                          ; 0xc3ed7 vbe.c:687
    pop ES                                    ; 07                          ; 0xc3ed8
    mov word [es:di], bx                      ; 26 89 1d                    ; 0xc3ed9
    call 03947h                               ; e8 68 fa                    ; 0xc3edc vbe.c:688
    push SS                                   ; 16                          ; 0xc3edf
    pop ES                                    ; 07                          ; 0xc3ee0
    mov bx, word [bp-008h]                    ; 8b 5e f8                    ; 0xc3ee1
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3ee4
    call 038beh                               ; e8 d4 f9                    ; 0xc3ee7 vbe.c:689
    push SS                                   ; 16                          ; 0xc3eea
    pop ES                                    ; 07                          ; 0xc3eeb
    cmp ax, word [es:bx]                      ; 26 3b 07                    ; 0xc3eec
    jbe short 03f03h                          ; 76 12                       ; 0xc3eef
    mov ax, word [bp-00ah]                    ; 8b 46 f6                    ; 0xc3ef1 vbe.c:690
    call 0390fh                               ; e8 18 fa                    ; 0xc3ef4
    mov word [bp-006h], 00200h                ; c7 46 fa 00 02              ; 0xc3ef7 vbe.c:691
    jmp short 03f03h                          ; eb 05                       ; 0xc3efc vbe.c:693
    mov word [bp-006h], 00100h                ; c7 46 fa 00 01              ; 0xc3efe vbe.c:696
    push SS                                   ; 16                          ; 0xc3f03 vbe.c:699
    pop ES                                    ; 07                          ; 0xc3f04
    mov ax, word [bp-006h]                    ; 8b 46 fa                    ; 0xc3f05
    mov bx, word [bp-00eh]                    ; 8b 5e f2                    ; 0xc3f08
    mov word [es:bx], ax                      ; 26 89 07                    ; 0xc3f0b
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3f0e vbe.c:700
    pop di                                    ; 5f                          ; 0xc3f11
    pop si                                    ; 5e                          ; 0xc3f12
    pop bp                                    ; 5d                          ; 0xc3f13
    retn                                      ; c3                          ; 0xc3f14
  ; disGetNextSymbol 0xc3f15 LB 0xe5 -> off=0x0 cb=00000000000000e5 uValue=00000000000c3f15 'private_biosfn_custom_mode'
private_biosfn_custom_mode:                  ; 0xc3f15 LB 0xe5
    push bp                                   ; 55                          ; 0xc3f15 vbe.c:726
    mov bp, sp                                ; 89 e5                       ; 0xc3f16
    push si                                   ; 56                          ; 0xc3f18
    push di                                   ; 57                          ; 0xc3f19
    push ax                                   ; 50                          ; 0xc3f1a
    push ax                                   ; 50                          ; 0xc3f1b
    push ax                                   ; 50                          ; 0xc3f1c
    mov si, dx                                ; 89 d6                       ; 0xc3f1d
    mov dx, cx                                ; 89 ca                       ; 0xc3f1f
    mov di, strict word 0004fh                ; bf 4f 00                    ; 0xc3f21 vbe.c:739
    push SS                                   ; 16                          ; 0xc3f24 vbe.c:740
    pop ES                                    ; 07                          ; 0xc3f25
    mov al, byte [es:si]                      ; 26 8a 04                    ; 0xc3f26
    test al, al                               ; 84 c0                       ; 0xc3f29 vbe.c:741
    jne short 03f4fh                          ; 75 22                       ; 0xc3f2b
    push SS                                   ; 16                          ; 0xc3f2d vbe.c:743
    pop ES                                    ; 07                          ; 0xc3f2e
    mov cx, word [es:bx]                      ; 26 8b 0f                    ; 0xc3f2f
    mov bx, dx                                ; 89 d3                       ; 0xc3f32 vbe.c:744
    mov bx, word [es:bx]                      ; 26 8b 1f                    ; 0xc3f34
    mov ax, word [es:si]                      ; 26 8b 04                    ; 0xc3f37 vbe.c:745
    shr ax, 008h                              ; c1 e8 08                    ; 0xc3f3a
    and ax, strict word 0007fh                ; 25 7f 00                    ; 0xc3f3d
    mov byte [bp-008h], al                    ; 88 46 f8                    ; 0xc3f40
    cmp AL, strict byte 008h                  ; 3c 08                       ; 0xc3f43 vbe.c:750
    je short 03f55h                           ; 74 0e                       ; 0xc3f45
    cmp AL, strict byte 010h                  ; 3c 10                       ; 0xc3f47
    je short 03f55h                           ; 74 0a                       ; 0xc3f49
    cmp AL, strict byte 020h                  ; 3c 20                       ; 0xc3f4b
    je short 03f55h                           ; 74 06                       ; 0xc3f4d
    mov di, 00100h                            ; bf 00 01                    ; 0xc3f4f vbe.c:751
    jmp near 03febh                           ; e9 96 00                    ; 0xc3f52 vbe.c:752
    push SS                                   ; 16                          ; 0xc3f55 vbe.c:756
    pop ES                                    ; 07                          ; 0xc3f56
    test byte [es:si+001h], 080h              ; 26 f6 44 01 80              ; 0xc3f57
    je short 03f63h                           ; 74 05                       ; 0xc3f5c
    mov ax, strict word 00040h                ; b8 40 00                    ; 0xc3f5e
    jmp short 03f65h                          ; eb 02                       ; 0xc3f61
    xor ax, ax                                ; 31 c0                       ; 0xc3f63
    mov byte [bp-006h], al                    ; 88 46 fa                    ; 0xc3f65
    cmp cx, 00280h                            ; 81 f9 80 02                 ; 0xc3f68 vbe.c:759
    jnc short 03f73h                          ; 73 05                       ; 0xc3f6c
    mov cx, 00280h                            ; b9 80 02                    ; 0xc3f6e vbe.c:760
    jmp short 03f7ch                          ; eb 09                       ; 0xc3f71 vbe.c:761
    cmp cx, 00a00h                            ; 81 f9 00 0a                 ; 0xc3f73
    jbe short 03f7ch                          ; 76 03                       ; 0xc3f77
    mov cx, 00a00h                            ; b9 00 0a                    ; 0xc3f79 vbe.c:762
    cmp bx, 001e0h                            ; 81 fb e0 01                 ; 0xc3f7c vbe.c:763
    jnc short 03f87h                          ; 73 05                       ; 0xc3f80
    mov bx, 001e0h                            ; bb e0 01                    ; 0xc3f82 vbe.c:764
    jmp short 03f90h                          ; eb 09                       ; 0xc3f85 vbe.c:765
    cmp bx, 00780h                            ; 81 fb 80 07                 ; 0xc3f87
    jbe short 03f90h                          ; 76 03                       ; 0xc3f8b
    mov bx, 00780h                            ; bb 80 07                    ; 0xc3f8d vbe.c:766
    mov dx, strict word 0ffffh                ; ba ff ff                    ; 0xc3f90 vbe.c:772
    mov ax, 003b6h                            ; b8 b6 03                    ; 0xc3f93
    call 03960h                               ; e8 c7 f9                    ; 0xc3f96
    mov si, ax                                ; 89 c6                       ; 0xc3f99
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc3f9b vbe.c:775
    cwd                                       ; 99                          ; 0xc3f9f
    sal dx, 003h                              ; c1 e2 03                    ; 0xc3fa0
    db  01bh, 0c2h
    ; sbb ax, dx                                ; 1b c2                     ; 0xc3fa3
    sar ax, 003h                              ; c1 f8 03                    ; 0xc3fa5
    imul ax, cx                               ; 0f af c1                    ; 0xc3fa8
    add ax, strict word 00003h                ; 05 03 00                    ; 0xc3fab vbe.c:776
    and AL, strict byte 0fch                  ; 24 fc                       ; 0xc3fae
    mov dx, bx                                ; 89 da                       ; 0xc3fb0 vbe.c:778
    mul dx                                    ; f7 e2                       ; 0xc3fb2
    cmp dx, si                                ; 39 f2                       ; 0xc3fb4 vbe.c:780
    jnbe short 03fbeh                         ; 77 06                       ; 0xc3fb6
    jne short 03fc3h                          ; 75 09                       ; 0xc3fb8
    test ax, ax                               ; 85 c0                       ; 0xc3fba
    jbe short 03fc3h                          ; 76 05                       ; 0xc3fbc
    mov di, 00200h                            ; bf 00 02                    ; 0xc3fbe vbe.c:782
    jmp short 03febh                          ; eb 28                       ; 0xc3fc1 vbe.c:783
    xor ax, ax                                ; 31 c0                       ; 0xc3fc3 vbe.c:787
    call 005cdh                               ; e8 05 c6                    ; 0xc3fc5
    movzx ax, byte [bp-008h]                  ; 0f b6 46 f8                 ; 0xc3fc8 vbe.c:788
    call 038d7h                               ; e8 08 f9                    ; 0xc3fcc
    mov ax, cx                                ; 89 c8                       ; 0xc3fcf vbe.c:789
    call 03880h                               ; e8 ac f8                    ; 0xc3fd1
    mov ax, bx                                ; 89 d8                       ; 0xc3fd4 vbe.c:790
    call 0389fh                               ; e8 c6 f8                    ; 0xc3fd6
    xor ax, ax                                ; 31 c0                       ; 0xc3fd9 vbe.c:791
    call 005f3h                               ; e8 15 c6                    ; 0xc3fdb
    mov al, byte [bp-006h]                    ; 8a 46 fa                    ; 0xc3fde vbe.c:792
    or AL, strict byte 001h                   ; 0c 01                       ; 0xc3fe1
    xor ah, ah                                ; 30 e4                       ; 0xc3fe3
    call 005cdh                               ; e8 e5 c5                    ; 0xc3fe5
    call 006c2h                               ; e8 d7 c6                    ; 0xc3fe8 vbe.c:793
    push SS                                   ; 16                          ; 0xc3feb vbe.c:801
    pop ES                                    ; 07                          ; 0xc3fec
    mov bx, word [bp-00ah]                    ; 8b 5e f6                    ; 0xc3fed
    mov word [es:bx], di                      ; 26 89 3f                    ; 0xc3ff0
    lea sp, [bp-004h]                         ; 8d 66 fc                    ; 0xc3ff3 vbe.c:802
    pop di                                    ; 5f                          ; 0xc3ff6
    pop si                                    ; 5e                          ; 0xc3ff7
    pop bp                                    ; 5d                          ; 0xc3ff8
    retn                                      ; c3                          ; 0xc3ff9

  ; Padding 0x606 bytes at 0xc3ffa
  times 1542 db 0

section VBE32 progbits vstart=0x4600 align=1 ; size=0x115 class=CODE group=AUTO
  ; disGetNextSymbol 0xc4600 LB 0x115 -> off=0x0 cb=0000000000000114 uValue=00000000000c0000 'vesa_pm_start'
vesa_pm_start:                               ; 0xc4600 LB 0x114
    sbb byte [bx+si], al                      ; 18 00                       ; 0xc4600
    dec di                                    ; 4f                          ; 0xc4602
    add byte [bx+si], dl                      ; 00 10                       ; 0xc4603
    add word [bx+si], cx                      ; 01 08                       ; 0xc4605
    add dh, cl                                ; 00 ce                       ; 0xc4607
    add di, cx                                ; 01 cf                       ; 0xc4609
    add di, cx                                ; 01 cf                       ; 0xc460b
    add ax, dx                                ; 01 d0                       ; 0xc460d
    add word [bp-048fdh], si                  ; 01 b6 03 b7                 ; 0xc460f
    db  003h, 0ffh
    ; add di, di                                ; 03 ff                     ; 0xc4613
    db  0ffh
    db  0ffh
    jmp word [bp-07dh]                        ; ff 66 83                    ; 0xc4617
    sti                                       ; fb                          ; 0xc461a
    add byte [si+005h], dh                    ; 00 74 05                    ; 0xc461b
    mov eax, strict dword 066c30100h          ; 66 b8 00 01 c3 66           ; 0xc461e vberom.asm:825
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc4624
    push edx                                  ; 66 52                       ; 0xc4626 vberom.asm:829
    push eax                                  ; 66 50                       ; 0xc4628 vberom.asm:830
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc462a vberom.asm:831
    add ax, 06600h                            ; 05 00 66                    ; 0xc4630
    out DX, ax                                ; ef                          ; 0xc4633
    pop eax                                   ; 66 58                       ; 0xc4634 vberom.asm:834
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc4636 vberom.asm:835
    in eax, DX                                ; 66 ed                       ; 0xc463c vberom.asm:837
    pop edx                                   ; 66 5a                       ; 0xc463e vberom.asm:838
    db  066h, 03bh, 0d0h
    ; cmp edx, eax                              ; 66 3b d0                  ; 0xc4640 vberom.asm:839
    jne short 0464ah                          ; 75 05                       ; 0xc4643 vberom.asm:840
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc4645 vberom.asm:841
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc464b
    retn                                      ; c3                          ; 0xc464e vberom.asm:845
    cmp bl, 080h                              ; 80 fb 80                    ; 0xc464f vberom.asm:847
    je short 0465eh                           ; 74 0a                       ; 0xc4652 vberom.asm:848
    cmp bl, 000h                              ; 80 fb 00                    ; 0xc4654 vberom.asm:849
    je short 0466eh                           ; 74 15                       ; 0xc4657 vberom.asm:850
    mov eax, strict dword 052c30100h          ; 66 b8 00 01 c3 52           ; 0xc4659 vberom.asm:851
    mov edx, strict dword 0a8ec03dah          ; 66 ba da 03 ec a8           ; 0xc465f vberom.asm:855
    or byte [di-005h], dh                     ; 08 75 fb                    ; 0xc4665
    in AL, DX                                 ; ec                          ; 0xc4668 vberom.asm:861
    test AL, strict byte 008h                 ; a8 08                       ; 0xc4669 vberom.asm:862
    je short 04668h                           ; 74 fb                       ; 0xc466b vberom.asm:863
    pop dx                                    ; 5a                          ; 0xc466d vberom.asm:864
    push ax                                   ; 50                          ; 0xc466e vberom.asm:868
    push cx                                   ; 51                          ; 0xc466f vberom.asm:869
    push dx                                   ; 52                          ; 0xc4670 vberom.asm:870
    push si                                   ; 56                          ; 0xc4671 vberom.asm:871
    push di                                   ; 57                          ; 0xc4672 vberom.asm:872
    sal dx, 010h                              ; c1 e2 10                    ; 0xc4673 vberom.asm:873
    and cx, strict word 0ffffh                ; 81 e1 ff ff                 ; 0xc4676 vberom.asm:874
    add byte [bx+si], al                      ; 00 00                       ; 0xc467a
    db  00bh, 0cah
    ; or cx, dx                                 ; 0b ca                     ; 0xc467c vberom.asm:875
    sal cx, 002h                              ; c1 e1 02                    ; 0xc467e vberom.asm:876
    db  08bh, 0c1h
    ; mov ax, cx                                ; 8b c1                     ; 0xc4681 vberom.asm:877
    push ax                                   ; 50                          ; 0xc4683 vberom.asm:878
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4684 vberom.asm:879
    push ES                                   ; 06                          ; 0xc468a
    add byte [bp-011h], ah                    ; 00 66 ef                    ; 0xc468b
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc468e vberom.asm:882
    db  00fh, 0b7h, 0c8h
    ; movzx cx, ax                              ; 0f b7 c8                  ; 0xc4694 vberom.asm:884
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc4697 vberom.asm:885
    add ax, word [bx+si]                      ; 03 00                       ; 0xc469d
    out DX, eax                               ; 66 ef                       ; 0xc469f vberom.asm:887
    mov edx, strict dword 0ed6601cfh          ; 66 ba cf 01 66 ed           ; 0xc46a1 vberom.asm:888
    db  00fh, 0b7h, 0f0h
    ; movzx si, ax                              ; 0f b7 f0                  ; 0xc46a7 vberom.asm:890
    pop ax                                    ; 58                          ; 0xc46aa vberom.asm:891
    cmp si, strict byte 00004h                ; 83 fe 04                    ; 0xc46ab vberom.asm:893
    je short 046c7h                           ; 74 17                       ; 0xc46ae vberom.asm:894
    add si, strict byte 00007h                ; 83 c6 07                    ; 0xc46b0 vberom.asm:895
    shr si, 003h                              ; c1 ee 03                    ; 0xc46b3 vberom.asm:896
    imul cx, si                               ; 0f af ce                    ; 0xc46b6 vberom.asm:897
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc46b9 vberom.asm:898
    div cx                                    ; f7 f1                       ; 0xc46bb vberom.asm:899
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc46bd vberom.asm:900
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc46bf vberom.asm:901
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc46c1 vberom.asm:902
    div si                                    ; f7 f6                       ; 0xc46c3 vberom.asm:903
    jmp short 046d3h                          ; eb 0c                       ; 0xc46c5 vberom.asm:904
    shr cx, 1                                 ; d1 e9                       ; 0xc46c7 vberom.asm:907
    db  033h, 0d2h
    ; xor dx, dx                                ; 33 d2                     ; 0xc46c9 vberom.asm:908
    div cx                                    ; f7 f1                       ; 0xc46cb vberom.asm:909
    db  08bh, 0f8h
    ; mov di, ax                                ; 8b f8                     ; 0xc46cd vberom.asm:910
    db  08bh, 0c2h
    ; mov ax, dx                                ; 8b c2                     ; 0xc46cf vberom.asm:911
    sal ax, 1                                 ; d1 e0                       ; 0xc46d1 vberom.asm:912
    push edx                                  ; 66 52                       ; 0xc46d3 vberom.asm:915
    push eax                                  ; 66 50                       ; 0xc46d5 vberom.asm:916
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc46d7 vberom.asm:917
    or byte [bx+si], al                       ; 08 00                       ; 0xc46dd
    out DX, eax                               ; 66 ef                       ; 0xc46df vberom.asm:919
    pop eax                                   ; 66 58                       ; 0xc46e1 vberom.asm:920
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc46e3 vberom.asm:921
    pop edx                                   ; 66 5a                       ; 0xc46e9 vberom.asm:923
    db  066h, 08bh, 0c7h
    ; mov eax, edi                              ; 66 8b c7                  ; 0xc46eb vberom.asm:925
    push edx                                  ; 66 52                       ; 0xc46ee vberom.asm:926
    push eax                                  ; 66 50                       ; 0xc46f0 vberom.asm:927
    mov edx, strict dword 0b86601ceh          ; 66 ba ce 01 66 b8           ; 0xc46f2 vberom.asm:928
    or word [bx+si], ax                       ; 09 00                       ; 0xc46f8
    out DX, eax                               ; 66 ef                       ; 0xc46fa vberom.asm:930
    pop eax                                   ; 66 58                       ; 0xc46fc vberom.asm:931
    mov edx, strict dword 0ef6601cfh          ; 66 ba cf 01 66 ef           ; 0xc46fe vberom.asm:932
    pop edx                                   ; 66 5a                       ; 0xc4704 vberom.asm:934
    pop di                                    ; 5f                          ; 0xc4706 vberom.asm:936
    pop si                                    ; 5e                          ; 0xc4707 vberom.asm:937
    pop dx                                    ; 5a                          ; 0xc4708 vberom.asm:938
    pop cx                                    ; 59                          ; 0xc4709 vberom.asm:939
    pop ax                                    ; 58                          ; 0xc470a vberom.asm:940
    mov eax, strict dword 066c3004fh          ; 66 b8 4f 00 c3 66           ; 0xc470b vberom.asm:941
    mov ax, 0014fh                            ; b8 4f 01                    ; 0xc4711
  ; disGetNextSymbol 0xc4714 LB 0x1 -> off=0x0 cb=0000000000000001 uValue=0000000000000114 'vesa_pm_end'
vesa_pm_end:                                 ; 0xc4714 LB 0x1
    retn                                      ; c3                          ; 0xc4714 vberom.asm:946

  ; Padding 0x6b bytes at 0xc4715
  times 107 db 0

section _DATA progbits vstart=0x4780 align=1 ; size=0x3726 class=DATA group=DGROUP
  ; disGetNextSymbol 0xc4780 LB 0x3726 -> off=0x0 cb=000000000000002e uValue=00000000000c0000 '_msg_vga_init'
_msg_vga_init:                               ; 0xc4780 LB 0x2e
    db  'Oracle VM VirtualBox Version 6.1.3 VGA BIOS', 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc47ae LB 0x36f8 -> off=0x0 cb=0000000000000080 uValue=00000000000c002e 'vga_modes'
vga_modes:                                   ; 0xc47ae LB 0x80
    db  000h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 001h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  002h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h, 003h, 000h, 000h, 004h, 000h, 0b8h, 0ffh, 002h
    db  004h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h, 005h, 001h, 002h, 002h, 000h, 0b8h, 0ffh, 001h
    db  006h, 001h, 002h, 001h, 000h, 0b8h, 0ffh, 001h, 007h, 000h, 001h, 004h, 000h, 0b0h, 0ffh, 000h
    db  00dh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h, 00eh, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 001h
    db  00fh, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 000h, 010h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  011h, 001h, 003h, 001h, 000h, 0a0h, 0ffh, 002h, 012h, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
    db  013h, 001h, 005h, 008h, 000h, 0a0h, 0ffh, 003h, 06ah, 001h, 004h, 004h, 000h, 0a0h, 0ffh, 002h
  ; disGetNextSymbol 0xc482e LB 0x3678 -> off=0x0 cb=0000000000000010 uValue=00000000000c00ae 'line_to_vpti'
line_to_vpti:                                ; 0xc482e LB 0x10
    db  017h, 017h, 018h, 018h, 004h, 005h, 006h, 007h, 00dh, 00eh, 011h, 012h, 01ah, 01bh, 01ch, 01dh
  ; disGetNextSymbol 0xc483e LB 0x3668 -> off=0x0 cb=0000000000000004 uValue=00000000000c00be 'dac_regs'
dac_regs:                                    ; 0xc483e LB 0x4
    dd  0ff3f3f3fh
  ; disGetNextSymbol 0xc4842 LB 0x3664 -> off=0x0 cb=0000000000000780 uValue=00000000000c00c2 'video_param_table'
video_param_table:                           ; 0xc4842 LB 0x780
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
    db  050h, 018h, 00eh, 000h, 010h, 000h, 003h, 000h, 002h, 067h, 05fh, 04fh, 050h, 082h, 055h, 081h
    db  0bfh, 01fh, 000h, 04fh, 00dh, 00eh, 000h, 000h, 000h, 000h, 09ch, 08eh, 08fh, 028h, 01fh, 096h
    db  0b9h, 0a3h, 0ffh, 000h, 001h, 002h, 003h, 004h, 005h, 014h, 007h, 038h, 039h, 03ah, 03bh, 03ch
    db  03dh, 03eh, 03fh, 00ch, 000h, 00fh, 008h, 000h, 000h, 000h, 000h, 000h, 010h, 00eh, 00fh, 0ffh
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
  ; disGetNextSymbol 0xc4fc2 LB 0x2ee4 -> off=0x0 cb=00000000000000c0 uValue=00000000000c0842 'palette0'
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
  ; disGetNextSymbol 0xc5082 LB 0x2e24 -> off=0x0 cb=00000000000000c0 uValue=00000000000c0902 'palette1'
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
  ; disGetNextSymbol 0xc5142 LB 0x2d64 -> off=0x0 cb=00000000000000c0 uValue=00000000000c09c2 'palette2'
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
  ; disGetNextSymbol 0xc5202 LB 0x2ca4 -> off=0x0 cb=0000000000000300 uValue=00000000000c0a82 'palette3'
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
  ; disGetNextSymbol 0xc5502 LB 0x29a4 -> off=0x0 cb=0000000000000010 uValue=00000000000c0d82 'static_functionality'
static_functionality:                        ; 0xc5502 LB 0x10
    db  0ffh, 0e0h, 00fh, 000h, 000h, 000h, 000h, 007h, 002h, 008h, 0e7h, 00ch, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5512 LB 0x2994 -> off=0x0 cb=0000000000000024 uValue=00000000000c0d92 '_dcc_table'
_dcc_table:                                  ; 0xc5512 LB 0x24
    db  010h, 001h, 007h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5536 LB 0x2970 -> off=0x0 cb=000000000000001a uValue=00000000000c0db6 '_secondary_save_area'
_secondary_save_area:                        ; 0xc5536 LB 0x1a
    db  01ah, 000h, 012h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc5550 LB 0x2956 -> off=0x0 cb=000000000000001c uValue=00000000000c0dd0 '_video_save_pointer_table'
_video_save_pointer_table:                   ; 0xc5550 LB 0x1c
    db  042h, 048h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
    db  036h, 055h, 000h, 0c0h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h
  ; disGetNextSymbol 0xc556c LB 0x293a -> off=0x0 cb=0000000000000800 uValue=00000000000c0dec 'vgafont8'
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
  ; disGetNextSymbol 0xc5d6c LB 0x213a -> off=0x0 cb=0000000000000e00 uValue=00000000000c15ec 'vgafont14'
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
  ; disGetNextSymbol 0xc6b6c LB 0x133a -> off=0x0 cb=0000000000001000 uValue=00000000000c23ec 'vgafont16'
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
  ; disGetNextSymbol 0xc7b6c LB 0x33a -> off=0x0 cb=000000000000012d uValue=00000000000c33ec 'vgafont14alt'
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
  ; disGetNextSymbol 0xc7c99 LB 0x20d -> off=0x0 cb=0000000000000144 uValue=00000000000c3519 'vgafont16alt'
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
  ; disGetNextSymbol 0xc7ddd LB 0xc9 -> off=0x0 cb=0000000000000009 uValue=00000000000c365d '_cga_msr'
_cga_msr:                                    ; 0xc7ddd LB 0x9
    db  02ch, 028h, 02dh, 029h, 02ah, 02eh, 01eh, 029h, 000h
  ; disGetNextSymbol 0xc7de6 LB 0xc0 -> off=0x0 cb=0000000000000015 uValue=00000000000c3666 '_vbebios_copyright'
_vbebios_copyright:                          ; 0xc7de6 LB 0x15
    db  'VirtualBox VESA BIOS', 000h
  ; disGetNextSymbol 0xc7dfb LB 0xab -> off=0x0 cb=0000000000000013 uValue=00000000000c367b '_vbebios_vendor_name'
_vbebios_vendor_name:                        ; 0xc7dfb LB 0x13
    db  'Oracle Corporation', 000h
  ; disGetNextSymbol 0xc7e0e LB 0x98 -> off=0x0 cb=0000000000000021 uValue=00000000000c368e '_vbebios_product_name'
_vbebios_product_name:                       ; 0xc7e0e LB 0x21
    db  'Oracle VM VirtualBox VBE Adapter', 000h
  ; disGetNextSymbol 0xc7e2f LB 0x77 -> off=0x0 cb=0000000000000023 uValue=00000000000c36af '_vbebios_product_revision'
_vbebios_product_revision:                   ; 0xc7e2f LB 0x23
    db  'Oracle VM VirtualBox Version 6.1.3', 000h
  ; disGetNextSymbol 0xc7e52 LB 0x54 -> off=0x0 cb=000000000000002b uValue=00000000000c36d2 '_vbebios_info_string'
_vbebios_info_string:                        ; 0xc7e52 LB 0x2b
    db  'VirtualBox VBE Display Adapter enabled', 00dh, 00ah, 00dh, 00ah, 000h
  ; disGetNextSymbol 0xc7e7d LB 0x29 -> off=0x0 cb=0000000000000029 uValue=00000000000c36fd '_no_vbebios_info_string'
_no_vbebios_info_string:                     ; 0xc7e7d LB 0x29
    db  'No VirtualBox VBE support available!', 00dh, 00ah, 00dh, 00ah, 000h

section CONST progbits vstart=0x7ea6 align=1 ; size=0x0 class=DATA group=DGROUP

section CONST2 progbits vstart=0x7ea6 align=1 ; size=0x0 class=DATA group=DGROUP

  ; Padding 0x15a bytes at 0xc7ea6
    db  001h, 000h, 000h, 000h, 000h, 001h, 000h, 000h, 000h, 000h, 000h, 000h, 044h, 03ah, 05ch, 052h
    db  065h, 070h, 06fh, 073h, 069h, 074h, 06fh, 072h, 079h, 05ch, 074h, 072h, 075h, 06eh, 06bh, 05ch
    db  06fh, 075h, 074h, 05ch, 077h, 069h, 06eh, 02eh, 061h, 06dh, 064h, 036h, 034h, 05ch, 072h, 065h
    db  06ch, 065h, 061h, 073h, 065h, 05ch, 06fh, 062h, 06ah, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h
    db  061h, 042h, 069h, 06fh, 073h, 033h, 038h, 036h, 05ch, 056h, 042h, 06fh, 078h, 056h, 067h, 061h
    db  042h, 069h, 06fh, 073h, 033h, 038h, 036h, 02eh, 073h, 079h, 06dh, 000h, 000h, 000h, 000h, 000h
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
    db  000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 000h, 010h
